#pragma once

#include <juce_core/juce_core.h>

// One sentence of what an FX card does, for the card's picture slot where the
// effect has no display of its own (UI review 13, V13-1, I13-3): the left half
// of a FREEZE or WIDENER card was blank, and an Airwindows algorithm's name
// said nothing about what it does. Keyed by FX type, and by Airwindows
// algorithm name for the Airwindows modules.
namespace FxInfoText
{
inline juce::String forType (int type)
{
    switch (type)
    {
        case 8:  return "Delays one side by a few milliseconds, so the sound seems to come from a wider place.";
        case 10: return "Repeats a short slice of the sound in time with the tempo.";
        case 11: return "Blurs the sound into a soft, diffuse wash.";
        case 12: return "HOLD captures the sound at that moment and sustains it. MIX blends it under the live signal.";
        case 14: return "A short, swept delay with feedback: a jet-like whoosh.";
        case 15: return "A subtle stereo thickener: short modulated delays that widen without an obvious sweep.";
        case 16: return "Chops the sound into rhythmic steps; draw the pattern below.";
        case 17: return "Slows the sound to a halt like a tape machine switched off.";
        case 18: return "Tips the balance between low and high frequencies around a pivot.";
        case 19: return "Gain, mono and phase invert: housekeeping for the signal.";
        case 22: return "Widens the stereo image by mixing the sides up against the middle.";
        case 23: return "Varies the volume in a steady pulse.";
        case 24: return "Moves every frequency up or down by the same number of Hz: bell-like and inharmonic.";
        case 25: return "Multiplies the sound by a tone: metallic, bell-like sidebands.";
        case 26: return "Adds a copy one octave down.";
        case 28: return "Feeds the output back through a delay and a tone filter: it rings and sings.";
        default: return {};
    }
}

inline juce::String forAirwindows (const juce::String& name)
{
    struct Entry { const char* name; const char* text; };
    static const Entry entries[] {
        { "ToTape6", "Tape saturation with head bump, softening and flutter." },
        { "IronOxide5", "Tape emulation with its own high and low response, flutter and noise." },
        { "Tape", "A quick tape glue: slam for the squash, bump for the low end." },
        { "Density", "Saturation that adds weight and density without harshness." },
        { "Drive", "A clean drive that thickens and then distorts, with a low cut ahead of it." },
        { "Spiral2", "Smooth saturation that curls into soft clipping with no harsh edges." },
        { "PurestDrive", "The cleanest saturation: adds harmonics and keeps the level." },
        { "Tube2", "Tube-style saturation with its own input and tube amounts." },
        { "Mojo", "Level-dependent harmonic weight, like a transformer pushed a little." },
        { "Coils2", "Transformer-coil saturation, with a cheapness control for the rough end." },
        { "Console7Channel", "A console channel: fader level with the desk's subtle saturation." },
        { "Console7Buss", "A console mix bus stage that glues with gentle saturation." },
        { "Channel9", "A console channel with a choice of desk types and drive." },
        { "Air", "A three-tap treble lift that adds air without harshness." },
        { "Air3", "Adds air to the top end, with a ground control for the low side." },
        { "Capacitor2", "Gentle low and high cuts, like capacitors in the signal path." },
        { "Baxandall2", "Bass and treble tone controls on a smooth Baxandall curve." },
        { "Isolator2", "A steep filter that isolates one band of the sound." },
        { "Holt2", "A resonant low pass filter with a choice of poles." },
        { "Pressure5", "A compressor that works on pressure rather than a threshold." },
        { "ButterComp2", "A smooth bipolar compressor that glues without pumping." },
        { "Logical4", "A full compressor: threshold, ratio, speed and makeup." },
        { "Pyewacket", "A compressor and limiter with a character of its own." },
        { "Pop2", "A punchy compressor that shapes the transient." },
        { "Galactic", "A huge reverb with detune and a shimmer of its own." },
        { "kCathedral", "A big, dark cathedral reverb." },
        { "Verbity2", "A clean room reverb with size, sustain and a mulch control." },
        { "Chamber", "A smooth chamber reverb: size, length, brightness and darkness." },
        { "MatrixVerb", "A matrix reverb with filter, damping, room size and flavour." },
        { "Wider", "Widens the stereo image and moves the centre." },
        { "Srsly2", "Stereo spatial enhancement: pulls the centre apart from the space." },
        { "ToVinyl4", "Filters the middle and the sides the way a vinyl cut has to." },
        { "DeRez2", "Reduces the sample rate and the bit depth." },
        { "Deckwrecka", "The sound of a damaged tape deck." },
        { "BitShiftGain", "Gain in whole bit steps." },
        { "DrumSlam", "Dense, hard saturation made for drums." },
        { "Inflamer", "Adds harmonics that inflate the sound, with drive and curve." },
        { "Flutter", "Tape flutter: a fast, small pitch wobble." },
        { "ChorusEnsemble", "A lush chorus with speed and range." },
        { "ToTape8", "Tape saturation with tilt, flutter and head bump." },
        { "TapeHack2", "Simple, warm tape saturation." },
        { "Density3", "Saturation that adds density, with a low cut." },
        { "TapeDelay2", "A tape echo with regeneration, tone and flutter." },
        { "PitchDelay", "A delay whose repeats shift in pitch." },
        { "StarChild2", "A grainy, sustaining echo." },
        { "PurestEcho", "A clean four-tap echo." },
        { "Doublelay", "Two short delays, one per side, for doubling." },
        { "Ensemble", "An ensemble of chorus voices." },
        { "Chorus", "A classic chorus." },
        { "Vibrato", "Pitch vibrato with its own FM." },
        { "Desk4", "Console desk saturation with power sag." },
        { "TapeDust", "The dust and crackle of old tape." },
        { "Dirt", "Dirty saturation with low and high cuts." },
        { "StereoFX", "Widens the sides, keeps the bass mono and squashes the centre." },
    };

    for (const auto& entry : entries)
        if (name == entry.name)
            return entry.text;

    return {};
}
} // namespace FxInfoText
