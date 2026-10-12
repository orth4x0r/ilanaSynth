#pragma once

// Roadmap step 40, follow-up (ilana 2026-10-12): high-quality stretchers that
// render a stretched copy of a sample off the audio thread, whenever the
// tempo, the region's length or the played note changes. Plain C++ (no
// JUCE), so each engine can be tested on its own.

#include <vector>

namespace OfflineStretch
{
// Appended after Repitch and Stretch in the TIME menu (oscN_sample_stretch).
enum class Engine
{
    Signalsmith, // Signalsmith Stretch (MIT): phase vocoder with predicted phases
    Bungee,      // Bungee Basic (MPL-2.0): granular phase vocoder
    Pghi,        // phase vocoder, phases rebuilt by Phase Gradient Heap Integration
    Paulstretch  // Paul Nasca's extreme stretch: long windows, random phases
};

struct Audio
{
    std::vector<std::vector<float>> channels; // one vector per channel, equal lengths
    double sampleRate = 44100.0;

    int numFrames() const noexcept { return channels.empty() ? 0 : (int) channels[0].size(); }
};

// Renders input stretched in time by lengthFactor (output length =
// round(input length * lengthFactor), exactly) and shifted in pitch by
// pitchRatio (2 = an octave up). Both are clamped to [1/16, 16]. Returns an
// empty Audio on failure. Thread-safe (no shared state); can take a while.
Audio render (Engine engine, const Audio& input, double lengthFactor, double pitchRatio);

// The engines, each in its own file.
Audio renderSignalsmith (const Audio& input, double lengthFactor, double pitchRatio);
Audio renderBungee (const Audio& input, double lengthFactor, double pitchRatio);
Audio renderPghi (const Audio& input, double lengthFactor, double pitchRatio);
Audio renderPaulstretch (const Audio& input, double lengthFactor, double pitchRatio);

// Shared helper: band-limited resampling of every channel to newLength frames
// (windowed sinc), for engines that stretch time only and shift pitch by
// stretching to lengthFactor * pitchRatio, then resampling.
Audio resampleTo (const Audio& input, int newLength);
} // namespace OfflineStretch
