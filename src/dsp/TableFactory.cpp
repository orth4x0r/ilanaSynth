#include "TableFactory.h"

#include <cmath>
#include <functional>

namespace
{
constexpr int frameSize = 2048;
constexpr int numFrames = 64;
constexpr int additivePartials = 160;

constexpr auto twoPi = juce::MathConstants<double>::twoPi;

float sine (double phase) { return (float) std::sin (twoPi * phase); }

float saw (double phase) { return (float) (2.0 * (phase - std::floor (phase + 0.5))); }

float pulse (double phase, double width)
{
    const auto frac = phase - std::floor (phase);
    return frac < width ? 1.0f : -1.0f;
}

float foldTriangle (float value)
{
    value = std::fmod (value + 1.0f, 4.0f);

    if (value < 0.0f)
        value += 4.0f;

    return value <= 2.0f ? value - 1.0f : 3.0f - value;
}

float additive (double phase, const float* amplitudes, int count)
{
    const auto angle = twoPi * phase;
    const auto cosStep = (float) std::cos (angle);
    const auto sinStep = (float) std::sin (angle);

    auto sinN = sinStep;
    auto cosN = cosStep;
    auto sum = 0.0f;

    for (int n = 1; n <= count; ++n)
    {
        sum += amplitudes[n] * sinN;

        const auto nextSin = sinN * cosStep + cosN * sinStep;
        cosN = cosN * cosStep - sinN * sinStep;
        sinN = nextSin;
    }

    return sum;
}

template <typename ShapeFunction>
std::vector<std::vector<float>> generateTable (ShapeFunction&& shapeForFrame)
{
    std::vector<std::vector<float>> frames ((size_t) numFrames, std::vector<float> ((size_t) frameSize, 0.0f));

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const auto t = (double) frame / (double) (numFrames - 1);
        auto& target = frames[(size_t) frame];

        for (int i = 0; i < frameSize; ++i)
            target[(size_t) i] = shapeForFrame (t, (double) i / (double) frameSize);
    }

    return frames;
}
} // namespace

int TableFactory::getNumFactoryTables()
{
    return 16;
}

juce::StringArray TableFactory::getFactoryTableNames()
{
    return { "Basic", "HardSync", "Wavefold", "FM Metal",
             "Formant", "Comb", "PWM", "DriveSaw",
             "Sine", "Triangle", "Analog",
             "Vowel", "Glass", "Fractal", "Riser", "Digital" };
}

std::vector<std::vector<float>> TableFactory::generate (int tableIndex)
{
    switch (tableIndex)
    {
        case 0:
            return generateTable ([] (double t, double phase)
            {
                const auto fade = (float) std::pow (1.0 - t, 2.0);
                auto sum = 0.0f;

                for (int n = 1; n <= 96; ++n)
                {
                    const auto gain = (n % 2 == 0 ? fade : 1.0f) / (float) n;
                    sum += gain * sine (phase * n);
                }

                return sum;
            });

        case 1:
            return generateTable ([] (double t, double phase)
            {
                return saw (phase * (1.0 + t * 7.0));
            });

        case 2:
            return generateTable ([] (double t, double phase)
            {
                return foldTriangle (sine (phase) * (float) (1.0 + t * 7.0));
            });

        case 3:
            return generateTable ([] (double t, double phase)
            {
                const auto index = t * 8.0;
                return (float) std::sin (twoPi * phase + index * std::sin (twoPi * 3.0 * phase));
            });

        case 4:
        {
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto center1 = 3.0 + t * 40.0;
                const auto center2 = 6.0 + t * 90.0;
                const auto width1 = 0.35 * center1 + 1.0;
                const auto width2 = 0.30 * center2 + 1.0;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto distance1 = ((double) n - center1) / width1;
                    const auto distance2 = ((double) n - center2) / width2;
                    const auto body = 0.15 / (double) n;

                    amplitudes[(size_t) n] = (float) (body
                                                      + std::exp (-0.5 * distance1 * distance1)
                                                      + 0.7 * std::exp (-0.5 * distance2 * distance2)) / (float) n;
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 5:
        {
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto combFrequency = 0.08 + t * 2.5;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto comb = 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * (double) n * combFrequency);
                    amplitudes[(size_t) n] = (float) comb / (float) n;
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 6:
            return generateTable ([] (double t, double phase)
            {
                return pulse (phase, 0.5 - t * 0.44);
            });

        case 7:
            return generateTable ([] (double t, double phase)
            {
                return (float) std::tanh (saw (phase) * (1.0 + t * 14.0));
            });

        case 8:
            return generateTable ([] (double, double phase)
            {
                return sine (phase);
            });

        case 9:
            return generateTable ([] (double t, double phase)
            {
                auto sum = 0.0f;
                const auto rolloff = 1.0f + (float) t * 2.0f;

                for (int n = 1; n <= 96; n += 2)
                {
                    const auto sign = ((n - 1) / 2) % 2 == 0 ? 1.0f : -1.0f;
                    const auto gain = sign / (float) (n * n);
                    sum += gain * sine (phase * n) * std::exp (-(float) n / (60.0f / rolloff));
                }

                return sum * 2.0f;
            });

        case 10:
            return generateTable ([] (double t, double phase)
            {
                auto sum = 0.0f;
                const auto rolloff = (float) (40.0 + t * 80.0);

                for (int n = 1; n <= 128; ++n)
                {
                    const auto gain = (1.0f / (float) n) * std::exp (-juce::jmax (0.0f, (float) n - rolloff) / 25.0f);
                    sum += gain * sine (phase * n);
                }

                return sum;
            });

        case 11:
        {
            // Vowel: morphs A -> E -> I -> O -> U through three formants.
            const struct Formant { double f1, f2, f3; };
            static const Formant vowels[] { { 730.0, 1090.0, 2440.0 }, { 530.0, 1840.0, 2480.0 },
                                            { 270.0, 2290.0, 3010.0 }, { 570.0, 840.0, 2410.0 },
                                            { 300.0, 870.0, 2240.0 } };
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto scaled = juce::jlimit (0.0, 4.0, t * 4.0);
                const auto vowelIndex = juce::jlimit (0, 3, (int) scaled);
                const auto frac = scaled - (double) vowelIndex;
                const auto& a = vowels[vowelIndex];
                const auto& b = vowels[juce::jmin (4, vowelIndex + 1)];

                const auto f1 = a.f1 + (b.f1 - a.f1) * frac;
                const auto f2 = a.f2 + (b.f2 - a.f2) * frac;
                const auto f3 = a.f3 + (b.f3 - a.f3) * frac;
                constexpr auto f0 = 110.0;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto frequency = f0 * (double) n;
                    const auto weight = [&] (double formant, double width, double gain)
                    {
                        const auto d = (frequency - formant) / width;
                        return gain * std::exp (-0.5 * d * d);
                    };

                    amplitudes[(size_t) n] = (float) ((weight (f1, 90.0, 1.0)
                                                       + weight (f2, 110.0, 0.55)
                                                       + weight (f3, 140.0, 0.3)) + 0.06 / std::sqrt ((double) n));
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 12:
            return generateTable ([] (double t, double phase)
            {
                // Glass: inharmonic two-operator FM, bell-like partials.
                const auto index = 0.4 + t * 7.0;

                return (float) (0.72 * std::sin (twoPi * phase + index * std::sin (twoPi * 3.5 * phase))
                                + 0.28 * std::sin (twoPi * 2.0 * phase + index * 0.5 * std::sin (twoPi * 7.03 * phase)));
            });

        case 13:
        {
            // Fractal: self-similar harmonic stack whose spectrum gets rougher.
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto power = 0.75 + t * 0.85;
                const auto roughness = t * 2.6;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto modulation = 0.55 + 0.45 * std::sin (std::pow ((double) n, 0.6) * 5.0
                                                                    + roughness * std::log ((double) n + 1.0));
                    amplitudes[(size_t) n] = (float) (std::pow ((double) n, -power) * modulation);
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 14:
        {
            // Riser: a resonant peak sweeping upward with growing high end.
            std::vector<float> amplitudes ((size_t) additivePartials + 1, 0.0f);

            return generateTable ([&amplitudes] (double t, double phase)
            {
                const auto peak = 4.0 + t * t * 140.0;
                const auto width = 6.0 + t * 34.0;

                for (int n = 1; n <= additivePartials; ++n)
                {
                    const auto d = ((double) n - peak) / width;
                    const auto bump = std::exp (-0.5 * d * d);
                    amplitudes[(size_t) n] = (float) (bump * 1.15 + (0.05 + t * 0.55) / std::sqrt ((double) n));
                }

                amplitudes[0] = 0.0f;
                return additive (phase, amplitudes.data(), additivePartials);
            });
        }

        case 15:
            return generateTable ([] (double t, double phase)
            {
                // Digital: phase distortion (Casio CZ style) sweeping saw -> sine.
                const auto bend = juce::jmax (0.02, 0.5 - t * 0.47);
                auto p = phase - std::floor (phase);

                const auto warped = p < bend ? 0.5 * p / bend
                                             : 0.5 + 0.5 * (p - bend) / (1.0 - bend);

                return sine (warped);
            });
    }

    return generateTable ([] (double, double phase) { return sine (phase); });
}
