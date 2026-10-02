"""Checks the DX7 presets (oscillators on the Operator EG) against the msfa (Dexed) engine, voice by voice.

    python tools/dx7_mode_check.py <bank.syx> <first preset index> [--voices 1-32]

Renders each voice's reference with build/reference/dx7/render/dx7render.exe
(C4, held 2 s of 3.5, velocity 100, transpose applied) and ilanaSynth's
factory preset at <first index> + voice - 1 with ilanaPresetRender (note.wav
plays C4 for the same times), then prints the envelope and spectrum errors
from dx7_compare.py.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RENDER = ROOT / "build" / "reference" / "dx7" / "render" / "dx7render.exe"
PRESET = ROOT / "build" / "ilanaPresetRender_artefacts" / "Release" / "ilanaPresetRender.exe"
OUT = ROOT / "build" / "dx7mode"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("bank")
    ap.add_argument("first", type=int)
    ap.add_argument("--voices", default="1-32")
    args = ap.parse_args()
    lo, hi = (int(x) for x in args.voices.split("-"))
    bank = Path(args.bank).read_bytes()
    worst = []
    for voice in range(lo, hi + 1):
        p = bank[6 + 128 * (voice - 1): 6 + 128 * voice]
        name = bytes(p[118:128]).decode("ascii", "replace").strip()
        transpose = p[117] - 24
        ref = OUT / f"ref_{Path(args.bank).stem}_{voice}.wav"
        subprocess.run([str(RENDER), args.bank, str(voice), str(60 + transpose), "100", "2", "3.5", str(ref)], check=True)
        index = args.first + voice - 1
        subprocess.run([str(PRESET), str(OUT), str(index), "1"], check=True, capture_output=True)
        result = subprocess.run([sys.executable, str(ROOT / "tools" / "dx7_compare.py"), str(ref), str(OUT / str(index) / "note.wav")],
                                capture_output=True, text=True).stdout
        level = re.search(r"needs ([+-][\d.]+) dB", result)
        env = re.search(r"envelope error .*: ([\d.]+)", result)
        harm = re.search(r"harmonic error .*: ([\d.]+)", result)
        line = f"{voice:2d} {name:10s} level {level.group(1) if level else '?':>6} env {env.group(1) if env else '?':>6} harm {harm.group(1) if harm else '?':>6}"
        print(line, flush=True)
        if harm:
            worst.append((float(harm.group(1)), line))
    worst.sort(reverse=True)
    print("worst:", *[w[1] for w in worst[:5]], sep="\n  ")
    return 0


if __name__ == "__main__":
    sys.exit(main())
