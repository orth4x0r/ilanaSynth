"""Partial-by-partial comparison of piano notes (.wav or .aiff).

usage:
  python tools/analyse_note.py <midi note> file [file ...]

For each file: trims leading silence, then prints partial levels (relative
to the strongest partial at the start) at several times, each partial's
decay rate, its frequency ratio to n*f0 (inharmonicity) and the level of
what lies between the partials (noise, buzz, body).
"""
import struct
import sys
import wave

import numpy as np


def read_aiff(path):
    data = open(path, "rb").read()
    assert data[:4] == b"FORM" and data[8:12] in (b"AIFF", b"AIFC")
    pos, channels, bits, rate, samples = 12, 1, 16, 44100.0, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack(">I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"COMM":
            channels, _, bits = struct.unpack(">hIh", body[:8])
            exponent, mantissa = struct.unpack(">HQ", body[8:18])
            rate = mantissa * 2.0 ** (exponent - 16383 - 63)
        elif cid == b"SSND":
            offset = struct.unpack(">I", body[:4])[0]
            raw = body[8 + offset:]
            if bits == 16:
                samples = np.frombuffer(raw, dtype=">i2").astype(np.float64) / 32768
            elif bits == 24:
                b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
                v = (b[:, 0] << 16) | (b[:, 1] << 8) | b[:, 2]
                samples = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
        pos += 8 + size + (size & 1)
    return samples.reshape(-1, channels).mean(axis=1), int(rate)


def read_wav(path):
    with wave.open(path) as w:
        rate, width, channels = w.getframerate(), w.getsampwidth(), w.getnchannels()
        raw = w.readframes(w.getnframes())
    if width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        data = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
    else:
        data = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768
    return data.reshape(-1, channels).mean(axis=1), rate


def load(path):
    x, rate = read_aiff(path) if path.lower().endswith((".aif", ".aiff")) else read_wav(path)
    peak = np.max(np.abs(x))
    start = np.argmax(np.abs(x) > peak * 0.05)
    return x[max(0, start - int(0.002 * rate)):], rate


def analyse(path, f0, partials, times):
    x, rate = load(path)
    size = 1 << 14
    window = np.hanning(size)
    freqs = np.fft.rfftfreq(size, 1 / rate)
    rows = []
    for t in times:
        start = int(t * rate)
        frame = x[start:start + size]
        if len(frame) < size:
            break
        spectrum = np.abs(np.fft.rfft(frame * window)) + 1e-12
        levels, ratios, between = [], [], []
        for n in range(1, partials + 1):
            band = (freqs >= n * f0 * 0.95) & (freqs <= n * f0 * 1.12)
            if not band.any():
                break
            k = np.argmax(spectrum * band)
            levels.append(20 * np.log10(spectrum[k]))
            ratios.append(freqs[k] / (n * f0))
            gap = (freqs > n * f0 * 1.3) & (freqs < n * f0 * 1.7)
            between.append(20 * np.log10(np.sqrt(np.mean(spectrum[gap] ** 2))))
        rows.append((t, np.array(levels), np.array(ratios), np.array(between)))
    reference = rows[0][1].max()
    return rows, reference


def main():
    note = int(sys.argv[1])
    f0 = 440.0 * 2 ** ((note - 69) / 12)
    partials = 12 if f0 < 1000 else 5
    times = [0.03, 0.25, 0.5, 1.0, 2.0, 4.0]
    for path in sys.argv[2:]:
        rows, reference = analyse(path, f0, partials, times)
        print(f"\n== {path.split('/')[-1].split(chr(92))[-1]}  (f0 {f0:.1f} Hz)")
        print("partial     " + " ".join(f"{n:6d}" for n in range(1, len(rows[0][1]) + 1)) + "   between(avg of first 6 gaps)")
        for t, levels, _, between in rows:
            print(f"t={t:4.2f}s    " + " ".join(f"{v - reference:6.1f}" for v in levels)
                  + f"   {np.mean(between[:6]) - reference:6.1f}")
        # Decay rate per partial between the 0.25 s and the last row.
        first, last = rows[1], rows[-1]
        span = last[0] - first[0]
        print("dB/s        " + " ".join(f"{(b - a) / span:6.1f}" for a, b in zip(first[1], last[1])))
        print("ratio       " + " ".join(f"{r:6.3f}" for r in rows[1][2]))


if __name__ == "__main__":
    main()
