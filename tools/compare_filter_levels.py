#!/usr/bin/env python3
"""Compare two filter level tables (ILANA_FILTER_LEVELS=1 output).

Usage: compare_filter_levels.py before.txt after.txt [--tol 1.0]
Prints every type/slope row where a cell moved more than the tolerance.
"""
import re
import sys

NAMES = ["Low Pass", "Band Pass", "High Pass", "Notch", "Ladder LP", "Ladder HP", "Diode LP", "MS-20 LP",
         "Comb +", "Comb -", "Formant", "Morph", "Ladder BP", "Ladder Drive", "SEM", "OTA LP", "OTA BP",
         "MS-20 HP", "Steiner", "Phaser Notch", "Comb Damped", "Comb Morph", "Vowel", "Talking", "Twin Peak",
         "303 Acid", "Moog Drive", "Vowel Morph", "Comb Body"]
CELLS = [f"{c}/{r}" for c in (300, 1000, 3000) for r in (0, 0.5, 0.9)]


def load(path):
    rows = {}
    for line in open(path):
        m = re.match(r"LEVEL \{ (\d+), (\d+), (.*) \},", line.strip())
        if m:
            rows[(int(m.group(1)), int(m.group(2)))] = [float(v) for v in m.group(3).split(",")]
    return rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tol = float(sys.argv[sys.argv.index("--tol") + 1]) if "--tol" in sys.argv else 1.0
    if "--tol" in sys.argv:
        args.remove(sys.argv[sys.argv.index("--tol") + 1])
    before, after = load(args[0]), load(args[1])
    bad = 0
    for key in sorted(before):
        if key not in after:
            continue
        diffs = [a - b for a, b in zip(after[key], before[key])]
        worst = max(abs(d) for d in diffs)
        if worst > tol:
            bad += 1
            name = NAMES[key[0]] if key[0] < len(NAMES) else str(key[0])
            cells = " ".join(f"{c}:{d:+.2f}" for c, d in zip(CELLS, diffs) if abs(d) > tol)
            print(f"{name} slope{'24' if key[1] else '12'}: {cells}")
    print(f"{bad} rows outside +-{tol} dB")


if __name__ == "__main__":
    main()
