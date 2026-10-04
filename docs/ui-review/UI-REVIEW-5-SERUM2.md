# UI review 5: ilanaSynth v1.3 against Serum 2 (fresh, adversarial)

Reviewer: fresh pass, no earlier reviews read. Benchmark is Serum 2's UI, compared from knowledge of it; I did not run Serum 2.
Inputs: offscreen shots in `shots5/` (1060x720 logical at 1.5x) plus the source under `src/gui/` and `src/PluginEditor.cpp`.
Paths below are relative to `shots5/` and `src/`. **[inferred]** marks claims I could not check in a shot or in code.

## Verdict

ilanaSynth has a huge feature set (6 oscillators with several types, 39 filters, 41 FX types, 16 envelopes, chaos LFOs, a clip sequencer, DX7 import), but the UI shows that size everywhere instead of hiding it until it's needed. A Serum 2 user would hit cryptic chip codes, dead index rows, duplicated panels and pages that are mostly empty space before they found the controls they wanted. The basic skin is clean and consistent, but the core workflow tools fall short: modulation depth handles, the clip editor, preset tags and FX instances are each a step behind Serum 2, and several layouts have real clipping or overlap bugs. It is a good personal power tool. It is not yet a fast instrument.

---

## Findings, worst first

### 1. Critical: the mod-source bar collapses into cryptic codes and fills up with unused envelopes
- **What's wrong:** Once the envelope pool has been revealed, the source bar at the bottom of *every* page changes from readable names (LFO 1, AMP ENV, PRESSURE) to 25 two- or three-letter chips: `L1 L2 L3 AMP FLT FLT2 MOD E5 E6 … E16 MSEG VEL KEY RND WHL AT`. Twelve of them (E5 to E16) are envelopes this patch doesn't use. `MOD` means envelope 4, not the mod wheel. `AT` means pressure, which the full-name bar calls PRESSURE. This bar is the main drag-to-modulate surface, and now you have to decode it before you can use it.
- **Serum 2:** The source bar only lists sources that exist. You add an LFO or envelope with a "+", and every chip keeps its full name.
- **Evidence:** compare `neuro/06-ENV-LFO.png` (full names) with `neuro/07-STEPS.png`, `neuro/08-MATRIX.png`, `neuro/11-FX.png` and `init/env-pool-full.png`. The same thing happens at 75% zoom in `small/08-MATRIX.png`.
- **Fix:** Show only the sources the patch actually uses, plus one "+" chip. Never abbreviate below the readable name. If chips don't fit, scroll the bar or group envelopes into one "ENV ▾" chip. Use one name per source across the chip, the matrix and the tab (see #13).

### 2. High: factory presets have no real tags; the grey "tags" are just macro names
- **What's wrong:** The grey words next to each row in the browser (`TONE · TALK · TEAR · SUB`) look like tags, but they are the preset's macro names. Factory presets carry no tags at all, and tag filter chips only appear for user presets. So the 659 factory presets can only be browsed by one category and by name. The search placeholder still says "Search names, categories **and tags**".
- **Serum 2:** Filters by tags, by author/pack and by favourites, and presets ship with real descriptive tags.
- **Evidence:** `extras/extra-browser-dropdown.png`, `extras/extra-browser-docked.png`. In code: `gui/PresetPanel.h:893` (`macroNamesFor`) draws that text at :987-995, and the comment at :1297 reads "only user presets have tags".
- **Fix:** Have the preset pipeline write 3 to 6 descriptive tags per factory preset (timbre, movement, character), show them as real chips with a tag-filter column, and drop the macro names from the row or show them only on hover.

### 3. High: the FX rack holds each effect type only once
- **What's wrong:** You can't stack two OTTs, two drives or two EQs. When a patch has a second instance, its card turns into dead text: "Shares its settings with the first OTT above (an older patch; the rack takes each effect once)". For sound design (drive → EQ → drive, delay into reverb into delay) this is a hard wall, and the explanation is engineering language, not user language.
- **Serum 2:** Any effect can be added any number of times, and each instance has its own settings.
- **Evidence:** `neuro/fx-20.png`. In code: `gui/pages/FxPage.h:1566`, and `PluginProcessor.h:56-57` (10 slots, 41 types, with parameters kept per type).
- **Fix:** Store parameters per slot, not per type. Until that's done, grey out types already in the rack in the "add" library and the type menu, with a tooltip, instead of letting the user create a dead card.

### 4. High: the clip editor is a cramped strip, not a piano roll
- **What's wrong:** The note grid is about 190 px tall at 100%. It has no keyboard column: only C rows are labelled (`C3`). Notes are about 8 px tall, and there are no bar numbers along the top. The selected state only blends the note colour 30% toward white (`ClipEditor.h` ~line 150), so `gen-clip-selected.png` is hard to tell apart from an unselected clip. Below it, the GENERATE panel (scale, strum, spray) takes the bottom half of the page and pushes the editor small. Useful behaviour (Ctrl+wheel zoom, arrow nudges, velocity lane) exists but is only described in a hint string.
- **Serum 2:** Its clip editor is a proper piano roll with a key strip, bar ruler, visible playhead, clear selection, and a large edit area that can be expanded.
- **Evidence:** `init/gen-clip.png`, `init/gen-clip-selected.png`, `neuro/gen-clip-selected.png`. In code: `gui/ClipEditor.h:16, 46, 421-450`.
- **Fix:** Add a piano key column (labels on every C, black/white key shading), a bar/beat ruler, a strong selection colour plus outline, and an "expand" button that gives the editor the full page. Fold GENERATE into a collapsible strip in clip mode.

### 5. High: modulation depth is set on 10 px badges beside the knob
- **What's wrong:** Each routing is a tiny numbered or pie badge next to the knob (`dotSize = 10`), and you drag the badge vertically to set depth. With three routings, the FRAME knob grows a column of three 10 px blobs (`2`, half-cyan, pink). At 75% zoom they are about 7 px. A knob shows at most 6 badges (`ParamControls.h:966`). The coloured arc on the knob is display only.
- **Serum 2:** You select a mod source, then drag the knob's own ring to set depth. Depth handles sit on the ring, are large, and are colour-coded per source.
- **Evidence:** `neuro/01-MAIN.png` (OSC1 FRAME, filter CUTOFF), `extras/extra-spec-and-card.png`, `small/03-OSC.png`. In code: `gui/ParamControls.h:281, 366-376, 966`.
- **Fix:** Make the arc ring draggable (Alt+drag on the knob, or drag on the ring band) for the routing that is hovered or selected. Grow badges to at least 14 px with a hit area of at least 20 px. Replace the 6-badge cap with a "+N" overflow badge.

### 6. High: a folded card leaks its child labels (layout bug)
- **What's wrong:** On PLAY, after oscillators are added, the folded SUB + NOISE card still draws `SHAPE`, `OCTAVE`, `SUB LEVE…` (clipped, with a mod badge on top of it) and `NOISE` under its title row. Those labels belong to the unfolded card. It looks broken.
- **Serum 2:** Not applicable. Its layout is fixed and never auto-folds.
- **Evidence:** `neuro/added-osc-MAIN-scrolled.png`, bottom-left card.
- **Fix:** Hide the child labels and controls when a card is auto-folded (`gui/pages/MainPage.h` ~:120-195 and :944). Add a screenshot test for each folded state.

### 7. High: chaos LFO knobs overlap their own values
- **What's wrong:** With the Lorenz shape, the six-knob grid (RATE, SMOOTH, SIGMA, RHO, BETA, SEED) is squeezed into rows that are too short. The value text (`1/16`, `0%`, `10.00`, `28.00`, `2.67`, `Free`) sits on top of the knob arcs, and the RATE sync badge collides with its label. It happens at 100% and at 75%.
- **Serum 2:** Its knob/value spacing never collapses like this.
- **Evidence:** `neuro/lfo-sim-lorenz.png`, `small/lfo-sim-lorenz.png`. The other `lfo-sim-*.png` shots probably show the same, **[inferred]**: only Lorenz was opened.
- **Fix:** Give each row its fixed knob-plus-value height, or move the extra parameters into a second row or a "more" fold. Never let the value box overlap the knob bounds.

### 8. High: auto-folding moves controls depending on how many oscillators are on
- **What's wrong:** PLAY and OSC fold cards "to fit". The DX7 voice shows five oscillator rows that say only "WAVETABLE - folded to fit - click to open". Neuro's OSC page folds SUB + NOISE, VOICE and ACOUSTIC KEYS. Turning on one more oscillator changes which controls are visible and where they are, so muscle memory never forms. Every click on a folded row also costs a click. On top of that, PLAY duplicates most of the OSC page.
- **Serum 2:** OSC A, OSC B, OSC C, Noise and Sub always sit in the same place with the same size.
- **Evidence:** `dx7/01-MAIN.png`, `neuro/03-OSC.png`, `neuro/added-osc-MAIN.png`, `neuro/added-osc-MAIN-scrolled.png`. In code: `gui/pages/MainPage.h:122-195`, `gui/pages/OscPage.h:412`.
- **Fix:** Give PLAY a fixed compact strip for each oscillator (wave thumbnail, mode, table, level, and one or two macros) that never folds. Leave full editing to OSC, where each oscillator gets a tab (OSC 1 … 6) instead of a fold list.

### 9. High: a factory preset ships with duplicate matrix rows, and the warning is cryptic
- **What's wrong:** Neuro Wobble has rows 1 and 12 set to the same routing (LFO 1 → Filter1 Cutoff, +35%). They are flagged with `1!` / `12!` badges and the note "2 repeated (marked !)". The editor's own drag-and-drop refuses to create duplicates (`ParamControls.h` comment near :396, "points at the existing routing instead of adding a second"), yet the presets still contain them. The `!` badge gives no explanation and no fix button.
- **Serum 2:** Each matrix row is unique per source and destination pair, or clearly stacks.
- **Evidence:** `neuro/08-MATRIX.png`, `remap/remap-matrix.png`.
- **Fix:** Have the preset pipeline merge duplicate rows and lint for them. In the UI, give the `!` badge a tooltip and a "Merge duplicates" action.

### 10. Medium: DX7 algorithm numbers don't match the DX7
- **What's wrong:** The header says `DX7 ALG 5`, but the highlighted tile is **11**. There are only 16 tiles, not the DX7's 32, so a DX7 user can't find "algorithm 5" by its number. FM_MODE options (Through-Zero, Phase) are unexplained.
- **Serum 2:** Not a DX7 synth. A dedicated FM synth like Dexed numbers algorithms 1-32 to match the hardware.
- **Evidence:** `dx7/09-FM.png`, `neuro/09-FM.png` ("CUSTOM - matches no algorithm here").
- **Fix:** When a DX7 voice is loaded, show the DX7's own 32-algorithm grid (or label each tile with its DX7 number). Never show two different numbers for the same algorithm.

### 11. Medium: the FM matrix is a wall of 42 identical knobs, and the FB labels are clipped
- **What's wrong:** In DX7 mode the matrix is 7 rows × 6 columns of the same knob, mostly at 0%. The `FB` label in each diagonal cell is drawn under the knob and clipped (`FB-`). There is no hierarchy between active routes and empty ones, and the operator graph on the left repeats the same information.
- **Serum 2:** FM is a per-oscillator source selector plus an amount, so you never see a grid of zeros.
- **Evidence:** `dx7/09-FM.png`, `neuro/09-FM.png`.
- **Fix:** Draw empty cells as small dots that turn into a knob on hover or drag. Let users edit amounts directly on the graph's arrows (they already show percentages). Fix the FB label position.

### 12. Medium: the filter page spends a third of its height on two duplicated 12-button type grids
- **What's wrong:** Each filter shows 4 category tabs × 12 type buttons, side by side, so 24 buttons are on screen to pick 2 values. Filter 2's frame also hides a separate WEST module behind a `FILTER 2 | WEST` toggle, which means you can't see Filter 2 and the wavefolder at the same time. The signal-flow diagram doesn't show where WEST sits. The response view parks marker 2 in the bottom-right corner on the frame edge, and an unlabelled hollow ring marker is also drawn.
- **Serum 2:** Filter type is one compact menu or browser with categories. Routing is a small, clear per-oscillator A/B selector, and the graph is large.
- **Evidence:** `neuro/05-FILTER.png`, `neuro/filter-west.png`, `init/filter-new-models.png`.
- **Fix:** Replace each grid with a type dropdown that has category sub-menus, plus prev/next arrows. Give WEST its own card or a node in SIGNAL FLOW. Clamp graph markers inside the plot and label the hollow ring (keytracked cutoff?) **[inferred meaning]**.

### 13. Medium: envelopes have three naming schemes
- **What's wrong:** One envelope appears as `MOD ENV` in tabs and cards, as `MOD` on the chip, and as `Env4` in the source code. Another is `FILT 2 ENV` in tabs and `FLT2` on the chip. The fifth is `ENV 5` on the PLAY tab and `E5` on the chip, and pool cards say `ENV 13`. Pressure is both `PRESSURE` and `AT`. The matrix source names are different again.
- **Serum 2:** ENV 1-4 and LFO 1-10, named the same everywhere.
- **Evidence:** `neuro/01-MAIN.png` (envelope tabs), `neuro/07-STEPS.png` (chips), `init/env-pool-full.png`.
- **Fix:** Pick one canonical label per source and use it everywhere: chip, tab, matrix and tooltip.

### 14. Medium: dead 1-16 index rows over carousels that show four cards
- **What's wrong:** Above both the LFO and envelope pools there is a row of numbers 1 to 16. Most of them point at slots that don't exist. Below it, a 4-card carousel with a thin scrollbar shows LFO 1-3 plus "+", or ENV 13-16. You can never see more than four at once, and the number row looks clickable when the slot doesn't exist.
- **Serum 2:** Tabs exist only for LFOs that exist, with a "+" to add one.
- **Evidence:** `neuro/06-ENV-LFO.png`, `init/env-pool-full.png`, `init/lfo-pool-full.png`.
- **Fix:** Show only existing slots plus "+". Use compact tabs instead of the number row plus carousel, which frees about 40 px per pool.

### 15. Medium: the STEPS page mixes up who plays what
- **What's wrong:** STEPS A says "LFO 1 isn't playing these", shows a `Use on LFO 1` button, *and* has an LFO 1 pill highlighted. STEPS B says "LFO 4 plays these steps", but the LFO pool only has LFO 1-3. There are no step count, value readout, snap or step-length controls. On the same page, the MSEG has no grid or snap control and uses its own Hz rate, separate from the LFO system.
- **Serum 2:** Any LFO can be a free-draw or step shape, edited in the same LFO editor with grid, snap and rate.
- **Evidence:** `neuro/07-STEPS.png`.
- **Fix:** Make "Steps" and "MSEG" shape modes of an LFO, edited in the LFO editor. If they stay separate, show one clear "Driving: LFO n ▾" selector and only list LFOs that exist.

### 16. Medium: the ARP shows a fake staircase and nothing to edit
- **What's wrong:** In ARP mode the big pattern area shows 16 rising bars with "ARP OFF", even when the arp has never been set up. It can't be edited per step (pitch, velocity, gate, tie), so 40% of the page shows nothing you can use.
- **Serum 2:** Its arp has an editable step lane per step (transpose, velocity, gate/length) and a pattern library.
- **Evidence:** `neuro/10-ARP-SEQ.png`.
- **Fix:** Either make the bars editable step lanes, or shrink the preview to a strip and use the space for the controls.

### 17. Medium: the GENERATE panel is laid out confusingly
- **What's wrong:** SCALE, STRUM and NOTE SPRAY share one header line, and the controls below don't line up with their section headings. SNAP PLAYED floats alone, about 120 px under SCALE. A single on/off switch sits at the far right of the NOTE SPRAY rule, so it looks like it only turns Note Spray on, though it turns on the whole panel **[inferred from position; check `gui/pages/SeqPage.h`]**. Large empty areas sit between controls.
- **Serum 2:** Its arp and clip sections are dense and grouped in boxes.
- **Evidence:** `neuro/10-ARP-SEQ.png`, `init/gen-euclid.png`.
- **Fix:** Use three boxed sub-cards (Scale, Strum, Spray), each with its own enable switch and its controls inside.

### 18. Medium: the FX page shows the chain twice, and splitters are buried
- **What's wrong:** The left CHAIN list (big 60 px rows that hold only a name and CPU) repeats the card stack on the right. "+ add effect" appears twice. The toolbar has 8 equal-weight buttons (RACK A/B, COPY, MOVE UP/DOWN, DICE, SAVE/LOAD CHAIN). Band splitting hides in a `FULL ▾` dropdown on each card; its crossovers are global and live in a callout. The `S` (solo) pill is unlabelled.
- **Serum 2:** One rack column with drag-to-reorder. Splitters are visible containers (L/R, M/S, Low/Mid/High) with their crossover inline.
- **Evidence:** `neuro/11-FX.png`, `neuro/fx-05.png`, `init/11-FX.png`. In code: `gui/pages/FxPage.h:393-401, 1002-1023`.
- **Fix:** Drop the left list once a chain exists (reorder by dragging card headers) and give its width to the cards. Draw splitter groups as bracketed containers with their crossover knobs. Rename `S` to "SOLO" or give it an icon with a tooltip.

### 19. Medium: "AW" names and two kinds of Airwindows effect
- **What's wrong:** The FX library lists `AW REVERB`, `AW TAPE`, `AW CONSOLE`, `AW MODULATION` … and also a plain `AIRWINDOWS` entry. Nothing tells a user what "AW" means or how `AIRWINDOWS` differs from the `AW *` entries. `AW SATURATION` and `AW MODULATION` are squeezed into smaller text to fit their buttons.
- **Serum 2:** Effects have plain names, one per job.
- **Evidence:** `init/11-FX.png`, `neuro/fx-36.png`.
- **Fix:** Name effects by what they do ("Tape", "Console", "Ensemble"), with an "Airwindows" badge and tooltip. Rename the catch-all to "Airwindows (all)".

### 20. Medium: preset master levels are about 16 dB apart
- **What's wrong:** MASTER is −11.7 dB on Neuro Wobble, −5.1 dB on Init, +3.0 dB on E.PIANO 1 and +4.2 dB on Hammered Strings. Stepping through the browser jumps in level, which goes against the user's own "never break level-wise" rule. **[inferred: the perceived loudness was not measured; this is only the knob value]**
- **Serum 2:** Factory presets are roughly level-matched.
- **Evidence:** `neuro/01-MAIN.png`, `init/01-MAIN.png`, `dx7/01-MAIN.png`, `neuro/keys-grand-osc.png`.
- **Fix:** Loudness-normalise the factory banks in the preset pipeline (e.g. LUFS on a held C3) and keep MASTER near one reference value.

### 21. Medium: the "Replace your edits?" dialog has no Save option
- **What's wrong:** The only choices are Cancel and Load anyway. Saving first means: Cancel → Save → find the preset again → load it again.
- **Serum 2:** Its unsaved-changes handling is lighter. **[not verified that it prompts at all]** Standard practice is Save / Discard / Cancel.
- **Evidence:** `confirm/00-confirm.png`. In code: `PluginEditor.cpp:1209-1230`.
- **Fix:** Add "Save and load" as the third button. It should save in place when a user preset is loaded and open Save As otherwise.

### 22. Medium: docking the browser makes the plugin window 510 px wider
- **What's wrong:** In docked mode the window grows from 1590 to 2100 px (at 1.5x). In a DAW the plugin window suddenly gets wider and may run past the screen. The docked list also squeezes rows into two lines.
- **Serum 2:** The browser replaces the main panel area inside the same window size.
- **Evidence:** `extras/extra-browser-docked.png` (2100 × 1080) compared with `neuro/01-MAIN.png` (1590 × 1080).
- **Fix:** Dock the browser over the page area (to the right of the tabs) without resizing the window, or make docking ask first.

### 23. Low: DX7 voices are listed but hidden by default
- **What's wrong:** The category list shows `DX7 288`, but "Show DX7 voices" is unticked and the header says `371 of 659`, so clicking around never makes it clear where the other 288 went.
- **Evidence:** `extras/extra-browser-dropdown.png`.
- **Fix:** Clicking DX7 should simply show them, so the checkbox can go. Or grey out the DX7 row with "(hidden)".

### 24. Low: the Save As dialog is thin
- **What's wrong:** It has no author field and no folder or sub-folder picker. The tags field is free text with no suggestions from existing tags. The validation line "Saved without : / (a file name can't hold them)" is shown before you have saved anything and reads oddly.
- **Evidence:** `extras/extra-save-as.png`.
- **Fix:** Add tag suggestions as chips and a folder picker. Phrase the validation as "`:` and `/` will be removed from the file name".

### 25. Low: the PHYSICAL tab is a dead end for most patches
- **What's wrong:** On a wavetable patch the PHYSICAL tab is a big empty preview, "NO BODY", and a "Switch to physical" button. It's a whole top-level sub-tab that does nothing for most patches. The OSC-page physical card also has a left preview showing the same curve for "burst" and "hammer" excitation. **[inferred: two shots look identical]**
- **Evidence:** `neuro/04-PHYSICAL.png`, `init/osc-physical.png`, `neuro/keys-grand-osc.png`.
- **Fix:** Only show the PHYSICAL sub-tab when an oscillator is in Physical mode, or fold its visual into the OSC card.

### 26. Low: the VECTOR/EVOLVE page is mostly empty
- **What's wrong:** VECTOR is off by default and the pad says "switch on to mix the corners". EVOLVE lists all 8 macros with a slider and two knobs each, all at 0%: 24 controls doing nothing.
- **Evidence:** `neuro/02-VECTOR.png`.
- **Fix:** Collapse EVOLVE to one row per macro that has evolve turned on, plus "+ macro".

### 27. Low: the 5-8 macro toggle hides half the macros
- **What's wrong:** The settled design has 8 macros, but only 4 are visible, behind a tiny `5-8` button. Macro names differ per preset (TONE/TALK vs BRIGHT/TONE vs HAMMER/LID), which is fine, but on Init they are just "MACRO 1".
- **Serum 2:** All 8 macros are visible at all times.
- **Evidence:** `neuro/01-MAIN.png`, `dx7/01-MAIN.png`.
- **Fix:** Show 8 smaller macro knobs in the bottom strip. GLIDE, LEGATO and MASTER can move to the header or a settings pop-over.

### 28. Low: the status line is always on and rarely useful
- **What's wrong:** A full-width 20 px row permanently says "Hover any control for details. Drag source chips…". `KEYS` and `?` are tiny buttons crammed into its right end.
- **Evidence:** every page shot.
- **Fix:** Show the line only while hovering (or overlay it on the macro strip), and give the vertical space back.

### 29. Low: header status readouts are hard to read
- **What's wrong:** VOICES is a dotted line of about 32 dots with one coloured dot. BPM is plain grey text. The animated waveform under the preset name is decoration and changes on every shot. The `A/B: A` button is wider than anything else in its group.
- **Evidence:** `neuro/01-MAIN.png` (header).
- **Fix:** Show voices as "3/32". Shrink A/B to a toggle like `A|B` (it stays in the header, per the settled decision).

### 30. Low: the matrix uses jargon and has dead columns
- **What's wrong:** POLARITY offers "Auto (Bipolar)" / "Auto (Unipolar)". VIA is a bare `+` button. CURVE is a 70 px line thumbnail that opens the remap editor, and that editor sits 2/3 wide with dead space to its right. There is no sort or filter by source or destination.
- **Serum 2:** Its matrix has clear Bipolar/Unipolar toggles, Aux source, an inline curve, and sortable columns.
- **Evidence:** `neuro/08-MATRIX.png`, `remap/remap-editor.png`.
- **Fix:** Use "Bi" / "Uni" toggles with "auto" as a tooltip, label VIA as "Aux: none", and allow sorting columns.

### 31. Low: text is tiny at 75% zoom
- **What's wrong:** At 795 px wide, the chip labels, the "folded to fit - click to open" hints, the matrix header and the mod badges are about 6 to 8 px tall.
- **Evidence:** `small/03-OSC.png`, `small/08-MATRIX.png`, `small/lfo-sim-lorenz.png`.
- **Fix:** Set a minimum font size of 9 logical px at every zoom level. Drop hint text before shrinking it.

### 32. Low: the SCOPE panel covers half the PLAY page
- **What's wrong:** Opening SCOPE hides the filter, envelope and LFO, which are the very things you want to watch while you tweak. Its options (`Normal ▾`, `OS`, `2x ▾`) are unlabelled jargon.
- **Evidence:** `neuro/scope-panel.png`.
- **Fix:** Show a smaller scope in the header (it already has a mini wave) and offer a detachable or expanded scope. Label OS as "Oversample".

---

## Overall

**Score: 5/10 vs Serum 2.** The sound engine and feature set are broader than Serum 2 in several areas. But the UI exposes that breadth through folding, cryptic abbreviations, duplicated panels and tiny handles, so in day-to-day speed and clarity it is clearly behind Serum 2.