#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// Keeps data the audio thread may still be reading alive until the audio
// thread has moved on (Fable B3/A1, 2026-11-24). The audio thread only bumps
// a block counter at the start of each block; a retired object is freed once
// the counter is two blocks past the one it was retired in (the block that
// could hold its raw pointer has finished), or after two seconds when no
// audio runs. Message and loader threads only; never call retire() or
// collect() from the audio thread (they lock and free).
class RetireQueue
{
public:
    // Audio thread, once per block before it reads any shared data.
    void audioBlockStarted() noexcept { blocks.fetch_add (1, std::memory_order_acq_rel); }

    template <typename T>
    void retire (std::shared_ptr<T> object)
    {
        if (object == nullptr)
            return;
        const std::lock_guard<std::mutex> lock (mutex);
        entries.push_back ({ std::static_pointer_cast<void> (std::move (object)), blocks.load (std::memory_order_acquire),
                             juce::Time::getMillisecondCounterHiRes() });
        collectLocked();
    }

    void collect()
    {
        const std::lock_guard<std::mutex> lock (mutex);
        collectLocked();
    }

    size_t pending() const
    {
        const std::lock_guard<std::mutex> lock (mutex);
        return entries.size();
    }

private:
    struct Entry
    {
        std::shared_ptr<void> object;
        std::uint64_t block;
        double millis;
    };

    void collectLocked()
    {
        const auto now = blocks.load (std::memory_order_acquire);
        const auto time = juce::Time::getMillisecondCounterHiRes();
        std::vector<std::shared_ptr<void>> freed; // (freed after the loop, outside the vector's churn)
        for (auto it = entries.begin(); it != entries.end();)
        {
            if (now >= it->block + 2 || time - it->millis > 2000.0)
            {
                freed.push_back (std::move (it->object));
                it = entries.erase (it);
            }
            else
                ++it;
        }
    }

    mutable std::mutex mutex;
    std::vector<Entry> entries;
    std::atomic<std::uint64_t> blocks { 0 };
};
