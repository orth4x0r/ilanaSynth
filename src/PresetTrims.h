#pragma once

// Per-preset trims from the preset critic (tools/tune_presets.py writes this
// file; don't edit it by hand). levelDb is added to the preset's master level
// so each preset sits at its category's loudness; macroScale multiplies the
// depth of every routing a macro drives, so each macro audibly changes the
// sound. Presets that aren't listed load exactly as written.

#include <vector>

namespace Presets
{
struct Trim
{
    const char* name;
    float levelDb;
    float macroScale[4];
};

inline const std::vector<Trim>& getTrims()
{
    static const std::vector<Trim> trims {
    };
    return trims;
}
} // namespace Presets
