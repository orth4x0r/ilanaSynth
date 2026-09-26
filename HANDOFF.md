# Current handoff

Updated 2026-09-26. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit concurrently.

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

**Not done / ideas:** no sound-quality listening pass was possible here. The user should audition the piano presets (hammer brightness, coupling amount, soundboard level). The MAIN LFO card relayouts on a showing-timer only. Prepared Piano is ~−25 dB RMS (percussive; peaks limited).

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
