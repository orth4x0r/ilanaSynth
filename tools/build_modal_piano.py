#!/usr/bin/env python3
"""Build the Modal Piano table (content/piano/modal-piano.bin) from a render
of a reference piano: one note per slot, every note in the list at each
velocity, velocity-major (all notes at the first velocity, then the next).

For each note and velocity it keeps the partials' frequencies (as ratios to
the note's nominal pitch), each partial's level at a fixed set of times after
the strike, and the first 100 ms of the recording itself (the hammer's
attack, which the partials take over from).

    tools/build_modal_piano.py render.wav --notes 21,33,45,... --velocities 20,60,124 \
        --slot 12 [--out content/piano/modal-piano.bin]

Needs numpy only. The render is read as 16/24/32-bit PCM WAV, mixed to mono.
"""

import argparse
import struct
import sys
import wave

import numpy as np

# Times (s after the strike) at which every partial's level is kept. The
# attack sample covers the first 80 ms, so the grid starts there.
TIMES = [0.05, 0.08, 0.12, 0.2, 0.3, 0.45, 0.7, 1.0, 1.5, 2.0, 3.0, 4.0, 5.5, 7.0, 9.0, 11.0]
MAX_PARTIALS = 100
MAX_FREQ = 16000.0
ATTACK_SECONDS = 0.1
# The board's own resonances: the strongest peaks below this that are not
# the note's partials, kept with their level over time (fixed in Hz).
MODE_TOP = 1200.0
MAX_MODES = 12


def read_wav(path):
    with wave.open(path, "rb") as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    if width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    elif width == 2:
        x = np.frombuffer(raw, "<i2") / 32768.0
    elif width == 4:
        x = np.frombuffer(raw, "<i4") / float(1 << 31)
    else:
        sys.exit("unsupported sample width %d" % width)
    return x.reshape(-1, channels).mean(axis=1), rate


def onset(x, rate):
    n = max(1, int(rate * 0.005))
    e = np.convolve(np.abs(x), np.ones(n) / n, "same")
    return max(0, int(np.argmax(e > 0.15 * e.max())) - int(0.004 * rate))


def spectrum(x, start, size):
    s = x[start:start + size]
    if len(s) < size:
        s = np.pad(s, (0, size - len(s)))
    w = np.hanning(size)
    return np.abs(np.fft.rfft(s * w)) * 2.0 / w.sum()


def track(mag, freqs, f0):
    """The partials' frequencies, following the stretch as it shows."""
    found, b, half = [], 3e-4, 0.35 * f0
    for n in range(1, MAX_PARTIALS + 1):
        centre = n * f0 * np.sqrt(1 + b * n * n)
        if centre > MAX_FREQ or centre > freqs[-1] * 0.95:
            break
        k = np.where((freqs > centre - half) & (freqs < centre + half))[0]
        if len(k) == 0:
            break
        j = k[np.argmax(mag[k])]
        f = freqs[j]
        if 0 < j < len(freqs) - 1:
            a, c0, c = np.log(mag[j - 1:j + 2] + 1e-12)
            den = a - 2 * c0 + c
            if den < 0:
                f = freqs[j] + 0.5 * (a - c) / den * (freqs[1] - freqs[0])
        found.append(f)
        if n >= 3:
            nn = np.arange(1, len(found) + 1)
            y = (np.array(found) / (nn * f0)) ** 2 - 1
            b = max(1e-5, float(np.sum(y * nn ** 2) / np.sum(nn ** 4)))
    return np.array(found)


def analyse(x, rate, note):
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    size = 16384 if f0 < 60 else 8192
    freqs = np.fft.rfftfreq(size, 1 / rate)
    # Frequencies from a frame 0.5 s in (settled, still strong).
    partials = track(spectrum(x, int(0.5 * rate), size), freqs, f0)
    half = max(2.0, 0.2 * f0)
    levels = np.full((len(partials), len(TIMES)), -160.0)
    for t, when in enumerate(TIMES):
        mag = spectrum(x, max(0, int(when * rate) - size // 2), size)
        for n, f in enumerate(partials):
            k = (freqs > f - half) & (freqs < f + half)
            levels[n, t] = 20 * np.log10(mag[k].max() + 1e-9)
    return f0, partials / f0, levels


def body_modes(x, rate, f0, partials):
    """Peaks between and below the partials: the soundboard and the other
    strings ringing, struck by the hammer (a treble note's low boom)."""
    size = 16384
    freqs = np.fft.rfftfreq(size, 1 / rate)
    level = 20 * np.log10(spectrum(x, int(0.3 * rate), size) + 1e-9)
    top = level.max()
    keep = np.ones(len(freqs), bool)
    for f in partials:
        keep &= np.abs(freqs - f) > max(8.0, 0.08 * f0)
    found = []
    for i in np.argsort(-level):
        if freqs[i] < 20 or freqs[i] > MODE_TOP or not keep[i] or level[i] < top - 55:
            continue
        if not (level[i] >= level[max(0, i - 2):i + 3]).all():
            continue
        if any(abs(freqs[i] - f) < 6.0 for f in found):
            continue
        found.append(float(freqs[i]))
        if len(found) == MAX_MODES:
            break
    return sorted(found)


def mode_levels(x, rate, modes):
    size = 16384
    freqs = np.fft.rfftfreq(size, 1 / rate)
    levels = np.full((len(modes), len(TIMES)), -160.0)
    for t, when in enumerate(TIMES):
        mag = spectrum(x, max(0, int(when * rate) - size // 2), size)
        for n, f in enumerate(modes):
            k = (freqs > f - 4.0) & (freqs < f + 4.0)
            levels[n, t] = 20 * np.log10(mag[k].max() + 1e-9)
    return levels


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("wav")
    parser.add_argument("--notes", required=True, help="comma-separated MIDI notes, or a range like 21-108")
    parser.add_argument("--velocities", default="20,60,124")
    parser.add_argument("--slot", type=float, required=True, help="seconds from one note's start to the next")
    parser.add_argument("--out", default="content/piano/modal-piano.bin")
    args = parser.parse_args()

    if "-" in args.notes:
        lo, hi = map(int, args.notes.split("-"))
        notes = list(range(lo, hi + 1))
    else:
        notes = [int(n) for n in args.notes.split(",")]
    velocities = [int(v) for v in args.velocities.split(",")]
    x, rate = read_wav(args.wav)
    slot = int(round(args.slot * rate))

    def segment(vi, ni):
        begin = (vi * len(notes) + ni) * slot
        seg = x[max(0, begin - 4000):begin + slot - int(0.05 * rate)]
        return seg[onset(seg, rate):]

    # One set of board resonances per note, found at its loudest velocity.
    modes = {}
    for ni, note in enumerate(notes):
        seg = segment(len(velocities) - 1, ni)
        f0, ratios, _ = analyse(seg, rate, note)
        modes[note] = body_modes(seg, rate, f0, ratios * f0)

    entries = []
    for vi, velocity in enumerate(velocities):
        for ni, note in enumerate(notes):
            seg = segment(vi, ni)
            f0, ratios, levels = analyse(seg, rate, note)
            attack = seg[:int(ATTACK_SECONDS * rate)]
            entries.append((note, velocity, f0, ratios, levels, attack, modes[note], mode_levels(seg, rate, modes[note])))
            print("note %d vel %d: %d partials, %d board modes, attack peak %.3f"
                  % (note, velocity, len(ratios), len(modes[note]), np.abs(attack).max()), flush=True)

    # Little-endian: magic, version, counts, the time grid, then each entry.
    out = bytearray(b"IMPT")
    out += struct.pack("<HHHf", 2, len(entries), len(TIMES), float(rate))
    out += struct.pack("<%df" % len(TIMES), *TIMES)
    for note, velocity, f0, ratios, levels, attack, modes, mlevels in entries:
        scale = max(1e-9, float(np.abs(attack).max()))
        out += struct.pack("<HHHI", note, velocity, len(ratios), len(attack))
        out += struct.pack("<f", scale)
        out += struct.pack("<%df" % len(ratios), *ratios.astype(np.float32))
        # Levels in dB, in 1/64 dB steps.
        out += np.clip(np.round(levels * 64), -32768, 32767).astype("<i2").tobytes()
        out += np.clip(np.round(attack / scale * 32767), -32767, 32767).astype("<i2").tobytes()
        # Version 2: the board's resonances, in Hz, and their levels.
        out += struct.pack("<H", len(modes))
        out += struct.pack("<%df" % len(modes), *modes)
        out += np.clip(np.round(mlevels * 64), -32768, 32767).astype("<i2").tobytes()
    with open(args.out, "wb") as f:
        f.write(out)
    print("wrote %s (%d bytes, %d entries)" % (args.out, len(out), len(entries)))


if __name__ == "__main__":
    main()
