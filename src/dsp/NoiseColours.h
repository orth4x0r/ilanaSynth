#pragma once

#include <cmath>

// The NOISE card's six colours, made from one white stream per voice (so
// the voice's random generator is drawn once per sample whatever the type,
// and White passes it through untouched: every older patch renders as it
// did). The others are cheap IIR / FIR shapes, no allocation, and each is
// scaled to the level of white (equal RMS) with a trim for how loud it
// sounds, so switching type does not jump the level. DC: Pink, Blue,
// Violet and Grey have none by construction; Brown's leaky integrator and
// a 10 Hz blocker keep its mean at zero.
namespace NoiseColours
{
enum Type { white = 0, pink, brown, blue, violet, grey, count };

inline const char* const names[count] { "White", "Pink", "Brown", "Blue", "Violet", "Grey" };

struct Generator
{
    void prepare (double sampleRate)
    {
        const auto onePole = [sampleRate] (double hz) { return (float) (1.0 - std::exp (-6.283185307179586 * hz / sampleRate)); };
        brownLeak = (float) std::exp (-6.283185307179586 * 40.0 / sampleRate);
        brownGain = std::sqrt (1.0f - brownLeak * brownLeak) * brownTrim;
        blockerCoeff = (float) std::exp (-6.283185307179586 * 10.0 / sampleRate);
        greyLowCoeff = onePole (140.0);
        greyHighCoeff = onePole (6500.0);
        reset();
    }

    void reset()
    {
        for (auto& b : kellet)
            b = 0.0f;
        brownState = blockerIn = blockerOut = 0.0f;
        previous1 = previous2 = 0.0f;
        greyLow = greyHigh = 0.0f;
    }

    // `white` is uniform in -1..1; the result has about white's RMS (0.577).
    float process (int type, float white)
    {
        switch (type)
        {
            case pink:
            {
                const auto w = white;
                kellet[0] = 0.99886f * kellet[0] + w * 0.0555179f;
                kellet[1] = 0.99332f * kellet[1] + w * 0.0750759f;
                kellet[2] = 0.96900f * kellet[2] + w * 0.1538520f;
                kellet[3] = 0.86650f * kellet[3] + w * 0.3104856f;
                kellet[4] = 0.55000f * kellet[4] + w * 0.5329522f;
                kellet[5] = -0.7616f * kellet[5] - w * 0.0168980f;
                const auto sum = kellet[0] + kellet[1] + kellet[2] + kellet[3] + kellet[4] + kellet[5] + kellet[6] + w * 0.5362f;
                kellet[6] = w * 0.115926f;
                return sum * pinkTrim;
            }
            case brown:
            {
                brownState = brownState * brownLeak + white * brownGain;
                // A 10 Hz DC blocker: the integrator's mean stays at zero.
                const auto out = brownState - blockerIn + blockerCoeff * blockerOut;
                blockerIn = brownState;
                blockerOut = out;
                return out;
            }
            case blue:
            {
                const auto out = (white - previous1) * blueTrim;
                previous1 = white;
                return out;
            }
            case violet:
            {
                const auto out = (white - 2.0f * previous1 + previous2) * violetTrim;
                previous2 = previous1;
                previous1 = white;
                return out;
            }
            case grey:
            {
                // White shaped by about the inverse of the ear's equal-loudness
                // curve: lows and the top lifted, the 2-5 kHz presence band
                // left where it is, so it sounds even rather than hissy.
                greyLow += greyLowCoeff * (white - greyLow);
                greyHigh += greyHighCoeff * (white - greyHigh);
                return (white * 0.35f + greyLow * 2.6f + (white - greyHigh) * 0.9f) * greyTrim;
            }
            default:
                return white;
        }
    }

    // Trims found by measuring (tests: every type's level is within 3 dB of
    // white's at 44.1, 48 and 96 kHz).
    float pinkTrim = 0.33f, brownTrim = 0.8f, blueTrim = 0.56f, violetTrim = 0.35f, greyTrim = 1.1f;

    float kellet[7] {};
    float brownState = 0.0f, blockerIn = 0.0f, blockerOut = 0.0f;
    float previous1 = 0.0f, previous2 = 0.0f;
    float greyLow = 0.0f, greyHigh = 0.0f;
    float brownLeak = 0.0f, brownGain = 0.0f, blockerCoeff = 0.0f, greyLowCoeff = 0.0f, greyHighCoeff = 0.0f;
};
} // namespace NoiseColours
