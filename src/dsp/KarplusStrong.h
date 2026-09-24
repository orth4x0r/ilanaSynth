#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <cmath>
#include <vector>

class KarplusStrong
{
public:
    enum class Excite
    {
        Burst = 0,
        Noise,
        Saw,
        Pulse
    };

    KarplusStrong()
        : random (nextSeed())
    {
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const auto size = juce::nextPowerOfTwo ((int) (sampleRate / 15.0) + 1);
        buffer.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePosition = 0;
        lowpassState = 0.0f;
        phase = 0.0;
    }

    void setFrequency (double hz)
    {
        frequency = juce::jlimit (15.0, sampleRate * 0.45, hz);
    }

    void setParams (Excite newExcite, float newSustainLevel, float newDamping, float newDecay)
    {
        excite = newExcite;
        sustainLevel = juce::jlimit (0.0f, 1.0f, newSustainLevel);
        damping = juce::jlimit (0.0f, 1.0f, newDamping);
        decay = juce::jlimit (0.0f, 1.0f, newDecay);

        lowpassCoefficient = 1.0f - damping * 0.96f;
        feedback = 0.90f + decay * 0.0995f;
    }

    void trigger (float velocity)
    {
        if (buffer.empty())
            return;

        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        for (auto& value : buffer)
            value = (random.nextFloat() * 2.0f - 1.0f) * level;

        writePosition = 0;
        lowpassState = 0.0f;
        phase = 0.0;
    }

    float process()
    {
        if (buffer.empty())
            return 0.0f;

        const auto size = (int) buffer.size();
        auto readPosition = (double) writePosition - sampleRate / frequency;

        while (readPosition < 0.0)
            readPosition += (double) size;

        const auto index = (int) readPosition;
        const auto nextIndex = (index + 1) % size;
        const auto fraction = (float) (readPosition - (double) index);
        const auto delayed = buffer[(size_t) index]
                             + (buffer[(size_t) nextIndex] - buffer[(size_t) index]) * fraction;

        lowpassState += (delayed - lowpassState) * lowpassCoefficient;

        auto excitation = 0.0f;

        switch (excite)
        {
            case Excite::Noise:
                excitation = (random.nextFloat() * 2.0f - 1.0f) * sustainLevel;
                break;

            case Excite::Saw:
                excitation = (float) (2.0 * phase - 1.0) * sustainLevel;
                break;

            case Excite::Pulse:
                excitation = (phase < 0.5 ? 1.0f : -1.0f) * sustainLevel;
                break;

            case Excite::Burst:
            default:
                break;
        }

        phase += frequency / sampleRate;

        if (phase >= 1.0)
            phase -= 1.0;

        buffer[(size_t) writePosition] = lowpassState * feedback + excitation;
        writePosition = (writePosition + 1) % size;

        return delayed;
    }

private:
    static int nextSeed()
    {
        static std::atomic<int> counter { 0 };
        return counter.fetch_add (1) * 7919 + 12345;
    }

    std::vector<float> buffer;
    juce::Random random;
    double sampleRate = 44100.0;
    double frequency = 440.0;
    double phase = 0.0;
    int writePosition = 0;
    float lowpassState = 0.0f;
    float lowpassCoefficient = 0.5f;
    float feedback = 0.99f;
    float sustainLevel = 0.0f;
    float damping = 0.35f;
    float decay = 0.75f;
    Excite excite = Excite::Burst;
};
