#pragma once

#include <juce_core/juce_core.h>

// Roadmap step 40: a sample that follows the host tempo. SYNC names the
// sample's own tempo, either as a BPM or as the length of its played region
// (Start to End) in beats or 4/4 bars; the sample then plays at the host's
// tempo over its own.
namespace SampleTempo
{
inline juce::StringArray syncNames()
{
    return { "Off", "BPM", "1 Beat", "2 Beats", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars" };
}

inline constexpr int off = 0, bpm = 1;

// The region's length in beats for a bar-length choice; 0 for Off and BPM.
inline double beatsFor (int sync) noexcept
{
    constexpr double beats[] { 0.0, 0.0, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return sync >= 0 && sync < (int) std::size (beats) ? beats[sync] : 0.0;
}

// The sample's own tempo: the BPM setting, or the one at which the region
// lasts its beats. 0 when SYNC is off (or the region has no length).
inline double sourceBpm (int sync, double bpmSetting, double regionSeconds) noexcept
{
    if (sync == bpm)
        return juce::jlimit (20.0, 999.0, bpmSetting);
    const auto beats = beatsFor (sync);
    if (beats <= 0.0 || regionSeconds <= 1.0e-4)
        return 0.0;
    return juce::jlimit (5.0, 2000.0, beats * 60.0 / regionSeconds);
}

// How much faster than its own speed the sample plays: host over source
// tempo (1 when SYNC is off), kept to a sane range.
inline double tempoRatio (int sync, double bpmSetting, double regionSeconds, double hostBpm) noexcept
{
    const auto source = sourceBpm (sync, bpmSetting, regionSeconds);
    if (source <= 0.0)
        return 1.0;
    return juce::jlimit (1.0 / 16.0, 16.0, juce::jmax (20.0, hostBpm) / source);
}
} // namespace SampleTempo
