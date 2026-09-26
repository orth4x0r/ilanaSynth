#pragma once

#include <array>

// Oscillator 3 retains its historical "sub_" parameter IDs. The separate
// subosc_* oscillator is not part of this indexed oscillator bank.
namespace OscillatorIds
{
inline constexpr int count = 6;
inline constexpr std::array<const char*, count> prefixes { "osc1", "osc2", "sub", "osc4", "osc5", "osc6" };
} // namespace OscillatorIds
