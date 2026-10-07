#pragma once

// M8.3: the west-coast voice. A wavefolder (FOLD, SYMMETRY, 1-4 stages)
// into a low-pass gate: a filter and an amplifier in one, both steered by a
// vactrol, so a strike opens it fast and it closes slowly, getting darker as
// it gets quieter (the "bongo").
//
// Folder: cascaded sine folds, y = sin(pi/2 g x), each stage feeding the
// next, with first-order antiderivative antialiasing (Parker, Zavalishin and
// Le Bivic, DAFx 2016) so heavy folding doesn't alias at the base rate.
//
// Vactrol: an LED lighting a photocell. Its response is asymmetric and
// level dependent (Parker and D'Angelo, "A digital model of the Buchla
// lowpass-gate", DAFx 2013): it lights up in about 12 ms and goes dark over
// about 250 ms, and the darker the cell, the slower it gets, which gives the
// long tail. The gate's cutoff follows the cell's conductance (a 2-pole
// low-pass), its gain a power of it.

#include <juce_core/juce_core.h>

#include <array>
#include <cmath>

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_WEST_SSE 1
 #include <emmintrin.h>
 #include <immintrin.h>
 // The folder's AVX path is compiled for AVX on its own and picked at run
 // time (the build targets SSE2).
 #if defined(__GNUC__) || defined(__clang__)
  #define ILANA_AVX_TARGET __attribute__ ((target ("avx")))
 #else
  #define ILANA_AVX_TARGET
 #endif
#else
 #define ILANA_WEST_SSE 0
#endif

#include "WestCoastTuning.h"

class Wavefolder
{
public:
    void reset()
    {
        previous.fill (0.0);
        previousCosine.fill (1.0);
        upHistory.fill (0.0f);
        downHistory.fill (0.0f);
        historyPosition = 0;
        dcIn = dcOut = 0.0f;
    }

    // fold 0..1 (0 is almost clean, 1 folds about a dozen times), symmetry
    // -1..1 (a bias: even harmonics), 1-4 stages.
    void setParams (float fold, float symmetry, int newStages)
    {
        gain = 0.35 + 11.65 * (double) (fold * fold);
        makeUp = 1.0 / std::sin (juce::MathConstants<double>::halfPi * juce::jmin (1.0, gain));
        bias = 0.5 * (double) symmetry;
        stages = juce::jlimit (1, maxStages, newStages);
    }

    // Four times oversampled (a 32-tap windowed-sinc polyphase FIR each
    // way), with the antialiased fold at the high rate.
    //
    // Each history is written twice, at i and i + length, so a run of taps
    // reads one straight span with no wrap. The four phases of a sample are
    // independent until the fold's memory joins them, so each step runs over
    // all four at once (they overlap in the CPU).
    float process (float input)
    {
        const auto& taps = firTaps();
        upHistory[(size_t) historyPosition] = input;
        upHistory[(size_t) (historyPosition + historyLength)] = input;
        // newestUp[-t] is the input t samples back.
        const auto* newestUp = upHistory.data() + historyPosition + historyLength;

        // Interpolate: the four phases of the 4x signal.
        double x[factor] {};
        for (int t = 0; t < tapsPerPhase; ++t)
        {
            const auto sample = (double) newestUp[-t];
            for (int phase = 0; phase < factor; ++phase)
                x[phase] += (double) taps[(size_t) (t * factor + phase)] * sample;
        }
        for (auto& v : x)
            v = v * (double) factor * gain + bias;

        // The fold, stage by stage.
        const auto stretch = 1.0 + 0.35 * gain / (double) stages; // each stage folds the last a little further
        for (int stage = 0; stage < stages; ++stage)
        {
            // First-order ADAA: the mean of the fold over the step from the
            // last input to this one, (F(x) - F(last)) / (x - last), with
            // F(x) = -cos(pi/2 x) / (pi/2).
            double cosine[factor];
            for (int phase = 0; phase < factor; ++phase)
                cosine[phase] = cosHalfPi (x[phase]);

            auto last = previous[(size_t) stage], lastCosine = previousCosine[(size_t) stage];
            for (int phase = 0; phase < factor; ++phase)
            {
                const auto difference = x[phase] - last;
                const auto y = std::abs (difference) < 1.0e-5 ? fold (0.5 * (x[phase] + last))
                                                               : (lastCosine - cosine[phase]) / (juce::MathConstants<double>::halfPi * difference);
                last = x[phase];
                lastCosine = cosine[phase];
                x[phase] = stage + 1 < stages ? y * stretch : y;
            }
            previous[(size_t) stage] = last;
            previousCosine[(size_t) stage] = lastCosine;
        }

        const auto downBase = historyPosition * factor;
        for (int phase = 0; phase < factor; ++phase)
        {
            const auto folded = (float) (x[phase] * makeUp);
            downHistory[(size_t) (downBase + phase)] = folded;
            downHistory[(size_t) (downBase + phase + downLength)] = folded;
        }

        // Decimate: the FIR again, over the 4x samples (four running sums).
        const auto* newestDown = downHistory.data() + downBase + factor - 1 + downLength;
        double sums[4] {};
        for (int t = 0; t < numTaps; t += 4)
            for (int k = 0; k < 4; ++k)
                sums[k] += (double) taps[(size_t) (t + k)] * (double) newestDown[-(t + k)];
        const auto out = (sums[0] + sums[1]) + (sums[2] + sums[3]);
        if (++historyPosition >= historyLength)
            historyPosition = 0;

        // The bias leaves an offset; block it.
        const auto y = (float) out;
        const auto blocked = y - dcIn + 0.9995f * dcOut;
        dcIn = y;
        dcOut = std::isfinite (blocked) ? blocked : 0.0f;
        return dcOut;
    }

    static double fold (double x) { return std::sin (juce::MathConstants<double>::halfPi * x); }

    // cos (pi/2 x) to within a few units in the last place (the library's
    // cosine, several times faster): x = q + f with q the nearest integer
    // (the quadrant) and |f| <= 1/2, then +-sin or +-cos of pi/2 f by
    // fdlibm's kernels. No branches: a folding signal changes quadrant all
    // the time.
    static double cosHalfPi (double x)
    {
        if (! (std::abs (x) < 1.0e8))
            return std::cos (juce::MathConstants<double>::halfPi * x);
        // (The offset keeps the truncation a floor.)
        const auto q = (long long) (x + (0.5 + 1073741824.0)) - 1073741824LL;
        const auto t = juce::MathConstants<double>::halfPi * (x - (double) q);
        const auto t2 = t * t;
        const double values[2] { cosSeries (t2), sinSeries (t, t2) };
        static constexpr double signs[4] { 1.0, -1.0, -1.0, 1.0 };
        return signs[q & 3] * values[q & 1];
    }

private:
    friend class StereoWavefolder;

    // fdlibm's kernels (__kernel_cos, __kernel_sin): within an ulp on
    // |t| <= pi/4.
    static constexpr double c1 = 4.16666666666666019037e-02, c2 = -1.38888888888741095749e-03, c3 = 2.48015872894767294178e-05,
                            c4 = -2.75573143513906633035e-07, c5 = 2.08757232129817482790e-09, c6 = -1.13596475577881948265e-11;
    static constexpr double s1 = -1.66666666666666324348e-01, s2 = 8.33333333332248946124e-03, s3 = -1.98412698298579493134e-04,
                            s4 = 2.75573137070700676789e-06, s5 = -2.50507602534068634195e-08, s6 = 1.58969099521155010221e-10;

    static double cosSeries (double t2)
    {
        return (1.0 - 0.5 * t2) + t2 * t2 * (c1 + t2 * (c2 + t2 * (c3 + t2 * (c4 + t2 * (c5 + t2 * c6)))));
    }

    static double sinSeries (double t, double t2)
    {
        return t + t * t2 * (s1 + t2 * (s2 + t2 * (s3 + t2 * (s4 + t2 * (s5 + t2 * s6)))));
    }

    static constexpr int maxStages = 4;
    static constexpr int factor = 4, tapsPerPhase = 8, numTaps = factor * tapsPerPhase;
    static constexpr int historyLength = tapsPerPhase, downLength = numTaps;

    static const std::array<float, numTaps>& firTaps()
    {
        // Low-pass at a quarter of the 4x rate's Nyquist (the base rate's
        // Nyquist), Blackman windowed, unity DC gain.
        static const auto taps = []
        {
            std::array<float, numTaps> t {};
            auto sum = 0.0;
            for (int i = 0; i < numTaps; ++i)
            {
                const auto m = (double) i - (numTaps - 1) * 0.5;
                const auto x = juce::MathConstants<double>::pi * m / (double) factor * 0.9;
                const auto sinc = std::abs (m) < 1.0e-9 ? 1.0 : std::sin (x) / x;
                const auto w = 0.42 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (numTaps - 1))
                               + 0.08 * std::cos (2.0 * juce::MathConstants<double>::twoPi * i / (numTaps - 1));
                t[(size_t) i] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (auto& v : t)
                v = (float) ((double) v / sum);
            return t;
        }();
        return taps;
    }

    std::array<double, maxStages> previous {}, previousCosine {};
    std::array<float, 2 * historyLength> upHistory {};
    std::array<float, 2 * downLength> downHistory {};
    int historyPosition = 0;
    double gain = 1.0, bias = 0.0, makeUp = 1.0;
    int stages = 1;
    float dcIn = 0.0f, dcOut = 0.0f;
};

// Both sides of a stereo voice through one folder setting: the same
// arithmetic as a Wavefolder on each side (the same result, sample for
// sample), with the two sides side by side in SSE2 registers.
class StereoWavefolder
{
public:
    void reset()
    {
       #if ILANA_WEST_SSE
        std::fill (std::begin (upHistory), std::end (upHistory), 0.0);
        std::fill (std::begin (downHistory), std::end (downHistory), 0.0);
        for (int stage = 0; stage < maxStages; ++stage)
        {
            previous[stage] = _mm_setzero_pd();
            previousCosine[stage] = _mm_set1_pd (1.0);
        }
        std::fill (std::begin (upFloat), std::end (upFloat), 0.0f);
        std::fill (std::begin (downFloat), std::end (downFloat), 0.0f);
        for (auto& p : previousFloat)
            p[0] = p[1] = 0.0f;
        historyPosition = 0;
        dcIn[0] = dcIn[1] = dcOut[0] = dcOut[1] = 0.0f;
       #else
        left.reset();
        right.reset();
       #endif
    }

    void setParams (float fold, float symmetry, int newStages)
    {
       #if ILANA_WEST_SSE
        gain = 0.35 + 11.65 * (double) (fold * fold);
        makeUp = 1.0 / std::sin (juce::MathConstants<double>::halfPi * juce::jmin (1.0, gain));
        bias = 0.5 * (double) symmetry;
        stages = juce::jlimit (1, maxStages, newStages);
       #else
        left.setParams (fold, symmetry, newStages);
        right.setParams (fold, symmetry, newStages);
       #endif
    }

    // ILANA_NO_AVX (or a CPU without it) keeps the SSE2 path, in double
    // precision; the AVX path runs in float, within about -125 dB of it.
    static bool& useAvx()
    {
       #if ILANA_WEST_SSE
        static bool use = juce::SystemStats::hasAVX()
                          && juce::SystemStats::getEnvironmentVariable ("ILANA_NO_AVX", "").isEmpty();
       #else
        static bool use = false;
       #endif
        return use;
    }

    void process (float& l, float& r)
    {
       #if ILANA_WEST_SSE
        if (useAvx())
        {
            processAvx (l, r);
            return;
        }
        const auto& taps = doubleTaps();
        auto* up = reinterpret_cast<__m128d*> (upHistory);
        auto* down = reinterpret_cast<__m128d*> (downHistory);
        const auto input = _mm_set_pd ((double) r, (double) l);
        up[historyPosition] = input;
        up[historyPosition + historyLength] = input;
        const auto* newestUp = up + historyPosition + historyLength;

        // Interpolate.
        __m128d x[factor];
        for (auto& v : x)
            v = _mm_setzero_pd();
        for (int t = 0; t < tapsPerPhase; ++t)
        {
            const auto sample = newestUp[-t];
            for (int phase = 0; phase < factor; ++phase)
                x[phase] = _mm_add_pd (x[phase], _mm_mul_pd (_mm_set1_pd (taps[(size_t) (t * factor + phase)]), sample));
        }
        const auto factorVec = _mm_set1_pd ((double) factor), gainVec = _mm_set1_pd (gain), biasVec = _mm_set1_pd (bias);
        for (auto& v : x)
            v = _mm_add_pd (_mm_mul_pd (_mm_mul_pd (v, factorVec), gainVec), biasVec);

        // The fold.
        const auto stretch = _mm_set1_pd (1.0 + 0.35 * gain / (double) stages);
        const auto halfPi = _mm_set1_pd (juce::MathConstants<double>::halfPi);
        for (int stage = 0; stage < stages; ++stage)
        {
            __m128d cosine[factor];
            for (int phase = 0; phase < factor; ++phase)
                cosine[phase] = cosHalfPi (x[phase]);

            auto last = previous[stage], lastCosine = previousCosine[stage];
            for (int phase = 0; phase < factor; ++phase)
            {
                const auto difference = _mm_sub_pd (x[phase], last);
                auto y = _mm_div_pd (_mm_sub_pd (lastCosine, cosine[phase]), _mm_mul_pd (halfPi, difference));
                // Too small a step to divide by: the fold at the midpoint.
                const auto small = _mm_movemask_pd (_mm_cmplt_pd (absolute (difference), _mm_set1_pd (1.0e-5)));
                if (small != 0)
                {
                    alignas (16) double ys[2], xs[2], lasts[2];
                    _mm_store_pd (ys, y);
                    _mm_store_pd (xs, x[phase]);
                    _mm_store_pd (lasts, last);
                    for (int side = 0; side < 2; ++side)
                        if ((small >> side) & 1)
                            ys[side] = Wavefolder::fold (0.5 * (xs[side] + lasts[side]));
                    y = _mm_load_pd (ys);
                }
                last = x[phase];
                lastCosine = cosine[phase];
                x[phase] = stage + 1 < stages ? _mm_mul_pd (y, stretch) : y;
            }
            previous[stage] = last;
            previousCosine[stage] = lastCosine;
        }

        // Each 4x sample is kept at float precision, as a Wavefolder keeps it.
        const auto downBase = historyPosition * factor;
        const auto makeUpVec = _mm_set1_pd (makeUp);
        for (int phase = 0; phase < factor; ++phase)
        {
            const auto folded = _mm_cvtps_pd (_mm_cvtpd_ps (_mm_mul_pd (x[phase], makeUpVec)));
            down[downBase + phase] = folded;
            down[downBase + phase + downLength] = folded;
        }

        // Decimate.
        const auto* newestDown = down + downBase + factor - 1 + downLength;
        __m128d sums[4] { _mm_setzero_pd(), _mm_setzero_pd(), _mm_setzero_pd(), _mm_setzero_pd() };
        for (int t = 0; t < numTaps; t += 4)
            for (int k = 0; k < 4; ++k)
                sums[k] = _mm_add_pd (sums[k], _mm_mul_pd (_mm_set1_pd (taps[(size_t) (t + k)]), newestDown[-(t + k)]));
        alignas (16) double out[2];
        _mm_store_pd (out, _mm_add_pd (_mm_add_pd (sums[0], sums[1]), _mm_add_pd (sums[2], sums[3])));
        if (++historyPosition >= historyLength)
            historyPosition = 0;

        l = blockDc ((float) out[0], 0);
        r = blockDc ((float) out[1], 1);
       #else
        l = left.process (l);
        r = right.process (r);
       #endif
    }

private:
   #if ILANA_WEST_SSE
    static constexpr int maxStages = 4, factor = 4, tapsPerPhase = 8, numTaps = factor * tapsPerPhase;
    static constexpr int historyLength = tapsPerPhase, downLength = numTaps;

    // The float path's tap vectors in lane order, built once.
    struct FloatLaneTaps
    {
        alignas (32) float up[tapsPerPhase][8];
        alignas (32) float down[numTaps / 4][8];
    };

    // process() in single precision, all four phases of both sides in one
    // AVX register ([L0 R0 L1 R1 L2 R2 L3 R3]). The fold's antiderivative
    // step is taken in its stable form,
    //   (cos (pi/2 a) - cos (pi/2 b)) / (pi/2 (b - a))
    //     = sin (pi/2 (a + b) / 2) * sin (pi/4 (b - a)) / (pi/4 (b - a)),
    // which has no cancellation as the step shrinks (and needs no special
    // case at zero), so float carries it. The same filters and fold as the
    // double paths, within float rounding (about -140 dB).
    ILANA_AVX_TARGET void processAvx (float& l, float& r)
    {
        const auto& lanes = floatLaneTapsImpl();
        float* up = upFloat;
        up[2 * historyPosition] = l;
        up[2 * historyPosition + 1] = r;
        up[2 * (historyPosition + historyLength)] = l;
        up[2 * (historyPosition + historyLength) + 1] = r;
        const auto* newestUp = up + 2 * (historyPosition + historyLength);

        // Interpolate.
        auto x = _mm256_setzero_ps();
        for (int t = 0; t < tapsPerPhase; ++t)
        {
            const auto pair = _mm_castpd_ps (_mm_load1_pd (reinterpret_cast<const double*> (newestUp - 2 * t)));
            const auto sample = _mm256_insertf128_ps (_mm256_castps128_ps256 (pair), pair, 1);
            x = _mm256_add_ps (x, _mm256_mul_ps (_mm256_load_ps (lanes.up[t]), sample));
        }
        x = _mm256_add_ps (_mm256_mul_ps (x, _mm256_set1_ps ((float) ((double) factor * gain))), _mm256_set1_ps ((float) bias));

        // The fold. Each phase's "last" is the phase before it (phase 0's,
        // the previous sample's phase 3).
        const auto stretch = _mm256_set1_ps ((float) (1.0 + 0.35 * gain / (double) stages));
        const auto limit = _mm256_set1_ps (1.0e6f);
        for (int stage = 0; stage < stages; ++stage)
        {
            x = _mm256_max_ps (_mm256_min_ps (x, limit), _mm256_sub_ps (_mm256_setzero_ps(), limit));
            const auto xd = _mm256_castps_pd (x);
            // [previous, phase 0, phase 1, phase 2] as LR pairs.
            const auto low = _mm256_permute2f128_pd (xd, _mm256_castpd128_pd256 (_mm_castps_pd (_mm_loadl_pi (_mm_setzero_ps(), reinterpret_cast<const __m64*> (previousFloat[stage])))), 0x02);
            const auto last = _mm256_castpd_ps (_mm256_shuffle_pd (low, xd, 0x4));
            _mm_storeh_pi (reinterpret_cast<__m64*> (previousFloat[stage]), _mm256_extractf128_ps (x, 1));

            const auto mid = _mm256_mul_ps (_mm256_add_ps (x, last), _mm256_set1_ps (0.5f));
            const auto half = _mm256_mul_ps (_mm256_sub_ps (x, last), _mm256_set1_ps (0.5f));
            const auto u = _mm256_mul_ps (half, _mm256_set1_ps ((float) juce::MathConstants<double>::halfPi));
            const auto zeroStep = _mm256_cmp_ps (u, _mm256_setzero_ps(), _CMP_EQ_OQ);
            const auto ratio = _mm256_blendv_ps (_mm256_div_ps (sinHalfPiAvx (half), u), _mm256_set1_ps (1.0f), zeroStep);
            auto y = _mm256_mul_ps (sinHalfPiAvx (mid), ratio);
            if (stage + 1 < stages)
                y = _mm256_mul_ps (y, stretch);
            x = y;
        }

        // Each 4x sample at float precision, as the other paths keep it.
        x = _mm256_mul_ps (x, _mm256_set1_ps ((float) makeUp));
        float* down = downFloat;
        const auto downBase = 2 * historyPosition * factor;
        _mm256_storeu_ps (down + downBase, x);
        _mm256_storeu_ps (down + downBase + 2 * downLength, x);

        // Decimate: the newest 4x sample is phase 3 of this one; the taps run
        // backward over the history, four 4x samples a step.
        const auto* newestDown = down + downBase + 2 * (factor - 1) + 2 * downLength;
        auto sum = _mm256_setzero_ps();
        for (int t = 0; t < numTaps; t += 4)
            sum = _mm256_add_ps (sum, _mm256_mul_ps (_mm256_load_ps (lanes.down[t / 4]), _mm256_loadu_ps (newestDown - 2 * (t + 3))));
        auto pairs = _mm_add_ps (_mm256_castps256_ps128 (sum), _mm256_extractf128_ps (sum, 1));
        pairs = _mm_add_ps (pairs, _mm_movehl_ps (pairs, pairs));
        alignas (16) float out[4];
        _mm_store_ps (out, pairs);
        if (++historyPosition >= historyLength)
            historyPosition = 0;

        l = blockDc (out[0], 0);
        r = blockDc (out[1], 1);
    }

    // sin (pi/2 x) on eight lanes: x = q + f, q the nearest integer, then
    // +-sin or +-cos of pi/2 f (|pi/2 f| <= pi/4) by cephes' float kernels.
    static ILANA_AVX_TARGET __m256 sinHalfPiAvx (__m256 x)
    {
        const auto q = _mm256_cvtps_epi32 (x); // round to nearest
        const auto t = _mm256_mul_ps (_mm256_set1_ps ((float) juce::MathConstants<double>::halfPi), _mm256_sub_ps (x, _mm256_cvtepi32_ps (q)));
        const auto t2 = _mm256_mul_ps (t, t);
        auto s = _mm256_set1_ps (-1.9515295891e-4f);
        s = _mm256_add_ps (_mm256_set1_ps (8.3321608736e-3f), _mm256_mul_ps (t2, s));
        s = _mm256_add_ps (_mm256_set1_ps (-1.6666654611e-1f), _mm256_mul_ps (t2, s));
        s = _mm256_add_ps (t, _mm256_mul_ps (_mm256_mul_ps (t, t2), s));
        auto c = _mm256_set1_ps (2.443315711809948e-5f);
        c = _mm256_add_ps (_mm256_set1_ps (-1.388731625493765e-3f), _mm256_mul_ps (t2, c));
        c = _mm256_add_ps (_mm256_set1_ps (4.166664568298827e-2f), _mm256_mul_ps (t2, c));
        c = _mm256_add_ps (_mm256_sub_ps (_mm256_set1_ps (1.0f), _mm256_mul_ps (_mm256_set1_ps (0.5f), t2)), _mm256_mul_ps (_mm256_mul_ps (t2, t2), c));

        // sin (pi/2 q + t): q = 0 sin t, 1 cos t, 2 -sin t, 3 -cos t. AVX has
        // no 256-bit integer ops, so the quadrant bits go through 128-bit halves.
        const auto q0 = _mm256_castsi256_si128 (q), q1 = _mm256_extractf128_si256 (q, 1);
        const auto bit = [] (__m128i v, int b) { return _mm_slli_epi32 (_mm_and_si128 (v, _mm_set1_epi32 (b)), b == 1 ? 31 : 30); };
        const auto odd = _mm256_castsi256_ps (_mm256_insertf128_si256 (_mm256_castsi128_si256 (bit (q0, 1)), bit (q1, 1), 1));
        const auto sign = _mm256_castsi256_ps (_mm256_insertf128_si256 (_mm256_castsi128_si256 (bit (q0, 2)), bit (q1, 2), 1));
        return _mm256_xor_ps (_mm256_blendv_ps (s, c, odd), sign);
    }



    static const std::array<double, numTaps>& doubleTaps()
    {
        static const auto taps = []
        {
            std::array<double, numTaps> t {};
            for (int i = 0; i < numTaps; ++i)
                t[(size_t) i] = (double) Wavefolder::firTaps()[(size_t) i];
            return t;
        }();
        return taps;
    }

    static const FloatLaneTaps& floatLaneTapsImpl()
    {
        static const auto lanes = []
        {
            const auto& k = Wavefolder::firTaps();
            FloatLaneTaps l {};
            for (int t = 0; t < tapsPerPhase; ++t)
                for (int phase = 0; phase < factor; ++phase)
                    l.up[t][2 * phase] = l.up[t][2 * phase + 1] = k[(size_t) (t * factor + phase)];
            // down[b] lines up with the 4x samples t + 3, t + 2, t + 1, t
            // back from the newest (memory runs forward, the taps backward).
            for (int t = 0; t < numTaps; t += 4)
                for (int j = 0; j < 4; ++j)
                    l.down[t / 4][2 * j] = l.down[t / 4][2 * j + 1] = k[(size_t) (t + 3 - j)];
            return l;
        }();
        return lanes;
    }

    static __m128d absolute (__m128d v) { return _mm_andnot_pd (_mm_set1_pd (-0.0), v); }

    // Wavefolder::cosHalfPi on both sides.
    static __m128d cosHalfPi (__m128d x)
    {
        if (_mm_movemask_pd (_mm_cmplt_pd (absolute (x), _mm_set1_pd (1.0e8))) != 3)
        {
            alignas (16) double v[2];
            _mm_store_pd (v, x);
            return _mm_set_pd (Wavefolder::cosHalfPi (v[1]), Wavefolder::cosHalfPi (v[0]));
        }
        const auto q = _mm_sub_epi32 (_mm_cvttpd_epi32 (_mm_add_pd (x, _mm_set1_pd (0.5 + 1073741824.0))), _mm_set1_epi32 (1073741824));
        const auto t = _mm_mul_pd (_mm_set1_pd (juce::MathConstants<double>::halfPi), _mm_sub_pd (x, _mm_cvtepi32_pd (q)));
        const auto t2 = _mm_mul_pd (t, t);

        // The two series, in Wavefolder's order.
        using W = Wavefolder;
        const auto step = [] (__m128d acc, __m128d u, double k) { return _mm_add_pd (_mm_set1_pd (k), _mm_mul_pd (u, acc)); };
        auto c = _mm_set1_pd (W::c6);
        c = step (c, t2, W::c5);
        c = step (c, t2, W::c4);
        c = step (c, t2, W::c3);
        c = step (c, t2, W::c2);
        c = step (c, t2, W::c1);
        c = _mm_add_pd (_mm_sub_pd (_mm_set1_pd (1.0), _mm_mul_pd (_mm_set1_pd (0.5), t2)), _mm_mul_pd (_mm_mul_pd (t2, t2), c));
        auto s = _mm_set1_pd (W::s6);
        s = step (s, t2, W::s5);
        s = step (s, t2, W::s4);
        s = step (s, t2, W::s3);
        s = step (s, t2, W::s2);
        s = step (s, t2, W::s1);
        s = _mm_add_pd (t, _mm_mul_pd (_mm_mul_pd (t, t2), s));

        // Odd quadrants take the sine; quadrants 1 and 2 are negative.
        const auto q64 = _mm_shuffle_epi32 (q, _MM_SHUFFLE (1, 1, 0, 0));
        const auto odd = _mm_castsi128_pd (_mm_cmpeq_epi32 (_mm_and_si128 (q64, _mm_set1_epi32 (1)), _mm_set1_epi32 (1)));
        const auto chosen = _mm_or_pd (_mm_and_pd (odd, s), _mm_andnot_pd (odd, c));
        const auto negative = _mm_slli_epi64 (_mm_and_si128 (_mm_add_epi32 (q64, _mm_set1_epi32 (1)), _mm_set_epi32 (0, 2, 0, 2)), 62);
        return _mm_xor_pd (chosen, _mm_castsi128_pd (negative));
    }

    float blockDc (float y, int side)
    {
        const auto blocked = y - dcIn[side] + 0.9995f * dcOut[side];
        dcIn[side] = y;
        dcOut[side] = std::isfinite (blocked) ? blocked : 0.0f;
        return dcOut[side];
    }

    alignas (32) float upFloat[2 * historyLength * 2] {};
    alignas (32) float downFloat[2 * downLength * 2] {};
    alignas (16) float previousFloat[maxStages][2] {};
    alignas (16) double upHistory[2 * historyLength * 2] {};
    alignas (16) double downHistory[2 * downLength * 2] {};
    __m128d previous[maxStages] {}, previousCosine[maxStages] {};
    int historyPosition = 0;
    double gain = 1.0, bias = 0.0, makeUp = 1.0;
    int stages = 1;
    float dcIn[2] {}, dcOut[2] {};
   #else
    Wavefolder left, right;
   #endif
};

// The vactrol: control (0..1, the LED's drive) to conductance (0..1).
class Vactrol
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;
        lastSeconds = -1.0;
    }
    void reset() { conductance = 0.0; }

    // decayScale: DECAY (1 = the vactrol's own 250 ms).
    void setDecay (float scale) { decayScale = juce::jlimit (0.05, 8.0, (double) scale); }

    float process (float control)
    {
        const auto& t = WestCoastTuning::get();
        // A strike overdrives the LED (up to twice a steady full drive), so
        // the cell lights fully in well under its time constant; the
        // conductance itself tops out at 1.
        const auto target = (double) juce::jlimit (0.0f, 2.0f, control);
        // Lighting up is fast; going dark is slow, and slower the darker it
        // is (the photocell's memory).
        const auto seconds = target > conductance
                                 ? (double) t.riseSeconds
                                 : (double) t.fallSeconds * decayScale * square (1.0 + (double) t.fallSlowing * (1.0 - conductance))
                                       / square (1.0 + (double) t.fallSlowing * 0.5);
        // (The step's coefficient is kept while the time constant holds:
        // rising, or settled on a steady drive.)
        if (seconds != lastSeconds)
        {
            lastSeconds = seconds;
            lastStep = 1.0 - std::exp (-1.0 / (juce::jmax (1.0e-4, seconds) * sampleRate));
        }
        conductance += (target - conductance) * lastStep;
        conductance = juce::jlimit (0.0, 1.0, conductance);
        return (float) conductance;
    }

    float getConductance() const { return (float) conductance; }
    // std::pow (x, 2.0) is x * x correctly rounded, which is what x * x is.
    static double square (double x) { return x * x; }
    bool sameAs (const Vactrol& other) const
    {
        return conductance == other.conductance && decayScale == other.decayScale && sampleRate == other.sampleRate;
    }

private:
    double sampleRate = 48000.0, conductance = 0.0, decayScale = 1.0;
    double lastSeconds = -1.0, lastStep = 0.0;
};

class LowPassGate
{
public:
    enum class Mode { Combo = 0, LowPass, Vca };

    void prepare (double rate)
    {
        sampleRate = rate;
        vactrol.prepare (rate);
        reset();
    }

    void reset()
    {
        vactrol.reset();
        s1 = s2 = 0.0;
        lastConductance = -1.0;
    }

    void setParams (Mode newMode, float decay, float resonance)
    {
        mode = newMode;
        vactrol.setDecay (decay);
        damping = 2.0 * (1.0 - 0.9 * (double) juce::jlimit (0.0f, 1.0f, resonance)) * 0.7071;
    }

    // One sample: `control` drives the LED.
    float process (float input, float control)
    {
        const auto c = (double) vactrol.process (control);
        updateCoefficients (c);
        return filter (input);
    }

    // Both sides of a stereo voice: one vactrol lights both cells alike (the
    // same drive, the same history), so `right`'s cell and coefficients are
    // this one's; each side keeps its own filter. The same result as
    // process() on each, unless a side has had to reset on its own.
    void processPair (float& left, float& right, float control, LowPassGate& other)
    {
        if (! other.vactrol.sameAs (vactrol) || other.mode != mode || other.damping != damping)
        {
            left = process (left, control);
            right = other.process (right, control);
            return;
        }
        const auto c = (double) vactrol.process (control);
        other.vactrol = vactrol;
        updateCoefficients (c);
        other.lastConductance = lastConductance;
        other.g = g;
        other.gain = gain;
        left = filter (left);
        right = other.filter (right);
    }

    float getConductance() const { return vactrol.getConductance(); }

private:
    // The filter's and the amplifier's settings follow the cell; they are
    // kept while it holds still.
    void updateCoefficients (double c)
    {
        if (c == lastConductance)
            return;
        const auto& t = WestCoastTuning::get();
        lastConductance = c;
        // Cutoff: exponential in the conductance, from a closed gate's
        // near-silence to the open top.
        // pow (r, x) as exp2 (x log2 r) with log2 r kept, and the default
        // curve's pow (c, 0.5) as a square root: the same values to an ulp
        // or so, at a third of the cost (this runs every sample while the
        // cell moves).
        const auto ratio = (double) t.openHz / (double) t.closedHz;
        if (ratio != log2RatioOf)
        {
            log2RatioOf = ratio;
            log2Ratio = std::log2 (ratio);
        }
        const auto shaped = t.cutoffCurve == 0.5f ? std::sqrt (c) : std::pow (c, (double) t.cutoffCurve);
        const auto cutoff = (double) t.closedHz * std::exp2 (shaped * log2Ratio);
        g = std::tan (juce::MathConstants<double>::pi * juce::jmin (cutoff, sampleRate * 0.45) / sampleRate);
        gain = c > 0.0 ? std::exp2 ((double) t.gainCurve * std::log2 (c)) : std::pow (c, (double) t.gainCurve);
    }

    float filter (float input)
    {
        auto y = (double) input;

        if (mode != Mode::Vca)
        {
            // TPT state-variable low-pass.
            const auto hp = (y - (damping + g) * s1 - s2) / (1.0 + damping * g + g * g);
            const auto bp = g * hp + s1;
            s1 = g * hp + bp;
            const auto lp = g * bp + s2;
            s2 = g * bp + lp;
            y = lp;
        }
        if (mode != Mode::LowPass)
            y *= gain;

        if (! std::isfinite (y))
        {
            reset();
            return 0.0f;
        }
        return (float) y;
    }

    Vactrol vactrol;
    Mode mode = Mode::Combo;
    double sampleRate = 48000.0, s1 = 0.0, s2 = 0.0, damping = 1.414;
    double lastConductance = -1.0, g = 0.0, gain = 0.0;
    double log2RatioOf = 0.0, log2Ratio = 0.0;
};
