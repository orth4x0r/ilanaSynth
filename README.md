# IlanaSynth — v0.9

**IlanaSynth, para un sonido más buto.** (for a more brutal sound)

An aggressive wavetable synthesizer for VST3, built with JUCE.

---

## What it is

IlanaSynth is a complete sound design machine: three oscillators with wavetable,
physical-modelling string and full sample modes, two routable filters, five
tension envelopes, four LFOs plus a step sequencer and MSEG, an 8-slot
modulation matrix, a 10-slot effects rack with 28 modules, an eight-mode
arpeggiator and a tuned resonator — wrapped in a hardware-inspired interface
with 80 factory presets.

---

## Features

### Oscillators (3)
- Every oscillator has three modes:
  - **Wavetable** — 16 factory tables (Basic, HardSync, Wavefold, FM Metal,
    Formant, Comb, PWM, DriveSaw, Sine, Triangle, Analog, Vowel, Glass,
    Fractal, Riser, Digital) plus 4 user slots. Load your own `.wav` with the
    LOAD button or by dragging a `.wav` file straight onto the waveform
    display.
  - **String** — Karplus–Strong physical model with excite, decay, damp and
    sustain controls.
  - **Sample** — load any `.wav` (up to 120 s). Tuned or untuned playback,
    loop / one-shot, reverse, start & end points and fades. Five factory
    samples are built in (Metal Hit, Vocal Ah, Sub Tone, Vinyl Loop,
    Noise Rise).
- Unison up to 8 voices per oscillator with detune and stereo spread.
- Per-oscillator level, pan, semitone and fine tuning.
- Chord modes (Octave, Fifth, Power, Major, Minor, Sus4).
- Frame morphing, modulatable like any other control.
- OSC 3 is the sub/third oscillator, with an extra octave switch and a noise
  layer.

### Filters (2)
- Low pass, band pass, high pass, notch — 12 dB or 24 dB per filter.
- Cutoff, resonance, drive, envelope amount, key tracking and audio-rate FM.
- **Serial** (F1 into F2) or **parallel** (both summed) routing, chosen from
  the routing switch with its signal-flow diagrams.

### Envelopes & LFOs
- 5 tension envelopes: AMP, FILTER 1, FILTER 2, MOD and ENV 4.
  - Drag the handles on the graph to shape A / D / S / R, and drag the curve
    (or its midpoint dots) to bend the **tension** — per envelope.
  - Velocity sensitivity on AMP and FILTER 1.
- 4 LFOs with Sine, Triangle, Saw up, Saw down, Square, S&H, Draw and Steps
  shapes. Free rate or host-synced divisions, per-LFO retrigger.
- **Step LFO editors** (two 16-step sequencers) and a **4-stage MSEG** live on
  the SEQ tab, which appears automatically when a step LFO shape is selected
  or the MSEG is assigned in the matrix.
- LFOs and envelopes share one tab, with the LFO row on top and the envelope
  row below.

### Modulation matrix
- 8 slots, 22 sources and 36 destinations, bipolar depth and live source
  meters.
- Sources: LFO 1–4, MOD / FILTER 1 / FILTER 2 / AMP / ENV 4 envelopes, MSEG,
  velocity, key track, random, mod wheel, aftertouch, expression, 4 macros
  and a clocked sample & hold.
- **Drag a source chip onto any knob** to assign it, or **right-click any
  knob** for a quick mod / reset / copy-paste menu.

### Effects rack (10 slots, 28 modules)
Drive (tube / fuzz / clean), Bit Crusher, Amp, Chorus, Phaser, Comb, Flanger,
Dimension, Tremolo, Auto-Pan, Delay, Tape Delay, Multi-Tap delay with a
drawable tap grid, Reverb (Room, Hall, Plate, Shimmer, Spring, Gated and
**user IR loading**), Feedback module, Smear, Freeze, Granular Stutter
(reverse + pitch), Gate, Tape Stop, Tilt EQ, Utility, OTT, Limiter, Stereo
Width, Pitch Shifter, Ring Mod, Octaver and Vowel filter.

- Every slot: bypass, blend, per-slot CPU readout.
- **Solo** a slot (wet only) by clicking its S badge.
- The rack starts empty — right-click a row to choose a module.
- **A/B chains** with copy A to B, so you can compare two effect chains.

### Voice & global
- 16-voice polyphony, MPE mode, voice spread, unison phase randomisation.
- Glide and pitch-bend range, next to the macros in the bottom bar.
- Cross modulation: FM (OSC 2 → OSC 1) with feedback, ring mod, hard sync and
  analogue-style drift.
- Resonator: tuned body after the filters with amount, decay, offset and key
  tracking.
- 2x oversampling for the voice engine (SCOPE tab).
- Soft clipper with clip gain, master volume, 4 macros with MIDI learn.

### Arpeggiator
- 8 modes: Up, Down, UpDown, Random, DownUp, Converge, Walk and Chord.
- 1–4 octaves, host-synced rates, gate length — with a live pattern display
  that steps one bar per note.

### Scope
- Oscilloscope / spectrum analyser, HOLD and PEAK hold, stereo meters with
  click-to-solo, plus a full-screen SCOPE tab.

### Interface
- Hardware-inspired skeuomorphic design: leather body, metal faceplate,
  machined knobs and glass displays.
- 4 colour themes (SKIN button).
- Tooltips on hover, right-click modulation everywhere, undo/redo with a
  history menu, a welcome tour (re-open it with the `?` button).
- Keyboard shortcuts: number keys 1–9 switch tabs.
- 80 factory presets in 8 categories (Bass, Lead, Pluck, Pad, Drone, FX and
  more) with search, favourites and user presets.

---

## Installing

### Windows
1. Run `ilanaSynth-0.9-Windows-Setup.exe` (installs the VST3 and the
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
- User presets are saved in `Documents/ilanaSynth Presets`; the FOLDER button
  in the preset browser opens that directory.
- The preset browser (click the preset name) has category chips, search,
  favourites (left-click a row to load, right-click for more).

### Good to know
- The SEQ tab is hidden until you need it: it appears when an LFO shape is
  set to Steps or the MSEG is assigned as a modulation source.
- The FX rack is empty on INIT — right-click any of the 10 rows to add an
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
- Use the **UI** button in the header to choose a zoom level (100–200%) so the
  window is a comfortable size while still rendering at native resolution.

---

## Building from source

Requirements: CMake 3.22+, a C++20 compiler and internet access for JUCE
(fetched automatically).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release      # Windows: cmake -B build
cmake --build build --config Release
```

Targets: `ilanaSynth_VST3`, `ilanaSynth_Standalone` and `ilanaTableTest`
(the offline regression suite).

---

*IlanaSynth v0.9 — Ilana Audio.*
