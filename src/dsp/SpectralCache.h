#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "SpectralWarp.h"
#include "OscillatorIds.h"
#include "Wavetable.h"

// Builds spectrally warped copies of the wavetables (factory and patch
// tables) on a worker thread, one per oscillator. The audio thread asks for (table, mode,
// amount) and gets the newest matching copy, or the closest one it has while
// a rebuild is on its way. Retired copies are kept alive for a while so no
// voice ever reads a freed table.
class SpectralCache : private juce::Thread
{
public:
    static constexpr int numOscillators = OscillatorIds::count;
    // A still amount plays the nearest of 64 steps (as it always has, so
    // patches sound the same); one that moves is built on a 256-step grid
    // (every 4th is one of the 64), and the oscillator fades from each copy
    // to the next (UnisonBank::setWavetable), so a sweep glides.
    static constexpr int amountSteps = 64;
    static constexpr int fineSteps = 256;
    static constexpr int fineScale = fineSteps / amountSteps;
    static constexpr double settleSeconds = 0.25;

    // sourceFor(choice) returns the table behind a table choice (a factory
    // table, or a patch table that edits replace), kept alive by the pointer
    // it returns; numChoicesIn is how many choices there are. It is called on
    // the worker thread only.
    SpectralCache (std::function<std::shared_ptr<const Wavetable> (int)> sourceForIn, int numChoicesIn)
        : juce::Thread ("ilanaSynth spectral warps"),
          sourceFor (std::move (sourceForIn)),
          numChoices (numChoicesIn)
    {
        startThread (juce::Thread::Priority::low);
    }

    ~SpectralCache() override { stopThread (4000); }

    // Offline rendering builds warps in place so every render is exact.
    void setSynchronous (bool shouldBeSynchronous) { synchronous = shouldBeSynchronous; }

    // Audio thread. Returns the table the oscillator should play; base is the
    // table behind the choice right now (a replaced patch table rebuilds).
    // blend (FRAME BLEND Spectral) asks for the copy with spectrally blended
    // in-between frames (Wavetable::makeSpectralBlend), of the warped table
    // when a warp is on; until it is built the oscillator crossfades.
    // blockSeconds is how long the block is: an amount that has not moved
    // for settleSeconds goes back to the 64 steps.
    const Wavetable* get (int osc, int tableChoice, const Wavetable* base, int mode, float amount, bool blend = false,
                          double blockSeconds = 0.0)
    {
        auto& slot = slots[(size_t) osc];

        if (base == nullptr)
            return base;

        if (mode < SpectralWarp::Off || mode >= SpectralWarp::Count)
            mode = SpectralWarp::Off;

        if ((mode == SpectralWarp::Off && ! blend) || tableChoice < 0 || tableChoice >= numChoices)
        {
            slot.wantedMode = SpectralWarp::Off;
            slot.wantedBlend = false;
            slot.heardAmount = -1.0f;
            return base;
        }

        // Moving: an amount that changed within settleSeconds (the first one
        // heard, or one after a table or mode change, counts as still).
        if (tableChoice != slot.heardTable || mode != slot.heardMode)
        {
            slot.heardTable = tableChoice;
            slot.heardMode = mode;
            slot.heardAmount = -1.0f;
        }

        if (slot.heardAmount >= 0.0f && amount != slot.heardAmount)
            slot.stillSeconds = 0.0;
        else if (slot.heardAmount < 0.0f)
            slot.stillSeconds = settleSeconds;
        else
            slot.stillSeconds += blockSeconds;

        slot.heardAmount = amount;
        const auto moving = slot.stillSeconds < settleSeconds;
        const auto clamped = juce::jlimit (0.0f, 1.0f, amount);
        const auto step = mode == SpectralWarp::Off ? 0
                          : moving                  ? juce::roundToInt (clamped * (float) fineSteps)
                                                    : juce::roundToInt (clamped * (float) amountSteps) * fineScale;
        slot.wantedTable = tableChoice;
        slot.wantedMode = mode;
        slot.wantedStep = step;
        slot.wantedBlend = blend;
        slot.wantedSource = base;

        if (synchronous)
        {
            const juce::ScopedLock buildLock (slot.buildLock);
            build (slot, tableChoice, mode, step, blend);
        }

        const juce::SpinLock::ScopedTryLockType lock (slot.lock);

        if (lock.isLocked() && slot.built != nullptr && slot.builtTable == tableChoice && slot.builtMode == mode
            && slot.builtBlend == blend && slot.builtSource == base)
        {
            slot.lastServed = slot.built.get();
            slot.lastServedTable = tableChoice;
            return slot.lastServed;
        }

        // Mid-rebuild of a different table or mode: keep what was playing if
        // it's still this table, else the plain table. (A blended copy and the
        // plain one have different frame counts, but the oscillator maps its
        // frame position over whichever it gets, so either is safe to play.)
        return slot.lastServed != nullptr && slot.lastServedTable == tableChoice ? slot.lastServed : base;
    }

    // Audio threads (any voice thread). The table for one voice's own amount:
    // a pooled copy built at the voice's step (32 steps), or the nearest
    // built step of the same table and mode while the worker builds it.
    // Returns fallback when nothing matching exists yet. Never blocks.
    const Wavetable* getForVoice (int tableChoice, int mode, float amount, const Wavetable* base, const Wavetable* fallback)
    {
        if (base == nullptr || mode <= SpectralWarp::Off || mode >= SpectralWarp::Count || tableChoice < 0 || tableChoice >= numChoices)
            return fallback;

        const auto step = juce::jlimit (0, voiceSteps, juce::roundToInt (amount * (float) voiceSteps));
        const auto key = makeKey (tableChoice, mode, step);
        const auto now = juce::Time::getMillisecondCounter();

        if (synchronous)
        {
            const juce::ScopedLock buildLock (voiceBuildLock);

            for (auto& entry : voicePool)
                if (entry.key.load() == key && entry.source.load() == base)
                    return entry.table.load();

            return buildVoiceTable (key, tableChoice, mode, step, now, sourceFor (tableChoice));
        }

        const Wavetable* nearest = fallback;
        auto nearestDistance = 1000;

        for (auto& entry : voicePool)
        {
            const auto entryKey = entry.key.load();

            if (entryKey == 0)
                continue;

            const auto* ptr = entry.table.load();

            if (ptr == nullptr || entry.source.load() != base)
                continue;

            if (entryKey == key)
            {
                entry.lastUsed.store (now);
                return ptr;
            }

            if (keyTable (entryKey) == tableChoice && keyMode (entryKey) == mode)
            {
                const auto distance = std::abs (keyStep (entryKey) - step);

                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearest = ptr;
                }
            }
        }

        // Ask the worker for the step (once: skip if it is already queued).
        for (auto& request : voiceRequests)
        {
            auto expected = request.load();

            if (expected == key)
                break;

            if (expected == 0 && request.compare_exchange_strong (expected, key))
                break;
        }

        return nearest;
    }

    // Tests: whether the pool holds this voice step.
    bool hasVoiceTable (int tableChoice, int mode, float amount) const
    {
        const auto step = juce::jlimit (0, voiceSteps, juce::roundToInt (amount * (float) voiceSteps));
        const auto key = makeKey (tableChoice, mode, step);

        for (const auto& entry : voicePool)
            if (entry.key.load() == key && entry.table.load() != nullptr)
                return true;

        return false;
    }

    // Message thread: the warped copy for display, if one matches the
    // oscillator's current table and mode.
    std::shared_ptr<const Wavetable> getForDisplay (int osc, int tableChoice, int mode) const
    {
        const auto& slot = slots[(size_t) osc];
        const juce::SpinLock::ScopedLockType lock (slot.lock);

        if (mode > SpectralWarp::Off && slot.builtTable == tableChoice && slot.builtMode == mode)
            return slot.warped;

        return {};
    }

    // Tests: whether the requested warp for an oscillator has been built.
    bool isReady (int osc) const
    {
        const auto& slot = slots[(size_t) osc];
        return slot.builtTable == slot.wantedTable.load() && slot.builtMode == slot.wantedMode.load()
               && slot.builtStep == slot.wantedStep.load() && slot.builtBlend == slot.wantedBlend.load()
               && slot.builtSource.load() == slot.wantedSource.load();
    }

private:
    static constexpr int voiceSteps = 32;
    static constexpr int voicePoolSize = 16;
    static constexpr int numVoiceRequests = 32;

    static juce::uint32 makeKey (int table, int mode, int step) { return (juce::uint32) (1 + step + (mode << 8) + (table << 16)); }
    static int keyStep (juce::uint32 key) { return (int) ((key - 1) & 255u); }
    static int keyMode (juce::uint32 key) { return (int) (((key - 1) >> 8) & 255u); }
    static int keyTable (juce::uint32 key) { return (int) ((key - 1) >> 16); }

    struct VoiceEntry
    {
        std::atomic<juce::uint32> key { 0 };
        std::atomic<const Wavetable*> table { nullptr };
        std::atomic<const Wavetable*> source { nullptr }; // the table it was warped from
        std::atomic<juce::uint32> lastUsed { 0 };
        std::shared_ptr<Wavetable> owner; // worker (or the builder holding voiceBuildLock) only
    };

    // Builds one pooled voice table, evicting the least recently used entry.
    // Callers hold voiceBuildLock (the worker) or are the synchronous path.
    const Wavetable* buildVoiceTable (juce::uint32 key, int table, int mode, int step, juce::uint32 now,
                                      const std::shared_ptr<const Wavetable>& source)
    {
        juce::ignoreUnused (table);

        if (source == nullptr)
            return nullptr;

        std::shared_ptr<Wavetable> warped (SpectralWarp::warpTable (*source, mode, (float) step / (float) voiceSteps));

        auto* victim = &voicePool[0];

        for (auto& entry : voicePool)
        {
            if (entry.key.load() == 0)
            {
                victim = &entry;
                break;
            }

            if (now - entry.lastUsed.load() > now - victim->lastUsed.load())
                victim = &entry;
        }

        // Hide the entry, retire its table (voices may still be reading it),
        // then publish the new one.
        victim->key.store (0);
        victim->table.store (nullptr);

        if (victim->owner != nullptr)
        {
            const juce::ScopedLock retiredLock (retiredGuard);
            retired.push_back ({ victim->owner, now });
        }

        victim->owner = warped;
        victim->lastUsed.store (now);
        victim->source.store (source.get());
        victim->table.store (warped.get());
        victim->key.store (key);
        return warped.get();
    }

    // Returns whether it built a table.
    bool serviceVoiceRequests()
    {
        for (auto& request : voiceRequests)
        {
            if (threadShouldExit())
                return false;

            const auto key = request.load();

            if (key == 0)
                continue;

            const auto source = sourceFor (keyTable (key));
            auto built = false;

            for (const auto& entry : voicePool)
                built = built || (entry.key.load() == key && entry.source.load() == source.get());

            if (! built)
            {
                const juce::ScopedLock buildLock (voiceBuildLock);
                buildVoiceTable (key, keyTable (key), keyMode (key), keyStep (key), juce::Time::getMillisecondCounter(), source);
                request.store (0);
                return true; // one build a pass: the oscillators' own tables come first
            }

            request.store (0);
        }

        return false;
    }

    struct Slot
    {
        std::atomic<int> wantedTable { -1 }, wantedMode { 0 }, wantedStep { 0 };
        std::atomic<int> builtTable { -1 }, builtMode { 0 }, builtStep { -1 };
        std::atomic<bool> wantedBlend { false }, builtBlend { false };
        std::atomic<const Wavetable*> wantedSource { nullptr }, builtSource { nullptr };
        mutable juce::SpinLock lock;  // guards built
        juce::CriticalSection buildLock; // one builder at a time
        std::shared_ptr<Wavetable> built;  // what plays: the warped table, spectrally blended when asked
        std::shared_ptr<Wavetable> warped; // the warped table on its own frames, for display (null when only blended)
        const Wavetable* lastServed = nullptr; // audio thread only
        int lastServedTable = -1;              // audio thread only
        // Audio thread only: whether the amount is moving.
        int heardTable = -1, heardMode = -1;
        float heardAmount = -1.0f;
        double stillSeconds = 0.0;
    };

    // Returns whether it built a table.
    bool build (Slot& slot, int table, int mode, int step, bool blend)
    {
        const auto source = sourceFor (table);

        if (source == nullptr)
            return false;

        if (table == slot.builtTable && mode == slot.builtMode && step == slot.builtStep && blend == slot.builtBlend
            && source.get() == slot.builtSource.load())
            return false;

        std::shared_ptr<Wavetable> warped;

        if (mode > SpectralWarp::Off)
            warped = SpectralWarp::warpTable (*source, mode, (float) step / (float) fineSteps);

        std::shared_ptr<Wavetable> played = warped;

        if (blend)
            if (auto blended = Wavetable::makeSpectralBlend (warped != nullptr ? *warped : *source))
                played = std::move (blended);

        // Nothing to build: a one-frame table with no warp plays as it is.
        if (played == nullptr)
            played = std::shared_ptr<Wavetable> (source, const_cast<Wavetable*> (source.get()));

        const juce::SpinLock::ScopedLockType lock (slot.lock);

        {
            const juce::ScopedLock retiredLock (retiredGuard);
            const auto now = juce::Time::getMillisecondCounter();

            if (slot.built != nullptr)
                retired.push_back ({ slot.built, now });

            if (slot.warped != nullptr && slot.warped != slot.built)
                retired.push_back ({ slot.warped, now });
        }

        slot.built = played;
        slot.warped = warped;
        slot.builtTable = table;
        slot.builtMode = mode;
        slot.builtStep = step;
        slot.builtBlend = blend;
        slot.builtSource = source.get();
        return true;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            auto busy = false;

            for (int osc = 0; osc < numOscillators && ! threadShouldExit(); ++osc)
            {
                auto& slot = slots[(size_t) osc];
                const auto table = slot.wantedTable.load();
                const auto mode = slot.wantedMode.load();
                const auto step = slot.wantedStep.load();
                const auto blend = slot.wantedBlend.load();

                if ((mode <= SpectralWarp::Off && ! blend) || table < 0 || table >= numChoices)
                    continue;

                const juce::ScopedLock buildLock (slot.buildLock);
                busy = build (slot, table, mode, step, blend) || busy;
            }

            busy = serviceVoiceRequests() || busy;

            // Free copies nothing can be playing any more.
            {
                const auto now = juce::Time::getMillisecondCounter();
                const juce::ScopedLock retiredLock (retiredGuard);
                retired.erase (std::remove_if (retired.begin(), retired.end(),
                                               [now] (const auto& entry) { return now - entry.second > 3000; }),
                               retired.end());
            }

            // A sweep keeps it building: look again almost at once, so the
            // copies follow the amount closely; idle, every 15 ms.
            wait (busy ? 1 : 15);
        }
    }

    std::function<std::shared_ptr<const Wavetable> (int)> sourceFor;
    int numChoices = 0;
    bool synchronous = false;
    std::array<Slot, numOscillators> slots;
    std::array<VoiceEntry, voicePoolSize> voicePool;
    std::array<std::atomic<juce::uint32>, numVoiceRequests> voiceRequests {};
    juce::CriticalSection voiceBuildLock;
    juce::CriticalSection retiredGuard;
    std::vector<std::pair<std::shared_ptr<Wavetable>, juce::uint32>> retired;
};
