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
    float massBass = 11.8894f, massTreble = 5.03f;
    float exponentBass = 2.05762f, exponentTreble = 2.82263f;
    float feltBass = 65.0f, feltTreble = 30.0f;
    float hardnessRange = 0.5f;
    // Felt hysteresis (units of 1e-4 s).
    float hysteresis = 0.12005f;
    // Hammer speed at velocity 0 and 127 (m/s).
    float speedLow = 0.2f, speedHigh = 3.56864f;
    // String impedance sqrt(T mu) at C4 and at A0 (kg/s).
    float impedanceC4 = 1.33796f, impedanceBass = 4.9012f;
    // Strike point as a fraction of the string, bass and treble.
    float strikeBass = 0.12705f, strikeTreble = 0.0929744f;
    // Inharmonicity B at C4, and its rise per octave (as a power of 2) up
    // and down the keyboard.
    float inharmonicityC4 = 0.00015378f, inharmonicityTreble = 1.036f, inharmonicityBass = -0.16085f;
    // The fundamental's T60 at C4 (DECAY 0.5), how it scales with pitch,
    // and the T60 at 3 kHz (DAMP 0.5); the vertical polarisation's share.
    float t60C4 = 15.775f, t60Register = 0.704095f, t60Upper = 8.124f, promptRatio = 0.092542f;
    // The horizontal polarisation: how much of the strike, its level and
    // its detuning (cents).
    float aftersound = 0.22463f, horizontalMix = 0.666726f, horizontalCents = 0.5098f;
    // The bass bark: tension modulation and the longitudinal modes.
    float tensionModulation = 0.017f, longitudinal = 0.03993f, longitudinalRatio = 15.0f;
    // Knock (level, frequency at C4, level per octave), level and balance
    // (level per octave from C4).
    float knock = 0.158342f, knockFrequency = 500.0f, knockRegister = 0.912434f;
    float outputGain = 0.683f, balance = 0.121181f;

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
    static const std::array<Field, 37>& fields()
    {
        static const std::array<Field, 37> list {
            Field { "massBass", &PianoModelTuning::massBass }, Field { "massTreble", &PianoModelTuning::massTreble },
            Field { "exponentBass", &PianoModelTuning::exponentBass }, Field { "exponentTreble", &PianoModelTuning::exponentTreble },
            Field { "feltBass", &PianoModelTuning::feltBass }, Field { "feltTreble", &PianoModelTuning::feltTreble },
            Field { "hardnessRange", &PianoModelTuning::hardnessRange }, Field { "hysteresis", &PianoModelTuning::hysteresis },
            Field { "speedLow", &PianoModelTuning::speedLow }, Field { "speedHigh", &PianoModelTuning::speedHigh },
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
        };
        return list;
    }

public:
    // The dense soundboard (a later part of M8.2).
    float boardMix = 2.95198f, boardDecay = 0.6f, boardLowCut = 125.428f, boardTilt = 0.569308f, boardDensity = 0.641009f;
    // The measured colour: dB per octave band, 31 Hz .. 16 kHz.
    std::array<float, 10> boardEq { 9.00f, 2.15f, -5.43f, 3.16f, 1.59f, 3.74f, 0.17f, -3.23f, -7.20f, -9.00f };
};
