# Surge Packs

Human-made wavetables from the third-party packs that ship with Surge XT
(`resources/data/wavetables_3rdparty` in github.com/surge-synthesizer/surge,
read 2026-10-18). Surge's installer carries them with their authors'
permission; they are for ilana's personal use in ilanaSynth and are not to be
redistributed.

Packs: A.Liv, Art of Sound, Damon Armani, Emu, Emu VSCO, Layzer, Ocean Swift
Synthesis, Philippe Favre, Qonundrai, TNMG, Venus Theory.

Rebuild with `python3 tools/import_surge_wavetables.py <that folder> "content/wavetables/Surge Packs"`.
Each table becomes a mono .wav with a `clm ` chunk naming its frame size.
Skipped: one-shot samples and anything whose length does not split into
frames (162 of 730 files); tables over 256 frames are thinned to 256.
Copy or link the folder into `Documents/ilanaSynth Wavetables`; the
LIBRARY button in the wavetable browser lists it.
