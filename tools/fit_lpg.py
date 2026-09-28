"""M8.3: fit the low-pass gate (WestCoastTuning.h) to recorded strikes.

usage: python tools/fit_lpg.py [iterations]

Reference: struck Make Noise Optomix gates (a vactrol low-pass gate of the
Buchla 292 family) from freesound, in build/reference/lpg/ (see
build/reference/SOURCES.md). They are performance excerpts with a backing
mix, so only the two clean low strikes (01, 02: tom-like thumps) are used,
and only what survives the noise floor: how long a strike takes to fall
10, 20 and 30 dB from its peak (tools/lpg_metrics.py). Their centroids
wander with the backing mix and are not fitted; the model's darkening as
it closes stays as the M8.3 tests check it.

The model is rendered by the test binary (ILANA_LPG_PROBE, a plain saw at
A2 through WEST) with ILANA_WEST_TUNING overrides; a coordinate search
over the vactrol's fall time and slowing and the gain curve minimises the
squared log ratios of the three fall times.

The first version (before recordings were reachable) fitted the published
vactrol description instead: 63 % dark at about 250 ms, 90 % at about 1 s.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from lpg_metrics import metrics  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE_NAME = "ilanaTableTest.exe" if os.name == "nt" else "ilanaTableTest"
EXE = os.environ.get("ILANA_FIT_EXE", os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", EXE_NAME))
REFERENCES = ["lpg_01_optomix_tomtom_a.wav", "lpg_02_optomix_tomtom_b.wav"]
KEYS = ["fall10", "fall20", "fall30"]


def target():
    values = [metrics(os.path.join(ROOT, "build", "reference", "lpg", name)) for name in REFERENCES]
    return {k: float(np.exp(np.mean([np.log(v[k]) for v in values]))) for k in KEYS}


def ours(tuning):
    folder = tempfile.mkdtemp(prefix="lpg")
    env = dict(os.environ, ILANA_LPG_PROBE=folder, ILANA_WEST_TUNING=";".join(f"{k}={v:.6g}" for k, v in tuning.items()))
    subprocess.run([EXE], env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return metrics(os.path.join(folder, "strike.45.wav"))


def error(measured, goal):
    return sum(np.log(max(measured[k], 1e-3) / goal[k]) ** 2 for k in KEYS)


def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    goal = target()
    print("reference fall times (ms):", {k: round(v * 1000) for k, v in goal.items()})
    values = {"fallSeconds": 0.325, "fallSlowing": 2.4, "gainCurve": 2.5}
    steps = {"fallSeconds": 0.08, "fallSlowing": 0.6, "gainCurve": 0.6}
    bounds = {"fallSeconds": (0.03, 1.0), "fallSlowing": (0.0, 5.0), "gainCurve": (1.0, 5.0)}
    current = error(ours(values), goal)
    print(f"start {values} error {current:.4f}", flush=True)
    for iteration in range(iterations):
        moved = False
        for name in values:
            for sign in (1, -1):
                trial = dict(values)
                trial[name] = float(np.clip(values[name] + sign * steps[name], *bounds[name]))
                score = error(ours(trial), goal)
                if score < current:
                    current, values, moved = score, trial, True
                    break
            else:
                steps[name] *= 0.6
        print(f"iteration {iteration}: {values} error {current:.4f}", flush=True)
        if not moved and max(steps[k] / (bounds[k][1] - bounds[k][0]) for k in steps) < 0.01:
            break
    m = ours(values)
    print("best:", ";".join(f"{k}={v:.4g}" for k, v in values.items()))
    print("ours (ms):", {k: round(m[k] * 1000) for k in KEYS}, "reference (ms):", {k: round(v * 1000) for k, v in goal.items()})


if __name__ == "__main__":
    main()
