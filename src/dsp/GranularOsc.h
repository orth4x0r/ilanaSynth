#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

#include "SamplePlayer.h"

// A granular oscillator: a cloud of short, windowed grains read from a
// sample around a position. Size, density, position spray, pitch spray and
// stereo spread shape the cloud; the note sets the grains' playback rate.
class GranularOsc
{
public:
    struct Params
    {
        const SampleData* sample = nullptr;
        float position = 0.0f;  // 0..1 through the sample
        float sizeMs = 80.0f;   // grain length
        float density = 0.5f;   // 0..1: from sparse single grains to a dense cloud
        float spray = 0.1f;     // 0..1: random offset around the position
        float pitchSpray = 0.0f; // 0..1: up to +-12 semitones of random detune per grain
        float spread = 0.5f;    // 0..1: stereo scatter
        bool reverse = false;   // grains play backwards
        // M7.5 live grains: the sample is the input's history, a ring
        // written up to liveWrite; position is how far back to read.
        bool live = false;
        int liveWrite = 0;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
    }

    void setParams (const Params& newParams) { params = newParams; }

    void setPlaybackRatio (double newRatio) { ratio = juce::jlimit (0.0001, 16.0, newRatio); }

    void reset (juce::uint32 seed)
    {
        for (auto& grain : grains)
            grain.active = false;

        random.setSeed ((juce::int64) seed);
        untilNextGrain = 0.0;    }

    void process (float& left, float& right)
    {
        left = right = 0.0f;
        const auto* sample = params.sample;

        if (sample == nullptr || sample->getNumSamples() < 64)
            return;

        const auto grainLength = juce::jmax (64.0, (double) params.sizeMs * 0.001 * sampleRate);

        // Density sets how many grains overlap: 0.5 to 12.
        const auto overlap = 0.5 * std::pow (24.0, (double) juce::jlimit (0.0f, 1.0f, params.density));

        if (untilNextGrain <= 0.0)
        {
            startGrain (*sample, grainLength);
            // A little jitter keeps the cloud from buzzing at the grain rate.
            untilNextGrain += grainLength / overlap * (0.75 + 0.5 * (double) random.nextFloat());
        }

        untilNextGrain -= 1.0;

        const auto numSamples = sample->getNumSamples();
        const auto* dataL = sample->buffer.getReadPointer (0);
        const auto* dataR = sample->buffer.getReadPointer (juce::jmin (1, sample->getNumChannels() - 1));
        auto sumL = 0.0f, sumR = 0.0f;

        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            const auto windowIndex = juce::jmin (windowSize - 1, (int) (grain.age * grain.windowStep));
            auto position = std::fmod (grain.position, (double) numSamples);

            if (position < 0.0)
                position += (double) numSamples;

            const auto index = (int) position;
            const auto next = index + 1 < numSamples ? index + 1 : 0;
            const auto frac = (float) (position - (double) index);
            const auto l = dataL[index] + (dataL[next] - dataL[index]) * frac;
            const auto r = dataR[index] + (dataR[next] - dataR[index]) * frac;
            const auto gain = window()[(size_t) windowIndex];

            sumL += l * gain * grain.gainL;
            sumR += r * gain * grain.gainR;

            grain.position += grain.increment;
            grain.age += 1.0;

            if (grain.age >= grain.length)
                grain.active = false;
        }

        // Hann-squared grains sum to about 3/8 of the overlap.
        const auto normalise = (float) (1.0 / std::sqrt (juce::jmax (1.0, overlap * 0.375)));
        left = sumL * normalise;
        right = sumR * normalise;
    }

private:
    struct Grain
    {
        bool active = false;
        double position = 0.0, increment = 1.0, age = 0.0, length = 1.0, windowStep = 0.0;
        float gainL = 1.0f, gainR = 1.0f;
    };

    void startGrain (const SampleData& sample, double grainLength)
    {
        Grain* free = nullptr;

        for (auto& grain : grains)
        {
            if (! grain.active)
            {
                free = &grain;
                break;
            }
        }

        if (free == nullptr)
            return;

        const auto numSamples = (double) sample.getNumSamples();
        const auto offset = (random.nextFloat() * 2.0f - 1.0f) * params.spray * 0.5f;
        const auto centre = juce::jlimit (0.0, 1.0, (double) (params.position + offset));
        const auto detune = (random.nextFloat() * 2.0f - 1.0f) * params.pitchSpray * 12.0f;
        const auto pan = (random.nextFloat() * 2.0f - 1.0f) * params.spread;
        const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

        free->active = true;
        free->age = 0.0;
        free->length = grainLength;
        free->windowStep = (double) windowSize / grainLength;
        free->increment = ratio * (sample.sampleRate / sampleRate) * std::exp2 ((double) detune / 12.0)
                          * (params.reverse ? -1.0 : 1.0);
        free->position = centre * (numSamples - 1.0);
        if (params.live)
        {
            // Start far enough behind the write head that the grain never
            // catches it up (a block of audio lands per render).
            const auto travel = grainLength * juce::jmax (0.0, free->increment) + 4096.0;
            const auto reach = juce::jmax (0.0, numSamples - travel - grainLength * 2.0);
            free->position = (double) params.liveWrite - travel - centre * reach;
        }
        free->gainL = std::cos (angle) * juce::MathConstants<float>::sqrt2;
        free->gainR = std::sin (angle) * juce::MathConstants<float>::sqrt2;
    }

    // Hann window (sin squared) as a table: one lookup per grain per sample.
    static constexpr int windowSize = 2048;

    static const std::array<float, windowSize>& window()
    {
        static const auto table = []
        {
            std::array<float, windowSize> values {};

            for (int i = 0; i < windowSize; ++i)
            {
                const auto s = std::sin (juce::MathConstants<double>::pi * (i + 0.5) / windowSize);
                values[(size_t) i] = (float) (s * s);
            }

            return values;
        }();

        return table;
    }

    Params params;
    std::array<Grain, 32> grains;
    // A fixed seed until the first reset (a default juce::Random seeds itself
    // from the clock, so live grains, which can start before a reset, varied
    // from run to run).
    juce::Random random { 0x6a41 };
    double sampleRate = 44100.0, ratio = 1.0, untilNextGrain = 0.0;
};
