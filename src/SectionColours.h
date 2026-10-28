#pragma once

#include <array>
#include <cstdint>

// Colour coding (ilana, 2026-10-27: "let the user pick the color in the
// section so they can color code"): a patch can give a section its own
// colour, picked from a fixed set of swatches. 0 = the section's default.
// The processor keeps the patch's choices (saved with it); the editor copies
// them into shown() and the theme's identity colours read them from there.
namespace SectionColours
{
    // OSC 1-6, SUB + NOISE, STRINGS + BODY, WEST COAST, the five main
    // envelopes (AMP ENV, FILT ENV = FILTER 1, FILT 2 ENV = FILTER 2,
    // MOD ENV, ENV 4) and LFO 1-16.
    inline constexpr int oscKey (int osc) { return osc; }
    inline constexpr int subKey = 6;
    inline constexpr int physicalKey = 7;
    inline constexpr int westKey = 8;
    inline constexpr int envKey (int env) { return 9 + env; }
    inline constexpr int lfoKey (int lfo) { return 14 + lfo; }
    inline constexpr int count = 30;
    inline constexpr int numEnvs = 5, numLfos = 16, numOscs = 6;

    using Table = std::array<std::uint32_t, count>;

    // What the editor paints with (the shown patch's choices).
    inline Table& shown()
    {
        static Table table {};
        return table;
    }

    // Bumped whenever shown() changes, so pages that keep a colour re-read it.
    inline int& version()
    {
        static int value = 0;
        return value;
    }

    inline std::uint32_t pickedFor (int key) { return key >= 0 && key < count ? shown()[(size_t) key] : 0u; }

    // The swatches: bright enough on graphite, apart from each other, and
    // apart from the accent orange.
    inline constexpr std::uint32_t swatches[]
    {
        0xfff2b83d, 0xffffd75a, 0xffbfe35a, 0xff3ecf9a, 0xff3fc7e8, 0xff5b8cff,
        0xff9aa7ff, 0xffa47bff, 0xffe070ff, 0xffff6fa8, 0xffe39a6b, 0xffc9ced8
    };
    inline constexpr int numSwatches = (int) (sizeof (swatches) / sizeof (swatches[0]));
}
