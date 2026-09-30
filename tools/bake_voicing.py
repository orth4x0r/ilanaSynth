"""Bakes a voicing file into src/PresetVoicing.h (the built-in table).

    ILANA_PRESET_VOICING=build/voicing.txt ilanaPresetRender build/x   (iterate)
    python tools/bake_voicing.py build/voicing.txt                     (bake)
    python tools/bake_voicing.py --extract build/voicing.txt           (header -> file)

The format is described at the top of src/PresetVoicing.h.
"""

import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "src" / "PresetVoicing.h"
PATTERN = re.compile(r'(R"VOICING\(\n)(.*?)(\)VOICING")', re.S)


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 1
    text = HEADER.read_text(encoding="utf-8")
    if args[0] == "--extract":
        Path(args[1]).write_text(PATTERN.search(text).group(2), encoding="utf-8")
        return 0
    lines = [l.rstrip() for l in Path(args[0]).read_text(encoding="utf-8").splitlines()]
    body = "\n".join(re.sub(r" +", " ", l) for l in lines).strip("\n") + "\n"
    HEADER.write_text(PATTERN.sub(lambda m: m.group(1) + body + m.group(3), text), encoding="utf-8")
    print(f"baked {sum(1 for l in lines if '|' in l and not l.startswith('#'))} voicings into {HEADER.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
