#include "PluginProcessor.h"
#include "dsp/MultiSample.h"

#ifdef ILANA_TEST_BUILD
#include "dsp/Modulation.h"
juce::AudioProcessorEditor* IlanaSynthAudioProcessor::createEditor() { return nullptr; }
#else
#include "PluginEditor.h"
juce::AudioProcessorEditor* IlanaSynthAudioProcessor::createEditor()
{
    return new IlanaSynthAudioProcessorEditor (*this);
}
#endif

#include "processor/ProcessorInternal.h"

namespace
{
class WavetableSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};
} // namespace

// Built once per process, on all cores (the 120 tables take a few
// seconds on one).
FactoryTables::FactoryTables()
{
    const auto names = TableFactory::getFactoryTableNames();
    const auto count = TableFactory::getNumFactoryTables();
    tables.resize ((size_t) count);
    std::atomic<int> next { 0 };
    const auto work = [&]
    {
        for (auto i = next++; i < count; i = next++)
        {
            auto table = std::make_unique<Wavetable>();
            table->setName (names[i]);
            table->buildFromFrames (TableFactory::generate (i));
            tables[(size_t) i] = std::move (table);
        }
    };
    std::vector<std::thread> threads;
    const auto helpers = juce::jlimit (0, 7, (int) std::thread::hardware_concurrency() - 1);
    for (int t = 0; t < helpers; ++t)
        threads.emplace_back (work);
    work();
    for (auto& thread : threads)
        thread.join();
}

const FactoryTables& FactoryTables::get()
{
    static FactoryTables instance;
    return instance;
}


IlanaSynthAudioProcessor::IlanaSynthAudioProcessor()
#if ILANA_FX
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts (*this, &undoManager, "PARAMS", createParameterLayout())
{
#if ILANA_FINGERPRINT_BUILD
    arpRandom.setSeed (31415);
    lfoRandom.setSeed (27182);
#endif

    for (int step = 0; step < 16; ++step)
    {
        const auto n = juce::String (step + 1);
        pseqChanceIds[(size_t) step] = "pseq_chance" + n;
        pseqRangeIds[(size_t) step] = "pseq_range" + n;
        pseqRatchetIds[(size_t) step] = "pseq_ratchet" + n;
        arpVelocityIds[(size_t) step] = "arp_vel" + n;
        arpLengthIds[(size_t) step] = "arp_len" + n;
        arpPitchIds[(size_t) step] = "arp_pitch" + n;
    }

    for (auto* parameter : getParameters())
        parameter->addListener (&paramEpochListener);

    arpHeldNotes.ensureStorageAllocated (128);
    clipHeld.ensureStorageAllocated (128);
    clipActive.reserve (1024);
    arpChordActive.ensureStorageAllocated (128);
    arpChordNotes.ensureStorageAllocated (128);
    spectralCache = std::make_unique<SpectralCache> ([] (int index) { return FactoryTables::get().tables[(size_t) index].get(); },
                                                     TableFactory::getNumFactoryTables());

    for (auto& value : modDisplayValues)
        value.store (0.0f);

    for (auto& value : displaySamplePositions)
        value.store (-1.0f);

    for (int i = 0; i < Mod::numMacros; ++i)
        macroCc[i].store (20 + i);

    for (size_t cc = 0; cc < ccParameter.size(); ++cc)
    {
        ccParameter[cc].store (-1);
        pendingCcValues[cc].store (0.0f);
        ccValuePending[cc].store (false);
    }

    scopeLeft.assign ((size_t) scopeSize, 0.0f);
    scopeRight.assign ((size_t) scopeSize, 0.0f);

    for (int lfo = 0; lfo < numLfos; ++lfo)
        for (int i = 0; i < lfoDrawSteps; ++i)
            lfoCustom[(size_t) lfo][(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * (double) i / (double) lfoDrawSteps);

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        lfoCurves[(size_t) lfo] = LfoCurve::preset (0);
        lfoCurves[(size_t) lfo].renderTable (lfoCurveTables[(size_t) lfo].data());
    }

    FactoryTables::get();

    userTables.resize ((size_t) numUserSlots);
    sampleSlots.resize ((size_t) numSampleOscs);

    for (int i = 0; i < SampleFactory::getNumFactorySamples(); ++i)
        factorySamples[(size_t) i] = SampleFactory::generate (i);

    {
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            oscCoreIds[(size_t) i] = { prefix + "_on", prefix + "_table", prefix + "_frame", prefix + "_level",
                                       prefix + "_pan", prefix + "_semi", prefix + "_fine", prefix + "_unison",
                                       prefix + "_detune", prefix + "_spread", prefix + "_spectral",
                                       prefix + "_spectral_amt", prefix + "_chord", prefix + "_out" };

            stringParamIds[(size_t) i] = { prefix + "_mode", prefix + "_excite", prefix + "_string_decay",
                                           prefix + "_string_damp", prefix + "_string_sustain",
                                           prefix + "_string_stiffness", prefix + "_string_pickup", prefix + "_string_excite_pos",
                                           prefix + "_string_pick_hardness", prefix + "_string_pick_pos", prefix + "_string_slap" };
            bowBuzzIds[(size_t) i] = { prefix + "_bow_pressure", prefix + "_bow_speed",
                                        prefix + "_bridge_buzz", prefix + "_fret_rattle" };
            keysParamIds[(size_t) i] = { prefix + "_hammer_hard", prefix + "_couple",
                                          prefix + "_damper", prefix + "_register" };
            electricParamIds[(size_t) i] = { prefix + "_ep_distance", prefix + "_ep_position" };
            feedbackParamIds[(size_t) i] = { prefix + "_fb_gain", prefix + "_fb_distance" };
            grainLiveIds[(size_t) i] = prefix + "_grain_live";
            sampleParamIds[(size_t) i] = { prefix + "_sample_tuned", prefix + "_sample_loop",
                                           prefix + "_sample_reverse", prefix + "_sample_start",
                                           prefix + "_sample_end", prefix + "_sample_fade_in",
                                           prefix + "_sample_fade_out" };
            grainParamIds[(size_t) i] = { prefix + "_grain_size", prefix + "_grain_density",
                                         prefix + "_grain_spray", prefix + "_grain_pitch",
                                         prefix + "_grain_spread" };
        }
    }

    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto prefix = "mod" + juce::String (i + 1);
        modSlotIds[(size_t) i] = { prefix + "_src", prefix + "_dst", prefix + "_amt", prefix + "_curve",
                                   prefix + "_pol", prefix + "_aux", prefix + "_byp" };

        const auto& ids = modSlotIds[(size_t) i];
        modSlotRaw[(size_t) i] = { apvts.getRawParameterValue (ids.src), apvts.getRawParameterValue (ids.dst),
                                   apvts.getRawParameterValue (ids.amt), apvts.getRawParameterValue (ids.curve),
                                   apvts.getRawParameterValue (ids.polarity), apvts.getRawParameterValue (ids.aux),
                                   apvts.getRawParameterValue (ids.bypass) };
    }

    for (const auto& destination : Mod::getParamDestinations())
    {
        ParamDestination entry;
        entry.raw = apvts.getRawParameterValue (destination.id);
        entry.parameter = apvts.getParameter (destination.id);

        if (entry.raw != nullptr)
            rawToParamDestination[entry.raw] = (int) paramDestinations.size();

        paramDestinations.push_back (entry);
    }

    for (int env = 6; env <= 16; ++env)
    {
        const auto prefix = "env" + juce::String (env);
        extraEnvIds[(size_t) (env - 6)] = { prefix + "_attack", prefix + "_decay", prefix + "_sustain",
                                            prefix + "_release", prefix + "_curve", prefix + "_velocity" };
    }

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        oscAmpEnvIds[(size_t) osc] = prefix + "_amp_env";
        for (size_t field = 0; field < OperatorEg::operatorFields().size(); ++field)
            operatorEgIds[(size_t) osc][field] = prefix + OperatorEg::operatorFields()[field].suffix;
        sampleFactoryIds[(size_t) osc] = prefix + "_sample_factory";

        for (int target = 0; target < OscillatorIds::count; ++target)
            fmMatrixIds[(size_t) osc][(size_t) target] = fmRouteId (osc, target);
    }
    for (size_t field = 0; field < OperatorEg::voiceFields().size(); ++field)
        operatorEgVoiceIds[field] = OperatorEg::voiceFields()[field].suffix;

    buildParamCache();

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto prefix = "lfo" + juce::String (lfo + 1);
        auto& ids = lfoIds[(size_t) lfo];
        ids.shape = prefix + "_shape";
        ids.rate = prefix + "_rate";
        ids.sync = prefix + "_sync";
        ids.div = prefix + "_div";
        ids.retrig = prefix + "_retrig";
        ids.phase = prefix + "_phase";
        ids.key = prefix + "_key";
        ids.physA = prefix + "_phys_a";
        ids.physB = prefix + "_phys_b";
        ids.kick = prefix + "_kick";
        for (int param = 0; param < LfoSimInfo::numParams; ++param)
            ids.sim[(size_t) param] = prefix + "_p" + juce::String (param + 1);
        ids.smooth = prefix + "_smooth";
        ids.axis = prefix + "_axis";
        ids.trigger = prefix + "_trigger";
        ids.loop = prefix + "_loop";
        ids.seed = prefix + "_seed";
        ids.stereo = prefix + "_stereo";
        ids.fire = prefix + "_fire";

        for (int step = 0; step < 16; ++step)
            ids.steps[(size_t) step] = prefix + "_step" + juce::String (step + 1);
    }

    for (int m = 0; m < Mod::numMacros; ++m)
    {
        macroIds[(size_t) m] = "macro" + juce::String (m + 1);
        evolveIds[(size_t) m] = { "macro" + juce::String (m + 1) + "_evolve", "macro" + juce::String (m + 1) + "_evolve_rate" };
    }
    for (int point = 0; point < numVectorPoints; ++point)
        vectorPathIds[(size_t) point] = { "vec_px" + juce::String (point + 1), "vec_py" + juce::String (point + 1) };
    evolve.reset (4242u);
    vectorDrift.reset (9191u);

    for (int slot = 0; slot < numFxSlots; ++slot)
    {
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        fxSlotIds[(size_t) slot] = { prefix, prefix + "_bypass", prefix + "_solo", prefix + "_mix", prefix + "_band" };
    }

    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            oscShapeIds[(size_t) osc] = { prefix + "_warp", prefix + "_warp_amt", prefix + "_uni_mode", prefix + "_uni_blend",
                                          prefix + "_route" };
            operatorIds[(size_t) osc] = { prefix + "_tune", prefix + "_ratio", prefix + "_ratio_snap", prefix + "_fixed_hz",
                                          prefix + "_key_level", prefix + "_fb_type", prefix + "_warp2", prefix + "_warp2_amt",
                                          prefix + "_pd_env", prefix + "_pd_env_amt" };
            fmNoiseIds[(size_t) osc] = "fm_noise" + juce::String (osc + 1);
        }

        for (int env = 0; env < 16; ++env)
        {
            const auto prefix = envelopePrefix (env);
            envelopeExtraIds[(size_t) env] = { prefix + "_delay", prefix + "_hold", prefix + "_keyrate" };
        }

        for (int point = 0; point < Mseg::numPoints; ++point)
        {
            msegLevelIds[(size_t) point] = "mseg_level" + juce::String (point + 1);
            msegTimeIds[(size_t) point] = "mseg_time" + juce::String (point + 1);
        }
    }

    for (int step = 0; step < 16; ++step)
    {
        tapStepIds[(size_t) step] = "fx_taps_step" + juce::String (step + 1);
        gateStepIds[(size_t) step] = "fx_gate_step" + juce::String (step + 1);
    }

    for (int i = 0; i < numUserSlots; ++i)
        resetUserTableToDefault (i);

    synth.addSound (new WavetableSound());

    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new Voice());

    synth.setNoteStealingEnabled (true);

    // LFO 1-4 draw from lfoRandom exactly as before the pool; LFO 5-16 have
    // their own generator so they never shift anyone else's random sequence.
    for (int lfo = 0; lfo < numLfos; ++lfo)
        lfoSampleHolds[(size_t) lfo].store (randomForLfo (lfo).nextFloat() * 2.0f - 1.0f);

    for (int lfo = 0; lfo < numLfos; ++lfo)
        lfoChaos[(size_t) lfo].reset (randomForLfo (lfo));

    // M7.5: ilanaSynth FX opens on a patch that plays its input.
    if (isEffectBuild)
        if (const auto index = getFactoryPresetNames().indexOf ("Live Body"); index >= 0)
            loadFactoryPreset (index);
}

namespace
{
std::uint32_t hashParamId (const char* id) noexcept
{
    // FNV-1a.
    std::uint32_t hash = 2166136261u;

    for (; *id != 0; ++id)
        hash = (hash ^ (std::uint8_t) *id) * 16777619u;

    return hash;
}
} // namespace

void IlanaSynthAudioProcessor::buildParamCache()
{
    const auto& parameters = getParameters();
    std::uint32_t size = 1;

    while (size < (std::uint32_t) parameters.size() * 4)
        size <<= 1;

    paramCache.assign (size, {});
    paramCacheMask = size - 1;
    paramCacheIds.clear();
    paramCacheIds.reserve ((size_t) parameters.size());

    for (auto* parameter : parameters)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            paramCacheIds.push_back (withId->paramID);

    for (const auto& id : paramCacheIds)
    {
        ParamCacheEntry entry;
        entry.id = id.toRawUTF8();
        entry.hash = hashParamId (entry.id);
        entry.value = apvts.getRawParameterValue (id);

        if (entry.value == nullptr)
            continue;

        // A stored choice or step isn't always an exact integer (index 7 of
        // 136 comes back as 6.9999995), and (int) casts would truncate it.
        {
            auto* parameter = apvts.getParameter (id);
            entry.discrete = dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr
                             || dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr
                             || dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr;
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                entry.fallback = ranged->convertFrom0to1 (ranged->getDefaultValue());
        }

        if (const auto found = rawToParamDestination.find (entry.value); found != rawToParamDestination.end())
            entry.destination = found->second;

        for (auto slot = entry.hash & paramCacheMask;; slot = (slot + 1) & paramCacheMask)
        {
            if (paramCache[slot].id == nullptr)
            {
                paramCache[slot] = entry;
                break;
            }
        }
    }
}

const IlanaSynthAudioProcessor::ParamCacheEntry* IlanaSynthAudioProcessor::findParam (const char* id) const
{
    if (paramCache.empty())
        return nullptr;

    const auto hash = hashParamId (id);

    for (auto slot = hash & paramCacheMask;; slot = (slot + 1) & paramCacheMask)
    {
        const auto& candidate = paramCache[slot];

        if (candidate.id == nullptr)
            return nullptr; // not a parameter

        if (candidate.hash == hash && std::strcmp (candidate.id, id) == 0)
            return &candidate;
    }
}

// Set while this thread runs processChunk. Modulation offsets are the audio
// thread's: another thread (the editor, a preset load) reads the plain
// value, rather than racing on offsets the audio thread rewrites each block
// (a preset load chose its macros' directions from them).
static thread_local bool readingOnAudioThread = false;

float IlanaSynthAudioProcessor::readParam (const ParamCacheEntry& entry) const
{
    auto result = entry.value->load();
    // A host (or a damaged state) can set NaN, which parameters don't clamp.
    if (! std::isfinite (result))
        result = entry.fallback;
    if (entry.discrete)
        result = std::round (result);

    if (readingOnAudioThread && anyParamModulation && entry.destination >= 0)
    {
        const auto offset = paramDestinationOffsets[(size_t) entry.destination];

        if (offset != 0.0f)
            if (auto* parameter = paramDestinations[(size_t) entry.destination].parameter)
                result = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (result) + offset));
        if (entry.discrete)
            result = std::round (result);
    }

    return result;
}

float IlanaSynthAudioProcessor::getParam (const char* id) const
{
    const auto* entry = findParam (id);
    return entry != nullptr ? readParam (*entry) : 0.0f;
}

float IlanaSynthAudioProcessor::getRawParam (const ParamRef& ref) const
{
    auto* entry = ref.entry.load (std::memory_order_relaxed);

    if (entry == nullptr)
    {
        entry = findParam (ref.toRawUTF8());

        if (entry == nullptr)
            return 0.0f;

        ref.entry.store (entry, std::memory_order_relaxed);
    }

    auto value = entry->value->load();
    if (! std::isfinite (value))
        value = entry->fallback;
    return entry->discrete ? std::round (value) : value;
}

float IlanaSynthAudioProcessor::getParam (const ParamRef& ref) const
{
    auto* entry = ref.entry.load (std::memory_order_relaxed);

    if (entry == nullptr)
    {
        entry = findParam (ref.toRawUTF8());

        if (entry == nullptr)
            return 0.0f;

        ref.entry.store (entry, std::memory_order_relaxed);
    }

    return readParam (*entry);
}

void IlanaSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    noteSpray.reset();
    arpHeldNotes.clearQuick();   // what's sounding is released by its gate
    generatedMidi.ensureSize (4096);
    clipScratch.ensureSize (8192);
    clipActive.clear();
    clipHeld.clearQuick();
    clipRunning = clipUsedHost = false;
    clipLastIndex = clipLastMode = -1;
    // A new stream: nothing to ease from.
    for (int channel = 0; channel < 2; ++channel)
        lastOutput[channel] = declick[channel] = 0.0f;

    currentSampleRate = sampleRate;
    sympatheticStrings.prepare (sampleRate);
    soundboard.prepare (sampleRate);
    denseSoundboard.prepare (sampleRate);
    pedalResonance.prepare (sampleRate);
    mechanicalNoise.prepare (sampleRate);
    keysPedalDown = false;
    pedalHeldNotes.fill (false);
    displaySampleRate.store (sampleRate);
    baseSampleRate = sampleRate;
    expectedBlockSize = juce::jmax (1, samplesPerBlock);

    oversamplingFactor.store (wantedOversamplingFactor());

    for (auto* oversampler : { &oversampler2x, &oversampler4x })
    {
        oversampler->initProcessing ((size_t) expectedBlockSize);
        oversampler->reset();
    }

    const auto voiceRate = baseSampleRate * (double) oversamplingFactor.load();

    synth.setCurrentPlaybackSampleRate (voiceRate);

    scaledMidiBuffer.ensureSize (1024);

    lfoBuffers.setSize (numLfoChannels, expectedBlockSize * oversamplingFactor.load(), false, false, true);

    // M7.5: room for the input at up to 4x oversampling, and 3 s of history.
    liveDry.setSize (2, expectedBlockSize, false, true, false);
    dryDelayRing.setSize (2, 1024, false, true, false);
    dryDelayRing.clear();
    dryDelayWrite = 0;
    liveVoice.assign ((size_t) expectedBlockSize * 4, 0.0f);
    liveEnvVoice.assign ((size_t) expectedBlockSize * 4, 0.0f);
    if (isEffectBuild)
    {
        liveHistory.buffer.setSize (2, (int) (sampleRate * 3.0), false, true, false);
        liveHistory.buffer.clear();
        liveHistory.sampleRate = sampleRate;
        liveHistory.name = "Live input";
    }
    liveHistoryWrite = 0;
    liveLast = liveEnvState = 0.0f;
    liveGateOpen = false;
    liveGateNote = -1;
    liveInputSamples = 0;
    for (auto& stage : dcBlock)
        for (auto& channel : stage)
            channel = { 0.0f, 0.0f };

    stutterBuffer.setSize (2, (int) (sampleRate * 2.0), false, false, true);

    for (int channel = 0; channel < 2; ++channel)
    {
        tapeShift[channel].prepare (sampleRate);
        shimmerShift[channel].prepare (sampleRate);
        smear[channel].prepare (sampleRate);
        freeze[channel].prepare (sampleRate);
    }

    haasLine.setMaximumDelayInSamples ((int) (sampleRate * 0.06));

    tapeStopBuffer.setSize (2, (int) (sampleRate * 2.0), false, false, true);
    tapeStopWrite = 0;
    tapeStopRead = 0.0;
    tapeStopRate = 1.0f;
    tapeStopLagFade = 1.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        octaverShift[channel].prepare (sampleRate);

        for (auto& band : eqBands[channel])
            band.reset();

        for (int band = 0; band < 3; ++band)
        {
            vowelFilters[channel][band].setSampleRate (sampleRate);
            vowelFilters[channel][band].setMode (Svf::Mode::BandPass);
            vowelFilters[channel][band].reset();
        }
    }

    flangerPhase = 0.0;
    dimPhase = 0.0;
    tremoloPhase = 0.0;
    shifterPhase = 0.0;
    ringPhase = 0.0;
    gatePhase = 0.0;
    gateEnvelope = 1.0f;
    gatedReverbEnvelope = 0.0f;
    duckEnvelope = 0.0f;

    for (int channel = 0; channel < 2; ++channel)
    {
        tiltLowState[channel] = 0.0f;
        tiltHighState[channel] = 0.0f;
        limiterEnvelope[channel] = 0.0f;
        shifterAllpass[channel] = 0.0f;
        shifterDelay[channel] = 0.0f;

        for (int band = 0; band < 3; ++band)
            ottEnvelope[channel][band] = 0.0f;
    }

    compEnvelope[0] = 0.0f;
    compEnvelope[1] = 0.0f;
    ampLowState[0] = 0.0f;
    ampLowState[1] = 0.0f;
    ampHighState[0] = 0.0f;
    ampHighState[1] = 0.0f;

    mseg.prepare (voiceRate);
    mseg.reset();
    wowPhase = 0.0;
    clockShPhase = 0.0;
    clockShValue = 0.0f;

    crushCounter = 0;
    delayDampState[0] = 0.0f;
    delayDampState[1] = 0.0f;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    haasLine.prepare (spec);
    haasLine.reset();

    feedbackLine.setMaximumDelayInSamples ((int) (sampleRate * 0.12));
    feedbackLine.prepare (spec);
    feedbackLine.reset();
    feedbackState[0] = 0.0f;
    feedbackState[1] = 0.0f;
    stutterPosition = 0.0;
    stutterRate = 1.0f;

    flangerLine.setMaximumDelayInSamples ((int) (sampleRate * 0.05));
    flangerLine.prepare (spec);
    flangerLine.reset();

    dimLine.setMaximumDelayInSamples ((int) (sampleRate * 0.15));
    dimLine.prepare (spec);
    dimLine.reset();

    springComb.setMaximumDelayInSamples ((int) (sampleRate * 0.05));
    springComb.prepare (spec);
    springComb.reset();

    chorus.prepare (spec);
    chorus.reset();

    phaser.prepare (spec);
    phaser.reset();

    convolution.prepare (spec);
    convolution.reset();

    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayLine.prepare (spec);
    delayLine.reset();

    combLine.setMaximumDelayInSamples ((int) (sampleRate * 0.15));
    combLine.prepare (spec);
    combLine.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();
    airwindowsModule.prepare (sampleRate, samplesPerBlock);
    for (auto& module : awCategoryModules)
        module.prepare (sampleRate, samplesPerBlock);
    vocoder.prepare (sampleRate);
    vocoderModulator.assign ((size_t) juce::jmax (samplesPerBlock, expectedBlockSize), 0.0f);
    chunkMidi.ensureSize (4096);
    updateLatency();
}

// A new patch starts from silence: the old voices stop and the effects'
// memory (reverb, delays, freeze, the piano body) is cleared, on the audio
// thread and without allocating.
void IlanaSynthAudioProcessor::cutPatchTails()
{
    synth.allNotesOff (0, false);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<Voice*> (synth.getVoice (i)))
            voice->resetForNewPatch();
    noteSpray.reset();

    for (auto& stage : dcBlock)
        for (auto& channel : stage)
            channel = { 0.0f, 0.0f };

    for (int channel = 0; channel < 2; ++channel)
    {
        declick[channel] = lastOutput[channel];
        tapeShift[channel].reset();
        shimmerShift[channel].reset();
        octaverShift[channel].reset();
        smear[channel].reset();
        freeze[channel].reset();
        feedbackState[channel] = 0.0f;
        delayDampState[channel] = 0.0f;
    }

    sympatheticStrings.reset();
    soundboard.reset();
    denseSoundboard.reset();
    pedalResonance.reset();
    mechanicalNoise.reset();
    stutterBuffer.clear();
    tapeStopBuffer.clear();
    haasLine.reset();
    feedbackLine.reset();
    flangerLine.reset();
    dimLine.reset();
    springComb.reset();
    chorus.reset();
    phaser.reset();
    convolution.reset();
    delayLine.reset();
    combLine.reset();
    reverb.reset();
    airwindowsModule.reset();
    for (auto& module : awCategoryModules)
        module.reset();
    vocoder.reset();

    // The effects' own LFOs and followers restart too (a Dimension or flanger
    // phase left by the last patch otherwise changes how this one starts).
    flangerPhase = 0.0;
    dimPhase = 0.0;
    tremoloPhase = 0.0;
    shifterPhase = 0.0;
    ringPhase = 0.0;
    gatePhase = 0.0;
    gateEnvelope = 1.0f;
    gatedReverbEnvelope = 0.0f;
    duckEnvelope = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
    {
        limiterEnvelope[channel] = 0.0f;
        for (int band = 0; band < 3; ++band)
            ottEnvelope[channel][band] = 0.0f;
    }
    compEnvelope[0] = compEnvelope[1] = 0.0f;

    // The modulators start over too, as in a new instance, so a patch sounds
    // the same whatever played before it (a slow free-running LFO otherwise
    // starts wherever the last patch left it).
    lfoRandom.setSeed (27182);
    lfoPoolRandom.setSeed (31415);
    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        lfoPhases[(size_t) lfo] = 0.0;
        lfoSampleHolds[(size_t) lfo].store (randomForLfo (lfo).nextFloat() * 2.0f - 1.0f);
        lfoFireWas[(size_t) lfo] = false;
        lfoBeatLast[(size_t) lfo] = -1;
        lfoSims[(size_t) lfo].restart();
        lfoSmoothers[(size_t) lfo].reset();
        lfoPreviousShapes[(size_t) lfo] = -1;
    }
    lfoSimSeedCounter = 1;
    for (int lfo = 0; lfo < numLfos; ++lfo)
        lfoChaos[(size_t) lfo].reset (randomForLfo (lfo));
    mseg.reset();
    clockShPhase = 0.0;
    clockShValue = 0.0f;
    vectorPathPhase = 0.0;
    evolve.reset (4242u);
    vectorDrift.reset (9191u);
    for (auto& drift : macroDrift)
        drift.store (0.0f);
}

// The oversamplers' filters delay the output a little; the host compensates.
void IlanaSynthAudioProcessor::updateLatency()
{
    const auto factor = oversamplingFactor.load();
    const auto latency = factor > 1 ? juce::roundToInt (activeOversampler().getLatencyInSamples()) : 0;
    setLatencySamples (latency);

    const auto delay = juce::jlimit (0, juce::jmax (0, dryDelayRing.getNumSamples() - 1), latency);
    if (delay != dryDelaySamples)
    {
        dryDelaySamples = delay;
        dryDelayRing.clear();
        dryDelayWrite = 0;
    }
}

void IlanaSynthAudioProcessor::delayLiveDry (int numSamples)
{
    const auto size = dryDelayRing.getNumSamples();
    if (dryDelaySamples <= 0 || size == 0)
        return;

    auto write = dryDelayWrite;
    for (int channel = 0; channel < juce::jmin (2, liveDry.getNumChannels()); ++channel)
    {
        auto* data = liveDry.getWritePointer (channel);
        auto* ring = dryDelayRing.getWritePointer (channel);
        write = dryDelayWrite;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto read = (write - dryDelaySamples + size) % size;
            ring[write] = data[i];
            data[i] = ring[read];
            write = (write + 1) % size;
        }
    }
    dryDelayWrite = write;
}

int IlanaSynthAudioProcessor::wantedOversamplingFactor() const
{
    if (getParam ("oversampling") < 0.5f)
        return 1;

    return (int) getParam ("os_factor") == 1 ? 4 : 2;
}

void IlanaSynthAudioProcessor::setOversampling (int factor)
{
    factor = factor >= 4 ? 4 : (factor >= 2 ? 2 : 1);

    if (factor == oversamplingFactor.load())
        return;

    suspendProcessing (true);

    oversamplingFactor.store (factor);

    for (auto* oversampler : { &oversampler2x, &oversampler4x })
    {
        oversampler->reset();
        oversampler->initProcessing ((size_t) expectedBlockSize);
    }

    const auto voiceRate = baseSampleRate * (double) factor;

    synth.setCurrentPlaybackSampleRate (voiceRate);
    mseg.prepare (voiceRate);
    mseg.reset();
    lfoBuffers.setSize (numLfoChannels, expectedBlockSize * factor, false, false, true);
    scaledMidiBuffer.ensureSize (1024);
    updateLatency();

    suspendProcessing (false);
}

void IlanaSynthAudioProcessor::releaseResources()
{
}

bool IlanaSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
   #if ILANA_FX
    if (input != juce::AudioChannelSet::disabled() && input != juce::AudioChannelSet::mono()
        && input != juce::AudioChannelSet::stereo())
        return false;
   #else
    if (input != juce::AudioChannelSet::disabled())
        return false;
   #endif

    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo() || output == juce::AudioChannelSet::mono();
}

void IlanaSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const auto total = buffer.getNumSamples();

    if (total <= expectedBlockSize)
    {
        processChunk (buffer, midiMessages);
        return;
    }

    // A host may hand over a longer block than it announced (some offline
    // renders do); the oversamplers are sized for the announced one.
    for (int start = 0; start < total; start += expectedBlockSize)
    {
        const auto length = juce::jmin (expectedBlockSize, total - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
        chunkMidi.clear();
        chunkMidi.addEvents (midiMessages, start, length, -start);
        processChunk (chunk, chunkMidi);
    }
}

void IlanaSynthAudioProcessor::captureLiveInput (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = juce::jmin (buffer.getNumSamples(), liveDry.getNumSamples());
    const auto inputs = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    for (int channel = 0; channel < 2; ++channel)
        liveDry.copyFrom (channel, 0, buffer, juce::jmin (channel, inputs - 1), 0, numSamples);
    liveInputSamples = numSamples;
    buffer.clear();
}

// Once per block: INPUT GAIN, the envelope follower, the history for live
// grains, the input at the voice rate, and GATE / DRONE notes.
void IlanaSynthAudioProcessor::prepareLiveInput (int numSamples, int factor, juce::MidiBuffer& midi)
{
    factor = juce::jlimit (1, 4, factor);
    const auto valid = juce::jmin (numSamples, liveInputSamples);
    const auto gain = juce::Decibels::decibelsToGain (getParam (inGainRef));
    const auto rate = (float) juce::jmax (1.0, baseSampleRate);
    const auto attack = std::exp (-1.0f / (getParam (inAttackRef) * 0.001f * rate));
    const auto release = std::exp (-1.0f / (getParam (inReleaseRef) * 0.001f * rate));
    const auto trigger = (int) getParam (inTriggerRef);
    const auto threshold = juce::Decibels::decibelsToGain (getParam (inThresholdRef));
    const auto note = juce::jlimit (0, 127, (int) getParam (inNoteRef));
    const auto* left = liveDry.getReadPointer (0);
    const auto* right = liveDry.getReadPointer (1);
    const auto historyLength = liveHistory.buffer.getNumSamples();
    auto* historyL = historyLength > 0 ? liveHistory.buffer.getWritePointer (0) : nullptr;
    auto* historyR = historyLength > 0 ? liveHistory.buffer.getWritePointer (1) : nullptr;
    auto peak = 0.0f;
    auto crossing = -1, falling = -1;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = i < valid ? left[i] * gain : 0.0f;
        const auto r = i < valid ? right[i] * gain : 0.0f;
        const auto x = 0.5f * (l + r);
        const auto magnitude = std::abs (x);
        liveEnvState = magnitude > liveEnvState ? attack * liveEnvState + (1.0f - attack) * magnitude
                                                : release * liveEnvState + (1.0f - release) * magnitude;
        // A full-scale sine reads about 0.64 before the scaling.
        const auto envelope = juce::jmin (1.0f, liveEnvState * 1.5f);
        peak = juce::jmax (peak, magnitude);

        if (trigger == 1)
        {
            if (! liveGateOpen && crossing < 0 && envelope > threshold)
                crossing = i;
            else if (liveGateOpen && falling < 0 && envelope < threshold * 0.5f)
                falling = i;
        }

        for (int step = 0; step < factor; ++step)
        {
            const auto t = (float) (step + 1) / (float) factor;
            liveVoice[(size_t) (i * factor + step)] = liveLast + (x - liveLast) * t;
            liveEnvVoice[(size_t) (i * factor + step)] = envelope;
        }
        liveLast = x;

        if (historyL != nullptr)
        {
            historyL[liveHistoryWrite] = l;
            historyR[liveHistoryWrite] = r;
            liveHistoryWrite = (liveHistoryWrite + 1) % historyLength;
        }
    }

    inputLevelDisplay.store (juce::jmax (peak, inputLevelDisplay.load() * 0.9f));
    inputEnvDisplay.store (juce::jmin (1.0f, liveEnvState * 1.5f));

    // GATE plays IN NOTE while the input is over the threshold (it lets go
    // 6 dB under it); DRONE holds it down; either way it lets go when turned off.
    const auto noteOff = [this, &midi] (int position)
    {
        if (liveGateOpen && liveGateNote >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (1, liveGateNote), position);
        liveGateOpen = false;
    };
    if (trigger == 1)
    {
        if (liveGateOpen && (falling >= 0 || note != liveGateNote))
            noteOff (juce::jmax (0, falling));
        if (! liveGateOpen && crossing >= 0)
        {
            const auto velocity = (juce::uint8) juce::jlimit (30, 127, 30 + (int) (97.0f * juce::jmin (1.0f, peak * 1.5f)));
            midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), crossing);
            liveGateOpen = true;
            liveGateNote = note;
        }
    }
    else if (trigger == 2)
    {
        // A new patch starts the drone again (loading one stops the voices).
        if (liveGateOpen && (note != liveGateNote || liveRetrigger.exchange (false)))
            noteOff (0);
        if (! liveGateOpen)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            liveGateOpen = true;
            liveGateNote = note;
        }
    }
    else if (liveGateOpen)
    {
        noteOff (0);
    }
}

void IlanaSynthAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const auto startTicks = juce::Time::getHighResolutionTicks();
    struct AudioThreadReads
    {
        bool previous = readingOnAudioThread;
        AudioThreadReads() { readingOnAudioThread = true; }
        ~AudioThreadReads() { readingOnAudioThread = previous; }
    } audioThreadReads;

    juce::ScopedNoDenormals noDenormals;
    spectralCache->setSynchronous (isNonRealtime());

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // M7.5: the input goes aside (the engine plays it, DRY adds it back at
    // the end) and the buffer starts silent, as it does for the instrument.
    liveInputSamples = 0;
    if (totalNumInputChannels > 0)
        captureLiveInput (buffer);

    if (patchCut.exchange (false))
        cutPatchTails();

    {
        const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
        activeLfoCustom = lfoCustom;
        activeLfoCurveTables = lfoCurveTables;
    }

    if (const auto epoch = remapEpoch.load(); epoch != activeRemapEpoch)
    {
        const juce::SpinLock::ScopedLockType lock (remapLock);
        activeModRemapTables = modRemapTables;
        for (size_t i = 0; i < activeModRemapOn.size(); ++i)
            activeModRemapOn[i] = modRemapOn[i].load();
        activeRemapEpoch = epoch;
    }

    {
        const auto scope = previewFifo.read (previewFifo.getNumReady());

        const auto addPreview = [&midiMessages, this] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& event = previewEvents[(size_t) i];
                midiMessages.addEvent (event.isOn ? juce::MidiMessage::noteOn (1, event.note, event.velocity)
                                                  : juce::MidiMessage::noteOff (1, event.note),
                                       0);
            }
        };

        addPreview (scope.startIndex1, scope.blockSize1);
        addPreview (scope.startIndex2, scope.blockSize2);
    }

    {
        if (wantedOversamplingFactor() != oversamplingFactor.load())
            triggerAsyncUpdate();
    }

    if (isEffectBuild)
        prepareLiveInput (buffer.getNumSamples(), oversamplingFactor.load(), midiMessages);

    // Read every mod slot once; switched-on slots are packed for the voices
    // and evaluated synth-wide for the effects and the knob rings.
    Mod::Slot activeSlots[Mod::maxSlots];
    auto numActiveSlots = 0;

    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        auto slot = readModSlot (i);
        slot.remap = activeModRemapOn[(size_t) i] ? activeModRemapTables[(size_t) i].data() : nullptr;

        if (slot.isActive())
            activeSlots[numActiveSlots++] = slot;
    }

    lfoRouted.fill (false);
    lfoRoutedB.fill (false);
    for (int i = 0; i < numActiveSlots; ++i)
        for (const auto source : { activeSlots[i].source, activeSlots[i].aux })
        {
            if (const auto lfo = Mod::lfoIndexFor (source); lfo >= 0)
                lfoRouted[(size_t) lfo] = true;
            if (const auto lfo = Mod::lfoBIndexFor (source); lfo >= 0)
                lfoRoutedB[(size_t) lfo] = true;
        }

    // M8.3: the WEST gate's strike source is read by the voices directly.
    if (getParam ("west_on") > 0.5f)
    {
        const auto westSource = (Mod::Source) (int) getParam ("west_src");
        if (const auto lfo = Mod::lfoIndexFor (westSource); lfo >= 0)
            lfoRouted[(size_t) lfo] = true;
        if (const auto lfo = Mod::lfoBIndexFor (westSource); lfo >= 0)
            lfoRoutedB[(size_t) lfo] = true;
    }

    const auto factor = oversamplingFactor.load();

    if (factor > 1)
    {
        const auto savedRate = currentSampleRate;
        currentSampleRate = baseSampleRate * (double) factor;
        renderLfos (buffer.getNumSamples() * factor, midiMessages);
        currentSampleRate = savedRate;
    }
    else
    {
        renderLfos (buffer.getNumSamples(), midiMessages);
    }

    for (const auto metadata : midiMessages)
    {
        if (metadata.getMessage().isNoteOn())
        {
            displayFrequency.store ((float) juce::MidiMessage::getMidiNoteInHertz (metadata.getMessage().getNoteNumber()));
            displayPhase.store (0.0f);
            ++noteOnCount;
        }
    }

    if (displayFrequency.load() > 0.0f)
    {
        auto phase = displayPhase.load() + displayFrequency.load() / (float) currentSampleRate * (float) buffer.getNumSamples();

        while (phase >= 1.0f)
            phase -= 1.0f;

        displayPhase.store (phase);
    }

    for (int lfo = 0; lfo < numLfos; ++lfo)
        lfoPhaseDisplays[(size_t) lfo].store ((float) lfoPhases[(size_t) lfo]);

    updateEvolveAndVector (buffer.getNumSamples());
    evaluateGlobalModulation (activeSlots, numActiveSlots);

    VoiceParams p;

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto& ids = oscCoreIds[(size_t) osc];
        auto& settings = p.oscillators[(size_t) osc];
        const auto choice = (int) getParam (ids.table);
        p.oscillatorEnabled[(size_t) osc] = getParam (ids.on) > 0.5f;
        settings.table = p.oscillatorEnabled[(size_t) osc]
                             ? spectralCache->get (osc, choice, getTableForChoice (choice),
                                                   (int) getParam (ids.spectral),
                                                   getParam (ids.spectralAmount))
                             : getTableForChoice (choice);
        settings.frame = getParam (ids.frame);
        settings.level = getParam (ids.level) * vectorGains[(size_t) osc];
        settings.pan = getParam (ids.pan);
        settings.semitones = (double) getParam (ids.semi);
        settings.cents = (double) getParam (ids.fine);
        settings.unison = (int) getParam (ids.unison);
        settings.detuneCents = getParam (ids.detune);
        settings.spread = getParam (ids.spread);
        settings.chord = (int) getParam (ids.chord);
    }

    p.subOctaveOffset = 0;

    // Dedicated sub: the Sine, PWM (square) or Analog (saw) table at frame 0.
    {
        const auto shape = (int) getParam ("sub_shape");
        p.subOscEnabled = getParam ("subosc_on") > 0.5f;
        p.subOscLevel = getParam ("subosc_level");
        p.subOscOctave = (int) getParam ("sub_octave") == 0 ? -12 : -24;
        p.subOscRoute = (int) getParam ("subosc_route");
        p.subOscTable = getTableForChoice (shape == 0 ? 8 : (shape == 1 ? 6 : 10));
    }

    p.noiseLevel = getParam ("noise_level");

    p.filter1.type = juce::jlimit (0, FilterType::Count - 1, (int) getParam ("f1_type"));
    p.filter1.slope24 = getParam ("f1_slope") > 0.5f;
    p.filter1.cutoffHz = getParam ("f1_cutoff");
    p.filter1.resonance = getParam ("f1_reso");
    p.filter1.drive = getParam ("f1_drive");
    p.filter1.envAmount = getParam ("f1_env");
    p.filter1.keyTrack = getParam ("f1_keytrack");
    p.filter1.morph = getParam ("f1_morph");

    p.filter2.type = juce::jlimit (0, FilterType::Count - 1, (int) getParam ("f2_type"));
    p.filter2.slope24 = getParam ("f2_slope") > 0.5f;
    p.filter2.cutoffHz = getParam ("f2_cutoff");
    p.filter2.resonance = getParam ("f2_reso");
    p.filter2.drive = getParam ("f2_drive");
    p.filter2.envAmount = getParam ("f2_env");
    p.filter2.keyTrack = getParam ("f2_keytrack");
    p.filter2.morph = getParam ("f2_morph");

    p.filtersParallel = getParam ("filters_parallel") > 0.5f;
    p.filterBalance = getParam ("filter_balance");

    p.ampEnv = { getParam ("amp_attack"), getParam ("amp_decay"), getParam ("amp_sustain"),
                 getParam ("amp_release"), getParam ("amp_curve") };
    p.filterEnv = { getParam ("fe_attack"), getParam ("fe_decay"), getParam ("fe_sustain"),
                    getParam ("fe_release"), getParam ("fe_curve") };
    p.filter2Env = { getParam ("f2e_attack"), getParam ("f2e_decay"), getParam ("f2e_sustain"),
                     getParam ("f2e_release"), getParam ("f2e_curve") };
    p.modEnv = { getParam ("me_attack"), getParam ("me_decay"), getParam ("me_sustain"),
                 getParam ("me_release"), getParam ("me_curve") };
    p.env4 = { getParam ("e4_attack"), getParam ("e4_decay"), getParam ("e4_sustain"),
               getParam ("e4_release"), getParam ("e4_curve") };
    for (int env = 6; env <= 16; ++env)
    {
        const auto& ids = extraEnvIds[(size_t) (env - 6)];
        p.extraEnvs[(size_t) (env - 6)] = { getParam (ids.attack), getParam (ids.decay), getParam (ids.sustain),
                                            getParam (ids.release), getParam (ids.curve) };
        p.extraEnvVelocity[(size_t) (env - 6)] = getParam (ids.velocity);
    }
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        p.oscillators[(size_t) osc].ampEnv = juce::jlimit (0, OperatorEg::envelopeChoice, (int) getParam (oscAmpEnvIds[(size_t) osc]));

    // The Operator EG's settings as the DX7 voice its engine reads, only
    // while an enabled oscillator plays it.
    p.operatorEgUsed = false;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        p.operatorEgUsed = p.operatorEgUsed
                           || (p.oscillatorEnabled[(size_t) osc] && p.oscillators[(size_t) osc].ampEnv == OperatorEg::envelopeChoice);
    if (p.operatorEgUsed)
    {
        auto& voice = p.operatorEg;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            for (size_t field = 0; field < OperatorEg::operatorFields().size(); ++field)
            {
                const auto& info = OperatorEg::operatorFields()[field];
                // Operator k (1-6) lives at (6 - k) * 21 in the voice.
                voice[(size_t) ((5 - osc) * 21 + info.offset)]
                    = (std::uint8_t) juce::jlimit (0, info.maximum, juce::roundToInt (getParam (operatorEgIds[(size_t) osc][field])));
            }
        for (size_t field = 0; field < OperatorEg::voiceFields().size(); ++field)
        {
            const auto& info = OperatorEg::voiceFields()[field];
            voice[(size_t) info.offset] = (std::uint8_t) juce::jlimit (0, info.maximum, juce::roundToInt (getParam (operatorEgVoiceIds[field])));
        }
        voice[(size_t) OperatorEg::keyOffsetByte]
            = (std::uint8_t) (24 + juce::jlimit (-24, 24, juce::roundToInt (getParam (operatorEgKeyOffsetId))));
    }
    p.quality = juce::jlimit (0, 2, (int) getParam ("quality"));

    p.ampVelocity = getParam ("amp_velocity");
    p.filterVelocity = getParam ("filter_velocity");
    p.filter2EnvVelocity = getParam ("f2e_velocity");
    p.modEnvVelocity = getParam ("me_velocity");
    p.env4Velocity = getParam ("e4_velocity");
    p.glideTime = getParam ("glide");
    p.pitchBendRange = getParam ("bend_range");

    p.fmAmount = getParam ("fm_amount");
    p.fmFeedback = getParam ("fm_feedback");
    p.ringMod = getParam ("ring_mod");
    p.hardSync = getParam ("hard_sync") > 0.5f;
    p.drift = getParam ("drift");

    // FM matrix [source][target]; OSC 2 > 1 and OSC 1 feedback are the
    // original FM Amount and FM Feedback.
    p.fmMatrix[1][0] = p.fmAmount;
    p.fmMatrix[0][0] = p.fmFeedback;
    p.fmMatrix[0][1] = getParam ("fm_1to2");
    p.fmMatrix[0][2] = getParam ("fm_1to3");
    p.fmMatrix[1][2] = getParam ("fm_2to3");
    p.fmMatrix[2][0] = getParam ("fm_3to1");
    p.fmMatrix[2][1] = getParam ("fm_3to2");
    p.fmMatrix[1][1] = getParam ("fm_fb2");
    p.fmMatrix[2][2] = getParam ("fm_fb3");
    for (int source = 0; source < OscillatorIds::count; ++source)
        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            if (source < 3 && target < 3)
                continue;

            p.fmMatrix[source][target] = getParam (fmMatrixIds[(size_t) source][(size_t) target]);
        }
    p.fmMode = (int) getParam ("fm_mode");
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        p.oscOut[(size_t) osc] = getParam (oscCoreIds[(size_t) osc].out) > 0.5f;

    const auto fillStringParams = [this] (int oscIndex, VoiceParams::OscParams& osc)
    {
        const auto& ids = stringParamIds[(size_t) oscIndex];
        const auto mode = (int) getParam (ids[0]);
        osc.stringMode = mode == 1;
        osc.liveMode = mode == 4;
        osc.sampleMode = mode == 2 || mode == 3; // granular reads the sample too
        osc.granularMode = mode == 3;
        osc.stringExcite = (int) getParam (ids[1]);
        osc.stringDecay = getParam (ids[2]);
        osc.stringDamping = getParam (ids[3]);
        osc.stringSustain = getParam (ids[4]);
        osc.stringStiffness = getParam (ids[5]);
        osc.stringPickup = getParam (ids[6]);
        osc.stringExcitationPosition = getParam (ids[7]);
        osc.stringPickHardness = getParam (ids[8]);
        osc.stringPickPosition = getParam (ids[9]);
        osc.stringSlap = getParam (ids[10]) > 0.5f;
        const auto& extras = bowBuzzIds[(size_t) oscIndex];
        osc.bowPressure = getParam (extras[0]);
        osc.bowSpeed = getParam (extras[1]);
        osc.bridgeBuzz = getParam (extras[2]);
        osc.fretRattle = getParam (extras[3]);
        const auto& keys = keysParamIds[(size_t) oscIndex];
        osc.hammerHardness = getParam (keys[0]);
        osc.couple = getParam (keys[1]);
        osc.damper = getParam (keys[2]);
        osc.registerMap = getParam (keys[3]);
        const auto& electric = electricParamIds[(size_t) oscIndex];
        osc.epDistance = getParam (electric[0]);
        osc.epPosition = getParam (electric[1]);
        if (osc.stringExcite == 10)
        {
            osc.fbGain = getParam (feedbackParamIds[(size_t) oscIndex][0]);
            osc.fbDistance = getParam (feedbackParamIds[(size_t) oscIndex][1]);
        }
    };

    const auto fillSampleParams = [this] (int oscIndex, VoiceParams::OscParams& osc)
    {
        const auto& ids = sampleParamIds[(size_t) oscIndex];
        osc.sample = getSampleForOsc (oscIndex);
        osc.sampleTuned = getParam (ids[0]) > 0.5f;
        osc.sampleLoop = getParam (ids[1]) > 0.5f;
        osc.sampleReverse = getParam (ids[2]) > 0.5f;
        osc.sampleStart = getParam (ids[3]);
        osc.sampleEnd = getParam (ids[4]);
        osc.sampleFadeIn = getParam (ids[5]);
        osc.sampleFadeOut = getParam (ids[6]);

        const auto& grain = grainParamIds[(size_t) oscIndex];
        osc.grainSizeMs = getParam (grain[0]);
        osc.grainDensity = getParam (grain[1]);
        osc.grainSpray = getParam (grain[2]);
        osc.grainPitch = getParam (grain[3]);
        osc.grainSpread = getParam (grain[4]);
    };

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        fillStringParams (osc, p.oscillators[(size_t) osc]);
        fillSampleParams (osc, p.oscillators[(size_t) osc]);
    }

    p.voiceSpread = getParam ("voice_spread");
    p.stretch = getParam ("stretch");
    p.unisonRandom = getParam ("unison_random");
    p.filter1Fm = getParam ("f1_fm");
    p.filter2Fm = getParam ("f2_fm");

    p.resonatorOn = getParam ("res_on") > 0.5f;
    p.resonatorAmount = getParam ("res_amount");
    p.resonatorDecay = getParam ("res_decay");
    p.resonatorOffset = getParam ("res_offset");
    p.resonatorKeytrack = getParam ("res_keytrack");
    p.bodyType = (int) getParam ("body_type");
    p.bodyMaterial = getParam ("body_material");
    p.bodySize = getParam ("body_size");
    p.west.on = getParam ("west_on") > 0.5f;
    if (p.west.on)
    {
        p.west.position = (int) getParam ("west_pos");
        p.west.fold = getParam ("west_fold");
        p.west.symmetry = getParam ("west_sym");
        p.west.stages = (int) getParam ("west_stages");
        p.west.mode = (int) getParam ("west_mode");
        p.west.decay = getParam ("west_decay");
        p.west.resonance = getParam ("west_res");
        p.west.strike = getParam ("west_strike");
        p.west.open = getParam ("west_open");
        p.west.source = (int) getParam ("west_src"); // 0 strike, else the Mod::Source index
    }
    p.bodyCouplingMode = (int) getParam ("body_coupling_mode");
    p.bodyCoupling = getParam ("body_coupling");

    if (getParam ("mpe_mode") > 0.5f)
        p.pitchBendRange = 48.0f;

    p.clockSh = lfoBuffers.getReadPointer (4);
    p.mseg = lfoBuffers.getReadPointer (5);

    p.macros[0] = macroValue (0);
    p.macros[1] = macroValue (1);
    p.macros[2] = macroValue (2);
    p.macros[3] = macroValue (3);
    for (int m = 4; m < Mod::numMacros; ++m)
        p.macros[m] = macroValue (m);
    p.vectorX = vectorX.load();
    p.vectorY = vectorY.load();

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        p.lfoBuffers[lfo] = lfoBuffers.getReadPointer (lfoChannel (lfo));
        p.lfoBuffersB[lfo] = lfoBuffers.getReadPointer (lfoChannelB (lfo));
    }

    p.numModSlots = numActiveSlots;
    p.numActiveDestinations = 0;
    p.anyExtendedFmMods = false;

    for (int i = 0; i < numActiveSlots; ++i)
    {
        p.modSlots[i] = activeSlots[i];
        const auto destination = activeSlots[i].destination;

        if (! Mod::isExplicitDestination (destination))
        {
            p.anyExtendedFmMods = p.anyExtendedFmMods || Mod::extendedFmCellFor (destination) >= 0;
            continue;
        }

        auto seen = false;

        for (int d = 0; d < p.numActiveDestinations && ! seen; ++d)
            seen = p.activeDestinations[d] == destination;

        if (! seen)
            p.activeDestinations[p.numActiveDestinations++] = destination;
    }

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (p.oscillatorEnabled[(size_t) osc] && p.oscillators[(size_t) osc].ampEnv >= 5)
            p.extraEnvNeeded[(size_t) (p.oscillators[(size_t) osc].ampEnv - 5)] = true;
    for (int i = 0; i < numActiveSlots; ++i)
        for (const auto source : { activeSlots[i].source, activeSlots[i].aux })
            if (source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
                p.extraEnvNeeded[(size_t) ((int) source - (int) Mod::Source::Env6)] = true;

    // LFOs: retriggered ones run per voice (each note gets its own phase);
    // free-running ones are shared through the buffers rendered above.
    {
        const auto voiceRate = baseSampleRate * (double) oversamplingFactor.load();

        for (int lfo = 0; lfo < numLfos; ++lfo)
        {
            const auto& ids = lfoIds[(size_t) lfo];
            auto& lfoParams = p.lfos[lfo];

            auto rate = (double) getParam (ids.rate);

            if (getParam (ids.sync) > 0.5f)
                rate = (currentBpm.load() / 60.0) / getSyncDivisionBeats ((int) getParam (ids.div));

            for (int step = 0; step < 16; ++step)
                lfoStepValues[lfo][step] = getParam (ids.steps[(size_t) step]);

            lfoParams.keyTrack = getParam (ids.key) > 0.5f;
            lfoParams.perVoice = getParam (ids.retrig) > 0.5f || lfoParams.keyTrack;
            lfoParams.shape = (int) getParam (ids.shape);
            lfoParams.baseIncrement = juce::jlimit (0.001, 200.0, rate) / voiceRate;
            lfoParams.startPhase = getParam (ids.phase);
            lfoParams.physA = getParam (ids.physA);
            lfoParams.physB = getParam (ids.physB);
            lfoParams.kick = getParam (ids.kick) > 0.5f;
            lfoParams.steps = lfoStepValues[lfo];
            const auto isCurve = lfoParams.shape == curveShape;
            lfoParams.custom = isCurve ? activeLfoCurveTables[(size_t) lfo].data() : activeLfoCustom[(size_t) lfo].data();
            lfoParams.customSize = isCurve ? LfoCurve::tableSize : lfoDrawSteps;
            lfoParams.sim = readLfoSimSettings (lfo);
            lfoParams.smooth = getParam (ids.smooth);
            lfoParams.needsB = lfoRoutedB[(size_t) lfo];
            lfoParams.triggerCount = lfoTriggerCounts[(size_t) lfo];
        }
    }

    const auto fillWarpAndUnison = [this] (int oscIndex, VoiceParams::OscParams& osc)
    {
        const auto& ids = oscShapeIds[(size_t) oscIndex];
        osc.warpMode = (int) getParam (ids.warp);
        osc.warpAmount = getParam (ids.warpAmount);
        osc.unisonMode = (int) getParam (ids.unisonMode);
        osc.unisonBlend = getParam (ids.unisonBlend);
        osc.route = (int) getParam (ids.route);
    };

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        fillWarpAndUnison (osc, p.oscillators[(size_t) osc]);

    // M5 operators and M6 phase distortion.
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto& ids = operatorIds[(size_t) osc];
        auto& settings = p.oscillators[(size_t) osc];
        settings.tuneMode = juce::jlimit (0, OscTuning::Count - 1, (int) getParam (ids.tune));
        settings.ratio = OscTuning::snapRatio ((double) getParam (ids.ratio), (int) getParam (ids.snap));
        settings.fixedHz = (double) getParam (ids.fixedHz);
        settings.keyLevel = getParam (ids.keyLevel);
        settings.feedbackType = juce::jlimit (0, FmFeedback::Count - 1, (int) getParam (ids.feedbackType));
        settings.warpMode2 = Warp::modeForStageTwoChoice ((int) getParam (ids.warp2));
        settings.warpAmount2 = getParam (ids.warp2Amount);
        settings.pdEnv = juce::jlimit (0, 17, (int) getParam (ids.pdEnv));
        settings.pdEnvAmount = getParam (ids.pdEnvAmount);
        p.fmNoise[osc] = getParam (fmNoiseIds[(size_t) osc]);
    }
    p.fmNoiseColour = getParam ("fm_noise_color");

    for (int env = 0; env < 16; ++env)
    {
        const auto& ids = envelopeExtraIds[(size_t) env];
        const auto delay = getParam (ids.delay);
        const auto hold = getParam (ids.hold);
        auto& target = env == 0 ? p.ampEnv : env == 1 ? p.filterEnv : env == 2 ? p.filter2Env
                     : env == 3 ? p.modEnv : env == 4 ? p.env4 : p.extraEnvs[(size_t) (env - 5)];
        target.delay = delay;
        target.hold = hold;
        p.envKeyRate[(size_t) env] = getParam (ids.keyRate);
    }

    // Envelopes an oscillator uses for its warp (ENV 6-16 only run when
    // needed), and the MSEG as a per-voice envelope.
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        if (! p.oscillatorEnabled[(size_t) osc])
            continue;

        const auto& settings = p.oscillators[(size_t) osc];
        if (settings.pdEnv >= 6 && settings.pdEnv <= 16)
            p.extraEnvNeeded[(size_t) (settings.pdEnv - 6)] = true;
        p.msegEnvNeeded = p.msegEnvNeeded || settings.pdEnv == 17 || settings.ampEnv == 16;
    }

    if (p.msegEnvNeeded)
    {
        for (int point = 0; point < Mseg::numPoints; ++point)
        {
            p.msegShape.levels[point] = getParam (msegLevelIds[(size_t) point]);
            p.msegShape.times[point] = getParam (msegTimeIds[(size_t) point]);
        }

        p.msegShape.rateHz = (double) getParam ("mseg_rate");
        p.msegShape.loop = getParam ("mseg_loop") > 0.5f;
    }

    synth.setVoiceMode ((IlanaSynth::Mode) juce::jlimit (0, 2, (int) getParam ("voice_mode")),
                        (int) getParam ("poly_voices"), getParam ("glide_legato") > 0.5f);

    // Generative stage (scale snap, note spray) feeds the arpeggiator.
    {
        NoteSpray::Settings spray;
        spray.scale = (int) getParam ("gen_scale");
        spray.root = (int) getParam ("gen_root");
        spray.snapInput = getParam ("gen_snap") > 0.5f;
        spray.sprayOn = getParam ("spray_on") > 0.5f;
        spray.count = (int) getParam ("spray_count");
        spray.range = (int) getParam ("spray_range");
        spray.direction = (int) getParam ("spray_direction");
        spray.spreadSamples = (int) (getParam ("spray_spread") * 0.001 * currentSampleRate);
        spray.chance = getParam ("spray_chance");
        spray.velocityRandom = getParam ("spray_velocity");
        spray.strum = juce::jlimit (0, 2, (int) getParam ("spray_strum"));
        spray.strumSamples = (int) (getParam ("spray_strum_time") * 0.001 * currentSampleRate);
        noteSpray.process (midiMessages, generatedMidi, buffer.getNumSamples(), spray);
    }

    processArpeggiator (generatedMidi, buffer.getNumSamples(), midiForSynth);
    addEuclidExciterHits (midiForSynth, buffer.getNumSamples());
    processClip (midiForSynth, buffer.getNumSamples());

    p.exciterLevelMatch = exciterLevelMatch.load();

    // M7.5: the live input and its envelope for the voices; live grains
    // read the input's history instead of the sample.
    if (isEffectBuild)
    {
        p.liveInput = liveVoice.data();
        p.inputEnv = liveEnvVoice.data();
        p.inputToBody = getParam (inBodyRef);
        p.inputToStrings = getParam (inStringsRef);
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            auto& settings = p.oscillators[(size_t) osc];
            if (settings.granularMode && getParam (grainLiveIds[(size_t) osc]) > 0.5f)
            {
                settings.grainLive = true;
                settings.sample = &liveHistory;
                settings.liveWrite = liveHistoryWrite;
            }
        }
    }

    // Scala tuning: nullptr (off, or nothing loaded) keeps the 12-TET path.
    p.tuning = getParam (tuningOnRef) > 0.5f ? tuningState.acquireForAudio() : nullptr;
    // An MTS-ESP master in the session takes over (it retunes every client
    // live; new notes pick up its current table).
    if (mtsEsp.hasMaster())
    {
        for (int note = 0; note < 128; ++note)
            mtsTuning.setNote (note, mtsEsp.noteToFrequency (note), ! mtsEsp.shouldFilterNote (note));
        p.tuning = &mtsTuning;
    }
    synth.setTuning (p.tuning);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<Voice*> (synth.getVoice (i)))
            voice->setParams (p);

    if (factor > 1)
    {
        auto& oversampler = activeOversampler();
        juce::dsp::AudioBlock<float> baseBlock (buffer);
        auto upBlock = oversampler.processSamplesUp (baseBlock);
        float* upChannels[2] { upBlock.getChannelPointer (0),
                               upBlock.getNumChannels() > 1 ? upBlock.getChannelPointer (1)
                                                            : upBlock.getChannelPointer (0) };
        juce::AudioBuffer<float> upBuffer (upChannels, (int) upBlock.getNumChannels(), (int) upBlock.getNumSamples());

        scaledMidiBuffer.clear();

        for (const auto metadata : midiForSynth)
            scaledMidiBuffer.addEvent (metadata.getMessage(), metadata.samplePosition * factor);

        synth.renderNextBlock (upBuffer, scaledMidiBuffer, 0, upBuffer.getNumSamples());
        oversampler.processSamplesDown (baseBlock);
    }
    else
    {
        synth.renderNextBlock (buffer, midiForSynth, 0, buffer.getNumSamples());
    }

    {
        auto bestAmp = 0.0f;
        auto bestActivity = 0.0f;
        std::array<float, 11> extraEnvValues {};
        std::array<float, 16> envPositions;
        envPositions.fill (-1.0f);
        auto filterValue = 0.0f;
        auto filter2Value = 0.0f;
        auto modValue = 0.0f;
        auto env4Value = 0.0f;
        auto activeVoices = 0;
        std::array<float, OscillatorIds::count> samplePositions;
        samplePositions.fill (-1.0f);
        std::array<float, OscillatorIds::count> phases {};

        auto westLevel = 0.0f;
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<Voice*> (synth.getVoice (i)))
            {
                westLevel = juce::jmax (westLevel, voice->getWestGateLevel());
                const auto amp = voice->getLastAmpValue();
                const auto activity = voice->getLastLifetimeValue();

                if (activity > 0.001f)
                    ++activeVoices;

                if (activity > bestActivity)
                {
                    bestActivity = activity;
                    bestAmp = amp;
                    filterValue = voice->getLastFilterValue();
                    filter2Value = voice->getLastFilter2Value();
                    modValue = voice->getLastModValue();
                    env4Value = voice->getLastEnv4Value();
                    for (int osc = 0; osc < OscillatorIds::count; ++osc)
                    {
                        samplePositions[(size_t) osc] = voice->getLastSamplePosition (osc);
                        phases[(size_t) osc] = voice->getLastWavetablePhase (osc);
                    }
                    for (int env = 0; env < 11; ++env)
                        extraEnvValues[(size_t) env] = voice->getLastExtraEnvValue (env);
                    for (int env = 0; env < 16; ++env)
                        envPositions[(size_t) env] = voice->getEnvelopePosition (env);
                    monitorVelocity.store (voice->getVelocity());
                    monitorKeyTrack.store (voice->getKeyTrack());
                    monitorRandom.store (voice->getRandomValue());
                }
            }
        }
        westGateDisplay.store (westLevel);


        activeVoiceCount.store (activeVoices);
        envMonitorAmp.store (bestAmp);
        envMonitorFilter.store (filterValue);
        envMonitorFilter2.store (filter2Value);
        envMonitorMod.store (modValue);
        envMonitorEnv4.store (env4Value);
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            displaySamplePositions[(size_t) osc].store (samplePositions[(size_t) osc]);
            oscDisplayPhases[(size_t) osc].store (phases[(size_t) osc]);
        }
        for (int env = 0; env < 11; ++env)
            envMonitorExtra[(size_t) env].store (extraEnvValues[(size_t) env]);
        for (int env = 0; env < 16; ++env)
            envMonitorPositions[(size_t) env].store (envPositions[(size_t) env]);
    }

    processAcousticKeys (buffer, midiForSynth);

    const auto symOn = getParam ("sym_on") > 0.5f && getParam ("sym_amount") > 0.0f;
    if (! symOn && sympatheticWasOn)
        sympatheticStrings.reset();   // no stale ringing when switched back on
    sympatheticWasOn = symOn;

    if (symOn)
    {
        const char* const noteIds[] { "sym_note1", "sym_note2", "sym_note3", "sym_note4", "sym_note5", "sym_note6" };
        std::array<int, SympatheticStrings::maxStrings> notes {};
        for (int i = 0; i < SympatheticStrings::maxStrings; ++i)
            notes[(size_t) i] = (int) getParam (noteIds[i]);
        sympatheticStrings.setTuning ((int) getParam ("gen_scale"), (int) getParam ("gen_root"),
                                      getParam ("sym_manual") > 0.5f, notes);
        const auto count = (int) getParam ("sym_count");
        const auto amount = getParam ("sym_amount");
        const auto decay = getParam ("sym_decay");
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto input = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            const auto wet = sympatheticStrings.process (input, count, amount, decay);
            left[i] += wet;
            if (right != nullptr) right[i] += wet;
        }
    }

    // DC blockers (5 Hz one-pole high-passes) before and after the effects:
    // some warps, phase distortion and drives leave an offset (up to 0.4 of
    // full scale) that ate headroom, pushed the clipper and pumped the
    // dynamics, and asymmetric effects add their own.
    const auto blockDc = [this, &buffer] (int stage)
    {
        const auto pole = (float) std::exp (-juce::MathConstants<double>::twoPi * 5.0 / juce::jmax (1.0, baseSampleRate));
        for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            auto& state = dcBlock[stage][channel];
            auto x1 = state[0], y1 = state[1];
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto x = data[i];
                y1 = x - x1 + pole * y1;
                x1 = x;
                data[i] = y1;
            }
            state[0] = x1;
            state[1] = std::isfinite (y1) && std::abs (y1) > 1.0e-20f ? y1 : 0.0f;
        }
    };

    blockDc (0);
    processEffects (buffer);
    blockDc (1);

    // MASTER plus the preset's own level (output_trim, 0 unless a factory
    // preset set it), summed in dB.
    buffer.applyGain (juce::Decibels::decibelsToGain (getParam (masterRef) + getRawParam (outputTrimRef)));

    if (getParam ("master_clip") > 0.5f)
    {
        const auto clipGain = juce::Decibels::decibelsToGain (getParam ("master_clip_gain"));

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = std::tanh (data[i] * clipGain);
        }
    }

    // After a patch change: ease from where the old patch's output stopped
    // (a 2 ms decay) instead of stepping from it to the new patch.
    for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        const auto numSamples = buffer.getNumSamples();
        if (declick[channel] != 0.0f)
        {
            const auto decay = std::exp (-1.0f / (0.002f * (float) juce::jmax (1.0, baseSampleRate)));
            for (int i = 0; i < numSamples; ++i)
            {
                data[i] += declick[channel];
                declick[channel] *= decay;
            }
            if (std::abs (declick[channel]) < 1.0e-6f)
                declick[channel] = 0.0f;
        }
        lastOutput[channel] = numSamples > 0 ? data[numSamples - 1] : lastOutput[channel];
    }

    // M7.5 DRY: the untouched input back in, delayed by the oversamplers'
    // latency so it lines up with the wet signal.
    if (liveInputSamples > 0)
        delayLiveDry (juce::jmin (liveInputSamples, buffer.getNumSamples()));

    if (liveInputSamples > 0)
        if (const auto dry = getParam (inDryRef); dry > 0.0f)
            for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
                buffer.addFrom (channel, 0, liveDry, juce::jmin (channel, liveDry.getNumChannels() - 1), 0,
                                juce::jmin (liveInputSamples, buffer.getNumSamples()), dry);

    {
        const juce::SpinLock::ScopedLockType lock (scopeLock);
        auto writePosition = scopeWritePos.load();

        const auto* leftData = buffer.getReadPointer (0);
        const auto* rightData = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : leftData;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            scopeLeft[(size_t) writePosition] = leftData[i];
            scopeRight[(size_t) writePosition] = rightData[i];
            writePosition = (writePosition + 1) % scopeSize;
        }

        scopeWritePos.store (writePosition);
    }

    // Output peaks for the meter; the editor takes them when it reads.
    for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
    {
        const auto peak = buffer.getMagnitude (channel, 0, buffer.getNumSamples());
        auto& held = outputPeaks[(size_t) channel];

        if (peak > held.load())
            held.store (peak);
    }
    outputLevelDisplay.store (buffer.getMagnitude (0, buffer.getNumSamples()));

    if (activeVoiceCount.load() > 0 || outputLevelDisplay.load() > 1.0e-5f || inputLevelDisplay.load() > 1.0e-5f)
        ++liveEpoch;

    const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
    const auto availableSeconds = (double) buffer.getNumSamples() / juce::jmax (1.0, currentSampleRate);
    const auto usage = (float) juce::jlimit (0.0, 1.0, elapsedSeconds / juce::jmax (1.0e-6, availableSeconds));

    cpuUsage.store (cpuUsage.load() * 0.92f + usage * 0.08f);
}

void IlanaSynthAudioProcessor::copyScopeData (float* left, float* right, int numSamples) const
{
    numSamples = juce::jlimit (0, scopeSize, numSamples);

    const juce::SpinLock::ScopedLockType lock (scopeLock);
    auto start = (scopeWritePos.load() - numSamples + scopeSize) % scopeSize;

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = scopeLeft[(size_t) start];
        right[i] = scopeRight[(size_t) start];
        start = (start + 1) % scopeSize;
    }
}

const Wavetable* IlanaSynthAudioProcessor::getTableForChoice (int choiceIndex) const
{
    const auto& factory = FactoryTables::get().tables;
    const auto factoryCount = (int) factory.size();

    if (choiceIndex < factoryCount)
        return factory[(size_t) juce::jlimit (0, factoryCount - 1, choiceIndex)].get();

    const auto slot = juce::jlimit (0, numUserSlots - 1, choiceIndex - factoryCount);
    const juce::SpinLock::ScopedLockType lock (tableLock);
    return userTables[(size_t) slot].get();
}

void IlanaSynthAudioProcessor::flushAsyncUpdates()
{
    cancelPendingUpdate();
    handleAsyncUpdate();
}

const SampleData* IlanaSynthAudioProcessor::getSampleForOsc (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return nullptr;

    const auto factoryIndex = (int) getRawParam (sampleFactoryIds[(size_t) oscIndex]);

    if (factoryIndex > 0 && factoryIndex <= (int) factorySamples.size())
        return factorySamples[(size_t) (factoryIndex - 1)].get();

    const SampleData* user = nullptr;

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        user = sampleSlots[(size_t) oscIndex].get();
    }

    // Granular with nothing loaded: grains from the vocal sample rather than silence.
    if (user == nullptr && ! factorySamples.empty())
        if ((int) getRawParam (stringParamIds[(size_t) oscIndex][0]) == 3) // osc mode: Granular
            return factorySamples[(size_t) juce::jmin (1, (int) factorySamples.size() - 1)].get();

    return user;
}

bool IlanaSynthAudioProcessor::loadUserSample (int oscIndex, const juce::File& file)
{
    ++sampleEpoch;
    ++dataEpoch; // what the editor draws changes
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;

    // SoundFont and SFZ instruments: a multisample, its regions picked per note.
    if (MultiSample::isMultiSampleFile (file))
    {
        juce::String error;
        auto multi = MultiSample::load (file, error);
        if (multi == nullptr)
            return false;
        setUserSample (oscIndex, std::move (multi), file.getFullPathName());
        return true;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 1)
        return false;

    const auto maxSamples = (juce::int64) (reader->sampleRate * 120.0);
    const auto numSamples = (int) juce::jmin (reader->lengthInSamples, maxSamples);

    if (numSamples <= 1)
        return false;

    const auto numChannels = juce::jlimit (1, 2, (int) reader->numChannels);

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    data->name = file.getFileNameWithoutExtension();
    data->buffer.setSize (numChannels, numSamples);

    if (! reader->read (&data->buffer, 0, numSamples, 0, true, true))
        return false;

    setUserSample (oscIndex, std::move (data), file.getFullPathName());
    return true;
}

void IlanaSynthAudioProcessor::setUserSample (int oscIndex, std::shared_ptr<SampleData> data, const juce::String& path)
{
    ++sampleEpoch;
    ++dataEpoch; // what the editor draws changes
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return;

    const auto embedded = data != nullptr && path.isEmpty() ? data : nullptr;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        auto& retired = retiredSamples[(size_t) oscIndex];
        auto& index = retiredIndex[(size_t) oscIndex];
        retired[(size_t) index] = std::move (sampleSlots[(size_t) oscIndex]);
        index = (index + 1) % (int) retired.size();
        sampleSlots[(size_t) oscIndex] = std::move (data);
    }

    const juce::SpinLock::ScopedLockType lock (stateLock);
    samplePaths[(size_t) oscIndex] = path;
    embeddedSamples[(size_t) oscIndex] = embedded;
    // The latest sample wins over a clear or reload still queued.
    pendingSampleClear[(size_t) oscIndex] = false;
    pendingSamplePaths[(size_t) oscIndex].clear();
}

bool IlanaSynthAudioProcessor::isSampleEmbedded (int oscIndex) const
{
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return embeddedSamples[(size_t) oscIndex] != nullptr;
}

bool IlanaSynthAudioProcessor::loadUserWavetable (int slot, const juce::File& file, Wavetable::LoadMode mode)
{
    ++dataEpoch; // what the editor draws changes
    if (slot < 0 || slot >= numUserSlots)
        return false;

    WavetableDoc doc;
    if (! WavetableDoc::loadFromFile (file, doc, mode))
        return false;

    return setUserTable (slot, doc);
}

void IlanaSynthAudioProcessor::swapUserTable (int slot, std::shared_ptr<Wavetable> table)
{
    ++dataEpoch; // what the editor draws changes
    const juce::SpinLock::ScopedLockType lock (tableLock);
    auto& index = retiredTableIndex[(size_t) slot];
    retiredTables[(size_t) (slot * 3 + index)] = std::move (userTables[(size_t) slot]);
    index = (index + 1) % 3;
    userTables[(size_t) slot] = std::move (table);
}

bool IlanaSynthAudioProcessor::setUserTable (int slot, const WavetableDoc& doc)
{
    ++dataEpoch; // what the editor draws changes
    if (slot < 0 || slot >= numUserSlots || doc.frames.empty())
        return false;

    auto table = std::make_shared<Wavetable>();
    table->buildFromFrames (doc.frames);
    table->setName (doc.name);
    swapUserTable (slot, std::move (table));

    const juce::SpinLock::ScopedLockType lock (stateLock);
    userTableDocs[(size_t) slot] = std::make_shared<const WavetableDoc> (doc);
    userTablePaths[(size_t) slot] = doc.sourcePath;
    userTableModes[(size_t) slot] = doc.sourceMode;
    return true;
}

// A slot's default is a factory table. It shares the factory copy (the
// factory tables live for the whole program), so 16 slots cost nothing.
void IlanaSynthAudioProcessor::resetUserTableToDefault (int slot)
{
    ++dataEpoch; // what the editor draws changes
    if (slot < 0 || slot >= numUserSlots)
        return;

    const auto& factory = FactoryTables::get().tables;
    swapUserTable (slot, std::shared_ptr<Wavetable> (std::shared_ptr<Wavetable>(),
                                                     factory[(size_t) (slot % (int) factory.size())].get()));

    const juce::SpinLock::ScopedLockType lock (stateLock);
    userTablePaths[(size_t) slot].clear();
    userTableDocs[(size_t) slot].reset();
}

WavetableDoc IlanaSynthAudioProcessor::getUserTableDoc (int slot) const
{
    slot = juce::jlimit (0, numUserSlots - 1, slot);
    std::shared_ptr<const WavetableDoc> doc;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        doc = userTableDocs[(size_t) slot];
    }
    if (doc != nullptr)
        return *doc;
    auto fallback = WavetableDoc::fromFactory (slot % TableFactory::getNumFactoryTables());
    fallback.name = "User " + juce::String (slot + 1);
    return fallback;
}

bool IlanaSynthAudioProcessor::isUserSlotEdited (int slot) const
{
    if (slot < 0 || slot >= numUserSlots)
        return false;
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return userTableDocs[(size_t) slot] != nullptr;
}

int IlanaSynthAudioProcessor::findFreeUserSlot() const
{
    const auto factoryCount = TableFactory::getNumFactoryTables();
    std::array<bool, (size_t) numUserSlots> used {};
    for (const auto* prefix : OscillatorIds::prefixes)
        if (const auto* value = apvts.getRawParameterValue (juce::String (prefix) + "_table"))
        {
            const auto slot = juce::roundToInt (value->load()) - factoryCount;
            if (slot >= 0 && slot < numUserSlots)
                used[(size_t) slot] = true;
        }
    for (int slot = 0; slot < numUserSlots; ++slot)
        if (! used[(size_t) slot] && ! isUserSlotEdited (slot))
            return slot;
    return -1;
}

juce::String IlanaSynthAudioProcessor::getTableNotice() const
{
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return tableNotice;
}

void IlanaSynthAudioProcessor::clearTableNotice()
{
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        tableNotice.clear();
    }
    ++tableNoticeVersion;
}

