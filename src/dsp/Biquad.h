#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <complex>

// RBJ-cookbook biquad. Coefficients are plain values (no heap), so the EQ
// can recompute them on the audio thread every block.
struct Biquad
{
    struct Coefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        // Magnitude at a frequency, for the response display.
        double magnitude (double frequency, double sampleRate) const
        {
            const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
            const std::complex<double> z1 = std::polar (1.0, -w);
            const auto z2 = z1 * z1;
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
        }
    };

    static Coefficients normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    static Coefficients makePeak (double sampleRate, double frequency, double q, double gainDb)
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.45, frequency) / sampleRate;
        const auto alpha = std::sin (w) / (2.0 * juce::jmax (0.05, q));
        const auto cosw = std::cos (w);

        return normalise (1.0 + alpha * a, -2.0 * cosw, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * cosw, 1.0 - alpha / a);
    }

    static Coefficients makeShelf (double sampleRate, double frequency, double gainDb, bool high)
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.45, frequency) / sampleRate;
        const auto cosw = std::cos (w);
        const auto alpha = std::sin (w) / 2.0 * std::sqrt (2.0); // shelf slope 1
        const auto root = 2.0 * std::sqrt (a) * alpha;

        if (high)
            return normalise (a * ((a + 1.0) + (a - 1.0) * cosw + root), -2.0 * a * ((a - 1.0) + (a + 1.0) * cosw),
                              a * ((a + 1.0) + (a - 1.0) * cosw - root), (a + 1.0) - (a - 1.0) * cosw + root,
                              2.0 * ((a - 1.0) - (a + 1.0) * cosw), (a + 1.0) - (a - 1.0) * cosw - root);

        return normalise (a * ((a + 1.0) - (a - 1.0) * cosw + root), 2.0 * a * ((a - 1.0) - (a + 1.0) * cosw),
                          a * ((a + 1.0) - (a - 1.0) * cosw - root), (a + 1.0) + (a - 1.0) * cosw + root,
                          -2.0 * ((a - 1.0) + (a + 1.0) * cosw), (a + 1.0) + (a - 1.0) * cosw - root);
    }

    void setCoefficients (const Coefficients& c) { coeffs = c; }
    void reset() { z1 = z2 = 0.0; }

    float process (float input)
    {
        // Transposed direct form II.
        const auto x = (double) input;
        const auto y = coeffs.b0 * x + z1;
        z1 = coeffs.b1 * x - coeffs.a1 * y + z2;
        z2 = coeffs.b2 * x - coeffs.a2 * y;
        return (float) y;
    }

    Coefficients coeffs;
    double z1 = 0.0, z2 = 0.0;
};

// The EQ module's three bands, shared by the processor and the display.
struct EqSettings
{
    float lowFreq = 120.0f, lowGain = 0.0f;
    float midFreq = 1000.0f, midGain = 0.0f, midQ = 0.9f;
    float highFreq = 6000.0f, highGain = 0.0f;

    void makeCoefficients (double sampleRate, Biquad::Coefficients (&bands)[3]) const
    {
        bands[0] = Biquad::makeShelf (sampleRate, lowFreq, lowGain, false);
        bands[1] = Biquad::makePeak (sampleRate, midFreq, midQ, midGain);
        bands[2] = Biquad::makeShelf (sampleRate, highFreq, highGain, true);
    }
};
