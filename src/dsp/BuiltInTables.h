#pragma once

#include <juce_core/juce_core.h>

#include "Wavetable.h"
#include "WavetableDoc.h"

// The wavetables the synth carries inside itself (content/wavetables/
// builtin-wavetables.bin, made by tools/build_builtin_wavetables.py): the
// human-made packs, with their near-duplicates left out. They are factory
// tables: the table choice lists them after the User slots (choice
// factory + numUserSlots + index), so no saved choice moves, and a patch
// stores only the choice. The order is the file's (sorted by path): once a
// build has shipped, new tables are only appended to the file.
namespace BuiltInTables
{
    int count();

    // "Emu VSCO/Brass/SUS Trumpet": the pack, the folders and the table.
    juce::String path (int index);
    juce::String name (int index);
    // The folders above the table ("Emu VSCO / Brass"), for the browser's headings.
    juce::String group (int index);

    // The table ready to play (mip levels built), or null until build() has
    // run for it. Safe on the audio thread; the tables live for the whole program.
    const Wavetable* table (int index);

    // Builds it now (any thread but the audio thread; idempotent).
    const Wavetable* build (int index);

    // Asks for a table from the audio thread (a flag, no allocation); the
    // message thread then calls buildWanted().
    void want (int index);
    bool anyWanted();
    void buildWanted();

    // The table at the synth's frame size.
    bool load (int index, WavetableDoc& doc);

    // Two frames (the loudest one, then the first) at 96 points, for a card's picture.
    struct Preview
    {
        std::vector<float> first, middle;
    };
    const Preview& preview (int index);
}
