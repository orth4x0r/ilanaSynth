#pragma once

#include <array>

// Oscillator 3 retains its historical "sub_" parameter IDs. The separate
// subosc_* oscillator is not part of this indexed oscillator bank.
namespace OscillatorIds
{
inline constexpr int count = 6;
inline constexpr std::array<const char*, count> prefixes { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };
} // namespace OscillatorIds

// An oscillator's type (the oscN_mode choice; "sub_mode" for OSC 3). The
// index is what is saved: FM / DX7 was appended (2026-10-06), so the older
// five keep their meaning. An FM / DX7 oscillator is the wavetable engine
// with the operator's controls (ratio or fixed tuning, the Operator EG, the
// feedback type): it renders exactly as a wavetable oscillator with the same
// settings, the type only says which card it gets. Patches from before the
// type existed load their operators (a wavetable tuned by ratio or fixed Hz,
// or on the Operator EG) as FM / DX7 (migrateOperatorModes).
namespace OscMode
{
inline constexpr int wavetable = 0, physical = 1, sample = 2, granular = 3, live = 4, fmOperator = 5, count = 6;

inline constexpr std::array<const char*, count> names { "Wavetable", "Physical", "Sample", "Granular", "Live", "FM / DX7" };

// The modes the wavetable engine plays.
inline constexpr bool playsWavetable (int mode) { return mode == wavetable || mode == fmOperator; }

// The engine's mode for a saved one: FM / DX7 renders as a wavetable.
inline constexpr int engineMode (int mode) { return mode == fmOperator ? wavetable : mode; }
} // namespace OscMode
