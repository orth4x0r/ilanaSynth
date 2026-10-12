// Bungee Basic (MPL-2.0, github.com/bungee-audio-stretch/bungee), driven
// through its granular API over the whole sample.

#include "OfflineStretch.h"

#include "bungee/Bungee.h"

#include <algorithm>
#include <cmath>

namespace OfflineStretch
{
Audio renderBungee (const Audio& input, double lengthFactor, double pitchRatio)
{
    const auto numChannels = (int) input.channels.size();
    const auto inFrames = input.numFrames();
    if (numChannels == 0 || inFrames == 0)
        return {};

    lengthFactor = std::clamp (lengthFactor, 1.0 / 16.0, 16.0);
    pitchRatio = std::clamp (pitchRatio, 1.0 / 16.0, 16.0);
    const auto outFrames = std::max (1, (int) std::lround (inFrames * lengthFactor));
    const auto rate = std::max (8000, (int) std::lround (input.sampleRate));

    Bungee::Stretcher<Bungee::Basic> stretcher ({ rate, rate }, numChannels);
    Bungee::Request request {};
    request.position = 0.0;
    request.speed = 1.0 / lengthFactor;
    request.pitch = pitchRatio;
    request.reset = true;
    request.resampleMode = resampleMode_autoOut;
    stretcher.preroll (request);

    Audio output;
    output.sampleRate = input.sampleRate;
    output.channels.assign ((size_t) numChannels, {});
    for (auto& channel : output.channels)
        channel.reserve ((size_t) outFrames + 4096);

    std::vector<float> grain;
    // Grains run until the request passes the end; then the last one's
    // output is in. (A guard stops a stretcher that never gets there.)
    const auto maxGrains = 64 + (long) (4.0 * outFrames / 128.0);
    for (long count = 0; count < maxGrains && (int) output.channels[0].size() < outFrames; ++count)
    {
        const auto chunk = stretcher.specifyGrain (request);
        const auto length = chunk.end - chunk.begin;
        grain.assign ((size_t) length * (size_t) numChannels, 0.0f);
        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < length; ++i)
            {
                const auto at = chunk.begin + i;
                if (at >= 0 && at < inFrames)
                    grain[(size_t) c * (size_t) length + (size_t) i] = input.channels[(size_t) c][(size_t) at];
            }
        const auto muteHead = std::clamp (-chunk.begin, 0, length);
        const auto muteTail = std::clamp (chunk.end - inFrames, 0, length);
        stretcher.analyseGrain (grain.data(), length, muteHead, muteTail);

        Bungee::OutputChunk out {};
        stretcher.synthesiseGrain (out);

        // Each output frame's place in the input, from the requests at the
        // chunk's ends: frames before the sample's start are pre-roll.
        const auto* first = out.request[Bungee::OutputChunk::begin];
        const auto* last = out.request[Bungee::OutputChunk::end];
        if (out.data != nullptr && out.frameCount > 0 && first != nullptr && last != nullptr
            && std::isfinite (first->position) && std::isfinite (last->position))
        {
            for (int i = 0; i < out.frameCount; ++i)
            {
                const auto position = first->position + (last->position - first->position) * i / out.frameCount;
                if (position < 0.0)
                    continue;
                for (int c = 0; c < numChannels; ++c)
                    output.channels[(size_t) c].push_back (out.data[(size_t) i + (size_t) c * (size_t) out.channelStride]);
            }
        }
        stretcher.next (request);
    }

    for (auto& channel : output.channels)
    {
        channel.resize ((size_t) outFrames, 0.0f);
        for (auto& x : channel)
            if (! std::isfinite (x))
                x = 0.0f;
    }
    return output;
}
} // namespace OfflineStretch
