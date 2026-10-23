#!/usr/bin/env python3
"""Round 9 drafts: variations of the library presets that beat the generated drafts in round 8.

    python tools/taste_round9_draft.py <factory render> <out folder> [--per 6] [--seed 9]

Each winner is rebuilt from its params.txt over the defaults (plus the original, as draw 0, to check the
rebuild sounds like the factory preset) and varied along ONE axis at a time (tone, time or space) so the
variations differ from each other in one clear way. Names are "<Winner> <axis>~<draw>", categories as the
winner's, ready for ilanaPresetRender (ILANA_RENDER_FILES=<out folder>).
"""

import argparse
import csv
import json
import math
import random
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import preset_gen as pg  # noqa: E402

WINNERS = ["Sympathetic Haze", "Felt Upright", "Phaser Notch Sweep", "LFO 4 Sweep", "Vector Corner Morph"]
AXES = {
    "tone": [(r"^f[12]_cutoff$", "log", 0.4, 2.5), (r"^f[12]_reso$", "add", -0.15, 0.25),
             (r"^osc\d_frame$", "add", -0.25, 0.25)],
    "time": [(r"^amp_(attack|release)$", "log", 0.4, 3.0), (r"^amp_decay$", "log", 0.4, 2.5),
             (r".*_string_decay$", "add", -0.25, 0.2)],
    "space": [(r"^fx_slot\d+_mix$", "add", -0.25, 0.3), (r"^osc\d_detune$", "log", 0.4, 2.5),
              (r"^lfo\d_rate$", "log", 0.4, 2.5), (r"^mod\d+_amt$", "add", -0.3, 0.3)],
}


def read_params(root, index, defaults):
    out = dict(defaults)
    for line in (root / index / "params.txt").read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            try:
                out[key] = float(value)
            except ValueError:
                pass
    return out


def vary(params, axis, strength, rng):
    changed = 0
    for pattern, kind, lo, hi in AXES[axis]:
        for key in list(params):
            if not re.match(pattern, key):
                continue
            v = params[key]
            if kind == "log":
                if v <= 0:
                    continue
                new = v * math.exp(rng.uniform(math.log(lo), math.log(hi)) * strength)
            else:
                new = v + rng.uniform(lo, hi) * strength
                if 0 <= v <= 1:
                    new = min(1.0, max(0.0, new))
            if new != v:
                params[key], changed = new, changed + 1
    return changed


SKIP = re.compile(r"(_on$|_mode$|_type$|_table$|_src$|_dst$|^fx_slot\d+$|voices|unison|_semi$|^master|^output|^macro|^mod\d+_(src|dst)|schema|^osc\d_excite$|^midi|^glide|^voice)")


AUDIBLE = re.compile(r"^(f[12]_(cutoff|reso|drive|env)|osc\d_(level|frame|detune|fine|string_\w+|bow_\w+|body_\w+)|sub_level|amp_(attack|decay|sustain|release)|"
                     r"fx_slot\d+_mix|res_amount|sb_size|body_\w+|west_(fold|res|strike|open)|mod\d+_amt|lfo\d_(rate|depth)|vec_\w+|noise_level|fm_amount)$")


def active(params, key):
    """False for parameters of a switched-off oscillator or an empty effect slot."""
    m = re.match(r"^osc(\d)_", key)
    if m and params.get(f"osc{m.group(1)}_on", 1 if m.group(1) == "1" else 0) < 0.5:
        return False
    m = re.match(r"^fx_slot(\d+)_mix$", key)
    return not (m and params.get(f"fx_slot{m.group(1)}", 0) < 0.5)


def chaos(params, rng, count):
    """Perturb COUNT random continuous parameters hard: [0,1] ones are redrawn near-uniformly, others scaled."""
    keys = [k for k, v in params.items() if AUDIBLE.search(k) and not SKIP.search(k) and active(params, k)]
    rng.shuffle(keys)
    for key in keys[:count]:
        v = params[key]
        if 0 <= v <= 1:
            params[key] = min(1.0, max(0.0, v + rng.choice([-1, 1]) * rng.uniform(0.25, 0.7)))
        elif v > 0:
            params[key] = v * math.exp(rng.uniform(math.log(0.2), math.log(5.0)))
    return len(keys[:count])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("factory")
    ap.add_argument("out")
    ap.add_argument("--per", type=int, default=6)
    ap.add_argument("--seed", type=int, default=9)
    ap.add_argument("--chaos", type=int, default=0, help="per winner: this many hard random variations instead of axes")
    args = ap.parse_args()
    root, out = Path(args.factory), Path(args.out)
    defaults = pg.read_defaults()
    pg.DEFAULTS.update(defaults)
    rng = random.Random(args.seed)
    rows = {r["name"]: r for r in csv.DictReader(open(root / "index.csv", newline="", encoding="utf-8"))}
    manifest = []
    for name in WINNERS:
        row = rows[name]
        base = read_params(root, row["index"], defaults)
        macros = [row[f"macro{i}"] for i in range(1, 5)]
        pg.write_preset(out / f"{name} original~0.ilanapreset", f"{name} original~0", row["category"], base, macros)
        manifest.append({"stem": f"{name} original~0", "concept": f"{name} original", "draw": 0})
        for draw in range(1, args.chaos + 1):
            p = dict(base)
            chaos(p, rng, rng.randint(5, 11))
            stem = f"{name} mix~{draw}"
            pg.write_preset(out / f"{stem}.ilanapreset", stem, row["category"], p, macros)
            manifest.append({"stem": stem, "concept": f"{name} mix", "draw": draw})
        for axis in ([] if args.chaos else AXES):
            for draw in range(1, args.per + 1):
                p = dict(base)
                if vary(p, axis, 1.0, rng) == 0:
                    break
                stem = f"{name} {axis}~{draw}"
                pg.write_preset(out / f"{stem}.ilanapreset", stem, row["category"], p, macros)
                manifest.append({"stem": stem, "concept": f"{name} {axis}", "draw": draw})
    (out.parent / (out.name + ".json")).write_text(json.dumps(manifest, indent=1))
    print(len(manifest), "drafts in", out)


if __name__ == "__main__":
    main()
