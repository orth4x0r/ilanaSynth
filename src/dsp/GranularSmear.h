#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

class GranularSmear
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const auto size = juce::nextPowerOfTwo ((int) (sampleRate * 1.0) + 1);
        buffer.assign ((size_t) size, 0.0f);
        writePosition = 0;
        writeCount = 0;
        spawnCounter = 0.0f;

        for (auto& grain : grains)
            grain.active = false;

        random = juce::Random (0x51ee7);
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePosition = 0;
        writeCount = 0;

        for (auto& grain : grains)
            grain.active = false;
    }

    void setParams (float grainMillis, float grainsPerSecond)
    {
        length = juce::jlimit (32, juce::jmax (32, (int) (sampleRate * 0.4)),
                               (int) (sampleRate * juce::jlimit (0.005f, 0.4f, grainMillis * 0.001f)));
        density = juce::jlimit (0.5f, 80.0f, grainsPerSecond);
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

        spawnCounter += density / (float) sampleRate;

        if (spawnCounter >= 1.0f)
        {
            spawnCounter -= 1.0f;
            spawnGrain();
        }

        auto sum = 0.0f;
        auto activeCount = 0;

        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            const auto window = 0.5f - 0.5f * (float) std::cos (
                juce::MathConstants<double>::twoPi * (double) grain.age / (double) grain.length);

            sum += readAt (grain.start + grain.age) * window;
            ++activeCount;

            if (++grain.age >= grain.length)
                grain.active = false;
        }

        if (activeCount == 0)
            return 0.0f;

        return sum / std::sqrt ((float) activeCount);
    }

private:
    struct Grain
    {
        juce::int64 start = 0;
        int age = 0;
        int length = 0;
        bool active = false;
    };

    void spawnGrain()
    {
        for (auto& grain : grains)
        {
            if (grain.active)
                continue;

            const auto history = (juce::int64) (random.nextFloat() * (float) sampleRate * 0.25f);
            grain.start = juce::jmax ((juce::int64) 0, writeCount - (juce::int64) length - history);
            grain.age = 0;
            grain.length = length;
            grain.active = true;
            return;
        }
    }

    float readAt (juce::int64 position) const
    {
        if (position < 0 || position >= writeCount || buffer.empty())
            return 0.0f;

        return buffer[(size_t) (position % (juce::int64) buffer.size())];
    }

    std::vector<float> buffer;
    Grain grains[8];
    juce::Random random;
    double sampleRate = 44100.0;
    juce::int64 writeCount = 0;
    int writePosition = 0;
    int length = 2400;
    float density = 12.0f;
    float spawnCounter = 0.0f;
};
