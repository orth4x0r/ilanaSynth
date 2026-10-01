// Shared by the src/processor/*.cpp files, which together define
// IlanaSynthAudioProcessor's member functions (see docs/ARCHITECTURE.md).
// File-local helpers used by several of them live here; each translation
// unit gets its own copy (anonymous namespace), so they inline as before.
#pragma once

#include "../PluginProcessor.h"
#include "../dsp/MultiSample.h"
#include "../Presets.h"
#include "../PresetTrims.h"
#include "../PresetVoicing.h"
#include "../gui/ParamInfo.h"
#include "../dsp/LfoShape.h"
#include "../dsp/Modulation.h"
#include "../dsp/SampleFactory.h"
#include "../dsp/TableFactory.h"
#include "../dsp/FmAlgorithms.h"
#include "../dsp/Voice.h"

#include <cmath>
#include <memory>
#include <set>
#include <thread>
#include <vector>

// The factory wavetables, built once per process (defined in PluginProcessor.cpp).
struct FactoryTables
{
    std::vector<std::unique_ptr<Wavetable>> tables;

    FactoryTables();
    static const FactoryTables& get();
};

// M8.6: the bounce render thread (the constructor and destructor need it complete).
class IlanaSynthAudioProcessor::BounceThread : public juce::Thread
{
public:
    BounceThread (IlanaSynthAudioProcessor& o, juce::ValueTree s, const BounceRequest& r)
        : juce::Thread ("ilanaSynth bounce"), owner (o), state (std::move (s)), request (r) {}

    void run() override
    {
        auto audio = renderBounce (state, request, &owner.bounceProgress, &cancel);
        if (cancel.load())
            return;
        {
            const juce::SpinLock::ScopedLockType lock (owner.stateLock);
            owner.bounceResult = std::move (audio);
            owner.bounceReady = true;
        }
        owner.triggerAsyncUpdate();
    }

    std::atomic<bool> cancel { false };

private:
    IlanaSynthAudioProcessor& owner;
    juce::ValueTree state;
    BounceRequest request;
};

namespace
{
// Parameter prefixes of ENV 1..16: the five named envelopes keep their ids.
juce::String envelopePrefix (int env)
{
    static const char* const named[] { "amp", "fe", "f2e", "me", "e4" };
    return env < 5 ? juce::String (named[env]) : "env" + juce::String (env + 1);
}

constexpr int numVoices = 32;

juce::StringArray getSyncDivisionNames()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
}

double getSyncDivisionBeats (int index)
{
    static const double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                    2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };

    return beats[juce::jlimit (0, 10, index)];
}

float lfoValue (int shape, double phase, float sampleHold, const float* customShape = nullptr,
                const float* curveTable = nullptr)
{
    switch (shape)
    {
        case IlanaSynthAudioProcessor::curveShape:
        {
            if (curveTable == nullptr)
                return 0.0f;

            const auto position = phase * (double) LfoCurve::tableSize;
            const auto index = (int) position % LfoCurve::tableSize;
            const auto next = (index + 1) % LfoCurve::tableSize;
            const auto frac = (float) (position - std::floor (position));
            return curveTable[index] + (curveTable[next] - curveTable[index]) * frac;
        }

        case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        case 1: return (float) (1.0 - 4.0 * std::abs (phase - 0.5));
        case 2: return (float) (2.0 * phase - 1.0);
        case 3: return (float) (1.0 - 2.0 * phase);
        case 4: return phase < 0.5 ? 1.0f : -1.0f;
        case 5: return sampleHold;
        case 6:
        {
            if (customShape == nullptr)
                return 0.0f;

            const auto position = phase * 64.0;
            const auto index = (int) position % 64;
            const auto next = (index + 1) % 64;
            const auto frac = (float) (position - std::floor (position));

            return customShape[index] + (customShape[next] - customShape[index]) * frac;
        }
        default: return 0.0f;
    }
}

float foldTriangle (float value)
{
    value = std::fmod (value + 1.0f, 4.0f);

    if (value < 0.0f)
        value += 4.0f;

    return value <= 2.0f ? value - 1.0f : 3.0f - value;
}

// Keeps extreme module stacks from overflowing into Inf/NaN and latching
// into delay lines or reverb tanks. 24 dB above full scale, inaudible normally.
void sanitiseBuffer (juce::AudioBuffer<float>& buffer)
{
    constexpr auto limit = 16.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = data[i];
            data[i] = std::isfinite (value) ? juce::jlimit (-limit, limit, value) : 0.0f;
        }
    }
}

juce::StringArray getOscTableChoices()
{
    auto choices = TableFactory::getFactoryTableNames();

    for (int i = 1; i <= IlanaSynthAudioProcessor::numUserSlots; ++i)
        choices.add ("User " + juce::String (i));

    return choices;
}
} // namespace
