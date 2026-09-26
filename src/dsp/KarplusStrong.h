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
        Bow,
        Hammer,  // M4: a felt hammer strikes, harder with velocity
        External // M4: driven by the other oscillators (the FM matrix inputs)
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
        hammerElapsed = hammerTotal = 0;
        bridgeInput = 0.0f;
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

    // M4 keys: hammer hardness (felt, 0..1) and how strongly the damper stops
    // the string once the note is released (0 = rings on as before).
    void setKeysParams (float newHammerHardness, float newDamper)
    {
        hammerHardness = juce::jlimit (0.0f, 1.0f, newHammerHardness);
        damper = juce::jlimit (0.0f, 1.0f, newDamper);
    }

    // Force arriving from the bridge (the other strings of the same note);
    // it is added on the next sample.
    void addBridgeInput (float value) { bridgeInput += value; }

    void trigger (float velocity)
    {
        if (buffer.empty())
            return;

        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        // A bowed string starts still; the bow sets it moving (processBowed).
        if (excite == Excite::Bow)
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            bowNeckWrite = bowBridgeWrite = 0;
            lowpassState = 0.0f;
            strikeVelocity = level;
            bowRamp = 0.0f;
            bowRampCoefficient = (float) (1.0 - std::exp (-1.0 / (0.03 * sampleRate)));
            bridgeInput = 0.0f;
            return;
        }

        // A hammer puts no energy in the string up front: it pushes during a
        // short contact instead (see process). Harder hammers and faster keys
        // mean a shorter contact, so a brighter tone.
        if (excite == Excite::Hammer || excite == Excite::External)
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writePosition = 0;
            lowpassState = 0.0f;
            phase = 0.0;
            dispersionState[0] = dispersionState[1] = 0.0f;
            dispersionInput[0] = dispersionInput[1] = 0.0f;
            slapRemaining = slap ? (int) (sampleRate * 0.004) : 0;
            slapLevel = level;
            strikeVelocity = level;
            bridgeInput = 0.0f;

            if (excite == Excite::Hammer)
            {
                const auto hardness = juce::jlimit (0.0f, 1.0f, hammerHardness * 0.75f + level * 0.5f - 0.1f);
                // Treble hammers are small and light: shorter contact up high.
                const auto registerScale = juce::jlimit (0.35, 2.0, std::pow (220.0 / frequency, 0.35));
                const auto contactMs = (0.5 + 3.5 * (1.0 - (double) hardness)) * registerScale;
                hammerLength = juce::jmax (4, (int) (sampleRate * contactMs * 0.001));
                // Softer felt compresses more: a rounder, lower-peaked push.
                hammerShape = 1.0f + 2.0f * (1.0f - hardness);
                hammerLevel = std::pow (level, 1.3f) * 8.0f / std::sqrt ((float) hammerLength / 20.0f + 1.0f);
                // Strike point along the string; 1/8 by default like a grand.
                hammerOffset = juce::jmax (1.0, (double) (sampleRate / frequency)
                                                    * (excitationPosition > 0.0f ? (double) excitationPosition : 0.125));
                hammerElapsed = 0;
                hammerTotal = hammerLength + (int) std::ceil (hammerOffset) + 1;
            }
            return;
        }

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

    float process (float expression = 0.0f, bool noteHeld = true, float externalInput = 0.0f)
    {
        if (buffer.empty())
            return 0.0f;

        if (excite == Excite::Bow)
            return processBowed (expression, noteHeld);

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


            case Excite::Hammer:
                if (hammerElapsed < hammerTotal)
                {
                    // The strike launches two pulses along the string; the
                    // one reflected from the near end arrives a strike
                    // distance later and notches out the harmonics that have
                    // a node at the strike point.
                    const auto elapsed = (double) hammerElapsed++;
                    excitation = hammerForce (elapsed) - hammerForce (elapsed - hammerOffset);
                }
                break;

            case Excite::External:
                excitation = juce::jlimit (-2.0f, 2.0f, externalInput) * (0.02f + sustainLevel * 0.3f)
                             * (0.25f + 0.75f * strikeVelocity);
                break;

            case Excite::Burst:
            default:
                break;
        }

        excitation += bridgeInput;
        bridgeInput = 0.0f;

        phase += frequency / sampleRate;

        if (phase >= 1.0)
            phase -= 1.0;

        auto loopValue = lowpassState * feedback;

        // Dampers: once the key is up, felt lands on the string. The loss is
        // per trip round the loop, so low strings die away more slowly.
        if (damper > 0.0f && ! noteHeld)
            loopValue *= 1.0f - damper * 0.16f; // full: about 0.15 s to silence at middle C

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
            && buzz == 0.0f && rattle == 0.0f && excite != Excite::Bow
            && excite != Excite::Hammer && excite != Excite::External)
            return output;

        return std::isfinite (output) ? juce::jlimit (-8.0f, 8.0f, output) : 0.0f;
    }

private:
    // A bowed string as two waveguides either side of the bow (the STK
    // Bowed model): waves reflect, inverted, at the nut and at the bridge,
    // where the string also loses energy. The bow grips while the string
    // moves with it and slips once the velocity difference grows (the
    // friction table); more pressure flattens the table, so the bow holds
    // on to larger differences. Both lines share the string's buffer.
    float processBowed (float expression, bool noteHeld)
    {
        const auto size = (int) buffer.size();
        const auto half = size / 2;
        const auto period = sampleRate / frequency;
        const auto position = juce::jlimit (0.06, 0.5, excitationPosition > 0.0f ? (double) excitationPosition : 0.127);
        const auto base = juce::jmax (4.0, period - 1.0 - (1.0 - (double) lowpassCoefficient) / (double) lowpassCoefficient);
        const auto bridgeLength = juce::jlimit (2.0, (double) half - 2.0, base * position);
        const auto neckLength = juce::jlimit (2.0, (double) half - 2.0, base * (1.0 - position));

        const auto readLine = [this, half] (int offset, int write, double length)
        {
            auto read = (double) write - length;
            while (read < 0.0)
                read += (double) half;
            const auto index = (int) read;
            const auto fraction = (float) (read - (double) index);
            const auto a = buffer[(size_t) (offset + index)];
            const auto b = buffer[(size_t) (offset + (index + 1) % half)];
            return a + (b - a) * fraction;
        };

        const auto bridgeOut = readLine (0, bowBridgeWrite, bridgeLength);
        const auto neckOut = readLine (half, bowNeckWrite, neckLength);

        // Bridge: loss and damping on every reflection (DECAY and DAMP).
        lowpassState += (bridgeOut - lowpassState) * lowpassCoefficient;
        auto bridgeReflection = -lowpassState * feedback;
        if (damper > 0.0f && ! noteHeld)
            bridgeReflection *= 1.0f - damper * 0.16f;
        const auto neckReflection = -neckOut;
        const auto stringVelocity = bridgeReflection + neckReflection;

        auto bowForce = 0.0f;

        if (noteHeld)
        {
            const auto pressure = juce::jlimit (0.0f, 1.0f, bowPressure + expression * 0.75f);
            bowRamp += (1.0f - bowRamp) * bowRampCoefficient;
            const auto bowVelocity = (0.03f + 0.2f * bowSpeed) * (0.6f + 0.4f * strikeVelocity) * bowRamp;
            const auto difference = bowVelocity - stringVelocity;
            const auto slope = 5.0f - 4.0f * pressure;
            const auto table = juce::jmin (1.0f, std::pow (std::abs ((difference + 0.001f) * slope) + 0.75f, -4.0f));
            bowForce = difference * table + (random.nextFloat() * 2.0f - 1.0f) * 0.0005f * bowSpeed * bowRamp;
        }

        bowForce += bridgeInput;
        bridgeInput = 0.0f;

        buffer[(size_t) bowBridgeWrite] = juce::jlimit (-4.0f, 4.0f, neckReflection + bowForce);
        buffer[(size_t) (half + bowNeckWrite)] = juce::jlimit (-4.0f, 4.0f, bridgeReflection + bowForce);
        bowBridgeWrite = (bowBridgeWrite + 1) % half;
        bowNeckWrite = (bowNeckWrite + 1) % half;

        const auto output = bridgeOut * bowOutputGain;
        return std::isfinite (output) ? juce::jlimit (-8.0f, 8.0f, output) : 0.0f;
    }

    // Hammer force over the contact: a raised half-sine, flatter for soft felt.
    float hammerForce (double elapsed) const
    {
        if (elapsed < 0.0 || elapsed >= (double) hammerLength)
            return 0.0f;

        const auto shape = std::sin (juce::MathConstants<double>::pi * elapsed / (double) hammerLength);
        return hammerLevel * (float) std::pow (shape, (double) hammerShape);
    }

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
    float hammerHardness = 0.5f, damper = 0.0f, bridgeInput = 0.0f;
    float bowRamp = 0.0f, bowRampCoefficient = 0.001f;
    int bowNeckWrite = 0, bowBridgeWrite = 0;
    static constexpr float bowOutputGain = 2.0f;
    float hammerLevel = 0.0f, hammerShape = 1.0f;
    double hammerOffset = 1.0;
    int hammerLength = 1, hammerElapsed = 0, hammerTotal = 0;
    Excite excite = Excite::Burst;
};
