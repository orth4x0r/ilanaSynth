#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <vector>

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

// Shared by the editors whose drag writes parameters directly (step, gate,
// tap and MSEG editors, the vector pad's path points): the host's change
// gesture is opened on exactly the parameters the drag touches and closed on
// mouse-up, and their starting values are kept so Escape can put them back
// (UI review 14, G-03, G-15). Message thread only.
namespace IlanaDrag
{
class Gestures
{
public:
    // Opens the gesture on `parameter` the first time the drag touches it.
    void open (juce::RangedAudioParameter* parameter)
    {
        if (parameter == nullptr)
            return;

        for (const auto& item : touched)
            if (item.parameter == parameter)
                return;

        parameter->beginChangeGesture();
        touched.push_back ({ parameter, parameter->getValue() });
    }

    void set (juce::RangedAudioParameter* parameter, float normalised)
    {
        if (parameter == nullptr)
            return;

        open (parameter);
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }

    // Escape: every touched parameter back to its value at the drag's start.
    // The drag then ignores the mouse until it is released.
    void cancel()
    {
        for (const auto& item : touched)
            item.parameter->setValueNotifyingHost (item.start);

        wasCancelled = true;
    }

    // Closes the gestures. Call on mouse-up (and after a one-shot write).
    void end()
    {
        for (const auto& item : touched)
            item.parameter->endChangeGesture();

        touched.clear();
    }

    void begin() { wasCancelled = false; }
    bool isOpen() const { return ! touched.empty(); }
    bool cancelled() const { return wasCancelled; }

private:
    struct Item { juce::RangedAudioParameter* parameter; float start; };
    std::vector<Item> touched;
    bool wasCancelled = false;
};

// A fast sweep skips columns between mouse events: `write (index, value)` for
// every column from the last one painted to this one, the value running
// linearly between them. `lastIndex < 0` means nothing was painted yet.
template <typename Write>
inline void paintRun (int lastIndex, float lastValue, int index, float value, Write&& write)
{
    if (lastIndex < 0 || lastIndex == index)
    {
        write (index, value);
        return;
    }

    const auto count = std::abs (index - lastIndex);
    const auto direction = index > lastIndex ? 1 : -1;

    for (int i = 1; i <= count; ++i)
        write (lastIndex + direction * i, lastValue + (value - lastValue) * (float) i / (float) count);
}
} // namespace IlanaDrag
