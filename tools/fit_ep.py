"""Fit the electric piano models (Tine, Reed) to their references.

usage: python tools/fit_ep.py tine|reed [iterations]

Renders the Tine Keys or Reed Keys preset (MIDI 40, 60, 84 at velocity 40
and 120, released after 4 s) through ilanaTableTest (ILANA_NOTE_DEBUG with
ILANA_NOTE_SET=ep), varying the model constants (ILANA_EP_TUNING), and
compares with the default sounds of Stage-73 V2 (tine) and Wurli V2 (reed),
rendered by ilanaRefHost into build/reference/ep/: onset spectrum, the decay
of every partial, the bark or growl (how much the upper partials rise from
soft to hard), the soft -> hard gain and the balance between registers. A
parallel coordinate search keeps whatever lowers the error. Progress goes to
build/fit-ep/<model>/log.txt, the best set to best.txt there (copy it into
src/dsp/EpTuning.h).
"""
import concurrent.futures
import json
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from fit_model import load  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE = os.environ.get("ILANA_FIT_EXE") or os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", "ilanaTableTest.exe")
NOTES = [(40, 12), (60, 10), (84, 5)]
VELOCITIES = [40, 120]
TIMES = [0.03, 0.25, 0.5, 1.0, 2.0, 3.5]
SIZE = 1 << 13

MODELS = {
    "tine": {"preset": "Tine Keys", "reference": "rhodes"},
    "reed": {"preset": "Reed Keys", "reference": "wurli"},
}

# name, low, high, step
COMMON = [
    ("contactMs", 0.3, 12.0, 0.6),
    ("contactVelocity", 0.0, 1.8, 0.2),
    ("contactRegister", 0.0, 1.2, 0.12),
    ("ampLow", 0.1, 6.0, 0.25),
    ("ampHigh", 0.05, 4.0, 0.15),
    ("velocityFloor", 0.0, 0.6, 0.04),
    ("velocityCurve", 0.5, 4.0, 0.25),
    ("offset", -1.0, 1.5, 0.1),
    ("offsetRegister", -1.0, 1.0, 0.1),
    ("t60", 1.0, 20.0, 1.0),
    ("t60Register", 0.0, 1.5, 0.1),
    ("barLevel", 0.0, 1.0, 0.08),
    ("h2Level", 0.0, 0.8, 0.04),
    ("h2T60", 0.05, 2.0, 0.15),
    ("beam2Level", 0.0, 2.0, 0.1),
    ("beam2T60", 0.02, 0.6, 0.1),   # the tine partial dies long before the note
    ("beam3Level", 0.0, 1.0, 0.05),
    ("beam3T60", 0.01, 0.2, 0.04),
    ("toneCut", 800.0, 20000.0, 800.0),
    ("gainTilt", -12.0, 12.0, 1.0),
]
REED = [
    ("highPass", 20.0, 1500.0, 60.0),
    ("drive", 0.0, 6.0, 0.4),
    ("bias", -1.0, 1.0, 0.1),
]
# The header's starting values.
DEFAULTS = {
    "tine": {"contactMs": 2.0, "contactVelocity": 0.8, "contactRegister": 0.35, "ampLow": 1.2, "ampHigh": 0.6,
             "velocityFloor": 0.08, "velocityCurve": 1.4, "offset": 0.55, "offsetRegister": 0.0, "t60": 6.0, "t60Register": 0.5,
             "barLevel": 0.15, "h2Level": 0.05, "h2T60": 0.5, "beam2Level": 0.25, "beam2T60": 0.4,
             "beam3Level": 0.1, "beam3T60": 0.12, "toneCut": 6000.0, "gainTilt": 0.0},
    "reed": {"contactMs": 3.0, "contactVelocity": 0.8, "contactRegister": 0.35, "ampLow": 0.5, "ampHigh": 0.3,
             "velocityFloor": 0.08, "velocityCurve": 1.4, "offset": 0.2, "offsetRegister": 0.0, "t60": 4.0, "t60Register": 0.5,
             "barLevel": 0.05, "h2Level": 0.05, "h2T60": 0.5, "beam2Level": 0.15, "beam2T60": 0.4,
             "beam3Level": 0.1, "beam3T60": 0.12, "toneCut": 5000.0, "gainTilt": 0.0,
             "highPass": 250.0, "drive": 1.0, "bias": 0.2},
}


def parameters(model):
    return COMMON + (REED if model == "reed" else [])


def features(path, midi, partials):
    x, rate = load(path)
    f0 = 440.0 * 2 ** ((midi - 69) / 12)
    window = np.hanning(SIZE)
    freqs = np.fft.rfftfreq(SIZE, 1 / rate)
    levels, noise = [], []
    for t in TIMES:
        frame = x[int(t * rate):int(t * rate) + SIZE]
        if len(frame) < SIZE:
            frame = np.pad(frame, (0, SIZE - len(frame)))
        spectrum = np.abs(np.fft.rfft(frame * window)) + 1e-9
        row, gaps = [], []
        for n in range(1, partials + 1):
            band = (freqs >= n * f0 * 0.97) & (freqs <= n * f0 * 1.03)
            row.append(20 * np.log10(spectrum[band].max()))
            gap = (freqs > n * f0 * 1.3) & (freqs < n * f0 * 1.7)
            gaps.append(np.mean(spectrum[gap] ** 2))
        levels.append(row)
        noise.append(10 * np.log10(np.mean(gaps)))
    levels = np.array(levels)
    loud = 20 * np.log10(np.sqrt(np.mean(x[:int(0.3 * rate)] ** 2)) + 1e-9)
    return {"levels": levels, "noise": np.array(noise), "loud": loud}


def reference(model):
    name = MODELS[model]["reference"]
    folder = os.path.join(ROOT, "build", "reference", "ep")
    return {(midi, v): features(os.path.join(folder, f"{name}.v{v}.n{midi}.wav"), midi, partials)
            for midi, partials in NOTES for v in VELOCITIES}


def upper(levels, peak):
    """Power mean of partials 2.. at the onset, relative to the peak (dB)."""
    rel = levels[0][1:] - peak
    return 10 * np.log10(np.mean(10 ** (rel / 10)) + 1e-12)


def error(ours, ref, detail=False):
    parts = {"onset": [], "decay": [], "noise": [], "bark": [], "dynamics": [], "balance": []}
    for midi, _ in NOTES:
        for v in VELOCITIES:
            a, b = ours[(midi, v)], ref[(midi, v)]
            peak_a, peak_b = a["levels"][0].max(), b["levels"][0].max()
            onset_a = np.maximum(a["levels"][0] - peak_a, -60)
            onset_b = np.maximum(b["levels"][0] - peak_b, -60)
            parts["onset"].append(np.mean((onset_a - onset_b) ** 2))
            audible = onset_b > -50
            decay_a = np.maximum(a["levels"][1:] - a["levels"][0], -60)[:, audible]
            decay_b = np.maximum(b["levels"][1:] - b["levels"][0], -60)[:, audible]
            parts["decay"].append(np.mean((decay_a - decay_b) ** 2))
            parts["noise"].append(np.mean(((a["noise"][:2] - peak_a) - (b["noise"][:2] - peak_b)).clip(-40, 40) ** 2))
        soft_a, hard_a = ours[(midi, 40)], ours[(midi, 120)]
        soft_b, hard_b = ref[(midi, 40)], ref[(midi, 120)]
        bark_a = upper(hard_a["levels"], hard_a["levels"][0].max()) - upper(soft_a["levels"], soft_a["levels"][0].max())
        bark_b = upper(hard_b["levels"], hard_b["levels"][0].max()) - upper(soft_b["levels"], soft_b["levels"][0].max())
        parts["bark"].append((bark_a - bark_b) ** 2)
        parts["dynamics"].append(((hard_a["loud"] - soft_a["loud"]) - (hard_b["loud"] - soft_b["loud"])) ** 2)
        for v in VELOCITIES:
            bal_a = ours[(midi, v)]["loud"] - ours[(60, v)]["loud"]
            bal_b = ref[(midi, v)]["loud"] - ref[(60, v)]["loud"]
            parts["balance"].append((bal_a - bal_b) ** 2)
    weights = {"onset": 1.0, "decay": 1.0, "noise": 0.2, "bark": 2.0, "dynamics": 2.0, "balance": 1.0}
    total = sum(weights[k] * float(np.mean(v)) for k, v in parts.items())
    if detail:
        return total, {k: round(float(np.mean(v)), 2) for k, v in parts.items()}
    return total


def render(model, values, tag):
    folder = os.path.join(ROOT, "build", "fit-ep", model, tag)
    os.makedirs(folder, exist_ok=True)
    env = dict(os.environ)
    env["ILANA_NOTE_DEBUG"] = MODELS[model]["preset"]
    env["ILANA_NOTE_SET"] = "ep"
    env["ILANA_NOTE_FOLDER"] = folder
    env["ILANA_EP_TUNING"] = ";".join(f"{model}.{n}={values[n]:.6g}" for n, *_ in parameters(model))
    subprocess.run([EXE], env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return {(midi, v): features(os.path.join(folder, f"ours.v{v}.n{midi}.wav"), midi, partials)
            for midi, partials in NOTES for v in VELOCITIES}


def evaluate(args):
    model, values, tag, ref = args
    return error(render(model, values, tag), ref)


def main():
    model = sys.argv[1] if len(sys.argv) > 1 else "tine"
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    params = parameters(model)
    fit = os.path.join(ROOT, "build", "fit-ep", model)
    os.makedirs(fit, exist_ok=True)
    ref = reference(model)
    values = dict(DEFAULTS[model])
    best_path = os.path.join(fit, "best.txt")
    if os.path.exists(best_path):
        values.update(json.load(open(best_path)))
    bounds_now = {n: (low, high) for n, low, high, _ in parameters(model)}
    values = {n: float(np.clip(v, *bounds_now[n])) if n in bounds_now else v for n, v in values.items()}
    steps = {n: step for n, _, _, step in params}
    bounds = {n: (low, high) for n, low, high, _ in params}
    log = open(os.path.join(fit, "log.txt"), "a")

    current, detail = error(render(model, values, "base"), ref, detail=True)
    print(f"start error {current:.2f} {detail}", file=log, flush=True)
    print(f"start error {current:.2f} {detail}", flush=True)

    workers = max(2, (os.cpu_count() or 4) - 2)
    with concurrent.futures.ProcessPoolExecutor(workers) as pool:
        for iteration in range(iterations):
            candidates = []
            for name, *_ in params:
                for sign in (1, -1):
                    trial = dict(values)
                    trial[name] = float(np.clip(values[name] + sign * steps[name], *bounds[name]))
                    if trial[name] != values[name]:
                        candidates.append((name, sign, trial))
            scores = list(pool.map(evaluate, [(model, t, f"c{i}", ref) for i, (_, _, t) in enumerate(candidates)]))
            improving = [(s, c) for s, c in zip(scores, candidates) if s < current]
            improving.sort(key=lambda item: item[0])
            moved = set()
            if improving:
                options = [(improving[0][0], dict(improving[0][1][2]))]
                combos = []
                for count in sorted({2, 5, len(improving)}):
                    if 1 < count <= len(improving):
                        combined = dict(values)
                        for _, (name, _, trial) in improving[:count]:
                            combined[name] = trial[name]
                        combos.append(combined)
                combo_scores = list(pool.map(evaluate, [(model, c, f"combo{i}", ref) for i, c in enumerate(combos)]))
                options += list(zip(combo_scores, combos))
                current, values = min(options, key=lambda item: item[0])
                values = dict(values)
                moved = {c[0] for _, c in improving}
            for name, *_ in params:
                if name not in moved:
                    steps[name] *= 0.6
            _, detail = error(render(model, values, "base"), ref, detail=True)
            line = f"iteration {iteration}: error {current:.2f} {detail} moved {sorted(moved)}"
            print(line, file=log, flush=True)
            print(line, flush=True)
            json.dump(values, open(best_path, "w"), indent=1)


if __name__ == "__main__":
    main()
