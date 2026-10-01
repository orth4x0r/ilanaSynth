"""Matches new preset voicings' loudness to the presets they replace.

    python tools/voicing_level_match.py <old render dir> <new render dir> <new lines.txt> [names...]

Both render dirs come from ilanaPresetRender (index.csv plus <index>/note.wav
and chord.wav). For each named preset (default: every line in the file) the
RMS over the held part of note.wav and chord.wav is compared, and the line
gets master+=<difference in dB> so the preset keeps its fitted level.
Rewrites the lines file in place.
"""

import csv
import sys
from pathlib import Path

import numpy as np
import soundfile as sf


def loudness(folder):
    total = []
    for name in ("note.wav", "chord.wav"):
        x, rate = sf.read(folder / name, always_2d=True)
        held = x[: int(2.0 * rate)]
        total.append(20 * np.log10(np.sqrt(np.mean(held ** 2)) + 1e-9))
    return float(np.mean(total))


def indices(folder):
    with open(folder / "index.csv", newline="", encoding="utf-8") as f:
        return {row["name"]: row["index"] for row in csv.DictReader(f)}


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    old_dir, new_dir, lines_path = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    old_index, new_index = indices(old_dir), indices(new_dir)
    lines = lines_path.read_text(encoding="utf-8").splitlines()
    wanted = set(sys.argv[4:])
    out = []
    for line in lines:
        name = line.split("|")[0].strip() if "|" in line else ""
        if name and (not wanted or name in wanted) and name in old_index and name in new_index:
            diff = loudness(old_dir / old_index[name]) - loudness(new_dir / new_index[name])
            head, _, rest = line.partition("|")
            params, bar, macros = rest.partition("|")
            params = " ".join(t for t in params.split() if not t.startswith("master+=")) + f" master+={diff:.2f} "
            line = head + "| " + params + bar + macros
            print(f"{name}: {diff:+.2f} dB")
        out.append(line)
    lines_path.write_text("\n".join(out) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
