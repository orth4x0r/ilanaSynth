#!/usr/bin/env python3
"""Picks A/B pairs for ilana's taste vote and writes their listening clips.

    python tools/taste_pairs.py <render folder> <out folder> [--per 8] [--seed 1] [--exclude pairs.json ...]

<render folder> is an ilanaPresetRender folder with clap_embeddings.npy next to
index.csv (tools/clap_score.py writes it). In each of the five main categories
the presets are sampled farthest-first in CLAP space (so the pairs cover the
whole range of sounds, not one crowd) and paired two by two. Writes
<out>/clips/<id>-a.wav, -b.wav (mono, 22.05 kHz, 3.5 s, both matched to -20 dB
RMS) and <out>/pairs.json: [{id, category, a: {index, name}, b: {index, name}}].
Presets named in an --exclude file's pairs are not picked again.
"""

import argparse
import csv
import json
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ab_clips import resample  # noqa: E402
from preset_critic import read_wav  # noqa: E402

CATEGORIES = ["Bass", "Pad", "Lead", "Keys", "Pluck"]
CLIP = {"Pad": "chord.wav", "Keys": "chord.wav", "Bass": "note.wav", "Lead": "note.wav", "Pluck": "note.wav"}
OUT_RATE = 22050
SECONDS = 3.5


def clip(root, index, category):
    left, right = read_wav(root / str(index) / CLIP[category])
    mono = (0.5 * (left + right))[: int(SECONDS * 48000)]
    level = float(np.sqrt(np.mean(mono ** 2)))
    if level < 1e-5:
        return None
    mono = mono * (10 ** (-20 / 20) / level)
    peak = float(np.max(np.abs(mono)))
    if peak > 10 ** (-1 / 20):
        mono *= 10 ** (-1 / 20) / peak
    out = resample(mono, 48000, OUT_RATE)
    fade = int(0.01 * OUT_RATE)
    out[:fade] *= np.linspace(0, 1, fade)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out


def write(path, samples):
    data = np.clip(samples * 32767.0, -32768, 32767).astype("<i2").tobytes()
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(OUT_RATE)
        w.writeframes(data)


def farthest_first(vectors, count, rng):
    chosen = [int(rng.integers(len(vectors)))]
    nearest = 1.0 - vectors @ vectors[chosen[0]]
    while len(chosen) < count:
        nxt = int(np.argmax(nearest))
        chosen.append(nxt)
        nearest = np.minimum(nearest, 1.0 - vectors @ vectors[nxt])
    return chosen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("out")
    ap.add_argument("--per", type=int, default=8)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--exclude", nargs="*", default=[])
    args = ap.parse_args()

    root, out = Path(args.render), Path(args.out)
    rows = list(csv.DictReader(open(root / "index.csv", newline="", encoding="utf-8")))
    emb = np.load(root / "clap_embeddings.npy")
    emb = emb / np.linalg.norm(emb, axis=1, keepdims=True)
    used = set()
    for path in args.exclude:
        for pair in json.load(open(path)):
            used.update((pair["a"]["name"], pair["b"]["name"]))

    (out / "clips").mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)
    pairs = []
    for category in CATEGORIES:
        ids = [i for i, r in enumerate(rows) if r["category"] == category and r["name"] not in used
               and (root / r["index"] / CLIP[category]).exists()]
        usable = [i for i in ids if clip(root, rows[i]["index"], category) is not None]
        picks = farthest_first(emb[usable], 2 * args.per, rng)
        for k in range(args.per):
            a, b = rows[usable[picks[2 * k]]], rows[usable[picks[2 * k + 1]]]
            if rng.random() < 0.5:
                a, b = b, a
            pid = f"{category.lower()}-{k + 1}"
            write(out / "clips" / f"{pid}-a.wav", clip(root, a["index"], category))
            write(out / "clips" / f"{pid}-b.wav", clip(root, b["index"], category))
            pairs.append({"id": pid, "category": category,
                          "a": {"index": int(a["index"]), "name": a["name"]},
                          "b": {"index": int(b["index"]), "name": b["name"]}})
    (out / "pairs.json").write_text(json.dumps(pairs, indent=1), encoding="utf-8")
    print(len(pairs), "pairs in", out)


if __name__ == "__main__":
    main()
