#pragma once

#include <cmath>

// The DAMPER knob of the string, piano and electric-piano models: how fast
// the sound dies once the key is up. The knob sets an extra ring-out time
// (T60) that follows an exponential taper, so every part of its travel
// shortens the tail by a similar ratio and the sweep is heard from the first
// quarter to the stop. The string's own loss and this one add as rates.
namespace DamperTaper
{
// T60 in seconds at middle C: 1.2 s at a hair above 0, 0.24 s at half, 48 ms
// at full. Bass strings are damped a little more slowly, treble ones faster.
inline float t60 (float damper, double frequency)
{
    const auto d = damper < 0.0f ? 0.0f : (damper > 1.0f ? 1.0f : damper);
    const auto f = frequency < 20.0 ? 20.0 : frequency;
    return (float) (1.2 * std::pow (0.04, (double) d) * std::pow (261.63 / f, 0.25));
}

// The gain on every trip round a string loop that lasts `period` samples.
inline float tripGain (float damper, double period, double sampleRate)
{
    const auto frequency = sampleRate / (period < 2.0 ? 2.0 : period);
    return (float) std::exp (-6.9078 * period / (sampleRate * (double) t60 (damper, frequency)));
}

// The per-trip gain, recomputed only when the knob or the pitch moved.
struct Cache
{
    float get (float damper, double period, double sampleRate)
    {
        if (damper != lastDamper || period != lastPeriod || sampleRate != lastRate)
        {
            gain = tripGain (damper, period, sampleRate);
            lastDamper = damper;
            lastPeriod = period;
            lastRate = sampleRate;
        }
        return gain;
    }

    float gain = 1.0f, lastDamper = -1.0f;
    double lastPeriod = -1.0, lastRate = -1.0;
};
} // namespace DamperTaper
