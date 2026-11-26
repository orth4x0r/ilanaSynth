#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include <complex>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "Presets.h"
#include "PluginProcessor.h"
#include "BuildInfo.h"
#include "ThreadChecks.h"
#include "gui/ParamInfo.h"
#include "dsp/GranularPitchShift.h"
#include "dsp/FilterUnit.h"
#include "dsp/FmAlgorithms.h"
#include "dsp/Formula.h"
#include "SafeXml.h"
#include "processor/PatchExtras.h"
#include "ClipState.h"
#include "dsp/Tuning.h"
#include "dsp/MultiSample.h"
#include "dsp/GranularSmear.h"
#include "dsp/LfoCurve.h"
#include "dsp/LfoSim.h"
#include "dsp/LfoShape.h"
#include "dsp/Modulation.h"
#include "dsp/PitchDetect.h"
#include "dsp/SampleRange.h"
#include "dsp/SpectralFreeze.h"
#include "dsp/Svf.h"
#include "dsp/TableFactory.h"
#include "dsp/UnisonBank.h"
#include "dsp/VectorShape.h"
#include "dsp/Voice.h"
#include "dsp/Wavetable.h"
#include "dsp/TableFFT.h"
#include "dsp/WavetableOscillator.h"
#include "SamplingProfiler.h"

#include "CoreOscTests.inc"
#include "StringModelTests.inc"
#include "PresetFxIntegrityTests.inc"
#include "VoiceModTests.inc"
#include "FrameBlendTests.inc"
#include "FilterArpLibraryTests.inc"
#include "LfoPhysicsGranularTests.inc"
#include "M4Tests.inc"
#include "FmReferenceTests.inc"
#include "GenerativeEpWavetableBodyTests.inc"
#include "UnisonBenchTests.inc"

// ILANA_TEST_TIMES=1 prints how long each suite of the full run takes, to
// find the slow ones.
template <typename Suite>
void timedRun (const char* name, Suite&& suite)
{
    static const auto report = juce::SystemStats::getEnvironmentVariable ("ILANA_TEST_TIMES", "").isNotEmpty();
    // ILANA_TEST_SUITE="runStereoEverywhereTest": run that suite alone (where the main list reaches it).
    static const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_TEST_SUITE", "");
    if (only.isNotEmpty() && only != name)
        return;
    const auto start = juce::Time::getMillisecondCounterHiRes();
    suite();

    if (report)
        std::cout << "TIME " << name << " " << juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 2) << " s" << std::endl;
}

#include "M81Tests.inc"
#include "M82Tests.inc"
#include "M83Tests.inc"
#include "M84Tests.inc"
#include "M85Tests.inc"
#include "M86Tests.inc"
#include "M10Tests.inc"
#include "PolishTests.inc"
#include "PlanTests.inc"
#include "TuningTests.inc"
#include "OperatorEgTests.inc"
#include "OscModeTests.inc"
#include "AirwindowsTests.inc"
#include "VocoderTests.inc"
#include "FilterTests.inc"
#include "MultiSampleTests.inc"
#include "SampleTempoTests.inc"
#include "SamplerTests.inc"
#include "SplitterTests.inc"
#include "ParallelFxTests.inc"
#include "PatchGraphTests.inc"
#include "FxPatcherTests.inc"
#include "FxLfoTests.inc"
#include "TensionEnvTests.inc"
#include "PhysModTests.inc"
#include "VoiceKnobTests.inc"
#include "FilterPairTests.inc"
#include "CpuPassTests.inc"
#include "CpuRound3Tests.inc"
#include "StressTests.inc"
#include "OscRoutingTests.inc"
#include "OscSlotTests.inc"
#include "NoiseTests.inc"
#include "AirwindowsFilterTests.inc"
#include "ClipTests.inc"
#include "GranularTests.inc"
#include "BuiltInTableTests.inc"
#include "ParamInfoTests.inc"
#include "HostIntegrationTests.inc"
#include "HostHardeningTests.inc"
#include "HardeningTests.inc"
#include "PatchExtrasTests.inc"
#include "SweepTests.inc"
#include "Round3Tests.inc"
#include "ParamManifestTests.inc"
#include "DemoRender.inc"
#include "OldStateTests.inc"

#include "BenchTools.inc"
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::SystemStats::setApplicationCrashHandler ([] (void*)
    {
        std::cout << "CRASH" << std::endl;
        std::cout << juce::SystemStats::getStackBacktrace() << std::endl;
    });

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_OPEN_FILTER_CHECK", "").isNotEmpty())
    {
        runOpenFilterCheck();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_NULL_CHECK", "").isNotEmpty())
    {
        runNullCheck();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_BENCH", "").isNotEmpty())
    {
        runBodyBench();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_MODAL_BENCH", "").isNotEmpty())
    {
        runModalBench();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_LIBRARY_BENCH", "").isNotEmpty())
    {
        runLibraryBench();
        return 0;
    }

    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_PROFILE", ""); preset.isNotEmpty())
    {
        runPresetProfile (preset);
        return 0;
    }

    if (const auto profile = juce::SystemStats::getEnvironmentVariable ("ILANA_PROFILE", ""); profile.isNotEmpty())
    {
        runProfile (juce::jmax (1, profile.getIntValue()));
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_UNISON_BENCH", "").isNotEmpty())
    {
        runUnisonBankEquivalenceTest();
        runUnisonBenchmark();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_STEREO_BENCH", "").isNotEmpty())
    {
        runStringPairTest();
        runStereoStringBench();
        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    // ILANA_BENCH=1 runs only the CPU benchmark (for profiling).
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_BENCH", "").isNotEmpty())
    {
        runPhase2StateAndCpuTest();
        return 0;
    }

    // ILANA_TEST_ONLY=<key>[,<key>...] runs just those groups of tests, in the order
    // given, instead of the full run (e.g. ILANA_TEST_ONLY=sweep,oldstate).
    if (const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_TEST_ONLY", ""); only.isNotEmpty())
    {
        struct Group { const char* key; void (*run)(); };
        static const Group groups[] = {
            { "remap", [] { runModRemapTests(); } },
            { "generative", [] { runM71GenerativeTests(); runScaleRandomReleaseTest(); runArpHostStopTests(); } },
            { "plan", [] { runPlanTests(); } },
            { "oscmode", [] { runOscModeTests(); } },
            { "opeg", [] { runOperatorEgTests(); } },
            { "tuning", [] { runTuningTests(); } },
            { "airwindows", [] { runAirwindowsTests(); } },
            { "noise", [] { runNoiseTests(); } },
            { "graph", [] { runPatchGraphTests(); } },
            { "oscrouting", [] { runOscRoutingTests (true); } },
            { "oscslot", [] { runOscSlotTests(); } },
            { "fxlfo", [] { runFxLfoTests(); } },
            { "fxpatcher", [] { runFxPatcherTests(); } },
            { "parallelfx", [] { runParallelFxTests(); } },
            { "splitter", [] { runSplitterTests(); } },
            { "awfilter", [] { runAirwindowsFilterTests(); } },
            { "builtin", [] { runBuiltInTableTests(); } },
            { "clip", [] { runClipTests(); } },
            { "sweep", [] { runSweepFixTests(); } },
            { "round3", [] { runRound3FixTests(); } },
            { "hardening", [] { runHardeningTests(); } },
            { "host", [] { runHostIntegrationTests(); runHostHardeningTests(); } },
            { "paraminfo", [] { runParamInfoTests(); } },
            { "extras", [] { runPatchExtrasTests(); } },
            { "manifest", [] { runParamManifestTests(); } },
            { "stress", [] { runStressTests(); } },
            { "cpu3", [] { runCpuRound3Tests(); } },
            { "cpupass", [] { runCpuPassTests(); } },
            { "filterpair", [] { runFilterPairTests(); } },
            { "physmod", [] { runPhysModTests(); } },
            { "tensionenv", [] { runTensionEnvTests(); } },
            { "sampletempo", [] { runSampleTempoTests(); } },
            { "sampler", [] { runSamplerTests(); } },
            { "granular", [] { runGranularControlTests(); } },
            { "multisample", [] { runMultiSampleTests(); } },
            { "vocoder", [] { runVocoderTests(); } },
            { "filter", [] { runFilterOverhaulTests(); } },
            { "voiceknob", [] { runVoiceKnobTests(); } },
            { "oldstate", [] { runOldStateTests(); } },
            { "polish", [] { runPolishTests(); } },
            { "presetpack", [] { runM10Tests(); } },
            { "resample", [] { runM86Tests(); } },
            { "feedback", [] { runM85Tests(); } },
            { "filtermodels", [] { runM84FilterTests(); } },
            { "west", [] { runM83WestTests(); } },
            { "filter2", [] { runFilter2CrossingTest(); runSpectralSweepTest(); } },
            { "piano", [] { runM82PianoTests(); } },
            { "modulators", [] { runM81ModulatorTests(); } },
            { "epiano", [] { runM73ElectricPianoTests(); runM74WavetableEditorTests(); runSplitRenderTest(); } },
            { "body", [] { runM72BodyTests(); } },
            { "extfm", [] { runM70ExtendedFmModTests(); } },
            { "library", [] { runFactoryLibraryTest(); } },
            { "sympathetic", [] { runSympatheticResonanceTest(); } },
            { "preset", [] { runPresetSanityTest(); runPresetTuningTest(); runOldStateTests(); } },
            { "arp", [] { runScaleRandomReleaseTest(); runArpHostStopTests(); runArpLaneTests(); } },
            { "fm", [] { runM5DeepFmTests(); runM6PhaseDistortionTests(); runM6bMatrixTests(); runWarpTests(); runFmMatrixTests(); } },
            { "ks", [] { runKarplusStrongTest(); runPhysicalStringTest(); runLoopDecayAcrossRatesTest(); } },
            { "keys", [] { runM4Tests(); runSympatheticTuningTest(); runSympatheticResonanceTest(); } },
            { "engine", [] { runM3bEngineTests(); } },
        };

        juce::StringArray wanted;
        wanted.addTokens (only, ",", "");
        wanted.trim();
        wanted.removeEmptyStrings();

        for (const auto& name : wanted)
        {
            const auto known = std::any_of (std::begin (groups), std::end (groups), [&] (const Group& group) { return name == group.key; });
            if (! known)
            {
                std::cout << "ILANA_TEST_ONLY: unknown group '" << name << "'. Groups:";
                for (const auto& group : groups)
                    std::cout << " " << group.key;
                std::cout << std::endl;
                return 2;
            }
        }

        for (const auto& name : wanted)
            for (const auto& group : groups)
                if (name == group.key)
                    group.run();

        std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << " (" << failures << " failures)" << std::endl;
        return failures == 0 ? 0 : 1;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_FEEDBACK_PROBE", "").isNotEmpty())
    {
        FeedbackGuitarTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_FEEDBACK_TUNING", ""));
        M85::probe();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO_PROBE", "").isNotEmpty())
    {
        PianoModelTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_PIANO2_TUNING", ""));
        M82::probe();
        return 0;
    }

    if (juce::SystemStats::getEnvironmentVariable ("ILANA_PARAM_TEXT", "").isNotEmpty())
    {
        // Every parameter's text at its minimum, default and maximum (to
        // review units and formats).
        IlanaSynthAudioProcessor processor;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                std::cout << "HELP " << withId->paramID << " | " << describeParameter (withId->paramID) << std::endl
                          << "PARAM " << withId->paramID << " | " << parameter->getText (0.0f, 64) << " | "
                          << parameter->getText (parameter->getDefaultValue(), 64) << " | " << parameter->getText (1.0f, 64) << std::endl;
        return 0;
    }

    if (const auto target = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_SEQ", ""); target.isNotEmpty())
    {
        Polish::probeSequence (target);
        return 0;
    }

    if (const auto list = juce::SystemStats::getEnvironmentVariable ("ILANA_LEVEL_PROBE", ""); list.isNotEmpty())
    {
        Polish::probeLevels (list);
        return 0;
    }

    if (const auto spec = juce::SystemStats::getEnvironmentVariable ("ILANA_PHYSMOD_RENDER", ""); spec.isNotEmpty())
        return renderPhysModPreset (spec);

    if (const auto folder = juce::SystemStats::getEnvironmentVariable ("ILANA_BODY_FIT", ""); folder.isNotEmpty())
    {
        renderBodyFit (juce::File (folder));
        return 0;
    }

    // ILANA_TABLE_DUMP=<file>: the first 40 factory tables' built frames
    // (level 0 and level 6), for comparing two builds.
    if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_DUMP", ""); dump.isNotEmpty())
    {
        juce::FileOutputStream out { juce::File (dump) };
        out.setPosition (0);
        out.truncate();
        for (int i = 0; i < 40; ++i)
        {
            Wavetable table;
            table.buildFromFrames (TableFactory::generate (i));
            for (int level : { 0, 6 })
                for (int frame = 0; frame < table.getNumFrames(); ++frame)
                    out.write (table.getFrameData (level, frame), 2051 * sizeof (float));
        }
        return 0;
    }

    // ILANA_TABLE_FRAMES=<file>: frames 0, 32 and 63 of every factory table.
    if (const auto dump = juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_FRAMES", ""); dump.isNotEmpty())
    {
        juce::FileOutputStream out { juce::File (dump) };
        out.setPosition (0);
        out.truncate();
        for (int i = 0; i < TableFactory::getNumFactoryTables(); ++i)
        {
            const auto frames = TableFactory::generate (i);
            for (int frame : { 0, 32, 63 })
                out.write (frames[(size_t) frame].data(), 2048 * sizeof (float));
        }
        return 0;
    }

    // ILANA_LPG_PROBE=<folder>: single strikes of the low-pass gate (WEST on,
    // a plain saw, fold off) at A2 and A4, with ILANA_WEST_TUNING applied,
    // as <folder>/strike.<note>.wav (tools/fit_lpg.py measures them).
    if (const auto probe = juce::SystemStats::getEnvironmentVariable ("ILANA_LPG_PROBE", ""); probe.isNotEmpty())
    {
        WestCoastTuning::get().apply (juce::SystemStats::getEnvironmentVariable ("ILANA_WEST_TUNING", ""));
        juce::File (probe).createDirectory();
        for (int note : { 45, 69 })
        {
            IlanaSynthAudioProcessor processor;
            for (const auto& [id, value] : std::vector<std::pair<const char*, float>> {
                     { "west_on", 1.0f }, { "west_fold", 0.0f }, { "osc1_table", 0.0f }, { "osc1_frame", 0.0f },
                     { "osc2_on", 0.0f }, { "sub_on", 0.0f }, { "subosc_on", 0.0f }, { "f1_cutoff", 20000.0f },
                     { "f1_env", 0.0f }, { "osc1_unison", 1.0f }, { "amp_sustain", 1.0f }, { "amp_release", 0.01f } })
                setParam (processor, id, value);
            processor.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> out (1, 48000), buffer (2, 256);
            for (int start = 0; start < 48000; start += 256)
            {
                juce::MidiBuffer midi;
                if (start == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                if (start == 256 * 180)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
                buffer.clear();
                processor.processBlock (buffer, midi);
                out.copyFrom (0, start, buffer, 0, 0, juce::jmin (256, 48000 - start));
            }
            const auto file = juce::File (probe).getChildFile ("strike." + juce::String (note) + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 48000.0, 1, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                }
        }
        return 0;
    }

    // ILANA_TABLE_TIMING=1: how long each factory table takes to build.
    if (juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_TIMING", "").isNotEmpty())
    {
        auto total = 0.0;
        for (int i = 0; i < TableFactory::getNumFactoryTables(); ++i)
        {
            const auto start = juce::Time::getMillisecondCounterHiRes();
            Wavetable table;
            table.buildFromFrames (TableFactory::generate (i));
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            total += ms;
            if (ms > 40.0)
                std::cout << i << " " << TableFactory::getFactoryTableNames()[i] << ": " << ms << " ms" << std::endl;
        }
        std::cout << "all tables: " << total << " ms" << std::endl;
        return 0;
    }

    if (const auto preset = juce::SystemStats::getEnvironmentVariable ("ILANA_NOTE_DEBUG", ""); preset.isNotEmpty())
    {
        debugPresetNotes (preset);
        return 0;
    }

    if (const auto demo = juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_DEMO", ""); demo.isNotEmpty())
    {
        Demo::renderTables (juce::File (demo), juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_FROM", "120").getIntValue(),
                            juce::SystemStats::getEnvironmentVariable ("ILANA_TABLE_TO", "9999").getIntValue());
        return 0;
    }

    if (const auto demo = juce::SystemStats::getEnvironmentVariable ("ILANA_RENDER_DEMO", ""); demo.isNotEmpty())
    {
        // ILANA_DEMO_ONLY=m81 (or m82, ...) renders one milestone's demos.
        const auto only = juce::SystemStats::getEnvironmentVariable ("ILANA_DEMO_ONLY", "");
        if (only.isEmpty())
        {
            renderKeysDemos (juce::File (demo));
            renderFmPdDemos (juce::File (demo).getChildFile ("fm-pd"));
        }
        if (only.isEmpty() || only == "m81")
            Demo::renderM81 (juce::File (demo).getChildFile ("m81"));
        if (only.isEmpty() || only == "m82")
            Demo::renderM82 (juce::File (demo).getChildFile ("m82"));
        if (only.isEmpty() || only == "m83")
            Demo::renderM83 (juce::File (demo).getChildFile ("m83"));
        if (only.isEmpty() || only == "m85")
            Demo::renderM85 (juce::File (demo).getChildFile ("m85"));
        if (only.isEmpty() || only == "m86")
            Demo::renderM86 (juce::File (demo).getChildFile ("m86"));
        if (only.isEmpty() || only == "m10")
            Demo::renderM10 (juce::File (demo).getChildFile ("m10"));
        return 0;
    }

    std::cout << "ilanaSynth table tests" << std::endl;

    timedRun ("runPolarityTests", [] { runPolarityTests(); });
    timedRun ("runMipmapTests", [] { runMipmapTests(); });
    timedRun ("runAliasTests", [] { runAliasTests(); });
    timedRun ("runLoaderTest", [] { runLoaderTest(); });
    timedRun ("runFilterTests", [] { runFilterTests(); });
    timedRun ("runPitchShiftInterpolationTest", [] { runPitchShiftInterpolationTest(); });
    timedRun ("runVoiceSmokeTest", [] { runVoiceSmokeTest(); });
    timedRun ("runFrameModulationTest", [] { runFrameModulationTest(); });
    timedRun ("runCrossModulationTest", [] { runCrossModulationTest(); });
    timedRun ("runKarplusStrongTest", [] { runKarplusStrongTest(); });
    timedRun ("runFactoryTableContentTest", [] { runFactoryTableContentTest(); });
    timedRun ("runWeirdDspTest", [] { runWeirdDspTest(); });
    timedRun ("runSampleOscTest", [] { runSampleOscTest(); });
    timedRun ("runSampleRangeTest", [] { runSampleRangeTest(); });
    timedRun ("runSampleLoopPointsTest", [] { runSampleLoopPointsTest(); });
    timedRun ("runSampleRootTest", [] { runSampleRootTest(); });
    timedRun ("runSampleLoopCrossfadeTest", [] { runSampleLoopCrossfadeTest(); });
    timedRun ("runPresetSanityTest", [] { runPresetSanityTest(); });
    timedRun ("runOldStateTests", [] { runOldStateTests(); });
    timedRun ("runOsc2Test", [] { runOsc2Test(); });
    timedRun ("runOscLevelTest", [] { runOscLevelTest(); });
    timedRun ("runPresetTuningTest", [] { runPresetTuningTest(); });
    timedRun ("runParameterStressTest", [] { runParameterStressTest(); });
    timedRun ("runFxModuleIsolationTest", [] { runFxModuleIsolationTest(); });
    timedRun ("runFxIntegrityTest", [] { runFxIntegrityTest(); });
    timedRun ("runStateRoundTripTest", [] { runStateRoundTripTest(); });
    timedRun ("runSoakTest", [] { runSoakTest(); });
    timedRun ("runLegacyPresetFxTest", [] { runLegacyPresetFxTest(); });
    timedRun ("runLfo34Test", [] { runLfo34Test(); });
    timedRun ("runOversamplingTest", [] { runOversamplingTest(); });
    timedRun ("runFxSlotAssignTest", [] { runFxSlotAssignTest(); });
    timedRun ("runPresetNameTest", [] { runPresetNameTest(); });
    timedRun ("runFilterModelTests", [] { runFilterModelTests(); });
    timedRun ("runVoiceModeTests", [] { runVoiceModeTests(); });
    timedRun ("runWarpTests", [] { runWarpTests(); });
    timedRun ("runUnisonTests", [] { runUnisonTests(); });
    timedRun ("runPerVoiceSpectralTest", [] { runPerVoiceSpectralTest(); });
    timedRun ("runSpectralSweepTest", [] { runSpectralSweepTest(); });
    timedRun ("runFrameBlendTests", [] { runFrameBlendTests(); });
    timedRun ("runStereoModulationTest", [] { runStereoModulationTest(); });
    timedRun ("runFilter2CrossingTest", [] { runFilter2CrossingTest(); });
    timedRun ("runStereoEverywhereTest", [] { runStereoEverywhereTest(); });
    timedRun ("runStereoStringsTest", [] { runStereoStringsTest(); });
    timedRun ("runStringPairTest", [] { runStringPairTest(); });
    timedRun ("runCpuSleepTests", [] { runCpuSleepTests(); });
    timedRun ("runHeldSleepFilterWakeTest", [] { runHeldSleepFilterWakeTest(); });
    timedRun ("runHeldSleepGainTest", [] { runHeldSleepGainTest(); });
    timedRun ("runPerVoiceLfoSpectralTest", [] { runPerVoiceLfoSpectralTest(); });
    timedRun ("runAudioImportModesTest", [] { runAudioImportModesTest(); });
    timedRun ("runPerVoiceLfoTest", [] { runPerVoiceLfoTest(); });
    timedRun ("runMatrixTests", [] { runMatrixTests(); });
    timedRun ("runStaleModulationTest", [] { runStaleModulationTest(); });
    timedRun ("runPhase2StateAndCpuTest", [] { runPhase2StateAndCpuTest(); });
    timedRun ("runUnisonBankEquivalenceTest", [] { runUnisonBankEquivalenceTest(); });
    timedRun ("runExtraFilterTests", [] { runExtraFilterTests(); });
    timedRun ("runFilterRoutingTest", [] { runFilterRoutingTest(); });
    timedRun ("runOversampledTuningTest", [] { runOversampledTuningTest(); });
    timedRun ("runResynthesisTest", [] { runResynthesisTest(); });
    timedRun ("runCurveLfoTest", [] { runCurveLfoTest(); });
    timedRun ("runFactoryLibraryTest", [] { runFactoryLibraryTest(); });
    timedRun ("runTapeStopLatencyTest", [] { runTapeStopLatencyTest(); });
    timedRun ("runTranceGateTest", [] { runTranceGateTest(); });
    timedRun ("runTranceGateLengthTest", [] { runTranceGateLengthTest(); });
    timedRun ("runGenerativeTests", [] { runGenerativeTests(); });
    timedRun ("runOsc3MigrationTest", [] { runOsc3MigrationTest(); });
    timedRun ("runFmMatrixTests", [] { runFmMatrixTests(); });
    timedRun ("runM3bEngineTests", [] { runM3bEngineTests(); });
    timedRun ("runSpectralWarpTests", [] { runSpectralWarpTests(); });
    timedRun ("runChaosLfoTests", [] { runChaosLfoTests(); });
    timedRun ("runPhysicsLfoTests", [] { runPhysicsLfoTests(); });
    timedRun ("runPhysicsLfoMotionTest", [] { runPhysicsLfoMotionTest(); });
    timedRun ("runSympatheticTuningTest", [] { runSympatheticTuningTest(); });
    timedRun ("runSympatheticResonanceTest", [] { runSympatheticResonanceTest(); });
    timedRun ("runBridgeBuzzStabilityTest", [] { runBridgeBuzzStabilityTest(); });
    timedRun ("runIntegerValueTextTest", [] { runIntegerValueTextTest(); });
    timedRun ("runMissingParameterDefaultTest", [] { runMissingParameterDefaultTest(); });
    timedRun ("runM3PhysicalTests", [] { runM3PhysicalTests(); });
    timedRun ("runM3MissingParameterTest", [] { runM3MissingParameterTest(); });
    timedRun ("runGranularTests", [] { runGranularTests(); });
    timedRun ("runHeavyPresetCpuTest", [] { runHeavyPresetCpuTest(); });
    timedRun ("runPhysicalStringTest", [] { runPhysicalStringTest(); });
    timedRun ("runLoopDecayAcrossRatesTest", [] { runLoopDecayAcrossRatesTest(); });
    timedRun ("runPhysicalPatchMigrationTest", [] { runPhysicalPatchMigrationTest(); });
    timedRun ("runScaleRandomReleaseTest", [] { runScaleRandomReleaseTest(); });
    timedRun ("runArpHostStopTests", [] { runArpHostStopTests(); });
    timedRun ("runArpLaneTests", [] { runArpLaneTests(); });
    timedRun ("runM4Tests", [] { runM4Tests(); });
    timedRun ("runM5DeepFmTests", [] { runM5DeepFmTests(); });
    timedRun ("runM6PhaseDistortionTests", [] { runM6PhaseDistortionTests(); });
    timedRun ("runM6bMatrixTests", [] { runM6bMatrixTests(); });
    timedRun ("runM70ExtendedFmModTests", [] { runM70ExtendedFmModTests(); });
    timedRun ("runM71GenerativeTests", [] { runM71GenerativeTests(); });
    timedRun ("runM72BodyTests", [] { runM72BodyTests(); });
    timedRun ("runM73ElectricPianoTests", [] { runM73ElectricPianoTests(); });
    timedRun ("runM74WavetableEditorTests", [] { runM74WavetableEditorTests(); });
    timedRun ("runSplitRenderTest", [] { runSplitRenderTest(); });
    timedRun ("runM81ModulatorTests", [] { runM81ModulatorTests(); });
    timedRun ("runM82PianoTests", [] { runM82PianoTests(); });
    timedRun ("runM83WestTests", [] { runM83WestTests(); });
    timedRun ("runM84FilterTests", [] { runM84FilterTests(); });
    timedRun ("runM85Tests", [] { runM85Tests(); });
    timedRun ("runModRemapTests", [] { runModRemapTests(); });
    timedRun ("runM86Tests", [] { runM86Tests(); });
    timedRun ("runM10Tests", [] { runM10Tests(); });
    timedRun ("runPolishTests", [] { runPolishTests(); });
    timedRun ("runPlanTests", [] { runPlanTests(); });
    timedRun ("runTuningTests", [] { runTuningTests(); });
    timedRun ("runOperatorEgTests", [] { runOperatorEgTests(); });
    timedRun ("runOscModeTests", [] { runOscModeTests(); });
    timedRun ("runAirwindowsTests", [] { runAirwindowsTests(); });
    timedRun ("runVocoderTests", [] { runVocoderTests(); });
    timedRun ("runFilterOverhaulTests", [] { runFilterOverhaulTests(); });
    timedRun ("runMultiSampleTests", [] { runMultiSampleTests(); });
    timedRun ("runTensionEnvTests", [] { runTensionEnvTests(); });
    timedRun ("runPhysModTests", [] { runPhysModTests(); });
    timedRun ("runVoiceKnobTests", [] { runVoiceKnobTests(); });
    timedRun ("runFilterPairTests", [] { runFilterPairTests(); });
    timedRun ("runCpuPassTests", [] { runCpuPassTests(); });
    timedRun ("runCpuRound3Tests", [] { runCpuRound3Tests(); });
    timedRun ("runStressTests", [] { runStressTests(); });
    timedRun ("runSampleTempoTests", [] { runSampleTempoTests(); });
    timedRun ("runSamplerTests", [] { runSamplerTests(); });
    timedRun ("runSplitterTests", [] { runSplitterTests(); });
    timedRun ("runParallelFxTests", [] { runParallelFxTests(); });
    timedRun ("runPatchGraphTests", [] { runPatchGraphTests (false); });
    timedRun ("runFxPatcherTests", [] { runFxPatcherTests(); });
    timedRun ("runFxLfoTests", [] { runFxLfoTests(); });
    timedRun ("runOscRoutingTests", [] { runOscRoutingTests (false); });
    timedRun ("runOscSlotTests", [] { runOscSlotTests(); });
    timedRun ("runNoiseTests", [] { runNoiseTests(); });
    timedRun ("runClipTests", [] { runClipTests(); });
    timedRun ("runBuiltInTableTests", [] { runBuiltInTableTests(); });
    timedRun ("runGranularControlTests", [] { runGranularControlTests(); });
    timedRun ("runParamInfoTests", [] { runParamInfoTests(); });
    timedRun ("runParamManifestTests", [] { runParamManifestTests(); });
    timedRun ("runPatchExtrasTests", [] { runPatchExtrasTests(); });
    timedRun ("runHostIntegrationTests", [] { runHostIntegrationTests(); });
    timedRun ("runHostHardeningTests", [] { runHostHardeningTests(); });
    timedRun ("runHardeningTests", [] { runHardeningTests(); });
    timedRun ("runSweepFixTests", [] { runSweepFixTests(); });
    timedRun ("runRound3FixTests", [] { runRound3FixTests(); });
    timedRun ("runAirwindowsFilterTests", [] { runAirwindowsFilterTests(); });

    // Thread-ownership checks (src/ThreadChecks.h): the audio-thread flag is
    // on inside a processBlock and off after it, and no check fired in any
    // suite above (setFxPatchGraph, addFxEffect, setModSlotValue, beginEdit,
    // endEdit all ran on the message thread, never inside processBlock).
    {
        const auto flagBefore = IlanaThreads::onAudioThread();
        bool flagInside = false;
        { ILANA_AUDIO_THREAD_SCOPE(); flagInside = IlanaThreads::onAudioThread(); }
        check (! flagBefore && flagInside && ! IlanaThreads::onAudioThread(), "thread checks: the audio-thread scope turns on and off");
        check (IlanaThreads::violationCount.load() == 0, "thread checks: no violation in any suite ("
                                                          + juce::String (IlanaThreads::violationCount.load()) + ")");
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << " (" << failures << " failures)" << std::endl;

    return failures == 0 ? 0 : 1;
}
