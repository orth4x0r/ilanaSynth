# UI review 28 (2026-11-04)

main 450a569 (#221 EDIT BODY removed, #222 Mac port / JUCE 8.0.15, #223 FM operator card), rendered with
`ILANA_SNAPSHOT_SCALE=1.75 xvfb-run ilanaSnapshot` on Init, Neuro Wobble, Modal Grand, FM E-Piano, Operator Bell.
Report: https://claude.ai/artifact/6mEXm86o1Xt6mzrMrdQP1b (source `/mnt/project-files/ilanasynth/ui-review-28/ui-review-28.html`).

Scores: workflow 9.4 Vital / 9.2 Serum 2 (27: 9.3 / 9.1); polish 9.4 / 9.2 (unchanged).
Review 27: 12 of 17 pins fixed. Still open: KEY TRACK 3 px from BODY border, two off dim levels (rings 163 vs 92),
Physical MIX lacks OUT meter, FM canvas empty (127 of 476 px used), PATCH canvas empty (~1085 x 243 px).

New, bugs (no design call):
- R28-6 FM: with an operator on OP ENV (#223) the diagram canvas drops 480 -> 355 px; on DX7 algorithms 1/2
  names clip to "OSC" / "MO" and amount knobs sit on the boxes. Top bug.
- R28-1 Modal Piano STRING picture: hammer head drawn above the picture frame, half clipped.
- R28-11 VECTOR: PATH label 9 px tall and 3 px higher than its row (11 px).
Layout, needs ilana's yes: FM canvas fill + OUTPUT quiet state, PATCH canvas, browser detail pane (345 px empty),
scroll stop between rows on PLAY's oscillator area.
Harness note: the orange edited dots on tabs in ILANA_SNAPSHOT_PRESET pictures come from loading the preset behind
the editor (rememberLoadedFingerprint not called), not from the plugin.
Build note: a fresh container needs `apt-get install libxrandr-dev libxinerama-dev libxcursor-dev ...` (ci.yml list) before cmake.
