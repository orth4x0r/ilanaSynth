# UI review 19 (2026-10-12), main 078bf44 (through #86)

Scores: 9.4 Vital / 9.1 Serum 2 / 9.4 integration (review 18: 9.3 / 9.0 / 9.4). Goal 9.5.
Shots: /mnt/project-files/ilanasynth/ui-review/review19 (Neuro Wobble, `ilanaSnapshot`, 1.5x). Measure: review16/m.py.
Bare % (Neuro Wobble): PLAY 8, VECTOR 30, OSC 3, FILTER 21, MOD 21, MATRIX 6, FM 29, SEQ 38, FX 8, PATCH 6; mean 17.0 (review 18: 14.7; Vital 17.3). SEQ rose 15 -> 38 because of the chosen off-state (thin ticks, 0.02 alpha cells); the lanes now read calm, not a grey wall.

## Better since review 18
- PLAY fit (#81): OSC 1, OSC 2, folded OSC 3, add row and SUB + NOISE all whole, no scroll. Three osc cards per screen unpinned. (R18-2 closed unpinned.)
- PATCH (#83, #84): no mode switches, pan/zoom/minimap, drop-on-cable, FX LFO boxes. FIT now fills the canvas width. (R18-1 mostly closed.)
- SEQ off state (#86): thin ticks. (R18-3 closed.)
- Clip roll paint (#85), phys_bowed glide (#82), DAMPER (#80): behaviour, no layout change.

## Dropped by decision, not scored
FILTER response graph / SIGNAL FLOW (R18-4) and MOD graph sizes (R18-6). Dead space on VECTOR, FM: accepted. Effect windows stay floating.
In progress elsewhere, not scored: step 19 weirder wavetables, step 20 wavetable icon, step 48 settings table, envelope dot fix.

## Remaining gaps to 9.5 (ranked)
- R19-1 Medium: PATCH fit is width-limited. The graph is 1540x410 px in a 1560x630 px canvas, so about 110 px empty above and below, node labels about 8 px at 85 %. The summary strip (about 100 px) is one breadcrumb line, a hint sentence and three stats with an empty middle. Fix: lay the fitted graph out taller (stack sources and stages vertically, bigger boxes), one-line strip with the hint as a tooltip. Shots gap1-patch, patch.
- R19-2 Medium (integration): the FX tab still has CHAIN 1/2, SERIES / PARALLEL / PATCHER, while PATCH has no modes and shows the same routing as cables. Two routing UIs say different things. Fix: FX tab = the rack only (card list), routing lives on PATCH; or one line that says "routing: see PATCH". Shot gap2-fx-modes.
- R19-3 Low: SEQ right column: four knobs on about 100 px vertical gaps next to a 500 px lane block; lanes fine. Fix: two rows of two at the lane block's height, or tighten. Shot gap3-seq.
- R19-4 Low: VECTOR corner says "OSC 4: none" in the menu and again inside the empty picture (open since review 15). Shot gap4-vector.
- R19-5 Low (by choice): pinned SUB + NOISE leaves 2.3 osc cards (play-3osc-pinned, play-6osc-pinned). Unpinned is the default and fits 3.

Fix list: R19-1 and R19-2 move the score; with them 9.5 against Vital. R19-3 to R19-5 are polish. Nothing started.
