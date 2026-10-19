#pragma once

// Factory presets left out of the browser because another one sounds almost
// the same (ilana, 2026-10-19: "same goes for almost duplicated presets",
// "quality over quantity"). Judged on the audio: each preset's note and chord
// rendered by ilanaPresetRender, CLAP audio embeddings (tools/clap_score.py);
// a preset goes when its cosine to a preset kept is 0.93 or more (the taste
// tools' rule), and of two alike the one that fits its own name better stays
// (tools/dedupe_presets.py). Never retired: the original 80 (their indices are
// host program numbers), the presets the tests and the bounced presets name,
// Init, the FX Input presets (silent without input), rhythmic presets (a note render cannot show rhythm) and the DX7 voices (the
// browser lists each ROM bank whole). ilana's own presets (content/user-presets)
// are not factory presets at all. getFactoryPresets() drops these names after
// the packs are built; the definitions stay in the code. docs/PRESET-DEDUPE.md
// lists each with the preset it repeats.
namespace Presets
{
inline bool isRetiredPreset (const juce::String& name)
{
    static const juce::StringArray retired {
        "Bark Rhodes", "Baroque Lute", "Brass Section", "Breathing Bow", "Celeste", "Chorded Sixpack",
        "Diamond Growl", "Diode Rumble", "Disperser Sub", "Dual Body Coupling", "Evolve Macro Pad",
        "Evolving Vector Drone", "FM Kalimba", "FM Struck Body", "Fold Drone", "Ghost Note Strings",
        "Kalimba Bar", "Ladder Organ", "Mellow Reed Bell", "Mellow Rhodes", "Metallic Hit",
        "Morphing Strings", "Muted Cello Pizz", "Panned Engine Sweep", "Pedal Sustain Bloom",
        "Per-Voice Drift Pad", "Pluck Walker", "Pump Pad", "Routed Duo Pad", "Six-Voice Drift",
        "Soft Felt Piano", "Supersaw Stab", "Three Carrier Choir", "Upright Piano", "Vector Drift Pad",
        "Viola Drone", "Warm Analog Pad", "West Fold Bass", "Wood Bar Hybrid", "Wurli Drive"
    };
    return retired.contains (name);
}
} // namespace Presets
