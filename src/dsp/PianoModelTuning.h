#pragma once

#include <juce_core/juce_core.h>

#include <array>

// M8.2: the Piano exciter's constants (PianoString), fitted to a real grand
// across the keyboard at pp, mf and ff with tools/fit_piano2.py. Starting
// values from the literature (Chaigne and Askenfelt's hammers, typical
// string impedances). The test binary can override them
// (ILANA_PIANO2_TUNING="name=value;..."); the plugin uses the defaults.
struct PianoModelTuning
{
    // Hammer mass (g), felt exponent and felt force at 1 mm (N), from A0 to
    // C8; HAMMER multiplies the felt by 4^(+-hardnessRange).
    float massBass = 7.64f, massTreble = 5.1876f;
    float exponentBass = 2.5273f, exponentTreble = 2.5575f;
    float feltBass = 18.647f, feltTreble = 1.0f;
    float hardnessRange = 2.0f;
    // Felt hysteresis (units of 1e-4 s).
    float hysteresis = 0.29559f;
    // Hammer speed at velocity 0 and 127 (m/s).
    float speedLow = 0.22755f, speedHigh = 4.1225f;
    // Key velocity is raised to this power first (above 1 the loud end opens up).
    float velocityCurve = 2.5216f;
    // Level only (the sound stays as the hammer makes it): dB added at key
    // velocities 20, 60, 100 and 124 (a line between them, flat beyond), and dB
    // per octave above C5 (at the softest key, and added per unit of curved velocity).
    float velocityGainSoft = -8.4802f, velocityGainMedium = -5.8198f, velocityGainHigh = 7.8194f, velocityGainLoud = -0.80004f, trebleLevel = 19.0f, trebleLevelVelocity = -15.9f;
    // String impedance sqrt(T mu) at C4 and at A0 (kg/s).
    float impedanceC4 = 0.6f, impedanceBass = 4.9012f;
    // Strike point as a fraction of the string, bass and treble.
    float strikeBass = 0.12036f, strikeTreble = 0.11184f;
    // Inharmonicity B at C4, and its rise per octave (as a power of 2) up
    // and down the keyboard.
    float inharmonicityC4 = 8.1653e-05f, inharmonicityTreble = 0.84653f, inharmonicityBass = -0.17413f;
    // The fundamental's T60 at C4 (DECAY 0.5), how it scales with pitch (below and above C4),
    // and the T60 at 3 kHz (DAMP 0.5); the vertical polarisation's share.
    float t60C4 = 14.342f, t60Register = 0.57327f, t60RegisterTreble = 0.48961f, t60Upper = 32.013f, promptRatio = 0.12411f;
    // The horizontal polarisation: how much of the strike, its level and
    // its detuning (cents).
    float horizontalTrebleFall = 1.5574f; // dB per octave above C5
    float toneCutoff = 5303.1f; // Hz: the two-pole low-pass on the string's output
    float quadrature = 1.0f; // 1: the two string directions are heard a quarter turn apart (no partial cancels)
    float toneCutoffBass = 10116.0f; // Hz at A1 (toneCutoff is the cutoff from C5 up)
    float strikeBandwidth = 5670.5f; // Hz: the felt's rounding of the strike pulse
    float aftersound = 0.19214f, horizontalMix = 0.45391f, horizontalCents = 0.17714f;
    // The bass bark: tension modulation and the longitudinal modes.
    float tensionModulation = 0.017f, longitudinal = 0.03993f, longitudinalRatio = 15.0f;
    // Knock (level, frequency at C4, level per octave), level and balance
    // (level per octave from C4).
    // The hiss: noise of the board and the struck string, shaped by a three-pole
    // low-pass, fading over its own time. Level in dB at C4 (-200 = none) and
    // per octave, T60 (s) at C4 and its per-octave power of two, cutoff (Hz) at C4
    // and per octave, and how the level follows the hammer speed (power).
    float hissLevel = -40.0f, hissLevelRegister = -1.08f, hissDecay = 2.2226f, hissDecayRegister = -0.10849f;
    float hissCutoff = 622.13f, hissCutoffRegister = 0.45099f, hissVelocity = 1.3638f;
    float knock = 0.52191f, knockFrequency = 350.0f, knockRegister = 1.0557f;
    float outputGain = 2.1f, balance = 0.12118f;

    static PianoModelTuning& get()
    {
        static PianoModelTuning tuning;
        return tuning;
    }

    bool apply (const juce::String& text)
    {
        auto ok = true;
        for (const auto& item : juce::StringArray::fromTokens (text, ";", ""))
        {
            const auto name = item.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = item.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            auto found = false;
            for (auto& field : fields())
                if (name == field.first)
                {
                    this->*(field.second) = value;
                    found = true;
                }
            if (name.startsWith ("dipTrim"))
            {
                const auto point = name.getTrailingIntValue();
                if (point >= 0 && point < (int) dipTrim.size())
                {
                    dipTrim[(size_t) point] = value;
                    found = true;
                }
            }
            if (name.startsWith ("dip") && name != "dipQ")
            {
                const auto point = name.getTrailingIntValue();
                if (point >= 0 && point < (int) dip.size())
                {
                    dip[(size_t) point] = value;
                    found = true;
                }
            }
            if (name.startsWith ("boardEq"))
            {
                const auto band = name.getTrailingIntValue();
                if (band >= 0 && band < (int) boardEq.size())
                {
                    boardEq[(size_t) band] = value;
                    found = true;
                }
            }
            ok = ok && (found || name.isEmpty());
        }
        return ok;
    }

private:
    using Field = std::pair<const char*, float PianoModelTuning::*>;
    static const std::array<Field, 58>& fields()
    {
        static const std::array<Field, 58> list {
            Field { "massBass", &PianoModelTuning::massBass }, Field { "massTreble", &PianoModelTuning::massTreble },
            Field { "exponentBass", &PianoModelTuning::exponentBass }, Field { "exponentTreble", &PianoModelTuning::exponentTreble },
            Field { "feltBass", &PianoModelTuning::feltBass }, Field { "feltTreble", &PianoModelTuning::feltTreble },
            Field { "hardnessRange", &PianoModelTuning::hardnessRange }, Field { "hysteresis", &PianoModelTuning::hysteresis },
            Field { "hissLevel", &PianoModelTuning::hissLevel }, Field { "hissLevelRegister", &PianoModelTuning::hissLevelRegister }, Field { "hissDecay", &PianoModelTuning::hissDecay }, Field { "hissDecayRegister", &PianoModelTuning::hissDecayRegister }, Field { "hissCutoff", &PianoModelTuning::hissCutoff }, Field { "hissCutoffRegister", &PianoModelTuning::hissCutoffRegister }, Field { "hissVelocity", &PianoModelTuning::hissVelocity }, Field { "velocityGainSoft", &PianoModelTuning::velocityGainSoft }, Field { "velocityGainMedium", &PianoModelTuning::velocityGainMedium }, Field { "velocityGainHigh", &PianoModelTuning::velocityGainHigh }, Field { "velocityGainLoud", &PianoModelTuning::velocityGainLoud }, Field { "trebleLevel", &PianoModelTuning::trebleLevel }, Field { "trebleLevelVelocity", &PianoModelTuning::trebleLevelVelocity }, Field { "velocityCurve", &PianoModelTuning::velocityCurve }, Field { "speedLow", &PianoModelTuning::speedLow }, Field { "speedHigh", &PianoModelTuning::speedHigh },
            Field { "impedanceC4", &PianoModelTuning::impedanceC4 }, Field { "impedanceBass", &PianoModelTuning::impedanceBass },
            Field { "strikeBass", &PianoModelTuning::strikeBass }, Field { "strikeTreble", &PianoModelTuning::strikeTreble },
            Field { "inharmonicityC4", &PianoModelTuning::inharmonicityC4 }, Field { "inharmonicityTreble", &PianoModelTuning::inharmonicityTreble },
            Field { "inharmonicityBass", &PianoModelTuning::inharmonicityBass },
            Field { "t60C4", &PianoModelTuning::t60C4 }, Field { "t60Register", &PianoModelTuning::t60Register },
            Field { "t60Upper", &PianoModelTuning::t60Upper }, Field { "promptRatio", &PianoModelTuning::promptRatio },
            Field { "aftersound", &PianoModelTuning::aftersound }, Field { "horizontalMix", &PianoModelTuning::horizontalMix },
            Field { "horizontalCents", &PianoModelTuning::horizontalCents },
            Field { "tensionModulation", &PianoModelTuning::tensionModulation }, Field { "longitudinal", &PianoModelTuning::longitudinal },
            Field { "longitudinalRatio", &PianoModelTuning::longitudinalRatio },
            Field { "knock", &PianoModelTuning::knock }, Field { "knockFrequency", &PianoModelTuning::knockFrequency },
            Field { "knockRegister", &PianoModelTuning::knockRegister },
            Field { "outputGain", &PianoModelTuning::outputGain }, Field { "balance", &PianoModelTuning::balance },
            // Board (Soundboard::Dense; read by AcousticKeys.h).
            Field { "boardMix", &PianoModelTuning::boardMix }, Field { "boardDecay", &PianoModelTuning::boardDecay },
            Field { "boardLowCut", &PianoModelTuning::boardLowCut }, Field { "boardTilt", &PianoModelTuning::boardTilt },
            Field { "boardDensity", &PianoModelTuning::boardDensity },
            Field { "dipQ", &PianoModelTuning::dipQ }, Field { "strikeBandwidth", &PianoModelTuning::strikeBandwidth }, Field { "toneCutoff", &PianoModelTuning::toneCutoff }, Field { "quadrature", &PianoModelTuning::quadrature }, Field { "toneCutoffBass", &PianoModelTuning::toneCutoffBass }, Field { "horizontalTrebleFall", &PianoModelTuning::horizontalTrebleFall }, Field { "t60RegisterTreble", &PianoModelTuning::t60RegisterTreble },
        };
        return list;
    }

public:
    // The fundamental's dip: dB cut at the pitch at these notes (joined in a line), and its Q.
    static constexpr std::array<int, 8> dipNotes { 21, 33, 45, 57, 69, 81, 96, 108 };
    std::array<float, 8> dip { 0.0f, 0.0f, 6.0f, 3.0f, 6.0f, 15.0f, 0.0f, 0.0f };
    float dipQ = 1.2f;
    // The level back: dB added at the same notes, after the dip.
    std::array<float, 8> dipTrim { -8.9631f, -1.8656f, -3.2783f, -1.256f, -2.6688f, -0.36736f, -1.8733f, -0.23641f };
    // The dense soundboard (a later part of M8.2).
    float boardMix = 2.8457f, boardDecay = 0.6f, boardLowCut = 125.428f, boardTilt = 0.49641f, boardDensity = 0.641009f;
    // The measured colour: dB per octave band, 31 Hz .. 16 kHz.
    std::array<float, 10> boardEq { 11.344f, 6.5464f, -17.989f, -2.077f, 5.7791f, 2.6002f, -5.3017f, -13.108f, -0.4104f, -7.62f };
};
