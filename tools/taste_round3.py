#!/usr/bin/env python3
"""Round 3 of ilana's taste vote: the redesigned drafts.

    python tools/taste_round3.py <factory render> <old drafts render> <new drafts render> <out folder> --prev pairs.json ...

Two kinds of pair, shuffled together:
  fixcheck   a new draft of a concept ilana's round 2 votes marked weak, against the
             old draft she heard (before and after, blind);
  library    a new draft against a library preset she has not heard, as in round 2.
New drafts must pass a gate first (the critic's findings, a real decay for plucks).
Writes clips and pairs.json like tools/taste_round2.py; each pair has "kind".
"""

import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from taste_pairs import CLIP, clip, farthest_first, write  # noqa: E402
from taste_round2 import load, params  # noqa: E402

FIXED = ["Koto Snap", "Drop Pluck", "Picked Steel", "Kalimba Bar", "Reed Lead", "Bowed Lead", "Acid Fold Lead"]
QUOTA = {"Lead": 9, "Pluck": 10, "Pad": 5, "Keys": 5, "Bass": 4}
BAD_ISSUES = ("doesn't decay like a pluck", "harsh top", "silent", "dc offset")


def report(root):
    path = root / "report.csv"
    return {r["index"]: r for r in csv.DictReader(open(path, newline="", encoding="utf-8"))} if path.exists() else {}


def passes(row, rep):
    r = rep.get(row["index"])
    if not r:
        return True
    issues = (r.get("issues") or "").lower()
    if any(b in issues for b in BAD_ISSUES):
        return False
    if row["category"] == "Pluck" and float(r.get("sustain_db") or -99) > -14:
        return False
    return float(r.get("score") or 0) >= 55


def main():
    ap = argparse.ArgumentParser()
    for name in ("factory", "old", "new", "out"):
        ap.add_argument(name)
    ap.add_argument("--prev", nargs="*", default=[])
    ap.add_argument("--seed", type=int, default=3)
    args = ap.parse_args()
    fac_root, old_root, new_root, out = (Path(args.factory), Path(args.old), Path(args.new), Path(args.out))
    fac, fac_emb = load(fac_root)
    old, _ = load(old_root)
    new, new_emb = load(new_root)
    new_rep = report(new_root)
    rng = np.random.default_rng(args.seed)
    used, heard_drafts = set(), {}
    for path in args.prev:
        for pair in json.load(open(path)):
            for tag in "ab":
                side = pair[tag]
                used.add(side["name"])
                if side.get("source") == "draft":
                    heard_drafts.setdefault(side["name"].split("~")[0], side)
    (out / "clips").mkdir(parents=True, exist_ok=True)
    old_by_name = {r["name"]: r for r in old}
    new_ok = [i for i, r in enumerate(new) if r["category"] in CLIP and passes(r, new_rep) and clip(new_root, r["index"], r["category"]) is not None]
    print(len(new_ok), "of", len(new), "new drafts pass the gate")

    def entry(pid, category, kind, sides):
        e = {"id": pid, "category": category, "kind": kind}
        for tag, (source, root, row) in zip("ab", sides):
            write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], category))
            e[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row)}
        return e

    pairs = []
    # fixcheck pairs
    for concept in FIXED:
        cands = [i for i in new_ok if new[i]["name"].split("~")[0] == concept]
        heard = heard_drafts.get(concept)
        if not cands or not heard:
            print("skip fixcheck:", concept, "(no passing new draft)" if not cands else "(no heard draft)")
            continue
        best = max(cands, key=lambda i: float(new_rep.get(new[i]["index"], {}).get("score") or 0))
        old_row = old_by_name[heard["name"]]
        sides = [("draft", new_root, new[best]), ("old-draft", old_root, old_row)]
        if rng.random() < 0.5:
            sides.reverse()
        pairs.append(entry(f"r3-fix-{concept.lower().replace(' ', '-')}", new[best]["category"], "fixcheck", sides))
    # draft against library
    for category, quota in QUOTA.items():
        by_concept = {}
        for i in new_ok:
            if new[i]["category"] == category:
                by_concept.setdefault(new[i]["name"].split("~")[0], []).append(i)
        pool = []
        for ids in by_concept.values():
            rng.shuffle(ids)
            pool += ids[:2]
        if not pool:
            continue
        picks = [pool[k] for k in farthest_first(new_emb[pool], min(quota, len(pool)), rng)]
        lib = [i for i, r in enumerate(fac) if r["category"] == category and r["name"] not in used
               and clip(fac_root, r["index"], category) is not None]
        lib_picks = [lib[k] for k in farthest_first(fac_emb[lib], len(picks), rng)]
        rng.shuffle(lib_picks)
        for k, (d, f) in enumerate(zip(picks, lib_picks)):
            sides = [("draft", new_root, new[d]), ("library", fac_root, fac[f])]
            if rng.random() < 0.5:
                sides.reverse()
            used.add(fac[f]["name"])
            pairs.append(entry(f"r3-{category.lower()}-{k + 1}", category, "library", sides))
    order = rng.permutation(len(pairs))
    pairs = [pairs[i] for i in order]
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
