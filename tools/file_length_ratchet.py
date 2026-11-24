#!/usr/bin/env python3
"""File-length ratchet (architecture review A4).

Records the line count of the largest source files (src/**/*.cpp, *.h and
tools/*.cpp) in tools/file-length-ratchet.txt ("<count> <path>" per line).
The check fails when

  * a listed file has grown more than SLACK (50) lines past its recorded count, or
  * a file that is not listed is longer than MAX_UNLISTED (3000) lines.

A file that shrinks passes; run with --update to lower the recorded count.
Split a file that grows instead of raising its number.

    tools/file_length_ratchet.py            check
    tools/file_length_ratchet.py --update   rewrite tools/file-length-ratchet.txt
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tools" / "file-length-ratchet.txt"
SLACK = 50
MAX_UNLISTED = 3000
# Files at or above this length are written to the baseline by --update.
LISTING_FLOOR = 1500


def files():
    paths = []
    for pattern in ("*.cpp", "*.h"):
        paths += (ROOT / "src").rglob(pattern)
    paths += (ROOT / "tools").glob("*.cpp")
    return sorted(set(paths))


def line_count(path):
    with open(path, "rb") as f:
        return sum(1 for _ in f)


def rel(path):
    return str(path.relative_to(ROOT)).replace("\\", "/")


def read_baseline():
    base = {}
    if BASELINE.exists():
        for line in BASELINE.read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            n, name = line.split(None, 1)
            base[name] = int(n)
    return base


def main(argv):
    counts = {rel(p): line_count(p) for p in files()}
    if argv[:1] == ["--update"]:
        listed = {k: v for k, v in counts.items() if v >= LISTING_FLOOR}
        lines = ["# Max line counts (file-length ratchet; tools/file_length_ratchet.py --update rewrites this).",
                 f"# A listed file may grow {SLACK} lines past its count; an unlisted file may not pass {MAX_UNLISTED}."]
        lines += [f"{v} {k}" for k, v in sorted(listed.items(), key=lambda kv: (-kv[1], kv[0]))]
        BASELINE.write_text("\n".join(lines) + "\n")
        print(f"wrote {rel(BASELINE)}: {len(listed)} files")
        return 0
    base = read_baseline()
    bad, lower = [], []
    for name, n in sorted(counts.items()):
        if name in base:
            if n > base[name] + SLACK:
                bad.append(f"{name}: {n} lines, recorded {base[name]} (+{SLACK} slack): split it")
            elif n < base[name]:
                lower.append(f"{name}: {n} (recorded {base[name]})")
        elif n > MAX_UNLISTED:
            bad.append(f"{name}: {n} lines, over {MAX_UNLISTED} and not in {rel(BASELINE)}: split it")
    for name in sorted(base):
        if name not in counts:
            lower.append(f"{name}: gone")
    for line in bad:
        print("FAIL " + line)
    if lower:
        print("note: below the recorded count (tools/file_length_ratchet.py --update lowers it): " + "; ".join(lower))
    if bad:
        return 1
    print(f"file length: {len(counts)} files, none grown past its recorded count or over {MAX_UNLISTED} lines")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
