# Architecture map

For an agent arriving cold. Current state and rules: [HANDOFF.md](../HANDOFF.md). Why things are as they are: [DECISIONS.md](DECISIONS.md). Paths checked by `tools/check_doc_paths.py` (run by `tools/verify.sh`). `src/PluginEditor.cpp` is huge (thousands of lines): grep, don't read whole. `IlanaSynthAudioProcessor` is split across `src/PluginProcessor.cpp` and `src/processor/` (user samples: `src/processor/Samples.cpp`). The editor's header bar (preset loading and stepping, history, A/B, the preset, dice and settings menus, the preset panel) is in `src/PluginEditorHeader.cpp`.

## Two plugins, one source tree
| Target | What | Notes |
|---|---|---|
| `ilanaSynth` | Instrument (VST3, CLAP, AU on macOS, Standalone) | `juce_add_plugin` in `CMakeLists.txt` |
| `ilanaSynthFX` | Effect with audio input (`ILANA_FX=1`) | Same sources, parameters and presets. FX-only code sits behind `IlanaSynthAudioProcessor::isEffectBuild` (`src/PluginProcessor.h`) |

## Layout
| Path | Contents |
|---|---|
| `src/PluginProcessor.{h,cpp}` | `IlanaSynthAudioProcessor` (class in the header). The .cpp holds construction, `FactoryTables`, the parameter cache and `getParam`, `prepareToPlay`, `processBlock`/`processChunk` (MIDI/MPE, voices, scope), wavetable and sample slots, `getUiEpoch()` |
| `src/processor/*.cpp` | The rest of `IlanaSynthAudioProcessor`'s member functions, one area per file, moved verbatim: `ParameterLayout.cpp` (`createParameterLayout`), `Modulation.cpp` (LFO curves, mod slots, macros, vector pad and Evolve, `renderLfos`, global sources, acoustic keys), `Effects.cpp` (`processEffects`, `processSlot`, splitters, every `processXxx` effect incl. vocoder and Airwindows, FX chain save/switch), `FxFiles.cpp` (message-thread side of the FX: the IR reverb's file, Airwindows preloads), `Sequencers.cpp` (arpeggiator, Euclid exciter hits, clip sequencer `processClip`), `Presets.cpp` (preset lists, `loadFactoryPreset`, voicing, trims, default macros, `handleAsyncUpdate`), `State.cpp` (`buildFullState`, `applyFullState`, get/setStateInformation, tuning, FM algorithms), `Migrations.cpp` (every migration of old patches and factory recipes and the table that runs them, below; `PatchValues.h` is the values they read and write), `Bounce.cpp` (M8.6 resample; processor destructor), `SamplePortability.cpp` (embed, collect, find and relink sample files; step 31), `Dx7.cpp` (DX7 patch and bank import), `OscRouting.cpp` and `OscSlots.cpp` (oscillator routing and the slot pool), `ModuleClipboard.cpp` (copy and paste of a module's settings), `FxPatcher.cpp` (below). `ProcessorInternal.h` has the shared file-local helpers (`lfoValue`, `sanitiseBuffer`, ..., `FactoryTables`, `BounceThread`); the files are listed in `ILANA_PROCESSOR_SOURCES` in `CMakeLists.txt` |
| `src/PluginEditor.{h,cpp}` | `IlanaSynthAudioProcessorEditor` shell (~4,900 lines): construction, tabs, header, menus, layout, key handling, `ScopePanel`, the settings file (`playBandsFolded` etc.). The page classes are in `src/gui/pages/` |
| `src/gui/pages/*.h`, `*.cpp` | Page classes (~26k lines in all; `ls src/gui/pages` is the truth), split from PluginEditor.cpp (ARCH-8). Each page header declares its classes (global namespace) and includes what it uses (`tools/check_headers.sh`); the member functions are in the `.cpp` of the same name (one-line bodies and templates stay inline), listed in `ILANA_GUI_SOURCES` (CMakeLists.txt), so an edit in a page's `.cpp` recompiles only that file. PLAY is one tab (`PlayPage`) holding oscillators (`OscPageViewport` / `OscPage` from `OscPage.h`), filter and envelope/LFO/body bands, and the VECTOR pad (`VectorPage` in `VectorPage.h`). `PageHelpers.h` (`addAll`, `paintSectionTitle`, heading constants, `PillToggle`, `PillChoice`) and `FxWidgets.h` (`TapGrid`, `GateGrid`, `FxStackContent`, `SlotSwitch`) are header-only inline code shared by the pages. `OscPage.h` (`OscRole`, `OscPage`, `OscPageViewport`, `TuningPopover`), `EnvLfoPages.h` (`EnvSection`, `LfoSection`, which the source pop-ups host), `ModSourcePanel.h` (`SourcePopover`, `SourceEditor`, `SourceTab`, `ModulationCard`), `SeqPage.h` (SEQ tab), `PlayPage.h` (PLAY tab), `FmInputPages.h` (`FmPage`; `InputPage`, ilanaSynth FX's INPUT tab), `MatrixPage.h` (mod matrix), `FxPage.h` (FX tab), `PatchLfo.h` (LFO boxes on the PATCH tab), `PatchPage.h` (PATCH tab; takes the `FxPage`) |
| `src/gpu/` | The Vulkan editor renderer (~4k lines, Windows and Linux; CMake option `ILANA_VULKAN`): `VulkanDevice` (loader opened at run time through volk), `VulkanGraphics` (JUCE rasteriser semantics, Vulkan doing the fills and layers), `VulkanAttachment`, `VulkanSurface`, `VulkanCanvas.h`, pre-compiled shaders in `VulkanShaders.h` (`tools/build_vulkan_shaders.sh` regenerates them from `src/gpu/shaders`). Direct2D (Windows) and OpenGL (Linux) stay as the fallback; macOS keeps CoreGraphics |
| `src/Presets.h`, `PresetLibrary.h`, `PresetPackM10.h` | `FactoryPreset` / `Value`, the library `Builder`, the M10 pack |
| `src/PresetTrims.h` | Generated per-preset level and macro trims (`Trim`), written by `tools/tune_presets.py` |
| `src/TuningState.h`, `src/MtsEsp.{h,cpp}`, `src/thirdparty/mts-esp` | Scala tuning text saved in the patch; MTS-ESP client |
| `src/processor/PatchGraph.h`, `PatchGraphRunner.h`, `PatchGraphState.h`, `src/gui/PatchCableView.h` | Roadmap step A, the shared patch graph for steps 16 and 17: the node-and-cable model and its compiler (topological order, buffers pooled by liveness, in-place runs, dead branches dropped, one 64-sample chunk of delay at the least on each cable that closes a loop, up to its own delay), the audio-thread runner (plan swapped in by one atomic exchange, no allocation, per-node CPU, optional silence skip), saving in a `PatchGraph` ValueTree, `rackGraph` (today's SERIES / PARALLEL rack as a graph), and the cable view widget. `processEffectsGraph` (`Effects.cpp`) runs the rack through a fresh graph for the tests (`tests/PatchGraphTests.inc`, `ILANA_TEST_ONLY=graph`), the UI test (`tools/PatchCableUiTests.h`, `ILANA_UITEST_ONLY=PG`, pictures `ILANA_SNAPSHOT_GRAPH=1`) and `ILANA_BENCH_ONLY=graph ilanaFxCpuBench` |
| `src/processor/FxEngine.h`, `FxPatch.h`, `FxPatcher.cpp`, `src/gui/pages/PatchPage.h` | Roadmap step 17, the FX patcher and several instances of one effect. `FxEngine` is one effect instance's DSP state (`fxMain` is the shared one; `slotEngines[s]` points a slot at its own); an instance's settings are a value table swapped in for its slot (`runFxSlot`, thread-local `fxValues`). `FxPatch.h`: per slot {type, own, values} plus the patcher graph, saved as an `FxPatch` child only when not the default (presets, FX chains, chain switching, undo). `FxPatcher.cpp`: add/remove/duplicate/focus instances (one per type is in front on the `fx_<name>_*` parameters), reconcile after slot type changes, the rack compiled to a graph (`fxRackRunner`, recompiled when `fxRackSignature` changes) and the patcher graph (`fxPatchRunner`, used when `fx_patcher` is on, silence skip with `fxTailSamples`), loop gains capped at 70 % (new loops at 20 %), per-loop delay (`Cable::delayMs`). `PatchPage.h`: the PATCH tab, the voice cables and the FX patch on one cable view (zoom-to-fit, inspector, add menu, a selected loop's LEVEL and DELAY knobs, effect mini windows). `src/gui/FxInstanceControls.h`: a non-front instance's full RACK card, its ParamControls on a shadow parameter set written into the instance's values. The runner's loop guard and output guard (`PatchGraphRunner.h`) hold any loop at 0 dBFS. Tests `tests/FxPatcherTests.inc` (`ILANA_TEST_ONLY=fxpatcher`), UI `ILANA_UITEST_ONLY=FF`, pictures `ILANA_SNAPSHOT_PATCHER=1` |
| `src/ClipState.h`, `src/gui/ClipEditor.h` | Clip sequencer: up to 8 clips saved in the patch as a `Clips` child, MIDI import; played by `processClip` in `src/processor/Sequencers.cpp` (`clip_on`, `clip_index`, `clip_mode`); piano roll in the SEQ page's CLIP tab |
| `src/dsp/` | All DSP, mostly header-only; see audio path below |
| `src/dsp/airwindows/` | 39 Airwindows algorithms (MIT): `Registry.{h,cpp}`, `AirwindowsModule.h` (`Module`); generated by `tools/port_airwindows.py`; registry order is append-only |
| `src/gui/` | One header per view: `IlanaLookAndFeel.h` (`IlanaTheme`), `SectionPage.h` (tab pages), `AnimationUtils.h` (`IlanaAnim::FrameClock`, `ChangeGate`), `MatrixWidgets.h`, `OutputView.h` (PLAY's live output), `PhysicalView.h`, `VectorPad.h`, `WavetableEditor.h`, `PresetPanel.h`, ... Menus: every pop-up menu opens inside the editor through `IlanaTheme::menuOptions` (`MenuParent.h`; a plain ComboBox's list too, `IlanaLookAndFeel::getOptionsForComboBoxPopupMenu`), call-outs go into `IlanaTheme::calloutParent`, submenus are placed by `SubmenuPlacement.h`, and a select whose click opens a menu of its own derives from `OwnMenuBox.h` (`ComboControl::PopupCombo`, `SampleNameBox`). |
| `resources/fonts/` | Manrope 400/500/700 (OFL) |
| `content/` | `samples/` (VCSL CC0), `wavetables/` (AKWF CC0), `analysis/` (critic reports, embeddings); rebuilt by `tools/build_content.py` |
| `tests/` | `TableTest.cpp` + `*.inc` topic suites, `FxTest.cpp`, `fingerprints-linux.csv` (committed baseline) |
| `tools/` | Test-tool sources (`Snapshot.cpp` and the `*UiTests.h` it includes, `PresetFingerprint.cpp`, `PresetRender.cpp`, `StressTest.cpp`, `PolishRepro.cpp`, `MidiRender.cpp`, `KnobAudit.cpp`, `FxCpuBench.cpp`, `RefHost.cpp`) and Python/shell scripts |
| `cmake/` | `cmake/engine/CMakeLists.txt` (the engine static libraries, below), `BuildInfo.cmake`, `JuceLayerClip.cmake` |
| `docs/` | `HANDOFF-HISTORY.md` (per-milestone notes), `REVIEW-PLAN.md` (standing decisions, what is open), `REVIEW-PLAN-HISTORY.md` (finished steps), `RC-1.0-CHECKLIST.md`, `filter-overhaul-status.md` |
| `.github/workflows/` | `ci.yml`, `release.yml`: both manual-only |

## Audio path
`IlanaSynth` (`src/dsp/IlanaSynth.h`, a `juce::Synthesiser`, 32 voices) -> per `Voice` (`src/dsp/Voice.h`, `Voice.cpp`; settings in `VoiceParams`) -> FX chain in the processor -> output.
| Stage | Where |
|---|---|
| 6 oscillators (also FM operators); modes: wavetable, classic, physical, sample, granular, live input | `WavetableOscillator.h`, `Wavetable.{h,cpp}`, `UnisonBank.h`, `SamplePlayer.h` (with `MultiSample.h`: SF2/SFZ regions picked per note), `GranularOsc.h`, `OscillatorIds.h` |
| Warps, spectral work, table building | `SpectralWarp.h`, `SpectralCache.h`, `SpectralFreeze.h`, `TableFFT.h`, `TableFactory.{h,cpp}` |
| Physical models (exciters, strings, bodies) | `KarplusStrong.h`, `TunedString.h`, `PianoString.h`, `TensionAdsr.h`, `SympatheticStrings.h`, `MaterialBody.h`, `ResonatorBank.h`, `ElectricPiano.h`, `FeedbackGuitar.h`, `WestCoast.h`; fitted constants in `*Tuning.h` |
| FM (6x6 matrix, algorithms) | `FmAlgorithms.h`; view `src/gui/FmDiagram.h` |
| Two filters | `FilterUnit.h` (`FilterUnit`, `LadderFilter`, `DiodeFilter`, `Ms20Filter`), `FilterCore.h`, `FilterModels2.h`, `Svf.h`, `Biquad.h`; overhaul status in `docs/filter-overhaul-status.md` |
| Envelopes (16), LFOs (16), MSEG, chaos/physics shapes, Evolve, generative | `Modulation.h` (`Mod` namespace), `Mseg.h`, `LfoShape.h`, `LfoSim.h`, `LfoCurve.h`, `Evolve.h`, `Generative.h` |
| Tuning | `Tuning.h` (.scl/.kbm parsers, 128-note table) |
| FX chain (type 30 = Airwindows, 31 = Vocoder) | in `src/processor/Effects.cpp`; Airwindows via `src/dsp/airwindows/`; the vocoder is `Vocoder.h` (carrier = slot signal, modulator = audio input or built-in Talk) |

## Threads
Written 2026-11-16 from a read of the code (`grep` for the lock types in `src/`); the rows say what the code does today, not a design. Line numbers drift: search the lock's name.

**Thread kinds.** *Audio*: the host's `processBlock` (`PluginProcessor.cpp`), on a realtime thread; with MULTI-CORE on, voice workers (`src/dsp/VoiceThreads.h`) render voices for it. *Message*: the JUCE message thread, which the editor, the `Timer`s and `handleAsyncUpdate` run on. *Host state load*: `setStateInformation` can arrive on a host thread (`stateLoadLock`). *Offline*: a bounce (`Bounce.cpp`, `BounceThread`), an offline export or a test calling `processBlock` itself; it plays the audio thread's part on whatever thread that is. *Workers*: `SpectralCache` and `StretchRenderer` build tables and stretched samples off the audio thread; `ModalBowedLayers` has one builder thread; a sample the player drops or chooses is decoded on a thread of its own (`loadUserSampleInBackground`, `Samples.cpp`: it holds the processor's lifetime token, and the result is handed over on the message thread only if no newer load, patch, preset or oscillator move took the slot, `sampleRequests`).

**Rule for the audio thread:** never wait. A plain `ScopedLock` there is allowed only where the row says "short" (a pointer copy under a spin lock).

### Locks
| Lock (declared) | Guards | Takes it | Audio thread |
|---|---|---|---|
| `stateLoadLock` (`CriticalSection`, `PluginProcessor.h`) | one state load at a time (`State.cpp`: `setStateInformation`, `applyFullState`) | message, host state load | never |
| `stateLock` (`SpinLock`) | `samplePaths`, `embeddedSamples`, the preset/state hand-overs, the bounce result (`State.cpp`, `Presets.cpp`, `Samples.cpp`, `SamplePortability.cpp`, `Bounce.cpp`, `FxPatcher.cpp`, `PluginProcessor.cpp`) | message, host state load, bounce thread | never |
| `sampleLock` (`SpinLock`) | `sampleSlots`, `retiredSamples` | writes: message, state load; reads: `getSampleForOsc` | blocking, short (a `shared_ptr` raw pointer copy; retired samples keep the old one alive) |
| `tableLock` (`SpinLock`) | user wavetable slots (`swapUserTable`, `getTableForChoice`) | writes: message, state load | blocking, short |
| `previewLock` (`SpinLock`) | the sample browser's pending preview clip | message thread fills (blocking) | try-lock (`mixSamplePreview`) |
| `scopeLock` (`SpinLock`) | the scope buffers | editor reads (blocking) | try-lock, skips the block's scope write when busy |
| `lfoShapeLock` (`SpinLock`) | custom LFO shapes and curve tables | message, state load | try-lock at the top of a chunk; keeps the last block's shapes when busy |
| `remapLock` (`SpinLock`) | the mod remap tables | message, state load | blocking, only when `remapEpoch` moved (one table copy). The one audio-side wait that is not a try-lock |
| `fxEditLock` (`CriticalSection`) | the FX patch's edits (`FxPatcher.cpp`: add, remove, swap, graph, reconcile; `prepareToPlay`) | message, host state load | never |
| `fxSwapApplyLock` (`SpinLock`) | applying a staged slot swap (`applyFxSwap`) | message waits (`mayWait`), up to 250 ms | try-lock; a busy swap waits for the next block |
| `fxCompileLock` (`SpinLock`) | the rack runner's publish (`FxPatcher.cpp`) | message thread, or an offline render | never |
| `getCallbackLock()` (JUCE) | `setOversampling`'s buffer rebuild against `processBlock` | message (`setOversampling`); VST3 and standalone wrappers hold it around `processBlock` | try-lock (`processBlock` outputs one silent block when busy) |
| `TuningState::mutex` (`TuningState.h`) | the loaded Scala table and key mapping | message | try-lock (`acquireForAudio`) |
| `ClipState::mutex` (`ClipState.h`) | the clips | message | try-lock in the reader (`ClipState.h`) |
| `IlanaSynth::lock` (`src/dsp/IlanaSynth.h`, the `juce::Synthesiser` lock) | voice allocation, mode changes, pending notes | message (keyboard notes, `setVoiceMode`), audio (note on/off, render) | blocking, held by the render itself; the message-side holders are short |
| `SpectralCache` `slot.lock` (`SpinLock`), `buildLock`, `voiceBuildLock` (`src/dsp/SpectralCache.h`) | a built spectral table per slot; one builder at a time | worker, message (synchronous path when offline) | try-lock on `slot.lock` only ("never blocks") |
| `StretchRenderer` `queueLock` (`SpinLock`), `workerLock` (`src/dsp/StretchRenderer.h`) | the stretch request queue; one worker at a time | worker threads | try-lock on `queueLock` (`request` asks again next block) |
| `ModalBowedLayers` `mutex` (`std::mutex`), `ModalPiano.h` and `SamplePlayer.h` function-local `static std::mutex` | the layer bank list; per-rate noise designs and window tables, built once | message (prepare), the builder thread | never by design (all three are reached from `prepare`) |

Lock order: the one nesting found is `stateLock` inside `fxEditLock` (`FxPatcher.cpp`); `stateLock` alone is also taken without it. Keep that direction: never take `fxEditLock` while holding `stateLock`. Not checked by anything yet.

### Atomics (who writes, who reads)
There are about 120 `std::atomic` members of the processor; they fall into groups, and a new one should join a group.
| Group (members in `PluginProcessor.h`) | Writes | Reads |
|---|---|---|
| Parameter values (`ParamRef`, `getRawParam`, the `std::atomic<float>` behind each parameter) | host automation (any thread), editor (message) | audio, every block |
| Display and meter feeds: `scopeWritePos`, `scopeBlockSamples`, `scopeBlockMs`, `outputPeaks`, `outputLevelDisplay`, `inputLevelDisplay`, `cpuUsage`, `activeVoiceCount`, `lfoLastValues`, `lfoPhaseDisplays`, `lfoSampleHolds`, `modDisplayValues`, `envMonitor*`, `monitor*`, `displayPhase`, `oscDisplayPhases`, `fxSlotCpu`, `fxSlotIn`, `fxSlotOut`, `fxLfoShownValue`, `westGateDisplay`, `hostPpq`, `hostPlaying`, `currentBpm`, `engineDisplayStep`, `euclidDisplayStep`, `clipPlayhead` | audio | editor timers (message). Lossy by design |
| Version counters the editor polls: `tableNoticeVersion`, `stateLoadNoticeVersion`, `missingSamplesVersion`, `revealVersion`, `sectionColourVersion`, `remapEpoch`, `paramEpoch`, `liveEpoch`, `dataEpoch`, `sampleEpoch`, `patchGeneration`, `noteSerial`, `uiNoteEpoch` | message, state load (`paramEpoch` also any thread via its parameter listener) | editor (message); `remapEpoch` also audio |
| Pending input from the editor: `uiModWheel`, `uiPitchBend`, `uiPressure`, `previewFifo` (lock-free FIFO), `pendingProgramChange`, `pendingMacros`, `macroPendingFlags`, `macrosPending`, `liveRetrigger`, `patchCut`, `easeStep`, `exciterLevels`, `oversamplingFactor` | message | audio, next block |
| MIDI-learn and CC: `macroLearn`, `macroCc`, `paramLearn`, `ccParameter`, `pendingCcValues`, `pendingCcParameter`, `ccValuePending`, `ccPending`, `ccOverrides` | audio (a CC arrives) and message (learn setup) | message (applies the queued values), audio (`ccOverrides` in `getParam`) |
| Sample preview: `previewSerial`, `previewPlaying`, `previewGain` | message | audio |
| Bounce: `bounceState`, `bounceProgress` | bounce thread, message | message |
| Thread identity: `asyncOwnerThread` (`keepAsyncUpdatesOnThisThread`, `Presets.cpp`), `fxRackThread`, `fxLastRackMs` | the thread that renders (`beginFxBlock`), message | message, to decide whether a swap can wait for the audio thread |
| FX rack hand-over: `slotEngines`, `slotValues`, `fxLfoProgram`, `fxSwapRequested`, `fxSwapApplied`, `fxRackCompileWanted`, `fxReconcileWanted`, `raw` pointers in `FxSwapStage` | message (publish, release) | audio (acquire), applies between two blocks |
| Undo and echo guard: `ParamUndoRecorder::lastValues`, `ParameterEchoGuard` | every thread | message |

### Checks in code
`src/ThreadChecks.h`: `ILANA_AUDIO_THREAD_SCOPE()` (first line of `processBlock`), `ILANA_ASSERT_NOT_AUDIO()` and `ILANA_ASSERT_MESSAGE_THREAD()`. They exist in JUCE debug builds and in the test builds (`ILANA_TEST_BUILD`: `ilanaTableTest`, `ilanaFingerprint` and the tools on the engine libraries) and are empty macros everywhere else, the plugins included. A violation prints `THREAD CHECK FAILED`, counts, and breaks in a debug build; `ilanaTableTest` fails if the count is not 0 at the end. In place: `setFxPatchGraph`, `addFxEffect` (`FxPatcher.cpp`) and `setModSlotValue` (`Modulation.cpp`) must not run inside `processBlock`; `beginEdit` and `endEdit` (`State.cpp`) are message-thread only. Not yet covered: `loadFactoryPreset` and `assignModSlot` (`Presets.cpp`), the other editor-called setters in `PluginProcessor.cpp`, and a wrapper that asserts "no blocking lock on the audio thread". Add a check next to a new setter the same way.

## Parameters and modulation
- Parameters are created in `createParameterLayout()` (`src/processor/ParameterLayout.cpp`); per-block reads use a `ParamRef` member (resolved once).
- `namespace Mod` in `src/dsp/Modulation.h`: `Source` and `Destination` enums, `Slot`, `paramDestinationFor(i)`. 64 matrix slots, 8 macros, a vector pad.
- Destinations: 115 legacy parameter destinations at 96..210, OSC 4-6 from 211, newer parameters appended after `Destination::Count` in list order. `maxDestinations` = 1024 (`PluginProcessor.h`; 989 used after review 7).
- **Never renumber** parameter IDs, choice indices, destinations or sources. Append only (LFO 5-16, ENV 6-16 and slots 33-64 were all appended).
- Choice parameters are non-automatable (`withAutomatable (false)` in the layout); switches and macros stay automatable.
- New features default to old behaviour, skip their code while unused, and use their own random generators, so old presets stay bit-identical.
- Per-patch extras (tuning text, wavetables, bounced samples, reveal masks) are child nodes of the saved patch state.

## Presets
- Factory presets: `Presets.h` + `PresetLibrary.h` (`Builder`) + `PresetPackM10.h`. New presets are appended at the end.
- Levels and macro trims: `src/PresetTrims.h` is generated; change `LEVEL_OVERRIDES` in `tools/tune_presets.py` and rerun, don't edit the header.
- Old presets must not change: fingerprint before and after (below).

## Migrations
Old saved patches and factory recipes reach today's parameters through one table, `Migrations::table()` in `src/processor/Migrations.cpp` (declared in `Migrations.h`). Each row is `{ schemaKey, fromVersion, toVersion, function }` plus where it runs: on a saved state (`applyFullState`: `beforeDefaults`, on the state as saved, or `afterDefaults`, once every missing parameter has its default) and on a factory recipe (`loadFactoryPreset`: `legacyRecipes`, the original 80 before the voicing, or `afterTrims`), and what `set` does there with an id the patch lacks (add or skip) or, on a recipe, holds twice. A saved state runs a row when its stamp (1 if absent) is below `toVersion` and is stamped `toVersion`; a recipe starts at version 1 of every key. Rows run in table order; the saved and factory columns differ where the two loads always did, on purpose (tables, destinations, Lorenz and mod ranges are saved-state only; recipes get mod ranges on the live parameters after the default macros, in `loadFactoryPreset`). The functions see the patch through `PatchValues` (`TreePatchValues` for a saved state, `FactoryPatchValues` for a recipe).

**To add a migration: one row** at the end of the table (a new key, or the next version of an existing one), with its function next to the others in `Migrations.cpp`, and raise that key's version in `savedSchemas` (`State.cpp`) so new saves are stamped. Never reorder or change a row. Check with `ILANA_TEST_ONLY=oldstate ilanaTableTest` (the saved states in `tests/old-states/`, one per schema generation, and every factory preset's parameters) and the fingerprints; add a corpus state for the new generation (`ILANA_OLDSTATE_MAKE=1`, then `ILANA_OLDSTATE_WRITE=1` once the result is the intended one).

## Tests and probes
| Target | Notes |
|---|---|
| `ilanaTableTest` | `tests/TableTest.cpp` + `tests/*.inc`. Groups by one switch: `ILANA_TEST_ONLY=<key>[,<key>]` runs just those groups (unknown key prints the list). Keys: remap, generative, plan, oscmode, opeg, tuning, airwindows, noise, graph, oscrouting, oscslot, fxlfo, fxpatcher, parallelfx, splitter, awfilter, builtin, clip, sweep, round3, hardening, host, paraminfo, stress, manifest, cpu3, cpupass, filterpair, physmod, tensionenv, sampletempo, sampler, granular, multisample, vocoder, filter, voiceknob, oldstate, polish, presetpack, resample, feedback, filtermodels, west, filter2, piano, modulators, epiano, body, extfm, library, sympathetic, preset, arp, fm, ks, keys, engine. `oldstate` covers the old saved states in `tests/old-states/`. Probes: `ILANA_PRESET_OVERRIDES`, `ILANA_NOTE_DEBUG`, `ILANA_RENDER_DEMO`, `ILANA_PRESET_PROFILE`, `ILANA_BENCH`, `ILANA_PARAM_TEXT` |
| `ilanaFxTest` | `tests/FxTest.cpp`: the FX plugin (built with the real editor: one check opens it) |
| `ilanaStress` | `tools/StressTest.cpp`: the stress phrase (pedal, retrigger, stealing, preset change, random block lengths, sample-rate change) through every preset, multi-core on vs off |
| `ilanaTorture` | `tools/TortureTest.cpp`: the audio thread renders random blocks and notes while the message thread loads presets, swaps user tables and samples, undoes, redoes and swaps FX slots; fails on NaN/inf or a crash. For ThreadSanitizer: `cmake -B build-tsan -DILANA_SANITIZE=thread && cmake --build build-tsan --target ilanaTorture` (`address` likewise; the option touches only the test engine, `ilanaTableTest` and this tool) |
| `ilanaKnobAudit` | `tools/KnobAudit.cpp`: knob sweeps (`tools/knob_audit.py`) |
| `ilanaMidiRender` | `tools/MidiRender.cpp`: renders a MIDI file through a patch |
| `ilanaPolishRepro` | `tools/PolishRepro.cpp`: repro for polish-pass bugs |
| `ilanaFxCpuBench` | `tools/FxCpuBench.cpp`: what the FX rack costs per effect and routing |
| `ilanaSnapshot` | `tools/Snapshot.cpp`; `--uitest` (65 named groups in `uiGroups[]`; `--uitest --list` names them, `--uitest --only A,B` runs just those on a fresh editor each, `--chain` on one shared editor; `ILANA_UITEST_ONLY` still works), `--fps`, `--idlecpu`; needs a display (Xvfb on Linux) |
| `ilanaFingerprint` | `tools/PresetFingerprint.cpp`; `ilanaFingerprint out.csv`, then `python tools/compare_fingerprints.py before.csv after.csv`. Linux baseline: `tests/fingerprints-linux.csv` (compare only within a platform) |
| `ilanaPresetRender` | `tools/PresetRender.cpp`: renders test phrases per preset for `tools/preset_critic.py` |
| `ilanaRefHost` | `tools/RefHost.cpp`: headless host rendering installed plugins (Vital, Surge XT, ...) as references (`--presets`) |

Build graph: the engine sources are compiled once per define flavour into the static libraries `ilanaEngineTest` (`ILANA_TEST_BUILD`), `ilanaEngineFp` (+ `ILANA_FINGERPRINT_BUILD`) and `ilanaEngineFxBench` (+ `ILANA_FX`) (`cmake/engine/CMakeLists.txt`); `ilanaTableTest`, `ilanaFingerprint` and the dev tools link them. The two plugins, `ilanaSnapshot` and `ilanaFxTest` need the editor and compile the engine themselves (`ILANA_ENGINE_SOURCES`). `ilanaStress`, `ilanaPolishRepro`, `ilanaTorture`, `ilanaKnobAudit`, `ilanaMidiRender`, `ilanaPresetRender`, `ilanaRefHost` and `ilanaFxCpuBench` are `EXCLUDE_FROM_ALL`: build them with `--target ilanaTools` (or `tools/verify.sh --all`).

Gate: `tools/verify.sh [--quick] [--all]` builds the targets it runs (`--all`: also the dev tools), checks the doc paths, runs tests, `--uitest`, fingerprints against the Linux baseline, and pluginval.

## Tools (Python/shell)
| Script | Does |
|---|---|
| `tools/verify.sh` | The full gate |
| `tools/verify.ps1`, `tools/setup-runner.ps1` | The gate on Windows; registers the PC as the self-hosted runner (`docs/SELF-HOSTED-RUNNER.md`) |
| `tools/compare_fingerprints.py` | Diff two fingerprint CSVs |
| `tools/check_doc_paths.py` | Fails when `ARCHITECTURE.md` or `HANDOFF.md` names a repo path that does not exist (run by `verify.sh`) |
| `tools/check_headers.sh` | Compiles each `src/gui/*.h` and `src/gui/pages/*.h` on its own (`-fsyntax-only`, the editor's flags): every header includes what it uses (run by `verify.sh`; a header that is a piece of another, as `KnobRoutingModel.h`, says `header-check: fragment`) |
| `tools/preset_critic.py`, `clap_score.py`, `preset_diversity.py` | Score renders, CLAP embeddings, library diversity |
| `tools/tune_presets.py` | Generates `src/PresetTrims.h` |
| `tools/ab_clips.py` | Renders A/B listening clips |
| `tools/build_content.py` | Rebuilds `content/samples` and `content/wavetables` |
| `tools/port_airwindows.py` | Generates `src/dsp/airwindows/` |
| `tools/analyse_note.py`, `fit_model.py`, `fit_piano.py`, `fit_piano2.py`, `bake_piano2.py`, `fit_ep.py`, `fit_feedback.py`, `fit_lpg.py` | Fit models to reference recordings; bake results into `*Tuning.h` |
