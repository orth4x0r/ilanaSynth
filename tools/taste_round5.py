#!/usr/bin/env python3
"""Round 5 of ilana's taste vote: the Lead and Pluck concepts rebuilt from tutorial recipes.

    python tools/taste_round5.py <factory render> <new drafts render> <out folder> --prev pairs.json ...

Pairs: each new concept's draft against an unheard library preset, plus a few new-vs-new
pairs of different concepts. Near-duplicates are dropped (ilana, round 3): no two drafts
in the round, and no two sides of one pair, may measure above SAME in CLAP cosine.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from taste_pairs import CLIP, clip, farthest_first, write  # noqa: E402
from taste_round2 import load, params  # noqa: E402
from taste_round3 import passes, report  # noqa: E402

SAME = 0.93
NEW_VS_NEW = {"Lead": 2, "Pluck": 1}


def main():
    ap = argparse.ArgumentParser()
    for name in ("factory", "new", "out"):
        ap.add_argument(name)
    ap.add_argument("--prev", nargs="*", default=[])
    ap.add_argument("--seed", type=int, default=5)
    args = ap.parse_args()
    fac_root, new_root, out = Path(args.factory), Path(args.new), Path(args.out)
    fac, fac_emb = load(fac_root)
    new, new_emb = load(new_root)
    rep = report(new_root)
    rng = np.random.default_rng(args.seed)
    used = set()
    for path in args.prev:
        for pair in json.load(open(path)):
            used.update((pair["a"]["name"], pair["b"]["name"]))
    (out / "clips").mkdir(parents=True, exist_ok=True)
    ok = [i for i, r in enumerate(new) if r["category"] in CLIP and passes(r, rep)
          and clip(new_root, r["index"], r["category"]) is not None]
    chosen = []

    def distinct(i):
        return all(float(new_emb[i] @ new_emb[j]) < SAME for j in chosen)

    def entry(pid, category, kind, sides):
        e = {"id": pid, "category": category, "kind": kind}
        for tag, (source, root, row) in zip("ab", sides):
            write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], category))
            e[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row)}
        return e

    by_concept = {}
    for i in ok:
        by_concept.setdefault(new[i]["name"].split("~")[0], []).append(i)
    picks = {}  # concept -> draft index, one per concept, mutually distinct
    for concept, ids in by_concept.items():
        rng.shuffle(ids)
        for i in ids:
            if distinct(i):
                picks[concept] = i
                chosen.append(i)
                break
        else:
            print("no distinct passing draft for", concept)
    pairs, taken_lib = [], set()
    for concept, i in picks.items():
        category = new[i]["category"]
        lib = [k for k, r in enumerate(fac) if r["category"] == category and r["name"] not in used
               and k not in taken_lib and float(new_emb[i] @ fac_emb[k]) < SAME
               and clip(fac_root, r["index"], category) is not None]
        if not lib:
            continue
        f = lib[farthest_first(fac_emb[lib], min(6, len(lib)), rng)[-1]]
        taken_lib.add(f)
        sides = [("draft", new_root, new[i]), ("library", fac_root, fac[f])]
        if rng.random() < 0.5:
            sides.reverse()
        pairs.append(entry(f"r5-{concept.lower().replace(' ', '-')}", category, "library", sides))
    # new vs new: different concepts, extra drafts (still distinct from everything chosen)
    for category, count in NEW_VS_NEW.items():
        for k in range(count):
            extra = []
            for concept, ids in by_concept.items():
                if new[ids[0]]["category"] != category:
                    continue
                cand = [i for i in ids if distinct(i)]
                if cand:
                    extra.append(cand[0])
            rng.shuffle(extra)
            if len(extra) < 2:
                break
            a = extra[0]
            chosen.append(a)
            b = next((j for j in extra[1:] if distinct(j) and new[j]["name"].split("~")[0] != new[a]["name"].split("~")[0]), None)
            if b is None:
                continue
            chosen.append(b)
            pairs.append(entry(f"r5-{category.lower()}-nn{k + 1}", category, "newnew",
                               [("draft", new_root, new[a]), ("draft", new_root, new[b])]))
    pairs = [pairs[i] for i in rng.permutation(len(pairs))]
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
