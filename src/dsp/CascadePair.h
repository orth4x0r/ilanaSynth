#pragma once

// Two FilterCore::StageCascade filters (a voice's left and right) run as the
// two lanes of SSE2 registers, for the saturating-stage models (DriveLadder,
// the OTA cascades). Each lane does exactly the arithmetic of
// StageCascade::process, in the same order, so a pair run gives bit for bit
// what two separate runs give (tests/FilterPairTests.inc checks it). Only
// used while both filters have identical settings.
// Precedent: LadderFilter::processPair (linear stages) in FilterUnit.h.

#include "FilterCore.h"

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_HAS_CASCADE_PAIR 1
 #include <emmintrin.h>

class CascadePair
{
public:
    using Cascade = FilterCore::StageCascade;

    static bool sameSettings (const Cascade& a, const Cascade& b)
    {
        const auto &x = a.settings, &y = b.settings;
        return x.g == y.g && x.k == y.k && x.positiveFeedback == y.positiveFeedback && x.highPassStages34 == y.highPassStages34
               && x.stageDrive == y.stageDrive && x.loopLevel == y.loopLevel;
    }

    // Lane 0 is l, lane 1 is r.
    CascadePair (Cascade& l, Cascade& r) : left (l), right (r)
    {
        const auto& s = l.settings;
        saturating = s.stageDrive > 0.0;
        hp34 = s.highPassStages34;
        sg = _mm_set1_pd (s.g);
        drive = _mm_set1_pd (s.stageDrive);
        linearBigG = _mm_set1_pd (l.linearBigG);
        linearBeta = _mm_set1_pd (l.linearBeta);
        signK = _mm_set1_pd ((s.positiveFeedback ? 1.0 : -1.0) * s.k);
        loopA = (s.positiveFeedback ? -1.0 : 1.0) * s.k; // -sign * k
        level = s.loopLevel;
        for (size_t i = 0; i < 4; ++i)
        {
            st[i] = _mm_set_pd (r.state[i], l.state[i]);
            last[i] = _mm_set_pd (r.last[i], l.last[i]);
        }
    }

    void store()
    {
        alignas (16) double lanes[2];
        for (size_t i = 0; i < 4; ++i)
        {
            _mm_store_pd (lanes, st[i]);
            left.state[i] = lanes[0];
            right.state[i] = lanes[1];
            _mm_store_pd (lanes, last[i]);
            left.last[i] = lanes[0];
            right.last[i] = lanes[1];
        }
    }

    // StageCascade::process on two lanes: the four stage outputs and the loop input.
    void step (__m128d x, __m128d* y, __m128d& u)
    {
        __m128d bigG[4], beta[4];
        const auto one = _mm_set1_pd (1.0);
        if (saturating)
        {
            for (size_t i = 0; i < 4; ++i)
            {
                const auto g = _mm_mul_pd (sg, tanhOverX (_mm_mul_pd (drive, last[i])));
                beta[i] = _mm_div_pd (one, _mm_add_pd (one, g));
                bigG[i] = _mm_mul_pd (g, beta[i]);
            }
        }
        else
        {
            for (size_t i = 0; i < 4; ++i)
            {
                beta[i] = linearBeta;
                bigG[i] = linearBigG;
            }
        }

        // The output as a + b u.
        auto a = _mm_setzero_pd();
        auto b = one;
        for (size_t i = 0; i < 4; ++i)
        {
            if (hp34 && i >= 2)
            {
                a = _mm_mul_pd (beta[i], _mm_sub_pd (a, st[i]));
                b = _mm_mul_pd (beta[i], b);
            }
            else
            {
                a = _mm_add_pd (_mm_mul_pd (bigG[i], a), _mm_mul_pd (beta[i], st[i]));
                b = _mm_mul_pd (bigG[i], b);
            }
        }

        // u = sat (x + sign k (a + b u)), FilterCore::solveLoop with a per lane.
        const auto v = _mm_add_pd (x, _mm_mul_pd (signK, a));
        const auto aV = _mm_mul_pd (_mm_set1_pd (loopA), b);
        u = solveLoop (v, aV);

        auto in = u;
        for (size_t i = 0; i < 4; ++i)
        {
            const auto vs = _mm_mul_pd (_mm_sub_pd (in, st[i]), bigG[i]);
            const auto low = _mm_add_pd (vs, st[i]);
            st[i] = _mm_add_pd (low, vs);
            const auto out = hp34 && i >= 2 ? _mm_sub_pd (in, low) : low;
            last[i] = _mm_sub_pd (in, low);
            y[i] = out;
            in = out;
        }
    }

    // Halfband2x on two lanes (its memory is kept by the caller as lanes).
    static __m128d allpass (__m128d x, double coefficient, __m128d& x1, __m128d& y1)
    {
        const auto y = _mm_add_pd (_mm_mul_pd (_mm_set1_pd (coefficient), _mm_sub_pd (x, y1)), x1);
        x1 = x;
        y1 = y;
        return y;
    }

    static __m128d select (__m128d mask, __m128d yes, __m128d no)
    {
        return _mm_or_pd (_mm_and_pd (mask, yes), _mm_andnot_pd (mask, no));
    }

private:
    // FilterCore::tanhOverX.
    static __m128d tanhOverX (__m128d x)
    {
        const auto signMask = _mm_set1_pd (-0.0);
        const auto ax = _mm_andnot_pd (signMask, x);
        const auto x2 = _mm_mul_pd (x, x);
        const auto num = _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (17325.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (378.0), x2)))));
        const auto den = _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (62370.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (3150.0), _mm_mul_pd (x2, _mm_set1_pd (28.0)))))));
        return select (_mm_cmpgt_pd (ax, _mm_set1_pd (4.97)), _mm_div_pd (_mm_set1_pd (1.0), ax), _mm_div_pd (num, den));
    }

    // FilterCore::tanhApprox.
    static __m128d tanhApprox (__m128d x)
    {
        const auto x2 = _mm_mul_pd (x, x);
        const auto num = _mm_mul_pd (x, _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (17325.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (378.0), x2))))));
        const auto den = _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (62370.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (3150.0), _mm_mul_pd (x2, _mm_set1_pd (28.0)))))));
        auto result = _mm_div_pd (num, den);
        result = select (_mm_cmpgt_pd (x, _mm_set1_pd (4.97)), _mm_set1_pd (1.0), result);
        return select (_mm_cmplt_pd (x, _mm_set1_pd (-4.97)), _mm_set1_pd (-1.0), result);
    }

    // FilterCore::solveLoop (v, a, level): a per lane, so each lane takes
    // its own one or two Newton steps (a <= 0.5: one).
    __m128d solveLoop (__m128d v, __m128d a) const
    {
        const auto levelV = _mm_set1_pd (level), minusLevel = _mm_set1_pd (-level), invLevel = _mm_set1_pd (1.0 / level);
        const auto limit = _mm_set1_pd (4.97), minusLimit = _mm_set1_pd (-4.97);
        auto u = _mm_mul_pd (levelV, tanhApprox (_mm_div_pd (_mm_mul_pd (v, invLevel), _mm_add_pd (_mm_set1_pd (1.0), a))));
        const auto twoSteps = _mm_cmpgt_pd (a, _mm_set1_pd (0.5));
        for (int step = 0; step < 2; ++step)
        {
            const auto xs = _mm_mul_pd (_mm_sub_pd (v, _mm_mul_pd (a, u)), invLevel);
            const auto x2 = _mm_mul_pd (xs, xs);
            const auto nn = _mm_mul_pd (xs, _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (17325.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (378.0), x2))))));
            const auto d = _mm_add_pd (_mm_set1_pd (135135.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (62370.0), _mm_mul_pd (x2, _mm_add_pd (_mm_set1_pd (3150.0), _mm_mul_pd (x2, _mm_set1_pd (28.0)))))));
            const auto dd = _mm_mul_pd (d, d);
            const auto numerator = _mm_mul_pd (_mm_sub_pd (_mm_mul_pd (u, d), _mm_mul_pd (levelV, nn)), d);
            const auto denominator = _mm_add_pd (dd, _mm_mul_pd (a, _mm_sub_pd (dd, _mm_mul_pd (nn, nn))));
            const auto newton = _mm_sub_pd (u, _mm_div_pd (numerator, denominator));
            const auto flat = select (_mm_cmpgt_pd (xs, _mm_setzero_pd()), levelV, minusLevel);
            const auto outside = _mm_or_pd (_mm_cmpgt_pd (xs, limit), _mm_cmplt_pd (xs, minusLimit));
            const auto next = select (outside, flat, newton);
            u = step == 0 ? next : select (twoSteps, next, u);
        }
        return u;
    }

    Cascade &left, &right;
    bool saturating = false, hp34 = false;
    __m128d sg, drive, linearBigG, linearBeta, signK;
    double loopA = 0.0, level = 1.0;
    __m128d st[4], last[4];
};
#endif
