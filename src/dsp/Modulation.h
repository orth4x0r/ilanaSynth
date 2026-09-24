#pragma once

#include <juce_core/juce_core.h>

namespace Mod
{
enum class Source
{
    None = 0,
    Lfo1,
    Lfo2,
    ModEnv,
    FilterEnv,
    AmpEnv,
    Velocity,
    KeyTrack,
    Random,
    ModWheel,
    Aftertouch,
    Expression,
    Macro1,
    Macro2,
    Macro3,
    Macro4,
    ClockSh,
    Mseg,
    Env4,
    FilterEnv2,
    Lfo3,
    Lfo4,
    Count
};

inline int lfoIndexFor (Source source)
{
    switch (source)
    {
        case Source::Lfo1: return 0;
        case Source::Lfo2: return 1;
        case Source::Lfo3: return 2;
        case Source::Lfo4: return 3;
        default: return -1;
    }
}

enum class Destination
{
    None = 0,
    Osc1Pitch,
    Osc1Frame,
    Osc1Level,
    Osc2Pitch,
    Osc2Frame,
    Osc2Level,
    SubLevel,
    NoiseLevel,
    Filter1Cutoff,
    Filter1Reso,
    Filter2Cutoff,
    Filter2Reso,
    AmpLevel,
    Pan,
    Lfo1Rate,
    Lfo2Rate,
    FxDriveAmount,
    FxCrushMix,
    FxCombFreq,
    FxPhaserRate,
    FxChorusDepth,
    FxDelayMix,
    FxDelayFeedback,
    FxSmearMix,
    FxFreezeMix,
    FxReverbMix,
    FxReverbSize,
    SubPitch,
    SubFrame,
    Osc1SampleStart,
    Osc1SampleEnd,
    Osc2SampleStart,
    Osc2SampleEnd,
    SubSampleStart,
    SubSampleEnd,
    Count
};

struct Slot
{
    Source source = Source::None;
    Destination destination = Destination::None;
    float depth = 0.0f;
};

constexpr int maxSlots = 8;

inline juce::StringArray getSourceNames()
{
    return { "None", "LFO 1", "LFO 2", "Mod Env", "Filter Env", "Amp Env", "Velocity",
             "Key Track", "Random", "Mod Wheel", "Aftertouch", "Expression",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Clocked S&H", "MSEG",
             "Env 4", "Filter 2 Env", "LFO 3", "LFO 4" };
}

inline juce::StringArray getDestinationNames()
{
    return { "None", "Osc1 Pitch", "Osc1 Frame", "Osc1 Level", "Osc2 Pitch", "Osc2 Frame",
             "Osc2 Level", "Sub Level", "Noise Level", "Filter1 Cutoff", "Filter1 Reso",
             "Filter2 Cutoff", "Filter2 Reso", "Amp Level", "Pan", "LFO1 Rate", "LFO2 Rate",
             "Drive Amount", "Crush Mix", "Comb Freq", "Phaser Rate", "Chorus Depth",
             "Delay Mix", "Delay Feedback", "Smear Mix", "Freeze Mix", "Reverb Mix", "Reverb Size",
             "Sub Pitch", "Sub Frame", "Osc1 Sample Start", "Osc1 Sample End",
             "Osc2 Sample Start", "Osc2 Sample End", "Sub Sample Start", "Sub Sample End" };
}
} // namespace Mod
