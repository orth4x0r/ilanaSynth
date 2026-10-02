"""Compares an ilanaSynth render of a converted DX7 voice with the msfa reference.

    python tools/dx7_compare.py <reference.wav> <ours.wav> [--f0 261.63]

Both are levelled to the same peak loudness first. Prints the loudness
envelope difference over time and the first harmonics' levels (dB under the
strongest) at a few moments, side by side.
"""

import argparse
import sys

import numpy as np
import soundfile as sf


def mono(path):
    x, rate = sf.read(path, always_2d=True)
    return x.mean(axis=1), rate


def frames_db(x, rate, hop=0.02):
    n = int(rate * hop)
    count = len(x) // n
    rms = np.sqrt(np.mean(x[: count * n].reshape(count, n) ** 2, axis=1))
    return 20 * np.log10(rms + 1e-9)


def harmonics(x, rate, t, f0, count=12, window=0.1):
    start = int(t * rate)
    seg = x[start: start + int(window * rate)]
    if len(seg) < 256:
        return np.full(count, -120.0)
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 16))
    freqs = np.fft.rfftfreq(1 << 16, 1 / rate)
    levels = []
    for h in range(1, count + 1):
        band = (freqs > f0 * h * 0.97) & (freqs < f0 * h * 1.03)
        levels.append(20 * np.log10(spec[band].max() + 1e-12) if band.any() else -120.0)
    levels = np.array(levels)
    return levels - levels.max()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference")
    ap.add_argument("ours")
    ap.add_argument("--f0", type=float, default=261.63)
    args = ap.parse_args()
    ref, rate = mono(args.reference)
    ours, rate2 = mono(args.ours)
    if rate2 != rate:
        print("sample rates differ", file=sys.stderr)
        return 1
    n = min(len(ref), len(ours))
    ref, ours = ref[:n], ours[:n]
    a, b = frames_db(ref, rate), frames_db(ours, rate)
    offset = a.max() - b.max()
    b = b + offset
    print(f"level: ours needs {offset:+.1f} dB to match the reference peak")
    print("time  ref dB  ours dB  diff")
    for t in (0.02, 0.1, 0.3, 0.6, 1.0, 1.5, 1.98, 2.2, 2.6, 3.0, 3.4):
        i = min(len(a) - 1, int(t / 0.02))
        print(f"{t:4.2f}  {a[i]:6.1f}  {b[i]:7.1f}  {b[i] - a[i]:+5.1f}")
    active = (a > a.max() - 50)
    print(f"envelope error (rms dB over the audible part): {np.sqrt(np.mean((b[active] - a[active]) ** 2)):.2f}")
    total = 0.0
    for t in (0.01, 0.3, 1.0, 1.8):
        hr, ho = harmonics(ref, rate, t, args.f0), harmonics(ours, rate, t, args.f0)
        diff = np.clip(ho, -60, 0) - np.clip(hr, -60, 0)
        total += np.sqrt(np.mean(diff ** 2))
        print(f"t={t:4.2f} ref  " + " ".join(f"{v:5.0f}" for v in hr))
        print(f"       ours " + " ".join(f"{v:5.0f}" for v in ho))
    print(f"harmonic error (mean rms dB): {total / 4:.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
