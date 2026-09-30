"""How varied the factory library is, per category, against reference libraries.

    build/venv/Scripts/python tools/clap_score.py build/critic-v4 build/critic-refs build/critic-refs-surge
    python tools/preset_diversity.py build/critic-v4 --refs build/critic-refs --refs build/critic-refs-surge

Uses each render folder's clap_embeddings.npy (CLAP audio embeddings: what a
sound is like) together with the critic's measurements from report.csv
(brightness, width, movement, attack, sustain, release). Per focus category:

- spread: the median distance between two presets. The library's is set
  against the references' (Surge and Vital, which were made by many people).
- near-duplicates: pairs closer than 95 % of the references' own pairs.
- crowds: groups of presets that all sit within that distance of each other.
- gaps: reference presets whose nearest library preset is further than 90 %
  of the references' own nearest neighbours: sounds the library never makes.

Writes <library>/diversity.md.
"""

import csv
import sys
from pathlib import Path

import numpy as np

FOCUS = ["Bass", "Pad", "Lead", "Keys", "Pluck"]
FEATURES = ["centroid_hz", "width", "movement", "attack_s", "sustain_db", "release_s", "vel_db", "line_dip_db"]


def load(folder):
    with open(folder / "index.csv", newline="", encoding="utf-8") as f:
        index = list(csv.DictReader(f))
    embeddings = np.load(folder / "clap_embeddings.npy")
    report = {}
    if (folder / "report.csv").exists():
        with open(folder / "report.csv", newline="", encoding="utf-8") as f:
            report = {r["index"]: r for r in csv.DictReader(f)}
    rows = []
    for row, embedding in zip(index, embeddings):
        features = report.get(row["index"])
        rows.append({"name": row["name"], "category": row["category"], "embedding": embedding / np.linalg.norm(embedding),
                     "features": {k: float(features[k]) for k in FEATURES} if features else None})
    return rows


def feature_vectors(rows, stats):
    out = []
    for r in rows:
        f = r["features"]
        if f is None:
            out.append(np.zeros(len(FEATURES)))
            continue
        values = []
        for k in FEATURES:
            v = f[k]
            if k == "centroid_hz":
                v = np.log2(max(v, 20.0))
            if k in ("attack_s", "release_s"):
                v = np.log2(max(v, 0.001))
            mean, spread = stats[k]
            values.append((v - mean) / spread)
        out.append(np.array(values))
    return out


def distances(a, b):
    """Combined distance: CLAP cosine distance, plus the measured features
    (z-scored, scaled so both parts weigh about the same)."""
    cos = 1.0 - a["embedding"] @ b["embedding"]
    feat = np.linalg.norm(a["fv"] - b["fv"]) / np.sqrt(len(FEATURES)) * 0.25
    return float(cos + feat)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    root = Path(sys.argv[1])
    ref_roots = [Path(sys.argv[i + 1]) for i, a in enumerate(sys.argv[:-1]) if a == "--refs"]
    ours = load(root)
    refs = [r for folder in ref_roots for r in load(folder)]

    # One scale for the features, from everything.
    everything = [r for r in ours + refs if r["features"] is not None]
    stats = {}
    for k in FEATURES:
        values = []
        for r in everything:
            v = r["features"][k]
            if k == "centroid_hz":
                v = np.log2(max(v, 20.0))
            if k in ("attack_s", "release_s"):
                v = np.log2(max(v, 0.001))
            values.append(v)
        stats[k] = (float(np.mean(values)), float(np.std(values)) or 1.0)
    for rows in (ours, refs):
        for r, fv in zip(rows, feature_vectors(rows, stats)):
            r["fv"] = fv

    lines = ["# Preset diversity", "",
             "Distances combine what CLAP hears (timbre and character) with the critic's measurements "
             "(brightness, width, movement, envelope). 0 is identical.", "",
             "| Category | Presets | Median pair distance | References' | Near-duplicate pairs | In crowds | Gaps |",
             "|---|---:|---:|---:|---:|---:|---:|"]
    details = []
    for category in FOCUS:
        mine = [r for r in ours if r["category"] == category]
        theirs = [r for r in refs if r["category"] == category]
        if len(mine) < 3 or len(theirs) < 5:
            continue
        pair = lambda rows: [distances(rows[i], rows[j]) for i in range(len(rows)) for j in range(i + 1, len(rows))]
        mine_pairs, their_pairs = pair(mine), pair(theirs)
        close = float(np.percentile(their_pairs, 5))

        duplicates = sorted(((distances(mine[i], mine[j]), mine[i]["name"], mine[j]["name"])
                             for i in range(len(mine)) for j in range(i + 1, len(mine))
                             if distances(mine[i], mine[j]) < close))

        # Crowds: greedy groups where everyone is within `close` of the seed.
        left = list(range(len(mine)))
        crowds = []
        while left:
            seed = left.pop(0)
            group = [seed] + [k for k in left if distances(mine[seed], mine[k]) < close * 1.2]
            left = [k for k in left if k not in group]
            if len(group) >= 3:
                crowds.append([mine[k]["name"] for k in group])

        their_nn = [min(distances(t, u) for u in theirs if u is not t) for t in theirs]
        far = float(np.percentile(their_nn, 90))
        gaps = sorted(((min(distances(t, m) for m in mine), t["name"], t["features"]) for t in theirs), reverse=True)
        gaps = [g for g in gaps if g[0] > far]

        lines.append(f"| {category} | {len(mine)} | {np.median(mine_pairs):.3f} | {np.median(their_pairs):.3f} | "
                     f"{len(duplicates)} | {sum(len(c) for c in crowds)} | {len(gaps)} |")

        details += [f"## {category}", ""]
        details.append(f"Library spread {np.median(mine_pairs):.3f} against {np.median(their_pairs):.3f} in the references "
                       f"({100 * np.median(mine_pairs) / np.median(their_pairs):.0f} %).")
        details.append("")
        if crowds:
            details.append("**Crowds** (presets that sound alike):")
            details += [f"- {', '.join(c)}" for c in crowds]
            details.append("")
        if duplicates:
            details.append("**Closest pairs:**")
            details += [f"- {a} / {b} ({d:.3f})" for d, a, b in duplicates[:12]]
            details.append("")
        if gaps:
            details.append("**Gaps** (reference sounds with nothing like them in the library):")
            for d, name, f in gaps[:12]:
                if f:
                    desc = (f"{f['centroid_hz']:.0f} Hz bright, width {f['width']:.2f}, movement {f['movement']:.2f}, "
                            f"attack {f['attack_s'] * 1000:.0f} ms, sustain {f['sustain_db']:.0f} dB")
                else:
                    desc = ""
                details.append(f"- {name} ({d:.3f}): {desc}")
            details.append("")

    (root / "diversity.md").write_text("\n".join(lines + [""] + details), encoding="utf-8")
    print("\n".join(lines))
    print(f"wrote {root / 'diversity.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
