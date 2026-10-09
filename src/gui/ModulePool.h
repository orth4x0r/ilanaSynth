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

// Whether the matrix uses a source (as a slot's source or its VIA).
inline bool modSourceRouted (const IlanaSynthAudioProcessor& processor, Mod::Source source)
{
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.destination != 0 && (routing.source == source || routing.aux == source))
            return true;
    }

    return false;
}

// The patch-level MSEG module (UI review 8, I8-4 / S8-5 / V8-2: one MSEG).
// MSEG is an LFO shape now (SHAPE › MSEG); the old four-point module stays
// for the patches that use it: routed in the matrix, or an oscillator's
// ENVELOPE (choice 16) or warp envelope (choice 17). Only then does it have
// a card, a chip and a place in the source menus.
inline bool msegModuleInUse (const IlanaSynthAudioProcessor& processor)
{
    const auto read = [&processor] (const juce::String& id)
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        if ((processor.isOscillatorShown (osc) && juce::roundToInt (read (prefix + "_amp_env")) == 16)
            || juce::roundToInt (read (prefix + "_pd_env")) == 17)
            return true;
    }

    return modSourceRouted (processor, Mod::Source::Mseg);
}

// The Operator Env's parts (OP ENV, OP PITCH, OP LFO: UI review 8, I8-5 /
// S8-1 / V8-1) are in the patch only while an oscillator plays the Operator
// Env (a DX7 voice), or while the matrix still routes OP PITCH / OP LFO.
// Elsewhere they are absent: no card, no chip, not in the source menus.
inline bool operatorPoolShown (const IlanaSynthAudioProcessor& processor)
{
    return FmOperatorInfo::anyOperatorEnv (processor);
}

inline bool operatorSourceShown (const IlanaSynthAudioProcessor& processor, Mod::Source source)
{
    return operatorPoolShown (processor) || modSourceRouted (processor, source);
}

// Whether a mod source is in the patch: an LFO or envelope only once it is in
// its pool (added or in use), so menus never route one that has no card; the
// MSEG module and the Operator Env's sources while the patch uses them.
// Every other source always is.
inline bool modSourceInPatch (const IlanaSynthAudioProcessor& processor, Mod::Source source)
{
    if (source == Mod::Source::Mseg)
        return msegModuleInUse (processor);

    if (source == Mod::Source::OpLfo || source == Mod::Source::OpPitchEnv)
        return operatorSourceShown (processor, source);

    if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
        return processor.isLfoShown (lfo);

    if (const auto lfo = Mod::lfoBIndexFor (source); lfo >= 0)
        return processor.isLfoShown (lfo);

    for (int env = 0; env < 16; ++env)
        if (envelopeSource (env) == source)
            return envelopeShown (processor, env);

    return true;
}
