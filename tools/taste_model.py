#!/usr/bin/env python3
"""ilana's taste model: ridge regression on CLAP audio embeddings, trained on her A/B votes.

    python tools/taste_model.py eval  <dataset.jsonl> <render folder> [more render folders...]
    python tools/taste_model.py score <dataset.jsonl> <target render folder> <render folders...> [--alpha 3]

A dataset row is one vote (a, b, choice); each side's embedding is found by matching its
params text against the render folders' params.txt. Item labels: winner +1, loser -1,
both good +0.5, both bad -1. "eval" reports leave-one-pair-out accuracy on decisive pairs;
"score" fits on everything and writes taste_score.csv (index, name, category, taste) in
the target folder.
"""

import csv
import json
import sys
from pathlib import Path

import numpy as np
from sklearn.decomposition import PCA
from sklearn.linear_model import Ridge


def embeddings(roots):
    """params text -> unit embedding, over every render folder."""
    table = {}
    for root in map(Path, roots):
        rows = list(csv.DictReader(open(root / "index.csv", newline="", encoding="utf-8")))
        emb = np.load(root / "clap_embeddings.npy")
        emb = emb / np.linalg.norm(emb, axis=1, keepdims=True)
        for k, row in enumerate(rows):
            p = root / row["index"] / "params.txt"
            if p.exists():
                table.setdefault(p.read_text(encoding="utf-8").strip(), emb[k])
    return table


def items(dataset, table):
    labels = {"A": (1.0, -1.0), "B": (-1.0, 1.0), "both good": (0.5, 0.5), "both bad": (-1.0, -1.0)}
    rows = [json.loads(line) for line in open(dataset, encoding="utf-8")]
    pairs, missing = [], 0
    for r in rows:
        ea, eb = table.get(r["a"]["params"].strip()), table.get(r["b"]["params"].strip())
        if ea is None or eb is None:
            missing += 1
            continue
        la, lb = labels[r["choice"]]
        pairs.append((ea, eb, la, lb, r["choice"], r["category"]))
    return pairs, missing


def fit(pairs, alpha, comps=24):
    x = np.array([p[0] for p in pairs] + [p[1] for p in pairs])
    y = np.array([p[2] for p in pairs] + [p[3] for p in pairs])
    pca = PCA(n_components=min(comps, len(x) - 1)).fit(x)
    model = Ridge(alpha=alpha).fit(pca.transform(x), y)
    return lambda e: model.predict(pca.transform(np.atleast_2d(e)))


def main():
    mode, dataset, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    alpha = 3.0
    if "--alpha" in rest:
        k = rest.index("--alpha")
        alpha = float(rest[k + 1])
        rest = rest[:k] + rest[k + 2:]
    target = rest[0] if mode == "score" else None
    table = embeddings(rest if mode == "eval" else rest[1:])
    pairs, missing = items(dataset, table)
    print(len(pairs), "pairs with embeddings,", missing, "without")
    if mode == "eval":
        decisive = [i for i, p in enumerate(pairs) if p[4] in ("A", "B")]
        for a in (1, 3, 10, 30):
            hit = 0
            for i in decisive:
                f = fit([p for j, p in enumerate(pairs) if j != i], a)
                pa, pb = f(pairs[i][0])[0], f(pairs[i][1])[0]
                hit += (pa > pb) == (pairs[i][4] == "A")
            print(f"alpha {a}: {hit}/{len(decisive)} decisive pairs predicted")
    else:
        f = fit(pairs, alpha)
        root = Path(target)
        rows = list(csv.DictReader(open(root / "index.csv", newline="", encoding="utf-8")))
        emb = np.load(root / "clap_embeddings.npy")
        emb = emb / np.linalg.norm(emb, axis=1, keepdims=True)
        scores = f(emb)
        with open(root / "taste_score.csv", "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["index", "name", "category", "taste"])
            for r, s in zip(rows, scores):
                w.writerow([r["index"], r["name"], r["category"], f"{s:.4f}"])
        print("wrote", root / "taste_score.csv")


if __name__ == "__main__":
    main()
