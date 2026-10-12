// The Airwindows FX module's engine: runs the one chosen algorithm (Chris
// Johnson's Airwindows, MIT: see LICENSE.txt) on a stereo block, with the
// module's five knobs mapped onto that algorithm's own parameters.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "Registry.h"

namespace airwindows
{
class Module
{
public:
    static constexpr int numKnobs = 5;

    Module() : instances ((size_t) count()) {}
    ~Module()
    {
        for (auto& instance : instances)
            delete instance.load();
    }
    Module (const Module&) = delete;
    Module& operator= (const Module&) = delete;

    // Instances made so far are prepared again (their state cleared); the
    // dry copy for the mix is sized for the block.
    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        lastMix = std::numeric_limits<float>::quiet_NaN();
        dry.assign ((size_t) std::max (1, maxBlockSize), {});

        for (auto& instance : instances)
            if (auto* made = instance.load())
                made->prepare (sampleRate);
    }

    // Clears the chosen algorithm's state (a new patch), without allocating.
    void reset()
    {
        if (current != nullptr)
            current->reset();
        lastMix = std::numeric_limits<float>::quiet_NaN();
    }

    // Makes the algorithm's instance ahead of use (the message thread), so
    // the audio thread only allocates for one it has never seen. Either
    // thread may call it: the first instance published wins.
    Algorithm* preload (int algorithm)
    {
        if (algorithm < 0 || algorithm >= count())
            return nullptr;

        auto& slot = instances[(size_t) algorithm];

        if (auto* made = slot.load())
            return made;

        auto fresh = registry()[(size_t) algorithm].create();
        fresh->prepare (sampleRate);
        Algorithm* expected = nullptr;

        if (slot.compare_exchange_strong (expected, fresh.get()))
            return fresh.release();

        return expected;
    }

    int getCurrentAlgorithm() const { return currentIndex; }

    // In place. knobs are the module's five 0..1 knobs, mix its dry/wet.
    void process (float* left, float* right, int numSamples, int algorithm,
                  const std::array<float, numKnobs>& knobs, float mix)
    {
        algorithm = std::clamp (algorithm, 0, count() - 1);

        if (algorithm != currentIndex)
        {
            // Another algorithm starts from its own fresh state.
            current = preload (algorithm);
            current->reset();
            currentIndex = algorithm;
        }

        const auto& info = registry()[(size_t) algorithm];

        for (int k = 0; k < info.numKnobs; ++k)
            current->setParam (info.knobs[k].parameter, info.knobs[k].toPlugin (knobs[(size_t) k]));

        mix = std::clamp (mix, 0.0f, 1.0f);

        // MIX eases across the block from the last one (no step at the block edge).
        const auto mixFrom = std::isfinite (lastMix) ? lastMix : mix;
        const auto mixStep = numSamples > 0 ? (mix - mixFrom) / (float) numSamples : 0.0f;
        lastMix = mix;

        if ((mix >= 0.9999f && mixFrom >= 0.9999f) || dry.empty())
        {
            current->process (left, right, numSamples);
            return;
        }

        for (int start = 0; start < numSamples;)
        {
            const auto n = std::min (numSamples - start, (int) dry.size());

            for (int i = 0; i < n; ++i)
                dry[(size_t) i] = { left[start + i], right[start + i] };

            current->process (left + start, right + start, n);

            for (int i = 0; i < n; ++i)
            {
                const auto mixNow = mixFrom + mixStep * (float) (start + i);
                left[start + i] = dry[(size_t) i][0] + (left[start + i] - dry[(size_t) i][0]) * mixNow;
                right[start + i] = dry[(size_t) i][1] + (right[start + i] - dry[(size_t) i][1]) * mixNow;
            }

            start += n;
        }
    }

private:
    std::vector<std::atomic<Algorithm*>> instances; // owned; made when first chosen
    Algorithm* current = nullptr;
    int currentIndex = -1;
    float lastMix = std::numeric_limits<float>::quiet_NaN(); // MIX of the last block (eased from)
    double sampleRate = 44100.0;
    std::vector<std::array<float, 2>> dry;
};
} // namespace airwindows
