#pragma once

// The Operator Env's parameters (2026-10-02; "Operator EG" in the code): an
// oscillator whose ENVELOPE is "Operator Env" plays the DX7's own envelope
// generator (Dx7Engine.h) with these settings, so a DX7 voice is an ordinary patch. Each parameter is one
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
    const char* name;   // after "OscN " for operator fields; the synth's words (review 7)
    int offset;         // byte in the voice (operator fields: within the 21)
    int maximum;
    int defaultValue;
    bool choice = false;
};

// Display labels only (the saved value is the index).
// Short, so they fit the FM page's menus (review 7, I7-14).
inline const std::array<const char*, 4> curveNames { "-Lin", "-Exp", "+Exp", "+Lin" };
inline const std::array<const char*, 6> lfoWaveNames { "Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S&H" };

// Per operator: rates, levels, keyboard scaling, sensitivities, output level.
inline const std::array<Field, 17>& operatorFields()
{
    static const std::array<Field, 17> fields { {
        { "_eg_r1", "OP ENV Attack", 0, 99, 99 },
        { "_eg_r2", "OP ENV Decay 1", 1, 99, 99 },
        { "_eg_r3", "OP ENV Decay 2", 2, 99, 99 },
        { "_eg_r4", "OP ENV Release", 3, 99, 99 },
        { "_eg_l1", "OP ENV Peak", 4, 99, 99 },
        { "_eg_l2", "OP ENV Mid", 5, 99, 99 },
        { "_eg_l3", "OP ENV Sustain", 6, 99, 99 },
        { "_eg_l4", "OP ENV End", 7, 99, 0 },
        { "_eg_break", "OP ENV Scale Key", 8, 99, 39 },
        { "_eg_ldepth", "OP ENV Low Depth", 9, 99, 0 },
        { "_eg_rdepth", "OP ENV High Depth", 10, 99, 0 },
        { "_eg_lcurve", "OP ENV Low Curve", 11, 3, 0, true },
        { "_eg_rcurve", "OP ENV High Curve", 12, 3, 0, true },
        { "_eg_rate_key", "OP ENV Key Rate", 13, 7, 0 },
        { "_eg_ams", "OP ENV Amp Mod", 14, 3, 0 },
        { "_eg_vel", "OP ENV Velocity", 15, 7, 0 },
        { "_eg_out", "OP ENV Level", 16, 99, 99 },
    } };
    return fields;
}

// For the whole voice: the pitch envelope and the LFO. At their defaults
// (pitch levels 50, depths 0) they move nothing.
inline const std::array<Field, 15>& voiceFields()
{
    static const std::array<Field, 15> fields { {
        { "opeg_pitch_r1", "OP PITCH Attack", 126, 99, 99 },
        { "opeg_pitch_r2", "OP PITCH Decay 1", 127, 99, 99 },
        { "opeg_pitch_r3", "OP PITCH Decay 2", 128, 99, 99 },
        { "opeg_pitch_r4", "OP PITCH Release", 129, 99, 99 },
        { "opeg_pitch_l1", "OP PITCH Peak", 130, 99, 50 },
        { "opeg_pitch_l2", "OP PITCH Mid", 131, 99, 50 },
        { "opeg_pitch_l3", "OP PITCH Sustain", 132, 99, 50 },
        { "opeg_pitch_l4", "OP PITCH End", 133, 99, 50 },
        { "opeg_lfo_speed", "OP LFO Rate", 137, 99, 35 },
        { "opeg_lfo_delay", "OP LFO Delay", 138, 99, 0 },
        { "opeg_lfo_pmd", "OP LFO Pitch Depth", 139, 99, 0 },
        { "opeg_lfo_amd", "OP LFO Amp Depth", 140, 99, 0 },
        { "opeg_lfo_sync", "OP LFO Retrig", 141, 1, 1 },
        { "opeg_lfo_wave", "OP LFO Shape", 142, 5, 0, true },
        { "opeg_lfo_pms", "OP LFO Pitch Sens", 143, 7, 3 },
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
