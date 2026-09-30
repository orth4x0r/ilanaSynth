#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <memory>
#include <vector>
#include "dsp/Tuning.h"

// The patch's Scala tuning: the .scl and .kbm text (saved in the patch as a
// "Tuning" child, so it travels to other computers) and the table the audio
// thread reads. Loads happen on the message thread; the audio thread picks
// up a new table with a try-lock and never frees one (replaced tables are
// kept until this object goes), so it neither blocks nor deallocates.
class TuningState
{
public:
    // A new scale keeps the current keyboard mapping, and a new mapping the
    // current scale. On an error nothing changes.
    bool loadScale (const juce::String& sclText, juce::String& error)
    {
        return rebuild (sclText, getMappingText(), error);
    }

    bool loadMapping (const juce::String& kbmText, juce::String& error)
    {
        const auto scale = getScaleText();
        if (scale.isEmpty())
        {
            // Checked now, applied once a scale is loaded.
            Tuning::KeyboardMap map;
            if (! Tuning::parseKeyboardMap (kbmText, map, error))
                return false;
            const juce::SpinLock::ScopedLockType lock (mutex);
            pendingMapping = kbmText;
            return true;
        }
        return rebuild (scale, kbmText, error);
    }

    void reset()
    {
        publish (nullptr);
        const juce::SpinLock::ScopedLockType lock (mutex);
        pendingMapping.clear();
    }

    bool hasScale() const { return get() != nullptr; }
    std::shared_ptr<const Tuning> get() const
    {
        const juce::SpinLock::ScopedLockType lock (mutex);
        return current;
    }
    juce::String getDescription() const { const auto t = get(); return t != nullptr ? t->getDescription() : juce::String ("12-TET"); }
    juce::String getScaleText() const { const auto t = get(); return t != nullptr ? t->getScaleText() : juce::String(); }
    juce::String getMappingText() const
    {
        if (const auto t = get())
            return t->getMappingText();
        const juce::SpinLock::ScopedLockType lock (mutex);
        return pendingMapping;
    }

    // Audio thread: the loaded table, or nullptr for 12-TET.
    const Tuning* acquireForAudio() noexcept
    {
        const auto latest = version.load (std::memory_order_acquire);
        if (latest != audioVersion)
        {
            const juce::SpinLock::ScopedTryLockType lock (mutex);
            if (lock.isLocked())
            {
                audio = current;
                audioVersion = latest;
            }
        }
        return audio.get();
    }

    void saveTo (juce::ValueTree& state) const
    {
        state.removeChild (state.getChildWithName (treeType), nullptr);
        const auto scale = getScaleText();
        if (scale.isEmpty())
            return;
        juce::ValueTree tree (treeType);
        tree.setProperty ("scl", scale, nullptr);
        const auto mapping = getMappingText();
        if (mapping.isNotEmpty())
            tree.setProperty ("kbm", mapping, nullptr);
        state.appendChild (tree, nullptr);
    }

    // A state without a Tuning child is 12-TET. Removes the child, so the
    // rest of the state can go to the parameters.
    void loadFrom (juce::ValueTree& state)
    {
        const auto tree = state.getChildWithName (treeType);
        juce::String error;
        if (! tree.isValid() || ! rebuild (tree.getProperty ("scl").toString(), tree.getProperty ("kbm").toString(), error))
            reset();
        state.removeChild (tree, nullptr);
    }

    static constexpr const char* treeType = "Tuning";

private:
    mutable juce::SpinLock mutex;
    std::shared_ptr<const Tuning> current;
    std::vector<std::shared_ptr<const Tuning>> retired; // message thread only
    juce::String pendingMapping;
    std::atomic<std::uint32_t> version { 0 };
    std::uint32_t audioVersion = 0;
    std::shared_ptr<const Tuning> audio;

    bool rebuild (const juce::String& scl, const juce::String& kbm, juce::String& error)
    {
        auto tuning = std::make_shared<Tuning>();
        if (! Tuning::build (scl, kbm, *tuning, error))
            return false;
        publish (std::move (tuning));
        const juce::SpinLock::ScopedLockType lock (mutex);
        pendingMapping.clear();
        return true;
    }

    void publish (std::shared_ptr<const Tuning> next)
    {
        {
            const juce::SpinLock::ScopedLockType lock (mutex);
            if (current != nullptr)
                retired.push_back (current);
            current = std::move (next);
        }
        version.fetch_add (1, std::memory_order_release);
    }
};
