"""M8.2: the soundboard's colour, taken from the recordings.

usage: python tools/piano_board_eq.py <ours folder, board off> [--notes 21,...]

For every note and dynamic, the long-term (0-2 s) octave-band spectrum of
the reference grand over the bare string model's, averaged over the keyboard
(each band only where the reference has energy there), with the 125 Hz-4 kHz
mean taken out (the level is the model's). Prints ILANA_PIANO2_TUNING text
for PianoModelTuning::boardEq (31 Hz .. 16 kHz).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from piano_metrics import ALL_NOTES, DYNAMICS, REF, load_mono, reference_path  # noqa: E402

CENTRES = 31.25 * 2.0 ** np.arange(10)


def octave_levels(path):
    x, rate = load_mono(path, 2.0)
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    freqs = np.fft.rfftfreq(len(x), 1 / rate)
    out = []
    for c in CENTRES:
        band = (freqs >= c / np.sqrt(2)) & (freqs < c * np.sqrt(2))
        out.append(10 * np.log10(spectrum[band].sum() + 1e-20))
    return np.array(out)


def main():
    ours = sys.argv[1]
    notes = ALL_NOTES
    for a in sys.argv[2:]:
        if a.startswith("--notes="):
            notes = [int(v) for v in a.split("=", 1)[1].split(",")]
    diffs = [[] for _ in CENTRES]
    for midi in notes:
        for dyn in DYNAMICS:
            a_path = os.path.join(ours, f"ours.{dyn}.n{midi}.wav")
            b_path = reference_path(REF, midi, dyn)
            if not (os.path.exists(a_path) and os.path.exists(b_path)):
                continue
            a, b = octave_levels(a_path), octave_levels(b_path)
            a -= a.max()
            b -= b.max()
            for i in range(len(CENTRES)):
                if b[i] > -45 and a[i] > -80:
                    diffs[i].append(b[i] - a[i])
    curve = np.array([np.median(d) if d else 0.0 for d in diffs])
    middle = (CENTRES >= 125) & (CENTRES <= 4000)
    curve -= curve[middle].mean()
    curve = np.clip(curve, -18, 18)
    print(";".join(f"boardEq{i}={v:.2f}" for i, v in enumerate(curve)))


if __name__ == "__main__":
    main()
