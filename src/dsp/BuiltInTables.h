#pragma once

#include <juce_core/juce_core.h>

#include "WavetableDoc.h"

// The wavetables the synth carries inside itself (content/wavetables/
// builtin-wavetables.bin, made by tools/build_builtin_wavetables.py): the
// human-made packs, with their near-duplicates left out. They sit in the
// table browser after the factory and User tables; picking one copies it into
// a User slot, like any loaded table, so a patch keeps it.
namespace BuiltInTables
{
    int count();

    // "Emu VSCO/Brass/SUS Trumpet": the pack, the folders and the table.
    juce::String path (int index);
    juce::String name (int index);
    // The folders above the table ("Emu VSCO / Brass"), for the browser's headings.
    juce::String group (int index);

    // The table at the synth's frame size.
    bool load (int index, WavetableDoc& doc);

    // Two frames (the loudest one, then the first) at 96 points, for a card's picture.
    struct Preview
    {
        std::vector<float> first, middle;
    };
    const Preview& preview (int index);
}
