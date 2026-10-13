# ilana's pack (2026-10-13)

43 presets grown by `tools/preset_gen.py` from ilana's presets in `content/user-presets`.
Each concept was drawn 6 times; the draft kept scored best on: closeness to her presets
(CLAP audio embeddings, half weight on her presets of the same kind), distance from the
factory library and from the other picks, and the critic's score. Taste and novelty are
cosine measures (1 = the same sound as her nearest preset / as the nearest factory preset is 1 - novelty).

| Preset | Category | From her | What it adds | Taste | Novelty |
|---|---|---|---|---|---|
| Arp Gate Bass | Bass | ARPed pluck Bass | her arped pluck bass played straight through a trance gate instead | 0.85 | 0.12 |
| Cable Pluck Bass | Bass | patcher bass | her patcher bass re-cabled: oscillator 2 through the WEST voice | 0.93 | 0.22 |
| Disperser Sub | Bass | Retro Bass | a round sub with the Disperser's zap on every note | 0.91 | 0.12 |
| Operator Bass | Bass | E.PIANO 1 (ROM3A) modded | the DX piano's operators down two octaves with short envelopes: an FM bass | 0.62 | 0.17 |
| Reed Growl Bass | Bass | patcher bass | her patcher bass with the west folder pushed and an Airwindows acid filter | 0.98 | 0.25 |
| Upright Thump | Bass | real piano SOUND | the piano string plucked low and damped: an upright bass | 0.80 | 0.16 |
| West Fold Bass | Bass | Retro Bass | her retro bass folded by the WEST voice | 0.97 | 0.13 |
| Z Ladder Bass | Bass | Retro Bass | her retro bass on the Airwindows Z low pass, a short legato glide | 0.99 | 0.14 |
| Gate Stab Chords | Chords | ARPed pluck Bass | her arped bass's chord stack an octave up, gated instead of arpeggiated | 0.72 | 0.17 |
| Fold Drone | Drone | classic synth brass, no fx | her brass folded slowly by an LFO under a long space | 0.92 | 0.09 |
| Resonant Drone | Drone | phys_bowed pad | the bowed string held forever into sympathetic strings and the resonator | 0.81 | 0.08 |
| Chance Mallets | Generative | real piano SOUND | her piano's string under the probability sequencer, struck by a mallet | 0.83 | 0.11 |
| FM Walk | Generative | fm arp | her FM arp walking instead of running, in Airwindows space | 0.94 | 0.13 |
| Spray Harp | Generative | Strummed Pad modded | her strummed pad as a harp, every note spraying a few more up the scale | 0.83 | 0.15 |
| Bowed Piano | Keys | real piano SOUND | her piano with a quiet bowed string an octave up swelling in behind it | 0.86 | 0.14 |
| Felt Upright | Keys | real piano SOUND | her piano with a soft felt hammer, a shell body and sympathetic strings | 0.92 | 0.14 |
| Glass Harp Keys | Keys | velocity warp physical pluck | a stiff, bright hammered string ringing a bell body | 0.78 | 0.21 |
| Operator Clav | Keys | E.PIANO 1 (ROM3A) modded | the DX piano cut short and pushed through an Airwindows acid filter: a clavinet | 0.75 | 0.10 |
| Operator Tines | Keys | E.PIANO 1 (ROM3A) modded | her DX electric piano re-voiced on another algorithm with a brighter tine ratio | 0.80 | 0.13 |
| Reed Pump Organ | Keys | Hammered Strings Edited | her hammered strings blown by a reed exciter instead: a pump organ | 0.87 | 0.10 |
| Tine Room | Keys | real piano SOUND | the piano's string and board with a tine exciter on a bar body | 0.83 | 0.14 |
| Toy Box Piano | Keys | real piano SOUND | a small high piano on a plate body, through tape and lo-fi | 0.86 | 0.28 |
| West Marimba | Keys | caribbean keys | her caribbean keys as a struck low-pass gate on a bar body | 0.98 | 0.15 |
| Acid Fold Lead | Lead | classic synth brass, no fx | her brass folded hard and squeezed through the Airwindows acid filter, mono | 0.91 | 0.08 |
| Bowed Lead | Lead | phys_bowed pad | one bowed string, mono, with a few milliseconds of legato glide | 0.75 | 0.18 |
| Brass Section | Lead | classic synth brass, no fx | her classic brass widened by an ensemble | 0.98 | 0.07 |
| Operator Lead | Lead | E.PIANO 1 (ROM3A) modded | the DX piano's operators held as a mono lead with saturation | 0.68 | 0.13 |
| Pear Lead | Lead | trancey pad | her mono trance pad thinned into a lead, echoes instead of the wash | 0.78 | 0.16 |
| Reed Lead | Lead | patcher bass | her patcher bass's reed an octave up as a breathy mono lead | 0.67 | 0.22 |
| Bowed Choir | Pad | phys_bowed pad | her bowed pad sung through a vowel filter that drifts | 0.72 | 0.18 |
| Breathing Bow | Pad | phys_bowed pad | her bowed pad chopped soft by the trance gate before the space | 0.89 | 0.15 |
| Frozen Hammers | Pad | Hammered Strings Edited | her hammered strings held in a smear and a long Airwindows space | 0.70 | 0.16 |
| Holt Pad | Pad | trancey pad | her trance pad, polyphonic, through the Airwindows Holt filter swept by an LFO | 0.85 | 0.16 |
| Soundboard Bloom | Pad | phys_bowed pad | the bowed pad pressed into a dense soundboard and Airwindows spaces | 0.89 | 0.14 |
| Sympathetic Haze | Pad | Strummed Pad modded | her strummed pad ringing sympathetic strings into a hall | 0.82 | 0.10 |
| Vector Drift Pad | Pad | trancey pad | her trance pad made polyphonic, its vector path wandering slower | 0.87 | 0.16 |
| West Glow Pad | Pad | classic synth brass, no fx | her brass, slow and polyphonic, glowing through the west gate and an ensemble | 0.84 | 0.08 |
| Drop Pluck | Pluck | caribbean keys | a struck west-coast gate, the probability sequencer adding the odd octave | 0.84 | 0.11 |
| Euclid Mallets | Pluck | real piano SOUND | her piano's string struck in a euclidean rhythm by a mallet | 0.75 | 0.12 |
| Gated Glass Pluck | Pluck | fm arp | her FM arp held as chords and cut by a trance gate | 0.72 | 0.19 |
| Kalimba Bar | Pluck | velocity warp physical pluck | a tine plucked over a bar body, no soundboard | 0.90 | 0.22 |
| Koto Snap | Pluck | velocity warp physical pluck | a slapped, stiff plucked string zapped by the Disperser | 0.73 | 0.25 |
| Picked Steel | Pluck | crazy bass | her crazy bass's unison saw as a short pick, with a physical steel string under it | 0.70 | 0.12 |
