#pragma once

#include "WavetableOscillator.h"

#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_UNISON_SSE 1
 #include <emmintrin.h>
 #include <xmmintrin.h>
#else
 #define ILANA_UNISON_SSE 0
#endif

// All the unison voices of one oscillator, rendered together.
//
// A WavetableOscillator per unison voice repeats the same work sixteen times
// a sample; here the voices sit side by side in arrays and SSE handles four
// at once (as Vital does). (An eight-wide AVX2 version measured no faster:
// the per-lane table reads dominate, not the arithmetic.) Phases are 32-bit fixed point: a full cycle is
// 2^32, so wrapping is free integer overflow, the top 11 bits index the
// 2048-sample frame and the rest are the interpolation fraction.
//
// The voice asks for one sum per sample (mono for FM, left and right for the
// filter buses); each unison voice's gain and pan are folded into per-lane
// weights once per block.
class UnisonBank
{
public:
    static constexpr int maxLanes = 16;
    static constexpr int laneWidth = 4;

    struct Sums
    {
        float mono = 0.0f;
        float left = 0.0f;
        float right = 0.0f;
    };

    UnisonBank()
    {
        for (int lane = 0; lane < maxLanes; ++lane)
            frequency[lane] = 440.0;

        refreshAllLanes();
    }

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        refreshAllLanes();
    }

    // Called every block: tables are swapped by pointer, never rebuilt in
    // place, but the cached level rows are refreshed here anyway.
    void setWavetable (const Wavetable* newTable)
    {
        table = newTable;
        refreshAllLanes();
    }

    void setFrequency (int lane, double newFrequency)
    {
        frequency[lane] = newFrequency;
        refreshLane (lane);
    }

    // The first count lanes at base x ratio[lane]: the voice's sub-block
    // update, so it is kept lean (one division, inline level search).
    void setFrequencies (double base, const double* ratios, int count)
    {
        if (sampleRate <= 0.0)
            return;

        const auto baseCycles = base / sampleRate;
        const auto hasTable = table != nullptr && table->getNumFrames() > 0;
        const auto limit = 0.5 / (double) Wavetable::numHarmonics / stretch; // top harmonic at Nyquist

        for (int lane = 0; lane < count; ++lane)
        {
            frequency[lane] = base * ratios[lane];
            auto cycles = baseCycles * ratios[lane];
            incrementCycles[lane] = cycles;

            if (cycles >= 1.0 || cycles < 0.0)
                cycles -= fastFloor (cycles);

            increment[lane] = toFixedStep (cycles);

            if (hasTable)
            {
                // Wavetable::getLevelForFrequency: halve the harmonics until
                // the top one is under Nyquist.
                auto level = 0;
                for (auto top = incrementCycles[lane]; level < Wavetable::numLevels - 1 && top > limit; top *= 0.5)
                    ++level;

                rows[lane] = table->getLevelFrames (level);
            }
            else
            {
                rows[lane] = nullptr;
            }
        }
    }

    // Warps raise the harmonics, so they move the table level too. Only the
    // stretch is updated here: set the lanes' frequencies after the warps
    // (the voice does, every sub-block) and they pick up the new level.
    void setWarp (int newMode, float newAmount)
    {
        if (updateWarp (warpMode, warpAmount, newMode, newAmount))
            updateStretch();
    }

    // The PD chain's second stage, applied after the first.
    void setWarp2 (int newMode, float newAmount)
    {
        if (updateWarp (warpMode2, warpAmount2, newMode, newAmount))
            updateStretch();
    }

    void resetPhase (int lane, double newPhase) { phase[lane] = toFixed (newPhase - fastFloor (newPhase)); }
    float getPhase (int lane) const { return (float) ((double) phase[lane] * phaseToDouble); }
    bool lane0Wrapped() const { return wrapped; }

    // Per-lane gains (unison blend) and pans for the first count lanes; the
    // rest are silenced. count also sets how many lanes run.
    void setWeights (const float* gains, const float* panL, const float* panR, int count)
    {
        count = juce::jlimit (0, maxLanes, count);
        numGroups = (count + laneWidth - 1) / laneWidth;
        numLanesInUse = count;

        for (int lane = 0; lane < maxLanes; ++lane)
        {
            const auto gain = lane < count ? gains[lane] : 0.0f;
            weightMono[lane] = gain;
            weightLeft[lane] = lane < count ? gain * panL[lane] : 0.0f;
            weightRight[lane] = lane < count ? gain * panR[lane] : 0.0f;
        }
    }

    // One output sample. phaseModulation is in cycles and shared by every
    // lane; rate bends the frequency (through-zero and exponential FM, 1 is
    // the plain pitch); oversample runs two half steps and averages them.
    Sums render (double phaseModulation, const WavetableOscillator::FrameRead& frames, double rate, bool oversample)
    {
        Sums out;

        if (table == nullptr || table->getNumFrames() == 0 || sampleRate <= 0.0 || numGroups == 0)
            return out;

        const auto modulation = toFixed (phaseModulation - fastFloor (phaseModulation));
        const auto warping = (warpMode != Warp::Off && warpAmount > 0.0f)
                             || (warpMode2 != Warp::Off && warpAmount2 > 0.0f);
        const auto lerpFrames = frames.frac > 0.0f && frames.frame1 != frames.frame0;
        // Sync alone (the common case) runs vectorised; the result is the
        // same as warpedRead's, operation for operation.
        const auto syncOnly = warping && warpMode == Warp::Sync && warpAmount > 0.0f
                              && ! (warpMode2 != Warp::Off && warpAmount2 > 0.0f);
        juce::ignoreUnused (syncOnly);
        const auto numLanes = numGroups * laneWidth;

        // Steps for this sample. The plain pitch uses the cached increments;
        // FM rates and oversampling scale them here.
        const auto stepScale = rate * (oversample ? 0.5 : 1.0);
        const auto plainStep = stepScale == 1.0;
        const std::uint32_t* steps = increment;

        const auto lane0Step = incrementCycles[0] * stepScale;
        const auto substeps = oversample ? 2 : 1;

        // One voice (every FM operator, most oscillators): lane 0 alone, the
        // same operations in the same order as the vector path, without the
        // three silent lanes (their phases are left where they are).
        if (numLanesInUse == 1 && ! warping)
        {
            const auto step = plainStep ? increment[0] : toFixedStep (lane0Step - fastFloor (lane0Step));
            const auto* row0 = rows[0][frames.frame0];
            const auto* row1 = lerpFrames ? rows[0][frames.frame1] : nullptr;
            constexpr auto scale = 1.0f / (float) (1 << fractionBits);

            for (int sub = 0; sub < substeps; ++sub)
            {
                const auto before = phase[0];
                const auto read = before + modulation;
                const auto index = (int) (read >> (32 - frameBits));
                const auto f = (float) (std::int32_t) (read & ((1u << fractionBits) - 1u)) * scale;
                auto y0 = row0[index], y1 = row0[index + 1], y2 = row0[index + 2], y3 = row0[index + 3];

                if (lerpFrames)
                {
                    const auto* next = row1 + index;
                    y0 = y0 + (next[0] - y0) * frames.frac;
                    y1 = y1 + (next[1] - y1) * frames.frac;
                    y2 = y2 + (next[2] - y2) * frames.frac;
                    y3 = y3 + (next[3] - y3) * frames.frac;
                }

                const auto c1 = 0.5f * (y2 - y0);
                const auto c2 = (y0 + (y2 + y2)) - 0.5f * (5.0f * y1 + y3);
                const auto c3 = 0.5f * (3.0f * (y1 - y2) + (y3 - y0));
                auto value = c2 + f * c3;
                value = c1 + f * value;
                value = y1 + f * value;

                out.mono += value * weightMono[0];
                out.left += value * weightLeft[0];
                out.right += value * weightRight[0];
                phase[0] = before + step;
                wrapped = lane0Step > 0.0 && (lane0Step >= 1.0 || phase[0] < before);
            }

            if (oversample)
            {
                out.mono *= 0.5f;
                out.left *= 0.5f;
                out.right *= 0.5f;
            }

            return out;
        }

        if (! plainStep)
        {
            for (int lane = 0; lane < numLanes; ++lane)
            {
                const auto step = incrementCycles[lane] * stepScale;
                scaledIncrement[lane] = toFixedStep (step - fastFloor (step));
            }

            steps = scaledIncrement;
        }

       #if ILANA_UNISON_SSE
        auto accMono = _mm_setzero_ps();
        auto accLeft = _mm_setzero_ps();
        auto accRight = _mm_setzero_ps();
        const auto modulationVec = _mm_set1_epi32 ((int) modulation);
        const auto fractionMask = _mm_set1_epi32 ((1 << fractionBits) - 1);
        const auto fractionScale = _mm_set1_ps (1.0f / (float) (1 << fractionBits));
        const auto frameFrac = _mm_set1_ps (frames.frac);
        const auto half = _mm_set1_ps (0.5f);
        const auto three = _mm_set1_ps (3.0f);
       #endif


        for (int sub = 0; sub < substeps; ++sub)
        {
            const auto lane0Before = phase[0];

            for (int group = 0; group < numGroups; ++group)
            {
                const auto first = group * laneWidth;
                alignas (16) std::int32_t index[laneWidth];
                alignas (16) float fraction[laneWidth];
                alignas (16) float gain[laneWidth] { 1.0f, 1.0f, 1.0f, 1.0f };

               #if ILANA_UNISON_SSE
                const auto phases = _mm_load_si128 (reinterpret_cast<const __m128i*> (phase + first));
                __m128 fractionVec;

                if (! warping)
                {
                    const auto read = _mm_add_epi32 (phases, modulationVec);
                    _mm_store_si128 (reinterpret_cast<__m128i*> (index), _mm_srli_epi32 (read, 32 - frameBits));
                    fractionVec = _mm_mul_ps (_mm_cvtepi32_ps (_mm_and_si128 (read, fractionMask)), fractionScale);
                }
                else if (syncOnly)
                {
                    syncRead (_mm_add_epi32 (phases, modulationVec), index, fraction);
                    fractionVec = _mm_load_ps (fraction);
                }
                else
                {
                    for (int lane = 0; lane < laneWidth; ++lane)
                        warpedRead (phase[first + lane] + modulation, index[lane], fraction[lane], gain[lane]);

                    fractionVec = _mm_load_ps (fraction);
                }

                __m128 y0, y1, y2, y3;
                loadTaps (first, frames.frame0, index, y0, y1, y2, y3);

                if (lerpFrames)
                {
                    __m128 z0, z1, z2, z3;
                    loadTaps (first, frames.frame1, index, z0, z1, z2, z3);
                    y0 = _mm_add_ps (y0, _mm_mul_ps (_mm_sub_ps (z0, y0), frameFrac));
                    y1 = _mm_add_ps (y1, _mm_mul_ps (_mm_sub_ps (z1, y1), frameFrac));
                    y2 = _mm_add_ps (y2, _mm_mul_ps (_mm_sub_ps (z2, y2), frameFrac));
                    y3 = _mm_add_ps (y3, _mm_mul_ps (_mm_sub_ps (z3, y3), frameFrac));
                }

                // The same Catmull-Rom cubic as WavetableOscillator, in
                // Horner form: y1 + f (c1 + f (c2 + f c3)).
                const auto c1 = _mm_mul_ps (half, _mm_sub_ps (y2, y0));
                const auto c2 = _mm_sub_ps (_mm_add_ps (y0, _mm_add_ps (y2, y2)),
                                            _mm_mul_ps (half, _mm_add_ps (_mm_mul_ps (_mm_set1_ps (5.0f), y1), y3)));
                const auto c3 = _mm_mul_ps (half, _mm_add_ps (_mm_mul_ps (three, _mm_sub_ps (y1, y2)), _mm_sub_ps (y3, y0)));
                auto value = _mm_add_ps (c2, _mm_mul_ps (fractionVec, c3));
                value = _mm_add_ps (c1, _mm_mul_ps (fractionVec, value));
                value = _mm_add_ps (y1, _mm_mul_ps (fractionVec, value));

                if (warping)
                    value = _mm_mul_ps (value, _mm_load_ps (gain));

                accMono = _mm_add_ps (accMono, _mm_mul_ps (value, _mm_load_ps (weightMono + first)));
                accLeft = _mm_add_ps (accLeft, _mm_mul_ps (value, _mm_load_ps (weightLeft + first)));
                accRight = _mm_add_ps (accRight, _mm_mul_ps (value, _mm_load_ps (weightRight + first)));

                const auto stepVec = _mm_loadu_si128 (reinterpret_cast<const __m128i*> (steps + first));
                _mm_store_si128 (reinterpret_cast<__m128i*> (phase + first), _mm_add_epi32 (phases, stepVec));
               #else
                for (int lane = 0; lane < laneWidth; ++lane)
                {
                    const auto l = first + lane;

                    if (! warping)
                    {
                        const auto read = phase[l] + modulation;
                        index[lane] = (std::int32_t) (read >> (32 - frameBits));
                        fraction[lane] = (float) (read & ((1u << fractionBits) - 1u)) / (float) (1 << fractionBits);
                    }
                    else
                    {
                        warpedRead (phase[l] + modulation, index[lane], fraction[lane], gain[lane]);
                    }

                    const auto* taps = rows[l][frames.frame0] + index[lane];
                    float y[4] { taps[0], taps[1], taps[2], taps[3] };

                    if (lerpFrames)
                    {
                        const auto* next = rows[l][frames.frame1] + index[lane];
                        for (int k = 0; k < 4; ++k)
                            y[k] += (next[k] - y[k]) * frames.frac;
                    }

                    const auto f = fraction[lane];
                    const auto c1 = 0.5f * (y[2] - y[0]);
                    const auto c2 = y[0] + 2.0f * y[2] - 0.5f * (5.0f * y[1] + y[3]);
                    const auto c3 = 0.5f * (3.0f * (y[1] - y[2]) + y[3] - y[0]);
                    const auto value = (y[1] + f * (c1 + f * (c2 + f * c3))) * gain[lane];

                    out.mono += value * weightMono[l];
                    out.left += value * weightLeft[l];
                    out.right += value * weightRight[l];
                    phase[l] += steps[l];
                }
               #endif
            }

            // Hard sync listens for lane 0 passing the end of its cycle
            // going forwards. Like the old per-voice oscillator, only the
            // last oversampled half step counts.
            wrapped = lane0Step > 0.0 && (lane0Step >= 1.0 || phase[0] < lane0Before);
        }

       #if ILANA_UNISON_SSE
        out.mono = horizontalSum (accMono);
        out.left = horizontalSum (accLeft);
        out.right = horizontalSum (accRight);
       #endif

        if (oversample)
        {
            out.mono *= 0.5f;
            out.left *= 0.5f;
            out.right *= 0.5f;
        }

        return out;
    }

private:
    static constexpr int frameBits = 11; // 2^11 = Wavetable::frameSize
    static constexpr int fractionBits = 32 - frameBits;
    static constexpr double phaseToDouble = 1.0 / 4294967296.0;
    static_assert ((1 << frameBits) == Wavetable::frameSize);

    static std::uint32_t toFixed (double cycles) noexcept
    {
        // cycles is in [0, 1); the int64 step keeps the cast well defined.
        return (std::uint32_t) (std::int64_t) (cycles * 4294967296.0);
    }

    // Steps round to nearest, so the pitch has no bias (truncating would
    // run every lane a fraction of a step flat).
    static std::uint32_t toFixedStep (double cycles) noexcept
    {
        return (std::uint32_t) (std::int64_t) (cycles * 4294967296.0 + 0.5);
    }

    static bool updateWarp (int& mode, float& amount, int newMode, float newAmount)
    {
        const auto m = Warp::isOscillatorWarp (newMode) ? newMode : Warp::Off;
        const auto a = juce::jlimit (0.0f, 1.0f, newAmount);

        if (m == mode && std::abs (a - amount) < 1.0e-4f)
            return false;

        mode = m;
        amount = a;
        return true;
    }

    // The warp chain on one lane, exactly as WavetableOscillator runs it.
    void warpedRead (std::uint32_t fixedPhase, std::int32_t& index, float& fraction, float& gain) const
    {
        auto modulatedPhase = (double) fixedPhase * phaseToDouble;
        auto silent = false;
        gain = 1.0f;

        if (warpMode != Warp::Off && warpAmount > 0.0f)
            modulatedPhase = WavetableOscillator::applyStage (warpMode, warpAmount, modulatedPhase, silent, gain);

        if (warpMode2 != Warp::Off && warpAmount2 > 0.0f && ! silent)
        {
            auto gain2 = 1.0f;
            modulatedPhase = WavetableOscillator::applyStage (warpMode2, warpAmount2, modulatedPhase, silent, gain2);
            gain *= gain2;
        }

        if (silent)
        {
            index = 0;
            fraction = 0.0f;
            gain = 0.0f;
            return;
        }

        const auto samplePosition = modulatedPhase * (double) Wavetable::frameSize;
        index = juce::jlimit (0, Wavetable::frameSize - 1, (int) samplePosition);
        fraction = (float) (samplePosition - (double) index);
    }

   #if ILANA_UNISON_SSE
    // Warp::apply's Sync on four lanes, two at a time in doubles, with the
    // same operations as warpedRead: phase = u / 2^32, p = phase (1 + 7a),
    // p - floor (p) (p >= 0, so floor is truncation), then the frame
    // position, its index and fraction.
    void syncRead (__m128i fixedPhases, std::int32_t* index, float* fraction) const noexcept
    {
        const auto ratio = _mm_set1_pd (1.0 + (double) juce::jlimit (0.0f, 1.0f, warpAmount) * 7.0);
        const auto toCycles = _mm_set1_pd (phaseToDouble);
        const auto frameSize = _mm_set1_pd ((double) Wavetable::frameSize);
        const auto offset = _mm_set1_pd (2147483648.0);
        // Unsigned to double: flip the sign bit, convert as signed, add 2^31.
        const auto flipped = _mm_xor_si128 (fixedPhases, _mm_set1_epi32 ((int) 0x80000000u));

        for (int half = 0; half < 2; ++half)
        {
            const auto signedPair = half == 0 ? flipped : _mm_shuffle_epi32 (flipped, _MM_SHUFFLE (1, 0, 3, 2));
            const auto unsignedPair = _mm_add_pd (_mm_cvtepi32_pd (signedPair), offset);
            const auto p = _mm_mul_pd (_mm_mul_pd (unsignedPair, toCycles), ratio);
            const auto whole = _mm_cvtepi32_pd (_mm_cvttpd_epi32 (p));
            const auto position = _mm_mul_pd (_mm_sub_pd (p, whole), frameSize);
            auto integer = _mm_cvttpd_epi32 (position);
            // jlimit (0, frameSize - 1): the position is under frameSize, so
            // only the top needs the clamp, as a guard.
            alignas (16) std::int32_t ints[4];
            _mm_store_si128 (reinterpret_cast<__m128i*> (ints), integer);
            ints[0] = juce::jmin (ints[0], Wavetable::frameSize - 1);
            ints[1] = juce::jmin (ints[1], Wavetable::frameSize - 1);
            integer = _mm_load_si128 (reinterpret_cast<const __m128i*> (ints));
            const auto frac = _mm_sub_pd (position, _mm_cvtepi32_pd (integer));
            alignas (16) double fractions[2];
            _mm_store_pd (fractions, frac);
            index[half * 2] = ints[0];
            index[half * 2 + 1] = ints[1];
            fraction[half * 2] = (float) fractions[0];
            fraction[half * 2 + 1] = (float) fractions[1];
        }
    }

    // Each lane's four cubic taps are adjacent (frames carry a guard sample
    // before and two after), so one unaligned load per lane fetches them;
    // the transpose turns four lanes' taps into four tap vectors.
    void loadTaps (int first, int frame, const std::int32_t* index,
                   __m128& y0, __m128& y1, __m128& y2, __m128& y3) const noexcept
    {
        y0 = _mm_loadu_ps (rows[first + 0][frame] + index[0]);
        y1 = _mm_loadu_ps (rows[first + 1][frame] + index[1]);
        y2 = _mm_loadu_ps (rows[first + 2][frame] + index[2]);
        y3 = _mm_loadu_ps (rows[first + 3][frame] + index[3]);
        _MM_TRANSPOSE4_PS (y0, y1, y2, y3);
    }

    static float horizontalSum (__m128 v) noexcept
    {
        const auto high = _mm_movehl_ps (v, v);
        const auto pair = _mm_add_ps (v, high);
        return _mm_cvtss_f32 (_mm_add_ss (pair, _mm_shuffle_ps (pair, pair, 1)));
    }
   #endif

    void updateStretch()
    {
        stretch = warpMode != Warp::Off ? Warp::harmonicStretch (warpMode, warpAmount) : 1.0;

        if (warpMode2 != Warp::Off)
            stretch *= Warp::harmonicStretch (warpMode2, warpAmount2);
    }

    void refreshAllLanes()
    {
        for (int lane = 0; lane < maxLanes; ++lane)
            refreshLane (lane);
    }

    // The increment and the band-limited table level follow the frequency.
    void refreshLane (int lane)
    {
        incrementCycles[lane] = sampleRate > 0.0 ? frequency[lane] / sampleRate : 0.0;
        increment[lane] = toFixedStep (incrementCycles[lane] - fastFloor (incrementCycles[lane]));

        if (table != nullptr && table->getNumFrames() > 0 && sampleRate > 0.0)
            rows[lane] = table->getLevelFrames (table->getLevelForFrequency (frequency[lane] * stretch, sampleRate));
        else
            rows[lane] = nullptr;
    }

    alignas (16) std::uint32_t phase[maxLanes] {};
    alignas (16) std::uint32_t increment[maxLanes] {};
    alignas (16) std::uint32_t scaledIncrement[maxLanes] {};
    alignas (16) float weightMono[maxLanes] {};
    alignas (16) float weightLeft[maxLanes] {};
    alignas (16) float weightRight[maxLanes] {};
    double incrementCycles[maxLanes] {};
    double frequency[maxLanes] {};
    const float* const* rows[maxLanes] {};

    const Wavetable* table = nullptr;
    double sampleRate = 44100.0;
    double stretch = 1.0;
    int numGroups = 0;
    int numLanesInUse = 0;
    int warpMode = Warp::Off;
    float warpAmount = 0.0f;
    int warpMode2 = Warp::Off;
    float warpAmount2 = 0.0f;
    bool wrapped = false;
};
