"""M8.5: fit the Feedback exciter's loop (FeedbackGuitarTuning.h).

usage: python tools/fit_feedback.py

No guitar-feedback recording was reachable from the cloud session, so the
reference is the usual description of amp feedback on a sustained note:
the plucked note holds instead of dying, and after about one to two
seconds blooms into one of its low harmonics (the fundamental, the octave
or the twelfth), whichever comes back in phase at that distance from the
amp. Scored over five notes at the default distance (0.5, about 1.1 m) and
at a closer and a farther one: a harmonic above the 3rd costs 10, a bloom
outside 0.8-3 s costs (log(t / 1.5))^2 x 4, a note that dies costs 5.
Grid search over the loop gain, the speaker's top and the pickup level.
"""
import itertools
import os
import re
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE = os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", "ilanaTableTest")
LINE = re.compile(r"note (\d+) distance ([\d.]+): harmonic (\d+) blooms at ([\d.]+) s, final ([-\d.]+) dB \(pluck ([-\d.]+)\)")


def score(tuning):
    env = dict(os.environ, ILANA_FEEDBACK_PROBE="1", ILANA_FEEDBACK_TUNING=tuning)
    out = subprocess.run([EXE], env=env, capture_output=True, text=True, cwd=ROOT).stdout
    total, detail = 0.0, []
    import math
    for m in LINE.finditer(out):
        note, distance, harmonic, bloom, final, pluck = int(m[1]), float(m[2]), int(m[3]), float(m[4]), float(m[5]), float(m[6])
        if distance not in (0.25, 0.5, 0.75):
            continue
        s = 0.0
        if harmonic > 3:
            s += 10.0
        if final < pluck - 6.0:
            s += 5.0
        s += 4.0 * math.log(max(bloom, 0.05) / 1.5) ** 2 if not (0.8 <= bloom <= 3.0) else 0.0
        total += s
        detail.append((note, distance, harmonic, bloom))
    return total, detail


def main():
    best = None
    for loop, top, pickup in itertools.product([0.005, 0.008, 0.011], [700.0, 1000.0, 1400.0, 2000.0], [1.0, 2.0]):
        tuning = f"loopGain={loop};speakerHigh={top};pickupLevel={pickup}"
        total, detail = score(tuning)
        print(f"{tuning}: {total:.1f}", flush=True)
        if best is None or total < best[0]:
            best = (total, tuning, detail)
    print("best", best[1], best[0])
    for row in best[2]:
        print("  note %d distance %.2f: harmonic %d, bloom %.2f s" % row)


if __name__ == "__main__":
    main()
