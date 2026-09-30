# Diversity pass 2: filter characters (2026-09-30)

62 presets re-voiced in `src/PresetVoicing.h` with the Airwindows filters and the Disperser (filter types 29-38). Levels re-fitted per preset with `tools/match_levels.py` (level rule: 0 of 62 break it). Measured with the step-3 tools on fresh renders of the Linux build.

| Category | Spread before | after | Crowds before (sizes) | after |
|---|---:|---:|---|---|
| Bass | 0.576 (78 %) | 0.599 (81 %) | [12, 11, 10, 5] | [11, 7, 7, 6, 5] |
| Pad | 0.477 (81 %) | 0.503 (85 %) | [21, 19, 4, 3] | [18, 13, 4, 3, 3, 3, 3] |
| Lead | 0.544 (76 %) | 0.547 (76 %) | [21, 9, 8, 6, 6] | [20, 7, 6, 5, 5, 3, 3] |
| Keys | 0.545 (88 %) | 0.545 (88 %) | [22, 9, 5, 4, 3] | [25, 5, 5, 4, 3] |
| Pluck | 0.667 (82 %) | 0.690 (85 %) | [17, 13, 4, 4, 3] | [17, 10, 4, 4, 4, 3] |

## Presets changed

### Bass
- Rip Bass: filter Low Pass (400 Hz) -> AW Acid (400 Hz, reso 0.8, morph 0.5, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- Neuro Wobble: filter Low Pass (1200 Hz) -> AW Z LP (300 Hz, reso 0.8, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- Reese Ripper: filter Low Pass (900 Hz) -> AW Angle (300 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Resonator Bass: filter Low Pass (500 Hz) -> Disperser (261.63 Hz, reso 1, morph 1, key track 1, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- Neon Bass: filter Low Pass (800 Hz) -> AW Holt (1100 Hz, reso 0.85, morph 0.2); macro 1 (TONE) now sweeps cutoff and morph
- Clocked Bass: filter Low Pass (1200 Hz) -> AW YNot LP (300 Hz, reso 0.9, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- Deep House Bass: filter Ladder LP (450 Hz) -> AW Holt (300 Hz, reso 0.9, morph 1); macro 1 (CUTOFF) now sweeps cutoff and morph
- Morph Filter Bass: filter Morph (700 Hz) -> AW Pear (300 Hz, reso 1, morph 0.9); macro 2 (CUTOFF) now sweeps cutoff and morph
- Per-Voice Wobble: filter Ladder LP (600 Hz) -> AW Pear (500 Hz, reso 0.9, morph 0.7)
- Spring Bounce Bass: filter Ladder LP (600 Hz) -> AW Z LP (700 Hz, reso 0.7, morph 0.3); macro 2 (CUTOFF) now sweeps cutoff and morph
- Warp Stack Bass: filter Low Pass (20000 Hz) -> AW Z LP (750 Hz, reso 1, morph 0.9, key track 0.3)
- Ring Bass: filter Low Pass (1600 Hz) -> AW Holt (1600 Hz, reso 0.9, morph 0.9, key track 0.3, filter env 1.5 oct)
- Distorted 808: filter Low Pass (3000 Hz) -> AW X LP (900 Hz, reso 0.6, morph 0.9); macro 2 (TONE) now sweeps cutoff and morph
- Sub 808 Arp: filter Low Pass (1200 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- Diode Acid: filter Diode LP (420 Hz) -> AW Acid (300 Hz, reso 0.95, morph 0.9); macro 1 (CUTOFF) now sweeps cutoff and morph
- Dark Ladder Pluck Bass: filter Ladder LP (250 Hz) -> AW Holt (300 Hz, reso 0.9, morph 0.9); macro 1 (CUTOFF) now sweeps cutoff and morph
- Through-Zero Growl: filter Ladder LP (1600 Hz) -> AW YNot LP (480 Hz, reso 1, morph 0.9); macro 4 (CUTOFF) now sweeps cutoff and morph
- OTA Squelch: filter OTA LP (450 Hz) -> AW Angle (450 Hz, reso 0.7, morph 0.6, filter env 3 oct); macro 1 (CUTOFF) now sweeps cutoff and morph

### Pad
- Metal Pad: filter Low Pass (3000 Hz) -> Disperser (261.63 Hz, reso 1, morph 1, key track 1); macro 1 (TONE) now sweeps cutoff and morph
- Freeze Pad: filter Low Pass (3500 Hz) -> AW Holt (600 Hz, reso 0.9, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- Shimmer Pad: filter Low Pass (3800 Hz) -> AW Pear (1140 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Vocal Pad: filter Low Pass (5200 Hz) -> AW Angle (750 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Granular Pad: filter High Pass (400 Hz) -> AW Z HP (600 Hz, reso 0.9, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- MPE Glass: filter Low Pass (6500 Hz) -> AW YNot LP (1500 Hz, reso 0.9, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- LFO 4 Sweep: filter Low Pass (900 Hz) -> AW Acid (1500 Hz, reso 0.6, morph 0.4); macro 1 (TONE) now sweeps cutoff and morph
- Ladder Swell Pad: filter Ladder LP (300 Hz) -> AW Z LP (600 Hz, reso 0.9, morph 1); macro 1 (CUTOFF) now sweeps cutoff and morph
- EQ Sculpted Pad: filter Low Pass (6000 Hz) -> AW Z BP (600 Hz, reso 0.85, morph 1)
- Feedback Choir: filter Low Pass (3200 Hz) -> AW X LP (960 Hz, reso 0.25, morph 0.9)
- Chaos Filter Pad: filter Low Pass (1800 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1); macro 2 (CUTOFF) now sweeps cutoff and morph
- String Ensemble Wide: filter Low Pass (6000 Hz) -> AW Pear (1500 Hz, reso 0.9, morph 0.9)
- Warp Envelope Pad: filter Low Pass (20000 Hz) -> AW Holt (1500 Hz, reso 0.7, morph 0.8)
- Vector Corner Morph: filter Low Pass (20000 Hz) -> AW Angle (1200 Hz, reso 0.9, morph 0.9); macro 3 (TONE) now sweeps cutoff and morph
- Wide Chord Pad: filter Low Pass (4200 Hz) -> AW YNot LP (1260 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph

### Lead
- Scream Lead: filter Band Pass (2500 Hz) -> AW Z BP (1500 Hz, reso 0.8, morph 0.8); macro 1 (TONE) now sweeps cutoff and morph
- FM Monster: filter Low Pass (6000 Hz) -> AW Z LP (1600 Hz, reso 0.9, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- Resonant Scream: filter Band Pass (1800 Hz) -> AW Acid (800 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Tape Lead: filter Low Pass (6000 Hz) -> AW Holt (1600 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Glitch Lead: filter Low Pass (5000 Hz) -> Disperser (261.63 Hz, reso 1, morph 1, key track 1, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- MPE Lead: filter Low Pass (7000 Hz) -> AW Pear (1600 Hz, reso 1, morph 1); macro 1 (TONE) now sweeps cutoff and morph
- Rig Lead: filter Low Pass (6000 Hz) -> AW Holt (1600 Hz, reso 0.8, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Supersaw Anthem: filter Low Pass (7000 Hz) -> AW Angle (1600 Hz, reso 0.9, morph 0.9); macro 1 (CUTOFF) now sweeps cutoff and morph
- Soft Saw Lead: filter Low Pass (650 Hz) -> AW YNot LP (800 Hz, reso 0.9, morph 1); macro 1 (CUTOFF) now sweeps cutoff and morph
- Sync Scream: filter Low Pass (6000 Hz) -> AW YNot LP (1600 Hz, reso 0.9, morph 0.9); macro 2 (TONE) now sweeps cutoff and morph
- Octave Screamer: filter Low Pass (5000 Hz) -> AW Z BP (900 Hz, reso 0.85, morph 0.4)

### Keys
- FM E-Piano: filter Low Pass (7000 Hz) -> AW Angle (2100 Hz, reso 0.9, morph 0.9)
- Mellow Rhodes: filter Low Pass (3000 Hz) -> AW Holt (900 Hz, reso 0.9, morph 0.9); macro 2 (TONE) now sweeps cutoff and morph
- Wurli Drive: filter Low Pass (3500 Hz) -> AW Z BP (1800 Hz, reso 0.7, morph 0.8)
- Celeste: filter Low Pass (9000 Hz) -> AW Pear (2200 Hz, reso 0.9, morph 0.9)
- Vinyl Dust Keys: filter Low Pass (2800 Hz) -> AW YNot LP (900 Hz, reso 0.9, morph 0.9); macro 2 (TONE) now sweeps cutoff and morph
- Drunk Tape Keys: filter Low Pass (2600 Hz) -> AW X LP (900 Hz, reso 0.5, morph 0.9); macro 2 (TONE) now sweeps cutoff and morph
- Six-Op Organ: filter Low Pass (20000 Hz) -> AW Holt (2500 Hz, reso 0.5, morph 0.7)
- Vibraphone: filter Low Pass (6000 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1)

### Pluck
- Tap Pluck: filter Low Pass (6000 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1, filter env 3.5 oct); macro 1 (TONE) now sweeps cutoff and morph
- Virus Pluck: filter Low Pass (6000 Hz) -> Disperser (261.63 Hz, reso 1, morph 1, key track 1, filter env 4 oct); macro 1 (TONE) now sweeps cutoff and morph
- Smear Pluck: filter Low Pass (5000 Hz) -> AW Z BP (1500 Hz, reso 0.7, morph 0.8); macro 1 (TONE) now sweeps cutoff and morph
- Chord Stab: filter Low Pass (5000 Hz) -> AW Holt (1500 Hz, reso 0.9, morph 0.9); macro 1 (TONE) now sweeps cutoff and morph
- Hyper Pluck: filter Low Pass (8000 Hz) -> AW Acid (800 Hz, reso 0.9, morph 0.9, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- Metal Keys: filter Low Pass (7000 Hz) -> Disperser (261.63 Hz, reso 0.7, morph 0.6, key track 1); macro 1 (TONE) now sweeps cutoff and morph
- Quad Mod Pluck: filter Low Pass (4500 Hz) -> AW YNot LP (1800 Hz, reso 0.6, morph 0.8); macro 1 (TONE) now sweeps cutoff and morph
- Resonator Pluck: filter Low Pass (6000 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
- Warp Pluck: filter Low Pass (3500 Hz) -> AW Z LP (1050 Hz, reso 0.9, morph 1)
- Arp Glass: filter Low Pass (8000 Hz) -> Disperser (261.63 Hz, reso 0.95, morph 1, key track 1, filter env 3 oct); macro 1 (TONE) now sweeps cutoff and morph
