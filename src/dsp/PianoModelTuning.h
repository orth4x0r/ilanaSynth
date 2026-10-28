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
    float massBass = 2.0f, massTreble = 2.5012f;
    float exponentBass = 1.4806f, exponentTreble = 2.7426f;
    float feltBass = 53.497f, feltTreble = 3.0f;
    float hardnessRange = 2.0f;
    // Felt hysteresis (units of 1e-4 s).
    float hysteresis = 0.39559f;
    // Hammer speed at velocity 0 and 127 (m/s).
    float speedLow = 0.2f, speedHigh = 3.5686f;
    // String impedance sqrt(T mu) at C4 and at A0 (kg/s).
    float impedanceC4 = 0.6f, impedanceBass = 4.9012f;
    // Strike point as a fraction of the string, bass and treble.
    float strikeBass = 0.12036f, strikeTreble = 0.10578f;
    // Inharmonicity B at C4, and its rise per octave (as a power of 2) up
    // and down the keyboard.
    float inharmonicityC4 = 0.00015378f, inharmonicityTreble = 1.036f, inharmonicityBass = -0.16085f;
    // The fundamental's T60 at C4 (DECAY 0.5), how it scales with pitch (below and above C4),
    // and the T60 at 3 kHz (DAMP 0.5); the vertical polarisation's share.
    float t60C4 = 17.707f, t60Register = 0.54761f, t60RegisterTreble = 0.39802f, t60Upper = 21.207f, promptRatio = 0.18447f;
    // The horizontal polarisation: how much of the strike, its level and
    // its detuning (cents).
    float horizontalTrebleFall = 0.0f; // dB per octave above C5
    float toneCutoff = 10116.0f; // Hz: the two-pole low-pass on the string's output
    float strikeBandwidth = 4870.5f; // Hz: the felt's rounding of the strike pulse
    float aftersound = 0.18f, horizontalMix = 1.448f, horizontalCents = 0.23987f;
    // The bass bark: tension modulation and the longitudinal modes.
    float tensionModulation = 0.017f, longitudinal = 0.03993f, longitudinalRatio = 15.0f;
    // Knock (level, frequency at C4, level per octave), level and balance
    // (level per octave from C4).
    float knock = 0.2383f, knockFrequency = 350.0f, knockRegister = 0.912434f;
    float outputGain = 0.96f, balance = 0.121181f;

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
    static const std::array<Field, 42>& fields()
    {
        static const std::array<Field, 42> list {
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
            Field { "dipQ", &PianoModelTuning::dipQ }, Field { "strikeBandwidth", &PianoModelTuning::strikeBandwidth }, Field { "toneCutoff", &PianoModelTuning::toneCutoff }, Field { "horizontalTrebleFall", &PianoModelTuning::horizontalTrebleFall }, Field { "t60RegisterTreble", &PianoModelTuning::t60RegisterTreble },
        };
        return list;
    }

public:
    // The fundamental's dip: dB cut at the pitch at these notes (joined in a line), and its Q.
    static constexpr std::array<int, 8> dipNotes { 21, 33, 45, 57, 69, 81, 96, 108 };
    std::array<float, 8> dip { 0.0f, 0.0f, 6.0f, 3.0f, 6.0f, 15.0f, 0.0f, 0.0f };
    float dipQ = 1.2f;
    // The level back: dB added at the same notes, after the dip.
    std::array<float, 8> dipTrim { 0.0f, 0.0f, 2.0f, 5.6591f, 5.5754f, 6.16f, -1.6561f, -4.6951f };
    // The dense soundboard (a later part of M8.2).
    float boardMix = 2.8457f, boardDecay = 0.6f, boardLowCut = 125.428f, boardTilt = 0.49641f, boardDensity = 0.641009f;
    // The measured colour: dB per octave band, 31 Hz .. 16 kHz.
    std::array<float, 10> boardEq { 11.344f, 6.5464f, -17.989f, -2.077f, 5.7791f, 2.6002f, -5.3017f, -13.108f, -0.4104f, -7.62f };
};
