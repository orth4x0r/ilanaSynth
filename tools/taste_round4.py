#!/usr/bin/env python3
"""Round 4 of ilana's taste vote: does the taste model pick better drafts than chance?

    python tools/taste_round4.py <factory render> <new drafts render> <out folder> --prev pairs.json ...

The new drafts folder needs report.csv (tools/preset_critic.py), clap_embeddings.npy and
taste_score.csv (tools/taste_model.py score). Pair kinds, shuffled together:
  modelrandom  a draft the taste model ranks high against a random gated draft of the
               same category (does the model help?);
  library      a model-ranked draft against a library preset she has not heard;
  fixcheck     a reworked concept's new draft against the draft of it she heard in
               round 3 (needs the round 3 pairs in --prev).
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
from taste_round3 import passes, report  # noqa: E402

REWORKED = ["Kalimba Bar", "Picked Steel", "Reed Lead", "Bowed Lead", "Acid Fold Lead"]
MODELRANDOM = {"Lead": 5, "Pluck": 5, "Pad": 2, "Keys": 2, "Bass": 2}
LIBRARY = {"Lead": 6, "Pluck": 6, "Pad": 2, "Keys": 2, "Bass": 3}


def main():
    ap = argparse.ArgumentParser()
    for name in ("factory", "new", "out"):
        ap.add_argument(name)
    ap.add_argument("--prev", nargs="*", default=[])
    ap.add_argument("--round3-root", default="build/gen/drafts3m-render")
    ap.add_argument("--seed", type=int, default=4)
    args = ap.parse_args()
    fac_root, new_root, out = Path(args.factory), Path(args.new), Path(args.out)
    r3_root = Path(args.round3_root)
    fac, fac_emb = load(fac_root)
    new, new_emb = load(new_root)
    rep = report(new_root)
    taste = {r["index"]: float(r["taste"]) for r in csv.DictReader(open(new_root / "taste_score.csv"))}
    rng = np.random.default_rng(args.seed)
    used, heard = set(), {}
    for path in args.prev:
        for pair in json.load(open(path)):
            for tag in "ab":
                side = pair[tag]
                used.add(side["name"])
                if pair.get("id", "").startswith("r3-") and side.get("source") == "draft":
                    heard[side["name"].split("~")[0]] = side
    (out / "clips").mkdir(parents=True, exist_ok=True)
    ok = [i for i, r in enumerate(new) if r["category"] in CLIP and passes(r, rep)
          and clip(new_root, r["index"], r["category"]) is not None]
    print(len(ok), "of", len(new), "new drafts pass the gate")
    taken = set()

    def entry(pid, category, kind, sides):
        e = {"id": pid, "category": category, "kind": kind}
        for tag, (source, root, row) in zip("ab", sides):
            write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], category))
            e[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row),
                      "taste": taste.get(row["index"]) if root == new_root else None}
        return e

    def top_drafts(category, count):
        """Highest taste scores, one per concept, then spread out (farthest-first)."""
        best = {}
        for i in ok:
            if new[i]["category"] == category and i not in taken:
                c = new[i]["name"].split("~")[0]
                if c not in best or taste[new[i]["index"]] > taste[new[best[c]]["index"]]:
                    best[c] = i
        ranked = sorted(best.values(), key=lambda i: -taste[new[i]["index"]])
        pool = ranked[: max(count + 2, int(len(ranked) * 0.6))]
        return [pool[k] for k in farthest_first(new_emb[pool], min(count, len(pool)), rng)]

    pairs = []
    for concept in REWORKED:
        cands = [i for i in ok if new[i]["name"].split("~")[0] == concept]
        old = heard.get(concept)
        if not cands or not old:
            print("skip fixcheck:", concept)
            continue
        i = max(cands, key=lambda j: float(rep[new[j]["index"]]["score"]))
        taken.add(i)
        old_row = {"index": str(old["index"]), "name": old["name"]}
        sides = [("draft", new_root, new[i]), ("old-draft", r3_root, old_row)]
        if rng.random() < 0.5:
            sides.reverse()
        pairs.append(entry(f"r4-fix-{concept.lower().replace(' ', '-')}", new[i]["category"], "fixcheck", sides))
    for category, count in MODELRANDOM.items():
        for k, i in enumerate(top_drafts(category, count)):
            taken.add(i)
            others = [j for j in ok if new[j]["category"] == category and j not in taken
                      and new[j]["name"].split("~")[0] != new[i]["name"].split("~")[0]]
            j = int(rng.choice(others))
            taken.add(j)
            sides = [("draft-model", new_root, new[i]), ("draft-random", new_root, new[j])]
            if rng.random() < 0.5:
                sides.reverse()
            pairs.append(entry(f"r4-{category.lower()}-m{k + 1}", category, "modelrandom", sides))
    for category, count in LIBRARY.items():
        picks = top_drafts(category, count)
        lib = [i for i, r in enumerate(fac) if r["category"] == category and r["name"] not in used
               and clip(fac_root, r["index"], category) is not None]
        lib_picks = [lib[k] for k in farthest_first(fac_emb[lib], len(picks), rng)]
        rng.shuffle(lib_picks)
        for k, (d, f) in enumerate(zip(picks, lib_picks)):
            taken.add(d)
            used.add(fac[f]["name"])
            sides = [("draft-model", new_root, new[d]), ("library", fac_root, fac[f])]
            if rng.random() < 0.5:
                sides.reverse()
            pairs.append(entry(f"r4-{category.lower()}-l{k + 1}", category, "library", sides))
    pairs = [pairs[i] for i in rng.permutation(len(pairs))]
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
