#!/usr/bin/env python3
"""Port Airwindows plugins (Chris Johnson, MIT) into src/dsp/airwindows/.

Reads the official Windows VST sources (a sparse clone of
github.com/airwindows/airwindows in build/airwindows-src, not committed) and
writes one small class per plugin: Chris's per-sample processReplacing code
and his state, verbatim, with the VST glue dropped. It also writes
Registry.h (the list the Airwindows FX module offers) and Registry.cpp (the
factories).

    git clone --depth 1 --filter=blob:none --sparse https://github.com/airwindows/airwindows.git build/airwindows-src
    git -C build/airwindows-src sparse-checkout set plugins/WinVST/ToTape6 ...
    python tools/port_airwindows.py

The ALGORITHMS order is the order of the fx_aw_algo choice: append only.
"""

import os
import re
import sys
import textwrap

ROOT = os.path.dirname (os.path.dirname (os.path.abspath (__file__)))
SOURCE = os.path.join (ROOT, "build", "airwindows-src", "plugins", "WinVST")
OUT = os.path.join (ROOT, "src", "dsp", "airwindows")

# (plugin, category, knobs[, ranges]): knobs are the plugin's own parameter
# letters shown as the module's up to five knobs, in order; parameters left
# out keep Chris's defaults. ranges narrows a knob to part of the plugin's
# 0..1 where its far end is only a huge gain (the module's 0..1 then spans
# lo..hi). Append only: the index is saved in patches.
ALGORITHMS = [
    ("ToTape6", "Saturation & Tape", "ABCDE"),      # F (Dry/Wet) stays 1
    ("IronOxide5", "Saturation & Tape", "ABCDE"),   # F (Output Trim), G (Inv/Dry/Wet) stay at defaults
    ("Tape", "Saturation & Tape", "AB"),
    ("Density", "Saturation & Tape", "ABCD"),
    ("Drive", "Saturation & Tape", "ABCD"),
    ("Spiral2", "Saturation & Tape", "ABCDE"),
    ("PurestDrive", "Saturation & Tape", "A"),
    ("Tube2", "Saturation & Tape", "AB"),
    ("Mojo", "Saturation & Tape", "A"),
    ("Coils2", "Saturation & Tape", "ABC"),
    ("Console7Channel", "Console", "A"),
    ("Console7Buss", "Console", "A"),
    ("Channel9", "Console", "ABC"),
    ("Air", "EQ & Filter", "ABCDE",                 # F (Dry/Wet) stays 1
     { "A": (0.2, 0.8), "B": (0.2, 0.8), "C": (0.2, 0.8) }),  # the taps grow as a cube: their ends are huge boosts
    ("Air3", "EQ & Filter", "AB", { "A": (0.0, 0.7) }),          # air gain up to 1.4 (was 2)
    ("Capacitor2", "EQ & Filter", "ABCD"),
    ("Baxandall2", "EQ & Filter", "AB",
     { "A": (0.1875, 0.8125), "B": (0.1875, 0.8125) }),          # +/-15 dB (was +/-24)
    ("Isolator2", "EQ & Filter", "ABCD"),
    ("Holt2", "EQ & Filter", "ABCDE"),
    ("Pressure5", "Dynamics", "ABCDE"),             # F (Dry/Wet) stays 1
    ("ButterComp2", "Dynamics", "ABC"),
    ("Logical4", "Dynamics", "ABCDE", { "D": (0.0, 0.7) }),      # makeup up to +8 dB (was +20)
    ("Pyewacket", "Dynamics", "ABC"),
    ("Pop2", "Dynamics", "ABCDE"),
    ("Galactic", "Space", "ABCDE"),
    ("kCathedral", "Space", "A"),
    ("Verbity2", "Space", "ABCD"),
    ("Chamber", "Space", "ABCDE"),
    ("MatrixVerb", "Space", "ABEFG"),               # C (Speed), D (Vibrato) stay at defaults
    ("Wider", "Stereo", "ABC"),
    ("Srsly2", "Stereo", "ABCDE"),
    ("ToVinyl4", "Stereo", "ABCD"),
    ("DeRez2", "Lo-Fi", "ABCD"),
    ("Deckwrecka", "Lo-Fi", "A"),
    ("BitShiftGain", "Lo-Fi", "A", { "A": (0.25, 0.5625) }),   # -8..+2 bit shifts (was -16..+16)
    ("DrumSlam", "Character", "ABC"),
    ("Inflamer", "Character", "ABC"),
    ("Flutter", "Character", "A"),
    ("ChorusEnsemble", "Character", "ABC"),
]

HEADER = """// Airwindows @NAME@ by Chris Johnson (airwindows.com), MIT licence (see
// LICENSE.txt here). Ported by tools/port_airwindows.py from the official
// repository's plugins/WinVST/@NAME@: the state and the per-sample code of
// processReplacing are Chris's, as he wrote them (noise and dither included);
// only the VST glue is gone. Do not edit by hand: re-run the tool.
#pragma once

#include "Algorithm.h"

namespace airwindows
{
class @NAME@ final : public Algorithm
{
public:
@ENUM@
    @NAME@()
    {
@DEFAULTS@
        reset();
    }

    int getNumParameters() const override { return kNumParameters; }

    void reset() override
    {
        restartRandom();
@RESET@
    }

    void setParam (int index, float value) override
    {
        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        switch (index)
        {
@CASES@
            default: break;
        }
    }

    void process (float* left, float* right, int numSamples) override
    {
        float* inputs[2] { left, right };
        float* outputs[2] { left, right };
        processReplacing (inputs, outputs, numSamples);
    }

private:
    void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
@BODY@

@MEMBERS@
};
} // namespace airwindows
"""


def read (path):
    with open (path, encoding="latin-1") as f:
        return f.read().replace ("\r\n", "\n").replace ("\r", "\n")


def braced (text, start):
    """The {...} block starting at the first brace at or after start."""
    open_at = text.index ("{", start)
    depth = 0
    for i in range (open_at, len (text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_at:i + 1]
    raise ValueError ("unbalanced braces")


def fix_code (text):
    """Portable spellings: rand() gets a per-instance seed (every instance
    starts alike); near/far are empty macros once windows.h is in."""
    out = []
    for line in text.split ("\n"):
        code, sep, comment = line.partition ("//")
        code = code.replace ("rand()", "awRand()")
        code = re.sub (r"\bnear\b", "nearTap", code)
        code = re.sub (r"\bfar\b", "farTap", code)
        code = re.sub (r"\bnearLevel\b", "nearTapLevel", code)
        code = re.sub (r"\bfarLevel\b", "farTapLevel", code)
        out.append (code + sep + comment)
    return "\n".join (out)


def indent (text, spaces):
    pad = " " * spaces
    text = textwrap.dedent (text.expandtabs (4).strip ("\n"))
    return "\n".join ((pad + line).rstrip() for line in text.split ("\n"))


def port (name):
    folder = os.path.join (SOURCE, name)
    header = read (os.path.join (folder, name + ".h"))
    cpp = read (os.path.join (folder, name + ".cpp"))
    proc = read (os.path.join (folder, name + "Proc.cpp"))

    enum = re.search (r"enum\s*\{[^}]*kNumParameters[^}]*\};", header).group (0)
    count = int (re.search (r"kNumParameters\s*=\s*(\d+)", enum).group (1))
    letters = [chr (ord ("A") + i) for i in range (count)]

    # State: everything the class declares after the VST bookkeeping.
    members = header[header.index ("_canDo;") + len ("_canDo;"):]
    members = members[:members.rindex ("};")]
    members = textwrap.dedent (re.sub (r"//default stuff", "", members).expandtabs (4).strip ("\n"))

    # Constants the header declares before the class (delay lengths...).
    before = header[header.index (enum) + len (enum):header.index ("\nclass ")]
    constants = [m for m in re.findall (r"const\s+(int|double|float)\s+(\w+)\s*=\s*([^;]+);", before)
                 if not m[1].startswith ("kNum")]
    if constants:
        members = "\n".join ("static constexpr {} {} = {};".format (*m) for m in constants) + "\n" + members

    # The constructor's body up to the VST calls is the reset; the parameter
    # lines in it are the defaults.
    ctor = braced (cpp, cpp.index (name + "::" + name + "("))
    ctor = ctor[1:ctor.index ("_canDo.insert")] if "_canDo.insert" in ctor else ctor[1:-1]
    defaults = {}
    reset_lines = []
    for line in ctor.split ("\n"):
        m = re.match (r"\s*([A-J])\s*=\s*([-0-9.]+)f?\s*;\s*(//.*)?$", line)
        if m and m.group (1) in letters:
            defaults[m.group (1)] = float (m.group (2))
            continue
        reset_lines.append (line)
    for letter in letters:
        defaults.setdefault (letter, 0.0)

    # setParameter's own cases (a few compute extra state there).
    setp = braced (cpp, cpp.index (name + "::setParameter"))
    switch = braced (setp, setp.index ("switch"))[1:-1]
    switch = switch.replace ("throw;", "break;")
    switch = re.sub (r"\n\s*default:[^\n]*", "", switch)
    for i, letter in enumerate (letters):
        switch = switch.replace ("case kParam" + letter + ":", "case " + str (i) + ":")

    names = dict (re.findall (r'case kParam(\w+):\s*vst_strncpy\s*\(text,\s*"([^"]*)"',
                              cpp[cpp.index ("::getParameterName"):cpp.index ("::getParameterDisplay")]))

    body = braced (proc, proc.index (name + "::processReplacing"))

    # Function statics (ButterComp2's noise sources) become the instance's
    # own state, reset with the rest, so two instances don't share them.
    for kind, var, value in re.findall (r"\n[ \t]*static\s+(int|double|float)\s+(\w+)\s*=\s*([^;]+);", body):
        body = re.sub (r"\n[ \t]*static\s+" + kind + r"\s+" + var + r"\s*=\s*[^;]+;[^\n]*", "", body)
        members += "\n{} {}; // a function static in the original\n".format (kind, var)
        reset_lines.append ("{} = {}; // a function static in the original".format (var, value))

    fields = { "ENUM": "    " + re.sub (r"\s+", " ", enum),
               "DEFAULTS": "\n".join ("        {} = {}f;".format (l, repr (defaults[l])) for l in letters),
               "RESET": indent (fix_code ("\n".join (reset_lines)), 8),
               "CASES": indent (switch, 12),
               "BODY": indent (fix_code (body), 4),
               "MEMBERS": indent (members, 4) }
    code = HEADER
    for key, value in fields.items():
        code = code.replace ("@" + key + "@", value)
    code = code.replace ("@NAME@", name)
    code = re.sub (r"\n{3,}", "\n\n", code)
    with open (os.path.join (OUT, name + ".h"), "w", encoding="utf-8", newline="\n") as f:
        f.write (code)
    return letters, names, defaults


def main():
    os.makedirs (OUT, exist_ok=True)
    rows = []
    for entry in ALGORITHMS:
        name, category, knobs = entry[:3]
        ranges = entry[3] if len (entry) > 3 else {}
        letters, names, defaults = port (name)
        for k in knobs:
            assert k in letters, (name, k)
        assert len (knobs) <= 5, name
        row = []
        for k in knobs:
            lo, hi = ranges.get (k, (0.0, 1.0))
            default = min (1.0, max (0.0, (defaults[k] - lo) / (hi - lo)))
            row.append ((ord (k) - ord ("A"), names.get (k, k), default, lo, hi))
        rows.append ((name, category, row))

    def cstr (s):
        return '"' + s.replace ('"', '\\"') + '"'

    table = []
    for name, category, knobs in rows:
        knob_list = ", ".join ("{{ {}, {}, {}f, {}f, {}f }}".format (i, cstr (label), repr (round (d, 9)), repr (lo), repr (hi))
                               for i, label, d, lo, hi in knobs)
        table.append ("        {{ {}, {}, {}, {{ {} }}, &create{} }},".format (cstr (name), cstr (category), len (knobs), knob_list, name))

    with open (os.path.join (OUT, "Registry.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write ("""// The Airwindows algorithms the Airwindows FX module offers (Chris Johnson's
// plugins, MIT: see LICENSE.txt). Written by tools/port_airwindows.py.
// The order is the fx_aw_algo choice's: append only, never reorder.
#pragma once

#include <memory>
#include <vector>

#include "Algorithm.h"

namespace airwindows
{
struct Knob
{
    int parameter;       // the plugin's own parameter (A = 0, B = 1 ...)
    const char* name;    // as the plugin names it
    float defaultValue;  // the plugin's default, as the knob's 0..1
    float lo, hi;        // the part of the plugin's 0..1 the knob spans

    float toPlugin (float knob) const { return lo + (hi - lo) * knob; }
};

struct Info
{
    const char* name;
    const char* category;
    int numKnobs;        // up to 5; the module's knobs past this are hidden
    Knob knobs[5];
    std::unique_ptr<Algorithm> (*create)();
};

""")
        for name, _, _ in rows:
            f.write ("std::unique_ptr<Algorithm> create{}();\n".format (name))
        f.write ("""
inline const std::vector<Info>& registry()
{
    static const std::vector<Info> list {
""")
        f.write ("\n".join (table) + "\n")
        f.write ("""    };
    return list;
}

inline int count() { return (int) registry().size(); }
} // namespace airwindows
""")

    with open (os.path.join (OUT, "Registry.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write ("// The Airwindows factories. Written by tools/port_airwindows.py.\n\n#include \"Registry.h\"\n\n")
        for name, _, _ in rows:
            f.write ("#include \"{}.h\"\n".format (name))
        f.write ("\nnamespace airwindows\n{\n")
        for name, _, _ in rows:
            f.write ("std::unique_ptr<Algorithm> create{0}() {{ return std::make_unique<{0}>(); }}\n".format (name))
        f.write ("} // namespace airwindows\n")

    print ("ported", len (rows), "algorithms")


if __name__ == "__main__":
    sys.exit (main())
