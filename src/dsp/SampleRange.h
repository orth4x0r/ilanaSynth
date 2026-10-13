#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <optional>

// The sample display's zoom window and the typed START / END positions
// (REVIEW-PLAN step 30). Pure maths, no drawing, so the unit tests reach it.
namespace SampleRange
{
// The part of the sample the display shows, as fractions of its length.
struct View
{
    double lo = 0.0, hi = 1.0;

    double span() const noexcept { return hi - lo; }
    bool isFull() const noexcept { return lo <= 0.0 && hi >= 1.0; }
    double toView (double fraction) const noexcept { return (fraction - lo) / span(); }
    double fromView (double position) const noexcept { return lo + position * span(); }
};

// The narrowest window: 32 samples (a whole sample when it is shorter).
inline double minSpan (int numSamples) noexcept
{
    return juce::jlimit (1.0e-9, 1.0, 32.0 / (double) juce::jmax (1, numSamples));
}

// Zoom by `factor` (above 1 zooms in) keeping the point at `anchor` (a
// position within the window, 0..1) where it is.
inline View zoomed (View view, double anchor, double factor, int numSamples) noexcept
{
    const auto fraction = view.fromView (juce::jlimit (0.0, 1.0, anchor));
    const auto span = juce::jlimit (minSpan (numSamples), 1.0, view.span() / juce::jmax (1.0e-6, factor));
    View next;
    next.lo = juce::jlimit (0.0, 1.0 - span, fraction - juce::jlimit (0.0, 1.0, anchor) * span);
    next.hi = next.lo + span;
    return next;
}

// Slide the window by a fraction of its own width.
inline View panned (View view, double windowFractions) noexcept
{
    const auto span = view.span();
    View next;
    next.lo = juce::jlimit (0.0, 1.0 - span, view.lo + windowFractions * span);
    next.hi = next.lo + span;
    return next;
}

// The same window, centred on a sample position (for a handle that is
// about to be placed).
inline View centred (View view, double fraction) noexcept
{
    return panned (view, (fraction - (view.lo + view.hi) * 0.5) / view.span());
}

// Roadmap step 27: the loop's own span inside the played range. `from` and
// `to` are the loop markers, `start` and `end` the played range (all in the
// same unit: fractions or sample frames). The markers are held inside the
// range and never cross; a span shorter than `minimum` is no loop (the
// player then wraps the whole range, as before).
struct Loop
{
    double from = 0.0, to = 0.0;
    bool valid = false;
};

inline Loop resolveLoop (double start, double end, double from, double to, double minimum) noexcept
{
    Loop loop;
    loop.from = juce::jlimit (start, juce::jmax (start, end), from);
    loop.to = juce::jlimit (loop.from, juce::jmax (loop.from, end), to);
    loop.valid = loop.to - loop.from >= minimum;
    return loop;
}

// Roadmap step 28: the zero crossing nearest to `position` (a sample frame)
// within `radius` frames, on the two channels' mix (`right` may be null for
// a mono sample). Linear interpolation puts it between frames. Where the
// wave never crosses zero in reach, the position stays as it was.
inline double nearestZeroCrossing (const float* left, const float* right, int numSamples, double position, int radius) noexcept
{
    if (left == nullptr || numSamples < 2)
        return position;

    const auto mixAt = [&] (int i) { return right != nullptr ? (left[i] + right[i]) * 0.5f : left[i]; };
    const auto centre = juce::jlimit (0, numSamples - 2, (int) std::floor (position));

    // Whether the wave crosses zero between frame i and i + 1, and where.
    const auto crossingAt = [&] (int i, double& where)
    {
        if (i < 0 || i > numSamples - 2)
            return false;
        const auto a = mixAt (i), b = mixAt (i + 1);
        if (a == 0.0f)
        {
            where = (double) i;
            return true;
        }
        if ((a < 0.0f) == (b < 0.0f) && b != 0.0f)
            return false;
        where = (double) i + (double) (a / (a - b));
        return true;
    };

    auto best = position;
    auto bestDistance = 1.0e18;
    for (int step = 0; step <= radius; ++step)
        for (const auto i : { centre - step, centre + step })
        {
            double where = 0.0;
            if (crossingAt (i, where) && std::abs (where - position) < bestDistance)
            {
                best = where;
                bestDistance = std::abs (where - position);
            }
        }
    return best;
}

// "120", "120 ms" (milliseconds), "0.25 s" / "0.25s" (seconds), "4410 smp"
// / "4410 samples" (sample frames) -> a position as a fraction of the
// sample, clamped to 0..1. Nothing for text that isn't a position.
inline std::optional<double> parsePosition (const juce::String& text, double sampleRate, int numSamples)
{
    const auto trimmed = text.trim().toLowerCase();
    if (trimmed.isEmpty() || numSamples < 1 || sampleRate <= 0.0)
        return std::nullopt;

    const auto numberChars = trimmed.initialSectionContainingOnly ("0123456789.+-");
    if (numberChars.isEmpty() || numberChars.containsAnyOf ("+-") || numberChars.indexOfChar ('.') != numberChars.lastIndexOfChar ('.'))
        return std::nullopt;

    const auto value = numberChars.getDoubleValue();
    const auto unit = trimmed.substring (numberChars.length()).trim();
    double frames = 0.0;

    if (unit.isEmpty() || unit == "ms")
        frames = value * 0.001 * sampleRate;
    else if (unit == "s" || unit == "sec")
        frames = value * sampleRate;
    else if (unit == "smp" || unit == "samp" || unit == "sample" || unit == "samples")
        frames = value;
    else
        return std::nullopt;

    return juce::jlimit (0.0, 1.0, frames / (double) numSamples);
}

// A position as the editor shows it: milliseconds, with decimals only where
// they say something.
inline juce::String formatPosition (double fraction, double sampleRate, int numSamples)
{
    const auto milliseconds = fraction * (double) numSamples / juce::jmax (1.0, sampleRate) * 1000.0;
    return juce::String (milliseconds, milliseconds < 100.0 ? 2 : 1) + " ms";
}
} // namespace SampleRange
