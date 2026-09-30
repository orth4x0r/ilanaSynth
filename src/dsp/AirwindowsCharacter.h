#pragma once

// Airwindows character filters as mono voice filter models.
//
// The filter algorithms are by Chris Johnson (airwindows.com), MIT licence
// (see airwindows/LICENSE.txt): ZAcidLowpass, ZLowpass2, ZHighpass2,
// ZBandpass2, XLowpass, YNotLowpass, Holt2, AngleFilter and Pear2. They are
// ported by hand from the official repository's processReplacing code:
// the per-sample maths is Chris's, as he wrote it, with these changes for a
// synth voice (one channel of one voice):
//   * mono, and no dither / fpd noise (a tiny alternating offset keeps
//     denormals out instead);
//   * every model has  set (sampleRate, cutoffHz, resonance0to1, morph0to1)
//     which the voice calls every 16 samples. Coefficients are computed
//     there (never per sample) and glided linearly over the next 16 samples,
//     the way the plugins glide over a host buffer;
//   * pow() and sin() in the per-sample path are replaced by exact-enough
//     tables / polynomials (detail::fastPow, detail::fastSin);
//   * the plugins' knobs are mapped as documented on each class: cutoff Hz is
//     turned into the plugin's frequency knob by inverting its formula and
//     clamped to the knob's range, resonance and morph drive the plugin's
//     own character knobs, the rest are fixed (values listed per class);
//   * cascaded stages and drive move each plugin's -3 dB point away from its
//     knob, so the cutoff is corrected by measured tables (edge[] and
//     friends) and the output by a makeup table over resonance (level[],
//     dB): a saw at -12 dBFS, cutoff 1 kHz, resonance 0.3 comes out within
//     a dB or so of a plain 2-pole filter of the same kind.
// Small safety clamps (marked SAFETY) keep the models finite where a plugin
// relies on its host's sane knob ranges.
//
// The last model, Disperser, is not an Airwindows plugin: a tuneable chain
// of second-order all-pass sections.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Airwindows
{
namespace detail
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr int kGlideSamples = 16;

inline double clampd (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Linear glide of N values towards new targets over the next 16 samples.
// The first target after reset() is taken at once (nothing to glide from).
template <int N>
struct Glide
{
    double cur[N] {}, step[N] {}, target[N] {};
    int left = 0;
    bool primed = false;

    void reset() { left = 0; primed = false; }
    void set (const double (&t)[N])
    {
        for (int i = 0; i < N; ++i)
        {
            target[i] = t[i];
            if (! primed) cur[i] = t[i];
            step[i] = (t[i] - cur[i]) * (1.0 / kGlideSamples);
        }
        left = primed ? kGlideSamples : 0;
        primed = true;
    }
    inline void tick()
    {
        if (left > 0)
        {
            for (int i = 0; i < N; ++i)
                cur[i] += step[i];
            if (--left == 0)
                for (int i = 0; i < N; ++i)
                    cur[i] = target[i];
        }
    }
};

// sin(x) for |x| <= pi/2 (error below 1e-7).
inline double sinHalfPi (double x)
{
    const double x2 = x * x;
    return x * (1.0 + x2 * (-1.0 / 6.0 + x2 * (1.0 / 120.0 + x2 * (-1.0 / 5040.0 + x2 * (1.0 / 362880.0 + x2 * (-1.0 / 39916800.0))))));
}

// sin(x) for any x.
inline double fastSin (double x)
{
    double r = x - kTwoPi * std::floor (x * (1.0 / kTwoPi) + 0.5);
    if (r > 0.5 * kPi) r = kPi - r;
    else if (r < -0.5 * kPi) r = -kPi - r;
    return sinHalfPi (r);
}

// pow (y, p) for y in [0, 1] and p > 0 through log2 / exp2 tables with
// linear interpolation (relative error about 1e-6).
struct PowTables
{
    static constexpr int size = 1024;
    double logT[size + 1];
    double expT[size + 1];
    PowTables()
    {
        for (int i = 0; i <= size; ++i)
        {
            logT[i] = std::log2 (1.0 + (double) i / size);
            expT[i] = std::exp2 ((double) i / size);
        }
    }
};
inline const PowTables& powTables()
{
    static const PowTables tables;
    return tables;
}

inline double fastLog2 (const PowTables& t, double y) // y > 0, normal
{
    std::uint64_t bits;
    std::memcpy (&bits, &y, sizeof bits);
    const int exponent = (int) ((bits >> 52) & 0x7ff) - 1023;
    const std::uint64_t mantissa = bits & 0xfffffffffffffULL;
    const int idx = (int) (mantissa >> 42);
    const double frac = (double) (mantissa & ((1ULL << 42) - 1)) * (1.0 / (double) (1ULL << 42));
    return exponent + t.logT[idx] + (t.logT[idx + 1] - t.logT[idx]) * frac;
}

inline double fastExp2 (const PowTables& t, double v) // v <= 0 expected
{
    if (v < -1000.0) return 0.0;
    const double fl = std::floor (v);
    const double f = (v - fl) * PowTables::size;
    const int idx = (int) f;
    const double frac = f - idx;
    const double m = t.expT[idx] + (t.expT[idx + 1] - t.expT[idx]) * frac;
    const std::uint64_t bits = (std::uint64_t) ((int) fl + 1023) << 52;
    double scale;
    std::memcpy (&scale, &bits, sizeof scale);
    return m * scale;
}

inline double fastPow (const PowTables& t, double y, double p)
{
    if (y < 1e-300) return 0.0;
    return fastExp2 (t, p * fastLog2 (t, y));
}

// Airwindows' direct form I biquad (DF1) with the plugin's coefficients.
struct Df1
{
    double a0 = 1, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void lowpass (double f, double q) // f = frequency / sample rate
    {
        f = clampd (f, 1e-5, 0.45);
        const double k = std::tan (kPi * f);
        const double norm = 1.0 / (1.0 + k / q + k * k);
        a0 = k * k * norm;
        a1 = 2.0 * a0;
        a2 = a0;
        b1 = 2.0 * (k * k - 1.0) * norm;
        b2 = (1.0 - k / q + k * k) * norm;
    }
    inline double process (double x)
    {
        const double y = a0 * x + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void clear() { x1 = x2 = y1 = y2 = 0.0; }
};

// Piecewise-linear lookup of five evenly spaced values over x in 0..1.
inline double interp5 (const double (&t)[5], double x)
{
    const double pos = clampd (x, 0.0, 1.0) * 4.0;
    const int i = pos >= 4.0 ? 3 : (int) pos;
    return t[i] + (t[i + 1] - t[i]) * (pos - i);
}

// set() is called every block whether or not anything moved; identical
// arguments change nothing, so skip the maths.
struct SetCache
{
    double v[4] {};
    bool valid = false;
    bool same (double a, double b, double c, double d)
    {
        if (valid && v[0] == a && v[1] == b && v[2] == c && v[3] == d)
            return true;
        v[0] = a; v[1] = b; v[2] = c; v[3] = d;
        valid = true;
        return false;
    }
    void reset() { valid = false; }
};

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

inline double tinyFlip (double& t) { const double v = t; t = -t; return v; }
constexpr double kTiny = 1e-20;
} // namespace detail

// ---------------------------------------------------------------------------
// Z filters: ZLowpass2 / ZHighpass2 / ZBandpass2. One clipped biquad (up to
// four in series, faded in progressively by Poles) between an input drive
// and Airwindows' op-amp stage (sub-sonic IIR high-pass, two 15.5 kHz
// low-pass biquads with a soft clip between).
//
// Mapping: cutoff -> Freq (inverted: LP  Freq = B^3 *18930/sr+0.00162,
//                          HP  Freq = B^4 * 9500/sr+0.00076,
//                          BP  Freq = B^4 *14300/sr+0.00079, B clamped to 0..1,
//                          the cutoff first divided by the measured -3 dB
//                          point of the Poles setting);
// morph -> Poles (D 0.5..1: one biquad at 0, four at 1, faded in between);
// resonance -> Input drive (A 0.10..0.24, i.e. (A*10)^4 = 1x..33x into the clip).
// Fixed: Output C = 1.0 (x10 before makeup), Dry/Wet stays 100% wet.
class ZCore
{
public:
    enum class Mode { lowpass, highpass, bandpass };

    void reset()
    {
        glide.reset();
        cache.reset();
        for (auto& s : stage) { s.s1 = s.s2 = 0.0; }
        iir = 0.0;
        fixA.clear();
        fixB.clear();
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        if (sampleRate != lastRate)
        {
            lastRate = sampleRate;
            const double os = sampleRate / 44100.0;
            iirAmount = 0.00069 / os;
            fixA.lowpass (15500.0 / sampleRate, 0.935);
            fixB = fixA;
        }
        // cascaded stages pull the -3 dB point down: measured, divided out
        static const double lowEdge[5] = { 1.014, 0.724, 0.710, 0.696, 0.669 };
        static const double highEdge[5] = { 0.812, 1.010, 0.970, 0.933, 0.933 };
        static const double bandEdge[5] = { 1.016, 0.987, 0.987, 0.987, 0.987 };
        const double edge = interp5 (mode == Mode::lowpass ? lowEdge : (mode == Mode::highpass ? highEdge : bandEdge), (double) morph);
        const double f = clampd (cutoffHz / edge, 1.0, sampleRate * 0.45) / sampleRate;
        double knob, freq, reso, clipFactor;
        switch (mode)
        {
            case Mode::lowpass:
                knob = std::cbrt (clampd ((f - 0.00162) * sampleRate / 18930.0, 0.0, 1.0));
                freq = ((knob * knob * knob * 18930.0) / sampleRate) + 0.00162;
                clipFactor = 1.212 - ((1.0 - knob) * 0.496);
                reso = 0.7071;
                break;
            case Mode::highpass:
                knob = std::sqrt (std::sqrt (clampd ((f - 0.00076) * sampleRate / 9500.0, 0.0, 1.0)));
                freq = ((knob * knob * knob * knob * 9500.0) / sampleRate) + 0.00076;
                clipFactor = 1.0;
                reso = 1.0;
                break;
            default:
                knob = std::sqrt (std::sqrt (clampd ((f - 0.00079) * sampleRate / 14300.0, 0.0, 1.0)));
                freq = ((knob * knob * knob * knob * 14300.0) / sampleRate) + 0.00079;
                clipFactor = 1.0 - ((1.0 - knob) * 0.304);
                reso = 0.314;
                break;
        }
        freq = clampd (freq, 1e-5, 0.45); // SAFETY: tan() range at low sample rates
        const double k = std::tan (kPi * freq);
        const double norm = 1.0 / (1.0 + k / reso + k * k);
        double a0, a1, a2;
        if (mode == Mode::lowpass) { a0 = k * k * norm; a1 = 2.0 * a0; a2 = a0; }
        else if (mode == Mode::highpass) { a0 = norm; a1 = -2.0 * a0; a2 = a0; }
        else { a0 = k / reso * norm; a1 = 0.0; a2 = -a0; }

        const double driveKnob = 0.10 + 0.14 * clampd (resonance, 0.0, 1.0);
        double inTrim = driveKnob * 10.0;
        inTrim *= inTrim;
        inTrim *= inTrim;
        const double poleKnob = 0.5 + 0.5 * clampd ((double) morph, 0.0, 1.0);
        static const double lowLevel[5] = { -6.81, -17.24, -22.10, -23.08, -23.43 };
        static const double highLevel[5] = { -6.46, -16.88, -18.46, -18.48, -18.57 };
        static const double bandLevel[5] = { -11.04, -21.45, -24.36, -24.96, -25.52 };
        const double makeup = dbToGain (interp5 (mode == Mode::lowpass ? lowLevel : (mode == Mode::highpass ? highLevel : bandLevel), resonance));
        const double t[10] = { a0, a1, a2, 2.0 * (k * k - 1.0) * norm, (1.0 - k / reso + k * k) * norm,
                               inTrim, poleKnob * poleKnob, (0.1 + 3.712 * freq) / clipFactor, 1.0 / clipFactor, 10.0 * makeup };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const auto& g = glide.cur;
        const double a0 = g[0], a1 = g[1], a2 = g[2], b1 = g[3], b2 = g[4];
        const double inTrim = g[5], wet = g[6], trimOverClip = g[7], invClip = g[8];

        double dWet = wet * 4.0, cWet = 1.0, bWet = 1.0;
        if (dWet < 1.0) { bWet = 0.0; cWet = 0.0; dWet = 0.0; }
        else if (dWet < 2.0) { bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0; }
        else if (dWet < 3.0) { cWet = dWet - 2.0; dWet = 0.0; }
        else dWet -= 3.0;

        double x = (double) input + tinyFlip (tiny);
        x *= inTrim;
        x = x > 1.0 ? 1.0 : (x < -1.0 ? -1.0 : x);
        x *= trimOverClip;

        double dry = runStage (stage[0], x, a0, a1, a2, b1, b2);
        x = dry;
        const double weights[3] = { bWet, cWet, dWet };
        for (int i = 0; i < 3; ++i)
        {
            auto& s = stage[i + 1];
            if (weights[i] > 0.0)
            {
                x *= invClip;
                const double out = runStage (s, x, a0, a1, a2, b1, b2);
                x = out * weights[i] + dry * (1.0 - weights[i]);
                dry = x;
            }
            else
                s.s1 = s.s2 = 0.0;
        }
        x *= invClip;

        // op-amp stage
        if (std::fabs (iir) < 1.18e-37) iir = 0.0;
        iir = iir * (1.0 - iirAmount) + x * iirAmount;
        x -= iir;
        x = fixA.process (x);
        x = x > 1.0 ? 1.0 : (x < -1.0 ? -1.0 : x);
        x -= x * x * x * x * x * 0.1768;
        x = fixB.process (x);
        return (float) (x * g[9]);
    }

protected:
    explicit ZCore (Mode m) : mode (m) {}

private:
    struct Stage { double s1 = 0.0, s2 = 0.0; };
    static inline double runStage (Stage& s, double in, double a0, double a1, double a2, double b1, double b2)
    {
        double out = in * a0 + s.s1;
        out = out > 1.0 ? 1.0 : (out < -1.0 ? -1.0 : out);
        s.s1 = in * a1 - out * b1 + s.s2;
        s.s2 = in * a2 - out * b2;
        return out;
    }

    Mode mode;
    double lastRate = 0.0, iirAmount = 0.00069, iir = 0.0, tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<10> glide;
    Stage stage[4];
    detail::Df1 fixA, fixB;
};

class ZLowpass : public ZCore { public: ZLowpass() : ZCore (Mode::lowpass) {} };
class ZHighpass : public ZCore { public: ZHighpass() : ZCore (Mode::highpass) {} };
class ZBandpass : public ZCore { public: ZBandpass() : ZCore (Mode::bandpass) {} };

// ---------------------------------------------------------------------------
// ZAcidLowpass ("AW Acid"): a box-filter decimator with a cubic Bezier
// through the last four averages (the squelch), then the op-amp stage.
//
// Mapping: cutoff -> Freq (inverted: rate = A^3/os / (2/os^(0.5-(os-1)*0.0375)),
// A clamped to 0..1, so the top is about 2.8 kHz; the rate wanted is
// fc / (0.44 sr) scaled by a measured factor 2.4..4.5 over Over);
// morph -> Meltdown (D 0..1: the fresh input mixed into each Bezier point);
// resonance -> Over 0..0.5 (how much of the high remainder is kept: the edge)
// and Input drive (E 0.10..0.15, (E*10)^4 = 1x..5x).
// Fixed: Under C = 1.0, Output F = 1.0.
class AcidLowpass
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        cycle = samp = 0.0;
        pt[0] = pt[1] = pt[2] = pt[3] = 0.0;
        iir = 0.0;
        fixA.clear();
        fixB.clear();
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        const double os = sampleRate / 44100.0;
        if (sampleRate != lastRate)
        {
            lastRate = sampleRate;
            iirAmount = 0.00069 / os;
            fixA.lowpass (15500.0 / sampleRate, 0.935);
            fixB = fixA;
        }
        const double denom = os * (2.0 / std::pow (os, 0.5 - ((os - 1.0) * 0.0375)));
        const double res = clampd (resonance, 0.0, 1.0);
        static const double rateScale[5] = { 2.4, 3.3, 4.1, 4.3, 4.45 }; // measured: box edge, Bezier and Over
        const double rateWanted = clampd (cutoffHz, 1.0, sampleRate * 0.45) / (0.44 * sampleRate) * interp5 (rateScale, res);
        const double knob = std::cbrt (clampd (rateWanted * denom, 0.0, 1.0));
        double rate = knob * knob * knob / denom;
        rate = clampd (rate, 0.0001, 1.0);
        const double half = rate * 0.5;
        const double acidTrim = 1.0 - std::pow (half, 1.0 / half);
        double inTrim = (0.10 + 0.05 * res) * 10.0;
        inTrim *= inTrim;
        inTrim *= inTrim;
        static const double level[5] = { 0.11, -3.94, -7.60, -10.61, -12.60 };
        const double t[6] = { rate, acidTrim, 0.5 * res, clampd ((double) morph, 0.0, 1.0), inTrim, dbToGain (interp5 (level, res)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const double cutoff = glide.cur[0], acidTrim = glide.cur[1], over = glide.cur[2];
        const double meltdown = glide.cur[3], inTrim = glide.cur[4];
        double x = (double) input + tinyFlip (tiny);

        cycle += cutoff;
        samp += x * cutoff;
        if (cycle > 1.0)
        {
            cycle -= 1.0;
            pt[3] = pt[2]; pt[2] = pt[1]; pt[1] = pt[0];
            pt[0] = samp * (1.0 - meltdown) + x * meltdown;
            samp = 0.0;
        }
        const double X = cycle * acidTrim;
        const double o = 1.0 - X;
        const double mid = pt[3] * o * o * o + pt[2] * 3.0 * o * o * X + pt[1] * 3.0 * o * X * X + pt[0] * X * X * X;
        x = mid + (x - mid) * over; // Under = 1.0
        x *= inTrim;

        // op-amp stage
        if (std::fabs (iir) < 1.18e-37) iir = 0.0;
        iir = iir * (1.0 - iirAmount) + x * iirAmount;
        x -= iir;
        x = fixA.process (x);
        x = x > 1.0 ? 1.0 : (x < -1.0 ? -1.0 : x);
        x -= x * x * x * x * x * 0.1768;
        x = fixB.process (x);
        return (float) (x * glide.cur[5]);
    }

private:
    double lastRate = 0.0, iirAmount = 0.00069, iir = 0.0, tiny = detail::kTiny;
    double cycle = 0.0, samp = 0.0, pt[4] {};
    detail::SetCache cache;
    detail::Glide<6> glide;
    detail::Df1 fixA, fixB;
};

// ---------------------------------------------------------------------------
// XLowpass ("AW X LP"): a sine-clipped biquad low-pass (up to five in
// series, faded in by Nuke).
//
// Mapping: cutoff -> Freq (inverted: f = B^2 * 20000 / sr, min 0.001, the
// cutoff first divided by the measured -3 dB point of the stage count);
// resonance -> Nuke (C 0..1: 0 is one stage, 0.25 two, ... 1.0 five stages,
// clipped harder as it rises).
// Fixed: Gain A = 0.5 (unity), Dry/Wet D = 1.0.
class XLowpass
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        for (auto& s : stage) s = Stage();
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) 0.0f))
            return;
        using namespace detail;
        const double nuke = clampd (resonance, 0.0, 1.0);
        static const double edge[5] = { 1.03, 0.627, 0.359, 0.282, 0.344 }; // stages pull the -3 dB point down
        const double f0 = clampd (cutoffHz / interp5 (edge, nuke), 1.0, sampleRate * 0.45) / sampleRate;
        const double knob = std::sqrt (clampd (f0 * sampleRate / 20000.0, 0.0, 1.0));
        double f = knob * knob * 20000.0 / sampleRate;
        if (f < 0.001) f = 0.001;
        f = clampd (f, 0.001, 0.45); // SAFETY: tan() range at low sample rates
        const double compensation = std::sqrt (f) * 6.4;
        const double clipFactor = 1.0 + (f * f * nuke * 32.0);
        const double k = std::tan (kPi * f);
        const double norm = 1.0 / (1.0 + k / 0.7071 + k * k);
        const double a0 = k * k * norm;
        static const double level[5] = { 0.93, 1.10, -1.06, -6.37, -9.04 };
        const double t[9] = { a0, 2.0 * a0, a0, 2.0 * (k * k - 1.0) * norm, (1.0 - k / 0.7071 + k * k) * norm,
                              clipFactor, 1.0 / compensation, nuke, dbToGain (interp5 (level, nuke)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const auto& g = glide.cur;
        const double clipFactor = g[5], invComp = g[6], nuke = g[7];
        double x = (double) input + tinyFlip (tiny);

        double weights[4] = { 1.0, 1.0, 1.0, nuke * 4.0 - 3.0 };
        const double dWet = nuke * 4.0;
        if (dWet < 1.0) { weights[0] = dWet; weights[1] = weights[2] = weights[3] = 0.0; }
        else if (dWet < 2.0) { weights[1] = dWet - 1.0; weights[2] = weights[3] = 0.0; }
        else if (dWet < 3.0) { weights[2] = dWet - 2.0; weights[3] = 0.0; }
        else weights[3] = dWet - 3.0;

        x = runStage (stage[0], x, g, clipFactor, invComp);
        for (int i = 0; i < 4; ++i)
        {
            if (weights[i] > 0.0)
            {
                const double out = runStage (stage[i + 1], x, g, clipFactor, invComp);
                x = out * weights[i] + x * (1.0 - weights[i]);
            }
            else
                stage[i + 1] = Stage();
        }
        return (float) (x * g[8]);
    }

private:
    struct Stage { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    static inline double runStage (Stage& s, double in, const double* g, double clipFactor, double invComp)
    {
        double out = g[0] * in + g[1] * s.x1 + g[2] * s.x2 - g[3] * s.y1 - g[4] * s.y2;
        s.x2 = s.x1; s.x1 = in; s.y2 = s.y1;
        out *= clipFactor;
        out = out > 1.57079633 ? 1.57079633 : (out < -1.57079633 ? -1.57079633 : out);
        s.y1 = detail::sinHalfPi (out);
        return out * invComp;
    }

    double tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<9> glide;
    Stage stage[5];
};

// ---------------------------------------------------------------------------
// YNotLowpass ("AW YNot LP"): a resonant low-pass sandwiched between a
// power-law encode and its inverse (torridgristle), so the resonance
// distorts its own edges.
//
// Mapping: cutoff -> Freq (inverted: f = B^3 * 20000, min 15 Hz);
// resonance -> Reson8 (C 0..1: Q = C^2 * 15 + 0.5571);
// morph -> ResEdge (D 0.1..0.6: exponent (D+0.9)^4 = 1 (neutral) .. 5.1).
// Fixed: Gain A = 0.1 (x1), Output E = 1.0, Dry/Wet F = 1.0; the 20 kHz
// Butterworth ultrasonic filters before and after are the plugin's.
class YNotLowpass
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        s1 = s2 = 0.0;
        fixA.clear();
        fixB.clear();
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        if (sampleRate != lastRate)
        {
            lastRate = sampleRate;
            fixA.lowpass (20000.0 / sampleRate, 0.7071);
            fixB = fixA;
        }
        const double fc = clampd (cutoffHz, 15.0, sampleRate * 0.45);
        const double knob = std::cbrt (clampd (fc / 20000.0, 0.0, 1.0));
        double freq = knob * knob * knob * 20000.0;
        if (freq < 15.0) freq = 15.0;
        freq = clampd (freq / sampleRate, 1e-5, 0.45);
        const double c = clampd (resonance, 0.0, 1.0);
        const double q = (c * c * 15.0) + 0.5571;
        const double k = std::tan (kPi * freq);
        const double norm = 1.0 / (1.0 + k / q + k * k);
        const double a0 = k * k * norm;
        double d = 0.9 + 0.1 + 0.5 * clampd ((double) morph, 0.0, 1.0);
        d *= d;
        const double p = d * d;
        static const double level[5] = { 0.25, -0.67, -1.86, -3.35, -5.30 };
        const double t[8] = { a0, 2.0 * a0, a0, 2.0 * (k * k - 1.0) * norm, (1.0 - k / q + k * k) * norm, p, 1.0 / p,
                              dbToGain (interp5 (level, c)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const auto& g = glide.cur;
        const auto& tables = powTables();
        double x = (double) input + tinyFlip (tiny);
        x = fixA.process (x);
        x = shape (tables, x, g[5]);
        const double y = x * g[0] + s1;
        s1 = x * g[1] - y * g[3] + s2;
        s2 = x * g[2] - y * g[4];
        x = shape (tables, y, g[6]);
        x = fixB.process (x);
        return (float) (x * g[7]);
    }

private:
    static inline double shape (const detail::PowTables& t, double x, double p)
    {
        if (x > 1.0) return 1.0;
        if (x > 0.0) return 1.0 - detail::fastPow (t, 1.0 - x, p);
        if (x < -1.0) return -1.0;
        if (x < 0.0) return -1.0 + detail::fastPow (t, 1.0 + x, p);
        return x;
    }

    double lastRate = 0.0, s1 = 0.0, s2 = 0.0, tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<8> glide;
    detail::Df1 fixA, fixB;
};

// ---------------------------------------------------------------------------
// Holt2 ("AW Holt"): Holt's double-exponential smoothing (level and trend)
// as a resonant low-pass, one to eight stages faded in by Poles.
//
// Mapping: cutoff -> Freq (alpha wanted = (1-exp(-2 pi fc/sr)) divided by the
// measured -3 dB point of Poles and Reso; the plugin's A^4 + (1-beta)*A^3
// law is then inverted by Newton iteration, A clamped to 0..1);
// resonance -> Reso (B 0..1: beta = alpha*((B*0.15)+0.12)^2);
// morph -> Poles (C = (1+7 morph)/8: one stage at 0, eight at 1, faded).
// Fixed: Output D = 1.0, Dry/Wet E = 1.0.
class HoltFilter
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        for (auto& s : stage) s = Stage();
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        const double m = clampd ((double) morph, 0.0, 1.0);
        const double rc = (clampd (resonance, 0.0, 1.0) * 0.15) + 0.12;
        const double fc = clampd (cutoffHz, 1.0, sampleRate * 0.45);
        static const double edge[5] = { 1.035, 0.583, 0.478, 0.433, 0.408 }; // poles pull the -3 dB point down
        const double wanted = clampd ((1.0 - std::exp (-kTwoPi * fc / sampleRate)) / (interp5 (edge, m) * (1.0 + 0.12 * clampd (resonance, 0.0, 1.0))), 0.0, 1.0 + 0.9);
        // invert  alpha = A^4 + 1e-5 + (1 - beta) A^3,  beta = (A^4 + 1e-5) rc^2
        double a = std::cbrt (wanted);
        for (int i = 0; i < 4; ++i)
        {
            const double a3 = a * a * a, a4 = a3 * a;
            const double beta = (a4 + 0.00001) * rc * rc;
            const double value = a4 + 0.00001 + (1.0 - beta) * a3 - wanted;
            const double slope = 4.0 * a3 + 3.0 * a * a * (1.0 - beta) - a3 * rc * rc * 4.0 * a3;
            a = clampd (a - value / (slope > 1e-9 ? slope : 1e-9), 0.0, 1.0);
        }
        const double a3 = a * a * a, a4 = a3 * a;
        double alpha = a4 + 0.00001;
        if (alpha > 1.0) alpha = 1.0;
        const double beta = alpha * rc * rc;
        alpha += (1.0 - beta) * a3;
        if (alpha > 1.0) alpha = 1.0;
        static const double level[5] = { -0.04, -0.19, -0.32, -0.43, -0.51 };
        const double t[4] = { alpha, beta, (1.0 + 7.0 * m) / 8.0, dbToGain (interp5 (level, resonance)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const double alpha = glide.cur[0], beta = glide.cur[1], c = glide.cur[2];
        const double hWet = c * 8.0;
        const double dry = (double) input + tinyFlip (tiny);
        double x = dry, prevRaw = dry;
        // the plugin's progressive wet stages: stage n fades in as hWet passes n..n+1
        for (int i = 0; i < 8; ++i)
        {
            const double w = clampd (hWet - i, 0.0, 1.0);
            auto& s = stage[i];
            if (w <= 0.0)
            {
                s = Stage();
                continue;
            }
            const double trend = beta * (x - s.sample) + ((0.999 - beta) * s.trend);
            const double forecast = s.sample + s.trend;
            const double out = (alpha * x) + ((0.999 - alpha) * forecast);
            s.sample = out;
            s.trend = trend;
            x = (out * w) + (prevRaw * (1.0 - w));
            prevRaw = out;
        }
        return (float) (x * glide.cur[3]);
    }

private:
    struct Stage { double sample = 0.0, trend = 0.0; };
    double tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<4> glide;
    Stage stage[8];
};

// ---------------------------------------------------------------------------
// AngleFilter ("AW Angle"): a chain of one-pole "angle" filters whose
// cutoff opens with the signal level (Hard) and that ring (Reso), ending in
// a sine wavefold.
//
// Mapping: cutoff -> Freq (primary, the plugin's A^(os+2), from a measured
// table so the -3 dB point of the four stages lands on fc for every Reso and
// Hard, clamped to 0..1); resonance -> Reso (C 0..0.6, tertiary =
// min(primary + C, 1)); morph -> Hard (B 0.3..0.9; 0.5 is level-independent).
// Fixed: Poles D = 0.1 (four stages). SAFETY: FT is limited to 0..1.
class AngleFilter
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        for (int i = 0; i < stages; ++i) angle[i] = position[i] = 0.0;
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        const double fc = clampd (cutoffHz, 1.0, sampleRate * 0.45);
        const double res = clampd (resonance, 0.0, 1.0), m = clampd ((double) morph, 0.0, 1.0);
        // The plugin's FT acts squared (FT * FR) and Reso adds to it, so the
        // -3 dB point is not proportional to the coefficient. This table is
        // the coefficient (primary) that puts the -3 dB point of the four
        // stages at fc, measured at 48 kHz for fc = 100 Hz * 2^i (columns)
        // and Reso 0, .25, .5, .75, 1 (rows), Hard at its middle.
        static const double primaryFor[5][8] = {
            { 0.08841, 0.12452, 0.17290, 0.23925, 0.33226, 0.46218, 0.62955, 0.79476 },
            { 0.06160, 0.09558, 0.14234, 0.20872, 0.30447, 0.44017, 0.61565, 0.78895 },
            { 0.03966, 0.06913, 0.11299, 0.17874, 0.27680, 0.41784, 0.60118, 0.78638 },
            { 0.02430, 0.04797, 0.08767, 0.15138, 0.25035, 0.39563, 0.59030, 0.78638 },
            { 0.01613, 0.03478, 0.06900, 0.12855, 0.22618, 0.37399, 0.59030, 0.78638 } };
        static const double hardLow[4] = { 1.194, 1.079, 1.033, 1.015 };  // Hard at 0, columns 0, 2, 4, 6
        static const double hardHigh[4] = { 0.803, 0.921, 0.967, 0.985 }; // Hard at 1
        const double column = clampd (std::log2 (fc * 48000.0 / sampleRate / 100.0), 0.0, 7.0);
        const double resPos = res * 4.0;
        const int r0 = std::min (3, (int) resPos), c0 = std::min (6, (int) column);
        const double rf = resPos - r0, cf = column - c0;
        auto rowValue = [&] (int r) { return primaryFor[r][c0] + (primaryFor[r][c0 + 1] - primaryFor[r][c0]) * cf; };
        double primary = rowValue (r0) + (rowValue (r0 + 1) - rowValue (r0)) * rf;
        const double hardPos = clampd (column * 0.5, 0.0, 3.0);
        const int h0 = std::min (2, (int) hardPos);
        const double hf = hardPos - h0;
        auto hardScale = [&] (const double (&t)[4]) { return t[h0] + (t[h0 + 1] - t[h0]) * hf; };
        primary *= m < 0.5 ? 1.0 + (hardScale (hardLow) - 1.0) * (1.0 - 2.0 * m) : 1.0 + (hardScale (hardHigh) - 1.0) * (2.0 * m - 1.0);
        primary = clampd (primary, 0.0, 1.0);
        const double secondary = 0.3 + 0.6 * m;
        const double tertiary = std::min (primary + 0.6 * res, 1.0);
        static const double level[5] = { 0.22, 0.21, 0.18, 0.12, 0.02 };
        const double t[4] = { primary, secondary, tertiary, dbToGain (interp5 (level, res)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const double primary = glide.cur[0], secondary = glide.cur[1], tertiary = glide.cur[2];
        double x = (double) input + tinyFlip (tiny);
        const double shift = secondary - 0.5;
        for (int i = 0; i < stages; ++i)
        {
            double ft = primary + (std::fabs (x) * shift);
            ft = ft < 0.0 ? 0.0 : (ft > 1.0 ? 1.0 : ft);
            const double fr = (ft * (1.0 - tertiary)) + tertiary;
            angle[i] = (angle[i] * (1.0 - ft)) + ((x - position[i]) * ft);
            const double m = (position[i] + (angle[i] * ft * fr)) * (1.0 - ft);
            x = m + (x * ft);
            position[i] = m + (x * ft); // the plugin uses the new x here
        }
        return (float) (fastSin (x) * glide.cur[3]);
    }

private:
    static constexpr int stages = 4;
    double tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<4> glide;
    double angle[stages] {}, position[stages] {};
};

// ---------------------------------------------------------------------------
// Pear2 ("AW Pear"): cascaded two-state slew/average low-pass poles whose
// coefficient follows the signal (NonLin).
//
// Mapping: cutoff -> Freq (knob^2 = (1-exp(-2 pi fc/sr)) divided by the
// measured -3 dB point of the stage count, min 0.0001); resonance -> Poles
// (C = 0.3 + 0.4 res, stages = ceil (C^3 * 64 / 4): 1 at 0, 2 at 0.25 to 0.5,
// 4 at 0.75, 6 at 1); morph -> NonLin (B 0..0.7, B^2 = 0..0.49).
// Fixed: Inv/Wet D = 1.0 (all wet, low-pass).
class PearFilter
{
public:
    void reset()
    {
        glide.reset();
        cache.reset();
        for (int i = 0; i < maxStages; ++i) p0[i] = p1[i] = 0.0;
        tiny = detail::kTiny;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        const double fc = clampd (cutoffHz, 1.0, sampleRate * 0.45);
        const double res = clampd (resonance, 0.0, 1.0);
        const double b = 0.7 * clampd ((double) morph, 0.0, 1.0);
        const double c = 0.3 + 0.4 * res;
        const double maxPoles = c * c * c * 64.0;
        count = std::min (maxStages, std::max (1, (int) std::ceil (maxPoles / 4.0)));
        // stages pull the -3 dB point down (measured by stage count)
        static const double edge[7] = { 1.559, 1.559, 1.05, 0.9, 0.78, 0.73, 0.679 };
        double freq = (1.0 - std::exp (-kTwoPi * fc / sampleRate)) / edge[std::min (count, 6)];
        freq = clampd (freq, 0.0001, 1.0);
        static const double level[5] = { 0.13, 0.12, 0.12, 0.12, 0.14 };
        const double t[3] = { freq, b * b, dbToGain (interp5 (level, res)) };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const double freq = glide.cur[0], nonLin = glide.cur[1];
        double x = (double) input + tinyFlip (tiny);
        for (int i = 0; i < count; ++i)
        {
            double di = std::fabs (freq * (1.0 + (x * nonLin)));
            if (di > 1.0) di = 1.0;
            const double slew = ((x - p0[i]) + p1[i]) * di * 0.5;
            x = (di * x) + ((1.0 - di) * (p0[i] + p1[i]));
            p0[i] = x;
            p1[i] = slew;
        }
        return (float) (x * glide.cur[2]);
    }

private:
    static constexpr int maxStages = 16;
    int count = 2;
    double tiny = detail::kTiny;
    detail::SetCache cache;
    detail::Glide<3> glide;
    double p0[maxStages] {}, p1[maxStages] {};
};

// ---------------------------------------------------------------------------
// Disperser: a chain of second-order all-pass sections, all centred on the
// cutoff, so key tracking tunes the chirp. Each section is a zero-delay
// state-variable filter with output x - 2 BP / Q (equal to the RBJ all-pass,
// but it stays bounded while the cutoff is modulated, which the direct
// forms do not). Magnitude is flat; the group delay peaks at the cutoff
// (4 Q / w0 per section) and grows with the section count.
//
// Mapping: cutoff -> section centre; resonance -> all-pass Q (0.5..8,
// exponential); morph -> number of sections, 1..64 smoothly: the output
// crossfades between the chains of N and N+1 sections.
class Disperser
{
public:
    static constexpr int maxStages = 64;

    void reset()
    {
        glide.reset();
        cache.reset();
        for (auto& s : stage) s = Stage();
        tiny = detail::kTiny;
        used = 0;
    }

    void set (double sampleRate, double cutoffHz, double resonance, float morph)
    {
        if (cache.same (sampleRate, cutoffHz, resonance, (double) morph))
            return;
        using namespace detail;
        const double fc = clampd (cutoffHz, 10.0, sampleRate * 0.45);
        const double q = 0.5 * std::pow (16.0, clampd (resonance, 0.0, 1.0));
        const double g = std::tan (kPi * fc / sampleRate);
        const double k = 1.0 / q;
        const double count = 1.0 + clampd ((double) morph, 0.0, 1.0) * (maxStages - 1);
        const double t[3] = { g, k, count };
        glide.set (t);
    }

    float process (float input)
    {
        using namespace detail;
        glide.tick();
        const double g = glide.cur[0], k = glide.cur[1], count = glide.cur[2];
        // a1 follows the gliding g and k exactly (interpolating it separately
        // lets the loop drift out of balance mid-glide)
        const double a1 = 1.0 / (1.0 + k * g + g * g);
        const double ga = g * a1, twoK = 2.0 * k, twoG = 2.0 * g, twoGa = 2.0 * ga;
        const double c1 = k + g;
        const double gain = 1.0 - twoK * ga; // y = gain * x + offset, offset depends on the states only
        int n = (int) count;
        double frac = count - n;
        if (n >= maxStages) { n = maxStages; frac = 0.0; }
        if (n < 1) { n = 1; frac = 0.0; }
        const int need = std::min (maxStages, n + (frac > 1e-9 ? 1 : 0));
        for (int i = need; i < used; ++i) // a section that drops out starts clean when it returns
            stage[i] = Stage();
        used = need;

        double x = (double) input + tinyFlip (tiny);
        double atN = x;
        for (int i = 0; i < need; ++i)
        {
            auto& s = stage[i];
            const double t = c1 * s.s1 + s.s2;
            const double base = s.s1 - ga * t;            // bp = ga * x + base
            const double bp = ga * x + base;
            s.s1 += twoGa * (x - t);
            s.s2 += twoG * bp;
            x = gain * x - twoK * base;
            if (i == n - 1)
                atN = x;
        }
        return (float) (need > n ? atN + (x - atN) * frac : x);
    }

private:
    struct Stage { double s1 = 0.0, s2 = 0.0; };
    double tiny = detail::kTiny;
    int used = 0;
    detail::SetCache cache;
    detail::Glide<3> glide;
    Stage stage[maxStages];
};
} // namespace Airwindows
