#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "../PluginProcessor.h"
#include "../dsp/OperatorEgParams.h"
#include "../dsp/OscillatorIds.h"
#include "ParamInfo.h"

// What any page needs to show an oscillator as the FM operator it is (UI
// review 6): its tuning and level as the FM page reads them, whether it
// plays the Operator Env, whether the Amp Env still shapes anything, the
// ENVELOPE menu in sections, and a way to open the FM page on an operator.
// The FM page, the MOD pools, PLAY and OSC all use these, so one oscillator
// reads the same everywhere.
namespace FmOperatorInfo
{
// The ENVELOPE choice's one name on every page.
inline constexpr const char* envelopeLabel = "ENVELOPE";

inline float read (const IlanaSynthAudioProcessor& p, const juce::String& id)
{
    const auto* value = p.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

inline juce::String prefixOf (int osc)
{
    return OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)];
}

// Added to the patch and switched on.
inline bool isPlaying (const IlanaSynthAudioProcessor& p, int osc)
{
    return p.isOscillatorShown (osc) && read (p, prefixOf (osc) + "_on") > 0.5f;
}

inline bool usesOperatorEnv (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::roundToInt (read (p, prefixOf (osc) + "_amp_env")) == OperatorEg::envelopeChoice;
}

// Some playing oscillator is on the Operator Env (its pitch envelope and LFO
// then have something to move).
inline bool anyOperatorEnv (const IlanaSynthAudioProcessor& p)
{
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (isPlaying (p, osc) && usesOperatorEnv (p, osc))
            return true;
    return false;
}

// Whether the Amp Env (ENV 1) shapes anything: an oscillator that plays it,
// or the sub and noise, which always follow it. A DX7 voice's operators all
// play the Operator Env, so there it does nothing.
inline bool ampEnvelopeInUse (const IlanaSynthAudioProcessor& p)
{
    auto anyPlaying = false;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        if (! isPlaying (p, osc))
            continue;
        anyPlaying = true;
        if (juce::roundToInt (read (p, prefixOf (osc) + "_amp_env")) == 0)
            return true;
    }
    return ! anyPlaying || read (p, "subosc_on") > 0.5f || read (p, "noise_level") > 0.0005f;
}

// "x1.00", "+7 st" or "440 Hz": how the operator is tuned.
inline juce::String tuningText (const IlanaSynthAudioProcessor& p, int osc)
{
    const auto prefix = prefixOf (osc);
    const auto tune = juce::roundToInt (read (p, prefix + "_tune"));
    if (tune == OscTuning::Ratio)
        return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (p.getSnappedRatio (osc), 2);
    if (tune == OscTuning::Fixed)
        return describeValue (prefix + "_fixed_hz", read (p, prefix + "_fixed_hz"));
    const auto semi = juce::roundToInt (read (p, prefix + "_semi"));
    return (semi > 0 ? "+" : "") + juce::String (semi) + " st";
}

// Its level: LEVEL in % or, on the Operator Env, its output level in dB with
// TRIM (LEVEL, 50% = as set) on top.
inline juce::String levelText (const IlanaSynthAudioProcessor& p, int osc)
{
    const auto prefix = prefixOf (osc);
    const auto level = read (p, prefix + "_level");
    if (! usesOperatorEnv (p, osc))
        return juce::String (juce::roundToInt (level * 100.0f)) + "%";
    const auto out = juce::roundToInt (read (p, prefix + "_eg_out"));
    if (out <= 0 || level <= 0.0f)
        return "Off";
    const auto db = (Dx7::scaleOutLevel (out) - 127) * 6.0206 / 8.0 + juce::Decibels::gainToDecibels ((double) level / 0.5);
    return describeFixed ((float) db, 1) + " dB";
}

// "x1.00 · OP ENV · OUT": an oscillator as an operator, in a line.
inline juce::String summary (const IlanaSynthAudioProcessor& p, int osc)
{
    const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    return tuningText (p, osc) + dot + (usesOperatorEnv (p, osc) ? "OP ENV" : "") + (usesOperatorEnv (p, osc) ? dot : "")
           + (read (p, prefixOf (osc) + "_out") > 0.5f ? "OUT" : "MOD");
}

// The ENVELOPE menu (oscN_amp_env) in sections: the pool's envelopes, the
// shapes, and the FM page's Operator Env. Item order (the saved index) is
// unchanged; the headings carry no id.
inline void sectionEnvelopeMenu (juce::ComboBox& combo)
{
    const auto selected = combo.getSelectedItemIndex();
    const auto names = combo.getNumItems() > 0 ? [&combo]
    {
        juce::StringArray list;
        for (int i = 0; i < combo.getNumItems(); ++i)
            list.add (combo.getItemText (i));
        return list;
    }() : juce::StringArray();
    if (names.size() <= OperatorEg::envelopeChoice)
        return;

    combo.clear (juce::dontSendNotification);
    combo.addSectionHeading ("ENVELOPES");
    for (int i = 0; i < 16; ++i)
        combo.addItem (names[i], i + 1);
    combo.addSectionHeading ("SHAPE");
    combo.addItem (names[16], 17);
    combo.addSectionHeading ("FM PAGE");
    combo.addItem (names[OperatorEg::envelopeChoice], OperatorEg::envelopeChoice + 1);
    combo.setSelectedItemIndex (selected, juce::dontSendNotification);
}

// Opens the FM page on an operator (0-5), or on the Operator Env's pitch
// envelope and LFO (-1). Set by the editor while it exists.
struct Hooks
{
    std::function<void (int)> openOperator;
};

inline Hooks& hooks()
{
    static Hooks instance;
    return instance;
}

inline void openOperator (int osc)
{
    if (hooks().openOperator != nullptr)
        hooks().openOperator (osc);
}
} // namespace FmOperatorInfo
