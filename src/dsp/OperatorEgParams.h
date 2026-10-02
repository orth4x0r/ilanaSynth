#pragma once

// The Operator EG's parameters (2026-10-02): an oscillator whose ENVELOPE is
// "Op EG" plays the DX7's own envelope generator (Dx7Engine.h) with
// these settings, so a DX7 voice is an ordinary patch. Each parameter is one
// byte of the 156-byte voice the engine reads; the processor fills that voice
// from them, and the .syx reader sets them from a voice. All appended; their
// defaults are the DX7's init voice, and nothing reads them unless an
// oscillator picks the Operator EG.

#include <array>

namespace OperatorEg
{
// The ENVELOPE choice (oscN_amp_env) that selects it: appended after MSEG.
constexpr int envelopeChoice = 17;

struct Field
{
    const char* suffix; // after the oscillator prefix, or the whole id
    const char* name;   // after "OscN " for operator fields
    int offset;         // byte in the voice (operator fields: within the 21)
    int maximum;
    int defaultValue;
    bool choice = false;
};

inline const std::array<const char*, 4> curveNames { "-LIN", "-EXP", "+EXP", "+LIN" };
inline const std::array<const char*, 6> lfoWaveNames { "Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S&H" };

// Per operator: rates, levels, keyboard scaling, sensitivities, output level.
inline const std::array<Field, 17>& operatorFields()
{
    static const std::array<Field, 17> fields { {
        { "_eg_r1", "EG Rate 1", 0, 99, 99 },
        { "_eg_r2", "EG Rate 2", 1, 99, 99 },
        { "_eg_r3", "EG Rate 3", 2, 99, 99 },
        { "_eg_r4", "EG Rate 4", 3, 99, 99 },
        { "_eg_l1", "EG Level 1", 4, 99, 99 },
        { "_eg_l2", "EG Level 2", 5, 99, 99 },
        { "_eg_l3", "EG Level 3", 6, 99, 99 },
        { "_eg_l4", "EG Level 4", 7, 99, 0 },
        { "_eg_break", "EG Break Point", 8, 99, 39 },
        { "_eg_ldepth", "EG Left Depth", 9, 99, 0 },
        { "_eg_rdepth", "EG Right Depth", 10, 99, 0 },
        { "_eg_lcurve", "EG Left Curve", 11, 3, 0, true },
        { "_eg_rcurve", "EG Right Curve", 12, 3, 0, true },
        { "_eg_rate_key", "EG Rate Scaling", 13, 7, 0 },
        { "_eg_ams", "EG Amp Mod Sens", 14, 3, 0 },
        { "_eg_vel", "EG Velocity Sens", 15, 7, 0 },
        { "_eg_out", "EG Output Level", 16, 99, 99 },
    } };
    return fields;
}

// For the whole voice: the pitch envelope and the LFO. At their defaults
// (pitch levels 50, depths 0) they move nothing.
inline const std::array<Field, 15>& voiceFields()
{
    static const std::array<Field, 15> fields { {
        { "opeg_pitch_r1", "Op Pitch EG Rate 1", 126, 99, 99 },
        { "opeg_pitch_r2", "Op Pitch EG Rate 2", 127, 99, 99 },
        { "opeg_pitch_r3", "Op Pitch EG Rate 3", 128, 99, 99 },
        { "opeg_pitch_r4", "Op Pitch EG Rate 4", 129, 99, 99 },
        { "opeg_pitch_l1", "Op Pitch EG Level 1", 130, 99, 50 },
        { "opeg_pitch_l2", "Op Pitch EG Level 2", 131, 99, 50 },
        { "opeg_pitch_l3", "Op Pitch EG Level 3", 132, 99, 50 },
        { "opeg_pitch_l4", "Op Pitch EG Level 4", 133, 99, 50 },
        { "opeg_lfo_speed", "Op LFO Speed", 137, 99, 35 },
        { "opeg_lfo_delay", "Op LFO Delay", 138, 99, 0 },
        { "opeg_lfo_pmd", "Op LFO Pitch Depth", 139, 99, 0 },
        { "opeg_lfo_amd", "Op LFO Amp Depth", 140, 99, 0 },
        { "opeg_lfo_sync", "Op LFO Key Sync", 141, 1, 1 },
        { "opeg_lfo_wave", "Op LFO Wave", 142, 5, 0, true },
        { "opeg_lfo_pms", "Op LFO Pitch Sens", 143, 7, 3 },
    } };
    return fields;
}

// The key the operators' keyboard and rate scaling follow, in semitones from
// the played note (a DX7 voice's TRANSPOSE; its pitch is in each SEMI knob).
inline constexpr const char* keyOffsetId = "opeg_key_offset";
inline constexpr int keyOffsetByte = 144; // stored as offset + 24

// The DX7 algorithm (1-32, 0 for none) the FM routing was set from: shown on
// the FM page, never read by the sound.
inline constexpr const char* dx7AlgorithmId = "fm_dx7_algorithm";
} // namespace OperatorEg
