"""Bakes a voicing file into src/PresetVoicing.h (the built-in table).

    ILANA_PRESET_VOICING=build/voicing.txt ilanaPresetRender build/x   (iterate)
    python tools/bake_voicing.py build/voicing.txt                     (bake)
    python tools/bake_voicing.py --extract build/voicing.txt           (header -> file)

The format is described at the top of src/PresetVoicing.h. The table is
written as several raw string literals of at most CHUNK_BYTES each: MSVC
refuses a string literal over 16 KB (error C2026).
"""

import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "src" / "PresetVoicing.h"
CHUNKS = re.compile(r'(inline const char\* const voicingChunks\[\] = \{\n)(.*?)(\n\};)', re.S)
CHUNK = re.compile(r'R"VOICING\(\n(.*?)\)VOICING"', re.S)
CHUNK_BYTES = 12000


def split_chunks(body):
    chunks, current = [], ""
    for line in body.splitlines(keepends=True):
        if current and len((current + line).encode("utf-8")) > CHUNK_BYTES:
            chunks.append(current)
            current = ""
        current += line
    if current:
        chunks.append(current)
    return chunks


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    text = HEADER.read_text(encoding="utf-8")
    if args[0] == "--extract":
        block = CHUNKS.search(text).group(2)
        Path(args[1]).write_text("".join(CHUNK.findall(block)), encoding="utf-8")
        return 0
    lines = [l.rstrip() for l in Path(args[0]).read_text(encoding="utf-8").splitlines()]
    body = "\n".join(re.sub(r" +", " ", l) for l in lines).strip("\n") + "\n"
    block = "\n".join(f'R"VOICING(\n{chunk})VOICING",' for chunk in split_chunks(body))
    HEADER.write_text(CHUNKS.sub(lambda m: m.group(1) + block + m.group(3), text), encoding="utf-8")
    print(f"baked {sum(1 for l in lines if '|' in l and not l.startswith('#'))} voicings into {HEADER.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
