"""M8.2: fit the Piano exciter (PianoString) to a real grand across the keyboard.

usage: python tools/fit_piano2.py [iterations] [--all]

Renders the Grand Piano preset at pp, mf and ff (ILANA_NOTE_DEBUG with
ILANA_NOTE_SET=keyboard) on a spread of notes, with the model's constants
(ILANA_PIANO2_TUNING) and a few preset knobs (ILANA_PRESET_OVERRIDES) varied,
and scores them with tools/piano_metrics.py against the reference in
build/reference/piano/SalamanderGrandPiano. Each iteration tries a step up
and down on a random subset of the parameters in parallel, keeps the best
move (and the best combination), and shrinks the steps that didn't help.
Log: build/fit82/log.txt; best set: build/fit82/best.json.
"""
import concurrent.futures
import json
import os
import random
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from piano_metrics import compare  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXE_NAME = "ilanaTableTest.exe" if os.name == "nt" else "ilanaTableTest"
EXE = os.environ.get("ILANA_FIT_EXE", os.path.join(ROOT, "build", "ilanaTableTest_artefacts", "Release", EXE_NAME))
FIT = os.environ.get("ILANA_FIT_DIR", os.path.join(ROOT, "build", "fit82"))
NOTES = [21, 30, 39, 48, 60, 66, 75, 84, 93, 102]
ALL_NOTES = list(range(21, 109, 3))

# name, kind, start, low, high, step (log: multiplicative step)
PARAMETERS = [
    ("massBass", "tuning", 11.0, 4.0, 20.0, 1.5),
    ("massTreble", "tuning", 5.5, 2.0, 12.0, 1.0),
    ("exponentBass", "tuning", 2.3, 1.8, 3.5, 0.2),
    ("exponentTreble", "tuning", 3.0, 2.0, 4.0, 0.25),
    ("feltBass", "tuning", 90.0, 10.0, 600.0, 25.0),
    ("feltTreble", "tuning", 300.0, 30.0, 3000.0, 80.0),
    ("hysteresis", "tuning", 1.0, 0.0, 5.0, 0.5),
    ("speedLow", "tuning", 0.4, 0.1, 2.0, 0.1),
    ("speedHigh", "tuning", 5.0, 2.0, 10.0, 0.6),
    ("impedanceC4", "tuning", 1.6, 0.6, 4.0, 0.3),
    ("impedanceBass", "tuning", 7.0, 2.0, 20.0, 1.2),
    ("strikeBass", "tuning", 0.12, 0.06, 0.2, 0.015),
    ("strikeTreble", "tuning", 0.08, 0.03, 0.16, 0.012),
    ("inharmonicityC4", "tuning", 3.0e-4, 2.0e-5, 1.5e-3, 6.0e-5),
    ("inharmonicityTreble", "tuning", 1.0, 0.2, 1.8, 0.15),
    ("inharmonicityBass", "tuning", 0.4, -0.5, 1.2, 0.15),
    ("t60C4", "tuning", 12.0, 2.0, 40.0, 2.5),
    ("t60Register", "tuning", 0.7, 0.2, 1.4, 0.1),
    ("t60Upper", "tuning", 3.0, 0.3, 12.0, 0.6),
    ("promptRatio", "tuning", 0.25, 0.05, 0.8, 0.06),
    ("aftersound", "tuning", 0.3, 0.0, 1.0, 0.08),
    ("horizontalMix", "tuning", 1.0, 0.1, 3.0, 0.2),
    ("horizontalCents", "tuning", 0.3, -1.5, 1.5, 0.2),
    ("tensionModulation", "tuning", 0.02, 0.0, 0.2, 0.01),
    ("longitudinal", "tuning", 0.05, 0.0, 0.5, 0.03),
    ("longitudinalRatio", "tuning", 17.0, 8.0, 30.0, 2.0),
    ("knock", "tuning", 0.03, 0.0, 0.3, 0.01),
    ("knockFrequency", "tuning", 2500.0, 500.0, 8000.0, 500.0),
    ("knockRegister", "tuning", 0.3, -0.5, 1.0, 0.1),
    ("balance", "tuning", 0.0, -3.0, 3.0, 0.4),
    ("osc1_detune", "preset", 0.6, 0.0, 4.0, 0.25),
    ("osc1_couple", "preset", 0.4, 0.0, 1.0, 0.1),
]


# Phase 2 (--board): the dense soundboard (Grand Piano's sb_model = Dense),
# with the knock and the strings' decay, once the strings are fitted.
BOARD_PARAMETERS = [
    ("boardMix", "tuning", 0.5, 0.0, 3.0, 0.2),
    ("boardDecay", "tuning", 0.08, 0.01, 0.6, 0.03),
    ("boardLowCut", "tuning", 60.0, 20.0, 200.0, 12.0),
    ("boardTilt", "tuning", 0.0, -3.0, 3.0, 0.4),
    ("boardDensity", "tuning", 1.0, 0.0, 3.0, 0.3),
    ("knock", "tuning", 0.03, 0.0, 0.5, 0.02),
    ("knockFrequency", "tuning", 2500.0, 500.0, 8000.0, 500.0),
    ("knockRegister", "tuning", 0.3, -0.5, 1.5, 0.15),
    ("t60Upper", "tuning", 3.0, 0.3, 12.0, 0.5),
    ("t60Register", "tuning", 0.7, 0.2, 1.4, 0.08),
    ("balance", "tuning", 0.0, -3.0, 3.0, 0.3),
    ("sb_mix", "preset", 1.0, 0.0, 1.0, 0.1),
    ("sb_tone", "preset", 0.5, 0.0, 1.0, 0.08),
]
if "--both" in sys.argv:
    PARAMETERS = BOARD_PARAMETERS + [p for p in PARAMETERS if p[0] not in {b[0] for b in BOARD_PARAMETERS}]
    ACTIVE = {p[0] for p in PARAMETERS}
    sys.argv.append("--board")  # same renders (the dense board on) and best file
elif "--both" in sys.argv:
    PARAMETERS = BOARD_PARAMETERS + [p for p in PARAMETERS if p[0] not in {b[0] for b in BOARD_PARAMETERS}]
    ACTIVE = {p[0] for p in PARAMETERS}
    sys.argv.append("--board")  # same renders (the dense board on) and best file
elif "--board" in sys.argv:
    PARAMETERS = BOARD_PARAMETERS + [p for p in PARAMETERS if p[0] not in {b[0] for b in BOARD_PARAMETERS}]
    ACTIVE = {b[0] for b in BOARD_PARAMETERS}
else:
    ACTIVE = {p[0] for p in PARAMETERS}
# Constants every render carries (the board's colour, measured by
# tools/piano_board_eq.py), from build/fit82/extra.txt when present.
EXTRA_PATH = os.path.join(FIT, "extra.txt")


def render(values, tag, notes):
    folder = os.path.join(FIT, tag)
    os.makedirs(folder, exist_ok=True)
    env = dict(os.environ)
    env["ILANA_NOTE_DEBUG"] = "Grand Piano"
    env["ILANA_NOTE_SET"] = "keyboard"
    env["ILANA_NOTE_LIST"] = ",".join(str(n) for n in notes)
    env["ILANA_NOTE_SECONDS"] = "4"
    env["ILANA_NOTE_FOLDER"] = folder
    extra = open(EXTRA_PATH).read().strip() if os.path.exists(EXTRA_PATH) else ""
    env["ILANA_PIANO2_TUNING"] = ";".join([f"{n}={values[n]:.6g}" for n, kind, *_ in PARAMETERS if kind == "tuning"] + ([extra] if extra else []))
    overrides = [f"{n}={values[n]:.6g}" for n, kind, *_ in PARAMETERS if kind == "preset"]
    if "--board" in sys.argv:
        overrides += ["sb_on=1", "sb_model=1"]
    env["ILANA_PRESET_OVERRIDES"] = ";".join(overrides)
    subprocess.run([EXE], env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return folder


def evaluate(args):
    values, tag, notes = args
    try:
        return compare(render(values, tag, notes), notes=notes)
    except Exception as error:  # a broken render scores badly
        print("evaluate failed", tag, error, file=sys.stderr)
        return 1e9


def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else 20
    notes = ALL_NOTES if "--all" in sys.argv else NOTES
    os.makedirs(FIT, exist_ok=True)
    values = {n: start for n, _, start, *_ in PARAMETERS}
    best_path = os.path.join(FIT, "best-board.json" if "--board" in sys.argv else "best.json")
    if "--board" in sys.argv and not os.path.exists(best_path) and os.path.exists(os.path.join(FIT, "best.json")):
        values.update({k: v for k, v in json.load(open(os.path.join(FIT, "best.json"))).items() if k in values})
    if os.path.exists(best_path):
        values.update({k: v for k, v in json.load(open(best_path)).items() if k in values})
    steps = {n: step for n, _, _, _, _, step in PARAMETERS}
    bounds = {n: (low, high) for n, _, _, low, high, _ in PARAMETERS}
    log = open(os.path.join(FIT, "log.txt"), "a")

    current = evaluate((values, "base", notes))
    _, detail, registers, _ = compare(os.path.join(FIT, "base"), notes=notes, detail=True)
    print(f"start error {current:.1f} {detail} {registers}", file=log, flush=True)

    workers = max(2, (os.cpu_count() or 4) - 1)
    for a in sys.argv:
        if a.startswith("--workers="):
            workers = max(1, int(a.split("=", 1)[1]))
    rng = random.Random(82)
    with concurrent.futures.ProcessPoolExecutor(workers) as pool:
        for iteration in range(iterations):
            names = [n for n, *_ in PARAMETERS if steps[n] > 1e-9 and n in ACTIVE]
            chosen = rng.sample(names, min(len(names), 3 * workers))
            candidates = []
            for name in chosen:
                for sign in (1, -1):
                    trial = dict(values)
                    low, high = bounds[name]
                    trial[name] = float(min(high, max(low, values[name] + sign * steps[name])))
                    if trial[name] != values[name]:
                        candidates.append((name, trial))
            scores = list(pool.map(evaluate, [(t, f"c{i}", notes) for i, (_, t) in enumerate(candidates)]))
            improving = sorted([(s, c) for s, c in zip(scores, candidates) if s < current], key=lambda item: item[0])
            moved = set()
            if improving:
                options = [(improving[0][0], dict(improving[0][1][1]))]
                combos = []
                for count in sorted({2, 4, len(improving)}):
                    if 1 < count <= len(improving):
                        combined = dict(values)
                        used = set()
                        for _, (name, trial) in improving[:count]:
                            if name not in used:
                                combined[name] = trial[name]
                                used.add(name)
                        combos.append(combined)
                combo_scores = list(pool.map(evaluate, [(c, f"combo{i}", notes) for i, c in enumerate(combos)]))
                options += list(zip(combo_scores, combos))
                current, values = min(options, key=lambda item: item[0])
                values = dict(values)
                moved = {c[0] for _, c in improving}
            for name in chosen:
                if name not in moved:
                    steps[name] *= 0.7
                else:
                    steps[name] *= 1.1
            json.dump(values, open(best_path, "w"), indent=1)
            print(f"iteration {iteration}: error {current:.1f} moved {sorted(moved)}", file=log, flush=True)

    evaluate((values, "base", notes))
    _, detail, registers, _ = compare(os.path.join(FIT, "base"), notes=notes, detail=True)
    print(f"final error {current:.1f} {detail} {registers}", file=log, flush=True)


if __name__ == "__main__":
    main()
