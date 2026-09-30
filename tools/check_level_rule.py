#!/usr/bin/env python3
"""The filter overhaul's level rule on two fingerprint CSVs.

Every preset's rms_db must stay within +-1 dB of the baseline and its peak
must not rise more than 1 dB above the baseline's. Presets whose rows are
identical are counted as unchanged.

    python3 tools/check_level_rule.py before.csv after.csv [--tol 1.0]

Exits 1 when a preset breaks the rule.
"""
import csv
import math
import sys


def load(path):
    with open(path, newline="") as handle:
        return {row["name"]: row for row in csv.DictReader(handle)}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tol = 1.0
    if "--tol" in sys.argv:
        tol = float(sys.argv[sys.argv.index("--tol") + 1])
        args = [a for a in args if a != sys.argv[sys.argv.index("--tol") + 1]]
    before, after = load(args[0]), load(args[1])
    changed = broken = 0
    for name, old in before.items():
        new = after.get(name)
        if new is None:
            continue
        if all(old[k] == new[k] for k in ("rms_db", "peak", "centroid_hz", "side_ratio")):
            continue
        changed += 1
        rms = float(new["rms_db"]) - float(old["rms_db"])
        old_peak, new_peak = float(old["peak"]), float(new["peak"])
        peak = 20 * math.log10(max(new_peak, 1e-9) / max(old_peak, 1e-9))
        centroid = float(new["centroid_hz"]) / max(float(old["centroid_hz"]), 1.0)
        flag = abs(rms) > tol or peak > tol
        if flag:
            broken += 1
        print(f"{'BREAKS' if flag else 'ok    '} {name}: rms {rms:+.2f} dB, peak {peak:+.2f} dB, centroid x{centroid:.2f}")
    print(f"{changed} presets changed, {broken} break the level rule")
    sys.exit(1 if broken else 0)


if __name__ == "__main__":
    main()
