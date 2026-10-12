#!/usr/bin/env python3
"""Generate src/PresetTags.h: 3-6 descriptive tags for every factory preset.

The tags come from what a preset is, not from what it is called alone: its
name (the instrument it imitates), its parameters (engine, envelope, filter,
effects, modulation) and its rendered fingerprint (brightness, width).

    build/ilanaFingerprint_artefacts/Release/ilanaFingerprint /tmp/x.csv   # with
        ILANA_PRESET_PARAMS=/tmp/params.jsonl in the environment
    python3 tools/tag_presets.py /tmp/params.jsonl tests/fingerprints-linux.csv > src/PresetTags.h

Re-run it whenever factory presets are added or changed. The vocabulary is
fixed (VOCABULARY below) so the browser's tag chips stay a short list.
"""
import csv
import json
import re
import sys

# Every tag the generator can give, in the order a row shows them: what the
# sound is first, then its character, its shape and its engine.
VOCABULARY = [
    # what it imitates (from the name)
    "Piano", "E-Piano", "Organ", "Clav", "Bell", "Mallet", "Strings", "Brass", "Woodwind", "Vocal",
    "Guitar", "Harp", "World", "Percussion", "Chiptune", "Sweep", "Noise",
    # character
    "Acid", "Supersaw", "Metallic", "Gritty", "Lo-Fi", "Resonant", "Detuned", "Chord", "Evolving",
    "Arpeggiated", "Rhythmic", "Glide", "Live Input",
    # engine
    "FM", "Physical", "Sample", "Granular",
    # tone and shape
    "Bright", "Dark", "Warm", "Percussive", "Slow Attack", "Sustained",
    # space
    "Wide", "Spacious", "Echo", "Mono",
    # filler
    "Wavetable", "80s",
]

# Name keywords (whole words or word starts, case-insensitive).
NAME_TAGS = [
    ("E-Piano", r"e\.? ?piano|epiano|e-piano|rhodes|wurl|\btine|\bep\b|dx ?ep"),
    ("Piano", r"piano|\bgrand\b|upright|honky"),
    ("Organ", r"organ|drawbar|\bb-?3\b|church|harmonium|calliope"),
    ("Clav", r"\bclav|harpsi"),
    ("Bell", r"bell|chime|gong|\btubular"),
    ("Mallet", r"marimba|vibe|vibra|xylo|glock|kalimba|celest|steel ?drum|mallet|\bmbira"),
    ("Strings", r"string|violin|viola|cello|orch|ensemble|pizz|\bbowed"),
    ("Brass", r"brass|horn|trumpet|trombone|\btuba|\bbrs\b"),
    ("Woodwind", r"flute|clarinet|oboe|\bsax|bassoon|\breed|whis|ocarina|recorder|\bpan ?pipe|shakuhachi|harmonica"),
    ("Vocal", r"choir|\bvox|voice|vocal|\bahh|\booh|formant|chant"),
    ("Guitar", r"guitar|\bguit|\bgtr|banjo|mandolin|\bfunk ?gtr|\bpicked"),
    ("Harp", r"\bharp\b|\bharp "),
    ("World", r"sitar|koto|shamisen|bouzouki|\btabla|\bshaku|gamelan|\bbalaf"),
    ("Percussion", r"\bdrum|\bdrm|\bkick|snar|\bhat\b|\bhats\b|\btom\b|\btoms\b|\bperc|\bclap|cymbal|conga|bongo|timpani|cowbell|\bwood ?block|\brim"),
    ("Chiptune", r"chip|8-?bit|retro|arcade|\bnes\b"),
    ("Sweep", r"sweep|riser|\brise|\bfall|swoosh|\bzap|laser|\bwhoosh"),
    ("Noise", r"noise|\bwind\b|ocean|\brain\b|\bsurf\b|static|\bdust"),
    ("Acid", r"\bacid|\b303|squelch"),
    ("Supersaw", r"supersaw|hypersaw|\btrance"),
    ("Metallic", r"metal|clang|\bgong|anvil"),
    ("Lo-Fi", r"lo-?fi|vinyl|\btape|crush|\bbit"),
    ("Chord", r"chord"),
    ("Arpeggiated", r"\barp"),
    ("Glide", r"glide|portamento|\bslide"),
]

FX = {"Amp": 1, "Drive": 2, "Crush": 3, "Comb": 5, "Phaser": 6, "Chorus": 7, "Delay": 9, "Stutter": 10,
      "Reverb": 13, "Flanger": 14, "Dimension": 15, "Trance Gate": 16, "Tremolo": 23, "RingMod": 25,
      "AW Tape": 32, "AW Saturation": 33, "AW Reverb": 34, "AW Delay": 35, "AW Lo-Fi": 40}

OSC_PREFIXES = ["osc1", "osc2", "sub", "osc4", "osc5", "osc6"]
LFO_SOURCES = {1, 2, 20, 21, 8, 16, 17} | set(range(33, 45)) | set(range(46, 62))
PITCH_DESTINATIONS = {1, 4, 28}


def fx_slots(params):
    return {int(params[k]) for k in params if re.fullmatch(r"fx_slot\d+", k)}


def tags_for(preset, print_row):
    name = preset["name"]
    params = preset["params"]
    category = preset["category"]
    browse = preset["browse"]
    get = lambda key, default=0.0: params.get(key, default)
    dx7 = category == "DX7"
    voice_name = re.sub(r"\s*\([^)]*\)\s*$", "", name)
    lower = voice_name.lower()
    slots = fx_slots(params)
    found = []

    def add(tag):
        assert tag in VOCABULARY, tag
        if tag not in found:
            found.append(tag)

    # What it imitates, from the name.
    for tag, pattern in NAME_TAGS:
        if re.search(pattern, lower):
            add(tag)

    if "E-Piano" in found and "Piano" in found:
        found.remove("Piano")

    if browse == "Drums" and "Percussion" not in found:
        add("Percussion")

    # Character, from the parameters.
    reso, env, glide = get("f1_reso", 0.25), get("f1_env", 0.0), get("glide")
    if reso >= 0.6 and env >= 2.0 and (glide > 0.0 or int(get("voice_mode")) > 0):
        add("Acid")
    if get("osc1_unison", 1) >= 5 or get("osc2_unison", 1) >= 5:
        add("Supersaw" if int(get("osc1_table")) in (0, 1, 110) or "saw" in lower else "Detuned")
    elif get("osc1_unison", 1) >= 3 and get("osc1_detune", 15) >= 8:
        add("Detuned")
    if get("ring_mod") > 0.2 or FX["RingMod"] in slots or FX["Comb"] in slots and "Bell" in found:
        add("Metallic")
    if dx7 and ("Bell" in found or "Mallet" in found):
        add("Metallic")
    if slots & {FX["Drive"], FX["Amp"], FX["AW Saturation"]} or get("f1_drive", 1.5) >= 4.0:
        add("Gritty")
    if slots & {FX["Crush"], FX["AW Lo-Fi"], FX["AW Tape"]}:
        add("Lo-Fi")
    if reso >= 0.7 and "Acid" not in found:
        add("Resonant")
    if get("osc1_chord") > 0:
        add("Chord")

    moving = False
    for slot in range(1, 65):
        src, dst, amt = params.get(f"mod{slot}_src"), params.get(f"mod{slot}_dst"), params.get(f"mod{slot}_amt", 0.0)
        if src is None or dst is None or abs(amt) < 0.05:
            continue
        if int(src) in LFO_SOURCES and int(dst) not in PITCH_DESTINATIONS:
            moving = True
    if moving or get("opeg_lfo_amd") >= 20:
        add("Evolving")
    if get("arp_on") > 0 or browse == "Arp":
        add("Arpeggiated")
    if slots & {FX["Trance Gate"], FX["Stutter"]} or (FX["Tremolo"] in slots and get("fx_trem_depth", 0.5) > 0.4):
        add("Rhythmic")
    if glide >= 0.05 and int(get("voice_mode")) > 0:
        add("Glide")
    if any(int(get(p + "_mode")) == 4 for p in OSC_PREFIXES) or browse == "FX Input":
        add("Live Input")

    # Engine.
    modes = {int(get(p + "_mode")) for p in OSC_PREFIXES if p == "osc1" or get(p + "_on") > 0}
    fm_routes = any(k.startswith("fm_") and k[3].isdigit() and v > 0 for k, v in params.items())
    if dx7 or get("fm_amount") > 0.15 or fm_routes or re.search(r"\bfm\b|\bdx\b", lower):
        add("FM")
    if 1 in modes or get("osc1_excite") > 0:
        add("Physical")
    if 2 in modes:
        add("Sample")
    if 3 in modes or "osc1_grain_size" in params or re.search(r"grain|granular", lower):
        add("Granular")

    # Tone, from the rendered brightness.
    centroid = print_row.get("centroid") if print_row else None
    if centroid is not None:
        add("Bright" if centroid >= 2600 else "Dark" if centroid <= 550 else "Warm")

    # Shape: the amp envelope, or for a DX7 voice the carriers' envelopes.
    if dx7:
        carriers = [p for p in OSC_PREFIXES if get(p + "_out", 1.0) > 0]
        sustain = max((get(p + "_eg_l3", 99) for p in carriers), default=99)
        attack = min((get(p + "_eg_r1", 99) for p in carriers), default=99)
        shape = "Slow Attack" if attack < 55 else "Percussive" if sustain < 50 else "Sustained"
    else:
        attack, sustain = get("amp_attack", 0.005), get("amp_sustain", 0.8)
        shape = "Slow Attack" if attack >= 0.25 else "Percussive" if sustain <= 0.25 else "Sustained"
    add(shape)

    # Space.
    side = print_row.get("side") if print_row else None
    if side is not None and side >= 0.55:
        add("Wide")
    if (FX["Reverb"] in slots and get("fx_reverb_mix", 0.25) >= 0.35) or FX["AW Reverb"] in slots:
        add("Spacious")
    if (FX["Delay"] in slots and get("fx_delay_mix", 0.3) >= 0.2) or FX["AW Delay"] in slots:
        add("Echo")
    if int(get("voice_mode")) > 0 and "Glide" not in found:
        add("Mono")

    if dx7:
        add("80s")
    if not ({"FM", "Physical", "Sample", "Granular", "Live Input"} & set(found)):
        add("Wavetable")

    # Rank by the vocabulary's order, keep at most six; a sound with fewer
    # than three already has tone and shape, the engine fills the third.
    found.sort(key=VOCABULARY.index)
    return found[:6]


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2

    presets = [json.loads(line) for line in open(sys.argv[1]) if line.strip()]
    prints = {}
    with open(sys.argv[2], newline="") as handle:
        for row in csv.DictReader(handle):
            prints[row["name"]] = {"centroid": float(row["centroid_hz"]), "side": float(row["side_ratio"])}

    rows, seen = [], set()
    for preset in presets:
        if preset["name"] in seen:
            continue
        seen.add(preset["name"])
        tags = tags_for(preset, prints.get(preset["name"]))
        if not 3 <= len(tags) <= 6:
            print(f"warning: {preset['name']} has {len(tags)} tags: {tags}", file=sys.stderr)
        rows.append((preset["name"], tags))

    out = sys.stdout
    out.write("#pragma once\n\n")
    out.write("// Generated by tools/tag_presets.py from the factory presets' names,\n")
    out.write("// parameters and fingerprints; edit the script, not this file.\n\n")
    out.write("#include <juce_core/juce_core.h>\n\n#include <map>\n\n")
    out.write("namespace Presets\n{\n")
    out.write("struct FactoryTagRow\n{\n    const char* name;\n    const char* tags;\n};\n\n")
    out.write("inline const FactoryTagRow factoryTagRows[] {\n")
    for name, tags in rows:
        out.write("    { " + json.dumps(name, ensure_ascii=False) + ", " + json.dumps(", ".join(tags)) + " },\n")
    out.write("};\n\n")
    out.write("// A factory preset's tags (\"Bell, FM, Bright\"), or \"\" for any other name.\n")
    out.write("inline juce::String factoryTags (const juce::String& name)\n{\n")
    out.write("    static const auto table = []\n    {\n")
    out.write("        std::map<juce::String, const char*> map;\n\n")
    out.write("        for (const auto& row : factoryTagRows)\n")
    out.write("            map.emplace (juce::String::fromUTF8 (row.name), row.tags);\n\n")
    out.write("        return map;\n    }();\n\n")
    out.write("    const auto found = table.find (name);\n")
    out.write("    return found != table.end() ? juce::String (found->second) : juce::String();\n}\n")
    out.write("} // namespace Presets\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
