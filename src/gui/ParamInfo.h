#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "../dsp/OscillatorIds.h"
#include "../dsp/OperatorEgParams.h"
#include "../dsp/Dx7Engine.h"

// What the editor and the host show for a parameter: its value as text
// (describeValue) and its help line (describeParameter). Both come from a
// table of rows (step 48): each row names the IDs it covers (an exact ID,
// a prefix, a suffix, an oscillator's parameter...) and what to show. The
// first row that covers an ID wins, as the old if-chains did, so a row
// placed early can shadow a broader one below it. Exact IDs are found
// through a sorted index; only the pattern rows are tried one by one.

inline bool isOscParameter (const juce::String& id, const char* suffix, bool includeLegacyThird = true)
{
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if ((includeLegacyThird || osc != 2)
            && id == juce::String (OscillatorIds::prefixes[(size_t) osc]) + suffix)
            return true;

    return false;
}

// fx_aw_p1..p5 (the all-in-one) and fx_awsat_p1.. (the category modules).
inline bool isAirwindowsKnob (const juce::String& id)
{
    const auto tail = id.fromLastOccurrenceOf ("_", false, false);
    return id.startsWith ("fx_aw") && tail.length() == 2 && tail[0] == 'p' && juce::CharacterFunctions::isDigit (tail[1]);
}

// A silent level, the one spelling everywhere: "−∞ dB" (a true minus and
// infinity, UI review 8, V8-8).
inline juce::String silentDecibels() { return juce::String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e dB"); }

// A value rounded to a fixed number of decimals, never "-0.0".
inline juce::String describeFixed (float value, int decimals)
{
    const auto scale = std::pow (10.0f, (float) decimals);
    const auto rounded = std::round (value * scale) / scale;
    return juce::String (rounded == 0.0f ? 0.0f : rounded, decimals);
}

// A number with the decimals its size calls for (under 1: two, under 10:
// one, else none), judged after rounding, so 0.998 shows as "1.0" like 1.0
// does and typed text reads back the same.
inline juce::String describeNumber (float value, int maxDecimals = 2)
{
    const auto rounded = [value] (int decimals) { const auto scale = std::pow (10.0f, (float) decimals); return std::round (value * scale) / scale; };
    if (maxDecimals >= 2 && std::abs (rounded (2)) < 1.0f)
        return juce::String (rounded (2), 2);
    if (maxDecimals >= 1 && std::abs (rounded (1)) < 10.0f)
        return juce::String (rounded (1), 1);
    return juce::String (juce::roundToInt (value));
}

namespace ParamInfo
{
// ---------------------------------------------------------------------------
// Which IDs a row covers.

using IdTest = bool (*) (const juce::String&);

enum class Match : unsigned char
{
    none,
    is,     // the ID itself
    starts, // a prefix
    ends,   // a suffix
    has,    // anywhere in the ID
    tail,   // what follows the ID's first '_' ("smooth" in lfo3_smooth)
    osc,    // an oscillator's parameter: osc1, osc2, sub, osc4..osc6 + the name
    osc12,  // the same without sub (the legacy third oscillator has its own rows)
    test    // a function, for the few IDs no pattern names
};

struct Term
{
    Match match = Match::none;
    const char* names = ""; // one or more, split by '|'
    IdTest test = nullptr;
    bool negated = false;
};

// Up to three terms, all of which must hold: the first names the IDs, the
// others narrow it (also) or carve out exceptions (butNot).
struct Key
{
    std::array<Term, 3> terms {};

    constexpr Key also (Key other) const { return with (other.terms[0], false); }
    constexpr Key butNot (Key other) const { return with (other.terms[0], true); }

private:
    constexpr Key with (Term term, bool negated) const
    {
        auto copy = *this;
        term.negated = negated;
        for (auto& slot : copy.terms)
            if (slot.match == Match::none)
                return slot = term, copy;
        return copy;
    }
};

constexpr Key keyOf (Match match, const char* names) { Key key; key.terms[0] = { match, names }; return key; }
constexpr Key is (const char* names) { return keyOf (Match::is, names); }
constexpr Key starts (const char* names) { return keyOf (Match::starts, names); }
constexpr Key ends (const char* names) { return keyOf (Match::ends, names); }
constexpr Key has (const char* names) { return keyOf (Match::has, names); }
constexpr Key tail (const char* names) { return keyOf (Match::tail, names); }
constexpr Key osc (const char* names) { return keyOf (Match::osc, names); }
constexpr Key osc12 (const char* names) { return keyOf (Match::osc12, names); }
constexpr Key when (IdTest test) { Key key; key.terms[0] = { Match::test, "", test }; return key; }

inline std::string_view viewOf (const juce::String& id) { return { id.toRawUTF8(), id.getNumBytesAsUTF8() }; }

inline bool isOscPrefix (int osc, Match match) { return match == Match::osc || osc != 2; }

inline bool nameMatches (Match match, std::string_view id, std::string_view name)
{
    switch (match)
    {
        case Match::is: return id == name;
        case Match::starts: return id.starts_with (name);
        case Match::ends: return id.ends_with (name);
        case Match::has: return id.find (name) != std::string_view::npos;
        case Match::tail:
        {
            const auto underscore = id.find ('_');
            return (underscore == std::string_view::npos ? std::string_view() : id.substr (underscore + 1)) == name;
        }
        case Match::osc:
        case Match::osc12:
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
            {
                const std::string_view prefix (OscillatorIds::prefixes[(size_t) osc]);
                if (isOscPrefix (osc, match) && id.size() == prefix.size() + name.size() && id.starts_with (prefix) && id.ends_with (name))
                    return true;
            }
            return false;
        case Match::none:
        case Match::test: break;
    }
    return false;
}

template <typename Visit>
void forEachName (const char* names, Visit&& visit)
{
    const std::string_view all (names);
    for (size_t from = 0;;)
    {
        const auto bar = all.find ('|', from);
        visit (all.substr (from, bar == std::string_view::npos ? std::string_view::npos : bar - from));
        if (bar == std::string_view::npos)
            return;
        from = bar + 1;
    }
}

inline bool termMatches (const Term& term, const juce::String& id, std::string_view view)
{
    if (term.match == Match::test)
        return term.test (id);

    bool found = false;
    forEachName (term.names, [&] (std::string_view name) { found = found || nameMatches (term.match, view, name); });
    return found;
}

inline bool keyMatches (const Key& key, const juce::String& id, std::string_view view)
{
    for (const auto& term : key.terms)
        if (term.match != Match::none && termMatches (term, id, view) == term.negated)
            return false;

    return true;
}

// The rows whose first term names whole IDs (is, osc, osc12), by ID, and
// the rest in order. A lookup walks both in table order, so the first
// covering row still wins.
struct RowIndex
{
    std::vector<std::pair<std::string, int>> exact; // sorted
    std::vector<int> scanned;
};

template <typename Row, size_t count>
RowIndex indexRows (const Row (&rows)[count])
{
    RowIndex index;
    for (int row = 0; row < (int) count; ++row)
    {
        const auto& first = rows[row].key.terms[0];
        if (first.match == Match::is)
            forEachName (first.names, [&] (std::string_view name) { index.exact.emplace_back (std::string (name), row); });
        else if (first.match == Match::osc || first.match == Match::osc12)
            forEachName (first.names, [&] (std::string_view name)
            {
                for (int osc = 0; osc < OscillatorIds::count; ++osc)
                    if (isOscPrefix (osc, first.match))
                        index.exact.emplace_back (std::string (OscillatorIds::prefixes[(size_t) osc]) + std::string (name), row);
            });
        else
            index.scanned.push_back (row);
    }
    std::sort (index.exact.begin(), index.exact.end());
    return index;
}

// The first covering row for which `show` gives text (a row may decline by
// giving none), or empty.
template <typename Row, size_t count, typename Show>
juce::String firstShown (const Row (&rows)[count], const RowIndex& index, const juce::String& id, Show&& show)
{
    const auto view = viewOf (id);
    auto exact = std::lower_bound (index.exact.begin(), index.exact.end(), view,
                                   [] (const auto& entry, std::string_view name) { return std::string_view (entry.first) < name; });
    auto scanned = index.scanned.begin();

    for (;;)
    {
        const bool exactLeft = exact != index.exact.end() && std::string_view (exact->first) == view;
        const bool scannedLeft = scanned != index.scanned.end();
        if (! exactLeft && ! scannedLeft)
            return {};

        const auto row = (exactLeft && (! scannedLeft || exact->second < *scanned)) ? (exact++)->second : *scanned++;
        if (keyMatches (rows[row].key, id, view))
            if (auto text = show (rows[row]); text.isNotEmpty())
                return text;
    }
}

// ---------------------------------------------------------------------------
// Value text.

// How a row writes a value. Most need only a unit; the rest have a function.
enum class Unit : unsigned char
{
    whole,          // 3 + suffix
    percent,        // 0.25 is "25%"
    signedPercent,  // "+25%", "-25%", "0%"
    signedWhole,    // "+3" + suffix
    number1,        // describeNumber (v, 1) + suffix
    fixed1,         // one decimal + suffix
    signedFixed1,   // "+1.5" + suffix
    times1,         // "×2.5"
    times2,         // "×0.75"
    timesFixed2,    // "×1.00"
    milliseconds,   // "120 ms"
    seconds,        // "120 ms", "1.20 s"
    secondsOrZero,  // as seconds, but "0 ms" at 0
    hertz,          // "440 Hz", "1.20 kHz"
    custom
};

using ValueText = juce::String (*) (const juce::String& id, float value);

struct ValueRow
{
    constexpr ValueRow (Key k, Unit u, const char* s = "") : key (k), unit (u), suffix (s) {}
    constexpr ValueRow (Key k, ValueText text) : key (k), unit (Unit::custom), custom (text) {}

    Key key;
    Unit unit;
    const char* suffix = "";
    ValueText custom = nullptr;
};

inline juce::String times() { return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")); }

inline juce::String percentText (float value) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; }

inline juce::String secondsText (float value)
{
    // Judged after rounding, so 0.9996 s shows as "1.00 s" like 1 s does.
    return juce::roundToInt (value * 1000.0f) < 1000 ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms"
                                                     : juce::String (std::round (value * 100.0f) / 100.0f, 2) + " s";
}

inline juce::String hertzText (float value)
{
    const auto magnitude = std::abs (value);

    // Note juce::String (value, 0) means "default precision", not "no
    // decimals", so whole numbers go through roundToInt.
    return std::round (magnitude) >= 1000.0f ? juce::String (std::round (value / 10.0f) / 100.0f, 2) + " kHz"
                                             : describeNumber (value) + " Hz";
}

// Whole numbers without decimals, judged after rounding to two places
// (0.997 is "1", as 1.0 is), so text and value round-trip.
inline juce::String plainText (float value)
{
    const auto rounded = std::round (value * 100.0f) / 100.0f;
    return rounded == std::floor (rounded) ? juce::String (juce::roundToInt (rounded)) : juce::String (rounded, 2);
}

inline juce::String unitText (const ValueRow& row, const juce::String& id, float value)
{
    const juce::String suffix (row.suffix);
    switch (row.unit)
    {
        case Unit::whole: return juce::String (juce::roundToInt (value)) + suffix;
        case Unit::percent: return percentText (value);
        case Unit::signedPercent: return (juce::roundToInt (value * 100.0f) > 0 ? "+" : "") + percentText (value);
        case Unit::signedWhole: return (juce::roundToInt (value) > 0 ? "+" : "") + juce::String (juce::roundToInt (value)) + suffix;
        case Unit::number1: return describeNumber (value, 1) + suffix;
        case Unit::fixed1: return describeFixed (value, 1) + suffix;
        case Unit::signedFixed1: return (std::round (value * 10.0f) > 0.0f ? "+" : "") + describeFixed (value, 1) + suffix;
        case Unit::times1: return times() + describeNumber (value, 1);
        case Unit::times2: return times() + describeNumber (value, 2);
        case Unit::timesFixed2: return times() + describeFixed (value, 2);
        case Unit::milliseconds: return juce::String (juce::roundToInt (value)) + " ms";
        case Unit::seconds: return secondsText (value);
        // A time of 0 is "0 ms": "Off" is for switches (UI review 7, S7-29).
        case Unit::secondsOrZero: return value <= 0.0005f ? juce::String ("0 ms") : secondsText (value);
        case Unit::hertz: return hertzText (value);
        case Unit::custom: return row.custom (id, value);
    }
    return {};
}

// The values no unit covers.
namespace ValueTexts
{
inline juce::String stages (const juce::String&, float value)
{
    return juce::String (juce::roundToInt (value)) + (juce::roundToInt (value) == 1 ? " stage" : " stages");
}

// (A value, not a label that forgot one: review 11, S11-17.)
inline juce::String seed (const juce::String&, float value)
{
    return juce::roundToInt (value) == 0 ? juce::String ("Random") : "#" + juce::String (juce::roundToInt (value));
}

// Stereo position: C, or how far left or right.
inline juce::String pan (const juce::String&, float value)
{
    const auto percent = juce::roundToInt (value * 100.0f);
    return percent == 0 ? juce::String ("C") : (percent < 0 ? "L " : "R ") + juce::String (std::abs (percent));
}

// 0 is not a position: the exciter's own spot, or no pick filtering. Any
// other value goes on to the rows below.
inline juce::String autoAtZero (const juce::String&, float value) { return juce::roundToInt (value * 100.0f) == 0 ? "Auto" : ""; }
inline juce::String offAtZero (const juce::String&, float value) { return juce::roundToInt (value * 100.0f) == 0 ? "Off" : ""; }

inline juce::String rotate (const juce::String&, float value) { return "+" + juce::String (juce::roundToInt (value)); }

inline juce::String rangeAboveRoot (const juce::String&, float value)
{
    return juce::roundToInt (value) == 0 ? juce::String ("root") : "+" + juce::String (juce::roundToInt (value)) + " st";
}

inline juce::String lengthOrRest (const juce::String&, float value) { return value < 0.005f ? juce::String ("rest") : percentText (value); }

// Note name with C3 = MIDI 60, as on the keyboard strip.
inline juce::String noteName (const juce::String&, float value)
{
    static const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto note = juce::roundToInt (value);
    return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 2);
}

// A ratio as the FM page's diagram and headers write it (review 7, I7-33).
inline juce::String ratio (const juce::String&, float value)
{
    return times() + juce::String (std::round (value * 100.0f) / 100.0f, 2);
}

inline juce::String fixedHertz (const juce::String&, float value)
{
    return std::round (value) >= 1000.0f ? juce::String (std::round (value / 10.0f) / 100.0f, 2) + " kHz"
                                         : (std::round (value * 100.0f) / 100.0f < 100.0f ? juce::String (std::round (value * 100.0f) / 100.0f, 2)
                                                                                       : juce::String (std::round (value * 10.0f) / 10.0f, 1)) + " Hz";
}

inline juce::String keyLevel (const juce::String&, float value)
{
    return (std::round (value * 60.0f) > 0.0f ? "+" : "") + describeFixed (value * 6.0f, 1) + " dB/oct";
}

// The Operator Env (UI review 6): its 0-99 values in the synth's units (the
// DX7 number is in each knob's tooltip). A level is dB below the operator's
// peak (the DX7 has a step every two values up top); OUTPUT is dB below 99;
// the pitch levels are semitones; the LFO runs in Hz and its delay is the
// time until it is fully in. Rates depend on the levels around them, so the
// FM page shows their times itself.
inline juce::String egLevel (const juce::String&, float value)
{
    const auto dx = juce::roundToInt (value);
    return dx <= 0 ? silentDecibels() : describeFixed ((float) (((Dx7::scaleOutLevel (dx) >> 1) - 63) * 6.0206 / 4.0), 1) + " dB";
}

inline juce::String pitchLevel (const juce::String&, float value)
{
    const auto semitones = Dx7::PitchEnvelope::levelSteps (juce::roundToInt (value)) * 12.0 / 32.0;
    return (semitones > 0.0 ? "+" : "") + describeFixed ((float) semitones, 1) + " st";
}

inline juce::String egOutput (const juce::String&, float value)
{
    const auto dx = juce::roundToInt (value);
    return dx <= 0 ? silentDecibels() : describeFixed ((float) ((Dx7::scaleOutLevel (dx) - 127) * 6.0206 / 8.0), 1) + " dB";
}

inline juce::String lfoSpeed (const juce::String&, float value) { return describeNumber ((float) Dx7::Lfo::hz (juce::roundToInt (value)), 2) + " Hz"; }

inline juce::String lfoDelay (const juce::String&, float value)
{
    const auto dx = juce::roundToInt (value);
    if (dx <= 0)
        return "0 ms";
    const auto a = 99 - juce::jlimit (0, 99, dx);
    const auto first = (16 + (a & 15)) << (1 + (a >> 4));
    const auto second = juce::jmax (0x80, first & 0xff80);
    const auto seconds = 2147483648.0 / 25190424.0 * (1.0 / first + 1.0 / second);
    return seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms" : describeNumber ((float) seconds, 1) + " s";
}

inline juce::String ofNinetyNine (const juce::String&, float value) { return juce::String (juce::roundToInt (juce::roundToInt (value) * 100.0 / 99.0)) + "%"; }

// Review 7 (I7-12): the 0-7 and 0-3 scales in the synth's units. Key rate
// and velocity as a share of the most; amp mod in words; pitch sensitivity
// as the vibrato it gives at full PITCH DEPTH (the engine's table, about
// 4.6 st at 7); depths as a share of the most (the FM page shows them in dB
// an octave from SCALE KEY).
inline juce::String ofSeven (const juce::String&, float value) { return juce::String (juce::roundToInt (juce::jlimit (0, 7, juce::roundToInt (value)) * 100.0 / 7.0)) + "%"; }

// Amp mod as the share of the full depth each step gives (the engine's
// table: 0, 26, 43, 100 %); "Off" is for switches (V8-32).
inline juce::String ampModSensitivity (const juce::String&, float value)
{
    static const char* const names[] { "0%", "26%", "43%", "100%" };
    return names[juce::jlimit (0, 3, juce::roundToInt (value))];
}

inline juce::String pitchSensitivity (const juce::String&, float value)
{
    static constexpr int table[] { 0, 10, 20, 33, 55, 92, 153, 255 };
    const auto dx = juce::roundToInt (value);
    const auto semitones = table[juce::jlimit (0, 7, dx)] * 99.0 * 256.0 / 16777216.0 * 12.0;
    return dx <= 0 ? juce::String ("0 st") : juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + describeNumber ((float) semitones, 2) + " st";
}

inline juce::String depth (const juce::String&, float value) { return juce::String (juce::roundToInt (juce::jlimit (0, 99, juce::roundToInt (value)) * 100.0 / 99.0)) + "%"; }

// DX7 break points: 0 is A-1, 39 is C3 (middle C).
inline juce::String breakPoint (const juce::String&, float value) { return juce::MidiMessage::getMidiNoteName (juce::roundToInt (value) + 21, true, true, 3); }

// (A value, not an equation: V7-37.)
inline juce::String filterBalance (const juce::String&, float value)
{
    if (juce::roundToInt (value * 100.0f) == 0)
        return "Even";

    return value < 0.0f ? "F1 +" + juce::String (juce::roundToInt (-value * 100.0f)) + "%"
                        : "F2 +" + juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

inline juce::String bpm (const juce::String&, float value) { return juce::String (value, 1) + " BPM"; }

inline juce::String semitonesOrCents (const juce::String& id, float value)
{
    return juce::String (std::round (value * 10.0f) / 10.0f, std::round (value * 10.0f) == 10.0f * std::round (value) ? 0 : 1)
           + (id.endsWith ("_fine") || id.endsWith ("_detune") ? " ct" : " st");
}
} // namespace ValueTexts

// Every parameter's value text. A parameter no row covers is a plain number.
inline const ValueRow valueRows[] {
    // The vocoder card.
    { is ("fx_voc_bands"), Unit::whole, " bands" },
    { is ("fx_voc_width"), Unit::times2 },
    { is ("fx_voc_attack|fx_voc_release"), Unit::number1, " ms" },
    { is ("fx_voc_formant"), Unit::signedFixed1, " st" },
    { is ("fx_voc_rate"), Unit::number1, " Hz" },
    { is ("fx_voc_level"), Unit::fixed1, " dB" },
    { is ("fx_voc_unvoiced|fx_voc_mix"), Unit::percent },

    // Counts and ratios in their own words.
    { is ("arp_octaves"), Unit::whole, " oct" },
    { is ("west_stages"), ValueTexts::stages },
    { is ("fx_comp_ratio"), Unit::number1, ":1" },
    { is ("fx_crush_bits"), Unit::whole, " bits" },
    { is ("fx_crush_down"), Unit::whole, "x" },
    { is ("fx_smear_density"), Unit::whole, " grains" },
    { ends ("_seed").also (starts ("lfo")), ValueTexts::seed },
    // Amounts from -1 to 1 as signed percent (mod depths, step values).
    { ends ("_amt").also (starts ("mod")), Unit::signedPercent },
    { has ("_step").also (starts ("lfo")), Unit::signedPercent },
    // Plain 0..1 (or 0..2) amounts as percent.
    { is ("fx_feedback_tone|fx_flanger_depth|fx_dim_depth|fx_gate_smooth|fx_trem_depth|fx_delay_duck|fx_chorus_depth|fx_delay_wow"
          "|fx_phaser_depth|fx_reverb_size|fx_amp_bass|fx_amp_mid|fx_amp_treble|vec_x|vec_y"), Unit::percent },
    { starts ("fx_taps_step|vec_px|vec_py"), Unit::percent },
    { ends ("_fb_gain|_fb_distance"), Unit::percent },

    { ends ("_pan"), ValueTexts::pan },
    { ends ("_string_excite_pos"), ValueTexts::autoAtZero },
    { ends ("_string_pick_pos"), ValueTexts::offAtZero },

    // The filters' knobs in their own units.
    { is ("f1_reso|f2_reso|arp_gate"), Unit::percent },
    { is ("f1_drive|f2_drive"), Unit::times1 },
    { is ("f1_env|f2_env"), Unit::signedFixed1, " oct" },
    { is ("f1_keytrack|f2_keytrack|f1_fm|f2_fm"), Unit::signedPercent },

    // M8.3: the WEST card.
    { is ("west_decay"), Unit::timesFixed2 },
    { is ("west_strike|west_open|west_fold|west_res|west_sym"), Unit::percent },

    { is ("vec_drift"), Unit::percent },
    { is ("fx_gate_swing"), Unit::percent },
    { starts ("fx_gate_step").butNot (is ("fx_gate_steps")), Unit::percent },
    { is ("fx_gate_steps"), Unit::whole, " steps" },

    // M7.1 generative card.
    { is ("euc_steps"), Unit::whole, " steps" },
    { is ("euc_hits"), Unit::whole, " hits" },
    { is ("euc_rotate"), ValueTexts::rotate },
    { is ("euc_gate|pseq_gate"), Unit::percent },
    { starts ("pseq_chance"), Unit::percent },
    { is ("pseq_length"), Unit::whole, " steps" },
    { starts ("pseq_range"), ValueTexts::rangeAboveRoot },
    { starts ("pseq_ratchet"), Unit::whole, "x" },
    { is ("arp_steps"), Unit::whole, " steps" },
    { starts ("arp_len"), ValueTexts::lengthOrRest },
    { starts ("arp_pitch"), Unit::signedWhole, " st" },
    { is ("spray_strum_time"), Unit::milliseconds },
    { is ("spray_count"), Unit::whole, " notes" },
    { is ("sym_count"), Unit::whole, " strings" },
    { is ("sym_decay"), Unit::percent },

    // M7.5 audio input.
    { is ("in_attack|in_release"), Unit::number1, " ms" },
    { is ("in_gain|in_threshold"), Unit::fixed1, " dB" },
    { is ("in_dry|in_body|in_strings"), Unit::percent },
    { starts ("sym_note"), ValueTexts::noteName },
    { is ("in_note"), ValueTexts::noteName },
    { is ("spray_range"), Unit::whole, " st" },

    // M5 operators, M6 phase distortion, DAHDSR extras.
    { osc ("_ratio"), ValueTexts::ratio },
    { osc ("_fixed_hz"), ValueTexts::fixedHertz },
    { osc ("_key_level"), ValueTexts::keyLevel },
    { osc ("_eg_l1|_eg_l2|_eg_l3|_eg_l4"), ValueTexts::egLevel },
    { is ("opeg_pitch_l1|opeg_pitch_l2|opeg_pitch_l3|opeg_pitch_l4"), ValueTexts::pitchLevel },
    { osc ("_eg_out"), ValueTexts::egOutput },
    { is ("opeg_lfo_speed"), ValueTexts::lfoSpeed },
    { is ("opeg_lfo_delay"), ValueTexts::lfoDelay },
    { is ("opeg_lfo_pmd|opeg_lfo_amd"), ValueTexts::ofNinetyNine },
    { osc ("_eg_rate_key|_eg_vel"), ValueTexts::ofSeven },
    { osc ("_eg_ams"), ValueTexts::ampModSensitivity },
    { is ("opeg_lfo_pms"), ValueTexts::pitchSensitivity },
    { osc ("_eg_ldepth|_eg_rdepth"), ValueTexts::depth },
    { osc ("_eg_break"), ValueTexts::breakPoint },
    { is (OperatorEg::keyOffsetId), Unit::signedWhole, " st" },
    { osc ("_warp2_amt"), Unit::percent },
    { osc ("_pd_env_amt"), Unit::signedPercent },
    { ends ("_delay").butNot (starts ("fx_")), Unit::secondsOrZero },
    { ends ("_hold"), Unit::secondsOrZero },
    { ends ("_keyrate"), Unit::percent },

    { is ("spray_spread"), Unit::milliseconds },
    { starts ("fm_").butNot (is ("fm_mode")), Unit::percent },
    { is ("spray_chance|spray_velocity|arp_chance"), Unit::percent },
    { is ("filter_balance"), ValueTexts::filterBalance },

    { ends ("_cutoff|_freq|_rate"), Unit::hertz },
    { is ("lfo1_rate|lfo2_rate|mseg_rate|fx_shifter_shift"), Unit::hertz },
    { is ("fx_delay_time|fx_delay_time_r|fx_feedback_delay|fx_smear_size|fx_haas_delay|fx_comp_attack|fx_comp_release|fx_limit_release"),
      Unit::milliseconds },
    { ends ("_grain_size"), Unit::milliseconds },
    { ends ("_sample_bpm"), ValueTexts::bpm },
    { is ("poly_voices"), Unit::whole },

    // (Tape stop's time is in seconds: 0.60 s is "600 ms", not "1 ms".)
    { ends ("_attack"), Unit::seconds },
    { is ("glide|fx_tape_stop_time"), Unit::seconds },
    { ends ("_decay|_release").butNot (ends ("_string_decay")).butNot (is ("res_decay")), Unit::seconds },

    { ends ("_semi|_fine|_detune"), ValueTexts::semitonesOrCents },
    { is ("fx_delay_pitch|fx_stutter_pitch|res_offset|bend_range"), ValueTexts::semitonesOrCents },

    { is ("master|output_trim|master_clip_gain|fx_limit_ceiling|fx_tilt_level|fx_comp_makeup|fx_comp_threshold|fx_util_gain"),
      Unit::fixed1, " dB" },
    { ends ("_gain").also (starts ("fx_eq_")), Unit::fixed1, " dB" },

    { is ("fx_drive_amount|fx_amp_drive"), Unit::times1 },

    { ends ("_mix|_amount|_level|_width|_spread|_sustain|_velocity|_damping|_feedback|_curve|_frame|_start|_end|_fade_in|_fade_out"),
      Unit::percent },
    { is ("drift|ring_mod|fm_amount|fm_feedback|fx_fold|res_amount|res_keytrack|fx_tilt|fx_shifter_mix"), Unit::percent },
    { when (isAirwindowsKnob), Unit::percent },
    { is ("noise_level|noise_color|sub_colour|unison_random|voice_spread|body_material|body_size|body_coupling"), Unit::percent },
    { starts ("macro|mseg_level"), Unit::percent },
    { is ("res_decay"), Unit::percent },
    { ends ("_string_decay|_string_damp|_string_stiffness|_string_pickup|_string_excite_pos|_string_pick_hardness|_string_pick_pos"
            "|_phys_a|_phys_b"), Unit::percent },
    { ends ("_smooth|_stereo").also (starts ("lfo")), Unit::percent },
    { ends ("_bow_pressure|_bow_speed|_bridge_buzz|_fret_rattle|_hammer_hard|_couple|_damper|_register|_ep_distance|_ep_position"),
      Unit::percent },
    { is ("stretch|pedal_res|sym_amount|sym_decay"), Unit::percent },
    { starts ("sb_|mech_"), Unit::percent },
    { ends ("_warp_amt|_spectral_amt|_grain_density|_grain_spray|_grain_pitch|_grain_spread|_uni_blend|_uni_frame|_uni_warp|_phase|_morph"),
      Unit::percent },
};

inline const RowIndex& valueIndex()
{
    static const auto index = indexRows (valueRows);
    return index;
}

// ---------------------------------------------------------------------------
// Help text.

struct HelpRow
{
    Key key;
    const char* text; // UTF-8
};

// lfo1..lfo16 parameters.
inline bool isLfoParameter (const juce::String& id)
{
    return id.startsWith ("lfo") && id.length() > 4 && juce::CharacterFunctions::isDigit (id[3]) && id.contains ("_");
}

// A simulated LFO shape's own knobs, lfoN_p1, lfoN_p2...
inline bool isLfoShapeKnob (const juce::String& id)
{
    const auto suffix = id.fromFirstOccurrenceOf ("_", false, false);
    return suffix.length() == 2 && suffix[0] == 'p';
}

// env6..env16 (the per-voice envelopes added after the first five).
inline bool isEnvSixToSixteen (const juce::String& id)
{
    return id.startsWith ("env") && juce::isPositiveAndBelow (id.substring (3).getIntValue() - 6, 11);
}

inline const HelpRow helpRows[] {
    // M8.1: the simulated LFO shapes (any of the 16 LFOs).
    { tail ("smooth").also (starts ("lfo")), "Glide on the LFO's output, as a fraction of a cycle. Turns S&H, Steps and Square into slewed "
      "random or glide. Works on every shape." },
    { tail ("trigger").also (starts ("lfo")), "What restarts a simulated shape: Note (each note), Free (never), Beat (each DIVISION of the host's "
      "beat) or Generative (Euclid's hits, else the probability sequencer's steps). FIRE triggers it by hand." },
    { tail ("axis").also (starts ("lfo")), "Which axis of the attractor OUT 1 carries; OUT 2 carries the next one (X then Y, Y then Z, "
      "Z then X). Mix puts X and Z together on OUT 1, and Y on OUT 2. The graph names both." },
    { tail ("loop").also (starts ("lfo")), "Physics objects: start again once settled, instead of resting until the next trigger." },
    { tail ("seed").also (starts ("lfo")), "Which random sequence the shape plays. Free (0): every voice and every note gets its own. "
      "1-999: the same repeatable sequence everywhere, restarting on each trigger (or FIRE)." },
    { tail ("stereo").also (starts ("lfo")), "Random shapes: how far OUT 2 departs from OUT 1 (route OUT 2 to the other side, or another target)." },
    { tail ("fire").also (starts ("lfo")), "Triggers the LFO now." },
    { when (isLfoShapeKnob).also (starts ("lfo")), "The chosen shape's own parameter; its name and unit show on the knob (gravity, length, sigma...)." },
    // M7.1 generative card.
    { is ("euc_on"), "Euclidean rhythm: HITS spread as evenly as possible over STEPS." },
    { is ("euc_target"), "Notes: gates the arp's steps (with the arp off, plays the held chord on each hit). "
      "Exciter: re-strikes Physical strings on each hit. Trance Gate: the gate effect follows the rhythm." },
    { is ("euc_steps"), "Length of the rhythm in steps." },
    { is ("euc_hits"), "How many steps play, spread evenly over the length." },
    { is ("euc_rotate"), "Shifts the rhythm's start by this many steps." },
    { is ("euc_div"), "Length of one step, synced to the host tempo." },
    { is ("euc_gate"), "How long each hit holds its notes, as a share of the step." },
    { is ("pseq_on"), "Probability sequencer: plays the held keys through 16 steps, each with a chance, a pitch range and "
      "a ratchet, snapped to the scale. While on, it takes over from the arpeggiator." },
    { is ("pseq_div"), "Length of one step, synced to the host tempo." },
    { is ("pseq_length"), "How many of the 16 steps loop." },
    { is ("pseq_gate"), "How long each note holds, as a share of its step (or ratchet)." },
    { starts ("pseq_chance"), "The chance this step plays." },
    { starts ("pseq_range"), "How far above the held note it may land. In semitones, snapped to the scale." },
    { starts ("pseq_ratchet"), "Repeats within the step: 1 plays once, 4 plays four quick notes." },
    { is ("arp_steps"), "How many of the arp's 16 step lanes loop (the note order runs on by itself)." },
    { starts ("arp_vel"), "This arp step's velocity." },
    { starts ("arp_len"), "This arp step's note length, as a share of GATE (0 rests the step, 200% ties it into the next)." },
    { starts ("arp_pitch"), "Transposes this arp step, in semitones." },
    { is ("spray_strum"), "Strums chords (and sprayed notes): notes starting together are spread out, lowest first (Up) or highest first (Down)." },
    { is ("spray_strum_time"), "Time between strummed notes." },
    { is ("quality"), "Eco caps each oscillator at four unison voices; Normal preserves the original engine; "
      "High averages two wavetable reads per sample for smoother highs." },
    { osc ("_amp_env"), "The envelope for this oscillator's level. As an FM operator it sets how deep it modulates. Pick one of "
      "the 16 envelopes (Amp Env is the original), the MSEG run once per note, or the Operator Env, the DX7's "
      "envelope (four rates and levels, keyboard and velocity scaling), edited on the FM page (FM / DX7 oscillators)." },
    // The Operator Env (FM page): DX7 values 0-99, shown in the synth's units.
    { osc ("_eg_r1"), "ATTACK: how long the Operator Env takes to reach PEAK. Clockwise is longer, as on every envelope; "
      "the knob shows the time on C3. The value is the DX7's R1 (0-99, higher is faster: 99 is instant, 0 never moves)." },
    { osc ("_eg_l1"), "PEAK: the Operator Env's level 1, in dB below its top. The value is the DX7's L1 (0-99)." },
    { is ("opeg_pitch_r1"), "ATTACK: how long the pitch envelope takes to reach PEAK. Clockwise is longer. "
      "The value is the DX7's pitch R1 (0-99, higher is faster)." },
    { is ("opeg_pitch_l1"), "PEAK: the pitch envelope's level 1, in semitones from the note (the DX7's pitch L1, "
      "0-99: 50 is the note, 0 and 99 about four octaves down and up). Moves every ratio oscillator on the Operator Env." },
    { osc ("_eg_r2"), "DECAY 1: how long the Operator Env takes to reach MID. Clockwise is longer, as on every envelope; "
      "the knob shows the time on C3. The value is the DX7's R2 (0-99, higher is faster: 99 is instant, 0 never moves)." },
    { osc ("_eg_l2"), "MID: the Operator Env's level 2, in dB below its top. The value is the DX7's L2 (0-99)." },
    { is ("opeg_pitch_r2"), "DECAY 1: how long the pitch envelope takes to reach MID. Clockwise is longer. "
      "The value is the DX7's pitch R2 (0-99, higher is faster)." },
    { is ("opeg_pitch_l2"), "MID: the pitch envelope's level 2, in semitones from the note (the DX7's pitch L2, "
      "0-99: 50 is the note, 0 and 99 about four octaves down and up). Moves every ratio oscillator on the Operator Env." },
    { osc ("_eg_r3"), "DECAY 2: how long the Operator Env takes to reach SUSTAIN. Clockwise is longer, as on every envelope; "
      "the knob shows the time on C3. The value is the DX7's R3 (0-99, higher is faster: 99 is instant, 0 never moves)." },
    { osc ("_eg_l3"), "SUSTAIN: the Operator Env's level 3, held while the key is down, in dB below its top. The value is the DX7's L3 (0-99)." },
    { is ("opeg_pitch_r3"), "DECAY 2: how long the pitch envelope takes to reach SUSTAIN. Clockwise is longer. "
      "The value is the DX7's pitch R3 (0-99, higher is faster)." },
    { is ("opeg_pitch_l3"), "SUSTAIN: the pitch envelope's level 3, in semitones from the note (the DX7's pitch L3, "
      "0-99: 50 is the note, 0 and 99 about four octaves down and up). Moves every ratio oscillator on the Operator Env." },
    { osc ("_eg_r4"), "RELEASE: how long the Operator Env takes to reach END, after the key is released. Clockwise is longer, as on every envelope; "
      "the knob shows the time on C3. The value is the DX7's R4 (0-99, higher is faster: 99 is instant, 0 never moves)." },
    { osc ("_eg_l4"), "END: the Operator Env's level 4, where the release ends (and the note starts), in dB below its top. The value is the DX7's L4 (0-99)." },
    { is ("opeg_pitch_r4"), "RELEASE: how long the pitch envelope takes to reach END, after the key is released. Clockwise is longer. "
      "The value is the DX7's pitch R4 (0-99, higher is faster)." },
    { is ("opeg_pitch_l4"), "END: the pitch envelope's level 4, in semitones from the note (the DX7's pitch L4, "
      "0-99: 50 is the note, 0 and 99 about four octaves down and up). Moves every ratio oscillator on the Operator Env." },
    { osc ("_eg_out"), "Level on the Operator Env. This is the DX7's OUTPUT LEVEL, 0-99: how loud a carrier is, how deep "
      "a modulator modulates, in dB below 99. The oscillator's VOICE LEVEL (OSC page) scales it on top (50 % plays it as set)." },
    { osc ("_eg_break"), "Keyboard scaling's centre key. This is the DX7's BREAK POINT: LOW and HIGH DEPTH change the level below and above it." },
    { osc ("_eg_ldepth"), "Low-side level change. It follows LOW CURVE and shows the change an octave below SCALE KEY "
      "(the DX7's LEFT DEPTH, 0-99)." },
    { osc ("_eg_rdepth"), "High-side level change. It follows HIGH CURVE and shows the change an octave above SCALE KEY "
      "(the DX7's RIGHT DEPTH, 0-99)." },
    { osc ("_eg_lcurve|_eg_rcurve"), "Key scaling curve. Away from SCALE KEY, Down gets quieter, Up louder, along a straight (linear) or "
      "curved (exponential) line (the DX7's -LIN, -EXP, +EXP and +LIN)." },
    { osc ("_eg_rate_key"), "Key rate: higher notes run this operator's envelope faster, by up to 100% (the DX7's RATE SCALING, 0-7)." },
    { osc ("_eg_vel"), "Velocity: how much softer playing lowers this operator's level, from 0% to 100% (the DX7's KEY VELOCITY "
      "SENSITIVITY, 0-7)." },
    { osc ("_eg_ams"), "Amp modulation: how much the OP LFO's AMP DEPTH moves this operator, 0% to 100% (the DX7's AMS, 0-3) "
      "(tremolo on a carrier, wah on a modulator)." },
    { is ("opeg_lfo_speed"), "OP LFO rate (the DX7's LFO SPEED, 0-99: about 0.06 to 49 Hz)." },
    { is ("opeg_lfo_delay"), "OP LFO delay (the DX7's LFO DELAY, 0-99): the time after the key until the LFO is fully in." },
    { is ("opeg_lfo_pmd"), "OP LFO pitch depth, 0-99. This is the DX7's PMD: vibrato on every ratio oscillator on the Operator Env, scaled "
      "by PITCH SENS. The wheel and pressure add to it on DX7 voices." },
    { is ("opeg_lfo_amd"), "OP LFO amp depth, 0-99. This is the DX7's AMD: tremolo or wah on the operators whose AMP MOD is above 0." },
    { is ("opeg_lfo_sync"), "OP LFO retrigger (the DX7's KEY SYNC): each note starts the LFO from the top of its cycle." },
    { is ("opeg_lfo_wave"), "OP LFO shape: sine, triangle, saw up, saw down, square or sample and hold." },
    { is ("opeg_lfo_pms"), "Pitch sensitivity: how far PITCH DEPTH, the wheel and pressure bend the pitch, shown as the vibrato at full "
      "depth (the DX7's PMS, 0-7)." },
    { is (OperatorEg::keyOffsetId), "Shifts the key the scaling follows. In semitones (a DX7 voice's TRANSPOSE), for the operators' keyboard and rate scaling. "
      "It doesn't change the pitch: transpose with TRANSPOSE beside it, or each oscillator's SEMI." },
    { is (OperatorEg::dx7AlgorithmId), "The DX7 algorithm the FM routing came from. Set by a DX7 voice or the grid's DX7 pages, for the FM page's label." },
    { osc ("_tune"), "How the operator is tuned: in semitones (as before), as a ratio of the played note, "
      "or at a fixed frequency that ignores the keyboard (for drums and formants). SEMI and FINE still apply. "
      "Ratio and Fixed Hz are for FM / DX7 oscillators; a Wavetable is tuned in semitones." },
    { osc ("_ratio"), "Frequency ratio to the played note, used when TUNING is Ratio. SNAP pulls it to the nearest ratio of a set." },
    { osc ("_ratio_snap"), "Ratio set: Harmonic (whole numbers, clean tones), Inharmonic (square roots: metallic), "
      "Bell (partials of a tuned bell and a struck bar), or Free." },
    { osc ("_fixed_hz"), "Frequency when TUNING is Fixed Hz." },
    { osc ("_key_level"), "Level key scaling: louder (positive) or quieter (negative) towards the top of the keyboard, "
      "up to 6 dB per octave from C3. On a modulator it keeps FM brightness even across the keys." },
    { osc ("_fb_type"), "Feedback style for the FB cell. Plain (the raw last sample: buzzes at high amounts), "
      "Smooth (averaged and gently filtered: calm, saw-like at high amounts), Cross Pair (the amount runs "
      "between this oscillator and its pair: 1-2, 3-4, 5-6), or DX7 (the DX7's own: the average of the last "
      "two samples). Only acts while the operator's FB cell is above 0." },
    { osc ("_warp2"), "The second stage of the PD chain: a second warp applied after the first." },
    { osc ("_warp2_amt"), "Bend of the second warp stage." },
    { osc ("_pd_env"), "An envelope that opens the warp. Like the Casio CZ's DCW, it adds to both warp amounts." },
    { osc ("_pd_env_amt"), "Warp envelope depth. How far it moves the warp amounts; negative values close them." },
    { is ("fm_noise_type"), "Which noise modulates: Colour is the COLOUR knob's own noise (dark rumble to white); the others are the NOISE card's White, Pink, Brown, Blue, Violet and Grey." },
    { starts ("fm_noise").butNot (is ("fm_noise_color")), "The noise operator: how much noise frequency-modulates this oscillator (breath, grit, cymbals)." },
    { is ("fm_noise_color"), "Colour of the modulating noise. Dark rumble to full white noise (the NOISE FM row). "
      "The NOISE you hear, in SUB + NOISE on PLAY and OSC, is a separate source with its own COLOUR." },
    { ends ("_delay").butNot (starts ("fx_")), "DAHDSR: a wait after the note starts before the attack." },
    { ends ("_hold"), "DAHDSR: how long the envelope holds at the peak before the decay." },
    { ends ("_keyrate"), "Rate key scaling: every stage gets shorter up the keyboard (at 100%, half as long per octave above C3)." },
    { ends ("_velocity").also (when (isEnvSixToSixteen)), "How strongly velocity scales this envelope. It applies when the envelope is used as a source." },
    { ends ("_curve").also (when (isEnvSixToSixteen)), "Envelope tension: positive reaches the target early, negative reaches it late." },
    { when (isEnvSixToSixteen), "ADSR stage for this per-voice envelope. Drag its chip onto a knob, route it in MATRIX, or pick it as an oscillator amp envelope." },
    { is ("sym_on"), "Turns on the shared drone strings after the voices and before effects." },
    { is ("sym_amount"), "How much the shared strings ring in the mix." },
    { is ("sym_decay"), "How long the sympathetic strings ring on. From a quarter second to 12 seconds." },
    { is ("sym_count"), "Number of shared drone strings, from one to six." },
    { is ("sym_manual"), "Tunes each string note by note. It replaces the GENERATE scale and root. "
      "With no scale set they use an open tuning on the root (root, fifth, octave, third)." },
    { starts ("sym_note"), "Manual tuning for this drone string, used when MANUAL is on." },
    { ends ("_bow_pressure"), "Bow grip on the string. MPE pressure and channel aftertouch add to it." },
    { ends ("_bow_speed"), "Bow travel speed; changes the sustained tone and scrape." },
    { ends ("_bridge_buzz"), "Nonlinear bridge contact, from clean to sitar-like buzz." },
    { ends ("_fret_rattle"), "Velocity-scaled fret noise that follows the string. Zero is clean." },
    { osc ("_hammer_hard"), "Hammer felt hardness. Used by the Hammer and Piano exciters. Harder felt and faster keys give a shorter contact and a brighter tone." },
    { is ("in_gain"), "ilanaSynth FX: the input's level into the engine (DRY is not affected)." },
    { is ("in_dry"), "ilanaSynth FX: the untouched input, added back at the end." },
    { is ("in_body"), "ilanaSynth FX: how hard the input rings the BODY section (switch BODY on, any type but Classic)." },
    { is ("in_strings"), "ilanaSynth FX: how hard the input drives Physical strings, tines and reeds while a note plays." },
    { is ("in_trigger"), "ilanaSynth FX: Off plays on MIDI notes only; Gate plays NOTE while the input is over THRESHOLD; Drone holds NOTE down." },
    { is ("in_threshold"), "ilanaSynth FX: the level that opens the gate (it closes 6 dB below)." },
    { is ("in_note"), "ilanaSynth FX: the note the gate or drone plays. Strings, bodies and oscillators tune to it." },
    { is ("in_attack"), "ilanaSynth FX: how fast the input envelope (Input Env) rises." },
    { is ("in_release"), "ilanaSynth FX: how fast the input envelope falls (a time constant)." },
    { osc ("_grain_live"), "ilanaSynth FX: grains read the last three seconds of the input instead of the sample; POSITION is how far back." },
    { osc ("_ep_distance"), "Tine / Reed: how close the pickup sits. Closer makes hard notes bark (tine) or growl (reed)." },
    { osc ("_ep_position"), "Tine / Reed: the pickup's offset from the tine or reed. It changes the balance of even and odd harmonics." },
    { osc ("_couple"), "Strings share the bridge: needs UNISON 2-3. Fast first decay, long aftersound." },
    { osc ("_damper"), "How fast the dampers stop the string once the key (and pedal) is up. Hear it on a short note; AMP RELEASE lets the slow end through." },
    { osc ("_register"), "Changes the string across the keyboard. Stiffer and brighter in the treble, looser and longer in the bass." },
    { is ("stretch"), "Piano stretch tuning: bass slightly flat, treble slightly sharp, as a tuner does for real pianos." },
    { is ("tuning_on"), "Plays the Scala scale (and keyboard mapping) loaded from the settings menu instead of 12-TET. STRETCH still applies on top." },
    { is ("clip_on"), "Clip sequencer: plays the notes of the chosen clip. Off leaves the keyboard alone." },
    { is ("clip_index"), "Which of the eight clips plays and is shown in the piano roll." },
    { is ("clip_mode"), "Key transpose: a held key plays the clip, transposed from C3 (the clip as written), and gates it, like the arp. "
      "Host play: the clip plays in sync with the host's transport while it runs." },
    { is ("sb_on"), "A soundboard body after the voices: wooden modes driven by the strings." },
    { is ("west_on"), "The west-coast voice: a wavefolder into a low-pass gate (a vactrol-driven filter and amplifier in one)." },
    { is ("west_pos"), "After Filters: WEST processes the filters' output. Replace Filter 2: WEST takes Filter 2's place." },
    { is ("west_fold"), "How hard the wavefolder folds: 0 is almost clean, 100% folds about a dozen times." },
    { is ("west_sym"), "Offsets the fold: even harmonics, a hollower or reedier tone." },
    { is ("west_stages"), "Folders in a row: more stages, denser harmonics." },
    { is ("west_mode"), "Combo: filter and amplifier together (the classic bongo). Low Pass: the filter only. VCA: the level only." },
    { is ("west_decay"), "How long the vactrol takes to go dark, as a multiple of its own (about 250 ms to 63%, slower as it darkens)." },
    { is ("west_res"), "Resonance of the gate's filter." },
    { is ("west_strike"), "How hard the gate is struck (times velocity for a note strike, or the chosen source's level)." },
    { is ("west_open"), "Holds the gate partly open, so notes sustain under the strikes." },
    { is ("west_src"), "What strikes the gate: each note, or any mod source (an envelope, or LFO n B with Bounce for its impacts)." },
    { is ("sb_model"), "Classic: the M4 board. Dense: 48 wooden modes a side and the colour measured from a real grand (for the Piano exciter)." },
    { is ("sb_mix"), "How much soundboard resonance is heard." },
    { is ("sb_tone"), "Lid and mic position: closed and dark to open and bright." },
    { is ("sb_size"), "Soundboard size: a bigger board is lower and rings longer." },
    { is ("pedal_res"), "With the sustain pedal (CC64) down, the whole keyboard's strings ring in sympathy, shared by all voices." },
    { is ("mech_key"), "The wooden thock of a key returning when you let it go." },
    { is ("mech_damper"), "The felt of a damper landing on the strings." },
    { is ("mech_pedal"), "The sustain pedal mechanism, on press and release." },
    // Oscillators (patterned)
    { osc12 ("_table"), "Factory wavetable. Load your own with LOAD WAVETABLE (.wav)." },
    { osc12 ("_frame"), "Morphs through the table's frames. Modulate for movement." },
    { osc12 ("_level"), "Oscillator output level." },
    { osc12 ("_pan"), "Stereo position of the oscillator." },
    { osc12 ("_semi"), "Pitch offset in semitones. Hard Sync: sync ratio." },
    { osc12 ("_fine"), "Fine pitch offset in cents." },
    { osc12 ("_unison"), "Stacked detuned copies of the oscillator. More = bigger, costlier." },
    { osc12 ("_detune"), "Unison spread in cents. 20-40 is a classic supersaw." },
    { osc12 ("_spread"), "Stereo spread of the unison stack." },
    { osc12 ("_mode"), "Wavetable, Physical (a string, or the Tine and Reed electric pianos), a sample you drag onto the card, "
      "Granular: a cloud of tiny grains read from that sample (a vocal until you load your own), "
      "Live: the audio coming into ilanaSynth FX, "
      "or FM / DX7: an FM operator (tuned by ratio or fixed Hz, with the DX7's operator envelope)." },
    { osc ("_on"), "Turns this oscillator on and off. Switching fades in/out so it stays click-free." },
    { osc12 ("_excite"), "String excitation: Burst plucks, Noise/Saw/Pulse sustain the string. "
      "Tine and Reed swap the string for an electric piano: a Rhodes-style tine or a Wurlitzer-style reed. "
      "Piano Hammer strikes the string with a real felt hammer (it brightens as it compresses), with two polarisations "
      "and the bass's bark; Bright Hammer is the M4 model, kept for old presets." },
    { osc12 ("_string_decay"), "How long the string rings." },
    { osc12 ("_string_damp"), "Loop damping: higher is darker and more muted." },
    { osc12 ("_string_sustain"), "Level of the continuous excitation. Not used by Burst, Hammer, Bow or Piano." },
    { ends ("_string_stiffness"), "String stiffness. Higher values push upper harmonics sharp." },
    { ends ("_string_pickup"), "Pickup position. Move it to change the string's harmonic notches." },
    { ends ("_string_excite_pos"), "Excitation position on the string. Changes which harmonics ring. Auto (at 0) uses the exciter's natural spot, about an eighth of the string." },
    { ends ("_string_pick_hardness"), "Pick hardness. Harder picks give a brighter attack." },
    { ends ("_string_pick_pos"), "Where the pick strikes the string. Changes the attack spectrum. Off at 0." },
    { ends ("_string_slap"), "A 10 ms noise click at each note start, like a pick slapping the string. Louder on hard hits." },
    { is ("sub_level"), "Oscillator 3 level." },
    { is ("subosc_level"), "Sub oscillator an octave (or two) below the note." },
    { is ("subosc_on"), "Turns the sub oscillator on." },
    { is ("subosc_route"), "Where the sub and the noise enter the filters." },
    { is ("sub_shape"), "Sub oscillator shape: Sine for clean weight, Square/Saw for grit." },
    { is ("sub_octave"), "How far below the played note the sub sits." },
    { is ("noise_level"), "Noise volume: hats, breath, destruction. Pick the colour above; ENV and LFO can move it." },
    { is ("noise_on"), "Switch the noise on or off, apart from the sub." },
    { is ("noise_type"), "Noise colour: White is even, Pink and Brown darker, Blue and Violet brighter, Grey even to the ear." },
    { is ("sub_colour"), "Sub colour: 0 is the plain wave, higher adds warm harmonics so the sub carries on small speakers." },
    { is ("noise_color"), "Noise tone: dark rumble to full white noise (as the NOISE FM row's COLOUR on FM). Not on the NOISE card any more; older patches keep it." },
    // Cross modulation
    { is ("fm_amount"), "Osc2 phase-modulates Osc1. Raise Osc2 Level to hear it." },
    { is ("fm_feedback"), "Osc1 modulates its own phase. Noisy, chaotic FM." },
    { is ("ring_mod"), "Ring-modulates Osc1 with Osc2: metallic and clangy." },
    { is ("hard_sync"), "Resets Osc2 phase every Osc1 cycle. Tune with Osc2 SEMI for screams." },
    { is ("f1_fm|f2_fm"), "Audio-rate cutoff modulation from Osc2. Growl and filter-FM." },
    // Chords & voice
    { osc12 ("_chord"), "Spreads a chord across the unison voices. Needs Unison above 1." },
    { is ("voice_spread"), "Random stereo position per voice: instant width for pads." },
    { is ("unison_random"), "Randomises unison start phases for a softer, phasey attack." },
    { is ("mpe_mode"), "Per-note bend and pressure via MIDI channels, \xc2\xb1" "48 st bend range." },
    { is ("drift"), "Analog-style per-voice pitch instability." },
    // Filters
    { is ("f1_type|f2_type"), "SVF: clean Low / Band / High Pass and Notch.  Ladder: Moog-style, saturating and self-oscillating.  "
      "Diode: 303-style squelch.  MS-20: gnarly Sallen-Key.  Comb +/-: tuned to the cutoff, metallic "
      "to hollow.  Formant: vowels (MORPH picks A-E-I-O-U).  Morph: sweeps LP > BP > HP with MORPH." },
    { is ("f1_morph|f2_morph"), "Formant: moves through the vowels A, E, I, O, U.  Morph: blends low-pass into band-pass into "
      "high-pass.  Try an LFO on it." },
    { starts ("fx_eq_"), "Three-band EQ: low shelf, a sweepable mid peak (Q sets its width) and a high shelf.  "
      "Drag the points on the curve too." },
    { ends ("_route"), "Which filter this oscillator goes through.  Default follows the Serial / Parallel switch; "
      "Filter 2 skips Filter 1; No filter goes straight to the amp." },
    { is ("f1_slope|f2_slope"), "12 dB is gentle, 24 dB is steep and aggressive." },
    { is ("f1_cutoff|f2_cutoff"), "Filter frequency. Drag the marker on the response display too." },
    { is ("f1_reso|f2_reso"), "Emphasis at the cutoff. High values scream; near the top the filter rings on its own "
      "(self-oscillation) and can be played as a sine." },
    { is ("f1_drive|f2_drive"), "Saturates the signal going into the filter." },
    { is ("f1_env|f2_env"), "How far the filter envelope moves the cutoff, in octaves (negative closes it)." },
    { is ("f1_keytrack|f2_keytrack"), "Cutoff follows the played note. 100% tracks fully: an octave up the keyboard moves it an octave." },
    { is ("fm_mode"), "Phase: classic FM. Through-Zero: bends the pitch, even backwards. Exponential: pitch FM in octaves." },
    { osc ("_out"), "Off makes this oscillator a silent modulator: it still drives FM but isn't heard." },
    { starts ("fm_fb"), "Self-modulation: brighter, then noisier." },
    { starts ("fm_").butNot (is ("fm_mode")), "How hard one oscillator frequency-modulates another. Try whole-number pitch ratios for bells and keys." },
    { is ("gen_scale"), "SNAP TO KEY: the scale sprayed notes, PROB SEQ and the Scale Random arp snap to (in 12-TET steps, "
      "also under a Scala tuning). Off: no snapping." },
    { is ("gen_snap"), "Also snap the notes you play to SNAP TO KEY's scale." },
    { is ("spray_on"), "Each played note throws extra notes around itself." },
    { is ("spray_count"), "How many extra notes each played note can spray." },
    { is ("spray_range"), "How far (in semitones) the extra notes can land from the played note." },
    { is ("spray_spread"), "Spreads the extra notes out in time, up to this long after the played note." },
    { is ("spray_chance"), "Chance that each extra note actually plays." },
    { is ("spray_velocity"), "Randomises the extra notes' velocity." },
    { is ("arp_chance"), "Chance that each arp step plays; lower it for gaps and generative patterns." },
    { is ("fx_gate_steps"), "Trance gate pattern length. DIV sets the length of each step." },
    { is ("fx_gate_swing"), "Pushes every second step later for a shuffled gate." },
    { is ("filter_balance"), "Parallel mode: fades between Filter 1 and Filter 2. Centre plays both." },
    { is ("filters_parallel"), "Serial: F1 into F2. Parallel: both filters summed." },
    // Envelopes
    { is ("amp_attack|fe_attack|me_attack"), "Time to reach full level. Longer = softer entry." },
    { is ("amp_decay|fe_decay|me_decay"), "Time to fall from the peak to the sustain level." },
    { is ("amp_sustain|fe_sustain|me_sustain"), "Held level while the note is down." },
    { is ("amp_release|fe_release|me_release"), "Time to fade after the note is released." },
    { is ("amp_velocity|filter_velocity"), "How strongly velocity scales the envelope." },
    { is ("glide"), "Portamento time between notes." },
    { ends ("_warp").butNot (ends ("_uni_warp")), "Bends how the wave is read. Sync squeezes cycles in, Bend pushes the wave "
      "forwards or back, PWM squashes it into part of the cycle, Mirror plays it there and back, Asym "
      "skews it, Quantize steps it, FM and Ring use another oscillator (OSC 2 for OSC 1, OSC 1 for the others; the FM page draws them as "
      "dashed WARP arrows beside its own routes). "
      "PD modes are Casio CZ phase distortion (try the Sine table): Saw, Square and Pulse bend a sine into "
      "those shapes; Res I-III are the CZ's resonant waves, a sweepable formant. "
      "Formant replays the cycle faster under a window (a vowel-like peak that sweeps), Squeeze speeds up the middle of the cycle." },
    { ends ("_warp_amt"), "How hard the warp bends the wave. Modulate it for movement." },
    { ends ("_grain_size"), "Length of each grain. Short grains buzz and blur into texture; long ones keep the sample recognisable." },
    { ends ("_grain_density"), "How many grains overlap: from sparse, stuttering single grains to a thick, continuous cloud." },
    { ends ("_grain_spray"), "Random scatter of each grain's start around the position. More spray smears the sample into a cloud." },
    { ends ("_grain_pitch"), "Random detune per grain. Up to an octave either way. A little shimmers; a lot sounds like a swarm." },
    { ends ("_grain_spread"), "Stereo scatter of grains." },
    { ends ("_spectral"), "Reshapes the harmonics, not the waveform. Stretch pulls them apart into bells and "
      "metal, Shift moves them all up for hollow ring-mod tones, Odd/Even keeps odd then even harmonics, "
      "Formant moves the vowel without changing pitch, Smear blurs them into a haze, Harmonic Cut keeps only "
      "the lowest. Low Pass and High Pass roll the harmonics off above or below a corner, Phase Disperse "
      "rotates the higher ones for a chirp, Random Amps gives each its own fixed level, Skew bends the "
      "cycle's timing. Works on the factory tables." },
    { ends ("_spectral_amt"), "How far the spectral warp goes. For Formant, the middle leaves the vowel where it was. "
      "Changes rebuild the table in the background (64 steps), so modulating it follows with a short lag. "
      "Routed from an envelope, velocity, key track or random, every note morphs by its own amount (32 steps, up to 16 tables shared)." },
    { ends ("_scale").also (starts ("osc|sub")), "Snaps the pitch to a scale. The played note plus SEMI moves to the nearest scale note, so a chord "
      "or a held cluster always lands in key. Off leaves the pitch alone." },
    { ends ("_scale_root").also (starts ("osc|sub")), "The scale's root note (C to B) for this oscillator's scale snap." },
    { ends ("_uni_mode"), "How unison voices are spread: Classic evenly, Hypersaw bunched around the centre like a supersaw, "
      "Octaves, Fifths, Power, Major and Minor Chord, Harmonics and the rest also stack intervals for huge chords." },
    { ends ("_uni_frame"), "Spread across frames. The outer voices read frames up to half the table apart "
      "from the centre one, so the stack blends different timbres (Vital's frame spread)." },
    { ends ("_uni_warp"), "Spread across the warp. The outer voices get less and more of the warp than the "
      "centre one, so the stack thickens as the shape changes (Vital's distortion spread)." },
    { ends ("_uni_blend"), "Detuned voices' level against the centre. Lower keeps the pitch focused." },
    { ends ("_phase").also (starts ("lfo")), "Where the LFO starts in its cycle when a note retriggers it." },
    { ends ("_retrig").also (starts ("lfo")), "On: every note gets its own LFO, starting from the start phase (per-voice). "
      "Off: one free-running LFO shared by all notes." },
    { is ("voice_mode"), "Poly plays chords. Mono is one voice that retriggers the envelopes on every note. "
      "Legato is one voice where overlapping notes only slide the pitch: classic for basses and leads." },
    { is ("poly_voices"), "Maximum voices sounding at once in Poly mode. Fewer voices save CPU and make old notes "
      "give way sooner." },
    { is ("glide_legato"), "Only glide when notes overlap (mono modes). Detached notes jump straight to pitch." },
    { is ("bend_range"), "Pitch bend range in semitones." },
    { ends ("_shape").also (when (isLfoParameter)), "Waveform. Draw = design your own, Steps = 16-step sequencer. Random: S&H, Sine Random, Perlin and "
      "Drunk Walk, with a seed and stereo. Chaos: Lorenz, R\xc3\xb6ssler, Duffing, the logistic and H\xc3\xa9non maps and "
      "a double pendulum, solved properly. Physics: Bounce, Pendulum, Spring and Friction with real "
      "parameters. Each has two outputs (LFO n and LFO n B). The M2 versions stay as \"classic\"." },
    { ends ("_phys_a").also (when (isLfoParameter)), "Physics shape: Bounce height, Pendulum swing, Spring stiffness, or Friction drive." },
    { ends ("_phys_b").also (when (isLfoParameter)), "Physics shape: Bounce bounciness, Pendulum or Spring damping, or Friction stick time." },
    { ends ("_kick").also (when (isLfoParameter)), "Kicks a Pendulum on each note. The kick scales with velocity." },
    { ends ("_key").also (when (isLfoParameter)), "Key tracks the rate: the LFO runs per voice at the note's pitch times RATE / 4 Hz, so 4 Hz "
      "follows the note exactly. Route it to an oscillator level, an FM route or ring mod for audio-rate growl." },
    { ends ("_rate").also (when (isLfoParameter)), "Speed when Sync is off." },
    { ends ("_sync").also (when (isLfoParameter)), "Locks the LFO to host tempo." },
    { ends ("_div").also (when (isLfoParameter)), "Tempo-synced rate." },
    { has ("_step").also (when (isLfoParameter)), "Step value when the shape is Steps." },
    // Mod matrix (patterned)
    { ends ("_pol").also (starts ("mod")), "Polarity: Auto keeps the source's own range (LFOs, key, random and MSEG swing both ways; envelopes, velocity, macros and the wheel push one way); Unipolar moves 0 to +depth; Bipolar moves either side of the knob." },
    { ends ("_aux").also (starts ("mod")), "Via: a second source that scales this route (e.g. the mod wheel fading in an LFO). None = always full." },
    { ends ("_byp").also (starts ("mod")), "Bypass: switches this route off without losing its settings." },
    { ends ("_stereo").also (starts ("mod")), "Stereo: the left and right filters get opposite amounts from this route, so a filter cutoff spreads across the stereo field." },
    // Singles that had no help.
    { ends ("_sample_factory"), "Factory sample for Sample and Granular. A loaded or dropped wav replaces it." },
    { is ("gen_root"), "Root note of the GENERATE scale (scale snap, note spray, Euclid and the probability sequencer)." },
    { is ("spray_direction"), "Which way sprayed notes go from the one played: up, down or both." },
    { is ("os_factor"), "How much OVERSAMPLE raises the voice rate: 2x, or 4x for the cleanest highs at more CPU." },
    { ends ("_velocity").also (starts ("me_")), "How much note velocity scales the MOD envelope." },
    { ends ("_fb_gain"), "Feedback guitar: how loud the amp pushes sound back into the string (more sustains and blooms into feedback)." },
    { ends ("_fb_distance"), "Feedback guitar: how far the string is from the amp; changes which harmonic the feedback locks onto." },
    { starts ("fx_taps_step"), "Level of this step in the Taps delay's custom pattern." },
    { is ("vec_on"), "Vector pad on: the four corner oscillators crossfade by where the dot is (X / Y are also mod sources)." },
    { is ("vec_x"), "Vector position left to right." },
    { is ("vec_y"), "Vector position bottom to top." },
    { is ("vec_a|vec_b|vec_c|vec_d"), "The oscillator at this corner of the vector pad." },
    { is ("vec_path"), "Moves the dot round the drawn path at PATH RATE (X / Y then offset the whole path)." },
    { is ("vec_rate"), "How fast the dot goes round the path." },
    { starts ("vec_px|vec_py"), "A point of the vector path; drag the points on the pad." },
    { is ("vec_drift"), "Lets the dot wander on its own around where it is." },
    { is ("vec_drift_rate"), "How fast the drift wanders." },
    { ends ("_src").also (starts ("mod")), "Modulation source." },
    { ends ("_dst").also (starts ("mod")), "Target of the modulation." },
    { ends ("_amt").also (starts ("mod")), "Depth. Negative values invert the modulation." },
    { ends ("_level1|_level2|_level3|_level4").also (starts ("mseg_")), "MSEG point level. Drag the editor in MOD > STEPS & MSEG." },
    { ends ("_time1|_time2|_time3|_time4").also (starts ("mseg_")), "MSEG segment length (relative)." },
    { is ("mseg_rate"), "Speed of the multi-segment envelope." },
    { is ("mseg_loop"), "Loop the shape, or hold the last point." },
    { is ("clock_div"), "Tempo-synced rate of the Clocked S&H random source." },
    // Macros
    { starts ("macro"), "Assign in MOD > MATRIX, or drive it with MIDI CC 20-23." },
    // Arp
    { is ("arp_on"), "Plays held notes as a pattern. Off = normal playing. While PROB SEQ is on it plays instead." },
    { is ("arp_mode"), "The order held notes play in: up, down, both ways, random, converging, a random walk, all at once "
      "(Chord), or random notes of SNAP TO KEY's scale (Scale Random)." },
    { is ("arp_div"), "Step size, synced to host tempo." },
    { is ("arp_octaves"), "How many octaves the pattern climbs." },
    { is ("arp_gate"), "Note length within each step." },
    // BODY
    { is ("res_on"), "Turns the BODY section on. Classic keeps the original three-comb resonator." },
    { is ("body_type"), "Classic, bar, plate, bell or shell. Material bodies use the oscillator mix as their exciter." },
    { is ("body_material"), "Morph from wood through metal to glass: brightness, modal tuning and ring time." },
    { is ("body_size"), "Larger bodies ring at lower modal frequencies." },
    { is ("body_coupling_mode"), "Direction of energy transfer. String to body, body to string, or between strings." },
    { is ("body_coupling"), "Strength of the selected coupling path." },
    { is ("res_amount"), "Wet amount of the selected body." },
    { is ("res_decay"), "How long the resonances ring." },
    { is ("res_offset"), "Resonator tuning in semitones. Inharmonic = metallic." },
    { is ("res_keytrack"), "1.0 = follows the note, 0 = fixed pitch." },
    // Effects
    { is ("fx_drive_on|fx_drive_amount|fx_drive_mix"), "Saturation stage: warm to destroyed." },
    { is ("fx_fold"), "Wavefolder: folds peaks back for harsh, buzzy harmonics." },
    { is ("fx_drive_type"), "Soft Clip rounds the peaks like a tube; Hard Clip flattens them at the ceiling for a harder, brighter edge." },
    { is ("fx_fold_type"), "Linear folds the wave back in straight lines (bright, edgy); Sine folds it along a sine curve (smoother, more musical)." },
    { is ("fx_crush_on|fx_crush_bits|fx_crush_down|fx_crush_mix"), "Bit crusher: fewer bits and downsample for lo-fi digital filth." },
    { is ("fx_comb_on|fx_comb_freq|fx_comb_feedback|fx_comb_mix"), "Tuned comb resonator. Try harmonic frequencies with Physical mode." },
    { is ("fx_phaser_on|fx_phaser_rate|fx_phaser_depth|fx_phaser_feedback|fx_phaser_mix"), "Classic sweeping phaser: the Virus move." },
    { is ("fx_chorus_on|fx_chorus_rate|fx_chorus_depth|fx_chorus_mix"), "Thickening chorus for width." },
    { is ("fx_delay_on|fx_delay_time|fx_delay_sync|fx_delay_div|fx_delay_feedback|fx_delay_damping|fx_delay_pingpong|fx_delay_mix"), "Main delay. Damping darkens repeats." },
    { is ("fx_delay_pitch"), "Pitch-shifts the delay feedback: the Sophie trick." },
    { is ("fx_delay_wow"), "Tape-style pitch drift of the delay time." },
    { is ("fx_taps_on|fx_taps_pattern|fx_taps_mix"), "Multi-tap rhythmic echoes riding on the delay time." },
    { is ("fx_stutter_on|fx_stutter_div|fx_stutter_mix"), "Loops the current beat-slice. Flip on/off in time for glitches." },
    { is ("fx_smear_on|fx_smear_size|fx_smear_density|fx_smear_mix"), "Grain cloud: smears the input into a hazy texture." },
    { is ("fx_freeze_on|fx_freeze_mix"), "Spectral freeze: holds the current spectrum into a drone." },
    { is ("fx_reverb_keep_dry"), "Keeps the dry signal and adds the reverb on top. MIX 0 is the dry signal exactly, rather than "
      "trading dry for wet as MIX rises. The DX7 voices use it, so their SPACE macro opens the reverb." },
    { is ("fx_reverb_on|fx_reverb_size|fx_reverb_damping|fx_reverb_width|fx_reverb_mix"), "Space and depth." },
    { is ("fx_routing"), "How the rack's effects connect. Series: each effect feeds the next one. Parallel: every effect hears "
      "the rack's input and their outputs are averaged (so effects that leave the sound alone give it back "
      "unchanged); a soloed effect is heard alone." },
    { is ("fx_patcher"), "PATCHER: the rack's effects as boxes you wire with cables, output to input. Parallel branches, "
      "loops (each with a 1.3 ms delay) and several of the same effect. Off: SERIES or PARALLEL." },
    { is ("route_cables"), "ROUTING cables (FILTER tab): each oscillator goes where its cables go (Filter 1, Filter 2, WEST, "
      "straight out, or several at once), and the cables between the filters and WEST set their order. "
      "Off: the fixed chain (each oscillator's route, SERIAL / PARALLEL, WEST's position)." },
    { starts ("cable_"), "Where this oscillator, filter or WEST sends its signal, as the PATCH tab draws it. "
      "Heard only while the ROUTING cables are on." },
    { ends ("_bypass"), "Bypasses this rack slot without losing its settings." },
    { ends ("_solo"), "Solo (wet only): hear just this slot's output." },
    { ends ("_mix").also (starts ("fx_slot")), "Parallel blend for this slot: 1 = fully through the effect, 0 = dry." },
    { ends ("_band").also (starts ("fx_slot")), "Splitter: the part of the signal this slot works on (Full, a Low, Mid or High band, or the Mid or Side). "
      "The rest passes around it. Slots on the same band in a row are that band's chain." },
    { is ("fx_split_low"), "Splitter crossover between the Low and Mid bands (24 dB/oct)." },
    { is ("fx_split_high"), "Splitter crossover between the Mid and High bands (24 dB/oct)." },
    { starts ("fx_slot"), "Rack slot: choose which effect lives at this position." },
    { is ("fx_amp_mode"), "Tube is warm and asymmetric, Fuzz is hard, Clean is soft." },
    { is ("fx_amp_drive"), "Input gain into the amp: the main dirt control." },
    { is ("fx_amp_bass|fx_amp_mid|fx_amp_treble"), "Tone stack band gain." },
    { starts ("fx_amp_"), "Amp output level." },
    { is ("fx_comp_threshold"), "Level where compression starts." },
    { is ("fx_comp_ratio"), "How hard peaks are squashed." },
    { is ("fx_comp_attack"), "How fast the compressor clamps down." },
    { is ("fx_comp_release"), "How fast it lets go." },
    { is ("fx_comp_makeup"), "Output gain to compensate for squashing." },
    { starts ("fx_comp_"), "Dry/wet blend." },
    { is ("fx_haas_delay"), "Delays the right channel: width without chorus." },
    { is ("fx_haas_mix"), "How much of the delayed side you hear." },
    { is ("fx_reverb_type"), "Room, Hall, Plate, Shimmer, Spring, Gated, or IR (load your own impulse)." },
    { is ("fx_stutter_reverse"), "Plays the captured loop backwards." },
    { is ("fx_stutter_pitch"), "Pitches the captured loop up or down, tape style." },
    { is ("fx_feedback_amount"), "How much of the loop feeds back into itself." },
    { is ("fx_feedback_delay"), "Length of the feedback loop in milliseconds." },
    { is ("fx_feedback_tone"), "Damps the feedback loop: lower is darker." },
    { starts ("fx_feedback_"), "Mix of the recirculating feedback loop." },
    { starts ("fx_flanger_"), "Flanger: whooshing comb sweep from a very short modulated delay." },
    { starts ("fx_dim_"), "Dimension: lush slow modulated delays; width without wobble." },
    { starts ("fx_gate_"), "Tempo-synced trance gate with shaped patterns and smoothing." },
    { is ("fx_tape_stop_trigger"), "Triggers the tape stop: the signal pitches down and halts." },
    { is ("fx_tape_stop_time|fx_tape_stop_mix"), "Tape stop timing and blend." },
    { ends ("_algo").also (starts ("fx_aw")).butNot (is ("fx_aw_algo")), "Picks one of this module's effects. An Airwindows category module (Chris Johnson's, MIT). Changing it sets the knobs to that effect's own defaults." },
    { when (isAirwindowsKnob).butNot (starts ("fx_aw_")), "An Airwindows knob: what it does follows the chosen effect (its name is the knob's label)." },
    { is ("fx_aw_algo"), "Airwindows: Chris Johnson's algorithms (MIT); tape, consoles, EQ, dynamics, reverbs, stereo, lo-fi. Changing it starts the new one fresh." },
    { is ("fx_aw_mix"), "Blend of the Airwindows algorithm with the dry signal." },
    { starts ("fx_aw_"), "An Airwindows knob: what it does follows the chosen algorithm (its name is the knob's label), 0 to 1 as in the plugin." },
    { is ("fx_tilt|fx_tilt_level"), "Single-knob tone tilt: darker on one side, brighter on the other." },
    { starts ("fx_util_"), "Utility: gain trim, mono maker and polarity invert." },
    { starts ("fx_ott_"), "OTT-style three-band up/down compression. Loud, bright, modern." },
    { starts ("fx_limit_"), "Brickwall-style limiter ceiling and release." },
    { is ("fx_width|fx_width_mix"), "Stereo width via mid/side. 0 = mono, 2 = extra wide." },
    { starts ("fx_trem_"), "Tempo-free tremolo with selectable LFO shapes." },
    { is ("fx_shifter_shift|fx_shifter_mix"), "Frequency shifter (Hz, not semitones): inharmonic and metallic." },
    { is ("fx_ring_freq|fx_ring_mix"), "Ring modulation with an internal carrier oscillator." },
    { is ("fx_octaver_mix"), "Octaver: blends in an octave-down granulated layer." },
    { is ("fx_vowel_morph|fx_vowel_mix"), "Vowel filter: morphs A-E-I-O-U formant shapes." },
    { is ("fx_filter_type"), "The filter model: any of the FILTER page's types, run once on the effects' sound. In a PATCH loop it filters "
      "what comes back round." },
    { is ("fx_filter_slope"), "12 or 24 dB per octave, for the classic types (the others have their own)." },
    { is ("fx_filter_cutoff"), "Where the filter cuts or rings. Glides over about 20 ms, so sweeping it doesn't click." },
    { is ("fx_filter_reso"), "Resonance: a peak at the cutoff; high, some types ring by themselves." },
    { is ("fx_filter_drive"), "Drive into the filter: a soft clip in front, or inside the loop for 303 Acid and Moog Drive." },
    { is ("fx_filter_morph"), "The shape knob of the types that have one: vowel, comb, SEM, Airwindows." },
    { is ("fx_filter_spread"), "Opens the left side and closes the right, up to an octave apart: a stereo filter." },
    { is ("fx_voc_source"), "Where the voice comes from. Input: the audio input (ilanaSynth FX). Talk: a built-in vowel sweep of the "
      "synth's own sound, for the instrument. Auto: Input when the host gives one, else Talk." },
    { is ("fx_voc_bands"), "Number of vocoder bands, log-spaced 100 Hz to 8 kHz. More is clearer speech, fewer is rougher." },
    { is ("fx_voc_width"), "Band sharpness. Low: smooth and blurred, high: narrow, ringing and more robotic." },
    { is ("fx_voc_attack"), "How fast each band follows a rise in the voice. Short keeps consonants clear." },
    { is ("fx_voc_release"), "How fast each band lets go. Long smears the words, short is choppy." },
    { is ("fx_voc_formant"), "Shifts the synth's bands against the voice's, in semitones: the vowel character moves up or down "
      "(chipmunk to giant) without changing the notes." },
    { is ("fx_voc_unvoiced"), "Noise fed into the bands above 3 kHz, so S, T and F still come through when the synth has little up there." },
    { is ("fx_voc_rate"), "Speed of the built-in vowel sweep (Talk)." },
    { is ("fx_voc_level"), "Output level of the vocoded signal." },
    { starts ("fx_voc_"), "Blend of the vocoded signal with the dry synth." },
    { is ("fx_delay_time_r"), "Independent right-channel delay time." },
    { is ("fx_delay_duck"), "Ducks the delay repeats whenever the dry signal plays." },
    { is ("sub_mode"), "Wavetable, Physical string, a sample you drag onto the card, Granular: a cloud of tiny "
      "grains read from that sample (a vocal until you load your own), Live: the audio coming into ilanaSynth FX, "
      "or FM / DX7: an FM operator (tuned by ratio or fixed Hz, with the DX7's operator envelope)." },
    { is ("sub_table"), "Wavetable for OSC 3." },
    { is ("sub_frame"), "Morphs through the table's frames." },
    { is ("sub_pan"), "Stereo position of OSC 3." },
    { is ("sub_semi"), "OSC 3 pitch offset in semitones." },
    { is ("sub_fine"), "Fine pitch offset in cents." },
    { is ("sub_unison"), "Stacked detuned copies of OSC 3." },
    { is ("sub_detune"), "Unison spread in cents." },
    { is ("sub_spread"), "Stereo spread of the unison stack." },
    { is ("sub_excite"), "String excitation." },
    { starts ("sub_string_"), "Physical string parameter for OSC 3." },
    { is ("sub_chord"), "Spreads a chord across the unison voices." },
    { starts ("sub_"), "OSC 3 / sub oscillator." },
    { starts ("f2e_"), "Filter 2 has its own envelope." },
    { starts ("e4_"), "Envelope 4: a free mod source for the matrix." },
    { ends ("_curve"), "Envelope tension: positive is snappy, negative is slow then fast." },
    { is ("master"), "Final output level." },
    { is ("output_trim"), "The preset's own level, set when a factory preset loads (MASTER stays yours)." },
    { is ("master_clip"), "Soft-clips the output so extreme patches can't hard-clip." },
    { is ("master_clip_gain"), "Input gain into the soft clipper. Drive it to squash peaks harder." },
    { is ("midi_program_change"), "A MIDI program change loads the factory preset with that number. Off: program changes are ignored, so a host or keyboard cannot replace your patch." },
    { is ("oversampling"), "Renders the oscillators, filters and envelopes at 2x and downsamples, cutting aliasing from FM, sync and wavefolding. Uses more CPU." },
    { ends ("_sample_tuned"), "Tuned follows the played note; untuned plays the file at its original pitch." },
    { ends ("_sample_loop"), "Loops the start/end region instead of playing once." },
    { ends ("_sample_reverse"), "Plays the region backwards. With loop on, it keeps looping backwards." },
    { ends ("_sample_start"), "Start of the playback region, as a fraction of the sample." },
    { ends ("_sample_end"), "End of the playback region, as a fraction of the sample." },
    { ends ("_sample_fade_in"), "Fade-in, as a fraction of the region. Loops and one-shots never click." },
    { ends ("_sample_fade_out"), "Fades out over this fraction of the region." },
    { ends ("_sample_loop_own"), "LOOP PTS: with LOOP on, the loop is its own span between LOOP START and LOOP END (drag the two handles on the wave, "
      "or double-click one to type it). The note plays from START once, then repeats that span. Off, LOOP repeats the whole START to END region." },
    { ends ("_sample_loop_start"), "Where the loop begins. Held between START and LOOP END." },
    { ends ("_sample_loop_end"), "Where the loop ends and jumps back. Held between LOOP START and END." },
    { ends ("_sample_sync"), "Follows the host tempo. BPM: the sample's own tempo is the BPM knob. A length (1 beat to 16 bars): "
      "the region from START to END lasts that long at the sample's tempo. Off plays at the sample's own speed." },
    { ends ("_sample_bpm"), "The sample's own tempo, when SYNC is set to BPM. At the host's tempo it plays at its own speed." },
    { ends ("_sample_stretch"), "Repitch: one speed, so a faster tempo or a higher note also raises the pitch. "
      "Stretch: the note sets the pitch and the tempo sets the speed, apart (a time-stretch). "
      "Signalsmith, Bungee, PGHI and Paulstretch: higher-quality stretches, too heavy to run live, so a copy "
      "of the sample is rendered for each tempo and note (the original is kept) and the plain stretch "
      "plays until it's ready. Paulstretch smears the sound into a pad." },
};

inline const RowIndex& helpIndex()
{
    static const auto index = indexRows (helpRows);
    return index;
}
} // namespace ParamInfo

inline juce::String describeValue (const juce::String& id, float value)
{
    const auto text = ParamInfo::firstShown (ParamInfo::valueRows, ParamInfo::valueIndex(), id,
                                             [&] (const ParamInfo::ValueRow& row) { return ParamInfo::unitText (row, id, value); });
    return text.isNotEmpty() ? text : ParamInfo::plainText (value);
}

inline juce::String describeParameter (const juce::String& id)
{
    return ParamInfo::firstShown (ParamInfo::helpRows, ParamInfo::helpIndex(), id,
                                  [] (const ParamInfo::HelpRow& row) { return juce::String (juce::CharPointer_UTF8 (row.text)); });
}
