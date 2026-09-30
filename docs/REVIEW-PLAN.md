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
9. Lowest: a self-hosted CI runner; a GPU renderer for macOS and Linux.
