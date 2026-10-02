# Review plan: state and what's next

Updated 2026-09-30, at the move from the Windows PC to cloud sessions.
Review: claude.ai/artifact/LAbrHT1KnLVEiv8Yd3SjHS. Projected "potential" review
(what the synth should be once this plan ships): claude.ai/artifact/XecerytfKfBKdepvZLxKZt.

## The user's decisions (standing)
- Personal use only. Commercial work (licensing, store, installer polish) is parked and forgotten.
- Downloads of samples, wavetables, reference presets: allowed.
- Presets are made by automated pipelines only (no hired people). The user A/B-tests by ear.
- Positioning: "physical hybrid" synth.
- Old presets may change in sound, but must never break level-wise (no silent, clipping or wildly quieter presets).
- Filters: replace the old ones, improve them massively, make warps and the other spectral stuff modulatable.
  The user also suggests reusing Airwindows filters where they fit (there are good ones: e.g. Air, Baxandall,
  Capacitor, Isolator, Pop, Coils, and the uncoded Airwindows filter set).
- Everything from the review except scripting, plus SoundFont (SF2/SFZ) support.
- AI agents maintain the synth for years: keep docs short, current and machine-readable.
- 32 voices, 8 macros. Keep Grand Piano but rename it (it's "not a piano").
- The user has no host automation in sets (choice params made non-automatable).
- At most 2 agents in parallel (weekly usage).

## Done (all on main)
| Commit | What |
|---|---|
| addda8c | FX DRY delayed by the oversampling latency |
| f1fc370 | Live grains and string seeds repeatable; the input rings the Classic body; Init opens the filter; Filtered FM feedback by default; `ilanaPresetRender`, `tools/preset_critic.py`, `ILANA_PLAN_TEST` |
| f71b1e9 | 460 choice params non-automatable |
| 1885678 | Useless tests purged, loose ones tightened |
| b8200b4 | CPU: SSE2 Sync warp, one tanh for mono sources, 32 voices, DC blockers, `src/PresetTrims.h` |
| a9006ba, 3fb7c0d | Scala microtuning; a loaded scale survives factory presets |
| 39a2f21 | 8 macros; MTS-ESP client; level + macro trims for 289 presets; A/B clip tool; `tools/build_content.py` |
| 1cc7c25 | Airwindows FX (39 algorithms); Lead glide divided by 3 at load; `tools/preset_diversity.py` |
| c911267 | Filter overhaul groundwork (see below) |
| claude/project-thread-xhtug0 (2026-09-30, cloud) | Gate clean on Linux (a merge leftover broke the test build); new Linux fingerprint baseline; Lead A/B clips re-rendered with the reduced glide; docs/ARCHITECTURE.md, docs/DECISIONS.md; **preset diversity pass** (below) |
| claude/project-thread-xhtug0 (2026-09-30, cloud) | **Filter overhaul models** (step 4): all 25 rebuilt on solved zero-delay cores, in tune at self-oscillation, every preset's level held; 303 Acid, Moog Drive, Vowel Morph, Comb Body; filter tests; SPECTRAL AMOUNT is a modulation destination. See `docs/filter-overhaul-status.md` |
| claude/project-thread-xhtug0 (2026-09-30, cloud) | **Weak macros** (step 5): 106 macros on 85 presets rewired in `src/PresetVoicing.h`; `applyDefaultMacros` skips macros the voicing wires; presets re-levelled (`tune_presets.py`, two passes) |
| claude/project-thread-xhtug0 (2026-09-30, cloud) | **Interface** (step 6): PATCH card on PLAY, VECTOR pad fills its card, PHYSICAL preview, mod-ring UI test, physical-hybrid positioning |
| claude/project-thread-xhtug0 (2026-09-30, cloud) | **Features** (step 7): SF2/SFZ multisamples, FX splitters, vocoder, clip sequencer |

Diversity pass (step 3, 2026-09-30): `src/PresetVoicing.h` lays parameter changes and macro
rewiring over 172 factory recipes (Airwindows saturation and spaces instead of Hall, dry and mono,
lo-fi, wide, slow swells, decaying, echo, rhythm, other registers, tables and filter types). Spread
against the Vital/Surge references: Pad 53 -> 80 %, Bass 57 -> 80 %, Lead 66 -> 80 %, Keys 76 -> 90 %,
Pluck 68 -> 82 %; near-duplicate pairs roughly halved in every category. Crowds (the tool's greedy
groups) are still large: the target "no crowd bigger than ~4" is not met. Levels re-trimmed with
category loudness pinned at the old medians (`PINNED_TARGETS` in `tools/tune_presets.py`); the
fitted pianos kept their sound. Iterate with `ILANA_PRESET_VOICING=<file> ILANA_RENDER_CATEGORY=Pad
ilanaPresetRender`, then `tools/bake_voicing.py`.

CPU: the heavy benchmark went from 42-84 % to 19-25 %. Library loudness spread (10-90 %) 17.5 dB to 8 dB.

## Gate state at the move (fixed 2026-09-30: clean on Linux, see Done)
The last Windows run: FX tests and `--uitest` pass; 9 of 396 fingerprints moved (lead glide, expected).
Two table-test failures, both believed fixed or environmental but NOT re-verified:
- Vocal Chop -14.4 dB under the median (limit 14): its trim is now 0 dB (`LEVEL_OVERRIDES` in `tools/tune_presets.py`).
- The 64-slot matrix CPU test (499.9 ms/s): a parallel build was loading the machine.
First job in the cloud: build on Linux, run the gate, make a Linux fingerprint baseline.

## Content in the repo
- `content/samples/`: 365 CC0 samples from VCSL (0.76 GB), by category, 48 kHz 24-bit. The synth looks in
  `~/Documents/ilanaSynth Samples`: copy or symlink `content/samples` there.
- `content/wavetables/`: 147 AKWF tables (CC0). Link to `~/Documents/ilanaSynth Wavetables`.
- `tools/build_content.py` rebuilds both from their sources.
- `content/analysis/`: the critic's `report.csv`/`index.csv`, CLAP embeddings (`clap_embeddings.npy`)
  and reports for our library (`critic-v4`) and the references (`critic-refs` = 57 Vital presets,
  `critic-refs-surge` = 200 Surge XT patches). The WAV renders (5 GB) are not committed: re-render ours with
  `ilanaPresetRender`; `preset_diversity.py` works from the committed embeddings alone for the references.
  To re-render references, install Vital / Surge XT on Linux and use `ilanaRefHost --presets`.
- `content/analysis/ab-page/`: the A/B page's clips and manifest (artifact claude.ai/artifact/ESuBEENUzTpvuemWi3rXTM,
  votes in its db `votes` collection; 54+ votes so far).
- CLAP scoring: `pip install torch transformers`, model `laion/clap-htsat-unfused` (downloads itself).

## Next, in order
1. ~~Gate on Linux~~ (done).
2. ~~Re-render the Lead A/B clips~~ (done; all categories re-rendered again after step 3).
3. **Preset diversity** (first pass done, see Done; left: the user's A/B verdicts on the redesigns,
   and the crowds). Was: **Preset diversity (the user's top complaint after levels):** presets sound alike. Measured spread against
   the references: Bass 57 %, Pad 53 %, Lead 66 %, Keys 76 %, Pluck 68 %; 50 of 57 pads form one crowd.
   Causes: reverb on nearly everything (Hall 111x), FX chains nearly all Reverb / Delay+Reverb / Chorus+Reverb,
   macro 4 = SPACE/HALL/ROOM/ECHO on ~240 presets, 1 ms amp attack almost everywhere.
   Redesign each category (Pads first) around distinct archetypes; vary FX (dry, Airwindows, distortion,
   no reverb), macros, attack, register and motion; merge duplicates; fill the gaps listed in
   `content/analysis/critic-v4/diversity.md`; use the sample library and the physical engine.
   Target: spread at least ~80 % of the references', no crowd bigger than ~4, levels at the category target.
   Loop: `ilanaPresetRender` -> `preset_critic.py` -> `clap_score.py` -> `preset_diversity.py`, plus the user's A/B votes.
4. **Filter overhaul:** models done (status in `docs/filter-overhaul-status.md`: 25 rebuilt, 4 added, levels held,
   Airwindows' filters checked and not reused). Spectral amount is modulatable (block rate). **Done.**
5. **Weak macros:** ~80 presets whose macros barely do anything (`tools/tune_presets.py` lists them): fix by hand. **Done** 2026-09-30: 106 weak macros on 85 presets rewired in `src/PresetVoicing.h` (four more by hand after re-levelling: Arp Glass, S&H Techno, Oversampled Grind, Metal Hat); auto-mapped presets keep the voicing's macros; re-levelled. None left under 1.0 except the exempt ones (GLIDE, GATE, PEDAL, SWING, SPRAY).
6. **Interface:** a live patch view instead of PLAY's empty ADD OSCILLATOR box; the VECTOR pad fills its card
   and the evolve rows fit 8; a PHYSICAL page preview when no oscillator is physical; verify mod rings; identity pass.
   **Done** 2026-09-30: PLAY's spare tile is a PATCH card (the live, clickable signal flow, ADD OSC in its header;
   a plain ADD button when the tile is short); the VECTOR pad fills its card's height (EVOLVE narrower; its 8 rows
   already fit); PHYSICAL previews the string, plucking itself every 3 s, when the chosen oscillator isn't physical;
   rings checked end to end by a UI test (all 408 knob destinations show their depth; knobs on parameters that are
   not mod destinations, such as KEY TRK, UNISON and the Airwindows knobs, have none); identity: README opener and
   the plugin description say "physical hybrid synthesizer". Left for the user: a new logo, colours or type, and an
   Init patch that opens on a hybrid sound (it would change Init's sound).
7. **Features:** SoundFont (SF2/SFZ) in the Sample oscillator; FX splitters (multiband/LR/MS); vocoder; clip sequencer.
   **Done** 2026-09-30: SF2 (first preset) and SFZ load as key/velocity zones in the Sample oscillator (drop or right-click the display);
   any FX slot can work on one band (Low / Mid / High with two LR4 crossovers, or Mid / Side), the rest passing untouched;
   the vocoder is FX type 31 (audio input or a built-in Talk modulator, 8-24 bands); the clip sequencer plays 8 clips
   saved in the patch (Key transpose from C3 or Host play), with a CLIP tab piano roll and .mid import. Each has its
   test suite (`ILANA_MULTISAMPLE_TEST`, `ILANA_SPLITTER_TEST`, `ILANA_VOCODER_TEST`, `ILANA_CLIP_TEST`); old presets unchanged.
   Left for the user: listening to all four. Not done: mod-matrix destinations for the vocoder parameters.
8. **Maintenance for agents:** rename Grand Piano; split PluginEditor.cpp and PluginProcessor.cpp;
   trim slow tests (M81 coverage, arp release). (Architecture map, decision log and the README typo: done.)
   **Done** 2026-09-30: Grand Piano renamed Felt Hammer Board (same slot and sound); the editor's pages in
   `src/gui/pages/*.h`, the processor's areas in `src/processor/*.cpp`; `ILANA_TEST_TIMES=1` prints each suite's time
   (on Linux M81 and the arp tests take 4-7 s; the slow ones were two polish tests, 180 s to 48 s; full run ~300 s).
9. **Airwindows filters** (the user, 2026-09-30): Low / Band / High Pass and Notch replaced by Airwindows' Y filters
   (done, `docs/filter-overhaul-status.md`); Airwindows character filters (Z, X, YNot, Acid, Holt, Angle, Pear) and a
   tuneable **Disperser** (all-pass chain at the cutoff, key-tracked) as appended types; then a second diversity pass
   that uses them on the crowds. Presets may change tone "as long as they don't sound bad"; the level rule stands.
   **Second diversity pass done** 2026-09-30: 62 crowd presets (18 Bass, 15 Pad, 11 Lead, 8 Keys, 10 Pluck)
   re-voiced in `src/PresetVoicing.h` with filter types 29-38 (Acid squelch and Pear/Holt/YNot on basses, Disperser zaps
   with KEY TRK 1 on plucks and percussive basses, Holt/Pear/Angle warmth and Z high/band-pass on pads and keys, Z drive on
   leads); a TONE/CUTOFF/MORPH macro per preset now sweeps cutoff and MORPH. Levels re-fitted to the baseline per preset with
   `tools/match_levels.py` (level trims, iterate: fingerprint, `--apply`, rebuild; 0 of 62 break the level rule). Result
   (`content/analysis/diversity-pass2.md`; reports and embeddings in `content/analysis/critic-v5`, the five focus categories only): spread Bass 78 -> 81 %, Pad 81 -> 85 %, Pluck 82 -> 85 %, Lead 76 %, Keys 88 %
   (unchanged); largest crowds Bass 12 -> 11, Pad 21 -> 18, Lead 21 -> 20, Keys 22 -> 25 (greedy grouping, noisy). The filter
   alone moves CLAP little (a low-pass or all-pass change is ~0.03-0.1 cosine distance; only high/band-pass thinning moves it
   a lot and those break the level rule's peak limit), so the crowds are mostly set by oscillators and FX: the next pass has
   to change those, not the filter. A/B clips: `/mnt/project-files/ilanasynth/ab-filters/manifest.json` (`tools/ab_pairs.py`).
10. **UI review 1 fixes** (Vital / Serum 2 comparison, `/mnt/project-files/ilanasynth/ui-review/UI-REVIEW-1.md`):
   PLAY gets a live output scope or spectrum and SUB + NOISE collapses when off; every knob shows its live modulated
   value, and clicking a source chip highlights the knobs it drives (rings already cover all mod destinations);
   drag cutoff / resonance on the filter graph, frame / warp on the oscillator display, a spectrum view beside 3D;
   preset browser audition (arrow keys), category chips and macro names; group the header's actions and give save
   and the preset name weight; move VOICES / BEND / VOICE MODE out of the bottom bar to free room for macro names;
   larger small-caps labels and contrast on dimmed controls. **Recurring:** a UI review against Vital and Serum 2
   after each step that touches the interface (the user's ask); findings go to `UI-REVIEW-N.md` and this list.
   **Done** 2026-09-30 (cloud, `ui-fixes-1`; screenshots in `/mnt/project-files/ilanasynth/ui-fixes-1/`, the second review in
   `.../ui-review/UI-REVIEW-2.md`); no audio or preset change (fingerprints: 0 of 400 presets changed):
   1. Every mod-destination knob draws a marker disc where the modulation currently puts it, on the existing arc;
      clicking a bottom-bar source chip pins it (thicker halo on every knob it drives, chip outlined), a second click
      clears (`pinnedModSource()` beside `highlightedModSource()`; hover still works).
   2. PLAY: SUB + NOISE folds to one line while both are off (click the line to open it for the noise knob) and a live
      OUTPUT view (`src/gui/OutputView.h`: waveform over spectrum, click cycles both / wave / spectrum) takes the room
      under PATCH when 48 px or more is free.
   3. Filter display: was absolute-position drag within 24 px of a marker and no gestures; now a drag anywhere picks the
      nearest filter (cutoff across, resonance up and down) inside one begin/endChangeGesture per parameter.
   4. Preset browser: arrow keys already loaded the next / previous preset from the search box or list; now also with
      focus anywhere in the panel (`stepSelection`). The vertical category list with counts already was the category
      filter, so no second row of chips. New: each row lists the preset's four macro names (`getFactoryMacroNames`,
      worked out without loading and checked against a real load for all 371 presets in `--uitest`; the loaded preset
      shows its live names).
   5. Header: file (star, save, menu) / edit (undo, redo, history, A/B) / tools (dice, settings) parted by rules; SAVE is
      the accent-filled key; the preset name is 19 px.
   6. Voice mode, voices and pitch-bend range moved to the settings menu (Voice mode / Voices / Pitch bend range);
      macros use the freed width (glide, legato, master stay).
   7. Knob, field and chip labels were already 11-13 px (label 11.5, knob labels 13); dimmed controls went from 30-45 %
      to 60 % opacity (`IlanaTheme::dimmedAlpha`).
   **Round 2 done** 2026-09-30 (cloud, branch `claude/ui-review-fixes-i3zmpv`, merged into PR #5; screenshots in
   `/mnt/project-files/ilanasynth/ui-fixes-2/`, review 3 in `.../ui-review/UI-REVIEW-3.md`); no audio or preset change:
   1. Oscillator display (`WaveDisplay`): drag across scrubs the frame, up / down sets the first WARP's amount when one is
      chosen (shift fine, double-click zero), one host gesture each, with a FRAME / WARP readout; the corner key cycles
      WAVE / 3D / SPEC (the cycle's harmonics, warps included). OSC 4-6 displays follow frame modulation.
   2. Resting 0.35 s on a modulated knob opens `ModHoverPopup` (src/gui/ModHoverPopup.h, one per editor, reached through
      `modHoverHooks()`): each source with colour, depth and a live bar of what it adds.
   3. Preset browser DOCK: a 340 px column right of the page; the window grows by it (`currentDesignWidth()`, the scale
      is now height / 720), loading never closes it, the preset name toggles it, FLOAT returns it to the drop-down;
      `presetBrowserDocked` / `presetBrowserDockOpen` in the settings file.
   4. A live waveform strip under the preset name on every page (`OutputView::setStrip`; a click opens the scope).
   5. VOICES in the status line opens the voice settings (`showSettingsMenu (true)`) and reads MONO / LEGATO when not Poly.
   PHYSICAL and VECTOR empty states were already done in step 6. Tests in `--uitest`; `ILANA_SNAPSHOT_EXTRAS=1
   ilanaSnapshot <dir> <preset>` renders the new views. Open from review 3: macro names are nearly the same on every
   preset (content, for the preset rework), a draggable source card, macro yellow against OSC 1 gold, PHYSICAL's NO BODY
   box, a log-harmonic SPEC axis, a narrower docked layout.
   **Review 3 fixes** 2026-10-01 (same branch; screenshots in `.../ui-fixes-3/`): the source card's rows are controls
   (drag sideways or up / down for depth in one gesture, shift fine, double-click zero, right-click Bypass / Remove; the
   knob keeps the card open while the mouse is on it, `modHoverHooks().engaged`); mod arcs get a dark underlay and a
   source close in hue to the knob (macro yellow on OSC 1 gold) is drawn paler (`modArcColour`); PHYSICAL without a body
   draws only a dashed outline. Macro names were the preset rework's. Still open: log-harmonic SPEC, narrower dock.
11. ~~Lowest: a self-hosted CI runner; a GPU renderer for macOS and Linux.~~ **Done** 2026-10-01: the self-hosted gate
   workflow, `tools/setup-runner.ps1` and `tools/verify.ps1` (the user still has to register her PC:
   `docs/SELF-HOSTED-RUNNER.md`); the editor draws through OpenGL on macOS and Linux (HANDOFF, "GPU UI").
12. **Final detail review** (the user's ask, 2026-09-30): **done** 2026-10-01 (cloud, branch `claude/project-thread-mxim47`); fixes and what is left
   in `/mnt/project-files/ilanasynth/ui-review/DETAIL-REVIEW.md`. Biggest item left: generic automatic macro names (content).
13. **Detail review leftovers** (the user, 2026-10-01), in order:
   1. Per-preset macro names for the presets on automatic macros (BRIGHT / DARKEN, MORPH, DRIVE, SPACE from
      `applyDefaultMacros`): voicing lines in `src/PresetVoicing.h` with names and targets that fit each patch.
   2. DX7 names: a display-only formatter (Title Case, trailing `\` `^` junk trimmed) that keeps the saved names.
   3. Name pairs: Hypersaw / Supersaw, Self-Osc / Self Osc, Vocal Chop / Vocal Chops, Formant Scream II, Glass Keys /
      Stretched Glass Keys. Renames change fingerprint keys and saved sessions' preset names: update
      `tests/fingerprints-linux.csv` and keep the old names loadable.
   4. Mod chip row: "E6" beside "ENV 5" when crowded; shorten consistently or not at all.
   5. A DX7 envelope editor (DX7 voices show oscillator AMP ENV menus that DX7 mode ignores). **Done** 2026-10-02 (the
      user: fold DX7 mode into the synth): DX7 mode is gone; the Operator EG is an ENVELOPE choice any oscillator can use,
      edited on the FM page (see HANDOFF).
   6. Help text: "+-48 st" to ±, Rössler, Hénon.
14. **UI review 4 fixes** (the user, 2026-10-02: "add all these findings and fixes to the roadmap, then execute"). Two
   adversarial reviews against Vital and Serum 2: `/mnt/project-files/ilanasynth/ui-review/UI-REVIEW-4-VITAL.md` (V1-V31) and
   `UI-REVIEW-4-SERUM2.md` (S1-S29); merged report claude.ai/artifact/BWMkLJv7Ap8BtEw9ZQhkPL. Every finding is in one batch
   below (V/S numbers point at the full text). Rules: no audio or preset change (fingerprints unchanged), parameter IDs and
   choice orders untouched, each batch adds `--uitest` checks. Not done on purpose: a neutral Init (S18; step 6 left Init's
   sound to the user), the header A/B and history (settled).
   - **A. Undo and edit state** (V1, S1): one `beginEdit(name)` transaction per custom gesture (graphs, depth dots, card rows,
     menus, dice, EQ); clip, LFO curve, steps, MSEG and remap edits as undoable actions; EDITED counts that data; a confirm
     before a load replaces a modified patch; the tour stops promising more than undo does.
   - **B. Modulation** (V2, V8, V9, V16, V17, V18, V24, S7, S12; with 13.4): depth-dot double-click zeroes (remove moves to
     right-click); matrix rows numbered 1..n, duplicate source/destination pairs refused on drag and flagged, add button
     pinned, sortable columns, compact rows, VIA collapsed until used, polarity Unipolar / Bipolar; one colour per source
     (macros numbered, performance sources distinct, ENV 6-16 off a fixed palette, LFO 1 off the accent); pinned halo as a
     ring; chip row follows the pool, no mixed abbreviations; remap editor docked with close, shapes and a live input dot;
     MIDI learn on every control; "(35%)".
   - **C. LFOs and envelopes** (V4, V10, V29, S5, S19, S21, S22): the first drag on any LFO shape converts it to Curve;
     grid/snap on the panel; Steps reachable from the LFO, STEPS' hint a "Use on LFO n" button; MSEG grid, labels, playhead;
     envelope time ticks and a playhead dot on the curve; one RATE knob that reads divisions when synced; a 1-16 index row
     for the LFO and envelope pools.
   - **D. FX rack** (V5, V6, V7, V20, V21, S2, S3, S6, S13, S24, S26): cards sized to their knobs; a display per family
     (drive transfer curve, dynamics GR meters, delay taps, reverb decay); one MIX (Airwindows Dry/Wet pinned, slot blend in
     the card header); type menu, solo and band selector in each card header, no invisible solo zone; types already in the
     rack greyed out; names from one table (TAPE STOP), Airwindows display names, units out of labels; Airwindows category
     modules listed by function; RACK A/B, the move arrows, COPY and DICE labelled and undoable; delay's tap grid hidden
     while TAPS is off.
   - **E. Oscillators, filter, PLAY** (V11, V12, V13, V14, V22, V23, V26, V30, S4, S8, S10, S14, S15, S16, S27): filter
     graph draws the set cutoff (solid, dragged) and the modulated one (faint), markers kept inside the plot; columns
     fold their last card instead of clipping, and a UI test fails on clipped knobs at 100 % and 75 %; one dimming rule
     for controls with no effect; WAVE view without the phase playhead, frame as a readout; the same widget and order for a
     parameter on every page; the oscillator × goes; three knob sizes by role; VECTOR greys dead corners and dims while
     off; FM shows "DX7 ALG n", a CUSTOM state and ratio / level under each node; the physical card grouped by
     STRING / EXCITER / BODY.
   - **F. Browser, save, keys** (V3, V15, V25, V31, S11, S28, S29): SAVE overwrites the loaded user preset; SAVE AS is a
     themed panel that asks before overwriting and keeps legal characters; Ctrl+S, Ctrl+Shift+S, Ctrl+Left/Right, Esc;
     the browser sorts (name default), DX7 grouped by bank and out of "All", a star on each row, SURPRISE ME plain; the
     table browser gets search and consistent names.
   - **G. Clip sequencer** (S9): grid menu with triplets, velocity lane, Delete key, rubber-band selection with
     copy / paste / duplicate, audition on click (undo comes from A).
   - **H. Small things** (V19, V27, V28, S17, S20, S23, S25): tour content; 10 px minimum for interactive labels;
     scope meters with a dB scale and clip light; CPU and OUT labelled; chaos LFO outputs spelled out; SEQ GENERATE on one
     baseline per row, PROB SEQ's number row labelled.
   **Done** 2026-10-02 (cloud, branch `claude/project-thread-poh95j`, on top of PR #9's branch; one commit or merge per batch).
   Gate clean on Linux (unit, FX and UI tests, 0 of 698 fingerprints changed, pluginval 10 on both plugins). Each batch's
   `--uitest` checks are in `tools/Snapshot.cpp`. Notable choices: factory loads now clear clips (as user loads did); the
   mod polarity choice reads "Auto" (was "Natural"; display only); MIDI learn on every control saves as `midiCcMap` and
   factory loads keep it; macro CCs now drive all 8 macros and move only the macro that moved (was 1-4, others reset);
   user presets' category and tags are read back (JUCE's 8 KB header read had dropped them); text floors are 12.5 / 11 in
   the theme's font-height units (about 9.2 / 8 px em), not a global type-scale bump. Left: offering to merge duplicate
   routes (they are flagged), quantise and zoom buttons in the clip editor, a "Recent" sort (nothing records it), a live
   input dot on the FX transfer curves, the scope as a dock, DX7 rows still carry "(ROM1A)" (13.2).
