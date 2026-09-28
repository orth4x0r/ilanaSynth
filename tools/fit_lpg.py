"""M8.3: fit the low-pass gate's vactrol (WestCoastTuning.h) to its reference.

usage: python tools/fit_lpg.py

No recording of a struck Buchla low-pass gate was reachable, so the
reference is the published description of the 292's vactrol (Parker and
D'Angelo, "A digital model of the Buchla lowpass-gate", DAFx 2013;
VTL5C3 datasheet):
  - lights up to 63 % in about 12 ms (rise),
  - goes 63 % dark in about 250 ms after the LED turns off,
  - slows as it darkens (the photocell's memory): here taken as 90 % dark
    after about 1 s (four times the 63 % point, where one time constant
    would take 2.3 times).
The same equations as Vactrol::process (src/dsp/WestCoast.h) are simulated
and fallSeconds / fallSlowing searched to hit the two dark times.
"""
import itertools

import numpy as np

RATE = 48000.0
TARGET_63 = 0.250
TARGET_90 = 1.000


def dark_times(fall, slowing, rise=0.012):
    c = 0.0
    # Light fully (hold the LED on 0.2 s), then off.
    for _ in range(int(0.2 * RATE)):
        c += (1.0 - c) * (1.0 - np.exp(-1.0 / (rise * RATE)))
    start = c
    t63 = t90 = None
    for i in range(int(4.0 * RATE)):
        seconds = fall * (1.0 + slowing * (1.0 - c)) ** 2 / (1.0 + slowing * 0.5) ** 2
        c += (0.0 - c) * (1.0 - np.exp(-1.0 / (seconds * RATE)))
        if t63 is None and c <= start * 0.37:
            t63 = i / RATE
        if t90 is None and c <= start * 0.10:
            t90 = i / RATE
            break
    return t63 or 4.0, t90 or 4.0


def main():
    best = None
    for fall, slowing in itertools.product(np.arange(0.05, 0.4, 0.005), np.arange(0.0, 4.01, 0.1)):
        t63, t90 = dark_times(fall, slowing)
        error = (np.log(t63 / TARGET_63)) ** 2 + (np.log(t90 / TARGET_90)) ** 2
        if best is None or error < best[0]:
            best = (error, fall, slowing, t63, t90)
    error, fall, slowing, t63, t90 = best
    print(f"fallSeconds={fall:.3f};fallSlowing={slowing:.2f}  ->  63% dark {t63 * 1000:.0f} ms, 90% dark {t90 * 1000:.0f} ms (error {error:.4f})")


if __name__ == "__main__":
    main()
