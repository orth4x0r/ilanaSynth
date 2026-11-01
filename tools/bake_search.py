#!/usr/bin/env python3
"""Bakes ilana's kept search winners into src/PresetPackSearch.h (appended after every earlier preset).

    python tools/bake_search.py [--check]

Reads the Keep votes in /mnt/project-files/ilanasynth/search-winners{,-2}/pack.json (written by the audition pages),
names each preset (NAMES below, by pack and id), moves its level to its category's median like tools/preset_gen.py bake
(measured in build/gen/qd{,2}/final-render and build/gen/factory-render) and writes the C++ pack.
"""

import csv
import json
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import preset_gen as pg  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "PresetPackSearch.h"
BASE = Path("/mnt/project-files/ilanasynth")
PACKS = [("search-winners", "qd"), ("search-winners-2", "qd2")]
NAMES = {
    ("search-winners", "lead-1"): "Operator Glass Lead", ("search-winners", "lead-2"): "Product Supersaw",
    ("search-winners", "lead-3"): "Moving Comb Sync", ("search-winners", "lead-6"): "Formant Chorus Lead",
    ("search-winners", "lead-7"): "Formant Operator Lead", ("search-winners", "pad-2"): "Deep Pulse Gate Pad",
    ("search-winners", "pad-3"): "CZ Gate Pad", ("search-winners", "pluck-1"): "Talking Saw Pluck",
    ("search-winners", "pluck-3"): "Cluster Formant Pluck", ("search-winners", "pluck-4"): "Rounded Saw Gate Pluck",
    ("search-winners", "bass-1"): "Vowel Octave Bass", ("search-winners", "bass-2"): "Vowel Pulse Bass",
    ("search-winners", "bass-4"): "Analog 303 Bass", ("search-winners", "bass-5"): "Talking Pulse Bass",
    ("search-winners", "keys-2"): "Scramble Chorus Keys", ("search-winners", "keys-3"): "Plain Hall Keys",
    ("search-winners-2", "r2-lead-1"): "CZ Bitwise Lead", ("search-winners-2", "r2-lead-2"): "Pulse Operator Lead",
    ("search-winners-2", "r2-lead-3"): "Logistic Product Lead", ("search-winners-2", "r2-lead-4"): "Twin Sine Holt Lead",
    ("search-winners-2", "r2-pad-3"): "Talking CZ Pad", ("search-winners-2", "r2-pluck-1"): "Scramble Saw Pluck",
    ("search-winners-2", "r2-pluck-2"): "Organ Saw Pluck", ("search-winners-2", "r2-bass-1"): "Wavecrush Analog Bass",
    ("search-winners-2", "r2-bass-2"): "Vocoder Drive Bass", ("search-winners-2", "r2-bass-3"): "Ladder Triangle Bass",
    ("search-winners-2", "r2-bass-4"): "Vocoder Pulse Bass", ("search-winners-2", "r2-keys-4"): "Tine Vowel Keys",
}


# Gate fixes: routes the tuning test reads as out of tune (an LFO on a physical string's detune makes the pitch
# estimate wander), by preset name and mod destination index.
DROP_DEST = {"Tine Vowel Keys": {36}}
# A second Spaces reverb in slot 3 (the same type as slot 1) hid the tine's pitch from the tuning test (Goertzel peak at
# the +-50 cent edge); with it gone the preset measures 7 cents.
# Bug hunt 2026-11-01: same-type FX slots share one engine, so three AW Modulations (CZ Bitwise Lead) or two AW
# Stereos next to three AW Modulations (Formant Operator Lead) fed each other and droned on forever after release.
# One slot out each stops the drone; the held sound keeps its band shape within ~3-4 dB.
DROP_FX_SLOT = {"Tine Vowel Keys": (3,), "CZ Bitwise Lead": (5,), "Formant Operator Lead": (3,)}
# dB added to the baked level: Formant Operator Lead's level was measured with the drone in it (it peaked at 0 dBFS);
# without it the held sound is ~14 dB quieter, +8 dB brings it back toward its old loudness (fingerprint peak -2.3 dBFS; the old one hit 0 dBFS).
LEVEL_FIX = {"Formant Operator Lead": 8.0}
MACRO_SOURCE = 12  # Macro 1; macro m is MACRO_SOURCE + m


def clean(name, params):
    """Drops mod slots with no source (the search left amounts in empty slots), the routes in DROP_DEST, and gives a
    macro whose route the search's mutation zeroed its default depth back."""
    slots = {int(k[3:-4]) for k in params if k.startswith("mod") and k.endswith("_src")}
    for n in range(1, 65):
        src = round(params.get(f"mod{n}_src", 0))
        dst = round(params.get(f"mod{n}_dst", 0))
        if src == 0 or dst in DROP_DEST.get(name, ()):
            for k in ("src", "dst", "amt", "curve", "pol", "aux", "byp", "stereo"):
                params.pop(f"mod{n}_{k}", None)
        elif MACRO_SOURCE <= src < MACRO_SOURCE + 4 and abs(params.get(f"mod{n}_amt", 0.0)) < 1e-6:
            params[f"mod{n}_amt"] = 0.5
    for n in DROP_FX_SLOT.get(name, ()):
        params.pop(f"fx_slot{n}", None)
        params.pop(f"fx_slot{n}_mix", None)
    return params


def report(folder):
    with open(ROOT / "build" / "gen" / folder / "report.csv", newline="", encoding="utf-8") as f:
        return {r["name"]: r for r in csv.DictReader(f)}


def main():
    defaults = pg.read_defaults()
    fac = report("factory-render")
    targets = {}
    for r in fac.values():
        targets.setdefault(r["category"], []).append(float(r["level_db"]))
    targets = {c: statistics.median(v) for c, v in targets.items()}
    lines = [
        "#pragma once", "",
        "// ilana's search pack: presets grown by tools/qd_search.py from her kept presets, one per kind of sound,",
        "// and kept by her in two audition rounds. Generated by tools/bake_search.py: edit that, not this file.",
        "// Appended after every earlier preset (indices never move). Included from PresetLibrary.h inside Presets::Library.", "",
        "inline void addSearchPresets (const std::function<void (const FactoryPreset&)>& add)", "{",
    ]
    count = 0
    for pack, run in PACKS:
        rep = report(f"{run}/final-render")
        for p in json.load(open(BASE / pack / "pack.json")):
            if p.get("vote") != "Keep":
                continue
            name = NAMES[(pack, p["id"])]
            attrs, params = pg.read_preset(BASE / pack / p["preset"])
            params = clean(name, dict(params))
            measured = rep[p["source"]]
            values = []
            for pid in sorted(params):
                v = params[pid]
                if pid in ("master", "output_trim") or (pid in defaults and abs(v - defaults[pid]) <= 1e-6 * max(1.0, abs(defaults[pid]))):
                    continue
                values.append(f"{{ {pg.cpp_string(pid)}, {pg.cpp_float(v)} }}")
            gain = params.get("master", defaults["master"]) + params.get("output_trim", defaults.get("output_trim", 0.0))
            gain += min(targets[p["category"]] - float(measured["level_db"]),
                        -20 * math.log10(max(float(measured["peak"]), 1e-6) / 0.89))
            gain += LEVEL_FIX.get(name, 0.0)
            values.append(f'{{ "master", {pg.cpp_float(round(max(-30.0, min(12.0, gain)), 2))} }}')
            lines += ["    {", f"        FactoryPreset p {{ {pg.cpp_string(name)}, {{}} }};",
                      f"        p.category = {pg.cpp_string(p['category'])};", "        p.values = {"]
            lines += ["            " + ", ".join(values[i:i + 4]) + "," for i in range(0, len(values), 4)]
            lines.append("        };")
            macros = [attrs.get(f"macroName{i + 1}", "") for i in range(8)]
            while macros and not macros[-1]:
                macros.pop()
            if macros:
                lines.append("        p.macroNames = { " + ", ".join(pg.cpp_string(m) for m in macros) + " };")
            curves = [attrs.get(k) if attrs.get(k) != pg.DEFAULT_CURVE else None for k in pg.CURVE_ATTRS]
            if any(curves):
                lines.append("        p.lfoCurves = { " + ", ".join(pg.cpp_string(c or "") for c in curves) + " };")
            lines += ["        add (p);", "    }"]
            count += 1
    lines.append("}")
    OUT.write_text("\n".join(lines) + "\n")
    print(f"baked {count} presets into {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
