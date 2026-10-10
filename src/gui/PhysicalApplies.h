#pragma once

#include <juce_core/juce_core.h>

// Which of the Physical card's string and exciter knobs do something for the
// exciter in use. A knob that does nothing there dims in place (it keeps its
// column), so a quiet knob never reads as a broken one.
//   SUSTAIN: drives a continuous exciter (noise, saw, pulse, the oscillator
//            input, feedback), not a burst, hammer, bow or piano.
//   STIFF, PICKUP: not on the bow, which models the string differently.
//   COUPLE: needs two strings to share the bridge (UNISON 2 or more).
inline bool physicalKnobApplies (int excite, const juce::String& suffix, int unison)
{
    if (suffix == "_string_sustain")
        return excite == 1 || excite == 2 || excite == 3 || excite == 6 || excite == 10;

    if (suffix == "_string_stiffness" || suffix == "_string_pickup")
        return excite != 4;

    if (suffix == "_couple")
        return unison >= 2;

    return true;
}
