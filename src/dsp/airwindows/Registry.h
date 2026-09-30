// The Airwindows algorithms the Airwindows FX module offers (Chris Johnson's
// plugins, MIT: see LICENSE.txt). Written by tools/port_airwindows.py.
// The order is the fx_aw_algo choice's: append only, never reorder.
#pragma once

#include <memory>
#include <vector>

#include "Algorithm.h"

namespace airwindows
{
struct Knob
{
    int parameter;       // the plugin's own parameter (A = 0, B = 1 ...)
    const char* name;    // as the plugin names it
    float defaultValue;  // the plugin's default, as the knob's 0..1
    float lo, hi;        // the part of the plugin's 0..1 the knob spans

    float toPlugin (float knob) const { return lo + (hi - lo) * knob; }
};

struct Info
{
    const char* name;
    const char* category;
    int numKnobs;        // up to 5; the module's knobs past this are hidden
    Knob knobs[5];
    std::unique_ptr<Algorithm> (*create)();
};

std::unique_ptr<Algorithm> createToTape6();
std::unique_ptr<Algorithm> createIronOxide5();
std::unique_ptr<Algorithm> createTape();
std::unique_ptr<Algorithm> createDensity();
std::unique_ptr<Algorithm> createDrive();
std::unique_ptr<Algorithm> createSpiral2();
std::unique_ptr<Algorithm> createPurestDrive();
std::unique_ptr<Algorithm> createTube2();
std::unique_ptr<Algorithm> createMojo();
std::unique_ptr<Algorithm> createCoils2();
std::unique_ptr<Algorithm> createConsole7Channel();
std::unique_ptr<Algorithm> createConsole7Buss();
std::unique_ptr<Algorithm> createChannel9();
std::unique_ptr<Algorithm> createAir();
std::unique_ptr<Algorithm> createAir3();
std::unique_ptr<Algorithm> createCapacitor2();
std::unique_ptr<Algorithm> createBaxandall2();
std::unique_ptr<Algorithm> createIsolator2();
std::unique_ptr<Algorithm> createHolt2();
std::unique_ptr<Algorithm> createPressure5();
std::unique_ptr<Algorithm> createButterComp2();
std::unique_ptr<Algorithm> createLogical4();
std::unique_ptr<Algorithm> createPyewacket();
std::unique_ptr<Algorithm> createPop2();
std::unique_ptr<Algorithm> createGalactic();
std::unique_ptr<Algorithm> createkCathedral();
std::unique_ptr<Algorithm> createVerbity2();
std::unique_ptr<Algorithm> createChamber();
std::unique_ptr<Algorithm> createMatrixVerb();
std::unique_ptr<Algorithm> createWider();
std::unique_ptr<Algorithm> createSrsly2();
std::unique_ptr<Algorithm> createToVinyl4();
std::unique_ptr<Algorithm> createDeRez2();
std::unique_ptr<Algorithm> createDeckwrecka();
std::unique_ptr<Algorithm> createBitShiftGain();
std::unique_ptr<Algorithm> createDrumSlam();
std::unique_ptr<Algorithm> createInflamer();
std::unique_ptr<Algorithm> createFlutter();
std::unique_ptr<Algorithm> createChorusEnsemble();

inline const std::vector<Info>& registry()
{
    static const std::vector<Info> list {
        { "ToTape6", "Saturation & Tape", 5, { { 0, "Input", 0.5f, 0.0f, 1.0f }, { 1, "Soften", 0.5f, 0.0f, 1.0f }, { 2, "Head B", 0.5f, 0.0f, 1.0f }, { 3, "Flutter", 0.5f, 0.0f, 1.0f }, { 4, "Output", 0.5f, 0.0f, 1.0f } }, &createToTape6 },
        { "IronOxide5", "Saturation & Tape", 5, { { 0, "Input Trim", 0.5f, 0.0f, 1.0f }, { 1, "Tape High", 0.562341325f, 0.0f, 1.0f }, { 2, "Tape Low", 0.562341325f, 0.0f, 1.0f }, { 3, "Flutter", 0.5f, 0.0f, 1.0f }, { 4, "Noise", 0.5f, 0.0f, 1.0f } }, &createIronOxide5 },
        { "Tape", "Saturation & Tape", 2, { { 0, "Slam", 0.5f, 0.0f, 1.0f }, { 1, "Bump", 0.5f, 0.0f, 1.0f } }, &createTape },
        { "Density", "Saturation & Tape", 4, { { 0, "Density", 0.2f, 0.0f, 1.0f }, { 1, "Highpass", 0.0f, 0.0f, 1.0f }, { 2, "Out Level", 1.0f, 0.0f, 1.0f }, { 3, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createDensity },
        { "Drive", "Saturation & Tape", 4, { { 0, "Drive", 0.0f, 0.0f, 1.0f }, { 1, "Highpass", 0.0f, 0.0f, 1.0f }, { 2, "Out Level", 1.0f, 0.0f, 1.0f }, { 3, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createDrive },
        { "Spiral2", "Saturation & Tape", 5, { { 0, "Input", 0.5f, 0.0f, 1.0f }, { 1, "Highpass", 0.0f, 0.0f, 1.0f }, { 2, "Presence", 0.5f, 0.0f, 1.0f }, { 3, "Output", 1.0f, 0.0f, 1.0f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createSpiral2 },
        { "PurestDrive", "Saturation & Tape", 1, { { 0, "Drive", 0.0f, 0.0f, 1.0f } }, &createPurestDrive },
        { "Tube2", "Saturation & Tape", 2, { { 0, "Input", 0.5f, 0.0f, 1.0f }, { 1, "Tube", 0.5f, 0.0f, 1.0f } }, &createTube2 },
        { "Mojo", "Saturation & Tape", 1, { { 0, "Input", 0.5f, 0.0f, 1.0f } }, &createMojo },
        { "Coils2", "Saturation & Tape", 3, { { 0, "Saturate", 0.0f, 0.0f, 1.0f }, { 1, "Cheapness", 0.0f, 0.0f, 1.0f }, { 2, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createCoils2 },
        { "Console7Channel", "Console", 1, { { 0, "Fader", 0.772f, 0.0f, 1.0f } }, &createConsole7Channel },
        { "Console7Buss", "Console", 1, { { 0, "Master", 1.0f, 0.0f, 1.0f } }, &createConsole7Buss },
        { "Channel9", "Console", 3, { { 0, "Console Type", 0.0f, 0.0f, 1.0f }, { 1, "Drive", 0.0f, 0.0f, 1.0f }, { 2, "Output", 1.0f, 0.0f, 1.0f } }, &createChannel9 },
        { "Air", "EQ & Filter", 5, { { 0, "22K tap", 0.5f, 0.2f, 0.8f }, { 1, "15K tap", 0.5f, 0.2f, 0.8f }, { 2, "11K tap", 0.5f, 0.2f, 0.8f }, { 3, "filters Q", 0.0f, 0.0f, 1.0f }, { 4, "Output Level", 1.0f, 0.0f, 1.0f } }, &createAir },
        { "Air3", "EQ & Filter", 2, { { 0, "Air", 0.714285714f, 0.0f, 0.7f }, { 1, "Gnd", 0.5f, 0.0f, 1.0f } }, &createAir3 },
        { "Capacitor2", "EQ & Filter", 4, { { 0, "Lowpass", 1.0f, 0.0f, 1.0f }, { 1, "Highpass", 0.0f, 0.0f, 1.0f }, { 2, "NonLin", 0.0f, 0.0f, 1.0f }, { 3, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createCapacitor2 },
        { "Baxandall2", "EQ & Filter", 2, { { 0, "Treble", 0.5f, 0.1875f, 0.8125f }, { 1, "Bass", 0.5f, 0.1875f, 0.8125f } }, &createBaxandall2 },
        { "Isolator2", "EQ & Filter", 4, { { 0, "Freq", 1.0f, 0.0f, 1.0f }, { 1, "Reso", 0.0f, 0.0f, 1.0f }, { 2, "High", 0.0f, 0.0f, 1.0f }, { 3, "Low", 1.0f, 0.0f, 1.0f } }, &createIsolator2 },
        { "Holt2", "EQ & Filter", 5, { { 0, "Freq", 1.0f, 0.0f, 1.0f }, { 1, "Reso", 0.5f, 0.0f, 1.0f }, { 2, "Poles", 1.0f, 0.0f, 1.0f }, { 3, "Output", 1.0f, 0.0f, 1.0f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createHolt2 },
        { "Pressure5", "Dynamics", 5, { { 0, "Pressre", 0.0f, 0.0f, 1.0f }, { 1, "Speed", 0.25f, 0.0f, 1.0f }, { 2, "Mewines", 1.0f, 0.0f, 1.0f }, { 3, "PawClaw", 0.5f, 0.0f, 1.0f }, { 4, "Output", 0.5f, 0.0f, 1.0f } }, &createPressure5 },
        { "ButterComp2", "Dynamics", 3, { { 0, "Compress", 0.0f, 0.0f, 1.0f }, { 1, "Output", 0.5f, 0.0f, 1.0f }, { 2, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createButterComp2 },
        { "Logical4", "Dynamics", 5, { { 0, "Threshold", 0.5f, 0.0f, 1.0f }, { 1, "Ratio", 0.2f, 0.0f, 1.0f }, { 2, "Speed", 0.192020202f, 0.0f, 1.0f }, { 3, "MakeupGn", 0.714285714f, 0.0f, 0.7f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createLogical4 },
        { "Pyewacket", "Dynamics", 3, { { 0, "Input Gain", 0.5f, 0.0f, 1.0f }, { 1, "Release", 0.5f, 0.0f, 1.0f }, { 2, "Output Gain", 0.5f, 0.0f, 1.0f } }, &createPyewacket },
        { "Pop2", "Dynamics", 5, { { 0, "Compres", 0.5f, 0.0f, 1.0f }, { 1, "Attack", 0.5f, 0.0f, 1.0f }, { 2, "Release", 0.5f, 0.0f, 1.0f }, { 3, "Drive", 0.5f, 0.0f, 1.0f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createPop2 },
        { "Galactic", "Space", 5, { { 0, "Replace", 0.5f, 0.0f, 1.0f }, { 1, "Brightness", 0.5f, 0.0f, 1.0f }, { 2, "Detune", 0.5f, 0.0f, 1.0f }, { 3, "Bigness", 1.0f, 0.0f, 1.0f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createGalactic },
        { "kCathedral", "Space", 1, { { 0, "Wetness", 1.0f, 0.0f, 1.0f } }, &createkCathedral },
        { "Verbity2", "Space", 4, { { 0, "RmSize", 0.5f, 0.0f, 1.0f }, { 1, "Sustain", 0.5f, 0.0f, 1.0f }, { 2, "Mulch", 0.5f, 0.0f, 1.0f }, { 3, "Wetness", 1.0f, 0.0f, 1.0f } }, &createVerbity2 },
        { "Chamber", "Space", 5, { { 0, "Bigness", 0.35f, 0.0f, 1.0f }, { 1, "Longness", 0.35f, 0.0f, 1.0f }, { 2, "Liteness", 0.35f, 0.0f, 1.0f }, { 3, "Darkness", 0.35f, 0.0f, 1.0f }, { 4, "Wetness", 0.35f, 0.0f, 1.0f } }, &createChamber },
        { "MatrixVerb", "Space", 5, { { 0, "Filter", 1.0f, 0.0f, 1.0f }, { 1, "Damping", 0.0f, 0.0f, 1.0f }, { 4, "RmSize", 0.5f, 0.0f, 1.0f }, { 5, "Flavor", 0.5f, 0.0f, 1.0f }, { 6, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createMatrixVerb },
        { "Wider", "Stereo", 3, { { 0, "Width", 0.5f, 0.0f, 1.0f }, { 1, "Center", 0.5f, 0.0f, 1.0f }, { 2, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createWider },
        { "Srsly2", "Stereo", 5, { { 0, "Center", 0.5f, 0.0f, 1.0f }, { 1, "Space", 0.5f, 0.0f, 1.0f }, { 2, "Level", 1.0f, 0.0f, 1.0f }, { 3, "Q", 0.5f, 0.0f, 1.0f }, { 4, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createSrsly2 },
        { "ToVinyl4", "Stereo", 4, { { 0, "Mid HiP", 0.203419f, 0.0f, 1.0f }, { 1, "SideHiP", 0.3424051f, 0.0f, 1.0f }, { 2, "H Limit", 0.32f, 0.0f, 1.0f }, { 3, "Gv Wear", 0.064f, 0.0f, 1.0f } }, &createToVinyl4 },
        { "DeRez2", "Lo-Fi", 4, { { 0, "Rate", 1.0f, 0.0f, 1.0f }, { 1, "Rez", 1.0f, 0.0f, 1.0f }, { 2, "Hard", 1.0f, 0.0f, 1.0f }, { 3, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createDeRez2 },
        { "Deckwrecka", "Lo-Fi", 1, { { 0, "Wreck", 0.0f, 0.0f, 1.0f } }, &createDeckwrecka },
        { "BitShiftGain", "Lo-Fi", 1, { { 0, "BitShift", 0.8f, 0.25f, 0.5625f } }, &createBitShiftGain },
        { "DrumSlam", "Character", 3, { { 0, "Drive", 0.0f, 0.0f, 1.0f }, { 1, "Output", 1.0f, 0.0f, 1.0f }, { 2, "Dry/Wet", 1.0f, 0.0f, 1.0f } }, &createDrumSlam },
        { "Inflamer", "Character", 3, { { 0, "Drive", 0.5f, 0.0f, 1.0f }, { 1, "Curve", 0.5f, 0.0f, 1.0f }, { 2, "Effect", 1.0f, 0.0f, 1.0f } }, &createInflamer },
        { "Flutter", "Character", 1, { { 0, "Flutter", 0.0f, 0.0f, 1.0f } }, &createFlutter },
        { "ChorusEnsemble", "Character", 3, { { 0, "Speed", 0.5f, 0.0f, 1.0f }, { 1, "Range", 0.5f, 0.0f, 1.0f }, { 2, "Dry/Wet", 0.8f, 0.0f, 1.0f } }, &createChorusEnsemble },
    };
    return list;
}

inline int count() { return (int) registry().size(); }
} // namespace airwindows
