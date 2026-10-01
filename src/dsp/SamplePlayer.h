#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <memory>
#include <vector>

struct SampleData;

// One region of a multisample (a SoundFont or SFZ instrument): the audio a
// range of keys and velocities plays, the key it was recorded at and its
// loop.
struct SampleZone
{
    std::shared_ptr<SampleData> data;
    int loKey = 0, hiKey = 127, loVel = 1, hiVel = 127;
    double rootNote = 60.0; // fractional: tuning folded in
    float gain = 1.0f;
    bool loop = false;
    int loopStart = 0, loopEnd = 0; // samples into data

    bool covers (int note, int velocity) const noexcept
    {
        return note >= loKey && note <= hiKey && velocity >= loVel && velocity <= hiVel;
    }

    double rootHz() const noexcept { return 440.0 * std::exp2 ((rootNote - 69.0) / 12.0); }
};

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    juce::String name;
    // A multisample's regions. Empty for a plain sample; otherwise buffer
    // holds the region under middle C, for the displays.
    std::vector<SampleZone> zones;

    int getNumChannels() const noexcept { return buffer.getNumChannels(); }
    int getNumSamples() const noexcept { return buffer.getNumSamples(); }

    // The region a note plays: the one that covers it, else the nearest by
    // key (a velocity outside every range takes the closest layer).
    const SampleZone* zoneFor (int note, int velocity) const noexcept
    {
        const SampleZone* best = nullptr;
        auto bestDistance = 1 << 30;
        for (const auto& zone : zones)
        {
            if (zone.data == nullptr)
                continue;
            if (zone.covers (note, velocity))
                return &zone;
            const auto keyDistance = note < zone.loKey ? zone.loKey - note : (note > zone.hiKey ? note - zone.hiKey : 0);
            const auto velDistance = velocity < zone.loVel ? zone.loVel - velocity : (velocity > zone.hiVel ? velocity - zone.hiVel : 0);
            const auto distance = keyDistance * 128 + velDistance;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &zone;
            }
        }
        return best;
    }
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
        float gain = 1.0f;
        // A multisample region's own loop (samples); used when loopEnd > loopStart.
        int loopStart = 0, loopEnd = 0;
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

        // A region's sustain loop: forward only, inside the played range.
        if (params.loopEnd > params.loopStart && ! params.reverse && position >= (double) params.loopEnd
            && params.loopEnd <= regionEnd && params.loopStart >= regionStart)
            position -= (double) (params.loopEnd - params.loopStart);

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

        gain *= (double) params.gain;
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
