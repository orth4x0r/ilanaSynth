#!/usr/bin/env python3
"""Layout-literal ratchet (ARCH-2).

Counts the raw numbers passed to layout calls (removeFrom*, withTrimmed*,
reduced, expanded, withHeight, withWidth, withSizeKeepingCentre, withX, ...)
in src/gui, src/PluginEditor.cpp and src/PluginEditorHeader.cpp, per file, and compares them with the
committed tools/layout_literals.json. A file whose count rises fails; a file
whose count falls passes (and says so: run with --update to lower the
committed number). Use a token from IlanaTheme::Space (Space::s8, Space::cardPad...)
instead of a number where one is equal, or tag a number that has no token
"// off-grid" and, if it is really needed, raise the committed count with
--update in the same commit. Zero is not counted.

    tools/layout_literal_ratchet.py            check
    tools/layout_literal_ratchet.py --update   write the current counts
    tools/layout_literal_ratchet.py --list FILE   show the counted lines
"""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tools" / "layout_literals.json"

CALL = re.compile(
    r"\b(?:removeFrom(?:Top|Bottom|Left|Right)|withTrimmed(?:Top|Bottom|Left|Right)|reduced|expanded"
    r"|withHeight|withWidth|withSizeKeepingCentre|withX|withY|withLeft|withRight|withTop|withBottom"
    r"|translated)\s*\(([^()]*)\)")
NUMBER = re.compile(r"^[-+]?\d+(?:\.\d+)?f?$")


def code_part(line):
    """The line without its // comment (a // inside a string is kept)."""
    in_str = False
    i = 0
    while i < len(line):
        c = line[i]
        if c == '"' and (i == 0 or line[i - 1] != "\\"):
            in_str = not in_str
        elif not in_str and line.startswith("//", i):
            return line[:i]
        i += 1
    return line


def literals(line):
    found = []
    for m in CALL.finditer(code_part(line)):
        for arg in m.group(1).split(","):
            arg = arg.strip()
            if NUMBER.match(arg) and float(arg.rstrip("f")) != 0:
                found.append(arg)
    return found


def files():
    paths = sorted((ROOT / "src" / "gui").rglob("*.h")) + sorted((ROOT / "src" / "gui").rglob("*.cpp"))
    paths.append(ROOT / "src" / "PluginEditor.cpp")
    paths.append(ROOT / "src" / "PluginEditorHeader.cpp")
    return [p for p in paths if p.exists()]


def count(path):
    n = 0
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        n += len(literals(line))
    return n


def main(argv):
    if argv[:1] == ["--list"]:
        p = ROOT / argv[1]
        for i, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            if literals(line):
                print(f"{i}: {line.strip()}")
        return 0
    counts = {str(p.relative_to(ROOT)).replace("\\", "/"): count(p) for p in files()}
    counts = {k: v for k, v in counts.items() if v}
    if argv[:1] == ["--update"]:
        BASELINE.write_text(json.dumps(counts, indent=1, sort_keys=True) + "\n")
        print(f"wrote {BASELINE.relative_to(ROOT)}: {len(counts)} files, {sum(counts.values())} literals")
        return 0
    base = json.loads(BASELINE.read_text()) if BASELINE.exists() else {}
    bad = []
    lower = []
    for name, n in sorted(counts.items()):
        was = base.get(name, 0)
        if n > was:
            bad.append(f"{name}: {n} raw layout literals, the committed count is {was}")
        elif n < was:
            lower.append(f"{name}: {n} (committed {was})")
    for line in bad:
        print("FAIL " + line)
    if lower:
        print("note: below the committed count (tools/layout_literal_ratchet.py --update lowers it): " + "; ".join(lower))
    if bad:
        print("Use an IlanaTheme::Space token (Space::s8, Space::cardPad ...) or, for a value the grid lacks, tag it // off-grid and raise the count with --update.")
        return 1
    print(f"layout literals: {sum(counts.values())} in {len(counts)} files, none above the committed counts")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
