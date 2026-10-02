"""Writes src/Dx7Banks.h: the DX7 cartridges preloaded as factory presets.

    python tools/bake_dx7_banks.py <bank.syx> [...]

Each bank is a 32-voice DX7 bulk dump (4104 bytes: F0 43 0n 09 20 00, 4096
bytes of packed voices, checksum, F7). The bank's label is the file name
without its extension, upper-cased (rom1a.syx -> ROM1A).
"""

import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "src" / "Dx7Banks.h"


def main():
    paths = [Path(p) for p in sys.argv[1:]]
    if not paths:
        print(__doc__)
        return 1
    lines = ["#pragma once", "",
             "// DX7 cartridges preloaded as factory presets (tools/bake_dx7_banks.py).",
             "// Each is the 4096 bytes of 32 packed voices from a bulk dump.", "",
             "#include <cstdint>", "", "namespace Dx7Banks", "{",
             "struct Bank", "{", "    const char* label;", "    const std::uint8_t* data; // 32 x 128 bytes", "};", ""]
    labels = []
    for i, path in enumerate(paths):
        data = path.read_bytes()
        start = data.find(bytes([0xF0, 0x43]))
        if start < 0 or len(data) < start + 6 + 4096 or data[start + 3] != 9:
            print(f"skipping {path}: not a 32-voice bulk dump")
            continue
        voices = data[start + 6: start + 6 + 4096]
        label = path.stem.upper()
        labels.append((label, i))
        lines.append(f"inline const std::uint8_t bank{i}[4096] {{")
        for row in range(0, 4096, 32):
            lines.append("    " + ", ".join(str(b) for b in voices[row: row + 32]) + ",")
        lines.append("};")
        lines.append("")
    lines.append(f"inline const Bank banks[] {{")
    for label, i in labels:
        lines.append(f'    {{ "{label}", bank{i} }},')
    lines.append("};")
    lines.append("} // namespace Dx7Banks")
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(labels)} banks to {OUT.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
