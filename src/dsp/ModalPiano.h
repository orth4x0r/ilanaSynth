#pragma once

// The Modal Piano exciter: a grand piano rebuilt from its partials.
//
// Where the Piano exciter (PianoString) models the hammer and string, this
// one plays back what a reference grand measured: for each recorded note and
// velocity, every partial's frequency and its level over time
// (ModalPianoTable, from tools/build_modal_piano.py). A note between two
// recorded ones blends their two nearest notes and velocity layers. Each
// partial is a sine (a complex rotator; four at a time with SSE) whose level
// follows the measured curve at a block rate. The first few tens of
// milliseconds are the recording's own attack (hammer thump and all),
// resampled to the note's pitch, crossfading into the partials.
//
// The knobs move away from the recording, which they reproduce at their
// middle (DECAY, STIFF, HAMMER) or at 0 (DAMP):
//   DECAY   stretches or shortens the whole decay (x0.35 .. x2.8)
//   DAMP    makes the upper partials die away faster
//   STIFF   0 is a harmonic string, the middle the measured stretch, 1 twice it
//   HAMMER  plays the timbre of a softer or harder strike at the same loudness
//   DAMPER  how fast the dampers stop the note on release (0 rings on)
// Eco keeps the first half of the partials.

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <cmath>

#if defined(_M_X64) || defined(__x86_64__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
 #define ILANA_MODAL_SSE 1
 #include <xmmintrin.h>
#else
 #define ILANA_MODAL_SSE 0
#endif

#include "ModalPianoTable.h"

class ModalPiano
{
public:
    static constexpr int maxPartials = 100; // a multiple of 4
    static constexpr int maxModes = 16;     // the board's resonances
    static constexpr int maxSlots = maxPartials + maxModes;
    static constexpr int blockSize = 32;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset()
    {
        count = slots = 0;
        attackLeft = 0;
        silent = true;
    }

    void setNote (double hz, int midiNote)
    {
        if (hz == frequency && midiNote == note)
            return;
        frequency = juce::jlimit (8.0, sampleRate * 0.45, hz);
        note = midiNote;
        pitchDirty = true;
    }

    void setParams (float newDecay, float newDamp, float newStiffness, float newHardness, float newDamper, bool newEco)
    {
        timeScale = std::exp2 ((juce::jlimit (0.0f, 1.0f, newDecay) - 0.5f) * 3.0f);
        const auto damp = juce::jlimit (0.0f, 1.0f, newDamp);
        dampRate = 40.0f * damp * damp;
        if (const auto stretch = 2.0f * juce::jlimit (0.0f, 1.0f, newStiffness); stretch != stiffness)
        {
            stiffness = stretch;
            pitchDirty = true;
        }
        hardness = juce::jlimit (0.0f, 1.0f, newHardness);
        damper = juce::jlimit (0.0f, 1.0f, newDamper);
        eco = newEco;
    }

    void trigger (float velocity, float randomUnit)
    {
        const auto& table = ModalPianoTable::get();
        count = slots = 0;
        attackLeft = 0;
        silent = true;
        if (! table.valid())
            return;

        const auto played = 127.0f * juce::jlimit (0.0f, 1.0f, velocity);
        // HAMMER: the timbre of a softer or harder strike (up to 50 velocity
        // steps either way), brought back to this velocity's loudness.
        const auto timbre = played + (hardness - 0.5f) * 100.0f;
        Corners at, loudness;
        findCorners (table, note, timbre, at);
        findCorners (table, note, played, loudness);
        auto gainDb = energyDb (table, loudness) - energyDb (table, at);
        // Below the softest recorded layer, quieter still.
        const auto softest = (float) table.velocities.front();
        if (played < softest)
            gainDb += 30.0f * std::log10 (juce::jmax (1.0f, played) / softest);

        // The partials: frequency ratios blended in log, levels in dB.
        auto partials = 0;
        for (const auto& c : at.entries)
            partials = juce::jmax (partials, c->partials);
        partials = juce::jmin (partials, eco ? maxPartials / 2 : maxPartials);
        const auto top = juce::jmin (16000.0, sampleRate * 0.45);
        auto seed = (std::uint32_t) (randomUnit * 4294967040.0f) | 1u;

        for (int k = 0; k < partials; ++k)
        {
            auto logRatio = 0.0f;
            for (int c = 0; c < 4; ++c)
                logRatio += at.weights[c] * std::log (ratioOf (*at.entries[c], k));
            const auto ratio = std::exp (logRatio);
            if (ratio * frequency > top)
                break;
            measured[k] = ratio;
            harmonic[k] = (float) (k + 1);
            // Random start phases: a real strike's partials don't line up.
            seed = seed * 1664525u + 22695477u;
            const auto phase = (float) (seed >> 8) * (juce::MathConstants<float>::twoPi / 16777216.0f);
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = step[k] = 0.0f;
            count = k + 1;
        }
        corners = at;
        levelOffset = gainDb;

        // The nearest recorded note gives the attack and the board's
        // resonances (fixed in Hz, they don't follow the key), at the two
        // velocity layers round the timbre.
        const auto nearest = at.noteWeight < 0.5f ? at.noteIndex : at.noteIndex + (at.noteWeight > 0.0f ? 1 : 0);
        modeA = &table.at (nearest, at.velocityIndex);
        modeB = &table.at (nearest, juce::jmin (at.velocityIndex + 1, (int) table.velocities.size() - 1));
        slots = count;
        const auto modes = juce::jmin ((int) modeA->modes.size(), (int) modeB->modes.size(), maxModes);
        for (int m = 0; m < modes; ++m)
        {
            const auto k = slots++;
            modeHz[m] = modeA->modes[(size_t) m];
            seed = seed * 1664525u + 22695477u;
            const auto phase = (float) (seed >> 8) * (juce::MathConstants<float>::twoPi / 16777216.0f);
            re[k] = std::cos (phase);
            im[k] = std::sin (phase);
            amp[k] = step[k] = 0.0f;
        }
        for (int k = slots; k < roundedCount(); ++k)
        {
            measured[k] = harmonic[k] = re[k] = im[k] = amp[k] = step[k] = 0.0f;
            rotCos[k] = 1.0f;
            rotSin[k] = 0.0f;
        }
        updatePitch();

        // The attack, resampled to this note's pitch.
        attackA = &modeA->attack;
        attackB = &modeB->attack;
        attackMix = at.velocityWeight;
        attackGain = std::pow (10.0f, gainDb / 20.0f);
        const auto anchorHz = 440.0 * std::exp2 ((table.notes[(size_t) nearest] - 69) / 12.0);
        attackStep = frequency / anchorHz * table.rate / sampleRate;
        const auto usable = juce::jmin ((double) juce::jmin (attackA->size(), attackB->size()) - 2.0, 0.08 * table.rate);
        attackLength = juce::jmax (1, (int) (usable / attackStep));
        fadeStart = juce::jmin ((int) (0.025 * sampleRate), attackLength / 3);
        attackLeft = attackLength;
        attackPosition = 0.0;
        attackFade = 1.0f;
        partialRamp = 0.0f;

        elapsed = 0;
        released = -1;
        segment = -1;
        untilBlock = 0;
        silent = slots == 0 && attackLeft == 0;
    }

    float process (bool noteHeld)
    {
        if (silent)
            return 0.0f;
        if (! noteHeld && released < 0)
            released = elapsed;
        if (untilBlock == 0)
            nextBlock();
        --untilBlock;
        ++elapsed;

        auto out = 0.0f;
        const auto n = roundedCount();
       #if ILANA_MODAL_SSE
        auto sum = _mm_setzero_ps();
        for (int k = 0; k < n; k += 4)
        {
            const auto r = _mm_load_ps (re + k), i = _mm_load_ps (im + k);
            const auto c = _mm_load_ps (rotCos + k), s = _mm_load_ps (rotSin + k);
            const auto a = _mm_load_ps (amp + k);
            _mm_store_ps (re + k, _mm_sub_ps (_mm_mul_ps (r, c), _mm_mul_ps (i, s)));
            const auto nextIm = _mm_add_ps (_mm_mul_ps (r, s), _mm_mul_ps (i, c));
            _mm_store_ps (im + k, nextIm);
            sum = _mm_add_ps (sum, _mm_mul_ps (a, nextIm));
            _mm_store_ps (amp + k, _mm_add_ps (a, _mm_load_ps (step + k)));
        }
        alignas (16) float lanes[4];
        _mm_store_ps (lanes, sum);
        out = (lanes[0] + lanes[1]) + (lanes[2] + lanes[3]);
       #else
        for (int k = 0; k < n; ++k)
        {
            const auto r = re[k];
            re[k] = r * rotCos[k] - im[k] * rotSin[k];
            im[k] = r * rotSin[k] + im[k] * rotCos[k];
            out += amp[k] * im[k];
            amp[k] += step[k];
        }
       #endif
        out *= partialRamp;
        partialRamp += partialRampStep;

        if (attackLeft > 0)
        {
            const auto i = (int) attackPosition;
            const auto f = (float) (attackPosition - i);
            const auto& a = *attackA;
            const auto& b = *attackB;
            const auto fromA = a[(size_t) i] + f * (a[(size_t) i + 1] - a[(size_t) i]);
            const auto fromB = b[(size_t) i] + f * (b[(size_t) i + 1] - b[(size_t) i]);
            out += (fromA + attackMix * (fromB - fromA)) * attackGain * attackFade;
            attackFade += attackFadeStep;
            attackPosition += attackStep;
            --attackLeft;
        }
        return out * outputGain;
    }

private:
    struct Corners
    {
        std::array<const ModalPianoTable::Entry*, 4> entries {}; // lo/hi note x lo/hi velocity
        std::array<float, 4> weights {};
        int noteIndex = 0, velocityIndex = 0;
        float noteWeight = 0.0f, velocityWeight = 0.0f;
    };

    static void bracket (const std::vector<int>& values, float x, int& index, float& weight)
    {
        index = 0;
        weight = 0.0f;
        const auto last = (int) values.size() - 1;
        if (last <= 0 || x <= (float) values.front())
            return;
        if (x >= (float) values.back())
        {
            index = last;
            return;
        }
        while (index < last - 1 && (float) values[(size_t) index + 1] <= x)
            ++index;
        weight = (x - (float) values[(size_t) index]) / (float) (values[(size_t) index + 1] - values[(size_t) index]);
    }

    static void findCorners (const ModalPianoTable& table, int midiNote, float velocity, Corners& c)
    {
        bracket (table.notes, (float) midiNote, c.noteIndex, c.noteWeight);
        bracket (table.velocities, velocity, c.velocityIndex, c.velocityWeight);
        const auto nextNote = juce::jmin (c.noteIndex + 1, (int) table.notes.size() - 1);
        const auto nextVelocity = juce::jmin (c.velocityIndex + 1, (int) table.velocities.size() - 1);
        c.entries = { &table.at (c.noteIndex, c.velocityIndex), &table.at (nextNote, c.velocityIndex),
                      &table.at (c.noteIndex, nextVelocity), &table.at (nextNote, nextVelocity) };
        c.weights = { (1.0f - c.noteWeight) * (1.0f - c.velocityWeight), c.noteWeight * (1.0f - c.velocityWeight),
                      (1.0f - c.noteWeight) * c.velocityWeight, c.noteWeight * c.velocityWeight };
    }

    // A partial past an entry's last one: the stretch carried on from it.
    static float ratioOf (const ModalPianoTable::Entry& e, int k)
    {
        if (k < e.partials)
            return juce::jmax (0.05f, e.ratios[(size_t) k]);
        if (e.partials < 2)
            return (float) (k + 1);
        const auto last = (float) e.partials;
        const auto b = juce::jmax (1.0e-7f, ((e.ratios.back() / last) * (e.ratios.back() / last) - 1.0f) / (last * last));
        const auto n = (float) (k + 1);
        return n * std::sqrt (1.0f + b * n * n);
    }

    static float levelOf (const ModalPianoTable::Entry& e, int k, int t, int times)
    {
        return k < e.partials ? e.levels[(size_t) (k * times + t)] : -160.0f;
    }

    // The strike's loudness: all partials' power at the first time.
    static float energyDb (const ModalPianoTable& table, const Corners& c)
    {
        const auto times = (int) table.times.size();
        auto power = 1.0e-20;
        for (int k = 0; k < maxPartials; ++k)
        {
            auto level = 0.0f;
            for (int i = 0; i < 4; ++i)
                level += c.weights[(size_t) i] * levelOf (*c.entries[(size_t) i], k, 0, times);
            power += std::pow (10.0, level / 10.0);
        }
        return (float) (10.0 * std::log10 (power));
    }

    int roundedCount() const { return (slots + 3) & ~3; }

    void updatePitch()
    {
        pitchDirty = false;
        for (int k = 0; k < count; ++k)
        {
            const auto ratio = harmonic[k] + (measured[k] - harmonic[k]) * stiffness;
            hz[k] = (float) (ratio * frequency);
            const auto w = juce::MathConstants<double>::twoPi * juce::jmin ((double) hz[k], sampleRate * 0.49) / sampleRate;
            rotCos[k] = (float) std::cos (w);
            rotSin[k] = (float) std::sin (w);
        }
        for (int k = count; k < slots; ++k)
        {
            hz[k] = modeHz[k - count];
            const auto w = juce::MathConstants<double>::twoPi * hz[k] / sampleRate;
            rotCos[k] = (float) std::cos (w);
            rotSin[k] = (float) std::sin (w);
        }
    }

    // Every block: each partial's level on the measured curve (log time
    // between the table's times), the knobs' extra losses, and a ramp to it.
    void nextBlock()
    {
        untilBlock = blockSize;
        if (pitchDirty)
            updatePitch();

        const auto& table = ModalPianoTable::get();
        const auto times = (int) table.times.size();
        const auto now = (double) (elapsed + blockSize) / sampleRate;
        const auto t = now / timeScale;

        auto s = 0;
        while (s < times - 2 && t >= table.times[(size_t) s + 1])
            ++s;
        if (s != segment)
        {
            segment = s;
            for (int k = 0; k < count; ++k)
            {
                auto a = 0.0f, b = 0.0f;
                for (int i = 0; i < 4; ++i)
                {
                    a += corners.weights[(size_t) i] * levelOf (*corners.entries[(size_t) i], k, s, times);
                    b += corners.weights[(size_t) i] * levelOf (*corners.entries[(size_t) i], k, s + 1, times);
                }
                levelA[k] = a + levelOffset;
                levelB[k] = b + levelOffset;
            }
            const auto w = corners.velocityWeight;
            for (int k = count; k < slots; ++k)
            {
                const auto m = (size_t) (k - count);
                const auto at = [times] (const ModalPianoTable::Entry& e, size_t mode, int t) { return e.modeLevels[mode * (size_t) times + (size_t) t]; };
                levelA[k] = at (*modeA, m, s) + w * (at (*modeB, m, s) - at (*modeA, m, s)) + levelOffset;
                levelB[k] = at (*modeA, m, s + 1) + w * (at (*modeB, m, s + 1) - at (*modeA, m, s + 1)) + levelOffset;
            }
        }

        const auto t0 = table.times[(size_t) s], t1 = table.times[(size_t) s + 1];
        const auto lastTime = table.times.back();
        // Before the first time: held. Past the last: the last segment's
        // fall per second carries on (at least 1 dB/s).
        const auto u = (float) juce::jlimit (0.0, 1.0, (std::log (t + 0.005) - std::log (t0 + 0.005))
                                                         / (std::log (t1 + 0.005) - std::log (t0 + 0.005)));
        const auto beyond = (float) juce::jmax (0.0, t - lastTime);
        const auto releasedFor = released < 0 ? 0.0f : (float) ((elapsed - released) / sampleRate);
        const auto releaseRate = damper * 250.0f * juce::jlimit (0.4f, 1.5f, std::sqrt ((float) frequency / 262.0f));
        const auto seconds = (float) now;

        auto loudest = -200.0f;
        for (int k = 0; k < slots; ++k)
        {
            auto db = levelA[k] + u * (levelB[k] - levelA[k]);
            if (beyond > 0.0f)
                db += juce::jmin (-1.0f, (levelB[k] - levelA[k]) / (t1 - t0)) * beyond;
            db -= (dampRate * juce::jmin (8.0f, hz[k] / 1000.0f)) * seconds
                  + releaseRate * releasedFor;
            loudest = juce::jmax (loudest, db);
            const auto target = db < -140.0f ? 0.0f : std::pow (10.0f, db / 20.0f);
            step[k] = (target - amp[k]) / (float) blockSize;
            // Keep the rotators on the unit circle.
            const auto norm = 1.5f - 0.5f * (re[k] * re[k] + im[k] * im[k]);
            re[k] *= norm;
            im[k] *= norm;
        }

        // The crossfade from the recording's attack into the partials, as
        // per-sample ramps to where it is at the block's end (a gain stepped
        // once a block buzzes: steps 1.5 kHz apart, heard as a hiss on top).
        const auto atEnd = (int) (attackLength - attackLeft) + blockSize;
        auto fadeTarget = 1.0f, rampTarget = 0.0f;
        if (atEnd > fadeStart)
        {
            const auto x = juce::jlimit (0.0f, 1.0f, (float) (atEnd - fadeStart) / (float) juce::jmax (1, attackLength - fadeStart));
            fadeTarget = std::cos (x * juce::MathConstants<float>::halfPi);
            rampTarget = std::sin (x * juce::MathConstants<float>::halfPi);
        }
        attackFadeStep = (fadeTarget - attackFade) / (float) blockSize;
        partialRampStep = (rampTarget - partialRamp) / (float) blockSize;

        if (loudest < -130.0f && attackLeft == 0)
            silent = true;
    }

    double sampleRate = 44100.0, frequency = 261.63;
    int note = 60;
    float timeScale = 1.0f, dampRate = 0.0f, stiffness = 1.0f, hardness = 0.5f, damper = 0.5f;
    bool eco = false, pitchDirty = true, silent = true;
    // The table's sinusoid levels are the reference's in dBFS; this brings
    // the Modal Piano to the Piano exciter's loudness.
    static constexpr float outputGain = 1.0f;

    Corners corners;
    float levelOffset = 0.0f;
    int count = 0, slots = 0, segment = -1, untilBlock = 0; // partials, then partials and modes
    long long elapsed = 0, released = -1;

    alignas (16) float re[maxSlots] {}, im[maxSlots] {}, rotCos[maxSlots] {}, rotSin[maxSlots] {};
    alignas (16) float amp[maxSlots] {}, step[maxSlots] {};
    float measured[maxSlots] {}, harmonic[maxSlots] {}, hz[maxSlots] {};
    float levelA[maxSlots] {}, levelB[maxSlots] {};

    float modeHz[maxModes] {};
    const ModalPianoTable::Entry* modeA = nullptr;
    const ModalPianoTable::Entry* modeB = nullptr;
    const std::vector<float>* attackA = nullptr;
    const std::vector<float>* attackB = nullptr;
    float attackMix = 0.0f, attackGain = 1.0f, attackFade = 1.0f, partialRamp = 0.0f;
    float attackFadeStep = 0.0f, partialRampStep = 0.0f;
    double attackPosition = 0.0, attackStep = 1.0;
    int attackLength = 1, attackLeft = 0, fadeStart = 0;
};
