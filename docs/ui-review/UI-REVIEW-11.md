# UI review 11 (pass 6): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `3ff0d38`. Evidence: `/home/user/shots11/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (cited as `set/file`), plus FX contact sheets I made from the 41 FX shots in `/home/user/shots11/review-extra/fxA..fxD.jpg`. I read the source in `src/gui/` to confirm causes. I could not click or drag, so live behaviour is marked *(inferred)*. The snapshot tool edits the patch on the OSC page, so "EDITED", the physical-to-wavetable flip after 03-OSC (the dead PHYSICAL page in `neuro/04-PHYSICAL`, `keys/04-PHYSICAL`) and the "+13" chip are tool artefacts; I did not count them. I did not render new sets: the 9 existing sets cover every page at 100 % and 75 %.

## Verdict and scores

Review 10's loud problems are gone. The mojibake is fixed, the table browser is themed, the Save dialog has air, the OSC drawer has a sane grouping, `VOICE` has one door, operators show one level on PLAY, the bottom bar is quiet, the matrix has a filter, macro warnings are words ("· OFF"), the PLAY strips have square thumbnails. The build now looks like one instrument, and on most pages a screenshot could be mistaken for a shipping product.

What is left is not big. It is **three kinds of small**:

1. **Features that appear only sometimes.** The PATCH tile shows on Init and Keys but not on Neuro, and vanishes with a 4th oscillator; the PHYSICAL page is a giant dim string when OSC 1 is not physical; the "›" on each PLAY strip is a navigation link in a different idiom from every other "EDIT ›". A new user cannot tell what is always there.
2. **Space still not owned by the content.** FX cards stretch (OTT's graph is 900 px wide for two knobs), the SEQ and PROB SEQ control rows leave 1,000 px of nothing, the Airwindows cards have an empty graph half, the DX7 OSC page shows a 450 x 570 sine and a second WAVE combo. These are all "the box is the size the layout gave it, not the size the content needs".
3. **Half-landed fixes** where the review 10 line of the doc was met but the screen was not: the DX7 operator still shows two level controls on the OSC page, the ARP lanes still draw bars while the ARP is off, the remap dock still touches its own caption and now truncates its tile labels, the matrix hint is cut to "Drag a source o...".

| Angle | Review 9 | Review 10 | Review 11 | Target |
|---|---|---|---|---|
| Against Vital | 8.0 | 8.4 | **8.7** | 9.5 |
| Against Serum 2 | 8.0 | 8.3 | **8.6** | 9.5 |
| Integration | 8.3 | 8.7 | **8.9** | 9.5 |

**Did the review 10 fixes land well?**

- *Square capped PLAY strips* (`MainPage.h:1297-1310`): **landed**. Thumbnails are square, no portrait ticks (`init/01-MAIN`, `neuro/01-MAIN`). But the cap is a compromise: strips are 215 px with a 110 px control row, so 40 % of each OSC card is still empty under the knobs on Neuro and Keys (V11-2), and the PATCH tile now depends on a state (V11-1).
- *Card-interior emptiness test*: I found no sign it exists as a gate in the shots (FX OTT, SEQ rows, DX7 OSC thumbnail all pass today). **Not verified**; the screens say the limit, if there is one, is loose (V11-3).
- *Text tracking limited*: **landed**. "PianoHammer" now reads "Piano Hammer", "AMPENV" reads "AMP ENV" (`keys/03-OSC`, `neuro/08-FM`). No regression seen at 75 % (`small/03-OSC`).
- *Header VOICES opens OSC > VOICE*: **landed** in the doc and tour; the VOICE tab is still the second of five bottom tabs on a page nobody opens for polyphony (S11-1).
- *OSC drawer regrouped* (SUB + NOISE, VOICE, SPREAD & DRIFT, STRINGS, SOUNDBOARD): **landed**, but the drawer switch at the far right changes meaning with the tab and is off-screen for most tabs (S11-4).
- *One level on DX7 operators*: **half-way**. PLAY and FM show OUTPUT only; the OSC page still shows OUTPUT and VOICE LEVEL side by side on the same operator (`dx7/03-OSC`) (I11-1).
- *Bottom bar: four macros plus used, with a +*: **landed** and quiet (`neuro/01-MAIN`), but the "+" tile floats between macro 4 and MASTER with no name, and the unassigned macros read "MACRO 1 + ASSIGN" on Init (S11-9).
- *Matrix filter*: **landed**; its field now cuts the page's own hint to "Drag a source o..." (`neuro/07-MATRIX`) (V11-8). A regression.

---

# Part 1: Against Vital

Vital's strengths I measure against: every page full and balanced, a modulation drag that always shows where it will land, one control style, type that is one size everywhere, and no state that appears only sometimes.

## Fixed since review 10 / still open (V10-1 to V10-22)

| # | Review 10 item | Status |
|---|---|---|
| V10-1 | "Â·" printed on wavetable views | **Fixed**: "frame 5 of 64, playing" (`neuro/03-OSC`). The caption still disagrees with the knob (V11-17) |
| V10-2 | Dead space moved inside PLAY strips; portrait thumbnails | **Partly fixed**: thumbnails are square; the interior below the controls is still about 40 % empty on Neuro/Keys (V11-2) |
| V10-3 | Dead-area test blind to card interiors | **Not verified** (V11-3) |
| V10-4 | PATCH tile/OUTPUT view never appear | **Half-fixed**: PATCH shows on Init and Keys; not on Neuro or any 4+ oscillator patch; OUTPUT view never seen (V11-1) |
| V10-5 | FX/SEQ cards stretch controls apart | **Still open** on FX (OTT, AW cards) and SEQ rows; VECTOR and PHYSICAL fixed (V11-4, V11-5) |
| V10-6 | Word gaps closed to fit | **Fixed** (`keys/03-OSC`, `neuro/08-FM`) |
| V10-7 | Labels in one row different sizes | **Fixed** in the shots I read (UNISON equals SEMI; KEY RATE equals CURVE at 100 %; `small/06` not re-rendered) |
| V10-8 | Unit spacing varies in a column | **Fixed**: "-30.9 dB", "-15.1 dB", "-7.5 dB" (`dx7/01-MAIN`) |
| V10-9 | Off modules keep their controls | **Mostly fixed**: SUB + NOISE folds on PLAY, BODY folds on PHYSICAL; the OSC page's SUB + NOISE strip still shows dimmed knobs (`dx7/03-OSC`) (V11-14) |
| V10-10 | "OPEN" badge where a switch belongs | **Fixed** (no pill in `neuro/05-FILTER`) |
| V10-11 | Save dialog tag field touches chips | **Fixed** (gap, `extras/extra-save-as`); two ways to enter tags remain (V11-26) |
| V10-12 | Table browser unthemed | **Fixed** (`neuro/table-browser`: themed HEAR, themed scroll bar) |
| V10-13 | FM matrix cells fixed size at 3 ops | **Still open** (V11-12) |
| V10-14 | VECTOR boxes air | **Fixed** (`neuro/02-VECTOR`: tight boxes); the pad is still empty when off (V11-24) |
| V10-15 | Two sizes of "add oscillator" | **Still open**, trivial (V11-30) |
| V10-16 | "OP ENV" over the curve; coloured routing caption | **Fixed** (caption at card foot) |
| V10-17 | Meter 64 px, grip crosses it | **Still open** (`neuro/01-MAIN` bottom right) (V11-10) |
| V10-18 | Mod rings touch labels | **Partly fixed**: FRAME ring still reaches its label (`neuro/01-MAIN`) (V11-23) |
| V10-19 | Time axes show two ticks | **Fixed** ("10 ms, 100 ms, 200 ms", `neuro/06-ENV-LFO`) |
| V10-20 | "GRID 8" looks like a label | **Fixed** (a combo with a chevron) |
| V10-21 | Long LFO caption | **Fixed** ("shared · restarts on note · 2 outputs") |
| V10-22 | Remap text tight | **Still open** (V11-9) |

## Findings (Vital), worst first

**V11-1. High: the PATCH tile is state-dependent, so PLAY has no stable shape.**
- What: Init and Keys end the left column with a PATCH signal-flow tile (`init/01-MAIN`, `keys/01-MAIN`). Neuro does not (`neuro/01-MAIN`: two open strips plus a folded one, "add OSC 4" row, SUB + NOISE, then nothing). Add a fourth oscillator and it goes (`neuro/added-osc-MAIN`); a DX7 voice never has it (`dx7/01-MAIN`). The OUTPUT view never appears in any shot. The reason is the spare-height arithmetic in `MainPage.h:464-471, 546-557` with `patchMinHeight = 90`. A user who learns that PLAY shows the patch flow loses it the moment they add a fold.
- Vital: a panel is either in the layout or not; it never comes and goes with other panels' heights.
- Fix: decide. Either PATCH is a permanent 90 px row (strips cap at about 150 px and scroll or fold before it is dropped), or it moves to the header as a toggle. Delete the OUTPUT view code if it never fits.
- Evidence: `init/01-MAIN` vs `neuro/01-MAIN` vs `neuro/added-osc-MAIN`.

**V11-2. Medium-high: PLAY oscillator strips are still 40 % empty inside, and the title column is empty.**
- What: a Neuro strip is about 215 px tall: the controls sit in the middle 110 px with 50 px above and below (`neuro/01-MAIN`). The left 170 px under "OSC 1 / OUT" is blank. SUB + NOISE has the same hole and 40 px of air under its knobs. Keys's OSC 1 has the same (`keys/01-MAIN`).
- Vital: cards are as tall as their controls; spare height goes to the graph panels on the right.
- Fix: strips at 150 px (`maxGrownSlotHeight`, `MainPage.h:1310`), give the 65 px per card back to the right column's FILTER response and ENVELOPE graphs.

**V11-3. Medium-high: there is no evidence in the build of the card-interior emptiness test, and FX/SEQ/Airwindows pass what a test would catch.**
- What: the largest blank rectangle inside a card on current shots: OTT card (`neuro/10-FX`) about 400 x 200 px; AW COMP card (`fxD.jpg` first shot) about 330 x 170 of empty graph area; SEQ's controls row about 1,000 x 120 (`neuro/gen-probseq`); the DX7 OSC page's WAVE row beside a 250 px blank (`dx7/03-OSC`).
- Fix: a UI test: largest content-free rectangle per card, limit 25 % of the card area, failing today on those four. (inferred that no such test exists; I did not run `--uitest`.)

**V11-4. Medium-high: FX cards stretch their graph, not their content.**
- What: with three FX, OTT takes a full-width row: its IN/OUT graph is 900 px wide and two knobs sit right of it (`neuro/10-FX`). VOWEL (card 1) and DRIVE (card 2) are half width. The same effect type looks three different widths depending on what is next to it (`fxA.jpg`: Comb, Phaser full width; Drive half). Phaser's NOTCHES display is a 1,000 px stripe.
- Vital: an effect card has one width per type; the order of cards never changes how a card looks.
- Fix: three fixed widths by control count (2-3 knobs: half; 4-6: full), displays capped at 300 px with the surplus given to knob gaps. Cards that do not fill a row sit left; do not stretch the last one (`FxPage.h` row layout).

**V11-5. Medium-high: SEQ control rows leave most of the width empty and align inconsistently.**
- What: ARP row: RATE (combo, left-aligned) / STEPS / GATE / MODE (combo) / OCTAVES / CHANCE, each in a fixed slot so the combos sit left of centre and the knobs centred in the same slot, 1,000 px in total (`neuro/09-ARP-SEQ`). PROB SEQ: three controls then 1,000 px of nothing (`neuro/gen-probseq`). GENERATE: three sub-boxes each with 30-50 px of nothing below their controls.
- Vital: n/a (it has no step sequencer); Vital's LFO page keeps one control row left-aligned.
- Fix: left-align every row on one 90 px grid; give the lane editor the freed height (the grid is 340 px; lanes are cramped at 16 steps).

**V11-6. Medium: the Airwindows FX cards reserve a graph area they leave empty.**
- What: AW COMP, EQ, CONSOLE, WIDENER, CRUSH show an ALGORITHM combo on the left in a 330 px blank column where the built-in card has its graph (`fxD.jpg`, `fxC.jpg` row 2). Built-in COMP on the same page has a transfer curve. The card is half empty and the control that matters (ALGORITHM) is small and far from its title.
- Vital: n/a.
- Fix: put ALGORITHM in the card header beside the BUILT-IN | AIRWINDOWS switch (where the algorithm name is half-printed already: "AIRWINDOWS · TO TAPE 8" in `fxC.jpg`) and let the knobs start at the left edge.

**V11-7. Medium: the DX7 OSC page's big picture is a 450 x 570 sine with a second WAVE picker under it.**
- What: `dx7/03-OSC`: the left panel is a full-height sine and a "< Sine >" picker at its top; the WAVE row on the right repeats the same choice as a combo ("Sine"). On the same page the OP ENV graph is wide and good. Two controls for one choice, and the larger picture shows the least useful thing (a sine for a carrier).
- Fix: on operator mode, draw the OP ENV curve large on the left and keep only the WAVE combo; or hide the prev/next picker when the mode is Operator.

**V11-8. Medium: the matrix filter field truncates the page's hint to "Drag a source o...".**
- What: `neuro/07-MATRIX`: the hint ends mid-word at x = 592, the filter field starts at 611. The hint is drawn into a rect trimmed 280 px left (`MatrixPage.h:114`) while the filter sits over it. Regression from the filter.
- Fix: put the filter right-aligned, the hint left, and drop the hint once a route exists (it is already 12 routes; the hint's own condition is `visibleRows.empty()` or equal, so it shows too often).

**V11-9. Medium: the remap dock's text still touches QUICK SHAPES and its tile labels are cut.**
- What: `remap/remap-editor`: "Click to add a point, drag the middle dots..." sits 2 px above the caption; tile labels read "STRAIG..." and "DEADZ..." (the names are "Straight" and "Dead Zone", `RemapEditor.h:95`); the row beneath the open dock is cut half-way through (row 9 at y = 635). The code says "6 px clear" (`MatrixPage.h:218`), the render says otherwise.
- Fix: tile width 62 px or the names "LINE" and "DEAD"; hide the third hint sentence when the dock is under 190 px tall; stop the list at a whole row.

**V11-10. Medium: the output meter is still the smallest instrument on the screen and the grip crosses it.**
- What: 150 x 24 px at the bottom right, no numbers, with the resize grip drawn over its right end (`neuro/01-MAIN`). The big L/R meter exists only inside SCOPE (`neuro/scope-panel`). Vital has a 100 px stereo meter in the header.
- Fix: 110 px wide, left of the grip, show the peak number on hover; or put the same meter in the header beside CPU.

**V11-11. Medium: the filter SIGNAL FLOW is a big black box with little in it, and cramped when busy.**
- What: Neuro: 470 px tall, nodes in the top 60 %, the BALANCE knob floating bottom right, WEST/BODY ghost boxes at the bottom (`neuro/05-FILTER`). With WEST and BODY on and parallel mode (`neuro/filter-flow-busy`) the same box is cramped: "STRINGS + SOUNDBOA" wraps and is cut, BYPASS crosses a line, PARALLEL overlaps F1/F2.
- Fix: size the node boxes to the box (min 26 px apart), shorten "STRINGS + SOUNDBOARD" to "PHYSICAL" (see I11-10), and shrink the flow card to its content; give the freed height to RESPONSE.

**V11-12. Medium: the FM matrix is a 3 x 3 grid of 140 px cells on a 3-operator patch, and the DX7 matrix has dead space below.**
- What: `init/fm-no-input`, `neuro/08-FM`: nine big cells, one with a dot, two with ghosts; EXTRAS block below with a prose caption. `dx7/08-FM` is well filled, so only the small case is wrong. The algorithm strip shows "+3" on all 16 DX7 tiles on a 3-operator patch (`init/fm-no-input`; `FmWidgets.h:171`), which is noise: all tiles say the same.
- Fix: 100 px cells when 3 or fewer operators, centred in the card; drop the "+n" badge when every tile has the same n, and print "needs 6 oscillators" once in the card caption.

**V11-13. Medium: the filter response graph's title is drawn as a tab.**
- What: "RESPONSE" (underlined) on FILTER and "MODULATION", "ALGORITHMS" on MOD/FM have the same underline as a selected tab, but click nothing (`neuro/05-FILTER`, `neuro/07-MATRIX`). Underlined small caps read as a switch.
- Fix: plain section captions, no underline; underline only for real tabs.

**V11-14. Medium-low: the OSC page's SUB + NOISE strip keeps all its controls when off.**
- What: `dx7/03-OSC`, `keys/03-OSC`: "sub off, no noise: switch the sub on to set them" is printed, with the switch far right, but the strip is fully tall; on PLAY the same module folds to 40 px. One rule, applied on one page only (the V10-9 fix was meant to be all three).
- Fix: fold the drawer body to the header line when the module is off, same animation as PLAY.

**V11-15. Medium-low: the PHYSICAL page with no physical oscillator is a huge dim picture and a default-styled button.**
- What: `keys/04-PHYSICAL`, `neuro/04-PHYSICAL`: a 1,400 x 500 dim string and body with ghost labels "BOW", "NO BODY", a sentence, and a flat grey "SWITCH TO PHYSICAL" button not in the theme's button style (`FilterVectorPhysicalPages.h:530`). (Reached in real use whenever OSC 1 is wavetable and the user opens PHYSICAL, so I count it, though the snapshot tool causes it here.)
- Fix: show the empty state at card height only: sentence, a normal outlined button, and a small preview above. The page tab itself is already dimmed; so a click on it could switch OSC 1 and show the string.

**V11-16. Low-medium: the ARP lanes draw full-strength bars while ARP is off.**
- What: `neuro/09-ARP-SEQ`: VEL and STEP LEN draw 16 grey bars, "ARP OFF" in the corner, text says "draw to switch it on". Dimmer than before (a change since review 10) but still a pattern that looks in use. (S10-13 still open.)
- Fix: lanes at 25 % opacity with only a faint baseline until the first drawn bar.

**V11-17. Low-medium: the FRAME caption and the knob still disagree.**
- What: "frame 5 of 64, playing" under a knob at 30 % (`neuro/03-OSC`). The code comment says playing includes modulation (`WaveDisplay.h:1005`), which is right, but the user sees two numbers. A moving frame under LFO 1 is expected, a static one is not. (inferred whether the snapshot was mid-sweep.)
- Fix: "now frame 5 · knob 30 %" when modulated, plain "frame 19 of 64" when not.

**V11-18. Low-medium: the "›" on each PLAY strip's title has no label and no hover state.**
- What: `neuro/01-MAIN`: a faint chevron at x = 173 beside "OSC 1" on every strip (`MainPage.h:1210-1214`). The conventions say jumps are "EDIT <WHAT> ›" (`UI-CONVENTIONS.md`); this is a bare chevron, so it is the one jump in the app with no words.
- Fix: write "EDIT ›" at the right of the title row like FILTER/ENVELOPE/LFO on the same page.

**V11-19. Low-medium: the filter card's "F1 / F2" and "EDIT ›" pills are three different pill styles in one line.**
- What: `neuro/01-MAIN`: F1 (purple rimmed, selected), F2 (flat), "EDIT ›" (flat, no rim) in one row; ENVELOPE row: AMP ENV (red rim), FILT ENV, FILT 2 ENV, EDIT ›. The EDIT link looks like a fourth tab.
- Fix: set the link apart with a 12 px gap and the link style (text only, accent colour), as the conventions' `styleJumpLink` describes.

**V11-20. Low: tiny type below 10 px in several places.**
- What: tile labels in the remap dock, "KEY UP", axis ticks, "OUT"/"CLIP" in the meter, "BANDS" on OTT: about 9 px logical, 7 px at 75 % (`small/` shots). (inferred from the pixel heights.)
- Fix: floor of 10 px logical for anything that carries information; ticks may stay smaller.

**V11-21. Low: the header's "BPM 120.0" and "CPU 3 %" are still two grey fragments above the preset arrows.**
- What: `neuro/01-MAIN`: right of the preset name, small grey text on two different baselines from the VOICES button. (V9-27, "mostly fixed", is still only mostly.)
- Fix: same baseline as VOICES; both as 24 px-tall read-outs with the same rim.

**V11-22. Low: the header scope squiggle still reads as an underline of the preset name.**
- What: `neuro/01-MAIN`, `init/01-MAIN`: a red line at the foot of the preset box. (S10-14 still open.) On Init it is a flat line that looks like a border bug.
- Fix: put it in a 22 px band with a baseline, or remove it (SCOPE exists).

**V11-23. Low: modulation rings still touch the label on the FRAME knob.**
- What: `neuro/01-MAIN`: FRAME's three coloured rings run up to the "FRAME" label; UNISON's dots touch its label too.
- Fix: 4 px more space below labels on knobs that draw rings (`ParamControls.h`).

**V11-24. Low: VECTOR pad is blank when the engine is off.**
- What: `neuro/02-VECTOR`: a 720 px square of grid with a single dim dot and the text "VECTOR OFF · switch on to mix the corners". The switch is a tiny toggle at the far top right of the card.
- Fix: show the four corner waveforms dim in the corners and put the switch next to the title (the convention says header right, but this card is 1,500 px wide).

**V11-25. Low: "OSC 4 (not added)" is still offered in the VECTOR corner combo.**
- What: `neuro/02-VECTOR`; `FilterVectorPhysicalPages.h:474`. A choice that can only produce silence.
- Fix: list only added oscillators plus "ADD OSC 4 ›" (an action in the menu, sentence case).

**V11-26. Low: SAVE dialog: name warning is applied after typing, and tags can be typed or clicked.**
- What: `extras/extra-save-as`: orange sentence about ":" and "/"; the TAGS field and the tag chips edit the same list (V9-30, V10-11).
- Fix: strip the characters as typed; make the field read-only (the chips edit it).

**V11-27. Low: the tour's keyboard-shortcut line is a row of small grey text with no key styling.**
- What: `neuro/00-tutorial`: "Ctrl+1-7 switch tabs  Ctrl+Shift+1-3 switch pages  Ctrl+Z / Ctrl+Shift+Z undo / redo  Ctrl+S save" in one line, no spacing between pairs.
- Fix: draw keys as small rimmed caps; two columns.

**V11-28. Low: 75 % layout holds, but text in the matrix and tabs reaches the floor.**
- What: `small/*` render without cuts; the shots show smaller labels but no clipping. No new defect; only noting that V11-20 matters most there.
- Fix: none beyond V11-20.

**V11-29. Low: "FX" dice and SAVE / LOAD CHAIN on the FX page are three different button styles.**
- What: `neuro/10-FX`: CHAIN 1 (filled), CHAIN 2 (flat), COPY TO 2 (flat), a dice with "FX" (rimmed, icon), SAVE / LOAD CHAIN (flat, wider). The dice-with-label is the only icon button on the page.
- Fix: one group: chain tabs, then a gap, then COPY TO 2, then a "..." menu holding RANDOMISE FX and SAVE CHAIN / LOAD CHAIN.

**V11-30. Low: "ADD OSC" is a dashed pill on OSC (128 px) and a dashed row on PLAY.**
- What: unchanged since review 10 (V10-15): `neuro/03-OSC` vs `neuro/01-MAIN`. Harmless; say so in the conventions or make them one shape.

---

# Part 2: Against Serum 2

Serum 2's strengths I measure against: the oscillator page as the main event, a modulation matrix with one-line rows, the 'drag to anywhere' flow, a browser that is a destination, and effects with graphs and a clear order.

## Workflow speed (the same four jobs)

| Job | Serum 2 | ilanaSynth (this build) |
|---|---|---|
| Change polyphony | OSC/Global tab, one click | Header VOICES opens OSC > VOICE (**one click**, but the tab is second in a drawer, S11-1) |
| Assign an LFO to a knob | Drag from the LFO header | Drag a chip onto any knob; chips in one bar (**as fast**) |
| Edit a modulation depth | Drag the ring on the knob | Same, and the matrix filters by name (**faster for the 12th row**) |
| Audition and pick a preset | Browser tab, one click per row | Pop-up/docked browser, details card, HEAR for tables (**as fast**, S11-8) |
| Load a sample and map it | Drop on the oscillator | LOAD... in the sample page; drop *(not verified)* (S11-10) |

## Fixed since review 10 / still open (S10-1 to S10-15)

| # | Review 10 item | Status |
|---|---|---|
| S10-1 | Voice settings have two doors, two names | **Fixed** (header VOICES opens OSC > VOICE) |
| S10-2 | SEQ "Off" combos duplicate the switches | **Fixed**: STRUM/SPRAY show a switch and the word "off", the combos read the remembered choice (`neuro/09-ARP-SEQ`) |
| S10-3 | "UNISON" two things on one page | **Fixed** (the drawer tab is "SPREAD & DRIFT") |
| S10-4 | Drawer of unrelated tabs | **Partly fixed**: grouped, but the switch at the right behaves per tab (S11-4) |
| S10-5 | Browser pop-up and docked differ | **Fixed**: pop-up has the details card (`extras/extra-browser-dropdown`); the two differ only in size and the DOCK/FLOAT label |
| S10-6 | 3D/WAVE captions, cut hint | **Mostly fixed**: "frame 5 of 64, playing", "drag across: frame"; the "up / down: warp" half of the hint is still missing in 3D view (S11-6) |
| S10-7 | FX header row names | **Fixed** (SAVE / LOAD CHAIN, dice "FX"); still three styles (V11-29) |
| S10-8 | Matrix is 12 rows and a hard stop | **Partly fixed**: filter added; no scroll seen, since 12 rows fit; "12 of 64 routes" and rows 34 px |
| S10-9 | PROB SEQ/EUCLID rows | **Still open** (V11-5) |
| S10-10 | Macros 5-8 "ASSIGN" placeholders | **Fixed** on used patches; Init shows "MACRO 1 + ASSIGN" x 4 (S11-9) |
| S10-11 | "AW" boxes in the add menu | **Fixed in part**: a legend line "AW = the Airwindows version of the effect beside it" (`neuro/fx-empty`); "AIRWINDOWS" is spelled out on two rows, "AW" on others (S11-12) |
| S10-12 | GENERATE boxes three heights | **Still open** (V11-5) |
| S10-13 | ARP VEL lane draws full bars while off | **Half-fixed** (V11-16) |
| S10-14 | Header scope squiggle | **Still open** (V11-22) |
| S10-15 | NAME accepts ":" and "/" | **Still open** (V11-26) |

## Findings (Serum 2), worst first

**S11-1. Medium-high: VOICE is the second of five drawer tabs under the oscillator and nothing on the OSC page says polyphony lives there.**
- What: the header VOICES button opens it (good), but a user on the OSC page sees "SUB + NOISE | VOICE | SPREAD & DRIFT | STRINGS | SOUNDBOARD" and no word "polyphony" (`neuro/03-OSC`). Mode, bend range, glide and legato are global to the patch, but sit in a drawer headed by one oscillator's SUB.
- Serum 2: Global tab, own page for voicing, bend, glide.
- Fix: put VOICE first in the drawer and name it "VOICE · mono / poly / legato / glide" in its caption, or hoist the voice settings into the header VOICES drop-down as a compact panel (convention says one place; keep that and make the drawer's first tab the default whenever the header opens it, which it does).

**S11-2. Medium-high: the OSC page's card has headings (SHAPE, PITCH & LEVEL, UNISON) but knob columns are not aligned between them.**
- What: `neuro/03-OSC`: FRAME sits at x = 710, LEVEL at 712 (aligned), but WARP and SPECTRAL combos float at 845/1115, TUNING combo at 960, ENVELOPE at 1340; the three rows share no grid beyond the first column. In `dx7/03-OSC`: OUTPUT at 1208, PAN at 1322, ENVELOPE at 1428; WAVE row has FEEDBACK at 866, FB TYPE at 995, VOICE LEVEL at 1136. Each row looks hand-placed.
- Serum 2: rows aligned on one column grid; combos live in one column.
- Fix: one 114 px column grid shared by the three rows, combos taking one cell width each.

**S11-3. Medium: the OSC page's right-hand inner panel leaves a third of its width blank.**
- What: `neuro/03-OSC`, `keys/03-OSC`: the knob panel is 1,040 px wide, content ends near x = 1,420 of 1,540; the bottom 90 px of the panel is empty too. On the sample page the same holds.
- Fix: size the card to content; use the rest for a mini scope or the frame table list (Serum shows the table position along a slider).

**S11-4. Medium: the drawer's right-hand switch is the SUB + NOISE switch only, but it stays on screen for every tab.**
- What: in `neuro/osc-sympathetic` the strip shows the SUB + NOISE tab with a dot, an orange switch at x = 1,510; when another tab is selected (STRINGS, SOUNDBOARD) *(inferred from `keys/04`: the switch is the grey off-state)*, the switch at the right belongs to that tab. The text "sub off, no noise: switch the sub on to set them" (`dx7/03-OSC`) is long, and the control it points to is 1,000 px away.
- Fix: each tab draws its own header with its own switch at the left of the caption; the strip frame carries nothing.

**S11-5. Medium: SEQ is three screens in one (ARP lanes, PROB SEQ lanes, a piano roll) with a different control row each.**
- What: `neuro/09-ARP-SEQ`, `neuro/gen-probseq`, `neuro/gen-clip-expanded`. Each has RATE and STEPS in different places and different sizes, and the clip has "SLOT / MODE / LENGTH / GRID / ZOOM / DRAW / QUANTISE / IMPORT / EXPORT / COLLAPSE" in one row of ten.
- Serum 2: n/a (it has no clip engine); a competent step page keeps rate/length in a fixed strip.
- Fix: one shared top strip: RATE, STEPS, GATE (and the engine's extras after). The clip's buttons into a group: SLOT/MODE/LENGTH/GRID on the left, DRAW/QUANTISE | IMPORT/EXPORT on the right.

**S11-6. Medium: 3D view hint still drops half its words.**
- What: `neuro/03-OSC`: "drag across: frame" (the "up / down: warp" half was cut; `osc-pd-chain.png` shows the full "drag up / down: warp" on WAVE). Serum shows both gestures at all times.
- Fix: one line "drag: frame (across) · warp (up / down)" at 9 px, or the hint in the view's tooltip only, and a one-time text overlay.

**S11-7. Medium: the table browser has no category tabs outside the scroll, and no way to load a user table from it.**
- What: `neuro/table-browser`: search + HEAR, then BASIC / ANALOG ... scroll. No favourites row (the preset browser has one), no "LOAD .WAV" in it (the button is on the OSC card, so it takes two steps). (inferred from the one shot; the stars the review 10 text mentions are not in this shot's tiles.)
- Fix: pin FAVOURITES and USER above BASIC; a "LOAD..." tile at the end of USER.

**S11-8. Medium-low: the preset browser has no sort by date or rating and no 'recent'.**
- What: `extras/extra-browser-docked`: "SORT: NAME" only (inferred menu), Favourites 0, User 0; categories by count only.
- Fix: RECENT and NEW filters above the categories; sort menu items: name, category, newest.

**S11-9. Medium-low: unassigned macros on a patch with none read "MACRO 1 + ASSIGN" four times and the "+" tile floats unlabelled.**
- What: `init/01-MAIN`: four copies of the same pill. The "+" between macro 4 and MASTER (`neuro/01-MAIN`, x = 1170) is not tied to macros or to MASTER.
- Fix: the unassigned macros as one dim "MACROS: drag a chip here, or + ASSIGN"; the "+" moved to sit directly after macro 4 with the tooltip "Show macros 5 to 8".

**S11-10. Medium-low: SAMPLE page: the zone map is the hero, but mapping is by reading a text row.**
- What: `neuro/osc-sample-sfz`: "test 2 zones · 1-shot", key map C1 to C5 with two tinted blocks. No visible root-note marker or velocity layers in the picture; no drag-drop hint. (inferred for drag-drop.)
- Fix: root marker on each zone, "drop a .wav or .sfz here" text in the empty state (`osc-sample-empty`).

**S11-11. Low-medium: LFO pool chips and the routing text in them are cut.**
- What: `neuro/06-ENV-LFO`: "FILTER 1 › Cutoff +2 more" sits over the LFO thumbnail; LFO 2's "OSC 1 › Frame +1 more" crosses its waveform. The text is useful; the chip is 50 px tall and carries three things.
- Fix: the thumbnail 20 px lower, routing text in its own row.

**S11-12. Low-medium: "AW" and "AIRWINDOWS" in the same menu.**
- What: `neuro/fx-empty`: small "AW" boxes on 8 rows; the words "AIRWINDOWS" (as a right-aligned tag) on TAPE and CONSOLE; the legend line at the foot says AW = Airwindows. Two spellings for one thing, in a 1,500 px menu with room.
- Fix: write "AIRWINDOWS" on every row that has it, in the right-hand tag slot.

**S11-13. Low-medium: "ENV 5", "ENV 6", "ENV 16" and "9 MORE" in the envelope pool are numbered names beside named ones.**
- What: `neuro/env-pool-full`: AMP ENV, FILT ENV, FILT 2 ENV, MOD ENV, ENV 5, ENV 6, ENV 16. The "MOD ENV" name is unlike all others.
- Fix: rename MOD ENV to ENV 4 (see I11-6), keep the first three named.

**S11-14. Low: the FX output row's caption repeats the section name.**
- What: `neuro/10-FX`: "OUTPUT · SOFT CLIP · CLIP GAIN · after the rack, before the master volume". Fine content, but the row is 1,500 px for three items; and "SOFT CLIP" is the toggle's label above its own switch, unlike the rest of the page where a label sits left of the switch.
- Fix: label left of the switch; caption right-aligned.

**S11-15. Low: the browser's 'UP/DOWN browse · Enter keep · Esc close' text differs between the pop-up (right of the title) and the docked one (centre).**
- What: `extras/extra-browser-dropdown` vs `extra-browser-docked`. Trivial; the same hint, two positions.
- Fix: one position.

**S11-16. Low: the tour card still runs the width of the window.**
- What: `neuro/00-tutorial`: a 1,380 px card with lines 1,000 px long; hard to read.
- Fix: 640 px column.

**S11-17. Low: LFO "SEED: Free" and EXCITE POS "Auto" are words in a number slot.**
- What: `neuro/lfo-sim-lorenz`, `neuro/physical-page`. (I10-15, open.) It reads as a label that forgot its value.
- Fix: "Random" and "Auto" in the value colour used by other menu-like values, or a switch.

**S11-18. Low: macros 5-8 appear only when assigned; there is no visible way to see them all.**
- What: `neuro/01-MAIN`: four named macros and "+". `extras/extra-browser-docked` lists macros 1-4 for the preset. The "+" is the only door. (inferred: macros 5-8 appear in the "used" order.)
- Fix: tooltip on "+": "Add a macro (up to 8)".

---

# Part 3: Integration (DX7/FM, physical, sample, Airwindows and the rest)

The question: does each newer engine look like the rest, or does it have its own corner, vocabulary or widgets?

## Fixed since review 10 / still open (I10-1 to I10-16)

| # | Review 10 item | Status |
|---|---|---|
| I10-1 | DX7 operator: two level controls | **Half-fixed**: PLAY and FM show OUTPUT; the OSC page shows OUTPUT and VOICE LEVEL (`dx7/03-OSC`) (I11-1) |
| I10-2 | "Go to the editor" / "pick an oscillator" drawn differently | **Partly fixed**: OscPicker is shared (pills on OSC, PHYSICAL, FM); the PLAY chevron is still a lone idiom (V11-18); FM operator chips in the card are a fourth pill set (I11-3) |
| I10-3 | Randomiser and SAVE names | **Fixed** (conventions list them; menu and FX use RANDOMISE and SAVE / LOAD CHAIN) |
| I10-4 | DX7 stack seams | **Partly fixed**: "EDIT OP PITCH · OP LFO ›" button on FM; the OP ENV tab is first on PLAY and MOD; "operator mode" on OSC (I11-1, I11-2) |
| I10-5 | Ring mod/sync/noise FM inline vs behind a button | **Fixed**: always one EXTRAS row/button; 3-op patch shows it inline, DX7 behind "EXTRAS · RING MOD · SYNC · NOISE FM ›" (`dx7/08-FM`): still two layouts (I11-5) |
| I10-6 | Sim LFO switch label cut | **Fixed** ("PER VOICE", `neuro/lfo-sim-lorenz`); TRIGGER, OUT 1 AXIS and FIRE still three controls in one row |
| I10-7 | Engine cards drawn with different structure | **Partly fixed** (I11-6) |
| I10-8 | FM depth shown three ways | **Still open** (I11-4) |
| I10-9 | Airwindows reads as a plug-in | **Partly fixed** (S11-12, V11-6, I11-7) |
| I10-10 | STRINGS / SYMPATHETIC STRINGS / BODY / SOUNDBOARD | **Mostly fixed**: tab "STRINGS", "SOUNDBOARD" the same as PHYSICAL's; the signal flow still says "STRINGS + SOUNDBOARD" (I11-8) |
| I10-11 | LOAD and LOAD .WAV | **Fixed** ("LOAD..." both, `neuro/osc-sample-sfz`, `neuro/03-OSC`) |
| I10-12 | PHYSICAL page non-physical state contradicts itself | **Half-fixed** (V11-15) |
| I10-13 | KEYS meaning | **Fixed** (SEQ chain: "KEYS > STRUM > ... > VOICES") |
| I10-14 | Macro warnings bare triangles | **Fixed** ("TALK · OFF", `neuro/scope-panel`) |
| I10-15 | "Free"/"Auto" in value slot | **Still open** (S11-17) |
| I10-16 | A/B, CHAIN copies unguarded | **Settled** |

## Findings (integration), worst first

**I11-1. High: a DX7 operator still has two level controls on the OSC page, and the OSC page's picture is the wrong one.**
- What: `dx7/03-OSC` shows OUTPUT (0.0 dB, PITCH & OUTPUT row) and VOICE LEVEL (50 %, WAVE row) on the same operator. The PLAY and FM pages show one (OUTPUT). A DX7 user who knows only "output level" sees "VOICE LEVEL" and cannot tell what it adds. (`OscPage.h:332` names `_level` "VOICE LEVEL"; `FmInputPages.h:509` comments on the choice.) Meanwhile the left picture is a huge sine; the OP ENV graph is on the right (V11-7).
- Benchmark: Dexed and the DX7 have one operator output level; Serum has one level.
- Fix: for operator mode hide VOICE LEVEL (leave it at 100 %, or fold it into OUTPUT in the dB display); if it must exist, put it under an "ADVANCED" caption and not at the same weight. (Changing its function would alter DX7 presets' levels; hiding only changes display, and "settled" says to keep levels.)

**I11-2. Medium-high: the DX7 stack still has four doors for the same operator, three with different pills.**
- What: PLAY strips (OSC 1-6 with OP ENV thumbnails), the OSC page's pills ("OSC 2 MOD › 1"), the FM page's pills (1..6, bare numbers) and the MOD page's OP ENV tab pills (1..6) (`dx7/01-MAIN`, `dx7/03-OSC`, `dx7/08-FM`, `dx7/06-ENV-LFO`). Same operator, three notations: "OSC 2", "MOD › 1", "2".
- Fix: the pill is always the OscPicker (dot + "OSC n"); the number alone only when narrow (convention). FM and OP ENV should use it too (`FmWidgets`).

**I11-3. Medium-high: FM operator pickers are a fourth style.**
- What: `dx7/08-FM`: "(•) 1  (•) 2 ..." as a pill row at the right of the OSC 1 card; on the OSC page the pills are wide ("OSC 1 OUT") and have a role tag. The convention says OscPicker everywhere.
- Fix: use OscPicker in `FmInputPages.h`; role tag optional.

**I11-4. Medium: FM depth is still shown in four places.**
- What: the diagram's arrow label ("30 %", `neuro/08-FM`), the matrix cell's knob ("30 %"), the OSC page's "FM FROM OSC 2" caption (no number, `neuro/03-OSC`), the modulation matrix row "FM › OSC 2 › OSC 1" (`neuro/07-MATRIX`). All edit the same value via different widgets. The caption on the OSC page is the only one that does not show a number or a link.
- Fix: make the OSC caption a link ("FM FROM OSC 2 · 30 % ›" to the FM page) and keep the other two; the matrix rows already group by destination.

**I11-5. Medium: ring mod / sync / noise FM take two layouts.**
- What: 3-op patch: an inline block "EXTRAS · OSC 1 x OSC 2 only" with a caption of two sentences (`neuro/08-FM`). DX7 patch: one button "EXTRAS · RING MOD · SYNC · NOISE FM ›" (`dx7/08-FM`) that opens something not rendered.
- Fix: one layout: always the button; the inline controls appear in the popover.

**I11-6. Medium: engine cards differ in structure: header, tabs, switch and EDIT link are at different places per engine.**
- What: PHYSICAL's BODY switch left of the label and SOUNDBOARD's right of it (`neuro/physical-page`, `keys/04`); WEST has its switch at the far right with the caption between; BODY the same; the OSC drawer's switch is far right; FX cards: switch top right; FM operators: pills.
- Fix: one header: title, caption, spacer, EDIT link, switch (the card-header helper `IlanaTheme::cardSwitchBounds`); and PHYSICAL's sub-boxes use it.

**I11-7. Medium: Airwindows keeps its own vocabulary: SOFTEN, HEAD BUMP, FLUTTER, "Pressure 5", "Console 7 Channel".**
- What: `fxC.jpg` cards. The algorithm name is the plug-in's own, and the knobs are named by algorithm; the built-in COMP card says THRESH, RATIO, ATTACK. This is legitimate for a port, but nothing tells a user that these are a second engine apart from a small segmented BUILT-IN | AIRWINDOWS control, and the header sometimes shows "AIRWINDOWS · TO TAPE 8" in full caps in the title line.
- Fix: keep the names, but place the ALGORITHM combo in the card header (V11-6) and add a one-line description under it taken from the algorithm table.

**I11-8. Medium-low: the signal flow says "STRINGS + SOUNDBOARD" while the OSC drawer and PHYSICAL page say STRINGS and SOUNDBOARD separately and the FILTER flow in review 10 said "STRINGS".**
- What: `neuro/filter-flow-busy` (wrapped, cut: "STRINGS + SOUNDBOA"); `keys/01-MAIN` PATCH tile: "SOUNDBOARD"; `neuro/05-FILTER`: "STRINGS". Three names for the final stage depending on which parts are on.
- Fix: the node reads "PHYSICAL" with a sub-line, or two nodes; never a concatenation that must wrap.

**I11-9. Medium-low: unison on a Physical oscillator exists but means nothing visible.**
- What: `keys/03-OSC`: the UNISON row (UNI MODE, UNISON 3, DETUNE 0 ct, BLEND, SPREAD, CHORD) under a Physical string with no explanation; physical strings detune by their own banks. (inferred whether it is audible.)
- Fix: label the row "UNISON · copies of this string" or hide when mode is Physical.

**I11-10. Medium-low: three words for the oscillator's own output: OUT, LEVEL, VOICE LEVEL, OUTPUT.**
- What: PLAY strip captions say "OUT" (a role: the oscillator goes to the output), LEVEL (knob), OUTPUT (the DX7 knob in dB), VOICE LEVEL (OSC page, operator). "OUT" next to "OUTPUT" in one strip (`dx7/01-MAIN`: "OSC 1 OUT" with OUTPUT knob right of it).
- Fix: the role caption reads "TO FILTER" / "MODULATES 1"; keep OUTPUT for the DX7 level.

**I11-11. Medium-low: sample and Live oscillators put no engine-specific warning in the header and show the wavetable's UNISON/SPREAD row with dead values.**
- What: `neuro/osc-sample-sfz`: UNISON 1, DETUNE 15 ct, BLEND 100 %, SPREAD 50 % in dim, with CHORD Off. Dim is right; but the section is full height for one live control (UNISON).
- Fix: fold the row to a header line when unison = 1 (as FILTER folds WEST).

**I11-12. Low-medium: the vocabulary in the FM algorithms strip: "B1 ... B9", "BASIC", "DX7 1-16", "DX7 17-32", "CUSTOM: NEAR B1 2-OP STACK".**
- What: `neuro/08-FM`: the BASIC set is named B1-B9 (no names), a custom routing says "NEAR B1 2-OP STACK". Names a user will never learn.
- Fix: "STACK 2", "CHAIN 3", "PARALLEL 3" etc.; keep "ALGORITHM 5" for DX7.

**I11-13. Low-medium: ARP/EUCLID/PROB SEQ/CLIP have a "dot + word" tab but PATTERN chain says KEYS > STRUM > EUCLID > CLIP > VOICES in a different colour scheme.**
- What: `neuro/gen-clip-expanded`: active parts of the chain in orange, inactive grey; the tabs on the right use a switch. Two indicators for one state, 700 px apart (the chain at the left and the tab switches at the right). Reasonable, but a click on the chain does nothing *(inferred)*.
- Fix: make a chain word a click target that selects the engine tab.

**I11-14. Low: "EDIT TABLE ›", "EDIT STRING ›", "EDIT OP ENV ›", "EDIT OP PITCH · OP LFO ›", "EDIT SOUNDBOARD ›", "EDIT ON FM ›": consistent form, mixed placement.**
- What: sometimes at the right of the card title (OSC), sometimes at the foot of the card (SOUNDBOARD, `neuro/physical-page`), sometimes under a paragraph (PLAY's OP ENV, `dx7/01-MAIN`), sometimes beside the OSC pills (`dx7/08-FM`).
- Fix: always top right of the card (title row).

**I11-15. Low: the LFO pool still calls the same thing "OUT 2" (LFO 2 chip), "2 outputs" (caption), "OUT 1 AXIS" and "LFO 1 B" in the chip list (inferred from `ModNames.h`).**
- What: `neuro/lfo-sim-lorenz`: chips "LFO 1 OUT 2", "LFO 2 OUT 2". Readable; four words for one idea across the page.
- Fix: one: "OUT 2" everywhere, "second output" in tooltips.

**I11-16. Low: the PLAY OSC strip title for an operator reads "OSC 2 / MOD > 1" and for wavetables "OSC 2 / OUT, MOD > 1".**
- What: `dx7/01-MAIN`, `neuro/01-MAIN`: the second line is a role line, correct, but the ">" glyph is a greater-than sign where the rest of the app uses "›" or "→".
- Fix: "MODULATES OSC 1" or "→ OSC 1" (the matrix uses "→").

**I11-17. Low: "NOISE FM" has its own row in the FM matrix with a COLOUR knob inside the label column.**
- What: `neuro/08-FM`, `dx7/08-FM` (the bottom row in `neuro/08-FM`). The row is a source without a switch (matrix rows 1-6 have an OUT switch). One row, different left cell.
- Fix: switch for the NOISE source too (gates nothing, but makes the row the same), or a caption.

---

## What stands between this build and 9.5

The shortest list. These are all small; none needs new parameters.

1. **Make PLAY's shape stable (V11-1, V11-2).** PATCH either always or never; strips at 150 px.
2. **One width rule for FX and SEQ (V11-4, V11-5, V11-6).** Cards sized by control count, graphs capped, rows left-aligned on one grid, AW cards get a header combo.
3. **Finish the DX7 level (I11-1, V11-7, I11-2/3).** One level on every page, the OSC page's picture is the envelope, one picker style.
4. **Fix the half-landed fixes (V11-8, V11-9, V11-15, V11-16, V11-26).** The matrix hint, the remap dock, the empty PHYSICAL page, ARP lane state, the Save dialog's typing rule.
5. **One header for every card (I11-6, I11-14, S11-4).** Title, caption, link, switch, in the same order; the OSC drawer's switch inside the tab.
6. **A real card-interior test (V11-3)** so the next round starts clean.

With those done I would score 9.2 against Vital, 9.1 against Serum 2, 9.3 for integration. The last 0.3 is craft you cannot list: hover and drag feedback, animation timing, and audio-preview behaviour, which I could not judge from stills.
