# Review 6, cycle 2 (review 7): work plan

ilana (2026-10-02): "do two full cycles of that. polish." Cycle 1 (review 6) is merged on this branch; this is cycle 2.

Sources (every finding in them is in scope; each package owns the findings in its area). IDs are the finding numbers in each file:
- `UI-REVIEW-7-VITAL.md` (V7-1..43), `UI-REVIEW-7-SERUM2.md` (S7-1..42), `UI-REVIEW-7-INTEGRATION.md` (I7-1..45).
- Each file also has a "Fixed since review 6 / still open" table: rows marked partly fixed or still open in your area are in scope too.
- Renders the reviewers made are in /home/user/shots7/ (vital-extra, serum-extra, integration-extra, and per-preset folders).

Rules: as cycle 1 (REVIEW-6-WORKPLAN.md). Parameter IDs and choice indices append only; old presets render the same (fingerprints); new features default to old behaviour; settled decisions in docs/REVIEW-PLAN.md stay (header A/B and history, 8 macros, Init sound, one FX instance per type); string literals under 16 KB; add `--uitest` checks for what you fix. Snapshot tool artefact: it flips OSC 1 to Physical and back on the OSC page.

One shared vocabulary for every package (use exactly these words):
- The DX7 operator envelope is the **Operator Env** (short **OP ENV**). Never "Operator EG", "Op EG" or "Op Env" in visible text.
- The DX7 pitch envelope is **OP PITCH**, the DX7 LFO is **OP LFO**.
- An operator's output level (dB) is **LEVEL**; the oscillator's trim (%) is **TRIM** on every page when the oscillator is an FM operator.
- Ratio is written `×1.00` everywhere.

## Packages

**Q1 FM and DX7** (FmInputPages.h, FmDiagram.h, dsp/FmAlgorithms.h, dsp/OperatorEgParams.h, processor/Dx7.cpp, Dx7Presets.h, State.cpp algorithm switching; plus the Operator Env naming strings anywhere, incl. OscPage.h, MainPage.h, TutorialOverlay.h, ParamInfo names):
I7-2, 3, 4, 6, 9, 10, 11, 12, 13, 14, 15, 33, 34, 42; V7-1, 2, 3, 9, 10, 12, 13, 14, 30, 41; S7-4, 5, 9 (SPACE part), 13, 14, 34, 41; V7-8.
Notes: Operator Env time knobs must turn the normal way (longer clockwise) and its "Attack Rate" destination likewise; presets must not change (invert the display/control mapping, not the stored DX7 rate, or convert on load without changing sound). SPACE on DX7 voices must work (e.g. reverb enabled at mix 0 if that is bit-exact dry, or point SPACE at something live); fingerprints must stay 0 changed. Algorithm change keeps feedback amount. Fixed operator card height. Op Env graph time scale usable for short segments; levels at 0 read in dB (−∞). KEYS & VELOCITY in synth units (note names, %, curve names that fit).

**Q2 Modulation** (ParamControls.h, ModHoverPopup.h, MatrixPage.h, MatrixWidgets.h, RemapEditor.h, ModSourceChip.h, MacroStrip.h, ModNames.h, chip bar in PluginEditor.cpp):
I7-5, 16, 17, 45; V7-6, 20, 27, 28, 39, 40; S7-8, 20, 33, 35.
Notes: every knob that looks modulatable accepts a source, or is visibly marked as not modulatable (prefer making it modulatable by appending destinations); chip bar folding stable across presets and zoom, folds rare sources before LFO 2/3; source menu groups OP LFO / OP PITCH / MSEG where the pools show them; new destinations use "Module › Control" names.

**Q3 MOD page pools** (EnvLfoPages.h, LfoDisplay.h, EnvelopeDisplay.h, EnvThumbs.h, LfoThumbs.h, PoolIndexRow.h, ModulePool.h, Steps/MSEG editors):
I7-7, 8, 18, 29, 30, 44; V7-5, 17, 19, 23, 24, 29 (MOD part); S7-6, 7, 26, 27, 28, 29, 40.
Notes: OP ENV / OP PITCH / OP LFO as real pool cards on MOD (editable in place, same editors as FM uses), greyed with a reason when no DX7-style operator uses them; AMP ENV dimmed with "unused" on DX7 voices without truncation; MSEG as an LFO shape mode (one drawn-shape system: drawable, Steps, MSEG all as LFO shape modes; keep the MSEG params, append only); no sideways-scrolling pools; time ticks outside the plot; one time scale; "Off" only for switches.

**Q4 PLAY, OSC, PHYSICAL, VECTOR** (MainPage.h, OscPage.h, WaveDisplay.h, physical and vector parts of FilterVectorPhysicalPages.h, physical widgets):
I7-19, 20, 21, 22, 24, 25, 26, 27; V7-4, 15, 16, 21, 26, 29 (PHYSICAL part), 31, 32, 33; S7-2, 3, 12, 21, 22, 25, 39; I7-2 PLAY/OSC side (show TRIM for operators, use Q1's words).
Notes: PLAY oscillator column without empty fixed slots (compact, one "+ ADD OSC"); knob order fixed regardless of mode/FM role; operator strip shows operator things (ratio, LEVEL dB, OP ENV mini) not FRAME; OSC page operator card without irrelevant wavetable/unison rows; VEC X / Y chips go to the chip bar (coordinate: add them as Mod sources via the existing chip spec list, Q2 owns the bar code: keep your change to one push_back); BODY in one place with one name; BOUNCE one meaning; SF2 LOAD and zone view; 3D default only for multi-frame tables.

**Q5 FILTER and FX** (FilterWidgets.h, FilterDisplay.h, filter part of FilterVectorPhysicalPages.h, FxPage.h, FxDisplays.h, FxWidgets.h, FxLibrary.h):
I7-1, 28, 32, 38, 39, 40, 43; V7-7, 18, 34, 35, 37, 42, 43, 29 (FX part); S7-1, 11, 15, 16, 23, 24, 36; V7-22 (RACK A/B part only: the header A/B is settled).
Notes: SIGNAL FLOW must never drop F1/F2 (I7-1, uitest on Felt Hammer Board); one dry/wet per FX card; switched-off cards dimmed; Airwindows as models of the family's effect where possible.

**Q6 Browser, header, global consistency** (PresetPanel.h, SavePresetOverlay.h, ConfirmOverlay, TutorialOverlay.h (except Q1's naming), ScopeDisplay.h, OutputView.h, header in PluginEditor.cpp, shared widgets StateTabs.h / section headers):
I7-23, 31, 35, 36, 37, 41; V7-25, 36; S7-9 (macro content beyond SPACE: per-category macro names on DX7 voices, keep sound), 10, 30, 31, 32, 42.
Notes: the consistency items (captions, switch placement, tab "on" indicator, casing) are done in the shared widgets so every page picks them up; touch other packages' files only for one-line call-site changes. DX7 duplicate voices: hide exact duplicates in the browser (keep the files), Title Case and expanded names.

**Q7 ARP, SEQ, CLIP, GENERATE** (ClipEditor.h, SeqPage.h, arp lanes):
V7-11, 38; S7-17, 18, 19, 37, 38.
Notes: pills as tabs with a separate on light; off-range note indicators; paint mode (drag to draw, click to place); lane readouts and an off overlay that doesn't hide the data.

**After Q1-Q7 (by the thread):** merge in order Q1, Q2, Q3, Q4, Q5, Q6, Q7, full gate, fresh renders, update HANDOFF.md, REVIEW-PLAN.md and PR #13.
