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
- **A slot with MIX 0.0 still runs the whole effect** (0.066 % against 0.061 % at mix 1.0 in the first run: no saving). Bypassed slots are skipped.
- Whole-rack sleep: after 2 s of silence in and out. There is no per-effect or per-branch idle skip.
- Block size: effects scale with samples, not with calls. The same 41 effects run as 8 chunks of 64 cost the same total microseconds as one chunk of 512 (table below), with one exception, Freeze. So rendering the FX graph in short chunks, which a short feedback loop needs, is almost free. What is not free is the engine's own fixed cost per processBlock: about 25 microseconds plus 0.12 microseconds per sample (no FX, one Live voice): block 32 = 4.5 % of a core, 64 = 2.5 %, 128 = 1.5 %, 256 = 1.0 %, 512 = 0.8 %, 1024 = 0.7 %. This is the host's choice, not the graph's.

Run as 8 x 64 samples versus 1 x 512 (microseconds per 512 samples): Reverb 22.9 / 25.5, Chorus 14.6 / 12.1, Delay 15.9 / 12.7, Phaser 34.6 / 36.2, EQ 11.4 / 9.9, Vocoder 119 / 120, Airwindows 94 / 93, AW Tape 72 / 71. **Freeze 9.9 / 178.6**: something in it is per call or per block-size dependent, not per sample. Look at it before the patcher runs it in chunks.

Side finding (not FX): in the effect build, a Live oscillator fed digital silence costs about 5.8 % of a core, about seven times the cost with signal (0.8 %), and the rack never goes to sleep because of it. Reproduce with `ILANA_BENCH_ONLY=sleep`. Not investigated further; flagged for whoever touches Live input.

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

1. **Before building:** fix or explain Freeze's per-call cost, and decide who owns the silent-input cost in the Live oscillator.
2. **A:** the compiled op list, pooled buffers, feedback ring, the silence detector, and the CPU readout, built and benchmarked on a fake graph of Utility nodes (target: 0.4 % for 16 nodes). No UI yet.
3. **17:** per-instance state, then the compiled graph behind the existing slot list, then the patcher sub-tab. Re-run `ilanaFxCpuBench` on each step and paste the numbers into the PR.
4. **16:** the compiled routing table in `Voice`, default table equals today's chain.

## Reproducing

- `cmake --build build --target ilanaFxCpuBench` then `build/ilanaFxCpuBench_artefacts/Release/ilanaFxCpuBench`. `ILANA_BENCH_ONLY=types|series|parallel|overhead|sleep`, `ILANA_BENCH_BLOCKSIZE=64`, `ILANA_BENCH_BLOCKS=1500`. Needs the Linux JUCE libraries installed (alsa, X11, freetype, fontconfig, GL, curl). Nothing else runs during a measurement; the run-to-run noise on the box was about 0.05 % of a core, so only differences above 0.1 % mean anything.
- `g++ -O2 -std=c++20 -pthread docs/routing-cpu/routing_proto.cpp -o routing_proto && ./routing_proto`.
- Raw output of the runs behind this page: `docs/routing-cpu/bench-output.txt`.

## Limits of this study

One machine, one input signal, default settings of every effect (some defaults are mild: Delay and Reverb at default MIX), no multi-voice patch under the rack, no Windows measurement, no real host. Hosts that run several instances at once or use fewer cores than this box change the thread numbers; the denormal and per-node overhead numbers should carry over. Where the box is noisy the table says 'about'.
