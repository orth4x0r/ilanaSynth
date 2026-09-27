# Current handoff

Updated 2026-09-27. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit at the same time.

**Keep this file short** (under about 60 lines): current state, next step, open issues and the rules below. When a milestone is done, move its detailed notes to the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).

## State
- **Done:** M1–M7 (see [ROADMAP.md](ROADMAP.md)). M7.3 (electric pianos), M7.4 (wavetable editor) and M7.5 (ilanaSynth FX) were done in one pass by Claude on 2026-09-27, followed by a debug and polish pass. Notes for every milestone are at the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).
- **Next:** M8.1, the west-coast voice (wavefolder, low-pass gate; reference: online recordings, Buchla Easel V default as a fallback).
- **Release:** v1.2 is tagged `v1.2.0`; the version is still 1.2.0 (v1.3 = M7–M10). The installer has not been rebuilt.
- **Git:** everything through M7.5 and the debug/polish pass is committed on `main`, tagged `m7.3-done`, `m7.4-done`, `m7.5-done` and `m7-done`. Push only when the user asks.
- **Plugin:** ilanaSynth and ilanaSynth FX installed to `C:\Program Files\Common Files\VST3` on 2026-09-27 (includes the CPU pass).
- **Two plugins:** `ilanaSynth` (instrument) and `ilanaSynthFX` (`ILANA_FX=1`, an effect with audio input) build from the same sources and share parameters and presets. FX-only code is behind `IlanaSynthAudioProcessor::isEffectBuild`.
- **Tools added:** `ilanaRefHost` (renders an installed VST3's default sound), `tools/fit_ep.py`, `ilanaFxTest` (the effect's tests; `ILANA_RENDER_DEMO=<folder>` renders FX demos), `ILANA_M73_TEST=1` (M7.3/M7.4 and split-render tests). `ilanaSnapshot <dir> <preset name>` now takes a name.

## Demos
`ILANA_RENDER_DEMO=build/demo ilanaTableTest` writes the keys demos (now with Tine Keys and Reed Keys) and `build/demo/fm-pd/`. `ILANA_RENDER_DEMO=build/demo/fx ilanaFxTest` writes the FX presets over drums, plucks and a voice. The EP references are in `build/reference/ep/` and the fitted notes in `build/fit-ep/<model>/base/`.

## Rules that still apply
- **Old presets must not change.** Check with `ilanaFingerprint` before and after, then `python tools/compare_fingerprints.py before.csv after.csv`. `build/fingerprints-m7.5.csv` is the current baseline (256 presets; the first 250 match `build/fingerprints-m7.2.csv`) (`build/fingerprints-v1.2.csv` is v1.2's). New presets are appended at the end of the list. Against v1.2, 26 reverb presets differ at the tool's 48 kHz only, because the reverb now gets the real sample rate. At 44.1 kHz they are unchanged. Two more (Through-Zero Growl, Shift Bass) differ only because voice random state carries over between presets within the run.
- **Parameter IDs and choice indices are never renumbered.** New choices, destinations and sources are appended.
  - Mod destinations: the 115 legacy parameter destinations are fixed at 96..210; OSC 4–6 start at 211; newer parameters are appended after `Destination::Count` (`paramDestinationFor`), in list order (M4's 89, then M5/M6's 52). `maxDestinations` is 512 (411 used: M7.3's pickup and M7.5's input parameters came after M7.2's BODY).
  - LFO 5–16 and ENV 6–16 are appended sources. Mod slots 33–64 are appended parameters.
  - Appended choices: warp modes PD Saw..PD Res III are 10–15; amp envelope MSEG is 16; excite Tine 7, Reed 8; oscillator mode Live 4; wavetable choices User 5–16 after User 4; mod source Input Env after LFO 16.
- **Randomness:** new features get their own random generators (as `lfoPoolRandom` and the FM noise operator's `fmNoiseRandom` do), so existing presets' random sequences don't shift. Physical string banks have explicit seeds for the same reason.
- **Per-block parameter reads** use a `ParamRef` member (resolved once), not a string built each block (CPU pass).
- **New features default to the old behaviour** and skip their code while unused (feedback type, noise, key scaling, PD chain), so old presets render bit-identically.
- **Sounds are fitted to a reference before they count as done** (ROADMAP, "When a milestone is done"). Tools: `tools/analyse_note.py`, `tools/fit_piano.py`, `tools/fit_ep.py`, `ilanaRefHost`, `ILANA_NOTE_DEBUG`, `ILANA_RENDER_DEMO`, `ILANA_PRESET_OVERRIDES`. Reference recordings go in `build/reference/` (not committed).
- **Each milestone ends with** all targets building, `ilanaTableTest` and `ilanaSnapshot --uitest` at 0 failures, the fingerprint check, a commit, and an `mN-done` tag.

## Verify
```
cmake --build build --config Release
build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe
build/ilanaFxTest_artefacts/Release/ilanaFxTest.exe
build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest
build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/after.csv
```
`ILANA_M5_TEST=1 ilanaTableTest.exe` runs only the M5/M6/M6b tests (about a minute).

## Open issues
- **EP fit gap:** tine 124, reed 135 (both from about 2000; listening round passed 2026-09-27); the rest is mostly the onset spectrum and between-partial noise (see the history file).
- **ilanaSynth FX:** DRY is not delayed to match the oversamplers' latency; an input into a Classic body does nothing (material bodies only); live grains' randomness is unseeded, so their exact output varies run to run.
- **Host automation** of the wavetable choice now also maps differently (the list grew by 12), like the other grown lists below.
- **CPU tests on this machine:** it runs Windows' "Silent" power plan with background load, and the heavy-patch test swings 42–84% between identical runs (limit 50%). Compare builds by alternating runs (`ILANA_BENCH=1`) against a baseline build in a worktree (`git worktree add ../ilana-baseline <commit>`, then configure with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<this build>/_deps/juce-src`). Measured that way, M5/M6 cost about +2% (heavy 42.4% against 41.6%).
- **Flaky tests:** the Grain Choir tuning estimate, Glitch Gate's loudness, and "Eco is cheaper" in the six-oscillator CPU check (the two differ by a few % and machine load swings more). All pass on rerun.
- **BODY is a tuned, high-Q resonator:** a drive a few cents off the note (a Physical string sits about 5 cents flat; vibrato) misses its first mode. Plucks ring it through STRING TO BODY coupling, as the Hybrid presets do.
- **Glass fit check** reads an MP3 reference and needs `pip install soundfile`; it was checked by Codex, not rechecked after the gain change (the gain is one factor for all modes, so ratios and decays are unchanged, as bar/plate/bell confirm).
- **Plain FM feedback** splits into a buzz at half the sample rate above about 0.2 (the one-sample loop does this in any ideal renderer too). It is kept for old patches; the new Filtered type is the fix.
- **Host automation** of a choice parameter whose list grew (warp, amp envelope, mod destination) maps to other entries, because hosts store it normalised. Saved sessions are fine (they store the index).
- **Piano:** fitted to the Iowa grand (error 90.7) but not yet convincing. More work waits for M17, with a time limit.
- The MAIN LFO card relayouts on a showing-timer only.
