#pragma once

// M8.4: the filter models appended after Morph (FilterType 12..25). Each is a
// small class with makeCoefficients-style setup and a per-sample process,
// used by FilterUnit. All are zero-delay-feedback (TPT) designs:
// g = tan(pi fc / fs), one-pole v = (x - s) G, y = v + s, s = y + v.

#include <juce_core/juce_core.h>

#include "FilterCore.h"

#include <array>
#include <cmath>
#include <complex>
#include <vector>

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

// Moog ladder with every stage saturating (the transistor pairs: each
// stage's cutoff falls with the current through it, Huovilainen's
// character), now zero-delay (FilterCore::StageCascade) at twice the sample
// rate, so it self-oscillates at the cutoff at any pitch. Warmer and more
// compressed than the linear-stage Ladder.
class DriveLadder
{
public:
    void set (double sampleRate, double cutoff, double resonance, bool bandPassTaps)
    {
        FilterCore::StageCascade::Settings settings;
        settings.g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        settings.k = 4.15 * juce::jlimit (0.0, 1.0, resonance);
        settings.stageDrive = stageDrive;
        settings.loopLevel = loopLevel;
        cascade.set (settings);
        k = settings.k;
        bandPass = bandPassTaps;
    }
    void reset() { cascade.reset(); oversampler.reset(); }
    float process (float input)
    {
        double first, second;
        oversampler.upsample ((double) input * (1.0 + 0.5 * k) * inputGain, first, second);
        const auto a = step (first);
        const auto b = step (second);
        return (float) oversampler.downsample (a, b);
    }

    // Stage saturation (0.5 keeps the self-oscillation within 0.2 % of the
    // cutoff) and the old model's input and tap gains (its level table
    // within 0.8 dB).
    static constexpr double stageDrive = 0.5, loopLevel = 1.0, inputGain = 0.8, bandGain = 3.9, lowGain = 1.25;

private:
    double step (double x)
    {
        std::array<double, 4> y;
        double u;
        cascade.process (x, y, u);
        return bandPass ? (y[1] - y[2]) * bandGain : y[3] * lowGain;
    }

    FilterCore::StageCascade cascade;
    FilterCore::Halfband2x oversampler;
    double k = 0.0;
    bool bandPass = false;
};

// Oberheim SEM: a 2-pole state-variable with the SEM's single mode knob
// sweeping low-pass -> notch -> high-pass (morph), a soft input stage, and
// modest resonance (it never quite self-oscillates). Its two integrators
// are OTAs: each one's gain sags with the signal it integrates (a secant
// gain from the last sample), so loud notes round off and the resonance
// thickens instead of ringing.
class SemFilter
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        g = prewarp (sampleRate, cutoff);
        k = 2.0 * (1.0 - 0.94 * juce::jlimit (0.0, 1.0, resonance));
        mode = juce::jlimit (0.0f, 1.0f, morph);
    }
    void reset() { s1 = s2 = lastHp = lastBp = 0.0; }
    float process (float input)
    {
        const auto x = std::tanh ((double) input * 0.9) / 0.9;
        const auto g1 = g * FilterCore::tanhOverX (otaDrive * lastHp);
        const auto g2 = g * FilterCore::tanhOverX (otaDrive * lastBp);
        // TPT SVF with the two integrators' own gains.
        const auto hp = (x - (k + g2) * s1 - s2) / (1.0 + g1 * (k + g2));
        const auto bp = g1 * hp + s1;
        s1 = g1 * hp + bp;
        const auto lp = g2 * bp + s2;
        s2 = g2 * bp + lp;
        lastHp = hp;
        lastBp = bp;
        const auto m = (double) mode;
        // LP to notch (LP + HP) to HP, keeping level even.
        const auto y = m < 0.5 ? lp + hp * (2.0 * m) : hp + lp * (2.0 - 2.0 * m);
        return (float) y;
    }

    static constexpr double otaDrive = 0.5;

private:
    double g = 0.1, k = 2.0, s1 = 0.0, s2 = 0.0, lastHp = 0.0, lastBp = 0.0;
    float mode = 0.0f;
};

// OTA cascade (CEM3320 / SSM2040 style, FilterCore::StageCascade): four
// transconductance stages whose differential pairs saturate, the loop solved
// with zero delay so it self-oscillates at the cutoff. Low-pass with
// negative feedback, or band-pass (two low-pass and two high-pass stages,
// as the chip's BP configuration) with positive feedback: both reach the
// oscillation threshold at k = 4 exactly at the cutoff.
class OtaFilter
{
public:
    void set (double sampleRate, double cutoff, double resonance, bool bandPassMode)
    {
        FilterCore::StageCascade::Settings settings;
        settings.g = FilterCore::prewarp (sampleRate, cutoff);
        settings.k = juce::jlimit (0.0, 1.0, resonance) * (bandPassMode ? bandK : lowK);
        settings.positiveFeedback = bandPassMode;
        settings.highPassStages34 = bandPassMode;
        settings.stageDrive = bandPassMode ? bandStageDrive : lowStageDrive;
        cascade.set (settings);
        k = settings.k;
        bandPass = bandPassMode;
    }
    void reset() { cascade.reset(); }
    float process (float input)
    {
        std::array<double, 4> y;
        double u;
        const auto in = (double) input * (bandPass ? bandInput : 1.0 + lowComp * k);
        cascade.process (in, y, u);
        return (float) (bandPass ? y[3] * (bandGain + bandPerK * k) : y[3] * lowGain);
    }

    // Fitted to the old models' level tables (every cell within 1 dB).
    static constexpr double lowK = 4.1, lowStageDrive = 0.562, lowComp = 0.405, lowGain = 0.814;
    static constexpr double bandK = 4.1, bandStageDrive = 0.730, bandInput = 1.088, bandGain = 1.85, bandPerK = 0.169;

private:
    FilterCore::StageCascade cascade;
    double k = 0.0;
    bool bandPass = false;
};

// Korg-35 high-pass (the MS-20's HPF, FilterCore::Korg35Core): the dual of
// its low-pass, the resonance fed back through a low-pass and the diode
// limiter, at twice the sample rate; self-oscillates at the cutoff.
class Ms20HighPass
{
public:
    void set (double sampleRate, double cutoff, double resonance)
    {
        const auto g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        core.set (g, 0.01 + juce::jlimit (0.0, 1.0, resonance) * 2.05, true, limit);
    }
    void reset() { core.reset(); oversampler.reset(); }
    float process (float input)
    {
        double first, second;
        oversampler.upsample ((double) input, first, second);
        const auto a = core.process (first);
        const auto b = core.process (second);
        return (float) (oversampler.downsample (a, b) * outputGain);
    }

    // Limiter level and output gain: the old model's level table within 1 dB
    // (1.5 dB at 3 kHz and resonance 0.9).
    static constexpr double limit = 0.6, outputGain = 0.68;

private:
    FilterCore::Korg35Core core;
    FilterCore::Halfband2x oversampler;
};

// Steiner-Parker: one 2-pole loop with three inputs. Feeding the source
// into the low, band or high input gives H(s) = (a_L + a_B s + a_H s^2) /
// (s^2 + k s + 1), the same as mixing a state-variable's outputs, which is
// how it is built here (TPT, twice the sample rate); morph slides the input
// from low to band to high. Its resonance is diode-limited: the damping
// grows with the band-pass level, so near full resonance the damping goes
// negative and the loop self-oscillates at the cutoff at a level the
// diodes hold, without clipping the states.
class SteinerParker
{
public:
    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        const auto r = juce::jlimit (0.0, 1.0, resonance);
        kOut = 2.0 - 1.97 * r;
        // As the output's damping up to 0.9, then down through zero (-0.03
        // at full resonance) so the loop oscillates.
        kLoop = kOut - 0.6 * juce::jmax (0.0, r - 0.9);
        const auto m = juce::jlimit (0.0f, 1.0f, morph) * 2.0f;
        inLow = (double) juce::jmax (0.0f, 1.0f - m);
        inBand = (double) (1.0f - std::abs (m - 1.0f));
        inHigh = (double) juce::jmax (0.0f, m - 1.0f);
    }
    void reset() { s1 = s2 = lastBp = 0.0; oversampler.reset(); }
    float process (float input)
    {
        double first, second;
        oversampler.upsample ((double) input, first, second);
        const auto a = step (first);
        const auto b = step (second);
        const auto y = oversampler.downsample (a, b);
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        return (float) y;
    }

    // The diodes' extra damping at large band-pass levels.
    // Fitted to the old model's level table: within 0.7 dB up to resonance
    // 0.5; at 0.9 it is 1.8 dB louder at 300 Hz and quieter at 3 kHz (the
    // old model clipped its states, which depended on the cutoff).
    static constexpr double diodeDamping = 0.637, diodeScale = 3.3;

private:
    double step (double x)
    {
        const auto k = kLoop + diodeDamping * (1.0 - FilterCore::tanhOverX (diodeScale * lastBp));
        const auto hp = (x - (k + g) * s1 - s2) / (1.0 + g * (k + g));
        const auto bp = g * hp + s1;
        s1 = g * hp + bp;
        const auto lp = g * bp + s2;
        s2 = g * bp + lp;
        lastBp = bp;
        return inLow * lp + inBand * bp * kOut * 1.2 + inHigh * hp;
    }

    FilterCore::Halfband2x oversampler;
    double g = 0.1, kOut = 1.4, kLoop = 1.4, s1 = 0.0, s2 = 0.0, lastBp = 0.0, inLow = 1.0, inBand = 0.0, inHigh = 0.0;
};

// Phaser notch: four first-order allpasses around the cutoff (spread an
// octave either side by morph) summed with the input: two notches, with
// feedback (resonance, saturating, solved with zero delay) sharpening them
// into peaks between.
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
    void reset() { state.fill (0.0); }
    float process (float input)
    {
        // The chain's output is A x + S for its input x; solve the loop
        // x = in + feedback tanh (A x + S) with zero delay (Newton; the loop
        // gain feedback |A| <= 0.85 keeps it monotone).
        auto gain = 1.0, offset = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            const auto a = coefficients[(size_t) i];
            offset = a * offset + state[(size_t) i];
            gain *= a;
        }
        const auto in = (double) input;
        auto x = in + feedback * FilterCore::tanhApprox (gain * in + offset);
        for (int step = 0; step < 2; ++step)
        {
            const auto t = FilterCore::tanhApprox (gain * x + offset);
            x -= (x - in - feedback * t) / (1.0 - feedback * gain * FilterCore::tanhSlope (t));
        }
        for (int i = 0; i < 4; ++i)
        {
            const auto a = coefficients[(size_t) i];
            const auto y = a * x + state[(size_t) i];
            state[(size_t) i] = x - a * y;
            x = y;
        }
        const auto out = 0.5 * (in + x);
        if (! std::isfinite (out)) { reset(); return 0.0f; }
        return (float) out;
    }

private:
    std::array<double, 4> coefficients {}, state {};
    double feedback = 0.0;
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

// ---- Appended by the filter overhaul (FilterType 25..28) ----

// 303 Acid: the TB-303's diode ladder (FilterCore::DiodeLadderCore with the
// top capacitor halved, Stinchcombe's model) with the one-pole high-pass
// (150 Hz, Open303's value) in its feedback, the coupling capacitor that
// keeps the resonance from swallowing the bass. The high-pass leads the
// loop's phase, so the core's frequency is solved from its response so the
// loop still oscillates exactly at the cutoff, and the threshold follows.
// The drive goes into the loop's input sum (the 303's squelch).
class Acid303
{
public:
    // Shared with Diode LP: the core slides up as resonance falls.
    static constexpr double lift0 = 4.034, liftPower = 1.475;
    static constexpr double feedbackHpHz = 150.0;

    void set (double sampleRate, double cutoff, double resonance)
    {
        const auto r = juce::jlimit (0.0, 1.0, resonance);
        const auto lift = std::pow (lift0, 1.0 - std::pow (r, liftPower));
        const auto g = FilterCore::prewarp (sampleRate, cutoff * lift);
        const auto gHp = FilterCore::prewarp (sampleRate, feedbackHpHz);
        const auto tuning = tuningFor (gHp / g);
        const auto hpGain = g / std::sqrt (g * g + gHp * gHp);
        FilterCore::DiodeLadderCore::Settings settings;
        settings.h = g / tuning.ratio;
        settings.k = r * 1.03 / (tuning.gain * hpGain);
        // Make up the level the feedback takes (as Diode LP), part into the
        // core (it drives the loop), part after it.
        const auto makeUp = 1.0 + makeUpPerK * settings.k;
        post = std::pow (makeUp, 0.2) * outputGain;
        pre = makeUp / std::pow (makeUp, 0.2) * inputGain;
        settings.halfTopCap = true;
        settings.feedbackHpG = gHp / (1.0 + gHp);
        settings.diodeDrive = 0.5;
        core.set (settings);
    }
    void setDrive (float amount) { drive = (double) amount; }
    void reset() { core.reset(); }
    float process (float input)
    {
        std::array<double, 4> y;
        core.process ((double) input * drive * pre, y);
        return (float) (y[3] * post);
    }

    // Make-up per unit of feedback: the level stays within about 3 dB of
    // -10 dB on the level table's saw, except that full resonance keeps the
    // bass under the high-pass corner (the 303's trait) and gets louder there.
    static constexpr double inputGain = 1.0, outputGain = 1.0, makeUpPerK = 0.45;

    // Where the core's loop phase reaches -180 degrees less the high-pass's
    // lead atan (rho) (rho = high-pass corner / oscillation frequency, both
    // prewarped): that frequency in core units, and the core's gain there.
    struct Tuning { double ratio, gain; };
    static Tuning tuningFor (double rho)
    {
        struct Point { double x, phase, gain; };
        static const std::vector<Point> grid = []
        {
            std::vector<Point> points;
            auto unwrapped = 0.0, previous = 0.0;
            for (int i = 0; i <= 4000; ++i)
            {
                const auto x = 0.05 * std::pow (400.0, (double) i / 4000.0);
                const auto h = FilterCore::DiodeLadderCore::coreResponse ({ 0.0, x }, true);
                const auto phase = std::arg (h);
                if (i > 0)
                {
                    auto step = phase - previous;
                    while (step > juce::MathConstants<double>::pi) step -= juce::MathConstants<double>::twoPi;
                    while (step < -juce::MathConstants<double>::pi) step += juce::MathConstants<double>::twoPi;
                    unwrapped += step;
                }
                else
                {
                    unwrapped = phase;
                }
                previous = phase;
                points.push_back ({ x, unwrapped, std::abs (h) });
            }
            return points;
        }();

        const auto target = -juce::MathConstants<double>::pi - std::atan (juce::jmax (0.0, rho));
        // Phase falls monotonically: binary search for the crossing.
        size_t lo = 0, hi = grid.size() - 1;
        if (grid[hi].phase > target)
            return { grid[hi].x, grid[hi].gain };
        while (hi - lo > 1)
        {
            const auto mid = (lo + hi) / 2;
            (grid[mid].phase > target ? lo : hi) = mid;
        }
        const auto t = (target - grid[lo].phase) / (grid[hi].phase - grid[lo].phase);
        return { grid[lo].x + (grid[hi].x - grid[lo].x) * t, grid[lo].gain + (grid[hi].gain - grid[lo].gain) * t };
    }

private:
    FilterCore::DiodeLadderCore core;
    double drive = 1.0, pre = 1.0, post = 1.0;
};

// Moog Drive: the ladder driven hard from inside: the drive goes into the
// loop's input pair, every stage saturates (FilterCore::StageCascade at 2x),
// and the passband loss as resonance rises is only partly made up, so the
// bass thins like the Minimoog's. 24 dB; self-oscillates at the cutoff.
class MoogDrive
{
public:
    void set (double sampleRate, double cutoff, double resonance)
    {
        FilterCore::StageCascade::Settings settings;
        settings.g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        settings.k = 4.15 * juce::jlimit (0.0, 1.0, resonance);
        settings.stageDrive = 0.5;
        cascade.set (settings);
        k = settings.k;
    }
    void setDrive (float amount) { drive = (double) amount; }
    void reset() { cascade.reset(); oversampler.reset(); }
    float process (float input)
    {
        double first, second;
        oversampler.upsample ((double) input * drive * (1.0 + 0.25 * k) * inputGain, first, second);
        std::array<double, 4> y;
        double u;
        cascade.process (first, y, u);
        const auto a = y[3];
        cascade.process (second, y, u);
        return (float) (oversampler.downsample (a, y[3]) * outputGain);
    }

    static constexpr double inputGain = 1.0, outputGain = 1.0;

private:
    FilterCore::StageCascade cascade;
    FilterCore::Halfband2x oversampler;
    double k = 0.0, drive = 1.0;
};

// Vowel Morph: a Klatt-style cascade of five formant resonators (each a
// unity-gain two-pole low-pass, so the formants' relative levels come out of
// the cascade as in a real vocal tract). MORPH walks A E I O U; the cutoff
// moves from a male voice (1 kHz, Csound's tenor table) to a female one
// (2 kHz, the soprano table), and scales the formants beyond those; the
// resonance narrows the bandwidths.
class VowelMorph
{
public:
    struct Formants { double hz[5], bandwidth[5]; };

    static const std::array<Formants, 5>& male()
    {
        static const std::array<Formants, 5> table {
            Formants { { 650, 1080, 2650, 2900, 3250 }, { 80, 90, 120, 130, 140 } },
            Formants { { 400, 1700, 2600, 3200, 3580 }, { 70, 80, 100, 120, 120 } },
            Formants { { 290, 1870, 2800, 3250, 3540 }, { 40, 90, 100, 120, 120 } },
            Formants { { 400, 800, 2600, 2800, 3000 }, { 40, 80, 100, 120, 120 } },
            Formants { { 350, 600, 2700, 2900, 3300 }, { 40, 60, 100, 120, 120 } },
        };
        return table;
    }

    static const std::array<Formants, 5>& female()
    {
        static const std::array<Formants, 5> table {
            Formants { { 800, 1150, 2900, 3900, 4950 }, { 80, 90, 120, 130, 140 } },
            Formants { { 350, 2000, 2800, 3600, 4950 }, { 60, 100, 120, 150, 200 } },
            Formants { { 270, 2140, 2950, 3900, 4950 }, { 60, 90, 100, 120, 120 } },
            Formants { { 450, 800, 2830, 3800, 4950 }, { 40, 80, 100, 120, 120 } },
            Formants { { 325, 700, 2700, 3800, 4950 }, { 50, 60, 170, 180, 200 } },
        };
        return table;
    }

    // The formants (Hz) and bandwidths for a morph, cutoff and resonance.
    static Formants formantsFor (double cutoff, double resonance, double morph)
    {
        const auto position = juce::jlimit (0.0, 1.0, morph) * 4.0;
        const auto index = juce::jmin (3, (int) position);
        const auto frac = position - (double) index;
        const auto voice = juce::jlimit (0.0, 1.0, std::log2 (juce::jmax (1.0, cutoff) / 1000.0));
        const auto scale = cutoff < 1000.0 ? std::sqrt (juce::jmax (0.1, cutoff / 1000.0))
                                           : cutoff > 2000.0 ? std::sqrt (cutoff / 2000.0) : 1.0;
        const auto narrow = 1.6 - 1.3 * juce::jlimit (0.0, 1.0, resonance);
        Formants out;
        for (int f = 0; f < 5; ++f)
        {
            const auto at = [&] (const std::array<Formants, 5>& table, bool bandwidth)
            {
                const auto a = bandwidth ? table[(size_t) index].bandwidth[f] : table[(size_t) index].hz[f];
                const auto b = bandwidth ? table[(size_t) index + 1].bandwidth[f] : table[(size_t) index + 1].hz[f];
                return bandwidth ? a + (b - a) * frac : std::exp (std::log (a) + (std::log (b) - std::log (a)) * frac);
            };
            const auto hz = std::exp (std::log (at (male(), false)) + (std::log (at (female(), false)) - std::log (at (male(), false))) * voice);
            const auto bandwidth = at (male(), true) + (at (female(), true) - at (male(), true)) * voice;
            out.hz[f] = hz * scale;
            out.bandwidth[f] = bandwidth * scale * narrow;
        }
        return out;
    }

    void set (double sampleRate, double cutoff, double resonance, float morph)
    {
        const auto formants = formantsFor (cutoff, resonance, (double) morph);
        for (int f = 0; f < 5; ++f)
        {
            const auto hz = juce::jmin (formants.hz[f], sampleRate * 0.45);
            resonators[(size_t) f].set (sampleRate, hz, formants.bandwidth[f] / hz);
        }
    }
    void reset() { for (auto& r : resonators) r.reset(); }
    float process (float input)
    {
        auto x = (double) input;
        for (auto& r : resonators)
        {
            double lp, bp, hp;
            r.process (x, lp, bp, hp);
            x = lp;
        }
        if (! std::isfinite (x)) { reset(); return 0.0f; }
        return (float) (x * outputGain);
    }

    static constexpr double outputGain = 0.4;

private:
    std::array<Svf2, 5> resonators;
};

// Comb Body's body: five modes at a free-free tube's (a chime's) partial
// ratios, each a band-pass whose ring shortens with the mode number; fed by
// the comb at the cutoff (FilterUnit), MORPH blends comb and body.
class ModalBody
{
public:
    static constexpr double ratios[5] { 1.0, 2.756, 5.404, 8.933, 13.34 };

    void set (double sampleRate, double cutoff, double resonance)
    {
        const auto q = 8.0 + 120.0 * juce::jlimit (0.0, 1.0, resonance);
        for (int m = 0; m < 5; ++m)
        {
            const auto hz = cutoff * ratios[m];
            active[(size_t) m] = hz < sampleRate * 0.45;
            if (! active[(size_t) m])
                continue;
            // Higher modes decay faster (Q falls with the mode number).
            const auto modeQ = q / std::sqrt (ratios[m]);
            modes[(size_t) m].set (sampleRate, hz, 1.0 / modeQ);
            gains[(size_t) m] = 1.0 / std::sqrt (ratios[m]);
        }
    }
    void reset() { for (auto& m : modes) m.reset(); }
    double process (double x)
    {
        auto sum = 0.0;
        for (size_t m = 0; m < 5; ++m)
        {
            if (! active[m])
                continue;
            double lp, bp, hp;
            modes[m].process (x, lp, bp, hp);
            sum += bp * modes[m].k * gains[m];
        }
        return sum;
    }

private:
    std::array<Svf2, 5> modes;
    std::array<double, 5> gains {};
    std::array<bool, 5> active {};
};

} // namespace Filters2
