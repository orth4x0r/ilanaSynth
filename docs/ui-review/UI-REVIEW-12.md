# UI review 12 (pass 7): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `3928212`. Evidence: `/home/user/shots12/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (cited as `set/file`). I read about 45 of those shots at full size, and the source in `src/gui/` and `src/PluginEditor.cpp` to confirm causes (file:line given where I have it). I could not click or drag, so live behaviour is marked *(inferred)*. The snapshot tool edits the patch on the OSC page, so "EDITED", the physical-to-wavetable flip after 03-OSC (the dead PHYSICAL page in `neuro/04-PHYSICAL` and `dx7/04-PHYSICAL`) and the "+13" chip are tool artefacts; I did not count them. I rendered no new shots: the nine sets cover every page at 100 % and 75 %.

`ilanaSnapshot --uitest`: `UI TESTS PASSED (0 failures)`.

## Verdict and scores

Review 11's fix round landed well. The three loud problems of the last pass are mostly gone: PLAY now always ends in a PATCH tile on the three-oscillator patches, the FX cards have a natural width each and wrap, the DX7 OSC page shows the envelope big and one level up front, the matrix hint no longer fights its filter, the PHYSICAL empty state is a card instead of a dim wall, the filter signal flow says PHYSICAL and fits when busy, FM operator pickers are the shared picker, and the macro "+" tile says "+ MACRO". On most pages a screenshot could be a shipping product.

What is left is the same three kinds of small, each one smaller than last time, plus one new kind:

1. **Rules that hold on the screenshot patches but not at the edges.** PATCH is "always" only while the left column fits. With five or six operators it sits behind a scroll bar, and with five oscillators the column leaves a 170 px hole and still hides it (`init/added-osc-MAIN`, `dx7/01-MAIN`). SUB + NOISE folds nowhere, while an off oscillator folds everywhere. The "OSC 4 (not added)" combo item breaks a convention the build wrote down.
2. **Dead space inside boxes that are the right size.** SEQ's control rows sit in the left 40 % of a 1,500 px card. The FM matrix has a 100 px blank band above its first control and 200 px below its last. Every OSC-page inner panel keeps 70 px of empty floor. The FX page leaves 800 x 270 px beside a lone third card.
3. **Half-landed fixes again.** "Voice level" is demoted to ADVANCED but is still a second level beside OUTPUT. The remap tiles still cut "EASE O...". The 3D hint still drops half. The PLAY strips' "OUT" caption is still a lone word.
4. **New: the surface is quiet, but the vocabulary is not yet one.** The same engines are drawn with a dot, a number, a switch, a pill or a word on different pages (the SEQ chain's order is not the tab order; the BODY switch sits left, the SOUNDBOARD switch right; "AIRWINDOWS" is a button on some rows and a tag on others).

| Angle | Review 10 | Review 11 | Review 12 | Target |
|---|---|---|---|---|
| Against Vital | 8.4 | 8.7 | **8.9** | 9.5 |
| Against Serum 2 | 8.3 | 8.6 | **8.8** | 9.5 |
| Integration | 8.7 | 8.9 | **9.1** | 9.5 |

**Did the review 11 fixes land well? (the brief's list)**

- *PLAY always ends in a PATCH tile* (`MainPage.h:479-489, 1317`): **landed on three-oscillator patches**, with the tile 130 to 240 px. It does not survive a six-operator voice (the column scrolls, `dx7/01-MAIN`) or an Init with five oscillators (170 px hole, `init/added-osc-MAIN`) (V12-1). The tile also reads as three different things for a DX7 voice (hidden), so the rule is really "when it fits".
- *WARP / DETUNE back on the wavetable strips*: **landed** (`neuro/01-MAIN`: SEMI, LEVEL, FRAME, WARP, UNI, DET and the WARP-mode combo). With five strips the third combo drops (`init/added-osc-MAIN`: Wavetable and Basic only), so a strip's contents depend on the count of strips (V12-14).
- *One natural width per FX type, wrapping rows*: **landed in the card** (VOWEL and OTT the same half width, `neuro/10-FX`). The rows now end ragged: DRIVE stops at x = 1468 of 1548, and card 3 sits alone with an 800 x 270 px hole beside it (V12-4).
- *SEQ rows on one grid*: **landed** (RATE / STEPS / GATE on the same x in ARP and PROB SEQ, `neuro/09-ARP-SEQ`, `init/gen-probseq`). The row is still 1,000 px of nothing on the right (V12-5).
- *Tighter card-interior test*: **not verified** (the test passes; I could not read its limit). The shots show it did not catch the FM matrix's blank band, the SEQ row or the OSC panels' floor (V12-3).
- *One level, no big sine on DX7 OSC pages*: **half-landed.** The picture is now the OP ENV curve (good, `dx7/03-OSC`). OUTPUT is up front. VOICE LEVEL moved under an "ADVANCED" caption but is still a 50 % knob beside a 0 dB OUTPUT on the same operator (I12-1).
- *Shared picker on FM*: **landed** (`dx7/08-FM`, `neuro/08-FM` use the dot-and-number pills). The PLAY strip's own "›" chevron is gone; each strip has "EDIT ›" now.
- *One card-header order*: **mostly landed** (title, caption, EDIT link, switch on PLAY, FILTER, FX, FM). It breaks on PHYSICAL's BODY (switch left of the word, `keys/04-PHYSICAL` x = 765) and on the OSC page's drawer (switch inside the tab now, fine) (I12-5).
- *VOICE first in the drawer*: **landed** ("VOICE POLY" is the first tab and says its mode). The drawer's tabs STRINGS and SOUNDBOARD still show on a DX7 voice (I12-12).
- *Macro and "+" named*: **landed** ("+ MACRO", `neuro/01-MAIN`). On Init four macro names read "MACRO 1..4" and only the first shows "+ ASSIGN" (a good fix, `init/01-MAIN`).

Nothing the fix round did made a page worse. The two regressions I found are small: the remap dock's last visible row is still cut at the bottom (`remap/remap-editor`, V12-17) and the 3D hint (V12-16) is unchanged.

---

# Part 1: Against Vital

Vital's strengths I measure against: every panel full and balanced, one control style, one text size per role, the modulation drag always landing where it says, and no state that appears only sometimes.

## Fixed since review 11 / still open (V11-1 to V11-30)

| # | Review 11 item | Status |
|---|---|---|
| V11-1 | PATCH tile state-dependent | **Mostly fixed**: always present on 1-3 oscillators; hidden behind the scroll bar with 5-6 operators or oscillators (V12-1) |
| V11-2 | PLAY strips 40 % empty inside | **Half fixed**: strips cap at 180 px and the PATCH tile takes the rest; the title column under "OSC 1" is still blank and the control row still hangs from the top on Init (V12-2) |
| V11-3 | No card-interior test evidence | **Still open** (V12-3) |
| V11-4 | FX cards stretch | **Fixed** (natural widths); the ragged right edge and the hole remain (V12-4) |
| V11-5 | SEQ rows empty, off-grid | **Half fixed**: one grid, but the row is left-heavy (V12-5) |
| V11-6 | AW cards have an empty graph half | *Not rendered in this set; `neuro/10-FX` DRIVE shows BUILT-IN / AIRWINDOWS as a header segment, which is the fix I suggested* |
| V11-7 | DX7 OSC picture is a sine | **Fixed** (OP ENV curve) |
| V11-8 | Matrix hint cut | **Fixed** (header "12 of 64 routes", filter right) |
| V11-9 | Remap dock text and tile names | **Half fixed**: spacing is clear now; "EASE O..." and "Dead" tile names are cut or abbreviated (V12-17) |
| V11-10 | Output meter small, grip crosses it | **Still open** (V12-9) |
| V11-11 | Filter signal flow | **Fixed**: PHYSICAL node, fits when busy (`neuro/filter-flow-busy`); BYPASS label still crosses a line (V12-23) |
| V11-12 | FM matrix cells, "+3" tiles | **Half fixed**: "+3" now reads "+1 OSC" on the tiles that need one; the big cells and the blank bands remain (V12-6) |
| V11-13 | "RESPONSE" drawn as a tab | **Fixed** (plain caption) |
| V11-14 | OSC page SUB + NOISE keeps controls when off | **Moved**: SUB + NOISE is now a drawer tab with a dot (fine); on PLAY it still never folds (V12-7) |
| V11-15 | PHYSICAL empty page | **Fixed** (a card, a themed button); the unused 270 px below the card is the page (V12-24) |
| V11-16 | ARP lanes full strength while off | **Fixed** (faint cells, `neuro/09-ARP-SEQ`) |
| V11-17 | FRAME caption disagrees | **Fixed**: "frame 6 of 64, playing" at 75 %; still two numbers, but the caption is honest (V12-28) |
| V11-18 | Lone "›" on PLAY strips | **Fixed** ("EDIT ›") |
| V11-19 | F1/F2 and EDIT pills one style | **Fixed** (EDIT is text only) |
| V11-20 | Type below 10 px | **Still open** (V12-18) |
| V11-21 | BPM / CPU grey fragments | **Still open** (V12-10) |
| V11-22 | Header scope squiggle reads as underline | **Still open** (V12-11) |
| V11-23 | Rings touch the FRAME label | **Still open** (V12-12) |
| V11-24 | VECTOR pad blank when off | **Half fixed**: switch beside the title, "OSC 3: off" / "OSC 4: none" in the corners; pad is still a 720 px dark square with one dot (V12-19) |
| V11-25 | "OSC 4 (not added)" in the corner combo | **Still open** and against the conventions (V12-8) |
| V11-26 | Save dialog typing rule | **Still open** (V12-20) |
| V11-27 | Tour shortcuts line | **Fixed enough** (a plain row; the card is 1,080 px wide, readable) |
| V11-28 | 75 % text at the floor | **Held**: `small/*` shows no cut text; uitest passes |
| V11-29 | FX header buttons three styles | **Still open** (V12-21) |
| V11-30 | "ADD OSC" two sizes | **Still open**, trivial (V12-30) |

## Findings (Vital), worst first

**V12-1. High: PATCH is "always" only when the left column fits; at five or six operators it is behind a scroll bar, and at five oscillators the column leaves a hole and hides it.**
- What: `init/added-osc-MAIN` (OSC 1, 4, 5 on, plus SUB + NOISE and the add row) shows a scroll bar at x = 880 and 170 px of empty floor under SUB + NOISE, and no PATCH tile. `dx7/01-MAIN` and `dx7-small/01-MAIN` have six strips and the PATCH tile is not in view; the thumb says there is content below. Cause: `MainPage.h:479-489` computes `patchMinHeight = 130`; when the floors do not fit, the column scrolls and `lastWholeBottom` stops at the last card that fits whole (`:497-503`), so a card that does not fit whole is hidden, not clipped. The 170 px is the gap between SUB + NOISE and where PATCH would start. (The snapshot may have caught `added-osc` mid-fold; I read the layout rule from the source and it gives the same result *(inferred for the exact numbers)*.)
- Vital: a panel is in the layout or it is not; a scroll bar on the main page is never needed.
- Fix: let PATCH take the height and shrink to a 90 px strip (nodes only, no WEST/BODY row) before it is ever hidden; and fold SUB + NOISE (V12-7) so five operators fit.
- Evidence: `init/added-osc-MAIN`, `dx7/01-MAIN`.

**V12-2. Medium-high: PLAY oscillator strips on Init and Keys are 190 px with the controls hung from the top; the title column under "OSC 1" is empty.**
- What: `init/01-MAIN`: OSC 1's controls end at y = 290 and the card ends at 338, 50 px of empty floor; SUB + NOISE the same (`init/01-MAIN` y = 690-735). The 170 px under "OSC 1" is blank (Neuro has an "OUT" line there, Init and Keys have nothing). `keys/01-MAIN`: OSC 1 is 190 px for four controls. `maxGrownSlotHeight = 180` (`MainPage.h:1317`) is a cap, but a card whose controls need 110 px is still given 180.
- Vital: panels are as tall as their content; spare height goes to the graphs.
- Fix: strips 140 px for any engine whose controls fit in 110; vertically centre the control block; give the leftover to PATCH (up to 300 px, a bigger and more readable tile) or to the ENVELOPE / LFO column on the right.
- Evidence: `init/01-MAIN`, `keys/01-MAIN`.

**V12-3. Medium-high: the card-interior emptiness test passes on blanks it should catch.**
- What: current blank rectangles inside cards: the FM matrix card's band above FM MODE (about 400 x 100 px) and the 200 px below EXTRAS (`neuro/08-FM`); SEQ's PROB SEQ control row (1,000 x 130 px, `init/gen-probseq`); the OSC page's inner panel floor (1,040 x 70 px, `neuro/03-OSC`, `keys/03-OSC`); the VOICE drawer's right 600 px; PHYSICAL's BODY box when off (400 x 150 px, `keys/04-PHYSICAL`); the DX7 ENVELOPE card on PLAY (the sentence fills a third of the card, `dx7/01-MAIN`). `--uitest` is green on all of these.
- Fix: print the largest content-free rectangle per card in the test output and fail at 20 % of the card area (a hard limit, not 25 %); list failures that are allowed by name.

**V12-4. Medium-high: the FX rack has a ragged right edge and a lone card leaves a hole.**
- What: `neuro/10-FX`: VOWEL (half), DRIVE (three knobs, stops at x = 1468), then OTT alone on the next row with 800 x 270 px of nothing right of it. At 75 % (`small/10-FX`) the same. Serum's rack and Vital's both end flush at the right edge. Cause: `FxPage.h:1077-1235` (`naturalWidth`, rows break when the next card would not fit; the last card of a row is not stretched, on purpose after review 11).
- Fix: fill the hole with the next useful thing: an "+ ADD EFFECT" tile sized as a card (it is a 1,500 px dashed bar today) so the grid reads as a grid; or distribute the row slack equally as the gap between cards.

**V12-5. Medium: SEQ control rows are on one grid now but the row is left-heavy and every engine's row is different.**
- What: ARP: RATE, STEPS, GATE, MODE, OCTAVES, CHANCE across x = 66 to 1000 and nothing from 1000 to 1530 (`neuro/09-ARP-SEQ`). PROB SEQ: RATE, STEPS, GATE then 1,000 px of nothing (`init/gen-probseq`). CLIP: ten items across the whole width (`init/gen-clip-expanded`). Three rows, three densities.
- Fix: fix the control row to six slots of 130 px at the left and put the engine's one extra (CHANCE, SWING, or the clip's buttons) in the right half; or move the row beside the lane editor as a column, so the lanes use the full width and the card closes at 560 px instead of 710.

**V12-6. Medium: the FM matrix card has dead bands above and below its controls.**
- What: `neuro/08-FM`: header "FM MATRIX · rows modulate columns", then 100 px of nothing, then FM MODE; 3 x 3 cells; EXTRAS button; then 200 px of nothing to the card foot. The DX7 card (`dx7/08-FM`) is dense and right, so this is the small-patch case only. `FmInputPages.h:807` is the hint text; the vertical layout centres the grid in the card.
- Fix: top-align the block under the header; use the freed height for a one-line read-out ("OSC 2 modulates OSC 1 at 30 %") or the algorithm names strip, or make the cells square at 110 px.

**V12-7. Medium: SUB + NOISE never folds on PLAY, though an off oscillator does.**
- What: `keys/01-MAIN`: SUB + NOISE off, 190 px tall, dimmed knobs; OSC 2 and OSC 3 off fold to 60 px with their switch. `dx7/01-MAIN`: SUB + NOISE off at 100 px. `MainPage.h:479` counts SUB + NOISE as a flexible card always (`flexible = shown - folded + 1`), `isFolded` (`:757`) only covers oscillator strips. This is V10-9 and V11-14, still half done.
- Fix: fold SUB + NOISE when its switch is off, the same animation and height as an oscillator.

**V12-8. Medium: the VECTOR corner combo still offers "OSC 4 (not added)", against the written convention.**
- What: `neuro/02-VECTOR`: "OSC 4 (not added)"; `FilterVectorPhysicalPages.h:482`. `UI-CONVENTIONS.md` says "A missing or switched-off part in a label reads 'OSC 4: none', 'OSC 2: off', not in parentheses". The pad's own corners say "OSC 3: off" / "OSC 4: none" (correct). The same screen uses both forms.
- Fix: list only added oscillators; or write "OSC 4: none" and keep it selectable as the silent corner.

**V12-9. Medium: the OUT meter is still the smallest instrument on the screen and the grip crosses it.**
- What: `neuro/01-MAIN` bottom right: a 150 x 24 px bar, "OUT - dB", the grip lines over its right end. The L/R meter exists in SCOPE only (`neuro/scope-panel`, with clip hold and peak numbers). `PluginEditor.cpp:431` creates it; `OutputMeter.h:37-43` draws "OUT" and the clip light on the first line.
- Fix: 110 px stereo bars left of the grip with the numeric peak on hover; or the same meter in the header beside CPU.

**V12-10. Medium-low: BPM, VOICES and CPU are still three unequal things on two baselines.**
- What: `neuro/01-MAIN`: "120.0 BPM" (grey, right-aligned at 366), VOICES button (rimmed combo), "CPU 3 %" (a different grey and size). `PluginEditor.cpp:1245-1285`: the three are drawn with separate rects at `statusY = 40`, 14 px tall, BPM and CPU as text, VOICES as a button.
- Fix: one status row: BPM and CPU as 24 px read-outs with the same rim as VOICES, or fold BPM and CPU into one text and keep VOICES a button.

**V12-11. Low-medium: the header's scope squiggle still reads as the preset box's underline.**
- What: `neuro/01-MAIN`, `init/01-MAIN`: a red line at the foot of the preset box. On Init it is a flat line that looks like a border bug. (S10-14, V11-22.)
- Fix: remove it (SCOPE exists) or give it its own 22 px band with a baseline and the label "OUT".

**V12-12. Low-medium: modulation rings crowd the value text on the FRAME knob.**
- What: `neuro/01-MAIN`: the three FRAME rings run to the knob's label; `init/added-osc-MAIN` the ring sits on the value "50%". `ParamControls.h` draws ring and label from one rect.
- Fix: four more pixels between the arc and the text on ring-drawing knobs; rings drawn thinner when three or more sources are on one knob.

**V12-13. Low-medium: the ENVELOPE card on a DX7 voice is a graph and a paragraph.**
- What: `dx7/01-MAIN`: the right two-thirds says "6 oscillators play the Operator Env, each operator its own. It shapes their levels; AMP ENV is unused." in 11 px grey at the top, 150 px of blank below. The card has four tabs (OP ENV, AMP ENV, FILT ENV, +1) and none of them edits anything on this screen.
- Fix: show the six operator levels as six small bars beside the graph (the data is there), and put the sentence in the card's tooltip.

**V12-14. Low-medium: a PLAY strip's content depends on how many strips there are.**
- What: with three strips OSC 1 has three combos (Wavetable, table, WARP mode, `neuro/01-MAIN`); with five it has two (`init/added-osc-MAIN`). Compact is reasonable; the WARP-mode choice just vanishing is not signalled.
- Fix: a "WARP" line in the strip's tooltip; or keep the warp-mode combo in the compact row by dropping the table name (it is already in the thumbnail's tooltip).

**V12-15. Low-medium: "COLOUR" on SUB + NOISE reads as disabled while the card is on.**
- What: `neuro/01-MAIN`, `init/01-MAIN`: COLOUR is dim at 100 % under NOISE 0 %, while SUB and NOISE are bright. The dimming is right (colour does nothing at 0 % noise) but nothing says why.
- Fix: a tooltip that says "Noise colour. Needs NOISE above 0."; the Vital way is to show it live.

**V12-16. Low-medium: the 3D view still drops "up / down: warp".**
- What: `neuro/03-OSC`: "drag across: frame" at the foot; `WaveDisplay.h:1459-1461` builds the full text only when a warp is chosen (`canDragWarp()`), and with Warp Off the second half is withheld. Reasonable but a user cannot find the gesture. (S11-6 asked for both gestures always.)
- Fix: always "drag across: frame · up / down: warp (choose a warp first)" in the 3D view.

**V12-17. Low-medium: remap tiles still truncate, and the matrix beneath the dock is cut mid-row.**
- What: `remap/remap-editor`: "EASE O..." (the short names are "Ease out", `RemapEditor.h:101`, drawn upper-case at 9 px in a 64 px tile); the row under row 7 (y = 575) is cut half way. The extra text "Click to add a point, drag the middle dots to bend, double-click to remove." is fine.
- Fix: "EASE OUT" at 8 tiles of 72 px, or the names "IN" and "OUT" with a caption; clip the list at a whole row.

**V12-18. Low: informative text still at the 9 px floor.**
- What: tile labels in the remap dock, "KEY UP", "100 ms" ticks, "CLIP" in the meter, "OUT" in the bottom bar, the "FM FROM OSC 2 30 % ›" link on the OSC page (about 10 px, `neuro/03-OSC`). The uitest floor is met, so these are legal; they are still the smallest type on a page where 14 px has room.
- Fix: 10.5 px for anything that is a link or a value; ticks may stay.

**V12-19. Low: the VECTOR pad is a 720 px square with one dot when off.**
- What: `neuro/02-VECTOR`: the corner text is there, the pad is dark, with the switch beside the title now. A pad with the four corner waveforms drawn dim would show what the switch gives.
- Fix: draw each corner's waveform thumbnail dim in its corner.

**V12-20. Low: SAVE AS still warns after typing, and TAGS is typed and clicked.**
- What: `extras/extra-save-as`: orange "':' and '/' will be left out of the file name" under NAME; the TAGS field repeats the chips. (V9-30, V10-11, V11-26.)
- Fix: strip the characters as typed and drop the warning; make the TAGS field read-only.

**V12-21. Low: the FX header buttons are three styles, and AIRWINDOWS is a button on some rows and a tag on others.**
- What: `neuro/fx-empty`: CHAIN 1 (filled pill), CHAIN 2 (outlined pill), COPY TO 2 (flat button), FX dice (button with icon), SAVE / LOAD CHAIN (flat button). In the add menu, REVERB | AIRWINDOWS is two buttons, while TAPE and CONSOLE show "AIRWINDOWS" as a dim tag in the row (they have no built-in version).
- Fix: tabs for CHAIN 1/2 only; a "CHAIN ▾" menu for COPY TO 2, SAVE, LOAD; the dice at the right. In the menu draw the Airwindows-only rows with the same pill, in a different colour, or put "AW ONLY" as a sub-line.

**V12-22. Low: the "+13" modulation-chip overflow shows eight coloured dots and no names.**
- What: `neuro/07-MATRIX`, `neuro/10-FX`, `neuro/scope-panel`: a "+13" box with a dot string between FILT 2 ENV and VELOCITY, a chip of its own; on patches where the tool had not added routes it does not appear. (The brief says this chip is the tool's, so I note only that its dot row is 7 px.) *(inferred, tool artefact)*
- Fix: none required for real patches; if it shows, show its dots at 9 px.

**V12-23. Low: the signal flow's BYPASS label crosses a connector in parallel mode.**
- What: `neuro/filter-flow-busy`: "BYPASS" sits on the line from OSC 1 to WEST, and "PARALLEL" overlaps the gap between F1 and F2.
- Fix: label chips get a 6 px opaque ground.

**V12-24. Low: the PHYSICAL empty-state page is a card and 270 px of floor.**
- What: `neuro/04-PHYSICAL`, `dx7/04-PHYSICAL`: the card is right; the page is half empty. A user on a DX7 voice is offered "SWITCH TO PHYSICAL", which replaces operator 1's FM role with a string (`FilterVectorPhysicalPages.h:542`). *(inferred: the button flips OSC 1's mode.)*
- Fix: on an Operator oscillator say "OSC 1 is an FM operator. Pick another oscillator for a string." and offer the pills only for non-operator oscillators; add a confirm when the button changes an operator.

**V12-25. Low: LFO chips mix three label styles in one bar.**
- What: `neuro/06-ENV-LFO`: "LFO 1  FILTER 1 › Cutoff +2 more" as a rim-less pill, "LFO 2  OUT 2" as a blue rim, "LFO 3 4.0 Hz" as plain text. The routing pill is now a good idea (S11-11 fixed); the three tokens still differ in weight and colour.
- Fix: one slot per token: name, tag (OUT 2), summary, rate right-aligned; same weight.

**V12-26. Low: Filter 2's card has no visible switch and goes dim by value.**
- What: `neuro/05-FILTER`: FILTER 2 header has a grey dot at the left and no switch at the right where WEST and BODY have theirs; the controls dim when the cutoff is at 20 kHz. *(inferred: the dot is the switch.)* The convention says "a card's own module: switch in the header, at the right".
- Fix: a header switch at the right on FILTER 2 (the dot may stay as the on-dot).

**V12-27. Low: the filter card's response caption is not a sentence and not a fragment.**
- What: "drag across for cutoff, up and down for resonance" (`EnvLfoPages.h:65`) sits in the header caption slot; the conventions say captions are noun fragments and hints are full sentences with a full stop.
- Fix: "Drag across for cutoff, up and down for resonance." in the hint style at the foot of the graph.

**V12-28. Low: two numbers still show for FRAME when the knob and the playing frame differ.**
- What: `small/03-OSC`: "frame 6 of 64, playing" under a knob at 30 %; honest, but it appears twice on the page with a ring on the knob that already shows a moving frame.
- Fix: "frame 6 of 64" and the word "playing" only when modulated.

**V12-29. Low: the tutorial says "Ctrl+1-7 switch tabs" and "Ctrl+Shift+1-3 switch pages"; the settings menu is not mentioned.**
- What: `neuro/00-tutorial`. Fine content; the shortcut row is 11 px grey with no key styling.
- Fix: key-cap styling, 12 px.

**V12-30. Low: "+ ADD OSC 4" is a 128 px dashed pill on OSC and a full-width dashed row on PLAY.**
- What: `neuro/03-OSC` vs `neuro/01-MAIN`. Trivial; one is a picker, one a list item.
- Fix: leave it, or name both "+ OSC 4".

---

# Part 2: Against Serum 2

Serum 2's strengths: the oscillator page as the main event, a one-line-per-route matrix, drag-to-anywhere modulation, a browser that is a destination, and an FX rack with graphs and a clear order.

## Workflow speed (the same five jobs)

| Job | Serum 2 | ilanaSynth (this build) |
|---|---|---|
| Change polyphony | OSC/Global tab | Header VOICES opens OSC > VOICE, the first drawer tab, headed "VOICE POLY" (**one click**, finally obvious) |
| Assign an LFO to a knob | Drag from the LFO | Drag a chip onto any knob (**as fast**) |
| Edit a modulation depth | Ring on the knob | Same; matrix filter for the 12th row (**faster**) |
| Audition and pick a preset | Browser tab, one click | Pop-up or docked browser, details card, RECENT, HEAR (**as fast**; sort is name only, S12-6) |
| Load a sample and map it | Drop on the oscillator | LOAD... on the sample page; no drop hint (S12-7) |
| Reorder an effect | Drag, with an insertion line | Drag a card's header; a new effect inserts at the end (**as fast**, the insertion line is *(inferred)*) |

## Fixed since review 11 / still open (S11-1 to S11-18)

| # | Review 11 item | Status |
|---|---|---|
| S11-1 | VOICE hard to find | **Fixed** (first tab, mode in the tab) |
| S11-2 | OSC page columns not aligned | **Mostly fixed**: LEVEL, PAN, SEMI, FINE and the rows below share x = 727, 874, 1169, 1317 (`neuro/03-OSC`); the combos (WARP, SPECTRAL, TUNING, ENVELOPE, UNI MODE, CHORD) still sit in their own cells (S12-1) |
| S11-3 | OSC inner panel blank right third | **Half fixed**: the panel ends at 1,540, its floor is 70 px empty (S12-1) |
| S11-4 | Drawer switch per tab | **Fixed** (dots on tabs, no far-right switch) |
| S11-5 | SEQ three screens in one | **Half fixed**: RATE / STEPS / GATE on one grid; the clip bar is one row of ten (S12-3) |
| S11-6 | 3D hint | **Still open** (V12-16) |
| S11-7 | Table browser tabs | **Fixed** (category chips, USER; LOAD in the browser *(inferred from the chip row)*) |
| S11-8 | Preset browser sort | **Half fixed**: RECENT 3 is there; sort is still "NAME" (S12-6) |
| S11-9 | Macros "ASSIGN" x4, "+" unlabelled | **Fixed** ("+ MACRO"; only the first shows "+ ASSIGN") |
| S11-10 | SAMPLE page text row | **Half fixed**: the zone map has root dots (`neuro/osc-sample-sfz` at C2 and C4); no drop hint (S12-7) |
| S11-11 | LFO chips cut | **Fixed** |
| S11-12 | AW / AIRWINDOWS | **Fixed** (spelled out; legend line) |
| S11-13 | ENV 5, MOD ENV | **Fixed** ("ENV 4", "9 MORE ▾") |
| S11-14 | FX output row | **Fixed** ("SOFT CLIP [switch]") |
| S11-15 | Browser hint position | **Fixed** |
| S11-16 | Tour card width | **Fixed enough** (1,080 px) |
| S11-17 | "Free"/"Auto" in value slot | **Still open** (S12-5) |
| S11-18 | Macros 5-8 | **Fixed** (the "+" says "Add a macro", *(inferred tooltip)*) |

## Findings (Serum 2), worst first

**S12-1. Medium-high: the OSC page's three rows are on one value grid; the combos are not, and the inner panel keeps a 70 px empty floor.**
- What: `neuro/03-OSC`: FRAME at x = 727, WARP combo at 806-1090, SPECTRAL at 1105-1385; PITCH & LEVEL: LEVEL 727, PAN 874, TUNING combo 960, SEMI 1169, FINE 1317, ENVELOPE 1400; UNISON: UNI MODE 660-795, UNISON 874, DETUNE 1021, BLEND 1169, SPREAD 1317, CHORD 1400. The knobs now align down the page (good). The combos have six different widths and three different left edges. The panel ends at y = 718, content at 685.
- Serum 2: knobs and combos in the same column grid; the picture owns the page's left third.
- Fix: every combo is one 140 px cell in column 2, 4 or 6; the panel closes 12 px under its last label.

**S12-2. Medium: the OSC page's left picture still has two jobs and a hidden gesture.**
- What: `neuro/03-OSC`: the 3D view is a good picture, with WAVE / 3D / SPEC at the top; the "drag across: frame" hint at the foot. The frame list (the table's own name, "Neuro") has prev/next arrows. The page has no frame scrubber, and a user must find the gesture on the picture.
- Serum 2: a frame slider under the view.
- Fix: a thin frame scrubber (64 ticks) under the picture, bound to FRAME, which also shows modulation.

**S12-3. Medium: the clip editor's bottom bar is one row of ten items.**
- What: `init/gen-clip-expanded`: SLOT, MODE, LENGTH, GRID, ZOOM (− FIT +), DRAW, QUANTISE, IMPORT MIDI, EXPORT MIDI, COLLAPSE in a single row. COLLAPSE has the same orange rim as DRAW, so it reads as an active mode.
- Fix: two groups with a gap (what plays: SLOT, MODE, LENGTH | how you edit: GRID, ZOOM, DRAW, QUANTISE | files: IMPORT, EXPORT); COLLAPSE as the "›" at the card's title.

**S12-4. Medium: the SEQ chain's order is not the tab order.**
- What: `init/gen-probseq`: the chain reads "KEYS › STRUM › PROB SEQ › EUCLID › VOICES" (signal order); the tabs read ARP, EUCLID, PROB SEQ, CLIP (a different list, with the ARP missing from the chain because it is off). A word in the chain does nothing when clicked *(inferred)*.
- Fix: draw the tab list in the chain's order; make each chain word select its tab.

**S12-5. Medium-low: "Auto" and "Random" still sit in value slots.**
- What: `keys/04-PHYSICAL` (EXCITE POS "Auto"), `neuro/lfo-sim-lorenz` (SEED "Random"). They read like a label that lost its number.
- Fix: the value colour of a menu entry (lighter, no unit), or a tiny "A" badge.

**S12-6. Medium-low: the preset browser sorts by name only and its rows carry four pills each.**
- What: `neuro/preset-browser`: "SORT: NAME ▾"; each row has three tag pills and a category pill, so the list is a wall of rims. Serum's list is names, a star, a category dot.
- Fix: tag pills only on the hovered or selected row; sort by newest, category, rating; keep the category pill (it is the colour code).

**S12-7. Medium-low: the sample page has no "drop here" text and no velocity map.**
- What: `neuro/osc-sample-sfz`: zone map with root dots, "test · 2 zones · 1-shot", LOAD...; the empty sample page says nothing about drag-and-drop (inferred from `osc-sample-empty`, not re-read). SFZ velocity layers are not drawn.
- Fix: "Drop a .wav or .sfz here" in the empty state; draw velocity as bar height inside the zones.

**S12-8. Low-medium: the OSC page's "FM FROM OSC 2 30 % ›" is a 10 px grey link in the card's title row.**
- What: `neuro/03-OSC`. It is the only place on the OSC page that says an oscillator is FM-modulated, and it is hard to see.
- Fix: a coloured pill "FM 30 % ‹OSC 2›" at the card's right, same colour as OSC 2.

**S12-9. Low-medium: the matrix has no group headings or alternating tint; the colour bar at the row's left is the only grouping.**
- What: `neuro/07-MATRIX`: twelve rows sorted by source; colours of sources help; there is no sub-heading ("LFO 1: 3 routes"). At 40 routes this is a wall.
- Fix: sort-by-source option with a thin group heading; or collapse by source with a count.

**S12-10. Low-medium: the macro strip's names are macro names, not what the macro does.**
- What: `neuro/01-MAIN`: TONE, TALK, TEAR, SUB with 0 % each; no route indicator. A hover popup exists (`ModHoverPopup.h`) *(inferred)*.
- Fix: show "→ 3" (route count) as a small suffix when assigned.

**S12-11. Low: the LFO page's DRIVES list repeats the matrix.**
- What: `neuro/06-ENV-LFO`: "FILTER 1 › Cutoff +70%" and so on. A useful read; its rows are not clickable *(inferred)*.
- Fix: click a row to jump to the matrix filtered to it.

**S12-12. Low: the table browser's HEAR is its only audio preview and sits far from the tile.**
- What: `neuro/table-browser`: the HEAR button at top right, the selected tile at the left. Serum previews on click.
- Fix: preview on selection with a small speaker toggle.

**S12-13. Low: the DX7 PLAY card labels each operator "OPERATOR" over a combo reading "Sine".**
- What: `dx7/01-MAIN`: six times "OPERATOR / Sine". The word describes the card and the combo describes the wave.
- Fix: "WAVE" over the combo, "OPERATOR" once in the title.

**S12-14. Low: the FX "SOLO" button is a bordered grey box beside the orange switch.**
- What: `neuro/10-FX`. Two controls with similar weight in the card header; SOLO is rarely used.
- Fix: a small "S" toggle left of the switch.

**S12-15. Low: the empty FX page's help sentence is 13 px and two lines.**
- What: `neuro/fx-empty`: "Pick an effect to start the chain..." fine; the legend line "AIRWINDOWS beside an effect is its Airwindows version; a card switches between the two." is 9 px grey.
- Fix: keep, at 11 px.

---

# Part 3: Integration (DX7/FM, physical, sample, Airwindows and the rest)

The question: does each newer engine look like the rest, or does it have its own corner, vocabulary or widgets?

## Fixed since review 11 / still open (I11-1 to I11-17)

| # | Review 11 item | Status |
|---|---|---|
| I11-1 | DX7 operator: two levels on OSC | **Half fixed**: OUTPUT up front, VOICE LEVEL under ADVANCED (I12-1) |
| I11-2 | Four doors to the same operator | **Mostly fixed**: PLAY strip pills, OSC pills ("OSC 2 MOD → 1"), FM pills (dot + number), OP ENV pills (dot + number): the same picker (I12-2) |
| I11-3 | FM operator pickers a fourth style | **Fixed** |
| I11-4 | FM depth shown in four places | **Half fixed**: the OSC page link "FM FROM OSC 2 30 % ›" now has the number (S12-8) |
| I11-5 | Ring mod / sync / noise FM two layouts | **Fixed**: one "EXTRAS · RING MOD · SYNC · NOISE FM ›" button on both (`neuro/08-FM`, `dx7/08-FM`) |
| I11-6 | Engine cards differ in structure | **Mostly fixed** (I12-5) |
| I11-7 | Airwindows own vocabulary | **Half fixed**: BUILT-IN / AIRWINDOWS segment in the card header (`neuro/10-FX`); knob names are the plug-in's (I12-7) |
| I11-8 | "STRINGS + SOUNDBOARD" | **Fixed** ("PHYSICAL" node, `neuro/filter-flow-busy`; "SOUNDBOARD" on Keys PATCH) |
| I11-9 | Physical unison unexplained | **Fixed** ("STRING COPIES", `keys/03-OSC`) |
| I11-10 | Four words for output | **Half fixed**: OUT remains on PLAY strips as a role line (I12-3) |
| I11-11 | Sample unison row dead | **Fixed** (UNISON, CHORD only, `neuro/osc-sample-sfz`) |
| I11-12 | FM algorithm names | **Half fixed**: tiles read "1..9 +1 OSC"; the title "CUSTOM, NEAR 2-OP STACK" remains (I12-9) |
| I11-13 | SEQ chain not clickable | **Still open** (S12-4) |
| I11-14 | EDIT links in mixed places | **Half fixed**: PLAY, OSC, FILTER, MOD put them top-right; SOUNDBOARD's is inside the sub-box (I12-5) |
| I11-15 | LFO "OUT 2" four words | **Fixed**: "OUT 2" everywhere (chip, caption, `neuro/lfo-sim-lorenz`) |
| I11-16 | ">" glyph in strip role | **Fixed** ("MOD → 1") |
| I11-17 | NOISE FM matrix row | *Not rendered in these sets* |

## Findings (integration), worst first

**I12-1. Medium-high: a DX7 operator still has two level controls on its own page.**
- What: `dx7/03-OSC`: OUTPUT 0.0 dB in the PITCH & OUTPUT row; VOICE LEVEL 50 % under "ADVANCED" at the foot. PLAY (`dx7/01-MAIN`) and FM (`dx7/08-FM`) show OUTPUT only. `OscPage.h:345` names it "VOICE LEVEL", `:1018` files it under ADVANCED; `ParamInfo.h:473` explains in a tooltip that it scales OUTPUT. A user who turns OUTPUT to -6 dB and VOICE LEVEL to 25 % gets -18 dB with two knobs.
- Benchmark: the DX7 and Dexed have one output level per operator; Serum has one level.
- Fix: for an Operator oscillator, hide VOICE LEVEL on the OSC page and show it only when it is not 50 % (the default), with a "reset" link. The settled decision (level behaviour) is respected, only the surface changes.
- Evidence: `dx7/03-OSC`, `dx7/01-MAIN`.

**I12-2. Medium: the operator pickers are one style, but the DX7 voice still has two kinds of pill row with different widths.**
- What: `dx7/03-OSC`: "OSC 1 OUT", "OSC 2 MOD → 1", "OSC 3 OUT" wide pills with a role; `dx7/08-FM`: "OSC 1" selected and "2 3 4 5 6" as dot + number; `dx7/06-ENV-LFO`: the same as FM. The convention allows the number alone when tight; the same cards on the same patch are tight on one page and not on another.
- Fix: one rule: the selected pill wide, others dot + number, on every page; role tags in the tooltip only.

**I12-3. Medium: the PLAY strip role line is still a word that means a different thing from the knob under it.**
- What: `dx7/01-MAIN`: "OSC 1 / OUT" with an OUTPUT knob; "OSC 2 / MOD → 1" with an OUTPUT of -30.9 dB. A modulator's OUTPUT is its modulation index, a carrier's is volume; both read "OUTPUT". On wavetable strips "OUT" (`neuro/01-MAIN`) is the role line under the title with a LEVEL knob.
- Fix: carriers "TO OUTPUT", modulators "MODULATES 1"; the knob on a modulator reads "DEPTH" (its value is dB below the DX7 scale; the FM page's matrix cell already shows %, so the vocabulary exists).

**I12-4. Medium: engine switches, EDIT links and tabs are close to one grammar but the physical page is not on it.**
- What: PHYSICAL's lower card has no title; STRING and EXCITER are headings inside a box; BODY and SOUNDBOARD are sub-boxes without a rim; BODY's switch sits right of its word (`keys/04-PHYSICAL` x = 765) and SOUNDBOARD's switch at the far right; the SOUNDBOARD sub-box has its MODEL combo at x = 820 and MIX at 1075 on different baselines. No other page in the app draws sub-boxes this way (SEQ's GENERATE sub-boxes have rims and header switches at the right).
- Fix: the same sub-box as SEQ GENERATE (`init/gen-probseq`): a rim, title at the left, switch at the right, controls on one row; BODY and SOUNDBOARD as two boxes of equal width.

**I12-5. Medium: SOUNDBOARD's EDIT link is inside its sub-box; every other EDIT is in a card title.**
- What: `keys/04-PHYSICAL`: "EDIT SOUNDBOARD ›" at x = 1200-1460 on the SOUNDBOARD row, beside the switch. PLAY, OSC and FM put it at the card's title row top right. (I11-14.)
- Fix: with I12-4's sub-box header it falls into the right place.

**I12-6. Medium: the OSC drawer offers STRINGS and SOUNDBOARD on a DX7 voice.**
- What: `dx7/03-OSC`: tabs VOICE POLY, SUB + NOISE, SPREAD & DRIFT, STRINGS, SOUNDBOARD. None of the last two can do anything in an operator patch, and a user cannot tell without clicking. The tab dots are off. A DX7 voice has no place for them; Vital would show fewer tabs.
- Fix: hide STRINGS and SOUNDBOARD unless an oscillator is physical or its strings are already on; the tab bar shrinks to three.

**I12-7. Medium: Airwindows knob names are the plug-in's and nothing on the card says what the algorithm does.**
- What: SOFTEN, HEAD BUMP, FLUTTER, "Pressure 5", "Console 7 Channel" (from review 11; `neuro/10-FX` shows DRIVE with BUILT-IN / AIRWINDOWS as a header segment, no algorithm line). Not re-rendered in the shots I have.
- Fix: a one-line description under the algorithm name from the algorithm table; "(inferred)" if it exists.

**I12-8. Medium-low: "OUT" on the macro strip is a meter caption; "OUT" on the PLAY strip is a role; "OUT 2" on an LFO is an output number.**
- What: `neuro/01-MAIN`: three unrelated uses of the word on one screen.
- Fix: the meter's caption "LEVEL", LFO's "OUT 2" as "2ND OUT".

**I12-9. Medium-low: the FM algorithm title still says "CUSTOM, NEAR 2-OP STACK" and tiles are numbered 1-9.**
- What: `neuro/08-FM`: "ALGORITHMS CUSTOM, NEAR 2-OP STACK" as a caption; the basic set is tiles 1 to 9 with a diagram each; a DX7 voice says "DX7 ALGORITHM 5" (`dx7/08-FM`). Good for DX7 (where numbers are the language); for the basic set the names a user would learn are Stack, Chain, Parallel.
- Fix: tile tooltips with names; caption "2-op stack, edited".

**I12-10. Medium-low: a Physical oscillator on PLAY shows DECAY and DAMP knobs where wavetables show FRAME and WARP, which is right, but the first combo says "Physical" and the second "Piano Hammer".**
- What: `keys/01-MAIN`: "Physical" (engine) over "Piano Hammer" (the exciter); on wavetable strips the two combos are "Wavetable" over the table name, then a third combo "Off" (warp mode). Same slots, different grammar: engine / sub-choice / mode.
- Fix: label the slots in the tooltip: "Engine", "Source", "Warp mode".

**I12-11. Low-medium: the DX7 voice shows a SUB + NOISE card on PLAY, off, at full height.**
- What: `dx7/01-MAIN`: a card that does nothing on a six-operator voice, dimmed, at 100 px with the thumbnail and four controls. (V12-7.)
- Fix: fold, and hide the card on Operator voices unless the sub is on.

**I12-12. Low-medium: on a DX7 voice the PLAY filter shows a flat response with the same markers as a patch with a filter.**
- What: `dx7/01-MAIN`: Low Pass at 20 kHz, 0 % resonance: a flat purple line and two numbered handles at 10k. The DX7 voices bypass the filter by value; the card does not say "filter is open".
- Fix: a quiet "open" word in the graph (the old OPEN badge was removed, V10-10); a text in the graph's empty area is not a badge.

**I12-13. Low-medium: the filter F2 card says nothing when F2 is off by value.**
- What: `neuro/05-FILTER`: F2 is dimmed but shows 20 kHz and 12 dB; it is not clear whether it is on. (V12-26.)
- Fix: header switch.

**I12-14. Low-medium: the PLAY pool "OP LFO" and "OP PITCH" chips look like normal sources but belong to the voice only.**
- What: `dx7/01-MAIN`: OP LFO, LFO 1-3, OP PITCH in the bar. Their rows exist only for Operator voices; on a non-DX7 patch they vanish. A learner cannot tell what OP PITCH is from its name.
- Fix: tooltip: "DX7 pitch envelope (the voice's own)".

**I12-15. Low: the settings, tour and preset text use "patch" and "preset" for the same thing.**
- What: the browser header "PRESETS 641 of 641"; Save dialog "Save preset as"; tour "rolls a fresh patch"; confirm text *(inferred)*; HANDOFF's "patch". Both words are right in a synth manual; the app uses both in the same dialog sequence.
- Fix: "preset" for anything saved or loaded, "patch" for the current sound.

**I12-16. Low: the Vector page title "four oscillators at the corners" and the corner combos say "OSC 4: none" while the tab says OSC 4 not added.**
- What: V12-8; the language is the same inconsistency across the page.

**I12-17. Low: DX7 voices say "6 oscillators" on PLAY and "6 operators" in the same card.**
- What: `dx7/01-MAIN`: the graph caption "6 operators"; the paragraph "6 oscillators play the Operator Env". One noun.
- Fix: "6 operators" both times.

**I12-18. Low: the OSC page is called OSCILLATORS, its tab OSC, its picker "OSC 1"; the DX7 pages call them operators in the FM tab only.**
- What: `dx7/08-FM` "OSC 2 MOD" in the diagram nodes: nodes read "OSC 2 / MOD" and "OSC 1 / OUT". The FM page does not say "operator" anywhere; the PLAY strip says "OPERATOR".
- Fix: keep OSC as the noun (it is the app's) and say "operator" only as the oscillator mode name.

---

## What stands between this build and 9.5

The shortest list. None needs a new parameter.

1. **Make PLAY's left column close its own arithmetic (V12-1, V12-2, V12-7).** Fold SUB + NOISE when off, strips at 140 px, PATCH shrinks to a 90 px strip before it hides. Then five or six operators fit with no scroll bar, and the PATCH tile is really "always".
2. **Make the cards own their dead space (V12-3, V12-4, V12-5, V12-6, S12-1).** Fail the interior test at 20 %, then fix the five cards it will flag: FM matrix, SEQ rows, OSC inner panel, FX hole, PHYSICAL BODY box.
3. **Finish the DX7 level (I12-1, I12-3, I12-6).** One level on the OSC page, "MODULATES 1" and "DEPTH" on modulators, STRINGS and SOUNDBOARD hidden on an operator voice.
4. **Put PHYSICAL on the same grammar as the rest (I12-4, I12-5, S12-4).** Sub-box headers as SEQ's, EDIT link in the title row, SEQ chain in tab order and clickable.
5. **The ten polish items that are still literally the same as review 10 (V12-9, V12-10, V12-11, V12-12, V12-17, V12-20, V12-21, S12-5, S12-6, V12-8).** Output meter, status row, scope line, ring clearance, remap tile names, SAVE AS typing, FX header buttons, "Auto"/"Random" slots, preset row pills, "OSC 4 (not added)".

With those done I would score 9.3 against Vital, 9.2 against Serum 2, 9.4 for integration. The remaining 0.2 to 0.3 is what stills cannot show: hover and drag feedback, animation timing, preview behaviour and the audio itself. Score discipline: I scored 8.9, 8.8 and 9.1 because the loud problems of passes 9 to 11 are closed and what is left is craft, but a commercial product at 9.5 would not have a scroll bar on its main page, a duplicated level knob, or four 1,000 px empty rows. The gain from review 11 to 12 (+0.2 on each) is smaller than the gain from 10 to 11, as it should be: the easy fixes are done.

---

## UI test

`xvfb-run -a ilanaSnapshot --uitest` run once on this build (output kept at `/home/user/shots12/uitest12.txt`, outside the repo). Result line: `UI TESTS PASSED (0 failures)` (899 PASS lines, none FAIL).
