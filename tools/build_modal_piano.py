#!/usr/bin/env python3
"""Build the Modal Piano table (content/piano/modal-piano.bin) from a render
of a reference piano: one note per slot, every note in the list at each
velocity, velocity-major (all notes at the first velocity, then the next).

For each note and velocity it keeps the partials' frequencies (as ratios to
the note's nominal pitch), each partial's level at a fixed set of times after
the strike, and the first 85 ms of the recording itself (the hammer's
attack, which the partials take over from).

    tools/build_modal_piano.py render.wav --notes 21,33,45,... --velocities 20,60,124 \
        --slot 12 [--out content/piano/modal-piano.bin]

Needs numpy only. The render is read as 16/24/32-bit PCM WAV. A stereo render
is not mixed to mono: a piano's spaced microphones are far out of phase with
each other (Piano V's American Grand: L/R correlation down to -0.9), and their
sum cancels whole partials. Levels are the two channels' power average (what
two ears hear). The stereo picture is kept apart from them: each partial's and
board resonance's balance and phase between the channels (steady over the
note, so one pair each, from the loudest velocity), and the attack in both
channels.
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
ATTACK_SECONDS = 0.085
# The board's own resonances: the strongest peaks below this that are not
# the note's partials, kept with their level over time (fixed in Hz).
MODE_TOP = 1200.0
MAX_MODES = 12
# The noise between the partials and modes (hammer, dampers, the board's
# hiss): its level in these bands (Hz), over time.
NOISE_BANDS = [(0, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]


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
    return x.reshape(-1, channels), rate


def onset(x, rate):
    n = max(1, int(rate * 0.005))
    e = np.convolve(np.abs(x).sum(axis=1), np.ones(n) / n, "same")
    return max(0, int(np.argmax(e > 0.15 * e.max())) - int(0.004 * rate))


def spectrum(x, start, size):
    """Magnitude, the channels' power average."""
    s = x[start:start + size]
    if len(s) < size:
        s = np.pad(s, ((0, size - len(s)), (0, 0)))
    w = np.hanning(size)
    power = (np.abs(np.fft.rfft(s * w[:, None], axis=0)) ** 2).mean(axis=1)
    return np.sqrt(power) * 2.0 / w.sum()


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


def noise_levels(x, rate, f0, partials, modes):
    """Median magnitude per band of the bins away from every partial and
    mode: the same measure the engine's noise is calibrated against."""
    size = 16384
    freqs = np.fft.rfftfreq(size, 1 / rate)
    keep = np.ones(len(freqs), bool)
    for f in partials:
        keep &= np.abs(freqs - f) > max(6.0, 0.2 * f0)
    for f in modes:
        keep &= np.abs(freqs - f) > 6.0
    levels = np.full((len(NOISE_BANDS), len(TIMES)), -160.0)
    for t, when in enumerate(TIMES):
        mag = spectrum(x, max(0, int(when * rate) - size // 2), size)
        for b, (lo, hi) in enumerate(NOISE_BANDS):
            k = keep & (freqs >= max(lo, 20)) & (freqs < hi)
            if k.sum() >= 8:
                levels[b, t] = 20 * np.log10(np.median(mag[k]) + 1e-9)
    return levels


def stereo(x, rate, freqs_hz):
    """Each component's balance (dB, left over right) and phase (right
    against left, radians), from the cross-spectrum over the first seconds."""
    size = 16384
    freqs = np.fft.rfftfreq(size, 1 / rate)
    w = np.hanning(size)
    cross = np.zeros(len(freqs_hz), complex)
    left = np.zeros(len(freqs_hz))
    right = np.zeros(len(freqs_hz))
    for when in (0.15, 0.4, 0.8, 1.4, 2.2):
        start = max(0, int(when * rate) - size // 2)
        s = x[start:start + size]
        if len(s) < size:
            break
        X = np.fft.rfft(s * w[:, None], axis=0)
        for n, f in enumerate(freqs_hz):
            k = int(round(f / freqs[1]))
            if k < 2 or k >= len(freqs) - 2:
                continue
            k += int(np.argmax(np.abs(X[k - 2:k + 3]).sum(axis=1))) - 2
            cross[n] += X[k, 1] * np.conj(X[k, 0])
            left[n] += abs(X[k, 0]) ** 2
            right[n] += abs(X[k, 1]) ** 2
    balance = np.clip(10 * np.log10((left + 1e-30) / (right + 1e-30)), -40, 40)
    return balance, np.angle(cross)


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
    parser.add_argument("--hold", type=float, default=0.0,
                        help="seconds each note is held: the time grid stops before the release (default: the slot)")
    parser.add_argument("--out", default="content/piano/modal-piano.bin")
    args = parser.parse_args()

    # Each level is read from a window round its time: keep the times whose
    # window ends before the key is let go (or the next note starts).
    global TIMES
    hold = args.hold if args.hold > 0 else args.slot
    TIMES = [t for t in TIMES if t + 0.2 < hold]

    if "-" in args.notes:
        lo, hi = map(int, args.notes.split("-"))
        notes = list(range(lo, hi + 1))
    else:
        notes = [int(n) for n in args.notes.split(",")]
    velocities = [int(v) for v in args.velocities.split(",")]
    x, rate = read_wav(args.wav)
    slot = int(round(args.slot * rate))

    def segment(vi, ni, hold=hold):
        begin = (vi * len(notes) + ni) * slot
        seg = x[max(0, begin - 4000):begin + int(min(args.slot, hold + 0.5) * rate) - int(0.05 * rate)]
        return seg[onset(seg, rate):]

    # One set of board resonances per note, found at its loudest velocity,
    # and the note's stereo picture (mono renders: centred).
    modes, pictures = {}, {}
    for ni, note in enumerate(notes):
        seg = segment(len(velocities) - 1, ni)
        f0, ratios, _ = analyse(seg, rate, note)
        modes[note] = body_modes(seg, rate, f0, ratios * f0)
        if seg.shape[1] >= 2:
            pictures[note] = (stereo(seg, rate, ratios * f0), stereo(seg, rate, np.array(modes[note])))
        else:
            centre = lambda n: (np.zeros(n), np.zeros(n))
            pictures[note] = (centre(len(ratios)), centre(len(modes[note])))

    entries = []
    for vi, velocity in enumerate(velocities):
        for ni, note in enumerate(notes):
            seg = segment(vi, ni)
            f0, ratios, levels = analyse(seg, rate, note)
            start = seg[:int(ATTACK_SECONDS * rate)]
            attack = (start[:, 0], start[:, 1 if start.shape[1] > 1 else 0])
            # The stereo picture's partials, as many as this velocity's.
            (pb, pp), mpic = pictures[note]
            count = min(len(ratios), len(pb))
            ppic = (np.pad(pb[:count], (0, len(ratios) - count)), np.pad(pp[:count], (0, len(ratios) - count)))
            entries.append((note, velocity, f0, ratios, levels, attack, modes[note], mode_levels(seg, rate, modes[note]),
                            noise_levels(seg, rate, f0, ratios * f0, modes[note]), ppic, mpic))
            print("note %d vel %d: %d partials, %d board modes, attack peak %.3f"
                  % (note, velocity, len(ratios), len(modes[note]), np.abs(start).max()), flush=True)

    # Little-endian: magic, version, counts, the time grid, then each entry.
    out = bytearray(b"IMPT")
    out += struct.pack("<HHHf", 4, len(entries), len(TIMES), float(rate))
    out += struct.pack("<%df" % len(TIMES), *TIMES)
    for note, velocity, f0, ratios, levels, (attack, attackRight), modes, mlevels, nlevels, ppic, mpic in entries:
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
        # Version 3: the noise, per band (NOISE_BANDS, the engine's bands).
        out += struct.pack("<H", len(NOISE_BANDS))
        out += np.clip(np.round(nlevels * 64), -32768, 32767).astype("<i2").tobytes()
        # Version 4: the right channel's attack, then the stereo picture:
        # balance (1/64 dB) and phase (1/10000 rad) per partial, then per mode.
        right = max(1e-9, float(np.abs(attackRight).max()))
        out += struct.pack("<f", right)
        out += np.clip(np.round(attackRight / right * 32767), -32767, 32767).astype("<i2").tobytes()
        for balance, phase in (ppic, mpic):
            out += np.clip(np.round(balance * 64), -32768, 32767).astype("<i2").tobytes()
            out += np.clip(np.round(phase * 10000), -32768, 32767).astype("<i2").tobytes()
    with open(args.out, "wb") as f:
        f.write(out)
    print("wrote %s (%d bytes, %d entries)" % (args.out, len(out), len(entries)))


if __name__ == "__main__":
    main()
