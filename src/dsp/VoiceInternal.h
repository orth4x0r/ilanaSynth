#pragma once
// File-local helpers shared by Voice.cpp, VoiceRender.cpp and VoiceUpdate.cpp.
// Moved verbatim out of Voice.cpp; each including file gets its own copy.

#include "Voice.h"
#include "FastMath.h"
#include <array>
#include <cmath>

namespace
{
// The SUB card's COLOUR: 0 passes the wave as it is (every older patch); up
// to 1 drives it into a tanh, which adds harmonics to a sine and rounds the
// edges of a square or saw, scaled so the peak stays at the wave's own.
inline float colourSub (float wave, float colour)
{
    if (colour <= 0.0005f)
        return wave;
    const auto drive = 1.0f + 6.0f * colour;
    return std::tanh (wave * drive) / std::tanh (drive);
}

int chordInterval (int mode, int voiceIndex)
{
    switch (mode)
    {
        case 1:  { static const int table[] = { 0, 12 }; return table[voiceIndex % 2]; }
        case 2:  { static const int table[] = { 0, 7 }; return table[voiceIndex % 2]; }
        case 3:  { static const int table[] = { 0, 7, 12 }; return table[voiceIndex % 3]; }
        case 4:  { static const int table[] = { 0, 4, 7, 12 }; return table[voiceIndex % 4]; }
        case 5:  { static const int table[] = { 0, 3, 7, 12 }; return table[voiceIndex % 4]; }
        case 6:  { static const int table[] = { 0, 5, 7, 12 }; return table[voiceIndex % 4]; }
        default: return 0;
    }
}

// Unison voices are spread over -1..1; each mode maps that position to a
// pitch offset. Classic is linear; Hypersaw bunches voices near the centre
// like the JP-8000 supersaw; Octaves and Fifths stack intervals too.
double unisonDetunePosition (int mode, float position)
{
    if (mode == UnisonMode::Hypersaw)
        return (position >= 0.0f ? 1.0 : -1.0) * std::pow ((double) std::abs (position), 1.5);

    return (double) position;
}

int unisonStackInterval (int mode, int voiceIndex, int numVoices)
{
    if (numVoices < 2)
        return 0;

    if (mode == UnisonMode::Octaves)
    {
        static const int table[] = { 0, 12, -12 };
        return table[voiceIndex % 3];
    }

    if (mode == UnisonMode::Fifths)
    {
        static const int table[] = { 0, 7, 12 };
        return table[voiceIndex % 3];
    }

    switch (mode)
    {
        case UnisonMode::CenterDrop: return voiceIndex == numVoices / 2 ? -12 : 0;
        case UnisonMode::Octaves2:   { static const int t[] { 0, 12, 24, -12, -24 }; return t[voiceIndex % 5]; }
        case UnisonMode::PowerChord: { static const int t[] { 0, 7 }; return t[voiceIndex % 2]; }
        case UnisonMode::MajorChord: { static const int t[] { 0, 4, 7 }; return t[voiceIndex % 3]; }
        case UnisonMode::MinorChord: { static const int t[] { 0, 3, 7 }; return t[voiceIndex % 3]; }
        // Harmonic n sits 12 log2 (n) semitones above the root.
        case UnisonMode::Harmonics:     return (int) std::lround (12.0 * std::log2 ((double) (voiceIndex + 1)));
        case UnisonMode::OddHarmonics:  return (int) std::lround (12.0 * std::log2 ((double) (2 * voiceIndex + 1)));
        default: break;
    }

    return 0;
}

float lfoShapeAt (int shape, double phase, float hold, const float* steps, const float* custom, int customSize)
{
    switch (shape)
    {
        case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        case 1: return (float) (1.0 - 4.0 * std::abs (phase - 0.5));
        case 2: return (float) (2.0 * phase - 1.0);
        case 3: return (float) (1.0 - 2.0 * phase);
        case 4: return phase < 0.5 ? 1.0f : -1.0f;
        case 5: return hold;
        case 7: return steps != nullptr ? steps[juce::jlimit (0, 15, (int) (phase * 16.0))] : 0.0f;
        default:
        {
            // Draw (6) and any table-driven shape.
            if (custom == nullptr || customSize <= 0)
                return 0.0f;

            const auto position = phase * (double) customSize;
            const auto index = (int) position % customSize;
            const auto next = (index + 1) % customSize;
            const auto frac = (float) (position - std::floor (position));
            return custom[index] + (custom[next] - custom[index]) * frac;
        }
    }
}

// Scales for explicit destinations: how far a full-depth modulation moves
// the target, in the target's own units.
// (The rest are each knob's whole range: Mod::Range.)
constexpr float driveRange = 9.0f;       // drive multiplier

// How much of the cabled audio a material body takes in besides its own
// excitation, so audio patched into BODY rings the body.
constexpr float bodyAudioShare = 0.3f;
} // namespace
