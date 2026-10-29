#pragma once

#include <vector>

// The Modal Piano's data (content/piano/modal-piano.bin, from
// tools/build_modal_piano.py): for each recorded note and velocity, the
// partials' frequencies (as ratios to the note's nominal pitch), each
// partial's level at a fixed set of times after the strike, and the start of
// the recording itself (the hammer's attack).
struct ModalPianoTable
{
    struct Entry
    {
        int note = 60, velocity = 64, partials = 0;
        std::vector<float> ratios;  // [partials]
        std::vector<float> levels;  // [partials * times], dB
        std::vector<float> attack;  // at the table's rate
    };

    std::vector<float> times;       // seconds after the strike
    float rate = 44100.0f;
    std::vector<int> notes, velocities; // sorted, each listed once
    std::vector<Entry> entries;     // velocity-major: entries[v * notes.size() + n]

    const Entry& at (int noteIndex, int velocityIndex) const
    {
        return entries[(size_t) velocityIndex * notes.size() + (size_t) noteIndex];
    }

    bool valid() const { return ! entries.empty() && entries.size() == notes.size() * velocities.size(); }

    // Parsed once, on first use (src/dsp/ModalPianoTable.cpp).
    static const ModalPianoTable& get();
};
