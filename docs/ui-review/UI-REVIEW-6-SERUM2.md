# UI review 6: ilanaSynth v1.3 against Serum 2 (fresh, adversarial)

Reviewer: a fresh pass. The benchmark is Serum 2's UI, compared from what I know of it; I did not run Serum 2.
Inputs: the shots in `/home/user/shots6/` (1060x720 logical at 1.5x; `small/` is 75 %), one extra render in
`/home/user/shots6/serum-extra/strings/` (DX7 voice "STRINGS 1 (ROM1A)", PLAY and FM only), and the source in `src/gui/`
and `src/PluginEditor.cpp`. Shot paths below are relative to `shots6/`. **(inferred)** marks claims I could not confirm in a
shot or in the code.

Caveat on the shots: the snapshot tool changes parameters between pictures. `keys/04-PHYSICAL.png` ("OSC 1 plays a wavetable")
and `dx7/04-PHYSICAL.png` are wrong for that reason (`tools/Snapshot.cpp:4245-4277` sets `osc1_mode` back to 0 after the OSC
page). LFO 4 on STEPS (`Snapshot.cpp:4072`) and the 16 revealed envelopes behind the coded chip bar are also artefacts. I did
not count any of these as bugs.

## Verdict

The DX7 fold-in works as sound, but in the UI it is still a separate instrument stuck onto the FM page. A DX7 voice opens on a
PLAY page showing six "WAVETABLE - folded to fit" rows and an AMP ENV that the voice ignores. Its real envelope, a second LFO and
a pitch envelope sit behind a tab on the FM page, in 0-99 DX7 vocabulary, and none of them is a mod source. On that same page the
operator diagram collapses into an 85 px strip where OSC 1 and OSC 2 fall off the left edge. In the browser, DX7 voices form their
own category with the same four macro names on all 288 voices. Outside DX7, most of review 5's structural findings are still
open: single-instance FX, the clip editor, small depth badges, duplicated filter grids, the STEPS/ARP/GENERATE confusion and the
leaked fold labels. A few things got better: the FX library greys out types already in the rack, the matrix sorts, and the
DX7 algorithm label no longer contradicts the tile. It is a powerhouse, but it is still unwieldy, and polish is uneven.

**Score: 5.5 / 10 against Serum 2.**

---

## Findings, worst first

### 1. Critical: the Operator EG (the DX7 envelope) is a second envelope system in its own corner
- **What's wrong:** On a DX7 voice every oscillator's AMP ENV is set to "Op EG", but no envelope surface knows about it.
  - PLAY's ENVELOPE card and MOD > ENV / LFO open on AMP ENV (5 ms / 300 ms / 80 % / 250 ms), which this voice does not use.
  - The envelope pool lists AMP ENV, FILT ENV and FILT 2 ENV, but not the Operator EG.
  - The Op EG is not in the source bar, so it can't be dragged onto anything. `Mod::Source` (`src/dsp/Modulation.h:12-51`) has no Op EG entry.
  - Its editor is on FM > "OSC n AS AN OPERATOR". The controls are 17 raw DX7 controls (`R1 R2 R3 R4 L1 L2 L3 L4 BREAK L DEPTH R DEPTH L CURVE R CURVE RATE KEY AMS VEL OUTPUT`), with values like 96, 25, `A-1` and `-LIN`.
  - The graph can't be dragged. `OperatorEgGraph` (`gui/pages/FmInputPages.h:10`) has no mouse handlers, while every other envelope says "drag the graph or the knobs".
- **Serum 2:** One envelope system: ENV 1-4 with the same curve editor, the same drag-to-modulate and the same names everywhere.
- **Fix:** Treat the Op EG as one more envelope.
  - When an oscillator uses it, PLAY's ENVELOPE card and the ENV / LFO pool show an "OP EG (OSC n)" tab, and AMP ENV is marked "not used by OSC 1-6".
  - Show rates as times (ms / s) and levels as %, with the 0-99 value in the tooltip.
  - Let the breakpoints be dragged.
  - Give it a source chip so it can modulate other things.
  - Move BREAK / DEPTH / CURVE into a collapsed "KEY SCALING" fold.
- **Evidence:** `dx7/01-MAIN.png`, `dx7/06-ENV-LFO.png`, `dx7/09-FM.png`; `FmInputPages.h:186-206`.

### 2. Critical (layout): the FM operator diagram collapses and clips on every DX7 voice
- **What's wrong:** When the selected oscillator uses the Op EG, the operator card takes `min(400, 82 %)` of the left column (`FmInputPages.h`, `resized()`, `operatorCard = area.removeFromBottom (egShown ? juce::jmin (400, area.getHeight() * 82 / 100) ...)`). The diagram gets about 85 px:
  - OSC 1 and OSC 2 are pushed off the left edge (only a partial ring shows).
  - Arrows run on top of each other: in "STRINGS 1", the 5→4 and 4→3 routes overlap in one band.
  - The ratio and level captions under each node are cut in half.
  This is the one picture that explains a DX7 voice, and it is unreadable on all 288 of them.
- **Serum 2:** Not applicable (no operator graph), but no Serum 2 panel ever crops its main display.
- **Fix:** Give the diagram a minimum height of about 190 px. Put the EG rows in a two-column layout beside the graph, or scroll the operator card instead of the diagram. Add a UI test that every node's bounds are inside the diagram.
- **Evidence:** `dx7/09-FM.png`, `serum-extra/strings/09-FM.png`; compare `init/fm-dx-keys.png`, which is an Init patch on Amp Env, so the diagram has room.

### 3. High: a DX7 voice's PLAY and OSC pages show nothing of what makes it sound
- **What's wrong:** PLAY shows OSC 1 as "Wavetable / Sine / Warp Off / Frame / Unison / Detune", then five rows of "WAVETABLE - folded to fit - click to open". OSC shows the same, plus SPECTRAL, CHORD and UNI MODE controls that an FM operator doesn't use. Nothing says that OSC 1, 3 and 5 are carriers and 2, 4 and 6 are modulators, or what their ratios are. A new user sees "six sine wavetables" and an unused amp envelope.
- **Serum 2:** Each oscillator panel shows its own FM-from-another-osc warp, so the FM is visible where the oscillator is.
- **Fix:**
  - On PLAY and OSC, an oscillator that is an FM modulator or carrier says so in its row: `OSC 2 · MOD → OSC 1 · ×1.00 · 100 %`, and its OUT state is shown.
  - When any FM route exists, PLAY's spare tile shows a mini operator graph that links to FM.
  - Hide or dim WARP, SPECTRAL, UNISON and CHORD on operators that use the Op EG **(inferred: check which of them the DX engine ignores)**.
- **Evidence:** `dx7/01-MAIN.png`, `dx7/03-OSC.png`, `serum-extra/strings/01-MAIN.png`.

### 4. High: DX7 voices are their own category, with identical macros and shouty names
- **What's wrong:**
  - DX7 is a category beside Bass, Lead and Keys, so "E.PIANO 1" is not in Keys and "BASS 1" is not in Bass.
  - All 288 rows show the same macro names, `BRIGHT · TONE · DRIFT · SPACE`, so the grey row text says nothing.
  - Names are all caps with a redundant "(ROM1A)", although bank chips (ROM1A … DEXED01) already say the bank. Plan item 13.2 (the display formatter) is not done.
  - The "Show DX7 voices" checkbox stays unticked while the list shows DX7 voices after a click on the DX7 category, so the checkbox seems to do nothing.
- **Serum 2:** Packs and banks are a filter (author / pack), and every preset still has its type category and tags.
- **Fix:**
  - Map each DX7 voice to a type category by name: E.PIANO → Keys, BASS → Bass, BRASS → Lead/Pad, and so on. Keep "DX7" and the bank as a pack filter chip.
  - Display names in Title Case without the bank suffix.
  - Drop the checkbox: the DX7 pack chip is the switch.
- **Evidence:** `extras/extra-browser-dx7.png`, `extras/extra-browser-docked-dx7.png`, header `DX7` above the name in `dx7/01-MAIN.png`.

### 5. High: two ways to say "level", "tune" and "envelope" for the same operator
- **What's wrong:**
  - The same parameter (`oscN_amp_env`) is labelled **AMP ENV** on OSC and **ENVELOPE** on FM (`FmInputPages.h:180`). Its value reads "Op EG", while the tooltips and headings say "Operator EG".
  - An operator has LEVEL 50 % (OSC and FM) and OUTPUT 99 (FM). HANDOFF says LEVEL 0.5 is "the DX7 level", so a user who sets LEVEL to 100 % doubles a carrier without knowing why.
  - SEMI and FINE appear on both pages. RATIO / SNAP / TUNING exist only on FM.
- **Serum 2:** One level and one tuning block per oscillator.
- **Fix:** One label everywhere ("ENVELOPE"; values "Amp Env", "Operator EG"). Show the operator OUTPUT as the operator's level and hide or relabel the 50 % LEVEL as "TRIM". Put TUNING / RATIO on the OSC card too, so the two pages agree.
- **Evidence:** `dx7/03-OSC.png` vs `dx7/09-FM.png`.

### 6. High: the Operator EG's PITCH / LFO is a hidden second LFO system
- **What's wrong:** FM > PITCH / LFO holds a pitch envelope and a DX7 LFO (SPEED 34, DELAY 33, PITCH DEPTH, AMP DEPTH, KEY SYNC, WAVE, PITCH SENS). It sits in the same pill row as OSC 1-6, as if it were an oscillator. It is not in the LFO pool and has no chip. Its depths go through the operators' AMS and PITCH SENS, not the matrix. Its graph says "PITCH +/-3.0 st ... NO PITCH MOVEMENT".
- **Serum 2:** Every LFO is LFO 1-10 in one place, and its depth is set by dragging it to a target.
- **Fix:** On import, convert the DX7 LFO into a pool LFO plus matrix rows (pitch and amp), which can be edited and dragged like any other. Show the pitch envelope as an envelope in the pool. If that is too big, at least move PITCH / LFO out of the operator pills into its own card titled "DX7 PITCH EG + LFO".
- **Evidence:** `dx7/09-FM-pitch-lfo.png`; `FmInputPages.h:279-303`.

### 7. High: DX7 algorithms are only 7 of 32, and the numbering still doesn't match
- **What's wrong:** The grid has 16 tiles. Nine are generic and seven are named after DX algorithms (`src/dsp/FmAlgorithms.h:28-46`). E.PIANO 1 lights tile **11**, which is labelled "DX 5 Keys". STRINGS 1 (DX algorithm 2) shows "CUSTOM - matches no algorithm here", so 25 of the 32 DX algorithms show up as CUSTOM.
- **Serum 2:** Not an FM synth. Dexed and FM8 show all 32 algorithms by their DX numbers.
- **Fix:** When any oscillator is on the Op EG (or the patch came from a .syx), show a "DX7 1-32" grid that toggles with the generic one. Always print the DX number on the tile itself.
- **Evidence:** `dx7/09-FM.png`, `serum-extra/strings/09-FM.png`.

### 8. High: a folded SUB + NOISE card still leaks its labels (layout bug, cause found)
- **What's wrong:** When PLAY auto-folds SUB + NOISE, the card still draws `SHAPE OCTAVE SUB LEVE… NOISE` and a mod badge under the title. `layoutSubCard` hides the controls only when `subFolded` is true and ignores `subAutoFolded` (`gui/pages/MainPage.h:909`: `control->setVisible (! subFolded)`). The same shot also shows a "+ ADD OSC" button inside the OSC 5 row.
- **Serum 2:** Fixed layout, nothing folds.
- **Fix:** Use `! (subFolded || subAutoFolded)` at :909 and return early for both. Add a `--uitest` check that a folded card has no visible child.
- **Evidence:** `neuro/added-osc-MAIN-scrolled.png`.

### 9. High: the FX rack still takes each type once
- **What's wrong:** A second OTT becomes the dead card "Shares its settings with the first OTT above (an older patch; the rack takes each effect once)". The library now greys out types already in the rack (REVERB in `dx7/11-FX.png`), which is better, but you still can't stack two drives or two EQs.
- **Serum 2:** Any effect any number of times, each with its own settings.
- **Fix:** Store parameters per slot. Until then, word the card as "Duplicate OTT (not supported): remove".
- **Evidence:** `neuro/fx-20.png`; `gui/pages/FxPage.h` (the shares text).

### 10. High: the clip editor is still a strip, not a piano roll
- **What's wrong:**
  - The grid is about 190 px tall at 100 %.
  - Only "C3" is labelled. There is no key column and no bar or beat numbers.
  - Selected notes look exactly like unselected ones (`neuro/gen-clip-selected.png`).
  - GENERATE takes the lower half of the page.
- **Serum 2:** A real piano roll with a key strip, a bar ruler, a clear selection, and an expand option.
- **Fix:** Add a key column with labels on every C, a bar/beat ruler, a strong selection fill plus outline, and an EXPAND button that hides GENERATE.
- **Evidence:** `neuro/gen-clip-selected.png`, `init/gen-clip.png`; `gui/ClipEditor.h`.

### 11. High: modulation depth is still set on 10 px badges
- **What's wrong:** FRAME on Neuro Wobble grows a column of three 10 px badges (2, a half cyan pie, a pink pie). CUTOFF has three more. At 75 % they are about 7 px.
- **Serum 2:** Depth is dragged on the knob's ring, with large colour-coded handles.
- **Fix:** Make the ring band draggable for the hovered or pinned source. Use badges of at least 14 px with a 20 px hit area, and a "+N" overflow badge.
- **Evidence:** `neuro/01-MAIN.png`, `small/01-MAIN.png`; `gui/ParamControls.h` (`dotSize`).

### 12. High: chaos LFO knobs still overlap their values (cause found)
- **What's wrong:** On Lorenz, `1/16`, `0%`, `10.00`, `28.00`, `2.67` and `Free` sit on the knob arcs, and the RATE sync badge hits its label. The panel splits its inner height into two knob rows of about 70 px (`gui/pages/EnvLfoPages.h:940-947`, `knobHeight = (inner.getHeight() - rowGap) / 2`). A knob, its label and its value need about 100 px.
- **Fix:** Lay out a chaos shape's 6-8 knobs in one row of mini knobs, or give the LFO panel more height when a sim shape is chosen. Add the snapshot's clip check for this panel.
- **Evidence:** `neuro/lfo-sim-lorenz.png`.

### 13. Medium: two editors for one physical oscillator, which disagree
- **What's wrong:**
  - The OSC card for Felt Hammer Board shows STRING / EXCITER / BODY & BUZZ with REGISTER, DAMPER and COUPLING, plus a "REED PICKUP" curve.
  - The PHYSICAL page for the same oscillator shows DECAY, DAMP, SUSTAIN, STIFF, EXCITE POS, **PICKUP** and HAMMER (no REGISTER, DAMPER or COUPLING) and draws a "PIANO HAMMER" string.
  Same oscillator, two control sets and two different pictures, so a user can't tell which is the real editor.
- **Serum 2:** One editor per oscillator.
- **Fix:** Have both pages build from one control list, so PHYSICAL is the big view of the OSC card. Show the pickup curve on PHYSICAL too (as a second view).
- **Evidence:** `keys/03-OSC.png` vs `keys/physical-page.png`.

### 14. Medium: "BODY" means two places and four labels
- **What's wrong:** The same body (`res_on`, `body_type`) is edited on FILTER (switch, body type labelled "BODY", AMOUNT … COUPLE, plus a "COUPLING" menu) and on OSC > PHYSICAL (the switch labelled "BODY", the type labelled "BODY TYPE", nothing else). PHYSICAL calls it "a body and a soundboard for every string". FILTER calls it "oscillator mix excites the body". The physical oscillator card also has its own "COUPLING" knob, which is a different parameter.
- **Fix:** Put the body in one place (FILTER's SIGNAL FLOW card or PHYSICAL), with a link from the other. Name the parameters the same everywhere: "BODY ON", "BODY TYPE", "COUPLING MODE", "COUPLING".
- **Evidence:** `neuro/05-FILTER.png`, `keys/physical-page.png`; `gui/pages/EnvLfoPages.h:22-36`, `gui/pages/FilterVectorPhysicalPages.h:409-412`.

### 15. Medium: a macro that drives a switched-off effect gets no warning
- **What's wrong:** On E.PIANO 1, macro 4 SPACE → Reverb Mix +35 %, but the reverb slot is off ("REVERB ▾ off", power switch off, MIX 0 %). The macro does nothing, and nothing in the UI says so. The same is likely on many DX7 voices (all 288 share the macro set) **(inferred)**.
- **Serum 2:** Not a direct analogue, but its mod list makes dead targets easy to see.
- **Fix:** Dim a matrix row and the macro's name when the destination's module is off, with the tooltip "Reverb is off". Lint presets for this.
- **Evidence:** `dx7/08-MATRIX.png` (row 6), `dx7/11-FX.png`.

### 16. Medium: SIGNAL FLOW wires modulators into the filter
- **What's wrong:** On a DX7 voice, FILTER > SIGNAL FLOW draws OSC 1-6 all into F1. OSC 2, 4 and 6 are FM modulators with OUT off, so they are never heard. The flow tests `_on`, not `_out` (`gui/FilterWidgets.h:364-365`).
- **Fix:** Draw a non-OUT oscillator's wire dashed, or route it into its carrier, labelled "FM → OSC 1".
- **Evidence:** `dx7/05-FILTER.png`, and PLAY's PATCH card in the same state **(inferred)**.

### 17. Medium: the FM matrix is still a wall of knobs with clipped FB labels
- **What's wrong:** 6 × 6 + noise = 42 mini knobs, mostly at 0 %. The diagonal "FB DX" / "FB~" tag is drawn top-left inside the cell, under the knob, so it is clipped to "FB" plus a scrap (`FmInputPages.h`, paint: `drawText (... "FB DX" ..., cell.reduced (6, 4), topLeft)`).
- **Serum 2:** FM is a per-oscillator source plus amount, so there are no zero grids.
- **Fix:** Draw empty routes as dots that become knobs on hover. Put the FB tag above the knob, or replace it with a small "FB" glyph and show the type on hover.
- **Evidence:** `dx7/09-FM.png`, `small/09-FM.png`.

### 18. Medium: the tutorial still advertises "DX7 MODE", and DX7 import is buried
- **What's wrong:** The tour's first new-feature chip is "DX7 MODE + BANKS: ... the FM page plays them with their own envelopes" (`gui/TutorialOverlay.h:29`), but DX7 mode is gone. Importing a .syx is the last item of the "…" menu (`PluginEditor.cpp:1590`) and reports through a native, unthemed `AlertWindow` (`:1616`).
- **Fix:** Rename the chip to "DX7 BANKS + OPERATOR EG". Add "Import .syx…" to the browser's DX7 pack chip or its FOLDER row. Report the import in the themed `ConfirmOverlay` style, with "Show in browser".

### 19. Medium: the mod source bar still switches to codes when it's crowded
- **What's wrong:** It is consistent now (all chips shorten together, `PluginEditor.cpp:690-740`), but the codes are still cryptic: `MOD` is ENV 4, `AT` is PRESSURE and `E5`-`E16`. Codes appear as soon as the full names don't fit.
- **Serum 2:** Full names, and only the sources that exist.
- **Fix:** Group ENV 5-16 into one "ENV ▾" chip, and LFO 4-16 into "LFO ▾", before shortening anything. Rename the MOD chip "ENV 4" (or the source "MOD ENV" everywhere).
- **Evidence:** `neuro/08-MATRIX.png`, `dx7/09-FM.png`.

### 20. Medium: the filter page still spends a third of its height on two 12-button grids, and the taxonomy contradicts itself
- **What's wrong:** Each filter shows tabs (CLASSIC / ANALOG / VOICE / AIRWINDOWS) over BASIC / CHARACTER / SPECIAL grids. The CLASSIC tab holds LADDER, DIODE and MS-20, which a user would look for under ANALOG. PLAY's filter card uses a TYPE dropdown for the same parameter, so the same parameter has two widgets.
- **Fix:** Use one type dropdown with category sub-menus plus prev/next arrows on both pages, and move the analogue models under ANALOG.
- **Evidence:** `neuro/05-FILTER.png` vs `neuro/01-MAIN.png`.

### 21. Medium: response markers are still unlabelled and parked on the frame
- **What's wrong:** A hollow ring at about 10 kHz has no label. Filter 2's marker sits on the bottom-right corner when it's at 20 kHz, and on DX7 both markers overlap there (`dx7/05-FILTER.png`). The hint says "drag the markers", but a drag anywhere works.
- **Fix:** Label the ring in its tooltip (keytracked or modulated cutoff, **(inferred)**), keep markers 8 px inside the plot, and fan out overlapping markers.

### 22. Medium: STEPS still says two things at once
- **What's wrong:** STEPS A says "LFO 1 isn't playing these" next to a "Use on LFO 1" button *and* a highlighted LFO 1 pill. The steps have no count, length or value readout. The MSEG's 1.5 s point covers its "1.5 s" label, and its "S&H CLOCK" knob has no explanation.
- **Serum 2:** Step and free-draw shapes are modes of an LFO, edited in the LFO editor.
- **Fix:** One "Played by: LFO n ▾" selector (or "not used"). Add a step-count control. Move axis labels below the plot.
- **Evidence:** `neuro/07-STEPS.png`.

### 23. Medium: ARP still shows a fake staircase
- **What's wrong:** 16 rising bars and "ARP OFF". Nothing can be edited per step.
- **Serum 2:** Editable step lanes (transpose, velocity, gate).
- **Fix:** Make the bars velocity/gate lanes, or shrink them to a 40 px preview.
- **Evidence:** `neuro/10-ARP-SEQ.png`.

### 24. Medium: GENERATE is still laid out confusingly
- **What's wrong:** The SCALE / STRUM / NOTE SPRAY headings don't sit over their controls: PITCH sits under NOTE SPRAY, and TIME/SPREAD fall under DIRECTION and PITCH. SNAP PLAYED floats about 120 px below SCALE. The panel's only switch sits at the end of the NOTE SPRAY rule.
- **Fix:** Three boxed sub-cards, each with its own switch.
- **Evidence:** `neuro/10-ARP-SEQ.png`, `neuro/gen-clip-selected.png`.

### 25. Medium: the FX page shows the chain twice and has two wet controls per card
- **What's wrong:**
  - The left CHAIN list repeats the card stack, and "+ add effect" / "+ ADD EFFECT" appear in both.
  - SLOT BLEND appears only on the selected card's header (Vowel, not Drive or OTT), so headers change shape with selection.
  - SLOT BLEND 100 % and MIX 70 % both mean "wet", which goes against review 4's "one MIX".
  - The `S` (solo) pill is still unlabelled.
- **Serum 2:** One rack column, drag to reorder, one mix per effect.
- **Fix:** Drop the left list once a chain exists. Show SLOT BLEND on every card, or fold it into MIX for the effects that have one. Label `S` as "SOLO".
- **Evidence:** `neuro/11-FX.png`, `neuro/fx-20.png`.

### 26. Medium: FX cards waste width
- **What's wrong:** The Vowel card is full width with two knobs centred in it. On a one-effect patch (DX7), 60 % of the FX page is empty.
- **Fix:** Let cards flow in two columns when they hold three knobs or fewer, or show the library to the right of the rack instead of under the chain.
- **Evidence:** `neuro/11-FX.png`, `dx7/11-FX.png`.

### 27. Medium: "AW" names and the catch-all AIRWINDOWS entry
- **What's wrong:** `AW REVERB`, `AW TAPE`, `AW LO-FI`, `AW SATURATION` (squeezed into smaller text) and a bare `AIRWINDOWS`, with no hint of what AW means.
- **Fix:** Name effects by their job, add an "Airwindows" badge, and call the catch-all "Airwindows (all)".
- **Evidence:** `dx7/11-FX.png`.

### 28. Medium: factory presets still have no tags
- **What's wrong:** The grey words on each row are macro names, and the search box still says "Search names, categories and tags". On DX7 every row says BRIGHT · TONE · DRIFT · SPACE.
- **Serum 2:** Real tags plus pack and favourite filters.
- **Fix:** 3-6 pipeline tags per preset, shown as chips with a tag filter. Show macro names on hover only.
- **Evidence:** `extras/extra-browser-dropdown.png`; `gui/PresetPanel.h` (`macroNamesFor`).

### 29. Medium: duplicate routes still ship, and sorting can't be found
- **What's wrong:** Neuro Wobble rows 1 and 12 are the same route (`1!` / `12!`, "2 repeated (marked !)"). Sorting exists (`gui/pages/MatrixPage.h:69-91`), but the headings look exactly like plain labels until you hover them.
- **Fix:** Merge duplicates in the preset pipeline, and add a "Merge" action on the `!`. Draw a faint ↕ on sortable headings.
- **Evidence:** `neuro/08-MATRIX.png`.

### 30. Medium: the matrix names things differently from the rest of the UI
- **What's wrong:** "Osc2 Level", "Filter1 Cutoff" and "LFO1 Rate" against "OSC 2", "F1" / "FILTER 1" and "LFO 1" everywhere else. POLARITY reads "Auto (Unipolar)" on every row.
- **Fix:** Build destination names from the same table as the page labels ("OSC 2 · LEVEL"). Show polarity as a Uni / Bi toggle with "auto" in the tooltip.
- **Evidence:** `dx7/08-MATRIX.png`; `gui/MatrixWidgets.h:442`.

### 31. Medium: docking the browser still widens the window
- **What's wrong:** 1590 → 2100 px at 1.5x.
- **Serum 2:** The browser replaces the main panel inside the same window.
- **Fix:** Dock over the page area, or ask before resizing.
- **Evidence:** `extras/extra-browser-docked-dx7.png`.

### 32. Low: the physical card's title and subtitle collide on PLAY
- **What's wrong:** "REED PICKUP" and "charge vs reed position" are drawn left- and right-justified in the same 18 px rectangle, so at PLAY's width they overlap ("REED PICKUPcharge vs reed position").
- **Fix:** Drop the subtitle when `font width(title) + width(subtitle) > header width` (`gui/WaveDisplay.h:530-534`).
- **Evidence:** `keys/01-MAIN.png`.

### 33. Low: two ways to add an oscillator in the same column
- **What's wrong:** OSC page: "OSC 2 OFF - switch on to edit", "OSC 3 OFF", and also "+ ADD OSCILLATOR". PLAY: a "+ ADD OSC" button inside the OSC 3 row next to its own switch.
- **Fix:** Show off oscillators as one line ("OSC 2, 3 off · + ADD"), or drop the separate ADD button.
- **Evidence:** `keys/03-OSC.png`, `neuro/01-MAIN.png`.

### 34. Low: the dead 1-16 index rows over 4-card carousels are still there
- **What's wrong:** The rows read 1 … 16 when three LFOs exist. The envelope carousel's scroll bar draws as a stray line under AMP ENV.
- **Fix:** Show tabs only for existing slots, plus "+".
- **Evidence:** `neuro/lfo-sim-lorenz.png`, `init/env-pool-full.png`.

### 35. Low: removing an LFO or envelope is hidden
- **What's wrong:** The only way to remove one is a right-click item, which is disabled while the module is routed ("Remove (unroute it first)", `gui/LfoThumbs.h:222`, `gui/EnvThumbs.h:226`).
- **Fix:** A hover × on the card. When it's routed, ask "Remove LFO 2 and its 3 routes?".

### 36. Low: VECTOR / EVOLVE is still mostly zeros, and Vector X/Y can't be found
- **What's wrong:** EVOLVE lists 8 macros × (slider + 2 knobs), all at 0 %. The header says "Vector X / Y are mod sources", but they have no chip.
- **Fix:** One row per macro that has evolve on, plus "+ macro". Add VEC X and VEC Y chips while the vector is on.
- **Evidence:** `neuro/02-VECTOR.png`.

### 37. Low: the PHYSICAL sub-tab is still a dead end on most patches
- **What's wrong:** Better than before (it says why and offers a switch), but it is still a top-level sub-tab on every wavetable patch.
- **Fix:** Show it only when an oscillator is Physical, or show it greyed with the reason in the tooltip.
- **Evidence:** `dx7/04-PHYSICAL.png` (an artefact state, but the empty state is real).

### 38. Low: the 5-8 macro toggle still hides half the macros
- **Serum 2:** All 8 macros are always visible.
- **Fix:** 8 smaller knobs. Move GLIDE and LEGATO into the voice menu.
- **Evidence:** every page.

### 39. Low: the status line is always on
- **What's wrong:** "Hover any control…" takes 20 px on every page, with KEYS and `?` crammed into its right end.
- **Fix:** Show it only on hover, or overlay it on the chip bar.

### 40. Low: the header readouts are still weak
- **What's wrong:** VOICES is 32 dots. The `A/B: A` button is the widest key in its group. The "EDITED" badge is small and orange-on-dark.
- **Fix:** Show "3/32". Make A/B an `A|B` toggle (it stays, per the settled decision).

### 41. Low: SCOPE still covers half of PLAY and repeats OUTPUT
- **What's wrong:** With SCOPE open, PLAY shows both the OUTPUT spectrum (left) and the scope's spectrum (right). The "OS" and "Normal ▾" options have no labels.
- **Fix:** Use the space for an expanded OUTPUT tile, or make the scope a floating panel. Label "Oversample".
- **Evidence:** `dx7/scope-panel.png`.

### 42. Low: Save As is still thin
- **What's wrong:** The warning "Saved without : / (a file name can't hold them)" shows before the user has saved. There are no tag suggestions and no folder picker.
- **Fix:** Show it as "`:` and `/` will be dropped from the file name". Add existing tags as chips.
- **Evidence:** `extras/extra-save-as.png`.

### 43. Low: master levels differ by about 19 dB between presets
- **What's wrong:** MASTER is −11.7 dB (Neuro Wobble), −5.1 dB (Init), +3.0 dB (E.PIANO 1) and +7.0 dB (Felt Hammer Board). **(inferred: this is the knob value, not measured loudness; the pipeline may compensate.)**
- **Fix:** Keep MASTER at one reference and put the trims inside the preset.

### 44. Low: the Op EG graph caption is cryptic
- **What's wrong:** "C3 VEL 100 ... KEY UP 2.5 / 2.6 s" over a graph with no axis.
- **Fix:** Write "Shown for C3, velocity 100 · release starts 2.5 s · silent at 2.6 s", and add a time axis like the other envelope graphs.
- **Evidence:** `dx7/09-FM.png`; `FmInputPages.h:147`.

### 45. Low: 6-operator algorithm tiles look clickable on a 3-oscillator patch
- **What's wrong:** On Neuro Wobble (3 oscillators) tiles 10-16 look exactly like the others. **(inferred: the code may add oscillators on click; if it does, say so on hover.)**
- **Fix:** A small "+3 osc" corner tag on tiles that would add oscillators.
- **Evidence:** `small/09-FM.png`.

### 46. Low: text is still tiny at 75 %
- **What's wrong:** On FM at 75 %, the chip codes, the matrix "0%" values and the algorithm numbers are about 7 px.
- **Fix:** A 9 logical px floor at every zoom. Drop hint text before shrinking it.
- **Evidence:** `small/09-FM.png`.

---

## Still open from review 5

| R5 | Finding | Now |
|---|---|---|
| 1 | Mod-source bar codes | **Partly fixed:** the chips shorten consistently, but `MOD`/`AT`/`E5-E16` remain (#19) |
| 2 | No factory tags | **Open** (#28) |
| 3 | FX one instance per type | **Partly fixed:** the library greys used types; the wall stays (#9) |
| 4 | Clip editor strip | **Open** (#10) |
| 5 | 10 px depth badges | **Open** (#11) |
| 6 | Folded card leaks labels | **Open**, cause found at `MainPage.h:909` (#8) |
| 7 | Chaos LFO knob overlap | **Open**, cause found at `EnvLfoPages.h:940-947` (#12) |
| 8 | Auto-folding moves controls | **Open**, and worse on DX7: 5 of 6 oscillators folded on PLAY and OSC (#3) |
| 9 | Duplicate matrix rows | **Open**; sort added but hidden (#29) |
| 10 | DX7 algorithm numbers | **Partly fixed:** the label matches the tile ("DX 5 Keys" on 11); only 7 of 32 exist (#7) |
| 11 | FM matrix wall, clipped FB | **Open** (#17) |
| 12 | Duplicated filter grids, WEST | **Open** (#20) |
| 13 | Three envelope naming schemes | **Open**, and the Op EG adds a fourth (#1, #5) |
| 14 | Dead 1-16 index rows | **Open** (#34) |
| 15 | STEPS confusion | **Open** (#22) |
| 16 | ARP fake staircase | **Open** (#23) |
| 17 | GENERATE layout | **Open** (#24) |
| 18 | FX chain shown twice, `S` pill | **Open** (#25) |
| 19 | AW names | **Open** (#27) |
| 20 | Master levels | **Open** (inferred, #43) |
| 21 | Replace dialog has no Save | Not re-checked (no new confirm shot) |
| 22 | Dock widens window | **Open** (#31) |
| 23 | DX7 hidden by default | **Open**, the checkbox is now contradictory (#4) |
| 24 | Thin Save As | **Open** (#42) |
| 25 | PHYSICAL dead end | **Improved** (it explains and offers a switch); still a sub-tab (#37) |
| 26 | EVOLVE zeros | **Open** (#36) |
| 27 | 5-8 macro toggle | **Open** (#38) |
| 28 | Status line | **Open** (#39) |
| 29 | Header readouts | **Open** (#40) |
| 30 | Matrix jargon | **Partly fixed:** sortable, but "Auto (Unipolar)" and mixed names remain (#30) |
| 31 | 75 % text | **Open** (#46) |
| 32 | Scope covers PLAY | **Open** (#41) |

## Overall

**5.5 / 10 against Serum 2.** The engine is broader than Serum 2: physical strings, 6-operator FM with real DX7 envelopes,
39 filters and 41 effects. The skin is clean, and some review-4 work landed: greyed FX library, matrix sort, consistent chip
shortening, an honest PHYSICAL empty state. But the newest feature, the DX7 fold-in, repeats the mistakes the earlier
reviews warned about. It is a second envelope system, a second LFO system, its own vocabulary, its own browser category and a
clipped diagram. Most of review 5's layout bugs are still there, and two now have exact causes (`MainPage.h:909`,
`EnvLfoPages.h:940-947`). Fixing #1-#8 and the two one-line layout bugs would do more for "powerhouse, not unwieldy" than any new
feature.
