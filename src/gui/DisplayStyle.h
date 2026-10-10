#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// One drawing style for the synth's trace displays: the oscillator wave
// (WaveDisplay), the LFO and envelope graphs, and later the filter response
// (roadmap step 26 applies it there). Before this the LFO and envelope
// graphs stroked with butt ends and a different width from the oscillator's
// rounded 1.6 px trace, so side by side they looked like different
// instruments (ilana, 2026-10-10).
//
//   trace     a 1.6 px curved, round-capped line in the display's colour
//   area      the soft fill under a trace, 12 % of the colour
//   motion    anything that moves (a playhead, a phase dot) reads its
//             published value through IlanaAnim::PhaseTracker / EnvTracker
//             (AnimationUtils.h), which run on between audio blocks at the
//             display's frame rate; repaints come from the FrameClock,
//             gated by IlanaAnim::ChangeGate
namespace IlanaTrace
{
inline constexpr float width = 1.6f;
inline constexpr float areaAlpha = 0.12f;

inline juce::PathStrokeType stroke (float lineWidth = width)
{
    return juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
}

inline void paintTrace (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float lineWidth = width)
{
    g.setColour (colour);
    g.strokePath (path, stroke (lineWidth));
}

// The trace closed down to `baseY` between its first and last x, filled softly.
inline void paintArea (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float baseY, float fromX, float toX)
{
    auto filled = path;
    filled.lineTo (toX, baseY);
    filled.lineTo (fromX, baseY);
    filled.closeSubPath();
    g.setColour (colour.withAlpha (areaAlpha));
    g.fillPath (filled);
}
} // namespace IlanaTrace
