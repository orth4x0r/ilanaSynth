"""M8.3 re-fit: measure struck low-pass-gate notes (reference or ours).

usage: python tools/lpg_metrics.py file.wav [...]

Per file: when the strike peaks, how long it takes to fall 10, 20 and 30 dB
(20 ms RMS windows), and the spectral centroid at the peak and 60 and
150 ms later (the gate darkening as it closes). The recordings in
build/reference/lpg/ are performance excerpts with a noise floor, so
nothing below -30 dB is measured.
"""
import sys

import numpy as np
import soundfile as sf


def metrics(path):
    x, rate = sf.read(path)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x = x / (np.abs(x).max() + 1e-12)
    hop, win = int(0.005 * rate), int(0.02 * rate)
    env = np.array([np.sqrt(np.mean(x[i:i + win] ** 2) + 1e-20) for i in range(0, len(x) - win, hop)])
    peak = int(env.argmax())
    db = 20 * np.log10(env / env[peak])

    def fall(level):
        below = np.where(db[peak:] < level)[0]
        return (below[0] * hop / rate) if len(below) else float("nan")

    def centroid(t):
        start = int(t * rate)
        frame = x[start:start + 2048]
        if len(frame) < 2048:
            return float("nan")
        spectrum = np.abs(np.fft.rfft(frame * np.hanning(2048)))
        freqs = np.fft.rfftfreq(2048, 1 / rate)
        return float((spectrum * freqs).sum() / (spectrum.sum() + 1e-12))

    t0 = peak * hop / rate
    return {"peak": t0, "fall10": fall(-10), "fall20": fall(-20), "fall30": fall(-30),
            "c0": centroid(t0), "c60": centroid(t0 + 0.06), "c150": centroid(t0 + 0.15)}


if __name__ == "__main__":
    for path in sys.argv[1:]:
        m = metrics(path)
        print(f"{path.split('/')[-1]}: peak {m['peak'] * 1000:.0f} ms, -10/-20/-30 dB after {m['fall10'] * 1000:.0f}/"
              f"{m['fall20'] * 1000:.0f}/{m['fall30'] * 1000:.0f} ms, centroid {m['c0']:.0f} -> {m['c60']:.0f} -> {m['c150']:.0f} Hz")
