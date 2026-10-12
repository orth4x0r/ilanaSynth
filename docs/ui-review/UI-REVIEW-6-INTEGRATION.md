# UI review 6: integration audit

Date: 2026-10-02. Branch `main` at e9eaf10 (after PR #11, "Fold DX7 mode into the FM engine: the Operator EG", and PR #12).
Reviewer: a fresh adversarial reviewer. No code was changed.

**Question asked:** which features feel bolted on, or like a separate machine inside the synth, rather than part of its
normal engines? The owner's goal: "a powerhouse, yet one that's not unwieldy". The DX7 integration comes first because no
reviewer has looked at it yet.

**Evidence.** Screenshots in `/home/user/shots6/{init,neuro,dx7,keys,small,extras,remap}/`. New renders in
`/home/user/shots6/integration-extra/`:
- `strings/`: "STRINGS 1 (ROM1A)", DX7 algorithm 2.
- `tubbells/`: "TUB BELLS (ROM1A)".
- `epiano-small/`: "E.PIANO 1 (ROM1A)" at 75 % width.

Code references are `file:line` on main. A claim marked **(inferred)** comes from reading code and was not seen on
screen or heard.

**Snapshot caveat.** The snapshot tool edits the patch between shots: it switches OSC 1 to Physical and back to
Wavetable, reveals pools, and so on. That is why most shots after `03-OSC` say EDITED, and why `keys/04-PHYSICAL.png`
claims OSC 1 "plays a wavetable". Those are artefacts of the tool and are not reported as findings.

**Settled, not re-argued** (`docs/REVIEW-PLAN.md`): a neutral Init (S18), the header A/B and history, choice parameters
not automatable, and append-only parameter IDs and choice orders. Every fix below respects the last rule: new choices
and parameters are appended, and DSP stays bit-identical unless a fix says otherwise.

---

## Verdict

**The DX7 fold-in is right in the engine and only half done in the interface.**

What works:
- A DX7 voice really is an ordinary patch now: six oscillators, the FM matrix, an ENVELOPE choice, plain parameters,
  undo and save.
- The banks load bit-identically.

What still looks like DX7 mode with the walls painted over:
- The Operator EG is edited in one place only: two rows of knobs on the FM page's operator card. Every other part of the
  synth says something different. The PLAY and MOD pages show the AMP ENV and badge it "Amp" on a DX7 patch, even though
  nothing uses it. The envelope pool has no Operator EG. No source chip names it. The OSC card says "AMP ENV: Op EG" with
  nothing behind it.
- The DX7's own pitch envelope and LFO are a second modulation system beside the synth's 16 envelopes and 16 LFOs:
  - neither is a matrix source;
  - none of their 33 knobs is a destination;
  - they move only the oscillators on the Operator EG;
  - they show raw 0-99 numbers under names the rest of the synth does not use (SPEED, WAVE, KEY SYNC, RATE KEY, AMS).
- The worst problem is plain breakage. On every six-operator patch whose operator uses the Operator EG (all 288 DX7
  presets), the FM diagram squeezes its hexagon flat:
  - OSC 1 and OSC 2 are drawn underneath OSC 5 and OSC 4;
  - the 2→1 route looks like a 4→5 route;
  - a feedback loop appears on the wrong operator.
- The diagram and the algorithm grid are what tell a DX7 user "this is my voice", and both fail. The diagram is wrong,
  and the grid says "CUSTOM - matches no algorithm here" for 25 of the 32 DX7 algorithms.

The other special features follow the same pattern on a smaller scale:
- **Parallel editors.**
  - STEPS A/B duplicate the LFO panel's step editor.
  - The single MSEG duplicates the LFO "Curve" shape.
  - The PHYSICAL page repeats about half of the physical oscillator card.
  - Three output views share the screen.
- **Hidden switches.**
  - WEST and four note engines live behind card tabs that never show what is on.
  - FX-splitter crossovers sit behind a right-click.
  - Scala is only in a settings submenu.
  - Engine quality sits inside the scope.
- **Name collisions.** There are four different things called BODY, three called BOUNCE, two called DRIFT, two kinds of
  "DX7" feedback, and "AW" brand prefixes.

None of this needs a new engine. It needs:
- each feature to be shown where its siblings are, under its siblings' names;
- the special systems either exposed to the matrix or explained as what they are.

**Bottom line:** fix findings 1 to 6 before calling the DX7 fold-in done. Then take the naming and duplication passes
(7 to 26) as one or two batches.

---

## Findings, worst first

Severity: **Critical** (wrong information or broken display), **High** (a feature feels like a separate machine, or a
user would be misled), **Medium** (friction, duplication or inconsistency that makes the synth feel unwieldy),
**Low** (polish).

### 1. Critical: the FM diagram hides OSC 1 and OSC 2 and draws false routes on every Operator EG patch

**What's wrong.** When the selected operator uses the Operator EG, the operator card takes up to 82 % of the left
column (`src/gui/pages/FmInputPages.h:576`). That leaves the OPERATORS diagram about 80 px tall. With six operators,
`FmDiagram::operatorCentres` (`src/gui/FmDiagram.h:426-435`) places them on an ellipse whose vertical radius is half the
remaining height, and that height is nearly zero. The ellipse collapses to a line: OSC 1 lands exactly on OSC 5 and OSC 2
exactly on OSC 4, and the later circle is painted over the earlier one.

What that does on screen:
- On E.PIANO 1, the blue 2→1 arrow runs between OSC 4 and OSC 5 under a "100%" label. It looks like a 4→5 route, which
  does not exist.
- On STRINGS 1, OSC 2's feedback loop is drawn on OSC 4.
- The captions under the nodes (ratio and level) are cut off by the card's bottom edge.

**Why it matters.** The diagram is the one picture of a DX7 voice's algorithm. It now shows the wrong algorithm on all
288 DX7 presets, and on any user patch that puts an operator on the Operator EG.

**Fix.**
- In `FmDiagram.h`, lay operators out by depth when the area is wider than tall: carriers on the bottom row, each
  modulator one row above what it drives, as `FmAlgorithmStrip::paintAlgorithm` already does for the grid icons
  (`src/gui/FmWidgets.h:53`). This is also the layout DX7 users read.
- Keep the ring only when there is room for it.
- In `FmInputPages.h::resized`, cap the operator card so the diagram keeps at least about 5 × the operator radius.
  Alternatively, fold the EG's second row (scaling) behind a "SCALING" toggle.
- Add a `--uitest` check in `tools/Snapshot.cpp`: for six shown operators, no two node circles overlap and every caption
  lies inside the diagram.

**Evidence.**
- `dx7/09-FM.png`: OSC 6, 5, 4 and 3 are visible, OSC 1 and 2 are missing, and there is a blue arrow between 4 and 5.
- `integration-extra/strings/09-FM.png`: the feedback ring drawn on OSC 4 is OSC 2's (the matrix shows OSC 2 → OSC 2 at 50 %).
- `integration-extra/tubbells/09-FM.png`.
- `integration-extra/epiano-small/09-FM.png`.
- Compare `dx7/fm-dx-keys.png` (Init with algorithm 11, no Op EG): the full-height hexagon is fine.

### 2. High: on a DX7 patch the envelope pages say the AMP ENV shapes the sound; the Operator EG appears nowhere outside the FM page

**What's wrong.**
- On a DX7 voice every operator plays the Operator EG, and the amp envelope "steps aside" (`src/dsp/Voice.cpp:1140`).
  Yet the PLAY page's ENVELOPE card opens on AMP ENV with its ADSR knobs (5 ms / 300 ms / 80 % / 250 ms). Editing them
  does nothing.
- The MOD > ENV/LFO pool's first card reads "AMP ENV • Amp". `EnvThumbs.h:388` always adds "Amp" for envelope 1, and
  `EnvThumbs.h:258` always counts it as in use.
- There is no Operator EG card in the pool, no "OP EG" chip in the source row, and the "Osc1 Amp" badge logic only
  understands envelopes 1-16 (`EnvThumbs.h:398`).

**Why it feels separate.** A user who opens the envelopes to shorten an E.PIANO's release finds a normal-looking amp
envelope, edits it, and hears nothing change. The real envelopes are on another page, under another name, in another
widget.

**Fix.**
- In `src/gui/EnvThumbs.h`, give envelope 1 the "Amp" badge only when some shown oscillator's `_amp_env` is 0. Write
  "unused: oscillators use Op EG" otherwise, and dim the card as other unused envelopes are dimmed.
- Add an "OP EG" thumbnail at the end of the envelope pool while any oscillator uses it. Show one mini curve per
  operator on it, coloured by oscillator, using `OperatorEgGraph`'s calculation. Clicking it opens the FM page on that
  operator (as the STEPS hint's "Use on LFO" button already jumps between pages).
- On PLAY's ENVELOPE card (`src/gui/pages/MainPage.h`), open on the OP EG tab when no oscillator uses ENV 1.
- In `FmInputPages.h` and `ParamInfo.h`, call it "OP ENV" or "Operator Env" in the UI, so it reads as one of the synth's
  envelopes. Keep the saved choice text "Op EG".

**Evidence.**
- `dx7/01-MAIN.png`: ENVELOPE shows AMP ENV, 5 ms / 300 ms / 80 % / 250 ms.
- `dx7/06-ENV-LFO.png`: "AMP ENV • Amp", and no Op EG anywhere.
- `src/gui/EnvThumbs.h:255-283` and `:385-400`.

### 3. High: the PITCH / LFO tab is a second, private modulation system beside the synth's envelopes and LFOs

**What's wrong.**
- The voice-wide pitch envelope (R1-R4, L1-L4) and the DX7 LFO (SPEED, DELAY, PITCH DEPTH, AMP DEPTH, KEY SYNC, WAVE,
  PITCH SENS) sit behind a seventh tab among the operator tabs on the FM page (`FmInputPages.h:279-301`).
- They are not modulation sources: `Mod::Source` (`src/dsp/Modulation.h:12-51`) has no Op LFO and no pitch EG.
- They cannot drive anything else (a filter, an FX), and nothing else can drive them.
- They move only "ratio oscillators on the Operator EG" (`Voice.cpp:1598-1603`). In a mixed patch, an oscillator on Amp
  Env ignores the voice's vibrato and pitch envelope, with no warning.
- The tab is hidden unless some oscillator uses the Operator EG (`FmInputPages.h:632`), so the feature cannot be found.

**Why it feels separate.** ilanaSynth already has 16 LFOs with Triangle, Saw, Square, Sine and S&H, plus a fade
(START/RETRIG), and 16 envelopes, and every one of them is a matrix source drawn as a chip. The DX7 LFO is a 17th LFO
that doesn't appear in the LFO pool, the chip row or the matrix. This is exactly the "parallel system" the owner wanted
gone.

**Fix.** Two levels.
- **Without changing sound.**
  - Add two read-only sources at the end of `Mod::Source`: "Op LFO" and "Op Pitch Env". Feed them from the values
    `Dx7Engine` already computes, and give them chips and colours (`src/gui/ModSourceChip.h`, `src/dsp/Modulation.h`,
    `getSourceNames`).
  - Add destinations for `opeg_lfo_speed`, `opeg_lfo_pmd` and `opeg_lfo_amd` (append to `getParamDestinations`, as the
    filter overhaul did for SPECTRAL AMOUNT).
  - Show the Op LFO as a 17th card in the LFO strip on MOD > ENV/LFO. It can use the same `voiceEg` controls, and is
    visible only while some oscillator uses the Operator EG.
  - Show the pitch envelope as a card in the envelope pool.
  - Move PITCH / LFO out of the operator tab row and into the operator card's header as a "VOICE PITCH & LFO" button.
- **Native (changes the import, not the factory sound if done carefully).** On import, map the DX7 LFO to a pool LFO and
  route it through the matrix to "OscN Pitch" with PMS as depth. Map the pitch EG to a pool envelope with a "Rate/Level"
  mode. This is larger; do it only if the read-only sources still feel separate.

**Evidence.**
- `dx7/09-FM-pitch-lfo.png`.
- `integration-extra/strings/09-FM-pitch-lfo.png`.
- `src/gui/ParamInfo.h:405-418` ("the DX7's LFO").
- `dx7/08-MATRIX.png`: the source list has no Op LFO.

### 4. High: nothing on the Operator EG is modulatable, and the wheel and pressure do nothing on 288 DX7 presets

**What's wrong.**
- None of the 102 per-operator EG parameters, the 15 voice parameters or KEY OFFSET is a modulation destination
  (`Modulation.h:291-480` has no `_eg_` or `opeg_` entry).
- On the operator card they sit as ordinary knobs next to LEVEL, FINE and SEMI, which are destinations. Dropping a chip
  on R1 or OUTPUT does nothing, while the same drop works on LEVEL two knobs up.
- The DX7 import routes only the four macros (`src/Dx7Presets.h:136-158`). The mod wheel, the classic DX7 vibrato
  control, is dead on every DX7 preset (`dx7/08-MATRIX.png`: six macro rows, no WHL).

**Why it feels separate.** "Everything is modulatable" is the synth's promise (rings on all 408 knob destinations). The
Operator EG breaks it silently.

**Fix.**
- Append destinations in `src/dsp/Modulation.h` for at least each operator's `_eg_out` (output level, "Osc N Op
  Output"), `_eg_r1`/`_eg_r4` (attack and release rate), `opeg_lfo_pmd`/`_amd`/`_speed` and `opeg_pitch_l1`. The engine
  reads the voice at note-on; block-rate is enough, as for other parameter destinations, and `Dx7Engine` would need to
  re-read the levels per block.
- Until then, knobs that are not destinations should look different: no ring track, and a tooltip line "not
  modulatable", as the Airwindows knobs already do.
- In `Dx7Presets.h::values`, add `route (wheel → opeg_lfo_pmd, 1.0)`, the DX7's default wheel-to-pitch-mod assignment,
  and aftertouch to the same. Check fingerprints: no sound change without wheel input.

**Evidence.**
- `dx7/09-FM.png`.
- `dx7/08-MATRIX.png`.
- `src/dsp/Modulation.h:291-480`.

### 5. High: the Operator EG can only be reached from the FM page; the OSC and PLAY cards hide an operator's real settings

**What's wrong.**
- The OSC page's card shows "AMP ENV: Op EG" (`OscPage.h:76`) with no graph and no controls, and no link to them.
- A ratio-tuned operator's RATIO (x1.000, x14.000) and FIXED Hz appear only on the FM page. The OSC card and PLAY's OSC 1
  card show "SEMI 0 st", which reads as "untuned".
- The folded headers on OSC say "WAVETABLE - folded to fit" for all six operators. Nothing says "carrier, x1.00, Op EG".
- The same choice is labelled **AMP ENV** on the OSC page and **ENVELOPE** on the FM page.

**Why it feels separate.** An operator is an oscillator in this synth. But its tuning and envelope exist only in the FM
page's private card, so the OSC page shows a different, wrong picture of the same oscillator.

**Fix.**
- In `src/gui/pages/OscPage.h`, when `_tune` is Ratio or Fixed, show RATIO or FIXED in place of SEMI's slot, using the
  FM page's `KnobControl`. Do the same in the PLAY card (`src/gui/pages/MainPage.h`).
- When `_amp_env` is Op EG, add an "EDIT OP ENV" pill beside the menu that calls `showPage ("FM")` and
  `selectOperator (i)`.
- Folded header text: "SINE · x1.00 · OP ENV · MOD/OUT".
- Use one label for `_amp_env` everywhere: "ENVELOPE" (it isn't only amplitude on an operator).

**Evidence.**
- `dx7/03-OSC.png`: "AMP ENV Op EG", SEMI 0 st, no ratio, six identical folded headers.
- `dx7/01-MAIN.png`.
- `FmInputPages.h:179` vs `OscPage.h:76`.

### 6. High: 25 of the 32 DX7 algorithms show "CUSTOM - matches no algorithm here", and the DX7 algorithm number is gone

**What's wrong.**
- The grid has 16 house algorithms. Only seven are DX7 layouts, named "DX 1", "DX 5 Keys", "DX 7", "DX 16", "DX 19",
  "DX 22" and "DX 32 Organ" (`src/dsp/FmAlgorithms.h:38-44`).
- A DX7 voice on any of the other 25 algorithms reads "CUSTOM - matches no algorithm here" (`FmInputPages.h:607`).
  Example: STRINGS 1, algorithm 2.
- The match also requires the feedback route to exist (`State.cpp:262-265`), so a DX7 voice with feedback 0 never lights
  even its own house algorithm **(inferred)**.
- UI review 4 (S14) asked for "DX7 ALG n". Commit 1d306f6 removed that label when DX7 mode went, and nothing replaced it.
- The house cell named "DX 5 Keys" is DX7 algorithm 5, but it sits in cell 11 of the grid.

**Why it feels separate.** For a DX7 user the algorithm number is the voice's identity. The grid reads as if it was
built for a different synth, and "matches no algorithm here" sounds like an error.

**Fix.**
- In `IlanaSynthAudioProcessor::findMatchingFmAlgorithm` (`src/processor/State.cpp:251`), also test the routing against
  `Dx7::routing (a)` for a = 0..31 (`src/dsp/Dx7Engine.h:146`). Treat "no feedback route" as matching when the DX7
  feedback level is 0.
- `getAlgorithmLabel` then shows "DX7 ALGORITHM 2" or "DX 5 KEYS = DX7 ALGORITHM 5".
- Give `FmAlgorithmStrip` (`src/gui/FmWidgets.h`) a second page, "DX7 1-32", using the same cell drawing.
  Clicking one applies the DX7 routing.
- Rename the house cells "DX7 1", "DX7 5", ... (labels only; the grid order is unchanged).
- Replace "CUSTOM - matches no algorithm here" with "CUSTOM ROUTING", in text2 colour rather than as a warning.

**Evidence.**
- `integration-extra/strings/09-FM.png` ("CUSTOM - matches no algorithm here" on a factory DX7 voice).
- `neuro/09-FM.png`: the same label on an ordinary two-oscillator FM bass.

### 7. Medium-High: the Operator EG speaks DX7, not ilanaSynth (raw 0-99 numbers, different names for the same ideas)

**What's wrong.**
- **Units.** Rates and levels show bare 0-99. The LFO's SPEED shows "34" and DELAY "33": `ParamInfo.h:199-201` forces
  plain numbers, although its own help text says SPEED is "about 0.06 to 49 Hz". Every other envelope and LFO shows
  ms, s or Hz.
- **Names.** The same idea has a different name in the synth's own envelopes and LFOs:

  | Synth envelopes / LFOs | Operator EG |
  |---|---|
  | KEY RATE | RATE KEY |
  | RETRIG | KEY SYNC |
  | RATE | SPEED |
  | SHAPE | WAVE |
  | LEVEL | OUTPUT |
  | TENSION | (no equivalent) |
  | "Env" | "EG" |

- **Jargon.** AMS, BREAK, L/R CURVE "-LIN/+EXP" and "PITCH SENS" are DX7 terms with no ilanaSynth equivalent explained
  on the card.
- **ASCII.** "PITCH +/-3.0 st" (`FmInputPages.h:143`), where step 13.6 moved help text to ±.

**Why it feels separate.** The knobs look native, but their numbers and words belong to a 1983 front panel.

**Fix.**
- **Display only** (saved values stay 0-99 for import fidelity): in `src/gui/ParamInfo.h`, show each rate as the time
  the stage takes from the previous level at C3, computed with `Dx7::Envelope` as the graph already does, e.g.
  "R1 96 · 4 ms". Show L1-L4 as dB below full ("L2 75 · -18 dB"), SPEED in Hz, and DELAY in s.
- **Labels** in `FmInputPages.h:188` and `:281`: KEY RATE (matching the envelope page), RETRIG, RATE, SHAPE, "AMP MOD"
  for AMS, "SCALE KEY" for BREAK.
- Use "Op Env" in place of "Op EG" in the UI.

**Evidence.**
- `dx7/09-FM.png`, `dx7/09-FM-pitch-lfo.png` vs `dx7/06-ENV-LFO.png` (KEY RATE, ms values).
- `src/gui/ParamInfo.h:199-206`.

### 8. Medium: the operator card has two controls for each of level, depth, velocity and key scaling

**What's wrong.** With the Operator EG, the card shows:
- **LEVEL** (50 % is "the DX7's own level", `Dx7Presets.h:47-51`, `Voice.h:705` `dx7LevelScale = 2`) **and OUTPUT**
  (0-99). LEVEL means "half volume" on every other oscillator but "unity" here.
- **KEY LVL** (dB/oct, the synth's) **and** BREAK / L DEPTH / R DEPTH / L CURVE / R CURVE (the DX7's).
- **VEL** (0-7). The synth's envelope VEL and `amp_velocity` (which the import zeroes) are separate again.
- **Matrix route amounts** fixed at 100 % on DX7 voices, while modulation depth really lives in each modulator's OUTPUT.

**Why it feels unwieldy.** Two knobs per concept, and a user can't tell which one wins.

**Fix.**
- With Op EG selected, hide KEY LVL and show LEVEL relabelled "TRIM" with 0 dB at its centre. A display formatter in
  `ParamInfo.h` reads "0 dB" at 0.5 when `_amp_env` is Op EG.
- Add a one-line hint in the card: "depth = the modulator's OUTPUT; the matrix amount scales it".
- Alternatively, make OUTPUT the knob in LEVEL's position and move LEVEL to the scaling row.

**Evidence.**
- `dx7/09-FM.png`: LEVEL 50 %, KEY LVL 0.0 dB/oct, OUTPUT 99, and BREAK / DEPTH on one card.

### 9. Medium: the Operator EG graph is a picture, not an editor

**What's wrong.**
- `OperatorEgGraph` (`FmInputPages.h:10-160`) only paints. It has no drag handles, no time axis, no playhead, and no
  per-stage markers.
- Every other envelope in the synth is edited by dragging its graph and has time ticks and a playhead dot (UI review 4,
  batch C).
- The graph's top-left reads "C3 VEL 100" with no explanation.

**Fix.**
- Make the four break points draggable: horizontal is the rate, vertical is the level, one `beginEdit` per gesture, as
  in `src/gui/EnvelopeDisplay.h`.
- Add the same time ticks and a live playhead from the voice's stage.
- Reuse `EnvelopeDisplay`'s painter for axes and handles so the two look the same.

**Evidence.**
- `dx7/09-FM.png` vs `dx7/06-ENV-LFO.png`.

### 10. Medium: FB TYPE names and glyphs are confusing, and FB TYPE shows on operators that have no feedback

**What's wrong.**
- FB TYPE offers "Filtered", described as "smoothed like a DX7" in README:175 and the help text, **and** "DX7", "the
  DX7's own two-sample average" (`Voice.h:55-59`). Two options both claim to be the DX7.
- In the matrix, diagonal cells show "FB", "FB~", "FB<>" or "FB DX" (`FmInputPages.h:403-404`). In six-operator compact
  cells the label overlaps the knob ("FB D…" under the dial).
- FB TYPE is shown, and not dimmed, on every operator, including the five in a DX7 voice that have no feedback route.

**Fix.**
- Rename the labels (display only): "Plain", "Smooth", "Cross pair", "DX7". Drop "DX7-style" from Filtered's help.
- In compact cells, put the type glyph under the value, or show it only on hover.
- Add an `effectRules` entry: FB TYPE dims with "no feedback route" when `fm_fbN` is 0 (`FmInputPages.h`, the same
  rule mechanism as `EnvLfoPages.h:57`).

**Evidence.**
- `dx7/09-FM.png`: diagonal cells, and FB TYPE "DX7" on OSC 1, which has no feedback.

### 11. Medium (inferred): clicking an algorithm on a DX7 voice mixes two depth scales and two feedback types

**What's wrong.** `applyFmAlgorithm` (`src/processor/State.cpp:200-249`):
- adds new routes at 35 % and new feedback at 15 %;
- forces new feedback to Filtered.

DX7 routes sit at 100 %, with depth set by OUTPUT, and feedback is DX7 type. After one click, a voice has some routes at
100 % and others at 35 %, and one operator on Filtered feedback while the rest are on DX7.

**Fix.** When any shown operator uses the Operator EG, new routes start at 100 % and new feedback takes that patch's
existing FB TYPE (or DX7).

**Evidence.** `src/processor/State.cpp:237-244`; `src/dsp/FmAlgorithms.h:66-67`.

### 12. Medium (inferred): switching an oscillator to Op EG changes its loudness, differently per mode

**What's wrong.**
- On Op EG, an oscillator's gain is LEVEL × 2 × the EG gain (`Voice.cpp:1611`).
- A wavetable carrier is then scaled by 1/16 (`Voice.cpp:1627`, `dx7CarrierScale`). The string, sample and granular
  paths below skip that scaling.
- So picking "Op EG" on a normal wavetable oscillator at LEVEL 100 % drops it by roughly 12 dB, while the same choice on
  a Physical or Sample oscillator does not. Not heard; worked out from the code.

**Fix.**
- Make the choice level-neutral: when a user picks Op EG on an oscillator, scale so a default EG (OUTPUT 99, L1-L3 99)
  matches the old Amp Env sustain.
- Apply `dx7CarrierScale` on every mode's output path, or none.
- Add a test in `tests/OperatorEgTests.inc` that compares RMS before and after the switch, for wavetable and for a
  string.

**Evidence.** `src/dsp/Voice.cpp:1609-1630`; `src/dsp/Voice.h:702-705`.

### 13. Medium: FM also happens in three places the FM page does not show

**What's wrong.**
- **WARP "FM" and "Ring" on the OSC card.** Osc 1 is modulated by osc 2; the others by osc 1 (`Voice.cpp:1600`,
  `WavetableOscillator.h:44`). Neither appears in the FM matrix or the diagram.
- **WARP "Sync".** Separate from the FM page's HARD SYNC 1>2.
- **RING MOD and HARD SYNC 1>2** on the FM page are two-oscillator controls left over from the three-oscillator synth.
  They sit above a six-by-six matrix and say nothing about oscillators 3-6.

**Why it feels unwieldy.** Three FM systems, two ring mods and two syncs, in two places.

**Fix.**
- Draw warp-FM and warp-Ring as dashed arrows in `FmDiagram.h`, and add a "via WARP" note in the matrix cell they use.
- Move RING MOD and HARD SYNC into the OSC 1 and OSC 2 operator cards as "RING × OSC 2" and "SYNC TO OSC 1" (labels only),
  or caption them "OSC 1 × OSC 2".
- In the WARP menu's help, say "FM from OSC 2 (also see FM page)".

**Evidence.**
- `dx7/09-FM.png` (RING MOD, HARD SYNC 1>2).
- `init/osc-bounce.png` (WARP menu).
- `src/dsp/WavetableOscillator.h:44-53`.

### 14. Medium: matrix destination names don't match the page labels, and the FM routes have two naming schemes

**What's wrong.**
- The 2→1 route is "FM Amount" and OSC 1's feedback is "FM Feedback". Every other route is "FM Osc4 > Osc5" or
  "FM Osc2 Feedback" (`Modulation.h:654-660` vs `:400-407`).
- More generally, destinations read "Filter1 Cutoff", "Osc1 Frame", "Drive Amount" and "Vowel Morph", while the knobs
  read FILTER 1 CUTOFF, OSC 1 FRAME and so on.
- On a DX7 patch the BRIGHT macro rows read "Osc2 Level", "Osc4 Level", "Osc6 Level", but on the FM page those are the
  modulators' LEVEL knobs inside operator cards labelled "OSC 2 AS AN OPERATOR".

**Fix.**
- Add a display-name table in `src/gui/MatrixWidgets.h` that turns destination names into the page's own labels:
  "OSC 2 → OSC 1 FM", "OSC 1 FEEDBACK", "FILTER 1 · CUTOFF", "FX VOWEL · MORPH".
- Keep the saved names; this is the same approach as the DX7 name formatter in step 13.2.

**Evidence.**
- `remap/remap-matrix.png` ("FM Amount", "Filter1 Cutoff").
- `dx7/08-MATRIX.png`.

### 15. Medium-High: card tabs hide what is switched on (the note engines, and FILTER 2 vs WEST)

**What's wrong.**
- `CardTabs` (`src/gui/CardTabs.h`) has no "on" indicator.
- On SEQ, ARP, EUCLID, PROB SEQ and CLIP share one card and one header switch, and that switch belongs to the visible
  tab. Several engines can be on at once:
  - PROB SEQ silently takes over the arp;
  - Euclid in Notes mode rests the arp's steps (`src/processor/Sequencers.cpp:89-96`).
  None of this shows on the tabs.
- On FILTER, the second card's FILTER 2 / WEST tabs work the same way. WEST can be on while FILTER 2 is the tab showing.
  The page opens on WEST only if it was on when the editor was built (`EnvLfoPages.h:41`).

**Why it feels separate.** Each engine is a hidden mode, not part of one note path.

**Fix.**
- Give `CardTabs` a per-tab lit dot: `setTabOn (index, bool)`, driven from a timer, as the PHYSICAL page's osc tabs dim.
- Add a one-line chain readout in PATTERN's header: "ARP → PROB SEQ (PROB SEQ plays the notes)".
- Put the on-switch inside each engine's body. A single header switch reads as if it turns the whole card off.

**Evidence.**
- `neuro/10-ARP-SEQ.png`.
- `init/gen-clip.png`.
- `neuro/05-FILTER.png` vs `init/filter-west.png`.

### 16. Medium-High: WEST is bolted onto Filter 2's card and missing from the signal flow and the response graph

**What's wrong.**
- WEST is a wavefolder and low-pass gate with its own on/off and a PLACE choice ("After Filters" or "Replace Filter 2").
  It is shown as a tab of Filter 2's card.
- The SIGNAL FLOW graph (`src/gui/FilterWidgets.h:380-430`) draws OSC → F1 → F2 → BODY → OUT only:
  - no WEST block, even when it is on;
  - no sympathetic strings, no soundboard;
  - the BODY block is drawn in Filter 2's blue (`FilterWidgets.h:417`), while the BODY card is orange.
- The RESPONSE graph ignores WEST's low-pass.
- With "Replace Filter 2", the FILTER 2 tab still shows editable controls that do nothing **(inferred: no effect rule
  found)**.
- WEST uses the accent orange "not a source's colour" (`FilterVectorPhysicalPages.h:121`), the same as BODY, the arp
  and SAVE.

**Fix.**
- In `FilterWidgets.h`'s flow layout, add a WEST block after F2 (or in F2's place). It is clickable to toggle, like BODY.
- In `FilterDisplay.h`, draw WEST's gate as a third, dashed curve when it is on.
- Dim FILTER 2's tab content with "replaced by WEST" via `effectRules`.
- Give WEST its own colour in `IlanaTheme`.

**Evidence.**
- `init/filter-west.png` (WEST on, the flow shows no WEST).
- `neuro/05-FILTER.png`.

### 17. Medium: four different things are called BODY, and the soundboard's mix is labelled "BODY MIX"

**What's wrong.**
1. The FILTER page's **BODY** card (`res_on`, `body_type`: Classic, Bar, Plate…).
2. The filter type **"BODY"** (Comb Body) in the filter grid (`FilterWidgets.h:55`).
3. The physical oscillator's **"BODY & BUZZ"** row (pickup, buzz, rattle, coupling) (`OscPage.h:1291`).
4. The PHYSICAL page's **BODY** card. It mixes `res_on` (labelled "BODY") with the Acoustic Keys soundboard (`sb_on`,
   labelled "SOUNDBOARD"; `FilterVectorPhysicalPages.h:409-412`). On the OSC page the same `sb_on` is "BOARD", and
   `sb_mix` is "BODY MIX" (`OscPage.h:129`).

The INPUT page's "TO BODY" adds a fifth reference.

**Fix.**
- Name by role:
  - "RESONATOR BODY" for `res_*` on FILTER and PHYSICAL (with one shared component);
  - "COMB BODY" in the filter grid;
  - "PICKUP & BUZZ" for the oscillator row;
  - "SOUNDBOARD" / "SOUNDBOARD MIX" everywhere for `sb_*`.
- Files: `OscPage.h`, `FilterVectorPhysicalPages.h`, `EnvLfoPages.h`, `FilterWidgets.h`, `ParamInfo.h`.

**Evidence.**
- `init/physical-page.png`.
- `neuro/05-FILTER.png`.
- `keys/keys-grand-osc-scrolled.png`.

### 18. Medium: the PHYSICAL page repeats half of the physical oscillator card

**What's wrong.**
- The PHYSICAL page's "OSC 1 STRING" card has EXCITE, DECAY, DAMP, SUSTAIN, STIFF, EXCITE POS, PICKUP and HAMMER.
- The OSC card has those **plus** REGISTER, DAMPER, HARDNESS, PICK POS, SLAP, BRIDGE BUZZ, FRET RATTLE, COUPLING, BOW
  PRESS and BOW SPEED.
- So the PHYSICAL page is neither the full editor nor a pure view. A user has to remember which knobs live where.
- The OSC card's hint "OSC › PHYSICAL shows it moving" admits the split.

**Fix.** Pick one:
- Make PHYSICAL the full editor (all controls, in the same STRING / EXCITER / BODY & BUZZ groups, built from the same
  component as `OscPage.h:1280-1300`), with the OSC card showing a compact summary and an "EDIT ON PHYSICAL" link; or
- Keep PHYSICAL as the animated view only, with no knobs.

**Evidence.**
- `init/physical-page.png` vs `keys/keys-grand-osc.png`.

### 19. Medium: the ENVELOPE choice mixes 16 pool envelopes with two editors that live elsewhere

**What's wrong.**
- An oscillator's ENVELOPE / AMP ENV menu lists "Amp Env, Filter Env, F2 Env, Mod Env, Env 5 … Env 16, MSEG, Op EG"
  (`ParameterLayout.cpp:742-748`).
- The MSEG is edited on MOD > STEPS & MSEG and the Op EG on FM. Neither is in the envelope pool.
- "F2 Env" is spelled "FILT 2 ENV" on its card, "FLT2" on its chip and "Filter 2 Env" in the matrix.

**Fix.**
- Section the menu (display only): "Envelopes: Amp, Filter, Filter 2, Mod, Env 5-16 / Shapes: MSEG / FM: Operator Env
  (edit on FM)".
- Rename "F2 Env" to "Filter 2 Env".
- In `src/gui/ParamControls.h`'s combo, add a small "→" jump button when the choice is edited on another page.

**Evidence.**
- `src/processor/ParameterLayout.cpp:742-748`.
- `dx7/03-OSC.png`.

### 20. Medium: STEPS A and B duplicate the LFO panel's step editor on another tab

**What's wrong.**
- The LFO display already edits the Steps shape ("Steps: drag to set steps", `LfoDisplay.h:29`), and its menu can
  convert a shape to 16 steps (`LfoDisplay.h:670`).
- MOD > STEPS & MSEG then gives two more step editors (STEPS A, STEPS B), each re-targeting any LFO. They show "LFO 1
  isn't playing these" when that LFO is not on Steps.

That means two editors for the same data, on two tabs, and one of them often edits steps nobody hears.

**Fix.**
- Remove STEPS A/B from MOD > STEPS & MSEG. Give the LFO panel a taller "steps" view when SHAPE is Steps.
- Give MSEG the freed room (see 21).
- If a second step row is wanted for two LFOs side by side, put it in the LFO panel ("compare with LFO n").

**Evidence.**
- `neuro/07-STEPS.png`.
- `src/gui/pages/SeqPage.h:82-110`.

### 21. Medium: the single MSEG repeats the LFO "Curve" shape, and an unrelated S&H CLOCK knob is parked in its card

**What's wrong.**
- Each LFO can draw a free Curve, with points, bends and a grid.
- The MSEG is one more drawn curve, with RATE and LOOP, which is what a Curve LFO is. It is also an ENVELOPE choice.
- The MSEG card holds "S&H CLOCK" (`SeqPage.h:22`, `clock_div`). That knob sets the division of the separate "Clocked S&H"
  source and has nothing to do with the MSEG.
- The MSEG editor (`SequencerEditors.h:133`) and the LFO Curve editor (`LfoDisplay.h`) are different widgets with
  different gestures.

**Fix.**
- Treat the MSEG as "LFO with one-shot/loop". Either fold it into the LFO pool as a mode ("MSEG" = Curve shape + one-shot
  trigger), or keep the source but use `LfoDisplay`'s curve editor and gestures.
- Move S&H CLOCK next to the Clocked S&H chip's hover card, or into the LFO panel when SHAPE is S&H.

**Evidence.**
- `neuro/07-STEPS.png`.
- `init/lfo-curve.png`.

### 22. Medium: five curve editors with five gestures, and three names for "bend"

**What's wrong.** The synth draws curves in:
- the envelope graph (drag handles, TENSION);
- the LFO Curve (click to add, drag dot on line to bend);
- Draw/Steps (drag across);
- the MSEG (drag points);
- the mod slot's remap editor (points, SHAPES menu), plus a separate per-slot "Curve" bend dragged in the same matrix
  cell (`MatrixWidgets.h:127-170`).

The Operator EG graph is a sixth curve, and read-only. The bend is called TENSION (envelopes), Curve (mod slots) and
"bend" (LFO help).

**Fix.**
- Extract one curve-editing component (points, bends, grid, double-click delete, right-click shapes) from `LfoDisplay.h`.
  Use it in `RemapEditor.h`, `SequencerEditors.h` (MSEG) and, read-write, in the Operator EG graph.
- Call the bend "CURVE" everywhere: display labels for `*_curve` (now "TENSION") and the mod slot.

**Evidence.**
- `remap/remap-editor.png`.
- `neuro/07-STEPS.png`.
- `init/06-ENV-LFO.png`.

### 23. Medium: the LFO SHAPE list is 30 flat entries with near-duplicates

**What's wrong.** `ParameterLayout.cpp:439-445` lists, in one menu:
- "Chaos/Bounce/Pendulum/Spring/Friction (classic)" and the newer Bounce/Pendulum/Spring/Friction;
- "Drunk" and "Drunk Walk";
- "S&H" and "Random S&H";
- "Smooth Random" and "Sine Random";
- "Rossler" and "Henon Map", where 13.6 wanted Rössler and Hénon.

**Fix** (display only; indices stay).
- Group the popup in `src/gui/pages/EnvLfoPages.h` into Basic / Drawn (Draw, Steps, Curve) / Random / Chaos / Physics.
- Move the "(classic)" variants into a "Legacy" submenu that shows only when the loaded patch uses one.
- Fix the diacritics.

**Evidence.**
- `init/lfo-sim-lorenz.png`.
- `src/processor/ParameterLayout.cpp:439`.

### 24. Medium: the chaos LFOs' extra knobs overlap their values, and output B is reachable only from a matrix dropdown

**What's wrong.**
- On a chaos shape the LFO panel squeezes RATE, SMOOTH, SIGMA, RHO, BETA and SEED into two short rows. Each value is
  drawn over its dial ("1.0 Hz" across RATE's knob, "10.00" across SIGMA's).
- Each LFO's second output ("LFO n B", `Modulation.h:44`) has no chip and is not draggable. The scope's "Output B: Y axis"
  label is painted text (`LfoSimView.h:154`). The only way to use it is to pick "LFO 1 B" in a matrix row.

**Fix.**
- In `EnvLfoPages.h`, give the chaos parameters a third row or use the mini knob size so value boxes clear the dials.
  Add the `--uitest` overlap check used for clipped knobs.
- Make the "Output A" and "Output B" labels in `LfoSimView.h` drag sources, and add a "B" sub-chip on the LFO's chip
  while its shape has a second output.

**Evidence.**
- `init/lfo-sim-lorenz.png`.

### 25. Medium: EVOLVE lives on the VECTOR tab, and "drift" means three different things

**What's wrong.**
- EVOLVE ("macros drift in range") moves the eight macros. It has nothing to do with the vector pad, but it fills the
  right third of PLAY > VECTOR.
- The vector card has DRIFT and DRIFT RATE (pad wander).
- The matrix has a "Drift" destination (oscillator analogue drift), which the DX7 voices' DRIFT macro targets.
- Vector X/Y are both mod **sources** and **destinations** under the same names.
- EVOLVE is not a modulation source and draws no ring on the macro knobs it moves **(inferred)**.

**Fix.**
- Move EVOLVE to the macro strip: a small "EVOLVE" toggle per macro, or the 5-8 page's spare row. Draw its motion as a
  ring on the macro knob.
- Rename the vector's DRIFT to "WANDER" and the destination to "Analog Drift" (display names).
- Name the vector source "Vector X out" in chips and the matrix.

**Evidence.**
- `neuro/02-VECTOR.png`.
- `dx7/08-MATRIX.png` (Macro 3 DRIFT → Drift).

### 26. Medium: the "AW" brand prefix and parallel twins in the FX library and the filter types

**What's wrong.**
- The FX library has REVERB and AW REVERB, DELAY and AW DELAY, EQ and AW EQ, COMP and AW DYNAMICS, a generic AIRWINDOWS
  module, and AW TAPE, AW SATURATION, AW CONSOLE, AW LO-FI, AW STEREO and AW MODULATION. Two names are in a squeezed font.
- The filter grid's Airwindows page uses product names ("YNOT", "HOLT", "PEAR", "ANGLE", "X LP", "Z LP") that say
  nothing about the sound.
- Airwindows knobs are not modulation destinations (known, step 6).

**Why it feels separate.** "AW" reads as a separate plugin bundle inside the rack.

**Fix.**
- Label by function and show the origin as a small badge: "TAPE", "SATURATION", "CONSOLE", "LO-FI", "STEREO",
  "MODULATION", "PLATE/ROOM (AW)".
- Merge each twin pair into one family entry with a MODEL menu, e.g. REVERB: Classic / Airwindows. Saved types stay
  separate indices.
- Filter cells: "Z LP" → "SMOOTH LP", "YNOT" → "RESO LP", and so on (display only, `FilterWidgets.h:50-56`).
- Files: `src/gui/pages/FxPage.h`, `FxWidgets.h`, `FilterWidgets.h`.

**Evidence.**
- `init/11-FX.png`.
- `neuro/05-FILTER.png`.

### 27. Medium: FX-splitter crossovers are hidden behind a right-click, shared by every slot, and not modulatable

**What's wrong.**
- Each FX card's header has a FULL / LOW / MID / HIGH / MID / SIDE menu.
- The two crossover frequencies (`fx_split_low`, `fx_split_high`) live in a callout reached only by right-clicking the
  slot row (`FxPage.h:403`, `:959`, `:1002`).
- They are shared by all slots, and they are not destinations.

**Fix.**
- When any slot is banded, show a thin crossover strip above the rack with two draggable markers on a frequency axis,
  as the EQ curve already draws.
- Add the two parameters as destinations.

**Evidence.**
- `init/fx-31.png` (FULL menu).
- `src/gui/pages/FxPage.h:959-1010`.

### 28. Medium: SF2/SFZ loading has no button, and loaded zones are invisible

**What's wrong.**
- A wavetable oscillator has a LOAD .WAV header button. A Sample oscillator has none: samples and SoundFonts load only
  by clicking the display (a popup) or dropping a file (`WaveDisplay.h:277`, `:357-366`).
- After an SF2 loads, the display shows one waveform. There are no key or velocity zones, and START / END / FADE give no
  sign of applying per zone **(inferred)**.

**Fix.**
- Add a "LOAD" header button in Sample mode, opening the same file chooser (`OscPage.h:1191-1193`).
- When a multisample is loaded, draw a zone strip (keys across, velocity up) under the waveform in `WaveDisplay.h`, and
  name the file and zone count in the corner.

**Evidence.**
- `init/osc-bounce.png` (OSC 2 Sample: no LOAD button).

### 29. Low-Medium: the vocoder is the only FX family without a display, and nothing on it is modulatable

**What's wrong.**
- The VOCODER card is knobs only: there is no band meter, while other families got displays in review 4 batch D.
- None of its parameters is a destination (noted as left in step 7).
- Its MODULATOR "Auto" hides that it chooses between the audio input (INPUT page) and the built-in Talk oscillator.
- It is filed under MOTION.

**Fix.**
- Add a band-energy display (the 8-24 band envelopes it already computes) in `src/gui/FxDisplays.h`.
- Append destinations for FORMANT, UNVOICED and MIX.
- Rename "Auto" to "Input, else Talk".
- File it under a "VOICE" family with VOWEL and TALK.

**Evidence.**
- `init/fx-31.png`.

### 30. Low-Medium: GENERATE's single switch turns on only one of its three parts, and its "SCALE" is not the tuning scale

**What's wrong.**
- The GENERATE card has SCALE, STRUM and NOTE SPRAY. Its only switch sits on the NOTE SPRAY rule line (`spray_on`), so
  SCALE and STRUM are always on.
- Its SCALE (snap to a musical scale) shares a word with the Scala tuning "scale" in settings. With a Scala scale
  loaded, GENERATE still snaps in 12-TET steps **(inferred)**.
- There are two "CHANCE" knobs (ARP and NOTE SPRAY) on one page.

**Fix.**
- Give each part its own small switch: SCALE Off is already a menu value; STRUM has DIRECTION Off.
- Rename to "SNAP TO KEY"; label the spray's CHANCE "SPRAY CHANCE".
- If Scala is on, add the note "snaps in 12-TET".

**Evidence.**
- `neuro/10-ARP-SEQ.png`.

### 31. Low-Medium: Scala tuning is only reachable from a settings submenu, with no indicator

**What's wrong.**
- Loading a .scl or .kbm, and switching tuning on, lives in the gear menu (`PluginEditor.cpp:1721-1765`).
- Nothing on screen says a non-12-TET tuning is playing: no status-line tag, and no badge on the keyboard.

**Fix.** Add a "TUNING: <scale name>" tag to the status line next to BPM and VOICES. Clicking it opens the same submenu.
Show it only while tuning is on, or when MTS-ESP is connected.

**Evidence.** `src/PluginEditor.cpp:1721-1735`.

### 32. Low-Medium: three output views, and engine quality and oversampling sit in the scope's header

**What's wrong.**
- The output is shown three times:
  - the waveform strip under the preset name;
  - PLAY's OUTPUT card;
  - the SCOPE overlay.
- The SCOPE overlay's VIEW row also holds the engine **Quality** (Eco / Normal / High) and **Oversampling** (OS, 2x/4x)
  (`ScopeDisplay.h:128-144`). These are sound-engine settings, also in the gear menu, and they read as scope display
  options next to WAVE / SPEC / BOTH.

**Fix.**
- Remove Quality and OS from the scope row (keep them in settings), or move them to a labelled "ENGINE" group at the
  right of the scope header.
- Keep the strip and the SCOPE overlay. PLAY's OUTPUT card could become the scope docked (step 14 lists "the scope as a
  dock" as left).

**Evidence.**
- `init/scope-panel.png` ("Normal", "OS", "2x" beside BOTH).

### 33. Low: BOUNCE means three things

**What's wrong.** BOUNCE is:
- the oscillator header button that resamples the patch;
- an LFO physics shape, plus "Bounce (classic)";
- the physics LFO's "BOUNCE" knob (`EnvLfoPages.h:793`).

Bounced samples are also named "Bounce C4".

**Fix.** Rename the header button "RESAMPLE" (`OscPage.h:145`, `:1448`; tour entry `TutorialOverlay.h:41`). The sample
name can stay.

**Evidence.**
- `init/osc-bounce.png`.
- `init/lfo-bounce.png`.

### 34. Low: the exciter list mixes string exciters, electric-piano models and a feedback guitar, with a "(classic)" twin

**What's wrong.**
- EXCITE offers Burst, Noise, Saw, Pulse, Bow, "Hammer (classic)", Osc In, Tine, Reed, Piano and Feedback
  (`ParameterLayout.cpp:371`). "Hammer (classic)" sits beside the newer Piano hammer.
- Tine and Reed are electric-piano models. The display then titles itself "REED PICKUP - charge vs reed position",
  while the PHYSICAL page still calls it "OSC 1 STRING".
- Osc In (FM into a string) is explained only in a tooltip on the FM page's "NO FM IN" column.

**Fix** (display only).
- Group the menu: "Strike: Burst, Noise, Hammer, Piano / Drive: Saw, Pulse, Osc In (FM) / Bow / Electric: Tine, Reed /
  Feedback guitar".
- Rename "Hammer (classic)" to "Hammer (bright)".
- Title the PHYSICAL card by model ("OSC 1 REED", "OSC 1 TINE").

**Evidence.**
- `keys/03-OSC.png`.
- `keys/keys-grand-osc.png`.

### 35. Low: the tour still advertises "DX7 MODE"

**What's wrong.**
- The "New in 1.3" chip reads "DX7 MODE + BANKS: … the FM page plays them with their own envelopes"
  (`TutorialOverlay.h:29`).
- DX7 mode no longer exists, and the chip is the first thing a returning user reads about it.

**Fix.** "OPERATOR ENV + DX7 BANKS: any oscillator can use the DX7's envelope (ENVELOPE › Op Env); DX7 voices load as
normal patches."

**Evidence.**
- `integration-extra/epiano-small/00-tutorial.png`.

### 36. Low: DX7 voices still form an island in the browser

**What's wrong.**
- The 288 voices sit in their own "DX7" category, kept out of All by default (a review 4 choice, kept here).
- They all carry "(ROM1A)" suffixes (left from 13.2) and the identical macro names BRIGHT / TONE / DRIFT / SPACE.
- .syx files import only through "…" › Import DX7 / Dexed bank. Dropping a .syx on the window does nothing (no
  `isInterestedInFileDrag` outside `WaveDisplay.h`).
- The import writes 32 files and leaves the browser where it was **(inferred)**.

**Fix.**
- Keep the DX7 category, but tag each voice with a real category (Keys, Bass, Brass, Organ, Bell, Pad…) from its name.
  A small table in `src/Dx7Presets.h` is enough. Then a "Keys" filter finds E.PIANO 1 with "Show DX7 voices" ticked.
- Accept .syx drops on the editor.
- After an import, open the browser on the new bank.

**Evidence.**
- `extras/extra-browser-dx7.png`.
- `src/processor/Dx7.cpp:63-93`.

### 37. Low: an off oscillator looks off in the FM diagram but live in the matrix

**What's wrong.**
- On "Felt Hammer Board", OSC 2 and 3 are off. Their diagram nodes are dimmed.
- Their matrix rows, OUT switches (lit orange) and operator tabs look fully active, and their cells can be edited.

**Fix.** In `FmInputPages.h::paint` and `refreshFmInputs`, treat `_on == 0` like the "no FM in" state:
- dim the row and column;
- caption the row head "OSC 2: OFF";
- grey the operator tab.

**Evidence.**
- `keys/09-FM.png`.

### 38. Low: the PITCH / LFO tab sits among the operator tabs

**What's wrong.**
- "PITCH / LFO" is a seventh pill in the OSC 1 … OSC 6 row, though it is voice-wide (`FmInputPages.h:298-301`, `:671`).
- Its card header "OPERATOR EG PITCH AND LFO - every oscillator on the Operator EG" is long and says "EG" again.

**Fix.** See 3: either move it into the synth's pools, or make it a header button on the operator card, "VOICE PITCH &
LFO", separated by a rule.

**Evidence.**
- `dx7/09-FM-pitch-lfo.png`.

### 39. Low: KEY OFFSET and a DX7 voice's transpose live in different places

**What's wrong.**
- The import writes a voice's TRANSPOSE into every operator's SEMI (`Dx7Presets.h:96-99`). KEY OFFSET on the PITCH / LFO
  tab moves only the keyboard and rate scaling.
- Transposing a DX7 voice means turning six SEMI knobs, and turning KEY OFFSET (which looks like a transpose) changes
  only the timbre.

**Fix.**
- Rename KEY OFFSET "SCALING KEY SHIFT" (display label, `FmInputPages.h:290`).
- Add a hint line: "transpose with the oscillators' SEMI or the global octave".

**Evidence.**
- `dx7/09-FM-pitch-lfo.png`.

### 40. Low: matrix feedback amounts don't use DX7 numbers on DX7 voices

**What's wrong.** A DX7 feedback of 6 imports as 25 % and 7 as 50 % (`Dx7Presets.h:117-118`). A DX7 user reads 0-7.

**Fix.** When the operator's FB TYPE is DX7, show the cell value as "FB 6 · 25%" in `ParamInfo.h`'s formatter for
`fm_fbN` (display only).

**Evidence.**
- `dx7/09-FM.png` (OSC 6 → OSC 6: 25 %).

### 41. Low: the CUSTOM label on ordinary patches

**What's wrong.**
- "CUSTOM - matches no algorithm here" appears on most normal patches. For example, Neuro Wobble: 2→1 with OSC 2 and 3
  audible.
- Clicking any six-operator algorithm silently adds oscillators 4-6 (`State.cpp:225-227`).

**Fix.**
- See 6 for the label.
- Show an "adds OSC 4-6" note in the tooltip of six-operator cells, and in the hover state of cells needing more
  oscillators than are shown (`FmWidgets.h`).

**Evidence.**
- `neuro/09-FM.png`.

---

## What already feels native (keep it)

- **DX7 voices as patches.** Plain parameters, undo, EDITED, save and A/B all work on them. There is no hidden child
  state, and old DX7-mode patches convert on load (`State.cpp:568-600`).
- **WEST's strike source.** It takes any modulation source by name (`ParameterLayout.cpp:950-955`). That is the right
  pattern for Op EG and EVOLVE to copy.
- **Physical controls as destinations.** The physical oscillator's string, exciter and body controls are mod
  destinations, appended cleanly.
- **The FM matrix's "NO FM IN" columns.** They explain sample, granular and string limits in place.
- **The PHYSICAL page's empty state.** "OSC 1 plays a wavetable… SWITCH TO PHYSICAL" is the model for how the FM page
  could explain an operator on Amp Env vs Op EG.
- **The vector pad.** Its "VECTOR OFF - switch on to mix the corners" overlay and greyed dead corners read as one system.

## Suggested batches

1. **DX7 correctness:** findings 1, 6 (matching), 10 (dimming), 11, 12, 35.
2. **Operator EG as a native envelope:** findings 2, 3 (read-only sources, pool cards), 4, 5, 7, 8, 9, 19, 38, 39, 40.
3. **Hidden state and duplicates:** findings 15, 16, 18, 20, 21, 22, 37.
4. **Names:** findings 13, 14, 17, 23, 25, 26, 30, 33, 34 (display-name tables only; IDs and choice orders untouched).
5. **Reach:** findings 24, 27, 28, 29, 31, 32, 36, 41.

Each batch should add `--uitest` checks in `tools/Snapshot.cpp`, as review 4 did, for example:
- no overlapping operator nodes;
- the Amp badge absent when every oscillator uses Op EG;
- every DX7 factory voice lights an algorithm;
- no knob value box overlapping its dial on chaos LFO panels.
