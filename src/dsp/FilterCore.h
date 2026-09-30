#pragma once

// Building blocks for the filter models (FilterUnit.h, FilterModels2.h):
// a cheap accurate tanh and its secant gain, TPT one-poles, a 2x polyphase
// half-band oversampler, and the solved zero-delay-feedback cores shared by
// the ladder, OTA, diode and Korg-35 models.
//
// Nonlinear loops are solved per sample: the loop's memoryless saturation
// (where the circuit's feedback sum saturates) is solved exactly with a
// Newton step, and the stages' own saturation uses secant gains from the
// previous sample (Zavalishin, "The Art of VA Filter Design", 6.x; Pirkle's
// "cheap" nonlinear ZDF). A memoryless saturator adds no phase, so it limits
// a self-oscillation without pulling its pitch off the cutoff.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <complex>

namespace FilterCore
{
// tanh as a [7/6] Pade approximant (error < 2e-7 below 3, < 1e-3 at the
// clamp), clamped where it reaches 1.
inline double tanhApprox (double x)
{
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    const auto x2 = x * x;
    return x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0)));
}

// tanh(x) / x (the secant gain), 1 at 0.
inline double tanhOverX (double x)
{
    const auto a = std::abs (x);
    if (a > 4.97) return 1.0 / a;
    const auto x2 = x * x;
    return (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0)));
}

// d/dx tanh(x), from tanh(x).
inline double tanhSlope (double t) { return 1.0 - t * t; }

inline double prewarp (double sampleRate, double cutoff)
{
    return std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sampleRate * 0.45, cutoff) / sampleRate);
}

// 2x oversampling: a polyphase IIR half-band (two allpass paths, four
// coefficients; de Soras' design with transition 0.08: flat to 16 kHz at
// 48 kHz, more than 63 dB down on anything that would fold below 18 kHz).
class Halfband2x
{
public:
    void reset() { up0.fill (0.0); up1.fill (0.0); down0.fill (0.0); down1.fill (0.0); }

    // One input sample to two at the doubled rate.
    void upsample (double x, double& first, double& second)
    {
        first = section (section (x, c[0], up0[0], up0[1]), c[2], up0[2], up0[3]);
        second = section (section (x, c[1], up1[0], up1[1]), c[3], up1[2], up1[3]);
    }

    // Two samples at the doubled rate back to one.
    double downsample (double first, double second)
    {
        const auto a = section (section (second, c[0], down0[0], down0[1]), c[2], down0[2], down0[3]);
        const auto b = section (section (first, c[1], down1[0], down1[1]), c[3], down1[2], down1[3]);
        return 0.5 * (a + b);
    }

private:
    // First-order allpass (c + z^-1) / (1 + c z^-1); x1 / y1 are its memory.
    static double section (double x, double coefficient, double& x1, double& y1)
    {
        const auto y = coefficient * (x - y1) + x1;
        x1 = x;
        y1 = y;
        return y;
    }

    static constexpr double c[4] { 0.091994723790, 0.317108117416, 0.584460827036, 0.854180068706 };
    std::array<double, 4> up0 {}, up1 {}, down0 {}, down1 {};
};

// TPT one-pole: G = g / (1 + g).
struct OnePole
{
    double s = 0.0;
    double lowPass (double x, double bigG)
    {
        const auto v = (x - s) * bigG;
        const auto y = v + s;
        s = y + v;
        return y;
    }
    double highPass (double x, double bigG) { return x - lowPass (x, bigG); }
    void reset() { s = 0.0; }
};

// Solve u = sat (v - a u) for u, sat = drive-scaled tanh with ceiling
// 'level': u = level * tanh ((v - a u) / level). Monotone in u for a >= 0;
// from a linear-loop start, one Newton step is within 1e-4 of the root for
// a <= 0.5 (the ladders at any cutoff), two within 1e-3 for any a <= 4.
// Each Newton step folds tanh's Pade quotient and the step's own division
// into one division (the loop is latency-bound, and divisions dominate).
inline double solveLoop (double v, double a, double level)
{
    const auto inv = 1.0 / level;
    auto u = level * tanhApprox (v * inv / (1.0 + a));
    const auto steps = a <= 0.5 ? 1 : 2;
    for (int step = 0; step < steps; ++step)
    {
        const auto x = (v - a * u) * inv;
        if (std::abs (x) > 4.97)
        {
            // Past the clamp tanh is flat: u = +-level.
            u = x > 0.0 ? level : -level;
            continue;
        }
        // tanh (x) = n / d; f = u - level n / d; f' = 1 + a (1 - n^2 / d^2).
        const auto x2 = x * x;
        const auto n = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
        const auto d = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
        u -= (u * d - level * n) * d / (d * d + a * (d * d - n * n));
    }
    return u;
}

// Four one-pole stages with a solved feedback loop: the Moog ladder, the
// CEM3320 / SSM2040 OTA cascade, and the OTA band-pass (two high-pass
// stages). Each stage may saturate (the transistor or OTA pair: its
// cutoff falls with the current through it, a secant gain from the last
// sample), and the loop's input sum saturates.
class StageCascade
{
public:
    struct Settings
    {
        double g = 0.1;             // tan (pi fc / fs) at the rate it runs
        double k = 0.0;             // loop gain (4 self-oscillates the low-pass)
        bool positiveFeedback = false;
        bool highPassStages34 = false;
        double stageDrive = 0.0;    // 0: linear stages; ~0.3..1: saturating
        double loopLevel = 1.0;     // the input sum's clip level
    };

    void set (const Settings& s)
    {
        settings = s;
        linearBeta = 1.0 / (1.0 + s.g);
        linearBigG = s.g * linearBeta;
    }
    void reset() { state.fill (0.0); last.fill (0.0); }

    // Returns the four stage outputs in y and the loop input u.
    void process (double x, std::array<double, 4>& y, double& u)
    {
        const auto& s = settings;
        std::array<double, 4> bigG, beta;
        if (s.stageDrive > 0.0)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                const auto g = s.g * tanhOverX (s.stageDrive * last[i]);
                beta[i] = 1.0 / (1.0 + g);
                bigG[i] = g * beta[i];
            }
        }
        else
        {
            beta.fill (linearBeta);
            bigG.fill (linearBigG);
        }

        // The output as a + b u.
        auto a = 0.0, b = 1.0;
        for (size_t i = 0; i < 4; ++i)
        {
            if (s.highPassStages34 && i >= 2)
            {
                a = beta[i] * (a - state[i]);
                b = beta[i] * b;
            }
            else
            {
                a = bigG[i] * a + beta[i] * state[i];
                b = bigG[i] * b;
            }
        }

        const auto sign = s.positiveFeedback ? 1.0 : -1.0;
        // u = sat (x + sign k (a + b u))
        u = solveLoop (x + sign * s.k * a, -sign * s.k * b, s.loopLevel);

        auto in = u;
        for (size_t i = 0; i < 4; ++i)
        {
            const auto v = (in - state[i]) * bigG[i];
            const auto low = v + state[i];
            state[i] = low + v;
            const auto out = s.highPassStages34 && i >= 2 ? in - low : low;
            last[i] = in - low; // the current through the stage
            y[i] = out;
            in = out;
        }
    }

private:
    Settings settings;
    double linearBeta = 1.0, linearBigG = 0.0;
    std::array<double, 4> state {}, last {};
};

// The Korg-35 (MS-20) loop: an input one-pole (low- or high-pass), then a
// second one-pole of the same kind whose output is fed back through a
// one-pole of the other kind with gain K and a diode limiter:
//   low-pass:  u = LP1 (x) + sat (K HP (y)), y = LP2 (u)
//   high-pass: u = HP1 (x) + sat (K LP (y)), y = HP2 (u)
// which is 1 / (s^2 + (2 - K) s + 1) (or s^2 over it): it self-oscillates at
// K = 2 exactly at the cutoff. The limiter is solved per sample (Newton;
// the loop's local gain c = K G beta is at most 0.52, so it is monotone,
// and one step from the start below is within 3e-4 of the root).
class Korg35Core
{
public:
    void set (double g, double k, bool highPassMode, double limit)
    {
        bigG = g / (1.0 + g);
        beta = 1.0 / (1.0 + g);
        loopK = k;
        highPass = highPassMode;
        level = limit;
    }

    void reset() { in.reset(); forward.reset(); feedback.reset(); }

    double process (double x)
    {
        const auto y1 = highPass ? in.highPass (x, bigG) : in.lowPass (x, bigG);
        // The fed-back one-pole's output as c u + d.
        double c, d;
        if (highPass)
        {
            c = loopK * bigG * beta;
            d = loopK * beta * (feedback.s - bigG * forward.s);
        }
        else
        {
            c = loopK * bigG * beta;
            d = loopK * beta * (beta * forward.s - feedback.s);
        }

        // u = y1 + level tanh ((c u + d) / level)
        const auto inv = 1.0 / level;
        const auto linear = (y1 + d) / (1.0 - c);
        auto u = y1 + level * tanhApprox ((c * linear + d) * inv);
        const auto arg = (c * u + d) * inv;
        if (std::abs (arg) > 4.97)
        {
            u = y1 + (arg > 0.0 ? level : -level);
        }
        else
        {
            // tanh (arg) = n / m; one division per step (see solveLoop).
            const auto x2 = arg * arg;
            const auto n = arg * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
            const auto m = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
            u -= ((u - y1) * m - level * n) * m / (m * m - c * (m * m - n * n));
        }

        const auto y = highPass ? forward.highPass (u, bigG) : forward.lowPass (u, bigG);
        if (highPass)
            feedback.lowPass (y, bigG);
        else
            feedback.highPass (y, bigG);
        return y;
    }

private:
    double bigG = 0.1, beta = 0.9, loopK = 0.0, level = 1.0;
    bool highPass = false;
    OnePole in, forward, feedback;
};

// The diode ladder: four capacitors coupled by diode pairs (each node sees
// its neighbours), so the stages load each other. Linearised it is
// Stinchcombe's H(s) = 1 / D(s) with D = s^4 + 7s^3 + 15s^2 + 10s + 1 for
// equal capacitors, and D = s^4 + 6.727s^3 + 14.142s^2 + 9.514s + 1 with the
// top capacitor halved as in the TB-303 ("Diode ladder filters", 2008).
// Each diode pair's conductance saturates (a secant gain from the last
// sample), the loop's input sum saturates, and an optional one-pole
// high-pass sits in the feedback (the 303's coupling capacitor). Solved per
// sample (tridiagonal, Thomas) with zero-delay feedback.
class DiodeLadderCore
{
public:
    struct Settings
    {
        double h = 0.1;            // g = tan (pi fc / fs) scaled to the core
        double k = 0.0;            // feedback gain
        bool halfTopCap = false;   // the TB-303's arrangement
        double feedbackHpG = 0.0;  // TPT G of the feedback high-pass (0 = none)
        double diodeDrive = 0.5;   // conductance saturation scale
        double loopLevel = 1.0;
    };

    void set (const Settings& s) { settings = s; }
    void reset() { state.fill (0.0); diff.fill (0.0); hp.reset(); }

    // Node voltages out (y[3] is the ladder's output).
    void process (double x, std::array<double, 4>& y)
    {
        const auto& p = settings;
        // Conductances c1 (input to node 1) .. c4 (node 3 to node 4).
        std::array<double, 4> c;
        for (size_t i = 0; i < 4; ++i)
            c[i] = tanhOverX (p.diodeDrive * diff[i]);

        // Trapezoidal step: (I - h Cinv L) v = s + h Cinv c1 u e1, with L
        // the ladder's conductance matrix. Rows scaled by h / C_i.
        const auto h4 = p.halfTopCap ? 2.0 * p.h : p.h;
        const double hs[4] { p.h, p.h, p.h, h4 };
        std::array<double, 4> lower {}, diag {}, upper {};
        diag[0] = 1.0 + hs[0] * (c[0] + c[1]);  upper[0] = -hs[0] * c[1];
        lower[1] = -hs[1] * c[1]; diag[1] = 1.0 + hs[1] * (c[1] + c[2]); upper[1] = -hs[1] * c[2];
        lower[2] = -hs[2] * c[2]; diag[2] = 1.0 + hs[2] * (c[2] + c[3]); upper[2] = -hs[2] * c[3];
        lower[3] = -hs[3] * c[3]; diag[3] = 1.0 + hs[3] * c[3];

        // Two right-hand sides: the states (u = 0) and a unit u.
        std::array<double, 4> ra { state[0], state[1], state[2], state[3] }, rb { hs[0] * c[0], 0.0, 0.0, 0.0 };
        std::array<double, 4> cp {};
        {
            auto m = 1.0 / diag[0];
            cp[0] = upper[0] * m;
            ra[0] *= m;
            rb[0] *= m;
            for (size_t i = 1; i < 4; ++i)
            {
                m = 1.0 / (diag[i] - lower[i] * cp[i - 1]);
                cp[i] = upper[i] * m;
                ra[i] = (ra[i] - lower[i] * ra[i - 1]) * m;
                rb[i] = (rb[i] - lower[i] * rb[i - 1]) * m;
            }
            for (int i = 2; i >= 0; --i)
            {
                ra[(size_t) i] -= cp[(size_t) i] * ra[(size_t) i + 1];
                rb[(size_t) i] -= cp[(size_t) i] * rb[(size_t) i + 1];
            }
        }

        // Feedback: y4 (high-passed when set) back to the input, negative.
        auto fa = ra[3], fb = rb[3];
        if (p.feedbackHpG > 0.0)
        {
            const auto g = p.feedbackHpG;
            // hp = y - (g (y - s) + s) = (1 - g) (y - s)
            fa = (1.0 - g) * (fa - hp.s);
            fb = (1.0 - g) * fb;
        }
        const auto u = solveLoop (x - p.k * fa, p.k * fb, p.loopLevel);

        for (size_t i = 0; i < 4; ++i)
        {
            const auto v = ra[i] + rb[i] * u;
            y[i] = v;
            state[i] = 2.0 * v - state[i];
        }
        if (p.feedbackHpG > 0.0)
            hp.lowPass (y[3], p.feedbackHpG);

        diff[0] = u - y[0];
        diff[1] = y[0] - y[1];
        diff[2] = y[1] - y[2];
        diff[3] = y[2] - y[3];
    }

    // The linear core's response at s (normalised to the stage frequency):
    // from the same tridiagonal model the processor runs.
    static std::complex<double> coreResponse (std::complex<double> s, bool halfTopCap)
    {
        // Node equations: C_i s v_i = sum of neighbour conductances.
        const double caps[4] { 1.0, 1.0, 1.0, halfTopCap ? 0.5 : 1.0 };
        // Solve (s C - L) v = e1 u for v4 / u with u = 1 by elimination.
        std::complex<double> d[4], up[3], lo[4], r[4];
        d[0] = s * caps[0] + 2.0; up[0] = -1.0; r[0] = 1.0;
        lo[1] = -1.0; d[1] = s * caps[1] + 2.0; up[1] = -1.0; r[1] = 0.0;
        lo[2] = -1.0; d[2] = s * caps[2] + 2.0; up[2] = -1.0; r[2] = 0.0;
        lo[3] = -1.0; d[3] = s * caps[3] + 1.0; r[3] = 0.0;
        for (int i = 1; i < 4; ++i)
        {
            const auto m = lo[i] / d[i - 1];
            d[i] -= m * up[i - 1];
            r[i] -= m * r[i - 1];
        }
        return r[3] / d[3];
    }

private:
    Settings settings;
    std::array<double, 4> state {}, diff {};
    OnePole hp;
};
} // namespace FilterCore
