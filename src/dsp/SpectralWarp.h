#pragma once

#include <juce_dsp/juce_dsp.h>

#include <complex>
#include <vector>

#include "Wavetable.h"

// Spectral warps reshape a wavetable's harmonics rather than its waveform.
// They're applied to whole tables (off the audio thread) and the voices read
// the warped copy.
namespace SpectralWarp
{
enum
{
    Off = 0,
    Stretch,     // pushes harmonics apart: metallic, bell-like
    Shift,       // moves every harmonic up: hollow, ring-mod-like
    OddEven,     // odd only, then even only
    Formant,     // moves the spectral envelope, keeps the pitch
    Smear,       // blurs harmonics together and scatters their phases
    HarmonicCut, // keeps only the lowest harmonics
    Count
};

inline juce::StringArray getNames()
{
    return { "Off", "Stretch", "Shift", "Odd/Even", "Formant", "Smear", "Harmonic Cut" };
}

// One frame through the chosen warp. amount runs 0..1 (Formant: 0.5 = none).
inline std::vector<float> warpFrame (const float* source, int mode, float amount, juce::dsp::FFT& fft, juce::Random& random)
{
    constexpr int size = Wavetable::frameSize;
    constexpr int half = size / 2;
    using Complex = std::complex<float>;

    std::vector<Complex> time ((size_t) size), spectrum ((size_t) size), warped ((size_t) size, Complex {});

    auto originalPeak = 0.0f;

    for (int i = 0; i < size; ++i)
    {
        time[(size_t) i] = Complex (source[i], 0.0f);
        originalPeak = juce::jmax (originalPeak, std::abs (source[i]));
    }

    fft.perform (time.data(), spectrum.data(), false);

    const auto a = juce::jlimit (0.0f, 1.0f, amount);

    switch (mode)
    {
        case Stretch:
        {
            const auto exponent = 1.0 + (double) a * 0.6;

            for (int k = 1; k < half; ++k)
            {
                const auto target = (int) std::lround (std::pow ((double) k, exponent));

                if (target < half)
                    warped[(size_t) target] += spectrum[(size_t) k];
            }

            break;
        }

        case Shift:
        {
            const auto offset = (int) std::lround (a * 32.0f);

            for (int k = 1; k + offset < half; ++k)
                warped[(size_t) (k + offset)] = spectrum[(size_t) k];

            break;
        }

        case OddEven:
        {
            const auto b = a * 2.0f;
            const auto oddGain = b <= 1.0f ? 1.0f : 2.0f - b;
            const auto evenGain = b <= 1.0f ? 1.0f - b : b - 1.0f;

            for (int k = 1; k < half; ++k)
                warped[(size_t) k] = spectrum[(size_t) k] * (k % 2 == 1 ? oddGain : evenGain);

            break;
        }

        case Formant:
        {
            // Read each harmonic's magnitude from where the envelope was
            // before the shift; keep its own phase.
            const auto ratio = std::exp2 ((double) (a - 0.5f) * 4.0);

            for (int k = 1; k < half; ++k)
            {
                const auto from = (double) k / ratio;
                const auto index = (int) from;
                const auto frac = (float) (from - (double) index);
                auto magnitude = 0.0f;

                if (index >= 1 && index + 1 < half)
                    magnitude = std::abs (spectrum[(size_t) index]) * (1.0f - frac) + std::abs (spectrum[(size_t) index + 1]) * frac;
                else if (index >= 1 && index < half)
                    magnitude = std::abs (spectrum[(size_t) index]);

                warped[(size_t) k] = std::polar (magnitude, std::arg (spectrum[(size_t) k]));
            }

            break;
        }

        case Smear:
        {
            const auto width = 1 + (int) std::lround (a * 24.0f);

            for (int k = 1; k < half; ++k)
            {
                auto sum = 0.0f;
                auto count = 0;

                for (int j = juce::jmax (1, k - width); j <= juce::jmin (half - 1, k + width); ++j)
                {
                    sum += std::abs (spectrum[(size_t) j]);
                    ++count;
                }

                const auto phase = std::arg (spectrum[(size_t) k]) + a * random.nextFloat() * juce::MathConstants<float>::twoPi;
                warped[(size_t) k] = std::polar (sum / (float) juce::jmax (1, count), phase);
            }

            break;
        }

        case HarmonicCut:
        {
            const auto keep = juce::jmax (1.0, std::pow (512.0, 1.0 - (double) a));

            for (int k = 1; k < half; ++k)
            {
                const auto fade = juce::jlimit (0.0, 1.0, keep + 1.0 - (double) k);
                warped[(size_t) k] = spectrum[(size_t) k] * (float) fade;
            }

            break;
        }

        default:
            for (int k = 1; k < half; ++k)
                warped[(size_t) k] = spectrum[(size_t) k];
            break;
    }

    // Mirror into a real signal and transform back.
    for (int k = 1; k < half; ++k)
        warped[(size_t) (size - k)] = std::conj (warped[(size_t) k]);

    warped[0] = {};
    warped[(size_t) half] = {};
    fft.perform (warped.data(), time.data(), true);

    std::vector<float> out ((size_t) size);
    auto peak = 0.0f;

    for (int i = 0; i < size; ++i)
    {
        out[(size_t) i] = time[(size_t) i].real();
        peak = juce::jmax (peak, std::abs (out[(size_t) i]));
    }

    // Keep the frame's level where it was.
    if (peak > 1.0e-9f)
        for (auto& value : out)
            value *= originalPeak / peak;

    return out;
}

// A warped copy of a whole table.
inline std::unique_ptr<Wavetable> warpTable (const Wavetable& source, int mode, float amount)
{
    juce::dsp::FFT fft (11);
    juce::Random random (1234);
    std::vector<std::vector<float>> frames;

    for (int frame = 0; frame < source.getNumFrames(); ++frame)
        frames.push_back (warpFrame (source.getFrameData (0, frame), mode, amount, fft, random));

    auto table = std::make_unique<Wavetable>();
    table->setName (source.getName());
    table->buildFromFrames (frames);
    return table;
}
} // namespace SpectralWarp
