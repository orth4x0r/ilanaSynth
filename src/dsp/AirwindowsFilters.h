#pragma once

// Voice filters ported from Airwindows (Chris Johnson, https://www.airwindows.com,
// MIT license: see airwindows/LICENSE.txt). Each is one channel of one voice,
// set from a cutoff in Hz and a resonance 0..1 like the other models; the
// plugins' own knobs are fixed at the values noted.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

namespace Airwindows
{
// The encode/decode curve the Y filters wrap around their biquad
// (by torridgristle, MIT): y = 1 - (1 - x)^p on each polarity, clamped to
// +-1. A table per exponent (the plugins call pow per sample).
class PowCurve
{
public:
    explicit PowCurve (double exponent)
    {
        for (size_t i = 0; i < table.size(); ++i)
        {
            const auto x = (double) i / (double) (size - 1);
            table[i] = (float) (1.0 - std::pow (1.0 - x, exponent));
        }
    }

    double operator() (double x) const
    {
        const auto a = std::abs (x);
        if (a >= 1.0)
            return x > 0.0 ? 1.0 : -1.0;

        const auto position = a * (double) (size - 1);
        const auto index = (size_t) position;
        const auto fraction = (float) (position - (double) index);
        const auto y = (double) (table[index] + (table[index + 1] - table[index]) * fraction);
        return x > 0.0 ? y : -y;
    }

private:
    static constexpr int size = 4097;
    std::array<float, (size_t) size + 1> table {};
};

// A transposed direct form II biquad whose coefficients glide to a new set
// over the next 16 samples (the voice sets them every 16), as the plugins
// glide theirs over a host buffer.
struct GlideBiquad
{
    std::array<double, 5> from {}, to {}, now {};
    double s1 = 0.0, s2 = 0.0;
    int step = 0;
    static constexpr int steps = 16;

    void set (const std::array<double, 5>& target, bool jump)
    {
        from = jump ? target : now;
        to = target;
        step = jump ? steps : 0;
        if (jump)
            now = target;
    }

    double process (double x)
    {
        if (step < steps)
        {
            ++step;
            const auto t = (double) step / (double) steps;
            for (size_t i = 0; i < 5; ++i)
                now[i] = from[i] + (to[i] - from[i]) * t;
        }

        const auto y = x * now[0] + s1;
        s1 = x * now[1] - y * now[3] + s2;
        s2 = x * now[2] - y * now[4];
        return y;
    }

    void reset() { s1 = s2 = 0.0; }
};

// YLowpass, YHighpass, YBandpass and YNotch: a resonant two-pole filter
// between the encode and decode curves (RESEDGE), with fixed 20 kHz
// Butterworth low-passes on the way in and out. The plugins' resonant stage is
// a biquad whose coefficients glide over a buffer; here it is the same
// response computed as a zero-delay state-variable filter, because a voice's
// filter envelope moves the cutoff every 16 samples and a direct-form biquad
// overshoots under that (up to +3.7 dB peaks on the presets). Resonance 0..1
// sets the damping as the old SVF models did (Q = 1 / (2 - 2 r), so their
// linear response and every preset's tone balance carry over), topped at
// Q 40: the Y filters ring hard but never oscillate on their own.
class YFilter
{
public:
    enum class Mode { LowPass, BandPass, HighPass, Notch };

    struct Coefficients
    {
        double g = 0.0, k = 2.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        std::array<double, 5> fixed {};
    };

    // RESEDGE: the plugins' D knob, p = (D + 0.9)^4. At its default 0.1 the
    // curve is a straight line; above it the resonant peak gets its edge.
    static constexpr double resEdge = 0.25;
    // The voice runs hotter than a mix bus: the curve sees the signal at this
    // level (and the gain comes back after).
    static constexpr double headroom = 0.5;

    static double exponent() { return std::pow (resEdge + 0.9, 4.0); }

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        Coefficients c;
        const auto f = juce::jlimit (15.0, sampleRate * 0.45, cutoff) / sampleRate;
        const auto r = juce::jlimit (0.0, 1.0, resonance);
        c.g = std::tan (juce::MathConstants<double>::pi * f);
        c.k = juce::jmax (2.0 - 2.0 * r, 1.0 / 40.0);
        c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
        c.a2 = c.g * c.a1;
        c.a3 = c.g * c.a2;

        const auto fixedQ = 0.7071;
        const auto k = std::tan (juce::MathConstants<double>::pi * juce::jmin (20000.0, sampleRate * 0.45) / sampleRate);
        const auto norm = 1.0 / (1.0 + k / fixedQ + k * k);
        c.fixed = { k * k * norm, 2.0 * k * k * norm, k * k * norm, 2.0 * (k * k - 1.0) * norm, (1.0 - k / fixedQ + k * k) * norm };
        return c;
    }

    void setMode (Mode newMode) { mode = newMode; }

    void setCoefficients (const Coefficients& c)
    {
        coefficients = c;
        fixedIn.set (c.fixed, true);
        fixedOut.set (c.fixed, true);
    }

    void reset()
    {
        ic1 = ic2 = ic3 = ic4 = 0.0;
        fixedIn.reset();
        fixedOut.reset();
    }

    // 24 dB: the second stage sits inside the same encode/decode pair.
    float process (float input, bool slope24 = false)
    {
        auto x = encode (fixedIn.process ((double) input) * headroom);
        x = stage (x, ic1, ic2);
        if (slope24)
            x = stage (x, ic3, ic4);
        return (float) fixedOut.process (decode (x) / headroom);
    }

private:
    double stage (double x, double& s1, double& s2) const
    {
        const auto& c = coefficients;
        const auto v3 = x - s2;
        const auto v1 = c.a1 * s1 + c.a2 * v3;
        const auto v2 = s2 + c.a2 * s1 + c.a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;

        switch (mode)
        {
            case Mode::LowPass:  return v2;
            case Mode::BandPass: return v1; // peak gain Q, as the old band-pass
            case Mode::HighPass: return x - c.k * v1 - v2;
            case Mode::Notch:    return x - c.k * v1;
        }
        return x;
    }

    inline static const PowCurve encode { exponent() };
    inline static const PowCurve decode { 1.0 / exponent() };

    Mode mode = Mode::LowPass;
    Coefficients coefficients;
    double ic1 = 0.0, ic2 = 0.0, ic3 = 0.0, ic4 = 0.0;
    GlideBiquad fixedIn, fixedOut;
};
} // namespace Airwindows
