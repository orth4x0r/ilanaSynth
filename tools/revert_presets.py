"""Puts presets' voicing lines and level trims back to how they were at a commit.

    python tools/revert_presets.py <commit> "Preset A" "Preset B" ...

For each name: the line in src/PresetVoicing.h becomes the commit's line (or
is removed if the commit had none), and its entry in src/PresetTrims.h
becomes the commit's entry. Used after an A/B vote preferred the old sound.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import bake_voicing  # noqa: E402


def show(commit, path):
    return subprocess.run(["git", "show", f"{commit}:{path}"], cwd=ROOT, capture_output=True, text=True,
                          encoding="utf-8", check=True).stdout


def voicing_lines(header_text):
    # Older headers kept the table as one literal, not the chunk array.
    header_text = header_text.replace("\r\n", "\n")
    match = bake_voicing.CHUNKS.search(header_text)
    block = match.group(2) if match else header_text
    return "".join(bake_voicing.CHUNK.findall(block)).splitlines()


def name_of(line):
    return line.split("|")[0].strip() if "|" in line and not line.startswith("#") else None


def main():
    commit, names = sys.argv[1], sys.argv[2:]
    old = {name_of(l): l for l in voicing_lines(show(commit, "src/PresetVoicing.h")) if name_of(l)}
    current = voicing_lines((ROOT / "src" / "PresetVoicing.h").read_text(encoding="utf-8"))
    out, seen = [], set()
    for line in current:
        n = name_of(line)
        if n in names:
            seen.add(n)
            if n in old:
                out.append(old[n])
            continue
        out.append(line)
    for n in names:
        if n not in seen and n in old:
            out.append(old[n])
    temp = ROOT / "build" / "voicing-revert.txt"
    temp.write_text("\n".join(out) + "\n", encoding="utf-8")
    bake_voicing.HEADER.write_text(bake_voicing.HEADER.read_text(encoding="utf-8"), encoding="utf-8")
    sys.argv = ["bake_voicing.py", str(temp)]
    bake_voicing.main()

    trims_path = ROOT / "src" / "PresetTrims.h"
    trims = trims_path.read_text(encoding="utf-8")
    old_trims = show(commit, "src/PresetTrims.h")
    for n in names:
        pattern = re.compile(r'\{ "' + re.escape(n) + r'", [^\n]*\},')
        before = pattern.search(old_trims)
        now = pattern.search(trims)
        if before and now:
            trims = trims[: now.start()] + before.group(0) + trims[now.end():]
        print(f"{n}: voicing {'restored' if n in old else 'removed'}, trim {'restored' if before and now else 'unchanged'}")
    trims_path.write_text(trims, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
