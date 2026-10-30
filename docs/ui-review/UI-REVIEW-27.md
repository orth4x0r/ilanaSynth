# UI review 27 (2026-10-30)

main dc442bf (#192 layout pass 2 + FM option A, #193/#194 Modal Piano), rendered with
`ILANA_SNAPSHOT_SCALE=1.75 xvfb-run ilanaSnapshot` on Init, Neuro Wobble, Modal Grand.
Report: https://claude.ai/artifact/U2bfrXMztirN2eFNNyxRku (source `/mnt/project-files/ilanasynth/ui-review-27/ui-review-27.html`).

Scores: workflow 9.3 Vital / 9.1 Serum 2 (26: 9.2 / 9.0); polish 9.4 / 9.2 (26: 9.3 / 9.1, 8.8 on 3 oscs).
Review 26 items all fixed except PATCH canvas space (half) and VECTOR empty corners.

Open (pins in the report). Bugs, no design call:
- R27-5/6 Modal Piano: EXCITER row clips the 12th (selected) icon; HAMMER ring ~4 px under the icons.
- R27-1 STRINGS TUNING heading 10 px (6 design px) under COUNT label; R27-2 BODY KEY TRACK label ends at the card border.
- R27-3 SUB off: wave picture stays lit (208 vs rings 164); off cards dim to 164 but OSC 2 off to 100.
- R27-4 LFO card "Drives nothing" drawn as a red pill on every unused LFO.
- R27-10 MATRIX STEREO chip lit on rows that are not filter cutoff; R27-15 PATCH NOT IN USE boxes still draw cables.
- R27-12/13 FM "+1 OSC" over algorithm glyphs; ROUTES "1 · each one automatable".
Layout, needs ilana's yes: R27-7/8 Physical MORE panel holds DAMPER only, MIX lacks FILTER pills / OUT meter;
R27-9 LFO pop-up RETRIG/KEY sit over SMOOTH/START like labels; R27-16 VECTOR empty corner boxes; R27-17 OTT empty graph space.
Left to the Impeccable 35 batch: R27-11 FM diagram space, R27-14 PATCH canvas space.
