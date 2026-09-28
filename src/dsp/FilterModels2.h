#pragma once

// M8.4: the filter models appended after Morph (FilterType 12..25). Each is a
// small class with makeCoefficients-style setup and a per-sample process,
// used by FilterUnit. All are zero-delay-feedback (TPT) designs:
// g = tan(pi fc / fs), one-pole v = (x - s) G, y = v + s, s = y + v.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <complex>

namespace Filters2
{
inline double prewarp (double sampleRate, double cutoff)
{
    return std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, sampleRate * 0.45, cutoff) / sampleRate);
}

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
    void reset() { s = 0.0; }
};

// A TPT state-variable filter giving all outputs at once (Zavalishin).
struct Svf2
{
    double g = 0.1, k = 1.414, a1 = 0.0, s1 = 0.0, s2 = 0.0;
    void set (double sampleRate, double cutoff, double damping)
    {
        g = prewarp (sampleRate, cutoff);
        k = juce::jmax (0.005, damping);
        a1 = 1.0 / (1.0 + k * g + g * g);
    }
    void process (double x, double& lp, double& bp, double& hp)
    {
        hp = (x - (k + g) * s1 - s2) * a1;
        bp = g * hp + s1;
        s1 = g * hp + bp;
        lp = g * bp + s2;
        s2 = g * bp + lp;
    }
    void reset() { s1 = s2 = 0.0; }
};

// Moog ladder with a tanh in every stage (Huovilainen, DAFx 2004), solved
// with the feedback taken from the previous sample's output and a half-
// sample compensation, oversampled 2x inside: warmer and more compressed
// than the linear-loop Ladder.
class DriveLadder
{
public:
    void set (double sampleRate, double cutoff, double resonance, bool bandPassTaps)
    {
        // 2x internally.
        const auto fc = juce::jlimit (10.0, sampleRate * 0.45, cutoff);
        const auto x = fc / (2.0 * sampleRate);
        // Huovilainen's tuning polynomial.
        g = 1.0 - std::exp (-juce::MathConstants<double>::twoPi * x * (1.0 + x * (-0.44 + x * 0.34)));
        k = 4.0 * juce::jlimit (0.0, 1.0, resonance) * (1.0 + x * (0.4 - x * 0.2)) * 1.02;
        bandPass = bandPassTaps;
    }
    void reset() { y.fill (0.0); w.fill (0.0); delayed = 0.0; }
    float process (float input)
    {
        auto out = 0.0;
        for (int pass = 0; pass < 2; ++pass)
        {
            const auto in = std::tanh ((double) input * (1.0 + 0.5 * k) * 0.8 - k * delayed);
            y[0] += g * (in - w[0]);
            w[0] = std::tanh (y[0]);
            y[1] += g * (w[0] - w[1]);
            w[1] = std::tanh (y[1]);
            y[2] += g * (w[1] - w[2]);
            w[2] = std::tanh (y[2]);
            y[3] += g * (w[2] - std::tanh (y[3]));
            delayed = 0.5 * (y[3] + last);
            last = y[3];
            out = bandPass ? 2.0 * (y[1] - y[2]) * 1.8 : y[3] * 1.25;
        }
        if (! std::isfinite (out)) { reset(); return 0.0f; }
        return (float) out;
    }

private:
    std::array<double, 4> y {}, w {};
    double g = 0.1, k = 0.0, delayed = 0.0, last = 0.0;
    bool bandPass = false;
};

// Oberheim SEM: a 2-pole state-variable with the SEM's single mode knob
// sweeping low-pass -> notch -> high-pass (morph), a soft input stage, and
// modest resonance (it never quite self-oscillates).
class SemFilter
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        svf.set (sampleRate, cutoff, 2.0 * (1.0 - 0.94 * juce::jlimit (0.0, 1.0, resonance)));
        mode = juce::jlimit (0.0f, 1.0f, morph);
    }
    void reset() { svf.reset(); }
    float process (float input)
    {
        double lp, bp, hp;
        svf.process (std::tanh ((double) input * 0.9) / 0.9, lp, bp, hp);
        const auto m = (double) mode;
        // LP to notch (LP + HP) to HP, keeping level even.
        const auto y = m < 0.5 ? lp + hp * (2.0 * m) : hp + lp * (2.0 - 2.0 * m);
        return (float) y;
    }

private:
    Svf2 svf;
    float mode = 0.0f;
};

// OTA cascade (CEM3320 / SSM2040 style): four transconductance stages, each
// with the differential pair's tanh at its input, resonance fed back from
// the fourth stage. Low-pass, or band-pass (two low-pass poles and two
// high-pass differences, as the chip's BP configuration).
class OtaFilter
{
public:
    void set (double sampleRate, double cutoff, double resonance, bool bandPassMode)
    {
        const auto g = prewarp (sampleRate, cutoff);
        bigG = g / (1.0 + g);
        k = juce::jlimit (0.0, 1.0, resonance) * (bandPassMode ? 3.2 : 3.9);
        bandPass = bandPassMode;
    }
    void reset() { for (auto& p : poles) p.reset(); feedback = 0.0; }
    float process (float input)
    {
        auto x = std::tanh ((double) input * (1.0 + 0.3 * k) - k * feedback);
        std::array<double, 4> y {};
        for (int i = 0; i < 4; ++i)
        {
            const auto lp = poles[(size_t) i].lowPass (std::tanh (x), bigG);
            y[(size_t) i] = bandPass && i >= 2 ? x - lp : lp;
            x = y[(size_t) i];
        }
        feedback = y[3];
        if (! std::isfinite (feedback)) { reset(); return 0.0f; }
        return (float) (bandPass ? y[3] * (2.0 + k * 0.5) : y[3]);
    }

private:
    std::array<OnePole, 4> poles;
    double bigG = 0.1, k = 0.0, feedback = 0.0;
    bool bandPass = false;
};

// Korg-35 high-pass (the MS-20's HPF): the Sallen-Key dual of its low-pass,
// resonance through a saturating loop.
class Ms20HighPass
{
public:
    void set (double sampleRate, double cutoff, double resonance)
    {
        const auto g = prewarp (sampleRate, cutoff);
        bigG = g / (1.0 + g);
        k = 0.01 + juce::jlimit (0.0, 1.0, resonance) * 1.98;
        alpha0 = 1.0 / (1.0 - k * bigG + k * bigG * bigG);
        hpf2Beta = -bigG / (1.0 + g);
        lpf1Beta = 1.0 / (1.0 + g);
        g1 = g;
    }
    void reset() { hpf1.reset(); hpf2.reset(); lpf1.reset(); }
    float process (float input)
    {
        const auto y1 = (double) input - hpf1.lowPass ((double) input, bigG); // first HP
        const auto s35 = hpf2Beta * hpf2.s + lpf1Beta * lpf1.s;
        auto u = alpha0 * (y1 + s35);
        u = std::tanh (u * 1.2) / 1.2;
        const auto y = k * (u - hpf2.lowPass (u, bigG));
        lpf1.lowPass (y, bigG);
        const auto out = y / k;
        juce::ignoreUnused (g1);
        if (! std::isfinite (out)) { reset(); return 0.0f; }
        return (float) out;
    }

private:
    OnePole hpf1, hpf2, lpf1;
    double bigG = 0.1, k = 0.01, alpha0 = 1.0, hpf2Beta = 0.0, lpf1Beta = 0.0, g1 = 0.1;
};

// Steiner-Parker: one 2-pole loop with three inputs. Feeding the source
// into the low, band or high input gives H(s) = (a_L + a_B s + a_H s^2) /
// (s^2 + k s + 1), the same as mixing a state-variable's outputs, which is
// how it is built here; morph slides the input from low to band to high.
// Its resonance is diode-limited: the band state is soft-limited in the
// loop, so it screams without hard clipping.
class SteinerParker
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        g = prewarp (sampleRate, cutoff);
        k = 2.0 - 1.97 * juce::jlimit (0.0, 1.0, resonance);
        const auto m = juce::jlimit (0.0f, 1.0f, morph) * 2.0f;
        inLow = (double) juce::jmax (0.0f, 1.0f - m);
        inBand = (double) (1.0f - std::abs (m - 1.0f));
        inHigh = (double) juce::jmax (0.0f, m - 1.0f);
    }
    void reset() { s1 = s2 = 0.0; }
    float process (float input)
    {
        const auto x = (double) input;
        const auto hp = (x - (k + g) * s1 - s2) / (1.0 + k * g + g * g);
        const auto bp = g * hp + s1;
        s1 = g * hp + bp;
        const auto lp = g * bp + s2;
        s2 = g * bp + lp;
        s1 = std::tanh (s1 * 0.5) * 2.0;
        const auto y = inLow * lp + inBand * bp * k * 1.2 + inHigh * hp;
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        return (float) y;
    }

private:
    double g = 0.1, k = 1.4, s1 = 0.0, s2 = 0.0, inLow = 1.0, inBand = 0.0, inHigh = 0.0;
};

// Phaser notch: four first-order allpasses around the cutoff (spread an
// octave either side by morph) summed with the input: two notches, with
// feedback (resonance) sharpening them into peaks between.
class PhaserNotch
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        const auto spread = 1.0 + 3.0 * (double) juce::jlimit (0.0f, 1.0f, morph);
        for (int i = 0; i < 4; ++i)
        {
            const auto hz = cutoff * std::pow (spread, ((double) i - 1.5) / 1.5);
            const auto t = prewarp (sampleRate, hz);
            coefficients[(size_t) i] = (t - 1.0) / (t + 1.0);
        }
        feedback = 0.85 * juce::jlimit (0.0, 1.0, resonance);
    }
    void reset() { state.fill (0.0); last = 0.0; }
    float process (float input)
    {
        auto x = (double) input + feedback * last;
        for (int i = 0; i < 4; ++i)
        {
            const auto a = coefficients[(size_t) i];
            const auto y = a * x + state[(size_t) i];
            state[(size_t) i] = x - a * y;
            x = y;
        }
        last = std::tanh (x);
        const auto out = 0.5 * ((double) input + x);
        if (! std::isfinite (out)) { reset(); return 0.0f; }
        return (float) out;
    }

private:
    std::array<double, 4> coefficients {}, state {};
    double feedback = 0.0, last = 0.0;
};

// Formant bank: five band-passes at a vowel's formants with their
// bandwidths and levels. Vowel: male A E I O U (morph walks through them).
// Talking: an eight-vowel path (female formants) for speech-like sweeps.
class VowelFilter
{
public:
    struct Formant { double hz, bandwidth, db; };
    using Vowel = std::array<Formant, 5>;

    static const std::array<Vowel, 5>& maleVowels()
    {
        // A, E, I, O, U (bass/tenor voice; Hz, Hz, dB), after the classic
        // Csound formant tables.
        static const std::array<Vowel, 5> table {
            Vowel { Formant { 600, 60, 0 }, { 1040, 70, -7 }, { 2250, 110, -9 }, { 2450, 120, -9 }, { 2750, 130, -20 } },
            Vowel { Formant { 400, 40, 0 }, { 1620, 80, -12 }, { 2400, 100, -9 }, { 2800, 120, -12 }, { 3100, 120, -18 } },
            Vowel { Formant { 250, 60, 0 }, { 1750, 90, -30 }, { 2600, 100, -16 }, { 3050, 120, -22 }, { 3340, 120, -28 } },
            Vowel { Formant { 400, 40, 0 }, { 750, 80, -11 }, { 2400, 100, -21 }, { 2600, 120, -20 }, { 2900, 120, -40 } },
            Vowel { Formant { 350, 40, 0 }, { 600, 80, -20 }, { 2400, 100, -32 }, { 2675, 120, -28 }, { 2950, 120, -36 } },
        };
        return table;
    }

    static const std::array<Vowel, 8>& talkingVowels()
    {
        // i, e, ae, a, o, u, uh, er (soprano-ish formants, wider bands).
        static const std::array<Vowel, 8> table {
            Vowel { Formant { 310, 50, 0 }, { 2790, 120, -12 }, { 3310, 150, -16 }, { 4000, 200, -24 }, { 4950, 200, -30 } },
            Vowel { Formant { 480, 60, 0 }, { 2480, 100, -10 }, { 3000, 150, -14 }, { 4000, 200, -22 }, { 4950, 200, -30 } },
            Vowel { Formant { 690, 80, 0 }, { 2100, 100, -6 }, { 2900, 150, -14 }, { 4000, 200, -22 }, { 4950, 200, -30 } },
            Vowel { Formant { 850, 80, 0 }, { 1220, 90, -4 }, { 2810, 150, -16 }, { 3900, 200, -24 }, { 4950, 200, -30 } },
            Vowel { Formant { 500, 70, 0 }, { 840, 80, -8 }, { 2830, 150, -24 }, { 3800, 200, -28 }, { 4950, 200, -36 } },
            Vowel { Formant { 370, 50, 0 }, { 950, 80, -18 }, { 2670, 150, -30 }, { 3700, 200, -32 }, { 4950, 200, -38 } },
            Vowel { Formant { 760, 80, 0 }, { 1400, 90, -6 }, { 2780, 150, -16 }, { 3800, 200, -24 }, { 4950, 200, -30 } },
            Vowel { Formant { 500, 60, 0 }, { 1500, 90, -8 }, { 1700, 120, -10 }, { 3600, 200, -24 }, { 4950, 200, -30 } },
        };
        return table;
    }

    // cutoff shifts the whole set (1 kHz = as tabled, like Formant does),
    // resonance narrows the bands.
    void set (double sampleRate, double cutoff, double resonance, float morph, bool talking)
    {
        const auto shift = juce::jlimit (0.4, 2.5, std::sqrt (cutoff / 1000.0));
        const auto sharp = 1.6 - 1.2 * juce::jlimit (0.0, 1.0, resonance);
        talkingSet = talking;
        const auto count = talking ? 8 : 5;
        const auto position = (double) juce::jlimit (0.0f, 1.0f, morph) * (double) (count - 1);
        const auto index = juce::jmin (count - 2, (int) position);
        const auto frac = position - (double) index;
        for (int f = 0; f < 5; ++f)
        {
            const auto& a = talking ? talkingVowels()[(size_t) index][(size_t) f] : maleVowels()[(size_t) index][(size_t) f];
            const auto& b = talking ? talkingVowels()[(size_t) index + 1][(size_t) f] : maleVowels()[(size_t) index + 1][(size_t) f];
            const auto hz = std::exp (std::log (a.hz) + (std::log (b.hz) - std::log (a.hz)) * frac) * shift;
            const auto bandwidth = (a.bandwidth + (b.bandwidth - a.bandwidth) * frac) * sharp * shift;
            const auto db = a.db + (b.db - a.db) * frac;
            // Band-pass SVF: damping = bandwidth / centre, peak gain 1 at k = 1.
            bands[(size_t) f].set (sampleRate, hz, bandwidth / hz);
            gains[(size_t) f] = std::pow (10.0, db / 20.0);
        }
    }
    void reset() { for (auto& b : bands) b.reset(); }
    float process (float input)
    {
        auto sum = 0.0;
        for (int f = 0; f < 5; ++f)
        {
            double lp, bp, hp;
            bands[(size_t) f].process ((double) input, lp, bp, hp);
            sum += bp * bands[(size_t) f].k * gains[(size_t) f];
        }
        return (float) (sum * (talkingSet ? 3.2 : 1.6));
    }

private:
    bool talkingSet = false;
    std::array<Svf2, 5> bands;
    std::array<double, 5> gains {};
};

// Twin Peak: two resonant band-passes, the second morph octaves (0-2) above
// the cutoff, blended with a little low-pass body.
class TwinPeak
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        const auto k = 1.2 - 1.15 * juce::jlimit (0.0, 1.0, resonance);
        first.set (sampleRate, cutoff, k);
        second.set (sampleRate, cutoff * std::pow (2.0, 2.0 * (double) juce::jlimit (0.0f, 1.0f, morph)), k);
        damping = k;
    }
    void reset() { first.reset(); second.reset(); }
    float process (float input)
    {
        double lp1, bp1, hp1, lp2, bp2, hp2;
        first.process ((double) input, lp1, bp1, hp1);
        second.process ((double) input, lp2, bp2, hp2);
        return (float) (0.3 * lp1 + (bp1 + bp2) * damping * 0.8);
    }

private:
    Svf2 first, second;
    double damping = 1.0;
};

} // namespace Filters2
