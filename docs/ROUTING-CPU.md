# Routing CPU guide

Rules and budgets for the shared patch-graph foundation (REVIEW-PLAN step A), the FX patcher (17) and reroutable oscillators (16). Written 2026-10-10 from measurements on the cloud Linux box (4 cores, gcc Release, 48 kHz, block 512 unless stated) and from reading JUCE, Vital and Surge XT source. Absolute numbers are for that box; ratios and the rules are what carries over. Every number below can be reproduced with the two tools in the last section. Nothing in the shipping engine changed.

Where a claim is **measured** it came from the tools here; **read** means seen in source code; **unverified** means a forum or docs claim nobody here reproduced.

## The finding in one paragraph

The FX rack is cheap and the voices are not. The 41 effect types cost 2 to 120 microseconds per 512-sample block each (0.02 to 1.1 % of a core), a full rack of ten ordinary effects is about 1 to 3 %, and a heavy patch's four held notes alone cost around 4.6 % (HANDOFF, library bench). So the patcher does not need clever scheduling to be affordable. It needs three things: no per-node overhead beyond a few microseconds, no per-block allocation or copies it can avoid, and a rule that stops one badly built node (a denormal tail, an FFT per call) from costing 10 to 100 times what it should. Multi-core for FX branches is not worth building: only 4 of the 41 effects cost enough per block to beat the thread hand-off.

## Measured: what exists today

Engine in effect mode, Live oscillator held by DRONE, 0.3 sine plus noise in, `ilanaFxCpuBench`. "net" is cost above the empty rack. 1 % of a core is 107 microseconds per 512-sample block.

| Effect (default settings) | us / block | | Effect | us / block |
|---|---|---|---|---|
| Utility, Widener, Stutter | 2 to 4 | | Phaser | 36 |
| Crush, Haas, TapeStop, Tilt, Trance Gate | 4 to 6 | | AW Delay, AW Console, AW Lo-Fi | 44 to 57 |
| Tremolo, FreqShift, RingMod, Limiter, Feedback | 5 to 8 | | AW Modulation, AW Tape | 71 to 75 |
| EQ, Comp, Comb, Vowel, Delay, Chorus | 10 to 13 | | Airwindows (all-in-one) | 93 |
| Flanger, Octaver, Drive, Amp, Reverb, Dimension, OTT | 19 to 26 | | Vocoder | 120 |
| Smear, AW Reverb, AW Saturation, AW Dynamics, AW EQ, AW Stereo | 16 to 37 | | **Freeze** | **179** |

Plumbing around a slot (measured, 512 block unless noted):

- Series versus parallel (`fx_routing`) cost the same within noise: 4 x Drive 0.51 % series, 0.48 % parallel; 8 x Delay 1.0 % series, 1.85 % parallel (the parallel copy and mix-down, about 0.8 % extra at 8 branches).
- Per-slot overhead (meters, ramps, level scans, the CPU timer): about 1.3 microseconds per slot per block at 512 (10 Utility slots = 0.13 % net), about 4 microseconds per slot per call at 64.
- A slot with MIX 1.0 runs in place. MIX below 1, or SOLO, copies the block to a scratch buffer and blends: +0.04 to 0.1 %. A band split adds two filter pairs: +0.15 %.
- A slot whose MIX has settled at 0.0 is skipped like a bypassed one (since the FX CPU leaks change; before it ran the whole effect for no saving). Its memory is left as it was, as for a bypassed slot.
- Whole-rack sleep: after 2 s of silence in and out. There is no per-effect or per-branch idle skip.
- Block size: effects scale with samples, not with calls. The same 41 effects run as 8 chunks of 64 cost the same total microseconds as one chunk of 512 (table below), with one exception, Freeze. So rendering the FX graph in short chunks, which a short feedback loop needs, is almost free. What is not free is the engine's own fixed cost per processBlock: about 25 microseconds plus 0.12 microseconds per sample (no FX, one Live voice): block 32 = 4.5 % of a core, 64 = 2.5 %, 128 = 1.5 %, 256 = 1.0 %, 512 = 0.8 %, 1024 = 0.7 %. This is the host's choice, not the graph's.

Run as 8 x 64 samples versus 1 x 512 (microseconds per 512 samples, **median per call**): Reverb 22.9 / 25.5, Chorus 14.6 / 12.1, Delay 15.9 / 12.7, Phaser 34.6 / 36.2, EQ 11.4 / 9.9, Vocoder 119 / 120, Airwindows 94 / 93, AW Tape 72 / 71. Freeze read 9.9 / 178.6, which looked like a per-call cost. **It was not (corrected 2026-10-10, see the FX CPU leaks section below):** Freeze does one FFT frame every 512 samples (its hop), so at 64 samples only every eighth call does the work and the median call is the cheap one. Its mean cost per 512 samples is the same at either block size. Any benchmark of an effect with a hop longer than the block must report the mean, not the median (the bench prints both now).

**Silent Live input (corrected 2026-10-10).** The first version of this page said a Live oscillator fed digital silence cost about seven times what it does with signal and kept the rack awake. That was a bug in the benchmark, not in the engine: the sleep scenario passed the same `MidiBuffer` to every `processBlock` call, so the drone's single note-on was replayed on every block and retriggered the voice each time (any input level did it, loud noise too). With a fresh MIDI buffer per block, silence costs the same as signal (0.95 % against 0.98 % of a core) and the rack falls asleep about 2 s after the last tail. Hosts hand over a fresh buffer each call, so nothing shipped was affected.

## Measured: prototypes (`docs/routing-cpu/routing_proto.cpp`, standalone)

**Hand-off to worker threads** (3 workers plus the caller, 4 cores, wake from idle each block, like `VoiceThreads`):

| work per branch | serial, 2 branches | futex wait | spin |
|---|---|---|---|
| 5 us | 10 | 34 | 6 |
| 20 us | 40 | 48 | 21 |
| 50 us | 100 | 79 | 51 |
| 100 us | 200 | 132 | 101 |
| 300 us | 600 | 326 | 301 |

The futex path pays about 30 microseconds of wake latency per block; spinning pays about 1 but burns a core all the time (not acceptable in a plugin). Break-even for futex wake: a branch must cost more than about 40 microseconds per block, and there must be at least two of them. By the table above that is Phaser, Airwindows, Vocoder, AW Tape, Freeze, and the AW delay/lo-fi/modulation modules. Everything else is cheaper than the wake. Real hosts and Windows add their own scheduling noise (unverified: Windows WaitOnAddress wake times were not measured here).

**Denormals**: an 8-comb feedback tail (reverb-like, 20 s) runs **14.4x slower with FTZ/DAZ off** (83 ms against 5.8 ms). With FTZ/DAZ off, adding a 1e-18 alternating offset in the loop fixes it completely (5.5 ms). `processBlock` already sets `ScopedNoDenormals` (PluginProcessor.cpp:1087) and `VoiceThreads` sets it in each worker. Hosts do clobber MXCSR between calls, so set it at the top of every callback, not once.

**Buffer pooling**: random graphs with one buffer per node output need N buffers; releasing a buffer after its last reader needs 3.8 for 6 nodes, 5.1 for 10, 6.7 for 16, 8.7 for 24. Each stereo 512 buffer is 4 KB, so the whole saving is cache footprint (fits L1/L2 either way), not time. Copying a stereo 1024-float block costs 54 ns and adding it with a gain 230 ns. Both are noise next to a 5 microsecond effect. Pooling is worth doing because it is easy, not because it is fast.

**Silence detector**: a SIMD peak scan of a stereo block costs about 0.5 microseconds. Cheap enough to run on every node every block.

## Findings from other software (sources read 2026-10-10)

- **JUCE AudioProcessorGraph** (`juce_AudioProcessorGraph.cpp`, read): the render sequence is built on the message thread and swapped in on the audio thread with a try-lock and a pointer swap (no allocation on the audio thread); buffers are reused after their last consumer; shorter branches get compensating delay lines sized at build time. Not usable as is: a cycle is not rejected and a feedback edge just reads zeros (no delay, no previous block), it takes a callback lock per node per block, and it renders serially.
- **Vital** (`ProcessorRouter`, `feedback.cpp`, read): a cycle is detected when connecting and a Feedback node is inserted automatically. It is a 128-sample ring giving exactly one block of delay. Disabled processors are skipped entirely; the reverb hard-resets when switched off (user-driven, not tail-aware). FTZ/DAZ is set at the top of the audio callback.
- **Surge XT** (`SurgeSynthesizer.cpp`, `Effect.cpp`, read): inserts are serial, sends parallel and summed. `process_ringout` counts blocks since input and skips an effect once the count passes its own ring-out time (default never; Distortion 1600 blocks, Vocoder 500, Conditioner 100, Convolution from the IR length). Block size is fixed at 32.
- **Bitwig Grid** (user guide): a direct feedback cable is refused; the Long Delay module is the sanctioned loop and its minimum delay is one block.
- **FL Studio Patcher** (manual): modules at the same depth run on different threads, depths run in order, latency compensation is automatic. Loops are not documented.
- **Reaper**: anticipative FX and an auto-bypass of tail-reporting plugins since 6.71 exist, from release notes and forums; internals unverified.
- **Not found anywhere**: published CPU numbers for any of these, a measured multi-core wake cost, or a lazy-oversampling gain. Nothing above is borrowed as a number.

## Budgets

Per 512-sample block at 48 kHz, on the measuring box. A budget is a ceiling to hold the builders to, not a target.

| Item | Budget |
|---|---|
| Graph overhead per node per block (meters, ramps, copies, bookkeeping), at 512 | 3 us (measured today: 1.3) |
| Graph overhead per node per call at 64 samples | 6 us (measured today: about 4) |
| Allocation, locks, system calls on the audio thread | 0 |
| An effect run at 64 samples against at 512 | within 25 % of the same total microseconds |
| Default rack of ten ordinary effects | 3 % of a core or less |
| Worst case rack (ten of the 120 us class) | 11 % of a core; a patcher preset above this gets a CPU warning on its card |
| Whole graph, 16 nodes, no effect work (all Utility) | 0.4 % of a core or less |
| A patch that uses no routing | the same as main today, within noise (the fingerprint check proves the sound, `ilanaFxCpuBench` the cost) |
| Feedback loop extra cost | one block copy per loop per chunk (54 ns) plus the nodes in the loop |
| Silence skip for a node whose input is silent and tail is spent | under 1 us per block |

## Rules the builders must follow

### A. Shared patch-graph foundation (step A) and the FX patcher (17)

1. **Compile, don't interpret.** Editing the graph builds a flat list of operations (node, input buffer indices, output buffer index, gain) on the message thread, with a topological sort and every buffer allocated. The audio thread runs the list and nothing else. Swap the new list in with one atomic pointer exchange (a try-lock or a lock-free slot as JUCE does); the old list is freed on the message thread. A cable drag must never allocate on the audio thread.
2. **Cycles are allowed only through a feedback edge.** Find back edges by depth-first search on connect, and insert a delay on each (Vital's approach). The delay is one render chunk, see rule 12. Report the loop's latency in milliseconds on the cable. Never let a direct zero-delay loop run (JUCE reads zeros, Bitwig refuses; both are worse than a one-chunk delay).
3. **Pool buffers by liveness.** Release a buffer after its last reader (the 3.8 to 8.7 buffers above). Process in place whenever a node has one reader and the node supports it. Never size a buffer or call `setSize` in the audio callback (today's `fxBranch.setSize` and `fxParallelIn.setSize` calls in `Effects.cpp` are the pattern to remove).
4. **Every node owns its state.** Several instances of one effect need their own delay lines, filters and ramps. Today the per-effect state and `fx_<name>_*` parameters are one set per type, so two Reverbs in the current slot model share state (the parallel and series benchmarks above only show the compute cost). Instances need their own state objects, created off the audio thread when the node is added or the sample rate changes.
5. **Skip, don't just mute.** A node whose input is silent (peak under -110 dB, matching the rack sleep) and whose tail is spent produces silence without running. Track the tail per node: effects declare a tail length in samples (reverb, delay and comb from their own time and feedback), a counter of samples since last input is compared to it. Surge's ring-out is the model. Run the detector on every node input, every block (about 0.5 us). Bypassed nodes and nodes that cannot be heard (mix 0 and no downstream reader of the dry path, muted, disconnected from the output) are skipped by the compiler, not by the audio thread. A slot at MIX 0 runs today; the compiler must drop it.
6. **Dead branches don't run.** Anything not on a path to the output (and not a meter the user is looking at) is left out of the compiled list.
7. **Parallel is a plain sum, not a copy per branch.** One shared input buffer, branches write into pooled buffers, a mix-down node adds them with a gain (230 ns per branch). Do not copy the input into every branch when the branch's first node can read it in place.
8. **Don't thread the FX graph yet.** The break-even is 40 microseconds per branch with at least two branches that big. Only Phaser, Vocoder, Airwindows, the AW modules and Freeze reach it, and a typical patcher preset will not have two in parallel. If it is wanted later, reuse the `VoiceThreads` hand-off (atomic wait, no mutex, caller works too) and send a branch to a worker only when the running per-slot CPU average (already in `fxSlotCpu`) is above 40 microseconds and a sibling branch also is. Re-decide with measurements from the real patcher.
9. **Denormals.** Keep `ScopedNoDenormals` at the top of every callback, and in any worker thread. Inside any feedback loop (the graph's own and every effect's), also add a tiny alternating offset or flush state below 1e-20 where state is stored, because MXCSR can be reset by the host (measured: 14x slower tails otherwise, the offset alone fully fixes it).
10. **Ramps and smoothing cost per sample; keep them per node and skip them while stable.** `blendRamp.begin` per slot is fine at 1.3 us; per-sample smoothing of unchanged parameters is the thing to avoid.
11. **Report cost where the user sees it.** Keep the per-node CPU average (`fxSlotCpu` today). Show it on the node and warn above 120 microseconds of one node or 11 % of a core for the whole graph.
12. **Chunk size for loops.** A graph with a feedback edge renders in chunks of 64 samples (1.3 ms of loop delay at 48 kHz; effect cost does not depend on chunk size, per the table, except Freeze, which must be fixed first or excluded from loops). A graph without feedback renders the whole host block in one pass. 32 samples is the floor (0.67 ms): below that the per-call overhead (4 us per node) starts to show.
13. **Oversampling is lazy.** An oversampled node only up- and down-samples while its nonlinearity is active (drive above 0, MIX above 0). No data was found on a measured saving; the rule is the same as for any other skipped work: if the output cannot change, don't run it.
14. **Migration check stays.** A patch with no routing compiles to the same op list as today's series or parallel chain. The fingerprint check proves the sound, `ilanaFxCpuBench` proves it costs the same.

### B. Reroutable oscillators (16)

Read from `Voice::renderNextBlock` (not benchmarked here; HANDOFF has the voice numbers): the voice already runs in 16-sample chunks with per-route buses (`chunkBusL/R[maxChunk][FilterRoute::Count]`) and stage loops for drive, Filter 1, Filter 2, WEST, body and amp.

1. **Routing is chosen per patch, not per sample.** Compile the oscillator-to-bus assignment and the filter order when the patch or a cable changes (message thread), store it as a small table in `Voice`, and let the chunk loops index buses by it. No branch on the route inside the sample loop. A route change while a voice sounds takes effect on the next chunk; use a short crossfade (one chunk) if it clicks.
2. **Skip empty stages.** A filter with no oscillator feeding it and no other input is not run (today Filter 2 at 19 kHz and low resonance already takes an open bypass). The compiler marks stages with no input unused; the voice loop checks one flag per chunk.
3. **Voices stay independent.** A per-voice graph compiles once and every voice uses the same table, so voices still render in parallel on `VoiceThreads` and a patch with unchanged routing must keep the same chunk code path (so the default routing costs what main costs now, which is the budget).
4. **Cross-feedback (filter F2 into F1, oscillator into itself) needs a one-chunk delay** (16 samples, 0.33 ms at 48 kHz), the same rule as 2 above. Today filter FM and the body-coupling mode already drop to one sample per chunk; do not add more modes that do.
5. **Voice-side buffers are per voice and fixed size** (`maxChunk` arrays on the stack today). Adding a bus means adding a column to that array, not a heap buffer.

## Recommended order of work

1. **Before building:** done 2026-10-10: Freeze has no per-call cost (it is one FFT frame per hop) and is now about 40 % cheaper; the silent Live input cost was a benchmark bug.
2. **A:** the compiled op list, pooled buffers, feedback ring, the silence detector, and the CPU readout, built and benchmarked on a fake graph of Utility nodes (target: 0.4 % for 16 nodes). No UI yet.
3. **17:** per-instance state, then the compiled graph behind the existing slot list, then the patcher sub-tab. Re-run `ilanaFxCpuBench` on each step and paste the numbers into the PR.
4. **16:** the compiled routing table in `Voice`, default table equals today's chain.

## Reproducing

- `cmake --build build --target ilanaFxCpuBench` then `build/ilanaFxCpuBench_artefacts/Release/ilanaFxCpuBench`. `ILANA_BENCH_ONLY=types|series|parallel|overhead|sleep`, `ILANA_BENCH_BLOCKSIZE=64`, `ILANA_BENCH_BLOCKS=1500`. Needs the Linux JUCE libraries installed (alsa, X11, freetype, fontconfig, GL, curl). Nothing else runs during a measurement; the run-to-run noise on the box was about 0.05 % of a core, so only differences above 0.1 % mean anything.
- `g++ -O2 -std=c++20 -pthread docs/routing-cpu/routing_proto.cpp -o routing_proto && ./routing_proto`.
- Raw output of the runs behind this page: `docs/routing-cpu/bench-output.txt`.

## Limits of this study

One machine, one input signal, default settings of every effect (some defaults are mild: Delay and Reverb at default MIX), no multi-voice patch under the rack, no Windows measurement, no real host. Hosts that run several instances at once or use fewer cores than this box change the thread numbers; the denormal and per-node overhead numbers should carry over. Where the box is noisy the table says 'about'.


## FX CPU leaks (2026-10-10, branch `claude/fx-cpu-leaks-td00rw`)

What the two suspected leaks turned out to be, and the cheap rules applied. Numbers: `ilanaFxCpuBench`, same box, interleaved before/after runs; run-to-run noise about 0.05-0.1 % of a core, so the block-level rows are indicative and the micro-benchmark rows are the ones to trust.

- **Silent Live input:** not a leak (see above). `tools/FxCpuBench.cpp` now clears the MIDI buffer each block.
- **Freeze:** not per call; the cost is the FFT work per hop, 4 FFTs of 2048 points per hop (forward and inverse, two channels), plus an `atan2`, a `sincos` and a double-precision `floor` for each of the 1025 bins in each channel. Now the left channel rides in the real part and the right in the imaginary part of one complex FFT (2 FFTs per hop), the phase is a 1e-5 rad polynomial `atan2`, the turn-wrapping is one conditional, the band edges are tabled and the rings use masks. Freeze alone, block 512, `ilanaFxCpuBench`: mean 2.92 % to 2.27 % of a core (net of the empty rack 1.90 % to 1.07 %); at block 64 mean 5.71 % to 4.29 % (the median call stays cheap, p99 is the FFT call). Direct timing of the old two-mono-freezers code against the new class on identical noise (stereo, one frame per 512-sample block, two runs): 156 / 217 us to 96 / 128 us live, 172 / 229 us to 104 / 139 us frozen, about 40 % less. The output differs from the old mono-pair code by float rounding only (largest sample difference 3.6e-5 on a 0.53 peak, about -83 dB).
- **FTZ/DAZ:** `ScopedNoDenormals` is now the first line of `processBlock` (it was already in `processChunk`).
- **MIX 0 skip:** `runFxSlot` returns before running the effect when the slot's blend was 0 last block and is 0 now (not solo, not banded). Meters and the CPU share still update.
- **Checked:** 0 of 698 preset fingerprints changed; unit, FX, UI tests and pluginval at strictness 10 pass; the changed files compile with clang-cl against the MSVC library and Windows SDK. The MIX 0 skip is within the bench noise (a settled mix-0 slot now measures the same as a bypassed one; before it measured the same as mix 1.0).


## Step A measured (2026-10-11, branch `claude/patch-graph-foundation-phkb1a`)

`ILANA_BENCH_ONLY=graph ilanaFxCpuBench`, same box, 48 kHz. "per node" is the graph's time minus the same work done in a plain loop, divided by 16.

| 16 gain nodes | block 512 | block 64 |
|---|---|---|
| series (in place, 1 buffer, 0 copies) | 0.010 us / node (0.06 with the per-node CPU readout) | 0.012 (0.067) |
| parallel (3 pooled buffers) | 0.041 (0.100) | 0.025 (0.067) |
| series with one loop (64-sample chunks) | 0.27 (0.87) | 0.033 (0.091) |

Whole graph: 0.02 to 0.16 % of a core for 16 nodes (budget 0.4 %). The per-node readout reads the clock twice per node per chunk, so a looped graph at a big host block pays it eight times per block; still under the 3 us budget.

The real rack through its migrated graph (`processEffectsGraph`) against the fixed loop, block 512, mean us per block: 10 x Utility series 39.7 / 40.1, parallel 44.6 / 45.1; 4 x Drive series 116.6 / 115.7; 8 x Delay parallel 174.2 / 176.8; Drive, Chorus, Delay, Reverb series 90.0 / 90.9, parallel 108.6 / 108.9. At block 64 the differences are 0.0 to 0.9 us. The whole engine against main (the overhead group and `ilanaSnapshot --heldcpu "Neuro Wobble"`, alternating runs) is unchanged within noise: 10.1 % against 10.1 %.

## Step 17 measured (2026-10-11, branch `claude/fx-patcher-zhhz79`)

Same box, 48 kHz, block 512, but the box was slower and noisier than for step A (the same `--heldcpu` run reads 15-18 % today against 10.1 % then), so every figure is a mean of alternating runs of the step A build (`871d8a3`) and this one.

What changed on the audio path: SERIES and PARALLEL now run through the rack's compiled graph (`fxRackRunner`); each slot looks up its engine and value table (`engineForSlot`, `slotValues`); PATCHER runs its own plan (`fxPatchRunner`) with the silence skip on and per-type tails (`fxTailSamples`). A patch that uses none of it renders sample for sample as before (0 of 698 fingerprints, `ILANA_GRAPH_TEST`).

| `ILANA_BENCH_ONLY=graph`, rack via `processEffects`, mean us per block (5 runs each) | before | after |
|---|---|---|
| 10 x Utility series | 47.3 (45.9-48.6) | 53.3 (47.0-61.7) |
| 10 x Utility parallel | 54.8 (51.4-63.9) | 60.1 (51.2-68.5) |
| 4 x Drive series | 150.8 | 145.0 |
| 8 x Delay parallel | 257.2 (251.8-266.6) | 353.8 (289.5-452.3) |
| Drive, Chorus, Delay, Reverb series | 157.0 | 164.5 |
| Drive, Chorus, Delay, Reverb parallel | 164.8 | 192.6 (157.5-251.9) |

The 16-node graph overhead is unchanged (0.03 to 0.96 us per node). About 0.5 us per slot of bookkeeping is new (the Utility rows), under the 3 us budget. **8 x Delay parallel is the open item:** it is consistently slower and its spread is wide, but callgrind counts the same instructions per Delay call before and after (`processDelay` against its delay line, the same mix to 0.02 %) and the same simulated cache misses, so the cause is where the engine's memory now sits (the effect state moved from the processor into a separate `FxEngine` allocation), not extra work. Single Delays in series (`ILANA_BENCH_ONLY=series`) and the whole engine show no change. Re-measure on the Windows PC before deciding whether to pin the engine's layout.

Whole engine, `ilanaSnapshot --heldcpu "Neuro Wobble"`, 4 alternating runs: before 15.8 % (14.7-17.9), after 16.1 % (14.5-16.7). Unchanged within noise.

The patcher itself (`ILANA_BENCH_ONLY=patcher`, the same processor, PATCHER off and on in turn, 3 runs):

| Drive, Chorus, Delay, Reverb | PATCHER on minus off |
|---|---|
| same series wiring | -0.7 to +3.6 us |
| series plus a Reverb -> Drive loop at 50 % (64-sample chunks) | +21.8 to +22.6 us (14 %) |

The loop's cost is eight chunks instead of one for the four effects in it: about 0.7 us per node per extra call (budget 6 us), and 14 % more than the same effects in one pass (budget 25 %). Loop cables are capped at 70 % and start at 50 %, so a loop can't grow without bound (a 0.7 loop through four effects peaks at 6.9 in the test, then decays).

Rules followed: A1 (plans compiled on the message thread; a slot change runs the old fixed loop for the block until the new plan is in, offline it compiles at once), A2 (loops only through a 64-sample delay, shown on the cable and in the inspector), A3 (pooled buffers, no `setSize` on the audio path), A4 (an instance owns an `FxEngine`, made off the audio thread; freed engines are deleted on the message thread at least a second later), A5 (PATCHER skips a node whose input is silent and tail is spent; effects that can sound on their own are never skipped), A6/A7 (from step A), A9 (the loop ring flushes denormals), A11 (CPU per node on the node and in the inspector, total in the inspector), A14 (fingerprints 0 changed).

## Step 16 measured (2026-10-11, branch `claude/reroutable-oscillators-hl008x`)

What follows the rules in B: the cables compile to a small table (`VoiceRouting::compile`: stage order, used stages, one bus per distinct set of cables, at most 8) when the ten `cable_*` masks change, checked once per block by a 40-bit signature, no allocation. The voice loops index buses by it per chunk; stages nothing feeds, or that feed nothing, are not run. With ROUTING on CHAIN (every existing patch) the voice runs today's code path unchanged: 0 of 698 fingerprints changed, and `ILANA_OSC_ROUTING_TEST=1 ilanaTableTest` plays 600 of 659 presets sample for sample as cables (57 are not repeatable run to run, 2 are corners the cables don't model, 0 differ). Deviation from rule B1: the table is compiled on the audio thread (cheap, allocation-free) rather than on the message thread, so a host automating nothing and a preset load both stay in step with no lock.

Whole engine, `ilanaSnapshot --heldcpu` (12 held notes, multi-core on), 4 alternating runs, mean % of real time: main / this branch (chain) / the same preset's chain as cables (`ILANA_HELD_CABLES=1`) / cables with OSC 1 to F1 and OUT and OSC 2 to F2 (three buses, both filters run):

| Preset | main | branch | cables | split |
|---|---|---|---|---|
| Neuro Wobble | 11.3 (10.7-11.8) | 11.4 (10.6-12.0) | 11.8 (10.7-14.1) | 12.0 (10.6-12.5) |
| Supersaw Anthem | 11.1 (10.3-11.7) | 10.9 (9.7-12.5) | 11.4 (9.6-14.1) | 11.3 (9.1-14.3) |

All within the run-to-run noise of this box (about 2 points). A split patch costs the extra filter it runs and a bus add per chunk, nothing else.

## Step 17 follow-up: loop guard, loop level and delay (2026-10-11)

ilana's patch (Reverb into Smear and EQ, Smear into EQ, EQ back to Reverb at 50 %) ran away: two branches meet, so the loop gain was over 1, and it sat at the ring's old +-8 clamp (+24 dB) for good. Rule A2 now reads: a loop's ring passes the **loop guard** (a limiter at 0 dBFS, instant attack, 200 ms release, gain set every 16 samples and glided, then clipped), and a graph with a loop passes the **output guard** (the same on its output). Each loop cable has its own delay (one 64-sample chunk to 1 s; rings are sized for 1 s and taken over by the next plan when the cable is the same, so editing doesn't cut the loop) and its level (0-70 %, new ones 20 %).

Cost (`ILANA_BENCH_ONLY=graph` and `patcher`, alternating runs of main `44db408` and this branch, 48 kHz, block 512): under the ceiling a guard is one vectorised peak pass per chunk (16 gain nodes in series with a loop: 6.3 / 6.5 us on main, 5.4 / 8.2 us here, noise). Drive, Chorus, Delay, Reverb with a Reverb to Drive loop at 50 % (it rings, so both guards work): PATCHER on minus off 20.8 / 19.2 / 24.5 us on main, 24.2 / 21.7 / 19.6 us here. Unchanged within noise.

## FILTER effect measured (2026-10-12, branch `claude/reroutable-oscillators-hl008x`)

FX type 42 runs one voice filter model per channel on the rack, coefficients every 32 samples. `ILANA_BENCH_ONLY=filter ilanaFxCpuBench` (block 512 at 48 kHz, reso 0.8, drive 2; net = minus the empty rack, 1.18 % of a core; cloud box):

| Model | net, SPREAD 0 | net, SPREAD 100 % |
|---|---|---|
| Low Pass (AW Y) | 0.47 % | 0.41 % |
| Ladder LP | 1.41 % | 1.42 % |
| Moog Drive | 1.94 % | 2.02 % |
| Vowel | 0.52 % | 0.48 % |
| Comb + | 0.48 % | 0.56 % |
| AW Pear | 0.40 % | 0.66 % |
| Disperser | 0.18 % | 0.19 % |

For scale: Reverb 0.31 %, EQ 0.20 %, Tilt 0.09 % in the same run. The empty rack and every other effect run the same code as before (FILTER only runs when a slot holds it); 0 of 698 presets changed. In a loop it costs the same per pass, as every effect does (rules A1-A3 above).

