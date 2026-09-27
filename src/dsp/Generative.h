#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <vector>

// Scales for snapping generated (and optionally played) notes.
namespace Scales
{
inline juce::StringArray getNames()
{
    return { "Off", "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
             "Harmonic Minor", "Melodic Minor", "Major Pentatonic", "Minor Pentatonic", "Blues",
             "Whole Tone", "Hirajoshi", "Phrygian Dominant" };
}

inline juce::StringArray getRootNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

// Bit n set = the scale contains the note n semitones above the root.
inline int mask (int scale)
{
    static constexpr int masks[] {
        0xFFF,                                                   // Off (chromatic)
        0b101010110101, 0b010110101101, 0b011010101101,          // major, minor, dorian
        0b010110101011, 0b101011010101, 0b011010110101,          // phrygian, lydian, mixolydian
        0b010101101011, 0b100110101101, 0b101010101101,          // locrian, harmonic minor, melodic minor
        0b001010010101, 0b010010101001, 0b010011101001,          // major pent., minor pent., blues
        0b010101010101, 0b000110001101, 0b010110110011           // whole tone, hirajoshi, phrygian dominant
    };

    return masks[juce::jlimit (0, (int) std::size (masks) - 1, scale)];
}

inline bool contains (int note, int scale, int root)
{
    return (mask (scale) >> (((note - root) % 12 + 12) % 12) & 1) != 0;
}

// Nearest note in the scale (ties go up).
inline int quantize (int note, int scale, int root)
{
    if (scale <= 0)
        return juce::jlimit (0, 127, note);

    for (int distance = 0; distance < 12; ++distance)
    {
        if (note + distance <= 127 && contains (note + distance, scale, root))
            return note + distance;

        if (note - distance >= 0 && contains (note - distance, scale, root))
            return note - distance;
    }

    return juce::jlimit (0, 127, note);
}
} // namespace Scales

// Euclidean rhythm: HITS spread as evenly as possible over STEPS (Bresenham),
// the first hit on step 0 before ROTATE shifts the pattern later.
inline bool euclidHit (int step, int hits, int steps, int rotate)
{
    if (steps <= 0 || hits <= 0)
        return false;

    if (hits >= steps)
        return true;

    const auto index = (((step - rotate) % steps) + steps) % steps;
    return (index * hits) % steps < hits;
}

// Note spray: every played note throws extra notes around itself, snapped
// to the scale, spread in time, with random velocity and a chance to skip.
// The extra notes end when the note that spawned them is released.
// Allocation-free after construction.
class NoteSpray
{
public:
    struct Settings
    {
        int scale = 0, root = 0;
        bool snapInput = false;
        bool sprayOn = false;
        int count = 3;           // extra notes per played note
        int range = 12;          // semitones
        int direction = 2;       // 0 up, 1 down, 2 both
        int spreadSamples = 0;   // latest start of an extra note
        float chance = 1.0f;     // probability each extra note plays
        float velocityRandom = 0.3f;
        int strum = 0;           // 0 off, 1 up (lowest first), 2 down
        int strumSamples = 0;    // between strummed notes
    };

    NoteSpray()
    {
        pending.reserve (maxPending);
        reset();
    }

    void reset()
    {
        pending.clear();
        inputMap.fill (-1);
        soundingCount.fill (0);
        strummedPlayed.fill (false);

        for (auto& list : children)
            list.count = 0;
    }

    void process (const juce::MidiBuffer& input, juce::MidiBuffer& output, int numSamples, const Settings& settings)
    {
        output.clear();
        const auto strumming = settings.strum != 0;
        auto groupPosition = -1;

        for (const auto metadata : input)
        {
            const auto message = metadata.getMessage();
            const auto position = metadata.samplePosition;

            // Notes that start together form one strum.
            if (strumming && position != groupPosition)
            {
                finishStrum (groupPosition, settings);
                groupPosition = position;
            }

            if (message.isNoteOn())
                handleNoteOn (message, position, output, settings);
            else if (message.isNoteOff())
                handleNoteOff (message, position, output);
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                reset();
                output.addEvent (message, position);
            }
            else
                output.addEvent (message, position);
        }

        if (strumming)
            finishStrum (groupPosition, settings);

        // Extra notes whose time has come.
        for (size_t i = 0; i < pending.size();)
        {
            auto& item = pending[i];

            if (item.due < numSamples)
            {
                startChild (item, item.due, output);
                pending[i] = pending.back();
                pending.pop_back();
            }
            else
            {
                item.due -= numSamples;
                ++i;
            }
        }
    }

private:
    static constexpr size_t maxPending = 512;
    static constexpr int maxChildren = 16;

    struct Pending
    {
        int due = 0, note = 0, velocity = 0, channel = 1, parent = 0;
        bool strum = false;   // in the strum being gathered
    };

    // Lays the notes gathered at one position out in pitch order, one
    // strum step apart (replacing the spray's random spread).
    void finishStrum (int position, const Settings& settings)
    {
        if (position < 0)
            return;

        std::array<int, maxPending> group {};
        auto count = 0;

        for (int i = 0; i < (int) pending.size(); ++i)
            if (pending[(size_t) i].strum)
                group[(size_t) count++] = i;

        std::sort (group.begin(), group.begin() + count, [this, &settings] (int a, int b)
        {
            const auto noteA = pending[(size_t) a].note, noteB = pending[(size_t) b].note;
            return settings.strum == 2 ? noteA > noteB : noteA < noteB;
        });

        for (int k = 0; k < count; ++k)
        {
            auto& item = pending[(size_t) group[(size_t) k]];
            item.due = position + k * juce::jmax (0, settings.strumSamples);
            item.strum = false;
        }
    }

    struct ChildList
    {
        std::array<int, maxChildren> notes {};
        std::array<int, maxChildren> channels {};
        int count = 0;
    };

    void noteOn (int channel, int note, int velocity, int position, juce::MidiBuffer& output)
    {
        output.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), position);
        ++soundingCount[(size_t) note];
    }

    // Only release a note once nothing else is holding it.
    void noteOff (int channel, int note, int position, juce::MidiBuffer& output)
    {
        auto& count = soundingCount[(size_t) note];

        if (count > 0 && --count == 0)
            output.addEvent (juce::MidiMessage::noteOff (channel, note), position);
    }

    void startChild (const Pending& item, int position, juce::MidiBuffer& output)
    {
        auto& list = children[(size_t) item.parent];

        if (list.count >= maxChildren)
            return;

        list.notes[(size_t) list.count] = item.note;
        list.channels[(size_t) list.count] = item.channel;
        ++list.count;
        noteOn (item.channel, item.note, item.velocity, position, output);
    }

    void handleNoteOn (const juce::MidiMessage& message, int position, juce::MidiBuffer& output, const Settings& settings)
    {
        const auto input = message.getNoteNumber();
        const auto channel = message.getChannel();
        const auto velocity = (int) message.getVelocity();

        // A retrigger of a held key releases what it spawned before.
        if (inputMap[(size_t) input] >= 0)
            handleNoteOff (juce::MidiMessage::noteOff (channel, input), position, output);

        const auto played = settings.snapInput ? Scales::quantize (input, settings.scale, settings.root) : input;
        inputMap[(size_t) input] = played;

        // Strummed: the played note waits its turn like the extra notes.
        if (settings.strum != 0 && pending.size() < maxPending)
        {
            Pending item;
            item.note = played;
            item.channel = channel;
            item.parent = input;
            item.velocity = velocity;
            item.due = position;
            item.strum = true;
            pending.push_back (item);
            strummedPlayed[(size_t) input] = true;
        }
        else
        {
            noteOn (channel, played, velocity, position, output);
        }

        if (! settings.sprayOn)
            return;

        for (int k = 0; k < settings.count; ++k)
        {
            if (random.nextFloat() >= settings.chance || pending.size() >= maxPending)
                continue;

            // Pick an offset, snap it, and avoid doubling the played note.
            auto note = -1;

            for (int attempt = 0; attempt < 6 && note < 0; ++attempt)
            {
                const auto magnitude = 1 + random.nextInt (juce::jmax (1, settings.range));
                const auto sign = settings.direction == 0 ? 1 : (settings.direction == 1 ? -1 : (random.nextBool() ? 1 : -1));
                const auto candidate = Scales::quantize (played + sign * magnitude, settings.scale, settings.root);

                if (candidate != played && candidate >= 0 && candidate <= 127)
                    note = candidate;
            }

            if (note < 0)
                continue;

            Pending item;
            item.note = note;
            item.channel = channel;
            item.parent = input;
            item.velocity = juce::roundToInt ((float) velocity * (1.0f - settings.velocityRandom * random.nextFloat()));
            item.due = position + (settings.spreadSamples > 0 ? random.nextInt (settings.spreadSamples) : 0);
            item.strum = settings.strum != 0;
            pending.push_back (item);
        }
    }

    void handleNoteOff (const juce::MidiMessage& message, int position, juce::MidiBuffer& output)
    {
        const auto input = message.getNoteNumber();
        const auto channel = message.getChannel();
        const auto played = inputMap[(size_t) input];

        if (played < 0)
        {
            output.addEvent (message, position);
            return;
        }

        // Extra notes due before this release still get their moment.
        for (size_t i = 0; i < pending.size();)
        {
            if (pending[i].parent == input)
            {
                if (pending[i].due < position)
                    startChild (pending[i], pending[i].due, output);

                pending[i] = pending.back();
                pending.pop_back();
            }
            else
            {
                ++i;
            }
        }

        auto& list = children[(size_t) input];

        for (int i = 0; i < list.count; ++i)
            noteOff (list.channels[(size_t) i], list.notes[(size_t) i], position, output);

        list.count = 0;

        // A strummed played note was one of its own children.
        if (! strummedPlayed[(size_t) input])
            noteOff (channel, played, position, output);

        strummedPlayed[(size_t) input] = false;
        inputMap[(size_t) input] = -1;
    }

    std::vector<Pending> pending;
    std::array<int, 128> inputMap {};
    std::array<int, 128> soundingCount {};
    std::array<bool, 128> strummedPlayed {};
    std::array<ChildList, 128> children {};
    juce::Random random;
};
