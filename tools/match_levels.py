#!/usr/bin/env python3
"""Moves presets' level trims so the level rule holds against a baseline.

    ilanaFingerprint build/after.csv
    python3 tools/match_levels.py tests/fingerprints-linux.csv build/after.csv [--names "A,B"] [--apply]

For each preset whose rms differs from the baseline's by more than 0.25 dB (or
whose peak is more than 0.75 dB above), computes the level trim (dB, added to
the preset's master level) that would bring the rms back to the baseline's,
lowered where needed to keep the peak within the rule (peak <= old + 1 dB),
and with --apply rewrites those trims in src/PresetTrims.h (macro scales are
kept). The fingerprints are not linear in the trim (the output soft clipper),
so rebuild and repeat until tools/check_level_rule.py passes.
Complements tools/tune_presets.py, which levels by category.
"""
import csv
import math
import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parent.parent / "src" / "PresetTrims.h"
ROW = re.compile(r'\{ "((?:[^"\\]|\\.)*)", (-?[\d.]+)f, \{ (-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f \} \}')


def load(path):
    with open(path, newline="") as handle:
        return {row["name"]: row for row in csv.DictReader(handle)}


def db(x):
    return 20 * math.log10(max(x, 1e-9))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    only = None
    if "--names" in sys.argv:
        only = set(sys.argv[sys.argv.index("--names") + 1].split(","))
        args = [a for a in args if a != sys.argv[sys.argv.index("--names") + 1]]
    before, after = load(args[0]), load(args[1])
    text = HEADER.read_text(encoding="utf-8")
    trims = {}
    for m in ROW.finditer(text):
        trims[m.group(1).replace('\\"', '"')] = [float(m.group(2)), [float(m.group(k)) for k in range(3, 7)]]

    moved = []
    for name, old in before.items():
        new = after.get(name)
        if new is None or (only and name not in only):
            continue
        d_rms = float(new["rms_db"]) - float(old["rms_db"])
        d_peak = db(float(new["peak"])) - db(float(old["peak"]))
        if abs(d_rms) <= 0.5 and d_peak <= 0.8:
            continue
        step = -d_rms
        if d_peak + step > 0.8:  # keep the peak inside old + 1 dB
            step = 0.8 - d_peak
        if abs(step) < 0.05:
            continue
        level, scales = trims.get(name, [0.0, [1.0, 1.0, 1.0, 1.0]])
        new_level = max(-24.0, min(12.0, round(level + step, 2)))
        trims[name] = [new_level, scales]
        moved.append(f"{name}: {level:+.2f} -> {new_level:+.2f} (rms {d_rms:+.2f}, peak {d_peak:+.2f})")
    print("\n".join(moved))
    print(f"{len(moved)} trims to move")
    if "--apply" in sys.argv and moved:
        def literal(t):
            return t.replace("\\", "\\\\").replace('"', '\\"')
        body = "\n".join(
            f'        {{ "{literal(n)}", {l:.2f}f, {{ {s[0]:.3f}f, {s[1]:.3f}f, {s[2]:.3f}f, {s[3]:.3f}f }} }},'
            for n, (l, s) in sorted(trims.items()) if l != 0.0 or any(x != 1.0 for x in s))
        start = text.index("static const std::vector<Trim> trims {") + len("static const std::vector<Trim> trims {")
        end = text.index("    };", start)
        HEADER.write_text(text[:start] + "\n" + body + "\n" + text[end:], encoding="utf-8")
        print("wrote", HEADER)
    return 0


if __name__ == "__main__":
    sys.exit(main())
