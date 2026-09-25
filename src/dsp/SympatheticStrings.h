#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Generative.h"
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
        const auto capacity = (size_t) std::ceil (sampleRate / 40.0) + 4;
        for (auto& string : strings)
        {
            string.delay.assign (capacity, 0.0f);
            string.write = 0;
            string.filter = 0.0f;
        }
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
            const auto frequency = juce::MidiMessage::getMidiNoteInHertz (strings[(size_t) i].note);

            // The loop's one-pole filter adds (1 - a) / a samples of delay at
            // low frequencies; take it off the delay line so the string rings
            // at its note rather than flat.
            strings[(size_t) i].period = juce::jlimit (2.0, (double) strings[(size_t) i].delay.size() - 2.0,
                                                      sampleRate / frequency - (1.0 - loopFilter) / loopFilter);
        }
    }

    int noteFor (int index) const { return strings[(size_t) juce::jlimit (0, maxStrings - 1, index)].note; }

    float process (float input, int count, float amount, float decay)
    {
        if (amount <= 0.0f || count <= 0) return 0.0f;
        const auto feedback = 0.93f + 0.0695f * juce::jlimit (0.0f, 1.0f, decay);
        auto output = 0.0f;
        for (int i = 0; i < juce::jmin (count, maxStrings); ++i)
        {
            auto& string = strings[(size_t) i];
            const auto size = (int) string.delay.size();
            if (size < 4) continue;
            auto read = (double) string.write - string.period;
            if (read < 0.0) read += size;
            const auto index = (int) read;
            const auto fraction = (float) (read - index);
            const auto delayed = string.delay[(size_t) index]
                               + (string.delay[(size_t) ((index + 1) % size)] - string.delay[(size_t) index]) * fraction;
            string.filter += (delayed - string.filter) * (float) loopFilter;
            string.delay[(size_t) string.write] = juce::jlimit (-4.0f, 4.0f,
                string.filter * feedback + juce::jlimit (-2.0f, 2.0f, input) * 0.035f);
            string.write = (string.write + 1) % size;
            output += delayed;
        }
        const auto wet = output * amount * (0.8f / (float) juce::jmax (1, count));
        return std::isfinite (wet) ? juce::jlimit (-4.0f, 4.0f, wet) : 0.0f;
    }

private:
    struct String
    {
        std::vector<float> delay;
        int write = 0, note = 48;
        double period = 200.0;
        float filter = 0.0f;
    };
    static constexpr double loopFilter = 0.35;
    std::array<String, maxStrings> strings;
    double sampleRate = 48000.0;
};
