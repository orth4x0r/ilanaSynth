#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

class GranularPitchShift
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        const auto size = juce::nextPowerOfTwo ((int) (sampleRate * 0.5) + 1);
        buffer.assign ((size_t) size, 0.0f);
        grainLength = juce::jmax (32, (int) (sampleRate * 0.015));
        writeCount = 0;
        writePosition = 0;
        resetGrains();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeCount = 0;
        resetGrains();
    }

    void setRatio (double newRatio) { ratio = juce::jlimit (0.25, 4.0, newRatio); }

    double getLatencySamples() const
    {
        return (double) grainLength * ratio;
    }

    void push (float sample)
    {
        if (buffer.empty())
            return;

        buffer[(size_t) writePosition] = sample;
        writePosition = (writePosition + 1) % (int) buffer.size();
        ++writeCount;
    }

    float process()
    {
        if (buffer.empty())
            return 0.0f;

        auto output = 0.0f;

        for (auto& grain : grains)
        {
            const auto window = 0.5f - 0.5f * (float) std::cos (
                juce::MathConstants<double>::twoPi * (double) grain.age / (double) grainLength);

            const auto position = grain.start + (juce::int64) ((double) grain.age * ratio);
            output += readAt (position) * window;

            if (++grain.age >= grainLength)
                restartGrain (grain);
        }

        return output;
    }

private:
    struct Grain
    {
        juce::int64 start = 0;
        int age = 0;
    };

    void resetGrains()
    {
        const auto lookback = (juce::int64) ((double) grainLength * ratio);
        grains[0] = { juce::jmax ((juce::int64) 0, (juce::int64) writeCount - lookback), 0 };
        grains[1] = { juce::jmax ((juce::int64) 0, (juce::int64) writeCount - lookback), grainLength / 2 };
    }

    void restartGrain (Grain& grain)
    {
        grain.start = juce::jmax ((juce::int64) 0, (juce::int64) writeCount - (juce::int64) ((double) grainLength * ratio));
        grain.age = 0;
    }

    float readAt (juce::int64 position) const
    {
        if (position < 0 || position >= writeCount || buffer.empty())
            return 0.0f;

        return buffer[(size_t) (position % (juce::int64) buffer.size())];
    }

    std::vector<float> buffer;
    Grain grains[2];
    double sampleRate = 44100.0;
    double ratio = 1.0;
    juce::int64 writeCount = 0;
    int writePosition = 0;
    int grainLength = 720;
};
