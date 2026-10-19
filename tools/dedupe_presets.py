#!/usr/bin/env python3
"""Finds factory presets that sound almost the same as another and writes the list the synth leaves out.

    ilanaPresetRender build/preset-render
    python tools/clap_score.py build/preset-render
    python tools/dedupe_presets.py build/preset-render [--threshold 0.93] [--header src/PresetRetired.h] [--report docs/PRESET-DEDUPE.md]

Uses the render folder's clap_embeddings.npy (CLAP audio embeddings of each
preset's note and chord) and clap.csv. A preset is a duplicate when its cosine
to a preset already kept is at least the threshold (0.93, the rule of the taste
tools). Of two alike a designed preset beats a DX7 ROM voice, and otherwise the
one whose sound fits its own name better (clap.csv name_score) is kept. Never dropped: the original 80 presets (their
indices are host program numbers), presets that tests or bounceFrom recipes
name, rhythmic ones (a note and a chord cannot show a gate pattern or a sequence), the Init and FX Input presets (the latter render silent without input)
and the DX7 voices (the browser and its tests list each ROM bank whole).
Needs numpy.
"""

import argparse
import csv
import re
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
PROTECTED_CATEGORIES = {"Init", "FX Input", "DX7"}
ORIGINAL = 80
# A note and a chord cannot show a rhythm, so presets made of one are never judged alike.
RHYTHM = re.compile(r"gate|euclid|tresillo|seq|arp|ratchet|rhythm|clock|beat|groove|pulse|over four|machine", re.I)


def named_somewhere():
    """Preset names the tests and the tools' C++ name, and the bounce sources."""
    text = ""
    for folder in (ROOT / "tests", ROOT / "tools"):
        for f in folder.iterdir():
            if f.is_file() and f.suffix in (".h", ".cpp", ".inc"):
                text += f.read_text(errors="ignore")
    bounce = ""
    for f in (ROOT / "src").glob("Preset*.h"):
        bounce += f.read_text(errors="ignore")
    return text, set(re.findall(r'bounceFrom \("([^"]+)"', bounce))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("render", type=Path)
    ap.add_argument("--threshold", type=float, default=0.93)
    ap.add_argument("--header", type=Path)
    ap.add_argument("--report", type=Path)
    args = ap.parse_args()

    emb = np.load(args.render / "clap_embeddings.npy")
    emb = emb / np.linalg.norm(emb, axis=1, keepdims=True)
    index = list(csv.DictReader(open(args.render / "index.csv", encoding="utf-8", newline="")))
    clap = list(csv.DictReader(open(args.render / "clap.csv", encoding="utf-8", newline="")))
    sim = emb @ emb.T
    name_score = np.array([float(r["name_score"]) for r in clap])

    tests, bounce = named_somewhere()
    protected = set(range(ORIGINAL))
    for i, row in enumerate(index):
        if row["category"] in PROTECTED_CATEGORIES or RHYTHM.search(row["name"]) or row["name"] in bounce or '"%s"' % row["name"] in tests:
            protected.add(i)

    order = sorted(range(len(emb)), key=lambda i: (i not in protected, index[i]["category"] == "DX7", -name_score[i]))
    kept, dropped = [], []
    for i in order:
        match = [j for j in kept if sim[i, j] >= args.threshold]
        if match and i not in protected:
            dropped.append((i, max(match, key=lambda j: sim[i, j])))
        else:
            kept.append(i)
    print("%d presets, %d kept, %d near-duplicates" % (len(emb), len(kept), len(dropped)))

    if args.header:
        names = sorted(index[i]["name"] for i, _ in dropped)
        esc = lambda n: n.replace("\\", "\\\\").replace('"', '\\"')
        lines, cur = [], "       "
        for n in names:
            piece = ' "%s",' % esc(n)
            if len(cur) + len(piece) > 110:
                lines.append(cur)
                cur = "       "
            cur += piece
        lines.append(cur)
        text = args.header.read_text()
        start = text.index("    static const juce::StringArray retired {\n") + len("    static const juce::StringArray retired {\n")
        end = text.index("\n    };", start)
        args.header.write_text(text[:start] + "\n".join(lines).rstrip(",") + text[end:])

    if args.report:
        groups = {}
        for i, j in dropped:
            groups.setdefault(j, []).append(i)
        out = ["# Near-duplicate factory presets", "",
               "%d of %d rendered factory presets are left out of the browser (`src/PresetRetired.h`); "
               "`tools/dedupe_presets.py` made the list (CLAP cosine %.2f or more to a preset kept)." % (len(dropped), len(emb), args.threshold), ""]
        for j, members in sorted(groups.items(), key=lambda kv: (-len(kv[1]), index[kv[0]]["name"])):
            out.append("- **kept** %s (%s)" % (index[j]["name"], index[j]["category"]))
            for i in sorted(members, key=lambda m: index[m]["name"]):
                out.append("  - left out %s (%s, %.3f)" % (index[i]["name"], index[i]["category"], sim[i, j]))
        args.report.write_text("\n".join(out) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
