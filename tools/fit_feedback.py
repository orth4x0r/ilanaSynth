"""M8.5: fit the Feedback exciter's loop (FeedbackGuitarTuning.h).

usage: python tools/fit_feedback.py

Reference (re-fitted in M10): two clean recordings of a held electric-guitar
note feeding back through an amp, from freesound (build/reference/feedback,
see build/reference/SOURCES.md): feedback_01 (A3, open) and feedback_02
(F4, fretted). Measured by their harmonic levels over time:
  - the note never dies: its fundamental swells by 12 and 21 dB,
  - and comes within 3 dB of its final level after about 2 and 2.5 s;
  - the fundamental ends loudest in both (in 01 the twelfth also swells by
    60 dB but stays 25 dB under it).
So over five notes (E2-E4) at three amp distances, a note that settles on
the fundamental scores best, a low harmonic (2nd, 3rd) costs 2, anything
higher 10; a bloom time away from 2.2 s costs 4 (log ratio)^2, a swell
away from +16 dB costs ((swell - 16) / 8)^2, a note that dies costs 10.
Grid search over the loop gain, the speaker's top and the pickup level.

(The first fit, before recordings were reachable, used a written
description: a bloom after one to two seconds into one of the low
harmonics.)
"""
import itertools
import math
import os
import re
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE_NAME = "ilanaTableTest.exe" if os.name == "nt" else "ilanaTableTest"
EXE = os.environ.get("ILANA_FIT_EXE", os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", EXE_NAME))
LINE = re.compile(r"note (\d+) distance ([\d.]+): harmonic (\d+) blooms at ([\d.]+) s, final ([-\d.]+) dB \(pluck ([-\d.]+)\)")
BLOOM, SWELL = 2.2, 16.0


def score(tuning):
    env = dict(os.environ, ILANA_FEEDBACK_PROBE="1", ILANA_FEEDBACK_TUNING=tuning)
    out = subprocess.run([EXE], env=env, capture_output=True, text=True, cwd=ROOT).stdout
    total, detail = 0.0, []
    for m in LINE.finditer(out):
        note, distance, harmonic, bloom, final, pluck = int(m[1]), float(m[2]), int(m[3]), float(m[4]), float(m[5]), float(m[6])
        if distance not in (0.25, 0.5, 0.75):
            continue
        swell = final - pluck
        s = 0.0 if harmonic == 1 else 2.0 if harmonic <= 3 else 10.0
        if swell < -6.0:
            s += 10.0
        s += 4.0 * math.log(max(bloom, 0.05) / BLOOM) ** 2
        s += ((swell - SWELL) / 8.0) ** 2
        total += s
        detail.append((note, distance, harmonic, bloom, swell))
    return total, detail


def main():
    best = None
    for loop, top, pickup in itertools.product([0.004, 0.006, 0.008, 0.011], [1000.0, 1400.0, 2000.0, 3000.0], [0.5, 1.0, 2.0]):
        tuning = f"loopGain={loop};speakerHigh={top};pickupLevel={pickup}"
        total, detail = score(tuning)
        print(f"{tuning}: {total:.1f}", flush=True)
        if best is None or total < best[0]:
            best = (total, tuning, detail)
    print("best", best[1], round(best[0], 1))
    for row in best[2]:
        print("  note %d distance %.2f: harmonic %d, bloom %.2f s, swell %+.1f dB" % row)


if __name__ == "__main__":
    main()
