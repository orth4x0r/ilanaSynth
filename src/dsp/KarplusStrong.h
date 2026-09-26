#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
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
        std::fill (std::begin (dispersionState), std::end (dispersionState), 0.0f);
        std::fill (std::begin (dispersionInput), std::end (dispersionInput), 0.0f);
        slapRemaining = 0;
        hammerElapsed = hammerTotal = 0;
        hammerDc = 0.0f;
        bridgeInput = 0.0f;
    }

    void setFrequency (double hz)
    {
        frequency = juce::jlimit (15.0, sampleRate * 0.45, hz);
        updateHammerFeedback();
        updatePianoDispersion();
    }

    void setParams (Excite newExcite, float newSustainLevel, float newDamping, float newDecay)
    {
        excite = newExcite;
        updateDispersionDelay();
        sustainLevel = juce::jlimit (0.0f, 1.0f, newSustainLevel);
        damping = juce::jlimit (0.0f, 1.0f, newDamping);
        decay = juce::jlimit (0.0f, 1.0f, newDecay);

        lowpassCoefficient = 1.0f - damping * 0.96f;
        feedback = 0.90f + decay * 0.0995f;
        updateHammerFeedback();
        updatePianoDispersion();
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
        updateDispersionDelay();
        updatePianoDispersion();
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
    // it is added on the next sample. It acts once per trip round the loop,
    // so scale it by pitch: the same loss per second in every register
    // (calibrated at C3; lower strings keep their long bass sustain),
    // rather than wiping out the treble.
    void addBridgeInput (float value) { bridgeInput += value * (float) juce::jmin (1.0, 130.81 / frequency); }

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
            bowDcInput = bowDcOutput = 0.0f;
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
            std::fill (std::begin (dispersionState), std::end (dispersionState), 0.0f);
            std::fill (std::begin (dispersionInput), std::end (dispersionInput), 0.0f);
            slapRemaining = slap ? (int) (sampleRate * 0.004) : 0;
            slapLevel = level;
            strikeVelocity = level;
            bridgeInput = 0.0f;

            if (excite == Excite::Hammer)
                startHammer (level);
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
        std::fill (std::begin (dispersionState), std::end (dispersionState), 0.0f);
        std::fill (std::begin (dispersionInput), std::end (dispersionInput), 0.0f);
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
        const auto hammered = excite == Excite::Hammer;
        const auto useDispersion = hammered ? pianoCoefficient != 0.0f : (stiffness > 0.0f && period > 3.5);
        const auto stages = (double) dispersionStages();
        auto delay = useDispersion ? juce::jmin ((double) dispersionDelay, period - 1.25) : 0.0;
        auto coefficient = delay < (double) dispersionDelay
                               ? (float) ((stages - delay) / (stages + delay)) : dispersionCoefficient;

        // Piano strings: the designed allpass, and the loop's exact delay at
        // the fundamental taken off so the note stays in tune.
        if (hammered)
        {
            delay = pianoLoopDelay;
            coefficient = pianoCoefficient;
        }
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
            for (int stage = 0; stage < dispersionStages(); ++stage)
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
                    excitation = hammerExcitation();
                    output += hammerThump();
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

        // A hammer's push has a net area, and a piano string's loop keeps DC
        // almost forever: bleed it away slowly (1 Hz, far below any note).
        if (excite == Excite::Hammer)
        {
            hammerDc += (loopValue - hammerDc) * hammerDcCoefficient;
            loopValue -= hammerDc;
        }

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
    // Piano strings are much stiffer than the lengths of wire the Physical
    // mode was built for: more allpass stages make the upper partials
    // audibly sharp, as in a real piano's bass.
    static constexpr int maxDispersionStages = 8;
    int dispersionStages() const { return excite == Excite::Hammer ? maxDispersionStages : 2; }
    void updateDispersionDelay()
    {
        dispersionDelay = (float) dispersionStages() * (1.0f - dispersionCoefficient) / (1.0f + dispersionCoefficient);
    }

    // A stiff piano string's partials run sharp: partial n sits at
    // n f0 sqrt (1 + B n^2), and that stretch (largest in the bass, where the
    // loop is long) is much of what makes a piano sound like one. STIFF maps
    // to the inharmonicity B; the allpass coefficient is solved so that a
    // reference partial lands where B puts it, and the loop is shortened by
    // the exact allpass and damping delay at the fundamental.
    static double allpassPhaseDelay (double a, double w)
    {
        const auto e = std::polar (1.0, -w);
        return -std::arg ((a + e) / (1.0 + a * e)) / w;
    }

    double lowpassPhaseDelay (double w) const
    {
        const auto a = (double) lowpassCoefficient;
        return -std::arg (a / (1.0 - (1.0 - a) * std::polar (1.0, -w))) / w;
    }

    void updatePianoDispersion()
    {
        if (excite != Excite::Hammer)
            return;

        if (std::abs (frequency - pianoFrequency) < frequency * 0.002 && stiffness == pianoStiffness
            && lowpassCoefficient == pianoLowpass)
            return;

        pianoFrequency = frequency;
        pianoStiffness = stiffness;
        pianoLowpass = lowpassCoefficient;

        const auto period = sampleRate / frequency;
        const auto w0 = juce::MathConstants<double>::twoPi / period;
        const auto dampingDelay = lowpassPhaseDelay (w0);
        pianoCoefficient = 0.0f;
        pianoLoopDelay = juce::jmin (dampingDelay, period - 1.25);

        const auto reference = juce::jmin (12, (int) (0.3 * sampleRate / frequency));
        if (stiffness <= 0.0f || reference < 2 || period < 8.0)
            return;

        const auto b = 1.0e-5 * std::pow (10.0, 3.0 * (double) stiffness);
        const auto n = (double) reference;
        const auto wTarget = n * w0 * std::sqrt (1.0 + b * n * n) / std::sqrt (1.0 + b);
        const auto needed = juce::MathConstants<double>::twoPi * n / wTarget - period; // negative
        const auto stages = (double) maxDispersionStages;
        const auto error = [&] (double a) { return stages * (allpassPhaseDelay (a, wTarget) - allpassPhaseDelay (a, w0)) - needed; };

        auto low = -0.95, high = 0.0;
        if (error (low) > 0.0)
            high = low; // as far as the filter goes
        else
            for (int iteration = 0; iteration < 30; ++iteration)
            {
                const auto middle = 0.5 * (low + high);
                (error (middle) > 0.0 ? high : low) = middle;
            }

        // The loop cannot be shorter than the filters' own delay.
        auto a = high;
        while (a < -0.01 && stages * allpassPhaseDelay (a, w0) + dampingDelay > period - 2.0)
            a *= 0.8;

        pianoCoefficient = (float) a;
        pianoLoopDelay = stages * allpassPhaseDelay (a, w0) + dampingDelay;
    }

    // Commuted piano synthesis (Smith and Van Duyne): rather than model the
    // hammer, string and soundboard together, the string is driven by what
    // the hammer and board would give it. That is the felt's push (shorter
    // and brighter for harder hammers and faster keys), plus the board's
    // knock: a short burst of wooden resonance, longer in the bass, brighter
    // for a harder strike. Both pass the strike point, which notches out the
    // partials with a node there.
    void startHammer (float level)
    {
        const auto hardness = juce::jlimit (0.0f, 1.0f, hammerHardness * 0.75f + level * 0.5f - 0.1f);
        const auto period = sampleRate / frequency;
        // 0 in the treble .. 1 in the deep bass.
        const auto bassness = (float) juce::jlimit (0.0, 1.0, std::log2 (523.25 / frequency) / 4.0);

        // Treble hammers are small and light: they leave the string within
        // half a cycle, so the strike still reaches the note itself.
        const auto registerScale = juce::jlimit (0.2, 2.2, std::sqrt (220.0 / frequency));
        const auto contactMs = (0.5 + 3.0 * (1.0 - (double) hardness)) * registerScale;
        hammerLength = juce::jlimit (4, juce::jmax (4, (int) (period * 0.5)), (int) (sampleRate * contactMs * 0.001));
        // Softer felt compresses more: a rounder, lower-peaked push.
        hammerShape = 1.0f + 2.0f * (1.0f - hardness);
        // Same strike energy in every register: a short treble contact pushes harder.
        hammerLevel = std::pow (level, 1.3f) * 8.0f * std::sqrt (20.0f / (float) (hammerLength + 20));
        // No hammer hits a note's strings exactly evenly; the difference is
        // what feeds the aftersound of coupled strings.
        hammerLevel *= 0.85f + 0.3f * random.nextFloat();

        // Strike point along the string; 1/8 by default like a grand.
        const auto offset = period * (excitationPosition > 0.0f ? (double) excitationPosition : 0.125);
        hammerOffset = juce::jlimit (1.0, (double) hammerHistorySize - 1.0, offset);
        hammerReflection = (float) juce::jlimit (0.35, 0.9, hammerOffset / (0.5 * (double) hammerLength));

        // The board's knock: filtered noise with a fast decay.
        const auto knockSeconds = 0.012 + 0.045 * (double) bassness;
        knockLength = (int) (sampleRate * knockSeconds * 4.0);
        knockDecay = (float) std::exp (-1.0 / (sampleRate * knockSeconds));
        const auto knockCutoff = 700.0 + 5500.0 * (double) (hardness * hardness);
        knockCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * knockCutoff / sampleRate));
        // Filtering lowers the noise's level; put it back, then set it to
        // about a tenth of the felt's push.
        const auto filteredLevel = std::sqrt (knockCoefficient / (2.0f - knockCoefficient)) * 0.8f;
        knockLevel = hammerLevel * 0.1f / juce::jmax (0.05f, filteredLevel);
        knockEnvelope = 1.0f;
        knockLow1 = knockLow2 = 0.0f;

        // Heard directly too: the thump of felt and wood, strongest in the bass.
        thumpLength = (int) (sampleRate * 0.03);
        thumpDecay = (float) std::exp (-1.0 / (sampleRate * 0.007));
        thumpCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * (160.0 + 240.0 * (double) hardness) / sampleRate));
        thumpLevel = std::pow (level, 1.5f) * (0.25f + 0.6f * bassness);
        thumpEnvelope = 1.0f;
        thumpLow1 = thumpLow2 = 0.0f;

        std::fill (hammerHistory.begin(), hammerHistory.end(), 0.0f);
        hammerHistoryWrite = 0;
        hammerElapsed = 0;
        hammerTotal = juce::jmax (hammerLength, knockLength) + (int) std::ceil (hammerOffset) + 2;
        hammerDc = 0.0f;
        hammerDcCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi / sampleRate));
    }

    float hammerExcitation()
    {
        const auto elapsed = hammerElapsed++;
        auto raw = hammerForce ((double) elapsed);

        if (elapsed < knockLength)
        {
            const auto noise = random.nextFloat() * 2.0f - 1.0f;
            knockLow1 += (noise - knockLow1) * knockCoefficient;
            knockLow2 += (knockLow1 - knockLow2) * knockCoefficient;
            raw += knockLow2 * knockEnvelope * knockLevel;
            knockEnvelope *= knockDecay;
        }

        hammerHistory[(size_t) hammerHistoryWrite] = raw;
        const auto readIndex = (hammerHistoryWrite - (int) hammerOffset + hammerHistorySize) % hammerHistorySize;
        hammerHistoryWrite = (hammerHistoryWrite + 1) % hammerHistorySize;
        return raw - hammerReflection * hammerHistory[(size_t) readIndex];
    }

    float hammerThump()
    {
        if (hammerElapsed > thumpLength)
            return 0.0f;

        const auto noise = random.nextFloat() * 2.0f - 1.0f;
        thumpLow1 += (noise - thumpLow1) * thumpCoefficient;
        thumpLow2 += (thumpLow1 - thumpLow2) * thumpCoefficient;
        thumpEnvelope *= thumpDecay;
        return thumpLow2 * thumpEnvelope * thumpLevel * 6.0f;
    }

    // A piano string's decay is set in seconds, not per trip round the loop:
    // otherwise the treble (many trips per second) would die almost at once.
    // DECAY maps to a T60 at middle C of 0.5-25 s, and higher strings ring
    // shorter, but only gently: T60 falls as (261.6 / f)^0.7.
    void updateHammerFeedback()
    {
        if (excite != Excite::Hammer)
            return;

        const auto t60Middle = 0.5 * std::pow (50.0, (double) decay);
        const auto t60 = juce::jlimit (0.05, 40.0, t60Middle * std::pow (261.63 / frequency, 0.7));

        // DAMP's loop low-pass also takes a little off the fundamental on
        // every pass, which in the treble would swamp DECAY: make it up, so
        // DAMP only shapes the upper partials.
        const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto a = (double) lowpassCoefficient;
        const auto gainAtFundamental = a / std::abs (std::complex<double> (1.0, 0.0) - (1.0 - a) * std::polar (1.0, -w));
        feedback = (float) juce::jmin (0.99995, std::pow (10.0, -3.0 / (t60 * frequency)) / juce::jmax (0.1, gainAtFundamental));
    }

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

        // The bow's steady push leaves an offset on the bridge: block DC.
        const auto blocked = bridgeOut - bowDcInput + 0.995f * bowDcOutput;
        bowDcInput = bridgeOut;
        bowDcOutput = std::isfinite (blocked) ? blocked : 0.0f;

        const auto output = bowDcOutput * bowOutputGain;
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
    float dispersionState[maxDispersionStages] {};
    float dispersionInput[maxDispersionStages] {};
    bool slap = false;
    int slapRemaining = 0;
    float slapLevel = 0.0f;
    float bowPressure = 0.0f, bowSpeed = 0.5f, buzz = 0.0f, rattle = 0.0f, strikeVelocity = 0.0f;
    float hammerHardness = 0.5f, damper = 0.0f, bridgeInput = 0.0f;
    float bowRamp = 0.0f, bowRampCoefficient = 0.001f;
    int bowNeckWrite = 0, bowBridgeWrite = 0;
    float bowDcInput = 0.0f, bowDcOutput = 0.0f;
    static constexpr float bowOutputGain = 2.0f;
    float hammerLevel = 0.0f, hammerShape = 1.0f;
    double hammerOffset = 1.0;
    int hammerLength = 1, hammerElapsed = 0, hammerTotal = 0;
    float hammerReflection = 0.9f;
    float pianoCoefficient = 0.0f, pianoStiffness = -1.0f, pianoLowpass = -1.0f;
    double pianoLoopDelay = 0.0, pianoFrequency = 0.0;
    static constexpr int hammerHistorySize = 512;
    std::array<float, hammerHistorySize> hammerHistory {};
    int hammerHistoryWrite = 0;
    int knockLength = 0, thumpLength = 0;
    float knockDecay = 0.0f, knockCoefficient = 0.1f, knockLevel = 0.0f, knockEnvelope = 0.0f, knockLow1 = 0.0f, knockLow2 = 0.0f;
    float thumpDecay = 0.0f, thumpCoefficient = 0.02f, thumpLevel = 0.0f, thumpEnvelope = 0.0f, thumpLow1 = 0.0f, thumpLow2 = 0.0f;
    float hammerDc = 0.0f, hammerDcCoefficient = 0.0001f;
    Excite excite = Excite::Burst;
};
