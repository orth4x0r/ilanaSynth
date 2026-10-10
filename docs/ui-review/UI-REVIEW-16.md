# UI review 16 (2026-10-09 ART), main 78507fd through #29

Report: see thread 'do another review/report'. Shots were in the cloud box /home/user/shots16 (not kept). Measure: m.py here.

# Part A: PLAY, VECTOR, OSC, PHYSICAL (review 16)

Shots read at full size from /home/user/shots16. "Bare" is bare % from measure.tsv. Px are at 1.5x. Note: `neuro/vector-page` and `neuro/osc-*` (except sympathetic/bow) are really the Init patch (title bar says Init).

## 1. Page / state table

| page | bare R15 -> now | what the empty area is now | competitor | verdict |
|---|---|---|---|---|
| PLAY neuro/01-MAIN | 11 -> 14 | Cards are full. Dead: a 70 px band in OSC 1 and 2 cards between the dropdown row and knobs (init/01-MAIN: 85 px gap, OSC 1 card 295 px tall for 2 rows); SUB+NOISE card is cut at the bottom edge by the column scroll (neuro/01-MAIN y 940); MODULATION graph 380x270 px | Vital VOICE 17, Serum 2 main 5 | same as Vital (better on neuro, worse on init: 18 and an unused LFO 1 graph) |
| PLAY dx7/01-MAIN | n/a -> 16 | six OSC cards scroll; ENV graph + 6-bar list fill the right card, no dead block | Vital ADVANCED 18 | better |
| PLAY keys/01-MAIN | n/a -> 12 | PATCH node graph fills the left; MODULATION shows a full LFO 1 graph for an unused LFO (0 routes) | Serum 5 | same |
| PLAY added-osc-MAIN (6 osc) | n/a -> 12-13 | scroll column with OSC 4 and 5 cards cut by the viewport (y 940), 85 px gap inside each | Serum 2 | same |
| PLAY chrome (header, tab bar, chip row, macro strip) | n/a | header 52 px and tab bar 34 px full; macro strip right end has an empty 165x42 px meter well beside MASTER (neuro/01-MAIN x 1385-1550) | Vital's header is bare | same |
| VECTOR off, neuro/02-VECTOR | 42 -> 37 | pad is 725x725 px with a dimmed wave, a dot and tinted corners, but ~70 % of it is flat dark; corners pictures + knob row full | Serum 2 bare 5, Vital 17 | worse (37 vs 17) but better than R15 |
| VECTOR on, init/vector-page | n/a -> 35 | octagon path + wave across pad, 4 corner pics live; pad still 70 % flat dark | Vital 17 | worse |
| OSC neuro/03-OSC (2 wt + off) | 31 -> 3 | none; card rows full width. New: 138 px voice drawer is packed 3 rows | Serum 2 osc panel 5 | better |
| OSC added-osc-OSC / dx7 03-OSC | -> 2 / 12 | dx7: OP ENV graph now 225x105 px, controls beside it; 130 px of empty right on each row (x 1410-1550) | Vital 17 | better |
| OSC keys/03-OSC physical | n/a -> 4 | right 45 % of the OSC 1 card is a partials strip + a 330 px OUT bar, mostly dark (x 900-1550, y 195-335) | Serum 2 | better |
| OSC sample (osc-sample-empty / sfz) | n/a -> 17 | 1500x230 px drop zone (empty state OK), SAMPLE and PITCH&LEVEL rows spaced 190 px, two OSC cards below | Serum 2 sample panel | same |
| table-browser | 45 (browser) -> n/a | 1110x780 px crop shows full grid; 3 of 5 columns empty on the BASIC second row (x 650-1085, y 268-385) | Vital wavetable browser | same |
| PHYSICAL (non-physical osc) neuro/04-PHYSICAL | 46 -> 50 | 1450x210 px string preview with one thin line; 5 rows of 7 knobs + PARTIALS at right; BODY and SOUNDBOARD rows off. Reads as full but 50 % bare by measure | Serum swaps panel in place | worse |
| PHYSICAL live keys/physical-page | n/a -> 41 | string preview is a 1450x210 px dark box, EXCITER row of keys has 4 empty slots (x 700-1250, y 600-710), SOUNDBOARD row full | no equivalent | same |
| physical-feedback | n/a -> 39 | same page; EXCITER row full (feedback), string row 1 empty slot | | same |

## 2. Review-15 item status

- V15-6 half: neuro/02-VECTOR draws the dimmed wave, tinted corners and dot; bare 42 -> 37. Pad still 725 px square and 70 % flat dark.
- V15-7 half: neuro/04-PHYSICAL controls are no longer greyed ghosts and BODY/SOUNDBOARD live there, but the page is still 50 % bare and unchanged in height. The 'fold to one line' fix was not done.
- V15-9 fixed: neuro/03-OSC rows are 5-6 controls across full width, WARP CHAIN merged, bare 31 -> 3. (But see A16-1.)
- V15-10 fixed: dx7/03-OSC graph is a 225 px thumbnail with controls beside, bare 12.
- V15-11 half: keys/03-OSC picture is 225x130 px, STRING row present, but the EXCITER row is only the one dropdown (no HAMMER, EXCITE POS) and the right 45 % is a strip.
- V15-12 fixed: help sentence gone from the tab band (neuro/03-OSC).
- V15-14 open: neuro/01-MAIN and init/01-MAIN COLOUR is a grey ring at 100 % with bright value.
- V15-17 open and worse: init/01-MAIN MACRO 1 "+ ASSIGN", 2-4 "0 %"; neuro/vector-page "MACR..." clipped with a wave icon (tiles 1 and 3) while 2 and 4 show + ASSIGN; neuro/01-MAIN shows "-> 2".
- V15-18 half: dx7/01-MAIN and neuro/hover-line say "3 MORE" / "13 MORE"; the chip row still says "+13 v" (neuro/hover-line x 735-820).
- V15-19 fixed: neuro/01-MAIN OSC 2 subtitle is now 13 px sentence case "to output, modulates 1".
- S15-3 half: neuro/osc-sample-empty SAMPLE row is lit and no longer dimmed, but still 8 controls on 190 px spacing and START/END knobs duplicate the wave flags.
- S15-4 open: neuro/osc-sample-sfz has START/END flags and a key map but no loop flags and no velocity layers; LOOP is off so (inferred) loop markers do not show.
- S15-5 open by design: no change (neuro/03-OSC shows 3 tables as thumbnails, usable).
- I15-4 open: keys/03-OSC shows EXCITE, DECAY, DAMP; STIFF, REGISTER, DAMPER, COUPLING, SUSTAIN only on keys/physical-page.
- I15-5 half: sample has a dashed drop zone + LOAD (neuro/osc-sample-empty); PHYSICAL ghost is live-looking controls + SWITCH TO PHYSICAL; VECTOR dimmed; still different styles.
- I15-8 open: neuro/03-OSC still has STRINGS and SOUNDBOARD in the drawer and again on PHYSICAL (keys/physical-page).
- I15-9 open: keys/04-PHYSICAL "1 OFF" is orange text on FELT, no visible affordance (tooltip inferred absent).
- I15-10 open: neuro/02-VECTOR corner says "NOT ADDED", menu and pad "OSC 4: none".
- I15-12 open: neuro/03-OSC pill "FM FROM OSC 2 30 %"; neuro/01-MAIN OSC 2 "to output, modulates 1", no amount.

## 3. New findings (ranked)

- **A16-1 High, regression/bug**: on every wavetable OSC card the right cluster collides. The "OUT" level bar (a 330x8 px coloured bar + vertical tick) is drawn over the SCALE dropdown and the "WRP SPR 0%" label, and the FM pill ("FM FROM OSC 2 30 %") sits on top of the "SCALE / Off" label. Evidence: neuro/03-OSC OSC 1-3 (x 1150-1550, y 255-285 and 460-490), neuro/osc-pd-chain, init/added-osc-OSC all four cards. Fix: give the OUT bar and the FM pill their own 24 px row (or put the FM pill in the card header next to the type dropdown) and keep SCALE/ROOT clean.
- **A16-2 Medium, clipped text**: truncated strings. neuro/hover-line hint "OSC 1 > Semi  Pitch offset in semitones. With Hard Sync this set..." runs into MASTER at y 1042; init/01-MAIN MODULATION overlay "unused: drag it onto a knob to u..." (x 897-1090, y 597); dx7/01-MAIN chip "PRESSUR" and a clipped "MOD WHEEL" badge (x 1120-1370, y 988); neuro/vector-page macro "MACR...". Fix: ellipsise to a tooltip, or shorten to "PRESSURE" via a narrower badge and "unused" only.
- **A16-3 Medium, dead space**: PLAY OSC 1 card has a 70-85 px empty band between the dropdown row and the knobs, and on init/keys the card is 295 px for one row (init/01-MAIN y 245-290; keys/01-MAIN y 245-270). Fix: align the knob row right under the dropdowns and give 60 px to a 2-line mini-ENV or voices display, or shrink the card so OSC 2 / SUB+NOISE do not need the scroll.
- **A16-4 Medium, dead space**: PHYSICAL page, both states, keeps a 1450x210 px preview with one 3 px line; the EXCITER row has 4 empty slots on the hammer excite (keys/physical-page x 700-1250, y 600-710). Fix: hide empty slots (dock EXCITER to 3 knobs + the partials) and grow PARTIALS to fill; for non-physical oscillators show the static preview at 140 px.
- **A16-5 Medium, dead space**: VECTOR pad is 70 % flat dark. neuro/02-VECTOR. Fix: tint the quadrants stronger and draw all 4 corner waves faded, or cut the pad to 560 px.
- **A16-6 Medium, inconsistency**: PLAY shows a full LFO 1 graph with "0 ROUTES" for an unused LFO and a SINE shape (init/01-MAIN, keys/01-MAIN) that looks live. Fix: show the hint card for an unrouted source, as the overlay says, but at the graph size and dim the rest.
- **A16-7 Low-medium, dead space**: the macro strip's meter well right of MASTER is a 165x42 px empty box on every page (neuro/01-MAIN x 1385-1550, y 1020-1062). Fix: draw the output meter, or remove.
- **A16-8 Low, inconsistency**: row labels on PHYSICAL (EXCITE POS, MATERIAL etc.) sit 8 px under the previous row's value text and read as part of it (neuro/04-PHYSICAL y 600-620 and 710-730). Fix: add 12 px row padding.
- **A16-9 Low**: the OSC voice drawer has "STRINGS" / "SOUNDBOARD" at 10 px in the same row as VOICE, with SUB underneath; a 3-row, 140 px strip with no card title (neuro/03-OSC y 810-945). Fix: group labels as cards.

## 4. Sub-score for scope

- vs Vital: 9.0 / 10. The OSC page is now clearly denser than Vital's, but the OUT-bar collision (A16-1), PHYSICAL and VECTOR bare numbers (37 to 50 vs Vital 17) and clipped strings keep it from 9.3.
- vs Serum 2: 8.8 / 10. The OSC card is better than Serum's, PLAY is close in density; VECTOR/PHYSICAL page empties are still worse than Serum's 5 bare main page.

# Review 16, part B: FILTER, MOD, MATRIX, FM
Evidence: /home/user/shots16/{neuro,init,dx7,keys}, measure.tsv (bare %), refshots Vital/Serum 2. px at 1.5x.

## 1. Page / state table
| page | bare R15 -> now | what the empty area is now | competitor | verdict |
|---|---|---|---|---|
| FILTER, Neuro (neuro/05-FILTER) | 31 -> 24 | RESPONSE is 1540 x 270 holding one curve and a flat floor; SIGNAL FLOW strip 1540 x 60 with a ~450 px dead run (hint + greyed BALANCE); WEST and BODY open at 190 px with all knobs dimmed when off (2 x 770 x 190 of grey) | Vital filter 350 x 180 + 6 knobs, VOICE bare 17 | worse than Vital (24 vs 17), improved |
| FILTER, WEST + BODY on (neuro/filter-west, filter-flow-busy) | n/a -> 20 / 21 | Same geometry, the page no longer jumps; response is 1540 x 270 with a dashed WEST line | same | same as Vital (bare 20) |
| FILTER, DX7 (dx7/05-FILTER) | n/a -> 30 | Flat 1540 x 270 line for a filter that is off, 50 % empty | Vital n/a | worse (and says nothing, see I15-2) |
| FILTER, Init / Vowel (init/filter-models-2) | -> 30 | Vowel model drops the 12/24 dB pills, leaving a 150 px blank in F1's header row; curve fills the graph | Vital shows a model-specific display | same |
| MOD ENV/LFO, Neuro (neuro/06-ENV-LFO) | 30 -> 24 | LFO graph 800 x 340 holds one triangle line; DRIVES well 700 x 140 is half used; ENV graph 610 x 150 fine | Vital LFO 550 x 190 with controls beside | worse than Vital (24 vs 17), improved |
| MOD, MSEG / steps (neuro/lfo-mseg, lfo-steps) | n/a -> 8-9 | Steps bars fill the graph | Vital 12-step LFO | better |
| MOD, physics LFO (neuro/lfo-sim-lorenz, lfo-physics-12..15) | n/a -> 14-29 | Lorenz card fills (14 to 20); pendulum/spring/Henon pages are 27-29: 800 x 340 graph with a single faint line, 400 x 150 blank right of SWING/DAMP | Serum 2 Lorenz display | mixed; Lorenz better, pendulum worse |
| MOD, DX7 OP LFO (dx7/06-ENV-LFO-env3) | -> 25 | "ON THE OPERATORS" band has ~80 px blank above the knob row; sine graph 720 x 360 | Dexed has no such page | same |
| MATRIX 12 routes (neuro/07-MATRIX) | 4 -> 7 | VIA column 60 px of 12 dashed "+" (S15-2), AMOUNT track 300 px | Vital MATRIX 22 | better |
| MATRIX 5 routes + remap (keys/07-MATRIX) | 33 -> 37 | 5 route rows end at y 470; remap right column 680 x 400 is prose, ~100 px gap above QUICK SHAPES, curve 820 x 340 | Vital 22 | worse (37 vs 22) |
| MATRIX 0 routes (init/07-MATRIX) | n/a -> 77 | 840 x 375 intro card + two chip rows centred; ~480 px empty each side and a 160 px band above | Vital shows empty rows | worse, but a one-off empty state |
| FM 1 osc (init/08-FM, keys/08-FM) | 46 -> 44 | Two 170 x 240 px cells with one 12 px dot each (V15-3 unchanged); algorithm canvas 920 x 310 with two OFF discs and a 30 px sentence; matrix column 640 x 480 | Serum FM is a knob; Vital matrix rows 37 px | worse (44 vs 17) |
| FM 2 osc (neuro/08-FM) | 35 -> 39 | 5 of 6 matrix cells are 170 x 155 px with a dot; OSC 3 OFF disc | same | worse than R15 |
| FM 6 op DX7 (dx7/08-FM) | 10 -> 11 | Full | Dexed / Serum 2 | better |
| FM no-input (neuro/fm-no-input) | n/a -> 45 | Same 1-osc layout with a 40 % arrow ghost | n/a | worse |

## 2. Review-15 items
- V15-1 FILTER/MOD part: **half**. Bare 31 to 24 and 30 to 24, but the RESPONSE is still 1540 x 270 for one curve (neuro/05-FILTER) and the LFO graph 800 x 340 (neuro/06-ENV-LFO); the knob rows were not enlarged (55 px knobs).
- V15-3: **open** (init/08-FM, keys/08-FM: 170 x 240 slabs; neuro/08-FM now 170 x 155; no "ADD OSC 2" line, no square cap).
- V15-8: **fixed**. WEST/BODY open at fixed 190 px in both states, graph 270 px in all of neuro/05-FILTER, filter-west, filter-flow-busy.
- V15-13: **half**. Quick shapes moved to the bottom row and are bigger, but the column still restates "ROW 1 / FELT / -> FILTER 1 > Cutoff" under a title strip that already says it (keys/07-MATRIX).
- V15-16: **fixed**. LFO and ENV tiles are both 70 px; the "+" tile matches (neuro/06-ENV-LFO).
- S15-2: **open**. VIA still 12 of 12 empty (neuro/07-MATRIX, keys/07-MATRIX).
- S15-6: **half**. Physics card now has a DRIVES box and OUT chips, but it holds only "+3 more (MATRIX)" (neuro/lfo-sim-lorenz), the label is still "OUT 1 AXIS" and SIGMA/RHO share the RATE row.
- S15-7: **fixed**. Both pool rows use the same outlined pill (neuro/06-ENV-LFO).
- S15-8: **accepted** (no change wanted).
- I15-1: **half**. On the FM page DX7 cards say OUTPUT and the matrix hint says "OUTPUT" (dx7/08-FM); PLAY and OSC pages are not in my scope, not re-checked.
- I15-2: **open**. dx7/05-FILTER still shows a normal graph and live F1 knobs; no "FILTER OFF" banner.
- I15-3: **half**. Chip "OUT 2" now also appears on the LFO tile and the mini chip bar (neuro/lfo-sim-lorenz), the DRIVES list is partial.
- I15-6: **fixed**. WEST/BODY are full cards with a header switch like FILTER 2 (neuro/05-FILTER); the 60 px strip is gone.
- I15-11: **half**. The FM page still carries the two-line footer "Wavetable: tuned in semitones. Ratio and OP ENV are FM / DX7's." under OSC 1 plus a SWITCH TO FM / DX7 button (neuro/08-FM, init/08-FM); the sentence is no longer a full-width strip.

## 3. New findings (ranked)
1. **N16-1, High.** FM matrix and algorithm canvas waste half the page on 1-2 osc patches: 170 x 240 slabs with a dot, 920 x 310 canvas with two "OFF" discs and a 30 px sentence, 640 x 480 right column. Evidence init/08-FM, keys/08-FM, neuro/fm-no-input (bare 44-45 %), neuro/08-FM (39 %). Fix: cap cells to ~100 px squares, collapse the canvas to 170 px for one osc, and put the freed 300 px under the matrix into OSC 1's envelope/knobs (grow the 380 px EG).
2. **N16-2, Medium-high.** DX7 I15-2 and the flat FILTER graph: a 1540 x 270 empty response for a filter that is off. Evidence dx7/05-FILTER. Fix: banner "FILTER OFF - a DX7 voice has none", dim F1/F2 knob rows, and use the 270 px for the op-spectrum.
3. **N16-3, Medium.** DX7 FM page OSC 1 ENVELOPE dropdown is blank (empty combo box) while the OP ENV graph is drawn. Evidence dx7/08-FM. Fix: show "OP ENV" as the selected item.
4. **N16-4, Medium.** FILTER page response (1540 x 270) and LFO page graph (800 x 340) are still sized by the page, not the data; mean FILTER bare 24-30 against Vital 17. Evidence neuro/05-FILTER, neuro/06-ENV-LFO, dx7/05-FILTER. Fix: response 1000 px wide, put a second row of F1/F2 info (slope, formant list) at the right, give the LFO editor's blank bottom-right (700 x 140) a second row of LFO knobs.
5. **N16-5, Medium.** Matrix empty state (init/07-MATRIX, bare 77 %) shows one 840 x 375 card; compared with Vital's always-filled rows it is the emptiest page in the build. Fix: show the 12 starter rows as dimmed ghost rows and put the six "start from" chips inline; (inferred: the empty state is shown only on a fresh patch).
6. **N16-6, Medium-low.** Clipped text: "PER VOI..." toggle label (neuro/lfo-sim-lorenz), "FM > OSC 2 > OS..." in DRIVES on the pendulum card (neuro/lfo-physics-13). Fix: widen the toggle cell or rename to "VOICE"; drop DRIVES bars to a 60 px column so the names fit (or enlarge the card).
7. **N16-7, Medium-low.** Physics LFO pages have a lot of unused card (pendulum, spring, Henon bare 27-29 %): a 400 x 150 blank right of SWING/DAMP and a graph holding one faint line. Evidence neuro/lfo-physics-13. Fix: let the DRIVES list span the full card width and show the phase portrait as in Lorenz.
8. **N16-8, Low.** WEST and BODY keep a full 190 px of dimmed knobs when off (neuro/05-FILTER); geometry is stable, but 770 x 190 x 2 of grey is the largest static dead area on the page. Fix: dim the knobs less and put the DRIVE / pre-post options into the free grid column.
9. **N16-9, Low.** Remap's right column leaves ~100 px blank above QUICK SHAPES and the restated route (V15-13). Evidence keys/07-MATRIX. Fix: replace the restatement with a live readout of the amount and a tiny in/out table.
10. **N16-10, Low.** Capture bug: dx7/08-FM-env2..env5 show the MOD page, not FM; the ENV-LFO states are not captured on FM, so FM EG states are unreviewed. Fix: the shot script should switch to FM first.
11. **N16-11, Low.** OP LFO page: "ON THE OPERATORS" has an 80 px blank over the knob row (dx7/06-ENV-LFO-env3). Fix: centre the row or move RATE/DELAY up.

## 4. Sub-score for this scope
- vs Vital: **8.9 / 10**. FILTER and MOD improved 6 to 7 points of bare and V15-8 jump is gone, but FM 1-2 osc (44 %) and FILTER/MOD 24 % are still above Vital's 17.
- vs Serum 2: **8.7 / 10**. Serum's panels are denser (22 / 5) and its Lorenz display carries the same row as every LFO; our physics LFOs still have their own controls and a partial DRIVES list.

# Review 16, part C: SEQ, FX, preset browser, SCOPE panel, tutorial

Shots: /home/user/shots16/{neuro,init,dx7,keys}. Bare = measure.tsv "bare %". Px are 1.5x screenshot px. (inferred) = not seen in a shot.
Note: the task text says 15 items but names 11 (V15-1 ARP part, V15-2, V15-4, V15-5, V15-15, S15-1, S15-9, S15-10, S15-11, I15-7, I15-13); all 11 are covered.

## 1. Page / state table

| page | bare R15 -> now | what the empty area is now | competitor | verdict |
|---|---|---|---|---|
| SEQ ARP, ARP off (neuro/09, dx7/09, init/09, keys/09 identical) | 41 -> 45 (empty 64) | Three ghost lanes 1000 x 170 px each, all dark, one "DRAW A STEP" pill in the middle; control column x1190-1570 has 70 px blank above RATE and 70 px below CHANCE, the 380 x 190 px strip above RATE is empty | Vital has no sequencer page; Vital ADVANCED 37/18 | worse than benchmark (17) and worse than R15 |
| SEQ ARP, ARP on (init/gen-arp-lanes) | n/a -> 23 | Lanes filled with data; right column same 70 px floors | - | same |
| SEQ EUCLID (gen-euclid) | n/a -> 40 | Under the step bars 1150 x 150 px dark, ring leaves 4 dark corners of 120 px; right column floats | - | worse (40 vs 17) |
| SEQ PROB SEQ (gen-probseq) | n/a -> 21 | Right column 330 x 220 px under GATE | - | same |
| SEQ CLIP (gen-clip 13, clip-selected 12, expanded 20) | n/a -> 12-20 | Almost none; GENERATE band folds to a 50 px strip when expanded | Serum 2 has no piano roll in the synth page | better |
| SEQ GENERATE band (all gen-*) | - | Three cards of 440 x 190 px with 70 px floor under the knobs when off | - | same |
| FX rack, 3 cards (neuro/10-FX) | 28 -> 29 (empty 54) | Dashed ADD EFFECT hole 1490 x 230 px; OTT graph 845 x 105 px for one straight line | Vital EFFECTS 30/12 | worse (29 vs 12) |
| FX rack, 2 cards (keys/10-FX) | n/a -> 42 | Hole 1490 x 350 px; LIMITER graph 670 px wide, one diagonal | Vital 30/12 | worse |
| FX rack, 1 card (dx7/10-FX) | 52 -> 58 (empty 70) | Hole 1490 x 500 px below a 155 px card, bigger than R15's 270 px floor because the card shrank | Vital rack always full | worse and regressed |
| FX empty (fx-empty, init/10-FX) | n/a -> 26 | Picker fills the page; 70 px floor under the Airwindows legend, last column ("VOICE") 1260 x 250 px short | Vital shows all modules | same |
| fx-split | -> 8 | none worth noting | Vital none | better |
| fx-midside | -> 17 | hole 1490 x 130 px | - | same |
| fx-duplicate | -> 9 | Duplicate OTT card is a sentence plus REMOVE, 1100 x 60 px mostly text | - | same |
| 41 single-effect shots (neuro/fx-01..41, sampled 22 incl. 01,02,05,08,09,12,16,18,20,26,35 in full; all 41 in a montage) | 28 -> 26-36 (empty 49-57; dx7/init 68-74 empty, 56-63 bare) | Always a 230-500 px dashed hole plus, on 16 of 41, a "WHAT IT DOES" well | Vital 33/12 | worse |
| Preset browser (all sets, 641 presets) | 45 -> 46 | Detail panel 365 x 480 px empty under 3 tag chips; list shows 12 rows | Serum 2 browser has description, author, audition | worse; no audition |
| SCOPE panel playing (neuro/scope-panel) | n/a -> 59 | Real graph; bare number is traces on black, honest | Serum 2 has an oscilloscope; Vital a small one | better |
| SCOPE panel idle (dx7, init, keys) | n/a -> 67 / 59 / 47 | Flat line plus nothing: 1360 x 330 px wave pane and 1360 x 300 px spectrum pane are empty until a note plays | - | worse (idle) |
| Tutorial (00-tutorial) | n/a -> 42 | Card 1080 x 530 px; text column ends x1090, 250 px dead at right; 70 px between last bullet and shortcut row | - | same |

## 2. Review 15 items

- V15-1 (SEQ ARP part): **regressed**. neuro/09-ARP-SEQ still three 1000 x 170 px lanes (bare 41 -> 45); the "fold to one lane" fix was not made, a ghost grid and a pill were added instead.
- V15-2: **half**. OTT graph 1170 -> 845 px (neuro/10-FX), LIMITER 1040 -> 670 px (keys/10-FX), REVERB 540 x 260 -> 500 x 130 px (dx7/10-FX); each still carries a straight line, and the freed width is just dark.
- V15-4: **regressed**. dx7/10-FX hole is 1490 x 500 px (R15: 270 px floor); the add-effect box stretches but holds one word.
- V15-5: **open**. 16 of 41 cards (HAAS fx-08, TILT fx-16, SMEAR, DIMENSION, TAPE STOP, UTILITY, FLANGER, STUTTER, WIDENER, TREMOLO, FREQ SHIFT, RING MOD, OCTAVER fx-26, FEEDBACK, AW CHORUS, AW WIDENER) show the "WHAT IT DOES" well, 290-1000 px wide with 1 line in it.
- V15-15: **open**. neuro/09: control column still ends 70 px above the panel floor and begins 70 px below its top.
- S15-1: **open**. No audition control anywhere in PresetPanel.h (grep: no "audition"/"preview"); detail panel 365 x 480 px empty (neuro/preset-browser).
- S15-9: **fixed (inferred)**. SavePresetOverlay.h:156-162 opens chips first, the text field only behind "type your own"; no shot of SAVE AS exists in shots16.
- S15-10: **half**. Bottom row RANDOM/SAVE AS/DELETE/FOLDER/IMPORT all 40 px; DOCK top-right is 28 px (neuro/preset-browser); DELETE still dim with no reason shown (tooltip inferred).
- S15-11: **open**. "DX7 270" is still the first chip, in the same row as the tags, before "Evolving" (neuro/preset-browser).
- I15-7: **fixed**. Every card shows its BUILT-IN or AIRWINDOWS pill at the same x 224-349 under the name (neuro/10-FX, fx-35).
- I15-13: **open**. Help text still in a well (fx-08), a header caption (fx-empty), a footer legend (fx-empty "AIRWINDOWS beside an effect...") and "after the rack..." in OUTPUT.

## 3. New findings (ranked)

- **N16-1 High.** FX rack ends in a dashed ADD EFFECT hole on every state with fewer than 5 cards: 230 px at 3 cards, 350 px at 2, 500 px at 1. Evidence: neuro/10-FX, keys/10-FX, dx7/10-FX, all fx-NN. Fix: put a "SUGGESTED NEXT" row of 3-5 compact effect cards (REVERB, DELAY, CHORUS, EQ) inside the hole, and let the 1-card rack grow its graph to 300 px height.
- **N16-2 High.** SEQ ARP-off page is 45 % bare, worse than R15, because the empty lanes are drawn at full height. Evidence: neuro/09-ARP-SEQ, dx7/09-ARP-SEQ. Fix: while ARP is off draw one 170 px VEL lane with the DRAW pill, fold STEP LEN and PITCH to 30 px headers, and give the freed 330 px to the control column (larger knobs, or an 8-note held-chord preview).
- **N16-3 Medium-high.** 16 of 41 effects are a sentence in a well; the picture the review asked for was not drawn. Evidence: neuro/fx-08, fx-16, fx-26, montage of fx-01..41. Fix: draw tiny pictures (Haas two offset pulses, Tilt seesaw, Tremolo/Flanger the LFO shape already drawn for Chorus fx-04, Octaver two stacked waves) and move the sentence to the title tooltip.
- **N16-4 Medium.** The FX toolbar and cards run to the window edge: RANDOMISE FX ends at x 1590 and cards at x 1588, while SEQ, PLAY and the rail keep a 20 px gutter (panel ends x 1570). Evidence: neuro/10-FX, fx-empty, keys/10-FX vs neuro/09-ARP-SEQ. Fix: 20 px right gutter on the FX page.
- **N16-5 Medium.** The picker allows adding an effect that cannot work: a duplicate card is a 1100 px sentence ("shares slot 1's settings") plus REMOVE, and in fx-duplicate the scrolled rack leaves the top card cut off under the toolbar with no fade. Evidence: neuro/fx-02, fx-20, fx-duplicate. Fix: grey the picker entry of an effect already in the chain with "already in chain: slot 1", and fade the clipped card edge.
- **N16-6 Medium.** Preset browser detail panel is 480 px of floor and there is no way to hear a preset. Evidence: neuro/preset-browser, dx7/preset-browser. Fix: PLAY C3 button plus comment text and macro names in the panel (as S15-1).
- **N16-7 Medium-low.** Idle SCOPE is two empty panes (67 % bare on dx7). Evidence: dx7/scope-panel, init/scope-panel. Fix: draw "play a note" ghost trace and spectrum silhouette, or auto-fall back to the last 2 s of the master hold buffer.
- **N16-8 Low.** Euclid page leaves 1150 x 150 px under the step bars and 4 dark corners around the ring. Evidence: neuro/gen-euclid. Fix: use the strip for a per-hit velocity row.
- **N16-9 Low.** Tutorial text column stops 250 px short of the card edge and the dimmed UI behind is cut mid-label ("EY TRK", "gle"). Evidence: neuro/00-tutorial. Fix: widen the text column to the card width, put "WHAT'S NEW" on the right.
- **N16-10 Low.** AIRWINDOWS EQ response is a flat line in a 190 px well, AIRWINDOWS DELAY echoes one spike (feedback 0). Evidence: neuro/fx-31, fx-35. Fix: show an octave grid and label the one value, or fold the graph.

## 4. Workflow speed (clicks; mark (inferred) where not seen)

| task | clicks | notes |
|---|---|---|
| Audition 10 presets | 1 (name opens browser) + 10 Up-arrow keys (title bar says "Up/Down browse") + 10 note presses (inferred, no audition button) | Faster than Serum 2 only by keys; per preset 2 inputs, same as Vital. Header arrows (1 click each, neuro/01-MAIN) also work (inferred). |
| Add a reverb | 3: FX tab, ADD EFFECT, REVERB (2 when the rack is empty: picker is the page, init/10-FX) | Faster than Vital (drag a module on) |
| Add LFO to cutoff | 1 drag: LFO 1 chip (bottom bar or MODULATION card) onto CUTOFF (neuro/01-MAIN text "drag a tab onto any knob"); amount set by the next drag on the knob (inferred) | Matches Vital, faster than Serum 2's matrix |
| Make a wobble bass | about 6 actions on Init: drag LFO 1 to cutoff (1), set cutoff lower (1), reso up (1), LFO rate/sync (2), pick shape (1) (inferred; no wobble shot) | PLAY page has all of it on one screen; best of the three |

## 5. Sub-score for this scope

- vs Vital: **8.5 / 10**. The FX rack has more effects than Vital with a graph on 25 of 41, split-band and mid/side rows are better than Vital; but 29 vs 12 bare on FX, a 45 % bare ARP page and a 480 px empty detail panel keep it below parity on the space goal.
- vs Serum 2: **8.3 / 10**. Serum 2's browser has audition and description, and its FX modules are always filled; here the browser has neither and a one-card rack is 58 % bare.
