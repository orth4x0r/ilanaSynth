#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// The clip sequencer's data: up to eight clips, each a list of notes (start
// and length in beats, MIDI note, velocity) and a length in bars (4/4). Saved
// in the patch as a "Clips" child (absent = no clips). Edits happen on the
// message thread and publish a fresh immutable snapshot; the audio thread
// picks it up with a try-lock and never frees one (a replaced snapshot is
// dropped on the message thread once nobody holds it), like TuningState.
struct ClipNote
{
    float start = 0.0f;  // beats from the clip's start
    float length = 1.0f; // beats
    int note = 60;
    int velocity = 100;
};

struct Clip
{
    std::vector<ClipNote> notes;
    int bars = 2;
};

class ClipState
{
public:
    static constexpr int numClips = 8;
    static constexpr int maxBars = 16;
    static constexpr int maxNotes = 512;
    static constexpr int beatsPerBar = 4;
    static constexpr int rootNote = 60; // C3: the key that plays a clip untransposed

    using Clips = std::array<Clip, numClips>;

    ClipState() : current (std::make_shared<const Clips>()), audio (current) {}

    // Message thread: a copy of one clip to show.
    Clip getClip (int index) const
    {
        const auto snapshot = get();
        return (*snapshot)[(size_t) juce::jlimit (0, numClips - 1, index)];
    }

    // Message thread: replaces a clip (notes are clamped and sorted).
    void setClip (int index, Clip clip)
    {
        clip.bars = juce::jlimit (1, maxBars, clip.bars);
        const auto lengthBeats = (float) (clip.bars * beatsPerBar);

        for (auto& n : clip.notes)
        {
            n.note = juce::jlimit (0, 127, n.note);
            n.velocity = juce::jlimit (1, 127, n.velocity);
            n.start = juce::jlimit (0.0f, lengthBeats - 0.0625f, n.start);
            n.length = juce::jlimit (0.0625f, lengthBeats - n.start, n.length);
        }

        if ((int) clip.notes.size() > maxNotes)
            clip.notes.resize (maxNotes);

        std::stable_sort (clip.notes.begin(), clip.notes.end(),
                          [] (const ClipNote& a, const ClipNote& b) { return a.start < b.start; });

        auto next = std::make_shared<Clips> (*get());
        (*next)[(size_t) juce::jlimit (0, numClips - 1, index)] = std::move (clip);
        publish (std::move (next));
    }

    void reset() { publish (std::make_shared<const Clips>()); }

    // Message thread: every clip at once, for undo (the snapshot is shared
    // and never changed, so keeping it costs nothing).
    std::shared_ptr<const Clips> getAll() const { return get(); }
    void setAll (std::shared_ptr<const Clips> clips) { publish (clips != nullptr ? std::move (clips) : std::make_shared<const Clips>()); }

    static bool sameClip (const Clip& a, const Clip& b)
    {
        if (a.bars != b.bars || a.notes.size() != b.notes.size())
            return false;

        for (size_t i = 0; i < a.notes.size(); ++i)
        {
            const auto& x = a.notes[i];
            const auto& y = b.notes[i];

            if (x.start != y.start || x.length != y.length || x.note != y.note || x.velocity != y.velocity)
                return false;
        }

        return true;
    }

    bool hasAny() const
    {
        const auto snapshot = get();

        for (const auto& clip : *snapshot)
            if (! isDefault (clip))
                return true;

        return false;
    }

    // Audio thread: the latest snapshot (never blocks, never frees).
    const Clips& acquireForAudio() noexcept
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

        return *audio;
    }

    void saveTo (juce::ValueTree& state) const
    {
        state.removeChild (state.getChildWithName (treeType), nullptr);
        const auto snapshot = get();
        juce::ValueTree tree (treeType);

        for (int i = 0; i < numClips; ++i)
        {
            const auto& clip = (*snapshot)[(size_t) i];

            if (isDefault (clip))
                continue;

            juce::ValueTree c ("Clip");
            c.setProperty ("index", i, nullptr);
            c.setProperty ("bars", clip.bars, nullptr);

            for (const auto& n : clip.notes)
            {
                juce::ValueTree note ("N");
                note.setProperty ("s", n.start, nullptr);
                note.setProperty ("l", n.length, nullptr);
                note.setProperty ("n", n.note, nullptr);
                note.setProperty ("v", n.velocity, nullptr);
                c.appendChild (note, nullptr);
            }

            tree.appendChild (c, nullptr);
        }

        if (tree.getNumChildren() > 0)
            state.appendChild (tree, nullptr);
    }

    // A state without a Clips child has no clips. Removes the child, so the
    // rest of the state can go to the parameters.
    void loadFrom (juce::ValueTree& state)
    {
        const auto tree = state.getChildWithName (treeType);
        auto next = std::make_shared<Clips>();

        for (const auto c : tree)
        {
            const auto index = (int) c.getProperty ("index", -1);

            if (index < 0 || index >= numClips)
                continue;

            auto& clip = (*next)[(size_t) index];
            clip.bars = juce::jlimit (1, maxBars, (int) c.getProperty ("bars", 2));

            for (const auto n : c)
            {
                if ((int) clip.notes.size() >= maxNotes)
                    break;

                clip.notes.push_back ({ (float) (double) n.getProperty ("s", 0.0), (float) (double) n.getProperty ("l", 1.0),
                                        juce::jlimit (0, 127, (int) n.getProperty ("n", 60)),
                                        juce::jlimit (1, 127, (int) n.getProperty ("v", 100)) });
            }
        }

        publish (std::move (next));
        state.removeChild (tree, nullptr);
    }

    // Reads the first track of a Standard MIDI File that has notes into a
    // clip: start and length in beats (quarter notes), the length in whole
    // bars (at most maxBars; later notes are dropped). Not quantised.
    static bool importMidi (const juce::File& file, Clip& out, juce::String& error)
    {
        juce::FileInputStream stream (file);

        if (! stream.openedOk())
        {
            error = "Couldn't open " + file.getFileName();
            return false;
        }

        juce::MidiFile midi;

        if (! midi.readFrom (stream))
        {
            error = file.getFileName() + " is not a MIDI file";
            return false;
        }

        const auto ticks = midi.getTimeFormat();

        if (ticks <= 0)
        {
            error = "SMPTE-timed MIDI files aren't supported";
            return false;
        }

        for (int t = 0; t < midi.getNumTracks(); ++t)
        {
            const auto* track = midi.getTrack (t);
            std::array<std::vector<std::pair<double, int>>, 128> open; // per note: (start beat, velocity) of unfinished notes
            Clip clip;
            auto lastEnd = 0.0;

            for (int e = 0; e < track->getNumEvents(); ++e)
            {
                const auto& message = track->getEventPointer (e)->message;
                const auto beat = message.getTimeStamp() / (double) ticks;

                if (message.isNoteOn())
                {
                    open[(size_t) message.getNoteNumber()].push_back ({ beat, message.getVelocity() });
                }
                else if (message.isNoteOff())
                {
                    auto& pending = open[(size_t) message.getNoteNumber()];

                    if (pending.empty())
                        continue;

                    const auto [start, velocity] = pending.front();
                    pending.erase (pending.begin());
                    clip.notes.push_back ({ (float) start, (float) juce::jmax (0.0625, beat - start),
                                            message.getNoteNumber(), juce::jlimit (1, 127, velocity) });
                    lastEnd = juce::jmax (lastEnd, beat);
                }
            }

            if (clip.notes.empty())
                continue;

            clip.bars = juce::jlimit (1, maxBars, (int) std::ceil (lastEnd / beatsPerBar - 1.0e-6));
            const auto lengthBeats = (float) (clip.bars * beatsPerBar);
            clip.notes.erase (std::remove_if (clip.notes.begin(), clip.notes.end(),
                                              [lengthBeats] (const ClipNote& n) { return n.start >= lengthBeats; }),
                              clip.notes.end());
            out = std::move (clip);
            return true;
        }

        error = "No notes in " + file.getFileName();
        return false;
    }

    static constexpr const char* treeType = "Clips";

private:
    mutable juce::SpinLock mutex;
    std::shared_ptr<const Clips> current;
    std::vector<std::shared_ptr<const Clips>> retired; // message thread only
    std::atomic<std::uint32_t> version { 0 };
    std::uint32_t audioVersion = 0;
    std::shared_ptr<const Clips> audio;

    static bool isDefault (const Clip& clip) { return clip.notes.empty() && clip.bars == 2; }

    std::shared_ptr<const Clips> get() const
    {
        const juce::SpinLock::ScopedLockType lock (mutex);
        return current;
    }

    void publish (std::shared_ptr<const Clips> next)
    {
        {
            const juce::SpinLock::ScopedLockType lock (mutex);
            retired.push_back (current);
            current = std::move (next);

            // Snapshots nobody holds any more (the audio thread's copy counts).
            retired.erase (std::remove_if (retired.begin(), retired.end(),
                                           [] (const std::shared_ptr<const Clips>& s) { return s.use_count() == 1; }),
                           retired.end());
        }
        version.fetch_add (1, std::memory_order_release);
    }
};
