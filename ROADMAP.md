# ilanaSynth roadmap

This is the plan for what comes after v1.1. M1 through M3 are complete; later milestones remain planned.

The core of the plan is **one physical modelling engine used in two places**:
- **PHYSICAL oscillator mode**: any oscillator can be a string or other modelled instrument. It replaces String mode, and old String patches migrate to it.
- **BODY section**: an upgrade of the resonator. The whole oscillator mix becomes the exciter, which gives the "weird hybrid" sounds: supersaw-rung sitars, granular-bowed plates.

**Priorities:** weird hybrids and plucked or bowed strings come first, then keys. Deep FM and the generative and physics-modulation work run alongside.

**CPU budget:** balanced, meaning a rich physical patch should cost about what a heavy unison patch costs today. A QUALITY setting (Eco / Normal / High), added in M3b, trades detail for CPU.

**Engine:** from M3b on, the synth has up to **6 full oscillators** that are also the FM operators, and a **pool of 16 envelopes**. The later milestones build on that engine.

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

### M3b: Oscillator engine (6 oscillators, envelope pool) — DONE
This comes before M4 and M5, which both build on it.
- **Refactor first, with no sound change.** The three hard-wired oscillators become "oscillator N" throughout the voice, processor and editor. Existing parameter IDs stay (`osc1_*`, `osc2_*`, and `sub_*` for OSC 3), and the preset fingerprints must show no change.
- **Up to 6 oscillators**, each with every oscillator mode (Wavetable, Physical, Sample, Granular, and later modes). OSC 4–6 default to off, so old patches sound the same, and an oscillator that is off costs no CPU.
- **Routing**: each oscillator goes to Filter 1, Filter 2, both filters, or no filter. "Both" is new; the others exist today.
- **FM between any oscillators**: today's 3×3 FM matrix becomes 6×6. M5 builds its operator features on top of this.
- **Envelope pool**: 16 envelopes, all automatable.
  - The existing AMP, FLT 1, FLT 2, MOD and ENV 4 become envelopes 1–5 and keep their parameter IDs.
  - The UI shows only the envelopes in use, with a "+" to add the next. Which ones are shown is saved with the patch, not in the editor settings.
- **Per-oscillator amp envelope**: each oscillator picks the envelope that shapes its level. The default is envelope 1, the main AMP, which is how it works today. The two filters keep their own envelope slots, and every envelope is also a mod matrix source.
- **Voice lifetime**: a voice ends only when every envelope controlling an oscillator's level has finished, so a long per-oscillator tail is never cut off.
- **QUALITY setting** (Eco / Normal / High): 6 oscillators with unison and physical models can outrun any CPU, so this lands here rather than later.
- **UI**: the OSC page shows six oscillators as a compact list with one expanded card, and the MAIN page's oscillator strip adapts. The mod matrix gets destinations for each oscillator.

### M4: Acoustic keys
- **Hammer exciter**: hardness depends on velocity, and the hammer stays in contact with the string briefly.
- **1–3 coupled strings per note**: a detune control gives the beating and the two-stage decay (a loud attack, then a long quieter tail).
- **Soundboard body**, with a tone control for lid or mic position.
- **Sustain pedal (MIDI CC64)**: dampers lift and sympathetic resonance spreads across the keyboard. The resonance is shared across voices.
- **Stretch tuning**, and a **register map** running from wound bass strings to short bright trebles.
- **Mechanical noises**: key release, dampers and pedal, each with a level.
- **Hybrids**: prepared piano (the M3 buzz and rattle as objects on the strings), bowed piano, and piano struck by an oscillator.
- **Realistic target**: a good, playable piano with real pedal bloom. It is not a match for dedicated piano modellers such as Pianoteq.

### M4b: Electric pianos
- **Tine** (Rhodes-style): a struck tine and tone bar. Pickup position and distance set how hard it barks.
- **Reed** (Wurlitzer-style): a struck reed with a pickup, whose growl feeds into the drives and amps.

### M5: Deep FM
- **Operators are the M3b oscillators**: the 6×6 FM matrix from M3b, plus a noise operator. Old patches map onto operators 1–3.
- **Tuning**: each operator has a frequency ratio that snaps to harmonic, inharmonic or bell sets, or a fixed frequency in Hz.
- **Per-operator envelopes** from the M3b envelope pool (DAHDSR, or an MSEG), plus key-scaling of level and rate.
- **16 algorithm presets**: one-click routings in the operator diagram, all still editable.
- **Non-sine operators**: already true from M3b, since every operator is a full oscillator (wavetable, granular cloud, physical string from M1–M4).
- **Feedback types**: plain, filtered, and cross-feedback between two operators.

### M6: Phase distortion (CZ-style)
- **New warp modes**: PD Saw, PD Square, PD Pulse, and PD Resonance I, II and III.
- **PD envelope**: an optional per-oscillator envelope on warp depth, like the CZ's DCW.
- **PD chain**: two phase-distortion stages in series.

---

## v1.3: Bodies, audio input, west coast, generative tools

### M7: Generative card
- **Euclidean rhythm**: steps, hits and rotation. It can trigger notes, the physical exciter or the trance gate.
- **Probability sequencer**: each of 16 steps has a chance, a pitch range and a ratchet, all snapped to the scale.
- **Strum mode** for note spray: up or down, with timing.

### M8: BODY section
- **Material bodies**: bar, plate, bell and shell. One MATERIAL knob morphs wood → metal → glass, with SIZE and DECAY.
- **Coupling**: a string drives a body, a body rings a string, or two strings ring each other.
- **The oscillator mix is the exciter.**
- **Old patches**: the current resonator becomes the "Classic" body, so old patches sound the same.

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

### M11: Feedback guitar and Evolve
- **Feedback guitar**: a new exciter type that puts an amp and speaker inside the string's feedback loop, with FEEDBACK and GAIN controls.
- **Evolve**: each macro can drift slowly within a range and at a rate you set. A **freeze** button captures the current state.

### M12: Polish
- A PHYSICAL page with an animated string, body and hammer view.
- Tour and README updates.

---

## v1.4: Later release

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
- Bounce a note or the whole patch into a sample or wavetable slot.
- Then granulate it, warp it, or use it as a pulsar source or exciter.
- The result is saved with the patch.

### M17: Preset pack
All new presets are deferred to the end of the roadmap, so they can use every new feature. Planned (about 120):

- **Physical instruments**
  - Strings (M1/M3): 10
  - Oscillator engine layers (M3b): 8
  - Physics LFOs (M2): 6
  - Acoustic keys (M4): 8
  - Electric pianos (M4b): 6
- **Synthesis**
  - Deep FM (M5): 12
  - Phase distortion (M6): 8
  - Pulsar, vector and FOF (M13–M15): 12
- **Generative and bodies**
  - Generative (M7): 8
  - Bodies and hybrids (M8): 12
  - West coast (M10): 10
  - Feedback guitar and Evolve (M11): 8
- **FX plugin (M9)**: 10 presets
- **Resample-based (M16)**: 8

---

## Applies to every milestone

- **Tuning**: the tuning test. Stretch tuning is accounted for.
- **Stability**: extreme settings with maximum coupling and feedback must never produce NaN or runaway output.
- **CPU**: per-quality limits in the heavy-preset CPU test.
- **Old patches**: migration tests (String → PHYSICAL, 3 oscillators → 6, the five named envelopes → the envelope pool, FM 3×3 → 6×6, Resonator → Classic body), and preset fingerprints to confirm the original presets still sound the same.
- **UI**: tests for each new page or card.
- **Shipping**: each milestone builds and ships on its own.

## Decisions made

| Question | Decision |
|---|---|
| How the physical engine fits in | Both: an oscillator mode **and** the BODY section |
| Electric pianos | Their own milestone, M4b |
| Audio input | Two plugins (instrument and FX) |
| Evolve | Per macro |
| Feedback guitar | An exciter type |
| Sympathetic strings | Follow the GENERATE scale, with a manual override |
| Presets | Deferred to M17, at the end |
| Oscillator count | 6 full oscillators (M3b), which are also the FM operators |
| Envelopes | A pool of 16 automatable envelopes, shown as they are used; a fixed count because hosts need a fixed parameter list |
| Per-oscillator amp | An envelope picker per oscillator, defaulting to the main AMP |
| Order | M3b before M4 and M5 |
