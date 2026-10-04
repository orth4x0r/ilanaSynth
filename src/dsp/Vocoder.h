#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <cmath>

#include "Biquad.h"

// Channel vocoder (FX type 31). The modulator is split into log-spaced band-
// pass bands; each band's envelope follower scales the carrier's same band,
// the carrier bands optionally shifted (FORMANT). UNVOICED feeds noise into
// the upper carrier bands so consonants and sibilance get through. Without a
// modulator input, TALK derives one from the carrier itself: its mono sum
// through three formant filters that sweep the vowels A-E-I-O-U.
class Vocoder
{
public:
    static constexpr int maxBands = 24;
    static constexpr float lowestHz = 100.0f;
    static constexpr float highestHz = 8000.0f;

    struct Settings
    {
        int bands = 16;
        float width = 1.0f;       // scales the band Q
        float attackMs = 5.0f;
        float releaseMs = 60.0f;
        float formantSemitones = 0.0f;
        float unvoiced = 0.0f;    // noise into the upper bands
        float talkRate = 1.0f;    // Hz, the vowel sweep of TALK
        float levelDb = 0.0f;
        float mix = 1.0f;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (8000.0, newSampleRate);
        reset();
        designed = false;
    }

    void reset()
    {
        for (auto& band : bands)
        {
            band.modulator[0].reset();
            band.modulator[1].reset();
            for (auto& channel : band.carrier)
            {
                channel[0].reset();
                channel[1].reset();
            }
            band.envelope = 0.0f;
        }
        for (auto& channel : talk)
            for (auto& filter : channel)
                filter.reset();
        talkPhase = 0.0;
        noiseState = 0x2545F491u; // its own generator, so nothing else's sequence moves
    }

    // Carrier in `left` / `right` (in place). `modulator` is one mono block,
    // or nullptr for TALK. `numChannels` is 1 or 2.
    void process (float* left, float* right, int numSamples, const float* modulator, const Settings& s)
    {
        design (s);
        const auto count = activeBands;
        const auto attack = std::exp (-1.0f / (juce::jmax (0.05f, s.attackMs) * 0.001f * (float) sampleRate));
        const auto release = std::exp (-1.0f / (juce::jmax (0.5f, s.releaseMs) * 0.001f * (float) sampleRate));
        const auto level = juce::Decibels::decibelsToGain (s.levelDb) * outputScale;
        const auto noiseGain = s.unvoiced * 0.6f;
        const auto talkStep = juce::MathConstants<double>::twoPi * juce::jlimit (0.05f, 20.0f, s.talkRate) / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto carrierL = left[i];
            const auto carrierR = right != nullptr ? right[i] : carrierL;
            auto x = 0.0f;

            if (modulator != nullptr)
                x = modulator[i];
            else
                x = talkSample (0.5f * (carrierL + carrierR), talkStep);

            const auto noise = nextNoise() * noiseGain;
            auto sumL = 0.0f, sumR = 0.0f;

            for (int b = 0; b < count; ++b)
            {
                auto& band = bands[(size_t) b];
                const auto m = std::abs (band.modulator[1].process (band.modulator[0].process (x)));
                band.envelope = m > band.envelope ? attack * band.envelope + (1.0f - attack) * m
                                                  : release * band.envelope + (1.0f - release) * m;

                // The upper bands (about 3 kHz up) also hear the noise.
                const auto hiss = band.unvoiced ? noise : 0.0f;
                sumL += band.carrier[0][1].process (band.carrier[0][0].process (carrierL + hiss)) * band.envelope;
                if (right != nullptr)
                    sumR += band.carrier[1][1].process (band.carrier[1][0].process (carrierR + hiss)) * band.envelope;
            }

            const auto wetL = std::tanh (sumL * level); // soft limit: a hot LEVEL or loud voice can't run away
            left[i] = carrierL + (wetL - carrierL) * s.mix;
            if (right != nullptr)
            {
                const auto wetR = std::tanh (sumR * level);
                right[i] = carrierR + (wetR - carrierR) * s.mix;
            }
        }
    }

    // A band's envelope follower (the modulator's level in it), for display.
    float getBandLevel (int band) const { return juce::isPositiveAndBelow (band, activeBands) ? bands[(size_t) band].envelope : 0.0f; }

private:
    struct Band
    {
        Biquad modulator[2];
        Biquad carrier[2][2];
        float envelope = 0.0f;
        bool unvoiced = false;
    };

    static Biquad::Coefficients bandPass (double sampleRate, double frequency, double q)
    {
        const auto w = juce::MathConstants<double>::twoPi * juce::jlimit (20.0, sampleRate * 0.45, frequency) / sampleRate;
        const auto alpha = std::sin (w) / (2.0 * q);
        return Biquad::normalise (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * std::cos (w), 1.0 - alpha);
    }

    // Band centres and Q from the settings; redone only when they change.
    void design (const Settings& s)
    {
        const auto count = juce::jlimit (4, maxBands, s.bands);
        const auto formant = juce::jlimit (-24.0f, 24.0f, s.formantSemitones);
        const auto width = juce::jlimit (0.25f, 4.0f, s.width);

        if (designed && count == designedBands && formant == designedFormant && width == designedWidth)
            return;

        const auto octaves = std::log2 (highestHz / lowestHz);
        const auto perOctave = (double) count / octaves;
        // Neighbouring bands cross at about -3 dB; two cascaded sections narrow
        // each band, so the Q of one section is a little lower.
        const auto q = (double) width * 0.9 / (std::pow (2.0, 0.5 / perOctave) - std::pow (2.0, -0.5 / perOctave));
        const auto shift = std::pow (2.0, (double) formant / 12.0);
        const auto ceiling = sampleRate * 0.42;

        for (int b = 0; b < count; ++b)
        {
            auto& band = bands[(size_t) b];
            const auto position = count > 1 ? (double) b / (double) (count - 1) : 0.0;
            const auto centre = (double) lowestHz * std::pow ((double) highestHz / lowestHz, position);
            const auto modulatorCoeffs = bandPass (sampleRate, juce::jmin (centre, ceiling), q);
            const auto carrierCoeffs = bandPass (sampleRate, juce::jmin (centre * shift, ceiling), q);

            for (auto& filter : band.modulator)
                filter.setCoefficients (modulatorCoeffs);
            for (auto& channel : band.carrier)
                for (auto& filter : channel)
                    filter.setCoefficients (carrierCoeffs);

            band.unvoiced = centre >= 3000.0;
            if (b >= activeBands)
                band.envelope = 0.0f;
        }

        activeBands = count;
        designedBands = count;
        designedFormant = formant;
        designedWidth = width;
        designed = true;
    }

    // Three formant filters, sweeping A, E, I, O, U and back with the LFO.
    float talkSample (float input, double step)
    {
        static constexpr float f1[5] { 800.0f, 400.0f, 350.0f, 450.0f, 325.0f };
        static constexpr float f2[5] { 1150.0f, 1600.0f, 1700.0f, 800.0f, 700.0f };
        static constexpr float f3[5] { 2800.0f, 2700.0f, 2700.0f, 2830.0f, 2530.0f };

        // Recomputed every 32 samples: the sweep is slow.
        if (talkCounter++ % 32 == 0)
        {
            const auto sweep = 0.5 - 0.5 * std::cos (talkPhase); // 0..1..0 over a cycle
            const auto position = (float) sweep * 4.0f;
            const auto index = juce::jlimit (0, 3, (int) position);
            const auto frac = position - (float) index;
            const auto mix = [&] (const float* table) { return table[index] + (table[index + 1] - table[index]) * frac; };
            const double centres[3] { mix (f1), mix (f2), mix (f3) };

            for (int k = 0; k < 3; ++k)
                talk[0][k].setCoefficients (bandPass (sampleRate, centres[k], 6.0));
        }

        talkPhase += step;
        if (talkPhase > juce::MathConstants<double>::twoPi)
            talkPhase -= juce::MathConstants<double>::twoPi;

        // (Made up to a usable level: a formant filter takes most of a saw away.)
        return 8.0f * (talk[0][0].process (input) + 0.7f * talk[0][1].process (input) + 0.4f * talk[0][2].process (input));
    }

    float nextNoise()
    {
        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        return (float) (noiseState & 0xffffff) / (float) 0x800000 - 1.0f;
    }

    // Set so the output is about as loud as the modulator (a saw carrier).
    static constexpr float outputScale = 12.0f;

    double sampleRate = 48000.0;
    std::array<Band, maxBands> bands;
    std::array<std::array<Biquad, 3>, 1> talk;
    double talkPhase = 0.0;
    unsigned talkCounter = 0;
    std::uint32_t noiseState = 0x2545F491u;
    int activeBands = 16;
    int designedBands = 0;
    float designedFormant = 0.0f, designedWidth = 0.0f;
    bool designed = false;
};
