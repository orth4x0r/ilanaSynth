# Diversity pass 3: Bass, Pad, Keys

Oscillator, modulation and FX re-voicing of 77 presets (24 Bass, 37 Pad, 16 Keys) in `src/PresetVoicing.h`, with level trims refitted in `src/PresetTrims.h`. Earlier passes mostly moved filters; this pass changes wavetables, warp, FM, engines, LFO/envelope movement, velocity response, release/tail length and FX chains, and gives each preset four macros named for its sound. Airwindows filters were kept where a filter fits.

Measured with `tools/preset_diversity.py` (CLAP + critic features). Crowds are presets within 1.2 x the references' close-pair distance of a seed; in brackets the total number of presets in crowds. The remaining crowds are mostly seeded by presets this pass must not touch (Rip Bass, FM E-Piano, Neon Bass, Wurli Drive and the piano/EP family).

| Category | Spread before | Spread after | Crowds before | Crowds after |
|---|---:|---:|---|---|
| Bass | 0.581 | 0.665 | 20,11,3,5,3 (42) | 15,3,7,9 (34) |
| Pad | 0.504 | 0.556 | 15,20,3,4,3,3 (48) | 15,11,3,8,4 (41) |
| Keys | 0.546 | 0.613 | 30,5,6 (41) | 20,4,5,6 (35) |

Level rule: every changed preset is within +-1 dB rms of its row in `tests/fingerprints-linux.csv` and its peak is no more than 1 dB above (`tools/check_level_rule.py`). Fingerprints and trims were fitted over full-library runs. Several presets were reverted to their previous voicing because they could not meet the rule or a test: Neuro Formant, Curve Motion Pad, Reed Keys, PD Growl Bass, West Folded Bass, Tabled Cello (its test needs the Cello Section patch table).

Format: preset: oscillator/filter changes; FX chain; macros.

## Bass

- Neuro Wobble: Neuro, Growl, FM; fx Vowel/Drive/OTT; macros TONE/TALK/TEAR/SUB
- Reese Ripper: Reese, Reese; fx Comb/Drive/Dimension/Comp; macros GRIND/SPLIT/COMB/AIR
- Tape Reese: Soft_Saw, Warm_Sub; fx Airwindows/Chorus/Delay/EQ; macros WARMTH/HISS/WOW/ECHO
- FM Growl: Sine, Sine, FM; fx Amp/Crush/Comp; macros INDEX/SNARL/BITE/SUB
- Rust Bass: Grind; fx Crush/Airwindows/Tilt; macros RUST/DECAY/SCRAPE/DARK
- Clocked Bass: Pulse_Sweep; fx Gate/Delay/Comp; macros CHOP/SWING/PULSE/ECHO
- Crushed Trap: 808; fx Crush/Tilt/Limiter; macros KICK/CRUSH/TAIL/BITE
- LFO 3 Wobble: Wobble, Warm_Sub; fx Drive/Flanger/Limiter; macros WOBBLE/FRAME/JET/SQUASH
- Octave Hyper Bass: Harmonic_Stack, Organ, filter SEM; fx Tilt/Widener/Limiter; macros CLOSE/OCTAVES/DETUNE/FOCUS
- Rubber Mono: Skew_Triangle, Rounded_PWM, warp Asym; fx Airwindows/Comp; macros RUBBER/PWM/OPEN/SQUASH
- Per-Voice Wobble: Bitwise; fx Airwindows/Drive/OTT; macros RATE/DEPTH/FRAME/FLUTTER
- Shift Bass: Sine_To_Saw, Odd_Tilt; fx FreqShift/Chorus/EQ; macros SHIFT/FRAME/WARM/CHORUS
- Ladder Sub: Warm_Sub, Deep_Pulse; fx EQ/Airwindows; macros OPEN/SQUEEZE/PUNCH/GRIT
- MS-20 Growl: Square_Sync, PWM; fx Drive/Reverb; macros SCREAM/SYNC/WOBBLE/GATE
- Pluck Sub Bass: engine 1; fx EQ/Comp/Delay; macros PLUCK/DECAY/SUB/ECHO
- Deep House Bass: Deep_Pulse, Jazz_Organ; fx Dimension/Tilt/Comp; macros OPEN/ENV/PULSE/AIR
- Distorted 808: 808; fx Drive/Crush/Comp/Limiter; macros DIST/TONE/LENGTH/PUNCH
- Dark Ladder Pluck Bass: Ladder_Saw, Ladder_Square; fx EQ/Delay/Reverb; macros OPEN/RESO/DECAY/ECHO
- Stick-Slip Growl: engine 1; fx Drive/Airwindows; macros PRESSURE/SPEED/BUZZ/GRIT
- Morph Filter Bass: Acid_Ramp; fx Phaser/Drive; macros FILTER/SWEEP/PEAK/PHASE
- Comb Bass: engine 1, Comb_Sweep; fx Drive/EQ/Comp; macros COMB/RING/NOISE/SWEEP
- Steiner Growl: Yeah, Nasal, warp Mirror; fx Drive/Vowel/Crush/Reverb; macros CUTOFF/INPUT/RESO/VOWEL
- Warp Wobble: Fold_Sweep, warp Mirror; fx Airwindows/OTT/Limiter; macros FILTER/DEPTH/WARP/DIRT
- Hyper Reese: Reese, Wobble; fx OTT/Airwindows/Widener; macros CUTOFF/DETUNE/DRIVE/WIDTH

## Pad

- Metal Pad: engine 2, FM_Metal; fx RingMod/Tilt; macros RING/CLANG/STRIKE/BLOOM
- Virus Pad: HardSync, warp Off; fx Phaser/Airwindows/Chorus/Widener; macros SYNC/SWEEP/DRIVE/PHASE
- Tape Choir: Pad_Choir, Choir, filter AW_Holt; fx Airwindows/Delay/EQ/Reverb; macros AGE/CHOIR/ECHO/FLUTTER
- Morph Pad: Wave_Scan, Shimmer_Stack, filter AW_Holt; fx Flanger/Airwindows/Tilt/Reverb/Widener; macros SCAN/OPEN/JET/GALAXY
- Granular Pad: engine 3; fx Smear/Reverb/Widener; macros DUST/GRAIN/CLOUD/SHIMMER
- Comb Choir: Vowels_Female; fx Chorus; macros RESONANCE/BREATH/VOWEL/CHORUS
- Per-Voice Drift Pad: Soft_Saw, Soft_Saw; fx Chorus/Airwindows/Reverb/Widener; macros CUTOFF/DRIFT/WANDER/ENSEMBLE
- Routed Duo Pad: Warm_Pad, Air_Pad, warp Off; fx Chorus/Drive; macros LOW BODY/HIGH AIR/BALANCE/SPLIT
- Tape Strings: Bowed, Bowed, filter Low_Pass; fx Airwindows/Widener/Reverb; macros BOW/WOBBLE/TONE/WOW
- Six-Voice Drift: recipe oscillators; fx Delay/Reverb; macros DRIFT/CUTOFF/UPPER/RING
- Pendulum Swing Pad: Flute, Bell; fx Tremolo; macros SWING/DEPTH/BELL/TREMOLO
- Twin Peak Pad: Saw_Octaves, Square_Sweep; fx Airwindows/Reverb; macros PEAKS/CUTOFF/RESO/ROOM
- Evolve Macro Pad: Partial_Cloud, Glass_Harmonics; fx Widener/Reverb; macros CUTOFF/BANDS/AIR/WIDTH
- Vector Corner Morph: Vowels_Male, Glass_Harmonics; fx Octaver; macros X/Y/TONE/OCTAVE
- Vector Path Drift: Air_Pad, Choir; fx Airwindows/Delay/Reverb/Widener; macros X/Y/TONE/ECHO
- Freeze Pad: Shimmer_Stack, Glass_Harmonics; fx Freeze/Airwindows/Reverb/Widener; macros THAW/FROST/GLOW/AIR
- Shimmer Pad: Partial_Cloud, Sine_To_Saw; fx Octaver/Delay/Reverb/Widener; macros SHIMMER/PARTIALS/GLOW/ECHO
- LFO 4 Sweep: Saw_To_Square, Saw_To_Square; fx Phaser/Reverb; macros SWEEP/DEPTH/PHASE/SPRING
- Warm Analog Pad: RC_Saw, RC_Saw; fx Chorus/Airwindows; macros OPEN/DRIFT/SWELL/TAPE
- Vowel Pad: Diphthong, Vowels_Female; fx Chorus/Airwindows/Reverb; macros VOWEL/SPEED/FORMANT/GALAXY
- Morphing Strings: Bowed, Reed; fx Chorus/Reverb/Utility/Widener; macros MORPH/CUTOFF/SWELL/HALL
- Dark Matter: Rossler, Henon; fx Airwindows/Reverb; macros CUTOFF/CHAOS/UNREST/VOID
- Shimmer Glass Pad: Bell, Glass_Harmonics; fx Flanger/Airwindows/Reverb; macros FRAME/OVERTONE/GLINT/CATHEDRAL
- Octave Stack Pad: Full_Organ; fx Tremolo/Airwindows; macros OCTAVES/TONE/DETUNE/SWIRL
- Pulse Width Pad: Rounded_PWM, Rounded_PWM, warp PWM, osc2 warp PWM; fx Chorus/Widener; macros CUTOFF/PWM/SPEED/CHORUS
- Feedback Choir: Throat, FM; fx Chorus/Reverb; macros VOWEL/FEEDBACK/FM/SPACE
- Harmonic Cut Pad: Harmonic_Stack, Odd_To_Even; fx Chorus; macros DRIFT/OCTAVE/TONE/CHORUS
- String Ensemble Wide: Soft_Saw, RC_Saw, filter AW_Pear; fx Airwindows/Reverb/Widener; macros SWELL/OCTAVE/DETUNE/HALL
- Lorenz Wash: Lorenz, Air_Pad; fx Airwindows/Reverb; macros CHAOS/DEPTH/TONE/SPACE
- Chord Pad: CZ_Reso; fx Airwindows/Chorus/Widener; macros BRIGHT/RESO/DRIVE/TUBE
- MSEG Sweep: Wave_Scan; fx Delay/Reverb; macros BRIGHT/SCAN/DRIVE/ECHO
- Vocal Pad: recipe oscillators; fx Dimension/Reverb/Widener; macros FORMANT/START/RESO/HALO
- Wide Chord Pad: Shimmer_Stack; fx Airwindows/Dimension/Reverb/Widener; macros TONE/CHORD/RESO/SPACE
- Grain Choir: engine 3; fx Airwindows/Reverb/Widener; macros SCRUB/SPRAY/SHIMMER/WRECK
- EQ Sculpted Pad: Harmonic_Bands, Harmonic_Bands; fx EQ/Widener; macros SCULPT/PRESENCE/AIR/WIDTH
- Swing Gate Pad: Saw_Octaves; fx Gate/Chorus; macros SWING/GATE/TONE/CHORUS
- Layered Sync Choir: Choir, Pad_Choir, osc2 warp Sync; fx Chorus/Delay/Utility/Reverb; macros VOWEL/SYNC/MOVE/ECHO

## Keys

- Ladder Organ: Jazz_Organ, Triangle; fx Amp/Tremolo/Utility/Reverb/Widener; macros DRAWBARS/TONE/GRIT/LESLIE
- Warm Clav: Clav, filter Band_Pass; fx Phaser/Amp; macros PICKUP/FUNK/WAH/PHASE
- Mellow Rhodes: Sine, Triangle, warp Off; fx Tremolo/Dimension/Reverb/Widener; macros BARK/TONE/TREMOLO/ROOM
- Digital Bell Keys: Bell, FM_1:2; fx Chorus/Delay/Reverb/Widener; macros FRAME/BELL/LENGTH/ECHO
- Vibraphone: Marimba, Sine, filter AW_Z_LP; fx Tremolo/Reverb/Widener; macros MOTOR/OVERTONE/LENGTH/ROOM
- Celeste: Bell, Sine, filter AW_Z_LP; fx Reverb; macros SPARKLE/STRIKE/LENGTH/SPACE
- Soft Synth Piano: Analog, Triangle, filter AW_Z_LP; fx Chorus/Reverb/Widener; macros BRIGHT/HAMMER/LENGTH/SPACE
- Stretched Glass Keys: recipe oscillators; fx Chorus/Airwindows/Reverb; macros MORPH/TONE/DECAY/CATHEDRAL
- Vinyl Dust Keys: Soft_Saw; fx Chorus/Airwindows/Crush; macros DUST/TONE/WOBBLE/TAIL
- Drunk Tape Keys: recipe oscillators; fx Airwindows/Chorus/Widener; macros WARBLE/TONE/DUST/TAIL
- Vintage Tine: recipe oscillators; fx Tremolo/Haas/Reverb/Widener; macros BARK/TREMOLO/TONE/WIDE
- Chorused Tine: recipe oscillators; fx Chorus/Dimension/Reverb/Widener; macros CHORUS/BARK/TONE/ROOM
- Growl Reed: Reed_Piano, filter AW_Angle; fx Airwindows/Amp; macros GROWL/REED/BREATH/MELLOW
- Distorted Tine Rock: recipe oscillators; fx Airwindows/Amp/Phaser/Reverb/Widener; macros DRIVE/BARK/TONE/PHASE
- Two Pairs Keys: recipe oscillators; fx Delay; macros TINE/BODY/DECAY/ECHO
- DX Keys Classic: recipe oscillators; fx Chorus/Reverb/Widener; macros TINE/BODY/DECAY/CHORUS
