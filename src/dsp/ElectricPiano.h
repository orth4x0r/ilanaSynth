#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

#include "EpTuning.h"

// M7.3 electric pianos: a struck tine (Rhodes-style) or reed (Wurlitzer-
// style), heard through its pickup.
//
// The tine or reed is a small bank of decaying modes: the fundamental, the
// tone bar (a slower mode at the same pitch), a little of twice the pitch
// (the tip moves on an arc), and the clamped-free beam modes at about 6.27
// and 17.55 times the pitch, which give the attack its ping. The hammer
// pushes on every mode for its contact time (a half-sine force), so harder,
// shorter hits reach further up.
//
// The pickup is where the character comes from. It reads the rate of change
// of a nonlinear function of the displacement:
// - Tine: the magnetic flux of a pole piece next to the tine's tip, a bump
//   1 / (1 + ((x - offset) / d)^2). Swinging further than d across the pole
//   folds the wave over: the bark.
// - Reed: the capacitance of the reed inside the pickup slot, 1 / (1 - x / d)
//   (softly limited before it reaches the plate), then the preamp's input
//   high-pass and an asymmetric soft clip: the growl.
class ElectricPiano
{
public:
    enum class Model { Tine = 0, Reed };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
        updateModes();
    }

    void reset()
    {
        for (auto& mode : modes)
            mode.re = mode.im = 0.0f;
        hammerSample = hammerLength = 0;
        lastPickup = 0.0f;
        tone = highPassIn = highPassOut = 0.0f;
        released = false;
        primed = false;
    }

    void setModel (Model newModel)
    {
        if (model != newModel)
        {
            model = newModel;
            dirty = true;
        }
    }

    // The Physical card's knobs, 0..1: DECAY and DAMP (the tone), pickup
    // distance and position, hammer hardness, and the damper.
    void setParams (float newDecay, float newDamping, float newDistance, float newPosition,
                    float newHardness, float newDamper)
    {
        if (newDecay != decay || newDamping != damping || newDistance != distanceKnob
            || newPosition != positionKnob || newHardness != hardness || newDamper != damper)
        {
            decay = newDecay;
            damping = newDamping;
            distanceKnob = newDistance;
            positionKnob = newPosition;
            hardness = newHardness;
            damper = newDamper;
            dirty = true;
        }
    }

    void setFrequency (double hz)
    {
        if (std::abs (hz - frequency) > frequency * 1.0e-6)
        {
            frequency = juce::jlimit (15.0, sampleRate * 0.45, hz);
            dirty = true;
        }
    }

    void trigger (float velocity)
    {
        if (dirty)
            updateModes();
        const auto& t = tuning();
        velocityLevel = juce::jlimit (0.0f, 1.0f, velocity);
        released = false;
        applyDecay();

        // Contact time: shorter for harder hammers, faster keys and higher
        // notes.
        const auto note = noteOf (frequency);
        auto contact = t.contactMs * 0.001f
                       * std::pow (2.0f, -(note - 60.0f) / 12.0f * t.contactRegister)
                       * (1.0f + t.contactVelocity * (0.5f - velocityLevel))
                       * (1.0f + t.contactHardness * (0.5f - hardness));
        contact = juce::jlimit (0.0002f, 0.02f, contact);
        hammerLength = juce::jmax (2, (int) (contact * (float) sampleRate));
        hammerSample = 0;

        // Displacement at full velocity, per register, then the velocity
        // curve. The force's sum over the contact is normalised so each mode
        // reaches its level (less the contact's own low-pass).
        const auto full = std::exp (std::log (t.ampLow) + (std::log (t.ampHigh) - std::log (t.ampLow))
                                     * (note - 40.0f) / 44.0f);
        const auto amplitude = full * (t.velocityFloor + (1.0f - t.velocityFloor)
                                                             * std::pow (velocityLevel, t.velocityCurve));
        forceScale = amplitude * juce::MathConstants<float>::pi * 0.5f / (float) hammerLength;
        // Retriggering a ringing tine keeps its motion (as the real one does).
        if (! primed)
            lastPickup = pickupOf (0.0f);
        primed = true;
    }

    // M7.5: live audio pushing on the tine or reed (added on the next sample).
    void addForce (float value)
    {
        liveForce += value;
        if (! primed && value != 0.0f)
        {
            if (dirty)
                updateModes();
            lastPickup = pickupOf (0.0f);
            primed = true;
        }
    }

    float process (bool noteHeld)
    {
        if (! primed)
            return 0.0f;
        if (dirty)
            updateModes();
        if (! noteHeld && ! released)
        {
            released = true;
            applyDecay();
        }

        auto force = liveForce;
        liveForce = 0.0f;
        if (hammerSample < hammerLength)
        {
            force = forceScale * std::sin (juce::MathConstants<float>::pi * ((float) hammerSample + 0.5f) / (float) hammerLength);
            ++hammerSample;
        }

        auto x = 0.0f;
        for (int i = 0; i < activeModes; ++i)
        {
            auto& mode = modes[(size_t) i];
            const auto re = mode.re * mode.cr - mode.im * mode.ci + force * mode.drive;
            const auto im = mode.re * mode.ci + mode.im * mode.cr;
            mode.re = re;
            mode.im = im;
            x += im;
        }

        const auto& t = tuning();
        const auto pickup = pickupOf (x);
        auto y = (pickup - lastPickup) * derivativeScale;
        lastPickup = pickup;

        if (model == Model::Reed)
        {
            // The preamp's input network and its asymmetric soft clip.
            highPassOut = highPassCoefficient * (highPassOut + y - highPassIn);
            highPassIn = y;
            y = highPassOut;
            if (t.drive > 0.0f)
                y = (std::tanh (t.drive * y + t.bias) - biasOffset) / t.drive;
        }

        tone += toneCoefficient * (y - tone);
        return tone * outputGain;
    }

private:
    struct Mode
    {
        float re = 0.0f, im = 0.0f;
        float cr = 1.0f, ci = 0.0f; // the rotation, including the decay
        float drive = 0.0f;
        float angle = 0.0f, t60 = 1.0f;
    };

    const EpModelTuning& tuning() const
    {
        return model == Model::Tine ? EpTuning::get().tine : EpTuning::get().reed;
    }

    float pickupOf (float x) const
    {
        if (model == Model::Tine)
        {
            const auto u = (x - pickupOffset) * inverseDistance;
            return 1.0f / (1.0f + u * u);
        }
        auto z = (x + pickupOffset) * inverseDistance;
        if (z > 0.8f)
            z = 0.8f + 0.17f * std::tanh ((z - 0.8f) / 0.17f);
        return 1.0f / (1.0f - z);
    }

    static float noteOf (double hz) { return 69.0f + 12.0f * (float) std::log2 (hz / 440.0); }

    void updateModes()
    {
        dirty = false;
        const auto& t = tuning();
        const auto note = noteOf (frequency);
        const auto octaves = (note - 60.0f) / 12.0f;
        const auto registerScale = std::pow (2.0f, -octaves * t.t60Register);
        // DECAY: its default (0.75) is the fitted sustain; each step of 0.25
        // doubles or halves it.
        const auto decayScale = std::pow (2.0f, (decay - 0.75f) * 4.0f);

        const struct { float ratio, level, t60; } table[] {
            { 1.0f, 1.0f, t.t60 },
            { std::pow (2.0f, t.barDetune / 1200.0f), t.barLevel, t.t60 * t.barT60 },
            { 2.0f, t.h2Level, t.t60 * t.h2T60 },
            { t.beam2Ratio, t.beam2Level, t.beam2T60 },
            { t.beam3Ratio, t.beam3Level, t.beam3T60 },
        };

        activeModes = 0;
        for (const auto& entry : table)
        {
            const auto hz = frequency * entry.ratio;
            if (hz >= sampleRate * 0.45 || entry.level == 0.0f)
                continue;
            auto& mode = modes[(size_t) activeModes++];
            mode.angle = (float) (juce::MathConstants<double>::twoPi * hz / sampleRate);
            // DAMP: the higher a mode, the more it shortens (0.35, the fitted
            // tone, leaves them alone); the fundamental is untouched.
            const auto dampScale = std::pow (2.0f, -(damping - 0.35f) * 4.0f * std::log2 (juce::jmax (1.0f, entry.ratio)));
            mode.t60 = juce::jmax (0.005f, entry.t60 * registerScale * decayScale * dampScale);
            mode.drive = entry.level;
        }
        for (int i = activeModes; i < (int) modes.size(); ++i)
            modes[(size_t) i].re = modes[(size_t) i].im = 0.0f;
        applyDecay();

        // Pickup distance: 0.5 is the fitted distance; each quarter of the
        // knob halves or doubles it (closer = more bark or growl).
        const auto distance = t.distance * std::pow (2.0f, (distanceKnob - 0.5f) * 4.0f);
        inverseDistance = 1.0f / juce::jmax (0.02f, distance);
        pickupOffset = (t.offset + t.offsetRegister * octaves + t.offsetRange * (positionKnob - 0.5f)) * distance;
        if (model == Model::Tine)
        {
            // Normalised by the steepest slope of the bump (at x = d / sqrt 3).
            slopeNorm = 0.6495f * inverseDistance;
        }
        else
        {
            const auto rest = juce::jlimit (-0.7f, 0.7f, pickupOffset * inverseDistance);
            slopeNorm = inverseDistance / ((1.0f - rest) * (1.0f - rest));
        }
        derivativeScale = (float) (sampleRate / (juce::MathConstants<double>::twoPi * frequency)) / slopeNorm;

        // DAMP: its default (0.35) is the fitted tone; more darkens it.
        const auto cut = juce::jlimit (200.0f, (float) sampleRate * 0.45f,
                                       t.toneCut * std::pow (2.0f, (0.35f - damping) * 5.0f));
        toneCoefficient = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cut / (float) sampleRate);
        highPassCoefficient = std::exp (-juce::MathConstants<float>::twoPi * t.highPass / (float) sampleRate);
        biasOffset = std::tanh (t.bias);
        outputGain = t.gain * juce::Decibels::decibelsToGain (t.gainTilt * octaves);
    }

    // The per-sample rotation of each mode, with its decay: the damper's
    // T60 once the key is up (blended by the DAMPER knob).
    void applyDecay()
    {
        const auto& t = tuning();
        for (int i = 0; i < activeModes; ++i)
        {
            auto& mode = modes[(size_t) i];
            auto t60 = mode.t60;
            if (released && damper > 0.0f)
                t60 = juce::jmin (t60, t60 + (t.damperT60 - t60) * damper);
            const auto radius = std::exp (-6.9078f / (juce::jmax (0.002f, t60) * (float) sampleRate));
            mode.cr = radius * std::cos (mode.angle);
            mode.ci = radius * std::sin (mode.angle);
        }
    }

    double sampleRate = 48000.0, frequency = 261.63;
    Model model = Model::Tine;
    float decay = 0.75f, damping = 0.35f, distanceKnob = 0.5f, positionKnob = 0.5f, hardness = 0.5f, damper = 0.0f;
    std::array<Mode, 5> modes {};
    int activeModes = 0;
    bool dirty = true, released = false, primed = false;
    float velocityLevel = 0.0f, forceScale = 0.0f, liveForce = 0.0f;
    int hammerSample = 0, hammerLength = 0;
    float inverseDistance = 1.0f, pickupOffset = 0.0f, slopeNorm = 1.0f, derivativeScale = 1.0f;
    float lastPickup = 0.0f;
    float tone = 0.0f, toneCoefficient = 1.0f, outputGain = 1.0f;
    float highPassIn = 0.0f, highPassOut = 0.0f, highPassCoefficient = 1.0f, biasOffset = 0.0f;
};
