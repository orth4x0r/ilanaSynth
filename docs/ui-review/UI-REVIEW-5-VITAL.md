# UI review 5: ilanaSynth against Vital (fresh, adversarial)

Reviewer: fresh agent with no earlier review read. Evidence: offscreen shots in `shots5/` (1060x720 logical at 1.5x, note held) plus source in `src/PluginEditor.cpp` and `src/gui/`. The Vital comparisons come from what I know of its UI; I did not run it. Anything marked **(inferred)** was not confirmed in a screenshot or in code.

## Verdict

ilanaSynth looks clean and consistent at a glance, but a Vital user hits structural problems within minutes. Modulation is shown as 10 px pie dots beside the knob, not as range rings on it. The wavetable is shown as a flat single cycle with the 3D view hidden. Related features are spread over seven tabs and fifteen sub-views. Several screens contradict themselves: DX7 algorithm numbers, a hidden envelope that is still being edited, an "on" output for an oscillator that is off, and labels drawn inside a folded card. Density is uneven: some pages are cramped (Lorenz LFO knobs, the 6x6 FM grid) while others are half empty (the matrix, every FX slot, VECTOR, PHYSICAL on a wavetable osc). The polish holds up in a still image and breaks down once you use it.

## Findings, worst first

### 1. Critical: modulation depth is not shown on the knob
**What's wrong.** A routed knob gets a column of 10 px dots to its right, one per source. Each dot is a tiny pie showing depth (`ParamControls.h:280-325`, `dotSize = 10`). The knob arc only shows the *current* modulated position, in one colour taken from the strongest source (`ParamControls.h:775-825`). Nothing on the knob shows the range a routing will sweep, so you can't read "LFO1 sweeps cutoff from here to there" at a glance. With three sources, the dot column runs below the knob and pushes against its neighbour (`neuro/01-MAIN.png` OSC1 FRAME and CUTOFF; `neuro/05-FILTER.png` CUTOFF). At 75% zoom the dots are about 7 px and the macro digits inside them are unreadable (`small/01-MAIN.png`).
**Vital instead.** Each routing draws a coloured arc ring around the knob covering its full range. You drag the ring itself, and the live value moves inside it.
**Fix.** Draw one thin concentric range arc per routing (source colour, base to base+depth) inside or outside the value arc, and make the arc draggable to set depth. Keep the dots only as a hover legend, or drop them.

### 2. Critical: the DX7 algorithm grid uses its own numbering
**What's wrong.** On a DX7 voice the heading reads "DX7 ALG 5", but the highlighted tile is **11**. The grid has 16 house algorithms and the label is the real DX7 number from 1 to 32 (`pages/FmInputPages.h:359` vs the tile strip). Anyone who knows DX7 algorithms will think the import is broken.
**Vital instead.** Not applicable (Vital has no DX mode). Any FM synth that imports DX7 voices (Dexed, FM8) shows the DX7 algorithm in DX7 numbering.
**Fix.** In DX mode, show the 32 DX7 algorithms with their numbers. Otherwise label the house tiles A to P, or "1 to 16 (ilana)", and print "= DX7 alg 5" on the matching tile.
**Evidence:** `dx7/09-FM.png`.

### 3. High: the envelope pool shows cards that don't match the index row, and it edits envelopes it doesn't show
**What's wrong.** The ENV/LFO page has a 1 to 16 index row spread across the full width above only 3 or 4 cards. In `init/06-ENV-LFO-env5.png`, index **5** is selected and the editor says "ENV 5", but there is no ENV 5 card: the cards are AMP, FILT, FILT 2 and "+". In `init/env-pool-revealed.png`, "6" is highlighted above the *second* card while the ENV 6 card is the *fourth*. The PLAY tab's envelope sub-tabs list MOD ENV and ENV 5, which the MOD page pool hides (`neuro/01-MAIN.png` vs `neuro/06-ENV-LFO.png`). The LFO pool has the same layout: 13 of its 16 index columns sit over nothing.
**Vital instead.** A fixed set of LFO and ENV tabs, each with its own panel. What you see is exactly what you're editing.
**Fix.** Drop the 16-number ruler. Show one card per envelope that exists, with the selected one highlighted, and an "+" card at the end. If the ruler stays, put each number directly above its card.

### 4. High: the FM page shows OSC 3 output as on while OSC 3 is off
**What's wrong.** In Neuro Wobble, OSC 3 is OFF on PLAY and OSC. The FM page still draws OSC 3 as an OUT operator, and its OUT switch is lit orange (`neuro/09-FM.png`, `small/09-FM.png`). In the DX7 voice, OSC 2/4/6 OUT are off (correct), but the diagram scatters six operators with no stacking logic (OSC 6 is placed left of and below OSC 1).
**Vital instead.** The FM/RM routing lives on the oscillator that is modulated. A disabled oscillator is greyed out everywhere.
**Fix.** Grey out the operator node and its matrix row and column when the oscillator is off. Lay operators out as carrier stacks, DX style (carriers on the bottom row, modulators above).

### 5. High: a folded card draws its children's labels over the header
**What's wrong.** After adding oscillators, the SUB + NOISE card on PLAY folds, but "SHAPE  OCTAVE  SUB LEVEL  NOISE" plus a mod badge are still drawn on top of the folded title strip, overlapping "folded to fit - click to open". This is a clipping or visibility bug.
**Evidence:** `neuro/added-osc-MAIN.png` (bottom left card). Source: `pages/MainPage.h:166-176` paints the folded text, but the child controls are not hidden.
**Fix.** Hide or clip the card's children when it is folded, and add a UI test that fails if any child is visible inside a folded card.

### 6. High: the wavetable display is a flat 2D single cycle and 3D is hidden
**What's wrong.** The headline wavetable view is one 2D cycle, plus a scrub bar and the text "FRAME 1/64" overlapping the wave (`dx7/01-MAIN.png`, `neuro/01-MAIN.png`). A 3D waterfall exists but is reached only by cycling a small "WAVE" corner label through WAVE → 3D → SPEC (`WaveDisplay.h:66,1105`). SPEC on a near-sine frame shows one yellow bar in a black box (`extras/extra-browser-dropdown.png`, behind the browser).
**Vital instead.** The oscillator always shows the 3D stacked wavetable with the current frame highlighted. It is large, and it is the first thing you see.
**Fix.** Make 3D the default on OSC (where there is space). Replace the cycling label with a three-way segmented control. Keep the frame label out of the plot area.

### 7. High: the information architecture is scattered
**What's wrong.** VECTOR sits under PLAY. PHYSICAL is under OSC. BODY is under FILTER. Sympathetic Strings and Acoustic Keys are folded cards on OSC. MSEG is under MOD › STEPS & MSEG, apart from the LFOs, which are under MOD › ENV/LFO. Steps are edited on a different sub-page from the LFO that plays them. Generate sits under SEQ. With 7 tabs, up to 3 sub-tabs each, plus SCOPE, there are about 15 views. LFOs alone come in three kinds (shape LFOs, Steps rows bound to an LFO, a single MSEG), each edited in a different place.
**Vital instead.** Four tabs (Voice, Effects, Matrix, Advanced). The voice page shows oscillators, filters, every LFO and envelope (each LFO is itself an MSEG) and the macros at once.
**Fix.** Merge MSEG and Steps into the LFO editor as shape modes (the LFO editor already has Draw/Steps/Curve per `LfoDisplay.h:28`). Move BODY next to PHYSICAL. Move VECTOR to OSC. Aim for 5 tabs.

### 8. High: the modulation matrix is a long static form
**What's wrong.** Each row is ON toggle, source combo, VIA "+", amount slider, a "+35%" readout, a curve thumbnail, a polarity combo, a destination combo and ×. There is no live meter showing what each route is doing now, no sorting or grouping by source or destination, and no filter. Neuro Wobble ships duplicate rows (1 and 12, both LFO1 → Filter1 Cutoff +35%), flagged only by a "1!" glyph. Below 13 rows, the bottom 35% of the page is empty (`neuro/08-MATRIX.png`). The remap editor opens inline and pushes every row down (`remap/remap-editor.png`).
**Vital instead.** The matrix shows a live modulation bar for every row, sorts by any column, and has a fixed side panel for the remap curve so rows never jump.
**Fix.** Add a per-row live value bar. Make the headers sortable. Put the remap editor in a docked panel in the empty lower area. Have the preset pipeline merge duplicate routings, and have the UI offer "merge" on "!" rows.

### 9. High: FX slots waste space and most have no display
**What's wrong.** The chain list column takes about 30% of the width for three 50 px rows and nothing below them (`neuro/11-FX.png`). Each slot card is wide but shows 2 or 3 knobs centred in empty space (Vowel, Comb, AW Delay). There's no delay tap view, no vowel formant curve and no comb response (`neuro/fx-05.png`, `init/fx-35.png`). SLOT BLEND appears only on the selected slot, so slot headers change shape when you click them. The selected type in the add-effect grid is drawn *dimmer* than the rest (OTT in `init/fx-20.png`, AW DELAY in `init/fx-35.png`), which reads as "disabled".
**Vital instead.** A narrow, reorderable effect list on the left. Each effect gets a large, specific display (delay feedback taps, the EQ curve, the reverb decay shape, the distortion transfer curve), and its controls fill the module.
**Fix.** Narrow the chain list to about 180 px. Give every FX type a display (start with delay, reverb, vowel, comb, chorus and phaser). Highlight the current type; don't dim it.

### 10. High: the Lorenz/chaos LFO controls overlap their own values
**What's wrong.** With a sim shape chosen, six knobs are squeezed into the LFO panel, and their values ("1.0 Hz", "0%", "10.00", "28.00", "2.67", "Free") are drawn across the knob bodies.
**Evidence:** `init/lfo-sim-lorenz.png` (RATE / SMOOTH / SIGMA / RHO / BETA / SEED).
**Fix.** Make the panel two rows of normal-sized knobs, or use the space under the shape combo. Add a layout test for "value text does not intersect knob bounds".

### 11. High: the source chip bar changes form and becomes cryptic
**What's wrong.** On PLAY/OSC/FILTER the chips read "LFO 1 … PRESSURE". Once more sources have been revealed, every chip switches to codes: L1 L2 L3 AMP FLT FLT2 MOD E5 … E16 MSEG VEL KEY RND WHL AT (`PluginEditor.cpp:647-700`; `neuro/08-MATRIX.png`, `neuro/09-FM.png`). E5 to E16 appear for envelopes that do nothing, and the colours repeat, so the colour key loses its meaning. MOD ENV is "MOD", which is ambiguous next to the MOD tab.
**Vital instead.** Sources live in their own panels (LFO 1 to 8, ENV 1 to 6, Random, Macros), and each panel has a drag handle. The labels never change.
**Fix.** Show chips only for sources that exist and are in use or were added. Group them as LFO | ENV | PERF with dividers. Scroll the row rather than switch to codes.

### 12. Medium: macros are labelled but show 0% with no routing hint
**What's wrong.** Neuro Wobble's TONE/TALK/TEAR/SUB macros all sit at 0%, with 8 matrix rows behind them. Nothing on the macro knob shows where it routes or how far, and the knobs have no mod ring (`neuro/01-MAIN.png`). "5-8" is a cryptic page toggle. The preset browser's "tags" column shows these macro names (TONE · TALK · TEAR · SUB) instead of descriptive tags (`extras/extra-browser-dropdown.png`).
**Vital instead.** Each macro is a source with a drag handle. Hovering highlights its destinations, and the macro name is editable in place.
**Fix.** Show destination count and names in a hover card on the macro. Label the toggle "MACROS 5-8". Keep macro names out of the tag column.

### 13. Medium: the preset browser lacks the basics
**What's wrong.** It sorts by name only. There's no author, no comments or description, and no audition. "All 371" excludes DX7, yet the DX7 category shows 288 in the same list, and the header says "371 of 659". DELETE is dimmed with no reason given. The last row is clipped mid-word ("PD Casio Lead"). The drop-down covers the middle of the synth but leaves knobs visible on both sides (`extras/extra-browser-dropdown.png`). Docking widens the plugin window to 2100 px (`extras/extra-browser-docked.png`), which may not fit a laptop DAW **(window growth inferred from shot width)**.
**Vital instead.** A full-height browser with folders, style filters, author, comments, star ratings and a search across all of them.
**Fix.** Add author/comment fields to Save As and show them in the list. Make "All" mean all, with the DX7 switch acting as a filter chip. Give DELETE a tooltip ("factory presets can't be deleted"). Dock inside the existing width instead of growing the window.

### 14. Medium: the confirm dialog contradicts the header
**What's wrong.** "Replace your edits? 'Neuro Wobble' has changes that aren't saved." The header behind it shows "Neuro Wobble" with **no** EDITED badge (`confirm/00-confirm.png`). Either the badge missed an edit or the dialog fired for no reason. Whichever it is, you can't trust the badge.
**Fix.** Use the same dirty source for both (`rememberLoadedFingerprint` in `PluginEditor.cpp:~2230`) and add a test: if the dialog asks, the badge must show.

### 15. Medium: the PHYSICAL page is a dead end for non-physical oscillators
**What's wrong.** Opening OSC › PHYSICAL on a wavetable oscillator shows a large animated string and "NO BODY" frame, then "OSC 1 plays a wavetable, so it has no string" and a SWITCH TO PHYSICAL button (`neuro/04-PHYSICAL.png`). That's a full page of decoration for a feature that isn't in use. The OSC 2 and OSC 3 pills look disabled with no explanation. The physical osc card says "OSC › PHYSICAL shows it moving", pointing you to a different page to see your own sound (`init/osc-physical.png`). Its STRING graphic is the same static curve for Burst and Hammer (`keys-grand-osc.png` vs `init/osc-physical.png`).
**Fix.** Hide the PHYSICAL sub-tab, or grey it with a tooltip, when no oscillator is physical. Show the moving string inside the oscillator card. Make the static graphic reflect the exciter.

### 16. Medium: the VECTOR page is mostly inert
**What's wrong.** Vector is off by default, so the XY pad says "VECTOR OFF - switch on" while the corners still read "OSC 1 25%". It picks OSC 3/4 as corners even though they're off ("OSC 3 (off)"). The EVOLVE column is 8 identical rows of slider + EVOLVE + RATE, so the page is about 60% empty space (`neuro/02-VECTOR.png`).
**Fix.** Fold Evolve into the macro strip (a small "drift" control per macro). Put the vector pad on OSC as a compact panel, and grey out the corner readouts while vector is off.

### 17. Medium: the filter type picker mixes two kinds of tab
**What's wrong.** CLASSIC / ANALOG / VOICE / AIRWINDOWS are model tabs. Under CLASSIC there are BASIC / CHARACTER / SPECIAL column headers, which line up under the tabs as if they belonged to them (`neuro/05-FILTER.png`). FILTER 2 adds a FILTER 2/WEST toggle on the same header row as 12/24 dB, so there are three pill groups in one row. Filter 2 open at 20 kHz still draws node 2 at the bottom right of the response, which looks like a closed filter. A BALANCE knob reading "F1 = F2" stays live-looking in serial mode, with only a caption saying it does nothing.
**Vital instead.** One model dropdown, then a style selector, and the graph is the main control (drag cutoff and resonance, scroll for morph).
**Fix.** Use a model dropdown plus a style row. Plot node 2 at its real cutoff and resonance. Disable BALANCE in serial mode.

### 18. Medium: envelope graphs have overlapping, odd time labels
**What's wrong.** Inside the envelope plots, "100 ms" and "+10 ms"/"+100 ms" labels sit under the release point and collide with it (`neuro/01-MAIN.png`, `init/01-MAIN.png`, `init/lfo-curve.png`: "+10 ms +100 ms" run together). The ruler is non-linear and has no visible scale. There are no curve-power handles on the segments, and TENSION is a separate knob.
**Vital instead.** A time ruler along the bottom and draggable curve handles on each segment. Values show while you drag.
**Fix.** Put tick labels outside the plot. Add per-segment curve handles. Show the dragged value inline.

### 19. Medium: the STEPS page contradicts the LFO pool
**What's wrong.** STEPS B says "LFO 4 plays these steps", but the LFO pool shows only LFO 1 to 3 and a "+" (`neuro/07-STEPS.png` vs `neuro/06-ENV-LFO.png`). STEPS A is bound to LFO 1 (pill highlighted pink) yet "isn't playing these". The highlighted pill looks like the active choice, which is the opposite of what's happening.
**Fix.** Edit steps inside the LFO editor (see #7). Until then, bind rows only to LFOs that exist, and style a "bound but not playing" pill differently from "playing".

### 20. Medium: some presets ship routings that do nothing
**What's wrong.** Neuro Wobble routes Macro 2 and LFO 2 to Vowel Morph, but the vowel FX is in the chain only as FX slot 1 and the filter is LP Classic **(inferred: "Vowel Morph" may mean the FX slot; the destination name doesn't say which)**. More generally, the destination names don't say which module they belong to ("Drive Amount": FX Drive or filter drive?).
**Vital instead.** Destinations are named module-first ("Filter 1 Cutoff", "Distortion Drive").
**Fix.** Name every destination as Module › Param, and show the module's on/off state in the destination cell.

### 21. Medium: arp and generate layout
**What's wrong.** The arp step view draws a staircase of identical bars even when ARP is off (a small "ARP OFF" label in the corner). Step numbers (1, 5, 9, 13) sit behind the bars. In GENERATE, the group headers SCALE / STRUM / NOTE SPRAY don't line up with their controls: TIME and SPREAD (strum) sit under the PITCH/NOTE SPRAY column and SNAP PLAYED floats alone. ROOT and SNAP PLAYED stay live with SCALE = Off. The enable switch is at the far right of the header row, unlike every other section (`neuro/10-ARP-SEQ.png`).
**Fix.** Grey out the pattern view when off. Lay out GENERATE as three boxed columns that each hold their own controls. Put enable switches in the same place as on other cards.

### 22. Medium: the clip editor has no pitch ruler or visible selection
**What's wrong.** The piano roll labels only "C3" and has no keyboard strip, so you can't tell other pitches. In `neuro/gen-clip-selected.png`, the "selected" notes look the same as the rest. No playhead is visible while a note is held.
**Fix.** Add a key column, outline selected notes, and draw a playhead.

### 23. Medium: the 6x6 FM matrix crams knobs and clips "FB"
**What's wrong.** In DX7 mode, 42 small knobs in a grid. The "FB" labels clip behind the knobs as "FB·", and every cell reads 0% except four (`dx7/09-FM.png`). It's hard to see the algorithm in it, and it repeats the operator diagram on the left.
**Fix.** Show non-zero cells as large knobs and zero cells as small "+" dots. Put the feedback amount on the diagonal as a different glyph, not a clipped label.

### 24. Medium: master level varies widely across presets
**What's wrong.** Master reads −11.7 dB (Neuro Wobble), −5.1 dB (Init), +3.0 dB (DX7 E.PIANO) and +4.2 dB (Hammered Strings). The user decision says presets must never break level-wise. Making up level with the master knob means any later move of that knob, or a copied master value, jumps loudness by about 16 dB **(inferred from readings, not measured)**.
**Fix.** Normalise preset level internally (an output trim that isn't the user's master) and leave master at 0 dB.

### 25. Medium: the tutorial is a text wall with "Don't show again" pre-checked
**What's wrong.** Five long bullets plus 14 "new in 1.3" pills. The checkbox is ticked by default, so a single click on GOT IT hides it for good (`init/00-tutorial.png`). "TABLE opens the browser", but TABLE is drawn as a dropdown.
**Fix.** Leave the checkbox unticked. Cut it to three bullets. Draw TABLE as a browser button (a waveform thumbnail plus name, as Vital does) rather than a combo.

### 26. Low: bare digit keys switch tabs
**What's wrong.** Keys 1 to 7 switch tabs whenever the editor has focus (`PluginEditor.cpp:2203-2215`). In hosts that use digits (Ableton's computer MIDI keyboard octave keys, Reaper actions) a stray key press switches pages **(conflict inferred, not tested in a host)**. 8 and 9 do nothing.
**Fix.** Use Ctrl/Cmd + digit, or make it an option.

### 27. Low: the oscillator card row is overcrowded with selector combos
**What's wrong.** Each OSC card on the OSC page has 7 combos (MODE, TABLE, WARP, SPECTRAL, UNI MODE, CHORD, AMP ENV) and 11 knobs in one row. The TABLE combo is 120 px and truncates long names **(truncation inferred)**. BOUNCE / EDIT / LOAD .WAV are text buttons in the header, styled like inactive tabs (`neuro/03-OSC.png`).
**Vital instead.** The table name with arrows sits on the wavetable display, and warp and spectral modes are dropdowns next to their knobs.
**Fix.** Put the table name and browse arrows on the display, and put each mode combo above its own amount knob.

### 28. Low: the scope's controls are cryptic
**What's wrong.** "Normal ▾  OS  2x ▾  HOLD  PEAK" in one row with no tooltip text visible. The "CLIP" box sits over L/R meters reading −6.8/−7.3 (`neuro/scope-panel.png`). Opening SCOPE takes over half of PLAY.
**Fix.** Label the controls (Trigger, Oversample) and float the scope as a resizable overlay.

### 29. Low: Save As has no metadata fields
**What's wrong.** Only name, category and tags; no author or comments. The warning that it "Saved without : / (a file name can't hold them)" appears while you're still typing, in past tense (`extras/extra-save-as.png`).
**Fix.** Add author and comment fields. Reword to "':' and '/' will be removed from the file name".

### 30. Low: the hint bar text never changes
**What's wrong.** The status line always says "Hover any control for details. Drag source chips onto knobs…" (every shot), even on pages with no knobs or while dragging. It takes a full row at the bottom.
**Fix.** Show only the hovered control's description and value. Hide the bar when it would repeat itself, or fold it into the tooltip.

## Score versus Vital

**5/10.** The visual system is coherent and the feature set is huge. But modulation feedback (dots instead of rings), the wavetable view, the IA split across 15 views and a series of self-contradicting screens put it clearly behind Vital in daily use.