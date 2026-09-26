# Current handoff

Updated 2026-09-26. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit at the same time.

**Keep this file short** (under about 60 lines): current state, next step, open issues and the rules below. When a milestone is done, move its detailed notes to the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).

## State
- **Done:** M1–M4 and M6b (see [ROADMAP.md](ROADMAP.md)). **M5** (deep FM) and **M6** (phase distortion) are built, tested and fitted, but wait for their listening round. All three were done in one pass; notes are at the end of the history file.
- **Next:** the listening round for M5/M6 (below), then M7, the Generative card.
- **Git:** M5–M6b are committed locally and tagged `m6b-done`; tag `m5-done` and `m6-done` once the listening round passes. Not pushed. Push only when the user asks.
- **Plugin:** built locally, not installed.

## Listening round (M5/M6, still to do)
`ILANA_RENDER_DEMO=build/demo` writes `build/demo/fm-pd/`: the three DX reference patches (ours and the ideal renderer, `*-reference.wav`) and the six CZ waves with a DCW sweep. The numeric fits pass (below); the milestones' sound-fitting rule still asks for a listen.

## Rules that still apply
- **Old presets must not change.** Check with `ilanaFingerprint` before and after, then `python tools/compare_fingerprints.py before.csv after.csv`: 0 of 241 changed.
- **Parameter IDs and choice indices are never renumbered.** New choices, destinations and sources are appended.
  - Mod destinations: the 115 legacy parameter destinations are fixed at 96..210; OSC 4–6 start at 211; newer parameters are appended after `Destination::Count` (`paramDestinationFor`), in list order (M4's 89, then M5/M6's 52). `maxDestinations` is 512 (394 used).
  - LFO 5–16 and ENV 6–16 are appended sources. Mod slots 33–64 are appended parameters.
  - Appended choices: warp modes PD Saw..PD Res III are 10–15; amp envelope MSEG is 16.
- **Randomness:** new features get their own random generators (as `lfoPoolRandom` and the FM noise operator's `fmNoiseRandom` do), so existing presets' random sequences don't shift. Physical string banks have explicit seeds for the same reason.
- **New features default to the old behaviour** and skip their code while unused (feedback type, noise, key scaling, PD chain), so old presets render bit-identically.
- **Sounds are fitted to a reference before they count as done** (ROADMAP, "When a milestone is done"). Tools: `tools/analyse_note.py`, `tools/fit_piano.py`, `ILANA_NOTE_DEBUG`, `ILANA_RENDER_DEMO`, `ILANA_PRESET_OVERRIDES`. Reference recordings go in `build/reference/` (not committed).
- **Each milestone ends with** all targets building, `ilanaTableTest` and `ilanaSnapshot --uitest` at 0 failures, the fingerprint check, a commit, and an `mN-done` tag.

## Verify
```
cmake --build build --config Release
build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe
build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest
build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/after.csv
```
`ILANA_M5_TEST=1 ilanaTableTest.exe` runs only the M5/M6/M6b tests (about a minute).

## Open issues
- **CPU tests on this machine:** it runs Windows' "Silent" power plan with background load, and the heavy-patch test swings 42–84% between identical runs (limit 50%). Compare builds by alternating runs (`ILANA_BENCH=1`). Measured that way, M5/M6 cost about +2% (heavy 42.4% against 41.6%).
- **Flaky tests:** the Grain Choir tuning estimate, Glitch Gate's loudness. Both pass on rerun.
- **Plain FM feedback** splits into a buzz at half the sample rate above about 0.2 (the one-sample loop does this in any ideal renderer too). It is kept for old patches; the new Filtered type is the fix.
- **Host automation** of a choice parameter whose list grew (warp, amp envelope, mod destination) maps to other entries, because hosts store it normalised. Saved sessions are fine (they store the index).
- **Piano:** fitted to the Iowa grand (error 90.7) but not yet convincing. More work waits for M17, with a time limit.
- The MAIN LFO card relayouts on a showing-timer only.
