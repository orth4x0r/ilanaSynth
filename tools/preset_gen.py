#!/usr/bin/env python3
"""The preset generator: new factory presets grown from ilana's own presets.

ilana's presets (content/user-presets) are the examples of what sounds nice.
Each concept below starts from one of them (its "seed": oscillators, filter,
envelopes, effects, the way the patch is finished) and swaps in one or more of
the synth's newer features (physical strings and bodies, the Operator EG, the
WEST voice, the vector pad, Airwindows filters and effects, the trance gate,
the pattern sequencer, sympathetic strings...). Every concept is drawn several
times with its ranges sampled; all the drafts are rendered and the one that
sounds best is kept:

  - close to ilana's presets (CLAP audio embeddings: what a sound is like),
    weighted to her presets of the same kind;
  - unlike the factory presets already there and unlike the other picks;
  - sane: no clipping, not silent, no harsh top, a level near its category.

Steps (each writes into build/gen/):

    python tools/preset_gen.py refs           stage her presets in build/gen/refs
    python tools/preset_gen.py draft [--per N] [--seed S]
                                              write N drafts per concept to build/gen/drafts
    ILANA_RENDER_FILES=build/gen/refs   build/ilanaPresetRender_artefacts/Release/ilanaPresetRender build/gen/refs-render
    ILANA_RENDER_FILES=build/gen/drafts build/ilanaPresetRender_artefacts/Release/ilanaPresetRender build/gen/drafts-render
    build-venv/bin/python tools/clap_score.py build/gen/refs-render build/gen/drafts-render build/critic-now
    python tools/preset_critic.py build/gen/drafts-render
    python tools/preset_gen.py pick           choose one draft per concept -> build/gen/picks.json
    python tools/preset_gen.py bake           write src/PresetPackIlana.h from the picks

`ILANA_DUMP_DEFAULTS=build/gen/defaults.txt ilanaPresetRender x` writes the
parameter defaults the drafts are measured against (draft and bake need it).
Only what a factory preset can carry is kept: parameter values, LFO curves and
macro names (no FX patch graphs, remap curves or sample files), so a draft
sounds as its baked preset will.
"""

import argparse
import csv
import json
import math
import random
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
USER = ROOT / "content" / "user-presets"
GEN = ROOT / "build" / "gen"
PACK = ROOT / "src" / "PresetPackIlana.h"

# What kind of sound each of her presets is (her own categories are loose:
# several say Other).
REF_CATEGORY = {
    "ARPed pluck Bass": "Bass",
    "Chord Augmentation Tech": "Keys",
    "E.PIANO 1 (ROM3A) modded": "Keys",
    "Hammered Strings Edited": "Keys",
    "Retro Bass": "Bass",
    "Strummed Pad modded": "Pad",
    "caribbean keys": "Keys",
    "classic synth brass, no fx": "Lead",
    "crazy bass": "Bass",
    "fm arp": "Pluck",
    "patcher bass": "Bass",
    "phys_bowed pad": "Pad",
    "real piano SOUND": "Keys",
    "trancey pad": "Pad",
    "velocity warp physical pluck": "Pluck",
}

# Attributes that are a preset's sound and that a factory preset can carry.
CURVE_ATTRS = [f"lfo{i}Curve" for i in range(1, 5)]


# --------------------------------------------------------------------------
# Preset files
# --------------------------------------------------------------------------

def read_preset(path):
    root = ET.parse(path).getroot()
    params = {}
    for child in root:
        if child.tag == "PARAM":
            try:
                params[child.get("id")] = float(child.get("value"))
            except (TypeError, ValueError):
                pass
    return root.attrib.copy(), params


# The state's schema versions now (State.cpp): a file without them is migrated
# on load as an old one.
CURRENT_SCHEMA = {"osc3Schema": "2", "stringSchema": "2", "destSchema": "2", "tableSchema": "4", "exciterLevels": "1"}
SCHEMA_KEYS = list(CURRENT_SCHEMA)


def write_preset(path, name, category, params, macro_names=None, curves=None, schema=None):
    root = ET.Element("PARAMS")
    for key, value in (schema or CURRENT_SCHEMA).items():
        root.set(key, value)
    root.set("presetName", name)
    root.set("presetCategory", category)
    root.set("presetAuthor", "ilanaSynth generator")
    for i, macro in enumerate((macro_names or [])[:8]):
        root.set(f"macroName{i + 1}", macro or "")
    for key, value in (curves or {}).items():
        root.set(key, value)
    for pid in sorted(params):
        child = ET.SubElement(root, "PARAM")
        child.set("id", pid)
        child.set("value", repr(float(params[pid])))
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.ElementTree(root).write(path, encoding="UTF-8", xml_declaration=True)


def read_defaults():
    path = GEN / "defaults.txt"
    if not path.exists():
        sys.exit(f"{path} is missing: run ILANA_DUMP_DEFAULTS={path.relative_to(ROOT)} ilanaPresetRender x")
    defaults = {}
    for line in path.read_text().splitlines():
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            defaults[key] = float(value)
        elif line.startswith("#slots="):
            MOD["slots"] = int(line[7:])
        elif line.startswith("#macro"):
            index, value = line[6:].split("=")
            MOD["macros"][int(index) - 1] = int(value)
        elif line.startswith("#dest "):
            name, value = line[6:].rsplit("=", 1)
            MOD["dests"][name] = int(value)
        elif line.startswith("#pdest "):
            name, value = line[7:].rsplit("=", 1)
            MOD["pdests"][name] = int(value)
    return defaults


# The modulation map from the defaults dump: slot count, each macro's source
# index, destinations by display name and by parameter id.
MOD = {"slots": 64, "macros": {}, "dests": {}, "pdests": {}}


def wire_macros(params, macros):
    """Clears the seed's macro routings and wires the concept's macros:
    [(NAME, [(parameter id or destination name, amount), ...]), ...]."""
    sources = set(MOD["macros"].values())
    free = []
    for slot in range(1, MOD["slots"] + 1):
        src = round(params.get(f"mod{slot}_src", 0))
        if src in sources:
            for key in ("src", "dst", "amt", "curve", "aux"):
                if f"mod{slot}_{key}" in params:
                    params[f"mod{slot}_{key}"] = 0.0
            src = 0
        if src == 0:
            free.append(slot)
    for index, (name, targets) in enumerate(macros):
        for target, amount in targets:
            dest = MOD["pdests"].get(target, MOD["dests"].get(target))
            if dest is None:
                sys.exit(f"macro {name}: unknown target {target}")
            if not free:
                sys.exit(f"macro {name}: no free modulation slot")
            slot = free.pop(0)
            params[f"mod{slot}_src"] = float(MOD["macros"][index])
            params[f"mod{slot}_dst"] = float(dest)
            params[f"mod{slot}_amt"] = float(amount)
    return params


def load_seeds(defaults):
    """Her presets as the synth loads them (old files migrated to the current
    schema): the parameters ilanaPresetRender listed for build/gen/refs."""
    rendered = {}
    render = GEN / "refs-render"
    if (render / "index.csv").exists():
        with open(render / "index.csv", newline="", encoding="utf-8") as f:
            for row in csv.DictReader(f):
                values = {}
                for line in (render / row["index"] / "params.txt").read_text().splitlines():
                    if "=" in line and not line.startswith("#"):
                        key, value = line.split("=", 1)
                        values[key] = float(value)
                rendered[row["name"]] = values
    seeds = {}
    for path in sorted(USER.glob("*.ilanapreset")):
        attrs, params = read_preset(path)
        full = dict(defaults)
        if path.stem in rendered:
            full.update(rendered[path.stem])
        else:
            full.update({k: v for k, v in params.items() if k in defaults})
        curves = {k: attrs[k] for k in CURVE_ATTRS if k in attrs}
        macros = [attrs.get(f"macroName{i}", "") for i in range(1, 9)]
        schema = {k: attrs[k] for k in SCHEMA_KEYS if k in attrs}
        seeds[path.stem] = {"params": full, "raw": params, "schema": schema, "curves": curves, "macros": macros,
                            "category": REF_CATEGORY.get(path.stem, "Other"),
                            "sample": attrs.get("osc1SamplePath")}
    return seeds


# --------------------------------------------------------------------------
# Steps
# --------------------------------------------------------------------------

def cmd_refs(_args):
    """Her presets, renamed by kind, as the generator scores them (no patch
    graphs, remaps or samples: what a factory preset could carry)."""
    defaults = read_defaults()
    out = GEN / "refs"
    for old in out.glob("*.ilanapreset"):
        old.unlink()
    for name, seed in load_seeds(defaults).items():
        # The raw file's values with its own schema, so the synth migrates them as it does hers.
        raw = {k: v for k, v in seed["raw"].items() if k in defaults}
        write_preset(out / f"{name}.ilanapreset", name, seed["category"], raw, seed["macros"], seed["curves"],
                     schema=seed["schema"])
    print(f"staged {len(REF_CATEGORY)} presets in {out}")


def sample(spec, rng):
    """A concept's value: a number, a (low, high) range, a ("log", low, high)
    range or a ("pick", a, b, ...) choice."""
    if isinstance(spec, tuple):
        if spec[0] == "pick":
            return rng.choice(spec[1:])
        if spec[0] == "log":
            return math.exp(rng.uniform(math.log(spec[1]), math.log(spec[2])))
        if spec[0] == "int":
            return rng.randint(spec[1], spec[2])
        return rng.uniform(spec[0], spec[1])
    return spec


def style_rules(params, category):
    """What her presets do and the factory ones often don't: no glide on
    polyphonic patches, a few ms legato glide at most on mono ones, quick
    attacks outside pads, releases that end."""
    mono = params.get("voice_mode", 0) >= 1 or params.get("poly_voices", 16) <= 1
    if not mono:
        params["glide"] = 0.0
    else:
        params["glide"] = min(params.get("glide", 0.0), 0.012)
        params["glide_legato"] = 1.0
    if category in ("Bass", "Pluck", "Keys", "Lead"):
        params["amp_attack"] = min(params.get("amp_attack", 0.003), 0.02)
        params["amp_release"] = min(params.get("amp_release", 0.3), 1.5 if category == "Keys" else 0.8)
    return params


def cmd_draft(args):
    from preset_gen_concepts import CONCEPTS  # the concepts, kept in their own file

    defaults = read_defaults()
    seeds = load_seeds(defaults)
    rng = random.Random(args.seed)
    out = GEN / "drafts"
    if out.exists():
        for old in out.rglob("*.ilanapreset"):
            old.unlink()
    manifest = []
    for concept in CONCEPTS:
        seed = seeds[concept["seed"]]
        for draw in range(args.per):
            params = dict(seed["params"])
            for key, spec in concept.get("set", {}).items():
                if key not in defaults:
                    sys.exit(f"{concept['name']}: unknown parameter {key}")
                params[key] = float(sample(spec, rng))
            for key, spec in concept.get("scale", {}).items():
                params[key] = params[key] * float(sample(spec, rng))
            params = style_rules(params, concept["category"])
            macros = [m for m, _ in concept.get("macros", [])]
            params = wire_macros(params, concept.get("macros", []))
            for macro in range(8):  # macros start at 0 (the seed's knob positions go)
                if f"macro{macro + 1}" in params:
                    params[f"macro{macro + 1}"] = defaults[f"macro{macro + 1}"]
            stem = f"{concept['name']}~{draw}"
            write_preset(out / f"{stem}.ilanapreset", stem, concept["category"], params, macros, seed["curves"])
            manifest.append({"stem": stem, "concept": concept["name"], "draw": draw})
    (GEN / "drafts.json").write_text(json.dumps(manifest, indent=1))
    print(f"{len(manifest)} drafts of {len(CONCEPTS)} concepts in {out}")


def load_render(folder):
    folder = Path(folder)
    with open(folder / "index.csv", newline="", encoding="utf-8") as f:
        index = list(csv.DictReader(f))
    import numpy as np
    embeddings = np.load(folder / "clap_embeddings.npy")
    report = {}
    if (folder / "report.csv").exists():
        with open(folder / "report.csv", newline="", encoding="utf-8") as f:
            report = {r["index"]: r for r in csv.DictReader(f)}
    for row, embedding in zip(index, embeddings):
        row["embedding"] = embedding / (np.linalg.norm(embedding) + 1e-9)
        row["report"] = report.get(row["index"], {})
    return index


def cmd_pick(args):
    import numpy as np

    refs = load_render(GEN / "refs-render")
    drafts = load_render(GEN / "drafts-render")
    factory = load_render(Path(args.factory))
    ref_matrix = np.stack([r["embedding"] for r in refs])
    fac_matrix = np.stack([r["embedding"] for r in factory])

    by_concept = {}
    for row in drafts:
        by_concept.setdefault(row["name"].split("~")[0], []).append(row)

    targets = category_levels(args.factory)

    def sane(row):
        """Judged at the level the bake will give it (the category's median)."""
        rep = row["report"]
        if not rep:
            return True, ""
        level, peak = float(rep["level_db"]), float(rep["peak"])
        high = float(rep.get("high_fraction", 0) or 0)
        # The bake stops short of the category's level when the peaks would
        # pass -1 dB (a sharp attack): only far short of it counts against it.
        short = (targets.get(row["category"], targets["Pad"]) - level) + 20 * math.log10(max(peak, 1e-6) / 0.89)
        if short > 10.0:
            return False, f"{short:.0f} dB under its category at full scale"
        if level < -40:
            return False, "silent"
        if high > 0.12:
            return False, "harsh"
        return True, ""

    picks, chosen = [], []
    for name, rows in by_concept.items():
        best, best_score = None, -1e9
        for row in rows:
            ok, why = sane(row)
            e = row["embedding"]
            sims = ref_matrix @ e
            same = [s for s, r in zip(sims, refs) if r["category"] == row["category"]]
            taste = 0.5 * float(np.max(sims)) + 0.5 * (float(np.max(same)) if same else float(np.max(sims)))
            novelty = 1.0 - float(np.max(fac_matrix @ e))
            spread = 1.0 - max([float(c @ e) for c in chosen], default=0.0)
            critic = float(row["report"].get("score", 50) or 50) / 100.0  # the critic's own 0-100 verdict
            score = taste + 0.6 * novelty + 0.3 * spread + 0.3 * critic - (10.0 if not ok else 0.0)
            row["scores"] = {"taste": taste, "novelty": novelty, "spread": spread, "ok": ok, "why": why}
            if score > best_score:
                best, best_score = row, score
        chosen.append(best["embedding"])
        level = float(best["report"]["level_db"]) if best["report"] else None
        peak = float(best["report"]["peak"]) if best["report"] else 1.0
        picks.append({"concept": name, "stem": best["name"], "category": best["category"], "level_db": level, "peak": peak,
                      **best["scores"]})
        print(f"{name:32s} {best['name']:36s} taste {best['scores']['taste']:.3f} "
              f"novelty {best['scores']['novelty']:.3f} {'' if best['scores']['ok'] else 'NOT OK: ' + best['scores']['why']}")
    (GEN / "picks.json").write_text(json.dumps(picks, indent=1))


# The Curve shape's starting points (an LFO nobody drew on).
DEFAULT_CURVE = "0.0000,0.0000,-0.450;0.2500,1.0000,0.450;0.5000,0.0000,-0.450;0.7500,-1.0000,0.450;1.0000,0.0000,0.000"


def cpp_float(value):
    text = f"{value:.7g}"
    if "." not in text and "e" not in text:
        text += ".0"
    return text + "f"


def cpp_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def category_levels(folder):
    """The median level of each category in a critic report."""
    import statistics
    levels = {}
    with open(Path(folder) / "report.csv", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            levels.setdefault(row["category"], []).append(float(row["level_db"]))
    return {c: statistics.median(v) for c, v in levels.items()}


def cmd_bake(args):
    from preset_gen_concepts import CONCEPTS

    defaults = read_defaults()
    targets = category_levels(args.factory)
    concepts = {c["name"]: c for c in CONCEPTS}
    picks = json.loads((GEN / "picks.json").read_text())
    lines = [
        "#pragma once",
        "",
        "// ilana's pack: presets grown from her own presets (content/user-presets)",
        "// by tools/preset_gen.py, each with one or more of the newer features.",
        "// Generated: edit the concepts in tools/preset_gen_concepts.py and re-bake,",
        "// not this file. Appended after every earlier preset (indices never move).",
        "// Included from PresetLibrary.h inside Presets::Library.",
        "",
        "inline void addIlanaPresets (const std::function<void (const FactoryPreset&)>& add)",
        "{",
    ]
    for pick in picks:
        concept = concepts[pick["concept"]]
        attrs, params = read_preset(GEN / "drafts" / f"{pick['stem']}.ilanapreset")
        lines.append(f"    // {concept['about']} (from \"{concept['seed']}\")")
        lines.append("    {")
        lines.append(f"        FactoryPreset p {{ {cpp_string(concept['name'])}, {{}} }};")
        lines.append(f"        p.category = {cpp_string(concept['category'])};")
        values = []
        for pid in sorted(params):
            value = params[pid]
            if pid in defaults and abs(value - defaults[pid]) <= 1e-6 * max(1.0, abs(defaults[pid])):
                continue
            if pid in ("master", "output_trim"):
                continue
            values.append(f"{{ {cpp_string(pid)}, {cpp_float(value)} }}")
        # The level: the draft's (MASTER plus OUTPUT TRIM) moved to the
        # category's median loudness, into MASTER (a factory load moves it to
        # the trim).
        gain = params.get("master", defaults["master"]) + params.get("output_trim", defaults.get("output_trim", 0.0))
        target = targets.get(pick["category"], targets.get("Pad"))
        if pick.get("level_db") is not None:
            # to the category's level, but with the peaks at -1 dB at most
            peak = pick.get("peak", 1.0)
            gain += min(target - pick["level_db"], -20 * math.log10(max(peak, 1e-6) / 0.89))
        values.append(f"{{ \"master\", {cpp_float(round(max(-30.0, min(12.0, gain)), 2))} }}")
        lines.append("        p.values = {")
        for i in range(0, len(values), 4):
            lines.append("            " + ", ".join(values[i:i + 4]) + ",")
        lines.append("        };")
        names = [m for m, _ in concept.get("macros", [])]
        if names:
            lines.append("        p.macroNames = { " + ", ".join(cpp_string(n) for n in names) + " };")
        curves = [attrs.get(k) if attrs.get(k) != DEFAULT_CURVE else None for k in CURVE_ATTRS]
        if any(curves):
            lines.append("        p.lfoCurves = { " + ", ".join(cpp_string(c or "") for c in curves) + " };")
        lines.append("        add (p);")
        lines.append("    }")
    lines.append("}")
    PACK.write_text("\n".join(lines) + "\n")
    print(f"baked {len(picks)} presets into {PACK.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("refs")
    draft = sub.add_parser("draft")
    draft.add_argument("--per", type=int, default=6)
    draft.add_argument("--seed", type=int, default=1)
    pick = sub.add_parser("pick")
    pick.add_argument("--factory", default=str(ROOT / "build" / "critic-now"))
    bake = sub.add_parser("bake")
    bake.add_argument("--factory", default=str(ROOT / "build" / "critic-now"))
    args = parser.parse_args()
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    {"refs": cmd_refs, "draft": cmd_draft, "pick": cmd_pick, "bake": cmd_bake}[args.cmd](args)


if __name__ == "__main__":
    main()
