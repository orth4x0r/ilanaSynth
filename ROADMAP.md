# ilanaSynth roadmap

This is the plan for what comes after v1.1. **M1–M6b are done. Next up: M7 (Generative card).**

The core of the plan is **one physical modelling engine used in two places**:
- **PHYSICAL oscillator mode**: any oscillator can be a string or other modelled instrument. It replaces String mode, and old String patches migrate to it.
- **BODY section**: an upgrade of the resonator. The whole oscillator mix becomes the exciter, which gives the "weird hybrid" sounds: supersaw-rung sitars, granular-bowed plates.

**Priorities:** weird hybrids and plucked or bowed strings come first, then keys. Deep FM and the generative and physics-modulation work run alongside.

**Platform:** ilanaSynth is built for Windows first. macOS, Linux and CI come at the end of v1.3 (M12b).

**CPU budget:** balanced, meaning a rich physical patch should cost about what a heavy unison patch costs today. A QUALITY setting (Eco / Normal / High), added in M3b, trades detail for CPU.

**Engine:** from M3b on, the synth has up to **6 full oscillators** that are also the FM operators, and a **pool of 16 envelopes** and **16 LFOs**. The later milestones build on that engine.

**Milestone numbers are fixed.** Milestones have moved between releases, but they keep their numbers so HANDOFF and commit messages stay readable.

---

## When a milestone is done

Every milestone ends with the checks below, a commit and a `mN-done` tag.

### Sounds are fitted to a reference before they count as done
Any modelled sound (physical models, bodies, electric pianos, the west-coast voice, the feedback guitar, formants) is fitted to a reference before the milestone is done. It does not count as done because it sounds plausible.
1. **Before building, pick the reference.** Usually this is a recording of the real thing (for example the Iowa MIS grand used for M4). If there is no real instrument, it is a recording of a target sound or a written description of it. Record the reference and its source in HANDOFF.
2. **Pick the measurements and a target error.** Examples are onset spectrum, per-partial decay, noise level, velocity response and register balance, as in `tools/fit_piano.py`.
3. **Fit it.** Render with `ILANA_PRESET_OVERRIDES`, measure with `tools/analyse_note.py`, and search the model's constants. Bake the results into a tuning header, the way `PianoTuning.h` holds the piano's.
4. **Listen halfway, not only at the end.** Render demos (`ILANA_RENDER_DEMO`) for a listening round in the middle of the milestone.
5. **Done means** the fitted error is under the target and the listening round has passed. If the error can't reach the target, write down the remaining gap rather than calling it done.

The fitting tool should become general (`tools/fit_model.py`, with the piano as its first user) when M4b or M8 starts.

### Checks for every milestone
- **Tuning**: the tuning test. Stretch tuning is accounted for.
- **Stability**: extreme settings with maximum coupling and feedback must never produce NaN or runaway output.
- **CPU**: per-quality limits in the heavy-preset CPU test.
- **Old patches**: migration tests (String → PHYSICAL, 3 oscillators → 6, the five named envelopes → the envelope pool, FM 3×3 → 6×6, Resonator → Classic body), and preset fingerprints to confirm the original presets still sound the same.
- **UI**: tests for each new page or card.
- **Shipping**: each milestone builds and ships on its own.

---

## v1.2: Physical engine, keys, deep FM

### M1: String core and PHYSICAL mode — done
- A stiff-string model. Stiffness pushes the overtones slightly sharp, which is what gives a piano its shimmer.
- Controls: damping, pickup position and excitation position.
- A pluck exciter, with pick hardness, pick position and a "slap" option.
- The new PHYSICAL oscillator mode. Old String patches migrate to it and sound the same.

### M2: Physics LFOs — done
New LFO shapes, reusing the per-voice chaos-LFO system:
- **Bounce**: a dropped ball whose bounces speed up as it settles (height, bounciness).
- **Pendulum**: a swing that slowly loses energy, with an optional kick on each note.
- **Spring**: an overshooting wobble (stiffness, damping).
- **Friction**: sudden stick-slip jumps.

### M3: Bow, buzz and sympathetic strings — done
- **Bow exciter**: a friction model with pressure and speed. MPE pressure and aftertouch map to bow pressure.
- **Sitar bridge buzz**, and a **fret rattle** that gets stronger with velocity.
- **Sympathetic strings**: up to 6 drone strings. They follow the GENERATE scale, with a manual override, and are shared across voices to save CPU.

### M3b: Oscillator engine (6 oscillators, envelope pool) — done
- **Up to 6 oscillators**, each with every oscillator mode. OSC 4–6 default to off, and an oscillator that is off costs no CPU. Existing parameter IDs stay (`osc1_*`, `osc2_*`, and `sub_*` for OSC 3).
- **Routing**: each oscillator goes to Filter 1, Filter 2, both filters, or no filter.
- **FM between any oscillators**: a 6×6 matrix.
- **Envelope pool**: 16 envelopes. The existing five keep their parameter IDs. Each oscillator picks its amp envelope, and a voice ends only when every one of them has finished.
- **QUALITY setting** (Eco / Normal / High).
- **UI**: Phase Plant-style modules, three shown by default with a "+" to add more.

### M4: Acoustic keys — done
- **LFO pool**: 16 LFOs; LFO 5–16 render only when routed.
- **Bow rebuilt** as a two-waveguide stick-slip friction model.
- **Hammer exciter**, 1–3 coupled strings per note, soundboard, sustain pedal resonance, stretch tuning, register map and mechanical noises.
- **Fitted** to the Iowa MIS grand recordings (E1, C4, C7 at mf and ff). The fit error went from 632 to 90.7. It is still not a convincing piano, so the presets are named as hammered-string instruments rather than pianos.

### M5: Deep FM — done
Fitted to an ideal phase-modulation renderer (EP, bell and bass patches: mean partial error 0.006–0.017 dB) and to Bessel sideband levels (0.02 dB). Listening round passed (2026-09-26).
- **Operators are the M3b oscillators**: the 6×6 FM matrix from M3b, plus a noise operator. Old patches map onto operators 1–3.
- **Tuning**: each operator has a frequency ratio that snaps to harmonic, inharmonic or bell sets, or a fixed frequency in Hz.
- **Per-operator envelopes** from the M3b envelope pool (DAHDSR, or an MSEG), plus key-scaling of level and rate.
- **16 algorithm presets**: one-click routings in the operator diagram, all still editable.
- **Non-sine operators**: already true from M3b, since every operator is a full oscillator.
- **Feedback types**: plain, filtered, and cross-feedback between two operators.
- **Reference**: classic DX-style patches (electric piano, bell, bass), matched by ear and spectrum as a check that the operators and envelopes behave as expected.

### M6: Phase distortion (CZ-style) — done
The waves match the CZ formulas to within float precision. The resonant waves use a zero-DC windowed sine (the CZ's spectrum shape, without its DC offset). Listening round passed (2026-09-26).
- **New warp modes**: PD Saw, PD Square, PD Pulse, and PD Resonance I, II and III.
- **PD envelope**: an optional per-oscillator envelope on warp depth, like the CZ's DCW.
- **PD chain**: two phase-distortion stages in series.
- **Reference**: the CZ waveforms, compared by waveform and spectrum.

### M6b: Modulation depth (64-slot matrix) — done
This comes at the end of v1.2. It is cheap, and the 16 envelopes and 16 LFOs already outgrow 32 slots.
- **The matrix grows from 32 to 64 slots.** Hosts need a fixed parameter list, so the extra slots are always declared.
- **Old patches**: slots 1–32 keep their parameter IDs, and slots 33–64 default to empty, so nothing changes in old presets.
- **UI**: the matrix page scrolls, and empty slots stay cheap to draw.
- **CPU**: an empty slot costs nothing; the modulation test gets a full-matrix case.

---

## v1.3: Bodies, wavetable editor, west coast, generative tools

### M7: Generative card
- **Euclidean rhythm**: steps, hits and rotation. It can trigger notes, the physical exciter or the trance gate.
- **Probability sequencer**: each of 16 steps has a chance, a pitch range and a ratchet, all snapped to the scale.
- **Strum mode** for note spray: up or down, with timing.

### M8: BODY section
- **Material bodies**: bar, plate, bell and shell. One MATERIAL knob morphs wood → metal → glass, with SIZE and DECAY.
- **Coupling**: a string drives a body, a body rings a string, or two strings ring each other.
- **The oscillator mix is the exciter.**
- **Old patches**: the current resonator becomes the "Classic" body, so old patches sound the same.
- **Reference**: struck recordings of a wooden bar (marimba), a metal plate, a bell and a glass. Fitted on partial ratios and decay per partial.

### M4b: Electric pianos
Moved from v1.2 so it comes after M8: a tine and its tone bar are a struck bar, which M8 builds. It keeps its number.
- **Tine** (Rhodes-style): a struck tine and tone bar. Pickup position and distance set how hard it barks.
- **Reed** (Wurlitzer-style): a struck reed with a pickup, whose growl feeds into the drives and amps.
- **Reference**: Rhodes and Wurlitzer recordings, soft and hard, low, middle and high. Fitted like the piano, with the bark and growl measured as the change in upper partials from soft to hard.

### M8b: Wavetable editor
The biggest gap against Vital and Serum 2, which both let you build tables in the plugin.
- **Frame editor** on the OSC page: a frame list, with add, duplicate, delete and reorder.
- **Draw mode**: draw a single-cycle wave with the mouse, with smoothing and snap options.
- **Spectrum mode**: edit each frame's harmonic amplitudes and phases as bars, using the existing spectral code (`SpectralWarp`, `SpectralCache`).
- **Generate**: additive presets, formula input, and morph or interpolate between frames.
- **Import**: today's resynthesis from a recording opens in the editor, so you can tidy it.
- **Export** as `.wav` that Vital and Serum can read (2048-sample frames with a `clm` chunk). Loading tables made in Vital or Serum works the same way.
- **Old patches**: unchanged, since factory tables stay read-only.

#### How edited wavetables are stored
Today a patch stores only each user slot's file path and load mode, so a patch loses its table if the `.wav` moves or the patch goes to another computer. The new format keeps every table with the patch, and allows three ways to describe it. A table can have any of them; the loader uses the best one it finds.

- **Recipe**: the editor's steps (drawn points, harmonic bars, formula, morphs, resynthesis settings). It is small and rebuilds the table exactly, and it is what makes the table editable again after reloading.
- **Embedded data**: the rendered frames, stored as 16-bit and compressed inside the patch. This is always written for tables that came from a file, and for any table whose recipe can't rebuild it exactly.
- **File reference**: the original path, kept as a hint so a re-edited source file can be reloaded.

The loader tries recipe first, then embedded data, then the file. If none works, it falls back to the default table and shows a notice.

Other rules:
- **Each table is a versioned `<Wavetable>` child** of the patch state, with a format version, so later fields can be added without breaking older ones.
- **Frames**: 1–256 frames. Frames of any length are resampled to 2048 samples on load.
- **Size**: a table of 256 frames is about 1 MB at 16 bits before compression. The editor warns above that. Tables are shared between the oscillators that use them, so each is stored once.
- **Slots**: the wavetable choice list is fixed for hosts, so M8b reserves **16 patch-table choices** once (replacing today's 4 user slots). Old patches' slots 1–4 map onto the first four.
- **Old patches**: path-only slots load as today, and are embedded the next time the patch is saved.
- **User library**: tables saved on their own go to `Documents/ilanaSynth Wavetables` as `.wav` plus a small `.ilwt` sidecar holding the recipe.
- **Later**: M16 (resample to oscillator) will write into the same format.

### M9: Audio input (two plugins)
This ships as two separate plugins, which is simpler technically:
- **ilanaSynth**: the instrument, unchanged.
- **ilanaSynth FX**: an effect plugin that takes audio input. The input can:
  - excite the BODY section (M8)
  - feed the strings and the feedback guitar
  - be the granular source, for live granulation
  - act as a "live oscillator" going into the filters, FM and effects

The FX plugin adds input gain, a gate/trigger, and an envelope follower as a new modulation source. Both plugins share the engine and the preset format.

### M10: West-coast voice
- **Wavefolder**: fold amount, symmetry, and 1–4 stages.
- **Low-pass gate**: a filter and amplifier in one, modelled on a vactrol, with its natural "bongo" decay.
- **How to play it**: strike it with the M2 Bounce LFO or any envelope.
- **Where it lives**: a WEST card on the FILTER page, used in place of Filter 2 or alongside it.
- **Reference**: recordings of a Buchla-style low-pass gate being struck, fitted on decay time and how brightness falls with level.

### M10b: Filter models
Grouped with M10 because the west-coast filter shares the filter code. Filter variety is 12 models today against 60+ in Serum 2, so the goal is a worthwhile jump, not parity.
- **About 12–16 new models**, in the same FilterUnit structure: more ladder and diode variants, a state-variable multimode, an OTA/Sallen-Key style, an analogue-style notch/phaser filter, comb and formant variants, and a vowel/talking filter.
- **Existing models keep their indices**, so old patches sound the same; new models are appended.
- **UI**: the type grid gets categories or a scrolling list so it stays readable.
- **Checks**: every new model gets the stability test (self-oscillation, extreme drive) and a fingerprint entry. Analogue-style models are checked against measured responses of the circuit they copy.

### M11: Feedback guitar and Evolve
- **Feedback guitar**: a new exciter type that puts an amp and speaker inside the string's feedback loop, with FEEDBACK and GAIN controls.
  - **Reference**: recordings of guitar feedback (a sustained note blooming into its harmonic), fitted on how fast it blooms and which harmonic it settles on.
- **Evolve**: each macro can drift slowly within a range and at a rate you set. A **freeze** button captures the current state.

### M12: Polish
- A PHYSICAL page with an animated string, body and hammer view.
- Tour and README updates.

### M12b: Portability and release quality
Left until the end on purpose: ilanaSynth is built for Windows first. Today it is a Windows VST3 plus standalone, with macOS as build-it-yourself.
- **macOS**: a signed and notarised pre-built VST3 and standalone.
- **AU** (macOS) and **CLAP** on the same JUCE build.
- **Linux**: VST3 and standalone builds.
- **CI**: builds and the regression suite (`ilanaTableTest`, the UI test, fingerprints) on Windows, macOS and Linux for every change.
- **Bug-fix pass**: run the stability, CPU and old-patch tests across hosts before each release.

### M17: Preset pack
Moved from v1.4 so v1.3 ships with presets for everything in it. All new presets wait until here, so they can use every new feature. Planned (about 100, with a stretch goal of 300+ in the library to close the gap with Vital and Serum 2):

- **Wavetable library**: grow from 40 to about 120 factory tables, made with the M8b editor.
- **Piano quality**: one more fitting and listening pass on the acoustic keys (M4), with a fixed time limit. It stops there even if it still isn't a convincing piano.
- **Physical instruments**
  - Strings (M1/M3): 10
  - Oscillator engine layers (M3b): 8
  - Physics LFOs (M2): 6
  - Acoustic keys (M4): 8
  - Electric pianos (M4b): 6
- **Synthesis**
  - Deep FM (M5): 12
  - Phase distortion (M6): 8
- **Generative and bodies**
  - Generative (M7): 8
  - Bodies and hybrids (M8): 12
  - West coast and new filters (M10/M10b): 10
  - Feedback guitar and Evolve (M11): 8
- **FX plugin (M9)**: 10 presets

---

## Later (v1.4, pushed back, not scheduled)

These are ideas to come back to after v1.3 ships. None of them block anything above. Each still follows the sound-fitting rule, and each gets its own presets when it lands.

### M13: Pulsar synthesis
- **New oscillator mode**: trains of short "pulsaret" wave packets, whose rate sweeps from rhythm up to pitch.
- **Controls**: the pulsaret shape comes from any wavetable, with a duty-cycle control.
- **Masking**: individual pulses can be dropped, driven by the M7 sequencers.

### M14: Vector synthesis
- **XY pad**: morphs between four sources (any four of the M3b oscillators, or four snapshots).
- **Movement**: the pad follows a drawable path, the Evolve drift, or joystick and MPE control.

### M15: FOF formant synthesis
- **New oscillator mode**: real vowel synthesis with 5 formants.
- **Controls**: vowel morph (A-E-I-O-U), consonant-style attacks, gender/size, and a choir spread.

### M16: Resample to oscillator
- Bounce a note or the whole patch into a sample or wavetable slot, using the M8b storage format.
- Then granulate it, warp it, or use it as a pulsar source or exciter.
- The result is saved with the patch.

---

## Decisions made

| Question | Decision |
|---|---|
| How the physical engine fits in | Both: an oscillator mode **and** the BODY section |
| Electric pianos | Their own milestone, M4b, in v1.3 after the M8 bodies |
| Audio input | Two plugins (instrument and FX) |
| Evolve | Per macro |
| Feedback guitar | An exciter type |
| Sympathetic strings | Follow the GENERATE scale, with a manual override |
| Presets | Deferred to M17, at the end of v1.3 |
| Oscillator count | 6 full oscillators (M3b), which are also the FM operators |
| Envelopes | A pool of 16 automatable envelopes, shown as they are used; a fixed count because hosts need a fixed parameter list |
| Per-oscillator amp | An envelope picker per oscillator, defaulting to the main AMP |
| Order | M3b before M4 and M5 |
| When a sound is done | When it is fitted to a reference and has passed a listening round, not when it sounds plausible |
| Platforms | Windows first. macOS, Linux and CI wait for M12b |
| v1.4 | Pushed back and unscheduled. v1.3 is the next full release |
| Edited wavetables | Stored in the patch as recipe, embedded data and file reference, in a versioned format; 16 patch-table slots |
| Milestone numbers | Fixed, even when a milestone moves to another release |
