#pragma once

#include "Svf.h"

#include <cmath>
#include <complex>
#include <vector>

// One channel of one voice filter. Wraps every filter model behind a single
// type index so the voice, the response display and the tests share it.
namespace FilterType
{
// Stored in presets and sessions: only ever append.
enum
{
    LowPass = 0,
    BandPass,
    HighPass,
    Notch,
    LadderLow,
    LadderHigh,
    DiodeLow,
    Ms20Low,
    CombPlus,
    CombMinus,
    Formant,
    Morph,
    Count
};

inline juce::StringArray getNames()
{
    return { "Low Pass", "Band Pass", "High Pass", "Notch", "Ladder LP", "Ladder HP",
             "Diode LP", "MS-20 LP", "Comb +", "Comb -", "Formant", "Morph" };
}

inline bool isSvf (int type) { return type >= LowPass && type <= Notch; }
inline bool isLadder (int type) { return type == LadderLow || type == LadderHigh; }
inline bool isComb (int type) { return type == CombPlus || type == CombMinus; }
inline bool usesMorph (int type) { return type == Formant || type == Morph; }

// Vowel formants (Hz) for A E I O U and their relative levels; the Formant
// filter's morph walks through them.
struct Vowel
{
    double f1, f2, f3;
};

inline Vowel vowelAt (double morph)
{
    static const Vowel vowels[] { { 730.0, 1090.0, 2440.0 }, { 530.0, 1840.0, 2480.0 }, { 270.0, 2290.0, 3010.0 },
                                  { 570.0, 840.0, 2410.0 }, { 300.0, 870.0, 2240.0 } };

    const auto position = juce::jlimit (0.0, 1.0, morph) * 4.0;
    const auto index = juce::jmin (3, (int) position);
    const auto frac = position - (double) index;
    const auto& a = vowels[index];
    const auto& b = vowels[index + 1];

    return { a.f1 + (b.f1 - a.f1) * frac, a.f2 + (b.f2 - a.f2) * frac, a.f3 + (b.f3 - a.f3) * frac };
}

// Cutoff shifts the formants: 1 kHz leaves them where they are, each
// octave of cutoff moves them half an octave.
inline double formantShift (double cutoff) { return juce::jlimit (0.35, 3.0, std::sqrt (cutoff / 1000.0)); }

constexpr double formantGains[3] { 1.0, 0.55, 0.3 };

inline double combFeedback (double resonance) { return juce::jlimit (0.0, 1.0, resonance) * 0.97; }

// Linear (small-signal) frequency response for the response display.
// s = j * f / cutoff; morph is only used by Formant and Morph.
inline std::complex<double> response (int type, bool slope24, double resonance, std::complex<double> s,
                                      double morph = 0.0, double cutoff = 1000.0)
{
    resonance = juce::jlimit (0.0, 1.0, resonance);
    std::complex<double> h;

    if (isLadder (type) || type == DiodeLow)
    {
        const auto k = type == DiodeLow ? juce::jmin (3.6, resonance * 3.8) : juce::jmin (3.95, resonance * 4.15);
        const auto onePole = 1.0 + s;
        const auto loop = std::pow (onePole, 4) + k;

        if (type == DiodeLow)
            return (1.0 + 0.4 * k) * (slope24 ? 1.0 / loop : onePole / loop);

        if (type == LadderLow)
            h = (1.0 + 0.5 * k) * (slope24 ? 1.0 / loop : onePole * onePole / loop);
        else
            h = slope24 ? std::pow (s, 4) / loop : s * s * onePole * onePole / loop;

        return h;
    }

    if (type == Ms20Low)
    {
        const auto k = juce::jmin (1.96, resonance * 2.0);
        h = 1.0 / (s * s + (2.0 - k) * s + 1.0);
        return slope24 ? h / (1.0 + s) : h;
    }

    if (isComb (type))
    {
        // The comb's delay is one period of the cutoff frequency.
        const auto feedback = combFeedback (resonance) * (type == CombMinus ? -1.0 : 1.0);
        const auto delay = std::exp (-juce::MathConstants<double>::twoPi * s);
        return (1.0 - 0.5 * std::abs (feedback)) / (1.0 - feedback * delay);
    }

    const auto k = 2.0 - 2.0 * juce::jmin (0.98, resonance);
    const auto svf = [&s] (double kk, int mode, double scale)
    {
        const auto ss = s / scale;
        const auto denominator = ss * ss + kk * ss + 1.0;

        switch (mode)
        {
            case BandPass: return ss / denominator;
            case HighPass: return ss * ss / denominator;
            case Notch:    return (ss * ss + 1.0) / denominator;
            default:       return 1.0 / denominator;
        }
    };

    if (type == Formant)
    {
        const auto vowel = vowelAt (morph);
        const auto shift = formantShift (cutoff);
        const auto kk = 2.0 - 2.0 * (0.55 + 0.4 * resonance);
        const double formants[3] { vowel.f1, vowel.f2, vowel.f3 };

        for (int band = 0; band < 3; ++band)
            h += formantGains[band] * svf (kk, BandPass, formants[band] * shift / cutoff);

        return h * kk * 1.5;
    }

    if (type == Morph)
    {
        const auto m = juce::jlimit (0.0, 1.0, morph) * 2.0;
        const auto lp = svf (k, LowPass, 1.0), bp = svf (k, BandPass, 1.0), hp = svf (k, HighPass, 1.0);
        h = m < 1.0 ? lp + (bp - lp) * m : bp + (hp - bp) * (m - 1.0);
        return slope24 ? h * h : h;
    }

    h = svf (k, type, 1.0);
    return slope24 ? h * h : h;
}
} // namespace FilterType

// Four one-pole stages in a zero-delay feedback loop (Moog topology). The
// feedback sum is solved linearly and then saturated, which keeps the filter
// stable while letting resonance scream and self-oscillate.
class LadderFilter
{
public:
    struct Coefficients
    {
        double bigG = 0.0;     // g / (1 + g)
        double beta = 1.0;     // 1 / (1 + g)
        double k = 0.0;        // feedback, self-oscillates from about 4
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        const auto clampedCutoff = juce::jlimit (10.0, sampleRate * 0.45, cutoff);
        const auto g = std::tan (juce::MathConstants<double>::pi * clampedCutoff / sampleRate);

        Coefficients c;
        c.bigG = g / (1.0 + g);
        c.beta = 1.0 / (1.0 + g);
        c.k = juce::jlimit (0.0, 1.0, resonance) * 4.15;
        return c;
    }

    void setCoefficients (const Coefficients& c) { coeffs = c; }
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0; }

    // Returns the low-pass (or high-pass) output; slope24 picks four or two
    // stages.
    float process (float input, bool highPass, bool slope24)
    {
        const auto G = coeffs.bigG;
        const auto b = coeffs.beta;
        const auto k = coeffs.k;
        const auto G2 = G * G;
        const auto G3 = G2 * G;
        const auto G4 = G3 * G;

        const auto sigma = G3 * b * s[0] + G2 * b * s[1] + G * b * s[2] + b * s[3];

        // Low-pass loses passband level as feedback rises; make some back so
        // turning resonance up doesn't thin the sound out.
        const auto compensation = highPass ? 1.0 : 1.0 + 0.5 * k;
        auto u = ((double) input * compensation - k * sigma) / (1.0 + k * G4);
        u = std::tanh (u);

        double y[4];
        auto in = u;

        for (int stage = 0; stage < 4; ++stage)
        {
            const auto v = (in - s[stage]) * G;
            y[stage] = v + s[stage];
            s[stage] = y[stage] + v;
            in = y[stage];
        }

        if (highPass)
            return (float) (slope24 ? u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3]
                                    : u - 2.0 * y[0] + y[1]);

        return (float) (slope24 ? y[3] : y[1]);
    }

private:
    Coefficients coeffs;
    double s[4] {};
};

// A 303-flavoured diode ladder: the ladder's feedback runs through a
// high-pass (so resonance thins the bass instead of booming), every stage
// soft-clips asymmetrically, and the 12 dB setting taps three poles for the
// squelchy 18 dB character.
class DiodeFilter
{
public:
    struct Coefficients
    {
        double bigG = 0.0;
        double beta = 1.0;
        double k = 0.0;
        double highPass = 0.0; // one-pole HP coefficient in the feedback path
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        const auto base = LadderFilter::makeCoefficients (sampleRate, cutoff, resonance);
        Coefficients c;
        c.bigG = base.bigG;
        c.beta = base.beta;
        c.k = juce::jlimit (0.0, 1.0, resonance) * 3.8;
        c.highPass = std::exp (-juce::MathConstants<double>::twoPi * 110.0 / sampleRate);
        return c;
    }

    void setCoefficients (const Coefficients& c) { coeffs = c; }

    void reset()
    {
        s[0] = s[1] = s[2] = s[3] = 0.0;
        feedbackHp = feedbackIn = 0.0;
    }

    float process (float input, bool slope24)
    {
        const auto G = coeffs.bigG;
        const auto b = coeffs.beta;
        const auto k = coeffs.k;
        const auto G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const auto sigma = G3 * b * s[0] + G2 * b * s[1] + G * b * s[2] + b * s[3];

        // Solve the loop linearly (as the ladder does), then pull the bass
        // out of the feedback estimate with the one-pole high-pass.
        const auto linear = ((double) input * (1.0 + 0.4 * k) - k * sigma) / (1.0 + k * G4);
        const auto feedbackEstimate = (double) input * (1.0 + 0.4 * k) - linear;
        feedbackHp = coeffs.highPass * (feedbackHp + feedbackEstimate - feedbackIn);
        feedbackIn = feedbackEstimate;

        auto u = diodeClip ((double) input * (1.0 + 0.4 * k) - feedbackHp);

        double y[4];

        for (int stage = 0; stage < 4; ++stage)
        {
            const auto v = (u - s[stage]) * G;
            y[stage] = v + s[stage];
            s[stage] = y[stage] + v;
            u = stage < 3 ? diodeClip (y[stage]) : y[stage];
        }

        return (float) (slope24 ? y[3] : y[2]);
    }

private:
    // Asymmetric soft clip: a little even-harmonic grit like a diode pair.
    static double diodeClip (double x) { return std::tanh (x + 0.12 * x * x * (x > 0.0 ? 1.0 : 0.6)); }

    Coefficients coeffs;
    double s[4] {};
    double feedbackHp = 0.0, feedbackIn = 0.0;
};

// Korg-35 style Sallen-Key low-pass (the MS-20 filter): two poles with a
// saturating feedback path that gets gnarly and self-oscillates near the top.
class Ms20Filter
{
public:
    struct Coefficients
    {
        double bigG = 0.0;
        double k = 0.01;
        double alpha0 = 1.0;
        double lpf2Beta = 0.0;
        double hpf1Beta = 0.0;
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        const auto clampedCutoff = juce::jlimit (10.0, sampleRate * 0.45, cutoff);
        const auto g = std::tan (juce::MathConstants<double>::pi * clampedCutoff / sampleRate);

        Coefficients c;
        c.bigG = g / (1.0 + g);
        c.k = 0.01 + juce::jlimit (0.0, 1.0, resonance) * 1.99;
        c.lpf2Beta = (c.k - c.k * c.bigG) / (1.0 + g);
        c.hpf1Beta = -1.0 / (1.0 + g);
        c.alpha0 = 1.0 / (1.0 - c.k * c.bigG + c.k * c.bigG * c.bigG);
        return c;
    }

    void setCoefficients (const Coefficients& c) { coeffs = c; }
    void reset() { lpf1 = lpf2 = hpf1 = extra = 0.0; }

    // extraPole adds a plain one-pole at the cutoff for the 24 dB setting.
    float process (float input, bool extraPole)
    {
        const auto G = coeffs.bigG;

        const auto onePoleLow = [G] (double x, double& state)
        {
            const auto v = (x - state) * G;
            const auto low = v + state;
            state = low + v;
            return low;
        };

        const auto y1 = onePoleLow ((double) input, lpf1);
        const auto feedback = coeffs.hpf1Beta * hpf1 + coeffs.lpf2Beta * lpf2;
        auto u = std::tanh (coeffs.alpha0 * (y1 + feedback) * 1.2) / 1.2;
        auto y = coeffs.k * onePoleLow (u, lpf2);

        // High-pass stage in the feedback loop.
        const auto v = (y - hpf1) * G;
        const auto low = v + hpf1;
        hpf1 = low + v;

        y /= coeffs.k;
        return (float) (extraPole ? onePoleLow (y, extra) : y);
    }

private:
    Coefficients coeffs;
    double lpf1 = 0.0, lpf2 = 0.0, hpf1 = 0.0, extra = 0.0;
};

class FilterUnit
{
public:
    struct Coefficients
    {
        Svf::Coefficients svf;
        LadderFilter::Coefficients ladder;
        DiodeFilter::Coefficients diode;
        Ms20Filter::Coefficients ms20;
        Svf::Coefficients formants[3];
        float formantNorm = 1.0f;
        double combDelay = 1.0;
        double combFeedback = 0.0;
        float morph = 0.0f;
    };

    static Coefficients makeCoefficients (int type, double sampleRate, double cutoff, double resonance, float morph = 0.0f)
    {
        Coefficients c;
        c.morph = juce::jlimit (0.0f, 1.0f, morph);

        switch (type)
        {
            case FilterType::LadderLow:
            case FilterType::LadderHigh:
                c.ladder = LadderFilter::makeCoefficients (sampleRate, cutoff, resonance);
                break;

            case FilterType::DiodeLow:
                c.diode = DiodeFilter::makeCoefficients (sampleRate, cutoff, resonance);
                break;

            case FilterType::Ms20Low:
                c.ms20 = Ms20Filter::makeCoefficients (sampleRate, cutoff, resonance);
                break;

            case FilterType::CombPlus:
            case FilterType::CombMinus:
                c.combDelay = sampleRate / juce::jlimit (20.0, sampleRate * 0.45, cutoff);
                c.combFeedback = FilterType::combFeedback (resonance) * (type == FilterType::CombMinus ? -1.0 : 1.0);
                break;

            case FilterType::Formant:
            {
                const auto vowel = FilterType::vowelAt (morph);
                const auto shift = FilterType::formantShift (cutoff);
                const auto q = 0.55 + 0.4 * juce::jlimit (0.0, 1.0, resonance);
                const double formants[3] { vowel.f1, vowel.f2, vowel.f3 };

                for (int band = 0; band < 3; ++band)
                    c.formants[band] = Svf::makeCoefficients (sampleRate, formants[band] * shift, q);

                // A band-pass SVF peaks at 1/k; scale that back to unity so
                // resonance sharpens the vowel rather than blasting it.
                c.formantNorm = (float) (2.0 - 2.0 * q);
                break;
            }

            default:
                c.svf = Svf::makeCoefficients (sampleRate, cutoff, resonance);
                break;
        }

        return c;
    }

    // Allocates the comb delay line; call from prepare, not the audio thread.
    void prepare (double sampleRate)
    {
        combBuffer.assign ((size_t) juce::nextPowerOfTwo ((int) (sampleRate / 20.0) + 8), 0.0f);
        combMask = (int) combBuffer.size() - 1;
        reset();
    }

    void setType (int newType, bool newSlope24)
    {
        if (newType != type)
        {
            // Switching model: drop the old model's state so it can't leak a
            // click into the next note.
            reset();
            type = newType;
        }

        slope24 = newSlope24;

        if (FilterType::isSvf (type))
        {
            const auto mode = (Svf::Mode) type;
            stage1.setMode (mode);
            stage2.setMode (mode);
        }
        else if (type == FilterType::Formant)
        {
            for (auto& band : formantBands)
                band.setMode (Svf::Mode::BandPass);
        }
    }

    void setCoefficients (const Coefficients& c)
    {
        switch (type)
        {
            case FilterType::LadderLow:
            case FilterType::LadderHigh: ladder.setCoefficients (c.ladder); break;
            case FilterType::DiodeLow:   diode.setCoefficients (c.diode); break;
            case FilterType::Ms20Low:    ms20.setCoefficients (c.ms20); break;

            case FilterType::CombPlus:
            case FilterType::CombMinus:
                combDelay = juce::jlimit (1.0, (double) juce::jmax (1, combMask - 4), c.combDelay);
                combFeedback = c.combFeedback;
                break;

            case FilterType::Formant:
                for (int band = 0; band < 3; ++band)
                    formantBands[band].setCoefficients (c.formants[band]);

                formantNorm = c.formantNorm;
                break;

            default:
                stage1.setCoefficients (c.svf);
                stage2.setCoefficients (c.svf);
                break;
        }

        morph = c.morph;
    }

    void reset()
    {
        stage1.reset();
        stage2.reset();
        ladder.reset();
        diode.reset();
        ms20.reset();

        for (auto& band : formantBands)
            band.reset();

        std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        combWrite = 0;
        combDamp = 0.0f;
    }

    float process (float input)
    {
        switch (type)
        {
            case FilterType::LadderLow:  return ladder.process (input, false, slope24);
            case FilterType::LadderHigh: return ladder.process (input, true, slope24);
            case FilterType::DiodeLow:   return diode.process (input, slope24);

            case FilterType::Ms20Low:    return ms20.process (input, slope24);

            case FilterType::CombPlus:
            case FilterType::CombMinus:  return processComb (input);

            case FilterType::Formant:
            {
                auto sum = 0.0f;

                for (int band = 0; band < 3; ++band)
                    sum += (float) FilterType::formantGains[band] * formantBands[band].processSample (input);

                return sum * formantNorm * 1.5f;
            }

            case FilterType::Morph:
            {
                auto out = morphStage (stage1, input);
                return slope24 ? morphStage (stage2, out) : out;
            }

            default:
            {
                auto out = stage1.processSample (input);
                return slope24 ? stage2.processSample (out) : out;
            }
        }
    }

private:
    float morphStage (Svf& svf, float input)
    {
        float lp, bp, hp;
        svf.processAll (input, lp, bp, hp);
        const auto m = morph * 2.0f;
        return m < 1.0f ? lp + (bp - lp) * m : bp + (hp - bp) * (m - 1.0f);
    }

    float processComb (float input)
    {
        if (combBuffer.empty())
            return input;

        // Fractional read one period back, damped a touch in the loop so
        // high resonance rings like a plucked tube instead of whistling.
        const auto readPosition = (double) combWrite - combDelay;
        const auto base = (int) std::floor (readPosition);
        const auto frac = (float) (readPosition - (double) base);
        const auto a = combBuffer[(size_t) (base & combMask)];
        const auto b = combBuffer[(size_t) ((base + 1) & combMask)];
        const auto delayed = a + (b - a) * frac;

        combDamp += 0.7f * (delayed - combDamp);
        const auto fed = input + (float) combFeedback * combDamp;
        combBuffer[(size_t) combWrite] = std::tanh (fed * 0.5f) * 2.0f;
        combWrite = (combWrite + 1) & combMask;

        return fed * (1.0f - 0.5f * (float) std::abs (combFeedback));
    }

    int type = FilterType::LowPass;
    bool slope24 = false;
    float morph = 0.0f;
    float formantNorm = 1.0f;
    Svf stage1, stage2;
    Svf formantBands[3];
    LadderFilter ladder;
    DiodeFilter diode;
    Ms20Filter ms20;

    std::vector<float> combBuffer;
    int combMask = 0;
    int combWrite = 0;
    double combDelay = 1.0;
    double combFeedback = 0.0;
    float combDamp = 0.0f;
};
