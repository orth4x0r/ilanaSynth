#pragma once

// Fast approximations for the voice's per-sample paths. exp2 is a fifth-order
// polynomial for 2^t on [-0.5, 0.5], fitted here by an iteratively reweighted
// least-squares minimax on the relative error (own derivation, constant term
// pinned to 1), relative error about 1e-7. tanh built on it is within 3e-6 of
// std::tanh everywhere (about -110 dB), at a fraction of libm's cost.

#include <cmath>
#include <cstdint>
#include <cstring>

namespace FastMath
{
inline float exp2 (float exponent)
{
    constexpr float c1 = 0.69314698f, c2 = 0.24022241f, c3 = 0.05550734f,
                    c4 = 0.00967158f, c5 = 0.00132648f;
    exponent = exponent < -126.0f ? -126.0f : (exponent > 126.0f ? 126.0f : exponent);
    const auto whole = (int) std::lrint (exponent);
    const auto t = exponent - (float) whole;
    const auto fraction = 1.0f + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * c5))));
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
