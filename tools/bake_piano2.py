"""M8.2: bake the fitted Piano constants into src/dsp/PianoModelTuning.h.

usage: python tools/bake_piano2.py build/fit82/best-board.json [build/fit82/extra.txt]

Tuning values replace the defaults in the header ("name = value" inside the
struct); boardEq comes from the extra file. Preset values (osc1_*, sb_*) are
printed for the Grand Piano preset in src/PresetLibrary.h.
"""
import json
import re
import sys

HEADER = "src/dsp/PianoModelTuning.h"


def literal(value):
    text = f"{value:.6g}"
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def main():
    values = json.load(open(sys.argv[1]))
    extra = open(sys.argv[2]).read().strip() if len(sys.argv) > 2 else ""
    text = open(HEADER).read()
    for name, value in values.items():
        if "_" in name:
            print(f"preset {name} = {value:.4g}")
            continue
        pattern = re.compile(r"(\b" + re.escape(name) + r"\s*=\s*)(-?[0-9.eE+-]+)f")
        text, count = pattern.subn(lambda m: m.group(1) + literal(value), text, count=1)
        if count == 0:
            print("not found:", name)
    if extra:
        bands = [float(item.split("=")[1]) for item in extra.split(";")]
        text = re.sub(r"std::array<float, 10> boardEq \{[^}]*\};",
                      "std::array<float, 10> boardEq { " + ", ".join(f"{b:.2f}f" for b in bands) + " };", text)
    open(HEADER, "w").write(text)


if __name__ == "__main__":
    main()
