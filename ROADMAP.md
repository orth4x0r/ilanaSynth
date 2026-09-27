# ilanaSynth roadmap

This is the plan for what comes after v1.1. **v1.2 (M1–M6b) is done, and so is M7 (M7.0–M7.5: follow-ups, the Generative card, the BODY section, electric pianos, the wavetable editor and ilanaSynth FX). Next up: real chaos and physics modulators (M8.1).**

The core of the plan is **one physical modelling engine used in two places**:
- **PHYSICAL oscillator mode**: any oscillator can be a string or other modelled instrument. It replaces String mode, and old String patches migrate to it.
- **BODY section**: an upgrade of the resonator. The whole oscillator mix becomes the exciter, which gives the "weird hybrid" sounds: supersaw-rung sitars, granular-bowed plates.

**Priorities:** weird hybrids and plucked or bowed strings come first, then keys. Deep FM and the generative and physics-modulation work run alongside.

**Platform:** ilanaSynth is built for Windows first. macOS, Linux and CI come at the end of v1.3 (M9).

**CPU budget:** balanced, meaning a rich physical patch should cost about what a heavy unison patch costs today. A QUALITY setting (Eco / Normal / High), added in M3b, trades detail for CPU.

**Engine:** from M3b on, the synth has up to **6 full oscillators** that are also the FM operators, and a **pool of 16 envelopes** and **16 LFOs**. The later milestones build on that engine.

**Milestone numbers:** finished milestones keep their numbers. The plan after v1.2 was regrouped on 2026-09-26 into M7–M10 (v1.3) and M11–M14 (v1.4), and again on 2026-09-27: M8 gained the modulator rework, the vector pad and resampling, and v1.4 shrank to two milestones. Each part notes its old number.

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

The fitting tool should become general (`tools/fit_model.py`, with the piano as its first user) when M7.2 (bodies) starts.

**Where references come from** (decided 2026-09-26: the user should not have to do anything):
- **Internet first**, from sources that need no login. University of Iowa MIS has marimba, xylophone, vibraphone, crotales and bells (M7.2), as well as the piano already used.
- **Installed plugins** only where their default (init) sound is the reference, rendered by a small headless JUCE host tool. This applies to Arturia Stage-73 V2 (Rhodes) and Wurli V2 (Wurlitzer) for M7.3. Picking specific presets needs the user, so it's avoided.
- The per-milestone plan: M7.2 bodies from Iowa (search online for glass); M7.3 electric pianos from Stage-73 / Wurli defaults; M8.3 low-pass gate from online recordings (Buchla Easel V's default as a fallback); M8.5 feedback guitar from online recordings; M8.2 piano from the Iowa notes in `build/reference/` (the whole keyboard at pp, mf and ff); M8.1 modulators from their equations.
- References rendered by a model (Stage-73, Wurli) are acceptable.

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

Regrouped on 2026-09-26 into four milestones. Each part notes its old number, so older notes stay traceable.

### M7: Instruments and input
Generative tools, the BODY section, electric pianos, the wavetable editor and audio input. Each part is fitted, tested and listened to before the next starts; the M7 tag waits for all six.

#### M7.0: Follow-ups from the 1.2 polish pass — done
Left over from the bug and polish pass on 2026-09-26 (see the end of `docs/HANDOFF-HISTORY.md`).
- **FM modulation for OSC 4–6**: the mod matrix can modulate only the nine original FM cells. Add destinations for the 27 cells to or from OSC 4–6, appended after the current list (never renumbered), and apply them per voice like the original nine. Old presets must fingerprint unchanged.
- **Filter type buttons**: group the 12 types by family (for example classic, character, comb/formant/morph) instead of one flat grid, in both filter panels. Show the user the layout options first; it's their call.
- **Empty MATRIX and FX pages**: use the space below the empty-state card (MATRIX) and the quick-add buttons (FX), for example a ghost grid of the 64 slots or more quick-add effects. Ask the user which layout they want.
- **Envelope and LFO pool cards**: show what each envelope or LFO modulates, as a small subtitle, and dim the unassigned ones.
- **Number keys 1–9**: they switch tabs while the editor has focus. Check in Ableton whether this gets in the way (Live's computer keyboard doesn't use the number row); if it does, move the shortcut to Ctrl+1–9.

#### M7.1: Generative card (was M7) — done
- **Euclidean rhythm**: steps, hits and rotation. It can trigger notes, the physical exciter or the trance gate.
- **Probability sequencer**: each of 16 steps has a chance, a pitch range and a ratchet, all snapped to the scale.
- **Strum mode** for note spray: up or down, with timing.

#### M7.2: BODY section (was M8) — done
- **Material bodies**: bar, plate, bell and shell. One MATERIAL knob morphs wood → metal → glass, with SIZE and DECAY.
- **Coupling**: a string drives a body, a body rings a string, or two strings ring each other.
- **The oscillator mix is the exciter.**
- **Old patches**: the current resonator becomes the "Classic" body, so old patches sound the same.
- **Reference**: struck recordings of a wooden bar (marimba), a metal plate, a bell and a glass. Fitted on partial ratios and decay per partial.

#### M7.3: Electric pianos (was M4b) — done
It comes after the bodies: a tine and its tone bar are a struck bar, which M7.2 builds.
- **Tine** (Rhodes-style): a struck tine and tone bar. Pickup position and distance set how hard it barks.
- **Reed** (Wurlitzer-style): a struck reed with a pickup, whose growl feeds into the drives and amps.
- **Reference**: Rhodes and Wurlitzer recordings, soft and hard, low, middle and high. Fitted like the piano, with the bark and growl measured as the change in upper partials from soft to hard.
- **Result:** fitted to Stage-73 V2 and Wurli V2 defaults (tine error 2499 → 124, reed 1677 → 135; `tools/fit_ep.py`). Listening round passed (2026-09-27), together with the M7.5 FX demos.

#### M7.4: Wavetable editor (was M8b) — done
The biggest gap against Vital and Serum 2, which both let you build tables in the plugin.
- **Frame editor** on the OSC page: a frame list, with add, duplicate, delete and reorder.
- **Draw mode**: draw a single-cycle wave with the mouse, with smoothing and snap options.
- **Spectrum mode**: edit each frame's harmonic amplitudes and phases as bars, using the existing spectral code (`SpectralWarp`, `SpectralCache`).
- **Generate**: additive presets, formula input, and morph or interpolate between frames.
- **Import**: today's resynthesis from a recording opens in the editor, so you can tidy it.
- **Export** as `.wav` that Vital and Serum can read (2048-sample frames with a `clm` chunk). Loading tables made in Vital or Serum works the same way.
- **Old patches**: unchanged, since factory tables stay read-only.

##### How edited wavetables are stored
Today a patch stores only each user slot's file path and load mode, so a patch loses its table if the `.wav` moves or the patch goes to another computer. The new format keeps every table with the patch, and allows three ways to describe it. A table can have any of them; the loader uses the best one it finds.

- **Recipe**: the editor's steps (drawn points, harmonic bars, formula, morphs, resynthesis settings). It is small and rebuilds the table exactly, and it is what makes the table editable again after reloading.
- **Embedded data**: the rendered frames, stored as 16-bit and compressed inside the patch. This is always written for tables that came from a file, and for any table whose recipe can't rebuild it exactly.
- **File reference**: the original path, kept as a hint so a re-edited source file can be reloaded.

The loader tries recipe first, then embedded data, then the file. If none works, it falls back to the default table and shows a notice.

Other rules:
- **Each table is a versioned `<Wavetable>` child** of the patch state, with a format version, so later fields can be added without breaking older ones.
- **Frames**: 1–256 frames. Frames of any length are resampled to 2048 samples on load.
- **Size**: a table of 256 frames is about 1 MB at 16 bits before compression. The editor warns above that. Tables are shared between the oscillators that use them, so each is stored once.
- **Slots**: the wavetable choice list is fixed for hosts, so M7.4 reserves **16 patch-table choices** once (replacing today's 4 user slots). Old patches' slots 1–4 map onto the first four.
- **Old patches**: path-only slots load as today, and are embedded the next time the patch is saved.
- **User library**: tables saved on their own go to `Documents/ilanaSynth Wavetables` as `.wav` plus a small `.ilwt` sidecar holding the recipe.
- **Later**: M14 (resample to oscillator) will write into the same format.

#### M7.5: Audio input, two plugins (was M9) — done
This ships as two separate plugins, which is simpler technically:
- **ilanaSynth**: the instrument, unchanged.
- **ilanaSynth FX**: an effect plugin that takes audio input. The input can:
  - excite the BODY section (M7.2)
  - feed the strings and the feedback guitar
  - be the granular source, for live granulation
  - act as a "live oscillator" going into the filters, FM and effects

The FX plugin adds input gain, a gate/trigger, and an envelope follower as a new modulation source. Both plugins share the engine and the preset format.

### M8: Modulators, piano, west coast, filters, feedback and polish
Real chaos and physics modulators, a piano rework, the west-coast voice and new filter models, the feedback guitar with Evolve and the vector pad, resampling, then the v1.3 polish.

#### M8.1: Chaos and physics modulators (new, redoes M2)
The M2 shapes are stand-ins: the physics shapes share two generic knobs (Physics A/B), Bounce relaunches itself when it settles, the Lorenz shape has fixed constants and only outputs X, and S&H and Steps have no smoothing. M8.1 makes them real simulations with real parameters, at least as deep as Vital's random LFOs.
- **Smoothing on every LFO**: a SMOOTH (glide) control, as a time or a fraction of a cycle, on every shape. It is what makes S&H, Steps and Square usable as slewed random or glide.
- **Random family** (Vital's four styles and more): S&H, Sine Interpolate, **Perlin** (with octaves), Drunk walk (step size), each with a STEREO offset, a seed, and per-voice or shared randomness.
- **Chaotic systems**, solved with a proper integrator (RK4, with sub-steps at high rates), their constants exposed:
  - **Lorenz**: sigma, rho, beta, and the output axis (X, Y, Z or a mix)
  - **Rossler**: a, b, c
  - **Duffing** (driven): drive and damping, which walk from a steady swing through period doubling into chaos
  - **Logistic and Henon maps**: stepped at the rate, with r morphing from a fixed value through the period-doubling cascade into chaos
  - **Double pendulum**: two arm lengths, masses and start angles
  - RATE sets the time scale; outputs are scaled to each attractor's known range so they fill -1 to +1 without clipping.
- **Physics objects with real parameters** (replacing Physics A/B):
  - **Bounce**: gravity, drop height, elasticity (restitution), air drag. It settles and stays down until retriggered, or loops if you choose. Outputs: height, and an **impact** pulse at each bounce (for striking the M8.3 low-pass gate).
  - **Pendulum**: length, gravity, damping, start angle, and a periodic drive.
  - **Spring**: mass, stiffness, damping, rest point; plucked by a trigger.
  - **Friction**: a real stick-slip model (a mass on a spring dragged over a surface) with static and sliding friction and drag speed.
- **Triggers**: note on, host sync, a Generative step (Euclid or the sequencer), free-running, or a manual button.
- **Two outputs per modulator** (for example X/Y, or height/impact), as mod sources appended to the list.
- **UI**: the LFO card draws the simulation (the ball, the pendulum, the attractor's trail) and shows named knobs for the chosen shape.
- **Old patches**: the current Chaos, Bounce, Pendulum, Spring and Friction keep their indices and behaviour (shown as "classic" versions); the new ones are appended, so fingerprints don't move.
- **Reference**: the equations themselves. Bounce times and heights match the analytic series, pendulum periods match length and gravity, Lorenz shows its known Lyapunov exponent (about 0.9) and bounds, and the random styles are compared with Vital's by ear.
- **Checks**: no NaN or runaway at any setting, and CPU with all 16 LFOs chaotic and per voice.

#### M8.2: Piano rework (new; replaces M10's piano pass)
M4's piano fits its measurements (error 632 to 90.7) but sounds closer to a harpsichord than a piano. The fit only covered three notes at two dynamics, and the model has structural gaps that no fitting can close:
- **The hammer is a pre-shaped force pulse, not a hammer.** A real felt hammer is a mass on a stiffening spring (force rising as compression to a power of about 2.5–3.5) that stays in contact with the moving string and is thrown back by it. That interaction is what makes soft notes dark and round and loud notes bright, with a smooth, even roll-off. The pulse is also high-passed before it reaches the string, which thins the fundamental: a thin, bright, even spectrum is the harpsichord sound.
- **The soundboard is a handful of modes.** A real board is thousands of dense modes that colour the attack and give the "wood" in the sound. Commuted synthesis (a measured soundboard response folded into the excitation) is the usual way to get it cheaply.
- **No bass "bark"**: the longitudinal and phantom partials from tension modulation, a large part of what makes a low piano note sound like a piano.
- **Loss by register** is two constants (DAMP and a register slope), not fitted note by note.

Plan:
- **Diagnose first**: render the full Iowa MIS set (every note at pp, mf and ff) and compare spectrograms and partial decays with the recordings, alongside A/B listening. Write down which gap costs the most before building.
- **A real hammer**: a nonlinear felt model (mass, stiffness, exponent, hysteresis) coupled to the string at the strike point, sample by sample. The strike-position notches and multiple contacts in the treble then come out of the physics.
- **Strings**: 2–3 per note with their own detuning and two polarisations each (beating and the double decay), and a loss filter fitted per register.
- **Tension modulation** for the bass bark and phantom partials.
- **Soundboard**: commuted synthesis, or a dense modal or FDN body, fitted to a soundboard response taken from the recordings.
- **The release**: the damper sound and the key-off decay.
- **Fitted on the whole keyboard at three dynamics**, with a metric that follows the ear: spectral envelope over time, per-partial decay curves and beating. **Done** means the listening round says "piano", not only that the error dropped.
- **Time limit**: a fixed budget. If it still isn't convincing, write down the gap and stop.
- **Old patches**: the current hammer stays as "Hammer (classic)" for existing presets; the new piano hammer is appended.
- **CPU**: a piano note will cost more (2–3 coupled strings and a hammer solved per sample); Eco keeps a lighter version.

#### M8.3: West-coast voice (was M8.1, before that M10)
- **Wavefolder**: fold amount, symmetry, and 1–4 stages.
- **Low-pass gate**: a filter and amplifier in one, modelled on a vactrol, with its natural "bongo" decay.
- **How to play it**: strike it with the M8.1 Bounce impacts or any envelope.
- **Where it lives**: a WEST card on the FILTER page, used in place of Filter 2 or alongside it.
- **Reference**: recordings of a Buchla-style low-pass gate being struck, fitted on decay time and how brightness falls with level.

#### M8.4: Filter models (was M8.2, before that M10b)
Grouped with M8.3 because the west-coast filter shares the filter code. Filter variety is 12 models today against 60+ in Serum 2, so the goal is a worthwhile jump, not parity.
- **About 12–16 new models**, in the same FilterUnit structure: more ladder and diode variants, a state-variable multimode, an OTA/Sallen-Key style, an analogue-style notch/phaser filter, comb and formant variants, and a vowel/talking filter.
- **Existing models keep their indices**, so old patches sound the same; new models are appended.
- **UI**: the type grid gets categories or a scrolling list so it stays readable.
- **Checks**: every new model gets the stability test (self-oscillation, extreme drive) and a fingerprint entry. Analogue-style models are checked against measured responses of the circuit they copy.

#### M8.5: Feedback guitar, Evolve and the vector pad (was M8.3, plus the simple part of M12)
- **Feedback guitar**: a new exciter type that puts an amp and speaker inside the string's feedback loop, with FEEDBACK and GAIN controls.
  - **Reference**: recordings of guitar feedback (a sustained note blooming into its harmonic), fitted on how fast it blooms and which harmonic it settles on.
- **Evolve**: each macro can drift slowly within a range and at a rate you set. A **freeze** button captures the current state.
- **Vector pad**: an XY pad that mixes any four of the six oscillators (the corners), plus X and Y as mod sources. It moves by hand, by a drawn path (a two-dimensional MSEG), by Evolve drift, or by MPE and joystick. Mostly UI: the DSP is four level controls. Morphing whole-patch snapshots stays in v1.4 (M12).

#### M8.6: Resample to oscillator (was M14, before that M16)
- **BOUNCE**: render a note, or the whole patch, into a patch table or sample slot, in the background.
- It reuses the offline note rendering the fitting tools already use, the M7.4 storage format, and resynthesis-to-wavetable.
- Then granulate it, warp it, or use it as an exciter; the result is saved with the patch.
- **New work**: samples embedded in the patch the way tables already are.

#### M8.7: Polish (was M8.4, before that M12)
- A PHYSICAL page with an animated string, body and hammer view.
- Tour and README updates.

### M9: Portability and release quality (was M12b)
Left until the end on purpose: ilanaSynth is built for Windows first. Today it is a Windows VST3 plus standalone, with macOS as build-it-yourself.
- **macOS**: a signed and notarised pre-built VST3 and standalone.
- **AU** (macOS) and **CLAP** on the same JUCE build.
- **Linux**: VST3 and standalone builds.
- **CI**: builds and the regression suite (`ilanaTableTest`, the UI test, fingerprints) on Windows, macOS and Linux for every change.
- **Bug-fix pass**: run the stability, CPU and old-patch tests across hosts before each release.

### M10: Preset pack (was M17)
Moved from v1.4 so v1.3 ships with presets for everything in it. All new presets wait until here, so they can use every new feature. Planned (about 100, with a stretch goal of 300+ in the library to close the gap with Vital and Serum 2):

- **Wavetable library**: grow from 40 to about 120 factory tables, made with the M7.4 editor.
- **Physical instruments**
  - Strings (M1/M3): 10
  - Oscillator engine layers (M3b): 8
  - Chaos and physics modulators (M8.1, M2): 8
  - Acoustic keys (M4, M8.2): 8
  - Electric pianos (M7.3): 6
- **Synthesis**
  - Deep FM (M5): 12
  - Phase distortion (M6): 8
- **Generative and bodies**
  - Generative (M7.1): 8
  - Bodies and hybrids (M7.2): 12
  - West coast and new filters (M8.3/M8.4): 10
  - Feedback guitar, Evolve and vector pad (M8.5): 8
  - Resampled sources (M8.6): 6
- **FX plugin (M7.5)**: 10 presets

---

## Later (v1.4, pushed back, not scheduled)

These are ideas to come back to after v1.3 ships. None of them block anything above. Each still follows the sound-fitting rule, and each gets its own presets when it lands.

### M11: Pulse-train oscillator: pulsar and FOF (was M11 and M13; before that M13 and M15)
One engine, because a FOF grain is a pulsar whose pulse is a decaying sine. It reuses the grain scheduler in `GranularOsc`, wavetable reading, and the unison bank.
- **New oscillator mode**: trains of short pulses at a rate that sweeps from rhythm up to pitch, band-limited at audio rates.
- **Pulse shape**: any wavetable with a duty-cycle control (pulsar), or a decaying sine with attack and bandwidth (FOF).
- **Masking**: individual pulses can be dropped, driven by the M7.1 sequencers.
- **Vowels (FOF)**: 5 formants with bandwidths, the vowel morph (A-E-I-O-U), male, female and child sets (the 3-formant table in `FilterUnit.h` is a start), consonant-style noise attacks, and a choir spread.
- **Reference**: recordings of sung vowels, fitted on formant positions and bandwidths. This is where the real risk is: like the acoustic piano, it may build easily and still not sound convincing.

### M12: Snapshot vector synthesis (was the rest of M12, before that M14)
- The M8.5 vector pad morphs between four whole-patch **snapshots** instead of four oscillators.
- **The hard part**: switches and mode choices can't blend, so each one needs a rule (switch at the midpoint, or keep the nearest corner's value).

M13 and M14 no longer exist: FOF merged into M11, and resampling moved to M8.6.

---

## Decisions made

| Question | Decision |
|---|---|
| How the physical engine fits in | Both: an oscillator mode **and** the BODY section |
| Electric pianos | Part of M7 (M7.3), after the bodies (M7.2) |
| Audio input | Two plugins (instrument and FX) |
| Evolve | Per macro |
| Feedback guitar | An exciter type |
| Sympathetic strings | Follow the GENERATE scale, with a manual override |
| Presets | Deferred to M10, at the end of v1.3 |
| Oscillator count | 6 full oscillators (M3b), which are also the FM operators |
| Envelopes | A pool of 16 automatable envelopes, shown as they are used; a fixed count because hosts need a fixed parameter list |
| Per-oscillator amp | An envelope picker per oscillator, defaulting to the main AMP |
| Order | M3b before M4 and M5 |
| When a sound is done | When it is fitted to a reference and has passed a listening round, not when it sounds plausible |
| Platforms | Windows first. macOS, Linux and CI wait for M9 |
| v1.4 | Pushed back and unscheduled. v1.3 is the next full release |
| Edited wavetables | Stored in the patch as recipe, embedded data and file reference, in a versioned format; 16 patch-table slots |
| Chaos and physics LFOs | Rebuilt in M8.1 as real simulations with named parameters and smoothing; the M2 versions stay as "classic" for old patches |
| Piano | Reworked in M8.2 (real hammer, strings, bass bark, soundboard, whole-keyboard fit), right after the modulators; the M4 hammer stays as "classic" |
| v1.4 scope | Pulsar and FOF share one pulse-train engine (M11); the vector pad and resampling moved into v1.3 (M8.5, M8.6); only snapshot morphing is left (M12) |
| Milestone numbers | Finished ones are fixed; the post-v1.2 plan was regrouped into M7–M14 (2026-09-26) and again on 2026-09-27, with old numbers noted |
