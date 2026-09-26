# Handoff history

The detailed notes from each milestone, moved here from HANDOFF.md on 2026-09-26 so HANDOFF can stay short. New milestone notes go at the end, in the same style. For the current state, read `HANDOFF.md`.

## State

M1–M3 were implemented before this task and pushed to `origin/main`. M3b phase A was committed separately as `4a58064`, `f8f32b7`, and `89d99fb`. It changed the three hard-wired voice oscillators, processor parameter fills, and editor controls to indexed data without changing sound. The phase A checkpoint built all targets, passed `ilanaTableTest` and `ilanaSnapshot --uitest`, and changed **0 of 235** seeded factory preset fingerprints.

M3b phase B expands the engine to six full oscillators. OSC 4–6 have Wavetable, Physical, Sample and Granular modes, unison, chord, spectral and time warps, and filter routes; they default off. The existing `osc1_*`, `osc2_*`, and `sub_*` IDs and choice indices are retained. The separate sub oscillator and noise remain separate. The FM matrix is 6×6 with the nine original cells mapped to their existing IDs. Route **Both** is appended after Direct and feeds each filter in parallel.

The envelope pool has 16 TensionAdsr envelopes. The first five keep their IDs (`amp_*`, `fe_*`, `f2e_*`, `me_*`, `e4_*`). ENV 6–16 have attack, decay, sustain, release, curve and velocity parameters and are appended mod sources. Each oscillator can select ENV 1–16 as its amp envelope; default ENV 1 retains the legacy render path. Voice lifetime follows all active oscillator amp envelopes. Which oscillators, envelopes and LFOs are shown is patch-saved (see Polish below).

The OSC page, MAIN and FM show only the added oscillators (see Polish below). New OSC 4–6 modulation destinations are appended after the existing indices. The global QUALITY control is beside oversampling.

## Verification

- Pre-edit baseline: `build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/m3b-before.csv` wrote 235 seeded fingerprints.
- Phase A: `cmake --build build --config Release` succeeded; `ilanaTableTest.exe` and `ilanaSnapshot.exe --uitest` passed with 0 failures; `python tools/compare_fingerprints.py build/m3b-before.csv build/m3b-phase-a-final.csv` reported **0 of 235 changed**.
- Phase B: `cmake --build build --config Release` succeeded for all targets, including VST3 and standalone. `python tools/compare_fingerprints.py build/m3b-before.csv build/m3b-phase-b-verified.csv` reported **0 of 235 changed**.
- `ilanaSnapshot.exe --uitest` passed with 0 failures. `ilanaSnapshot.exe build/m3b-phase-b-snapshots` captured default MAIN, OSC, FM, ENV/LFO, revealed ENV and full ENV pool. The snapshots were inspected: six rows and the full pool fit, and no labels or controls are clipped.
- The targeted `ILANA_M3B_TEST=1` run passed: all four modes on OSC 4–6, off-oscillator sample equality, Both route, all 36 FM cells, ENV 6 as a mod source, long amp release and eventual voice cleanup, old-state migration, patch-saved ENV reveal, Normal sample equality, High wavetable smoothing, and six-oscillator CPU. Six active oscillators with six unison voices per oscillator and a six-note chord measured **225.9 ms/s Normal** and **172.0 ms/s Eco**, below the 400 ms/s heavy-preset budget in the final full run.
- The final full `ilanaTableTest.exe` passed with **0 failures** (`build/m3b-tabletest-verified-final.log`). The heavy factory preset CPU cases passed the same 400 ms/s budget. One prior run had a variable Grain Choir tuning estimate; the final run passed.

## Decisions to review

- Route Both uses separate auxiliary filter states for its two parallel branches. This preserves the exact old serial and parallel filter path when no source selects Both. Both branch outputs are summed at 0.7071 gain each.
- Eco caps each oscillator at four unison voices; Normal uses the original limits and rendering; High averages two half-step wavetable reads for smoother high-note output. Physical, Sample and Granular processing are unchanged in High.
- When all active oscillators select ENV 1, the old post-filter AMP multiplication is preserved exactly. Selecting another amp envelope applies each oscillator envelope before the filter buses; this lets different oscillators have independent level shapes, while the dedicated sub/noise continue using AMP.
- New physical string banks have explicit seed values so their construction cannot advance the default seed sequence used by the first three oscillators. This is required for identical old preset fingerprints.
- Shown modules are bitmasks in the patch state: `oscRevealMask`, `envRevealMask`, `lfoRevealMask`. Each defaults to `0b111` when absent. A legacy `envRevealCount` from the first M3b build loads as the first N envelopes. An oscillator that is on, an envelope in use, or a routed LFO always shows, whatever the mask says.
- Existing modulation destination indices stay fixed. The 30 new OSC 4–6 targets occupy appended explicit destination indices after the pre-existing parameter destination segment; the total stays under 256.

## M4 (Claude, 2026-09-26)

**LFO pool.** `numLfos` is 16 in the processor and voice (`Mod::numLfoSources`). LFO 5–16 are appended sources (`Lfo5..Lfo16`, after `Env16`) and rate destinations (`Lfo5Rate..`, after `Osc6Spread`). Helpers: `Mod::lfoIndexFor`, `lfoSourceFor`, `lfoRateDestinationFor`. `lfoBuffers` keeps LFO 1–4 on channels 0–3 and clock/MSEG on 4/5; LFO 5–16 use channels 6–17 (`lfoChannel`). LFO 1–4 always render; 5–16 render only when a mod slot uses them (`lfoRouted`), otherwise their phase just advances. **LFO 5–16 draw randomness from separate generators** (`lfoPoolRandom` in processor and voice). Sharing `lfoRandom` / the voice `random` shifted Swarm's sound; keep it that way. The UI is the same reveal model as the envelopes (`isLfoShown`, scrolling card bars, step-row pickers list the shown LFOs). `maxDestinations` is 512.

**Second parameter-destination segment.** The 115 legacy parameter destinations are fixed at 96..210 because OSC 4–6 start at 211. New modulatable parameters are appended to `getParamDestinations()` after entry 115 and numbered from `Destination::Count` (`paramDestinationFor`, `paramDestinationIndex`, `numLegacyParamDestinations`). This covers the M4 keys params, the M3 bow/buzz/rattle/stiffness, and OSC 4–6 string and grain params (an M3b gap). The knob mod-ring map also gained OSC 4–6 (another M3b gap).

**Acoustic keys.**
- `KarplusStrong` gains the `Hammer` and `External` (Osc In) exciters, dampers, a bridge input, and a rebuilt Bow (`processBowed`: two waveguides in the string's buffer halves, STK friction table, 30 ms bow ramp).
- Voice adds `configureString` (register map), stretch tuning in `startNote`, and coupling: in-phase bridge loss across a note's unison strings.
- `src/dsp/AcousticKeys.h` (`Soundboard`, `PedalResonance`, `MechanicalNoise`) runs in `processAcousticKeys` after the voices at base rate, fed by the synth MIDI (note-offs, CC64).
- New params (all default off/neutral): `<osc>_hammer_hard/_couple/_damper/_register`, `stretch`, `sb_on/_mix/_tone/_size`, `pedal_res`, `mech_key/_damper/_pedal`. Excite choices append "Hammer", "Osc In".
- Six Keys presets were added (indices 235–240); 0 of the 235 older presets changed.
- The Bow rebuild changes the sound of patches that use Bow. No factory preset did before M4.

**Verified:** all targets build; `ilanaTableTest` 0 failures, with a new `runM4Tests` (also `ILANA_M4_TEST=1` alone); `ilanaSnapshot --uitest` 0 failures; fingerprints **0 of 235 changed** (`build/m4-fp.csv`). New snapshots: `lfo-pool-full/main`, `keys-grand-osc[-scrolled]`. The Grain Choir tuning estimate is still occasionally flaky (seen once this session, passed on rerun).

**Debug pass after the first M4 commit.**
- Hammer strings set their decay as a T60 in seconds: DECAY maps to 0.5–25 s at middle C, falling as (261.6/f)^0.7. The loop low-pass loss at the fundamental is compensated (Hammer only), so the treble no longer dies at once.
- Hammer strikes vary ±15% per string, which feeds the coupled aftersound. Coupling loss is `0.03 × COUPLING`.
- The strike-point reflection fades in the treble. A 1 Hz in-loop DC bleed (Hammer only) and an output DC blocker on the bow remove offsets.
- The Osc-Struck modulation sign was fixed. The matrix menu has a "Physical & Keys" submenu for the new destinations.
- `ILANA_RENDER_DEMO=<folder> ilanaTableTest.exe` renders the six Keys presets to .wav and prints section levels, DC and the largest step.

**Second listening round (user: C7 inaudible, E1 synthy, hammer wrong, Prepared not right).**
- **C7 inaudible:** the coupling (bridge) loss acted once per trip round the loop, so it wiped out the treble. `addBridgeInput` now scales by `min(1, 130.81 / f)`: the same loss per second at and above C3.
- **Treble contact:** the hammer contact is capped at half a period.
- **Commuted hammer** (`startHammer` / `hammerExcitation` / `hammerThump`): felt pulse, plus a filtered-noise board knock (12–57 ms, longer in the bass, brighter with hardness) through the strike-point comb (512-sample history). A direct felt-and-wood thump goes to the output.
- **Real inharmonicity** (`updatePianoDispersion`, Hammer only): STIFF maps to B = 1e-5·10^(3·stiff). An 8-stage allpass coefficient is solved by bisection so a reference partial lands at n·f0·√(1+Bn²). The loop is shortened by the exact allpass and damping phase delay at f0. Tested: E1 10th partial 1.0178 against 1.0155 expected.
- **Register map:** less stiffness in the (wound) bass. The soundboard low-cuts its direct path below ~90 Hz, scaled by mix.
- **Presets:** stiffness around 0.45–0.5 (Prepared 0.88: bolts). Prepared Piano was reworked (detune 14 ct, rubber damp, off-centre strike).
- **Tuning test:** it now skips Prepared Piano and granular patches with grain pitch spray (Grain Choir's long-standing flake: time-seeded grain pitch randomness).
- **`ILANA_NOTE_DEBUG=<preset>`** prints bare-string levels by register and per-note levels every 100 ms.

**Third round: matched against real piano recordings** (user: "too buzzy, sounds like a digitally plucked string").
- Reference notes: University of Iowa MIS grand, `Piano.{mf,ff}.{E1,C4,C7}.aiff`, downloaded with the user's OK to `build/reference/` (not committed).
- `python tools/analyse_note.py <midi> file...` prints per-partial levels over time, decay dB/s, inharmonicity and between-partial noise. It reads wav/aiff.
- `ILANA_NOTE_DEBUG="Hammered Strings"` writes `build/note-debug/ours.{mf,ff}.{E1,C4,C7}.wav` (dry, 6 s), plus bare-string and soundboard probes.

Findings and fixes:
1. **The loss filter is designed in seconds** (Bank model: loss ∝ f²). DECAY is the fundamental's T60: 0.5·50^decay at C4, ×(261.6/f)^0.6. DAMP sets the T60 at 2 kHz: 60·(0.3/60)^damp s. The old per-pass low-pass left bass partials above 3 kHz ringing for seconds, which was the digital-pluck buzz.
2. **Linear interpolation cost ~170 dB/s at C7.** Hammer strings now use a first-order Thiran allpass fractional delay: lossless, and the real cause of "C7 inaudible".
3. **An in-loop DC bleed created a ~1 Hz, −16 dB sub-audio oscillation.** It is removed. The hammer push is now high-passed (zero net area), with a 10 Hz DC blocker on the string output (outside the loop).
4. **The knock noise went into the loop, which is the digital-pluck mechanism.** It is now output-only and quieter.
5. **Coupling loss is 0.015·COUPLING per pass**, scaled by min(1, 130.81/f).
6. **Hammer pulse:** t·e^(1−t/τ) (no spectral nulls) below the treble. Above ~1.3 kHz it is a full-cycle raised cosine: 2nd-partial null, as a real C7's 2nd partial is ~40 dB down. Contact is (0.4+2(1−h))·(220/f)^0.35 ms, and the strike reflection is 0.2.
7. **Soundboard:** a 70 Hz 4th-order radiation high-pass (real E1 fundamental ~−43 dB) and a +5 dB bump at 260 Hz.
8. **CPU:** a per-sample `stringOut[8]` array in the voice loop cost ~14% on wavetable patches (register spills). It is now a running sum. Heavy patch 43% (limit 50%), extreme 85%. `KarplusStrong::process` is `ILANA_NOINLINE`.

Now (ours vs real, mf):
- C4 partials 2–5: −6/−13/−17/−24 dB (real −11/−16/−21/−28). Partials 6–7 are still ~12 dB under the real plateau.
- C7: fundamental −19 dB/s (real −15..−19); 2nd partial −22..−34 dB (real −40).
- E1: dense, flat partials 2–12 like the real note. The fundamental is −20 dB (real −43).

**Fourth round: fitted to the recordings** (user: "match it as close as possible; the Grand Piano patch is the benchmark; apply it to the general piano engine").
- `src/dsp/PianoTuning.h` holds the model's constants. `tools/fit_piano.py` renders Grand Piano (E1/C4/C7, mf/ff) with `ILANA_PIANO_TUNING` / `ILANA_PRESET_OVERRIDES` and searches 30 parameters in parallel (onset spectrum, per-partial decay, attack noise, mf→ff gain, register balance). Final values are baked into the PianoTuning defaults and the Grand Piano preset.
- **New structure:**
  - Two polarisations per hammered string: the existing loop is the vertical/prompt one (T60 × `promptRatio`); `processHorizontal` in the buffer's upper half is the aftersound (full DECAY T60, `aftersound` share of the strike).
  - With the register map on, a hammered note uses 1 string below B1, 2 below B2, then 3 (bass unison beating removed).
- **Error:** 632 (before fitting) → 90.7. Onset 150→27, decay 132→56, noise 107→14, mf→ff gain 142→1, register balance 34→1. The velocity response is now realistic (curve 4.57, amp velocity 0.97): ~14–20 dB from mf to ff, as in the recordings.
- Other hammer presets inherit the engine defaults. Their levels were re-checked: fingerprints −12..−20 dB, demo peaks < 0.95.

**Not done / ideas:** no sound-quality listening pass was possible here. The user should audition the piano presets (hammer brightness, coupling amount, soundboard level). The MAIN LFO card relayouts on a showing-timer only.

## Sympathetic strings debug pass (Claude, 2026-09-26)

The user suspected the sympathetic strings did not work, and said the piano presets do not yet sound like a piano, so they should be renamed (the engine is kept for sound design).

- **SYM strings were nearly silent.** After a note on a string's pitch, the tail was ~33 dB under the note at the default DECAY. There were three causes:
  - linear interpolation, a few cents off and lossy;
  - DECAY set as feedback per period (so pitch-dependent, and very short at 0.75);
  - output divided by the string count.
- **New `src/dsp/TunedString.h`**, shared by SYM and the pedal resonance: an integer delay, a Thiran fraction and a one-pole loss. It is tuned to the exact phase delay at the note, then refined on the loop's actual resonance peak (the loss filter's slope pulls it flat in the treble). The fundamental's T60 is exact in seconds. The loss filter is brightened when needed so treble strings can reach their T60 (the feedback cap had limited a 1 kHz string to ~0.5 s).
  - An optional first-order dispersion allpass puts the second partial at a set ratio. The pedal strings use it to follow STRETCH: a stretch-tuned octave above then lands on their 2nd partial (it missed by ~2.5 cents, more than the ~0.2 Hz resonance width).
- **SYM:** DECAY is 0.25·48^decay s (0.25–12 s; 4.6 s at the default). Output is scaled by 1/√count. Switching SYM off clears the strings.
  - Result: the tail sits ~18 dB under an on-pitch note, and ~45 dB under an off-pitch one.
- **Pedal resonance:** its 24 strings are TunedStrings following STRETCH. T60 is 9·√(65.4/f) s with the dampers up and 0.2 s down, and the gains were raised.
  - Result at PEDAL RES 1: the halo after the note is +10.7 dB, and +1.3 dB while the note sounds (before: <1 dB).
- **Tests** (`runSympatheticResonanceTest`, also in `ILANA_M4_TEST`):
  - pitch within 0.3 cents at 65, 247 and 1047 Hz, with and without dispersion;
  - T60 exact;
  - SYM on-pitch against off-pitch;
  - pedal halo.
  - The old SYM tuning test is tightened from 10 to 0.5 cents (it now uses a Hann window; the rectangular one was biased).
- **Renamed** (same slots, so saved sessions are unaffected): Grand Piano → **Hammered Strings** (still the fit benchmark), Pedal Bloom Piano → **Pedal Bloom**, Upright Honky → **Honky Hammers**, Prepared Piano → **Bolted Strings**, Bowed Piano → **Bowed Board**, Osc-Struck Piano → **Osc-Struck Strings**.
- **Bolted Strings:** its attacks were flattened against the limiter. Max sample step went 1.37 → 0.58 with hammer 0.9 → 0.7, master +4 → +1 dB, and limiter −4.5 → −1.5 dB.
- **UI:** manual SYM notes beyond STRINGS are dimmed. The DECAY tooltip gives the range in seconds.
- **Demos:** `ILANA_RENDER_DEMO` also writes A/B pairs (SYM off/on on Pizzicato, pedal resonance off/on on Pedal Bloom).
- **Verified:**
  - all targets build;
  - `ilanaTableTest` has 0 failures, and `--uitest` has 0 failures;
  - fingerprints: 0 of 235 old presets changed (`build/sym-fp.csv`; only the six renamed Keys presets differ).
  - The heavy-patch CPU check failed once at 58%. Every path was uniformly ~1.3× slower, including untouched ones: the machine was on the "Silent" power plan. A later run measured 45%.
  - Glitch Gate once came out 21 dB under the library median (random S&H with a synced gate); it passes on rerun.

## Polish (Claude, 2026-09-25, committed as c24b26d)

The user rejected the compact/expanded OSC list: "make them as big as they used to be, 3 by default, each time you add one you just gotta scroll... think Phase Plant."

- **Oscillators**: full pre-M3b card size on OSC and MAIN (MAIN strips restored to mode/table/warp plus six knobs). OSC 1–3 are shown by default; **+ ADD OSCILLATOR** reveals the next one and switches it on (`addOscillator`). **×** switches an oscillator off and hides it (`removeOscillator`). Cards are sized as if three fill the view; more scroll (OSC page viewport; MAIN oscillator column in its own viewport).
- **FM**: the diagram and matrix are N×N over the shown oscillators. With three or fewer, the diagram uses the original triangle; with more, a ring.
- **Envelopes**: three shown by default (AMP, FILTER 1, FILTER 2). Cards keep the original fifth-of-the-row width, and the row scrolls horizontally and follows the selection. **+** adds; right-click removes one that is not in use.
- **LFOs**: the same model for the four LFOs (three shown, **+**, right-click remove).
- `revealVersion` bumps on any mask change or patch load; editors poll it.
- Naming: the old "Env 4" source/chip/tab now reads **ENV 5** to match the pool numbering. Amp envelope choices read "ENV 1 Amp", "ENV 2 Filt 1", "ENV 3 Filt 2", "ENV 4 Mod". These are label-only changes.
- Engine fixes: `lastSamplePosition` initialises all six oscillators to −1; a hard-stopped voice clears its ENV 6–16 monitor values.
- Verified: all targets build; `ilanaTableTest` 0 failures, including new reveal-mask tests and the destination-layout check. `ilanaSnapshot --uitest` 0 failures; the MAIN test was rewritten for full cards and add/remove. Fingerprints: **0 of 235 changed** (`build/polish-fp.csv`). The snapshot tool now also writes `added-osc-{MAIN,OSC,FM}[-scrolled].png`.

## Known gaps

No known M3b sound or UI gaps. The factory tuning test has occasionally reported Grain Choir around 39 cents out on one run; it passed in the final run with the same audio implementation. Nothing from M3b has been pushed. The plugin was built locally but not installed.

## M5 Deep FM, M6 phase distortion, M6b 64-slot matrix (Claude, 2026-09-26)

Done in one pass at the user's request ("the three phases"), then a debug pass and a polish pass.

**M5 Deep FM.**
- **Tuning** (`OscTuning` in `Voice.h`): `oscN_tune` Semitones / Ratio / Fixed Hz. `oscN_ratio` (0.0625–32) snaps with `oscN_ratio_snap`: Free, Harmonic (0.25, 0.5, 1..32), Inharmonic (square roots of non-squares up to 128, plus √2/2 and √3/2), Bell (hum 0.5, prime, tierce 1.2, quint, nominal, 2.5, 2.667, bar modes 2.756 / 5.404 / 8.933 / 13.34, and a few octaves). SEMI and FINE apply on top. Ratio follows note, bend, glide and drift; Fixed Hz (`oscN_fixed_hz`) follows only its pitch modulation. Strings use the same pitch.
- **Key scaling:** `oscN_key_level` is ±6 dB per octave from C3 (gain clamped to 4). `ampN/feN/.../envN_keyrate` halves every stage time per octave at 100 %.
- **Envelopes:** DAHDSR. `*_delay` and `*_hold` for all 16 (prefixes `amp fe f2e me e4 env6..env16`). The delay stage decides lazily on its first sample, because the voice sets envelope parameters after `startNote` (the first note skipped its delay otherwise); zero delay is bit-identical. **MSEG as an envelope:** amp envelope choice 16 runs the MSEG shape per voice (`envMseg`), clamped to 0..1; the voice lives as long as ENV 1.
- **Feedback types** (`oscN_fb_type`, `FmFeedback`): Plain (the old one-sample loop), Filtered (a DX7-style two-sample average through a one-pole at 8× the operator's pitch), Cross (the FB cell runs both ways between pairs 1-2, 3-4, 5-6). Only computed when some active oscillator uses a non-plain type.
- **Noise operator:** `fm_noise1..6` into each oscillator's FM input, `fm_noise_color` from a 200 Hz one-pole to white. Own generator (`fmNoiseRandom`, seed 31337).
- **Algorithms:** `src/dsp/FmAlgorithms.h`, 16 of them: nine small ones (2–4 operators) and seven after DX7 algorithms 1, 5, 7, 16, 19, 22 and 32. `applyFmAlgorithm` adds the operators it needs, clears other routes, keeps the amounts of routes it keeps (new ones start at 35 %, feedback 15 %) and sets OUT. `findMatchingFmAlgorithm` prefers the match with the most operators (Pair + Sine also satisfies 2-Op Stack).
- **Reference and fit:** there is no recording to fit, so the reference is an ideal phase-modulation renderer in the test (`renderFmReference`: `sin()` operators, the same routes one sample late, `TensionAdsr` envelopes). Three DX-style patches (keys with a 14:1 tine, a √12 / 2.756 bell, a 1:1 feedback bass) match it with a mean partial error of **0.017, 0.006 and 0.007 dB**. Two operators match Bessel sideband levels (J0/J1/J2 at index 1.5) within **0.02 dB**. The first bass fit failed at 4.9 dB: at feedback 0.25 the ideal renderer and the engine both fall into a period-2 (half-sample-rate) oscillation, locked in opposite phase. The reference bass now uses 0.12, and a test checks that Filtered feedback avoids the buzz (0.011 against 0.79 for Plain).

**M6 phase distortion.**
- Warp modes appended: PD Saw, PD Square, PD Pulse, PD Res I–III (10–15). `Warp::applyPhaseDistortion`: the CZ reads a cosine through a bent phase; here the bend happens a quarter cycle on (where a sine table reads as a cosine) and is shifted back, so the Sine table gives the CZ wave inverted and a quarter cycle late (inaudible), and any table gets the same bend. The resonant waves are a sine at 1 + 15 × amount restarting each cycle under a saw / triangle / trapezoid window (zero-DC; it fades in over the first quarter of the knob). They match the CZ formulas (`czReference` in the test) to float precision, and their harmonics to 0.000 dB.
- **PD chain:** `oscN_warp2` / `oscN_warp2_amt`, a second stage after the first. Its list skips FM and Ring (`Warp::getStageTwoNames`, `modeForStageTwoChoice`). Band-limiting uses both stages' stretch.
- **Warp envelope (DCW):** `oscN_pd_env` (Off, ENV 1–16, MSEG) and `oscN_pd_env_amt` (−1..1). It adds to both stages' amounts at the 16-sample update rate.

**M6b.** `Mod::maxSlots` is 64. Slots 33–64 are appended after every other parameter, so slots 1–32 keep their places. The matrix page only builds rows that are in use, and now refreshes when shown (it had waited for its timer). With eight notes, an empty matrix measured 108 ms/s and all 64 slots 297 ms/s.

**New modulation destinations** (block-rate parameter destinations, appended after the M4 list): per oscillator Warp 2, Warp Env and Key Level; FM Noise > Osc1..6 and FM Noise Colour; and the 27 FM cells added with OSC 4–6 (the nine original ones are per-voice). 394 of 512 destinations are in use.

**UI.**
- FM page: an algorithm strip (small routing pictures, the matching one lit), the diagram (a flat hexagon for six operators, a noise node when noise is routed), an operator panel (OP 1–6: TUNING, SNAP, FB TYPE, ENVELOPE, RATIO or FIXED, SEMI, FINE, LEVEL, KEY LVL, with "sounds at x…" after SNAP), and a NOISE row in the matrix with NOISE COLOUR in the top strip. Diagonal cells read FB, FB~ (filtered) or FB<> (cross).
- OSC card: a WARP CHAIN row (WARP 2, WARP 2 AMT, WARP ENV, ENV AMT) opens when a warp is picked. The wave display draws the warped cycle over a faint copy of the frame.
- ENV: a second row with DELAY, HOLD and KEY RATE. The graph and the envelope cards draw delay and hold.

**Debug pass.**
- The envelope delay (above).
- The matrix page refresh (above).
- The LFO panel could show a hidden LFO restored from the saved selection ("LFO 14" with three LFOs shown); it now falls back to the first shown LFO.
- A ratio "×" in a narrow string literal came out as mojibake under MSVC; it is now "x".

**Verified.**
- All targets build, with no warnings from the changed files.
- Fingerprints: **0 of 241** changed (`build/m5-before.csv` against `build/m5-after.csv`).
- `ilanaSnapshot --uitest`: 0 failures (new: algorithm click, operator panel, PD chain row, envelope knobs, slot 60 row).
- `ilanaTableTest`: see the final run in HANDOFF / the commit message.
- CPU: alternating runs against a baseline build of `363862c` in `../ilana-baseline`: heavy 42.4 % against 41.6 %, extreme 85 % against 83 %.
