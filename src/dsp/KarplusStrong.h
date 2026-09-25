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
        Pulse,
        Bow
    };

    KarplusStrong()
        : random (nextSeed())
    {
    }

    explicit KarplusStrong (int seed)
        : random (seed)
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
        dispersionState[0] = dispersionState[1] = 0.0f;
        dispersionInput[0] = dispersionInput[1] = 0.0f;
        slapRemaining = 0;
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

    void setPhysicalParams (float newStiffness, float newPickup, float newExcitationPosition,
                            float newPickHardness, float newPickPosition, bool newSlap)
    {
        stiffness = juce::jlimit (0.0f, 1.0f, newStiffness);
        pickupPosition = juce::jlimit (0.0f, 1.0f, newPickup);
        excitationPosition = juce::jlimit (0.0f, 1.0f, newExcitationPosition);
        pickHardness = juce::jlimit (0.0f, 1.0f, newPickHardness);
        pickPosition = juce::jlimit (0.0f, 1.0f, newPickPosition);
        slap = newSlap;
        dispersionCoefficient = -0.7f * stiffness;
        dispersionDelay = 2.0f * (1.0f - dispersionCoefficient) / (1.0f + dispersionCoefficient);
    }

    void setBowAndBuzz (float pressure, float speed, float bridgeBuzz, float fretRattle)
    {
        bowPressure = juce::jlimit (0.0f, 1.0f, pressure);
        bowSpeed = juce::jlimit (0.0f, 1.0f, speed);
        buzz = juce::jlimit (0.0f, 1.0f, bridgeBuzz);
        rattle = juce::jlimit (0.0f, 1.0f, fretRattle);
    }

    void trigger (float velocity)
    {
        if (buffer.empty())
            return;

        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        for (auto& value : buffer)
            value = (random.nextFloat() * 2.0f - 1.0f) * level * (excite == Excite::Bow ? 0.02f : 1.0f);

        // Hardness 1 (the default) keeps the raw burst; lower values soften it.
        if (pickHardness < 1.0f || pickPosition > 0.0f || excitationPosition > 0.0f)
        {
            const auto period = juce::jlimit (2, (int) buffer.size() - 1, (int) (sampleRate / frequency));
            const auto offset = juce::jlimit (1, period - 1, (int) (period * juce::jmax (0.01f, pickPosition)));
            const auto exciteOffset = juce::jlimit (1, period - 1, (int) (period * excitationPosition));
            auto smooth = 0.0f;

            for (int i = 0; i < (int) buffer.size(); ++i)
            {
                auto value = buffer[(size_t) i];
                smooth += (value - smooth) * (0.15f + 0.85f * pickHardness);

                if (pickPosition > 0.0f)
                    value -= 0.75f * buffer[(size_t) ((i + offset) % (int) buffer.size())];

                if (excitationPosition > 0.0f)
                    value -= 0.6f * buffer[(size_t) ((i + exciteOffset) % (int) buffer.size())];

                buffer[(size_t) i] = juce::jmap (pickHardness, smooth, value);
            }
        }

        writePosition = 0;
        lowpassState = 0.0f;
        phase = 0.0;
        dispersionState[0] = dispersionState[1] = 0.0f;
        dispersionInput[0] = dispersionInput[1] = 0.0f;
        slapRemaining = slap ? (int) (sampleRate * 0.004) : 0;
        slapLevel = level;
        strikeVelocity = level;
    }

    float process (float expression = 0.0f, bool noteHeld = true)
    {
        if (buffer.empty())
            return 0.0f;

        const auto size = (int) buffer.size();
        const auto period = sampleRate / frequency;
        const auto useDispersion = stiffness > 0.0f && period > 3.5;
        const auto delay = useDispersion ? juce::jmin ((double) dispersionDelay, period - 1.25) : 0.0;
        const auto coefficient = delay < (double) dispersionDelay
                                     ? (float) ((2.0 - delay) / (2.0 + delay)) : dispersionCoefficient;
        auto readPosition = (double) writePosition - period
                            + delay;

        while (readPosition < 0.0)
            readPosition += (double) size;
        while (readPosition >= (double) size)
            readPosition -= (double) size;

        const auto index = (int) readPosition;
        const auto nextIndex = (index + 1) % size;
        const auto fraction = (float) (readPosition - (double) index);
        const auto rawDelayed = buffer[(size_t) index]
                             + (buffer[(size_t) nextIndex] - buffer[(size_t) index]) * fraction;

        auto delayed = rawDelayed;

        if (useDispersion)
        {
            for (int stage = 0; stage < 2; ++stage)
            {
                const auto next = coefficient * delayed + dispersionInput[stage]
                                  - coefficient * dispersionState[stage];
                dispersionInput[stage] = delayed;
                dispersionState[stage] = next;
                delayed = next;
            }
        }

        auto output = delayed;

        if (rattle > 0.0f && std::abs (delayed) > 0.18f)
            output += (random.nextFloat() * 2.0f - 1.0f) * rattle * strikeVelocity
                      * std::abs (delayed) * 0.12f;

        if (pickupPosition > 0.0f)
        {
            auto tap = readPosition - period * (double) pickupPosition;
            while (tap < 0.0)
                tap += (double) size;
            const auto tapIndex = (int) tap;
            const auto tapNext = (tapIndex + 1) % size;
            const auto tapFraction = (float) (tap - (double) tapIndex);
            output -= 0.75f * (buffer[(size_t) tapIndex]
                              + (buffer[(size_t) tapNext] - buffer[(size_t) tapIndex]) * tapFraction);
        }

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

            case Excite::Bow:
                if (noteHeld)
                {
                    const auto pressure = juce::jlimit (0.0f, 1.0f, bowPressure + expression * 0.75f);
                    const auto relativeSpeed = (0.05f + bowSpeed * 0.95f) - delayed;
                    const auto friction = std::tanh (relativeSpeed * (4.0f + pressure * 20.0f));
                    const auto bias = std::tanh ((0.05f + bowSpeed * 0.95f) * (4.0f + pressure * 20.0f));
                    const auto scrape = (random.nextFloat() * 2.0f - 1.0f) * 0.015f * bowSpeed;
                    excitation = (friction - bias + scrape) * pressure * strikeVelocity * 0.3f;
                }
                break;

            case Excite::Burst:
            default:
                break;
        }

        phase += frequency / sampleRate;

        if (phase >= 1.0)
            phase -= 1.0;

        auto loopValue = lowpassState * feedback;

        // Bridge buzz (jawari): the string grazes the bridge on loud swings,
        // flattening the peaks on every pass. The curve only ever reduces
        // magnitude, so it adds bright harmonics that fade with the note but
        // can never feed energy into the loop.
        if (buzz > 0.0f)
        {
            constexpr auto contact = 0.2f;
            loopValue -= buzz * 0.6f * (loopValue - contact * std::tanh (loopValue / contact));
        }

        buffer[(size_t) writePosition] = loopValue + excitation;
        writePosition = (writePosition + 1) % size;

        if (slapRemaining > 0)
        {
            const auto envelope = (float) slapRemaining / (float) juce::jmax (1, (int) (sampleRate * 0.004));
            output += (random.nextFloat() * 2.0f - 1.0f) * envelope * slapLevel;
            --slapRemaining;
        }

        if (stiffness == 0.0f && pickupPosition == 0.0f && slapRemaining == 0
            && buzz == 0.0f && rattle == 0.0f && excite != Excite::Bow)
            return output;

        return std::isfinite (output) ? juce::jlimit (-8.0f, 8.0f, output) : 0.0f;
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
    float stiffness = 0.0f;
    float pickupPosition = 0.0f;
    float excitationPosition = 0.0f;
    float pickHardness = 1.0f;
    float pickPosition = 0.0f;
    float dispersionCoefficient = 0.0f;
    float dispersionDelay = 1.0f;
    float dispersionState[2] {};
    float dispersionInput[2] {};
    bool slap = false;
    int slapRemaining = 0;
    float slapLevel = 0.0f;
    float bowPressure = 0.0f, bowSpeed = 0.5f, buzz = 0.0f, rattle = 0.0f, strikeVelocity = 0.0f;
    Excite excite = Excite::Burst;
};
