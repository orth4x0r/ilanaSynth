#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "SpectralWarp.h"
#include "Wavetable.h"

// Builds spectrally warped copies of the factory wavetables on a worker
// thread, one per oscillator. The audio thread asks for (table, mode,
// amount) and gets the newest matching copy, or the closest one it has while
// a rebuild is on its way. Retired copies are kept alive for a while so no
// voice ever reads a freed table.
class SpectralCache : private juce::Thread
{
public:
    static constexpr int numOscillators = 3;
    static constexpr int amountSteps = 64;

    // factoryTable(i) must return a table that lives for the whole session.
    SpectralCache (std::function<const Wavetable* (int)> factoryTableIn, int numFactoryTablesIn)
        : juce::Thread ("ilanaSynth spectral warps"),
          factoryTable (std::move (factoryTableIn)),
          numFactoryTables (numFactoryTablesIn)
    {
        startThread (juce::Thread::Priority::low);
    }

    ~SpectralCache() override { stopThread (4000); }

    // Offline rendering builds warps in place so every render is exact.
    void setSynchronous (bool shouldBeSynchronous) { synchronous = shouldBeSynchronous; }

    // Audio thread. Returns the table the oscillator should play.
    const Wavetable* get (int osc, int tableChoice, const Wavetable* base, int mode, float amount)
    {
        auto& slot = slots[(size_t) osc];

        if (mode <= SpectralWarp::Off || mode >= SpectralWarp::Count || tableChoice < 0 || tableChoice >= numFactoryTables)
        {
            slot.wantedMode = SpectralWarp::Off;
            return base;
        }

        const auto step = juce::jlimit (0, amountSteps, juce::roundToInt (amount * (float) amountSteps));
        slot.wantedTable = tableChoice;
        slot.wantedMode = mode;
        slot.wantedStep = step;

        if (synchronous)
        {
            const juce::ScopedLock buildLock (slot.buildLock);
            build (slot, tableChoice, mode, step);
        }

        const juce::SpinLock::ScopedTryLockType lock (slot.lock);

        if (lock.isLocked() && slot.built != nullptr && slot.builtTable == tableChoice && slot.builtMode == mode)
        {
            slot.lastServed = slot.built.get();
            slot.lastServedTable = tableChoice;
            return slot.lastServed;
        }

        // Mid-rebuild of a different table or mode: keep what was playing if
        // it's still this table, else the plain table.
        return slot.lastServed != nullptr && slot.lastServedTable == tableChoice ? slot.lastServed : base;
    }

    // Message thread: the warped copy for display, if one matches the
    // oscillator's current table and mode.
    std::shared_ptr<const Wavetable> getForDisplay (int osc, int tableChoice, int mode) const
    {
        const auto& slot = slots[(size_t) osc];
        const juce::SpinLock::ScopedLockType lock (slot.lock);

        if (mode > SpectralWarp::Off && slot.builtTable == tableChoice && slot.builtMode == mode)
            return slot.built;

        return {};
    }

    // Tests: whether the requested warp for an oscillator has been built.
    bool isReady (int osc) const
    {
        const auto& slot = slots[(size_t) osc];
        return slot.builtTable == slot.wantedTable.load() && slot.builtMode == slot.wantedMode.load()
               && slot.builtStep == slot.wantedStep.load();
    }

private:
    struct Slot
    {
        std::atomic<int> wantedTable { -1 }, wantedMode { 0 }, wantedStep { 0 };
        std::atomic<int> builtTable { -1 }, builtMode { 0 }, builtStep { -1 };
        mutable juce::SpinLock lock;  // guards built
        juce::CriticalSection buildLock; // one builder at a time
        std::shared_ptr<Wavetable> built;
        const Wavetable* lastServed = nullptr; // audio thread only
        int lastServedTable = -1;              // audio thread only
    };

    void build (Slot& slot, int table, int mode, int step)
    {
        if (table == slot.builtTable && mode == slot.builtMode && step == slot.builtStep)
            return;

        const auto* source = factoryTable (table);

        if (source == nullptr)
            return;

        std::shared_ptr<Wavetable> warped (SpectralWarp::warpTable (*source, mode, (float) step / (float) amountSteps));

        const juce::SpinLock::ScopedLockType lock (slot.lock);

        if (slot.built != nullptr)
        {
            const juce::ScopedLock retiredLock (retiredGuard);
            retired.push_back ({ slot.built, juce::Time::getMillisecondCounter() });
        }

        slot.built = warped;
        slot.builtTable = table;
        slot.builtMode = mode;
        slot.builtStep = step;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            for (int osc = 0; osc < numOscillators && ! threadShouldExit(); ++osc)
            {
                auto& slot = slots[(size_t) osc];
                const auto table = slot.wantedTable.load();
                const auto mode = slot.wantedMode.load();
                const auto step = slot.wantedStep.load();

                if (mode <= SpectralWarp::Off || table < 0 || table >= numFactoryTables)
                    continue;

                const juce::ScopedLock buildLock (slot.buildLock);
                build (slot, table, mode, step);
            }

            // Free copies nothing can be playing any more.
            {
                const auto now = juce::Time::getMillisecondCounter();
                const juce::ScopedLock retiredLock (retiredGuard);
                retired.erase (std::remove_if (retired.begin(), retired.end(),
                                               [now] (const auto& entry) { return now - entry.second > 3000; }),
                               retired.end());
            }

            wait (15);
        }
    }

    std::function<const Wavetable* (int)> factoryTable;
    int numFactoryTables = 0;
    bool synchronous = false;
    std::array<Slot, numOscillators> slots;
    juce::CriticalSection retiredGuard;
    std::vector<std::pair<std::shared_ptr<Wavetable>, juce::uint32>> retired;
};
