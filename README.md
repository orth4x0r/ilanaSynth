# IlanaSynth — v1.1

**IlanaSynth, para un sonido más buto.** (for a more brutal sound)

An aggressive wavetable synthesizer for VST3, built with JUCE.

---

## What it is

IlanaSynth is a complete sound design machine:
- **Oscillators:** three identical oscillators, each with wavetable, physical-modelling string, sample and granular modes, plus a dedicated sub.
- **FM:** a 3-operator FM matrix.
- **Wavetables:** 40 wavetables, with spectral warps that reshape their harmonics.
- **Filters and envelopes:** twelve filter models across two routable filters, and five tension envelopes.
- **Modulation:** four LFOs with chaos shapes, a step sequencer, an MSEG and a 32-slot modulation matrix.
- **Effects:** a 10-slot rack with 29 modules, including a trance gate.
- **Generative tools:** an arpeggiator with scale-random mode, plus note spray and scale snapping.
- **Resonator:** a tuned resonator after the filters.

It all sits in a hardware-inspired interface with 235 factory presets.

---

## What's new in 1.1

- **Filter page redesign**:
  - both filters side by side, each with a type grid and slope switch
  - a live signal-flow diagram
  - a balance control for parallel filters
  - the resonator
  - The XTRA page is gone: its controls live where you'd look for them.
- **MAIN is the hub**:
  - oscillator cards that follow each oscillator's mode
  - F1/F2 filter tabs
  - AMP / FLT 1 / FLT 2 / MOD / ENV 4 envelope tabs
  - LFO cards you can drag straight onto knobs
  - Each card's open button jumps to the full page.
- **OSC 3 is a full oscillator**, identical to OSC 1 and 2. The sub moved to its own section: sine, square or saw, one or two octaves down, with its own level and filter route.
- **FM page**: a 3-operator matrix.
  - Every oscillator can modulate every other oscillator and itself.
  - Three FM styles: Phase, Through-Zero and Exponential.
  - Each operator has an OUT switch, so it can be a silent modulator.
  - There's a draggable operator diagram.
- **40 wavetables in eight categories**, chosen from a visual table browser.
- **Spectral warps**: reshape a wavetable's harmonics per oscillator.
  - Modes: Stretch, Shift, Odd/Even, Formant, Smear and Harmonic Cut.
  - Built in the background, and exact in offline bounces.
- **Granular oscillator mode**: a cloud of grains read from any sample, with position, size, density, spray, pitch spray and stereo spread. The display animates the grains; drag it to move the position.
- **Chaos LFOs**:
  - Smooth Random, Drunk (random walk) and Chaos (Lorenz attractor) shapes.
  - A **KEY** switch runs an LFO per voice at the note's own pitch, for audio-rate modulation.
- **Trance gate** with up to 16 editable steps (level per step), swing, smoothing and host sync.
- **Generative MIDI** (ARP/SEQ page):
  - snap played notes to one of 16 scales
  - **note spray** adds random scale notes around every note you play (count, range, direction, strum spread, chance, velocity randomness)
  - the arp gets a **Scale Random** mode and a **chance** control
- **Envelope and LFO editing**:
  - The ADSR graph follows the mouse exactly.
  - The LFO section has been redesigned.
  - The envelope cards and LFO cards select in place.
- **FX rack**:
  - coloured, scrollable stacked modules (like Vital or Serum)
  - one-click effect buttons in an empty rack
  - an OUTPUT strip with the soft clipper
- **Scope** shows the waveform, spectrum, or both.
- **35 new factory presets** that show off the new features: drums (kick, FM snare, metal hat, granular clap, tom, rim), FM bells and growls, spectral keys and pads, granular choirs and swarms, chaos patches, trance-gated chords, generative patches, and FX.
- Fixed: tape stop no longer leaves latency behind after it's released.

---

## Features

### Oscillators (3, identical)
- Four modes per oscillator:
  - **Wavetable**: 40 factory tables in eight categories (Basic, Analog, Digital, Vocal, Spectral, Harsh, Organic, Chaos), plus 4 user slots.
    - Click the TABLE box for the visual browser.
    - Load your own `.wav` with LOAD (as frames, or resynthesised from any recording), or drag a `.wav` onto the waveform display.
  - **String**: Karplus–Strong physical model with excite, decay, damp and sustain.
  - **Sample**: any `.wav` up to 120 s.
    - Tuned or untuned playback, loop / one-shot, reverse, start and end points, and fades.
    - Five factory samples are built in: Metal Hit, Vocal Ah, Sub Tone, Vinyl Loop and Noise Rise. Right-click the display to pick one.
  - **Granular**: grains from the oscillator's sample (a vocal until you load your own).
    - Controls: POSITION, SIZE (10–500 ms), DENSITY, SPRAY, PITCH RND and STEREO.
- **Spectral warp** (wavetable mode): Stretch, Shift, Odd/Even, Formant, Smear or Harmonic Cut, with an amount. The display shows the warped wave.
- **Warp modes**: Sync, Bend +, Bend −, PWM, Mirror, Asym, Quantize, FM and Ring.
- **Unison** up to 16 voices (8 in string, sample and granular modes), with detune, stereo spread, stack mode (Classic, Hypersaw, Octaves, Fifths) and blend.
- **Per-oscillator controls**: level, pan, semitone and fine tuning, a filter route, and chord modes.
- **SUB**: a dedicated sub oscillator (sine, square or saw, −1 or −2 octaves) plus a noise layer, sharing a filter route.
- **VOICE**: voice spread, unison phase randomisation and analogue drift.

### FM
- A 3×3 matrix: rows modulate columns, and the diagonal is feedback.
- Styles: Phase, Through-Zero or Exponential.
- Each oscillator has an OUT switch; turn it off to make that oscillator a pure modulator.
- The operator diagram:
  - Drag from one oscillator to another to add or remove a route.
  - Drag onto an oscillator itself for feedback.
  - Click an oscillator to toggle its output.
- Ring mod and hard sync (1 > 2).
- Every route is a modulation destination.

### Filters (2)
- 12 models:
  - Low pass, Band pass, High pass and Notch (12 or 24 dB)
  - Ladder LP and Ladder HP
  - Diode LP and MS-20 LP
  - Comb + and Comb −
  - Formant (vowel morph)
  - Morph (LP → BP → HP)
- Controls: cutoff, resonance (self-oscillating at the top), drive, envelope amount, key tracking, audio-rate FM and morph.
- **Routing:**
  - **Serial** (F1 into F2) or **parallel**, with a BALANCE control between the two filters in parallel.
  - Each source can go to Filter 1, Filter 2 or skip the filters: click a source in the signal-flow diagram.
- Drag the markers on the response display to set cutoff and resonance.
- **Resonator**: a tuned body after the filters, with amount, decay, offset and key tracking.

### Envelopes & LFOs
- **5 tension envelopes**: AMP, FILTER 1, FILTER 2, MOD and ENV 4.
  - They're shown as cards that mark the ones in use. Drag a card onto a knob to modulate it.
  - Drag the graph's handles for A / D / S / R, and drag a curve to bend its tension. Double-click a handle to reset it.
- **4 LFOs**:
  - Shapes: Sine, Triangle, Saw up, Saw down, Square, S&H, Draw, Steps, Curve, Smooth Random, Drunk and Chaos.
  - Free rate or host-synced divisions, and a start phase.
  - **RETRIG** runs the LFO per voice.
  - **KEY** runs it per voice at the note's pitch × RATE / 4 Hz.
- **The Curve editor**:
  - Click to add a point, drag to move it, drag a segment to bend it, and double-click a point to delete it.
  - Right-click for presets, grid, flip and reverse.

### ARP/SEQ
- **Step sequencers and MSEG**: two 16-step LFO editors, and a 4-stage looping MSEG with a clocked sample & hold.
- **Arpeggiator**: 9 modes (Up, Down, UpDown, Random, DownUp, Converge, Walk, Chord, Scale Random), 1–4 octaves, host-synced rate, gate and step chance, with a live pattern display.
- **Generate**:
  - Scale and root, with optional snapping of played notes.
  - **Note spray** adds random notes from the scale around each note you play:
    - 1–8 notes, up to 24 semitones away
    - upward, downward or both
    - a strum spread in ms
    - chance and velocity randomness

### Modulation
- **Matrix**: 32 slots, 22 sources and over 200 destinations.
  - Each slot has depth, a response curve, polarity (Natural, Unipolar, Bipolar), a **Via** source that scales the routing, and bypass.
- **Sources**:
  - LFO 1–4 and the MSEG
  - the MOD / FILTER 1 / FILTER 2 / AMP / ENV 4 envelopes
  - velocity, key track and random
  - mod wheel, aftertouch and expression
  - 4 macros and a clocked sample & hold
- **Drag a source chip or card onto any knob** to assign it.
  - Drag the coloured dot next to the knob to set the depth, or double-click the dot to remove the routing.
  - Hovering a source highlights everything it modulates.
- **Right-click any knob** for quick modulation, reset, copy/paste and MIDI learn.
- **Four macros** with editable names (double-click a name), shown in the bottom bar and in the matrix.

### Effects rack (10 slots, 29 modules)
**Modules:**
- **Drive and dynamics**: Drive (tube / fuzz / clean), Bit Crusher, Amp, Compressor, OTT, Limiter, Utility
- **Modulation**: Chorus, Phaser, Flanger, Dimension, Tremolo, Comb
- **Delay and time**: Delay (with tape pitch and wow), Multi-Tap delay with a drawable tap grid, Feedback, Tape Stop
- **Space and texture**: Reverb (Room, Hall, Plate, Shimmer, Spring, Gated, and user IR loading), Smear, Freeze, Granular Stutter (reverse and pitch)
- **Trance Gate**: patterns or up to 16 custom steps, with swing
- **Tone**: Tilt EQ, 3-band parametric EQ
- **Stereo and pitch**: Haas, Stereo Width, Frequency Shifter, Ring Mod, Octaver, Vowel filter

**Rack controls:**
- Modules are coloured by family and stacked in a scrollable view.
- Every slot has bypass, blend, solo and a per-slot CPU readout.
- Drag rows to reorder them; bypass, solo and blend move with the effect.
- An empty rack offers one-click Reverb, Delay, Chorus, Drive, OTT, Trance Gate, Phaser and EQ.
- **A/B chains**, with copy A to B and save/load of whole chains.
- The OUTPUT strip has the soft clipper and clip gain.

### Voice & global
- **Voices**: 16-voice polyphony with a voice limit, and Poly, Mono and Legato modes.
- **Glide and bend**: glide (optionally legato-only, with the LEGATO switch next to it) and pitch-bend range.
- **MPE mode** in the settings menu.
- **Oversampling**: 2x or 4x for the voice engine (SCOPE tab).
- **Output**: master volume, and 4 macros with MIDI learn.

### Scope
- Waveform, spectrum or split view, with HOLD and PEAK hold.
- Stereo meters with click-to-solo.

### Interface
- **Design**: hardware-inspired, with 4 colour themes. Resize it from the corner (75–200%) or from the settings menu.
- **Tabs**: MAIN, OSC, FILTER, ENV/LFO, FM, ARP/SEQ, MATRIX, FX and SCOPE. Number keys 1–9 switch between them.
- **Header**:
  - preset name with category and an EDITED marker
  - previous / next and favourite
  - SAVE
  - undo / redo with history
  - A/B compare
  - DICE: randomise or mutate the patch, or one section of it
  - settings
- **Bottom bar**: the modulation source chips, the four macros, glide, legato, bend, voice mode, voices and master. The on-screen keyboard can be hidden.
- **Help**: tooltips on hover, and a welcome tour (re-open it with the `?` button).
- **Presets**: 235 factory presets in Bass, Lead, Pluck, Pad, Keys, Chords, Arp, Drone, Drums, Generative and FX. The browser has search (names, categories and tags), favourites and user presets.

---

## Installing

### Windows: build and install from source (one click)
Double-click **`build-and-install.cmd`** in the repository folder.

**What it does:**
- Asks for administrator rights and builds the Release VST3 and standalone.
- Copies the VST3 to `C:\Program Files\Common Files\VST3\ilanaSynth.vst3`.
- Puts the standalone in `C:\Program Files\ilanaSynth`, with a desktop shortcut.
- Tells you what went wrong if a tool is missing.

**You need:**
- Visual Studio 2022 (Community or Build Tools) with *Desktop development with C++*
- Git
- internet access the first time (JUCE is downloaded)

Close your DAW first, then rescan plugins once it finishes. To update, pull the changes and run it again:

```powershell
git checkout main
git pull origin main
.\build-and-install.cmd
```

### Windows: installer
1. Run `ilanaSynth-1.1-Windows-Setup.exe`, which installs the VST3 and the standalone. Or copy the `ilanaSynth.vst3` folder to `C:\Program Files\Common Files\VST3\`.
2. Rescan your plugin folder in your DAW.
3. The standalone `ilanaSynth.exe` needs no DAW at all.

### macOS
There is no pre-built macOS binary in this release. Building it yourself takes about ten minutes, and needs internet the first time for JUCE:

```bash
xcode-select --install   # Apple compilers, once
brew install cmake       # or the .pkg from cmake.org
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Copy the built `ilanaSynth.vst3` to `/Library/Audio/Plug-Ins/VST3/` (or `~/Library/Audio/Plug-Ins/VST3/`) and rescan in your DAW.

The standalone app is in `build/ilanaSynth_artefacts/Release/Standalone/`. If macOS refuses to open it, run `xattr -dr com.apple.quarantine <path to ilanaSynth.app>`.

---

## Using it

- **Hover any control** for a description and its value.
- **Right-click a knob** to assign a modulation source, clear modulation, reset to default or copy/paste the value.
- **Drag a source chip or LFO/envelope card** onto a knob to modulate it.
- **Drag a `.wav`** onto an oscillator's waveform display to load it as a sample. This switches the oscillator to Sample mode, or keeps Granular.
- **Click the TABLE box** to browse wavetables visually.
- **SAVE** asks for a name, category and tags, and stores the preset in `Documents/ilanaSynth Presets`.
  - The FOLDER button in the preset browser opens that directory.
  - Use the `…` menu to export a preset file anywhere.
- **The preset browser** (click the preset name) has category chips, search and favourites. Left-click a row to load it; right-click for more.

### Good to know
- The FX rack is empty on INIT: use the quick-add buttons, or click any of the 10 rows.
- Spectral warps work on the factory wavetables.
  - Changing a warp's amount rebuilds the table in the background, so it isn't a modulation target.
  - To move the tone over time, modulate FRAME instead.
- Note spray and scale-random arps are random by design, so each playthrough differs.
- **Old patches:**
  - OSC 3 was a sub-style oscillator before 1.1. Old patches are converted automatically on load, including the user wavetable slots, which moved to positions 41–44.
  - Older patches that used the XTRA page keep their settings; the controls now live on the FILTER, OSC and FM pages.

### Windows high-DPI displays
If the interface looks soft on a display scaled to 125–200%:
- In Live: **Preferences → Look/Feel → enable HiDPI mode**.
- Right-click the plugin's title bar (or its entry in Live's browser) and disable **Auto-Scale Plug-In Window**. Live upscales plugin windows, which softens everything; with it off, the interface is drawn 1:1.
- Choose a size from the settings (gear) menu, or drag the window's corner, so the window is a comfortable size while still rendering at native resolution.

---

## Building from source

**Requirements:** CMake 3.22+, a C++20 compiler, and internet access for JUCE (fetched automatically).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release      # Windows: cmake -B build
cmake --build build --config Release
```

**Targets:**
- `ilanaSynth_VST3` and `ilanaSynth_Standalone`
- `ilanaTableTest`: the offline regression suite
- `ilanaSnapshot` and `ilanaFingerprint`

**Tools:**
- `ilanaSnapshot snapshots/out [factory preset index]` renders every tab, sub-tab and FX module panel to PNGs, without a DAW or display.
- `ilanaSnapshot --uitest` drives the editor and checks its wiring.
- `ilanaFingerprint out.csv` records loudness, brightness and width for every factory preset.
- `python3 tools/compare_fingerprints.py before.csv after.csv` flags presets whose sound changed.
- `ctest --test-dir build` runs the regression suite and the UI test.

---

*IlanaSynth v1.1 — Ilana Audio.*
