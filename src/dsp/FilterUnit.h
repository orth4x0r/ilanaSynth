#pragma once

#include "Svf.h"

#include <cmath>
#include <complex>

// One channel of one voice filter. Wraps every filter model behind a single
// type index so the voice, the response display and the tests share it.
namespace FilterType
{
enum
{
    LowPass = 0,
    BandPass,
    HighPass,
    Notch,
    LadderLow,
    LadderHigh,
    Count
};

inline juce::StringArray getNames()
{
    return { "Low Pass", "Band Pass", "High Pass", "Notch", "Ladder LP", "Ladder HP" };
}

inline bool isSvf (int type) { return type >= LowPass && type <= Notch; }
inline bool isLadder (int type) { return type == LadderLow || type == LadderHigh; }

// Linear (small-signal) frequency response for the response display.
// s = j * f / cutoff.
inline std::complex<double> response (int type, bool slope24, double resonance, std::complex<double> s)
{
    resonance = juce::jlimit (0.0, 1.0, resonance);
    std::complex<double> h;

    if (isLadder (type))
    {
        const auto k = juce::jmin (3.95, resonance * 4.15);
        const auto onePole = 1.0 + s;
        const auto loop = std::pow (onePole, 4) + k;

        if (type == LadderLow)
            h = (1.0 + 0.5 * k) * (slope24 ? 1.0 / loop : onePole * onePole / loop);
        else
            h = slope24 ? std::pow (s, 4) / loop : s * s * onePole * onePole / loop;

        return h;
    }

    const auto k = 2.0 - 2.0 * juce::jmin (0.98, resonance);
    const auto denominator = s * s + k * s + 1.0;

    switch (type)
    {
        case BandPass: h = s / denominator; break;
        case HighPass: h = s * s / denominator; break;
        case Notch:    h = (s * s + 1.0) / denominator; break;
        default:       h = 1.0 / denominator; break;
    }

    return slope24 ? h * h : h;
}
} // namespace FilterType

// Four one-pole stages in a zero-delay feedback loop (Moog topology). The
// feedback sum is solved linearly and then saturated, which keeps the filter
// stable while letting resonance scream and self-oscillate.
class LadderFilter
{
public:
    struct Coefficients
    {
        double bigG = 0.0;     // g / (1 + g)
        double beta = 1.0;     // 1 / (1 + g)
        double k = 0.0;        // feedback, self-oscillates from about 4
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        const auto clampedCutoff = juce::jlimit (10.0, sampleRate * 0.45, cutoff);
        const auto g = std::tan (juce::MathConstants<double>::pi * clampedCutoff / sampleRate);

        Coefficients c;
        c.bigG = g / (1.0 + g);
        c.beta = 1.0 / (1.0 + g);
        c.k = juce::jlimit (0.0, 1.0, resonance) * 4.15;
        return c;
    }

    void setCoefficients (const Coefficients& c) { coeffs = c; }
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0; }

    // Returns the low-pass (or high-pass) output; slope24 picks four or two
    // stages.
    float process (float input, bool highPass, bool slope24)
    {
        const auto G = coeffs.bigG;
        const auto b = coeffs.beta;
        const auto k = coeffs.k;
        const auto G2 = G * G;
        const auto G3 = G2 * G;
        const auto G4 = G3 * G;

        const auto sigma = G3 * b * s[0] + G2 * b * s[1] + G * b * s[2] + b * s[3];

        // Low-pass loses passband level as feedback rises; make some back so
        // turning resonance up doesn't thin the sound out.
        const auto compensation = highPass ? 1.0 : 1.0 + 0.5 * k;
        auto u = ((double) input * compensation - k * sigma) / (1.0 + k * G4);
        u = std::tanh (u);

        double y[4];
        auto in = u;

        for (int stage = 0; stage < 4; ++stage)
        {
            const auto v = (in - s[stage]) * G;
            y[stage] = v + s[stage];
            s[stage] = y[stage] + v;
            in = y[stage];
        }

        if (highPass)
            return (float) (slope24 ? u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3]
                                    : u - 2.0 * y[0] + y[1]);

        return (float) (slope24 ? y[3] : y[1]);
    }

private:
    Coefficients coeffs;
    double s[4] {};
};

class FilterUnit
{
public:
    struct Coefficients
    {
        Svf::Coefficients svf;
        LadderFilter::Coefficients ladder;
    };

    static Coefficients makeCoefficients (int type, double sampleRate, double cutoff, double resonance)
    {
        Coefficients c;

        if (FilterType::isLadder (type))
            c.ladder = LadderFilter::makeCoefficients (sampleRate, cutoff, resonance);
        else
            c.svf = Svf::makeCoefficients (sampleRate, cutoff, resonance);

        return c;
    }

    void setType (int newType, bool newSlope24)
    {
        if (newType != type)
        {
            // Switching model: drop the old model's state so it can't leak a
            // click into the next note.
            reset();
            type = newType;
        }

        slope24 = newSlope24;

        if (FilterType::isSvf (type))
        {
            const auto mode = (Svf::Mode) type;
            stage1.setMode (mode);
            stage2.setMode (mode);
        }
    }

    void setCoefficients (const Coefficients& c)
    {
        if (FilterType::isLadder (type))
        {
            ladder.setCoefficients (c.ladder);
        }
        else
        {
            stage1.setCoefficients (c.svf);
            stage2.setCoefficients (c.svf);
        }
    }

    void reset()
    {
        stage1.reset();
        stage2.reset();
        ladder.reset();
    }

    float process (float input)
    {
        switch (type)
        {
            case FilterType::LadderLow:  return ladder.process (input, false, slope24);
            case FilterType::LadderHigh: return ladder.process (input, true, slope24);
            default:
            {
                auto out = stage1.processSample (input);
                return slope24 ? stage2.processSample (out) : out;
            }
        }
    }

private:
    int type = FilterType::LowPass;
    bool slope24 = false;
    Svf stage1, stage2;
    LadderFilter ladder;
};
