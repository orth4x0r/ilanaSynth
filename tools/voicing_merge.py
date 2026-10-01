"""Lays new voicing lines over a voicing file: a line for a preset already in
the file replaces it, a new one is appended.

    python tools/voicing_merge.py <base.txt> <new lines.txt> [...] -o <out.txt> [--dry]

--dry adds fx_slot1..10=0 to the merged lines (no effects), for comparing a
render with a dry reference.
"""

import argparse
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base")
    ap.add_argument("new", nargs="+")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--dry", action="store_true")
    args = ap.parse_args()
    lines = Path(args.base).read_text(encoding="utf-8").splitlines()
    for path in args.new:
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if "|" not in line or line.startswith("#"):
                continue
            name = line.split("|")[0].strip()
            if args.dry:
                head, _, rest = line.partition("|")
                params, bar, macros = rest.partition("|")
                # Slot 1 holds the Utility (unity by default): with every slot
                # empty, a preset load brings back the legacy FX chain.
                params += " arp_on=0 fx_slot1=19 " + " ".join(f"fx_slot{i}=0" for i in range(2, 11)) + " "
                line = head + "|" + params + bar + macros
            replaced = False
            for i, old in enumerate(lines):
                if "|" in old and not old.startswith("#") and old.split("|")[0].strip() == name:
                    lines[i] = line
                    replaced = True
            if not replaced:
                lines.append(line)
    Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
