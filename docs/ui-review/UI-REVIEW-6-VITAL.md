# UI review 6: ilanaSynth against Vital (fresh, adversarial)

Reviewer: a fresh agent. I read review 5 only to check what is still open. Evidence comes from offscreen shots in `/home/user/shots6/` (1060x720 logical at 1.5x; `small` is 75% zoom), plus new shots I rendered into `/home/user/shots6/vital-extra/`: `piano2/` is DX7 "PIANO 2 (ROM1A)", which uses DX7 algorithm 18, and `dx7-small/` is E.PIANO 1 at 75%. I also read the source in `src/gui/` and `src/PluginEditor.cpp`. The Vital comparisons come from what I know of its UI; I did not run it. Anything marked **(inferred)** was not confirmed in a shot or in code.

**Evidence caveat.** On the OSC page, the snapshot tool switches OSC 1 to Physical, sets the bow, buzz and rattle, turns Sympathetic Strings on, and then sets OSC 1 back to *Wavetable* (`tools/Snapshot.cpp:4243-4277`). That is why every shot after `03-OSC` shows EDITED. It is also why `keys/04-PHYSICAL.png` says "OSC 1 plays a wavetable" for a physical preset. I do not count either of these as UI bugs.

## Verdict

The DX7 fold-in is the most "bolted on" part of the synth. A DX7 voice now loads as a normal patch, but nearly every screen still treats it as a guest. PLAY shows six folded "WAVETABLE" cards and an AMP ENV the operators don't use. The real envelopes (the Operator EG) live on the FM page only. They use DX vocabulary (R1-R4, L1-L4, 0-99, BREAK A-1, -LIN), their graph can't be dragged, and they are not modulation sources. The voice's pitch EG and LFO add a fourth kind of LFO behind a tab. The algorithm grid knows 7 of the 32 DX7 algorithms, so most imports read "CUSTOM - matches no algorithm here". Worst of all, the six-operator diagram collapses into a strip on DX7 voices, and OSC 1 and OSC 2 are drawn hidden *behind* OSC 5 and OSC 4.

Outside DX7, most of review 5 is still open: depth dots instead of rings, the 1-16 index rows that don't line up with their cards, the folded SUB + NOISE card with labels drawn over it, Lorenz values over knobs, and the stale Steps binding. The FX rack is the one area that clearly got better. Polish is still uneven: text collides in at least five places.

## Findings, worst first

### 1. Critical: the six-operator FM diagram hides OSC 1 and OSC 2 behind other operators
**What's wrong.** On any patch where an operator uses the Operator EG, the operator card takes up to 82% of the left column (`pages/FmInputPages.h:575-577`). The diagram is left with a strip about 85 px tall. `FmDiagram::operatorCentres()` puts six nodes on an ellipse sized to that strip (`FmDiagram.h:426-438`). When the height collapses, OSC 1 lands on the same x as OSC 5 and OSC 2 on the same x as OSC 4, so the carrier pair OSC 2 → OSC 1 is drawn *under* other nodes. In E.PIANO 1, you see OSC 6 → OSC 5 and OSC 4 → OSC 3, plus a long blue arrow that seems to come from OSC 4 into OSC 5 (it is really OSC 2 → OSC 1). OSC 1, the main carrier, is invisible. The "ratio / level" captions under each node are cut off by the strip's bottom edge, and the OSC 6 feedback loop is clipped on the left. PIANO 2 is worse: no OUT node is visible at all.
**Vital instead.** Not applicable (Vital has no operator view). Dexed and FM8 always draw the full algorithm, with carriers on the bottom row.
**Fix.** Give the diagram a minimum height of about 150 px, or put the EG in a tab beside it. With 4 or more operators, lay them out as DX stacks (carriers on the bottom row, modulators above them, by routing depth) instead of a ring. Add a UI test that fails if two node circles overlap.
**Evidence:** `dx7/09-FM.png`, `dx7/09-FM-pitch-lfo.png`, `vital-extra/piano2/09-FM.png`, `vital-extra/dx7-small/09-FM.png`.

### 2. Critical: modulation depth is still dots, not rings (still open from review 5, #1)
**What's wrong.** Routed knobs still get a column of 10 px pie dots (`ParamControls.h:281`). Three sources stack beside CUTOFF and FRAME and push into the next knob (`neuro/01-MAIN.png`, `neuro/05-FILTER.png`). The FM matrix cell OSC 2 → OSC 1 carries two dots inside a 60 px cell (`neuro/09-FM.png`).
**Vital instead.** A coloured range arc on the knob for each routing, which you can drag.
**Fix.** As in review 5: draw one concentric arc per routing, which can be dragged, and keep the dots only as a hover legend.

### 3. High: a DX7 voice's PLAY page doesn't show FM and shows an envelope the voice ignores
**What's wrong.** E.PIANO 1 and PIANO 2 open on PLAY with six cards that read "WAVETABLE - folded to fit - click to open". OSC 1 is shown as a Wavetable playing "Sine". Nothing on PLAY says this is an FM patch, which operators are carriers, or what the algorithm is. The ENVELOPE card shows AMP ENV (5 ms / 300 ms / 80% / 250 ms) with the caption "drag the graph or the knobs". Yet every oscillator is on "Op EG", and the voice's gain and lifetime come from the Operator EG (`dsp/Voice.cpp:1612-1613, 1908-1916`). Dragging AMP ENV most likely does nothing audible **(inferred from the code path; not listened to)**. MOD › ENV/LFO also tags AMP ENV as "Amp" on DX7 voices (`dx7/06-ENV-LFO.png`).
**Vital instead.** What PLAY (Vital's Voice page) shows is what plays.
**Fix.** When every shown oscillator uses Op EG, replace PLAY's ENVELOPE card with the selected operator's Op EG graph, and add a compact algorithm and operator row in place of the six folded wavetable cards. Grey out AMP ENV with "not used: the operators play their Operator EG". Show "FM OPERATOR · x1.000" in the folded card text instead of "WAVETABLE".
**Evidence:** `dx7/01-MAIN.png`, `vital-extra/piano2/01-MAIN.png`, `dx7/06-ENV-LFO.png`.

### 4. High: the Operator EG is a second envelope system with its own vocabulary
**What's wrong.** Every other envelope in the synth is a draggable DAHDSR in ms and %, with a card in the MOD pool and a source chip. The Operator EG is:
- edited only on FM › "OSC n AS AN OPERATOR";
- set with 17 controls labelled R1 R2 R3 R4 L1 L2 L3 L4 OUTPUT BREAK L DEPTH R DEPTH L CURVE R CURVE RATE KEY VEL AMS, all reading raw 0-99 or DX codes ("A-1", "-LIN") (`pages/FmInputPages.h:184-186`);
- drawn as a graph with no mouse handling (`OperatorEgGraph`, `pages/FmInputPages.h:10-40`; there is no `mouseDrag` in the file), no time ruler, and a cryptic "KEY UP 2.5 / 2.6 s" corner label;
- absent from the envelope pool, the chip row and the matrix sources.

The same setting is labelled "ENVELOPE" on the FM page and "AMP ENV" on the OSC page (`dx7/03-OSC.png` vs `dx7/09-FM.png`).
**Vital instead.** One envelope model everywhere. Each envelope is draggable and doubles as a mod source.
**Fix.** Show R1-R4 and L1-L4 as draggable breakpoints on the graph, with times in ms and seconds (keep the 0-99 value in the tooltip for DX users). Add a time ruler. Put "OP EG" cards into the MOD envelope pool (one per operator that uses it, or one shared card with operator tabs). Call the setting "ENVELOPE" everywhere. Spell out the scaling labels ("BREAK POINT A-1", "LEFT CURVE -LIN", "RATE KEY SCALING", "AMP MOD SENS") and use ± and units.

### 5. High: the DX pitch EG and LFO are a fourth kind of LFO hidden behind a tab
**What's wrong.** "PITCH / LFO" appears as the seventh pill in the *operator* tab row, but it holds voice-wide settings ("OPERATOR EG PITCH AND LFO - every oscillator on the Operator EG"). It appears only while some operator uses Op EG (`pages/FmInputPages.h:633`). It contains an LFO (SPEED 34, DELAY 33, PITCH DEPTH, AMP DEPTH, KEY SYNC, WAVE) in 0-99 units, while every other LFO reads Hz or note divisions and lives in the MOD › LFO pool. The pitch graph reads "PITCH +/-3.0 st", in ASCII. The synth now has shape LFOs, Steps rows, MSEG and this LFO, each edited in a different place.
**Vital instead.** All LFOs live in one place and look alike.
**Fix.** Move the DX LFO into the LFO pool as an "LFO (DX)" card with its rate in Hz, and show its depth routings as matrix rows. Put the pitch EG in the envelope pool. Take PITCH / LFO out of the operator pill row (use a separate "VOICE EG" heading). Use "±".
**Evidence:** `dx7/09-FM-pitch-lfo.png`.

### 6. High: the algorithm grid knows 7 of 32 DX7 algorithms, and its numbers mean nothing
**What's wrong.** The 16 tiles are 9 house routings plus 7 DX7 algorithms (1, 5, 7, 16, 19, 22, 32) (`dsp/FmAlgorithms.h:28-44`). E.PIANO 1 (DX7 alg 5) highlights tile **11**, and the heading says "DX 5 Keys", which reads like a Yamaha DX5 model. PIANO 2 (alg 18) shows "CUSTOM - matches no algorithm here" with no tile lit (`vital-extra/piano2/09-FM.png`). Of ROM1A's 32 voices, 21 use an algorithm with no tile. Tiles carry only a number and no name, so the name appears only after you click. Review 5's "DX7 ALG n" label was dropped (commit 1d306f6), so a DX user now gets less information than before.
**Fix.** Add a "DX7 1-32" page or drop-down to the grid (the routings are a fixed table). Label each tile with its name on hover and under the selected one. Rename "DX 5 Keys" to "DX7 #5 (three pairs)". When a patch matches a DX7 algorithm, always say so.

### 7. High: SUB + NOISE folded-to-fit still draws its controls' labels over the header (still open, review 5 #5)
**What's wrong.** After oscillators are added, the card reads "SUB + NOISE  folded to fit - click to open". On top of that, and below it, sit "SHAPE  OCTAVE  SUB LEVEL  NOISE" plus a macro badge. The toggle sits at the top of the strip while the title is centred.
**Cause.** `layoutSubCard()` hides children and places the switch from `subFolded` only. It ignores `subAutoFolded` (`pages/MainPage.h:904, 909, 911`), while the paint uses `subFolded || subAutoFolded` (`:165`).
**Fix.** Use `subFolded || subAutoFolded` in all three places. Add a uitest for "no visible child inside a folded card".
**Evidence:** `neuro/added-osc-MAIN.png`.

### 8. High: the 1-16 index rows still don't line up with the cards (still open, review 5 #3)
**What's wrong.** `PoolIndexRow::cellBounds()` splits the full width into 16 equal cells (`PoolIndexRow.h:58-62`), while the pool shows 3-4 cards that are each 4 cells wide. "2" sits over the right half of card 1, and "5" to "16" sit over empty space or over the "+" card. In `neuro/lfo-sim-lorenz.png` the envelope pool also shows a stray horizontal scrollbar under the first card only.
**Vital instead.** A tab per module. What is selected is what you see.
**Fix.** Put each card's number in its own header (e.g. "1 · LFO 1") and drop the ruler, or make the ruler cells the same width as the cards and let it scroll with them. Hide the stray scrollbar.

### 9. High: the source chip row still switches to codes and shows chips for unused envelopes (still open, review 5 #11)
**What's wrong.** Once the envelope pool has been revealed, every chip becomes a code: L1 L2 L3 AMP FLT FLT2 MOD E5 … E16 MSEG VEL KEY RND WHL AT (`PluginEditor.cpp:355-373, 717`). The shortenings don't match the long names: PRESSURE becomes "AT", and MOD ENV becomes "MOD", which collides with the MOD tab above it. E5 to E16 stay up even though they route nowhere (`neuro/08-MATRIX.png`, `dx7/09-FM.png`).
**Fix.** Show chips only for envelopes and LFOs that are in use or that the user added this session, and let the rest scroll rather than shorten. Use "PRES" / "MOD E" if you must shorten.

### 10. High: four names for one envelope
**What's wrong.** Filter envelope 2 appears as "FILT 2 ENV" (PLAY tabs, MOD card, chip), "FLT2" (compact chip), "F2 Env" (oscillator ENVELOPE menu, `processor/ParameterLayout.cpp:744-745`), and "Filt Env 2" / "FilterEnv2" in matrix sources **(matrix name inferred)**. The amp envelope is "AMP ENV", "AMP", "Amp Env" and "Amp" (the tag on the MOD card). The oscillator's envelope selector is labelled "AMP ENV" on OSC and "ENVELOPE" on FM. Vital uses ENV 1-6 everywhere.
**Fix.** Pick one name per envelope and use it in menus, chips, cards and the matrix. Label the oscillator selector "ENVELOPE" everywhere.

### 11. High: the Lorenz/chaos LFO values still overlap the knobs (still open, review 5 #10)
**What's wrong.** The knobs are now smaller, but "1/16", "0%", "10.00", "28.00", "2.67" and "Free" are still drawn across the knob bodies, and the labels sit on the arcs.
**Evidence:** `neuro/lfo-sim-lorenz.png`.
**Fix.** Two rows of normal knobs below the shape and trigger combos, plus a uitest that checks value text against the knob bounds.

### 12. High: DX7 voices sit in the browser as a separate library
**What's wrong.** The 288 DX7 voices form one category, "DX7", next to Bass, Keys and Pad. That isn't a kind of sound: E.PIANO 1 is Keys and BASS 1 is Bass. Names are cartridge ALL CAPS with "(ROM1A)" appended, even when the ROM1A bank chip is already selected. Every voice shows the same tags column, "BRIGHT · TONE · DRIFT · SPACE" (the macro names), so the column carries no information. The "Show DX7 voices" checkbox stays *unticked* while the list shows DX7 voices (`extras/extra-browser-dx7.png`). The header's category line reads "DX7" where other presets read "BASS" or "KEYS" (`dx7/01-MAIN.png`). REVIEW-PLAN 13.2 (the display formatter for DX7 names) is not visible in any shot.
**Vital instead.** Imported banks are folders. Each preset has a style, an author and comments.
**Fix.** Give DX7 voices a sound category from their name (PIANO / E.PIANO → Keys, BASS → Bass, …), keep the bank as a filter chip, show the name in Title Case without the bank suffix (the bank goes in a dim second column), and drop the checkbox in favour of the category or bank chip. Put real tags in the tag column (see #26).

### 13. Medium: the FM page still lights OUT for oscillators that are off (partly fixed, review 5 #4)
**What's wrong.** The diagram now dims the nodes of off oscillators, but the matrix still shows their OUT switch lit orange, and their rows and columns look live (Neuro's OSC 3 in `neuro/09-FM.png`; OSC 2 and 3 in `neuro/added-osc-FM.png`). A "NO FM IN" column is greyed (`dx7/fm-no-input.png`), so the dimming rule exists. It just isn't applied to "oscillator off".
**Fix.** Apply the same dimming to a switched-off oscillator's row, column and OUT switch, and say "OSC 3 is off" in the header.

### 14. Medium: the FM matrix header mixes unrelated controls
**What's wrong.** The FM MATRIX card header holds FM MODE, RING MOD, NOISE COLOUR and HARD SYNC 1>2. Noise colour belongs to SUB + NOISE. Ring mod and hard sync are oscillator-pair features that are already shown as warps elsewhere (`neuro/09-FM.png`). A new user looking for the noise colour would never look on the FM page.
**Fix.** Move NOISE COLOUR to the SUB + NOISE card. Put RING MOD and HARD SYNC in a small "PAIR" row under the matrix with a one-line explanation.

### 15. Medium: the 6x6 matrix clips the "FB" labels behind the knobs (still open, review 5 #23)
**What's wrong.** The diagonal cells show "FB~" and "FB·DX", cut off by the knob drawn on top (`dx7/09-FM.png`, `vital-extra/piano2/09-FM.png`). With 42 cells nearly all at 0%, the algorithm is hard to read.
**Fix.** Draw feedback as a loop glyph under the value. Draw zero cells as small "+" targets and only non-zero cells as knobs.

### 16. Medium: the physical preset's display label collides with its caption
**What's wrong.** On PLAY, the OSC 1 display for Felt Hammer Board reads "REED PICKUPcharge vs reed position", with the two strings touching. A "rest" label sits on top of the curve at the bottom (`keys/01-MAIN.png`). On the OSC page there is room, but "rest" still overlaps the curve (`keys/03-OSC.png`).
**Fix.** Elide or wrap the caption under the title when the display is narrower than both strings. Move "rest" outside the plot.

### 17. Medium: envelope graph tick labels collide with handles and with each other (still open, review 5 #18)
**What's wrong.** On DAHDSR, "10 ms", "DELAY" and "100 ms" are drawn on top of one another ("10 msDELAY100 ms") (`neuro/env-dahdsr.png`). On every PLAY envelope, "+10 ms" / "+100 ms" / "+1 s" sit under the release dot (`keys/01-MAIN.png`, `dx7/01-MAIN.png`).
**Fix.** Put the time ruler outside the plot, below it. Write the segment names (DELAY, HOLD) at the top of their spans.

### 18. Medium: the filter graph still parks node 2 at the bottom-right corner (still open, review 5 #17)
**What's wrong.** With Filter 2 at 20 kHz, its node is drawn at the bottom-right corner, overlapping node 1 when F1 is also open (the "1 2" pair in `dx7/01-MAIN.png`, `keys/01-MAIN.png`). That reads as a closed filter. FILTER 2 still has three pill groups on one header row (FILTER 2 / WEST, 12 / 24 dB), and BALANCE ("F1 = F2") still looks live in serial mode (`neuro/05-FILTER.png`).
**Fix.** Plot each node at its cutoff and resonance, nudging overlapping nodes apart. Disable BALANCE in serial mode. Move WEST into the model row.

### 19. Medium: PLAY changes its layout from preset to preset
**What's wrong.** With oscillators off, PLAY adds a PATCH card (signal-flow graph) and an OUTPUT spectrum (`keys/01-MAIN.png`). With six operators, OUTPUT appears under the folded cards (`dx7/01-MAIN.png`). With two oscillators on, neither appears (`neuro/01-MAIN.png`). The patch graph lists OSC 2 and OSC 3 at full brightness although they are off, and a stray "BYPASS" label floats under the line. The user's eye has no fixed place to land.
**Vital instead.** The Voice page has the same layout for every patch. Empty slots stay as dim slots.
**Fix.** Keep the left column's structure fixed (oscillator cards, then SUB + NOISE). Show the output spectrum in the same spot every time, or not at all. Dim off oscillators in the patch graph and label the bypass branch on the line.

### 20. Medium: the STEPS page still binds to an LFO that doesn't exist (still open, review 5 #19)
**What's wrong.** STEPS B says "LFO 4 plays these steps", but the LFO pool holds LFO 1 to 3. STEPS A has LFO 1 lit next to a "Use on LFO 1" button, with "LFO 1 isn't playing these". The lit pill reads as "active", which is the opposite of what is happening (`neuro/07-STEPS.png`).
**Fix.** As before: edit steps inside the LFO editor. Until then, only offer existing LFOs and draw "bound, not playing" as an outline.

### 21. Medium: the wavetable view is still flat 2D, and 3D is behind a cycling label (still open, review 5 #6)
**What's wrong.** WAVE → 3D → SPEC is still one cycling button (`WaveDisplay.h:1105, 1276`). "FRAME 1 / 64" overlaps the waveform on DX7 sines (`dx7/01-MAIN.png`).
**Fix.** Use a segmented WAVE | 3D | SPEC control, make 3D the default on the OSC page, and keep the frame readout outside the plot.

### 22. Medium: the matrix page has duplicates, dead space and an inline remap that pushes rows (still open, review 5 #8)
**What's wrong.** Rows 1 and 12 are identical (LFO 1 → Filter1 Cutoff +35%) and flagged only with "!". The bottom third of the page is empty. Opening REMAP inserts a 300 px panel between row 1 and row 2, pushing rows 10 to 13 off screen (`remap/remap-editor.png`). The live bar beside each source is a 4x8 px sliver. Destination names run words together: "Filter1 Cutoff" and "Osc1 Frame" here, but "FILTER 1" and "OSC 1" everywhere else.
**Fix.** Dock the remap editor in the empty lower area. Offer "merge" on "!" rows. Widen the live bar into the amount slider track. Write destinations as "Filter 1 › Cutoff".

### 23. Medium: the PHYSICAL page is still a dead end for a wavetable oscillator (still open, review 5 #15)
**What's wrong.** A full-size animated string, a "BOW" exciter tag, a "NO BODY" box, then "OSC 1 plays a wavetable, so it has no string" (`neuro/04-PHYSICAL.png`). The OSC 2 and OSC 3 pills look disabled, with no reason given.
**Fix.** Grey out the PHYSICAL sub-tab, with a tooltip, when no oscillator is physical. Show the moving string in the physical oscillator's own card.

### 24. Medium: the physical oscillator card is half empty
**What's wrong.** On OSC, the physical card has four rows: STRING (6 knobs), EXCITER (2 knobs), BODY & BUZZ (1 knob, COUPLING) and VOICE (8 knobs). The middle two rows are mostly empty, while the display on the left is a tall "reed pickup" curve, not the string (`keys/03-OSC.png`).
**Fix.** Merge EXCITER and BODY & BUZZ into one row. Put the live string, from the PHYSICAL page, in the display and keep the pickup curve as a small inset.

### 25. Medium: the FX rack is better, but three types still have no display and the type palette is cryptic
**What's wrong.** Drive (transfer curve), OTT (in/out and bands) and Reverb (decay) now have displays. Vowel (MORPH, MIX), AW Delay (TIME 100% … MIX) and Comb are still a wide card with two to five knobs centred in empty space (`neuro/11-FX.png`, `init/fx-35.png`). AW Delay's TIME reads "100%", not ms. Types already in the rack are greyed in the palette (REVERB in `dx7/11-FX.png`), which reads as "disabled", with no tooltip. "AW SATURATION" and "AW MODULATION" are squeezed into a condensed font. "AW" is never spelled out, while one pill says "AIRWINDOWS". "S" (solo) and "FULL" (band) in each header have no labels. On the reverb display, "HIGHS" is drawn on the decay lines.
**Fix.** Add a vowel formant curve, a comb response and delay taps. Put a small "in rack" dot on types already used instead of greying them. Use "Airwindows ›" as a group with full names. Label the header controls "SOLO" and "BAND: FULL".

### 26. Medium: the macro strip and tags still say nothing about routing (still open, review 5 #12)
**What's wrong.** Every DX7 voice has the same four macros, BRIGHT / TONE / DRIFT / SPACE, all at 0%. SPACE drives the MIX of a reverb that is switched *off* in E.PIANO 1 (badge 4 on a greyed card in `dx7/11-FX.png`), so turning it may do nothing **(inferred: I could not tell whether the macro also enables the slot)**. "5-8" is still an unlabelled toggle. Macro names still fill the browser's tag column.
**Fix.** Show the destination count and names when hovering a macro, and warn when a target module is off. Label the toggle "MACROS 5-8". Use descriptive tags.

### 27. Medium: master level still swings by 19 dB across presets (still open, review 5 #24)
**What's wrong.** Master reads −11.7 dB (Neuro Wobble), −5.1 dB (Init), +3.0 dB (every DX7 voice shot) and +7.0 dB (Felt Hammer Board).
**Fix.** Use a hidden per-preset output trim and keep the user's MASTER at 0 dB.

### 28. Medium: the tutorial advertises a mode that no longer exists, and is still opted out by default
**What's wrong.** "DX7 MODE + BANKS" is still a NEW IN 1.3 pill (`gui/TutorialOverlay.h:29`), although DX7 mode was folded into the FM engine. "Don't show this again" is still pre-ticked (`TutorialOverlay.h:66`). The tour still says to "drag its coloured dot to set the depth", which is the design finding #2 wants to replace.
**Fix.** Rename the pill "DX7 VOICES (.syx)" and leave the box unticked.

### 29. Medium: VECTOR still looks live while it is off and names oscillators that don't exist (partly fixed, review 5 #16)
**What's wrong.** The corners read "OSC 1 25%" and "OSC 2 25%" while the pad says VECTOR OFF. "OSC 4 (off)" is shown for an oscillator Neuro Wobble doesn't have. EVOLVE is still eight identical rows at 0% (`neuro/02-VECTOR.png`).
**Fix.** Dim the whole pad while it's off, write "OSC 4 (none)" or offer "add", and fold EVOLVE into the macro strip.

### 30. Medium: arp and generate layout (still open, review 5 #21)
**What's wrong.** The arp staircase is drawn at full strength while ARP is off. In GENERATE, TIME and SPREAD (strum) sit under PITCH / NOTE SPRAY, SNAP PLAYED floats alone, and ROOT stays live with SCALE = Off (`neuro/10-ARP-SEQ.png`).
**Fix.** As before: three boxed columns, and dim whatever has no effect.

### 31. Medium: the clip editor still has no keyboard or visible selection (still open, review 5 #22)
**Evidence:** `neuro/gen-clip-selected.png`: only "C3" is labelled, and selected notes look the same as the rest.
**Fix.** Add a key column and outline selected notes.

### 32. Low: section headings use a dot that looks like a power LED
**What's wrong.** ALGORITHMS, OPERATORS, LFO, ENVELOPES, CHAIN, MODULATION and RESPONSE have a grey ring-dot before them. On PLAY cards, the same dot is the module's colour key next to a real on/off switch, and on FX cards it is the slot colour. A user will click the heading dots expecting a toggle.
**Fix.** Drop the dot on plain headings, or keep it only where it carries the module colour.

### 33. Low: FM page copy and units
**What's wrong.** "OSC 1 AS AN OPERATOR - sounds at x1.000 the note, on the Operator EG" is awkward. Node captions mix formats ("x1.00 100%" vs "0 st 60%", `dx7/fm-dx-keys.png`). "KEY UP 2.5 / 2.6 s" and "C3 VEL 100" are unexplained.
**Fix.** "OSC 1 · ratio 1.000 · Operator EG". Show the same caption format on every node, and give the graph's corner labels tooltips ("shown for C3 at velocity 100; key released after 2.5 s").

### 34. Low: the PLAY envelope tabs show ENV 5 when the patch has no ENV 5
**What's wrong.** AMP ENV | FILT ENV | FILT 2 ENV | MOD ENV | ENV 5 on every preset, while MOD's pool shows AMP / FILT / FILT 2 and "+" (`neuro/01-MAIN.png` vs `neuro/06-ENV-LFO.png`).
**Fix.** Make the tabs follow the pool.

### 35. Low: the preset browser basics are unchanged (still open, review 5 #13)
**What's wrong.** "All 371" excludes DX7 while the header says "371 of 659". DELETE is dimmed with no reason. The last row is cut mid-word ("PD Casio Lead"). Docking widens the window to 2100 px (`extras/extra-browser-docked-dx7.png`). There's no author or comment field.
**Fix.** As before.

### 36. Low: Save As is unchanged (still open, review 5 #29)
**What's wrong.** "Saved without : / (a file name can't hold them)." is shown in past tense while you are still typing, and there are no author or comment fields (`extras/extra-save-as.png`).

### 37. Low: the scope controls are still cryptic (partly fixed, review 5 #28)
**What's wrong.** The dB scale, CLIP light, EXPAND and CLOSE are good. "Normal ▾  OS  2x ▾  HOLD  PEAK" still have no labels (`dx7/scope-panel.png`).
**Fix.** "TRIGGER: Normal", "OVERSAMPLE 2x".

### 38. Low: bare digit keys switch tabs (still open, review 5 #26)
**What's wrong.** `PluginEditor.cpp:2272` handles '1' to '9' with no modifier. 8 and 9 do nothing (there are 7 tabs). The tour promotes this.
**Fix.** Ctrl/Cmd + digit.

### 39. Low: the hint bar never changes (still open, review 5 #30)
**What's wrong.** It shows the same sentence in every shot, including on pages with no knobs.

### 40. Low: OSC card headers keep text buttons that look like dead tabs (still open, review 5 #27)
**What's wrong.** BOUNCE / EDIT / LOAD .WAV look like inactive tabs (`neuro/03-OSC.png`). The OSC row still has seven combos.

### 41. Low: the operator pills in FM look disabled when they aren't selected
**What's wrong.** OSC 2 to OSC 6 in "OSC 1 AS AN OPERATOR" are dark grey pills that look like greyed-out controls, the same style as the PHYSICAL page's genuinely unusable OSC 2 and OSC 3 pills.
**Fix.** Give unselected pills the oscillator's colour dot and normal text contrast, and keep grey for "not available" only.

### 42. Low: the DX7 PLAY card shows a greyed DETUNE "15 ct" and UNISON 1 on every operator
**What's wrong.** Imported operators carry the patch default detune (greyed because unison is 1). It's correct but adds noise to a page that already says nothing about FM (`dx7/01-MAIN.png`).
**Fix.** In the operator view proposed in #3, hide the unison controls for Op EG operators.

## Still open from review 5

| # | Review 5 finding | Status now |
|---|---|---|
| 1 | Depth dots instead of rings | Open (#2) |
| 2 | DX7 algorithm numbering | Changed, not fixed: the "DX7 ALG n" label is gone; tile 11 is now "DX 5 Keys", and 25 of the 32 DX7 algorithms read CUSTOM (#6) |
| 3 | Pool index rows don't match cards | Open (#8, #34) |
| 4 | FM shows OUT for an off oscillator | Partly fixed: the node is dimmed; OUT and matrix are not (#13). Stack layout not done, and the six-operator case is now broken (#1) |
| 5 | Folded card draws children | Open; cause found (#7) |
| 6 | Flat 2D wavetable, 3D hidden | Open (#21) |
| 7 | Scattered IA | Open, and worse: Op EG and the DX LFO add two more places (#4, #5) |
| 8 | Static matrix | Partly fixed (tiny live bar, numbered rows); remap still inline, duplicates remain (#22) |
| 9 | FX slots without displays | Partly fixed (Drive, OTT, Reverb); Vowel, Comb, Delay remain; palette greying still reads as disabled (#25) |
| 10 | Lorenz values over knobs | Open (#11) |
| 11 | Cryptic chip codes | Open (#9) |
| 12 | Macros silent | Open (#26) |
| 13 | Browser basics | Open (#35) |
| 14 | Confirm dialog vs EDITED badge | Not re-checked (no confirm shot this round) |
| 15 | PHYSICAL dead end | Open (#23) |
| 16 | VECTOR inert | Partly fixed: "(off)" on corners; still looks live (#29) |
| 17 | Filter picker, node 2, BALANCE | Open (#18) |
| 18 | Envelope tick labels | Open, plus a new DAHDSR collision (#17) |
| 19 | STEPS vs LFO pool | Open (#20) |
| 20 | Destination naming | Open (#22) |
| 21 | Arp / generate layout | Open (#30) |
| 22 | Clip editor | Open (#31) |
| 23 | 6x6 FM matrix FB clipping | Open (#15) |
| 24 | Master level spread | Open, now 19 dB (#27) |
| 25 | Tutorial | Open, plus the stale DX7 MODE pill (#28) |
| 26 | Digit keys | Open (#38) |
| 27 | OSC row crowded | Open (#40) |
| 28 | Scope cryptic | Partly fixed (#37) |
| 29 | Save As | Open (#36) |
| 30 | Static hint bar | Open (#39) |

## Score versus Vital

**4.5 / 10** (review 5: 5 / 10). The FX rack and scope improved. But folding DX7 in added a second envelope system, a fourth kind of LFO and an algorithm grid that doesn't know most DX7 algorithms, all on one page and in DX vocabulary. On top of that, the operator diagram now hides the carriers of every six-operator voice. Almost all of review 5's structural items are still open. Vital's core promise, that you see what's moving and edit it where you see it, is still missing from modulation, envelopes and FM.
