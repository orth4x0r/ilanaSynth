# UI review 10 (pass 5): ilanaSynth v1.3 against Vital, Serum 2, and itself

One reviewer, three angles, one file. Build: branch `claude/project-thread-smvgfk`, `/home/user/ilanaSynth/build`, head `b8a73bb`. Evidence: `/home/user/shots10/{neuro,init,dx7,keys,small,dx7-small,extras,remap,confirm}/` (a shot is cited as `set/file`), plus my own render in `/home/user/shots10/review-extra/t2/` (the T2 set: VOICE tab, PROB SEQ, EUCLID, clip, tour) and a contact sheet of the 41 FX shots (`review-extra/fxsheet1.png`, `fxsheet2.png`). I read the source in `src/gui/` and `src/PluginEditor.cpp` to confirm causes. I could not click or drag, so anything about live behaviour (drag feedback, hover cards, audio) is marked *(inferred)*. The snapshot tool edits the patch on the OSC page, so "EDITED", the physical/wavetable flip after 03-OSC, the "+13" envelope chip in the chip bar and the DX7 algorithm on the Init FM page are tool artefacts; I did not count them.

## Verdict and scores

Review 9's biggest complaints are mostly answered. PLAY now fits six DX7 operators without a scroll, the left column no longer ends in air, the FILTER page folds WEST and BODY, the FM matrix and VECTOR page have boxes, the "Replace your edits?" dialog puts the safe button first, voice settings have a real home, the "add" affordance is one component, the legacy MSEG card and chip are gone, and the SEQ page tells you that an engine is off instead of hiding it. That is real progress, and it shows: the build now reads as one instrument at a glance.

What stands between it and 9.5 has changed character again. It is no longer missing pieces or large dead rectangles. It is **finish at the edges, and a few fixes that were done half-way or in a way the test cannot see**:

1. A visible text bug (mojibake, "Â·") on every wavetable oscillator view, which is exactly the kind of thing a commercial release cannot ship.
2. Space was "fixed" by stretching cards, so the emptiness moved inside the cards (PLAY's tall oscillator strips, FX cards with 400 px between graph and knobs, FM matrix cells, SEQ's six-slot rows). The dead-area test only measures bare page background, so it cannot see this.
3. Two surfaces still sit outside the theme (the table browser pop-up with a default-looking HEAR button and a bright blue scroll bar; the Save dialog's tag field with its chips touching it).
4. A text-fit rule that closes the gaps between words ("AMPENV", "PianoHammer") instead of widening a combo that has room.
5. Several things drawn two ways: the voice settings (header menu and VOICE tab), "UNISON" (card row and bottom tab), "SAVE" (header and FX), the randomiser (dice, SURPRISE ME, the dice on FX), the "go to the editor" links, and the "pick an oscillator" pills.
6. Fix-by-label: the filter "OPEN" pill is a passive badge sitting where the conventions say a switch goes; the PATCH tile and OUTPUT view built to fill PLAY's left column cannot appear at the default window size at all *(inferred from the layout maths; no shot shows them)*.

| Angle | Review 8 | Review 9 | Review 10 | Target |
|---|---|---|---|---|
| Against Vital | 7.5 | 8.0 | **8.4** | 9.5 |
| Against Serum 2 | 7.8 | 8.0 | **8.3** | 9.5 |
| Integration | 7.5 | 8.3 | **8.7** | 9.5 |

Cross-references: findings are filed once, in the section they fit best; "(see X)" points to the home of a related one.

**Did the review 9 additions land well?**

- *Dead-area UI test* (`tools/LayoutUiTests9.h:182-249`): the idea is right and it did force real fixes, but it measures only pixels that still equal the page background. A card that is stretched taller with the same contents passes. It cannot see empty card interiors, which is where the dead space went (V10-1, V10-2, V10-5). Add a second pass: inside each card, the largest rectangle with no text, graph or control (**landed half-way**).
- *VOICE tab on OSC*: works, labels are good (MODE, VOICES, BEND RANGE, GLIDE, LEGATO ONLY; `review-extra/t2/t2-osc-voice.png`). But it is the fifth tab in a strip at the bottom of the OSC page, a place nobody looks for "polyphony", and the header VOICES button still opens a separate nested menu with the same settings under different names (`PluginEditor.cpp:2252-2262`). Two doorways, two vocabularies (S10-1) (**landed, not finished**).
- *PATCH tile + OUTPUT view on PLAY* (`MainPage.h:528-545`): in none of the nine shot sets does either appear; they only show when the left column has 90+ px spare after the strips reach their 200 px cap. With one oscillator on and two folded (Init) the spare is zero. They are reachable only in the rare state of every extra oscillator off. As a fix for dead space it is mostly a no-op; the real fix was the strip growth (V10-2) (**does not land at default size**, inferred).
- *MSEG-to-LFO migration on load*: good. No MSEG card or chip in any shot; MSEG is a shape of an LFO. Not migrated: an oscillator whose ENVELOPE or warp envelope is set to MSEG (`ModulePool.h:77-96`) still brings the old module back. Acceptable, but say so in the tooltip (**landed**).
- *One `DashedAddButton`*: used on PLAY, OSC, FX, MATRIX and the VIA cell (`MatrixWidgets.h:16`, `FxPage.h:1905`, `MatrixPage.h:941`, `MainPage.h:1274`, `OscPage.h:1692`). Consistent and quiet. The OSC page's variant is a tab-sized 128 px dashed pill beside the tabs, the others are full-width rows, and the LFO/envelope pools keep a square "+" tile (**landed**).

---

# Part 1: Against Vital

Vital's strengths I measure against: every page full and balanced, a modulation drag that always shows where it will land, one control style, and type that is the same size everywhere.

## Fixed since review 9 / still open (V9-1 to V9-31)

| # | Review 9 item | Status |
|---|---|---|
| 1 | PLAY left column ends in dead space | **Partly fixed**: strips grow to a 200 px cap and the column fills (`init/01-MAIN`), but the extra height is empty card interior and a 83 x 190 px portrait thumbnail (V10-2) |
| 2 | DX7 voice hides two operators | **Fixed**: six compact rows, no scroll (`dx7/01-MAIN`, `neuro/added-osc-MAIN` with five) |
| 3 | FX bottom third empty; big add slot | **Fixed**: add slot is a slim row; cards grow (`neuro/10-FX`). New problem: cards stretch with 400 px of air between graph and knobs (V10-5) |
| 4 | Off modules keep their body | **Partly fixed**: WEST and BODY fold to one line on FILTER (`neuro/05-FILTER`); the OSC page's off SUB + NOISE panel and the PHYSICAL page's off BODY block still keep every dimmed control at full size (`keys/03-OSC`, `keys-grand-osc-scrolled`, `neuro/physical-page`) (V10-9) |
| 5 | Units disappear from values | **Partly fixed**: every value keeps its unit, but the space is dropped to fit, so one column reads "0.0 dB" then "-30.9dB" (`dx7/01-MAIN`, `IlanaLookAndFeel.h:262-292`) (V10-8) |
| 6 | Labels in a row shrunk to different sizes | **Still open**: UNISON smaller than SEMI/LEVEL (`neuro/01-MAIN`), KEY RATE smaller than CURVE (`neuro/06-ENV-LFO`), "KEYRATE" at 75 % (`small/06-ENV-LFO`) (V10-7) |
| 7 | VECTOR spreads six groups | **Mostly fixed**: CORNERS / POSITION / MOTION boxes (`neuro/02-VECTOR`); the boxes are still half empty (V10-14) |
| 8 | Small-patch FM matrix floats | **Partly fixed**: hint, header and cells top-aligned (`init/fm-no-input`); still a 3 x 3 grid of big empty cells and a half-empty card (V10-13) |
| 9 | OSC lower half is air | **Partly fixed**: AMT knobs hidden until a warp is chosen; the SUB + NOISE strip is still two knobs and three combos across 1500 px (`neuro/03-OSC`) (V10-9) |
| 10 | Algorithm 18 crowds | **Not verified** (no render this round) |
| 11 | Combos in one row at different sizes | **Still open**, and now worse to read: FM card "Harmonic" / "AMPENV", PHYSICAL "PianoHammer" are set with their word gaps closed (`neuro/fm-dx-keys`, `neuro/physical-page`) (V10-6) |
| 12 | Five "add" affordances | **Fixed** (one component; pools keep a "+" tile) |
| 13 | A do-nothing filter drawn at full strength | **Half-way**: a grey "OPEN" pill appears in the header, but it is a passive badge in a switch's place (V10-10) |
| 14 | SCOPE replaces PLAY but PLAY stays lit | **Fixed**: tab and pill dim (`neuro/scope-panel`) |
| 15 | Destructive button is primary | **Fixed**: SAVE AND LOAD is the filled button (`confirm/00-confirm`) |
| 16 | Routing text sits on LFO waveforms | **Mostly fixed**: routing is a pill in the header row (`neuro/06-ENV-LFO`); still "+2 more" truncation |
| 17 | Two "SUB"s and a noise with no switch | **Fixed** on PLAY (one title, one switch) |
| 18 | Modulation rings touch labels | **Still open**: the FRAME ring's dots overlap its label (`neuro/01-MAIN`) (V10-18) |
| 19 | Output meter is the smallest thing | **Partly fixed**: laid on its side, 64 px wide, and the resize grip still crosses it (`PluginEditor.cpp:1390`) (V10-17) |
| 20 | "B" unexplained | **Fixed** (the chips read "LFO 1 OUT 2") |
| 21 | Per-card CPU on some FX cards | **Not verified** (not visible in the FX sheet) |
| 22 | OP ENV card dashed outline unexplained | **Fixed** (inferred: solid outline and "6 ops" chip in `dx7/06-ENV-LFO`) |
| 23 | Scrolling pages cut content hard | **Not verified** (nothing scrolls at default size now) |
| 24 | Filter markers at 20 kHz | **Fixed** (separate dots, `init/01-MAIN`) |
| 25 | Disabled text below readable contrast | **Still open** *(inferred)*: dim "OSC 2 Wavetable · HardSync" rows and dim COLOUR / DETUNE labels look near 2:1 (`init/01-MAIN`) |
| 26 | 9 px text at 75 % | **Partly fixed** (screen floor in `fittedFont`); KEY RATE still the smallest label |
| 27 | BPM / VOICES / CPU are grey fragments | **Mostly fixed**: VOICES is a rimmed drop-down button; BPM and CPU remain grey text |
| 28 | Matrix headers "TO OSC 1" then bare "OSC 2" | **Fixed** (`init/fm-no-input`: "TO OSC n" on all, dim for off) |
| 29 | Log time axes label two ticks | **Still open**: AMP ENV shows "10 ms, 100 ms" only (`neuro/06-ENV-LFO`); PLAY's shows "100 ms" (V10-19) |
| 30 | Save dialog has two ways to enter tags | **Still open**, plus a new overlap (V10-11) |
| 31 | LFO DRIVES list is plain text | **Not verified** *(inferred: still plain text)* |

## Findings (Vital), worst first

**V10-1. High: a text bug is printed on every wavetable oscillator view.**
- What: the caption under the 3D / WAVE view reads "FRAME 6 / 64 Â· now" (`neuro/03-OSC`, `neuro/osc-bounce`, `review-extra/t2/t2-osc-voice.png`). Cause: `WaveDisplay.h:1008` builds the string from a UTF-8 literal that already holds the double-encoded characters (`" Â· now"`), so the middle dot shows as two glyphs. The rest of the file uses `\xc2\xb7`. The review 9 note (S9-12) asked for this caption to be reworded; the rewording shipped with a broken character.
- Vital: no text artefacts, ever.
- Fix: use `juce::String::fromUTF8 (" \xc2\xb7 now")`, then add the string to the UI test (fail on any "Â", "Ã" or "â€" in painted text; the 60-page test already walks every page).

**V10-2. High: the dead space moved inside the cards.**
- What: PLAY's oscillator strips now grow to 200 px while their contents stay 100 px tall. On Init, OSC 1's controls sit in the top 40 % of a 195 px card, with 70 px empty under them, and the waveform thumbnail is stretched into a portrait box (about 83 x 190) so a saw looks like a distorted tick; a physical string is drawn as a vertical hairline (`init/01-MAIN`, `keys/01-MAIN`). SUB + NOISE does the same (`init/01-MAIN`). The page is full, but not balanced.
- Vital: sections grow their content (graphs get taller), not their padding.
- Fix: cap the strip at about 150 px; give the surplus to the right column's graphs (FILTER response, ENVELOPE), which can use it, or centre the control row vertically and keep the thumbnail square (about 110 x 110) with a one-line caption under it (shape name, frame). Root: `MainPage.h:459` (`maxSlotHeight = 200`, `MainPage.h:1280`).

**V10-3. High: the dead-area test cannot see the dead areas that are left.**
- What: `tools/LayoutUiTests9.h:182-249` counts only pixels equal to the page background. Card fills, an empty FM cell, the black SIGNAL FLOW box (`neuro/05-FILTER`: about 40 % of it blank), PROB SEQ's blank margins (`neuro/gen-probseq`) all count as "painted". The commit "dead-area calibration" tuned the threshold to pass.
- Vital: n/a (process), but this is why review 10 still finds "dead area" findings.
- Fix: a second measure inside each card (largest rectangle with no glyph, graph line or control, found the same way on a "content mask" drawn by the widgets) with a limit of about 25 % of the card.

**V10-4. High: the PATCH tile and OUTPUT view do not appear at the default size** *(inferred from `MainPage.h:528-545`)*.
- What: the spare height after the strips reach the cap (`patchMinHeight = 90`, `MainPage.h:1293`) is zero for Init, Neuro, Keys and a six-operator voice, and the window is aspect-locked (`PluginEditor.cpp:763-767`), so a taller window never happens. Only "every extra oscillator off" frees 150 px. The feature (a live signal-flow tile on PLAY) is good, so it should not be reserved for an edge case.
- Fix: decide: either make it the place the surplus goes (V10-2: oscillator strips 150 px, then a PATCH tile of 90 px always) or delete the code. Do not leave it as a state nobody can see.

**V10-5. Medium-high: FX cards and the SEQ rows stretch their controls far apart.**
- What: a three-knob CHORUS or VOWEL card puts its graph at the left and its knobs on a 1,500 px span with 400 px between them (`neuro/fx-split`, `neuro/10-FX`: OTT has two knobs, 700 px apart); SEQ's RATE / STEPS / GATE / MODE / OCTAVES / CHANCE sit in six equal slots, with a left-aligned combo above a centred knob in the same slot (`neuro/09-ARP-SEQ`); PROB SEQ shows three controls in a row of six (`neuro/gen-probseq`).
- Vital: a card is as wide as its content; controls sit in a tight group beside their graph.
- Fix: place a card's knobs in a fixed 80 px grid starting at the graph's right edge; the surplus becomes a larger graph. For SEQ, left-align the row and give MODE and RATE the same cell width as knobs.

**V10-6. Medium-high: a long label shrinks by squeezing word gaps instead of using the room next to it.**
- What: `fittedFont` (`IlanaLookAndFeel.h:221-225`) applies up to -0.12 em of tracking before it gives up. On a narrow combo this closes the space between words: "AMPENV" and "Harmonic" on the FM card (`neuro/fm-dx-keys`), "PianoHammer" on the PHYSICAL page (`neuro/physical-page`), where the EXCITE combo is 125 px wide in a row with 700 px free. The conventions say text is "never condensed sideways"; tracking at -0.12 em has the same effect.
- Vital: a combo is sized for its longest option.
- Fix: size combos by their longest item (`getBestWidthForText`), and limit tracking to -0.04 em; a word gap should never close.

**V10-7. Medium-high: labels in one row are still different sizes (V9-6).**
- What: UNISON is visibly smaller than SEMI and LEVEL on PLAY (`neuro/01-MAIN`); KEY RATE is smaller than CURVE (and "KEYRATE" at 75 %) (`neuro/06-ENV-LFO`, `small/06-ENV-LFO`). Same cause as V10-6: each label fits itself.
- Fix: fit a row as one unit: take the smallest size any label in the row needs, apply it to all; or rename (KEY RATE to KEY RT would still need the row rule, so do the row rule).

**V10-8. Medium: values drop the space before the unit inconsistently.**
- What: the same column shows "0.0 dB", "-30.9dB" and "-151dB" *(the last reads like a lost decimal point at 100 %, and is "-15.1dB" at 75 %; `dx7/01-MAIN`, `dx7-small/01-MAIN`)*. The fitter removes the space and shrinks the tracking (`IlanaLookAndFeel.h:262-292`). V9-5 asked that a unit is never dropped; it is not, but the readout still changes shape.
- Fix: widen the value cell to 76 px logical (the knobs have 40 px free each side), or show "dB" once in the column header ("OUTPUT dB") and numbers only.

**V10-9. Medium: off modules on OSC and PHYSICAL keep every dimmed control.**
- What: FILTER's WEST/BODY fold to one line; the OSC page's SUB + NOISE panel off still shows six dimmed controls across 1500 px (`keys/03-OSC`, `keys-grand-osc-scrolled`), and the PHYSICAL page's off BODY block keeps TYPE, AMOUNT, DECAY, and "EDIT BODY ›" live (`neuro/physical-page`). One rule, applied on one page.
- Fix: fold to header + caption on all three, with the same animation.

**V10-10. Medium: the filter "OPEN" pill is a badge where a switch should be.**
- What: with cutoff at the top, F1 and F2 show a rimmed "OPEN" pill beside the 12 / 24 dB buttons (`neuro/05-FILTER`, `init/filter-six-osc`). It is drawn by `FilterVectorPhysicalPages.h:44-56` and is not clickable. It looks like the 12 dB button next to it, it states a state in text with no switch (`docs/UI-CONVENTIONS.md`, "Never state on or off in text without a switch"), and a user cannot turn the filter off from it.
- Vital: a filter has an on/off (a power switch in the header).
- Fix: a real header switch (append-only parameter `filterN_on`, off = bypass; default on) and drop the pill; if a parameter is not wanted, grey the whole card at the open end as V9-13 first proposed and remove the pill.

**V10-11. Medium: the Save dialog's tag field and its chips touch, and there are still two ways to enter tags.**
- What: the TAGS field and the first chip row have 0 px between them (`extras/extra-save-as`; `SavePresetOverlay.h:285-288`: `chipArea` starts at the field's bottom edge). The two inputs edit the same list (V9-30).
- Fix: 8 px gap; make the field read-only (it shows the list), or hide it and keep the chips.

**V10-12. Medium: the table browser is outside the theme.**
- What: the HEAR button is drawn in a default look (larger system sans, light rim, `HEAR` bigger than any other button) and the scroll bar is a bright blue (`neuro/table-browser`, `neuro/table-browser-editor`). Cause: the browser lives in a `CallOutBox` outside the editor's tree (`TableBrowser.h:73-90`, "the call-out is not scaled with the editor's content") and never gets `IlanaLookAndFeel`. The tiles and search field are themed by hand, so only the stock widgets are wrong.
- Fix: `setLookAndFeel` on the browser root (and on the call-out) with the editor's look; draw HEAR as the other toggle pills.

**V10-13. Medium: the FM matrix on a small patch is a 3 x 3 grid of empty cells and on a DX7 voice the bottom third of the card is blank.**
- What: `init/fm-no-input`, `neuro/08-FM`: nine 140 px cells, one has a dot; `dx7/08-FM`: below the MORE button there is a 130 px empty band. Cells have the same size at 3 and at 6 operators only because the card is a fixed height.
- Fix: size cells to fit (3 ops: 100 px cells, centred; 6 ops: as now), and move RING MOD / SYNC / NOISE FM inline for all counts (see I10-5).

**V10-14. Medium-low: the VECTOR boxes are mostly air.**
- What: CORNERS has 80 px of nothing above its first combo; POSITION puts two knobs 350 px apart with 90 px above; MOTION puts PATH (a toggle with a label above) far left of the knobs (`neuro/02-VECTOR`).
- Fix: 12 px padding, controls in a left-aligned row; give the pad the surplus.

**V10-15. Medium-low: two sizes of "add oscillator" on OSC and PLAY, and the PLAY row is inside the scroll area.**
- What: OSC page: tab-sized pill (128 px, `OscPage.h:877`); PLAY: a full-width row (`MainPage.h:1274`). Minor, but it is the one "add" idiom the review asked to unify.
- Fix: same shape, or leave and note.

**V10-16. Medium-low: the DX7 PLAY strips have "OP ENV" printed over the curve and the "OUT / MOD > 1" caption under the title in a different colour.**
- What: `dx7/01-MAIN`: the tiny "OP ENV" caption collides with the curve's peak in the thumbnail of OSC 1, 3, 5; the routing caption ("MOD > 1") is a 9 px line in the oscillator's colour that is the only coloured text in the strip.
- Fix: caption outside the plot (bottom-left); routing in the title row.

**V10-17. Low-medium: the output meter is 64 px wide and the resize grip draws across it** (`PluginEditor.cpp:1390`; `neuro/01-MAIN`, bottom right). V9-19 asked for the meter to be bigger; it is laid on its side but it is still the smallest control on the screen.
- Fix: 110 px wide, shifted left of the grip.

**V10-18. Low-medium: modulation rings touch the labels** (V9-18): the FRAME ring on PLAY (`neuro/01-MAIN`) reaches the label text. Fix: 4 px more space under labels on knobs with rings.

**V10-19. Low-medium: log time axes show two ticks** (V9-29). AMP ENV on MOD labels "10 ms" and "100 ms" and nothing until KEY UP (`neuro/06-ENV-LFO`). Add "1 s" and "10 s" when the plot spans them.

**V10-20. Low: "GRID 8" is a pill inside the LFO graph that looks like a label** (`neuro/06-ENV-LFO`, top right of the plot) *(inferred: it is a button for the grid)*. Make it a visible combo, or move it to the LFO card's control row.

**V10-21. Low: the LFO sub-line caption is three clauses long** ("shared · restarts on any new note (held notes jump too) · 2 outputs", `neuro/lfo-sim-lorenz`). Fix: "shared · restarts on note · 2 outputs", detail in the tooltip.

**V10-22. Low: remap editor text is tight.** "Left to right..." and "Click to add a point..." run into the QUICK SHAPES caption; DEAD ZONE touches EASE OUT (`remap/remap-editor`). Fix: 6 px of space, labels at the same size, 2 lines.

## Score against Vital: 8.4 / 10

Up 0.4 from review 9. The page structure, the matrix and the oscillator strips now compare to Vital's, and the drag/hover model is on par. Vital's finish is what is missing: a text artefact on a main view, controls spread on rows with no relation to their graphs, stock widgets in the table browser, labels that tighten themselves illegible, an unspecified "OPEN" badge. Fix V10-1, 2, 5, 6, 7, 10, 12 and it is a 9.

---

# Part 2: Against Serum 2

Serum 2's strengths I measure against: a short path to every common job, one place for every setting, a browser that is a tool, and a matrix and effects rack that carry a lot of data without scrolling.

## Workflow speed (the same four jobs)

| Job | Review 9 | Review 10 |
|---|---|---|
| Find and load a sound | 3 clicks, browser chrome heavy | **Better**: docked browser with a detail card (macros, tags, author) (`extras/extra-browser-docked`); the pop-up variant still has a two-row header and no card (`neuro/preset-browser`) |
| Route an LFO to a knob | drag the chip | **Same**: drag a chip; the drag feedback is in code (`ParamControls.h:989-1030`) *(inferred)* |
| Change polyphony / glide | 3 clicks in a nested menu | **Two doorways**: OSC > VOICE tab (2 clicks, `t2-osc-voice.png`) or header VOICES > nested submenus (S10-1) |
| Build a modulation set with 12+ routes | scroll, 56 px rows | **Slightly better**: rows are 34 logical px and 12 fit (`neuro/07-MATRIX`); still no multi-select or "make a macro from this" |
| Make a sequence | tabs select and enable separately | **Fixed**: "ARP is off: draw to switch it on" (`neuro/09-ARP-SEQ`), EXPORT MIDI exists (`gen-clip-expanded`) |

## Fixed since review 9 / still open (S9-1 to S9-26)

| # | Review 9 item | Status |
|---|---|---|
| 1 | Voice mode, voices, bend, glide behind grey text | **Fixed with duplication**: VOICE tab on OSC; VOICES is a rimmed button; both doors remain (S10-1) |
| 2 | "VOICE" names two things | **Mostly fixed**: the bottom OSC tab "UNISON" now stands apart from VOICE, but "UNISON" is also the card's row title (S10-3) |
| 3 | SEQ tabs select and enable separately | **Fixed**: the tab has a small switch; the grid says it is off and turns on when drawn |
| 4 | Modulation and parameters have three homes | **Partly fixed**: MOD owns routes; per-osc FM depth, per-card knobs and the matrix all show the same depth (I10-8) |
| 5 | Airwindows flip swaps every control | **Partly fixed**: a BUILT-IN / AIRWINDOWS segmented control in the card header; the AW card gets an ALGORITHM combo and its own knob names (INPUT, SOFTEN, HEAD BUMP, FLUTTER, OUTPUT) *(FX sheet; not read at full size)*; "AIRWINDOWS (ALL)" remains as a third kind (I10-9) |
| 6 | REMAP placeholder is 300 px of nothing | **Fixed**: the note fills only spare rows (commit `b8a73bb`); nothing on a full list (`neuro/07-MATRIX`) |
| 7 | Matrix rows 56 px tall | **Partly fixed**: 34 px logical; Serum's are 24-28 |
| 8 | Browser chrome eats the list | **Partly fixed**: docked variant is good; pop-up still spends 190 px above the list |
| 9 | STRUM / SNAP TO KEY gated twice | **Still open**: the header switch plus a combo with value "Off" for each (`neuro/09-ARP-SEQ`) (S10-5) |
| 10 | No MIDI export | **Fixed**: EXPORT MIDI beside IMPORT MIDI (`neuro/gen-clip-expanded`) |
| 11 | Clip VEL lane has no scale | **Fixed**: 127 / 1 ticks (`gen-clip-expanded`) |
| 12 | Frame readout and knob disagree | **Still open**, and now also broken (V10-1) (S10-6) |
| 13 | ARP lane numbers drift | **Not verified** (not in this round's ARP render) |
| 14 | FX card widths mix | **Still open** (V10-5) |
| 15 | PROB SEQ shows three controls and an empty half row | **Partly fixed**: centred, still three controls |
| 16 | "STEP OFF" unexplained | **Fixed** ("ARP OFF" + caption) |
| 17 | Unused AMP ENV chip on DX7 voices | **Fixed** (`dx7/01-MAIN`: OP LFO / OP PITCH instead) |
| 18 | No shown warp handles | **Partly fixed**: a hint "drag up / down: warp" on WAVE view (`osc-pd-chain`); the 3D view's hint is "drag across: frame", the "up / down: warp" half is cut (`neuro/03-OSC`) (S10-6) |
| 19 | Table browser: no audition or favourites | **Fixed** (HEAR, stars) but unthemed (V10-12) |
| 20 | Welcome tour is a wall | **Fixed** ("WHAT'S NEW IN 1.3" folded, `neuro/00-tutorial`) |
| 21 | Shortcuts reach only seven tabs | **Fixed**: "Ctrl+Shift+1-3 switch pages" in the tour |
| 22 | Duplicate FX card exposes the engine | **Fixed in wording** ("Duplicate Drive (not supported)... Remove it", FX sheet) |
| 23 | No author in the browser | **Fixed on the docked card** ("by ilanaSynth"); no author column in the list |
| 24 | Bank chips repeat in DX7 rows | **Same**, harmless |
| 25 | ADD OSC row inside the scroll | **Fixed** (compact strips, no scroll) |
| 26 | Header scope squiggle looks like an underline | **Still open** (`neuro/01-MAIN`) (S10-14) |

## Findings (Serum 2), worst first

**S10-1. High: voice settings have two doors with different names.**
- What: OSC > VOICE tab says MODE, VOICES, BEND RANGE, GLIDE, LEGATO ONLY (`t2-osc-voice.png`, `OscPage.h:920`). The header VOICES button opens a nested menu: "Voice mode: Poly", "Voices: 32", "Pitch bend range: 2 st", Glide (`PluginEditor.cpp:2252-2262`). Neither door mentions the other. The VOICE tab is the 5th tab of the lower strip of the OSC page, with the label "VOICE".
- Serum 2: one global voice section (Global tab: Poly/Mono, bend, portamento).
- Fix: the header button opens the same panel as a call-out (the VOICE tab's content), or jumps to OSC > VOICE; drop the nested menu. Name the tab VOICES / PLAYING.

**S10-2. High: SEQ's "Off" combos duplicate the switches (S9-9, still open).**
- What: SNAP TO KEY has a header switch and KEY = "Off"; STRUM has a header switch and DIRECTION = "Off" (`neuro/09-ARP-SEQ`); only SPRAY got a clean pair. With STRUM on, DIRECTION still offers "Off" (`gen-probseq` shows "Up").
- Fix: remove "Off" from DIRECTION and KEY; the switch is the only off.

**S10-3. Medium-high: "UNISON" is still two things on one page.**
- What: the card's UNISON row (UNI MODE, UNISON, DETUNE, BLEND, SPREAD, CHORD) and the bottom tab UNISON ("How the unison voices spread, start and drift", `OscPage.h:915`, `1016`). A user looking for "unison" finds the row, and does not know the tab holds more.
- Fix: rename the tab (UNISON ADVANCED, or fold it into the card with a "MORE" disclosure).

**S10-4. Medium-high: the oscillator's strip tabs ("SUB + NOISE", UNISON, SYMPATHETIC STRINGS, ACOUSTIC KEYS, VOICE) are a drawer of unrelated things.**
- What: one tab strip holds an oscillator part (SUB + NOISE), a unison mode panel, two body-simulation modules and the voice settings, with one on/off switch at the right that applies to only some tabs (`neuro/03-OSC`, `t2-osc-voice.png`). The switch appears or goes per tab; the tab dots are on/off for two of them.
- Serum 2: Global / Voice / Noise-Sub sit in separate places.
- Fix: group: left, SUB + NOISE; right, a "GLOBAL" drawer: UNISON · VOICE · SYMPATHETIC · ACOUSTIC KEYS, with a separator.

**S10-5. Medium: the browser pop-up and docked browser are two different layouts** (`neuro/preset-browser` vs `extras/extra-browser-docked`). The pop-up has no detail card (macros, tags), a 2-row header and the DOCK button; the docked variant has the card and a FLOAT button. Fine as a choice; the pop-up should carry the card on wide windows.

**S10-6. Medium: the 3D / WAVE view's captions.**
- What: "FRAME 6 / 64 Â· now" (V10-1) next to "drag across: frame" (the full string is "drag across: frame · up / down: warp", cut: `WaveDisplay.h:1441`, `neuro/03-OSC`). The frame is the playing (modulated) frame while the FRAME knob shows the set value 30 %; "now" now says so, but 6 / 64 against 30 % still reads as a contradiction.
- Fix: "frame 6 of 64 now" at the left, hint on a second line or in a tooltip.

**S10-7. Medium: the FX page header row uses names the rest of the app does not.**
- What: "CHAIN 1 / CHAIN 2 / COPY TO 2 / [dice] FX / SAVE / LOAD" (`neuro/10-FX`). "SAVE / LOAD" is the FX chain preset while the header SAVE is the patch; the dice "FX" randomises the chain, and there are two other dice (header, SURPRISE ME). 
- Fix: "SAVE CHAIN" / "LOAD CHAIN"; the dice labelled "RANDOMISE FX" in a tooltip; see I10-3.

**S10-8. Medium: matrix: the matrix is still 12 rows and a hard stop.**
- What: 34 px rows, 12 on screen out of up to 64 (`neuro/07-MATRIX`); a 20-route patch scrolls. No filter by destination, no collapse by source.
- Serum 2: 24-28 px rows, filter by source.
- Fix: 28 px rows (the controls fit); a destination filter field in the header.

**S10-9. Medium: PROB SEQ and EUCLID rows (S9-15).** The control row is RATE / STEPS / GATE centred with 400 px margins; ARP has six slots. Fix: left-align and fill with the engine's own parameters (swing, direction) or shorten the card (`neuro/gen-probseq`).

**S10-10. Medium-low: macros 5-8 on the bottom bar show "MACRO 5 + ASSIGN" at full width while 1-4 carry names (TONE, TALK, TEAR, SUB).** The bottom bar is 8 knobs where half are placeholders on most patches. *(Settled: 8 macros.)* Fix: show 4 slots and a "+" that adds the next.

**S10-11. Medium-low: "MORE AIRWINDOWS" / "AW" in the add menu.** The empty-rack picker has an "AW" box on 17 effects and a dim "AW" box where only Airwindows exists (TAPE, CONSOLE) (`neuro/fx-empty`). "AW" is not a word anyone knows. Fix: "AIRWINDOWS" as a small second-line caption, or a legend under the grid.

**S10-12. Low-medium: the SEQ "GENERATE" boxes are three different heights of content** (SNAP TO KEY: 4 controls; STRUM: 2; SPRAY: 6), each with 30 px of nothing under it (`neuro/09-ARP-SEQ`). Fix: align the control rows to the bottom of the box and let the box be as tall as its tallest.

**S10-13. Low-medium: the ARP VEL lane draws full bars while the ARP is off** (`neuro/09-ARP-SEQ`): the pattern looks active while the caption says "ARP is off". Fix: draw the lanes in the dim colour while the engine is off.

**S10-14. Low: the header scope squiggle sits under the preset name like an underline** (S9-26, `neuro/01-MAIN`). Give it a band with a baseline, or remove it (SCOPE exists).

**S10-15. Low: Save dialog "NAME" field accepts ":" and "/" and then explains it in orange after the fact** (`extras/extra-save-as`). Fix: strip as typed, or leave the warning but make it a hint, not an orange line.

## Score against Serum 2: 8.3 / 10

Up 0.3. The preset browser with its detail card, MIDI export, the "engine is off" messages, the table browser's HEAR and favourites, and the voice tab are the right moves. Serum 2 still wins on density of the matrix, one place for each global setting, and a browser whose pop-up and dock behave the same. S10-1, 2, 3, 4 and 7 are about a day of work and lift this by a half point.

---

# Part 3: Integration

How well DX7/FM, physical, sample, Airwindows and the newer features read as parts of one synth.

## Fixed since review 9 / still open (I9-1 to I9-26)

| # | Review 9 item | Status |
|---|---|---|
| 1 | Simulation LFOs: three controls for one idea | **Partly fixed**: one TRIGGER combo, a PER VOICE switch, a FIRE button (`neuro/lfo-sim-lorenz`); the switch label is cut to "PER V..." (I10-6) |
| 2 | Legacy MSEG is a separate card and chip | **Fixed on load** (no MSEG card or chip in any shot); an oscillator's MSEG envelope choice still brings the module back |
| 3 | Physical modelling has two homes | **Mostly fixed**: the OSC page shows EXCITE / DECAY / DAMP and "EDIT STRING ›"; the PHYSICAL page is the editor (`keys/03-OSC`, `neuro/physical-page`) |
| 4 | Switch placement differs between sub-boxes | **Partly fixed**: BODY and SOUNDBOARD switches sit in their title line, but BODY puts the label and switch on one line and SOUNDBOARD uses a different alignment (`neuro/physical-page`) |
| 5 | "Pick an oscillator" drawn four ways | **Partly fixed**: still three pill forms: OSC page ("OSC 1 OUT"), PHYSICAL page ("OSC 1" with a ring), FM / OP ENV (bare numbers) (I10-4) |
| 6 | An FM operator on OSC says "Wavetable" | **Fixed** on DX7 voices (mode "Operator", `dx7/03-OSC`); a plain patch that gets an algorithm keeps "Wavetable" (`init/osc-pd-chain`, inferred tool artefact) |
| 7 | Operator level has two names | **Half-way**: now OUTPUT (dB) and LEVEL (%) on the same operator, both visible (`dx7/01-MAIN`) (I10-1) |
| 8 | Macros named three ways | **Fixed** (bottom bar and matrix agree) |
| 9 | Caption convention on the matrix and FM | **Fixed** ("rows modulate columns"); FM hint sentences end with a stop |
| 10 | Name pairs (SUB/NOISE, STRINGS SOUNDBOARD) | **Mostly fixed**: SUB + NOISE everywhere; "STRINGS" in the signal flow vs "SYMPATHETIC STRINGS" tab (I10-10) |
| 11 | OP PITCH graph has no time ruler | **Fixed** *(inferred: not re-rendered)* |
| 12 | Macro warnings are bare triangles | **Still open** (TALK ⚠ in `neuro/scope-panel`) |
| 13 | ARP has GATE twice | **Fixed** (STEP LEN lane, GATE knob) |
| 14 | "Go to the full page" has five idioms | **Mostly fixed**: "EDIT OP ENV ›", "EDIT STRING ›", "EDIT ON FM ›", "EDIT OP PITCH · OP LFO ›"; the ↗ icon on PLAY's cards remains (I10-7) |
| 15 | Airwindows uses a second vocabulary | **Partly fixed**: units added; names are still the algorithm's own (SOFTEN, HEAD BUMP, FLUTTER) and the 39-algorithm "AIRWINDOWS (ALL)" card is a different layout (I10-9) |
| 16 | Option casing drifts | **Fixed** ("Low Band", "M/S Side", "Room") |
| 17 | Vector corner combos offer a missing oscillator | **Partly fixed**: "OSC 4 (not added)" is listed, but still offered (`neuro/02-VECTOR`) |
| 18 | LFO RATE knob shows a yellow tick | **Fixed** |
| 19 | Per-voice versus shared invisible | **Fixed** ("shared · runs free" in the LFO header on PLAY and MOD) |
| 20 | Macros 5-8 only by dragging | **Fixed** *(inferred: "+ ASSIGN" is a button)* |
| 21 | SEQ chain caption can be 38 chars | **Fixed** ("EUCLID's hits trigger the clip", `neuro/gen-clip-expanded`) |
| 22 | Sample / SF2 pages not rendered | **Seen now**: same shell as wavetable, own SAMPLE row, zone map (`neuro/osc-sample-sfz`); good, with two naming slips (I10-12) |
| 23 | "Not verified" items | **Partly done**: KEYS & VELOCITY tab exists on the operator envelope (`dx7/08-FM`); the rest not re-checked |
| 24 | A/B and CHAIN 1 / 2 | **Settled** |
| 25 | Value formats (Free, Auto) | **Still open**, minor ("Free" as a SEED value, `neuro/lfo-sim-lorenz`) |
| 26 | KEY LVL cryptic | **Fixed** ("KEY SCALE") |

## Does the build follow `docs/UI-CONVENTIONS.md`?

| Rule | Followed? |
|---|---|
| On/off switches in the header; tab dot or tab switch; no on/off text without a switch | **Broken once**: the filter "OPEN" pill (V10-10). Otherwise kept (WEST, BODY, SOUNDBOARD, FX, SEQ tabs all have switches) |
| Captions: lower-case fragments in headers, sentences under controls | **Kept**; but the LFO sub-line is a three-part sentence (V10-21) and "FM FROM OSC 2" on OSC is an upper-case fragment while others are lower-case ("OSC 1's string, moving as you play" on PHYSICAL) |
| Casing: names upper case, prose sentence case | **Kept**; "Acid: Bass / Mk 2" etc are user text |
| Buttons say what they do: SAVE, CANCEL, LOAD ANYWAY | **Kept in dialogs**; "HEAR" in the table browser is the odd one (and unthemed); "FIRE" is the one verb button on an LFO |
| Separators " · " | **Kept** (but a stray "Â·" is rendered in the frame caption: V10-1) |
| Text that may not fit: never condensed sideways | **Kept to the letter, broken in effect**: tracking at -0.12 em closes word gaps (V10-6) |
| Page switches right after the tabs | **Kept** |
| Cards without a family colour have no dot | **Kept** |
| The 60-page UI test paints every page and fails on cut text | **Kept**; extend it to fail on non-ASCII mojibake (V10-1) and to measure card-interior emptiness (V10-3) |

## Findings (Integration), worst first

**I10-1. High: the DX7 operator still carries two level controls on every surface, OUTPUT (dB) and LEVEL (%).**
- What: `dx7/01-MAIN` (PLAY), `dx7/03-OSC` (OSC), `dx7/08-FM` (FM) all show RATIO / (SEMI) / FINE / OUTPUT 0.0 dB / LEVEL 50 %. They are two stages (the operator's output into the algorithm, and its level into the voice), but a user can only tell by reading the tooltip. Every wavetable oscillator has one LEVEL.
- Benchmark: Dexed shows a single OUTPUT LEVEL (0-99).
- Fix: on a DX7 voice, call LEVEL "VOICE LEVEL" (or hide it behind a disclosure on PLAY and FM, which show a strip's 5 knobs), keep OUTPUT as the operator's level; label units consistently.

**I10-2. High: "go to the editor" and "pick an oscillator" are drawn in different ways, and each new feature adds a variant** (I9-5, I9-14 partly fixed).
- What: oscillator pickers: OSC page pills with routing text ("OSC 2 OUT, MOD > 1"), PHYSICAL page pills with a ring ("OSC 1", "OSC 2"), FM card / OP ENV pills with bare coloured numbers "1 2 3 4 5 6" (`dx7/08-FM`, `dx7/06-ENV-LFO`), the FM matrix rows ("OSC 1 OUT [switch]"). Links: the ↗ square icon on PLAY's cards, "EDIT OP ENV ›", "EDIT STRING ›", "EDIT ON FM ›", "EDIT OP PITCH · OP LFO ›" and the LFO pills.
- Fix: one picker component (coloured dot + "OSC n", optional status line under it) in two sizes; one link style ("EDIT ... ›") including on PLAY's cards, where the ↗ icon would become "EDIT ›".

**I10-3. Medium-high: one idea, three names: the randomiser, and two SAVEs.**
- What: the dice icon in the header, "SURPRISE ME" in the browser (`neuro/preset-browser`), the dice "FX" on the FX page, and "the dice rolls a fresh patch" in the tour; header SAVE vs FX "SAVE / LOAD" (S10-7). "KEYS" is the on-screen keyboard button, a preset category, the SEQ chain's input node and "ACOUSTIC KEYS", four meanings.
- Fix: a vocabulary line in `UI-CONVENTIONS.md`: RANDOMISE (dice), SAVE (patch) vs SAVE CHAIN, KEYBOARD (the button), KEYS (the SEQ node only).

**I10-4. Medium-high: the DX7 stack is the best-integrated special system, and still shows seams.**
- What: (a) PLAY and OSC and FM show the same operator in three layouts with different knob orders: PLAY has no SEMI, FM has SNAP and a dimmed FB TYPE with no FEEDBACK knob (feedback sits in the matrix's diagonal cell), OSC has WAVE + FEEDBACK + FB TYPE (`dx7/01-MAIN`, `dx7/03-OSC`, `dx7/08-FM`); (b) a plain patch that picks a DX7 algorithm keeps Wavetable oscillators in an operator topology (`init/fm-dx-keys` is a tool artefact, but a user can do the same); (c) on a DX7 voice AMP ENV is "unused" while its card, tab and knobs remain on PLAY and MOD (`dx7/01-MAIN`, `dx7/06-ENV-LFO`).
- Fix: put FEEDBACK on the FM card next to FB TYPE; picking a DX7 algorithm should offer "switch these oscillators to Operator mode"; hide the unused AMP ENV tab.

**I10-5. Medium: ring mod, sync and noise FM are inline on a 3-oscillator FM page and behind a button on a 6-operator one.**
- What: `neuro/08-FM` shows NOISE FM row, RING MOD knob, SYNC switch and their hint; `dx7/08-FM` shows "MORE: RING MOD · SYNC · NOISE FM" as a full-width button that reads like a heading (`FmInputPages.h:123`). Same controls, two layouts, depending on operator count.
- Fix: a section in both ("EXTRAS" row at the matrix foot) at the same place.

**I10-6. Medium: the simulation LFO's switch label is cut ("PER V...") and TRIGGER is three controls** (`neuro/lfo-sim-lorenz`: TRIGGER combo, OUT 1 AXIS combo, FIRE button, PER V... switch, KEY switch, SYNC switch). The caption says "restarts on any new note (held notes jump too)". I9-1 asked for one trigger model; there is one concept, but still six controls to set it, and the label ends in an ellipsis.
- Fix: label "PER VOICE" at full width (widen the cell to 80 px); fold FIRE into the TRIGGER combo as a "Manual" mode with a button.

**I10-7. Medium: the on-page cards that belong to one engine are drawn with different structure.**
- What: PHYSICAL page: STRING, EXCITER boxes with section labels at the top-left in orange; BODY and SOUNDBOARD sub-boxes with the switch left of the title; OSC page: STRING / PITCH & LEVEL / UNISON as left-column orange section labels (`neuro/physical-page`, `keys/03-OSC`). SEQ's GENERATE boxes use grey titles with a state word; FILTER's cards use a header switch at the right. Three styles of "box with a title".
- Fix: one sub-box component (title, optional state word, switch at right) shared by PHYSICAL, SEQ and the OSC page's lower strip.

**I10-8. Medium: modulation depth for FM is shown three ways.**
- What: the FM matrix cell ("30 %"), the algorithm diagram's link label ("30 %"), and the matrix route "FM › OSC 2 → OSC 1 +30 %" (`neuro/08-FM`, `neuro/07-MATRIX`): all three are the same number, editable in two places; and the destination name uses "→" while the rest of the matrix uses "›".
- Fix: "FM › OSC 2 › OSC 1" (one arrow), and make the diagram's link label a read-out only.

**I10-9. Medium: Airwindows still reads as a plug-in inside the rack.**
- What: BUILT-IN | AIRWINDOWS segmented control (clear), the AW card's ALGORITHM combo, and knob names that are the algorithm's own (INPUT, SOFTEN, HEAD BUMP, FLUTTER, OUTPUT) (FX sheet 2); "AIRWINDOWS (ALL)" is a third card kind with a large ALGORITHM combo and six knobs. In the empty-rack picker, "AW" boxes sit on 17 types and "AIRWINDOWS (ALL)" is a different, hidden place (`FxLibrary.h:138`).
- Fix: label the card's sub-title with the algorithm name ("Tape Hack 2") in the title; drop the ALL type from the picker for patches that do not use it; put units on every AW knob that has one.

**I10-10. Medium-low: "STRINGS" in the signal flow, "SYMPATHETIC STRINGS" in the tab, "BODY" and "SOUNDBOARD" on the PHYSICAL page, and "ACOUSTIC KEYS"** (`neuro/05-FILTER`, `neuro/03-OSC`, `neuro/physical-page`). The nodes in the flow do not match the tabs that edit them.
- Fix: one noun per module in the flow, the tab and the card header.

**I10-11. Medium-low: sample mode: "LOAD" and "LOAD .WAV".** The sample page's button is "LOAD" (`neuro/osc-sample-sfz`) and the wavetable page's is "LOAD .WAV" (`neuro/03-OSC`); "RESAMPLE" and "EDIT" in the same slot have different meaning (bounce vs wavetable editor). Fix: "LOAD..." on both; "RESAMPLE" to "BOUNCE" if that is what it does.

**I10-12. Medium-low: the PHYSICAL page's non-physical state contradicts itself.** The preview is dimmed under "PREVIEW · Physical oscillators only", the message below says "OSC 1 plays a wavetable. Above is the string it would get." (`keys/04-PHYSICAL`). Fix: one sentence ("OSC 1 plays a wavetable. Switch it to Physical to hear this string.") and drop the dimmed preview, or keep the preview and drop the header line.

**I10-13. Low-medium: four "KEYS" meanings and two "VOICE" ones** (I10-3). Fold into the vocabulary line.

**I10-14. Low-medium: macro warnings are still bare triangles** ("TALK ⚠" in the bottom bar, `neuro/scope-panel`; the explanation is a tooltip, I9-12). Fix: "TALK · OFF" in the warning colour, or a tooltip-free caption.

**I10-15. Low: seed and rate readouts: "Free" for SEED, "Auto" for EXCITE POS, "1/16" for a synced rate** (`neuro/lfo-sim-lorenz`, `neuro/physical-page`) are drawn like numbers in the value slot. Fine, but give them the same dim colour.

**I10-16. Low: a modal-less "Replace your edits?" appears only when loading; the A/B copy and CHAIN copy have no such guard** *(inferred)*. Settled as separate; no change.

## Score integration: 8.7 / 10

Up 0.4. The system is now recognisably one instrument: DX7 voices live in the normal oscillator, FM and MOD pages with their own sources only when needed; physical modelling has one editor; the MSEG is an LFO shape; the SEQ page chains its engines; voices, sample and clip features use the same widgets. The remaining integration cost is vocabulary and duplication (two level controls, two voice doors, four KEYS, three pickers) and a few modules that still need a special layout (Airwindows, FM extras).

---

# Part 4: Top 10 findings overall

1. **V10-1.** A broken character ("Â·") is printed on every wavetable oscillator view (`WaveDisplay.h:1008`).
2. **V10-2 / V10-3.** The dead space moved inside the cards (tall PLAY strips, stretched FX cards, empty FM cells), and the dead-area test cannot see it.
3. **V10-6 / V10-7.** The text fitter closes word gaps ("AMPENV", "PianoHammer") and still shrinks labels in one row to different sizes.
4. **V10-10.** The filter "OPEN" pill is a badge where a switch belongs; a filter still cannot be turned off.
5. **S10-1.** Voice settings have two doors with different names (OSC > VOICE and the header's nested menu).
6. **I10-1.** DX7 operators show OUTPUT (dB) and LEVEL (%) side by side on every page.
7. **V10-12.** The table browser's HEAR button and scroll bar are outside the theme (a call-out with no look and feel).
8. **V10-4.** The PATCH tile and OUTPUT view on PLAY cannot appear at the default window size *(inferred)*.
9. **I10-2 / I10-3.** One idea, several drawings or names: oscillator pickers, "go to editor" links, the randomiser (dice, SURPRISE ME, FX dice), SAVE (patch vs FX chain), KEYS.
10. **S10-2 / S10-3 / S10-4.** SEQ's switches plus "Off" combos, "UNISON" twice on the OSC page, and a bottom strip that mixes sub/noise, unison, body simulation and voice settings.

# Part 5: What stands between this build and 9.5

The shortest list (about two days of work; all are UI-only, no parameter changes except the filter switch, which is append-only):

1. **Fix the mojibake and add a test for it** (V10-1). 15 minutes.
2. **Stop stretching cards: cap oscillator strips at about 150 px, make thumbnails square, and give the surplus to graphs; same for FX cards (knobs beside the graph) and the FM matrix cells** (V10-2, V10-5, V10-13). Then add the card-interior emptiness test (V10-3). This lifts "full and balanced" from 8 to 9.
3. **One text-fit rule per row, and combos sized for their longest item; tracking limited to -0.04 em** (V10-6, V10-7, V10-8). This removes the remaining "squeezed text" look on FM, PHYSICAL and PLAY.
4. **Theme the table browser; fix the Save dialog's tag gap** (V10-12, V10-11).
5. **One voice panel (VOICE tab = header button), tabs regrouped; "Off" combos removed from SEQ's engines; UNISON tab renamed** (S10-1, S10-2, S10-3, S10-4).
6. **Give the filter a real switch (or remove the OPEN pill); fold off modules on OSC and PHYSICAL** (V10-9, V10-10).
7. **Two components to finish the "one vocabulary" job: an oscillator picker and an "EDIT ... ›" link; the vocabulary line for randomise, save and keys; one level on a DX7 operator** (I10-1, I10-2, I10-3).

With 1 to 6 done, vs Vital is about 9.1 and vs Serum 2 about 8.9; with 7 as well, integration is about 9.2. The last tenth against both is polish that only live use will find: drag feedback, hover cards, animation timing, and the pop-up browser's parity with the dock. A listening and clicking pass (not a screenshot pass) should come before the next review.
