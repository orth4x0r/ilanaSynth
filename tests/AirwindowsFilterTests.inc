// Airwindows character filter models (src/dsp/AirwindowsCharacter.h): every
// model stays finite and bounded, filters the way its type says, comes out
// at a comparable level, and the Disperser is flat with the right group
// delay. Also prints ns/sample per model. Included by TableTest.cpp;
// ILANA_AWFILTER_TEST=1 runs only these.

#include "dsp/AirwindowsCharacter.h"

#include <chrono>
#include <cmath>
#include <functional>

namespace AwFilterLab
{
using namespace Airwindows;

template <class T> struct Tag { using type = T; };

// Runs fn (Tag<Model>, display name, kind) for every model. kind: 0 low-pass,
// 1 high-pass, 2 band-pass, 3 disperser.
template <class F>
void forEachModel (F&& fn)
{
    fn (Tag<ZLowpass> {}, "AW Z LP", 0);
    fn (Tag<ZHighpass> {}, "AW Z HP", 1);
    fn (Tag<ZBandpass> {}, "AW Z BP", 2);
    fn (Tag<AcidLowpass> {}, "AW Acid", 0);
    fn (Tag<XLowpass> {}, "AW X LP", 0);
    fn (Tag<YNotLowpass> {}, "AW YNot LP", 0);
    fn (Tag<HoltFilter> {}, "AW Holt", 0);
    fn (Tag<AngleFilter> {}, "AW Angle", 0);
    fn (Tag<PearFilter> {}, "AW Pear", 0);
    fn (Tag<Disperser> {}, "Disperser", 3);
}

constexpr double twoPi = 6.283185307179586;

// Steady-state amplitude gain and phase (output relative to the input sine)
// of a model at one setting.
template <class M>
void sineResponse (double sr, double cutoff, double resonance, float morph, double hz, double settleSeconds,
                   double measureSeconds, double amplitude, double& gain, double& phase)
{
    M model;
    model.reset();
    const auto settle = (int) (settleSeconds * sr);
    const auto measure = (int) (measureSeconds * sr);
    double sumCos = 0.0, sumSin = 0.0, energy = 0.0;
    for (int n = 0; n < settle + measure; ++n)
    {
        if (n % 16 == 0)
            model.set (sr, cutoff, resonance, morph);
        const auto angle = twoPi * hz * n / sr;
        const auto y = (double) model.process ((float) (amplitude * std::sin (angle)));
        if (n >= settle)
        {
            sumCos += y * std::cos (angle);
            sumSin += y * std::sin (angle);
            energy += y * y;
        }
    }
    // y ~ A sin (angle + phase): sumSin = A/2 cos (phase) N, sumCos = A/2 sin (phase) N
    gain = std::sqrt (energy / measure * 2.0) / amplitude;
    phase = std::atan2 (sumCos, sumSin);
}

template <class M>
double gainAt (double sr, double cutoff, double resonance, float morph, double hz, double amplitude = 0.1)
{
    double gain, phase;
    sineResponse<M> (sr, cutoff, resonance, morph, hz, 0.3, 0.25, amplitude, gain, phase);
    return gain;
}

// Plain 2-pole reference (RBJ, Q 0.7071).
struct Plain2
{
    double a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0, s1 = 0, s2 = 0;
    Plain2 (int kind, double sr, double fc)
    {
        const auto k = std::tan (juce::MathConstants<double>::pi * fc / sr);
        const auto q = 0.7071;
        const auto norm = 1.0 / (1.0 + k / q + k * k);
        if (kind == 0) { a0 = k * k * norm; a1 = 2.0 * a0; a2 = a0; }
        else if (kind == 1) { a0 = norm; a1 = -2.0 * norm; a2 = norm; }
        else { a0 = k / q * norm; a1 = 0.0; a2 = -a0; }
        b1 = 2.0 * (k * k - 1.0) * norm;
        b2 = (1.0 - k / q + k * k) * norm;
    }
    double process (double x)
    {
        const auto y = a0 * x + s1;
        s1 = a1 * x - b1 * y + s2;
        s2 = a2 * x - b2 * y;
        return y;
    }
};

// RMS of a -12 dBFS 110 Hz saw through the model against the reference.
template <class M>
double sawLevelDb (int kind, double sr, double cutoff, double resonance, float morph)
{
    M model;
    model.reset();
    Plain2 reference (kind == 3 ? 0 : kind, sr, cutoff);
    const auto samples = (int) (sr * 1.5);
    double modelEnergy = 0.0, referenceEnergy = 0.0;
    for (int n = 0; n < samples; ++n)
    {
        if (n % 16 == 0)
            model.set (sr, cutoff, resonance, morph);
        const auto phase = std::fmod (110.0 * n / sr, 1.0);
        const auto x = 0.2512 * (2.0 * phase - 1.0);
        const auto y = (double) model.process ((float) x);
        const auto z = kind == 3 ? x : reference.process (x);
        if (n > samples / 3)
        {
            modelEnergy += y * y;
            referenceEnergy += z * z;
        }
    }
    return 10.0 * std::log10 (modelEnergy / referenceEnergy);
}

// Loud noise with the cutoff, resonance and morph thrown around every 16
// samples; returns the peak, or -1 when anything went non-finite.
template <class M>
double abuse (double sr, int seed, bool sweepSmoothly)
{
    M model;
    model.reset();
    juce::Random random (seed);
    const auto samples = (int) (sr * 2.0);
    double peak = 0.0, cutoff = 1000.0, resonance = 0.5, morph = 0.5;
    for (int n = 0; n < samples; ++n)
    {
        if (n % 16 == 0)
        {
            if (sweepSmoothly)
            {
                cutoff = 20.0 * std::pow (1000.0, 0.5 + 0.5 * std::sin (twoPi * 3.0 * n / sr));
                resonance = 0.5 + 0.5 * std::sin (twoPi * 0.7 * n / sr);
                morph = 0.5 + 0.5 * std::sin (twoPi * 0.4 * n / sr);
            }
            else
            {
                cutoff = 20.0 * std::pow (1000.0, (double) random.nextFloat());
                resonance = random.nextFloat();
                morph = random.nextFloat();
            }
            model.set (sr, cutoff, resonance, (float) morph);
        }
        const auto y = model.process (random.nextFloat() * 2.0f - 1.0f);
        if (! std::isfinite (y))
            return -1.0;
        peak = std::max (peak, (double) std::fabs (y));
    }
    return peak;
}

// Noise, then silence at a steady setting: the tail must die away.
template <class M>
double tailAfterNoise (double sr)
{
    M model;
    model.reset();
    juce::Random random (7);
    for (int n = 0; n < (int) (sr * 0.5); ++n)
    {
        if (n % 16 == 0)
            model.set (sr, 1000.0, 0.5, 0.5f);
        model.process (random.nextFloat() * 2.0f - 1.0f);
    }
    double last = 0.0;
    for (int n = 0; n < (int) (sr * 3.0); ++n)
    {
        if (n % 16 == 0)
            model.set (sr, 1000.0, 0.5, 0.5f);
        const auto y = model.process (0.0f);
        if (n > (int) (sr * 2.5))
            last = std::max (last, (double) std::fabs (y));
    }
    return last;
}

// Phase-derived group delay in seconds at hz (finite difference over +-0.5%).
template <class M>
double groupDelay (double sr, double cutoff, double resonance, float morph, double hz, double settle)
{
    double gain, low, high;
    sineResponse<M> (sr, cutoff, resonance, morph, hz * 0.995, settle, 0.25, 0.3, gain, low);
    sineResponse<M> (sr, cutoff, resonance, morph, hz * 1.005, settle, 0.25, 0.3, gain, high);
    auto difference = high - low;
    while (difference > juce::MathConstants<double>::pi) difference -= twoPi;
    while (difference < -juce::MathConstants<double>::pi) difference += twoPi;
    return -difference / (twoPi * hz * 0.01);
}

struct PlainBiquad
{
    double a0 = 0.02, a1 = 0.04, a2 = 0.02, b1 = -1.5, b2 = 0.6, s1 = 0, s2 = 0;
    float process (float x)
    {
        const auto y = a0 * x + s1;
        s1 = a1 * x - b1 * y + s2;
        s2 = a2 * x - b2 * y;
        return (float) y;
    }
};

template <class M>
double nsPerSample (double resonance, float morph)
{
    M model;
    model.reset();
    const auto samples = 1 << 19;
    juce::Random random (3);
    std::vector<float> input ((size_t) samples);
    for (auto& v : input)
        v = 0.5f * (random.nextFloat() * 2.0f - 1.0f);
    float sink = 0.0f;
    double best = 1e9;
    for (int pass = 0; pass < 3; ++pass)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int n = 0; n < samples; ++n)
        {
            if ((n & 15) == 0)
                model.set (48000.0, 800.0 + 200.0 * (double) ((n >> 4) & 7), resonance, morph);
            sink += model.process (input[(size_t) n]);
        }
        const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        best = std::min (best, seconds * 1e9 / samples);
    }
    if (sink == 12345.678f)
        std::cout << "";
    return best;
}
} // namespace AwFilterLab

void runAirwindowsFilterTests()
{
    using namespace AwFilterLab;
    std::cout << "\n=== Airwindows character filters ===" << std::endl;

    // 1. finite and bounded under loud noise and fast sweeps, three rates
    forEachModel ([&] (auto tag, const char* name, int)
    {
        using M = typename decltype (tag)::type;
        double worst = 0.0;
        bool finite = true;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int variant = 0; variant < 2; ++variant)
            {
                const auto peak = abuse<M> (sr, 11 + variant, variant == 1);
                if (peak < 0.0)
                    finite = false;
                worst = std::max (worst, peak);
            }
        check (finite && worst < 60.0, juce::String (name) + " is finite and bounded under noise and sweeps (peak "
                                           + juce::String (worst, 2) + ")");
    });

    // 2. the tail dies away
    forEachModel ([&] (auto tag, const char* name, int kind)
    {
        using M = typename decltype (tag)::type;
        if (kind == 3)
            return; // the disperser rings for seconds by design, and is all-pass
        const auto tail = tailAfterNoise<M> (48000.0);
        check (tail < 0.01, juce::String (name) + " tail dies away (" + juce::String (tail, 6) + ")");
    });

    // 3. each type does its job at cutoff 1 kHz, resonance 0.3
    forEachModel ([&] (auto tag, const char* name, int kind)
    {
        using M = typename decltype (tag)::type;
        if (kind == 3)
            return;
        const auto low = gainAt<M> (48000.0, 1000.0, 0.3, 0.0f, 200.0);
        const auto high = gainAt<M> (48000.0, 1000.0, 0.3, 0.0f, 8000.0);
        if (kind == 0)
            check (high < 0.5 * low, juce::String (name) + " attenuates 8 kHz against 200 Hz (" + juce::String (20.0 * std::log10 (high / low), 1) + " dB)");
        else if (kind == 1)
            check (low < 0.5 * high, juce::String (name) + " attenuates 200 Hz against 8 kHz (" + juce::String (20.0 * std::log10 (low / high), 1) + " dB)");
        else
        {
            double best = 0.0, bestHz = 0.0;
            for (double hz = 100.0; hz < 10000.0; hz *= 1.122)
            {
                const auto g = gainAt<M> (48000.0, 1000.0, 0.3, 0.0f, hz);
                if (g > best) { best = g; bestHz = hz; }
            }
            check (bestHz > 700.0 && bestHz < 1400.0 && best > 1.15 * low && best > 1.15 * high,
                   juce::String (name) + " peaks near its cutoff (" + juce::String (bestHz, 0) + " Hz)");
        }
    });

    // 3b. the cutoff moves the response (low-passes: half the cutoff, less 4 kHz)
    forEachModel ([&] (auto tag, const char* name, int kind)
    {
        using M = typename decltype (tag)::type;
        if (kind != 0)
            return;
        const auto closed = gainAt<M> (48000.0, 500.0, 0.3, 0.0f, 2000.0);
        const auto open = gainAt<M> (48000.0, 4000.0, 0.3, 0.0f, 2000.0);
        check (open > 2.0 * closed, juce::String (name) + " opens with the cutoff (2 kHz: "
                                        + juce::String (20.0 * std::log10 (closed), 1) + " dB at 500 Hz, "
                                        + juce::String (20.0 * std::log10 (open), 1) + " dB at 4 kHz)");
    });

    // 4. level normalisation
    forEachModel ([&] (auto tag, const char* name, int kind)
    {
        using M = typename decltype (tag)::type;
        const auto db = sawLevelDb<M> (kind, 48000.0, 1000.0, 0.3, 0.0f);
        check (std::fabs (db) < 3.0, juce::String (name) + " level against a plain 2-pole " + (kind == 3 ? "(the input)" : kind == 1 ? "high-pass" : kind == 2 ? "band-pass" : "low-pass")
                                         + " is " + juce::String (db, 2) + " dB");
    });
    {
        // and across resonance / morph, reported (the tables are per resonance)
        forEachModel ([&] (auto tag, const char* name, int kind)
        {
            using M = typename decltype (tag)::type;
            if (kind == 3)
                return;
            double lo = 100.0, hi = -100.0;
            for (double res : { 0.0, 0.5, 1.0 })
                for (float morph : { 0.0f, 1.0f })
                {
                    const auto db = sawLevelDb<M> (kind, 48000.0, 1000.0, res, morph);
                    lo = std::min (lo, db);
                    hi = std::max (hi, db);
                }
            std::cout << "  level spread over resonance and morph, " << name << ": " << lo << " .. " << hi << " dB" << std::endl;
            check (lo > -9.0 && hi < 9.0, juce::String (name) + " level stays within 9 dB over resonance and morph");
        });
    }

    // 5. the disperser: flat magnitude, group delay at the cutoff, growing with stages
    {
        struct Setting { int stages; double resonance; double cutoff; };
        const Setting settings[] = { { 1, 0.0, 1000.0 }, { 16, 0.5, 500.0 }, { 64, 1.0, 1000.0 }, { 64, 1.0, 6000.0 }, { 33, 0.5, 2000.0 } };
        double worst = 0.0;
        for (const auto& s : settings)
        {
            const auto morph = (float) ((s.stages - 1) / 63.0);
            const auto q = 0.5 * std::pow (16.0, s.resonance);
            const auto settle = std::min (4.0, 0.3 + 2.0 * s.stages * 4.0 * q / (twoPi * s.cutoff) + 16.0 * q / (twoPi * 100.0));
            for (double hz : { 100.0, 200.0, 450.0, 900.0, 1800.0, 3500.0, 7000.0, 10000.0 })
            {
                double gain, phase;
                sineResponse<Disperser> (48000.0, s.cutoff, s.resonance, morph, hz, settle, 0.3, 0.3, gain, phase);
                worst = std::max (worst, std::fabs (20.0 * std::log10 (gain)));
            }
        }
        check (worst < 0.5, "Disperser magnitude is flat within 0.5 dB, 100 Hz to 10 kHz, at 5 settings (worst "
                                + juce::String (worst, 3) + " dB)");

        const double grid[] = { 250.0, 500.0, 700.0, 850.0, 1000.0, 1150.0, 1400.0, 2000.0, 4000.0 };
        auto peakOf = [&] (double cutoff, int stages, double resonance, double& peakDelay)
        {
            const auto morph = (float) ((stages - 1) / 63.0);
            double best = -1e9, bestHz = 0.0;
            for (double hz : grid)
            {
                const auto scaled = hz * cutoff / 1000.0;
                const auto delay = groupDelay<Disperser> (48000.0, cutoff, resonance, morph, scaled, 0.6);
                if (delay > best) { best = delay; bestHz = hz; }
            }
            peakDelay = best;
            return bestHz;
        };
        double delay4 = 0.0, delay16 = 0.0, delay4High = 0.0;
        const auto peak4 = peakOf (1000.0, 4, 0.5, delay4);
        const auto peak16 = peakOf (1000.0, 16, 0.5, delay16);
        const auto peakHigh = peakOf (2000.0, 4, 0.5, delay4High);
        check (peak4 == 1000.0 && peak16 == 1000.0, "Disperser group delay peaks at the cutoff (" + juce::String (peak4, 0) + ", " + juce::String (peak16, 0) + " Hz for 1 kHz)");
        const auto expected4 = 4.0 * 4 * 2.0 / (twoPi * 1000.0);
        check (std::fabs (delay4 / expected4 - 1.0) < 0.15, "Disperser peak delay matches 4 Q / w0 per stage (" + juce::String (delay4 * 1000.0, 2) + " ms, expected " + juce::String (expected4 * 1000.0, 2) + ")");
        check (delay16 > 3.4 * delay4 && delay16 < 4.6 * delay4, "Disperser group delay grows with the stage count (4 stages " + juce::String (delay4 * 1000.0, 2) + " ms, 16 stages " + juce::String (delay16 * 1000.0, 2) + " ms)");
        // moving the cutoff moves the peak: 2 kHz cutoff peaks at 2 kHz (grid scaled) with half the delay
        check (peakHigh == 1000.0 && delay4High < 0.6 * delay4 && delay4High > 0.4 * delay4,
               "Disperser peak moves with the cutoff (2 kHz: delay " + juce::String (delay4High * 1000.0, 2) + " ms)");

        // crossfading the stage count is smooth: sweep the morph across many integers on a sine
        {
            Disperser model;
            model.reset();
            double maxStep = 0.0, previous = 0.0;
            const auto samples = 96000;
            for (int n = 0; n < samples; ++n)
            {
                if (n % 16 == 0)
                    model.set (48000.0, 800.0, 0.3, (float) n / samples);
                const auto y = (double) model.process (0.3f * (float) std::sin (twoPi * 300.0 * n / 48000.0));
                if (n > 0)
                    maxStep = std::max (maxStep, std::fabs (y - previous));
                previous = y;
            }
            check (maxStep < 0.1, "Disperser stage count sweeps without clicks (largest step " + juce::String (maxStep, 4) + ")");
        }
    }

    // 6. reset() returns a model to a fresh state
    forEachModel ([&] (auto tag, const char* name, int)
    {
        using M = typename decltype (tag)::type;
        M used, fresh;
        used.reset();
        fresh.reset();
        juce::Random random (5);
        for (int n = 0; n < 4000; ++n)
        {
            if (n % 16 == 0)
                used.set (48000.0, 900.0, 0.6, 0.4f);
            used.process (random.nextFloat() - 0.5f);
        }
        used.reset();
        double difference = 0.0;
        for (int n = 0; n < 2000; ++n)
        {
            if (n % 16 == 0)
            {
                used.set (48000.0, 2500.0, 0.2, 0.7f);
                fresh.set (48000.0, 2500.0, 0.2, 0.7f);
            }
            const auto x = 0.4f * (float) std::sin (0.05 * n);
            difference = std::max (difference, (double) std::fabs (used.process (x) - fresh.process (x)));
        }
        check (difference < 1e-6, juce::String (name) + " reset() matches a fresh instance (" + juce::String (difference, 9) + ")");
    });

    // 7. CPU
    {
        PlainBiquad biquad;
        juce::Random random (3);
        std::vector<float> input (1 << 19);
        for (auto& v : input)
            v = 0.5f * (random.nextFloat() * 2.0f - 1.0f);
        float sink = 0.0f;
        double baseline = 1e9;
        for (int pass = 0; pass < 3; ++pass)
        {
            const auto start = std::chrono::steady_clock::now();
            for (auto v : input)
                sink += biquad.process (v);
            baseline = std::min (baseline, std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count() * 1e9 / (double) input.size());
        }
        if (sink == 12345.678f)
            std::cout << "";
        std::cout << "  CPU (ns/sample, set() every 16 samples included): plain biquad " << baseline << std::endl;
        forEachModel ([&] (auto tag, const char* name, int kind)
        {
            using M = typename decltype (tag)::type;
            const auto typical = nsPerSample<M> (0.3, kind == 3 ? 0.25f : 0.0f);
            const auto heavy = nsPerSample<M> (1.0, 1.0f);
            std::cout << "  CPU " << name << ": " << typical << " ns typical (" << typical / baseline << "x biquad), "
                      << heavy << " ns at resonance 1 / morph 1" << (kind == 3 ? " (64 stages)" : "") << std::endl;
            checkTiming (typical < 40.0 * baseline + 200.0, juce::String (name) + " typical CPU is sane");
        });
    }
}
