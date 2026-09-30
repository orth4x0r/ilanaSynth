"""Writes src/PresetTrims.h from the preset critic's measurements.

    ilanaPresetRender build/critic-full          (renders with the current trims)
    python tools/preset_critic.py build/critic-full [--refs build/critic-refs ...]
    python tools/tune_presets.py build/critic-full

then rebuild and repeat once or twice: each pass corrects what the last one
left, because level and macro effect are not linear in the trims (the soft
clipper, drives, and macro depths that saturate at +-1).

- Level: each preset is moved to its category's loudness (the median of the
  library's own presets in that category, so the library as a whole stays as
  loud as it was), when it is more than 1.5 dB off. The trim is added to the
  preset's master level.
- Macros: a macro whose effect scores under 1.0 has every routing it drives
  scaled up (by up to 4x a pass, 8x in all); depths still stop at +-1, so a
  macro that drives something inaudible stays weak and is listed for a look.
"""

import csv
import re
import sys
from pathlib import Path

import numpy as np

HEADER = Path(__file__).resolve().parent.parent / "src" / "PresetTrims.h"
SKIP_CATEGORIES = {"FX Input", "Init"}
LEVEL_TOLERANCE_DB = 1.5
MACRO_THRESHOLD = 1.0
MAX_MACRO_SCALE = 8.0


def read_report(root):
    with open(root / "report.csv", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def read_trims():
    trims = {}
    if not HEADER.exists():
        return trims
    for match in re.finditer(r'\{ "((?:[^"\\]|\\.)*)", (-?[\d.]+)f, \{ (-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f \} \}',
                             HEADER.read_text(encoding="utf-8")):
        name = match.group(1).replace('\\"', '"')
        trims[name] = [float(match.group(2)), [float(match.group(k)) for k in range(3, 7)]]
    return trims


def category_targets(rows):
    targets = {}
    for category in {r["category"] for r in rows}:
        own = [float(r["level_db"]) for r in rows if r["category"] == category and float(r["level_db"]) > -45]
        if own:
            targets[category] = float(np.median(own))
    return targets


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    root = Path(sys.argv[1])
    rows = read_report(root)
    trims = read_trims()
    targets = category_targets(rows)
    stubborn = []

    for row in rows:
        name, category = row["name"], row["category"]
        if category in SKIP_CATEGORIES or float(row["level_db"]) < -45:
            continue
        level, scales = trims.get(name, [0.0, [1.0, 1.0, 1.0, 1.0]])

        off = float(row["level_db"]) - targets.get(category, float(row["level_db"]))
        if abs(off) > LEVEL_TOLERANCE_DB:
            level = round(float(np.clip(level - off, -24.0, 12.0)), 2)

        for k in range(4):
            effect = float(row[f"macro{k + 1}_effect"])
            if effect < MACRO_THRESHOLD:
                if scales[k] >= MAX_MACRO_SCALE:
                    stubborn.append(f"{name} macro {k + 1}")
                    continue
                factor = float(np.clip(2.0 * MACRO_THRESHOLD / max(effect, 0.05), 1.5, 4.0))
                scales[k] = round(float(min(MAX_MACRO_SCALE, scales[k] * factor)), 3)

        if level != 0.0 or any(s != 1.0 for s in scales):
            trims[name] = [level, scales]

    def literal(text):
        return text.replace("\\", "\\\\").replace('"', '\\"')

    lines = [
        "#pragma once",
        "",
        "// Per-preset trims from the preset critic (tools/tune_presets.py writes this",
        "// file; don't edit it by hand). levelDb is added to the preset's master level",
        "// so each preset sits at its category's loudness; macroScale multiplies the",
        "// depth of every routing a macro drives, so each macro audibly changes the",
        "// sound. Presets that aren't listed load exactly as written.",
        "",
        "#include <vector>",
        "",
        "namespace Presets",
        "{",
        "struct Trim",
        "{",
        "    const char* name;",
        "    float levelDb;",
        "    float macroScale[4];",
        "};",
        "",
        "inline const std::vector<Trim>& getTrims()",
        "{",
        "    static const std::vector<Trim> trims {",
    ]
    for name in sorted(trims):
        level, scales = trims[name]
        lines.append(f'        {{ "{literal(name)}", {level:.2f}f, {{ {scales[0]:.3f}f, {scales[1]:.3f}f, {scales[2]:.3f}f, {scales[3]:.3f}f }} }},')
    lines += ["    };", "    return trims;", "}", "} // namespace Presets", ""]
    HEADER.write_text("\n".join(lines), encoding="utf-8")

    levelled = sum(1 for t in trims.values() if t[0] != 0.0)
    scaled = sum(1 for t in trims.values() if any(s != 1.0 for s in t[1]))
    print(f"targets (dB): " + ", ".join(f"{c} {v:.1f}" for c, v in sorted(targets.items())))
    print(f"wrote {HEADER}: {len(trims)} presets trimmed ({levelled} levels, {scaled} with scaled macros)")
    if stubborn:
        print("macros still weak at the 8x cap (look at these by hand): " + "; ".join(stubborn))
    return 0


if __name__ == "__main__":
    sys.exit(main())
