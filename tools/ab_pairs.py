#!/usr/bin/env python3
"""Before / after A/B clips for the presets a voicing pass changed.

    python3 tools/ab_pairs.py <before render root> <after render root> <out folder> <changes.json>

The roots are ilanaPresetRender folders (index.csv and <index>/chord.wav, line.wav). changes.json maps a
preset name to {"change": "what changed", ...}. Writes <out>/<Category>/<Name>-before.wav and -after.wav
(mono, 16-bit, 22.05 kHz, the first 3.5 s: chord for Pad / Keys, the eight-note line for the others, both
matched to -20 dB RMS with the peak kept under -1 dB, 10 ms fades) and <out>/manifest.json:
{category: [{index, name, change, before, after, length}]}.
"""
import csv
import json
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ab_clips import resample  # noqa: E402
from preset_critic import read_wav  # noqa: E402

CLIP = {"Pad": "chord.wav", "Keys": "chord.wav", "Bass": "line.wav", "Lead": "line.wav", "Pluck": "line.wav"}
OUT_RATE = 22050
SECONDS = 3.5


def clip(root, row):
    left, right = read_wav(root / row["index"] / CLIP[row["category"]])
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


def main():
    before, after, out, changes = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), json.load(open(sys.argv[4]))
    rows_before = {r["name"]: r for r in csv.DictReader(open(before / "index.csv", newline="", encoding="utf-8"))}
    rows_after = {r["name"]: r for r in csv.DictReader(open(after / "index.csv", newline="", encoding="utf-8"))}
    manifest = {}
    for name, info in changes.items():
        if name not in rows_before or name not in rows_after:
            continue
        category = rows_after[name]["category"]
        a, b = clip(before, rows_before[name]), clip(after, rows_after[name])
        if a is None or b is None:
            print("silent clip, skipped:", name)
            continue
        folder = out / category
        folder.mkdir(parents=True, exist_ok=True)
        safe = "".join(c if c.isalnum() else "_" for c in name)
        write(folder / f"{safe}-before.wav", a)
        write(folder / f"{safe}-after.wav", b)
        manifest.setdefault(category, []).append({
            "index": int(rows_after[name]["index"]), "name": name, "change": info["change"],
            "before": f"{category}/{safe}-before.wav", "after": f"{category}/{safe}-after.wav",
            "length": round(len(a) / OUT_RATE, 2)})
    for entries in manifest.values():
        entries.sort(key=lambda e: e["index"])
    (out / "manifest.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    print(sum(len(v) for v in manifest.values()), "pairs written to", out)


if __name__ == "__main__":
    main()
