#pragma once

// Patch extras (Fable architecture review A3, 2026-11-24): everything a patch
// carries besides its parameters. Save, load, undo capture, restore, compare
// and the EDITED marker each handle these by hand (State.cpp: PatchData,
// capturePatchData, restorePatchData, samePatchData, getPatchDataHash), and a
// store one of them forgets is the "FX chain bank not saved" bug class.
//
// One row per store. tests/PatchExtrasTests.inc changes each row's data and
// proves that a saved state brings it back, that undo puts it back, and that
// the EDITED marker notices it. A new store needs a row here, or nothing
// checks it. (Moving the handlers themselves onto this table, one row at a
// time, is the next step; the rows already name what each one must cover.)

#include <functional>
#include <vector>
#include <juce_core/juce_core.h>

class IlanaSynthAudioProcessor;

namespace PatchExtras
{
struct Row
{
    const char* name;
    // Changes the store on this processor to a value no fresh patch has.
    std::function<void (IlanaSynthAudioProcessor&)> change;
    // The store as text, to compare two processors.
    std::function<juce::String (const IlanaSynthAudioProcessor&)> read;
    bool marksEdited = true; // the EDITED marker should light when it changes
};

const std::vector<Row>& rows();
} // namespace PatchExtras
