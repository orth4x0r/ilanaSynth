// Roadmap step 16: reroutable oscillators. The ROUTING cables are
// parameters (route_cables and one cable_* mask per source and stage,
// dsp/VoiceRouting.h); this file reads them for the voices and writes them
// for the page.

#include "ProcessorInternal.h"

bool IlanaSynthAudioProcessor::areRouteCablesOn() const
{
    return getRawParam (routeCablesRef) > 0.5f;
}

VoiceRouting::Patch IlanaSynthAudioProcessor::getRoutePatch() const
{
    VoiceRouting::Patch patch;
    for (int index = 0; index < VoiceRouting::numCableParams; ++index)
    {
        const auto mask = (std::uint8_t) juce::jlimit (0, 15, juce::roundToInt (getRawParam (cableRefs[(size_t) index])));
        if (index < VoiceRouting::numSources)
            patch.source[(size_t) index] = mask;
        else
            patch.stage[(size_t) (index - VoiceRouting::numSources)] = mask;
    }
    return patch;
}

VoiceRouting::Patch IlanaSynthAudioProcessor::getChainAsCables() const
{
    std::array<int, VoiceRouting::numSources> routes {};
    for (int osc = 0; osc < VoiceRouting::numOscSources; ++osc)
        routes[(size_t) osc] = juce::roundToInt (getParam (oscShapeIds[(size_t) osc].route));
    routes[(size_t) VoiceRouting::subNoiseSource] = juce::roundToInt (getParam ("subosc_route"));
    return VoiceRouting::fromChain (routes, getParam ("filters_parallel") > 0.5f, getParam ("west_on") > 0.5f,
                                    juce::roundToInt (getParam ("west_pos")));
}

void IlanaSynthAudioProcessor::setRouteCables (const VoiceRouting::Patch& patch, bool on)
{
    const auto write = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (value);
            if (parameter->getValue() != normalised)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        }
    };

    const auto clean = VoiceRouting::withoutLoops (patch);
    for (int index = 0; index < VoiceRouting::numCableParams; ++index)
        write (VoiceRouting::cableParamId (index),
               (float) (index < VoiceRouting::numSources ? clean.source[(size_t) index] : clean.stage[(size_t) (index - VoiceRouting::numSources)]));
    write ("route_cables", on ? 1.0f : 0.0f);

    // A cable into WEST plays it: WEST switches on (off, it would pass the
    // signal through untouched and the cable would seem to do nothing).
    if (on)
    {
        const auto table = VoiceRouting::compile (clean);
        if (table.used[VoiceRouting::west] && getParam ("west_on") < 0.5f)
            write ("west_on", 1.0f);
    }
}

const VoiceRouting::Table& IlanaSynthAudioProcessor::currentRoutingTable()
{
    static const VoiceRouting::Table chain; // off: the fixed chain

    if (getRawParam (routeCablesRef) < 0.5f)
        return chain;

    // Ten 4-bit masks.
    std::uint64_t signature = 0;
    for (int index = 0; index < VoiceRouting::numCableParams; ++index)
        signature = (signature << 4) | (std::uint64_t) juce::jlimit (0, 15, juce::roundToInt (getRawParam (cableRefs[(size_t) index])));

    if (signature != routingSignature)
    {
        routingTable = VoiceRouting::compile (getRoutePatch());
        routingSignature = signature;
    }
    return routingTable;
}
