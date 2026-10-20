#!/usr/bin/env python3
"""Round 9 of ilana's taste vote: variations of the library presets that beat the generated drafts in round 8.

    python tools/taste_round9.py <factory render> <variation render> <out folder>

Each pair is a library winner against one variation of it (tools/taste_round9_draft.py --chaos). A variation is
used only if it passes the critic gate, is clearly different from its winner (CLAP cosine below WINNER_SAME) and
from every other variation picked (below preset_dedupe.SAME). Writes clips/ and pairs.json like round 8.
"""

import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from preset_dedupe import SAME  # noqa: E402
from taste_pairs import clip, write  # noqa: E402
from taste_round2 import load, params  # noqa: E402
from taste_round3 import passes, report  # noqa: E402

WINNER_SAME = 0.93
PER_WINNER = 3


def main():
    fac_root, var_root, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    fac, fac_emb = load(fac_root)
    var, var_emb = load(var_root)
    rep = report(var_root)
    by_name = {r["name"]: k for k, r in enumerate(fac)}
    (out / "clips").mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(9)
    chosen, pairs = [], []
    winners = sorted({r["name"].split(" mix~")[0] for r in var if " mix~" in r["name"]})
    for name in winners:
        k = by_name[name]
        cands = [i for i, r in enumerate(var) if r["name"].startswith(name + " mix~") and passes(r, rep)
                 and float(var_emb[i] @ fac_emb[k]) < WINNER_SAME and clip(var_root, r["index"], r["category"]) is not None]
        cands.sort(key=lambda i: float(var_emb[i] @ fac_emb[k]))
        taken = 0
        for i in cands:
            if taken >= PER_WINNER:
                break
            if any(float(var_emb[i] @ var_emb[j]) >= SAME for j in chosen):
                continue
            chosen.append(i)
            taken += 1
            pid = f"r9-{name.lower().replace(' ', '-')}-{taken}"
            sides = [("draft", var_root, var[i]), ("winner", fac_root, fac[k])]
            if rng.random() < 0.5:
                sides.reverse()
            e = {"id": pid, "category": var[i]["category"], "kind": "vswinner"}
            for tag, (source, root, row) in zip("ab", sides):
                write(out / "clips" / f"{pid}-{tag}.wav", clip(root, row["index"], var[i]["category"]))
                e[tag] = {"source": source, "index": int(row["index"]), "name": row["name"], "params": params(root, row)}
            pairs.append(e)
        print(name, taken, "variations")
    pairs = [pairs[i] for i in rng.permutation(len(pairs))]
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
