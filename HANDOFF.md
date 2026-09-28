# Current handoff

Updated 2026-09-28. The source tree is the source of truth. Work on `main`; Claude and Codex take turns and should not edit at the same time.

**Keep this file short** (under about 60 lines): current state, next step, open issues and the rules below. When a milestone is done, move its detailed notes to the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).

## State
- **Done:** M1–M7, M8.1 (chaos and physics modulators) M8.2 (piano rework: the Piano exciter, a dense soundboard, the Grand Piano preset; re-fitted in M10 to the Iowa grand: whole keyboard 548 → 465) M8.3 (west-coast voice: wavefolder and vactrol low-pass gate, the WEST card) M8.4 (13 filter models appended, a two-page type grid) M8.5 (the Feedback exciter, Evolve, the vector pad on a new VECTOR tab) M8.6 (BOUNCE: resample the patch into an oscillator's sample or a patch wavetable, saved inside the patch) M8.7 (a PHYSICAL page with an animated string, exciter and body; tour and README) M10 (114 presets, 80 more wavetables; the low-pass gate and feedback guitar re-fitted to recordings) and M9 (CLAP everywhere, AU on macOS, Linux builds, CI on three platforms, pluginval and clap-validator clean, signed macOS releases once the Apple secrets exist; version 1.3.0), by Claude in a cloud session on Linux, 2026-09-27/28. Notes for every milestone are at the end of [docs/HANDOFF-HISTORY.md](docs/HANDOFF-HISTORY.md).
- **Now:** the release polish in ROADMAP (seven UI/UX rounds alternating with six debug rounds); UI 1-6 and debug 1-5 are done.
- **Next:** v1.3 is feature-complete (M7–M10). Left: the listening rounds below, building and installing on the Windows PC, and adding the Apple signing secrets for notarised macOS releases. Then v1.4 (ROADMAP, unscheduled). More piano fitting: `python tools/fit_piano2.py N --both`, then `python tools/bake_piano2.py build/fit82/best-board.json build/fit82/extra.txt` (set the printed preset values in Grand Piano by hand, then re-match `outputGain` to the fingerprint level: the metric ignores level).
- **Pending listening rounds (the user's):** M8.1's demos in `build/demo/m81/` (random shapes against Vital by ear); M8.2's in `build/demo/m82/` (Grand Piano against Hammered Strings, and A/B files: the Salamander note, then ours); M8.6's in `build/demo/m86/` (bounces played back); M8.3's in `build/demo/m83/` and M8.5's in `build/demo/m85/` (re-rendered after their M10 re-fits to recordings); M10's in `build/demo/m10/` (every pack preset, numbered by category). The piano's remaining gap is listed in the history file.
- **References** (`build/reference/`, not committed; sources and licences in `build/reference/SOURCES.md`): the Iowa MIS grand (whole keyboard at pp/mf/ff, `piano/iowa/`), the Salamander grand (`piano/SalamanderGrandPiano/`), struck Make Noise Optomix low-pass gates (`lpg/`), guitar feedback (`feedback/`).
- **CI:** GitHub Actions is manual-only (no minutes left on the account): run `tools/verify.sh` before pushing; run the CI workflow by hand for Windows and macOS.
- **Release:** the version is 1.3.0 (v1.2 is tagged `v1.2.0`). `.github/workflows/release.yml` (macOS package, Windows installer, Linux archive, draft release) and `ci.yml` (Windows, macOS, Linux) run by hand from the Actions tab.
- **Git:** M8 work is on branch `main-717xmh`, pushed after each sub-milestone. Tags `m8.1-done` onwards exist only in the cloud session (its git proxy refuses tag pushes): recreate them from the commit messages if wanted.
- **Plugin:** the installed VST3s (2026-09-27) predate M8; build and install on the Windows PC (`build-and-install.cmd`).
- **Two plugins:** `ilanaSynth` (instrument) and `ilanaSynthFX` (`ILANA_FX=1`, an effect with audio input) build from the same sources and share parameters and presets. FX-only code is behind `IlanaSynthAudioProcessor::isEffectBuild`.
- **Linux:** the tests and tools build headless with gcc and Ninja (see the M8.1 history notes for the packages). Compare fingerprints only against a baseline from the same platform.

## Demos
`ILANA_RENDER_DEMO=build/demo ilanaTableTest` writes the keys demos, `build/demo/fm-pd/` and one folder per M8 milestone (`m81/`...); `ILANA_DEMO_ONLY=m81` renders one milestone's. `ILANA_RENDER_DEMO=build/demo/fx ilanaFxTest` writes the FX presets over drums, plucks and a voice. The EP references are in `build/reference/ep/` and the fitted notes in `build/fit-ep/<model>/base/`.

## Rules that still apply
- **Old presets must not change.** Check with `ilanaFingerprint` before and after, then `python tools/compare_fingerprints.py before.csv after.csv`. `build/fingerprints-m7.5.csv` is the current Windows baseline (256 presets; the first 250 match `build/fingerprints-m7.2.csv`); on Linux, `tests/fingerprints-linux.csv` (committed; CI compares against it) (with a row per filter model; Live Body and Live Wah always differ run to run) (`build/fingerprints-v1.2.csv` is v1.2's). New presets are appended at the end of the list. Against v1.2, 26 reverb presets differ at the tool's 48 kHz only, because the reverb now gets the real sample rate. At 44.1 kHz they are unchanged. Two more (Through-Zero Growl, Shift Bass) differ only because voice random state carries over between presets within the run. Debug round 1 (release polish) changed 38 presets' fingerprints on purpose (continuous knobs; a patch load now resets tails and modulators, so fingerprints no longer depend on the preset before; see ROADMAP) and updated the Linux baseline.
- **Parameter IDs and choice indices are never renumbered.** New choices, destinations and sources are appended.
  - Mod destinations: the 115 legacy parameter destinations are fixed at 96..210; OSC 4–6 start at 211; newer parameters are appended after `Destination::Count` (`paramDestinationFor`), in list order (M4's 89, then M5/M6's 52). `maxDestinations` is 512 (411 used: M7.3's pickup and M7.5's input parameters came after M7.2's BODY).
  - LFO 5–16 and ENV 6–16 are appended sources. Mod slots 33–64 are appended parameters.
  - Appended choices: warp modes PD Saw..PD Res III are 10–15; amp envelope MSEG is 16; excite Tine 7, Reed 8; oscillator mode Live 4; wavetable choices User 5–16 after User 4; mod source Input Env after LFO 16, then LFO 1 B..16 B; LFO shapes 16–29 (M8.1).
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
`ILANA_M5_TEST=1 ilanaTableTest.exe` runs only the M5/M6/M6b tests (about a minute); `ILANA_M81_TEST=1` only M8.1's (likewise `ILANA_M82_TEST` ... `ILANA_M86_TEST`). `ILANA_POLISH_TEST=1` runs the release-polish regressions, `ILANA_LIBRARY_TEST=1` the factory library check (`ILANA_LIBRARY_LEVELS=1` prints each preset's level against the median), `ILANA_LEVEL_PROBE="A|B"` renders presets fresh and prints their levels, `ILANA_PARAM_TEXT=1` prints every parameter's help and its text at min/default/max, `ILANA_KS_TEST=1` the string tests.

## Open issues
- **EP fit gap:** tine 124, reed 135 (both from about 2000; listening round passed 2026-09-27); the rest is mostly the onset spectrum and between-partial noise (see the history file).
- **ilanaSynth FX:** DRY is not delayed to match the oversamplers' latency; an input into a Classic body does nothing (material bodies only); live grains' randomness is unseeded, so their exact output varies run to run.
- **Host automation** of the wavetable choice now also maps differently (the list grew by 12), like the other grown lists below.
- **CPU tests on this machine:** it runs Windows' "Silent" power plan with background load, and the heavy-patch test swings 42–84% between identical runs (limit 50%). Compare builds by alternating runs (`ILANA_BENCH=1`) against a baseline build in a worktree (`git worktree add ../ilana-baseline <commit>`, then configure with `-DFETCHCONTENT_SOURCE_DIR_JUCE=<this build>/_deps/juce-src`). Measured that way, M5/M6 cost about +2% (heavy 42.4% against 41.6%).
- **Flaky tests:** the Grain Choir tuning estimate and Glitch Gate's loudness. All pass on rerun. ("Eco is cheaper" now takes the best of three interleaved passes.)
- **BODY is a tuned, high-Q resonator:** a drive a few cents off the note (a Physical string sits about 5 cents flat; vibrato) misses its first mode. Plucks ring it through STRING TO BODY coupling, as the Hybrid presets do.
- **Glass fit check** reads an MP3 reference and needs `pip install soundfile`; it was checked by Codex, not rechecked after the gain change (the gain is one factor for all modes, so ratios and decays are unchanged, as bar/plate/bell confirm).
- **Plain FM feedback** splits into a buzz at half the sample rate above about 0.2 (the one-sample loop does this in any ideal renderer too). It is kept for old patches; the new Filtered type is the fix.
- **Host automation** of a choice parameter whose list grew (warp, amp envelope, mod destination) maps to other entries, because hosts store it normalised. Saved sessions are fine (they store the index).
- **Piano:** fitted to the Iowa grand (error 90.7) but not yet convincing. More work waits for M17, with a time limit.
- The MAIN LFO card relayouts on a showing-timer only.
- **pluginval on Linux CI** crashed on exit in some runs (not reproducible locally or under ASan); CI runs it under gdb to capture backtraces.
