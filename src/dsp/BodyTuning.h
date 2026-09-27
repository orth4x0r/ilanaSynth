#pragma once

#include <array>

// Modal ratios and relative strike strengths. The fitting script records the
// reference and can replace these constants without changing the DSP.
namespace BodyTuning
{
constexpr int modes = 8;
struct Shape
{
    std::array<float, modes> ratio;
    std::array<float, modes> gain;
    std::array<float, modes> lifetime;
};

// Free bar, plate, tuned bell, and curved shell. Ratios are relative to the
// pitch of the first prominent mode, rather than integer harmonics.
constexpr Shape shapes[] {
    { { 1.0f, 3.0f, 5.404f, 8.933f, 13.34f, 18.64f, 24.7f, 31.5f },
      { 1.0f, 0.1f, 0.012f, 0.008f, 0.005f, 0.003f, 0.002f, 0.001f },
      { 1.0f, 0.75f, 0.42f, 0.28f, 0.2f, 0.15f, 0.12f, 0.1f } },
    { { 1.0f, 1.606f, 2.0f, 2.091f, 2.606f, 5.182f, 5.995f, 6.48f },
      { 1.0f, 0.11f, 0.57f, 0.094f, 0.107f, 0.35f, 0.339f, 0.473f },
      { 1.0f, 0.76f, 0.40f, 0.22f, 0.46f, 0.32f, 0.27f, 0.19f } },
    { { 1.0f, 3.289f, 7.223f, 11.309f, 11.974f, 16.407f, 18.1f, 20.5f },
      { 1.0f, 0.5f, 0.6f, 0.9f, 0.9f, 1.5f, 0.2f, 0.1f },
      { 1.0f, 0.43f, 0.21f, 0.32f, 0.20f, 0.18f, 0.12f, 0.1f } },
    { { 1.0f, 1.27f, 1.81f, 2.42f, 3.16f, 4.08f, 5.12f, 6.32f },
      { 1.0f, 0.1f, 0.06f, 0.04f, 0.03f, 0.02f, 0.01f, 0.01f },
      { 1.0f, 0.7f, 0.5f, 0.4f, 0.3f, 0.2f, 0.15f, 0.1f } }
};
} // namespace BodyTuning
