# IlanaSynth — v1.0

**IlanaSynth, para un sonido más buto.** (for a more brutal sound)

An aggressive wavetable synthesizer for VST3, built with JUCE.

---

## What it is

IlanaSynth is a complete sound design machine: three oscillators with wavetable,
physical-modelling string and full sample modes, twelve filter models across
two routable filters, five tension envelopes, four LFOs with a drawable curve
shape, a step sequencer and MSEG, a 32-slot modulation matrix, a 10-slot
effects rack with 29 modules, an eight-mode arpeggiator and a tuned resonator,
wrapped in a hardware-inspired interface with 200 factory presets.

---

## What's new in 1.0

- **MAIN page**: oscillators, filter, amp envelope and LFOs on one screen.
- **Filters**: Ladder (LP/HP), Diode, MS-20, Comb +/−, Formant and a
  LP→BP→HP Morph filter join the SVF modes, and resonance now
  self-oscillates at the top of its range.
- **Per-oscillator routing**: send each oscillator to Filter 1, Filter 2 or
  straight past the filters.
- **Warp modes**: Sync, Bend +/−, PWM, Mirror, Asym, Quantize, FM and Ring,
  each with its own amount.
- **Unison up to 16 voices**, with Classic, Hypersaw, Octaves and Fifths
  stacks and a blend control.
- **Modulate almost anything**: a 32-slot matrix, each slot with a curve,
  polarity, a *Via* source that scales it, and bypass. Destinations include
  every envelope stage, filter drive and morph, warp amounts, unison, glide,
  string settings and most effect parameters.
- **Per-voice LFOs** (turn on RETRIG), a start-phase control, and a
  **Curve** shape you draw with points and bendable segments (12 starting
  shapes built in).
- **Mono and legato** voice modes with legato-only glide, and a polyphony
  limit.
- **3-band parametric EQ** effect with a draggable curve.
- **Oversampling** choice of 2x or 4x.
- **Audio → wavetable**: LOAD can resynthesise any recording into a
  64-frame wavetable (pitch-tracked).
- **200 factory presets**, every one with four named macros ready to play.
  The original 80 get default TONE / MORPH / DRIVE / SPACE macros that leave
  their sound untouched at zero.
- **Save dialog** with name, category and tags, and a resizable window
  (drag the corner, 75–200%).

---

## Features

### Oscillators (3)
- Every oscillator has three modes:
  - **Wavetable**: 16 factory tables (Basic, HardSync, Wavefold, FM Metal,
    Formant, Comb, PWM, DriveSaw, Sine, Triangle, Analog, Vowel, Glass,
    Fractal, Riser, Digital) plus 4 user slots. Load your own `.wav` with the
    LOAD button (as frames, or resynthesised from any recording) or drag a
    `.wav` straight onto the waveform display.
  - **String**: Karplus–Strong physical model with excite, decay, damp and
    sustain controls.
  - **Sample**: load any `.wav` (up to 120 s). Tuned or untuned playback,
    loop / one-shot, reverse, start & end points and fades. Five factory
    samples are built in (Metal Hit, Vocal Ah, Sub Tone, Vinyl Loop,
    Noise Rise).
- Warp modes: Sync, Bend +, Bend −, PWM, Mirror, Asym, Quantize, FM (from the
  other oscillator) and Ring.
- Unison up to 16 voices (8 in String and Sample modes) with detune, stereo
  spread, stack mode (Classic, Hypersaw, Octaves, Fifths) and blend.
- Per-oscillator level, pan, semitone and fine tuning, and a filter route.
- Chord modes (Octave, Fifth, Power, Major, Minor, Sus4).
- Frame morphing, modulatable like any other control.
- OSC 3 is the sub/third oscillator, with an extra octave switch and a noise
  layer.

### Filters (2)
- 12 models: Low pass, Band pass, High pass, Notch (12 or 24 dB), Ladder LP,
  Ladder HP, Diode LP, MS-20 LP, Comb +, Comb −, Formant (vowel morph) and
  Morph (LP → BP → HP).
- Cutoff, resonance (self-oscillating at the top), drive, envelope amount,
  key tracking, audio-rate FM and morph.
- **Serial** (F1 into F2) or **parallel** (both summed) routing, and each
  oscillator can instead go only to Filter 1, only to Filter 2, or skip the
  filters.
- Drag the markers on the response display to set cutoff and resonance.

### Envelopes & LFOs
- 5 tension envelopes: AMP, FILTER 1, FILTER 2, MOD and ENV 4, shown as cards
  that mark which ones the patch uses. Drag a card onto a knob to modulate it.
  - Drag the handles on the graph to shape A / D / S / R, and drag the curve
    to bend the **tension**.
  - Velocity sensitivity on AMP and FILTER 1.
- 4 LFOs with Sine, Triangle, Saw up, Saw down, Square, S&H, Draw, Steps and
  **Curve** shapes. Free rate or host-synced divisions, start phase, and
  RETRIG, which makes the LFO run separately in every voice.
- The **Curve** editor: click to add a point, drag to move, drag a segment up
  or down to bend it, double-click to delete; right-click for presets, grid,
  flip and reverse.
- **Step LFO editors** (two 16-step sequencers) and a **4-stage MSEG** live on
  the SEQ tab, which appears when a step LFO shape is selected or the MSEG is
  assigned.

### Modulation
- 32-slot matrix, 22 sources and over 190 destinations. Each slot has depth,
  a response curve, polarity (Natural, Unipolar, Bipolar), a **Via** source
  that scales the routing (e.g. the mod wheel fading in an LFO) and bypass.
- Sources: LFO 1–4, MOD / FILTER 1 / FILTER 2 / AMP / ENV 4 envelopes, MSEG,
  velocity, key track, random, mod wheel, aftertouch, expression, 4 macros
  and a clocked sample & hold.
- **Drag a source chip or card onto any knob** to assign it, then drag the
  coloured dot next to the knob to set the depth (double-click it to
  remove). Hovering a source highlights everything it modulates.
- **Right-click any knob** for a quick mod / reset / copy-paste / MIDI learn
  menu.
- Four macros with editable names (double-click a name), shown in the bottom
  bar and in the matrix.

### Effects rack (10 slots, 29 modules)
Drive (tube / fuzz / clean), Bit Crusher, Amp, Compressor, Chorus, Phaser,
Comb, Flanger, Dimension, Tremolo, Haas, Delay (tape pitch and wow), Multi-Tap
delay with a drawable tap grid, Reverb (Room, Hall, Plate, Shimmer, Spring,
Gated and **user IR loading**), Feedback module, Smear, Freeze, Granular
Stutter (reverse + pitch), Gate, Tape Stop, Tilt EQ, **3-band parametric EQ**,
Utility, OTT, Limiter, Stereo Width, Frequency Shifter, Ring Mod, Octaver and
Vowel filter.

- Every slot: bypass, blend, solo and a per-slot CPU readout; drag rows to
  reorder (bypass, solo and blend move with the effect).
- The rack starts empty: click an empty row to choose a module.
- **A/B chains** with copy A to B, plus save/load of whole chains.

### Voice & global
- 16-voice polyphony with a voice limit; Poly, Mono and Legato modes; MPE
  mode; voice spread and unison phase randomisation.
- Glide (optionally legato-only) and pitch-bend range in the bottom bar.
- Cross modulation: FM (OSC 2 → OSC 1) with feedback, ring mod, hard sync and
  analogue-style drift.
- Resonator: tuned body after the filters with amount, decay, offset and key
  tracking.
- 2x or 4x oversampling for the voice engine (SCOPE tab).
- Soft clipper with clip gain, master volume, 4 macros with MIDI learn.

### Arpeggiator
- 8 modes: Up, Down, UpDown, Random, DownUp, Converge, Walk and Chord.
- 1–4 octaves, host-synced rates, gate length, with a live pattern display.

### Scope
- Oscilloscope / spectrum analyser, HOLD and PEAK hold, stereo meters with
  click-to-solo, plus a full-screen SCOPE tab.

### Interface
- Hardware-inspired design with 4 colour themes, resizable from the corner
  (75–200%) or from the settings menu.
- Header: preset name with category and an EDITED marker, previous / next,
  favourite, SAVE, undo / redo with history, A/B, DICE (randomise or mutate
  the patch, or one section of it) and settings.
- The bottom bar holds the modulation source chips, the four macros, glide,
  bend, voice mode, voices and master; the on-screen keyboard can be hidden.
- Tooltips on hover, a welcome tour (re-open it with the `?` button) and
  number keys 1–9 to switch tabs.
- 200 factory presets in Bass, Lead, Pluck, Pad, Keys, Chords, Arp, Drone and
  FX, with search (names, categories and tags), favourites and user presets.

---

## Installing

### Windows: build and install from source (one click)
Double-click **`build-and-install.cmd`** in the repository folder. It asks for
administrator rights, builds the Release VST3 and standalone, copies the VST3
to `C:\Program Files\Common Files\VST3\ilanaSynth.vst3`, puts the
standalone in `C:\Program Files\ilanaSynth` with a desktop shortcut, and
tells you what went wrong if a tool is missing. You need Visual Studio 2022
(Community or Build Tools) with *Desktop development with C++*, Git, and
internet the first time (JUCE is downloaded). Close your DAW first, then
rescan plugins once it finishes. Run it again after pulling changes to update.

### Windows: installer
1. Run `ilanaSynth-1.0-Windows-Setup.exe` (installs the VST3 and the
   standalone), or copy the `ilanaSynth.vst3` folder to
   `C:\Program Files\Common Files\VST3\`.
2. Rescan your plugin folder in your DAW.
3. The standalone `ilanaSynth.exe` needs no DAW at all.

### macOS
There is no pre-built macOS binary in this release. Build it yourself (about
ten minutes, internet needed the first time for JUCE):

```bash
xcode-select --install   # Apple compilers, once
brew install cmake       # or the .pkg from cmake.org
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Copy the built `ilanaSynth.vst3` to `/Library/Audio/Plug-Ins/VST3/` (or
`~/Library/Audio/Plug-Ins/VST3/`) and rescan in your DAW. The standalone app
is in `build/ilanaSynth_artefacts/Release/Standalone/`; if macOS refuses to
open it, run `xattr -dr com.apple.quarantine <path to ilanaSynth.app>`.

---

## Using it

- **Hover any control** for a description and its value.
- **Right-click a knob** to assign a modulation source, clear modulation,
  reset to default or copy/paste the value.
- **Drag a source chip** from the bottom bar onto a knob to modulate it.
- **Drag a `.wav`** onto an oscillator's waveform display to load it as a
  sample (it switches the oscillator to Sample mode) — or use LOAD to fill a
  user wavetable slot.
- **SAVE** asks for a name, category and tags and stores the preset in
  `Documents/ilanaSynth Presets`; the FOLDER button in the preset browser
  opens that directory. Use the `…` menu to export a preset file anywhere.
- The preset browser (click the preset name) has category chips, search,
  favourites (left-click a row to load, right-click for more).

### Good to know
- The SEQ tab is hidden until you need it: it appears when an LFO shape is
  set to Steps or the MSEG is assigned as a modulation source.
- The FX rack is empty on INIT — click any of the 10 rows to add an
  effect.
- After adding the new factory wavetables, the user wavetable slots moved to
  positions 17–20 in the table list; very old patches that referenced a user
  slot may need the slot reselecting.

### Windows high-DPI displays
If the interface looks soft on a display scaled to 125–200%:
- In Live: **Preferences → Look/Feel → enable HiDPI mode**.
- Right-click the plugin's title bar (or its entry in Live's browser) and
  disable **Auto-Scale Plug-In Window**. Live upscales plugin windows, which
  softens everything; with it off the interface is drawn 1:1.
- Choose a size from the settings (gear) menu, or drag the window's corner, so
  the window is a comfortable size while still rendering at native resolution.

---

## Building from source

Requirements: CMake 3.22+, a C++20 compiler and internet access for JUCE
(fetched automatically).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release      # Windows: cmake -B build
cmake --build build --config Release
```

Targets: `ilanaSynth_VST3`, `ilanaSynth_Standalone`, `ilanaTableTest`
(the offline regression suite), `ilanaSnapshot` and `ilanaFingerprint`.

- `ilanaSnapshot snapshots/out [factory preset index]` renders every tab,
  sub-tab and FX module panel to PNGs without a DAW or display;
  `ilanaSnapshot --uitest` drives the editor and checks its wiring.
- `ilanaFingerprint out.csv` records loudness, brightness and width for every
  factory preset; `python3 tools/compare_fingerprints.py before.csv after.csv`
  flags presets whose sound changed.
- `ctest --test-dir build` runs the regression suite and the UI test.

---

*IlanaSynth v1.0 — Ilana Audio.*
