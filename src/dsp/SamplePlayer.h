#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;

    int getNumChannels() const noexcept { return buffer.getNumChannels(); }
    int getNumSamples() const noexcept { return buffer.getNumSamples(); }
};

class SamplePlayer
{
public:
    struct Params
    {
        const SampleData* sample = nullptr;
        bool loop = false;
        bool reverse = false;
        float start = 0.0f;
        float end = 1.0f;
        float fadeIn = 0.0f;
        float fadeOut = 0.0f;
    };

    void prepare (double)
    {
    }

    void reset()
    {
        active = false;
        position = regionStart;
    }

    void trigger()
    {
        if (params.sample == nullptr || params.sample->getNumSamples() <= 1)
        {
            active = false;
            return;
        }

        position = params.reverse ? regionEnd - 1.0 : regionStart;
        active = true;
    }

    void setParams (const Params& newParams)
    {
        const auto newSample = newParams.sample != params.sample;
        params = newParams;

        if (params.sample == nullptr || params.sample->getNumSamples() <= 1)
        {
            active = false;
            return;
        }

        const auto length = (double) params.sample->getNumSamples();
        regionStart = juce::jlimit (0.0, length - 1.0, (double) juce::jlimit (0.0f, 0.999f, params.start) * length);
        regionEnd = juce::jlimit (regionStart + 1.0, length, (double) juce::jlimit (0.0f, 1.0f, params.end) * length);
        regionLength = regionEnd - regionStart;

        if (newSample)
            trigger();
    }

    void setPlaybackRatio (double newRatio)
    {
        ratio = juce::jlimit (0.0001, 64.0, std::abs (newRatio));
    }

    bool isActive() const noexcept { return active; }

    double getSourcePosition() const noexcept { return position; }

    void process (float& left, float& right)
    {
        left = 0.0f;
        right = 0.0f;

        if (! active || params.sample == nullptr)
            return;

        if (position >= regionEnd || position < regionStart)
        {
            if (! params.loop)
            {
                active = false;
                return;
            }

            if (params.reverse)
            {
                while (position < regionStart)
                    position += regionLength;
            }
            else
            {
                while (position >= regionEnd)
                    position -= regionLength;
            }
        }

        const auto* sample = params.sample;
        const auto numSamples = sample->getNumSamples();
        const auto index = juce::jlimit (0, numSamples - 1, (int) std::floor (position));
        const auto next = juce::jmin (index + 1, numSamples - 1);
        const auto frac = (float) (position - (double) index);

        const auto readChannel = [&] (int channel)
        {
            const auto* data = sample->buffer.getReadPointer (juce::jmin (channel, sample->getNumChannels() - 1));
            return data[index] + (data[next] - data[index]) * frac;
        };

        const auto readL = readChannel (0);
        const auto readR = sample->getNumChannels() > 1 ? readChannel (1) : readL;

        const auto posSinceStart = params.reverse ? (regionEnd - position) : (position - regionStart);
        const auto posToEnd = params.reverse ? (position - regionStart) : (regionEnd - position);

        auto gain = 1.0;

        const auto fadeInLength = (double) params.fadeIn * regionLength;

        if (fadeInLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posSinceStart / fadeInLength);

        const auto fadeOutLength = (double) params.fadeOut * regionLength;

        if (fadeOutLength > 1.0)
            gain *= juce::jlimit (0.0, 1.0, posToEnd / fadeOutLength);

        left = readL * (float) gain;
        right = readR * (float) gain;

        position += params.reverse ? -ratio : ratio;
    }

private:
    Params params;
    double position = 0.0;
    double regionStart = 0.0;
    double regionEnd = 0.0;
    double regionLength = 0.0;
    double ratio = 1.0;
    bool active = false;
};
