#pragma once

#include "BodyTuning.h"

#include <algorithm>
#include <array>
#include <cmath>

// A bank of damped, inharmonic modes. Input is the whole oscillator mix.
// Coefficients change at the voice's 16-sample control rate; process() has no
// allocation and is skipped entirely when Classic or Off is selected.
class MaterialBody
{
public:
    void prepare (double rate) { sampleRate = std::max (8000.0, rate); lastShape = -1; reset(); }
    void reset() { for (auto& mode : state) mode = {}; }

    void configure (int shapeIndex, float material, float size, float decay,
                    double noteHz, int quality, float stereoOffset = 0.0f)
    {
        if (shapeIndex == lastShape && material == lastMaterial && size == lastSize
            && decay == lastDecay && noteHz == lastNote && quality == lastQuality
            && stereoOffset == lastStereo)
            return;
        lastShape = shapeIndex;
        lastMaterial = material;
        lastSize = size;
        lastDecay = decay;
        lastNote = noteHz;
        lastQuality = quality;
        lastStereo = stereoOffset;
        const auto& shape = BodyTuning::shapes[(size_t) std::clamp (shapeIndex, 0, 3)];
        const auto colour = std::clamp (material, 0.0f, 1.0f);
        const auto extent = std::clamp (size, 0.0f, 1.0f);
        const auto life = std::clamp (decay, 0.0f, 1.0f);
        activeModes = quality <= 0 ? 4 : quality == 1 ? 6 : 8;
        const auto pitch = std::clamp (noteHz * std::exp2 ((0.5 - extent) * 2.0), 25.0, sampleRate * 0.4);
        const auto seconds = 0.08 + 7.92 * (double) life * (double) life;
        const auto metal = std::clamp ((colour - 0.15f) / 0.55f, 0.0f, 1.0f);
        const auto glass = std::clamp ((colour - 0.8f) / 0.2f, 0.0f, 1.0f);
        const auto lifetimeScale = 0.9 + 0.6 * (double) metal - 0.75 * (double) glass;

        // The body is tuned to the note, so the drive sits on its first mode,
        // where a resonator's gain grows as 1 / (1 - r): +60 dB and more at
        // long DECAY. One factor, the first mode's exact peak gain, normalises
        // every mode, so a sustained drive comes out at unity at any DECAY and
        // pitch while the modes keep their relative balance (and their fit).
        const auto rootRadius = std::exp (-6.907755278982137 / (sampleRate * std::max (0.015, seconds * lifetimeScale)));
        const auto rootPhase = 6.283185307179586 * std::min (pitch, sampleRate * 0.45) / sampleRate;
        const auto peakNorm = (1.0 - rootRadius)
                              * std::sqrt (1.0 - 2.0 * rootRadius * std::cos (2.0 * rootPhase) + rootRadius * rootRadius);

        for (int i = 0; i < activeModes; ++i)
        {
            auto& mode = state[(size_t) i];
            const auto ratio = (double) shape.ratio[(size_t) i]
                               * (1.0 + (double) glass * (i == 0 ? 0.0 : 0.012 * i));
            const auto frequency = pitch * ratio * (1.0 + (double) stereoOffset * 0.001);
            if (frequency >= sampleRate * 0.46)
            {
                mode.gain = mode.strike = 0.0f;
                continue;
            }
            const auto t60 = seconds * (double) shape.lifetime[(size_t) i] * lifetimeScale;
            const auto radius = std::exp (-6.907755278982137 / (sampleRate * std::max (0.015, t60)));
            const auto phase = 6.283185307179586 * frequency / sampleRate;
            mode.a1 = (float) (2.0 * radius * std::cos (phase));
            mode.a2 = (float) (-radius * radius);
            const auto brightness = 1.0f - 0.7f * (1.0f - colour) * (float) i / 7.0f;
            const auto weight = shape.gain[(size_t) i] * brightness;
            mode.gain = (float) (8.0 * peakNorm) * weight;          // +12 dB: plucks never reach full build-up
            // A strike rings mode i at 0.25 * weight * sin(root) / sin(mode): the
            // first mode at a fixed level, the higher ones falling off as before.
            mode.strike = 0.5f * weight * (float) std::sin (rootPhase);

        }
    }

    // A physical string's initial pluck also transfers a strike into the
    // body. Its ring is set by amplitude, not by DECAY; the continuously
    // rendered oscillator mix remains the driver.
    void strike (float strength)
    {
        const auto amount = std::clamp (strength, 0.0f, 1.0f);
        for (int i = 0; i < activeModes; ++i)
            state[(size_t) i].y1 += state[(size_t) i].strike * amount;
    }

    float process (float input)
    {
        const auto drive = std::clamp (input, -2.0f, 2.0f);
        auto sum = 0.0f;
        for (int i = 0; i < activeModes; ++i)
        {
            auto& mode = state[(size_t) i];
            const auto value = mode.a1 * mode.y1 + mode.a2 * mode.y2 + mode.gain * drive;
            mode.y2 = mode.y1;
            mode.y1 = std::clamp (value, -8.0f, 8.0f);
            sum += mode.y1;
        }
        return std::clamp (sum * 0.5f, -2.0f, 2.0f);
    }

private:
    struct Mode { float y1 = 0.0f, y2 = 0.0f, a1 = 0.0f, a2 = 0.0f, gain = 0.0f, strike = 0.0f; };
    std::array<Mode, BodyTuning::modes> state {};
    double sampleRate = 44100.0;
    int activeModes = 6;
    int lastShape = -1, lastQuality = -1;
    float lastMaterial = -1.0f, lastSize = -1.0f, lastDecay = -1.0f, lastStereo = 0.0f;
    double lastNote = -1.0;
};
