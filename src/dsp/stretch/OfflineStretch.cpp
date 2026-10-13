#include "OfflineStretch.h"

#include <algorithm>
#include <cmath>

namespace OfflineStretch
{
Audio render (Engine engine, const Audio& input, double lengthFactor, double pitchRatio)
{
    if (input.numFrames() == 0)
        return {};

    Audio output;
    switch (engine)
    {
        case Engine::Signalsmith: output = renderSignalsmith (input, lengthFactor, pitchRatio); break;
        case Engine::Bungee: output = renderBungee (input, lengthFactor, pitchRatio); break;
        case Engine::Pghi: output = renderPghi (input, lengthFactor, pitchRatio); break;
        case Engine::Paulstretch: output = renderPaulstretch (input, lengthFactor, pitchRatio); break;
    }

    if (cancelled())
        return {};

    // Never hand back nothing: a failed engine falls back to plain
    // resampling at the right length.
    if (output.numFrames() == 0)
        output = resampleTo (input, std::max (1, (int) std::lround (input.numFrames() * std::clamp (lengthFactor, 1.0 / 16.0, 16.0))));
    output.sampleRate = input.sampleRate;
    return output;
}
} // namespace OfflineStretch
