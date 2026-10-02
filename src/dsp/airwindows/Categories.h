// The Airwindows category FX modules (FX types 32-41): each offers a few of
// the best-regarded Airwindows effects of one kind (picked 2026-10-01 from
// forum favourites and Chris Johnson's own recommendations), with the same
// five knobs and mix as the all-in-one Airwindows module (type 30), which
// stays for older patches. Indices are Registry.h's; a module's list is
// append only (its choice index is saved in patches).
#pragma once

#include <vector>

#include "Registry.h"

namespace airwindows
{
struct CategoryModule
{
    const char* label; // the FX type's name
    const char* id;    // parameter prefix: fx_<id>_algo, fx_<id>_p1..p5, fx_<id>_mix
    std::vector<int> algorithms;
};

constexpr int firstCategoryFxType = 32;

inline const std::vector<CategoryModule>& categoryModules()
{
    static const std::vector<CategoryModule> list {
        { "AW Tape",       "awtape",   { 39, 0, 1, 2, 40 } },    // ToTape8, ToTape6, IronOxide5, Tape, TapeHack2
        { "AW Saturation", "awsat",    { 41, 5, 8, 7, 6 } },     // Density3, Spiral2, Mojo, Tube2, PurestDrive
        { "AW Reverb",     "awverb",   { 24, 26, 25, 27, 28 } }, // Galactic, Verbity2, kCathedral, Chamber, MatrixVerb
        { "AW Delay",      "awdelay",  { 42, 43, 44, 45, 46 } }, // TapeDelay2, PitchDelay, StarChild2, PurestEcho, Doublelay
        { "AW Modulation", "awmod",    { 38, 47, 48, 49, 37 } }, // ChorusEnsemble, Ensemble, Chorus, Vibrato, Flutter
        { "AW Dynamics",   "awdyn",    { 19, 20, 23, 21, 22 } }, // Pressure5, ButterComp2, Pop2, Logical4, Pyewacket
        { "AW EQ",         "aweq",     { 15, 16, 14, 17, 18 } }, // Capacitor2, Baxandall2, Air3, Isolator2, Holt2
        { "AW Console",    "awcons",   { 10, 11, 12, 50 } },     // Console7Channel, Console7Buss, Channel9, Desk4
        { "AW Lo-Fi",      "awlofi",   { 32, 33, 51, 52, 34 } }, // DeRez2, Deckwrecka, TapeDust, Dirt, BitShiftGain
        { "AW Stereo",     "awstereo", { 29, 30, 31, 53 } },     // Wider, Srsly2, ToVinyl4, StereoFX
    };
    return list;
}

// The category module an FX type is, or -1.
inline int categoryForFxType (int type)
{
    const auto index = type - firstCategoryFxType;
    return index >= 0 && index < (int) categoryModules().size() ? index : -1;
}
} // namespace airwindows
