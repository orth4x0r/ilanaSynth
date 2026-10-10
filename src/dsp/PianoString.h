#pragma once

// M8.2: the piano string and its hammer, solved together.
//
// A felt hammer is a mass on a stiffening spring: F = K d^p (d, the felt's
// compression, p about 2.3 in the bass to 3 in the treble) plus a little
// hysteresis, pressed against the moving string at the strike point, sample
// by sample, until the string throws it back. Soft notes stay in contact
// longer and come out dark and round; loud ones compress the felt further,
// which stiffens it, so the spectrum brightens evenly with velocity. The
// strike-position notches and the treble's multiple contacts come out of
// the physics.
//
// The string is a digital waveguide split at the strike point: a short loop
// towards the agraffe (a rigid, inverting end) and a long one towards the
// bridge, where the loss filter (T60 in seconds, loss rising with frequency
// squared) and the stiffness allpasses sit. Two polarisations: the vertical
// one takes the hammer and drives the bridge hard (the loud prompt sound),
// the horizontal one gets a share and rings on (the aftersound). Bass
// strings get tension modulation: loud notes start a little sharp and fall
// back (the pitch glide of the "bark"), and the square of the motion drives
// longitudinal modes, which bring the phantom partials.
//
// It runs in the KarplusStrong's buffer (no allocation), sized for 15 Hz.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <complex>
#include <limits>

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_PIANO_SSE 1
 #include <xmmintrin.h>
#else
 #define ILANA_PIANO_SSE 0
#endif

#include "PianoModelTuning.h"

class PianoString
{
public:
    static constexpr int maxStages = 8;

    void prepare (double newSampleRate, float* storage, int storageSize)
    {
        sampleRate = newSampleRate;
        buffer = storage;
        size = storageSize;
        reset();
    }

    void reset()
    {
        if (buffer != nullptr)
            std::fill (buffer, buffer + size, 0.0f);
        nutWrite = bridgeWrite = horizontalWrite = 0;
        bridgeThiranIn = bridgeThiranOut = horizontalThiranIn = horizontalThiranOut = 0.0f;
        for (int stage = 0; stage < maxStages; ++stage)
            for (int lane = 0; lane < 4; ++lane)
                chainIn[stage][lane] = chainState[stage][lane] = 0.0f;
        lossState = horizontalLossState = 0.0f;
        bridgeReturn = 0.0f;
        bridgeInput = 0.0f;
        hammerActive = false;
        hammerY = stringY = hammerV = 0.0;
        previousCompression = 0.0;
        energy = 0.0f;
        for (auto& mode : longitudinal)
            mode.reset();
        squareDcIn = squareDcOut = 0.0f;
        dcIn = dcOut = 0.0f;
        knockEnvelope = 0.0f;
        knockLow1 = knockLow2 = 0.0f;
        forceHistoryWrite = 0;
        std::fill (forceHistory.begin(), forceHistory.end(), 0.0f);
    }

    // The sounding frequency (unison detune included) and the MIDI note it
    // belongs to (the register sets the hammer and string constants).
    //
    // Drift, vibrato and detune move the pitch a little every sub-block:
    // within retuneRange of the designed pitch only the loop lengths follow
    // (the filters barely change); further off, the whole string is redesigned.
    void setNote (double hz, int midiNote)
    {
        const auto lowest = sampleRate / ((double) size / 2.4);
        const auto newFrequency = juce::jlimit (lowest, sampleRate * 0.45, hz);
        if (newFrequency == frequency)
            return;
        frequency = newFrequency;
        if (designed && std::abs (frequency / designedFrequency - 1.0) < retuneRange)
        {
            retune();
            return;
        }
        note = midiNote;
        designed = false;
    }

    // DECAY and DAMP (the string's knobs), STIFF, the HAMMER knob, DAMPER,
    // the excite position (0 = the register's own) and the quality.
    void setParams (float newDecay, float newDamp, float newStiffness, float newHardness, float newDamper,
                    float newStrikePosition, bool newEco)
    {
        if (newDecay != decay || newDamp != damp || newStiffness != stiffness || newStrikePosition != strikeKnob || newEco != eco
            || newHardness != hardness)
            designed = false;
        decay = newDecay;
        damp = newDamp;
        stiffness = newStiffness;
        hardness = newHardness;
        damper = newDamper;
        strikeKnob = newStrikePosition;
        eco = newEco;
    }

    void addBridgeInput (float value) { bridgeInput += value; }

    // (The design waits for the first process(): a voice triggers all its
    // buffered unison strings, and only the ones in use are ever played.)
    void trigger (float velocity, float randomUnit)
    {
        const auto& t = PianoModelTuning::get();
        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        // Key speed to hammer speed: about 0.5 m/s (pp) to 5 m/s (ff).
        hammerV = (double) t.speedLow * std::pow ((double) t.speedHigh / (double) t.speedLow, (double) level);
        // No two strikes are identical (and the strings of a note differ).
        hammerV *= 1.0 + 0.04 * ((double) randomUnit - 0.5);
        hammerY = stringY = 0.0;
        previousCompression = 0.0;
        lastHammerForce = 0.0;
        hammerActive = true;
        contactSamples = 0;

        // The board's knock and the felt's thud, heard directly.
        knockEnvelope = (float) (hammerV / (double) t.speedHigh);
        knockEnvelope *= knockEnvelope;
    }

    float process (bool noteHeld)
    {
        if (buffer == nullptr)
            return 0.0f;
        if (! designed)
            design();

        // Waves arriving at the strike point: from the agraffe (inverted),
        // and back from the bridge (through its filters, already inverted).
        auto nutRead = nutWrite - nutDelay;
        if (nutRead < 0)
            nutRead += nutSize;
        const auto aIn = -buffer[nutOffset + nutRead];
        const auto bIn = bridgeReturn + bridgeInput;
        bridgeInput = 0.0f;

        // The hammer, solved implicitly each sample: the felt is far stiffer
        // than one sample can follow explicitly against the string's
        // impedance. With the force F over this sample,
        //   compression = predicted - F (dt^2 / m + dt / Z)
        //   F = K (s + a ds/dt),  s = (compression / 1 mm)^p
        // and F falls as it grows, so there is one root in [0, F(0)]. F - f
        // is convex, so Newton's method (from the last sample's force, kept
        // inside the bracket) gets there in a few steps.
        auto force = 0.0;
        if (hammerActive)
        {
            const auto dt = 1.0 / sampleRate;
            const auto incoming = (double) aIn + (double) bIn;
            const auto predicted = (hammerY + dt * hammerV) - (stringY + dt * incoming);
            const auto give = dt * dt / hammerMass + dt / impedance;
            const auto hysteresisRate = hammerHysteresis * sampleRate;
            // F = K (s + h (s - s_prev)) = scale s - memory.
            const auto scale = hammerStiffness * (1.0 + hysteresisRate);
            const auto memory = hammerStiffness * hysteresisRate * previousCompression;
            const auto shapedAt = [&] (double f)
            {
                const auto compression = predicted - f * give;
                return compression > 0.0 ? std::pow (compression / 0.001, hammerExponent) : 0.0;
            };
            auto shaped = shapedAt (0.0);
            const auto ceiling = scale * shaped - memory;
            if (ceiling > 0.0)
            {
                auto low = 0.0, high = ceiling;
                auto f = juce::jlimit (0.0, ceiling, lastHammerForce);
                for (int i = 0; i < 40; ++i)
                {
                    const auto compression = predicted - f * give;
                    shaped = shapedAt (f);
                    const auto g = scale * shaped - memory - f;
                    (g > 0.0 ? low : high) = f;
                    const auto slope = (compression > 0.0 ? -scale * give * hammerExponent * shaped / compression : 0.0) - 1.0;
                    auto next = f - g / slope;
                    if (! (next > low && next < high))
                        next = 0.5 * (low + high);
                    const auto step = std::abs (next - f);
                    f = next;
                    if (step <= ceiling * 1.0e-10)
                        break;
                }
                force = f;
                shaped = shapedAt (force);
            }
            lastHammerForce = force;
            previousCompression = shaped;

            const auto stringVelocity = incoming + force / impedance;
            stringY += stringVelocity * dt;
            hammerV -= force / hammerMass * dt;
            hammerY += hammerV * dt;
            ++contactSamples;

            // Thrown back and clear of the string: done (it can land again
            // while it is still moving forward, as in the treble).
            if (hammerV < 0.0 && hammerY < stringY - 0.002)
                hammerActive = false;
            if (contactSamples > (int) (0.05 * sampleRate))
                hammerActive = false;
        }

        const auto push = (float) (force / (2.0 * impedance));
        const auto aOut = bIn + push;
        const auto bOut = aIn + push;
        buffer[nutOffset + nutWrite] = aOut;
        if (++nutWrite >= nutSize)
            nutWrite = 0;
        buffer[bridgeOffset + bridgeWrite] = bOut;

        // Tension modulation: a loud string is stretched, so it runs sharp.
        auto length = bridgeLength;
        if (tensionAmount > 0.0f)
        {
            // Normalised to the loudest strike's wave (the hammer's top
            // speed), so ff in the bass starts a few cents sharp.
            energy += (bOut * bOut - energy) * energyCoefficient;
            length -= (double) (tensionAmount * energy * energyScale) * period;
            length = juce::jmax (2.0, length);
        }

        // The long loop: the fractional delay (Thiran), stiffness, loss.
        auto wave = tensionAmount > 0.0f ? readThiran (bridgeOffset, bridgeWrite, bridgeSize, length, bridgeThiranIn, bridgeThiranOut)
                                         : readThiran (bridgeOffset, bridgeWrite, bridgeSize, bridgeTap, bridgeThiranIn, bridgeThiranOut);
        if (++bridgeWrite >= bridgeSize)
            bridgeWrite = 0;
        // The horizontal polarisation reads its own loop (nothing below
        // writes it first), so both run their stiffness allpasses together:
        // lane 0 vertical, lane 1 horizontal, the same arithmetic per lane.
        auto h = 0.0f;
        if (! eco)
        {
            h = readThiran (horizontalOffset, horizontalWrite, horizontalSize, horizontalTap, horizontalThiranIn, horizontalThiranOut);
            dispersePair (wave, h);
        }
        else
        {
            for (int stage = 0; stage < stages; ++stage)
            {
                const auto next = dispersion * wave + chainIn[stage][0] - dispersion * chainState[stage][0];
                chainIn[stage][0] = wave;
                chainState[stage][0] = next;
                wave = next;
            }
        }
        lossState += (wave - lossState) * lossCoefficient;
        auto atBridge = lossState * lossGain;
        if (damper > 0.0f && ! noteHeld)
            atBridge *= 1.0f - damper * damperLoss;
        bridgeReturn = -atBridge;

        auto output = atBridge;

        // The horizontal polarisation: the same string, driven by a share of
        // the strike through the strike-point comb, losing energy slowly.
        if (! eco)
        {
            forceHistory[(size_t) forceHistoryWrite] = push;
            auto combRead = forceHistoryWrite - nutDelay;
            if (combRead < 0)
                combRead += forceHistorySize;
            forceHistoryWrite = forceHistoryWrite + 1 < forceHistorySize ? forceHistoryWrite + 1 : (forceHistoryWrite + 1) % forceHistorySize;
            const auto drive = (push - forceHistory[(size_t) combRead]) * aftersound;

            horizontalLossState += (h - horizontalLossState) * horizontalLossCoefficient;
            auto loop = horizontalLossState * horizontalLossGain;
            if (damper > 0.0f && ! noteHeld)
                loop *= 1.0f - damper * damperLoss;
            // One loop stands for both ends' reflections: no inversion.
            buffer[horizontalOffset + horizontalWrite] = loop + drive;
            if (++horizontalWrite >= horizontalSize)
                horizontalWrite = 0;
            output += loop * horizontalMix;

            // Phantom partials: the longitudinal modes, driven by the square
            // of the motion (sum and difference tones of the partials).
            if (longitudinalGain > 0.0f)
            {
                const auto square = bOut * bOut;
                const auto blocked = square - squareDcIn + 0.999f * squareDcOut;
                squareDcIn = square;
                squareDcOut = blocked;
                auto sum = 0.0f;
                for (int m = 0; m < numLongitudinal; ++m)
                    sum += longitudinal[(size_t) m].process (blocked) * (m == 0 ? 1.0f : 0.6f / (float) m);
                output += sum * longitudinalGain;
            }
        }

        // The knock: band-passed noise, a few milliseconds, louder and
        // brighter for a faster hammer.
        if (knockEnvelope > 1.0e-5f)
        {
            noiseSeed = noiseSeed * 1664525u + 1013904223u;
            const auto noise = (float) ((double) noiseSeed / 4294967295.0 * 2.0 - 1.0);
            knockLow1 += (noise - knockLow1) * knockCoefficient;
            knockLow2 += (knockLow1 - knockLow2) * knockCoefficient;
            output += (knockLow1 - knockLow2) * knockEnvelope * knockLevel;
            knockEnvelope *= knockDecay;
        }

        // DC (the hammer leaves the string displaced for a moment).
        const auto blocked = output - dcIn + 0.9987f * dcOut;
        dcIn = output;
        dcOut = isFinite (blocked) ? blocked : 0.0f;
        if (! isFinite (lossState) || ! isFinite (horizontalLossState))
            reset();
        return juce::jlimit (-8.0f, 8.0f, dcOut * outputGain);
    }

    // Register constants, for tests and the display.
    double getHammerMass() const { return hammerMass; }
    double getImpedance() const { return impedance; }
    double getStrikeFraction() const { return strikeFraction; }
    bool isHammerInContact() const { return hammerActive; }
    double getInharmonicity() const { return inharmonicity; }

private:
    // A two-pole resonator with unity peak gain.
    struct Resonator
    {
        float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f, y1 = 0.0f, y2 = 0.0f, x1 = 0.0f, x2 = 0.0f;
        void set (double rate, double hz, double q)
        {
            const auto w = juce::MathConstants<double>::twoPi * juce::jmin (hz, rate * 0.45) / rate;
            const auto r = std::exp (-w / (2.0 * q));
            a1 = (float) (-2.0 * r * std::cos (w));
            a2 = (float) (r * r);
            b0 = (float) ((1.0 - r * r) * 0.5);
        }
        void reset() { y1 = y2 = x1 = x2 = 0.0f; }
        float process (float x)
        {
            const auto y = b0 * (x - x2) - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // std::isfinite is a library call here; NaN and infinity both fail this.
    static bool isFinite (float x) { return std::abs (x) <= std::numeric_limits<float>::max(); }

    // A delay of `length` samples: `whole` from the line, the rest (0.5 ..
    // 1.5) from a first-order Thiran allpass with coefficient `a`.
    struct ThiranTap
    {
        int whole = 1;
        float a = 0.0f;
    };

    static ThiranTap thiranTap (double length)
    {
        const auto whole = (int) std::floor (length - 0.5);
        const auto part = length - (double) whole;
        return { whole, (float) ((1.0 - part) / (1.0 + part)) };
    }

    float readThiran (int offset, int write, int lineSize, double length, float& thiranIn, float& thiranOut) const
    {
        return readThiran (offset, write, lineSize, thiranTap (length), thiranIn, thiranOut);
    }

    float readThiran (int offset, int write, int lineSize, ThiranTap tap, float& thiranIn, float& thiranOut) const
    {
        auto read = write - tap.whole;
        while (read < 0)
            read += lineSize;
        const auto input = buffer[offset + read]; // write - whole: within one line length
        const auto a = tap.a;
        const auto out = a * input + thiranIn - a * thiranOut;
        thiranIn = input;
        thiranOut = out;
        return out;
    }

    static double allpassPhaseDelay (double a, double w)
    {
        const auto e = std::polar (1.0, -w);
        return -std::arg ((a + e) / (1.0 + a * e)) / w;
    }

    static double lowpassPhaseDelay (double w, double a)
    {
        return -std::arg (a / (1.0 - (1.0 - a) * std::polar (1.0, -w))) / w;
    }

    // Loss filter: a one-pole low-pass with gain, for a fundamental T60 and
    // a T60 at `upper` Hz (seconds, not per pass).
    void designLoss (double fundamentalT60, double upperT60, double upper, float& coefficient, float& gain) const
    {
        const auto passGain = [this] (double seconds) { return std::pow (10.0, -3.0 / (seconds * frequency)); };
        const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto w1 = juce::MathConstants<double>::twoPi * upper / sampleRate;
        const auto magnitude = [] (double pole, double w) { return (1.0 - pole) / std::sqrt (1.0 - 2.0 * pole * std::cos (w) + pole * pole); };
        const auto wanted = passGain (juce::jmin (upperT60, fundamentalT60 * 0.95)) / passGain (fundamentalT60);
        auto low = 0.0, high = 0.999;
        for (int i = 0; i < 40; ++i)
        {
            const auto pole = 0.5 * (low + high);
            (magnitude (pole, w1) / magnitude (pole, w0) > wanted ? low : high) = pole;
        }
        const auto pole = 0.5 * (low + high);
        coefficient = (float) (1.0 - pole);
        gain = (float) juce::jmin (0.99995, passGain (fundamentalT60) / magnitude (pole, w0));
    }

    // The loop lengths for the current pitch, with the filters' delays from
    // the last design. (The wave back from the bridge meets the hammer on the
    // next sample: one sample of the loop is already there.)
    void retune()
    {
        period = sampleRate / frequency;
        bridgeLength = juce::jlimit (1.5, (double) bridgeSize - 2.0, period - (double) nutDelay - bridgeFilterDelay - 1.0);
        horizontalLength = juce::jlimit (1.5, (double) horizontalSize - 2.0, period * horizontalRatio - horizontalFilterDelay);
        bridgeTap = thiranTap (bridgeLength);
        horizontalTap = thiranTap (horizontalLength);
    }

    void design()
    {
        designed = true;
        const auto& t = PianoModelTuning::get();
        period = sampleRate / frequency;
        // 0 at A0 .. 1 at C8.
        const auto x = juce::jlimit (0.0, 1.0, ((double) note - 21.0) / 87.0);
        const auto octavesFromC4 = std::log2 (frequency / 261.63);
        const auto lerp = [x] (double a, double b) { return a + (b - a) * x; };
        const auto logLerp = [x] (double a, double b) { return a * std::pow (b / a, x); };

        // Hammer: heavier, softer felt in the bass; HAMMER voices the felt.
        hammerMass = logLerp ((double) t.massBass, (double) t.massTreble) * 0.001;
        hammerExponent = lerp ((double) t.exponentBass, (double) t.exponentTreble);
        hammerStiffness = logLerp ((double) t.feltBass, (double) t.feltTreble) * std::pow (4.0, ((double) hardness - 0.5) * 2.0 * (double) t.hardnessRange);
        hammerHysteresis = (double) t.hysteresis * 1.0e-4;
        // String impedance sqrt(T mu): heavy wound strings in the bass.
        impedance = octavesFromC4 < 0.0 ? (double) t.impedanceC4 * std::pow ((double) t.impedanceBass / (double) t.impedanceC4, juce::jmin (1.0, -octavesFromC4 / 3.25))
                                        : (double) t.impedanceC4;

        // Strike point, from the agraffe: about 1/8, closer in the treble.
        strikeFraction = strikeKnob > 0.0f ? (double) strikeKnob * 0.5 : lerp ((double) t.strikeBass, (double) t.strikeTreble);

        // Inharmonicity: B rises towards the treble (short, thick strings);
        // STIFF sets the C4 value.
        inharmonicity = (double) t.inharmonicityC4 * std::pow (10.0, ((double) stiffness - 0.5) * 2.0)
                        * std::pow (2.0, octavesFromC4 * (octavesFromC4 > 0.0 ? (double) t.inharmonicityTreble : -(double) t.inharmonicityBass));
        stages = eco ? maxStages / 2 : maxStages;

        // Decay: DECAY sets the fundamental's T60 at C4; it shortens up the
        // keyboard. DAMP sets how fast the upper partials go (T60 at 3 kHz).
        const auto t60 = juce::jlimit (0.2, 60.0, (double) t.t60C4 * std::pow (4.0, ((double) decay - 0.5) * 2.0)
                                                      * std::pow (261.63 / frequency, (double) t.t60Register));
        const auto upper = juce::jlimit (frequency * 1.5, sampleRate * 0.4, 3000.0);
        const auto upperT60 = (double) t.t60Upper * std::pow (8.0, (0.5 - (double) damp) * 2.0);
        designLoss (juce::jmax (0.1, t60 * (double) t.promptRatio), upperT60 * (double) t.promptRatio, upper, lossCoefficient, lossGain);
        designLoss (t60, upperT60, upper, horizontalLossCoefficient, horizontalLossGain);

        // Stiffness allpasses, solved so a reference partial lands at
        // n sqrt(1 + B n^2) / sqrt(1 + B).
        const auto w0 = juce::MathConstants<double>::twoPi / period;
        dispersion = 0.0f;
        auto dispersionDelay = 0.0;
        const auto reference = juce::jmin (12, (int) (0.3 * sampleRate / frequency));
        if (reference >= 2 && period >= 8.0)
        {
            const auto n = (double) reference;
            const auto wTarget = n * w0 * std::sqrt (1.0 + inharmonicity * n * n) / std::sqrt (1.0 + inharmonicity);
            const auto needed = juce::MathConstants<double>::twoPi * n / wTarget - period;
            const auto error = [&] (double a) { return (double) stages * (allpassPhaseDelay (a, wTarget) - allpassPhaseDelay (a, w0)) - needed; };
            auto low = -0.95, high = 0.0;
            if (error (low) > 0.0)
                high = low;
            else
                for (int i = 0; i < 30; ++i)
                {
                    const auto middle = 0.5 * (low + high);
                    (error (middle) > 0.0 ? high : low) = middle;
                }
            auto a = high;
            while (a < -0.01 && (double) stages * allpassPhaseDelay (a, w0) > period * 0.6)
                a *= 0.8;
            dispersion = (float) a;
            dispersionDelay = (double) stages * allpassPhaseDelay (a, w0);
        }

        // Split the loop: the strike point's round trip to the agraffe is
        // the short line; the rest (minus the filters' delay) the long one.
        nutDelay = juce::jlimit (1, (int) (period * 0.45), (int) std::round (period * strikeFraction));
        bridgeFilterDelay = dispersionDelay + lowpassPhaseDelay (w0, (double) lossCoefficient);
        horizontalFilterDelay = dispersionDelay + lowpassPhaseDelay (w0, (double) horizontalLossCoefficient);
        horizontalRatio = std::exp2 ((double) t.horizontalCents / 1200.0);

        // Buffer layout: agraffe line, bridge line, horizontal loop (the long
        // lines leave room for a retune downwards).
        const auto retuneRoom = 1.0 + retuneRange;
        nutSize = juce::jmax (4, (int) (period * 0.5) + 4);
        bridgeSize = juce::jmax (4, (int) (period * retuneRoom) + 4);
        horizontalSize = juce::jmax (4, (int) (period * retuneRoom) + 4);
        nutOffset = 0;
        bridgeOffset = nutSize;
        horizontalOffset = nutSize + bridgeSize;
        if (horizontalOffset + horizontalSize > size)
            horizontalSize = juce::jmax (4, size - horizontalOffset);
        nutWrite %= nutSize;
        bridgeWrite %= bridgeSize;
        horizontalWrite %= horizontalSize;
        designedFrequency = frequency;
        retune();

        aftersound = t.aftersound;
        horizontalMix = t.horizontalMix;
        damperLoss = 0.16f;

        // The bass bark: tension modulation and longitudinal modes, fading
        // out by the tenor.
        const auto bass = (float) juce::jlimit (0.0, 1.0, -octavesFromC4 / 2.5);
        tensionAmount = t.tensionModulation * bass;
        energyCoefficient = (float) (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));
        energyScale = 1.0f / juce::jmax (0.01f, 0.25f * t.speedHigh * t.speedHigh);
        longitudinalGain = eco ? 0.0f : t.longitudinal * bass;
        const auto longitudinalHz = frequency * (double) t.longitudinalRatio;
        for (int m = 0; m < numLongitudinal; ++m)
            longitudinal[(size_t) m].set (sampleRate, longitudinalHz * (m + 1), 60.0);

        // Knock and level.
        knockLevel = t.knock * (float) std::pow (2.0, octavesFromC4 * (double) t.knockRegister);
        const auto knockHz = juce::jlimit (300.0, sampleRate * 0.4, (double) t.knockFrequency * std::pow (2.0, octavesFromC4 * 0.5));
        knockCoefficient = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * knockHz / sampleRate));
        knockDecay = (float) std::exp (-1.0 / (sampleRate * 0.004 * std::pow (2.0, -octavesFromC4 * 0.3)));
        outputGain = t.outputGain * (float) std::pow (2.0, octavesFromC4 * (double) t.balance);
    }

    // A redesign every 3 %, half a semitone.
    static constexpr double retuneRange = 0.03;

    double sampleRate = 48000.0, frequency = 261.63, period = 183.0, designedFrequency = 261.63;
    double bridgeFilterDelay = 0.0, horizontalFilterDelay = 0.0, horizontalRatio = 1.0;
    int note = 60;
    float* buffer = nullptr;
    int size = 0;
    bool designed = false, eco = false;
    float decay = 0.5f, damp = 0.5f, stiffness = 0.5f, hardness = 0.5f, damper = 0.0f, strikeKnob = 0.0f;

    // Hammer.
    double hammerMass = 0.0087, hammerExponent = 2.5, hammerStiffness = 140.0, hammerHysteresis = 0.0;
    double impedance = 1.6, strikeFraction = 0.12, inharmonicity = 4.0e-4;
    double hammerY = 0.0, stringY = 0.0, hammerV = 0.0, previousCompression = 0.0, lastHammerForce = 0.0;
    bool hammerActive = false;
    int contactSamples = 0;

    // Lines.
    int nutOffset = 0, nutSize = 4, nutWrite = 0, nutDelay = 1;
    int bridgeOffset = 4, bridgeSize = 4, bridgeWrite = 0;
    int horizontalOffset = 8, horizontalSize = 4, horizontalWrite = 0;
    double bridgeLength = 2.0, horizontalLength = 2.0;
    ThiranTap bridgeTap, horizontalTap; // for the lengths above (retune)
    float bridgeThiranIn = 0.0f, bridgeThiranOut = 0.0f, horizontalThiranIn = 0.0f, horizontalThiranOut = 0.0f;
    int stages = maxStages;
    float dispersion = 0.0f;
    // Per stage: lane 0 the vertical polarisation, lane 1 the horizontal.
    alignas (16) float chainIn[maxStages][4] {};
    alignas (16) float chainState[maxStages][4] {};

    void dispersePair (float& vertical, float& horizontal) noexcept
    {
       #if ILANA_PIANO_SSE
        const auto coefficient = _mm_set1_ps (dispersion);
        auto x = _mm_set_ps (0.0f, 0.0f, horizontal, vertical);
        for (int stage = 0; stage < stages; ++stage)
        {
            const auto next = _mm_sub_ps (_mm_add_ps (_mm_mul_ps (coefficient, x), _mm_load_ps (chainIn[stage])),
                                          _mm_mul_ps (coefficient, _mm_load_ps (chainState[stage])));
            _mm_store_ps (chainIn[stage], x);
            _mm_store_ps (chainState[stage], next);
            x = next;
        }
        alignas (16) float out[4];
        _mm_store_ps (out, x);
        vertical = out[0];
        horizontal = out[1];
       #else
        for (int stage = 0; stage < stages; ++stage)
            for (int lane = 0; lane < 2; ++lane)
            {
                auto& x = lane == 0 ? vertical : horizontal;
                const auto next = dispersion * x + chainIn[stage][lane] - dispersion * chainState[stage][lane];
                chainIn[stage][lane] = x;
                chainState[stage][lane] = next;
                x = next;
            }
       #endif
    }
    float lossCoefficient = 0.5f, lossGain = 0.99f, lossState = 0.0f;
    float horizontalLossCoefficient = 0.5f, horizontalLossGain = 0.99f, horizontalLossState = 0.0f;
    float bridgeReturn = 0.0f, bridgeInput = 0.0f;
    float aftersound = 0.3f, horizontalMix = 1.0f, damperLoss = 0.16f;

    static constexpr int forceHistorySize = 4096;
    std::array<float, forceHistorySize> forceHistory {};
    int forceHistoryWrite = 0;

    // Bark.
    float tensionAmount = 0.0f, energy = 0.0f, energyCoefficient = 0.01f, energyScale = 1.0f;
    static constexpr int numLongitudinal = 3;
    std::array<Resonator, numLongitudinal> longitudinal;
    float longitudinalGain = 0.0f, squareDcIn = 0.0f, squareDcOut = 0.0f;

    // Knock and output.
    float knockEnvelope = 0.0f, knockLevel = 0.0f, knockDecay = 0.99f, knockCoefficient = 0.1f, knockLow1 = 0.0f, knockLow2 = 0.0f;
    std::uint32_t noiseSeed = 22222u;
    float dcIn = 0.0f, dcOut = 0.0f, outputGain = 0.1f;
};
