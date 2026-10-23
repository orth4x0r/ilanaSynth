#!/usr/bin/env python3
"""Round 2 of ilana's taste vote: fresh generated drafts against library presets.

    python tools/taste_round2.py <factory render> <drafts render> <out folder> [--prev pairs.json ...] [--seed 2]

Pairs each draft (tools/preset_gen.py output, rendered with ilanaPresetRender)
with a library preset of the same category that has not been in an earlier
round. Drafts are spread over concepts (at most two per concept) and the
library side is picked farthest-first in CLAP space. Counts per category:
Lead 12, Pluck 10, Pad 6, Keys 6, Bass 6. Writes the clips like
tools/taste_pairs.py and pairs.json; each side carries its params.txt text
(the settings that differ from the defaults), so every vote is a row of
training data: [{id, category, a/b: {source, name, params}}].
"""

import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from taste_pairs import CLIP, clip, farthest_first, write  # noqa: E402

QUOTA = {"Lead": 12, "Pluck": 10, "Pad": 6, "Keys": 6, "Bass": 6}


def load(root):
    rows = list(csv.DictReader(open(root / "index.csv", newline="", encoding="utf-8")))
    emb = np.load(root / "clap_embeddings.npy")
    return rows, emb / np.linalg.norm(emb, axis=1, keepdims=True)


def params(root, row):
    path = root / row["index"] / "params.txt"
    return path.read_text(encoding="utf-8") if path.exists() else ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("factory")
    ap.add_argument("drafts")
    ap.add_argument("out")
    ap.add_argument("--prev", nargs="*", default=[])
    ap.add_argument("--seed", type=int, default=2)
    args = ap.parse_args()
    fac_root, dr_root, out = Path(args.factory), Path(args.drafts), Path(args.out)
    fac, fac_emb = load(fac_root)
    dr, dr_emb = load(dr_root)
    used = set()
    for path in args.prev:
        for pair in json.load(open(path)):
            used.update((pair["a"]["name"], pair["b"]["name"]))
    rng = np.random.default_rng(args.seed)
    (out / "clips").mkdir(parents=True, exist_ok=True)
    pairs = []
    for category, quota in QUOTA.items():
        # drafts: at most two per concept, spread farthest-first
        by_concept = {}
        for i, r in enumerate(dr):
            if r["category"] == category and clip(dr_root, r["index"], category) is not None:
                by_concept.setdefault(r["name"].split("~")[0], []).append(i)
        pool = []
        for ids in by_concept.values():
            rng.shuffle(ids)
            pool += ids[:2]
        picks = [pool[k] for k in farthest_first(dr_emb[pool], min(quota, len(pool)), rng)]
        lib = [i for i, r in enumerate(fac) if r["category"] == category and r["name"] not in used
               and clip(fac_root, r["index"], category) is not None]
        lib_picks = [lib[k] for k in farthest_first(fac_emb[lib], len(picks), rng)]
        rng.shuffle(lib_picks)
        for k, (d, f) in enumerate(zip(picks, lib_picks)):
            sides = [("draft", dr_root, dr[d]), ("library", fac_root, fac[f])]
            if rng.random() < 0.5:
                sides.reverse()
            pid = f"r2-{category.lower()}-{k + 1}"
            entry = {"id": pid, "category": category}
            for tag, (source, root, row) in zip("ab", sides):
                write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], category))
                entry[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row)}
                if source == "library":
                    used.add(row["name"])
            pairs.append(entry)
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
