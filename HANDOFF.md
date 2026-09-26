# Current handoff

Updated 2026-09-26. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit at the same time.

**Keep this file short** (under about 60 lines): current state, next step, open issues and the rules below. When a milestone is done, move its detailed notes to the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).

## State
- **Done:** M1–M4 (see [ROADMAP.md](ROADMAP.md)). The last sound work was the sympathetic strings pass and the piano fit (`e480c44`).
- **Next:** M5, Deep FM.
- **Git:** everything is committed. `main` is 11+ commits ahead of `origin/main` and has not been pushed; push only when the user asks.
- **Plugin:** built locally, not installed.

## Rules that still apply
- **Old presets must not change.** Check with `ilanaFingerprint` before and after, then `python tools/compare_fingerprints.py before.csv after.csv`: 0 of 235 changed.
- **Parameter IDs and choice indices are never renumbered.** New choices, destinations and sources are appended.
  - Mod destinations: the 115 legacy parameter destinations are fixed at 96..210; OSC 4–6 start at 211; newer parameters are appended after `Destination::Count` (`paramDestinationFor`). `maxDestinations` is 512.
  - LFO 5–16 and ENV 6–16 are appended sources.
- **Randomness:** new features get their own random generators (as `lfoPoolRandom` does), so existing presets' random sequences don't shift. Physical string banks have explicit seeds for the same reason.
- **Sounds are fitted to a reference before they count as done** (ROADMAP, "When a milestone is done"). Tools: `tools/analyse_note.py`, `tools/fit_piano.py`, `ILANA_NOTE_DEBUG`, `ILANA_RENDER_DEMO`, `ILANA_PRESET_OVERRIDES`. Reference recordings go in `build/reference/` (not committed).
- **Each milestone ends with** all targets building, `ilanaTableTest` and `ilanaSnapshot --uitest` at 0 failures, the fingerprint check, a commit, and an `mN-done` tag.

## Verify
```
cmake --build build --config Release
build/ilanaTableTest_artefacts/Release/ilanaTableTest.exe
build/ilanaSnapshot_artefacts/Release/ilanaSnapshot.exe --uitest
build/ilanaFingerprint_artefacts/Release/ilanaFingerprint.exe build/after.csv
```

## Open issues
- **Flaky tests:** the Grain Choir tuning estimate (time-seeded grain pitch, now skipped by the tuning test), Glitch Gate's loudness (once 21 dB under the median), and the heavy-patch CPU test (fails on Windows' Silent power plan). All pass on rerun.
- **Piano:** fitted to the Iowa grand (error 90.7) but not yet convincing. More work waits for M17, with a time limit.
- The MAIN LFO card relayouts on a showing-timer only.
