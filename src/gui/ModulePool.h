#pragma once

#include <juce_core/juce_core.h>

#include "../PluginProcessor.h"
#include "FmOperatorInfo.h"
// ENV 1-16 (0-based: amp, filter, filter 2, mod, ENV 5, ENV 6-16): their mod
// sources, and whether one plays a part in the patch. Shared by the MOD
// page's pool and PLAY's envelope tabs, so both list the same envelopes.
inline Mod::Source envelopeSource (int env)
{
    const Mod::Source fixed[] { Mod::Source::AmpEnv, Mod::Source::FilterEnv, Mod::Source::FilterEnv2, Mod::Source::ModEnv, Mod::Source::Env4 };
    return env < 5 ? fixed[juce::jlimit (0, 4, env)] : (Mod::Source) ((int) Mod::Source::Env6 + juce::jlimit (0, 10, env - 5));
}

// The amp envelope plays unless every oscillator is on the Operator Env (a
// DX7 voice: UI review 6, I6-2); the filter envelopes count when their
// filter's env amount is set; any envelope counts when an oscillator plays
// it or warps with it, or when it is routed in the matrix.
inline bool envelopeInUse (const IlanaSynthAudioProcessor& processor, int env)
{
    const auto read = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };
    const auto source = envelopeSource (env);

    if (source == Mod::Source::AmpEnv && FmOperatorInfo::ampEnvelopeInUse (processor))
        return true;

    if (source == Mod::Source::FilterEnv && std::abs (read ("f1_env")) > 0.001f)
        return true;

    if (source == Mod::Source::FilterEnv2 && std::abs (read ("f2_env")) > 0.001f)
        return true;

    // As an oscillator's amp envelope or its warp (DCW) envelope, whose
    // choices start with Off.
    for (const auto* prefix : OscillatorIds::prefixes)
        if (((int) read (juce::String (prefix) + "_amp_env") == env && source != Mod::Source::AmpEnv)
            || (int) read (juce::String (prefix) + "_pd_env") == env + 1)
            return true;

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.destination != 0 && (routing.source == source || routing.aux == source))
            return true;
    }

    return false;
}

// A card in the pool: added to the patch, or doing something in it. The amp
// envelope always has its card (greyed with "unused" on a DX7 voice).
inline bool envelopeShown (const IlanaSynthAudioProcessor& processor, int env)
{
    return env == 0 || processor.isRevealed (IlanaSynthAudioProcessor::Module::Envelope, env) || envelopeInUse (processor, env);
}

// Whether a mod source is in the patch: an LFO or envelope only once it is in
// its pool (added or in use), so menus never route one that has no card.
// Every other source always is.
inline bool modSourceInPatch (const IlanaSynthAudioProcessor& processor, Mod::Source source)
{
    if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
        return processor.isLfoShown (lfo);

    if (const auto lfo = Mod::lfoBIndexFor (source); lfo >= 0)
        return processor.isLfoShown (lfo);

    for (int env = 0; env < 16; ++env)
        if (envelopeSource (env) == source)
            return envelopeShown (processor, env);

    return true;
}
