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

            output += readInterpolated (grain.start, (double) grain.age * ratio) * window;

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

    // A 4-point Hermite read between samples: a ratio like the Octaver's 0.5
    // used to play each sample twice (a staircase that aliased).
    float readInterpolated (juce::int64 start, double offset) const
    {
        const auto whole = (juce::int64) offset;
        const auto t = (float) (offset - (double) whole);
        const auto index = start + whole;
        const auto y0 = readAt (index - 1);
        const auto y1 = readAt (index);
        const auto y2 = readAt (index + 1);
        const auto y3 = readAt (index + 2);
        const auto c1 = 0.5f * (y2 - y0);
        const auto c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const auto c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
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
