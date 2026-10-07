#pragma once

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #include <xmmintrin.h>
#endif

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>

#include "ElectricPiano.h"
#include "PianoString.h"
#include "FeedbackGuitar.h"
#include "PianoTuning.h"

#if defined (_MSC_VER)
 #define ILANA_NOINLINE __declspec (noinline)
#else
 #define ILANA_NOINLINE __attribute__ ((noinline))
#endif

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
        External, // M4: driven by the other oscillators (the FM matrix inputs)
        Tine,     // M7.3: a Rhodes-style tine and tone bar (ElectricPiano)
        Reed,     // M7.3: a Wurlitzer-style reed
        Piano,    // M8.2: a nonlinear felt hammer on a split waveguide (PianoString)
        Feedback  // M8.5: a pluck, then an amp and speaker in the loop (FeedbackLoop)
    };

    static constexpr int numExcites = 11;
    bool isElectric() const { return excite == Excite::Tine || excite == Excite::Reed; }
    bool isPiano() const { return excite == Excite::Piano; }

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
        electric.prepare (sampleRate);
        piano.prepare (sampleRate, buffer.data(), (int) buffer.size());
        feedbackLoop.prepare (sampleRate);
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePosition = 0;
        lowpassState = 0.0f;
        phase = 0.0;
        for (auto& stage : dispersionPairState) stage[0] = 0.0f;
        for (auto& stage : dispersionPairIn) stage[0] = 0.0f;
        slapRemaining = 0;
        hammerElapsed = hammerTotal = 0;
        resetHorizontal();
        pushLow = outputDcIn = outputDcOut = 0.0f;
        thiranInput = thiranOutput = 0.0f;
        bridgeInput = 0.0f;
        electric.reset();
        piano.reset();
    }

    void setFrequency (double hz)
    {
        electric.setFrequency (hz);
        frequency = juce::jlimit (15.0, sampleRate * 0.45, hz);
        piano.setNote (hz, juce::roundToInt (69.0 + 12.0 * std::log2 (juce::jmax (1.0, hz) / 440.0)));
        updateHammerFeedback();
        updatePianoDispersion();
    }

    void setParams (Excite newExcite, float newSustainLevel, float newDamping, float newDecay)
    {
        excite = newExcite;
        if (isElectric())
            electric.setModel (excite == Excite::Tine ? ElectricPiano::Model::Tine : ElectricPiano::Model::Reed);
        updateDispersionDelay();
        sustainLevel = juce::jlimit (0.0f, 1.0f, newSustainLevel);
        damping = juce::jlimit (0.0f, 1.0f, newDamping);
        decay = juce::jlimit (0.0f, 1.0f, newDecay);

        lowpassCoefficient = 1.0f - damping * 0.96f;
        feedback = 0.90f + decay * 0.0995f;
        updateHammerFeedback();
        updatePianoDispersion();
        updatePiano();
    }

    // M8.5: the Feedback exciter's amp: FEEDBACK is the string's SUSTAIN.
    void setFeedbackParams (float gain, float distance)
    {
        feedbackLoop.setParams (excite == Excite::Feedback ? sustainLevel : 0.0f, gain, distance);
    }

    // M8.2: Eco quality lightens the Piano exciter (fewer allpasses, one
    // polarisation, no longitudinal modes).
    void setEco (bool newEco)
    {
        eco = newEco;
        updatePiano();
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
        updatePiano();
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
        updatePiano();
    }

    // M7.3: the tine or reed's knobs (DECAY, DAMP, pickup distance and
    // position, hammer hardness, damper).
    void setElectricParams (float distance, float position)
    {
        electric.setParams (decay, damping, distance, position, hammerHardness, damper);
    }

    // M7.5: live audio driving the string (or tine / reed) on the next
    // sample, whatever the exciter.
    void addLiveInput (float value)
    {
        // Scaled so a string driven at its pitch comes out near the input's
        // level (the loop resonates, so a little goes a long way).
        if (isElectric())
            electric.addForce (value * 0.02f);
        else if (isPiano())
            piano.addBridgeInput (value * 0.1f);
        else
            bridgeInput += value * 0.1f;
    }

    // Force arriving from the bridge (the other strings of the same note);
    // it is added on the next sample. It acts once per trip round the loop,
    // so scale it by pitch: the same loss per second in every register
    // (calibrated at C3; lower strings keep their long bass sustain),
    // rather than wiping out the treble.
    void addBridgeInput (float value)
    {
        if (isPiano())
            piano.addBridgeInput (value * (float) juce::jmin (1.0, 130.81 / frequency));
        else
            bridgeInput += value * (float) juce::jmin (1.0, 130.81 / frequency);
    }

    void trigger (float velocity)
    {
        if (buffer.empty())
            return;

        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        if (isElectric())
        {
            strikeVelocity = level;
            electric.trigger (level);
            return;
        }

        if (isPiano())
        {
            strikeVelocity = level;
            piano.trigger (level, random.nextFloat());
            return;
        }

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
            for (auto& stage : dispersionPairState) stage[0] = 0.0f;
            for (auto& stage : dispersionPairIn) stage[0] = 0.0f;
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
        for (auto& stage : dispersionPairState) stage[0] = 0.0f;
        for (auto& stage : dispersionPairIn) stage[0] = 0.0f;
        slapRemaining = slap ? (int) (sampleRate * 0.004) : 0;
        slapLevel = level;
        strikeVelocity = level;
    }

    // Kept out of line: inlined into the voice's per-sample loop, this large
    // function slowed the wavetable path beside it (~14 % more CPU).
    ILANA_NOINLINE float process (float expression = 0.0f, bool noteHeld = true, float externalInput = 0.0f)
    {
        if (buffer.empty())
            return 0.0f;

        if (isElectric())
            return electric.process (noteHeld);

        if (isPiano())
            return piano.process (noteHeld);

        if (excite == Excite::Bow)
            return processBowed (expression, noteHeld);

        const auto size = (int) buffer.size();
        // The divisions below are cached on their inputs: the pitch moves
        // at most once a sub-block, so they rarely need doing.
        if (frequency != cachedFrequency || sampleRate != cachedSampleRate)
        {
            cachedFrequency = frequency;
            cachedSampleRate = sampleRate;
            cachedPeriod = sampleRate / frequency;
            cachedPhaseStep = frequency / sampleRate;
        }
        const auto period = cachedPeriod;
        const auto hammered = excite == Excite::Hammer;
        // Piano strings run two loops (see readHorizontal), one per half.
        const auto loopSize = hammered ? size / 2 : size;
        const auto useDispersion = hammered ? pianoCoefficient != 0.0f : (stiffness > 0.0f && period > 3.5);
        const auto stages = (double) dispersionStages();
        auto delay = useDispersion ? juce::jmin ((double) dispersionDelay, period - 1.25) : 0.0;
        auto coefficient = dispersionCoefficient;
        if (delay < (double) dispersionDelay)
        {
            if (delay != cachedShortDelay || stages != cachedShortStages)
            {
                cachedShortDelay = delay;
                cachedShortStages = stages;
                cachedShortCoefficient = (float) ((stages - delay) / (stages + delay));
            }
            coefficient = cachedShortCoefficient;
        }

        // Piano strings: the designed allpass, and the loop's exact delay at
        // the fundamental taken off so the note stays in tune.
        if (hammered)
        {
            delay = pianoLoopDelay;
            coefficient = pianoCoefficient;
        }
        // The damping low-pass in the loop delays the fundamental too (about
        // (1 - c) / c samples): take its phase delay off as well, or the
        // string sits flat by about 0.7 samples (11 cents at A4, 48 kHz).
        if (! hammered)
        {
            // Cached: the pitch and damping change at most once a sub-block.
            if (period != lowpassDelayPeriod || lowpassCoefficient != lowpassDelayCoefficient)
            {
                const auto omega = juce::MathConstants<double>::twoPi / juce::jmax (2.0, period);
                const auto pole = 1.0 - (double) lowpassCoefficient;
                lowpassDelay = std::atan2 (pole * std::sin (omega), 1.0 - pole * std::cos (omega)) / omega;
                lowpassDelayPeriod = period;
                lowpassDelayCoefficient = lowpassCoefficient;
            }
            delay = juce::jmin (delay + lowpassDelay, period - 1.25);
        }

        auto readPosition = (double) writePosition - period
                            + delay;

        while (readPosition < 0.0)
            readPosition += (double) size;
        while (readPosition >= (double) size)
            readPosition -= (double) size;

        // (A piano string reads through its Thiran allpass below instead.)
        auto rawDelayed = 0.0f;
        if (! hammered)
        {
            const auto index = (int) readPosition;
            const auto nextIndex = index + 1 < size ? index + 1 : (index + 1) % size;
            const auto fraction = (float) (readPosition - (double) index);
            rawDelayed = buffer[(size_t) index]
                       + (buffer[(size_t) nextIndex] - buffer[(size_t) index]) * fraction;
        }

        // Piano strings read the fractional part through a first-order
        // allpass (Thiran) instead: linear interpolation averages neighbours,
        // a low-pass that on a 23-sample treble loop costs ~170 dB/s.
        if (hammered)
        {
            const auto length = period - delay;
            if (length != verticalTap.length)
                verticalTap = thiranTapFor (length);
            const auto whole = verticalTap.whole;
            auto readIndex = writePosition - whole;
            while (readIndex < 0)
                readIndex += loopSize;
            const auto input = buffer[(size_t) (readIndex < loopSize ? readIndex : readIndex % loopSize)];
            const auto a = verticalTap.a;
            rawDelayed = a * input + thiranInput - a * thiranOutput;
            thiranInput = input;
            thiranOutput = rawDelayed;
        }

        auto delayed = rawDelayed;

        // A piano string's horizontal polarisation reads its own half of the
        // buffer, so it is read now and both run their (identical) stiffness
        // allpasses together, one SIMD lane each.
        auto horizontal = 0.0f;
        if (hammered)
            horizontal = readHorizontal (period);

        if (hammered && useDispersion)
        {
            disperseHammeredPair (delayed, horizontal);
        }
        else if (useDispersion)
        {
            for (int stage = 0; stage < dispersionStages(); ++stage)
            {
                const auto next = coefficient * delayed + dispersionPairIn[stage][0]
                                  - coefficient * dispersionPairState[stage][0];
                dispersionPairIn[stage][0] = delayed;
                dispersionPairState[stage][0] = next;
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

            case Excite::Feedback:
                // The amp hears the pickup and pushes the string back
                // through the air.
                excitation = feedbackLoop.process (output);
                break;

            case Excite::Burst:
            default:
                break;
        }

        const auto drive = excitation;
        excitation += bridgeInput;
        bridgeInput = 0.0f;

        phase += cachedPhaseStep;

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
        writePosition = writePosition + 1 < loopSize ? writePosition + 1 : (writePosition + 1) % loopSize;

        if (hammered)
            output += finishHorizontal (horizontal, drive, noteHeld);

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

        // Piano strings: a 10 Hz DC blocker on what we hear (outside the
        // loop; a high-pass inside it rings at a sub-audio rate).
        if (excite == Excite::Hammer)
        {
            const auto blocked = output - outputDcIn + 0.9987f * outputDcOut;
            outputDcIn = output;
            outputDcOut = std::isfinite (blocked) ? blocked : 0.0f;
            output = outputDcOut;
        }

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

    static double lowpassPhaseDelay (double w, double a)
    {
        return -std::arg (a / (1.0 - (1.0 - a) * std::polar (1.0, -w))) / w;
    }

    double lowpassPhaseDelay (double w) const { return lowpassPhaseDelay (w, (double) lowpassCoefficient); }

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
        const auto horizontalDampingDelay = lowpassPhaseDelay (w0, (double) horizontalLowpass);
        pianoCoefficient = 0.0f;
        pianoLoopDelay = juce::jmin (dampingDelay, period - 1.25);
        horizontalLoopDelay = juce::jmin (horizontalDampingDelay, period - 1.25);

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
        horizontalLoopDelay = juce::jmin (period - 1.25, stages * allpassPhaseDelay (a, w0) + horizontalDampingDelay);
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
        const auto& tuning = PianoTuning::get();
        const auto hardness = juce::jlimit (0.0f, 1.0f, hammerHardness * tuning.hardnessKnob
                                                         + level * tuning.hardnessVelocity - 0.1f);
        const auto period = sampleRate / frequency;
        // 0 in the treble .. 1 in the deep bass.
        const auto bassness = (float) juce::jlimit (0.0, 1.0, std::log2 (523.25 / frequency) / 4.0);

        // Contact time: shorter for harder, faster strikes and for the small
        // treble hammers. Up high the felt stays on for most of a cycle,
        // which leaves an almost pure fundamental, as in a real C7 (its 2nd
        // partial is ~40 dB down).
        const auto registerScale = juce::jlimit (0.2, 1.8, std::pow (220.0 / frequency, (double) tuning.contactRegister));
        const auto contactMs = ((double) tuning.contactBase + (double) tuning.contactSlope * (1.0 - (double) hardness)) * registerScale;
        hammerLength = juce::jmax (4, (int) (sampleRate * contactMs * 0.001));
        if (frequency > (double) tuning.trebleFrequency)
            hammerLength = juce::jmax (hammerLength, (int) (period * (double) tuning.trebleContact));
        // Felt stiffens as it compresses: a harder strike gives a sharper
        // force peak, so more upper partials; less so in the treble.
        const auto treble = (float) juce::jlimit (0.0, 1.0, (frequency - 500.0) / 1500.0);
        // In the treble the push becomes a raised cosine a cycle long: its
        // spectrum has a null at the 2nd partial and very low sidelobes.
        hammerShape = (1.0f + tuning.shapeHardness * hardness) * (1.0f - treble) + 2.0f * treble;
        // Same strike energy in every register: a short treble contact pushes harder.
        hammerLevel = std::pow (level, tuning.velocityCurve) * 8.0f * std::sqrt (20.0f / (float) (hammerLength + 20));
        // No hammer hits a note's strings exactly evenly; the difference is
        // what feeds the aftersound of coupled strings.
        hammerLevel *= 0.85f + 0.3f * random.nextFloat();

        // Strike point along the string; 1/8 by default like a grand.
        const auto offset = period * (excitationPosition > 0.0f ? (double) excitationPosition : (double) tuning.strikePosition);
        hammerOffset = juce::jlimit (1.0, (double) hammerHistorySize - 1.0, offset);
        hammerReflection = tuning.strikeReflection;

        // The board's knock: filtered noise with a fast decay.
        const auto knockSeconds = 0.012 + 0.045 * (double) bassness;
        knockLength = (int) (sampleRate * knockSeconds * 4.0);
        knockDecay = (float) std::exp (-1.0 / (sampleRate * knockSeconds));
        const auto knockCutoff = 700.0 + 5500.0 * (double) (hardness * hardness);
        knockCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * knockCutoff / sampleRate));
        // Filtering lowers the noise's level; put it back, then keep it a
        // quiet wooden tick under the note.
        const auto filteredLevel = std::sqrt (knockCoefficient / (2.0f - knockCoefficient)) * 0.8f;
        knockLevel = hammerLevel * tuning.knock / juce::jmax (0.05f, filteredLevel);
        knockEnvelope = 1.0f;
        knockLow1 = knockLow2 = 0.0f;

        // Heard directly too: the thump of felt and wood, strongest in the bass.
        thumpLength = (int) (sampleRate * 0.03);
        thumpDecay = (float) std::exp (-1.0 / (sampleRate * 0.007));
        thumpCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * (160.0 + 240.0 * (double) hardness) / sampleRate));
        thumpLevel = tuning.thump * std::pow (level, 1.5f) * (0.25f + 0.6f * bassness);
        thumpEnvelope = 1.0f;
        thumpLow1 = thumpLow2 = 0.0f;

        std::fill (hammerHistory.begin(), hammerHistory.end(), 0.0f);
        hammerHistoryWrite = 0;
        hammerElapsed = 0;
        // The push is high-passed (zero net area) so it never leaves an offset
        // in the string; its negative tail needs a few more milliseconds.
        pushLow = 0.0f;
        pushCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * juce::jmin (20.0, frequency * 0.25) / sampleRate));
        hammerRaisedCosine = treble > 0.5f;
        hammerTotal = juce::jmax (hammerLength * 4, knockLength) + (int) std::ceil (hammerOffset) + 2
                      + (int) (5.0 / (juce::MathConstants<double>::twoPi * juce::jmin (20.0, frequency * 0.25)) * sampleRate);
        outputDcIn = outputDcOut = 0.0f;
        thiranInput = thiranOutput = 0.0f;
        resetHorizontal();
    }

    float hammerExcitation()
    {
        const auto elapsed = hammerElapsed++;
        auto raw = hammerForce ((double) elapsed);
        pushLow += (raw - pushLow) * pushCoefficient;
        raw -= pushLow;

        hammerHistory[(size_t) hammerHistoryWrite] = raw;
        const auto readIndex = (hammerHistoryWrite - (int) hammerOffset + hammerHistorySize) % hammerHistorySize;
        hammerHistoryWrite = (hammerHistoryWrite + 1) % hammerHistorySize;
        return raw - hammerReflection * hammerHistory[(size_t) readIndex];
    }

    // Heard directly, never fed round the string: noise circulating in the
    // loop is exactly what makes a digital plucked string.
    float hammerThump()
    {
        auto value = 0.0f;

        if (hammerElapsed <= thumpLength)
        {
            const auto noise = random.nextFloat() * 2.0f - 1.0f;
            thumpLow1 += (noise - thumpLow1) * thumpCoefficient;
            thumpLow2 += (thumpLow1 - thumpLow2) * thumpCoefficient;
            thumpEnvelope *= thumpDecay;
            value += thumpLow2 * thumpEnvelope * thumpLevel * 6.0f;
        }

        if (hammerElapsed <= knockLength)
        {
            const auto noise = random.nextFloat() * 2.0f - 1.0f;
            knockLow1 += (noise - knockLow1) * knockCoefficient;
            knockLow2 += (knockLow1 - knockLow2) * knockCoefficient;
            value += knockLow2 * knockEnvelope * knockLevel;
            knockEnvelope *= knockDecay;
        }

        return value;
    }

    // A piano string's decay is set in seconds, not per trip round the loop:
    // otherwise the treble (many trips per second) would die almost at once.
    // DECAY maps to a T60 at middle C of 0.5-25 s, and higher strings ring
    // shorter, but only gently: T60 falls as (261.6 / f)^0.7.
    void updateHammerFeedback()
    {
        if (excite != Excite::Hammer)
            return;

        const auto& tuning = PianoTuning::get();

        // The voice calls this every sub-block; the 80 bisection steps below
        // were half a Hammered Strings voice's CPU. Same inputs, same answer.
        const std::array<double, 10> inputs { frequency, sampleRate, (double) decay, (double) damping,
                                              (double) tuning.t60Scale, (double) tuning.t60Range, (double) tuning.t60Register,
                                              (double) tuning.lossTop, (double) tuning.lossBottom, (double) tuning.promptRatio };
        if (hammerDesignValid && inputs == hammerDesignInputs)
        {
            lowpassCoefficient = hammerDesign[0];
            feedback = hammerDesign[1];
            horizontalLowpass = hammerDesign[2];
            horizontalFeedback = hammerDesign[3];
            return;
        }
        const auto t60Middle = (double) tuning.t60Scale * std::pow ((double) tuning.t60Range, (double) decay);
        const auto t60 = juce::jlimit (0.05, 40.0, t60Middle * std::pow (261.63 / frequency, (double) tuning.t60Register));

        // A real string's upper partials die in about 1-3 s whatever the
        // note, so the loss filter is designed in seconds too: the
        // fundamental rings for DECAY's T60, the partials around 1.5 kHz for
        // DAMP's (3 s down to 0.1 s). A fixed per-pass filter would leave a
        // long bass string bright and buzzing for many seconds, which is the
        // sound of a digital plucked string.
        // Loss grows with the square of frequency (Bank's piano-string
        // model). Anchored at 2 kHz, where a real grand's partials ring for
        // several seconds: 20 s with DAMP at 0, 7 s at 0.25, 0.3 s at 1.
        const auto upper = juce::jmin (sampleRate * 0.4, juce::jmax (2000.0, 2.0 * frequency));
        const auto lossAt2k = 6.91 / ((double) tuning.lossTop * std::pow ((double) tuning.lossBottom / (double) tuning.lossTop, (double) damping));
        const auto passGain = [this] (double seconds) { return std::pow (10.0, -3.0 / (seconds * frequency)); };
        const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto w1 = juce::MathConstants<double>::twoPi * juce::jmin (upper, sampleRate * 0.45) / sampleRate;
        const auto magnitude = [] (double pole, double w)
        {
            return (1.0 - pole) / std::sqrt (1.0 - 2.0 * pole * std::cos (w) + pole * pole);
        };

        // One-pole loss filter for a fundamental T60 (and the same extra
        // loss towards 2 kHz): find the pole giving the wanted ratio.
        const auto design = [&] (double fundamentalT60, float& coefficient, float& gain)
        {
            const auto lossUpper = 6.91 / fundamentalT60 + lossAt2k * (upper * upper - frequency * frequency) / (2000.0 * 2000.0);
            const auto upperT60 = juce::jmin (fundamentalT60 * 0.95, 6.91 / lossUpper);
            const auto wanted = passGain (upperT60) / passGain (fundamentalT60);
            auto low = 0.0, high = 0.999;
            for (int iteration = 0; iteration < 40; ++iteration)
            {
                const auto pole = 0.5 * (low + high);
                (magnitude (pole, w1) / magnitude (pole, w0) > wanted ? low : high) = pole;
            }
            const auto pole = 0.5 * (low + high);
            coefficient = (float) (1.0 - pole);
            gain = (float) juce::jmin (0.99995, passGain (fundamentalT60) / magnitude (pole, w0));
        };

        // Two polarisations: the vertical motion pushes the bridge hard, so
        // it radiates the loud prompt sound and dies fast; the horizontal
        // barely couples, and rings on as the quiet aftersound. DECAY sets
        // the aftersound; the prompt sound lasts promptRatio of it.
        design (juce::jmax (0.05, t60 * (double) tuning.promptRatio), lowpassCoefficient, feedback);
        design (t60, horizontalLowpass, horizontalFeedback);

        hammerDesignInputs = inputs;
        hammerDesign = { lowpassCoefficient, feedback, horizontalLowpass, horizontalFeedback };
        hammerDesignValid = true;
    }

    std::array<double, 10> hammerDesignInputs {};
    std::array<float, 4> hammerDesign {};
    bool hammerDesignValid = false;

    // The horizontal polarisation: the same string (same stiffness filter),
    // in the upper half of the buffer, taking aftersound's share of the
    // strike and losing energy far more slowly.
    float readHorizontal (double period)
    {
        const auto half = (int) buffer.size() / 2;
        const auto length = juce::jlimit (1.5, (double) half - 2.0, period - horizontalLoopDelay);
        if (length != horizontalTap.length)
            horizontalTap = thiranTapFor (length);
        auto readIndex = horizontalWrite - horizontalTap.whole;
        while (readIndex < 0)
            readIndex += half;
        const auto input = buffer[(size_t) (half + (readIndex < half ? readIndex : readIndex % half))];
        const auto a = horizontalTap.a;
        const auto delayed = a * input + horizontalThiranIn - a * horizontalThiranOut;
        horizontalThiranIn = input;
        horizontalThiranOut = delayed;
        return delayed;
    }

    // Both polarisations through the piano stiffness allpasses (lane 0 the
    // vertical, lane 1 the horizontal): the same arithmetic per lane.
    void disperseHammeredPair (float& vertical, float& horizontal) noexcept
    {
       #if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
        const auto c = _mm_set1_ps (pianoCoefficient);
        auto x = _mm_set_ps (0.0f, 0.0f, horizontal, vertical);
        for (int stage = 0; stage < maxDispersionStages; ++stage)
        {
            const auto next = _mm_sub_ps (_mm_add_ps (_mm_mul_ps (c, x), _mm_load_ps (dispersionPairIn[stage])),
                                          _mm_mul_ps (c, _mm_load_ps (dispersionPairState[stage])));
            _mm_store_ps (dispersionPairIn[stage], x);
            _mm_store_ps (dispersionPairState[stage], next);
            x = next;
        }
        alignas (16) float out[4];
        _mm_store_ps (out, x);
        vertical = out[0];
        horizontal = out[1];
       #else
        for (int lane = 0; lane < 2; ++lane)
        {
            auto& x = lane == 0 ? vertical : horizontal;
            for (int stage = 0; stage < maxDispersionStages; ++stage)
            {
                const auto next = pianoCoefficient * x + dispersionPairIn[stage][lane] - pianoCoefficient * dispersionPairState[stage][lane];
                dispersionPairIn[stage][lane] = x;
                dispersionPairState[stage][lane] = next;
                x = next;
            }
        }
       #endif
    }

    // The rest of the horizontal loop, after its allpasses.
    float finishHorizontal (float delayed, float drive, bool noteHeld)
    {
        const auto half = (int) buffer.size() / 2;
        horizontalLowState += (delayed - horizontalLowState) * horizontalLowpass;
        auto loopValue = horizontalLowState * horizontalFeedback;
        if (damper > 0.0f && ! noteHeld)
            loopValue *= 1.0f - damper * 0.16f;

        buffer[(size_t) (half + horizontalWrite)] = loopValue + drive * PianoTuning::get().aftersound;
        horizontalWrite = horizontalWrite + 1 < half ? horizontalWrite + 1 : (horizontalWrite + 1) % half;
        return delayed;
    }

    void resetHorizontal()
    {
        horizontalWrite = 0;
        horizontalThiranIn = horizontalThiranOut = horizontalLowState = 0.0f;
        for (auto& stage : dispersionPairIn) stage[1] = 0.0f;
        for (auto& stage : dispersionPairState) stage[1] = 0.0f;
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
    // Below the treble the felt's force rises fast and relaxes more slowly
    // (t e^-t): its spectrum falls smoothly with no nulls, as a real
    // hammer's does. In the treble it is a raised cosine a cycle long.
    float hammerForce (double elapsed) const
    {
        if (elapsed < 0.0)
            return 0.0f;

        if (hammerRaisedCosine)
        {
            if (elapsed >= (double) hammerLength)
                return 0.0f;
            const auto shape = std::sin (juce::MathConstants<double>::pi * elapsed / (double) hammerLength);
            return hammerLevel * (float) std::pow (shape, (double) hammerShape);
        }

        // Harder felt: a sharper peak (smaller time constant for the same
        // contact), set through hammerShape.
        const auto tau = (double) hammerLength / (2.0 + (double) hammerShape);
        const auto x = elapsed / tau;
        return x > 10.0 ? 0.0f : hammerLevel * 0.75f * (float) (x * std::exp (1.0 - x));
    }

    void updatePiano()
    {
        if (isPiano())
            piano.setParams (decay, damping, stiffness, hammerHardness, damper, excitationPosition, eco);
    }

public:
    // Each plugin instance starts the seed sequence over, so every instance
    // renders the same; the first instance in a process gets the seeds it
    // always had.
    static void restartSeeds() { seedCounter().store (0); }

private:
    static std::atomic<std::uint32_t>& seedCounter()
    {
        static std::atomic<std::uint32_t> counter { 0 };
        return counter;
    }

    static int nextSeed()
    {
        // Unsigned, so a long session wraps instead of overflowing an int
        // (undefined); the same seeds as before until then.
        return (int) (seedCounter().fetch_add (1) * 7919u + 12345u);
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
    double lowpassDelay = 0.0, lowpassDelayPeriod = -1.0;
    double cachedFrequency = -1.0, cachedSampleRate = -1.0, cachedPeriod = 1.0, cachedPhaseStep = 0.0;
    double cachedShortDelay = -1.0, cachedShortStages = -1.0;
    float cachedShortCoefficient = 0.0f;

    // A Thiran read's whole delay and allpass coefficient for one length.
    struct ThiranTap
    {
        double length = -1.0;
        int whole = 0;
        float a = 0.0f;
    };
    static ThiranTap thiranTapFor (double length)
    {
        ThiranTap tap;
        tap.length = length;
        tap.whole = (int) std::floor (length - 0.5);
        const auto part = length - (double) tap.whole; // 0.5 .. 1.5
        tap.a = (float) ((1.0 - part) / (1.0 + part));
        return tap;
    }
    ThiranTap verticalTap, horizontalTap;
    float lowpassDelayCoefficient = -1.0f;
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
    bool hammerRaisedCosine = false;
    float hammerReflection = 0.9f;
    float pianoCoefficient = 0.0f, pianoStiffness = -1.0f, pianoLowpass = -1.0f;
    double pianoLoopDelay = 0.0, pianoFrequency = 0.0;
    static constexpr int hammerHistorySize = 512;
    std::array<float, hammerHistorySize> hammerHistory {};
    int hammerHistoryWrite = 0;
    int knockLength = 0, thumpLength = 0;
    float knockDecay = 0.0f, knockCoefficient = 0.1f, knockLevel = 0.0f, knockEnvelope = 0.0f, knockLow1 = 0.0f, knockLow2 = 0.0f;
    float thumpDecay = 0.0f, thumpCoefficient = 0.02f, thumpLevel = 0.0f, thumpEnvelope = 0.0f, thumpLow1 = 0.0f, thumpLow2 = 0.0f;
    float pushLow = 0.0f, pushCoefficient = 0.001f, outputDcIn = 0.0f, outputDcOut = 0.0f;
    float thiranInput = 0.0f, thiranOutput = 0.0f;
    float horizontalLowpass = 0.5f, horizontalFeedback = 0.99f;
    float horizontalThiranIn = 0.0f, horizontalThiranOut = 0.0f, horizontalLowState = 0.0f;
    // The stiffness allpasses' state per stage: lane 0 the string (or the
    // vertical polarisation), lane 1 a piano string's horizontal one.
    alignas (16) float dispersionPairIn[maxDispersionStages][4] {};
    alignas (16) float dispersionPairState[maxDispersionStages][4] {};
    int horizontalWrite = 0;
    double horizontalLoopDelay = 0.0;
    Excite excite = Excite::Burst;
    ElectricPiano electric;
    PianoString piano;
    FeedbackLoop feedbackLoop;
    bool eco = false;
};
