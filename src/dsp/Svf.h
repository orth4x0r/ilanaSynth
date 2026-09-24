#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

class Svf
{
public:
    enum class Mode
    {
        LowPass = 0,
        BandPass,
        HighPass,
        Notch
    };

    struct Coefficients
    {
        double k = 2.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double a3 = 0.0;
        bool selfOscillating = false;
    };

    // Resonance runs 0..1. Up to 0.98 this is the classic linear SVF. Above
    // that the damping goes slightly negative so the filter rings on its own,
    // and a soft clip on the band-pass state holds the oscillation at a
    // stable level.
    static constexpr double linearResonanceLimit = 0.98;

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        const auto clampedCutoff = juce::jlimit (10.0, sampleRate * 0.45, cutoff);
        const auto clampedResonance = juce::jlimit (0.0, 1.0, resonance);
        const auto g = std::tan (juce::MathConstants<double>::pi * clampedCutoff / sampleRate);
        const auto k = clampedResonance <= linearResonanceLimit
                           ? 2.0 - 2.0 * clampedResonance
                           : 2.0 - 2.0 * linearResonanceLimit
                                 - (clampedResonance - linearResonanceLimit) * 3.0;

        Coefficients coefficients;
        coefficients.k = k;
        coefficients.a1 = 1.0 / (1.0 + g * (g + k));
        coefficients.a2 = g * coefficients.a1;
        coefficients.a3 = g * coefficients.a2;
        coefficients.selfOscillating = clampedResonance > linearResonanceLimit;

        return coefficients;
    }

    void setCoefficients (const Coefficients& coefficients)
    {
        k = coefficients.k;
        a1 = coefficients.a1;
        a2 = coefficients.a2;
        a3 = coefficients.a3;
        saturate = coefficients.selfOscillating;
    }

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateCoefficients();
    }

    void setCutoff (double hz)
    {
        cutoff = juce::jlimit (10.0, sampleRate * 0.45, hz);
        updateCoefficients();
    }

    void setResonance (double newResonance)
    {
        resonance = juce::jlimit (0.0, 0.98, newResonance);
        updateCoefficients();
    }

    void setMode (Mode newMode) { mode = newMode; }

    void reset() { ic1 = 0.0f; ic2 = 0.0f; }

    // All three outputs at once (for morphing between them).
    void processAll (float input, float& lowPass, float& bandPass, float& highPass)
    {
        const auto inputDouble = (double) input;
        const auto v3 = inputDouble - (double) ic2;
        const auto v1 = a1 * (double) ic1 + a2 * v3;
        const auto v2 = (double) ic2 + a2 * (double) ic1 + a3 * v3;

        ic1 = (float) (2.0 * v1) - ic1;
        ic2 = (float) (2.0 * v2) - ic2;

        if (saturate)
        {
            ic1 = 2.0f * std::tanh (ic1 * 0.5f);
            ic2 = 2.0f * std::tanh (ic2 * 0.5f);
        }

        lowPass = (float) v2;
        bandPass = (float) v1;
        highPass = input - (float) k * (float) v1 - (float) v2;
    }

    float processSample (float input)
    {
        const auto inputDouble = (double) input;
        const auto v3 = inputDouble - (double) ic2;
        const auto v1 = a1 * (double) ic1 + a2 * v3;
        const auto v2 = (double) ic2 + a2 * (double) ic1 + a3 * v3;

        ic1 = (float) (2.0 * v1) - ic1;
        ic2 = (float) (2.0 * v2) - ic2;

        if (saturate)
        {
            ic1 = 2.0f * std::tanh (ic1 * 0.5f);
            ic2 = 2.0f * std::tanh (ic2 * 0.5f);
        }

        switch (mode)
        {
            case Mode::LowPass:  return (float) v2;
            case Mode::BandPass: return (float) v1;
            case Mode::HighPass: return input - (float) k * (float) v1 - (float) v2;
            case Mode::Notch:    return input - (float) k * (float) v1;
        }

        return input;
    }

private:
    void updateCoefficients()
    {
        const auto g = std::tan (juce::MathConstants<double>::pi * cutoff / sampleRate);
        k = 2.0 - 2.0 * resonance;

        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    Mode mode = Mode::LowPass;
    double sampleRate = 44100.0;
    double cutoff = 20000.0;
    double resonance = 0.0;
    double k = 2.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
    float ic1 = 0.0f, ic2 = 0.0f;
    bool saturate = false;
};
