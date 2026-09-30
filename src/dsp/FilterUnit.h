#pragma once

#include "Svf.h"
#include "FilterModels2.h"
#include "FilterCore.h"

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
    // M8.4 (appended; patches store the index)
    LadderBand, LadderDrive, Sem, OtaLow, OtaBand, Ms20High, Steiner, PhaserNotch,
    CombDamped, CombMorph, VowelBank, Talking, TwinPeak,
    Count
};

inline juce::StringArray getNames()
{
    return { "Low Pass", "Band Pass", "High Pass", "Notch", "Ladder LP", "Ladder HP",
             "Diode LP", "MS-20 LP", "Comb +", "Comb -", "Formant", "Morph",
             "Ladder BP", "Ladder Drive", "SEM", "OTA LP", "OTA BP", "MS-20 HP", "Steiner", "Phaser Notch",
             "Comb Damped", "Comb Morph", "Vowel", "Talking", "Twin Peak" };
}

inline bool isSvf (int type) { return type >= LowPass && type <= Notch; }
inline bool isLadder (int type) { return type == LadderLow || type == LadderHigh; }
inline bool isComb (int type) { return type == CombPlus || type == CombMinus || type == CombDamped || type == CombMorph; }
inline bool usesMorph (int type)
{
    return type == Formant || type == Morph || type == Sem || type == Steiner || type == PhaserNotch
           || type == CombMorph || type == VowelBank || type == Talking || type == TwinPeak;
}
// The M8.4 models ignore the 12/24 dB switch (each has its own order).
inline bool usesSlope (int type) { return type < LadderBand; }
inline bool isModel2 (int type) { return type >= LadderBand && type < Count && ! isComb (type); }

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

    // M8.4 models (small-signal).
    if (type >= LadderBand)
    {
        const auto onePole = 1.0 / (1.0 + s);
        const auto svfBank = [] (std::complex<double> ss, double kk, double& unused) { unused = 0.0; return ss * ss + kk * ss + 1.0; };
        double unused = 0.0;
        switch (type)
        {
            case LadderBand:
            {
                const auto k = 4.0 * resonance;
                const auto g2 = onePole * onePole, g3 = g2 * onePole, g4 = g3 * onePole;
                return 3.6 * (g2 - g3) / (1.0 + k * g4) * (1.0 + 0.5 * k) * 0.8;
            }
            case LadderDrive:
            {
                const auto k = 4.0 * resonance;
                return std::pow (onePole, 4) / (1.0 + k * std::pow (onePole, 4)) * (1.0 + 0.5 * k) * 0.8;
            }
            case OtaLow:
            {
                const auto k = 3.9 * resonance;
                return std::pow (onePole, 4) / (1.0 + k * std::pow (onePole, 4)) * (1.0 + 0.3 * k);
            }
            case OtaBand:
            {
                const auto k = 3.2 * resonance;
                const auto core = onePole * onePole * std::pow (1.0 - onePole, 2);
                return core / (1.0 + k * core) * (2.0 + k * 0.5) * (1.0 + 0.3 * k);
            }
            case Ms20High:
            {
                const auto k = juce::jmin (1.96, resonance * 2.0);
                return s * s / (s * s + (2.0 - k) * s + 1.0);
            }
            case Sem:
            {
                const auto k = 2.0 * (1.0 - 0.94 * resonance);
                const auto d = svfBank (s, k, unused);
                const auto lp = 1.0 / d, hp = s * s / d;
                const auto m = juce::jlimit (0.0, 1.0, morph);
                return m < 0.5 ? lp + hp * (2.0 * m) : hp + lp * (2.0 - 2.0 * m);
            }
            case Steiner:
            {
                const auto k = 2.0 - 1.97 * resonance;
                const auto d = svfBank (s, k, unused);
                const auto m = juce::jlimit (0.0, 1.0, morph) * 2.0;
                const auto low = juce::jmax (0.0, 1.0 - m), band = 1.0 - std::abs (m - 1.0), high = juce::jmax (0.0, m - 1.0);
                return (low + band * s * k * 1.2 + high * s * s) / d;
            }
            case PhaserNotch:
            {
                const auto spread = 1.0 + 3.0 * juce::jlimit (0.0, 1.0, morph);
                std::complex<double> a (1.0, 0.0);
                for (int i = 0; i < 4; ++i)
                {
                    const auto w = std::pow (spread, ((double) i - 1.5) / 1.5);
                    a *= (w - s) / (w + s);
                }
                const auto f = 0.85 * resonance;
                return 0.5 * (1.0 + a / (1.0 - f * a));
            }
            case TwinPeak:
            {
                const auto k = 1.2 - 1.15 * resonance;
                const auto second = std::pow (2.0, 2.0 * juce::jlimit (0.0, 1.0, morph));
                const auto d1 = svfBank (s, k, unused);
                const auto s2 = s / second;
                const auto d2 = svfBank (s2, k, unused);
                return 0.3 / d1 + (s / d1 + s2 / d2) * k * 0.8;
            }
            case VowelBank:
            case Talking:
            {
                // Five band-passes at the vowel's formants (as VowelFilter).
                Filters2::VowelFilter::Vowel a, b;
                const auto count = type == Talking ? 8 : 5;
                const auto position = juce::jlimit (0.0, 1.0, morph) * (double) (count - 1);
                const auto index = juce::jmin (count - 2, (int) position);
                const auto frac = position - (double) index;
                a = type == Talking ? Filters2::VowelFilter::talkingVowels()[(size_t) index] : Filters2::VowelFilter::maleVowels()[(size_t) index];
                b = type == Talking ? Filters2::VowelFilter::talkingVowels()[(size_t) index + 1] : Filters2::VowelFilter::maleVowels()[(size_t) index + 1];
                const auto shift = juce::jlimit (0.4, 2.5, std::sqrt (cutoff / 1000.0));
                const auto sharp = 1.6 - 1.2 * resonance;
                std::complex<double> sum;
                for (int f = 0; f < 5; ++f)
                {
                    const auto hz = std::exp (std::log (a[(size_t) f].hz) + (std::log (b[(size_t) f].hz) - std::log (a[(size_t) f].hz)) * frac) * shift;
                    const auto bw = (a[(size_t) f].bandwidth + (b[(size_t) f].bandwidth - a[(size_t) f].bandwidth) * frac) * sharp * shift;
                    const auto db = a[(size_t) f].db + (b[(size_t) f].db - a[(size_t) f].db) * frac;
                    const auto kk = bw / hz;
                    const auto ss = s * cutoff / hz;
                    sum += ss / (ss * ss + kk * ss + 1.0) * kk * std::pow (10.0, db / 20.0);
                }
                return sum * (type == Talking ? 3.2 : 1.6);
            }
            default:
                break;
        }
    }

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
        auto feedback = combFeedback (resonance) * (type == CombMinus ? -1.0 : 1.0);
        if (type == CombMorph)
            feedback = combFeedback (resonance) * (juce::jlimit (0.0, 1.0, morph) * 2.0 - 1.0);
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

// The Moog ladder: four one-pole stages in a zero-delay feedback loop
// (FilterCore::StageCascade), the loop's input sum saturating and solved
// exactly each sample, run at twice the sample rate. The linear loop
// oscillates exactly at the cutoff (k = 4), so full resonance plays in tune.
class LadderFilter
{
public:
    struct Coefficients
    {
        double g = 0.1;        // tan (pi fc / 2 fs): the stages run at 2x
        double k = 0.0;        // feedback, self-oscillates from 4
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        Coefficients c;
        c.g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        c.k = juce::jlimit (0.0, 1.0, resonance) * 4.15;
        return c;
    }

    void setCoefficients (const Coefficients& c)
    {
        coeffs = c;
        FilterCore::StageCascade::Settings settings;
        settings.g = c.g;
        settings.k = c.k;
        cascade.set (settings);
    }

    void reset()
    {
        cascade.reset();
        oversampler.reset();
    }

    // Returns the low-pass (or high-pass) output; slope24 picks four or two
    // stages.
    float process (float input, bool highPass, bool slope24)
    {
        // Low-pass loses passband level as feedback rises; make some back so
        // turning resonance up doesn't thin the sound out.
        const auto compensation = highPass ? 1.0 : 1.0 + 0.5 * coeffs.k;
        double first, second;
        oversampler.upsample ((double) input * compensation, first, second);
        const auto a = step (first, highPass, slope24);
        const auto b = step (second, highPass, slope24);
        return (float) oversampler.downsample (a, b);
    }

private:
    double step (double x, bool highPass, bool slope24)
    {
        std::array<double, 4> y;
        double u;
        cascade.process (x, y, u);

        if (highPass)
            return slope24 ? u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3]
                           : u - 2.0 * y[0] + y[1];

        return slope24 ? y[3] : y[1];
    }

    Coefficients coeffs;
    FilterCore::StageCascade cascade;
    FilterCore::Halfband2x oversampler;
};

// The diode ladder (FilterCore::DiodeLadderCore, equal capacitors): four
// nodes coupled by diode pairs so the stages load each other, which rounds
// the knee and spreads the resonance. The linear ladder's loop phase crosses
// -180 degrees at sqrt (10/7) of its stage frequency, where its gain is
// 1 / 18.39; at full resonance the core is scaled to oscillate exactly at
// the cutoff. Its passband is far below that point (-3 dB at a tenth of
// it), so as resonance falls the core slides up (4x at resonance 0) to keep
// the old model's brightness and level (fitted to levels-before-linux.txt,
// every cell within 1 dB). The feedback's level loss is made up at the
// input. The 12 dB setting taps the third node.
class DiodeFilter
{
public:
    struct Coefficients
    {
        double h = 0.1;       // the core's stage frequency (TPT)
        double k = 0.0;       // feedback, self-oscillates from 18.39
        double gain = 1.0;    // passband make-up into the core
        double post = 1.0;    // and after it
    };

    static constexpr double oscillationRatio = 1.1952286093343936; // sqrt (10 / 7)
    static constexpr double threshold = 18.392857142857142;         // 1 / |H (j w0)|
    // Fitted to the old model's level table and its seven presets' levels
    // and peaks (every one within 1 dB).
    static constexpr double lift0 = 4.034, liftPower = 1.475;
    static constexpr double compensationBase = 1.0, compensationPerK = 0.653;
    // Share of the make-up gain applied after the core (the rest drives it).
    static constexpr double outputShare = 0.2;
    static constexpr double diodeDrive = 0.5;
    // The output stage's soft ceiling (the old model clipped every stage).
    static constexpr double clipLevel = 0.8;

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        Coefficients c;
        const auto r = juce::jlimit (0.0, 1.0, resonance);
        const auto lift = std::pow (lift0, 1.0 - std::pow (r, liftPower));
        c.h = FilterCore::prewarp (sampleRate, cutoff * lift) / oscillationRatio;
        c.k = r * threshold * 1.02;
        const auto makeUp = compensationBase * (1.0 + compensationPerK * c.k);
        c.post = std::pow (makeUp, outputShare);
        c.gain = makeUp / c.post;
        return c;
    }

    void setCoefficients (const Coefficients& c)
    {
        coeffs = c;
        FilterCore::DiodeLadderCore::Settings settings;
        settings.h = c.h;
        settings.k = c.k;
        settings.diodeDrive = diodeDrive;
        core.set (settings);
    }

    void reset() { core.reset(); }

    float process (float input, bool slope24)
    {
        std::array<double, 4> y;
        core.process ((double) input * coeffs.gain, y);
        const auto out = (slope24 ? y[3] : y[2]) * coeffs.post;
        return (float) (clipLevel * FilterCore::tanhApprox (out / clipLevel));
    }

private:
    Coefficients coeffs;
    FilterCore::DiodeLadderCore core;
};

// The MS-20 low-pass (FilterCore::Korg35Core): two poles with the
// resonance fed back through a high-pass and a diode limiter, solved per
// sample at twice the sample rate. K reaches 2 (self-oscillation, exactly at
// the cutoff) at resonance 0.97; the limiter holds the tone there.
class Ms20Filter
{
public:
    struct Coefficients
    {
        double g = 0.1;       // tan (pi fc / 2 fs)
        double k = 0.01;
    };

    static Coefficients makeCoefficients (double sampleRate, double cutoff, double resonance)
    {
        Coefficients c;
        c.g = FilterCore::prewarp (2.0 * sampleRate, juce::jlimit (10.0, sampleRate * 0.45, cutoff));
        c.k = 0.01 + juce::jlimit (0.0, 1.0, resonance) * 2.05;
        return c;
    }

    void setCoefficients (const Coefficients& c)
    {
        core.set (c.g, c.k, false, limit);
        extraG = c.g / (1.0 + c.g);
    }

    void reset()
    {
        core.reset();
        extra.reset();
        oversampler.reset();
    }

    // extraPole adds a plain one-pole at the cutoff for the 24 dB setting.
    float process (float input, bool extraPole)
    {
        double first, second;
        oversampler.upsample ((double) input, first, second);
        auto a = core.process (first), b = core.process (second);
        if (extraPole)
        {
            a = extra.lowPass (a, extraG);
            b = extra.lowPass (b, extraG);
        }
        return (float) (oversampler.downsample (a, b) * outputGain);
    }

    // The limiter's level, and the output gain that keeps the old model's
    // level (its presets and level table within 1 dB).
    static constexpr double limit = 0.3, outputGain = 0.82;

private:
    FilterCore::Korg35Core core;
    FilterCore::OnePole extra;
    FilterCore::Halfband2x oversampler;
    double extraG = 0.1;
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
        // M8.4 models set themselves from these.
        double sampleRate = 48000.0, cutoff = 1000.0, resonance = 0.0;
    };

    static Coefficients makeCoefficients (int type, double sampleRate, double cutoff, double resonance, float morph = 0.0f)
    {
        Coefficients c;
        c.morph = juce::jlimit (0.0f, 1.0f, morph);
        c.sampleRate = sampleRate;
        c.cutoff = cutoff;
        c.resonance = juce::jlimit (0.0, 1.0, resonance);

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
            case FilterType::CombDamped:
            case FilterType::CombMorph:
                c.combDelay = sampleRate / juce::jlimit (20.0, sampleRate * 0.45, cutoff);
                c.combFeedback = FilterType::combFeedback (resonance) * (type == FilterType::CombMinus ? -1.0 : 1.0);
                if (type == FilterType::CombMorph)
                    c.combFeedback = FilterType::combFeedback (resonance) * ((double) c.morph * 2.0 - 1.0);
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
            case FilterType::CombDamped:
            case FilterType::CombMorph:
                combFeedback = c.combFeedback;
                // Comb Damped: the loop loses its highs fast (a plucked-tube
                // tone); the others keep the gentle 0.7 damping.
                combDampCoefficient = type == FilterType::CombDamped ? 0.18f : 0.7f;
                // The damping one-pole delays the loop too; take its phase
                // delay at the pitch off the line so the comb rings exactly
                // at the cutoff (Comb -: at half of it).
                combDelay = juce::jlimit (2.0, (double) juce::jmax (2, combMask - 4),
                                          c.combDelay - dampingPhaseDelay (combDampCoefficient, juce::MathConstants<double>::twoPi / c.combDelay));
                break;

            case FilterType::LadderBand:
            case FilterType::LadderDrive: driveLadder.set (c.sampleRate, c.cutoff, c.resonance, type == FilterType::LadderBand); break;
            case FilterType::Sem:          sem.set (c.sampleRate, c.cutoff, c.resonance, c.morph); break;
            case FilterType::OtaLow:
            case FilterType::OtaBand:      ota.set (c.sampleRate, c.cutoff, c.resonance, type == FilterType::OtaBand); break;
            case FilterType::Ms20High:     ms20High.set (c.sampleRate, c.cutoff, c.resonance); break;
            case FilterType::Steiner:      steiner.set (c.sampleRate, c.cutoff, c.resonance, c.morph); break;
            case FilterType::PhaserNotch:  phaser.set (c.sampleRate, c.cutoff, c.resonance, c.morph); break;
            case FilterType::VowelBank:
            case FilterType::Talking:      vowel.set (c.sampleRate, c.cutoff, c.resonance, c.morph, type == FilterType::Talking); break;
            case FilterType::TwinPeak:     twinPeak.set (c.sampleRate, c.cutoff, c.resonance, c.morph); break;

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
        driveLadder.reset();
        sem.reset();
        ota.reset();
        ms20High.reset();
        steiner.reset();
        phaser.reset();
        vowel.reset();
        twinPeak.reset();

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
            case FilterType::CombMinus:
            case FilterType::CombDamped:
            case FilterType::CombMorph:  return processComb (input);

            case FilterType::LadderBand:
            case FilterType::LadderDrive: return driveLadder.process (input);
            case FilterType::Sem:         return sem.process (input);
            case FilterType::OtaLow:
            case FilterType::OtaBand:     return ota.process (input);
            case FilterType::Ms20High:    return ms20High.process (input);
            case FilterType::Steiner:     return steiner.process (input);
            case FilterType::PhaserNotch: return phaser.process (input);
            case FilterType::VowelBank:
            case FilterType::Talking:     return vowel.process (input);
            case FilterType::TwinPeak:    return twinPeak.process (input);

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

    // Phase delay (samples) at w (radians per sample) of the loop's damping
    // one-pole y += c (x - y).
    static double dampingPhaseDelay (double c, double w)
    {
        const auto h = c / (std::complex<double> (1.0, 0.0) - (1.0 - c) * std::exp (std::complex<double> (0.0, -w)));
        return -std::arg (h) / w;
    }

    float processComb (float input)
    {
        if (combBuffer.empty())
            return input;

        // Fractional read one period back (cubic Lagrange, flat in phase
        // well past the pitches a comb plays), damped a touch in the loop so
        // high resonance rings like a plucked tube instead of whistling.
        const auto readPosition = (double) combWrite - combDelay;
        const auto base = (int) std::floor (readPosition);
        const auto d = (float) (readPosition - (double) base);
        const auto xm1 = combBuffer[(size_t) ((base - 1) & combMask)];
        const auto x0 = combBuffer[(size_t) (base & combMask)];
        const auto x1 = combBuffer[(size_t) ((base + 1) & combMask)];
        const auto x2 = combBuffer[(size_t) ((base + 2) & combMask)];
        const auto delayed = -d * (d - 1.0f) * (d - 2.0f) / 6.0f * xm1 + (d + 1.0f) * (d - 1.0f) * (d - 2.0f) / 2.0f * x0
                             - (d + 1.0f) * d * (d - 2.0f) / 2.0f * x1 + (d + 1.0f) * d * (d - 1.0f) / 6.0f * x2;

        combDamp += combDampCoefficient * (delayed - combDamp);
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
    // M8.4
    Filters2::DriveLadder driveLadder;
    Filters2::SemFilter sem;
    Filters2::OtaFilter ota;
    Filters2::Ms20HighPass ms20High;
    Filters2::SteinerParker steiner;
    Filters2::PhaserNotch phaser;
    Filters2::VowelFilter vowel;
    Filters2::TwinPeak twinPeak;
    float combDampCoefficient = 0.7f;

    std::vector<float> combBuffer;
    int combMask = 0;
    int combWrite = 0;
    double combDelay = 1.0;
    double combFeedback = 0.0;
    float combDamp = 0.0f;
};
