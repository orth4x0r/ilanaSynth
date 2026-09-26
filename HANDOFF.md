# Current handoff

Updated 2026-09-26. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit at the same time.

**Keep this file short** (under about 60 lines): current state, next step, open issues and the rules below. When a milestone is done, move its detailed notes to the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).

## State
- **Done:** M1–M6b (see [ROADMAP.md](ROADMAP.md)). M5 (deep FM), M6 (phase distortion) and M6b (64 mod slots) were done in one pass; their listening round passed. Notes are at the end of the history file.
- **Release:** v1.2 is done (M1–M6b). The version is 1.2.0 (CMake, header, installer) and tagged `v1.2.0`. The installer has not been rebuilt.
- **Next:** M7 (v1.3), starting with M7.1, the Generative card. The roadmap after v1.2 was regrouped on 2026-09-26: M7 = generative, bodies, electric pianos, wavetable editor, audio input (old M7, M8, M4b, M8b, M9); M8 = west coast, filters, feedback guitar/Evolve, polish (old M10–M12); M9 portability (old M12b); M10 presets (old M17); v1.4 is M11–M14 (old M13–M16).
- **Git:** M5–M6b are committed, tagged `m5-done`, `m6-done`, `m6b-done`, and pushed to `origin/main` (2026-09-26), with the UI/debug pass and the 1.2 release commit. Push only when the user asks.
- **Plugin:** installed to `C:Program FilesCommon FilesVST3` (2026-09-26, with the signal-flow fix).
- **Folder cleanup (2026-09-26):** old snapshots, demos, fit runs, logs and the v1.1 portable binaries are deleted. `build/` keeps the CMake tree, `build/reference/` (the Iowa piano notes, for M17) and `build/fit-best-final.json`; `snapshots/v12` is the latest UI set. Demos come back with `ILANA_RENDER_DEMO=build/demo`.

## Demos
`ILANA_RENDER_DEMO=build/demo` writes `build/demo/fm-pd/` (DX reference patches and the CZ waves) alongside the keys demos.

## Rules that still apply
- **Old presets must not change.** Check with `ilanaFingerprint` before and after, then `python tools/compare_fingerprints.py before.csv after.csv`: 0 of 241 changed. `build/fingerprints-v1.2.csv` is the v1.2 baseline.
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
- **CPU tests on this machine:** it runs Windows' "Silent" power plan with background load, and the heavy-patch test swings 42–84% between identical runs (limit 50%). Compare builds by alternating runs (`ILANA_BENCH=1`) against a baseline build in a worktree (`git worktree add ../ilana-baseline <commit>`, then configure with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<this build>/_deps/juce-src`). Measured that way, M5/M6 cost about +2% (heavy 42.4% against 41.6%).
- **Flaky tests:** the Grain Choir tuning estimate, Glitch Gate's loudness. Both pass on rerun.
- **Plain FM feedback** splits into a buzz at half the sample rate above about 0.2 (the one-sample loop does this in any ideal renderer too). It is kept for old patches; the new Filtered type is the fix.
- **Host automation** of a choice parameter whose list grew (warp, amp envelope, mod destination) maps to other entries, because hosts store it normalised. Saved sessions are fine (they store the index).
- **Piano:** fitted to the Iowa grand (error 90.7) but not yet convincing. More work waits for M17, with a time limit.
- The MAIN LFO card relayouts on a showing-timer only.
