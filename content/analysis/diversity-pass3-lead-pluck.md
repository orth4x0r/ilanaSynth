# Diversity pass 3: Lead and Pluck

Bold re-voicing of 74 presets (37 Lead, 37 Pluck) in src/PresetVoicing.h and src/PresetTrims.h. Each changed preset got a new sound source (wavetable, physical Reed/Bow/Tine/Hammer, sample, granular, vocoder, arpeggiator, FM or sync), its own movement, its own FX chain and four specific macros. Airwindows filters are kept wherever a filter is used; AW Acid is not used. Presets on the forbidden list were not touched. Body-engine plucks keep their engine and macros and only got FX changes.

## Metrics (tools/preset_diversity.py, CLAP plus critic features)

| Category | Spread before | Spread after | Share of reference | Crowd members before | Crowd members after |
|---|---:|---:|---:|---|---|
| Lead | 0.537 | 0.608 | 75% to 86% | 47 in 5 crowds (20, 9, 5, 8, 5) | 38 in 4 crowds (18, 12, 3, 5) |
| Pluck | 0.654 | 0.708 | 82% to 89% | 43 in 5 crowds (17, 12, 7, 4, 3) | 42 in 8 crowds (10, 9, 3, 5, 3, 3, 6, 3) |

Pluck crowds are now smaller and more numerous: the largest went from 17 to 10. The 'before' numbers are from the baseline measured before this pass.

## Level rule

All 74 presets are within +-1 dB rms of their row in tests/fingerprints-linux.csv with peak at most +1 dB, on a full ilanaFingerprint run (tools/check_level_rule.py). Other presets outside this pass (for example Hyper Pluck, Soft Saw Lead) already differ from that csv independently of this work. The csv was not modified. Several crest-limited presets use a Limiter before a Utility gain to meet the rule.

## Changes per preset

### Lead

- Virus Hypersaw: 7-voice Hypersaw stack plus a 3-voice saw an octave down, AW YNot LP, 1/16 Gallop trance gate, chorus, dotted ping-pong delay; short decaying body. Macros CUTOFF, CHOP, SPREAD, ECHO.
- Resonant Scream: Mono Acid Ramp saw an octave down with noise, MS-20 LP at high resonance with a fast filter envelope, drive; dry, no tails. Macros SCREAM, SWEEP, GRIT, FUZZ.
- Tape Lead: RC Saw pair with hiss and drift, AW Holt, AW ToTape6, tape delay with wow, spring reverb. Macros TONE, WOW, ECHO, SPRING.
- MPE Lead: Physical string excited by a Reed model (sax-like), aftertouch opens the filter, vibrato LFO, EQ presence, dotted delay, plate. Macros BREATH, REED, DRIFT, ROOM.
- S&H Techno: PWM and saw pair; clocked sample-and-hold steps the cutoff and the comb frequency; comb FX, drive, gated reverb. Macros CUTOFF, COMB, GRIT, GATED.
- Warp Stab: Hard-sync table with a mod-envelope sweep on the sync warp, AW Z LP, phaser, spring reverb; short stab. Macros SYNC, DECAY, PHASER, SPRING.
- Frequency Shift: Through-zero FM between two sines (envelope-driven depth), octave up, bell-like decay, frequency shifter and shimmer reverb. Macros FM, SHIFT, RATIO, SHIMMER.
- Vowel Lead: Looped sampled "Ah" voice plus a Diphthong table whose frame is moved by an LFO, chorus, large hall; slow swell. Macros VOWEL, BREATH, CHORUS, HALL.
- Fifth Stack Lead: Brass table in Fifths-mode unison, an octave down, AW YNot LP, tube amp, flanger. Macros STACK, BRASS, FLANGE, DRIVE.
- Legato Glide Lead: Soft square with a sine an octave up, long glide and 12-semitone bend, Haas widening, ping-pong delay, plate. Macros GLIDE, BRIGHT, WIDTH, VIBRATO.
- Key FM Scream: Exponential FM (sine pair, ratio a fifth up) driven by velocity and the mod envelope, AW Angle, drive, flanger, spring. Macros FM, RATIO, FLANGE, DRIVE.
- Frozen Grain Lead: Granular engine on the Vocal Ah sample, smear FX and shimmer reverb, slow swell. Macros GRAIN, SPRAY, SMEAR, SHIMMER.
- Odd Octaver: Octave Pulse table, Octaver FX into a fuzz amp, Diode LP, spring; an octave down. Macros OCTAVE, FUZZ, TONE, SPACE.
- Formant Scream II: Throat table in 3-voice unison, Formant filter whose morph an LFO sweeps, OTT compression, flanger. Macros VOWEL, SCREAM, OTT, FLANGE.
- Hollow Pulse Lead: 3-voice CZ Square an octave up, PWM-style warp LFO, AW ChorusEnsemble, ping-pong delay. Macros HOLLOW, ENSEMBLE, TONE, ECHO.
- Talkbox Lead: Saw unison an octave down through the Vocoder FX with its internal talking modulator; dry. Macros CUTOFF, DETUNE, SPREAD, LEVEL.
- Diode Screamer: Wavefold table with an LFO on its frame, Diode LP, drive plus wavefolder FX, ping-pong delay. Macros FOLD, SCREAM, TONE, ECHO.
- Ring Lead: Ring Sine table in 2-voice unison, ring-mod FX, Dimension, hall; short decaying lead. Macros RING, TONE, DIM, HALL.
- Stereo Bend Lead: Two Digital-table oscillators panned apart with opposite bend warps, AW Pear, Dimension, Widener, hall, limiter. Macros BEND, DIM, WIDTH, DRIVE.
- Vintage Mono Lead: Analog saw plus Analog Pulse two octaves down, OTA LP, drift, glide, phaser, compressor. Macros CUTOFF, GLIDE, PHASER, DRIFT.
- Pan Flute: Flute table and band-limited noise through a key-tracked band-pass, breathy, small room and widener. Macros BREATH, TONE, WIDTH, HALL.
- Slide Fiddle: Physical string excited by a Bow model, long glide and vibrato, mid-boost EQ and room. Macros BOW, SLIDE, DAMP, ROOM.
- PD Casio Lead: PD Res I warp on a sine with a mod-envelope resonance sweep, chorus, gated reverb, AW Z LP. Macros RESO, CHORUS, TONE, GATED.
- Dual Stage Warp Lead: PD Res III warp with a mod-envelope sweep, AW Pear, OTT. Macros WARP, SWEEP, TONE, OTT.
- SEM Classic Lead: SEM filter with an LFO on its mode morph, Analog and Analog Pulse pair, phaser, spring; short pluck-lead envelope. Macros CUTOFF, MODE, PHASER, SPRING.
- Scream Lead: Mono formant saw with a wavefold oscillator an octave up, pitch scoop from the mod envelope, AW Angle, heavy drive with wavefolder, bit-crush, noise. Macros SCREAM, SCOOP, FOLD, DIRT.
- Scream Wobble: Growl and Reese tables, AW YNot LP with an LFO wobble on cutoff and frame, drive, Dimension. Macros WOBBLE, GROWL, GRIT, WIDE.
- Clock Weirdo: Henon chaos table and a Rossler table with ring mod; clocked S&H on cutoff, frame and pan; bit-crush and triplet ping-pong delay. Macros WEIRD, CRUSH, CUTOFF, ECHO.
- Step Sequence: Ladder Square and Ladder Saw, a 16-step LFO on cutoff and frame, Dual-pattern trance gate, dotted delay. Macros FILTER, GATE, PWM, ECHO.
- Theremin: Sine with a triangle shimmer layer, long glide, vibrato that fades in on the mod envelope, AW Chamber. Macros GLIDE, VIBRATO, AIR, SPACE.
- Chip Lead: PWM pulse with quantise warp plus a triangle bass, built-in arpeggiator (octaves, up-down, 1/16), bit-crush, PWM LFO; dry. Macros CRUSH, PWM, DUTY, BASS.
- Glass Lead: Glass Harmonics table with a Bell layer an octave up, slow swell, vibrato, shimmer reverb. Macros SHIMMER, BELL, BRIGHT, VIBRATO.
- Acid Squelch Lead: Mono saw through the 303 Acid filter, fast filter envelope, slide, drive, dotted delay. Macros CUTOFF, SQUELCH, SLIDE, ECHO.
- Formant Shifter: Talkbox table with the spectral Formant warp, frequency shifter and Dimension. Macros FORMANT, SHIFT, TONE, DIM.
- PD Formant Voice: Sine with PD Res II warp moved by an LFO, 2-voice unison, slow swell, chorus and a large hall. Macros FORMANT, TONE, CHORUS, HALL.
- Vowel Bank Voice: Vowels Male table whose frame a smooth-random LFO walks through vowels, chorus, hall. Macros VOWEL, TONE, CHORUS, HALL.
- Talking Filter Lead: Saw unison through the Talking filter with its morph driven by a smooth-random LFO and the wheel, drive, phaser. Macros TALK, RESO, DRIVE, PHASE.
### Pluck

- Comb Pluck: Comb table with its frame swept by the mod envelope, AW YNot LP, strong Comb FX (440 Hz, feedback 0.85), echo, spring. Macros COMB, FEEDBACK, BITE, ECHO.
- KS Bell: Physical string excited by a Tine model with high stiffness (inharmonic), an octave up, Verbity2 reverb. Macros DAMP, RING, BRIGHT, HALL.
- Chord Stab: 6-voice Hypersaw with the Major chord engine, AW Angle, drive, room, limiter; short house stab. Macros CUTOFF, STAB, GRIT, SPREAD.
- Minor Chord Stab: Full Organ table with the Minor chord engine, Leslie-style tremolo and chorus, spring. Macros LESLIE, CHORUS, TONE, SPRING.
- Vocal Chop: Sampled "Ah" voice an octave up, one-shot, band-pass formant filter, 1/8 delay, hall. Macros FORMANT, CHOP, ECHO, HALL.
- Metal Keys: Metal Hit sample an octave down, AW ChorusEnsemble, plate; low gong-like keys. Macros TONE, SHORTEN, ENSEMBLE, PLATE.
- Tremolo Keys: Physical Tine-excited tone with a tube amp, strong tremolo and spring reverb (suitcase electric piano). Macros TREM, DRIVE, TONE, SPRING.
- Resonator Bell: Noise-burst string through the Resonator bank (+12), hall; the body rings. Macros RING, TUNE, TONE, HALL.
- Nylon String: Physical string, burst excite, soft pick, body EQ and room. Macros BRIGHT, DAMP, BODY, ROOM.
- Pizzicato: Pluck Stiff table an octave up through AW Z LP with a strong filter-envelope bounce; dry. Macros BRIGHT, BOUNCE, DECAY, RESO.
- Diode Blip: Mono Acid Ramp blip an octave down, AW Z LP at high resonance, dotted ping-pong echo. Macros ACID, CUTOFF, GLIDE, ECHO.
- Sitar Pluck: Physical string with bridge buzz and stiffness, sympathetic Comb FX drone, hall. Macros BUZZ, DRONE, TONE, HALL.
- Detuned Zither: 3-voice Tine table with wide detune, chorus and hall, 4 s decay. Macros DETUNE, TINE, CHORUS, HALL.
- Envelope Warp Pluck: Sine with PD Res I warp swept by the mod envelope, tape delay with wow, room. Macros WARP, SWEEP, WOW, ECHO.
- Glass Bell: Glass Harmonics table with a Bell layer, shimmer reverb. Macros BRIGHT, PARTIALS, DECAY, SHIMMER.
- Ring Bell: Ring Sine table, ring-mod FX at 523 Hz, AW ChorusEnsemble. Macros RING, TONE, DECAY, ENSEMBLE.
- Glass Mallet: Physical Hammer-excited string, stiff and bright, an octave up, plate. Macros MALLET, RING, TONE, PLATE.
- Plucked Bell Stack: Three Bell-table layers (unison, octave, fifth above), chorus and kCathedral reverb. Macros STACK, TONE, CHORUS, CATHEDRAL.
- Harp Cascade: Physical string with the delay's Echo 123 taps cascading it, hall. Macros CASCADE, TONE, DAMP, HALL.
- Muted Cello Pizz: Wood table an octave down through AW Z LP with a filter-envelope pluck, body EQ, small room. Macros BODY, MUTE, TONE, ROOM.
- Baroque Lute: Harpsichord table pair through AW Z LP, mid-presence EQ, hall. Macros COURSES, BRIGHT, DECAY, HALL.
- Hypersaw Pluck: 7-voice Hypersaw with a big filter envelope, jet flanger and gated reverb. Macros CUTOFF, JET, SPREAD, GATED.
- Formant Pluck: Vowels Child table through the Formant filter with an envelope "wah" and the Vowel FX, ping-pong echo. Macros WAH, VOWEL, DECAY, ECHO.
- Resonant Drop: Hypersaw an octave down with a pitch drop from the mod envelope and an AW Z LP at high resonance, echo. Macros DROP, RESO, CUTOFF, ECHO.
- PWM Pulse: Two detuned PWM pulses a fifth apart, two LFOs on their widths, chorus and phaser. Macros PWM, TONE, CHORUS, PHASE.
- KS Marimba: Physical string with a Hammer excite and high stiffness, short wooden decay, Tilt EQ. Macros MALLET, WOOD, TONE, RING.
- Glass Keys: Glass table with a Tine layer and FM, chorus, shimmer reverb. Macros FM, TINE, CHORUS, SHIMMER.
- Muted Guitar: Physical string with a Pulse excite and heavy damping, an octave down, tube amp and mid-boost EQ. Macros CRUNCH, MUTE, TONE, MID.
- Wood Bar Hybrid: Body engine kept; dry, high-shelf EQ. (macros unchanged)
- Metal Plate Hybrid: Body engine kept; chorus and shimmer reverb. (macros unchanged)
- Bell Shell Hybrid: Body engine kept; AW Galactic and a half-note delay, +gain. (macros unchanged)
- Glass Shell Hybrid: Body engine kept; large shimmer reverb. (macros unchanged)
- Coupled String Body: Body engine kept; Dimension and hall. (macros unchanged)
- West Body Pluck: Body engine kept; drive and dotted delay. (macros unchanged)
- Buchla Bongo Melody: Body engine kept; ping-pong 1/8 delay and flanger. (macros unchanged)
- Glass Chime Body: Body engine kept; large shimmer reverb. (macros unchanged)
- Marimba Bar Body: Body engine kept; warm EQ, +gain. (macros unchanged)
