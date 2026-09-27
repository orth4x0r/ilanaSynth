"""Fit the piano model to real grand piano recordings.

usage: python tools/fit_piano.py [iterations]

Renders the Hammered Strings preset (E1, C4, C7 at mf and ff) through
ilanaTableTest (ILANA_NOTE_DEBUG), with engine constants
(ILANA_PIANO_TUNING) and preset parameters (ILANA_PRESET_OVERRIDES) varied,
and compares with the University of Iowa MIS notes in build/reference/:
onset spectrum, the decay of every partial over 4 s, between-partial noise,
the mf -> ff gain and the balance between registers. A parallel coordinate
search keeps whatever lowers the error. Progress goes to build/fit/log.txt;
the best set so far to build/fit/best.txt.
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
EXE = os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", "ilanaTableTest.exe")
FIT = os.path.join(ROOT, "build", "fit")
NOTES = [("E1", 28, 12), ("C4", 60, 12), ("C7", 96, 4)]
DYNAMICS = ["mf", "ff"]
TIMES = [0.03, 0.25, 0.5, 1.0, 2.0, 3.5]
SIZE = 1 << 14

# name, kind ("tuning" or "preset"), start, low, high, step
PARAMETERS = [
    ("contactBase", "tuning", 0.4, 0.1, 2.0, 0.15),
    ("contactSlope", "tuning", 2.0, 0.2, 5.0, 0.4),
    ("contactRegister", "tuning", 0.35, 0.0, 0.8, 0.08),
    ("trebleContact", "tuning", 1.0, 0.5, 1.5, 0.1),
    ("shapeHardness", "tuning", 1.5, 0.0, 4.0, 0.4),
    ("strikeReflection", "tuning", 0.2, 0.0, 0.9, 0.1),
    ("strikePosition", "tuning", 0.125, 0.06, 0.25, 0.02),
    ("velocityCurve", "tuning", 1.3, 0.6, 5.0, 0.4),
    ("t60Register", "tuning", 0.6, 0.2, 1.2, 0.08),
    ("lossTop", "tuning", 60.0, 5.0, 200.0, 15.0),
    ("coupling", "tuning", 0.015, 0.0, 0.05, 0.004),
    ("promptRatio", "tuning", 0.15, 0.03, 0.6, 0.06),
    ("aftersound", "tuning", 0.4, 0.0, 1.0, 0.1),
    ("radiationCut", "tuning", 70.0, 30.0, 150.0, 10.0),
    ("bumpFrequency", "tuning", 260.0, 120.0, 600.0, 40.0),
    ("bumpGain", "tuning", 5.0, -3.0, 12.0, 1.5),
    ("bumpQ", "tuning", 0.8, 0.3, 3.0, 0.2),
    ("knock", "tuning", 0.015, 0.0, 0.08, 0.008),
    ("thump", "tuning", 1.0, 0.0, 3.0, 0.3),
    ("osc1_string_decay", "preset", 0.9, 0.5, 1.0, 0.03),
    ("osc1_string_damp", "preset", 0.25, 0.0, 1.0, 0.06),
    ("osc1_string_stiffness", "preset", 0.45, 0.0, 0.9, 0.05),
    ("osc1_hammer_hard", "preset", 0.5, 0.0, 1.0, 0.08),
    ("osc1_couple", "preset", 0.6, 0.0, 1.0, 0.1),
    ("osc1_detune", "preset", 1.5, 0.0, 6.0, 0.5),
    ("osc1_register", "preset", 0.8, 0.0, 1.0, 0.1),
    ("sb_mix", "preset", 0.45, 0.0, 1.0, 0.08),
    ("sb_tone", "preset", 0.6, 0.0, 1.0, 0.08),
    ("sb_size", "preset", 0.6, 0.0, 1.0, 0.1),
    ("amp_velocity", "preset", 0.35, 0.0, 1.0, 0.08),
]


def find_f0(x, rate, nominal):
    """Best f0 within +/-70 cents: most energy in partials 2..6."""
    frame = x[int(0.1 * rate):int(0.1 * rate) + SIZE * 2]
    spectrum = np.abs(np.fft.rfft(frame * np.hanning(len(frame))))
    freqs = np.fft.rfftfreq(len(frame), 1 / rate)
    best, best_score = nominal, -1.0
    for cents in np.arange(-70, 71, 2):
        f0 = nominal * 2 ** (cents / 1200)
        score = 0.0
        for n in range(2, 7):
            k = int(round(n * f0 / (freqs[1])))
            if k + 2 < len(spectrum):
                score += spectrum[k - 2:k + 3].max()
        if score > best_score:
            best, best_score = f0, score
    return best


def features(path, midi, partials):
    x, rate = load(path)
    f0 = find_f0(x, rate, 440.0 * 2 ** ((midi - 69) / 12))
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
            stretch = np.sqrt(1 + 3e-4 * n * n) if midi < 90 else 1.0
            lo, hi = n * f0 * (0.985 if n > 1 else 0.97), n * f0 * max(1.03, stretch * 1.02)
            band = (freqs >= lo) & (freqs <= hi)
            row.append(20 * np.log10(spectrum[band].max()))
            gap = (freqs > n * f0 * 1.3) & (freqs < n * f0 * 1.7)
            gaps.append(np.mean(spectrum[gap] ** 2))
        levels.append(row)
        noise.append(10 * np.log10(np.mean(gaps)))
    levels = np.array(levels)
    loud = 20 * np.log10(np.sqrt(np.mean(x[:int(0.3 * rate)] ** 2)) + 1e-9)
    return {"levels": levels, "noise": np.array(noise), "loud": loud}


def reference():
    out = {}
    for name, midi, partials in NOTES:
        for dyn in DYNAMICS:
            out[(name, dyn)] = features(os.path.join(ROOT, "build", "reference", f"Piano.{dyn}.{name}.aiff"), midi, partials)
    return out


def error(ours, ref, detail=False):
    parts = {"onset": [], "decay": [], "noise": [], "dynamics": [], "balance": []}
    for name, midi, partials in NOTES:
        for dyn in DYNAMICS:
            a, b = ours[(name, dyn)], ref[(name, dyn)]
            peak_a, peak_b = a["levels"][0].max(), b["levels"][0].max()
            onset_a = np.maximum(a["levels"][0] - peak_a, -60)
            onset_b = np.maximum(b["levels"][0] - peak_b, -60)
            parts["onset"].append(np.mean((onset_a - onset_b) ** 2))
            # Decay of each partial relative to its onset (only where the
            # real partial is audible), clipped at the noise floor.
            audible = onset_b > -45
            decay_a = np.maximum(a["levels"][1:] - a["levels"][0], -50)[:, audible]
            decay_b = np.maximum(b["levels"][1:] - b["levels"][0], -50)[:, audible]
            parts["decay"].append(np.mean((decay_a - decay_b) ** 2))
            # Only the attack: later the recording's own hiss is the floor.
            parts["noise"].append(np.mean(((a["noise"][:2] - peak_a) - (b["noise"][:2] - peak_b)).clip(-40, 40) ** 2))
        dyn_a = ours[(name, "ff")]["loud"] - ours[(name, "mf")]["loud"]
        dyn_b = ref[(name, "ff")]["loud"] - ref[(name, "mf")]["loud"]
        parts["dynamics"].append((dyn_a - dyn_b) ** 2)
        bal_a = ours[(name, "mf")]["loud"] - ours[("C4", "mf")]["loud"]
        bal_b = ref[(name, "mf")]["loud"] - ref[("C4", "mf")]["loud"]
        parts["balance"].append((bal_a - bal_b) ** 2)
    weights = {"onset": 1.0, "decay": 1.0, "noise": 0.3, "dynamics": 2.0, "balance": 1.0}
    total = sum(weights[k] * float(np.mean(v)) for k, v in parts.items())
    if detail:
        return total, {k: round(float(np.mean(v)), 2) for k, v in parts.items()}
    return total


def render(values, tag):
    folder = os.path.join(FIT, tag)
    os.makedirs(folder, exist_ok=True)
    env = dict(os.environ)
    env["ILANA_NOTE_DEBUG"] = "Hammered Strings"
    env["ILANA_NOTE_FOLDER"] = folder
    env["ILANA_PIANO_TUNING"] = ";".join(f"{n}={values[n]:.6g}" for n, kind, *_ in PARAMETERS if kind == "tuning")
    env["ILANA_PRESET_OVERRIDES"] = ";".join(f"{n}={values[n]:.6g}" for n, kind, *_ in PARAMETERS if kind == "preset")
    subprocess.run([EXE], env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return {(name, dyn): features(os.path.join(folder, f"ours.{dyn}.{name}.wav"), midi, partials)
            for name, midi, partials in NOTES for dyn in DYNAMICS}


def evaluate(args):
    values, tag, ref = args
    return error(render(values, tag), ref)


def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 12
    os.makedirs(FIT, exist_ok=True)
    ref = reference()
    values = {n: start for n, _, start, *_ in PARAMETERS}
    best_path = os.path.join(FIT, "best.txt")
    if os.path.exists(best_path):
        values.update(json.load(open(best_path)))
    steps = {n: step for n, _, _, _, _, step in PARAMETERS}
    bounds = {n: (low, high) for n, _, _, low, high, _ in PARAMETERS}
    log = open(os.path.join(FIT, "log.txt"), "a")

    current, detail = error(render(values, "base"), ref, detail=True)
    print(f"start error {current:.2f} {detail}", file=log, flush=True)

    workers = max(2, (os.cpu_count() or 4) - 2)
    with concurrent.futures.ProcessPoolExecutor(workers) as pool:
        for iteration in range(iterations):
            candidates = []
            for name, *_ in PARAMETERS:
                for sign in (1, -1):
                    trial = dict(values)
                    trial[name] = float(np.clip(values[name] + sign * steps[name], *bounds[name]))
                    if trial[name] != values[name]:
                        candidates.append((name, sign, trial))
            scores = list(pool.map(evaluate, [(t, f"c{i}", ref) for i, (_, _, t) in enumerate(candidates)]))
            improving = [(s, c) for s, c in zip(scores, candidates) if s < current]
            improving.sort(key=lambda item: item[0])
            if improving:
                # Combine the best 2, 5 and all improving moves (in
                # parallel), and keep whichever beats the single best move.
                options = [(improving[0][0], dict(improving[0][1][2]))]
                combos = []
                for count in sorted({2, 5, len(improving)}):
                    if count > 1 and count <= len(improving):
                        combined = dict(values)
                        for _, (name, _, trial) in improving[:count]:
                            combined[name] = trial[name]
                        combos.append(combined)
                combo_scores = list(pool.map(evaluate, [(c, f"combo{i}", ref) for i, c in enumerate(combos)]))
                options += list(zip(combo_scores, combos))
                current, values = min(options, key=lambda item: item[0])
                values = dict(values)
                moved = {c[0] for _, c in improving}
            else:
                moved = set()
            for name, *_ in PARAMETERS:
                if name not in moved:
                    steps[name] *= 0.6
            _, detail = error(render(values, "base"), ref, detail=True)
            print(f"iteration {iteration}: error {current:.2f} {detail} moved {sorted(moved)}", file=log, flush=True)
            json.dump(values, open(best_path, "w"), indent=1)


if __name__ == "__main__":
    main()
