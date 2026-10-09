# UI review 7: integration audit (cycle 2)

Date: 2026-10-04. Branch `claude/project-thread-smvgfk` at 2a90bcd (review 6 cycle 1 merged: packages P1 to P7, the type
scale-up). No code was changed.

**Question asked:** after the review 6 fix round, does every special feature feel native to the synth's normal engines,
or is it still bolted on? The DX7 / FM integration comes first. Then every other special system. Then whether the seven
packages, built in parallel, look like one product.

**Evidence.**
- Screenshots in `/home/user/shots7/{init,neuro,dx7,keys,small,dx7-small,extras,remap,confirm}/`.
- New renders in `/home/user/shots7/integration-extra/`:
  - `epiano-fm/`, `strings-fm/`, `tubbells-fm/`: the FM page on "E.PIANO 1", "STRINGS 1" (DX7 algorithm 2) and "TUB
    BELLS". Each folder has every operator, KEYS & VELOCITY, PITCH & LFO, the voice switched to DX7 algorithm 1, and
    WARP FM (`ILANA_SNAPSHOT_FM=1`).
  - `strings/`: the full set for STRINGS 1.
  - `epiano-p1/`: the modulation set (`ILANA_SNAPSHOT_P1=1`) on E.PIANO 1.
  - `keys-filter/`, `init-filter/`: only the FILTER page (`ILANA_SNAPSHOT_PAGES=FILTER`), so the snapshot tool has not
    yet changed the patch. `flow-crop.png` in each is the SIGNAL FLOW, cropped.
  - `crop-*-05-FILTER.png`: SIGNAL FLOW crops from the standard sets.

Code references are `file:line` on this branch. A claim marked **(inferred)** comes from reading code and was not seen
on screen or heard.

**Snapshot caveats.** These are tool artefacts and are not reported:
- The tool turns OSC 1 physical and back on the OSC page, so EDITED shows after `03-OSC`, and Felt Hammer Board's
  PHYSICAL shot says OSC 1 "plays a wavetable".
- It turns on sympathetic strings (`tools/Snapshot.cpp:5998`), so STRINGS appears in later SIGNAL FLOW shots.
- The P1 set adds routings to show rings, so the macro card's five BRIGHT targets are the tool's.
- A revealed pool shows as "ENV +14".

**Settled, not re-argued** (`docs/REVIEW-PLAN.md`, HANDOFF "not done on purpose"):
- one FX instance per type;
- no level-neutral switch to the Operator Env;
- crossovers and vocoder knobs are not modulatable;
- no MSEG grid/snap and no STEPS count (new audio parameters).

I argue for none of these except where a finding says so.

---

## Verdict

**The review 6 round did most of what was asked, and the DX7 voice now reads as part of the synth on the surfaces people
see first.**

What works:
- The FM page shows "DX7 ALGORITHM 5" over all 32 DX7 algorithms, on two pages.
- The diagram stacks carriers under modulators with no overlap.
- The operator card reads ATTACK / DECAY / RELEASE in ms and s. Its graph is draggable and has a playhead.
- PLAY and OSC show RATIO, CARRIER / MOD › 1 and OP ENV for each operator.
- The wheel and pressure move vibrato.
- The browser files the 288 voices by sound.
- WEST has its own card, colour, response curve and flow block.
- Steps and MSEG live in the LFO pool.
- The note engines show a chain readout ("KEYS › STRUM › ARP › VOICES").

That is a real step change from review 6.

Where it is still bolted on, the problems are now subtler, and two are plain bugs:
- **One word, two parameters.** On a DX7 voice, "LEVEL" on the FM page is the Operator Env output (dB). "LEVEL" on PLAY
  and OSC is the oscillator level (%), which the FM page calls TRIM and hides on a second tab. The matrix and the
  macros target the second.
- **The Operator Env's time knobs run backwards.** Turning ATTACK, DECAY or RELEASE clockwise makes the stage *shorter*,
  the opposite of every other envelope knob in the synth.
- **The DX7 SPACE macro is dead on all 288 voices.** The import puts a reverb in slot 1 but never switches it on, so
  every DX7 voice shows "SPACE !".
- **SIGNAL FLOW drops F1 and F2** on every patch with a soundboard or sympathetic strings (every acoustic-keys preset).
  WEST's slot is reserved even when WEST is off, and the filters are squeezed to zero width.
- **The Op LFO, Op Pitch and OP ENV entries in the MOD pools are link pills, not pool cards.** Every click jumps to
  another page. OP ENV sits in the envelope row but is not a modulation source at all.
- **Many knobs that look modulatable are not, and nothing says so.** This covers seven of the nine Operator Env stage
  knobs, every Airwindows knob, the vocoder, the physics and chaos parameters, and EVOLVE. A dropped chip does nothing,
  and the knob is drawn exactly like one that works.

The other special systems mostly made it, but leftovers remain:
- BOUNCE still means three things.
- BODY is still four labels.
- "Hammer (classic)" is still in the exciter list.
- Sample mode still has no LOAD button and no zone view.
- EVOLVE still lives on the VECTOR tab.
- Airwindows is a sub-shelf in each FX family rather than a model of the family's effect.

**The seven packages mostly look like one product.** They share the type scale, the colours, the knob, the card header
and the chips. Where they differ is in the second layer:
- three styles of on/off placement;
- three styles of tab "on" indicator;
- two caption grammars;
- two ratio glyphs;
- two names for step count;
- three new widgets used once each (the BLEND pill, the EVOLVE mini-slider, the OP pills).

Fix findings 1 to 9 before calling cycle 2 done. Then take the naming and consistency passes as one batch.

---

## Fixed since review 6 / still open (I6 findings)

| I6 | Item | Status in this build | Note |
|---|---|---|---|
| 1 | FM diagram hides OSC 1/2 on Op EG patches | **Fixed** | Stacked by depth, no overlap (`epiano-fm/fm-1.png`). New: small nodes and a card that jumps (findings 10, 11). |
| 2 | AMP ENV claims to shape DX7 voices | **Partly** | "unused (Op Env)" badge, "Not used" note and EDIT OP ENV on PLAY. AMP ENV is still the tab that opens, fully editable (finding 8). |
| 3 | DX7 pitch EG / LFO a private system | **Partly** | Op LFO and Op Pitch are sources and chips, with pills in the pools. They are still edited only on FM, are filed under "Performance and more", and the Op LFO has no picture (findings 7, 16, 18). |
| 4 | Operator EG not modulatable, wheel dead | **Partly** | Wheel and pressure to Op LFO pitch depth on all DX7 voices. Thirteen destinations appended. Most stage knobs are still silent drop targets (finding 5). |
| 5 | OSC/PLAY hide an operator's real settings | **Mostly fixed** | RATIO, CARRIER / MOD › n, OP ENV, EDIT OP ENV. New LEVEL clash (finding 2). |
| 6 | 25 of 32 algorithms "CUSTOM" | **Fixed** | DX7 1-16 / 17-32 pages, "DX7 ALGORITHM n". |
| 7 | Op EG speaks DX7 (0-99, names) | **Partly** | Times in ms/s, levels in dB, RATE in Hz, the synth's stage names. Raw 0-7 / 0-99 scales remain, and the time knobs run backwards (findings 3, 12, 13). |
| 8 | Two controls per concept (LEVEL/OUTPUT, KEY LVL) | **Partly** | On the FM card, OUTPUT became LEVEL and LEVEL became TRIM. Every other page still calls TRIM "LEVEL" (finding 2). |
| 9 | Op EG graph a picture | **Fixed** | Draggable points, time ruler, playhead (`OperatorEnvDisplay.h:371`), hint line. |
| 10 | FB TYPE names, glyphs, dimming | **Fixed** | Plain / Smooth / Cross Pair / DX7. "FB 6 · 25%". Dimmed without a loop. |
| 11 | Algorithm click mixes depth scales | **Fixed / new issue** | 100 % routes and kept FB type. The feedback *level* is reset to a default (finding 9). |
| 12 | Op EG switch changes loudness | Not done on purpose | |
| 13 | FM in three places | **Fixed** | Dashed WARP FM bow in the diagram. "OSC 1 × OSC 2" caption on RING MOD and SYNC. |
| 14 | Matrix names ≠ page labels | **Mostly fixed** | "Filter 1 › Cutoff", "FM › OSC 2 → OSC 1". The Op entries are left out (finding 17). |
| 15 | Card tabs hide what is on | **Fixed** | Ring/dot per tab, chain readout, in-body switches. |
| 16 | WEST bolted on | **Mostly fixed** | Own card, colour, response curve, flow block, "Replaced by WEST" overlay. A flow layout bug drops F1/F2 (finding 1). |
| 17 | Four BODYs | **Partly** | SOUNDBOARD / SOUNDBOARD MIX on OSC, RESONATOR BODY on PHYSICAL. FILTER still says BODY, the filter type is "BODY", the flow says BOARD (finding 26). |
| 18 | PHYSICAL repeats half the card | **Fixed** | One control list, "the controls of its OSC card". |
| 19 | ENVELOPE menu mixed | **Fixed** | Sections ENVELOPES / SHAPE / FM PAGE, "Filt 2 Env". |
| 20 | STEPS A/B duplicate | **Fixed** | Steps edited in the LFO panel. |
| 21 | MSEG duplicates Curve, S&H CLOCK parked | **Mostly fixed** | The MSEG is a card in the LFO row. It is still its own editor widget and has no GRID (finding 30). |
| 22 | Five curve editors, three words | **Partly** | One word, "curve". The envelope, LFO, MSEG, remap and Op Env editors are still separate widgets. |
| 23 | 30 flat LFO shapes | **Fixed** | Basic / Drawn / Random / Chaos / Physics, a Legacy submenu, Rössler and Hénon. |
| 24 | Chaos knobs overlap, output B hidden | **Fixed** | Clean layout, "B" sub-chip, "A: X axis / B: Y axis" pills. |
| 25 | EVOLVE on VECTOR, three "drift"s | **Partly** | WANDER on the pad. EVOLVE is still on PLAY › VECTOR, and the destination still reads "Voice › Drift" (finding 21). |
| 26 | AW prefix, twins | **Partly** | Named by job, AIRWINDOWS badge, filter types "Smooth LP (Z)". The twins are still separate shelves (finding 28). |
| 27 | Crossovers behind right-click | **Fixed** | SPLIT BANDS group with draggable crossovers. Not modulatable, on purpose. |
| 28 | SF2/SFZ: no button, no zones | **Open** | `OscPage.h:1102-1104` shows LOAD only in Wavetable mode. There is no zone strip (finding 24). |
| 29 | Vocoder: no display | **Fixed** | Band display, "Input, else Talk", VOICE family. Knobs not modulatable, on purpose. |
| 30 | GENERATE switch, SCALE name | **Fixed** | Three boxed parts with their own switches, SNAP TO KEY, "snaps in 12-TET steps". |
| 31 | Scala hidden | **Mostly fixed** | "TUNING: …" tag in the header. Loading is still only in the gear menu. |
| 32 | Quality in the scope header | **Fixed** | Floating scope, VIEW / FREEZE / PEAK HOLD only. |
| 33 | BOUNCE means three things | **Open** | `OscPage.h:261`, `EnvLfoPages.h:908`, `TutorialOverlay.h:42` (finding 25). |
| 34 | Exciter list mixed, "(classic)" | **Open** | Not grouped. "Hammer (classic)" is still listed and is truncated on the OSC card (finding 27). |
| 35 | Tour advertises "DX7 MODE" | **Partly** | Now "DX7 BANKS + OPERATOR EG" and "ENVELOPE: Operator EG", but the menu says "Operator Env" (finding 6). |
| 36 | DX7 island in the browser | **Fixed** | Filed by sound, bank chips, Title Case, Import .syx, drops (finding 41 for leftovers). |
| 37 | Off oscillator live in matrix | **Fixed** | "OSC 3: OFF" rows and columns dimmed. |
| 38 | PITCH / LFO among operator tabs | **Partly** | A "PITCH & LFO" button after a rule, still in the operator pill row. |
| 39 | KEY OFFSET vs transpose | **Partly** | Renamed SCALE SHIFT with a hint, but the hint names a control that does not exist (finding 15). |
| 40 | FB amounts not in DX7 numbers | **Fixed** | "FB 7 · 50%". |
| 41 | CUSTOM label | **Fixed** | "CUSTOM ROUTING", "+1" badges on cells that add oscillators. |

Tally: 19 fixed, 1 fixed with a new issue, 5 mostly fixed, 12 partly, 3 open, 1 not done on purpose.

---

## Findings, worst first

Severity:
- **Critical:** wrong information or a broken display.
- **High:** a feature feels like a separate machine, or a user would be misled.
- **Medium:** friction, duplication or inconsistency.
- **Low:** polish.

### 1. Critical: SIGNAL FLOW loses F1 and F2 on every patch with a soundboard or sympathetic strings

**What's wrong.**
- `SignalFlow::computeLayout` (`src/gui/FilterWidgets.h:583-621`) lays the post-filter blocks out right to left: OUT,
  then the STRINGS/BOARD block, BODY and WEST.
- WEST's block is reserved whenever its PLACE is "After Filters". That is the default, so the block is reserved even when
  WEST is off.
- At the flow card's width (about 300 logical px), those blocks plus their gaps take more than the room left after the
  oscillator column. The area left for the filters goes negative, so F1 and F2 are drawn with no width.
- What the user sees is OSC → WEST → BODY → BOARD → OUT, with the oscillators' wires converging on nothing.
- This happens on Felt Hammer Board and every other preset with the soundboard or sympathetic strings on, with WEST on or
  off.

**Why it matters.** The flow is the one picture of where the filters sit. On the acoustic presets it says there are no
filters, and it shows a disabled WEST as part of the chain.

**Benchmark.** Vital's and Serum 2's routing views never drop a stage. They shrink labels or scroll.

**Fix.**
- Reserve WEST's block only while `west_on` is on, and draw a disabled WEST as a small dimmed stub.
- Compute a minimum filter area (2 × filterWidth + gap) first and shrink `blockWidth` and the post label ("STR+BRD" →
  "+BRD") to fit.
- Add a `--uitest` check that `layout.f1` and `layout.f2` have a positive width and sit inside the bounds, for Felt
  Hammer Board and for a patch with WEST, strings and board all on.

**Evidence.**
- `integration-extra/keys-filter/flow-crop.png`: a clean render with no tool edits. No F1 or F2 block.
- `integration-extra/init-filter/flow-crop.png`: correct on Init.
- `keys/filter-west.png`, `crop-dx7-05-FILTER.png`.

### 2. High: "LEVEL" is two different parameters for one operator, depending on the page

**What's wrong.** On an oscillator that uses the Operator Env:

| Where | "LEVEL" shows | Parameter |
|---|---|---|
| FM page operator card | 0.0 dB / -30.9 dB | `oscN_eg_out` (`FmInputPages.h:121`) |
| FM page, KEYS & VELOCITY tab | **TRIM** 0.0 dB | `oscN_level` (`FmInputPages.h:772-774`) |
| PLAY strip, OSC page | **LEVEL** 50 % | `oscN_level` |
| Matrix | "OSC 2 › Level" | `oscN_level` (the TRIM) |
| Matrix | "OSC 2 › Op Env Level" | `oscN_eg_out` (the FM page's LEVEL) |

- The DX7 import's BRIGHT macro targets "OSC 2/4/6 › Level" (`Dx7Presets.h:153`). Its rings therefore show on PLAY's
  LEVEL knobs, but not on the FM page's LEVEL knob, the one a DX7 user reaches for.
- The diagram's dB caption combines both (`FmOperatorInfo.h:85-96`). PLAY's "50 %" combines neither.

**Why it feels separate.** The FM page renamed things for itself, and the rest of the synth did not follow. A user who
turns LEVEL on PLAY and then looks at LEVEL on FM sees no change.

**Fix.**
- When `_amp_env` is the Operator Env, PLAY's strip and the OSC card show the FM card's LEVEL (`_eg_out`, read with
  `FmOperatorInfo::levelText`) and move `_level` to a TRIM knob, as the FM page does.
- In `ModNames.h`, name `oscN_level` "OSC n › Trim" while that oscillator is on the Operator Env (a dynamic name, as
  `whyDestinationIsIdle` already is), or always "OSC n › Level (trim)" on DX7 voices.
- Longer term: move the import's BRIGHT routes to `_eg_out`, but only if the fingerprints show no change without the
  macro moved.

**Evidence.**
- `dx7/01-MAIN.png` vs `integration-extra/epiano-fm/fm-2.png`: OSC 2 LEVEL is "50 %" vs "-30.9 dB".
- `dx7/07-MATRIX.png`: rows 1-3.
- `integration-extra/epiano-p1/p1-knob-card.png`: "OSC 2 › LEVEL, BRIGHT".

### 3. High: the Operator Env's time knobs turn the wrong way

**What's wrong.**
- ATTACK, DECAY 1, DECAY 2 and RELEASE on the operator card, and the four rate knobs on PITCH & LFO, are the DX7's rates
  (0-99, higher is faster) displayed as times (`FmInputPages.h:117-125`, `EgKnob`).
- So "1 ms" is a full arc, and turning a knob clockwise makes the stage shorter. On STRINGS 1, ATTACK "203 ms" sits at
  three quarters and DECAY 1 "1.7 s" at a quarter.
- Every other envelope in the synth (AMP ENV, ENV 5-16, MSEG) runs the other way.
- Modulation shares the problem. The appended destination is "Op Env Attack **Rate**" (`Modulation.h:486`), so a
  positive amount shortens the attack, and the ring on a knob labelled in ms moves the opposite way to the time.

**Why it matters.** It's the most basic gesture on the page, and it is backwards only here. A user who knows the synth
will get it wrong every time.

**Fix.**
- Display only: give the rate knobs a reversed rotary (`slider.setRotaryParameters` with start > end, or a `KnobControl`
  flag) so clockwise is a longer time. The saved values stay 0-99.
- Name the destinations "Op Env Attack Time" / "Release Time" and flip the sign in the depth display, or keep "Rate" in
  the name and show the ring reversed.
- Apply the same to the pitch envelope.

**Evidence.**
- `integration-extra/strings-fm/fm-1.png` (STAGES row).
- `epiano-fm/fm-pitch-lfo.png`: four "1 ms" knobs at full arc.
- `dx7/06-ENV-LFO.png` for the synth's own direction.

### 4. High: the SPACE macro does nothing on all 288 DX7 voices

**What's wrong.**
- `Dx7Presets.h:88-89` puts a reverb in FX slot 1 at mix 0 "for SPACE". `:156` routes Macro 4 to "Reverb Mix".
- Neither sets `fx_reverb_on`, which defaults to off (`ParameterLayout.cpp:661`).
- Every DX7 voice therefore loads with:
  - the macro strip showing "SPACE !";
  - matrix row 6 dimmed ("1 into a module that is off");
  - the FX page showing a REVERB card that is switched off, with a "4" badge on its MIX.

**Why it matters.** Four macros are the only performance controls a DX7 voice has. One of them is broken out of the box,
and the warning mark on 288 presets makes the import look unfinished.

**Fix.**
- In `Dx7Presets.h::values`, add `set ("fx_reverb_on", 1)`. At mix 0 the reverb should pass the dry signal; confirm with
  the fingerprints **(inferred)**.
- Or route SPACE to `fx_slot1` BLEND with the module on.
- Add a uitest: no factory preset loads with a macro whose every target is idle.

**Evidence.**
- `dx7/10-FX.png` ("REVERB off", MIX badge 4).
- `dx7/07-MATRIX.png` (row 6 dimmed).
- The macro strip in every `dx7/*.png`.

### 5. High: dozens of knobs look modulatable but silently refuse modulation

**What's wrong.** `KnobControl` only leaves the "Drop a mod source here" hint off the tooltip when a knob has no
destination (`ParamControls.h:672-676`). It draws the same ring track, accepts the same chip drop, and does nothing.

Knobs that are not destinations, on the special systems alone:
- **Operator card:** DECAY 1, DECAY 2, PEAK, MID, SUSTAIN, END, KEY RATE. Only ATTACK, RELEASE and LEVEL are
  destinations (`Modulation.h:485-487`).
- **KEYS & VELOCITY:** all eight knobs, including TRIM's twin, KEY LVL.
- **PITCH & LFO:** DELAY, PITCH SENS, SCALE SHIFT and seven of the eight pitch-envelope knobs.
- **Every Airwindows knob,** in eleven modules and the filter page.
- **The vocoder** (known, on purpose).
- **Physics and chaos LFO controls:** HEIGHT, BOUNCE, SIGMA, RHO, BETA (`lfoN_phys_a/b`).
- **EVOLVE** amount and rate.

**Why it feels separate.** The synth's promise, made louder by the new depth rings, is "drag any chip onto any knob".
The special systems are exactly where the promise breaks, without a word.

**Benchmark.** Vital and Serum 2 either make every knob a target or draw non-targets differently.

**Fix.**
- In `KnobControl`, when `ringConfig.destination == 0`:
  - draw no ring track, or a dotted one;
  - refuse the chip drop with a one-line toast, "OSC 1 › Peak can't be modulated";
  - add "Not modulatable" to the tooltip.
- Append destinations for the Op Env's D1/D2 rates and L1-L3 levels (append-only, block-rate like the existing ones).
- Append destinations for `lfoN_phys_a/b` and `macroN_evolve`.
- Add a uitest that counts knobs drawn with a ring track but with destination 0.

**Evidence.**
- `src/dsp/Modulation.h:479-492` (the whole Op Env list).
- No `fx_aw`, `voc`, `phys` or `evolve` entry in `Modulation.h`.

### 6. High: the Operator Env still has three names

**What's wrong.**
- **"Operator Env" / "OP ENV":** the ENVELOPE menu (`ParameterLayout.cpp:748`), the FM card header, the pool pill, PLAY's
  strip, OSC's EDIT OP ENV.
- **"Operator EG":** the OSC card header, "plays its Operator EG (FM page)" (`OscPage.h:110`); the EDIT OP ENV tooltips
  (`MainPage.h:193`, `OscPage.h:353`); the tour, "DX7 BANKS + OPERATOR EG … (ENVELOPE: Operator EG)"
  (`TutorialOverlay.h:28-30`).
- **"Op EG":** PLAY's envelope note, "Not used: the OSCs play their Op EG" (`MainPage.h:818`).
- The tour's instruction names a menu item, "Operator EG", that the menu no longer shows.

**Fix.** Use "Operator Env" / "OP ENV" in all five places. Add a uitest that greps the editor's visible text for "EG".

**Evidence.**
- `dx7/01-MAIN.png`: the note says "Op EG", next to a button reading "EDIT OP ENV".
- `dx7/03-OSC.png`.
- `dx7/00-tutorial.png`.

### 7. Medium-High: OP LFO, OP PITCH and OP ENV in the MOD pools are links, not pool members

**What's wrong.**
- On MOD › ENV / LFO, the DX7 parts appear as thin pills on the section heading line: "OP LFO ~ 5.4 Hz Sine → FM",
  "OP ENV 6 operators → FM", "OP PITCH no pitch movement → FM" (`OperatorPoolCards.h`).
- They are not cards in the LFO and envelope rows, so they cannot be selected and edited in the panel below as LFO 1 or
  ENV 5 can. Every click leaves MOD for the FM page.
- OP ENV sits in the ENVELOPES row, but it is not a modulation source: there are no "Op Env n" sources. It is the only
  thing in that row a user can't drag onto a knob.
- The Op LFO has no waveform picture anywhere. Its PITCH & LFO card graphs only the pitch envelope.

**Why it feels separate.** The pools now say "these exist", but the editing still lives in one private card on another
page. This is the same "second modulation system" as I6-3, with a sign pointing to it.

**Fix.**
- Make OP LFO a card at the end of the LFO row (an `LfoThumbs` card in the FM colour). Selecting it shows `VoiceControls`
  (SHAPE, RATE, DELAY, PITCH/AMP DEPTH, RETRIG) in the LFO panel, with an `LfoDisplay`-style read-only preview of its
  shape.
- Make OP PITCH an envelope card that opens the pitch `OperatorEnvDisplay` in the envelope panel.
- Give OP ENV operator tabs (1-6) in the envelope panel, or keep it as a link but move it out of the source row, with
  the label "edited per operator on FM".

**Evidence.**
- `dx7/06-ENV-LFO.png`, `dx7-small/06-ENV-LFO.png`, `dx7/lfo-pool-full.png`.

### 8. Medium-High: on a DX7 voice, AMP ENV is still the envelope that opens, fully editable

**What's wrong.**
- PLAY's ENVELOPE card opens on AMP ENV with live knobs. A grey note says "Not used". There is no OP ENV tab, only an
  EDIT OP ENV button that leaves the page.
- MOD's envelope row selects AMP ENV by default and shows its full editor under an "unused (Op Env)" badge. When the pool
  is full, the badge truncates to "unus…" (`dx7/lfo-pool-full.png`).
- The AMP ENV chip stays first in the chip bar.

**Why it matters.** A user's first envelope edit on a DX7 voice still does nothing. The badge explains why, but only
after they look.

**Fix.**
- PLAY (`MainPage.h:818` area): when `ampEnvelopeInUse` is false, open an "OP ENV" tab that shows the selected operator's
  `OperatorEnvDisplay` with an operator picker (1-6).
- MOD: select the OP ENV entry first.
- Badge: shorten it to "unused" so it fits the narrow card.

**Evidence.**
- `dx7/01-MAIN.png`, `dx7/06-ENV-LFO.png`, `dx7/lfo-pool-full.png`.

### 9. Medium-High: clicking a DX7 algorithm throws away the voice's feedback level

**What's wrong.** `applyFmRouting` (`src/processor/State.cpp:263-281`):
- zeroes every route that is not in the new algorithm, including the old feedback loop;
- seeds any new route that is at 0 with a default amount, `operatorEnvFeedbackAmount` for a new loop.

STRINGS 1 has feedback 7 (50 %) on OSC 2 under algorithm 2. Clicking algorithm 1 removes it and puts 25 % (feedback 6) on
OSC 6. On a DX7, FEEDBACK is a voice parameter that survives an algorithm change. Here, auditioning algorithms changes
the timbre twice.

**Fix.** Before zeroing, read the strongest existing self-route (amount and type) and write it to the new algorithm's
feedback operator. Add a test: alg 2 → 1 → 2 restores 50 %.

**Evidence.**
- `integration-extra/strings-fm/fm-1.png` ("FB 7 · 50%" on OSC 2) vs `strings-fm/fm-dx7-algorithm-1.png` ("FB 6 · 25%"
  on OSC 6).

### 10. Medium: the operator card jumps and shrinks when the algorithm changes

**What's wrong.**
- The card's height is `min (wanted, area - diagram.getMinimumHeight())` (`FmInputPages.h:549-553`), and the minimum
  depends on the algorithm's depth.
- On algorithm 5 (depth 2), the card starts at y≈567 with full-size knobs.
- On algorithm 1 or 2 (depth 4), it starts at y≈610, and every knob in it shrinks a size.
- Auditioning algorithms makes the whole lower half of the page hop and resize.

**Fix.**
- Give the operator card a fixed height (262).
- Let the diagram scale its node radius to fit a depth-4 stack in the remaining height. There is room: algorithm 1 uses
  a third of the diagram's width.

**Evidence.**
- `epiano-fm/fm-1.png` vs `epiano-fm/fm-dx7-algorithm-1.png`.

### 11. Medium: deep DX7 stacks draw tiny nodes, and node labels change between layouts

**What's wrong.**
- On algorithms 1 and 2, the nodes shrink to about 25 px radius in a box about 800 px wide. Their "MOD" / "OUT" caption
  crosses the circle's edge.
- The feedback indicator is a hollow ring offset to the node's left, overlapping it.
- Node names are "OSC 2" in the shallow DX7 layout and in a two-row stack, but "2" in deep stacks and in the ring layout
  (Neuro Wobble, Felt Hammer Board).

**Fix.**
- In `FmDiagram.h`, size nodes from both width and height and spread the columns.
- Draw feedback as the arc-with-arrowhead loop above the node, as the matrix's ↻ glyph does.
- Always label "OSC n", or always "n", and put the role underneath, inside the circle.

**Evidence.**
- `strings-fm/fm-1.png`, `epiano-fm/fm-dx7-algorithm-1.png`, `neuro/08-FM.png`.

### 12. Medium: raw DX7 numbers still sit beside the synth's units, under the synth's names

**What's wrong.**

| Knob | Shows | The synth's own knob of the same name shows |
|---|---|---|
| KEY RATE (Op Env) | 3 (0-7) | KEY RATE 0 % (AMP ENV) |
| VEL (KEYS & VELOCITY) | 2 (0-7) | VEL 0-100 % |
| AMP MOD | 0 (0-3) | — |
| LOW / HIGH DEPTH | 0 (0-99) | — |
| PITCH SENS | 3 (0-7) | — |
| SUSTAIN / END / MID level at 0 | "Off" | — (a level reads "-∞ dB" elsewhere) |
| SCALE KEY | A-1 | — |

**Fix.** Display only (`ParamInfo.h`):
- 0-7 scales as % of full (KEY RATE 3 → "43 %");
- AMP MOD as "Off / Low / Mid / Full";
- depths as dB at one octave;
- a zero level as "-∞ dB".

Keep the DX7 number in the tooltip, as the rate knobs already do.

**Evidence.** `epiano-fm/fm-1.png`, `epiano-fm/fm-1-keys.png`, `epiano-fm/fm-pitch-lfo.png`.

### 13. Medium: one envelope shape, two sets of stage names, on one page

**What's wrong.**
- The operator envelope's levels are PEAK, MID, SUSTAIN and END. The pitch envelope's are PITCH 1, PITCH 2, SUSTAIN and
  END (`FmInputPages.h:119` vs `:178`).
- It is the same four-rate / four-level DX7 envelope. Both sit under identical ATTACK / DECAY 1 / DECAY 2 / RELEASE
  rows.

**Fix.** Use PEAK / MID / SUSTAIN / END on both (the pitch versions in st), so the second envelope reads as the first one
with a different unit.

**Evidence.** `epiano-fm/fm-1.png` vs `epiano-fm/fm-pitch-lfo.png`.

### 14. Medium: KEYS & VELOCITY tab layout: truncated combos, labels off the baseline, squeezed type

**What's wrong.**
- LOW CURVE and HIGH CURVE combos read "-Line…": the value is cut off.
- Their labels sit about 6 px lower than the knob labels beside them.
- LOW DEPTH and HIGH DEPTH are drawn in the condensed fallback font.

The tab mixes combos and knobs on one nine-column grid (`FmInputPages.h:152-156`), and the combo cell is a knob cell.

**Fix.**
- Give the combos two grid columns, or use short display names ("−Lin", "+Exp" fit at 64 px).
- Align label tops across the row.
- Add a uitest for combo text that is truncated.

**Evidence.** `epiano-fm/fm-1-keys.png`, `dx7/08-FM.png` area.

### 15. Medium: the PITCH & LFO hint points to a control that doesn't exist

**What's wrong.**
- The hint reads "SCALE SHIFT moves only the keys… transpose with each oscillator's SEMI, or the octave on PLAY"
  (`FmInputPages.h:403-404`).
- PLAY has no octave or transpose control, and no such parameter exists.
- Transposing a DX7 voice still means turning six SEMI knobs, which the import set from the voice's TRANSPOSE.

**Fix.**
- Correct the text now ("…with each oscillator's SEMI").
- Then add a voice transpose (append-only parameter, default 0) next to SCALE SHIFT, or a "TRANSPOSE ALL ±" pair of
  buttons that move the six SEMIs in one undo step.

**Evidence.**
- `epiano-fm/fm-pitch-lfo.png`.
- No `transpose` / `octave` parameter in `ParameterLayout.cpp` other than `sub_octave`.

### 16. Medium: the source menu files OP LFO, OP PITCH and MSEG apart from where the pools show them

**What's wrong.**
- The matrix and knob source menus put MSEG, Op LFO and Op Pitch under "Performance and more", with velocity and the
  wheel (`ModNames.h:138-141`).
- The MOD page shows MSEG and OP LFO in the LFO row, and OP PITCH in the envelope row.
- The chip bar puts OP LFO and OP PITCH at its far right, after RANDOM and MSEG.

**Fix.** File MSEG and Op LFO under "LFOs", and Op Pitch under "Envelopes", in `fillSourceMenu`. Order the chips the same
way.

**Evidence.** `dx7/07-MATRIX.png` chip bar; `src/gui/ModNames.h:115-155`.

### 17. Medium: the new destinations skipped the "Module › Control" names

**What's wrong.**
- The matrix reads "Filter 1 › Cutoff", "OSC 2 › Level", "FX Reverb › Mix", but also "Op LFO Pitch Depth" and "Op Pitch
  Env Level 1" in plain form.
- `paramName` (`ModNames.h:388-393`) has no rule for them and falls through to `{ name, {} }`.
- "Op Pitch Env Level 1" is the knob labelled PITCH 1 on the page.
- "Voice › Drift" sits beside a vector pad that now says WANDER, and the DX7 macro is named DRIFT.

**Fix.**
- Add modules "Op LFO" and "Op Pitch" to the list at `ModNames.h:389`, giving "Op LFO › Pitch Depth" and "Op Pitch ›
  Pitch 1".
- Name `drift` "Voice › Analog Drift".

**Evidence.** `dx7/07-MATRIX.png` rows 5, 7, 8.

### 18. Medium: the Op LFO's shape list is not the LFO shape list

**What's wrong.**
- The Op LFO's SHAPE offers Triangle, Saw Down, Saw Up, Square, Sine and S&H, in the DX7's order
  (`OperatorEgParams.h:30`), as a plain combo.
- The pool LFOs use the grouped menu (Basic: Sine, Triangle, Saw Up, Saw Down, Square), and their plain "S&H" is now filed
  under Legacy (`LfoShapeMenu.h:56`).
- The host calls the parameter "Op LFO Wave", while the page says SHAPE.

**Fix.**
- Order the display list as the pool's Basic group, with "Random S&H" as the name (labels only; indices stay).
- Draw the shape in a small preview, as the pool cards do.

**Evidence.** `epiano-fm/fm-pitch-lfo.png`, `init/lfo-steps.png`.

### 19. Medium: PLAY's operator strip shows FRAME, but nothing about the operator's envelope

**What's wrong.**
- Each DX7 operator strip on PLAY has RATIO, LEVEL, FINE and FRAME.
- FRAME does nothing audible on a one-frame sine.
- The strip's thumbnail is the waveform, which is a sine on every operator.
- What distinguishes the six operators, their envelope shapes, is not shown.

**Fix.** On an Operator Env oscillator, swap FRAME for LEVEL in dB (see 2), with TRIM hidden. Draw the operator's Op Env
curve in the thumbnail (the pool card already computes it).

**Evidence.** `dx7/01-MAIN.png`.

### 20. Medium: the OSC page's operator card is mostly irrelevant rows

**What's wrong.**
- The OSC card for an Operator Env sine shows SHAPE (FRAME, WARP, SPECTRAL), PITCH & LEVEL and UNISON. They are spread
  over about 500 px with large empty bands.
- The Op Env itself is one button away.
- EDIT OP ENV only appears in Wavetable mode (`OscPage.h:1119`), although any mode can play the Operator Env.

**Fix.**
- Show the button whenever `usesOperatorEg`, whatever the mode.
- For an operator, add a compact read-only Op Env curve in the SHAPE row's empty right half, clickable to FM.

**Evidence.** `dx7/03-OSC.png`.

### 21. Medium: EVOLVE still lives on PLAY › VECTOR, with its own widget and no switch

**What's wrong.**
- EVOLVE moves the macros, but it fills the right third of the vector page (I6-25 asked for the macro strip).
- Its rows say "MACRO 1", "MACRO 3" rather than the macro's name (TONE, TALK…).
- They use a mini range slider found nowhere else.
- The card has a FREEZE button but no on switch, unlike every other card.
- The macro knobs in the strip show no evolve motion **(inferred)**.

**Fix.**
- Move the EVOLVE rows into the macro card (the macro strip's hover card from P1 already lists destinations), or give each
  macro knob an "EVOLVE" item in its menu.
- Draw the evolve range as a ring on the macro knob.
- Name rows by the macro's label.

**Evidence.** `init/vector-page.png`, `neuro/02-VECTOR.png`.

### 22. Medium: VECTOR X / VECTOR Y chips live in the vector card's header, not in the chip bar

**What's wrong.**
- With VECTOR on, two source chips appear in the card header (`init/vector-page.png`).
- Every other source, including the new OP LFO and OP PITCH chips, appears in the bottom chip bar.
- This is a second chip location used by one card.

**Fix.** Add VEC X / VEC Y to the chip bar while the vector is on. Keep the header chips only as small drag handles, or
drop them.

**Evidence.** `init/vector-page.png` vs `dx7/01-MAIN.png` chip bar.

### 23. Medium: the hidden preset trim is a level control nobody can see

**What's wrong.**
- `output_trim` ("Preset Level", -60 to +12 dB) is set by each factory preset so that MASTER reads 0 dB
  (`ParameterLayout.cpp:1082-1089`).
- It is saved with a patch and not shown on any page, and the MASTER tooltip does not mention it.
- Two patches both reading MASTER 0.0 dB can be 15 dB apart. HANDOFF notes that some trims sit at the +12 dB clamp.

**Fix.**
- Show the trim in MASTER's hover card ("Preset level -6.2 dB"), and in its right-click menu as "Reset preset level".
- In Save As, say "keeps this preset's level".

**Evidence.** `src/gui/ParamInfo.h:1309`; no `output_trim` reference in `src/gui/` beyond ParamInfo.

### 24. Medium: SF2/SFZ still has no LOAD button and no zone view

**What's wrong.**
- In Sample mode the header shows BOUNCE only. LOAD .WAV is hidden for every mode but Wavetable
  (`OscPage.h:1102-1104`).
- The only ways to load a SoundFont are a drop, or a menu reached by clicking the waveform display
  (`WaveDisplay.h:314`).
- After loading, nothing shows the key or velocity zones.
- The tour tells users to drop a file.

**Fix.**
- Show a "LOAD" header button in Sample mode, opening the existing chooser (`WaveDisplay.h:472`).
- Add a zone strip under the waveform: keys across, velocity up, plus the file name and zone count.

**Evidence.** `init/osc-bounce.png` (OSC 2 SAMPLE tab), the code lines above.

### 25. Medium: BOUNCE still means three things

**What's wrong.**
- The OSC header button that resamples the patch is BOUNCE (`OscPage.h:261`, `:1313`).
- There are LFO shapes "Bounce" and "Bounce (classic)".
- The physics LFO has a BOUNCE knob (`EnvLfoPages.h:908`, `:1184`).
- The tour advertises "BOUNCE" for the resampler (`TutorialOverlay.h:42`).

**Fix.** Label the button "RESAMPLE" and the tour chip "RESAMPLE". Sample names can stay "Bounce C4".

**Evidence.** `init/osc-bounce.png`, `init/lfo-physics-12.png`.

### 26. Medium: BODY is still four labels

**What's wrong.**
- The FILTER page card is titled "BODY" (`EnvLfoPages.h:81`), while PHYSICAL's summary calls the same `res_*` module
  "RESONATOR BODY" (`FilterVectorPhysicalPages.h:666`).
- The filter type's short name is "BODY" (`FilterWidgets.h:63`).
- SIGNAL FLOW labels the soundboard "BOARD" or "STR+BRD" (`:657`).

**Fix.**
- Title the FILTER card "RESONATOR BODY".
- Make the filter short name "COMB BODY".
- Make the flow block "BOARD" → "SOUNDBOARD" (or "SB" when narrow).

**Evidence.** `init/filter-west.png`, `init/physical-page.png`.

### 27. Medium: the exciter list is still mixed, and its names disagree

**What's wrong.**
- EXCITE lists Burst, Noise, Saw, Pulse, Bow, "Hammer (classic)", Osc In, Tine, Reed, Piano and Feedback, ungrouped.
- On the OSC card the combo truncates the classic hammer to "Hammer (clas…".
- The display name tables disagree:
  - `PhysicalView.h:226` says "HAMMER (CLASSIC)" and "PIANO HAMMER";
  - `WaveDisplay.h:515` says "HAMMER" and "PIANO HAMMER".
- On a piano hammer, the guitar controls (SLAP, PICK POS, BRIDGE BUZZ, FRET RATTLE) are drawn at full strength, the last
  two in the squeezed font. No effect rule dims them **(inferred)**.

**Fix.**
- Group the menu (Strike / Drive / Bow / Electric / Feedback) and rename "Hammer (classic)" → "Hammer (bright)", display
  only.
- Use one name table.
- Dim guitar-only controls for the hammer, piano, tine and reed exciters through `EffectRules`.

**Evidence.** `keys/keys-grand-osc-scrolled.png` (Hammered Strings).

### 28. Medium: Airwindows is still a shelf inside each family, not a model of the family's effect

**What's wrong.**
- The library lists REVERB and, under an "AIRWINDOWS" rule, SPACES. The same goes for DELAY / ECHOES, WIDENER / HAAS /
  STEREO, COMP / GLUE, and EQ / TONE.
- AIRWINDOWS (ALL) sits alone in the RHYTHM column, in Drive orange.
- Its card has no display, unlike every other card, including the category Airwindows cards.
- The category cards use the plugins' own knob names (BIGNESS, REPLACE) at a raw 50 %.

**Fix.**
- Keep the separate indices, but show each twin as a MODEL choice inside the family's card ("REVERB — Model: Plate /
  Room / Airwindows Galactic").
- Move AIRWINDOWS (ALL) to the bottom of the library as "More Airwindows…".
- Give it the category card's display.

**Evidence.** `init/fx-empty.png`, `init/fx-30.png`, `init/fx-34.png`.

### 29. Medium: the step editors don't share names or rulers

**What's wrong.**
- The arp's step count is STEPS; PROB SEQ's is LENGTH. Both show "12 steps".
- The arp lanes have a STEP 1…16 ruler. PROB SEQ has none. LFO Steps has a 1 / 5 / 9 / 13 ruler.
- The arp's pitch lane is "PITCH st"; PROB SEQ's is "RANGE +st".
- LFO Steps is drawn as pink bipolar bars, the arp lanes as orange unipolar bars, with different hover handling
  **(inferred)**.

**Fix.**
- Call it STEPS everywhere.
- Draw the same numbered ruler on all three.
- Share one lane component between the arp, PROB SEQ and LFO Steps (`SequencerEditors.h`, `LfoDisplay.h`).

**Evidence.** `init/gen-arp-lanes.png`, `init/gen-probseq.png`, `neuro/lfo-steps.png`.

### 30. Medium: the MSEG is a pool card with its own editor, no GRID, and an empty panel

**What's wrong.**
- The MSEG card sits in the LFO row, but after the "+" add card: LFO 1, LFO 2, LFO 3, +, MSEG.
- Its editor is still `MsegEditor` (`SequencerEditors.h:141`), not `LfoDisplay`'s curve editor. It has no "GRID 8"
  button, though every LFO graph has one.
- The MSEG panel holds LOOP and RATE in a card three times their size.

**Fix.**
- Put "+" last.
- Reuse `LfoDisplay`'s Curve editor for the MSEG (one gesture set), which also gives it the display-only GRID (snap of the
  drawing, not a new audio parameter).
- Fill the panel as the LFO panel is filled (SHAPE row → LOOP / ONE-SHOT, RATE, SYNC).

**Evidence.** `neuro/lfo-mseg.png`.

### 31. Medium: the scope floats over PLAY's cards, and its buttons are styled apart

**What's wrong.**
- SCOPE now opens a floating panel that covers PLAY's ENVELOPE and LFO cards (`init/scope-panel.png`).
- It has its own EXPAND and CLOSE header buttons and a "⠿" drag handle used nowhere else.
- The output waveform strip in the header remains, so the output still shows in two places.

**Fix.**
- Dock it by default where it is opened (a "dock" toggle in the same place as the browser's FLOAT / DOCK).
- Use the browser's FLOAT / × pair for consistency.

**Evidence.** `init/scope-panel.png` vs `extras/extra-browser-docked.png`.

### 32. Low-Medium: SIGNAL FLOW contradictions when WEST replaces Filter 2

**What's wrong.**
- The header still says "serial: F1 into F2" when F2 is replaced.
- The replaced block's "WEST" label clips to "…".
- BYPASS's wire lands on WEST (`FilterWidgets.h:459`, `link (layout.bypass, afterFilters)`). That says bypassed
  oscillators go through WEST **(inferred: check against the DSP)**.

**Fix.**
- Header: "serial: F1 into WEST".
- Fit the label, using the block width from finding 1.
- Land the bypass on the stage that really follows the filters for bypassed sources.

**Evidence.** `init/filter-west-replace.png`, `integration-extra/init-filter/flow-crop.png`.

### 33. Low-Medium: the ratio is written two ways

**What's wrong.**
- Knobs read "x1.000" and "x14.00", with a letter x and three decimals (`ParamInfo.h:52`).
- The diagram and card headers read "×1.00", with a multiplication sign and two decimals (`FmOperatorInfo.h:79`).
- Elsewhere "1.5x" (DRIVE) and "x1.0" (vocoder WIDTH) put the x on opposite sides.

**Fix.** One formatter: "×1.00" everywhere a multiplier is shown, symbol first.

**Evidence.** `epiano-fm/fm-1.png`, `init/fx-31.png`.

### 34. Low-Medium: the PITCH & LFO graph's empty-state text crosses the KEY UP marker

"No pitch movement: drag a point up or down" is drawn across the dashed KEY UP line (`epiano-fm/fm-pitch-lfo.png`).
Draw the line under a text backdrop, or put the text above the line.

### 35. Low: card captions use two grammars

**What's wrong.** Two styles sit side by side:

| Style | Examples |
|---|---|
| Lower-case fragments, no full stop | "rows modulate columns", "a string, what excites it and its body", "drag the graph or the knobs; the dot on a segment sets its curve" |
| Full sentences with full stops | "Drag the graph's points: across for time, up or down for level.", "On the Operator Env a modulator's depth is its LEVEL; a cell scales it. Hover a dot to add a route." |

PHYSICAL's right card reads "OSC 1   the controls of its OSC card", which reads as a broken sentence.

**Fix.**
- Header captions: lower-case fragments.
- Hints under a control: sentences.
- PHYSICAL: "OSC 1 · same controls as its OSC card".

**Evidence.** `epiano-fm/fm-1.png`, `init/physical-page.png`, `dx7/06-ENV-LFO.png`.

### 36. Low: on/off switches sit in four places

**What's wrong.**
- **Card header right:** WEST, BODY, VECTOR, SUB + NOISE, FX cards.
- **In the body, labelled:** ARP, PROB SEQ, CLIP.
- **Sub-box header with a state word:** SNAP TO KEY "off", STRUM "spreads chords", SPRAY "off".
- **None:** EVOLVE. And PHYSICAL's BODY summary says "on, Dense" in text, with no switch.

The P7 move (switch into the body) was right for a shared card, but the result is now different from every other card.

**Fix.** Write the rule into `docs/`:
- a card's own module: header switch;
- a tabbed card's engines: switch in the tab label (the tab's dot becomes the switch).

Then apply it to the SEQ engines.

**Evidence.** `neuro/09-ARP-SEQ.png`, `init/filter-west.png`, `init/vector-page.png`.

### 37. Low: three "on" indicators for tabs

**What's wrong.**
- **Ring that fills:** SEQ engines and OSC sub-tabs (○ / ●).
- **Coloured dot that is always lit:** FM operator pills (● 1 … ● 6). An off operator only dims the pill.
- **None:** PLAY's ENVELOPE and FILTER tabs (AMP ENV / FILT ENV, F1 / F2).

**Fix.** The filled-ring dot from `CardTabs` on every tab that names a switchable thing, the FM operator pills included.

**Evidence.** `epiano-fm/fm-1.png`, `neuro/09-ARP-SEQ.png`, `dx7/03-OSC.png`.

### 38. Low: BLEND is a pill, the only continuous value drawn as a button

FX card headers show "BLEND 100%" as a filled pill, and drag-to-set is not visible **(inferred)**. Every other amount in
the synth is a knob or a slider. Use the mini knob size (as RING MOD on FM), or a thin horizontal slider with the value
inside. Evidence: `init/fx-split.png`.

### 39. Low: FX card knobs ignore the family colour

The vocoder and chorus cards have a purple dot, the reverb a blue dot, and every knob is orange. On the synth pages,
knobs take their module's colour (filters purple, LFOs pink, WEST lime). Colour FX knobs by family, as the library and the
card dot already are. Evidence: `init/fx-31.png`, `init/fx-split.png`.

### 40. Low: PLAY and FILTER name the filter type differently

PLAY's TYPE box reads "LP", the FILTER page's picker "Low Pass", for the same choice (`FilterWidgets.h:58` short names).
PLAY has room for "Low Pass" at this width. Use the short name only below a width threshold. Evidence: `dx7/01-MAIN.png`
vs `init/filter-west.png`.

### 41. Low: DX7 names keep ROM truncations, and some are filed oddly

**What's wrong.**
- Title Case kept the DX7's 10-character cuts: "Harpsich 1", "E.Organ 1".
- Each row repeats the bank as a red "ROM1A" tag next to a bank filter chip of the same name.
- "Flute 1" and the brass voices are filed under LEAD.

**Fix.**
- A small expansion table in `src/Dx7Presets.h` (Harpsich → Harpsichord, E.Organ → E.Organ is fine).
- Show the bank tag in text3, not the accent red.
- Add BRASS and WIND to the sound categories, or file them under Keys / Pad by envelope.

**Evidence.** `extras/extra-browser-dx7.png`.

### 42. Low: the PITCH & LFO button is still in the operator pill row

It now has a rule before it (`FmInputPages.h:315`), but it still reads as a seventh operator. It is styled like the pills
and lights orange like a selected operator. Move it to the card's header right edge as an outlined button ("VOICE PITCH &
LFO"), or make it the OP LFO / OP PITCH pool entries (finding 7). Evidence: `epiano-fm/fm-1.png`.

### 43. Low: the WEST "Replace Filter 2" combo uses the squeezed font

The PLACE combo text drops to the condensed font for "Replace Filter 2" (`init/filter-west-replace.png`). Widen the
combo, or show "Replaces F2".

### 44. Low: "S&H", the most basic LFO shape, is now hidden as Legacy

`LfoShapeMenu.h:21-25` files plain S&H as legacy and hides that group unless the patch uses it. A user looking for S&H
finds "Random S&H" under Random, which is fine. But every patch that already uses S&H shows "S&H (classic)", which reads
as deprecated. Name the new one "S&H" and the old one "S&H (stepped)" (display only).

### 45. Low: the matrix's VIA column says "Aux: none" on every row

Eight identical "Aux: none" pills (`dx7/07-MATRIX.png`) are noise next to the new Uni/Bi and curve cells. Show an empty
dashed pill until a VIA is set, as the destination column's "+" does.

---

## Do the seven packages look like one product?

**Mostly yes.** The things that make a product feel whole are shared:
- the type scale and its 9 px floor;
- the card header with a coloured dot and a lower-case caption;
- one knob widget with depth rings;
- the source colours from `ModNames.h`;
- the chip bar;
- the pill-tab style.

A DX7 voice, the vocoder, WEST and the clip editor all use them.

**Where the parallel build shows** (each item is a finding above):
- **Units:** "x1.000" vs "×1.00" (33); raw 0-7 next to % (12); "Off" vs "-∞ dB" (12).
- **Names:**
  - LEVEL vs TRIM (2);
  - Op EG / Operator EG / Operator Env (6);
  - PEAK vs PITCH 1 (13);
  - STEPS vs LENGTH (29);
  - BODY ×4 (26);
  - BOUNCE ×3 (25);
  - HAMMER vs HAMMER (CLASSIC) (27);
  - LP vs Low Pass (40).
- **Direction:** Op Env time knobs reversed (3).
- **Widgets used once:** the BLEND pill (38), the EVOLVE mini-slider (21), the OP pool pills (7), the scope's drag handle
  and EXPAND / CLOSE (31).
- **Placement:** switches in four places (36); tab indicators in three styles (37); source chips in two places (22).
- **Captions:** two grammars (35).

None of these needs new engines. They need one shared rule per item, written down, and a uitest that checks it.

## What already feels native (keep it)

- **DX7 algorithm numbering:** the two pages, "DX7 ALGORITHM n", and "+1" badges on cells that add oscillators.
- **"FB 7 · 50%"** in the matrix, and FB TYPE dimmed without a loop.
- **The Operator Env graph,** draggable with a ruler and playhead, sitting where the synth's envelope graphs sit.
- **CARRIER / MOD › n / OP ENV** under each oscillator name on PLAY, and the OSC tabs' role text.
- **The "Replaced by WEST" overlay** on Filter 2, and WEST's dashed curve in RESPONSE.
- **The SEQ chain readout,** "KEYS › STRUM › PROB SEQ › EUCLID › VOICES", with a sentence on how they combine.
- **SPLIT BANDS** groups with inline crossovers.
- **The duplicate-FX card** that says plainly what it is and offers REMOVE.
- **The browser filing** DX7 voices by sound, with banks as chips.
- **The chaos LFO panel's** A / B axis pills and the "B" sub-chip.
- **The PHYSICAL page's** BODY summary with links to FILTER and ACOUSTIC KEYS. That is the pattern the OP pools should
  copy, but as cards.

## Suggested batches

1. **Bugs and dead controls:** findings 1, 4, 9, 14, 15 (text), 32. Each comes with a uitest.
2. **One operator, one vocabulary:** findings 2, 3, 6, 12, 13, 17, 18, 33.
3. **Op Env in the pools:** findings 7, 8, 16, 19, 20, 42.
4. **Modulatable means modulatable:** finding 5.
5. **Leftover names:** findings 25, 26, 27, 28, 40, 41, 44.
6. **One product:** findings 10, 11, 21, 22, 29, 30, 31, 35, 36, 37, 38, 39, 43, 45, 23, 24.

## Score

**6.5 / 10** for integration.
- **Up from review 6.** That review was not scored, but the DX7 voice was then visibly a separate machine with a broken
  diagram. Now the DX7 voice is mostly a patch that looks like the rest of the synth.
- **Why not higher:**
  - four confirmed bugs that show wrong information or a dead control (1, 4, 9, and 3's reversed knobs);
  - a naming clash on the most-used operator knob (2);
  - silent non-modulatable knobs across most special systems (5).
- **What would take it to 8:** fixing batches 1 to 4.
