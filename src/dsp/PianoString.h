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
        std::fill (std::begin (dispersionIn), std::end (dispersionIn), 0.0f);
        std::fill (std::begin (dispersionState), std::end (dispersionState), 0.0f);
        std::fill (std::begin (horizontalIn), std::end (horizontalIn), 0.0f);
        std::fill (std::begin (horizontalState), std::end (horizontalState), 0.0f);
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
    void setNote (double hz, int midiNote)
    {
        const auto lowest = sampleRate / ((double) size / 2.4);
        frequency = juce::jlimit (lowest, sampleRate * 0.45, hz);
        note = midiNote;
        designed = false;
    }

    // DECAY and DAMP (the string's knobs), STIFF, the HAMMER knob, DAMPER,
    // the excite position (0 = the register's own) and the quality.
    void setParams (float newDecay, float newDamp, float newStiffness, float newHardness, float newDamper,
                    float newStrikePosition, bool newEco)
    {
        if (newDecay != decay || newDamp != damp || newStiffness != stiffness || newStrikePosition != strikeKnob || newEco != eco)
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

    void trigger (float velocity, float randomUnit)
    {
        if (! designed)
            design();
        const auto& t = PianoModelTuning::get();
        const auto level = juce::jlimit (0.0f, 1.0f, velocity);

        // Key speed to hammer speed: about 0.5 m/s (pp) to 5 m/s (ff).
        hammerV = (double) t.speedLow * std::pow ((double) t.speedHigh / (double) t.speedLow, (double) level);
        // No two strikes are identical (and the strings of a note differ).
        hammerV *= 1.0 + 0.04 * ((double) randomUnit - 0.5);
        hammerY = stringY = 0.0;
        previousCompression = 0.0;
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
        // and F falls as it grows, so bisection finds the one root.
        auto force = 0.0;
        if (hammerActive)
        {
            const auto dt = 1.0 / sampleRate;
            const auto incoming = (double) aIn + (double) bIn;
            const auto predicted = (hammerY + dt * hammerV) - (stringY + dt * incoming);
            const auto give = dt * dt / hammerMass + dt / impedance;
            const auto hysteresisRate = hammerHysteresis * sampleRate;
            const auto feltForce = [&] (double f, double& shaped)
            {
                const auto compression = predicted - f * give;
                shaped = compression > 0.0 ? std::pow (compression / 0.001, hammerExponent) : 0.0;
                return hammerStiffness * (shaped + hysteresisRate * (shaped - previousCompression));
            };
            auto shaped = 0.0;
            const auto ceiling = feltForce (0.0, shaped);
            if (ceiling > 0.0)
            {
                auto low = 0.0, high = ceiling;
                for (int i = 0; i < 28; ++i)
                {
                    const auto middle = 0.5 * (low + high);
                    (feltForce (middle, shaped) > middle ? low : high) = middle;
                }
                force = 0.5 * (low + high);
                feltForce (force, shaped);
            }
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
        nutWrite = (nutWrite + 1) % nutSize;
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
        auto wave = readThiran (bridgeOffset, bridgeWrite, bridgeSize, length, bridgeThiranIn, bridgeThiranOut);
        bridgeWrite = (bridgeWrite + 1) % bridgeSize;
        for (int stage = 0; stage < stages; ++stage)
        {
            const auto next = dispersion * wave + dispersionIn[stage] - dispersion * dispersionState[stage];
            dispersionIn[stage] = wave;
            dispersionState[stage] = next;
            wave = next;
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
            forceHistoryWrite = (forceHistoryWrite + 1) % forceHistorySize;
            const auto drive = (push - forceHistory[(size_t) combRead]) * aftersound;

            auto h = readThiran (horizontalOffset, horizontalWrite, horizontalSize, horizontalLength, horizontalThiranIn, horizontalThiranOut);
            for (int stage = 0; stage < stages; ++stage)
            {
                const auto next = dispersion * h + horizontalIn[stage] - dispersion * horizontalState[stage];
                horizontalIn[stage] = h;
                horizontalState[stage] = next;
                h = next;
            }
            horizontalLossState += (h - horizontalLossState) * horizontalLossCoefficient;
            auto loop = horizontalLossState * horizontalLossGain;
            if (damper > 0.0f && ! noteHeld)
                loop *= 1.0f - damper * damperLoss;
            // One loop stands for both ends' reflections: no inversion.
            buffer[horizontalOffset + horizontalWrite] = loop + drive;
            horizontalWrite = (horizontalWrite + 1) % horizontalSize;
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
        dcOut = std::isfinite (blocked) ? blocked : 0.0f;
        if (! std::isfinite (lossState) || ! std::isfinite (horizontalLossState))
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

    float readThiran (int offset, int write, int lineSize, double length, float& thiranIn, float& thiranOut) const
    {
        const auto whole = (int) std::floor (length - 0.5);
        const auto part = length - (double) whole; // 0.5 .. 1.5
        auto read = write - whole;
        while (read < 0)
            read += lineSize;
        const auto input = buffer[offset + read % lineSize];
        const auto a = (float) ((1.0 - part) / (1.0 + part));
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
        const auto filterDelay = dispersionDelay + lowpassPhaseDelay (w0, (double) lossCoefficient);
        // (The wave back from the bridge meets the hammer on the next sample:
        // one sample of the loop is already there.)
        bridgeLength = juce::jmax (1.5, period - (double) nutDelay - filterDelay - 1.0);
        horizontalLength = juce::jmax (1.5, period * std::exp2 ((double) t.horizontalCents / 1200.0)
                                                - dispersionDelay - lowpassPhaseDelay (w0, (double) horizontalLossCoefficient));

        // Buffer layout: agraffe line, bridge line, horizontal loop.
        nutSize = juce::jmax (4, (int) (period * 0.5) + 4);
        bridgeSize = juce::jmax (4, (int) period + 4);
        horizontalSize = juce::jmax (4, (int) period + 4);
        nutOffset = 0;
        bridgeOffset = nutSize;
        horizontalOffset = nutSize + bridgeSize;
        if (horizontalOffset + horizontalSize > size)
            horizontalSize = juce::jmax (4, size - horizontalOffset);
        nutWrite %= nutSize;
        bridgeWrite %= bridgeSize;
        horizontalWrite %= horizontalSize;

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

    double sampleRate = 48000.0, frequency = 261.63, period = 183.0;
    int note = 60;
    float* buffer = nullptr;
    int size = 0;
    bool designed = false, eco = false;
    float decay = 0.5f, damp = 0.5f, stiffness = 0.5f, hardness = 0.5f, damper = 0.0f, strikeKnob = 0.0f;

    // Hammer.
    double hammerMass = 0.0087, hammerExponent = 2.5, hammerStiffness = 140.0, hammerHysteresis = 0.0;
    double impedance = 1.6, strikeFraction = 0.12, inharmonicity = 4.0e-4;
    double hammerY = 0.0, stringY = 0.0, hammerV = 0.0, previousCompression = 0.0;
    bool hammerActive = false;
    int contactSamples = 0;

    // Lines.
    int nutOffset = 0, nutSize = 4, nutWrite = 0, nutDelay = 1;
    int bridgeOffset = 4, bridgeSize = 4, bridgeWrite = 0;
    int horizontalOffset = 8, horizontalSize = 4, horizontalWrite = 0;
    double bridgeLength = 2.0, horizontalLength = 2.0;
    float bridgeThiranIn = 0.0f, bridgeThiranOut = 0.0f, horizontalThiranIn = 0.0f, horizontalThiranOut = 0.0f;
    int stages = maxStages;
    float dispersion = 0.0f;
    float dispersionIn[maxStages] {}, dispersionState[maxStages] {};
    float horizontalIn[maxStages] {}, horizontalState[maxStages] {};
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
