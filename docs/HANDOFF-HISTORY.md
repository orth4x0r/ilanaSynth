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
- `ilanaTableTest`: every functional test passes (`build/m5-full2.txt`). The one failure is the load-dependent heavy-patch CPU check: 67 % inside the long run, 41–42 % in isolated runs (`ILANA_BENCH=1`). `ILANA_M5_TEST=1` passes.
- CPU: alternating runs against a baseline build of `363862c` in `../ilana-baseline`: heavy 42.4 % against 41.6 %, extreme 85 % against 83 %.

## UI and debug pass after M5–M6b (Claude, 2026-09-26)

- **Matrix destination menu:** the M5/M6 parameter destinations were filed under "Physical & Keys". Warp 2, Warp Env and Key Level now sit in their oscillator's submenu, and FM noise and the OSC 4–6 FM cells in "FM".
- **Undo:** an algorithm click and a diagram drag set parameters directly, so they merged into the previous undo step (one undo reverted both). They now start their own step; a UI test checks it.
- **FM diagram:** the three-operator triangle was squashed by the shorter diagram area. It now scales x and y separately, and the algorithm strip is a little shorter.
- **KEY LVL** was computed only at note-on; it is now per block, so turning or modulating it reaches held notes.
- **Envelope cards:** an envelope used as an oscillator's warp (DCW) envelope now counts as in use, so its card shows.
- **OSC card:** WARP 2 AMT and ENV AMT dim while their selector is Off.
- The tutorial still described FM as "a 3-operator matrix"; it now says six operators with algorithms and a noise operator. The "New in 1.1" band is left as it was.
- Verified: fingerprints 0 of 241 changed; `--uitest` (plus a destination-menu test) and `ILANA_M5_TEST` 0 failures. The full suite's only failures were the two CPU-budget checks while the machine was loaded: the unchanged baseline build read 52–80 % on the same heavy patch in alternating runs, and Eco measured slower than Normal.

## Bug and polish pass after 1.2 (Claude, 2026-09-26)

Reported by the user: the arp not stopping in Ableton, FM into string oscillators, and the preset picker. Then a review by five Sonnet agents (presets, FM, DSP, visual design, editor usability), with the findings checked before fixing.

- **Arpeggiator** (`processArpeggiator`, rewritten):
  - Held keys were only dropped by single note-offs. All Notes Off / All Sound Off (which JUCE's VST3 wrapper passes on as CCs) and a host transport stop now clear them, so a clip or keyboard whose note-offs never arrive can't leave the pattern running. That was the likely cause in Ableton.
  - Events act at their own sample. Before, the whole block's keys were read first, so a release waited for the next block.
  - While the host plays, steps lock to its beat grid (from the ppq position). A key pressed just before a grid line keeps that line's step.
  - Chord mode ignored the gate; it now releases at the gate like the other modes.
  - Held-note arrays are preallocated. `prepareToPlay` drops held keys.
  - Tests: `runArpHostStopTests` (All Notes Off, All Sound Off, transport stop, chord gate). `ILANA_ARP_TEST=1` runs only the arp tests.
- **FM greying:** sample and granular oscillators never read FM, and a Physical string only with Excite = Osc In (then as excitation). Their matrix columns and noise cells are disabled and dimmed, with a tooltip saying why. The diagram gives them a dashed ring and a NO FM IN tag, draws routes into them faint, and won't add a new route by dragging (an existing one can still be removed). FM wasn't added to those modes, because presets with routes into them would change.
- **Preset browser** (`PresetPanel.h`, rewritten):
  - It was made visible when it was created, so the first click ran `close()` (the fade-out with nothing there). It now starts hidden.
  - It opened at a fixed top-right position. It now drops down under the preset name.
  - Chips covered 8 of the 11 categories used. Drums, Generative and Chords had none. A sidebar is now built from the categories in use, plus All, Favourites and User, each with a count.
  - New behaviour: a click loads and keeps the browser open; Up/Down step and load; Enter or a double-click keeps the preset and closes; Esc or a click outside closes. There's a star to favourite a row, multi-word search, a "now loaded" marker, SURPRISE ME (random from the list), and delete moves the file to the recycle bin.
  - A saved name that collides with a factory name gets a number, because favourites are keyed by name.
  - A preset load starts its own undo step.
- **Processor:**
  - The reverb was never given the sample rate, so it was tuned for 44.1 kHz at every rate.
  - The oversampling latency is now reported (`updateLatency`).
  - A block longer than the one announced is split into announced-size chunks (`processChunk`), since the oversamplers are sized for it.
  - Output peaks are kept for a meter.
- **UI:**
  - Knobs: bipolar parameters fill from the centre, a soft glow sits under the value arc, and a knob at zero keeps a visible tip instead of looking switched off.
  - Mod source chips are pills with grip dots (so they read as draggable, not as tabs). They glow with the live source value while that source is routed; macros, the wheel and pressure always glow.
  - A stereo output meter sits beside MASTER. It lights red after a clip; click it to reset.
  - The amp-envelope choices read "Amp Env", "Filter Env", "F2 Env", "Mod Env" and "Env 5" to "Env 16" instead of "ENV 1 Amp" and so on.
  - The duplicate WAVETABLE badge in the wave display is gone.
  - Empty FX slots have a brighter "+ add effect", and no LED.
  - Shift gives a fine drag on the matrix curve.
  - Cards destroyed under the mouse no longer leave knobs lit for a hovered source.
- **Verified:**
  - All targets build.
  - `ilanaTableTest`: 0 failures, including the CPU checks.
  - `--uitest`: 0 failures.
  - Fingerprints: the 28 changes against v1.2 are explained in HANDOFF, confirmed by building HEAD in a worktree and bisecting (with the old arp function, the two non-reverb presets match HEAD).
  - New snapshots include `fm-no-input.png` and `preset-browser.png`.

## M7.0: follow-ups from the polish pass (Claude, 2026-09-26)

- **FM for OSC 4–6 per note.** The 27 FM cells to or from OSC 4–6 were already destinations (block-rate, added in M5). Like every parameter destination, though, their per-voice sources followed the loudest voice, so an envelope or velocity drove all notes the same. Voices now evaluate those cells themselves, per sample (`Mod::extendedFmCellFor`, `Voice::fmCellMods`, flag `anyExtendedFmMods`), and the synth-wide offsets skip them. When no slot targets them, the path is unchanged. Test: velocity > FM 4>1 with a loud and a quiet note at once gives the loud note 3.6x the sideband (`ILANA_M70_TEST=1`).
- **Filter types** are laid out in three labelled families of four (CLASSIC, CHARACTER, SPECIAL) with dividers; the type indices are unchanged. The user picked this layout.
- **Empty MATRIX:** six one-click starters under the empty-state card, each a single undo step. WHEEL > VIBRATO routes LFO 2, via the mod wheel, to OSC 1–3 pitch.
- **Empty FX rack:** all 29 effects as quick-add buttons, in rows by group (Space, Drive, Motion, Rhythm, Tone & Level). The user picked both of these layouts.
- **Pool cards:** envelope and LFO cards carry a tag naming what they drive (built-in jobs such as Amp, Filter 1 or Osc2 Warp first, then matrix targets, as "first +N"). Unrouted LFOs are drawn faint. Refreshed four times a second.
- **Number keys:** kept. They act only without modifiers, and Live's computer MIDI keyboard uses letter keys, never the number row.
- **Verified:** all targets build. `--uitest` has 0 failures, with new tests for the starters and quick-add. Fingerprints: 0 of 241 changed against `build/fingerprints-polish.csv`. `ilanaTableTest`: one failure, the heavy-patch CPU check at 54.6 % in a run where unrelated timings were also about 1.3x slower (machine load). Isolated reruns read 41.5 % and 41.1 %, as before.

## M7.1: the Generative card (Claude, 2026-09-26)

The roadmap gave three lines. These details were settled while building it.

- **One note engine** (`processArpeggiator`) serves the arp, the probability sequencer and Euclid.
  - While on, the sequencer (mode -1) takes over from the arp.
  - Euclid in **Notes** mode rests the engine's steps that fall between hits. With the arp off, the engine plays the held chord (Chord mode) at Euclid's rate and gate.
  - Step numbers come from the host's beat grid while it plays, so patterns line up with the bar. Otherwise they count from the first key.
  - With only the arp on, behaviour is unchanged.
- **Euclid** (`euc_*`): steps 2–32, hits, rotate, rate, gate and target. `euclidHit()` in `Generative.h` is Bresenham: E(3,8) = x..x..x.
  - **Exciter** sends a private SysEx marker (0x7D "IL") into the synth's MIDI at each hit. `IlanaSynth::handleMidiEvent` calls `Voice::reExcite`, which re-triggers the Physical strings of keys still held. It is sample-accurate, including at 2x/4x oversampling.
  - **Trance Gate** makes `processGate` use Euclid's steps, rate and hits.
- **Probability sequencer** (`pseq_*`): rate, length (1–16) and gate, and per step a chance, a range (0–24 st upward from a random held key, snapped to `gen_scale`) and a ratchet (1–4). It has its own random generator (`pseqRandom`). Parameter IDs are built once.
- **Strum** (`spray_strum`, `spray_strum_time`) is in `NoteSpray`. Notes starting on the same sample, played and sprayed, are held as pending items, sorted by pitch and spaced by the strum time. A strummed played note is released through its children list, so a release mid-strum leaves nothing stuck (tested).
- **UI:**
  - The ARP card is now the GENERATIVE card, with ARP / EUCLID / PROB SEQ pill tabs and a hint line. `EuclidDisplay` draws a ring and a strip; drag it to set hits and rotate. `ProbSeqEditor` has chance, range and ratchet lanes. Both are in `gui/GenerativeWidgets.h`.
  - GENERATE gets a STRUM menu and a STRUM TIME knob. The spray's old knob that was labelled STRUM (the random spread) now reads SPREAD.
- **Fixed on the way:** the arp arrays' preallocation (from the polish pass) sat inside `#if ILANA_FINGERPRINT_BUILD`. It now runs in every build.
- **Presets** (Generative, appended at 241–244): Euclid Pluck Machine (arp + Euclid 7/16), Probability Bells (FM, A minor pentatonic, ratchets), Strummed Harp (strum up), Euclid Kalimba (Exciter 5/8). They are level-matched to the library (−17 to −22 dB RMS in the fingerprint).
- **Verified:**
  - All targets build.
  - `ilanaTableTest` (full suite, including the CPU checks) and `--uitest`: 0 failures. `ILANA_M71_TEST=1` runs the new tests: pattern, chord hits, arp gating, chance 0, ratchets, range and scale, strum order both ways, strum release, exciter hits, trance gate.
  - Fingerprints: 0 of the old 241 changed. The new baseline is `build/fingerprints-m7.1.csv`.
  - Snapshots: `gen-euclid.png` and `gen-probseq.png`.
  - No fitting to a reference: nothing here is a modelled sound. The listening round is the user's, in Live.

## M7.2: BODY section (Codex, 2026-09-26)

- **Engine:** `MaterialBody.h` is an eight-mode damped resonator. Eco uses four modes, Normal six, High eight. Bar, plate, bell and shell ratios, strike strengths and per-mode lifetimes live in `BodyTuning.h`. SIZE shifts modal pitch, MATERIAL morphs wood through metal to glass, and DECAY controls the ring time. Coefficients are cached between control updates. The entire pre-filter oscillator mix excites the body. A string pluck can transfer an initial strike; BODY-to-string and string-to-string bridge coupling are also available. The material tail continues after the exciter envelope releases.
- **Legacy:** `body_type` defaults to Classic. Classic continues through the unchanged three-comb `ResonatorBank` path, and new coupling is off by default. New choice indices and modulation destinations are appended. Five new BODY presets follow the existing 245.
- **UI:** the FILTER page's resonator card is now BODY, with type, material, size, coupling mode and coupling amount. Existing on/amount/decay/offset/key-track controls remain.
- **References picked before implementation:** University of Iowa MIS [marimba C4](https://theremin.music.uiowa.edu/MIS-Pitches-2012/MISMarimba2012.html), [Thai gong C4](https://theremin.music.uiowa.edu/MIS-Pitches-2012/MISGongsTamTams2012.html) as the struck metal plate, and [brass bell C5](https://theremin.music.uiowa.edu/MIS-Pitches-2012/MISBells2012.html). Glass is the [struck wine glass](https://freesound.org/people/nlux/sounds/636431/) public preview. Local copies are in `build/reference/body/` and are not committed.
- **Fitting:** `ILANA_BODY_FIT=build/fit/body/modal` renders impulse responses; `tools/fit_model.py check-body` measures each prominent partial's frequency ratio and decay. Targets: mean absolute ratio error ≤20 cents and mean absolute per-partial decay-slope error ≤5 dB/s. Bar: 1.8 cents / 4.8 dB/s (2 partials); plate: 3.8 / 2.2 (8); bell: 0.9 / 4.0 (6); glass: 0.0 / 1.8 (1). The glass reference exposes only one clear ringing partial. `fit_piano.py` now uses the shared loader in `fit_model.py`.

- **Finished by Claude (2026-09-26).** Codex's usage ran out during the full test run. The build, tests and fit were rerun, and these changes made:
  - **Gain.** The body is tuned to the note, so the drive sits on its first mode, where a two-pole mode's gain grows as 1 / (1 - r): +60 dB and more at long DECAY. The original version stayed bounded only because its safety clamps (±8 per mode, ±2 out) were limiting, so the five new presets fingerprinted at -5 to -7 dB RMS with peaks at 1.0. Every mode's input gain is now scaled by one factor, the first mode's exact peak gain ((1 - r)·|1 - r·e^(-2jθ)|), plus +12 dB. A sustained drive now comes out at unity at any DECAY and pitch. Since the factor is shared, the modes keep their balance and fit: bar 1.8 cents / 4.8 dB/s, plate 3.8 / 2.2, bell 0.9 / 4.9 after the change.
  - **Strike** (STRING TO BODY) is set by amplitude, not by the input gain, so plucks still ring at long DECAY. The first mode rings at 0.25; higher modes fall off as sin(root) / sin(mode), the balance Codex's version had (an equal balance put Bell Shell Hybrid 42 cents out in the preset tuning test).
  - **New test:** switching Classic to any body keeps the level within 12 dB of dry, for a sustained saw (+5 to +10 dB) and a coupled pluck (-3 to +9 dB), at DECAY 0.3 and 0.9. The bar-versus-plate check is relative now.
  - **Presets** re-levelled to -18 dB RMS or a 0.8 peak, whichever comes first (-18 to -22 dB RMS). The new baseline is `build/fingerprints-m7.2.csv`.
  - **UI:** the BODY card is one row. ON, the BODY menu and the COUPLING menu are stacked on the left, then seven full-size knobs. The coupling knob reads COUPLE, since the card already has an AMOUNT. MATERIAL and SIZE fade for Classic. COUPLE fades when coupling is off, or when a body-dependent mode has no body. The card is back to its old height, so the response display keeps its size. The signal-flow block reads BODY.
  - **CPU:** alternating `ILANA_BENCH` runs against an M7.1 build (worktree) give heavy 45.0 % against 44.4 %. That is no regression; the 72 % and 49 % readings in the full runs were machine load.
  - **Test fix (M7.1):** the trance-gate test rendered 2.2 s (105,472 samples) but measured the second hit up to sample 116,000, reading past the buffer. It failed now and then in full runs only. It renders 2.5 s now.
  - **Verified:** all targets build. `--uitest`: 0 failures. `ILANA_M72_TEST=1`: 0 failures. Fingerprints: 0 of the old 245 changed. `ilanaTableTest` full suite: only the CPU checks fail, under machine load (six-oscillator Normal read 284–434 ms/s and the heavy patch up to 62 % across identical runs, with Ableton Live open); the A/B above shows no regression.

## CPU pass (Claude, 2026-09-27)

- 8 notes x 3 osc x 16 unison went from 37.7% to about 10% of a core; idle from 2.1% to 0.2% (512 blocks) and 7.9% to 0.5% (128 blocks).
- `src/dsp/UnisonBank.h` renders each oscillator's wavetable unison with SSE (4 voices a step, 32-bit fixed-point phase) instead of 16 `WavetableOscillator`s. `getParam` uses a prebuilt hash and `ParamRef` IDs (resolved once) instead of the value tree's string map. Zero FM cells, unused LFO/envelope slots and unrouted LFO 1-4 waves are skipped. AVX2 was tried and measured no faster (the table reads dominate), so it was removed.
- Tools: `ILANA_UNISON_BENCH=1` (bench plus the bank-vs-oscillator equivalence test), `ILANA_PROFILE=<unison>` (sampling profiler with call stacks; `ILANA_PROFILE_NOTES`, `_BLOCK`, `_SECONDS`).
- Folder cleanup (2026-09-26): old snapshots, demos, fit runs, logs and the v1.1 portable binaries were deleted. `build/` keeps the CMake tree, `build/reference/` and `build/fit-best-final.json`; `snapshots/v12` is the latest UI set.

## M7.3: Electric pianos (Claude, 2026-09-27)

- **Engine:** `src/dsp/ElectricPiano.h`, used by `KarplusStrong` when EXCITE is **Tine** (7) or **Reed** (8), both appended to the excite lists. It is five decaying modes (complex rotators): the fundamental, the tone bar (a slightly detuned, slower mode), 2x (the tip's arc), and the clamped-free beam modes at 6.267x and 17.55x. A half-sine hammer force drives every mode for its contact time, which shortens with velocity, HAMMER and register. The output is the derivative of the pickup function of the displacement:
  - Tine: a magnetic bump, 1 / (1 + ((x - offset) / d)^2), normalised by its steepest slope.
  - Reed: the capacitive 1 / (1 - x / d), softly limited near the plate, then the preamp's high-pass and an asymmetric tanh.
- **Knobs:** DECAY and DAMP (tone) reuse the string's; new per-oscillator `_ep_distance` and `_ep_position` (DISTANCE, OFFSET; appended parameters and mod destinations); HAMMER and DAMPER reuse the M4 keys parameters. The Physical card shows only these for Tine and Reed, and the wave display draws the pickup curve with the swing of a medium and a hard note.
- **References:** `tools/RefHost.cpp` (`ilanaRefHost`) is a headless VST3 host that renders an installed instrument's default sound. It rendered Arturia Stage-73 V2 and Wurli V2 at MIDI 40, 60, 84 and velocity 40, 120 (4 s held, 1 s release) into `build/reference/ep/` (not committed).
- **Fit:** `tools/fit_ep.py tine|reed` (coordinate search like `fit_piano.py`) over `EpTuning.h`, via `ILANA_NOTE_DEBUG=<preset>` with `ILANA_NOTE_SET=ep` and `ILANA_EP_TUNING`. The error sums onset spectrum, per-partial decay, between-partial noise, **bark** (the rise of partials 2+ from soft to hard), soft-to-hard gain and register balance. Tine: 2499 to 124; reed: 1677 to 135. The tine's 6.27x partial is capped at 0.6 s T60 (unconstrained, the fitter let it ring 2 s, which is unphysical; the capped fit costs 8 points). The remaining error is mostly the onset spectrum (about 7.5 dB RMS per partial) and between-partial noise.
- **Presets:** Tine Keys and Reed Keys (Keys), level-matched.
- **Not done:** the listening round. It is the user's: `ILANA_RENDER_DEMO=build/demo` renders both presets with the keys demos, and `build/fit-ep/<model>/base/` holds the fitted notes beside the references.

## M7.4: Wavetable editor (Claude, 2026-09-27)

- **Data:** `src/dsp/WavetableDoc.{h,cpp}`: frames (2048 samples, 1-256) plus a per-frame recipe (Raw, Draw points and smoothing, Harmonics magnitudes and phases, Formula). `src/dsp/Formula.h` is a small recursive-descent evaluator (x, f, n; the usual maths functions plus saw, square, tri, pulse).
- **Storage:** each edited or loaded table is a versioned `<Wavetable slot=... version=1>` child of a `<Wavetables>` node in the patch state, with the recipe (an unedited factory copy stores just its index), 16-bit gzip base64 frames whenever the recipe can't rebuild it exactly, and the source path as a hint. The loader tries recipe, then data, then the file; failing all three, the slot resets and `getTableNotice()` explains why. The old `userTablePathN` properties are still written and read, so old patches load as before and embed their table on the next save.
- **Slots:** `numUserSlots` 4 to 16; the choices are appended after User 4, so no index moves. A slot never edited shares its factory table (no rebuild), so the extra slots cost nothing at startup.
- **Editor UI:** `src/gui/WavetableEditor.h`, an overlay over the whole window, opened by **EDIT** beside LOAD .WAV (a factory table is first copied into a free patch table). Frame list; DRAW (snap 8-64 steps, SMOOTH), SPECTRUM (harmonics 1-64, level in dB and phase), FORMULA (this frame or all frames); SHAPES; MORPH (crossfade or spectral, between frames or across the whole table at 8-256 frames); UNDO (30 steps); IMPORT (file or resynthesis); EXPORT; LIBRARY (`Documents/ilanaSynth Wavetables`, `.wav` plus an `.ilwt` recipe sidecar). Edits reach the sound after 120 ms (debounced), since rebuilding a big table takes tens of ms.
- **Files:** export writes 32-bit float with a `clm ` chunk (`<!>2048 00000000 wavetable (ilanaSynth)`); import reads the frame size from `clm ` or `uhWT` (`Wavetable::readWavFrameSize`) and resamples to 2048.
- **Tests:** `ILANA_M73_TEST=1` (M7.3 and M7.4) and the UI test's EDIT, DRAW, SPECTRUM, FORMULA, MORPH and CLOSE steps.

## M7.5: ilanaSynth FX (Claude, 2026-09-27)

- **Build:** a second plugin target `ilanaSynthFX` (VST3 and standalone, `ILANA_FX=1`, code `Ilnf`, category Fx, stereo or mono in, MIDI in) on the same sources, so presets and parameters are shared. `IlanaSynthAudioProcessor::isEffectBuild` switches the FX-only parts; the instrument never runs them.
- **Input path:** `captureLiveInput` copies the input aside and clears the buffer; `prepareLiveInput` applies INPUT GAIN, runs the envelope follower, writes 3 s of history for live grains, upsamples the input to the voice rate (linear, for 2x/4x), and sends GATE or DRONE notes (IN NOTE) into the MIDI before the generative stage. DRY adds the untouched input at the end (without the oversamplers' latency).
- **Where the input goes:** oscillator mode **Live** (4, appended); **LIVE** grains (`_grain_live`, granular mode reads the history ring, POSITION is how far back); `in_body` into the BODY exciter (material bodies); `in_strings` into Physical strings (`KarplusStrong::addLiveInput`, bridge input at 0.1, or a force on a tine or reed); **Input Env**, a new global mod source (appended; an INPUT chip in the FX build).
- **Presets** (category FX Input, silent in the instrument): Live Body (the effect opens on it), Live Wah, Live Grains, Live Strings.
- **UI:** an INPUT tab (last, FX build only) with level and envelope meters, the controls, help and quick starts. Live mode shows LIVE INPUT with a level bar.
- **Tests:** `ilanaFxTest` (built as the effect): layouts, no leak without routing, DRY, Live oscillator and gain, body, strings, gate, envelope follower, Live Wah, live grains, every FX preset, and the editor's INPUT page. `ILANA_RENDER_DEMO=build/demo/fx ilanaFxTest` renders every FX preset over drums, plucks and a voice.

## Debug and polish after M7.5 (Claude, 2026-09-27)

- **Split renders:** a MIDI event mid-block splits the voices' render, and the second part read the shared LFO, clocked S&H and MSEG buffers from the block's first sample, so modulation jumped at every note, arp step or CC. Voices now index them from where the render starts (`Voice::renderStart`). New test: an unused CC mid-block leaves an LFO-modulated note sample-identical. Fingerprints: unchanged (their renders don't split).
- Live Strings was 20 dB over its input (the loop resonates); the string drive is scaled down by 10x. The FX test's string check uses the Osc In exciter so it measures the input alone.
- Presets appended after Supersaw Bell Body (they had been inserted before it, moving its index).
- The factory-preset tuning, level and silence checks skip the FX Input category; the excite-list check allows appended choices.
- Knob text for the input's ms and dB values; a wave display tooltip for Tine/Reed and Live; the 3D button hides where it doesn't apply; a SMOOTH label in the editor; OSC card titles read LIVE.
- `build-and-install.cmd` builds and installs ilanaSynth FX as well.
