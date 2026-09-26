#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Generative.h"
#include "TunedString.h"
#include <array>
#include <cmath>
#include <vector>

// One shared bank after voice summing. All storage is allocated in prepare().
class SympatheticStrings
{
public:
    static constexpr int maxStrings = 6;

    void prepare (double rate)
    {
        sampleRate = juce::jmax (1.0, rate);
        for (auto& string : strings)
            string.loop.allocate ((int) std::ceil (sampleRate / 40.0) + 4);
    }

    void reset()
    {
        for (auto& string : strings)
            string.loop.reset();
    }

    // Manual notes, or ascending notes of the GENERATE scale from the root.
    // With no scale set (chromatic), consecutive notes would be a semitone
    // cluster, so the strings use an open tuning on the root instead.
    void setTuning (int scale, int root, bool manual, const std::array<int, maxStrings>& manualNotes)
    {
        constexpr int openTuning[maxStrings] { 0, 7, 12, 16, 19, 24 };
        const auto base = 48 + juce::jlimit (0, 11, root);
        auto candidate = base;

        for (int i = 0; i < maxStrings; ++i)
        {
            int note;

            if (manual)
                note = manualNotes[(size_t) i];
            else if (scale <= 0)
                note = base + openTuning[i];
            else
            {
                while (candidate < 108 && ! Scales::contains (candidate, scale, root))
                    ++candidate;

                note = candidate++;
            }

            strings[(size_t) i].note = juce::jlimit (36, 96, note);
            strings[(size_t) i].frequency = juce::MidiMessage::getMidiNoteInHertz (strings[(size_t) i].note);
        }
    }

    int noteFor (int index) const { return strings[(size_t) juce::jlimit (0, maxStrings - 1, index)].note; }

    // DECAY is the strings' ring time: 0.25 s to 12 s.
    float process (float input, int count, float amount, float decay)
    {
        if (amount <= 0.0f || count <= 0) return 0.0f;
        count = juce::jmin (count, maxStrings);
        if (decay != lastDecay)
        {
            lastDecay = decay;
            t60 = 0.25 * std::pow (48.0, (double) juce::jlimit (0.0f, 1.0f, decay));
        }
        const auto drive = juce::jlimit (-2.0f, 2.0f, input) * inputGain;
        auto output = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            auto& string = strings[(size_t) i];
            string.loop.tune (sampleRate, string.frequency, t60, loopFilter);
            output += string.loop.process (drive);
        }
        // Any one note only wakes one or two strings: scale by sqrt (count).
        const auto wet = output * amount / std::sqrt ((float) count);
        return std::isfinite (wet) ? juce::jlimit (-4.0f, 4.0f, wet) : 0.0f;
    }

private:
    struct String
    {
        TunedString loop;
        int note = 48;
        double frequency = 130.81;
    };
    static constexpr float loopFilter = 0.5f, inputGain = 0.066f;   // 3x (+9.5 dB) after listening, 2026-09-26
    std::array<String, maxStrings> strings;
    double sampleRate = 48000.0, t60 = 1.0;
    float lastDecay = -1.0f;
};
