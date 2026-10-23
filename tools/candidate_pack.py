#!/usr/bin/env python3
"""Candidate pack: the winning concepts' best distinct drafts, for ilana to audition.

    python tools/candidate_pack.py <out folder>

Takes up to PER gated drafts of each winning concept (mutually distinct: CLAP cosine below SAME
across the whole pack), writes clips/<id>.wav (the same -20 dB RMS clips as the vote pages), copies
the .ilanapreset files to presets/ and writes pack.json [{id, name, category, concept, preset}].
"""

import json
import shutil
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from taste_pairs import CLIP, clip, write  # noqa: E402
from taste_round2 import load  # noqa: E402
from taste_round3 import passes, report  # noqa: E402
from preset_dedupe import is_duplicate  # noqa: E402

PER = 3
GEN = Path("build/gen")
WINNERS = {  # render folder -> (preset folders, concepts)
    "drafts3m-render": (["drafts3", "drafts3b", "drafts"], ["Acid Fold Lead", "Drop Pluck", "Koto Snap"]),
    "drafts5-render": (["drafts5"], ["Vangelis Brass", "Snap Pluck", "Hyper Pluck", "Pad Pluck"]),
    "drafts6-render": (["drafts6"], ["FM Brass", "Chord Snap", "Crazy Snap", "Vowel Pluck", "Hammer Snap", "Strum Snap"]),
    "drafts7-render": (["drafts7"], ["FM Brass Wide", "Grit Bass", "Reese Sub", "Acid Low", "Slow Choir"]),
}


def main():
    out = Path(sys.argv[1])
    (out / "clips").mkdir(parents=True, exist_ok=True)
    (out / "presets").mkdir(exist_ok=True)
    chosen_emb, pack = [], []
    for render, (dirs, concepts) in WINNERS.items():
        root = GEN / render
        rows, emb = load(root)
        rep = report(root)
        for concept in concepts:
            ids = [i for i, r in enumerate(rows) if r["name"].split("~")[0] == concept and passes(r, rep)
                   and r["category"] in CLIP]
            ids.sort(key=lambda i: -float(rep.get(rows[i]["index"], {}).get("score") or 0))
            taken = 0
            for i in ids:
                if taken >= PER:
                    break
                if taken == 0 and is_duplicate(emb[i], concept, chosen_emb):
                    continue
                if taken and any(float(emb[i] @ e) >= 0.85 for e, _ in chosen_emb):
                    continue
                r = rows[i]
                src = next((GEN / d / f"{r['name']}.ilanapreset" for d in dirs if (GEN / d / f"{r['name']}.ilanapreset").exists()), None)
                samples = clip(root, r["index"], r["category"])
                if src is None or samples is None:
                    continue
                pid = f"{concept.lower().replace(' ', '-')}-{taken + 1}"
                write(out / "clips" / f"{pid}.wav", samples)
                shutil.copy(src, out / "presets" / f"{concept} {taken + 1}.ilanapreset")
                pack.append({"id": pid, "name": f"{concept} {taken + 1}", "category": r["category"], "concept": concept,
                             "source": r["name"], "preset": f"presets/{concept} {taken + 1}.ilanapreset"})
                chosen_emb.append((emb[i], concept))
                taken += 1
            if taken < PER:
                print(f"{concept}: only {taken} distinct drafts")
    (out / "pack.json").write_text(json.dumps(pack, indent=1), encoding="utf-8")
    print(len(pack), "candidates in", out)


if __name__ == "__main__":
    main()
