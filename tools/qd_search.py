#!/usr/bin/env python3
"""Quality-diversity preset search (MAP-Elites): grow presets from the ones ilana kept, one elite per kind of sound.

    python tools/qd_search.py run [--gens 6] [--per 40] [--seed 1]    evolve; archive in build/gen/qd/archive.json
    python tools/qd_search.py status                                    cells filled per category

Parents: the candidate-pack presets she kept (and her own presets). Each generation mutates elites of a category
(audible-parameter jitter, wavetable swap, effect swap, filter type swap, crossover of a parameter group with another
elite), renders the offspring with ilanaPresetRender and measures them with tools/preset_critic.py. A preset lives in a
cell of a grid of measured traits (brightness, attack, length, movement, width); it takes the cell if the cell is empty or
if it scores higher and passes the gate. Two presets can never share a cell, so the archive is diverse by construction.
"""

import argparse
import csv
import glob
import json
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import preset_gen as pg  # noqa: E402
import taste_round9_draft as t9  # noqa: E402
from taste_round3 import passes  # noqa: E402

GEN = ROOT / "build" / "gen"
QD = GEN / "qd"
RENDER = ROOT / "build" / "ilanaPresetRender_artefacts" / "Release" / "ilanaPresetRender"
PACK = Path("/mnt/project-files/ilanasynth/candidate-pack")
KEPT = ["acid-low-1", "crazy-snap-1", "drop-pluck-1", "fm-brass-2", "grit-bass-1", "hammer-snap-1", "koto-snap-1",
        "koto-snap-2", "pad-pluck-1", "reese-sub-1", "slow-choir-1", "snap-pluck-1", "strum-snap-1", "vangelis-brass-2"]
CATEGORIES = ["Lead", "Pad", "Pluck", "Bass", "Keys"]

def table_names():
    for line in (GEN / "defaults.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("#choice osc1_table="):
            return line.split("=", 1)[1].split("|")
    sys.exit("defaults.txt has no osc1_table choices")


NAMES = [n for n in table_names()]
ALL = [i for i, n in enumerate(NAMES) if not n.startswith("User")]
FAMILY = {
    "saw": [i for i, n in enumerate(NAMES) if "Saw" in n and "To" not in n],
    "pwm": [i for i, n in enumerate(NAMES) if any(w in n for w in ("PWM", "Pulse", "Square"))],
    "vowel": [i for i, n in enumerate(NAMES) if any(w in n for w in ("Vowel", "Choir", "Formant", "Throat", "Talkbox", "Diphthong",
                                                                      "Nasal", "Yeah", "Whisper"))],
}
POOLS = {"f1_type": [0, 1, 3, 4, 6, 8, 10, 11, 14, 15, 18, 19, 25, 29, 32, 35],
         "fx": [2, 7, 9, 11, 13, 15, 16, 20, 27, 32, 33, 34, 35, 36, 40, 41]}
VOWEL_FILTERS = [10, 22, 23, 27]  # Formant, Vowel, Talking, Vowel Morph
LFOS = [(1, 1), (2, 2), (20, 3), (21, 4)]  # (modulation source index, LFO number)
GROUPS = [("osc2_", "osc3_", "osc4_"), ("f1_", "f2_", "filters_"), ("fx_",), ("amp_", "fenv_", "env2_")]


def cell(row):
    """The grid cell of a measured preset: brightness x attack x length x movement x width."""
    f = lambda k: float(row.get(k) or 0)  # noqa: E731
    c = f("centroid_hz")
    return "/".join(map(str, (0 if c < 600 else 1 if c < 2500 else 2, int(f("attack_s") >= 0.03),
                              int(f("sustain_db") > -14), int(f("movement") >= 0.8), int(f("width") >= 0.35))))


def render(folder, out, keep=False):
    """Render every preset in FOLDER into OUT (chunks of 24 in separate processes: the renderer dies on big batches)."""
    files = sorted(Path(folder).glob("*.ilanapreset"))
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir(parents=True)
    rows, n = [], 0
    def run_chunk(batch):
        chunk = out.parent / f"{out.name}-chunk"
        shutil.rmtree(chunk, ignore_errors=True)
        (chunk / "in").mkdir(parents=True)
        for f in batch:
            shutil.copy(f, chunk / "in" / f.name)
        env = dict(os.environ, ILANA_RENDER_FILES=str(chunk / "in"))
        done = subprocess.run(["xvfb-run", "-a", str(RENDER), str(chunk / "out")], env=env,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        return chunk, done

    def collect(chunk):
        nonlocal n
        for row in csv.DictReader(open(chunk / "out" / "index.csv", newline="", encoding="utf-8")):
            shutil.copytree(chunk / "out" / row["index"], out / str(n))
            row["index"] = str(n)
            rows.append(row)
            n += 1
        shutil.rmtree(chunk)

    for c in range(0, len(files), 24):
        batch = files[c:c + 24]
        chunk, ok = run_chunk(batch)
        if ok:
            collect(chunk)
            continue
        for f in batch:  # a preset crashed the renderer: render one by one and skip the crashers
            chunk, ok = run_chunk([f])
            if ok:
                collect(chunk)
            else:
                print("renderer crashed on", f.name, flush=True)
    with open(out / "index.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    subprocess.run([sys.executable, str(ROOT / "tools" / "preset_critic.py"), str(out)], check=True, stdout=subprocess.DEVNULL)
    rep = {r["name"]: r for r in csv.DictReader(open(out / "report.csv", newline="", encoding="utf-8"))}
    for sub in ([] if keep else out.iterdir()):  # the wavs are not needed after measuring: the archive keeps the presets
        if sub.is_dir():
            shutil.rmtree(sub)
    return rep


def gate(row):
    return passes(row, {row["index"]: row})


def free_slot(params):
    for slot in range(1, pg.MOD["slots"] + 1):
        if round(params.get(f"mod{slot}_src", 0)) == 0 and f"mod{slot}_src" in params:
            return slot
    return None


def route(params, source, dest_name, amount):
    slot, dest = free_slot(params), pg.MOD["dests"].get(dest_name)
    if slot is None or dest is None:
        return False
    params[f"mod{slot}_src"], params[f"mod{slot}_dst"], params[f"mod{slot}_amt"] = float(source), float(dest), amount
    return True


def morph(params, dest_name, rng):
    """Slowly moves DEST_NAME: with a free LFO (slow rate, smooth shape) or the modulation envelope."""
    used = {round(params.get(f"mod{s}_src", 0)) for s in range(1, pg.MOD["slots"] + 1)}
    free = [(src, n) for src, n in LFOS if src not in used]
    amount = rng.choice([-1, 1]) * rng.uniform(0.3, 0.8)
    if free and rng.random() < 0.5:
        src, n = rng.choice(free)
        params[f"lfo{n}_shape"] = float(rng.choice([0, 1, 9, 18]))
        params[f"lfo{n}_rate"] = float(round(rng.uniform(0.15, 3.0), 3))
        params[f"lfo{n}_sync"] = 0.0
    else:
        src = rng.choice([3, 3, 4, 19])  # Mod Env, Filter Env, Filter 2 Env: the sound changes over the note
    return route(params, src, dest_name, amount)


def active_osc(params, rng):
    on = [n for n in "12" if params.get(f"osc{n}_on", 1 if n == "1" else 0) >= 0.5]
    return rng.choice(on) if on else None


def mutate(parent, mates, category, rng):
    attrs, params = pg.read_preset(parent)
    params = dict(params)
    if mates and rng.random() < 0.4:
        _, other = pg.read_preset(rng.choice(mates))
        group = rng.choice(GROUPS)
        for k, v in other.items():
            if k.startswith(group) and k in params:
                params[k] = v
    ops = rng.sample(["chaos", "table", "fx", "filter", "chaos", "morph", "morph", "vowel", "supersaw", "pwm"], rng.randint(1, 3))
    for op in ops:
        if op == "chaos":
            t9.chaos(params, rng, rng.randint(3, 8))
        elif op == "table":
            n = active_osc(params, rng)
            if n:
                pool = FAMILY[rng.choice(list(FAMILY))] if rng.random() < 0.4 else ALL
                params[f"osc{n}_table"] = float(rng.choice(pool))
        elif op == "morph":
            n = active_osc(params, rng)
            if n:
                morph(params, rng.choice([f"Osc{n} Frame", f"Osc{n} Frame", f"Osc{n} Detune", f"Osc{n} Unison Blend",
                                          f"Osc{n} Spread", "Filter1 Morph"]), rng)
        elif op == "supersaw":
            params.update({"osc1_on": 1.0, "osc1_table": float(rng.choice(FAMILY["saw"])), "osc1_unison": float(rng.randint(7, 11)),
                           "osc1_uni_mode": 1.0, "osc1_detune": rng.uniform(15, 45), "osc1_spread": rng.uniform(0.6, 1.0)})
        elif op == "pwm":
            params.update({"osc1_on": 1.0, "osc1_table": float(rng.choice([i for i in FAMILY["pwm"] if "PWM" in NAMES[i] or "Pulse" in NAMES[i]])),
                           "osc1_unison": float(rng.randint(1, 3))})
            morph(params, "Osc1 Frame", rng)
        elif op == "vowel":
            if rng.random() < 0.5:
                params["f1_type"] = float(rng.choice(VOWEL_FILTERS))
                morph(params, "Filter1 Morph", rng)
            else:
                slot = pg.free_fx_slot(params)
                if slot is not None:
                    params[f"fx_slot{slot}"] = 27.0
                    params[f"fx_slot{slot}_mix"] = rng.uniform(0.3, 0.7)
                    morph(params, "Vowel Morph", rng)
        elif op == "fx":
            slot = pg.free_fx_slot(params)
            if slot is not None:
                params[f"fx_slot{slot}"] = float(rng.choice(POOLS["fx"]))
                params[f"fx_slot{slot}_mix"] = rng.uniform(0.2, 0.5)
        elif op == "filter":
            params["f1_type"] = float(rng.choice(POOLS["f1_type"]))
    params = pg.style_rules(params, category, rng)
    macros = [attrs.get(f"macroName{i + 1}", "") for i in range(8)]
    skip = set(pg.SCHEMA_KEYS) | {"presetName", "presetCategory", "presetAuthor"} | {f"macroName{i + 1}" for i in range(8)}
    curves = {k: v for k, v in attrs.items() if k not in skip}
    schema = {k: attrs[k] for k in pg.SCHEMA_KEYS if k in attrs} or None
    return params, macros, curves, schema


WINNERS = Path("/mnt/project-files/ilanasynth/search-winners")
WINNERS2 = Path("/mnt/project-files/ilanasynth/search-winners-2")


def seed_parents():
    out = []
    if (WINNERS / "pack.json").exists():  # round 2: her Keep votes on the search winners are parents too
        for p in json.load(open(WINNERS / "pack.json")):
            if p.get("vote") == "Keep":
                out.append((WINNERS / p["preset"], p["category"]))
    for pid in KEPT:
        info = next(p for p in json.load(open(PACK / "pack.json")) if p["id"] == pid)
        out.append((PACK / info["preset"], info["category"]))
    if (WINNERS2 / "pack.json").exists():  # round 3: the second page's Keep votes too
        for p in json.load(open(WINNERS2 / "pack.json")):
            if p.get("vote") == "Keep":
                out.append((WINNERS2 / p["preset"], p["category"]))
    for f in sorted(glob.glob(str(ROOT / "content" / "user-presets" / "*.ilanapreset"))):
        cat = pg.REF_CATEGORY.get(Path(f).stem)
        if cat in CATEGORIES:
            out.append((Path(f), cat))
    return out


def cmd_run(args):
    global QD
    QD = GEN / args.out
    defaults = pg.read_defaults()
    pg.DEFAULTS.update(defaults)
    rng = random.Random(args.seed)
    QD.mkdir(parents=True, exist_ok=True)
    arch_path = QD / "archive.json"
    archive = json.load(open(arch_path)) if arch_path.exists() else {}
    start = max([a["gen"] for a in archive.values()] + [-1]) + 1
    for gen in range(start, start + args.gens):
        folder = QD / f"gen{gen}"
        shutil.rmtree(folder, ignore_errors=True)
        folder.mkdir(parents=True)
        meta = {}
        if gen == 0:
            for k, (path, cat) in enumerate(seed_parents()):
                name = f"{cat} seed{k}"
                shutil.copy(path, folder / f"{name}.ilanapreset")
                meta[name] = (cat, str(folder / f"{name}.ilanapreset"))
        else:
            for cat in CATEGORIES:
                elites = [a for a in archive.values() if a["category"] == cat]
                if not elites:
                    continue
                for k in range(args.per):
                    parent = rng.choice(elites)
                    mates = [Path(a["preset"]) for a in elites if a["preset"] != parent["preset"]]
                    params, macros, curves, schema = mutate(parent["preset"], mates, cat, rng)
                    name = f"{cat} g{gen}-{k}"
                    pg.write_preset(folder / f"{name}.ilanapreset", name, cat, params, macros, curves, schema)
                    meta[name] = (cat, str(folder / f"{name}.ilanapreset"))
        rep = render(folder, QD / f"gen{gen}-render")
        added = replaced = 0
        for name, row in rep.items():
            if name not in meta or not gate(row):
                continue
            cat, path = meta[name]
            key = f"{cat}/{cell(row)}"
            fit = float(row["score"])
            old = archive.get(key)
            if old is None or fit > old["fitness"]:
                keep = QD / "elites" / f"{key.replace('/', '_')}.ilanapreset"
                keep.parent.mkdir(exist_ok=True)
                shutil.copy(path, keep)
                archive[key] = {"category": cat, "preset": str(keep), "fitness": fit, "gen": gen, "name": name}
                added += old is None
                replaced += old is not None
        json.dump(archive, open(arch_path, "w"), indent=1)
        print(f"gen {gen}: {len(meta)} rendered, {added} new cells, {replaced} improved, {len(archive)} cells", flush=True)


def cmd_status(_args):
    archive = json.load(open(QD / "archive.json"))
    for cat in CATEGORIES:
        cells = [a for a in archive.values() if a["category"] == cat]
        print(cat, len(cells), "cells, mean fitness", round(sum(a["fitness"] for a in cells) / max(1, len(cells)), 1))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    run = sub.add_parser("run")
    run.add_argument("--gens", type=int, default=6)
    run.add_argument("--per", type=int, default=40)
    run.add_argument("--seed", type=int, default=1)
    run.add_argument("--out", default="qd", help="folder under build/gen for this run's archive")
    sub.add_parser("status")
    a = ap.parse_args()
    {"run": cmd_run, "status": cmd_status}[a.cmd](a)
