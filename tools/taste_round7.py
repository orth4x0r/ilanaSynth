#!/usr/bin/env python3
"""Round 7 of ilana's taste vote: FM lead variants of the round 6 winner, plus Bass and Pad concepts.

    python tools/taste_round6.py <factory render> <round 6 render> <new drafts render> <out folder> --prev pairs.json ...

Each variant of a round 5 winner is paired against that winner's draft (does the variant beat it?);
the concepts without a winner to beat go against an unheard library preset; two pairs are new
against new. Near-duplicates are dropped as in round 5: no two new drafts, and no two sides of a
pair, above SAME in CLAP cosine.
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
WINNERS = {  # winner draft (round 6 render) -> the round 7 variants that must beat it
    "FM Brass~2": ["FM Brass Wide", "FM Brass Echo", "FM Saturated Lead", "FM Pulse Lead"],
}
LIBRARY = ["Reese Sub", "Acid Low", "Grit Bass", "Stab Bass", "Slow Choir", "Soft Saw Pad", "Glass Swell"]


def main():
    ap = argparse.ArgumentParser()
    for name in ("factory", "old", "new", "out"):
        ap.add_argument(name)
    ap.add_argument("--prev", nargs="*", default=[])
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    fac_root, old_root, new_root, out = Path(args.factory), Path(args.old), Path(args.new), Path(args.out)
    fac, fac_emb = load(fac_root)
    old, old_emb = load(old_root)
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
    by_concept = {}
    for i in ok:
        by_concept.setdefault(new[i]["name"].split("~")[0], []).append(i)
    for ids in by_concept.values():
        rng.shuffle(ids)
    chosen = []

    def distinct(i):
        return all(float(new_emb[i] @ new_emb[j]) < SAME for j in chosen)

    def entry(pid, category, kind, sides):
        e = {"id": pid, "category": category, "kind": kind}
        for tag, (source, root, row) in zip("ab", sides):
            write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], category))
            e[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row)}
        return e

    def flip(sides):
        return sides[::-1] if rng.random() < 0.5 else sides

    pairs = []
    old_by_name = {r["name"]: k for k, r in enumerate(old)}
    for winner, concepts in WINNERS.items():
        k = old_by_name[winner]
        for concept in concepts:
            pick = next((i for i in by_concept.get(concept, []) if distinct(i)
                         and float(new_emb[i] @ old_emb[k]) < SAME), None)
            if pick is None:
                print("no distinct draft of", concept, "against", winner)
                continue
            chosen.append(pick)
            pairs.append(entry(f"r7-{concept.lower().replace(' ', '-')}", new[pick]["category"], "vswinner",
                               flip([("draft", new_root, new[pick]), ("winner", old_root, old[k])])))
    for concept in LIBRARY:
        pick = next((i for i in by_concept.get(concept, []) if distinct(i)), None)
        if pick is None:
            print("no distinct draft of", concept)
            continue
        chosen.append(pick)
        category = new[pick]["category"]
        lib = [j for j, r in enumerate(fac) if r["category"] == category and r["name"] not in used
               and float(new_emb[pick] @ fac_emb[j]) < SAME and clip(fac_root, r["index"], category) is not None]
        f = lib[farthest_first(fac_emb[lib], min(6, len(lib)), rng)[-1]]
        used.add(fac[f]["name"])
        pairs.append(entry(f"r7-{concept.lower().replace(' ', '-')}", category, "library",
                           flip([("draft", new_root, new[pick]), ("library", fac_root, fac[f])])))
    for category in ("Bass", "Pad"):
        extra = []
        for concept, ids in by_concept.items():
            if new[ids[0]]["category"] == category:
                cand = [i for i in ids if distinct(i)]
                if cand:
                    extra.append(cand[0])
        rng.shuffle(extra)
        for a in extra:
            b = next((j for j in extra if j != a and distinct(j) and float(new_emb[a] @ new_emb[j]) < SAME
                      and new[j]["name"].split("~")[0] != new[a]["name"].split("~")[0]), None)
            if b is None:
                continue
            chosen += [a, b]
            pairs.append(entry(f"r7-{category.lower()}-nn1", category, "newnew",
                               [("draft", new_root, new[a]), ("draft", new_root, new[b])]))
            break
    pairs = [pairs[i] for i in rng.permutation(len(pairs))]
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
