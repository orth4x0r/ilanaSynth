#pragma once

// Fast approximations for the voice's per-sample paths. exp2's polynomial is
// Vital's (futils::exp2, Matt Tytel, GPL-3; the synth is for personal use,
// see HANDOFF.md): a fifth-order fit of 2^t on [-0.5, 0.5], relative error
// about 2e-7. tanh built on it is within 3e-6 of std::tanh everywhere
// (about -110 dB), at a fraction of libm's cost.

#include <cmath>
#include <cstdint>
#include <cstring>

namespace FastMath
{
inline float exp2 (float exponent)
{
    constexpr float c1 = 16970.0f / 24483.0f, c2 = 1960.0f / 8161.0f, c3 = 1360.0f / 24483.0f,
                    c4 = 80.0f / 8161.0f, c5 = 32.0f / 24483.0f;
    exponent = exponent < -126.0f ? -126.0f : (exponent > 126.0f ? 126.0f : exponent);
    const auto whole = (int) std::lrint (exponent);
    const auto t = exponent - (float) whole;
    const auto cubic = t * (t * (t * c5 + c4) + c3) + c2;
    const auto fraction = t * (t * cubic + c1) + 1.0f;
    const auto bits = (std::uint32_t) (whole + 127) << 23;
    float power;
    std::memcpy (&power, &bits, sizeof (power));
    return power * fraction;
}

inline float tanh (float x)
{
    // tanh saturates to +-1 within float precision past 9.
    x = x < -9.0f ? -9.0f : (x > 9.0f ? 9.0f : x);
    return 1.0f - 2.0f / (exp2 (2.8853900817779268f * x) + 1.0f);
}
} // namespace FastMath
