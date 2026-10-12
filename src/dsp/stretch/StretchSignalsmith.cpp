// Signalsmith Stretch (MIT, github.com/Signalsmith-Audio/signalsmith-stretch),
// run over the whole sample at once with its exact() call. Pitch is a
// stretch to lengthFactor * pitchRatio and a resample back: its own
// transpose measured 4-7 cents sharp on steady tones, the resample is exact.

#include "OfflineStretch.h"

#include "signalsmith-stretch.h"

#include <algorithm>
#include <cmath>

namespace OfflineStretch
{
namespace
{
Audio stretchOnly (const Audio& input, double lengthFactor)
{
    const auto numChannels = (int) input.channels.size();
    const auto inFrames = input.numFrames();
    const auto outFrames = std::max (1, (int) std::lround (inFrames * lengthFactor));

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    stretch.presetDefault (numChannels, (float) input.sampleRate);

    // exact() needs a block or so of input: pad short samples with silence
    // (and the output in proportion), then cut back.
    const auto padFrames = std::max (0, 2 * (stretch.blockSamples() + stretch.intervalSamples()) - inFrames);
    std::vector<std::vector<float>> in (input.channels);
    for (auto& channel : in)
        channel.resize ((size_t) (inFrames + padFrames), 0.0f);
    const auto paddedOut = std::max (outFrames, (int) std::lround ((inFrames + padFrames) * lengthFactor));

    Audio output;
    output.sampleRate = input.sampleRate;
    output.channels.assign ((size_t) numChannels, std::vector<float> ((size_t) paddedOut, 0.0f));

    std::vector<const float*> inPointers;
    std::vector<float*> outPointers;
    for (int c = 0; c < numChannels; ++c)
    {
        inPointers.push_back (in[(size_t) c].data());
        outPointers.push_back (output.channels[(size_t) c].data());
    }

    if (! stretch.exact (inPointers.data(), inFrames + padFrames, outPointers.data(), paddedOut))
        return {};

    for (auto& channel : output.channels)
    {
        channel.resize ((size_t) outFrames);
        for (auto& x : channel)
            if (! std::isfinite (x))
                x = 0.0f;
    }
    return output;
}
} // namespace

Audio renderSignalsmith (const Audio& input, double lengthFactor, double pitchRatio)
{
    const auto inFrames = input.numFrames();
    if (input.channels.empty() || inFrames == 0)
        return {};

    lengthFactor = std::clamp (lengthFactor, 1.0 / 16.0, 16.0);
    pitchRatio = std::clamp (pitchRatio, 1.0 / 16.0, 16.0);
    const auto outFrames = std::max (1, (int) std::lround (inFrames * lengthFactor));
    if (std::abs (pitchRatio - 1.0) < 1.0e-9)
        return stretchOnly (input, lengthFactor);

    const auto stretched = stretchOnly (input, lengthFactor * pitchRatio);
    if (stretched.numFrames() == 0)
        return {};
    return resampleTo (stretched, outFrames);
}
} // namespace OfflineStretch
