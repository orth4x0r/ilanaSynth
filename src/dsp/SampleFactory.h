#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <memory>

#include "SamplePlayer.h"

namespace SampleFactory
{
inline constexpr double factorySampleRate = 48000.0;

inline int getNumFactorySamples() { return 5; }

inline juce::String getFactorySampleName (int index)
{
    switch (index)
    {
        case 0: return "Metal Hit";
        case 1: return "Vocal Ah";
        case 2: return "Sub Tone";
        case 3: return "Vinyl Loop";
        case 4: return "Noise Rise";
        default: return "Sample " + juce::String (index + 1);
    }
}

inline constexpr double rootHz = 261.6255653005986;

inline std::shared_ptr<SampleData> generate (int index)
{
    auto data = std::make_shared<SampleData>();
    data->sampleRate = factorySampleRate;
    data->name = getFactorySampleName (index);

    const auto rate = factorySampleRate;
    const auto length = (int) (rate * (index == 3 ? 2.0 : (index == 1 ? 1.6 : 1.0)));
    data->buffer.setSize (1, length);

    auto* out = data->buffer.getWritePointer (0);
    std::fill (out, out + length, 0.0f);

    switch (index)
    {
        case 0: // Metal Hit: harmonic partials with per-partial decay rates
        {
            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                auto value = 0.0;

                for (int partial = 1; partial <= 9; ++partial)
                {
                    const auto decay = std::exp (-t * (2.5 + partial * 1.35));
                    const auto phase = juce::MathConstants<double>::twoPi * rootHz * (double) partial * t;
                    value += decay * std::sin (phase) / (double) partial;
                }

                out[i] = (float) (value * 0.55);
            }

            break;
        }

        case 1: // Vocal Ah: harmonic stack shaped by formants
        {
            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                const auto vibrato = 1.0 + 0.004 * std::sin (juce::MathConstants<double>::twoPi * 5.2 * t);
                auto value = 0.0;

                for (int harmonic = 1; harmonic <= 24; ++harmonic)
                {
                    const auto frequency = rootHz * (double) harmonic * vibrato;
                    const auto formant1 = std::exp (-std::pow ((frequency - 730.0) / 220.0, 2.0));
                    const auto formant2 = std::exp (-std::pow ((frequency - 1090.0) / 280.0, 2.0));
                    const auto weight = (0.35 + 0.8 * formant1 + 0.5 * formant2) / (double) harmonic;

                    value += weight * std::sin (juce::MathConstants<double>::twoPi * frequency * t);
                }

                const auto envelope = juce::jmin (1.0, t * 12.0) * juce::jmin (1.0, (length / rate - t) * 6.0);
                out[i] = (float) (value * 0.35 * envelope);
            }

            break;
        }

        case 2: // Sub Tone: clean f0 sine with a transient click and soft saturation
        {
            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                const auto decay = std::exp (-t * 1.6);
                const auto click = t < 0.004 ? (1.0 - t / 0.004) * 0.8 : 0.0;
                const auto value = std::sin (juce::MathConstants<double>::twoPi * rootHz * t) * decay + click;

                out[i] = (float) std::tanh (value * 1.4) * 0.7f;
            }

            break;
        }

        case 3: // Vinyl Loop: hum + filtered crackle, loop-friendly
        {
            juce::Random random (0x51ab);
            auto lowpass = 0.0f;

            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                const auto hum = 0.22 * std::sin (juce::MathConstants<double>::twoPi * rootHz * t)
                                 + 0.1 * std::sin (juce::MathConstants<double>::twoPi * rootHz * 2.0 * t);

                auto noise = random.nextFloat() * 2.0f - 1.0f;
                lowpass += (noise - lowpass) * 0.18f;
                auto crackle = random.nextFloat() < 0.00035f ? (random.nextFloat() * 2.0f - 1.0f) * 0.9f : 0.0f;

                out[i] = (float) hum * 0.5f + lowpass * 0.12f + crackle;
            }

            break;
        }

        case 4: // Noise Rise: bandpass-swept noise with a rising envelope
        {
            juce::Random random (0x7321);
            auto lowState = 0.0f;
            auto highState = 0.0f;

            for (int i = 0; i < length; ++i)
            {
                const auto t = (double) i / rate;
                const auto sweep = (float) juce::jlimit (0.0, 1.0, t / (length / rate));
                const auto noise = random.nextFloat() * 2.0f - 1.0f;

                lowState += (noise - lowState) * (0.02f + 0.4f * sweep);
                highState += (lowState - highState) * 0.12f;

                const auto envelope = std::pow ((double) sweep, 1.6);
                out[i] = (float) ((lowState - highState) * 3.0 * envelope * 0.6);
            }

            break;
        }

        default:
            break;
    }

    // Normalise to a consistent level.
    auto peak = 0.0f;

    for (int i = 0; i < length; ++i)
        peak = juce::jmax (peak, std::abs (out[i]));

    if (peak > 0.001f)
    {
        const auto scale = 0.85f / peak;

        for (int i = 0; i < length; ++i)
            out[i] *= scale;
    }

    return data;
}
} // namespace SampleFactory
