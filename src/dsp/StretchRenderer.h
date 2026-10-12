#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "SamplePlayer.h"
#include "stretch/OfflineStretch.h"

// Step 40 follow-up (ilana 2026-10-12): the high-quality stretchers
// (Signalsmith, Bungee, PGHI, Paulstretch) cost 1-2 % of a core per voice
// when run live, so instead they render a stretched copy of the sample on a
// worker thread. The original is kept; a copy is rendered per (sample,
// engine, length, semitone) and rendered again whenever the tempo, the
// region's length or the played note asks for a new one. Voices play the copy
// at its own speed (only the leftover cents by resampling), and play the
// live WSOLA stretch of the original while a copy is on its way.
class StretchRenderer : private juce::Thread
{
public:
    static constexpr int poolSize = 96;
    static constexpr size_t maxBytes = (size_t) 768 << 20;  // all copies together
    static constexpr juce::uint32 keepUnusedMs = 20000;    // a copy nobody played for this long may go
    static constexpr juce::uint32 retireMs = 5000;          // and is freed this much later

    // sourceFor(osc) returns the sample an oscillator holds, kept alive by
    // the pointer; called on the worker thread only.
    explicit StretchRenderer (std::function<std::shared_ptr<const SampleData> (int)> sourceForIn)
        : juce::Thread ("ilanaSynth sample stretch"), sourceFor (std::move (sourceForIn))
    {
        startThread (juce::Thread::Priority::low);
    }

    ~StretchRenderer() override { stopThread (10000); }

    // Offline rendering (bounce, export, tests) renders in place, so every
    // note plays its copy from the first sample.
    void setSynchronous (bool shouldBeSynchronous) { synchronous = shouldBeSynchronous; }

    // A copy's length factor in steps of a thousandth of an octave, so a
    // tempo that wobbles in its last digits doesn't render again.
    static int lengthStep (double lengthFactor) noexcept
    {
        return juce::roundToInt (std::log2 (juce::jlimit (1.0 / 16.0, 16.0, lengthFactor)) * 1000.0);
    }

    static double lengthForStep (int step) noexcept { return std::exp2 (step / 1000.0); }

    // Audio threads (any voice thread; never blocks). The copy of source
    // (the sample osc holds, or one of its zones) rendered by engine at
    // lengthStep and semitones, or nullptr while it's on its way (asked for
    // here unless ask is false).
    const SampleData* get (int osc, const SampleData* source, int engine, int step, int semitones, bool ask = true)
    {
        if (source == nullptr || source->getNumSamples() < 2 || engine < 0 || engine > 3)
            return nullptr;

        const auto key = makeKey (engine, step, semitones);
        const auto now = juce::Time::getMillisecondCounter();

        for (auto& entry : pool)
            if (entry.key.load (std::memory_order_acquire) == key && entry.source.load() == source)
                if (const auto* data = entry.data.load (std::memory_order_acquire))
                {
                    entry.lastUsed.store (now, std::memory_order_relaxed);
                    return data;
                }

        if (synchronous)
            return renderNow (osc, source, key);

        if (ask)
            request (osc, source, key);
        return nullptr;
    }

    // Message or audio thread: copies being rendered or waiting.
    int pendingCount() const noexcept { return pending.load(); }

private:
    struct Entry
    {
        std::atomic<juce::uint64> key { 0 };
        std::atomic<const SampleData*> source { nullptr };
        std::atomic<const SampleData*> data { nullptr };
        std::atomic<juce::uint32> lastUsed { 0 };
        // Worker thread only:
        std::shared_ptr<const SampleData> owner, sourceOwner;
        size_t bytes = 0;
    };

    struct Request
    {
        int osc = 0;
        const SampleData* source = nullptr;
        juce::uint64 key = 0;
    };

    // engine 0-3 | semitones + 128 | lengthStep + 32768, never 0.
    static juce::uint64 makeKey (int engine, int step, int semitones) noexcept
    {
        return 1ull << 40 | (juce::uint64) (engine & 15) << 32 | (juce::uint64) ((semitones + 128) & 255) << 16
               | (juce::uint64) ((step + 32768) & 65535);
    }

    static int engineOf (juce::uint64 key) noexcept { return (int) ((key >> 32) & 15); }
    static int semitonesOf (juce::uint64 key) noexcept { return (int) ((key >> 16) & 255) - 128; }
    static int stepOf (juce::uint64 key) noexcept { return (int) (key & 65535) - 32768; }

    void request (int osc, const SampleData* source, juce::uint64 key)
    {
        const juce::SpinLock::ScopedTryLockType lock (queueLock);
        if (! lock.isLocked())
            return; // another voice is asking: this one asks again next block

        for (int i = 0; i < queueCount; ++i)
            if (queue[(size_t) i].key == key && queue[(size_t) i].source == source)
                return;
        if (queueCount < (int) queue.size())
        {
            queue[(size_t) queueCount++] = { osc, source, key };
            pending.store (queueCount);
            notify();
        }
    }

    // Worker (or, synchronous, the caller): the shared pointer that keeps
    // source alive, if osc still holds it (as its sample or one of its zones).
    std::shared_ptr<const SampleData> resolve (int osc, const SampleData* source) const
    {
        auto held = sourceFor (osc);
        if (held == nullptr)
            return nullptr;
        if (held.get() == source)
            return held;
        for (const auto& zone : held->zones)
            if (zone.data.get() == source)
                return zone.data;
        return nullptr;
    }

    static std::shared_ptr<const SampleData> renderCopy (const SampleData& source, juce::uint64 key)
    {
        OfflineStretch::Audio input;
        input.sampleRate = source.sampleRate;
        for (int c = 0; c < source.getNumChannels(); ++c)
        {
            const auto* read = source.buffer.getReadPointer (c);
            input.channels.emplace_back (read, read + source.getNumSamples());
        }

        const auto output = OfflineStretch::render ((OfflineStretch::Engine) engineOf (key), input, lengthForStep (stepOf (key)),
                                                    std::exp2 (semitonesOf (key) / 12.0));
        auto copy = std::make_shared<SampleData>();
        copy->sampleRate = source.sampleRate;
        copy->name = source.name;
        const auto frames = output.numFrames();
        copy->buffer.setSize (juce::jmax (1, (int) output.channels.size()), juce::jmax (2, frames));
        copy->buffer.clear();
        for (size_t c = 0; c < output.channels.size(); ++c)
            copy->buffer.copyFrom ((int) c, 0, output.channels[c].data(), frames);
        return copy;
    }

    const SampleData* renderNow (int osc, const SampleData* source, juce::uint64 key)
    {
        const juce::ScopedLock lock (workerLock);
        for (auto& entry : pool)
            if (entry.key.load() == key && entry.source.load() == source && entry.data.load() != nullptr)
                return entry.data.load();
        auto owner = resolve (osc, source);
        if (owner == nullptr)
            return nullptr;
        return store (source, key, renderCopy (*owner, key), owner);
    }

    // Worker side: puts a finished copy in a free entry, or in place of the
    // longest-unused one (whose copy is freed a while later).
    const SampleData* store (const SampleData* source, juce::uint64 key, std::shared_ptr<const SampleData> copy,
                             std::shared_ptr<const SampleData> sourceOwner)
    {
        const auto now = juce::Time::getMillisecondCounter();
        const auto bytes = (size_t) copy->getNumSamples() * (size_t) copy->getNumChannels() * sizeof (float);

        // Over the memory budget: let go of copies nobody has played lately.
        for (auto total = totalBytes(); total + bytes > maxBytes;)
        {
            auto* oldest = oldestUnused (now);
            if (oldest == nullptr)
                break;
            total -= oldest->bytes;
            retire (*oldest, now);
        }

        Entry* target = nullptr;
        for (auto& entry : pool)
            if (entry.key.load() == 0)
            {
                target = &entry;
                break;
            }
        if (target == nullptr)
            target = oldestUnused (now);
        if (target == nullptr)
            return nullptr; // every copy is in use: this one waits for a free entry
        retire (*target, now);

        target->owner = std::move (copy);
        target->sourceOwner = std::move (sourceOwner);
        target->bytes = bytes;
        target->lastUsed.store (now);
        target->data.store (target->owner.get(), std::memory_order_release);
        target->source.store (source, std::memory_order_release);
        target->key.store (key, std::memory_order_release);
        return target->owner.get();
    }

    Entry* oldestUnused (juce::uint32 now)
    {
        Entry* oldest = nullptr;
        for (auto& entry : pool)
            if (entry.key.load() != 0 && now - entry.lastUsed.load() > keepUnusedMs
                && (oldest == nullptr || entry.lastUsed.load() < oldest->lastUsed.load()))
                oldest = &entry;
        return oldest;
    }

    size_t totalBytes() const
    {
        size_t total = 0;
        for (const auto& entry : pool)
            if (entry.key.load() != 0)
                total += entry.bytes;
        return total;
    }

    void retire (Entry& entry, juce::uint32 now)
    {
        entry.key.store (0, std::memory_order_release);
        entry.data.store (nullptr, std::memory_order_release);
        entry.source.store (nullptr);
        if (entry.owner != nullptr)
            retired.push_back ({ std::move (entry.owner), now });
        entry.owner.reset();
        entry.sourceOwner.reset();
        entry.bytes = 0;
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            Request next;
            auto found = false;
            {
                const juce::SpinLock::ScopedLockType lock (queueLock);
                if (queueCount > 0)
                {
                    next = queue[0];
                    for (int i = 1; i < queueCount; ++i)
                        queue[(size_t) (i - 1)] = queue[(size_t) i];
                    --queueCount;
                    found = true;
                }
            }

            if (found)
            {
                const juce::ScopedLock lock (workerLock);
                auto done = false;
                for (auto& entry : pool)
                    done = done || (entry.key.load() == next.key && entry.source.load() == next.source);
                if (! done)
                    if (auto owner = resolve (next.osc, next.source))
                        store (next.source, next.key, renderCopy (*owner, next.key), owner);
                const juce::SpinLock::ScopedLockType queueGuard (queueLock);
                pending.store (queueCount);
                continue;
            }

            {
                // A copy is freed only well after it left the pool, so a voice
                // that read its pointer just before has long moved on.
                const juce::ScopedLock lock (workerLock);
                const auto now = juce::Time::getMillisecondCounter();
                retired.erase (std::remove_if (retired.begin(), retired.end(),
                                               [now] (const auto& item) { return now - item.second > retireMs; }),
                               retired.end());
            }
            wait (50);
        }
    }

    std::function<std::shared_ptr<const SampleData> (int)> sourceFor;
    std::array<Entry, poolSize> pool;
    juce::SpinLock queueLock;
    std::array<Request, 64> queue {};
    int queueCount = 0;
    std::atomic<int> pending { 0 };
    juce::CriticalSection workerLock;
    std::vector<std::pair<std::shared_ptr<const SampleData>, juce::uint32>> retired;
    std::atomic<bool> synchronous { false };
};
