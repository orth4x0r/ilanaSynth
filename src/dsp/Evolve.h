#pragma once

// M8.5: Evolve. Each macro can drift slowly around where it is set, within
// a RANGE and at a RATE, so a patch never plays quite the same twice. The
// drift is smooth (a sum of two incommensurate value-noise octaves, eased
// between random points) and deterministic for a given seed, so a render is
// repeatable. FREEZE captures the drifted values into the macros.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <cstdint>

class MacroEvolve
{
public:
    static constexpr int numMacros = 4;

    void reset (std::uint32_t seed = 4242u)
    {
        for (int m = 0; m < numMacros; ++m)
        {
            auto& d = drifts[(size_t) m];
            d.rng = seed * 2654435761u + (std::uint32_t) m * 40503u + 1u;
            d.phase = 0.0;
            d.a = next (d.rng);
            d.b = next (d.rng);
            d.c = next (d.rng);
            d.d = next (d.rng);
            d.value = 0.0f;
        }
    }

    // amount 0..1 (the largest excursion, as a share of the macro's range),
    // rate in Hz (new targets per second). Returns the offset for each macro.
    void advance (double seconds, const std::array<float, numMacros>& amount, const std::array<float, numMacros>& rate)
    {
        for (int m = 0; m < numMacros; ++m)
        {
            auto& d = drifts[(size_t) m];
            if (amount[(size_t) m] <= 0.0f)
            {
                d.value = 0.0f;
                continue;
            }
            d.phase += seconds * (double) juce::jlimit (0.001f, 20.0f, rate[(size_t) m]);
            while (d.phase >= 1.0)
            {
                d.phase -= 1.0;
                d.a = d.b;
                d.b = next (d.rng);
                d.c = d.d;
                d.d = next (d.rng);
            }
            const auto ease = [] (double t) { return t * t * (3.0 - 2.0 * t); };
            const auto slow = d.a + (d.b - d.a) * (float) ease (d.phase);
            // A second, faster wander at 2.7x (cycles through its own pair).
            const auto fastPhase = std::fmod (d.phase * 2.7, 1.0);
            const auto fast = d.c + (d.d - d.c) * (float) ease (fastPhase);
            d.value = amount[(size_t) m] * (0.75f * slow + 0.25f * fast);
        }
    }

    float offset (int macro) const { return drifts[(size_t) juce::jlimit (0, numMacros - 1, macro)].value; }

private:
    struct Drift
    {
        std::uint32_t rng = 1u;
        double phase = 0.0;
        float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f, value = 0.0f;
    };

    static float next (std::uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
    }

    std::array<Drift, numMacros> drifts;
};
