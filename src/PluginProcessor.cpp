#include "PluginProcessor.h"

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

#include "Presets.h"
#include "gui/ParamInfo.h"
#include "dsp/LfoShape.h"
#include "dsp/Modulation.h"
#include "dsp/SampleFactory.h"
#include "dsp/TableFactory.h"
#include "dsp/FmAlgorithms.h"
#include "dsp/Voice.h"

#include <cmath>
#include <memory>
#include <set>
#include <vector>

namespace
{
// Parameter prefixes of ENV 1..16: the five named envelopes keep their ids.
juce::String envelopePrefix (int env)
{
    static const char* const named[] { "amp", "fe", "f2e", "me", "e4" };
    return env < 5 ? juce::String (named[env]) : "env" + juce::String (env + 1);
}

class WavetableSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

struct FactoryTables
{
    std::vector<std::unique_ptr<Wavetable>> tables;

    FactoryTables()
    {
        const auto names = TableFactory::getFactoryTableNames();

        for (int i = 0; i < TableFactory::getNumFactoryTables(); ++i)
        {
            auto table = std::make_unique<Wavetable>();
            table->setName (names[i]);
            table->buildFromFrames (TableFactory::generate (i));
            tables.push_back (std::move (table));
        }
    }

    static const FactoryTables& get()
    {
        static FactoryTables instance;
        return instance;
    }
};

constexpr int numVoices = 16;

juce::StringArray getSyncDivisionNames()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
}

double getSyncDivisionBeats (int index)
{
    static const double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                    2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };

    return beats[juce::jlimit (0, 10, index)];
}

float lfoValue (int shape, double phase, float sampleHold, const float* customShape = nullptr,
                const float* curveTable = nullptr)
{
    switch (shape)
    {
        case IlanaSynthAudioProcessor::curveShape:
        {
            if (curveTable == nullptr)
                return 0.0f;

            const auto position = phase * (double) LfoCurve::tableSize;
            const auto index = (int) position % LfoCurve::tableSize;
            const auto next = (index + 1) % LfoCurve::tableSize;
            const auto frac = (float) (position - std::floor (position));
            return curveTable[index] + (curveTable[next] - curveTable[index]) * frac;
        }

        case 0: return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        case 1: return (float) (1.0 - 4.0 * std::abs (phase - 0.5));
        case 2: return (float) (2.0 * phase - 1.0);
        case 3: return (float) (1.0 - 2.0 * phase);
        case 4: return phase < 0.5 ? 1.0f : -1.0f;
        case 5: return sampleHold;
        case 6:
        {
            if (customShape == nullptr)
                return 0.0f;

            const auto position = phase * 64.0;
            const auto index = (int) position % 64;
            const auto next = (index + 1) % 64;
            const auto frac = (float) (position - std::floor (position));

            return customShape[index] + (customShape[next] - customShape[index]) * frac;
        }
        default: return 0.0f;
    }
}

float foldTriangle (float value)
{
    value = std::fmod (value + 1.0f, 4.0f);

    if (value < 0.0f)
        value += 4.0f;

    return value <= 2.0f ? value - 1.0f : 3.0f - value;
}

// Keeps extreme module stacks from overflowing into Inf/NaN and latching
// into delay lines or reverb tanks. 24 dB above full scale, inaudible normally.
void sanitiseBuffer (juce::AudioBuffer<float>& buffer)
{
    constexpr auto limit = 16.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = data[i];
            data[i] = std::isfinite (value) ? juce::jlimit (-limit, limit, value) : 0.0f;
        }
    }
}

juce::StringArray getOscTableChoices()
{
    auto choices = TableFactory::getFactoryTableNames();

    for (int i = 1; i <= IlanaSynthAudioProcessor::numUserSlots; ++i)
        choices.add ("User " + juce::String (i));

    return choices;
}
} // namespace

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
    }

    arpHeldNotes.ensureStorageAllocated (128);
    arpChordActive.ensureStorageAllocated (128);
    arpChordNotes.ensureStorageAllocated (128);
    spectralCache = std::make_unique<SpectralCache> ([] (int index) { return FactoryTables::get().tables[(size_t) index].get(); },
                                                     TableFactory::getNumFactoryTables());

    for (auto& value : modDisplayValues)
        value.store (0.0f);

    for (auto& value : displaySamplePositions)
        value.store (-1.0f);

    for (int i = 0; i < 4; ++i)
        macroCc[i].store (20 + i);

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
        sampleFactoryIds[(size_t) osc] = prefix + "_sample_factory";

        for (int target = 0; target < OscillatorIds::count; ++target)
            fmMatrixIds[(size_t) osc][(size_t) target] = fmRouteId (osc, target);
    }

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

    for (int slot = 0; slot < numFxSlots; ++slot)
    {
        const auto prefix = "fx_slot" + juce::String (slot + 1);
        fxSlotIds[(size_t) slot] = { prefix, prefix + "_bypass", prefix + "_solo", prefix + "_mix" };
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

juce::AudioProcessorValueTreeState::ParameterLayout IlanaSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto addFloat = [&layout] (const juce::String& id, const juce::String& name, float min, float max,
                                     float def, float skew = 1.0f, float interval = 0.0f)
    {
        const auto step = interval > 0.0f ? interval : (max - min) / 1000.0f;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (min, max, step, skew), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [id] (float value, int) { return describeValue (id, value); })));
    };

    // Integer parameters use the same value text as the others (units, note
    // names, "3 strings"), which ParamInfo already describes for them.
    const auto addInt = [&layout] (const juce::String& id, const juce::String& name, int min, int max, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { id, 1 }, name, min, max, def,
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([id] (int value, int) { return describeValue (id, (float) value); })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    // Note names ("C#3", C3 = 60) as well as plain numbers.
                    const auto trimmed = text.trim().toUpperCase();
                    const auto letter = trimmed.isNotEmpty() ? trimmed[0] : 0;
                    const int offsets[] { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G

                    if (letter >= 'A' && letter <= 'G')
                    {
                        auto note = offsets[letter - 'A'];
                        auto rest = trimmed.substring (1);

                        if (rest.startsWithChar ('#')) { ++note; rest = rest.substring (1); }
                        else if (rest.startsWithChar ('B')) { --note; rest = rest.substring (1); }

                        return note + 12 * (rest.getIntValue() + 2);
                    }

                    return trimmed.getIntValue();
                })));
    };

    const auto addBool = [&layout] (const juce::String& id, const juce::String& name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, def));
    };

    const auto addChoice = [&layout] (const juce::String& id, const juce::String& name,
                                      const juce::StringArray& choices, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, choices, def));
    };

    // OSC 1 and 2 retain their original parameter order and defaults.
    for (int osc = 0; osc < 2; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        const auto name = "Osc" + juce::String (osc + 1);
        const auto first = osc == 0;
        const auto id = [&prefix] (const char* suffix) { return prefix + "_" + suffix; };

        addBool (id ("on"), name + " On", first);
        addChoice (id ("table"), name + " Table", getOscTableChoices(), first ? 7 : 1);
        addFloat (id ("frame"), name + " Frame", 0.0f, 1.0f, first ? 0.35f : 0.0f);
        addFloat (id ("level"), name + " Level", 0.0f, 1.0f, first ? 0.8f : 0.6f);
        addFloat (id ("pan"), name + " Pan", -1.0f, 1.0f, 0.0f);
        addInt (id ("semi"), name + " Semi", -24, 24, 0);
        addFloat (id ("fine"), name + " Fine", -100.0f, 100.0f, first ? 0.0f : -7.0f, 1.0f, 1.0f);
        addInt (id ("unison"), name + " Unison", 1, VoiceParams::maxUnison, 1);
        addFloat (id ("detune"), name + " Detune", 0.0f, 50.0f, 15.0f);
        addFloat (id ("spread"), name + " Spread", 0.0f, 1.0f, 0.5f);
        addChoice (id ("warp"), name + " Warp", Warp::getNames(), 0);
        addFloat (id ("warp_amt"), name + " Warp Amount", 0.0f, 1.0f, 0.0f);
        addChoice (id ("spectral"), name + " Spectral Warp", SpectralWarp::getNames(), 0);
        addFloat (id ("spectral_amt"), name + " Spectral Amount", 0.0f, 1.0f, 0.5f);
        addChoice (id ("uni_mode"), name + " Unison Mode", UnisonMode::getNames(), 0);
        addFloat (id ("uni_blend"), name + " Unison Blend", 0.0f, 1.0f, 1.0f);
        addChoice (id ("route"), name + " Filter Route", FilterRoute::getNames(), 0);
    }

    // OSC 3: a full oscillator like OSC 1 and 2 (its ids keep the old "sub_"
    // prefix so saved patches still line up; see migrateLegacyOsc3).
    addBool ("sub_on", "Osc3 On", false);
    addFloat ("sub_level", "Osc3 Level", 0.0f, 1.0f, 0.6f);

    // The dedicated sub oscillator.
    addBool ("subosc_on", "Sub On", false);
    addFloat ("subosc_level", "Sub Level", 0.0f, 1.0f, 0.5f);
    addChoice ("sub_shape", "Sub Shape", { "Sine", "Square", "Saw" }, 0);
    addChoice ("sub_octave", "Sub Octave", { "-1 Oct", "-2 Oct" }, 0);
    addChoice ("subosc_route", "Sub + Noise Route", FilterRoute::getNames(), 0);
    addChoice ("sub_mode", "Osc3 Mode", { "Wavetable", "Physical", "Sample", "Granular", "Live" }, 0);
    addChoice ("osc1_sample_factory", "Osc1 Sample Source",
               { "User File", "Metal Hit", "Vocal Ah", "Sub Tone", "Vinyl Loop", "Noise Rise" }, 0);
    addChoice ("osc2_sample_factory", "Osc2 Sample Source",
               { "User File", "Metal Hit", "Vocal Ah", "Sub Tone", "Vinyl Loop", "Noise Rise" }, 0);
    addChoice ("sub_sample_factory", "Osc3 Sample Source",
               { "User File", "Metal Hit", "Vocal Ah", "Sub Tone", "Vinyl Loop", "Noise Rise" }, 0);
    addChoice ("sub_table", "Osc3 Table", getOscTableChoices(), 0);
    addFloat ("sub_frame", "Osc3 Frame", 0.0f, 1.0f, 0.0f);
    addFloat ("sub_pan", "Osc3 Pan", -1.0f, 1.0f, 0.0f);
    addInt ("sub_semi", "Osc3 Semi", -24, 24, 0);
    addFloat ("sub_fine", "Osc3 Fine", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f);
    addInt ("sub_unison", "Osc3 Unison", 1, VoiceParams::maxUnison, 1);
    addFloat ("sub_detune", "Osc3 Detune", 0.0f, 50.0f, 12.0f);
    addFloat ("sub_spread", "Osc3 Spread", 0.0f, 1.0f, 0.0f);
    addChoice ("sub_warp", "Osc3 Warp", Warp::getNames(), 0);
    addFloat ("sub_warp_amt", "Osc3 Warp Amount", 0.0f, 1.0f, 0.0f);
    addChoice ("sub_spectral", "Osc3 Spectral Warp", SpectralWarp::getNames(), 0);
    addFloat ("sub_spectral_amt", "Osc3 Spectral Amount", 0.0f, 1.0f, 0.5f);
    addChoice ("sub_uni_mode", "Osc3 Unison Mode", UnisonMode::getNames(), 0);
    addFloat ("sub_uni_blend", "Osc3 Unison Blend", 0.0f, 1.0f, 1.0f);
    addChoice ("sub_route", "Osc3 Filter Route", FilterRoute::getNames(), 0);
    addChoice ("sub_excite", "Osc3 Excite", { "Burst", "Noise", "Saw", "Pulse", "Bow", "Hammer (classic)", "Osc In", "Tine", "Reed", "Piano" }, 0);
    addFloat ("sub_string_decay", "Osc3 String Decay", 0.0f, 1.0f, 0.75f);
    addFloat ("sub_string_damp", "Osc3 String Damp", 0.0f, 1.0f, 0.35f);
    addFloat ("sub_string_sustain", "Osc3 String Sustain", 0.0f, 1.0f, 0.0f);
    addFloat ("sub_string_stiffness", "Osc3 Stiffness", 0.0f, 1.0f, 0.0f);
    addFloat ("sub_string_pickup", "Osc3 Pickup Position", 0.0f, 1.0f, 0.0f);
    addFloat ("sub_string_excite_pos", "Osc3 Excitation Position", 0.0f, 1.0f, 0.0f);
    addFloat ("sub_string_pick_hardness", "Osc3 Pick Hardness", 0.0f, 1.0f, 1.0f);
    addFloat ("sub_string_pick_pos", "Osc3 Pick Position", 0.0f, 1.0f, 0.0f);
    addBool ("sub_string_slap", "Osc3 Slap", false);
    addChoice ("sub_chord", "Osc3 Chord", { "Off", "Octave", "Fifth", "Power", "Major", "Minor", "Sus4" }, 0);
    addFloat ("noise_level", "Noise Level", 0.0f, 1.0f, 0.0f);

    // Filter 1
    addChoice ("f1_type", "F1 Type", FilterType::getNames(), 0);
    addChoice ("f1_slope", "F1 Slope", { "12 dB", "24 dB" }, 0);
    addFloat ("f1_cutoff", "F1 Cutoff", 20.0f, 20000.0f, 12000.0f, 0.25f);
    addFloat ("f1_reso", "F1 Reso", 0.0f, 1.0f, 0.25f);
    addFloat ("f1_drive", "F1 Drive", 1.0f, 10.0f, 1.5f);
    addFloat ("f1_env", "F1 Env", -5.0f, 5.0f, 1.5f);
    addFloat ("f1_keytrack", "F1 Key Track", -1.0f, 1.0f, 0.0f);
    addFloat ("f1_morph", "F1 Morph", 0.0f, 1.0f, 0.0f);

    // Filter 2
    addChoice ("f2_type", "F2 Type", FilterType::getNames(), 0);
    addChoice ("f2_slope", "F2 Slope", { "12 dB", "24 dB" }, 0);
    addFloat ("f2_cutoff", "F2 Cutoff", 20.0f, 20000.0f, 20000.0f, 0.25f);
    addFloat ("f2_reso", "F2 Reso", 0.0f, 1.0f, 0.0f);
    addFloat ("f2_drive", "F2 Drive", 1.0f, 10.0f, 1.0f);
    addFloat ("f2_env", "F2 Env", -5.0f, 5.0f, 0.0f);
    addFloat ("f2_keytrack", "F2 Key Track", -1.0f, 1.0f, 0.0f);
    addFloat ("f2_morph", "F2 Morph", 0.0f, 1.0f, 0.0f);
    addBool ("filters_parallel", "Filters Parallel", false);
    addFloat ("filter_balance", "Filter Balance", -1.0f, 1.0f, 0.0f);

    // Amp envelope
    addFloat ("amp_attack", "Amp Attack", 0.001f, 5.0f, 0.005f, 0.35f);
    addFloat ("amp_decay", "Amp Decay", 0.005f, 5.0f, 0.3f, 0.35f);
    addFloat ("amp_sustain", "Amp Sustain", 0.0f, 1.0f, 0.8f);
    addFloat ("amp_release", "Amp Release", 0.005f, 10.0f, 0.25f, 0.35f);

    // Filter envelope
    addFloat ("fe_attack", "FE Attack", 0.001f, 5.0f, 0.001f, 0.35f);
    addFloat ("fe_decay", "FE Decay", 0.005f, 5.0f, 0.5f, 0.35f);
    addFloat ("fe_sustain", "FE Sustain", 0.0f, 1.0f, 0.3f);
    addFloat ("fe_release", "FE Release", 0.005f, 10.0f, 0.3f, 0.35f);

    // Mod envelope
    addFloat ("me_attack", "ME Attack", 0.001f, 5.0f, 0.05f, 0.35f);
    addFloat ("me_decay", "ME Decay", 0.005f, 5.0f, 0.4f, 0.35f);
    addFloat ("me_sustain", "ME Sustain", 0.0f, 1.0f, 0.5f);
    addFloat ("me_release", "ME Release", 0.005f, 10.0f, 0.3f, 0.35f);

    // Filter 2 envelope
    addFloat ("f2e_attack", "F2E Attack", 0.001f, 5.0f, 0.01f, 0.35f);
    addFloat ("f2e_decay", "F2E Decay", 0.005f, 5.0f, 0.4f, 0.35f);
    addFloat ("f2e_sustain", "F2E Sustain", 0.0f, 1.0f, 0.4f);
    addFloat ("f2e_release", "F2E Release", 0.005f, 10.0f, 0.3f, 0.35f);
    addFloat ("f2e_curve", "F2E Tension", -1.0f, 1.0f, 0.0f);

    // Envelope 4
    addFloat ("e4_attack", "E4 Attack", 0.001f, 5.0f, 0.05f, 0.35f);
    addFloat ("e4_decay", "E4 Decay", 0.005f, 5.0f, 0.4f, 0.35f);
    addFloat ("e4_sustain", "E4 Sustain", 0.0f, 1.0f, 0.5f);
    addFloat ("e4_release", "E4 Release", 0.005f, 10.0f, 0.3f, 0.35f);
    addFloat ("e4_curve", "E4 Tension", -1.0f, 1.0f, 0.0f);

    addFloat ("amp_curve", "Amp Tension", -1.0f, 1.0f, 0.0f);
    addFloat ("fe_curve", "FE Tension", -1.0f, 1.0f, 0.0f);
    addFloat ("me_curve", "ME Tension", -1.0f, 1.0f, 0.0f);

    // Global
    addFloat ("amp_velocity", "Amp Velocity", 0.0f, 1.0f, 0.3f);
    addFloat ("filter_velocity", "Filter Velocity", 0.0f, 1.0f, 0.5f);
    addFloat ("glide", "Glide", 0.0f, 2.0f, 0.0f, 0.35f);
    addFloat ("bend_range", "Bend Range", 0.0f, 24.0f, 2.0f, 1.0f, 1.0f);
    addFloat ("drift", "Drift", 0.0f, 1.0f, 0.0f);

    // Oscillator cross-modulation
    addFloat ("fm_amount", "FM Osc2 > Osc1", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_feedback", "FM Osc1 Feedback", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_1to2", "FM Osc1 > Osc2", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_1to3", "FM Osc1 > Osc3", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_2to3", "FM Osc2 > Osc3", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_3to1", "FM Osc3 > Osc1", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_3to2", "FM Osc3 > Osc2", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_fb2", "FM Osc2 Feedback", 0.0f, 1.0f, 0.0f);
    addFloat ("fm_fb3", "FM Osc3 Feedback", 0.0f, 1.0f, 0.0f);
    addChoice ("fm_mode", "FM Mode", { "Phase", "Through-Zero", "Exponential" }, 0);
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        addBool (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_out",
                 "Osc" + juce::String (osc + 1) + " Output", true);
    addFloat ("ring_mod", "Ring Mod", 0.0f, 1.0f, 0.0f);
    addBool ("hard_sync", "Hard Sync", false);
    addFloat ("f1_fm", "F1 Audio FM", -1.0f, 1.0f, 0.0f);
    addFloat ("f2_fm", "F2 Audio FM", -1.0f, 1.0f, 0.0f);

    // Voice behaviour
    addChoice ("osc1_chord", "Osc1 Chord", { "Off", "Octave", "Fifth", "Power", "Major", "Minor", "Sus4" }, 0);
    addChoice ("osc2_chord", "Osc2 Chord", { "Off", "Octave", "Fifth", "Power", "Major", "Minor", "Sus4" }, 0);
    addFloat ("voice_spread", "Voice Spread", 0.0f, 1.0f, 0.0f);
    addFloat ("unison_random", "Unison Random", 0.0f, 1.0f, 0.0f);
    addBool ("mpe_mode", "MPE Mode", false);
    addChoice ("voice_mode", "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
    addInt ("poly_voices", "Poly Voices", 1, numVoices, numVoices);
    addBool ("glide_legato", "Glide Legato Only", false);

    // Resonator bank
    addBool ("res_on", "Resonator On", false);
    addFloat ("res_amount", "Res Amount", 0.0f, 1.0f, 0.5f);
    addFloat ("res_decay", "Res Decay", 0.0f, 1.0f, 0.7f);
    addFloat ("res_offset", "Res Offset", -12.0f, 12.0f, 0.0f, 1.0f, 1.0f);
    addFloat ("res_keytrack", "Res Key Track", 0.0f, 1.0f, 1.0f);
    addChoice ("body_type", "Body Type", { "Classic", "Bar", "Plate", "Bell", "Shell" }, 0);
    addFloat ("body_material", "Body Material", 0.0f, 1.0f, 0.0f);
    addFloat ("body_size", "Body Size", 0.0f, 1.0f, 0.5f);
    addChoice ("body_coupling_mode", "Body Coupling", { "Off", "String to body", "Body to string", "Strings" }, 0);
    addFloat ("body_coupling", "Body Coupling Amount", 0.0f, 1.0f, 0.0f);

    // Clocked sample and hold + MSEG
    addChoice ("clock_div", "Clock S&H Div", getSyncDivisionNames(), 3);
    addBool ("mseg_loop", "MSEG Loop", true);
    addFloat ("mseg_rate", "MSEG Rate", 0.02f, 8.0f, 0.5f, 0.4f);

    for (int point = 1; point <= 4; ++point)
    {
        const float levels[4] { 0.0f, 1.0f, 0.0f, -1.0f };
        addFloat ("mseg_level" + juce::String (point), "MSEG Level " + juce::String (point),
                  -1.0f, 1.0f, levels[point - 1]);
        addFloat ("mseg_time" + juce::String (point), "MSEG Time " + juce::String (point),
                  0.05f, 1.0f, 0.25f);
    }

    // String (Karplus-Strong) oscillators
    for (int osc = 1; osc <= 2; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);

        addChoice (prefix + "_mode", "Osc" + juce::String (osc) + " Mode", { "Wavetable", "Physical", "Sample", "Granular", "Live" }, 0);
        addChoice (prefix + "_excite", "Osc" + juce::String (osc) + " Excite", { "Burst", "Noise", "Saw", "Pulse", "Bow", "Hammer (classic)", "Osc In", "Tine", "Reed", "Piano" }, 0);
        addFloat (prefix + "_string_decay", "Osc" + juce::String (osc) + " String Decay", 0.0f, 1.0f, 0.75f);
        addFloat (prefix + "_string_damp", "Osc" + juce::String (osc) + " String Damp", 0.0f, 1.0f, 0.35f);
        addFloat (prefix + "_string_sustain", "Osc" + juce::String (osc) + " String Sustain", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_string_stiffness", "Osc" + juce::String (osc) + " Stiffness", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_string_pickup", "Osc" + juce::String (osc) + " Pickup Position", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_string_excite_pos", "Osc" + juce::String (osc) + " Excitation Position", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_string_pick_hardness", "Osc" + juce::String (osc) + " Pick Hardness", 0.0f, 1.0f, 1.0f);
        addFloat (prefix + "_string_pick_pos", "Osc" + juce::String (osc) + " Pick Position", 0.0f, 1.0f, 0.0f);
        addBool (prefix + "_string_slap", "Osc" + juce::String (osc) + " Slap", false);
    }

    for (const auto* prefix : { "osc1", "osc2", "sub" })
    {
        addFloat (juce::String (prefix) + "_bow_pressure", juce::String (prefix) + " Bow Pressure", 0.0f, 1.0f, 0.5f);
        addFloat (juce::String (prefix) + "_bow_speed", juce::String (prefix) + " Bow Speed", 0.0f, 1.0f, 0.5f);
        addFloat (juce::String (prefix) + "_bridge_buzz", juce::String (prefix) + " Bridge Buzz", 0.0f, 1.0f, 0.0f);
        addFloat (juce::String (prefix) + "_fret_rattle", juce::String (prefix) + " Fret Rattle", 0.0f, 1.0f, 0.0f);
        const auto id = juce::String (prefix);

        addBool (id + "_sample_tuned", id + " Sample Tuned", true);
        addBool (id + "_sample_loop", id + " Sample Loop", false);
        addBool (id + "_sample_reverse", id + " Sample Reverse", false);
        addFloat (id + "_sample_start", id + " Sample Start", 0.0f, 1.0f, 0.0f);
        addFloat (id + "_sample_end", id + " Sample End", 0.0f, 1.0f, 1.0f);
        addFloat (id + "_sample_fade_in", id + " Sample Fade In", 0.0f, 1.0f, 0.0f);
        addFloat (id + "_sample_fade_out", id + " Sample Fade Out", 0.0f, 1.0f, 0.0f);

        // Granular mode reads the same sample; its position is Sample Start.
        addFloat (id + "_grain_size", id + " Grain Size", 10.0f, 500.0f, 80.0f, 0.4f);
        addFloat (id + "_grain_density", id + " Grain Density", 0.0f, 1.0f, 0.5f);
        addFloat (id + "_grain_spray", id + " Grain Spray", 0.0f, 1.0f, 0.15f);
        addFloat (id + "_grain_pitch", id + " Grain Pitch Spray", 0.0f, 1.0f, 0.0f);
        addFloat (id + "_grain_spread", id + " Grain Stereo Spread", 0.0f, 1.0f, 0.6f);
    }

    // Arpeggiator
    addBool ("arp_on", "Arp On", false);
    addChoice ("arp_mode", "Arp Mode",
               { "Up", "Down", "UpDown", "Random", "DownUp", "Converge", "Walk", "Chord", "Scale Random" }, 0);
    addFloat ("arp_chance", "Arp Chance", 0.0f, 1.0f, 1.0f);

    // Generative: scale snapping and note spray
    addChoice ("gen_scale", "Scale", Scales::getNames(), 0);
    addChoice ("gen_root", "Scale Root", Scales::getRootNames(), 0);
    addBool ("sym_on", "Sympathetic Strings", false);
    addFloat ("sym_amount", "Sympathetic Amount", 0.0f, 1.0f, 0.5f);
    addFloat ("sym_decay", "Sympathetic Decay", 0.0f, 1.0f, 0.75f);
    addInt ("sym_count", "Sympathetic Strings Count", 1, 6, 3);
    addBool ("sym_manual", "Sympathetic Manual Tuning", false);
    constexpr int defaultSymNotes[] { 48, 55, 60, 64, 67, 72 };
    for (int i = 1; i <= 6; ++i)
        addInt ("sym_note" + juce::String (i), "Sympathetic Note " + juce::String (i), 36, 96,
                defaultSymNotes[i - 1]);
    addBool ("gen_snap", "Snap Played Notes", false);
    addBool ("spray_on", "Note Spray", false);
    addInt ("spray_count", "Spray Notes", 1, 8, 3);
    addInt ("spray_range", "Spray Range", 1, 24, 12);
    addChoice ("spray_direction", "Spray Direction", { "Up", "Down", "Both" }, 2);
    addFloat ("spray_spread", "Spray Spread", 0.0f, 2000.0f, 0.0f, 0.4f);
    addFloat ("spray_chance", "Spray Chance", 0.0f, 1.0f, 1.0f);
    addFloat ("spray_velocity", "Spray Velocity", 0.0f, 1.0f, 0.3f);
    addChoice ("arp_div", "Arp Div", getSyncDivisionNames(), 3);
    addInt ("arp_octaves", "Arp Octaves", 1, 4, 1);
    addFloat ("arp_gate", "Arp Gate", 0.05f, 1.0f, 0.5f);

    // LFOs
    const juce::StringArray lfoShapes { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Draw", "Steps", "Curve",
                                       "Smooth Random", "Drunk", "Chaos (classic)", "Bounce (classic)", "Pendulum (classic)",
                                       "Spring (classic)", "Friction (classic)",
                                       // M8.1 (appended: patches store the index)
                                       "Random S&H", "Sine Random", "Perlin", "Drunk Walk",
                                       "Lorenz", "Rossler", "Duffing", "Logistic Map", "Henon Map", "Double Pendulum",
                                       "Bounce", "Pendulum", "Spring", "Friction" };

    for (int lfo = 1; lfo <= numLfos; ++lfo)
    {
        const auto prefix = "lfo" + juce::String (lfo);

        addChoice (prefix + "_shape", "LFO" + juce::String (lfo) + " Shape", lfoShapes, 0);
        addFloat (prefix + "_rate", "LFO" + juce::String (lfo) + " Rate", 0.01f, 40.0f, 4.0f, 0.3f);
        addBool (prefix + "_sync", "LFO" + juce::String (lfo) + " Sync", false);
        addChoice (prefix + "_div", "LFO" + juce::String (lfo) + " Div", getSyncDivisionNames(), 2);
        addBool (prefix + "_retrig", "LFO" + juce::String (lfo) + " Retrig", false);
        addFloat (prefix + "_phase", "LFO" + juce::String (lfo) + " Start Phase", 0.0f, 1.0f, 0.0f);
        addBool (prefix + "_key", "LFO" + juce::String (lfo) + " Key Track", false);
        addFloat (prefix + "_phys_a", "LFO" + juce::String (lfo) + " Physics A", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "_phys_b", "LFO" + juce::String (lfo) + " Physics B", 0.0f, 1.0f, 0.5f);
        addBool (prefix + "_kick", "LFO" + juce::String (lfo) + " Pendulum Kick", false);

        for (int step = 0; step < 16; ++step)
            addFloat (prefix + "_step" + juce::String (step + 1), "LFO" + juce::String (lfo) + " Step " + juce::String (step + 1),
                      -1.0f, 1.0f, (float) std::sin (juce::MathConstants<double>::twoPi * (double) step / 16.0));
    }

    // Mod matrix
    const auto sourceNames = Mod::getSourceNames();
    const auto destinationNames = Mod::getDestinationNames();
    const auto addModSlotParameters = [&] (int slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        const auto name = "Mod" + juce::String (slot);

        addChoice (prefix + "_src", name + " Src", sourceNames, 0);
        addChoice (prefix + "_dst", name + " Dst", destinationNames, 0);
        addFloat (prefix + "_amt", name + " Amt", -1.0f, 1.0f, 0.0f);
        addFloat (prefix + "_curve", name + " Curve", -1.0f, 1.0f, 0.0f);
        addChoice (prefix + "_pol", name + " Polarity", { "Natural", "Unipolar", "Bipolar" }, 0);
        addChoice (prefix + "_aux", name + " Via", sourceNames, 0);
        addBool (prefix + "_byp", name + " Bypass", false);
    };
    constexpr int legacyModSlots = 32;

    for (int slot = 1; slot <= legacyModSlots; ++slot)
        addModSlotParameters (slot);

    // Macros
    for (int macro = 1; macro <= 4; ++macro)
        addFloat ("macro" + juce::String (macro), "Macro " + juce::String (macro), 0.0f, 1.0f, 0.0f);

    // Effects
    const juce::StringArray fxTypes { "None", "Amp", "Drive", "Crush", "Comp", "Comb", "Phaser",
                                      "Chorus", "Haas", "Delay", "Stutter", "Smear", "Freeze", "Reverb",
                                      "Flanger", "Dimension", "Trance Gate", "TapeStop", "Tilt", "Utility",
                                      "OTT", "Limiter", "Widener", "Tremolo", "FreqShift", "RingMod",
                                      "Octaver", "Vowel", "Feedback", "EQ" };
    for (int slot = 0; slot < numFxSlots; ++slot)
    {
        addChoice ("fx_slot" + juce::String (slot + 1), "FX Slot " + juce::String (slot + 1),
                   fxTypes, 0);
        addBool ("fx_slot" + juce::String (slot + 1) + "_bypass", "FX Bypass " + juce::String (slot + 1), false);
        addBool ("fx_slot" + juce::String (slot + 1) + "_solo", "FX Solo " + juce::String (slot + 1), false);
        addFloat ("fx_slot" + juce::String (slot + 1) + "_mix", "FX Blend " + juce::String (slot + 1),
                  0.0f, 1.0f, 1.0f);
    }

    addFloat ("fx_eq_low_freq", "EQ Low Freq", 20.0f, 800.0f, 120.0f, 0.4f);
    addFloat ("fx_eq_low_gain", "EQ Low Gain", -18.0f, 18.0f, 0.0f);
    addFloat ("fx_eq_mid_freq", "EQ Mid Freq", 100.0f, 12000.0f, 1000.0f, 0.3f);
    addFloat ("fx_eq_mid_gain", "EQ Mid Gain", -18.0f, 18.0f, 0.0f);
    addFloat ("fx_eq_mid_q", "EQ Mid Q", 0.2f, 8.0f, 0.9f, 0.4f);
    addFloat ("fx_eq_high_freq", "EQ High Freq", 1000.0f, 18000.0f, 6000.0f, 0.4f);
    addFloat ("fx_eq_high_gain", "EQ High Gain", -18.0f, 18.0f, 0.0f);

    addFloat ("fx_feedback_amount", "Feedback Amount", 0.0f, 0.95f, 0.6f);
    addFloat ("fx_feedback_delay", "Feedback Delay", 1.0f, 100.0f, 12.0f, 0.4f);
    addFloat ("fx_feedback_tone", "Feedback Tone", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_feedback_mix", "Feedback Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_stutter_reverse", "Stutter Reverse", false);
    addFloat ("fx_stutter_pitch", "Stutter Pitch", -12.0f, 12.0f, 0.0f, 1.0f, 1.0f);

    addFloat ("fx_flanger_rate", "Flanger Rate", 0.05f, 8.0f, 0.5f, 0.4f);
    addFloat ("fx_flanger_depth", "Flanger Depth", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_flanger_feedback", "Flanger Feedback", 0.0f, 0.9f, 0.5f);
    addFloat ("fx_flanger_mix", "Flanger Mix", 0.0f, 1.0f, 0.5f);

    addFloat ("fx_dim_rate", "Dimension Rate", 0.05f, 4.0f, 0.4f, 0.4f);
    addFloat ("fx_dim_depth", "Dimension Depth", 0.0f, 1.0f, 0.6f);
    addFloat ("fx_dim_mix", "Dimension Mix", 0.0f, 1.0f, 0.6f);

    addChoice ("fx_gate_div", "Gate Div", getSyncDivisionNames(), 4);
    addChoice ("fx_gate_pattern", "Gate Pattern",
               { "Straight", "Offbeat", "Triplet", "Gallop", "Random", "Dual", "Build", "Break", "Custom" }, 0);
    addInt ("fx_gate_steps", "Gate Steps", 2, 16, 16);
    addFloat ("fx_gate_swing", "Gate Swing", 0.0f, 0.5f, 0.0f);

    for (int step = 1; step <= 16; ++step)
        addFloat ("fx_gate_step" + juce::String (step), "Gate Step " + juce::String (step), 0.0f, 1.0f, 1.0f);
    addFloat ("fx_gate_smooth", "Gate Smooth", 0.0f, 1.0f, 0.2f);
    addFloat ("fx_gate_mix", "Gate Mix", 0.0f, 1.0f, 1.0f);

    addBool ("fx_tape_stop_trigger", "Tape Stop", false);
    addFloat ("fx_tape_stop_time", "Stop Time", 0.05f, 4.0f, 0.6f, 0.5f);
    addFloat ("fx_tape_stop_mix", "Stop Mix", 0.0f, 1.0f, 1.0f);

    addFloat ("fx_tilt", "Tilt", -1.0f, 1.0f, 0.0f);
    addFloat ("fx_tilt_level", "Tilt Level", -24.0f, 24.0f, 0.0f);

    addFloat ("fx_util_gain", "Utility Gain", -24.0f, 24.0f, 0.0f);
    addBool ("fx_util_mono", "Mono Maker", false);
    addBool ("fx_util_invert", "Polarity Invert", false);

    addFloat ("fx_ott_amount", "OTT Amount", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_ott_mix", "OTT Mix", 0.0f, 1.0f, 1.0f);

    addFloat ("fx_limit_ceiling", "Limiter Ceiling", -24.0f, 0.0f, -0.3f);
    addFloat ("fx_limit_release", "Limiter Release", 10.0f, 1000.0f, 120.0f, 0.4f);

    addFloat ("fx_width", "Width", 0.0f, 2.0f, 1.5f);
    addFloat ("fx_width_mix", "Width Mix", 0.0f, 1.0f, 0.5f);

    addFloat ("fx_trem_rate", "Tremolo Rate", 0.05f, 20.0f, 4.0f, 0.4f);
    addFloat ("fx_trem_depth", "Tremolo Depth", 0.0f, 1.0f, 0.7f);
    addChoice ("fx_trem_shape", "Tremolo Shape", { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, 0);

    addFloat ("fx_shifter_shift", "Freq Shift", -2000.0f, 2000.0f, 0.0f, 1.0f, 1.0f);
    addFloat ("fx_shifter_mix", "Shift Mix", 0.0f, 1.0f, 0.5f);

    addFloat ("fx_ring_freq", "Ring Freq", 1.0f, 5000.0f, 220.0f, 0.3f);
    addFloat ("fx_ring_mix", "Ring Mix", 0.0f, 1.0f, 0.5f);

    addFloat ("fx_octaver_mix", "Octaver Mix", 0.0f, 1.0f, 0.5f);

    addFloat ("fx_vowel_morph", "Vowel Morph", 0.0f, 1.0f, 0.0f);
    addFloat ("fx_vowel_mix", "Vowel Mix", 0.0f, 1.0f, 1.0f);

    addFloat ("fx_delay_time_r", "Delay Time R", 1.0f, 2000.0f, 500.0f, 0.4f);
    addFloat ("fx_delay_duck", "Delay Duck", 0.0f, 1.0f, 0.0f);

    addChoice ("fx_reverb_type", "Reverb Type", { "Room", "Hall", "Plate", "Shimmer", "Spring", "Gated", "IR" }, 0);

    addChoice ("fx_amp_mode", "Amp Mode", { "Tube", "Fuzz", "Clean" }, 0);
    addFloat ("fx_amp_drive", "Amp Drive", 1.0f, 20.0f, 4.0f);
    addFloat ("fx_amp_bass", "Amp Bass", 0.0f, 2.0f, 1.0f);
    addFloat ("fx_amp_mid", "Amp Mid", 0.0f, 2.0f, 1.0f);
    addFloat ("fx_amp_treble", "Amp Treble", 0.0f, 2.0f, 1.0f);
    addFloat ("fx_amp_level", "Amp Level", 0.0f, 2.0f, 1.0f);

    addFloat ("fx_comp_threshold", "Comp Threshold", -60.0f, 0.0f, -18.0f);
    addFloat ("fx_comp_ratio", "Comp Ratio", 1.0f, 20.0f, 4.0f);
    addFloat ("fx_comp_attack", "Comp Attack", 0.1f, 100.0f, 10.0f, 0.4f);
    addFloat ("fx_comp_release", "Comp Release", 10.0f, 1000.0f, 150.0f, 0.4f);
    addFloat ("fx_comp_makeup", "Comp Makeup", 0.0f, 24.0f, 6.0f);
    addFloat ("fx_comp_mix", "Comp Mix", 0.0f, 1.0f, 1.0f);

    addFloat ("fx_haas_delay", "Haas Delay", 1.0f, 40.0f, 14.0f);
    addFloat ("fx_haas_mix", "Haas Mix", 0.0f, 1.0f, 0.6f);

    addFloat ("fx_drive_amount", "Drive Amount", 1.0f, 20.0f, 3.0f);
    addFloat ("fx_drive_mix", "Drive Mix", 0.0f, 1.0f, 1.0f);
    addFloat ("fx_fold", "Fold", 0.0f, 1.0f, 0.0f);
    addBool ("fx_drive_on", "Drive On", true);

    addBool ("fx_crush_on", "Crush On", false);
    addFloat ("fx_crush_bits", "Crush Bits", 1.0f, 16.0f, 8.0f, 1.0f, 1.0f);
    addFloat ("fx_crush_down", "Crush Down", 1.0f, 32.0f, 2.0f, 1.0f, 1.0f);
    addFloat ("fx_crush_mix", "Crush Mix", 0.0f, 1.0f, 1.0f);

    addBool ("fx_chorus_on", "Chorus On", false);
    addFloat ("fx_chorus_rate", "Chorus Rate", 0.05f, 8.0f, 0.8f, 0.4f);
    addFloat ("fx_chorus_depth", "Chorus Depth", 0.0f, 1.0f, 0.3f);
    addFloat ("fx_chorus_mix", "Chorus Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_delay_on", "Delay On", false);
    addFloat ("fx_delay_time", "Delay Time", 1.0f, 2000.0f, 375.0f, 0.4f);
    addBool ("fx_delay_sync", "Delay Sync", false);
    addChoice ("fx_delay_div", "Delay Div", getSyncDivisionNames(), 3);
    addFloat ("fx_delay_feedback", "Delay Feedback", 0.0f, 0.95f, 0.35f);
    addFloat ("fx_delay_damping", "Delay Damping", 0.0f, 1.0f, 0.3f);
    addBool ("fx_delay_pingpong", "Delay Ping-Pong", false);
    addFloat ("fx_delay_mix", "Delay Mix", 0.0f, 1.0f, 0.3f);
    addFloat ("fx_delay_pitch", "Tape Pitch", -12.0f, 12.0f, 0.0f, 1.0f, 1.0f);
    addFloat ("fx_delay_wow", "Tape Wow", 0.0f, 1.0f, 0.0f);

    addBool ("fx_taps_on", "Taps On", false);
    addChoice ("fx_taps_pattern", "Taps Pattern", { "1/16 x2", "Gallop", "Triplet", "Echo 123", "Stutter 4", "Reverse", "Custom" }, 0);

    for (int step = 1; step <= 16; ++step)
        addFloat ("fx_taps_step" + juce::String (step), "Taps Step " + juce::String (step), 0.0f, 1.0f, 0.0f);
    addFloat ("fx_taps_mix", "Taps Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_smear_on", "Smear On", false);
    addFloat ("fx_smear_size", "Smear Size", 20.0f, 300.0f, 120.0f);
    addFloat ("fx_smear_density", "Smear Density", 2.0f, 40.0f, 12.0f);
    addFloat ("fx_smear_mix", "Smear Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_freeze_on", "Freeze On", false);
    addFloat ("fx_freeze_mix", "Freeze Mix", 0.0f, 1.0f, 0.8f);

    addBool ("fx_stutter_on", "Stutter On", false);
    addChoice ("fx_stutter_div", "Stutter Div", getSyncDivisionNames(), 4);
    addFloat ("fx_stutter_mix", "Stutter Mix", 0.0f, 1.0f, 1.0f);

    addBool ("fx_comb_on", "Comb On", false);
    addFloat ("fx_comb_freq", "Comb Freq", 20.0f, 2000.0f, 220.0f, 0.3f);
    addFloat ("fx_comb_feedback", "Comb Feedback", 0.0f, 0.97f, 0.6f);
    addFloat ("fx_comb_mix", "Comb Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_phaser_on", "Phaser On", false);
    addFloat ("fx_phaser_rate", "Phaser Rate", 0.05f, 8.0f, 0.5f, 0.4f);
    addFloat ("fx_phaser_depth", "Phaser Depth", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_phaser_feedback", "Phaser Feedback", 0.0f, 0.9f, 0.4f);
    addFloat ("fx_phaser_mix", "Phaser Mix", 0.0f, 1.0f, 0.5f);

    addBool ("fx_reverb_on", "Reverb On", false);
    addFloat ("fx_reverb_size", "Reverb Size", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_reverb_damping", "Reverb Damping", 0.0f, 1.0f, 0.5f);
    addFloat ("fx_reverb_width", "Reverb Width", 0.0f, 1.0f, 1.0f);
    addFloat ("fx_reverb_mix", "Reverb Mix", 0.0f, 1.0f, 0.25f);

    addFloat ("master", "Master", -60.0f, 12.0f, -6.0f, 1.0f, 0.1f);
    addBool ("master_clip", "Master Soft Clip", true);
    addFloat ("master_clip_gain", "Clip Gain", 0.0f, 24.0f, 0.0f, 1.0f, 0.1f);
    addBool ("oversampling", "Oversample", false);
    addChoice ("os_factor", "Oversampling Factor", { "2x", "4x" }, 0);

    // M3b additions are appended so all existing parameter IDs and choice
    // indices keep their saved meanings.
    for (int osc = 4; osc <= 6; ++osc)
    {
        const auto prefix = "osc" + juce::String (osc);
        const auto name = "Osc" + juce::String (osc);
        const auto id = [&prefix] (const char* suffix) { return prefix + "_" + suffix; };
        addBool (id ("on"), name + " On", false);
        addChoice (id ("mode"), name + " Mode", { "Wavetable", "Physical", "Sample", "Granular", "Live" }, 0);
        addChoice (id ("table"), name + " Table", getOscTableChoices(), 0);
        addFloat (id ("frame"), name + " Frame", 0.0f, 1.0f, 0.0f);
        addFloat (id ("level"), name + " Level", 0.0f, 1.0f, 0.6f);
        addFloat (id ("pan"), name + " Pan", -1.0f, 1.0f, 0.0f);
        addInt (id ("semi"), name + " Semi", -24, 24, 0);
        addFloat (id ("fine"), name + " Fine", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f);
        addInt (id ("unison"), name + " Unison", 1, VoiceParams::maxUnison, 1);
        addFloat (id ("detune"), name + " Detune", 0.0f, 50.0f, 15.0f);
        addFloat (id ("spread"), name + " Spread", 0.0f, 1.0f, 0.5f);
        addChoice (id ("warp"), name + " Warp", Warp::getNames(), 0);
        addFloat (id ("warp_amt"), name + " Warp Amount", 0.0f, 1.0f, 0.0f);
        addChoice (id ("spectral"), name + " Spectral Warp", SpectralWarp::getNames(), 0);
        addFloat (id ("spectral_amt"), name + " Spectral Amount", 0.0f, 1.0f, 0.5f);
        addChoice (id ("uni_mode"), name + " Unison Mode", UnisonMode::getNames(), 0);
        addFloat (id ("uni_blend"), name + " Unison Blend", 0.0f, 1.0f, 1.0f);
        addChoice (id ("route"), name + " Filter Route", FilterRoute::getNames(), 0);
        addChoice (id ("chord"), name + " Chord", { "Off", "Octave", "Fifth", "Power", "Major", "Minor", "Sus4" }, 0);
        addChoice (id ("excite"), name + " Excite", { "Burst", "Noise", "Saw", "Pulse", "Bow", "Hammer (classic)", "Osc In", "Tine", "Reed", "Piano" }, 0);
        addFloat (id ("string_decay"), name + " String Decay", 0.0f, 1.0f, 0.75f);
        addFloat (id ("string_damp"), name + " String Damp", 0.0f, 1.0f, 0.35f);
        addFloat (id ("string_sustain"), name + " String Sustain", 0.0f, 1.0f, 0.0f);
        addFloat (id ("string_stiffness"), name + " Stiffness", 0.0f, 1.0f, 0.0f);
        addFloat (id ("string_pickup"), name + " Pickup", 0.0f, 1.0f, 0.0f);
        addFloat (id ("string_excite_pos"), name + " Excitation Position", 0.0f, 1.0f, 0.0f);
        addFloat (id ("string_pick_hardness"), name + " Pick Hardness", 0.0f, 1.0f, 1.0f);
        addFloat (id ("string_pick_pos"), name + " Pick Position", 0.0f, 1.0f, 0.0f);
        addBool (id ("string_slap"), name + " Slap", false);
        addFloat (id ("bow_pressure"), name + " Bow Pressure", 0.0f, 1.0f, 0.5f);
        addFloat (id ("bow_speed"), name + " Bow Speed", 0.0f, 1.0f, 0.5f);
        addFloat (id ("bridge_buzz"), name + " Bridge Buzz", 0.0f, 1.0f, 0.0f);
        addFloat (id ("fret_rattle"), name + " Fret Rattle", 0.0f, 1.0f, 0.0f);
        addChoice (id ("sample_factory"), name + " Sample Source",
                   { "User File", "Metal Hit", "Vocal Ah", "Sub Tone", "Vinyl Loop", "Noise Rise" }, 0);
        addBool (id ("sample_tuned"), name + " Sample Tuned", true);
        addBool (id ("sample_loop"), name + " Sample Loop", false);
        addBool (id ("sample_reverse"), name + " Sample Reverse", false);
        addFloat (id ("sample_start"), name + " Sample Start", 0.0f, 1.0f, 0.0f);
        addFloat (id ("sample_end"), name + " Sample End", 0.0f, 1.0f, 1.0f);
        addFloat (id ("sample_fade_in"), name + " Sample Fade In", 0.0f, 1.0f, 0.0f);
        addFloat (id ("sample_fade_out"), name + " Sample Fade Out", 0.0f, 1.0f, 0.0f);
        addFloat (id ("grain_size"), name + " Grain Size", 10.0f, 500.0f, 80.0f, 0.4f);
        addFloat (id ("grain_density"), name + " Grain Density", 0.0f, 1.0f, 0.5f);
        addFloat (id ("grain_spray"), name + " Grain Spray", 0.0f, 1.0f, 0.15f);
        addFloat (id ("grain_pitch"), name + " Grain Pitch Spray", 0.0f, 1.0f, 0.0f);
        addFloat (id ("grain_spread"), name + " Grain Stereo Spread", 0.0f, 1.0f, 0.6f);
        addBool (id ("out"), name + " Output", true);
    }

    for (int source = 1; source <= OscillatorIds::count; ++source)
        for (int target = 1; target <= OscillatorIds::count; ++target)
        {
            if (source <= 3 && target <= 3)
                continue; // all nine original cells keep their parameter IDs

            const auto id = source == target ? "fm_fb" + juce::String (source)
                                              : "fm_" + juce::String (source) + "to" + juce::String (target);
            const auto name = source == target ? "FM Osc" + juce::String (source) + " Feedback"
                                                : "FM Osc" + juce::String (source) + " > Osc" + juce::String (target);
            addFloat (id, name, 0.0f, 1.0f, 0.0f);
        }

    juce::StringArray ampEnvelopeChoices;
    // Labels only; the saved value is the index, so naming the first five is safe.
    for (int env = 1; env <= 16; ++env)
        ampEnvelopeChoices.add (env == 1 ? "Amp Env" : env == 2 ? "Filter Env" : env == 3 ? "F2 Env"
                                : env == 4 ? "Mod Env" : "Env " + juce::String (env));
    ampEnvelopeChoices.add ("MSEG"); // M5: appended, index 16
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
        addChoice (prefix + "_amp_env", "Osc" + juce::String (osc + 1) + " Amp Envelope", ampEnvelopeChoices, 0);
    }
    for (int env = 6; env <= 16; ++env)
    {
        const auto prefix = "env" + juce::String (env);
        const auto name = "ENV " + juce::String (env);
        addFloat (prefix + "_attack", name + " Attack", 0.001f, 5.0f, 0.05f, 0.35f);
        addFloat (prefix + "_decay", name + " Decay", 0.005f, 5.0f, 0.4f, 0.35f);
        addFloat (prefix + "_sustain", name + " Sustain", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "_release", name + " Release", 0.005f, 10.0f, 0.3f, 0.35f);
        addFloat (prefix + "_curve", name + " Tension", -1.0f, 1.0f, 0.0f);
        addFloat (prefix + "_velocity", name + " Velocity", 0.0f, 1.0f, 0.0f);
    }
    addChoice ("quality", "Quality", { "Eco", "Normal", "High" }, 1);

    // M4 acoustic keys. Everything defaults to off, so old patches are unchanged.
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
        const auto name = "Osc" + juce::String (osc + 1);
        addFloat (prefix + "_hammer_hard", name + " Hammer Hardness", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "_couple", name + " String Coupling", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_damper", name + " Damper", 0.0f, 1.0f, 0.0f);
        addFloat (prefix + "_register", name + " Register Map", 0.0f, 1.0f, 0.0f);
    }
    addFloat ("stretch", "Stretch Tuning", 0.0f, 1.0f, 0.0f);
    addBool ("sb_on", "Soundboard", false);
    addFloat ("sb_mix", "Soundboard Mix", 0.0f, 1.0f, 0.5f);
    addFloat ("sb_tone", "Soundboard Tone", 0.0f, 1.0f, 0.5f);
    addFloat ("sb_size", "Soundboard Size", 0.0f, 1.0f, 0.5f);
    addFloat ("pedal_res", "Pedal Resonance", 0.0f, 1.0f, 0.0f);
    addFloat ("mech_key", "Key Noise", 0.0f, 1.0f, 0.0f);
    addFloat ("mech_damper", "Damper Noise", 0.0f, 1.0f, 0.0f);
    addFloat ("mech_pedal", "Pedal Noise", 0.0f, 1.0f, 0.0f);

    // M5 deep FM and M6 phase distortion. All default to the old behaviour.
    juce::StringArray warpEnvelopeChoices { "Off" };
    for (int env = 1; env <= 16; ++env)
        warpEnvelopeChoices.add ("ENV " + juce::String (env));
    warpEnvelopeChoices.add ("MSEG");

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
        const auto name = "Osc" + juce::String (osc + 1);
        addChoice (prefix + "_tune", name + " Tuning", OscTuning::getModeNames(), OscTuning::Semitones);
        addFloat (prefix + "_ratio", name + " Ratio", 0.0625f, 32.0f, 1.0f, 0.3f, 0.0001f);
        addChoice (prefix + "_ratio_snap", name + " Ratio Snap", OscTuning::getSnapNames(), 1);
        addFloat (prefix + "_fixed_hz", name + " Fixed Frequency", 1.0f, 20000.0f, 440.0f, 0.25f, 0.01f);
        addFloat (prefix + "_key_level", name + " Key Level Scaling", -1.0f, 1.0f, 0.0f);
        addChoice (prefix + "_fb_type", name + " Feedback Type", FmFeedback::getNames(), FmFeedback::Plain);
        addChoice (prefix + "_warp2", name + " Warp 2", Warp::getStageTwoNames(), 0);
        addFloat (prefix + "_warp2_amt", name + " Warp 2 Amount", 0.0f, 1.0f, 0.0f);
        addChoice (prefix + "_pd_env", name + " Warp Envelope", warpEnvelopeChoices, 0);
        addFloat (prefix + "_pd_env_amt", name + " Warp Envelope Amount", -1.0f, 1.0f, 1.0f);
    }

    for (int osc = 1; osc <= OscillatorIds::count; ++osc)
        addFloat ("fm_noise" + juce::String (osc), "FM Noise > Osc" + juce::String (osc), 0.0f, 1.0f, 0.0f);
    addFloat ("fm_noise_color", "FM Noise Colour", 0.0f, 1.0f, 1.0f);

    for (int env = 0; env < 16; ++env)
    {
        const auto prefix = envelopePrefix (env);
        const auto name = "ENV " + juce::String (env + 1);
        addFloat (prefix + "_delay", name + " Delay", 0.0f, 5.0f, 0.0f, 0.35f);
        addFloat (prefix + "_hold", name + " Hold", 0.0f, 5.0f, 0.0f, 0.35f);
        addFloat (prefix + "_keyrate", name + " Key Rate Scaling", 0.0f, 1.0f, 0.0f);
    }

    // M6b: mod slots 33-64, appended so slots 1-32 keep their parameter IDs.
    for (int slot = legacyModSlots + 1; slot <= Mod::maxSlots; ++slot)
        addModSlotParameters (slot);

    // Velocity for ENV 3-5, so every envelope has one. 0 keeps old patches.
    for (const auto* prefix : { "f2e", "me", "e4" })
        addFloat (juce::String (prefix) + "_velocity", "ENV " + juce::String (prefix[0] == 'f' ? 3 : prefix[0] == 'm' ? 4 : 5)
                                                           + " Velocity", 0.0f, 1.0f, 0.0f);

    // M7.1: the Generative card. Euclidean rhythm, probability sequencer and
    // strum. All off by default, so old patches play as before.
    addBool ("euc_on", "Euclid On", false);
    addChoice ("euc_target", "Euclid Target", { "Notes", "Exciter", "Trance Gate" }, 0);
    addInt ("euc_steps", "Euclid Steps", 2, 32, 16);
    addInt ("euc_hits", "Euclid Hits", 0, 32, 5);
    addInt ("euc_rotate", "Euclid Rotate", 0, 31, 0);
    addChoice ("euc_div", "Euclid Rate", getSyncDivisionNames(), 4);
    addFloat ("euc_gate", "Euclid Gate", 0.05f, 1.0f, 0.5f);

    addBool ("pseq_on", "Prob Seq On", false);
    addChoice ("pseq_div", "Prob Seq Rate", getSyncDivisionNames(), 4);
    addInt ("pseq_length", "Prob Seq Length", 1, 16, 16);
    addFloat ("pseq_gate", "Prob Seq Gate", 0.05f, 1.0f, 0.5f);

    for (int step = 1; step <= 16; ++step)
    {
        const auto n = juce::String (step);
        addFloat ("pseq_chance" + n, "Prob Seq Chance " + n, 0.0f, 1.0f, 1.0f);
        addInt ("pseq_range" + n, "Prob Seq Range " + n, 0, 24, 0);
        addInt ("pseq_ratchet" + n, "Prob Seq Ratchet " + n, 1, 4, 1);
    }

    addChoice ("spray_strum", "Strum", { "Off", "Up", "Down" }, 0);
    addFloat ("spray_strum_time", "Strum Time", 2.0f, 250.0f, 30.0f, 0.5f);

    // M7.5 audio input (ilanaSynth FX). Everything defaults to off, and the
    // instrument has no input, so it sounds as before.
    addFloat ("in_gain", "Input Gain", -24.0f, 24.0f, 0.0f, 1.0f, 0.1f);
    addFloat ("in_dry", "Input Dry", 0.0f, 1.0f, 0.0f);
    addFloat ("in_body", "Input to Body", 0.0f, 1.0f, 0.0f);
    addFloat ("in_strings", "Input to Strings", 0.0f, 1.0f, 0.0f);
    addChoice ("in_trigger", "Input Trigger", { "Off", "Gate", "Drone" }, 0);
    addFloat ("in_threshold", "Input Threshold", -60.0f, 0.0f, -30.0f, 1.0f, 0.1f);
    addInt ("in_note", "Input Note", 24, 96, 48);
    addFloat ("in_attack", "Input Env Attack", 0.1f, 200.0f, 5.0f, 0.4f);
    addFloat ("in_release", "Input Env Release", 5.0f, 2000.0f, 150.0f, 0.4f);
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        addBool (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_grain_live",
                 "Osc" + juce::String (osc + 1) + " Live Grains", false);

    // M7.3 electric pianos: the pickup of the Tine and Reed excites.
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
        const auto name = "Osc" + juce::String (osc + 1);
        addFloat (prefix + "_ep_distance", name + " Pickup Distance", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "_ep_position", name + " Pickup Offset", 0.0f, 1.0f, 0.5f);
    }

    // M8.1: the simulated LFO shapes' named parameters (mapped per shape,
    // see LfoSimInfo), SMOOTH for every shape, triggers and output B.
    // SMOOTH 0 and the rest only affect the new shapes, so old patches are
    // unchanged.
    for (int lfo = 1; lfo <= numLfos; ++lfo)
    {
        const auto prefix = "lfo" + juce::String (lfo);
        const auto name = "LFO" + juce::String (lfo);
        for (int param = 1; param <= LfoSimInfo::numParams; ++param)
            addFloat (prefix + "_p" + juce::String (param), name + " Param " + juce::String (param), 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "_smooth", name + " Smooth", 0.0f, 1.0f, 0.0f);
        addChoice (prefix + "_axis", name + " Output", { "X", "Y", "Z", "Mix" }, 0);
        addChoice (prefix + "_trigger", name + " Trigger", { "Note", "Free", "Beat", "Generative" }, 0);
        addBool (prefix + "_loop", name + " Loop", false);
        addInt (prefix + "_seed", name + " Seed", 0, 999, 0);
        addFloat (prefix + "_stereo", name + " Stereo", 0.0f, 1.0f, 0.0f);
        addBool (prefix + "_fire", name + " Fire", false);
    }

    // M8.2: the soundboard model. Classic keeps old patches as they were.
    addChoice ("sb_model", "Soundboard Model", { "Classic", "Dense" }, 0);

    return layout;
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

float IlanaSynthAudioProcessor::readParam (const ParamCacheEntry& entry) const
{
    auto result = entry.value->load();

    if (anyParamModulation && entry.destination >= 0)
    {
        const auto offset = paramDestinationOffsets[(size_t) entry.destination];

        if (offset != 0.0f)
            if (auto* parameter = paramDestinations[(size_t) entry.destination].parameter)
                result = parameter->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (result) + offset));
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

    return entry->value->load();
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
    liveVoice.assign ((size_t) expectedBlockSize * 4, 0.0f);
    liveEnvVoice.assign ((size_t) expectedBlockSize * 4, 0.0f);
    if (isEffectBuild)
    {
        liveHistory.buffer.setSize (2, (int) (sampleRate * 3.0), false, true, false);
        liveHistory.sampleRate = sampleRate;
        liveHistory.name = "Live input";
    }
    liveHistoryWrite = 0;
    liveLast = liveEnvState = 0.0f;
    liveGateOpen = false;
    liveGateNote = -1;
    liveInputSamples = 0;

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
    chunkMidi.ensureSize (4096);
    updateLatency();
}

// The oversamplers' filters delay the output a little; the host compensates.
void IlanaSynthAudioProcessor::updateLatency()
{
    const auto factor = oversamplingFactor.load();
    setLatencySamples (factor > 1 ? juce::roundToInt (activeOversampler().getLatencyInSamples()) : 0);
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

    {
        const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
        activeLfoCustom = lfoCustom;
        activeLfoCurveTables = lfoCurveTables;
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
        const auto slot = readModSlot (i);

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
        settings.level = getParam (ids.level);
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
        p.oscillators[(size_t) osc].ampEnv = juce::jlimit (0, 16, (int) getParam (oscAmpEnvIds[(size_t) osc]));
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
    p.bodyCouplingMode = (int) getParam ("body_coupling_mode");
    p.bodyCoupling = getParam ("body_coupling");

    if (getParam ("mpe_mode") > 0.5f)
        p.pitchBendRange = 48.0f;

    p.clockSh = lfoBuffers.getReadPointer (4);
    p.mseg = lfoBuffers.getReadPointer (5);

    p.macros[0] = getParam ("macro1");
    p.macros[1] = getParam ("macro2");
    p.macros[2] = getParam ("macro3");
    p.macros[3] = getParam ("macro4");

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
        auto filterValue = 0.0f;
        auto filter2Value = 0.0f;
        auto modValue = 0.0f;
        auto env4Value = 0.0f;
        auto activeVoices = 0;
        std::array<float, OscillatorIds::count> samplePositions;
        samplePositions.fill (-1.0f);
        std::array<float, OscillatorIds::count> phases {};

        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<Voice*> (synth.getVoice (i)))
            {
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
                    monitorVelocity.store (voice->getVelocity());
                    monitorKeyTrack.store (voice->getKeyTrack());
                    monitorRandom.store (voice->getRandomValue());
                }
            }
        }

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

    processEffects (buffer);

    buffer.applyGain (juce::Decibels::decibelsToGain (getParam ("master")));

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

    // M7.5 DRY: the untouched input back in (without the oversamplers'
    // latency, so keep it low when oversampling).
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

void IlanaSynthAudioProcessor::setLfoCustomPoint (int lfoIndex, int step, float value)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos || step < 0 || step >= lfoDrawSteps)
        return;

    const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
    lfoCustom[(size_t) lfoIndex][(size_t) step] = juce::jlimit (-1.0f, 1.0f, value);
}

float IlanaSynthAudioProcessor::getLfoCustomPoint (int lfoIndex, int step) const
{
    if (lfoIndex < 0 || lfoIndex >= numLfos || step < 0 || step >= lfoDrawSteps)
        return 0.0f;

    const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
    return lfoCustom[(size_t) lfoIndex][(size_t) step];
}

LfoCurve IlanaSynthAudioProcessor::getLfoCurve (int lfoIndex) const
{
    const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
    return lfoCurves[(size_t) juce::jlimit (0, numLfos - 1, lfoIndex)];
}

void IlanaSynthAudioProcessor::setLfoCurve (int lfoIndex, const LfoCurve& curve)
{
    if (lfoIndex < 0 || lfoIndex >= numLfos)
        return;

    auto sanitised = curve;
    sanitised.sanitise();

    std::array<float, LfoCurve::tableSize> table {};
    sanitised.renderTable (table.data());

    const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
    lfoCurves[(size_t) lfoIndex] = std::move (sanitised);
    lfoCurveTables[(size_t) lfoIndex] = table;
}

float IlanaSynthAudioProcessor::getLfoCurveValue (int lfoIndex, double phase) const
{
    const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
    const auto& table = lfoCurveTables[(size_t) juce::jlimit (0, numLfos - 1, lfoIndex)];
    const auto position = (phase - std::floor (phase)) * (double) LfoCurve::tableSize;
    const auto index = (int) position % LfoCurve::tableSize;
    const auto next = (index + 1) % LfoCurve::tableSize;
    const auto frac = (float) (position - std::floor (position));
    return table[(size_t) index] + (table[(size_t) next] - table[(size_t) index]) * frac;
}

void IlanaSynthAudioProcessor::triggerPreviewNote (int midiNote, bool isOn, float velocity)
{
    if (! juce::isPositiveAndBelow (midiNote, 128))
        return;

    const auto scope = previewFifo.write (1);

    if (scope.blockSize1 > 0)
        previewEvents[(size_t) scope.startIndex1] = { midiNote, juce::jlimit (0.0f, 1.0f, velocity), isOn };
    else if (scope.blockSize2 > 0)
        previewEvents[(size_t) scope.startIndex2] = { midiNote, juce::jlimit (0.0f, 1.0f, velocity), isOn };
}

void IlanaSynthAudioProcessor::startMacroLearn (int macroIndex)
{
    macroLearn.store (juce::jlimit (0, 3, macroIndex));
}

void IlanaSynthAudioProcessor::cancelMacroLearn()
{
    macroLearn.store (-1);
}

juce::File IlanaSynthAudioProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("ilanaSynth Presets");
}

juce::String IlanaSynthAudioProcessor::getModSlotParamId (int slotIndex, const juce::String& field) const
{
    return "mod" + juce::String (slotIndex + 1) + "_" + field;
}

void IlanaSynthAudioProcessor::setModSlotValue (int slotIndex, const juce::String& field, float value)
{
    if (auto* parameter = apvts.getParameter (getModSlotParamId (slotIndex, field)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

Mod::Slot IlanaSynthAudioProcessor::readModSlot (int slotIndex) const
{
    Mod::Slot slot;

    if (! juce::isPositiveAndBelow (slotIndex, Mod::maxSlots))
        return slot;

    const auto& raw = modSlotRaw[(size_t) slotIndex];
    const auto read = [] (const std::atomic<float>* value) { return value != nullptr ? value->load() : 0.0f; };

    slot.source = (Mod::Source) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) read (raw.src));
    slot.destination = juce::jlimit (0, Mod::getNumDestinations() - 1, (int) read (raw.dst));
    slot.depth = read (raw.amt);
    slot.curve = read (raw.curve);
    slot.polarity = (Mod::Polarity) juce::jlimit (0, 2, (int) read (raw.polarity));
    slot.aux = (Mod::Source) juce::jlimit (0, (int) Mod::Source::Count - 1, (int) read (raw.aux));
    slot.bypass = read (raw.bypass) > 0.5f;
    return slot;
}

int IlanaSynthAudioProcessor::getNumUsedModSlots() const
{
    auto used = 0;

    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto slot = readModSlot (i);

        if (slot.source != Mod::Source::None || slot.destination != 0)
            ++used;
    }

    return used;
}

void IlanaSynthAudioProcessor::clearModSlot (int slotIndex)
{
    setModSlotValue (slotIndex, "src", 0.0f);
    setModSlotValue (slotIndex, "dst", 0.0f);
    setModSlotValue (slotIndex, "amt", 0.0f);
    setModSlotValue (slotIndex, "curve", 0.0f);
    setModSlotValue (slotIndex, "pol", 0.0f);
    setModSlotValue (slotIndex, "aux", 0.0f);
    setModSlotValue (slotIndex, "byp", 0.0f);
}

bool IlanaSynthAudioProcessor::clearModSlotsForTarget (int destination)
{
    auto cleared = false;

    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        if (readModSlot (i).destination != destination)
            continue;

        clearModSlot (i);
        cleared = true;
    }

    return cleared;
}

LfoSimSettings IlanaSynthAudioProcessor::readLfoSimSettings (int lfo) const
{
    const auto& ids = lfoIds[(size_t) juce::jlimit (0, numLfos - 1, lfo)];
    LfoSimSettings settings;
    settings.shape = (int) getParam (ids.shape);
    if (! LfoSimShapes::isSim (settings.shape))
        return settings;
    for (int param = 0; param < LfoSimInfo::numParams; ++param)
        settings.p[(size_t) param] = getParam (ids.sim[(size_t) param]);
    settings.axis = (int) getParam (ids.axis);
    settings.loop = getParam (ids.loop) > 0.5f;
    settings.seed = (int) getParam (ids.seed);
    settings.stereo = getParam (ids.stereo);
    return settings;
}

void IlanaSynthAudioProcessor::renderLfos (int numSamples, const juce::MidiBuffer& midiMessages)
{
    if (lfoBuffers.getNumSamples() < numSamples)
        lfoBuffers.setSize (numLfoChannels, numSamples, false, false, true);

    if (auto* transport = getPlayHead())
    {
        if (const auto position = transport->getPosition())
        {
            if (const auto bpm = position->getBpm())
                currentBpm = *bpm;

            const auto ppq = position->getPpqPosition();
            hostPlaying = position->getIsPlaying() && ppq.hasValue();

            if (ppq.hasValue())
                hostPpq = *ppq;
        }
    }

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isController())
        {
            const auto controllerNumber = message.getControllerNumber();
            const auto controllerValue = (float) message.getControllerValue() / 127.0f;

            if (controllerNumber == 1)
                modWheelValue = controllerValue;
            else if (controllerNumber == 11)
                expressionValue = controllerValue;

            const auto learnTarget = macroLearn.load();

            if (learnTarget >= 0)
            {
                macroCc[juce::jlimit (0, 3, learnTarget)].store (controllerNumber);
                macroLearn.store (-1);
                triggerAsyncUpdate();
            }

            for (int macro = 0; macro < 4; ++macro)
            {
                if (macroCc[macro].load() == controllerNumber)
                {
                    pendingMacros[macro].store (controllerValue);
                    macrosPending.store (true);
                    triggerAsyncUpdate();
                }
            }
        }
        else if (message.isChannelPressure())
        {
            aftertouchValue = (float) message.getChannelPressureValue() / 127.0f;
        }
        else if (message.isProgramChange())
        {
            pendingProgramChange.store (message.getProgramChangeNumber());
            triggerAsyncUpdate();
        }
    }

    bool retriggers[numLfos] {};

    for (int lfo = 0; lfo < numLfos; ++lfo)
        retriggers[lfo] = getParam (lfoIds[(size_t) lfo].retrig) > 0.5f;

    for (const auto metadata : midiMessages)
    {
        if (! metadata.getMessage().isNoteOn())
            continue;

        for (int lfo = 0; lfo < numLfos; ++lfo)
        {
            const auto& ids = lfoIds[(size_t) lfo];
            const auto shape = (int) getParam (ids.shape);
            if (retriggers[lfo])
            {
                lfoPhases[(size_t) lfo] = 0.0;
                lfoChaos[(size_t) lfo].resetPhysics (shape, getParam (ids.physA));
                lfoPreviousShapes[(size_t) lfo] = shape;
            }
            if (shape == LfoShapes::Pendulum && getParam (ids.kick) > 0.5f)
            {
                if (lfoPreviousShapes[(size_t) lfo] != shape)
                {
                    lfoChaos[(size_t) lfo].resetPhysics (shape, getParam (ids.physA));
                    lfoPreviousShapes[(size_t) lfo] = shape;
                }
                lfoChaos[(size_t) lfo].kick (metadata.getMessage().getFloatVelocity());
            }
        }
    }

    // M8.1: the simulated shapes' settings and their triggers in this block,
    // as sample offsets (MIDI offsets are at the base rate).
    const auto oversampling = juce::jmax (1, juce::roundToInt (currentSampleRate / juce::jmax (1.0, baseSampleRate)));
    LfoSimSettings simSettings[numLfos];
    bool simShape[numLfos] {};
    int simTriggers[numLfos][8] {};
    int numSimTriggers[numLfos] {};
    float smoothCoefficients[numLfos] {};
    const auto addTrigger = [&] (int lfo, int offset)
    {
        if (numSimTriggers[lfo] < 8)
            simTriggers[lfo][numSimTriggers[lfo]++] = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);
    };

    // Steps of a beat grid in this block: calls hit (offset, step number).
    const auto forEachGridStep = [this, numSamples] (double beats, double& freePhase, long long& last, auto&& hit)
    {
        const auto perSample = (juce::jmax (20.0, currentBpm.load()) / 60.0) / juce::jmax (0.001, beats) / currentSampleRate;
        const auto start = hostPlaying.load() ? hostPpq.load() / beats : freePhase;
        if ((double) last > start + 1.0)
            last = (long long) std::floor (start) - 1;
        const auto end = start + (double) numSamples * perSample;
        for (auto k = (long long) std::ceil (start - 1.0e-9); (double) k < end; ++k)
        {
            if (k <= last)
                continue;
            last = k;
            hit ((int) std::ceil (((double) k - start) / perSample), k);
        }
        freePhase = std::fmod (end, 4096.0);
    };

    // The Generative steps: Euclid's hits, else the probability sequencer's
    // steps. Shared by every LFO set to Generative.
    int generativeSteps[16] {};
    int numGenerativeSteps = 0;
    {
        auto wanted = false;
        for (int lfo = 0; lfo < numLfos; ++lfo)
            wanted = wanted || (LfoSimShapes::isSim ((int) getParam (lfoIds[(size_t) lfo].shape))
                                && (int) getParam (lfoIds[(size_t) lfo].trigger) == 3);
        const auto euclid = getParam ("euc_on") > 0.5f;
        if (wanted && (euclid || getParam ("pseq_on") > 0.5f))
        {
            const auto beats = getSyncDivisionBeats ((int) getParam (euclid ? "euc_div" : "pseq_div"));
            const auto steps = juce::jlimit (2, 32, (int) getParam ("euc_steps"));
            const auto hits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
            const auto rotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
            forEachGridStep (beats, lfoGenerativePhase, lfoGenerativeLast, [&] (int offset, long long k)
            {
                if ((! euclid || euclidHit ((int) (((k % steps) + steps) % steps), hits, steps, rotate)) && numGenerativeSteps < 16)
                    generativeSteps[numGenerativeSteps++] = offset;
            });
        }
        else
        {
            lfoGenerativeLast = -1;
        }
    }

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        simSettings[lfo] = readLfoSimSettings (lfo);
        simShape[lfo] = LfoSimShapes::isSim (simSettings[lfo].shape);
        const auto& ids = lfoIds[(size_t) lfo];

        const auto fire = getParam (ids.fire) > 0.5f;
        if (fire && ! lfoFireWas[(size_t) lfo])
        {
            addTrigger (lfo, 0);
            ++lfoTriggerCounts[(size_t) lfo];
        }
        lfoFireWas[(size_t) lfo] = fire;

        if (! simShape[lfo])
            continue;

        const auto mode = (int) getParam (ids.trigger);
        if (mode == 0)
        {
            for (const auto metadata : midiMessages)
                if (metadata.getMessage().isNoteOn())
                {
                    addTrigger (lfo, metadata.samplePosition * oversampling);
                    if (simSettings[lfo].shape == LfoSimShapes::Pendulum && getParam (ids.kick) > 0.5f)
                        lfoSims[(size_t) lfo].trigger (simSettings[lfo], lfoSimSeedCounter++, true,
                                                       metadata.getMessage().getFloatVelocity());
                }
        }
        else if (mode == 2)
        {
            forEachGridStep (getSyncDivisionBeats ((int) getParam (ids.div)), lfoBeatPhase[(size_t) lfo], lfoBeatLast[(size_t) lfo],
                             [&] (int offset, long long) { addTrigger (lfo, offset); ++lfoTriggerCounts[(size_t) lfo]; });
        }
        else if (mode == 3)
        {
            for (int step = 0; step < numGenerativeSteps; ++step)
            {
                addTrigger (lfo, generativeSteps[step]);
                ++lfoTriggerCounts[(size_t) lfo];
            }
        }
        std::sort (simTriggers[lfo], simTriggers[lfo] + numSimTriggers[lfo]);
    }

    const auto makeRate = [this] (const ParamRef& syncId, const ParamRef& rateId, const ParamRef& divId,
                                  Mod::Destination rateDestination)
    {
        auto rate = getParam (rateId);

        if (getParam (syncId) > 0.5f)
        {
            const auto beats = getSyncDivisionBeats ((int) getParam (divId));
            rate = (float) ((currentBpm.load() / 60.0) / beats);
        }

        // Rate modulation from the previous block's synth-wide evaluation.
        const auto rateMod = modDisplayValues[(size_t) rateDestination].load();
        return juce::jlimit (0.001f, 200.0f, rate * std::exp2 (rateMod * 4.0f));
    };

    float lfoRates[numLfos] {};
    float lfoShapes[numLfos] {};
    float lfoPhysA[numLfos] {}, lfoPhysB[numLfos] {};
    float lfoSteps[numLfos][16] {};
    double lfoIncrements[numLfos] {};

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto& ids = lfoIds[(size_t) lfo];

        lfoRates[lfo] = makeRate (ids.sync, ids.rate, ids.div,
                                  Mod::lfoRateDestinationFor (lfo));
        lfoShapes[lfo] = (float) (int) getParam (ids.shape);
        lfoPhysA[lfo] = getParam (ids.physA);
        lfoPhysB[lfo] = getParam (ids.physB);
        if (lfoPreviousShapes[(size_t) lfo] != (int) lfoShapes[lfo])
        {
            lfoPreviousShapes[(size_t) lfo] = (int) lfoShapes[lfo];
            if (LfoShapes::isPhysics ((int) lfoShapes[lfo]))
                lfoChaos[(size_t) lfo].resetPhysics ((int) lfoShapes[lfo], lfoPhysA[lfo]);
        }
        lfoIncrements[lfo] = (double) lfoRates[lfo] / currentSampleRate;
        smoothCoefficients[lfo] = LfoSmoother::coefficientFor (getParam (ids.smooth), (double) lfoRates[lfo], currentSampleRate);
        lfoSims[(size_t) lfo].sampleRate = currentSampleRate;

        for (int step = 0; step < 16; ++step)
            lfoSteps[lfo][step] = getParam (ids.steps[(size_t) step]);
    }

    float* lfoBufferPointers[numLfos] {};

    float* lfoBufferPointersB[numLfos] {};

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        lfoBufferPointers[lfo] = lfoBuffers.getWritePointer (lfoChannel (lfo));
        lfoBufferPointersB[lfo] = lfoBuffers.getWritePointer (lfoChannelB (lfo));
        juce::FloatVectorOperations::clear (lfoBufferPointersB[lfo], numSamples);
    }
    int nextSimTrigger[numLfos] {};

    // LFO 1-4 always render, as before the pool. LFO 5-16 render only when a
    // mod slot uses them; otherwise their phase just moves on for the cards.
    bool renderLfo[numLfos] {};
    for (int lfo = 0; lfo < numLfos; ++lfo)
        renderLfo[lfo] = simShape[lfo] ? lfoRouted[(size_t) lfo] || lfoRoutedB[(size_t) lfo]
                                       : lfo < 4 || lfoRouted[(size_t) lfo] || lfoRoutedB[(size_t) lfo];

    auto* clockBuffer = lfoBuffers.getWritePointer (4);
    auto* msegBuffer = lfoBuffers.getWritePointer (5);
    static_assert (lfoChannel (3) == 3 && lfoChannel (4) == 6, "clock and MSEG keep channels 4 and 5");

    const auto clockBeats = getSyncDivisionBeats ((int) getParam ("clock_div"));
    const auto clockRate = (currentBpm.load() / 60.0) / juce::jmax (0.001, clockBeats);
    const auto clockIncrement = clockRate / currentSampleRate;

    {
        const float levels[4] { getParam ("mseg_level1"), getParam ("mseg_level2"),
                                getParam ("mseg_level3"), getParam ("mseg_level4") };
        const float times[4] { getParam ("mseg_time1"), getParam ("mseg_time2"),
                               getParam ("mseg_time3"), getParam ("mseg_time4") };

        const auto rateMod = modDisplayValues[(size_t) Mod::Destination::MsegRate].load();
        mseg.setParams (levels, times, getParam ("mseg_rate") * std::exp2 (rateMod * 4.0f), getParam ("mseg_loop") > 0.5f);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        for (int lfo = 0; lfo < numLfos; ++lfo)
        {
            if (! renderLfo[lfo])
                continue;

            const auto phase = lfoPhases[(size_t) lfo];
            const auto stepIndex = juce::jlimit (0, 15, (int) (phase * 16.0));
            const auto shape = (int) lfoShapes[lfo];

            if (simShape[lfo])
            {
                auto& sim = lfoSims[(size_t) lfo];
                while (nextSimTrigger[lfo] < numSimTriggers[lfo] && simTriggers[lfo][nextSimTrigger[lfo]] <= i)
                {
                    sim.trigger (simSettings[lfo], lfoSimSeedCounter++);
                    ++nextSimTrigger[lfo];
                }
                float a = 0.0f, b = 0.0f;
                sim.next (simSettings[lfo], lfoIncrements[lfo], a, b);
                if (smoothCoefficients[lfo] < 1.0f)
                    lfoSmoothers[(size_t) lfo].process (a, b, smoothCoefficients[lfo]);
                lfoBufferPointers[lfo][i] = a;
                lfoBufferPointersB[lfo][i] = b;
                auto next = phase + lfoIncrements[lfo];
                lfoPhases[(size_t) lfo] = next - std::floor (next);
                continue;
            }

            auto& chaos = lfoChaos[(size_t) lfo];
            const auto stateful = LfoShapes::isStateful (shape);

            if (shape == LfoShapes::Chaos)
                chaos.advance (lfoIncrements[lfo]);
            else if (LfoShapes::isPhysics (shape))
                chaos.advancePhysics (shape, lfoIncrements[lfo], lfoPhysA[lfo], lfoPhysB[lfo]);

            // LFO 1-4 always run (they share the clock's random generator,
            // so skipping their draws would shift it), but an unrouted one's
            // wave is only needed for the card's last value.
            if (lfoRouted[(size_t) lfo] || stateful || shape == 7 || i == numSamples - 1)
                lfoBufferPointers[lfo][i] = shape == 7
                                                ? lfoSteps[lfo][stepIndex]
                                                : (stateful ? chaos.value (shape, phase)
                                                            : lfoValue (shape, phase, lfoSampleHolds[(size_t) lfo].load(),
                                                                        activeLfoCustom[(size_t) lfo].data(),
                                                                        activeLfoCurveTables[(size_t) lfo].data()));
            else
                lfoBufferPointers[lfo][i] = 0.0f;

            // M8.1: SMOOTH and output B (a quarter cycle on for the
            // periodic shapes). Both off leave the classic path untouched.
            if (smoothCoefficients[lfo] < 1.0f || lfoRoutedB[(size_t) lfo])
            {
                auto a = lfoBufferPointers[lfo][i];
                auto b = a;
                if (lfoRoutedB[(size_t) lfo] && ! stateful && shape != 5)
                {
                    const auto quarter = phase + 0.25 - std::floor (phase + 0.25);
                    b = shape == 7 ? lfoSteps[lfo][juce::jlimit (0, 15, (int) (quarter * 16.0))]
                                   : lfoValue (shape, quarter, lfoSampleHolds[(size_t) lfo].load(),
                                               activeLfoCustom[(size_t) lfo].data(), activeLfoCurveTables[(size_t) lfo].data());
                }
                if (smoothCoefficients[lfo] < 1.0f)
                    lfoSmoothers[(size_t) lfo].process (a, b, smoothCoefficients[lfo]);
                lfoBufferPointers[lfo][i] = a;
                lfoBufferPointersB[lfo][i] = b;
            }

            auto nextPhase = phase + lfoIncrements[lfo];

            if (nextPhase >= 1.0)
            {
                nextPhase -= std::floor (nextPhase);
                lfoSampleHolds[(size_t) lfo].store (randomForLfo (lfo).nextFloat() * 2.0f - 1.0f);

                if (stateful && ! LfoShapes::isPhysics (shape))
                    chaos.onCycle (shape, randomForLfo (lfo));
            }

            lfoPhases[(size_t) lfo] = nextPhase;
        }

        clockBuffer[i] = clockShValue;
        msegBuffer[i] = mseg.getNextValue();

        clockShPhase += clockIncrement;

        if (clockShPhase >= 1.0)
        {
            clockShPhase -= std::floor (clockShPhase);
            clockShValue = lfoRandom.nextFloat() * 2.0f - 1.0f;
        }
    }

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        if (renderLfo[lfo])
        {
            if (numSamples > 0)
            {
                lfoLastValues[(size_t) lfo].store (lfoBufferPointers[lfo][numSamples - 1]);
                lfoLastValuesB[(size_t) lfo].store (lfoBufferPointersB[lfo][numSamples - 1]);
            }
            continue;
        }

        // Unrouted: silence for the voices, the phase moves on for the cards.
        juce::FloatVectorOperations::clear (lfoBufferPointers[lfo], numSamples);
        auto phase = lfoPhases[(size_t) lfo] + lfoIncrements[lfo] * (double) numSamples;
        phase -= std::floor (phase);
        lfoPhases[(size_t) lfo] = phase;
        const auto shape = (int) lfoShapes[lfo];
        lfoLastValuesB[(size_t) lfo].store (0.0f);
        lfoLastValues[(size_t) lfo].store (LfoShapes::isStateful (shape) || shape == 7 || LfoSimShapes::isSim (shape)
                                               ? 0.0f
                                               : lfoValue (shape, phase, lfoSampleHolds[(size_t) lfo].load(),
                                                           activeLfoCustom[(size_t) lfo].data(),
                                                           activeLfoCurveTables[(size_t) lfo].data()));
    }

    modWheelDisplay.store (modWheelValue);
    aftertouchDisplay.store (aftertouchValue);
    expressionDisplay.store (expressionValue);
    clockShDisplay.store (clockShValue);
    msegDisplay.store (numSamples > 0 ? msegBuffer[numSamples - 1] : 0.0f);
}

// Pedal resonance, the soundboard and mechanical noises: after the voices,
// at the base rate. Each part costs nothing while it is off.
void IlanaSynthAudioProcessor::processAcousticKeys (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const auto keyNoise = getParam ("mech_key");
    const auto damperNoise = getParam ("mech_damper");
    const auto pedalNoise = getParam ("mech_pedal");
    const auto pedalAmount = getParam ("pedal_res");
    using Kind = MechanicalNoise::Kind;

    for (const auto metadata : midi)
    {
        const auto& message = metadata.getMessage();
        const auto offset = metadata.samplePosition;

        if (message.isNoteOff())
        {
            const auto note = message.getNoteNumber();
            mechanicalNoise.trigger (Kind::KeyRelease, note, keyNoise, offset);

            // With the pedal down the damper stays up until the pedal lifts.
            if (keysPedalDown)
                pedalHeldNotes[(size_t) note] = true;
            else
                mechanicalNoise.trigger (Kind::Damper, note, damperNoise, offset);
        }
        else if (message.isNoteOn())
        {
            pedalHeldNotes[(size_t) message.getNoteNumber()] = false;
        }
        else if (message.isSustainPedalOn() && ! keysPedalDown)
        {
            keysPedalDown = true;
            mechanicalNoise.trigger (Kind::PedalDown, -1, pedalNoise, offset);
            pedalResonance.setPedal (true, pedalAmount);
        }
        else if (message.isSustainPedalOff() && keysPedalDown)
        {
            keysPedalDown = false;
            mechanicalNoise.trigger (Kind::PedalUp, -1, pedalNoise, offset);
            pedalResonance.setPedal (false, pedalAmount);

            // Every damper that was held up lands at once.
            auto landed = 0;
            for (int note = 0; note < 128; ++note)
                if (std::exchange (pedalHeldNotes[(size_t) note], false) && landed < 6)
                {
                    mechanicalNoise.trigger (Kind::Damper, note, damperNoise * 0.6f, offset + landed * 24);
                    ++landed;
                }
        }
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto numSamples = buffer.getNumSamples();

    pedalResonance.setStretch (getParam ("stretch"));
    if (pedalAmount > 0.0f && pedalResonance.isRinging())
        pedalResonance.process (left, right, numSamples, pedalAmount);

    const auto soundboardOn = getParam ("sb_on") > 0.5f;
    if (soundboardOn)
    {
        if (! soundboardWasOn)
        {
            soundboard.reset();
            denseSoundboard.reset();
        }
        if ((int) getParam ("sb_model") == 1)
            denseSoundboard.process (left, right, numSamples, getParam ("sb_mix"), getParam ("sb_tone"), getParam ("sb_size"));
        else
            soundboard.process (left, right, numSamples, getParam ("sb_mix"), getParam ("sb_tone"), getParam ("sb_size"));
    }
    soundboardWasOn = soundboardOn;

    mechanicalNoise.process (left, right, numSamples);
}

float IlanaSynthAudioProcessor::globalSourceValue (Mod::Source source) const
{
    if (const auto lfoIndex = Mod::lfoBIndexFor (source); lfoIndex >= 0)
        return lfoLastValuesB[(size_t) lfoIndex].load();
    if (source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
        return getEnvMonitorExtra ((int) source - (int) Mod::Source::Env6);
    if (const auto lfoIndex = Mod::lfoIndexFor (source); lfoIndex >= 0)
    {
        const auto& lfoId = lfoIds[(size_t) lfoIndex];
        const auto shape = (int) getParam (lfoId.shape);
        const auto phase = lfoPhases[(size_t) lfoIndex];

        if (LfoSimShapes::isSim (shape) || getParam (lfoId.smooth) > 0.0f)
            return lfoLastValues[(size_t) lfoIndex].load();

        if (LfoShapes::isStateful (shape))
            return lfoChaos[(size_t) lfoIndex].value (shape, phase);

        return shape == 7 ? getParam (lfoId.steps[(size_t) juce::jlimit (0, 15, (int) (phase * 16.0))])
                          : lfoValue (shape, phase, lfoSampleHolds[(size_t) lfoIndex].load(),
                                      activeLfoCustom[(size_t) lfoIndex].data(),
                                      activeLfoCurveTables[(size_t) lfoIndex].data());
    }

    switch (source)
    {
        case Mod::Source::AmpEnv:     return envMonitorAmp.load();
        case Mod::Source::FilterEnv:  return envMonitorFilter.load();
        case Mod::Source::FilterEnv2: return envMonitorFilter2.load();
        case Mod::Source::ModEnv:     return envMonitorMod.load();
        case Mod::Source::Env4:       return envMonitorEnv4.load();
        case Mod::Source::Velocity:   return monitorVelocity.load();
        case Mod::Source::KeyTrack:   return monitorKeyTrack.load();
        case Mod::Source::Random:     return monitorRandom.load();
        case Mod::Source::ClockSh:    return clockShValue;
        case Mod::Source::Mseg:       return lfoBuffers.getNumSamples() > 0 ? lfoBuffers.getSample (5, 0) : 0.0f;
        default:                      return staticSourceValue ((int) source);
    }
}

void IlanaSynthAudioProcessor::evaluateGlobalModulation (const Mod::Slot* slots, int numSlots)
{
    float totals[maxDestinations] {};

    // Param destinations are applied inside getParam(), so clear them first
    // or the source values read below would include last block's offsets.
    anyParamModulation = false;
    std::fill (paramDestinationOffsets.begin(), paramDestinationOffsets.end(), 0.0f);

    for (int i = 0; i < numSlots; ++i)
    {
        const auto& slot = slots[i];

        if (! juce::isPositiveAndBelow (slot.destination, maxDestinations))
            continue;

        auto value = Mod::shape (slot, globalSourceValue (slot.source));

        if (slot.aux != Mod::Source::None)
            value *= Mod::auxScale (slot.aux, globalSourceValue (slot.aux));

        totals[slot.destination] += slot.depth * value;
    }

    for (int d = 0; d < maxDestinations; ++d)
        modDisplayValues[(size_t) d].store (totals[d]);

    for (int i = 0; i < (int) paramDestinations.size(); ++i)
    {
        // The OSC 4-6 FM cells are modulated per voice instead.
        if (Mod::extendedFmCellFor (Mod::paramDestinationFor (i)) >= 0)
            continue;

        const auto offset = totals[Mod::paramDestinationFor (i)];

        if (offset != 0.0f && paramDestinations[(size_t) i].parameter != nullptr)
        {
            paramDestinationOffsets[(size_t) i] = offset;
            anyParamModulation = true;
        }
    }
}

float IlanaSynthAudioProcessor::getArpStepRateHz() const
{
    const auto beats = getSyncDivisionBeats ((int) getParam ("arp_div"));

    return (float) ((currentBpm.load() / 60.0) / juce::jmax (0.001, beats));
}

float IlanaSynthAudioProcessor::getSourceDisplayValue (int sourceIndex) const
{
    if (const auto lfoIndex = Mod::lfoBIndexFor ((Mod::Source) sourceIndex); lfoIndex >= 0)
        return lfoLastValuesB[(size_t) lfoIndex].load();
    const auto source = (Mod::Source) juce::jlimit (0, (int) Mod::Source::Count - 1, sourceIndex);
    if (source >= Mod::Source::Env6 && source <= Mod::Source::Env16)
        return getEnvMonitorExtra ((int) source - (int) Mod::Source::Env6);

    if (const auto lfoIndex = Mod::lfoIndexFor (source); lfoIndex >= 0)
    {
        const auto prefix = "lfo" + juce::String (lfoIndex + 1);
        const auto shape = (int) getParam ((prefix + "_shape").toRawUTF8());
        const auto phase = (double) lfoPhaseDisplays[(size_t) lfoIndex].load();

        if (LfoShapes::isStateful (shape) || LfoSimShapes::isSim (shape))
            return lfoLastValues[(size_t) lfoIndex].load();

        if (shape == 7)
            return getParam ((prefix + "_step"
                              + juce::String (juce::jlimit (0, 15, (int) (phase * 16.0)) + 1)).toRawUTF8());

        std::array<float, lfoDrawSteps> custom {};
        std::array<float, LfoCurve::tableSize> curve {};

        {
            const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
            custom = lfoCustom[(size_t) lfoIndex];
            curve = lfoCurveTables[(size_t) lfoIndex];
        }

        return lfoValue (shape, phase, lfoSampleHolds[(size_t) lfoIndex].load(), custom.data(), curve.data());
    }

    switch (source)
    {
        case Mod::Source::Macro1:     return getParam ("macro1");
        case Mod::Source::Macro2:     return getParam ("macro2");
        case Mod::Source::Macro3:     return getParam ("macro3");
        case Mod::Source::Macro4:     return getParam ("macro4");
        case Mod::Source::ModWheel:   return modWheelDisplay.load();
        case Mod::Source::Aftertouch: return aftertouchDisplay.load();
        case Mod::Source::Expression: return expressionDisplay.load();
        case Mod::Source::ClockSh:    return clockShDisplay.load();
        case Mod::Source::AmpEnv:     return envMonitorAmp.load();
        case Mod::Source::FilterEnv:  return envMonitorFilter.load();
        case Mod::Source::FilterEnv2: return envMonitorFilter2.load();
        case Mod::Source::ModEnv:     return envMonitorMod.load();
        case Mod::Source::Env4:       return envMonitorEnv4.load();
        case Mod::Source::Velocity:   return monitorVelocity.load();
        case Mod::Source::KeyTrack:   return monitorKeyTrack.load();
        case Mod::Source::Random:     return monitorRandom.load();
        case Mod::Source::Mseg:       return msegDisplay.load();
        case Mod::Source::InputEnv:   return inputEnvDisplay.load();
        default:                      return 0.0f;
    }
}

float IlanaSynthAudioProcessor::staticSourceValue (int sourceIndex) const
{
    switch ((Mod::Source) juce::jlimit (0, (int) Mod::Source::Count - 1, sourceIndex))
    {
        case Mod::Source::Macro1:     return getParam ("macro1");
        case Mod::Source::Macro2:     return getParam ("macro2");
        case Mod::Source::Macro3:     return getParam ("macro3");
        case Mod::Source::Macro4:     return getParam ("macro4");
        case Mod::Source::ModWheel:   return modWheelValue;
        case Mod::Source::Aftertouch: return aftertouchValue;
        case Mod::Source::Expression: return expressionValue;
        default:                      return 0.0f;
    }
}

void IlanaSynthAudioProcessor::processEffects (juce::AudioBuffer<float>& buffer)
{
    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto& ids = fxSlotIds[(size_t) (slot - 1)];
        const auto type = (int) getParam (ids.type);

        if (type == 0 || getParam (ids.bypass) > 0.5f)
        {
            fxSlotCpu[(size_t) (slot - 1)].store (0.0f);
            continue;
        }

        const auto solo = getParam (ids.solo) > 0.5f;
        const auto blend = getParam (ids.mix);
        const auto startTicks = juce::Time::getHighResolutionTicks();
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        if (solo || blend < 0.999f)
        {
            if (fxScratch.getNumChannels() < numChannels || fxScratch.getNumSamples() < numSamples)
                fxScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                fxScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            processSlot (type, fxScratch);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                if (solo)
                {
                    buffer.copyFrom (channel, 0, fxScratch, channel, 0, numSamples);
                }
                else
                {
                    buffer.applyGain (channel, 0, numSamples, 1.0f - blend);
                    buffer.addFrom (channel, 0, fxScratch, channel, 0, numSamples, blend);
                }
            }
        }
        else
        {
            processSlot (type, buffer);
        }

        sanitiseBuffer (buffer);

        const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks);
        const auto availableSeconds = (double) numSamples / juce::jmax (1.0, currentSampleRate);
        const auto usage = (float) juce::jlimit (0.0, 1.0, elapsedSeconds / juce::jmax (1.0e-9, availableSeconds));

        auto& cpu = fxSlotCpu[(size_t) (slot - 1)];
        cpu.store (cpu.load() * 0.9f + usage * 0.1f);
    }

    sanitiseBuffer (buffer);
}

void IlanaSynthAudioProcessor::processSlot (int type, juce::AudioBuffer<float>& buffer)
{
    switch (type)
    {
        case 1:  processAmp (buffer); break;
        case 2:  processDrive (buffer); break;
        case 3:  processCrush (buffer); break;
        case 4:  processCompressor (buffer); break;
        case 5:  processComb (buffer); break;
        case 6:  processPhaser (buffer); break;
        case 7:  processChorus (buffer); break;
        case 8:  processHaas (buffer); break;
        case 9:  processDelay (buffer); break;
        case 10: processStutter (buffer); break;
        case 11: processSmear (buffer); break;
        case 12: processFreeze (buffer); break;
        case 13: processReverb (buffer); break;
        case 14: processFlanger (buffer); break;
        case 15: processDimension (buffer); break;
        case 16: processGate (buffer); break;
        case 17: processTapeStop (buffer); break;
        case 18: processTilt (buffer); break;
        case 19: processUtility (buffer); break;
        case 20: processOtt (buffer); break;
        case 21: processLimiter (buffer); break;
        case 22: processWidener (buffer); break;
        case 23: processTremolo (buffer); break;
        case 24: processFreqShift (buffer); break;
        case 25: processRingMod (buffer); break;
        case 26: processOctaver (buffer); break;
        case 27: processVowel (buffer); break;
        case 28: processFeedback (buffer); break;
        case 29: processEq (buffer); break;
        default: break;
    }
}

void IlanaSynthAudioProcessor::processDrive (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_drive_on") > 0.5f)
    {
        const auto amount = juce::jlimit (1.0f, 20.0f, getParam ("fx_drive_amount") + getFxMod (Mod::Destination::FxDriveAmount, 19.0f));
        const auto mix = getParam ("fx_drive_mix");

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto driven = std::tanh (data[i] * amount);
                data[i] = data[i] + (driven - data[i]) * mix;
            }
        }
    }

    if (getParam ("fx_fold") > 0.001f)
    {
        const auto amount = getParam ("fx_fold");
        const auto gain = 1.0f + amount * 5.0f;
        const auto compensation = 1.0f / (1.0f + amount * 1.5f);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
                data[i] = foldTriangle (data[i] * gain) * compensation;
        }
    }
}

void IlanaSynthAudioProcessor::processCrush (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_crush_on") > 0.5f)
    {
        const auto bits = juce::jlimit (1.0f, 16.0f, getParam ("fx_crush_bits"));
        const auto downsample = juce::jlimit (1, 64, (int) getParam ("fx_crush_down"));
        const auto mix = juce::jlimit (0.0f, 1.0f, getParam ("fx_crush_mix") + getFxMod (Mod::Destination::FxCrushMix, 1.0f));
        const auto levels = std::pow (2.0f, bits) - 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (crushCounter == 0)
            {
                for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
                {
                    const auto value = buffer.getSample (channel, i);
                    crushHold[channel] = std::round (value * levels) / levels;
                }
            }

            crushCounter = (crushCounter + 1) % downsample;

            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                const auto value = buffer.getSample (channel, i);
                buffer.setSample (channel, i, value + (crushHold[channel] - value) * mix);
            }
        }
    }

}

void IlanaSynthAudioProcessor::processComb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_comb_on") > 0.5f)
    {
        const auto frequency = juce::jlimit (20.0f, 2000.0f, getParam ("fx_comb_freq") * std::exp2 (getFxMod (Mod::Destination::FxCombFreq, 4.0f)));
        const auto feedback = juce::jlimit (0.0f, 0.97f, getParam ("fx_comb_feedback"));
        const auto mix = getParam ("fx_comb_mix");
        const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.149),
                                                (float) (currentSampleRate / frequency));

        combLine.setDelay (delaySamples);

        for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto delayed = combLine.popSample (channel);
                const auto input = data[i];

                combLine.pushSample (channel, input + delayed * feedback);
                data[i] = input + delayed * mix;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processPhaser (juce::AudioBuffer<float>& buffer)
{
    if (getParam ("fx_phaser_on") > 0.5f)
    {
        phaser.setRate (juce::jlimit (0.05f, 8.0f, getParam ("fx_phaser_rate") * std::exp2 (getFxMod (Mod::Destination::FxPhaserRate, 4.0f))));
        phaser.setDepth (getParam ("fx_phaser_depth"));
        phaser.setCentreFrequency (800.0f);
        phaser.setFeedback (getParam ("fx_phaser_feedback"));
        phaser.setMix (getParam ("fx_phaser_mix"));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        phaser.process (context);
    }

}

void IlanaSynthAudioProcessor::processChorus (juce::AudioBuffer<float>& buffer)
{
    if (getParam ("fx_chorus_on") > 0.5f)
    {
        chorus.setRate (getParam ("fx_chorus_rate"));
        chorus.setDepth (juce::jlimit (0.0f, 1.0f, getParam ("fx_chorus_depth") + getFxMod (Mod::Destination::FxChorusDepth, 1.0f)));
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (getParam ("fx_chorus_mix"));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        chorus.process (context);
    }

}

void IlanaSynthAudioProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_delay_on") > 0.5f)
    {
        auto timeMs = getParam ("fx_delay_time");
        auto timeMsR = getParam ("fx_delay_time_r");

        if (getParam ("fx_delay_sync") > 0.5f)
        {
            const auto beats = getSyncDivisionBeats ((int) getParam ("fx_delay_div"));
            timeMs = (float) ((60.0 / currentBpm.load()) * beats * 1000.0);
            timeMsR = timeMs;
        }

        float delaySamples[2];
        float baseDelaySamples[2];
        baseDelaySamples[0] = juce::jlimit (1.0f, (float) (currentSampleRate * 1.9),
                                            (float) (timeMs * 0.001 * currentSampleRate));
        baseDelaySamples[1] = juce::jlimit (1.0f, (float) (currentSampleRate * 1.9),
                                            (float) (timeMsR * 0.001 * currentSampleRate));
        delaySamples[0] = baseDelaySamples[0];
        delaySamples[1] = baseDelaySamples[1];
        const auto feedback = juce::jlimit (0.0f, 0.95f, getParam ("fx_delay_feedback") + getFxMod (Mod::Destination::FxDelayFeedback, 0.9f));
        const auto mix = juce::jlimit (0.0f, 1.0f, getParam ("fx_delay_mix") + getFxMod (Mod::Destination::FxDelayMix, 1.0f));
        const auto damping = getParam ("fx_delay_damping");
        const auto pingPong = getParam ("fx_delay_pingpong") > 0.5f;
        const auto duck = getParam ("fx_delay_duck");
        const auto dampCoeff = 1.0f - juce::jlimit (0.0f, 0.95f, damping) * 0.92f;
        const auto channels = juce::jmin (2, numChannels);

        const auto wowAmount = getParam ("fx_delay_wow");

        const auto pitchSemitones = getParam ("fx_delay_pitch");
        const auto shiftActive = std::abs (pitchSemitones) > 0.01f;
        const auto shiftRatio = std::exp2 ((double) pitchSemitones / 12.0);

        if (shiftActive)
            for (int channel = 0; channel < 2; ++channel)
                tapeShift[channel].setRatio (shiftRatio);

        const auto duckCoefficient = (float) std::exp (-1.0 / (0.05 * currentSampleRate));

        static constexpr int maxTaps = 4;
        static constexpr float tapTimes[6][maxTaps] = {
            { 0.25f, 0.5f, 0.0f, 0.0f },
            { 0.25f, 0.375f, 0.75f, 0.0f },
            { 0.375f, 0.75f, 0.0f, 0.0f },
            { 1.0f, 2.0f, 3.0f, 0.0f },
            { 0.125f, 0.25f, 0.375f, 0.5f },
            { 1.5f, 1.0f, 0.5f, 0.0f }
        };
        static constexpr float tapGains[6][maxTaps] = {
            { 0.7f, 0.5f, 0.0f, 0.0f },
            { 0.6f, 0.5f, 0.4f, 0.0f },
            { 0.6f, 0.45f, 0.0f, 0.0f },
            { 0.5f, 0.35f, 0.25f, 0.0f },
            { 0.6f, 0.55f, 0.5f, 0.45f },
            { 0.4f, 0.5f, 0.6f, 0.0f }
        };

        const auto tapsOn = getParam ("fx_taps_on") > 0.5f;
        const auto tapPattern = juce::jlimit (0, 6, (int) getParam ("fx_taps_pattern"));
        const auto tapMix = getParam ("fx_taps_mix");
        float customTapGains[16] {};

        if (tapsOn && tapPattern == 6)
            for (int step = 0; step < 16; ++step)
                customTapGains[step] = getParam (tapStepIds[(size_t) step]);

        for (int i = 0; i < numSamples; ++i)
        {
            if (wowAmount > 0.001f)
            {
                wowPhase += 0.6 / currentSampleRate;
                wowPhase -= std::floor (wowPhase);

                const auto wowScale = 1.0f + wowAmount * 0.01f
                                                * (float) std::sin (juce::MathConstants<double>::twoPi * wowPhase);

                for (int channel = 0; channel < channels; ++channel)
                    delaySamples[channel] = baseDelaySamples[channel] * wowScale;
            }

            float delayed[2] {};
            float input[2] {};
            auto dryLevel = 0.0f;

            for (int channel = 0; channel < channels; ++channel)
            {
                input[channel] = buffer.getSample (channel, i);
                dryLevel = juce::jmax (dryLevel, std::abs (input[channel]));
                delayed[channel] = delayLine.popSample (channel, delaySamples[channel], true);
            }

            duckEnvelope = dryLevel + (duckEnvelope - dryLevel) * duckCoefficient;
            const auto duckGain = 1.0f - duck * juce::jlimit (0.0f, 1.0f, duckEnvelope * 6.0f);

            for (int channel = 0; channel < channels; ++channel)
            {
                auto feedbackSource = (pingPong && channels > 1) ? delayed[1 - channel] : delayed[channel];

                tapeShift[channel].push (feedbackSource);

                if (shiftActive)
                    feedbackSource = tapeShift[channel].process();

                delayDampState[channel] += (feedbackSource - delayDampState[channel]) * dampCoeff;
                delayLine.pushSample (channel, input[channel] + delayDampState[channel] * feedback);
                buffer.setSample (channel, i, input[channel] + (delayed[channel] * duckGain - input[channel]) * mix);
            }

            if (tapsOn)
            {
                for (int channel = 0; channel < channels; ++channel)
                {
                    auto sum = 0.0f;

                    if (tapPattern == 6)
                    {
                        for (int step = 0; step < 16; ++step)
                        {
                            if (customTapGains[step] < 0.001f)
                                continue;

                            const auto tapDelay = juce::jlimit (1.0f, (float) (currentSampleRate * 1.95),
                                                                delaySamples[channel] * (float) (step + 1) * 0.25f);
                            sum += delayLine.popSample (channel, tapDelay, false) * customTapGains[step];
                        }
                    }
                    else
                    {
                        for (int tap = 0; tap < maxTaps; ++tap)
                        {
                            const auto time = tapTimes[tapPattern][tap];

                            if (time <= 0.0f)
                                continue;

                            const auto tapDelay = juce::jlimit (1.0f, (float) (currentSampleRate * 1.95),
                                                                delaySamples[channel] * time);
                            sum += delayLine.popSample (channel, tapDelay, false) * tapGains[tapPattern][tap];
                        }
                    }

                    buffer.addSample (channel, i, sum * tapMix);
                }
            }
        }
    }

}

void IlanaSynthAudioProcessor::processStutter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto stutterOn = getParam ("fx_stutter_on") > 0.5f;

    if (stutterOn && ! stutterWasOn && stutterBuffer.getNumSamples() > 0)
    {
        const auto beats = getSyncDivisionBeats ((int) getParam ("fx_stutter_div"));
        const auto length = (int) ((60.0 / juce::jmax (20.0, currentBpm.load())) * beats * currentSampleRate);

        stutterLength = juce::jlimit (16, juce::jmax (16, stutterBuffer.getNumSamples()), length);
        stutterWrite = 0;
        stutterRead = 0;
        stutterPosition = 0.0;
        stutterRecording = true;
    }
    else if (! stutterOn && stutterWasOn)
    {
        stutterRecording = false;
    }

    stutterWasOn = stutterOn;

    if (stutterOn && stutterLength > 0 && stutterLength <= stutterBuffer.getNumSamples())
    {
        const auto mix = getParam ("fx_stutter_mix");
        const auto reverse = getParam ("fx_stutter_reverse") > 0.5f;
        const auto channels = juce::jmin (2, numChannels);
        stutterRate = (float) std::exp2 (getParam ("fx_stutter_pitch") / 12.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            if (stutterRecording)
            {
                for (int channel = 0; channel < channels; ++channel)
                    stutterBuffer.setSample (channel, stutterWrite, buffer.getSample (channel, i));

                if (++stutterWrite >= stutterLength)
                {
                    stutterWrite = 0;
                    stutterRead = 0;
                    stutterPosition = 0.0;
                    stutterRecording = false;
                }
            }
            else
            {
                const auto readPosition = reverse
                                              ? (double) (stutterLength - 1) - stutterPosition
                                              : stutterPosition;
                const auto index = juce::jlimit (0, stutterLength - 1, (int) readPosition);
                const auto next = (index + 1) % stutterLength;
                const auto frac = (float) (readPosition - (double) index);

                for (int channel = 0; channel < channels; ++channel)
                {
                    const auto value = buffer.getSample (channel, i);
                    const auto held = stutterBuffer.getSample (channel, index)
                                      + (stutterBuffer.getSample (channel, next)
                                         - stutterBuffer.getSample (channel, index)) * frac;
                    buffer.setSample (channel, i, value + (held - value) * mix);
                }

                stutterPosition += (double) stutterRate;

                if (stutterPosition >= (double) stutterLength)
                    stutterPosition -= (double) stutterLength;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processSmear (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto smearOn = getParam ("fx_smear_on") > 0.5f;
    const auto smearMix = juce::jlimit (0.0f, 1.0f, getParam ("fx_smear_mix") + getFxMod (Mod::Destination::FxSmearMix, 1.0f));
    const auto smearChannels = juce::jmin (2, numChannels);

    for (int channel = 0; channel < smearChannels; ++channel)
    {
        smear[channel].setParams (getParam ("fx_smear_size"), getParam ("fx_smear_density"));

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            smear[channel].push (data[i]);

            if (smearOn && smearMix > 0.001f)
            {
                const auto wet = smear[channel].process();
                data[i] = data[i] * (1.0f - smearMix) + wet * smearMix;
            }
        }
    }

}

void IlanaSynthAudioProcessor::processFreeze (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    const auto freezeOn = getParam ("fx_freeze_on") > 0.5f;
    const auto freezeMix = juce::jlimit (0.0f, 1.0f, getParam ("fx_freeze_mix") + getFxMod (Mod::Destination::FxFreezeMix, 1.0f));

    for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
        freeze[channel].process (buffer.getWritePointer (channel), numSamples, freezeOn,
                                 freezeOn ? freezeMix : 0.0f);

}

void IlanaSynthAudioProcessor::processReverb (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (getParam ("fx_reverb_on") > 0.5f)
    {
        const auto type = juce::jlimit (0, 6, (int) getParam ("fx_reverb_type"));

        if (type == 6 && reverbIrLoaded.load())
        {
            if (reverbScratch.getNumChannels() < numChannels || reverbScratch.getNumSamples() < numSamples)
                reverbScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                reverbScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            juce::dsp::AudioBlock<float> block (reverbScratch);
            juce::dsp::ProcessContextReplacing<float> context (block);
            convolution.process (context);

            const auto wet = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));

            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addFrom (channel, 0, reverbScratch, channel, 0, numSamples, wet);

            return;
        }

        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_size") + getFxMod (Mod::Destination::FxReverbSize, 1.0f));
        reverbParams.damping = getParam ("fx_reverb_damping");
        reverbParams.width = getParam ("fx_reverb_width");
        reverbParams.wetLevel = juce::jlimit (0.0f, 1.0f, getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f));
        reverbParams.dryLevel = 1.0f - reverbParams.wetLevel;
        reverbParams.freezeMode = 0.0f;

        switch (type)
        {
            case 1: // Hall: bigger, smoother
                reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, reverbParams.roomSize * 0.5f + 0.5f);
                reverbParams.damping = reverbParams.damping * 0.8f;
                break;

            case 2: // Plate: bright and wide
                reverbParams.damping = reverbParams.damping * 0.35f;
                reverbParams.width = 1.0f;
                break;

            case 4: // Spring: bright with a comb-y spring tank in front
                reverbParams.damping = reverbParams.damping * 0.25f;
                break;

            case 5: // Gated: long tank, wet-only, gated after the fact
                reverbParams.roomSize = juce::jlimit (0.0f, 1.0f, reverbParams.roomSize * 0.4f + 0.6f);
                reverbParams.wetLevel = 1.0f;
                reverbParams.dryLevel = 0.0f;
                break;

            default:
                break;
        }

        if (type == 4)
        {
            const auto springDelay = (float) (currentSampleRate * 0.008);
            springComb.setDelay (springDelay);

            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto delayed = springComb.popSample (channel, springDelay, true);
                    springComb.pushSample (channel, data[i] + delayed * 0.72f);
                    data[i] += delayed * 0.4f;
                }
            }
        }

        reverb.setParameters (reverbParams);

        if (type == 5)
        {
            if (reverbScratch.getNumChannels() < numChannels || reverbScratch.getNumSamples() < numSamples)
                reverbScratch.setSize (numChannels, numSamples, false, false, true);

            for (int channel = 0; channel < numChannels; ++channel)
                reverbScratch.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            if (numChannels >= 2)
                reverb.processStereo (reverbScratch.getWritePointer (0), reverbScratch.getWritePointer (1), numSamples);
            else
                reverb.processMono (reverbScratch.getWritePointer (0), numSamples);

            const auto gateCoefficient = (float) std::exp (-1.0 / (0.06 * currentSampleRate));
            const auto gateGain = getParam ("fx_reverb_mix") + getFxMod (Mod::Destination::FxReverbMix, 1.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                auto dryLevel = 0.0f;

                for (int channel = 0; channel < numChannels; ++channel)
                    dryLevel = juce::jmax (dryLevel, std::abs (buffer.getSample (channel, i)));

                if (dryLevel > 0.02f)
                    gatedReverbEnvelope = 1.0f;
                else
                    gatedReverbEnvelope *= gateCoefficient;

                for (int channel = 0; channel < numChannels; ++channel)
                    buffer.addSample (channel, i, reverbScratch.getSample (channel, i) * gatedReverbEnvelope * gateGain);
            }
        }
        else
        {
            if (numChannels >= 2)
                reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
            else
                reverb.processMono (buffer.getWritePointer (0), numSamples);
        }

        if (type == 3)
        {
            // Shimmer: add an octave-up granulated layer on top of the verb
            for (int channel = 0; channel < juce::jmin (2, numChannels); ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                shimmerShift[channel].setRatio (2.0);

                for (int i = 0; i < numSamples; ++i)
                {
                    shimmerShift[channel].push (data[i]);
                    data[i] += shimmerShift[channel].process() * 0.28f;
                }
            }
        }
    }
}

void IlanaSynthAudioProcessor::processAmp (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto mode = juce::jlimit (0, 2, (int) getParam ("fx_amp_mode"));
    const auto drive = getParam ("fx_amp_drive");
    const auto bass = getParam ("fx_amp_bass");
    const auto mid = getParam ("fx_amp_mid");
    const auto treble = getParam ("fx_amp_treble");
    const auto level = getParam ("fx_amp_level");

    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 250.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2200.0 / currentSampleRate);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            auto value = data[i] * drive;

            switch (mode)
            {
                case 1:  value = std::tanh (value * 1.6f); break;
                case 2:  value = value / (1.0f + std::abs (value) * 0.6f); break;
                default: value = std::tanh (value + 0.15f) - 0.1489f; break;
            }

            ampLowState[channel] += (value - ampLowState[channel]) * lowCoefficient;
            ampHighState[channel] += (value - ampHighState[channel]) * highCoefficient;

            const auto low = ampLowState[channel];
            const auto high = value - ampHighState[channel];
            const auto mids = value - low - high;

            data[i] = (low * bass + mids * mid + high * treble) * level;
        }
    }
}

void IlanaSynthAudioProcessor::processCompressor (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    const auto threshold = juce::Decibels::decibelsToGain (getParam ("fx_comp_threshold"));
    const auto ratio = juce::jmax (1.0f, getParam ("fx_comp_ratio"));
    const auto attackMs = juce::jmax (0.1f, getParam ("fx_comp_attack"));
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_comp_release"));
    const auto attackCoefficient = (float) std::exp (-1.0 / ((double) attackMs * 0.001 * currentSampleRate));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));
    const auto makeup = juce::Decibels::decibelsToGain (getParam ("fx_comp_makeup"));
    const auto mix = getParam ("fx_comp_mix");

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        auto envelope = compEnvelope[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto magnitude = std::abs (data[i]);
            const auto coefficient = magnitude > envelope ? attackCoefficient : releaseCoefficient;
            envelope = magnitude + (envelope - magnitude) * coefficient;

            auto gain = 1.0f;

            if (envelope > threshold)
                gain = std::pow (envelope / threshold, 1.0f / ratio - 1.0f);

            const auto processed = data[i] * gain * makeup;
            data[i] = data[i] + (processed - data[i]) * mix;
        }

        compEnvelope[channel] = envelope;
    }
}

void IlanaSynthAudioProcessor::processHaas (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());

    if (numChannels < 2)
        return;

    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.055),
                                            (float) (getParam ("fx_haas_delay") * 0.001 * currentSampleRate));
    const auto mix = getParam ("fx_haas_mix");

    haasLine.setDelay (delaySamples);

    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto input = right[i];
        haasLine.pushSample (1, input);
        const auto delayed = haasLine.popSample (1);
        right[i] = input + (delayed - input) * mix;
    }
}

void IlanaSynthAudioProcessor::processFlanger (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 8.0f, getParam ("fx_flanger_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_flanger_depth"));
    const auto feedback = juce::jlimit (0.0f, 0.9f, getParam ("fx_flanger_feedback"));
    const auto mix = getParam ("fx_flanger_mix");
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto base = (float) (currentSampleRate * 0.0025);
            const auto modulated = base * (1.0f + depth * 0.8f
                                                     * (float) std::sin (juce::MathConstants<double>::twoPi * flangerPhase
                                                                         + channel * 0.6));
            const auto delayed = flangerLine.popSample (channel, modulated, true);
            flangerLine.pushSample (channel, data[i] + delayed * feedback);
            data[i] = data[i] + (delayed - data[i]) * mix;
        }

        flangerPhase += increment;

        if (flangerPhase >= 1.0)
            flangerPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processDimension (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto rate = juce::jlimit (0.05f, 4.0f, getParam ("fx_dim_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_dim_depth"));
    const auto mix = getParam ("fx_dim_mix");
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto base = (float) (currentSampleRate * (channel == 0 ? 0.012 : 0.019));
            const auto modulated = base * (1.0f + depth * 0.25f
                                                     * (float) std::sin (juce::MathConstants<double>::twoPi * dimPhase
                                                                         + channel * 2.1));
            const auto delayed = dimLine.popSample (channel, modulated, true);
            dimLine.pushSample (channel, data[i]);
            data[i] = data[i] + (delayed - data[i]) * mix * 0.8f;
        }

        dimPhase += increment;

        if (dimPhase >= 1.0)
            dimPhase -= 1.0;
    }
}

float IlanaSynthAudioProcessor::gatePatternLevel (int pattern, int step)
{
    static constexpr juce::uint16 gatePatterns[8] = {
        0xFFFF, 0xAAAA, 0x9249, 0xEEEE, 0x0000, 0x3333, 0xF0F0, 0x0F0F
    };

    return (gatePatterns[juce::jlimit (0, 7, pattern)] & (1u << (step & 15))) != 0 ? 1.0f : 0.0f;
}

// Trance gate. DIV is the length of one step; STEPS the pattern length; the
// pattern is one of the built-ins or Custom (16 drawable step levels).
// Locked to the host's beat position while it plays.
void IlanaSynthAudioProcessor::processGate (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    // M7.1: with Euclid on and aimed at the Trance Gate, the gate plays the
    // Euclid rhythm at Euclid's rate and length.
    const auto euclid = getParam ("euc_on") > 0.5f && (int) getParam ("euc_target") == 2;
    const auto stepBeats = juce::jmax (0.001, getSyncDivisionBeats ((int) getParam (euclid ? "euc_div" : "fx_gate_div")));
    const auto stepsPerSample = (currentBpm.load() / 60.0) / stepBeats / currentSampleRate;
    const auto pattern = juce::jlimit (0, 8, (int) getParam ("fx_gate_pattern"));
    const auto steps = euclid ? juce::jlimit (2, 32, (int) getParam ("euc_steps"))
                              : juce::jlimit (2, 16, (int) getParam ("fx_gate_steps"));
    const auto euclidHits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto euclidRotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto swing = (double) juce::jlimit (0.0f, 0.5f, getParam ("fx_gate_swing"));
    const auto smooth = juce::jlimit (0.0f, 1.0f, getParam ("fx_gate_smooth"));
    const auto mix = getParam ("fx_gate_mix");

    // SMOOTH sets the edge times: quick clicks-free edges up to soft swells.
    const auto attackCoeff = 1.0f - std::exp (-1.0f / ((0.0005f + smooth * 0.03f) * (float) currentSampleRate));
    const auto releaseCoeff = 1.0f - std::exp (-1.0f / ((0.001f + smooth * 0.12f) * (float) currentSampleRate));

    std::array<float, 16> levels {};

    for (int step = 0; step < 16; ++step)
        levels[(size_t) step] = pattern == 8 ? getParam (gateStepIds[(size_t) step])
                                             : gatePatternLevel (pattern, step);

    // Position in steps: from the host while playing, else free-running.
    auto position = hostPlaying.load() ? hostPpq.load() / stepBeats : gatePhase;

    for (int i = 0; i < numSamples; ++i)
    {
        // Swing pushes every second step later within its pair.
        // (floor-based, so hosts that report negative pre-roll positions work)
        const auto pairStart = 2.0 * std::floor (position * 0.5);
        const auto pair = position - pairStart;
        const auto stepCount = (long long) pairStart + (pair < 1.0 + swing ? 0 : 1);
        const auto step = (int) (((stepCount % steps) + steps) % steps);

        if (step != gateLastStep)
        {
            // New cycle: re-roll Random, advance Build/Break once per cycle.
            if (step == 0)
            {
                gateRandomMask = (juce::uint16) lfoRandom.nextInt (0x10000);
                gateCycleCount = (gateCycleCount + 1) % 32;
            }

            gateLastStep = step;
        }

        auto level = euclid ? (euclidHit (step, euclidHits, steps, euclidRotate) ? 1.0f : 0.0f)
                            : levels[(size_t) (step & 15)];

        if (! euclid && pattern == 4)
            level = (gateRandomMask & (1u << step)) != 0 ? 1.0f : 0.0f;
        else if (! euclid && pattern == 6)
            level = step <= gateCycleCount % steps ? 1.0f : 0.0f;
        else if (! euclid && pattern == 7)
            level = step >= gateCycleCount % steps ? 1.0f : 0.0f;

        gateEnvelope += (level - gateEnvelope) * (level > gateEnvelope ? attackCoeff : releaseCoeff);
        const auto applied = 1.0f - (1.0f - gateEnvelope) * mix;

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer (channel)[i] *= applied;

        position += stepsPerSample;
    }

    gatePhase = std::fmod (position, 64.0);
    gateDisplayStep = gateLastStep;

    if (euclid)
        euclidDisplayStep.store (gateLastStep);
}

void IlanaSynthAudioProcessor::processTapeStop (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto size = tapeStopBuffer.getNumSamples();

    if (size < 16)
        return;

    const auto time = juce::jmax (0.05f, getParam ("fx_tape_stop_time"));
    const auto mix = getParam ("fx_tape_stop_mix");
    const auto target = getParam ("fx_tape_stop_trigger") > 0.5f ? 0.0f : 1.0f;
    const auto rateStep = 1.0f / (time * (float) currentSampleRate);
    const auto fadeStep = 1.0f / (0.03f * (float) currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        if (tapeStopRate < target)
            tapeStopRate = juce::jmin (target, tapeStopRate + rateStep);
        else
            tapeStopRate = juce::jmax (target, tapeStopRate - rateStep);

        // Slowing down leaves the tape behind the input. Once it is back up
        // to speed, crossfade to the live signal and re-lock the read head,
        // or that lag would stay as permanent latency.
        auto lag = (double) tapeStopWrite - tapeStopRead;

        if (lag < 0.0)
            lag += (double) size;

        const auto catchingUp = target >= 1.0f && tapeStopRate >= 1.0f && lag > 0.5;

        if (catchingUp)
            tapeStopLagFade = juce::jmax (0.0f, tapeStopLagFade - fadeStep);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            tapeStopBuffer.setSample (channel, tapeStopWrite, data[i]);

            const auto index = (int) tapeStopRead;
            const auto next = (index + 1) % size;
            const auto frac = (float) (tapeStopRead - (double) index);
            auto wet = tapeStopBuffer.getSample (channel, index)
                       + (tapeStopBuffer.getSample (channel, next) - tapeStopBuffer.getSample (channel, index)) * frac;

            if (catchingUp)
                wet = data[i] + (wet - data[i]) * tapeStopLagFade;

            data[i] = data[i] + (wet - data[i]) * mix;
        }

        if (catchingUp && tapeStopLagFade <= 0.0f)
        {
            tapeStopRead = (double) tapeStopWrite; // advanced below with the write head
            tapeStopLagFade = 1.0f;
        }

        tapeStopWrite = (tapeStopWrite + 1) % size;
        tapeStopRead += (double) tapeStopRate;

        while (tapeStopRead >= (double) size)
            tapeStopRead -= (double) size;
    }
}

void IlanaSynthAudioProcessor::processTilt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto tilt = juce::jlimit (-1.0f, 1.0f, getParam ("fx_tilt"));
    const auto level = juce::Decibels::decibelsToGain (getParam ("fx_tilt_level"));
    const auto lowGain = juce::Decibels::decibelsToGain (-tilt * 12.0f) * level;
    const auto highGain = juce::Decibels::decibelsToGain (tilt * 12.0f) * level;
    const auto coefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 700.0 / currentSampleRate);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            tiltLowState[channel] += (data[i] - tiltLowState[channel]) * coefficient;
            const auto low = tiltLowState[channel];
            const auto high = data[i] - low;
            data[i] = low * lowGain + high * highGain;
        }
    }
}

EqSettings IlanaSynthAudioProcessor::getEqSettings() const
{
    EqSettings settings;
    settings.lowFreq = getParam ("fx_eq_low_freq");
    settings.lowGain = getParam ("fx_eq_low_gain");
    settings.midFreq = getParam ("fx_eq_mid_freq");
    settings.midGain = getParam ("fx_eq_mid_gain");
    settings.midQ = getParam ("fx_eq_mid_q");
    settings.highFreq = getParam ("fx_eq_high_freq");
    settings.highGain = getParam ("fx_eq_high_gain");
    return settings;
}

void IlanaSynthAudioProcessor::processEq (juce::AudioBuffer<float>& buffer)
{
    Biquad::Coefficients coefficients[3];
    getEqSettings().makeCoefficients (currentSampleRate, coefficients);

    const auto channels = juce::jmin (2, buffer.getNumChannels());

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int band = 0; band < 3; ++band)
        {
            auto& filter = eqBands[channel][band];
            filter.setCoefficients (coefficients[band]);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                data[i] = filter.process (data[i]);
        }
    }
}

void IlanaSynthAudioProcessor::processUtility (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto gain = juce::Decibels::decibelsToGain (getParam ("fx_util_gain"));
    const auto mono = getParam ("fx_util_mono") > 0.5f;
    const auto invert = getParam ("fx_util_invert") > 0.5f;

    for (int channel = 0; channel < channels; ++channel)
        buffer.applyGain (channel, 0, numSamples, gain * (invert ? -1.0f : 1.0f));

    if (mono && channels > 1)
    {
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto average = (left[i] + right[i]) * 0.5f;
            left[i] = average;
            right[i] = average;
        }
    }
}

void IlanaSynthAudioProcessor::processOtt (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 1.0f, getParam ("fx_ott_amount"));
    const auto mix = getParam ("fx_ott_mix");
    const auto lowCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 200.0 / currentSampleRate);
    const auto highCoefficient = (float) juce::jlimit (0.0, 1.0, 2.0 * juce::MathConstants<double>::pi * 2000.0 / currentSampleRate);
    const auto envelopeCoefficient = (float) std::exp (-1.0 / (0.01 * currentSampleRate));

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = data[i];
            ottLowState[channel][0] += (input - ottLowState[channel][0]) * lowCoefficient;
            const auto low = ottLowState[channel][0];
            const auto rest = input - low;
            ottHighState[channel][0] += (rest - ottHighState[channel][0]) * highCoefficient;
            const auto mid = ottHighState[channel][0];
            const auto high = rest - mid;

            const float bands[3] { low, mid, high };
            auto output = 0.0f;

            for (int band = 0; band < 3; ++band)
            {
                const auto magnitude = std::abs (bands[band]);
                auto& envelope = ottEnvelope[channel][band];
                envelope = magnitude + (envelope - magnitude) * envelopeCoefficient;

                const auto gain = juce::jlimit (0.25f, 4.0f, std::pow (envelope + 0.001f, -0.6f * amount));
                output += bands[band] * gain;
            }

            data[i] = input + (output - input) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processLimiter (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto ceiling = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 0.0f, getParam ("fx_limit_ceiling")));
    const auto releaseMs = juce::jmax (1.0f, getParam ("fx_limit_release"));
    const auto releaseCoefficient = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * currentSampleRate));

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto magnitude = std::abs (data[i]);

            if (magnitude > limiterEnvelope[channel])
                limiterEnvelope[channel] = magnitude;
            else
                limiterEnvelope[channel] = magnitude + (limiterEnvelope[channel] - magnitude) * releaseCoefficient;

            const auto gain = limiterEnvelope[channel] > ceiling ? ceiling / limiterEnvelope[channel] : 1.0f;
            data[i] *= gain;
        }
    }
}

void IlanaSynthAudioProcessor::processWidener (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    const auto numSamples = buffer.getNumSamples();
    const auto width = juce::jlimit (0.0f, 2.0f, getParam ("fx_width"));
    const auto mix = getParam ("fx_width_mix");
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid = (left[i] + right[i]) * 0.5f;
        const auto side = (left[i] - right[i]) * 0.5f * width;
        const auto wetL = mid + side;
        const auto wetR = mid - side;
        left[i] = left[i] + (wetL - left[i]) * mix;
        right[i] = right[i] + (wetR - right[i]) * mix;
    }
}

void IlanaSynthAudioProcessor::processTremolo (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const auto rate = juce::jlimit (0.05f, 20.0f, getParam ("fx_trem_rate"));
    const auto depth = juce::jlimit (0.0f, 1.0f, getParam ("fx_trem_depth"));
    const auto shape = juce::jlimit (0, 5, (int) getParam ("fx_trem_shape"));
    const auto increment = (double) rate / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto value = lfoShapeValue (shape, tremoloPhase);
        const auto gain = 1.0f - depth * (0.5f - 0.5f * value);

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.getWritePointer (channel)[i] *= gain;

        tremoloPhase += increment;

        if (tremoloPhase >= 1.0)
            tremoloPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processFreqShift (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto shiftHz = juce::jlimit (-2000.0f, 2000.0f, getParam ("fx_shifter_shift"));
    const auto mix = getParam ("fx_shifter_mix");
    const auto increment = (double) shiftHz / currentSampleRate;
    constexpr auto coefficient = 0.6f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto phaseSin = (float) std::sin (juce::MathConstants<double>::twoPi * shifterPhase);
        const auto phaseCos = (float) std::cos (juce::MathConstants<double>::twoPi * shifterPhase);

        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto input = data[i];

            // First-order allpass quadrature approximation
            const auto quadrature = -coefficient * input + shifterDelay[channel] + coefficient * shifterAllpass[channel];
            shifterDelay[channel] = input;
            shifterAllpass[channel] = quadrature;

            const auto shifted = input * phaseCos - quadrature * phaseSin;
            data[i] = input + (shifted - input) * mix;
        }

        shifterPhase += increment;

        if (shifterPhase >= 1.0)
            shifterPhase -= 1.0;
        else if (shifterPhase < 0.0)
            shifterPhase += 1.0;
    }
}

void IlanaSynthAudioProcessor::processRingMod (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    const auto frequency = juce::jlimit (1.0f, 5000.0f, getParam ("fx_ring_freq"));
    const auto mix = getParam ("fx_ring_mix");
    const auto increment = (double) frequency / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto carrier = (float) std::sin (juce::MathConstants<double>::twoPi * ringPhase);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            data[i] = data[i] + (data[i] * carrier - data[i]) * mix;
        }

        ringPhase += increment;

        if (ringPhase >= 1.0)
            ringPhase -= 1.0;
    }
}

void IlanaSynthAudioProcessor::processOctaver (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto mix = getParam ("fx_octaver_mix");

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        octaverShift[channel].setRatio (0.5);

        for (int i = 0; i < numSamples; ++i)
        {
            octaverShift[channel].push (data[i]);
            const auto shifted = octaverShift[channel].process();
            data[i] = data[i] + (shifted - data[i]) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processVowel (juce::AudioBuffer<float>& buffer)
{
    static const float formant1[5] { 800.0f, 400.0f, 350.0f, 450.0f, 325.0f };
    static const float formant2[5] { 1150.0f, 1600.0f, 1700.0f, 800.0f, 700.0f };

    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto morph = juce::jlimit (0.0f, 1.0f, getParam ("fx_vowel_morph"));
    const auto mix = getParam ("fx_vowel_mix");
    const auto position = morph * 4.0f;
    const auto index = juce::jlimit (0, 3, (int) position);
    const auto frac = position - (float) index;
    const auto f1 = formant1[index] + (formant1[index + 1] - formant1[index]) * frac;
    const auto f2 = formant2[index] + (formant2[index + 1] - formant2[index]) * frac;

    for (int channel = 0; channel < channels; ++channel)
    {
        vowelFilters[channel][0].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][1].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][2].setMode (Svf::Mode::BandPass);
        vowelFilters[channel][0].setCutoff (f1);
        vowelFilters[channel][0].setResonance (0.82);
        vowelFilters[channel][1].setCutoff (f2);
        vowelFilters[channel][1].setResonance (0.82);
        vowelFilters[channel][2].setCutoff (juce::jlimit (100.0, currentSampleRate * 0.45, f2 * 2.4));
        vowelFilters[channel][2].setResonance (0.8);

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = data[i];
            const auto wet = vowelFilters[channel][0].processSample (input)
                             + vowelFilters[channel][1].processSample (input) * 0.7f
                             + vowelFilters[channel][2].processSample (input) * 0.35f;
            data[i] = input + (wet - input) * mix;
        }
    }
}

void IlanaSynthAudioProcessor::processFeedback (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    const auto amount = juce::jlimit (0.0f, 0.95f, getParam ("fx_feedback_amount"));
    const auto delaySamples = juce::jlimit (1.0f, (float) (currentSampleRate * 0.11),
                                            (float) (getParam ("fx_feedback_delay") * 0.001 * currentSampleRate));
    const auto tone = juce::jlimit (0.0f, 1.0f, getParam ("fx_feedback_tone"));
    const auto mix = getParam ("fx_feedback_mix");
    const auto coefficient = 1.0f - tone * 0.9f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);
            const auto delayed = feedbackLine.popSample (channel, delaySamples, true);

            feedbackState[channel] += (delayed - feedbackState[channel]) * coefficient;
            feedbackLine.pushSample (channel, data[i] + feedbackState[channel] * amount);
            data[i] += delayed * mix;
        }
    }
}

int IlanaSynthAudioProcessor::selectArpNote (int mode, int octaves)
{
    const auto count = arpHeldNotes.size();

    if (count == 0)
        return 60;

    const auto total = count * octaves;

    // Chord: all held notes stacked through the octaves, played together.
    if (mode == 7)
    {
        arpChordNotes.clear();

        for (int octave = 0; octave < octaves; ++octave)
            for (int i = 0; i < count; ++i)
                arpChordNotes.add (arpHeldNotes[i] + 12 * octave);

        return arpChordNotes[0];
    }

    if (mode == 3)
        return arpHeldNotes[arpRandom.nextInt (count)] + 12 * arpRandom.nextInt (octaves);

    // Scale Random: any scale note from the lowest held note up through the
    // octaves (random held notes when no scale is set).
    if (mode == 8)
    {
        const auto scale = (int) getParam ("gen_scale");

        if (scale <= 0)
            return arpHeldNotes[arpRandom.nextInt (count)] + 12 * arpRandom.nextInt (octaves);

        const auto low = arpHeldNotes[0];
        const auto span = juce::jmax (12 * octaves, arpHeldNotes[count - 1] - low + 1);
        return Scales::quantize (low + arpRandom.nextInt (span), scale, (int) getParam ("gen_root"));
    }

    if (total <= 1)
        return arpHeldNotes[0];

    auto index = 0;

    switch (mode)
    {
        case 2: // UpDown
        case 4: // DownUp (same zigzag, flipped)
        {
            arpStepIndex = juce::jlimit (0, total - 1, arpStepIndex);
            index = mode == 2 ? arpStepIndex : total - 1 - arpStepIndex;

            arpStepIndex += arpDirection;

            if (arpStepIndex >= total - 1 || arpStepIndex <= 0)
                arpDirection = -arpDirection;

            arpStepIndex = juce::jlimit (0, total - 1, arpStepIndex);
            break;
        }

        case 5: // Converge: low, high, 2nd low, 2nd high...
            index = (arpStepIndex % 2 == 0) ? (arpStepIndex / 2) : (total - 1 - arpStepIndex / 2);
            arpStepIndex = (arpStepIndex + 1) % total;
            break;

        case 6: // Walk: random walk up and down.
            arpStepIndex = (arpStepIndex + (arpRandom.nextBool() ? 1 : total - 1)) % total;
            index = arpStepIndex;
            break;

        default: // 0 Up, 1 Down
            index = mode == 1 ? total - 1 - (arpStepIndex % total) : (arpStepIndex % total);
            arpStepIndex = (arpStepIndex + 1) % total;
            break;
    }

    index = juce::jlimit (0, total - 1, index);
    return arpHeldNotes[index % count] + 12 * (index / count);
}

void IlanaSynthAudioProcessor::processArpeggiator (juce::MidiBuffer& midiMessages, int numSamples, juce::MidiBuffer& output)
{
    output.clear();

    const auto lastSample = juce::jmax (0, numSamples - 1);

    // One note engine serves three things (M7.1): the arpeggiator, the
    // probability sequencer (which takes over while on), and Euclid in Notes
    // mode, which rests the engine's off-beat steps. With only Euclid on,
    // the engine plays the held chord at Euclid's rate.
    const auto arpOn = getParam ("arp_on") > 0.5f;
    const auto pseqOn = getParam ("pseq_on") > 0.5f;
    const auto euclidNotes = getParam ("euc_on") > 0.5f && (int) getParam ("euc_target") == 0;
    const auto arpEnabled = arpOn || pseqOn || euclidNotes;
    const auto playing = hostPlaying.load();
    const auto transportStopped = arpHostWasPlaying && ! playing;
    arpHostWasPlaying = playing;

    const auto releaseSounding = [this, &output] (int position)
    {
        if (arpActiveNote >= 0)
            output.addEvent (juce::MidiMessage::noteOff (1, arpActiveNote), position);

        for (auto chordNote : arpChordActive)
            output.addEvent (juce::MidiMessage::noteOff (1, chordNote), position);

        arpChordActive.clearQuick();
        arpActiveNote = -1;
        arpGateRemaining = 0;
    };

    // Held keys are tracked even while the arp is off, so switching it on
    // over a held chord starts at once. All Notes Off / All Sound Off and a
    // host transport stop drop every held key: a clip or keyboard whose
    // note-offs never arrive must not leave the pattern running.
    const auto trackKeys = [this] (const juce::MidiMessage& message)
    {
        if (message.isNoteOn())
        {
            if (! arpHeldNotes.contains (message.getNoteNumber()))
            {
                arpHeldNotes.add (message.getNoteNumber());
                arpHeldNotes.sort();
            }
        }
        else if (message.isNoteOff())
        {
            arpHeldNotes.removeAllInstancesOf (message.getNoteNumber());
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            arpHeldNotes.clearQuick();
        }
    };

    if (transportStopped)
        arpHeldNotes.clearQuick();

    if (! arpEnabled)
    {
        if (arpWasEnabled)
        {
            releaseSounding (0);
            arpCounter = 0;
            arpWasEnabled = false;
        }

        for (const auto metadata : midiMessages)
            trackKeys (metadata.getMessage());

        output.addEvents (midiMessages, 0, numSamples, 0);
        return;
    }

    arpWasEnabled = true;

    const auto bpm = juce::jmax (20.0, currentBpm.load());
    auto beats = getSyncDivisionBeats ((int) getParam (pseqOn ? "pseq_div" : arpOn ? "arp_div" : "euc_div"));

    if (beats <= 0.0)
        beats = 0.5;

    const auto exactStep = (60.0 / bpm) * beats * currentSampleRate;
    const auto samplesPerStep = juce::jmax (16, (int) exactStep);
    const auto gate = juce::jlimit (0.05f, 1.0f, getParam (pseqOn ? "pseq_gate" : arpOn ? "arp_gate" : "euc_gate"));
    const auto gateSamples = juce::jmax (8, (int) ((float) samplesPerStep * gate));
    const auto octaves = arpOn && ! pseqOn ? juce::jlimit (1, 4, (int) getParam ("arp_octaves")) : 1;
    // Mode -1 is the probability sequencer; 7 (Chord) serves Euclid alone.
    const auto mode = pseqOn ? -1 : arpOn ? juce::jlimit (0, 8, (int) getParam ("arp_mode")) : 7;
    const auto chance = arpOn && ! pseqOn ? juce::jlimit (0.0f, 1.0f, getParam ("arp_chance")) : 1.0f;
    const auto euclidSteps = juce::jlimit (2, 32, (int) getParam ("euc_steps"));
    const auto euclidHits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto euclidRotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto pseqLength = juce::jlimit (1, 16, (int) getParam ("pseq_length"));
    const auto ppqAtBlockStart = hostPpq.load();

    // While the host plays, steps land on its beat grid. A step played
    // just before a grid line (a key pressed a little early) keeps that
    // line's slot instead of firing again straight after.
    const auto samplesToNextStep = [&] (int position)
    {
        if (! playing || exactStep < 1.0)
            return samplesPerStep;

        const auto ppq = ppqAtBlockStart + (double) position / currentSampleRate * (bpm / 60.0);
        const auto stepPosition = ppq / beats;
        auto toNext = (1.0 - (stepPosition - std::floor (stepPosition))) * exactStep;

        if (toNext < exactStep * 0.25)
            toNext += exactStep;

        return juce::jmax (16, juce::roundToInt (toNext));
    };

    if (transportStopped)
    {
        releaseSounding (0);
        arpCounter = 0;
    }

    // The step's number: from the host's beat grid while it plays (so
    // patterns line up with the bar), else counted from the first key.
    const auto stepNumber = [&] (int position)
    {
        if (! playing)
            return engineStepCount;

        const auto ppq = ppqAtBlockStart + (double) position / currentSampleRate * (bpm / 60.0);
        return (long long) std::llround (ppq / beats);
    };

    const auto triggerStep = [&] (int position)
    {
        releaseSounding (position);
        arpRatchetsLeft = 0;

        const auto number = stepNumber (position);
        ++engineStepCount;
        engineDisplayStep.store ((int) (number & 0xffffff));

        if (euclidNotes)
            euclidDisplayStep.store ((int) (((number % euclidSteps) + euclidSteps) % euclidSteps));

        if (euclidNotes && ! euclidHit ((int) (((number % euclidSteps) + euclidSteps) % euclidSteps),
                                        euclidHits, euclidSteps, euclidRotate))
        {
            // An off-beat of the Euclid rhythm: the step rests.
            arpGateRemaining = gateSamples;
            arpCounter = samplesToNextStep (position);
            return;
        }

        if (mode == -1)
        {
            // Probability sequencer: a held key, raised by up to RANGE and
            // snapped to the scale, played RATCHET times in the step.
            const auto step = (size_t) (((number % pseqLength) + pseqLength) % pseqLength);
            const auto stepChance = getParam (pseqChanceIds[step]);
            const auto range = juce::jlimit (0, 24, (int) getParam (pseqRangeIds[step]));
            const auto ratchet = juce::jlimit (1, 4, (int) getParam (pseqRatchetIds[step]));

            if (pseqRandom.nextFloat() < stepChance)
            {
                auto note = arpHeldNotes[pseqRandom.nextInt (arpHeldNotes.size())]
                            + (range > 0 ? pseqRandom.nextInt (range + 1) : 0);
                const auto scale = (int) getParam ("gen_scale");

                if (scale > 0)
                    note = Scales::quantize (note, scale, (int) getParam ("gen_root"));

                note = juce::jlimit (0, 127, note);
                const auto interval = juce::jmax (8, samplesPerStep / ratchet);
                output.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), position);
                arpActiveNote = note;
                arpGateRemaining = juce::jmax (4, (int) ((float) interval * gate));
                arpRatchetNote = note;
                arpRatchetsLeft = ratchet - 1;
                arpRatchetInterval = interval;
                arpRatchetCounter = interval;
            }
            else
            {
                arpGateRemaining = gateSamples;
            }

            arpCounter = samplesToNextStep (position);
            return;
        }

        const auto note = selectArpNote (mode, octaves);
        const auto rest = chance < 1.0f && arpRandom.nextFloat() >= chance;

        if (! rest)
        {
            if (mode == 7)
            {
                for (auto chordNote : arpChordNotes)
                {
                    output.addEvent (juce::MidiMessage::noteOn (1, chordNote, (juce::uint8) 100), position);
                    arpChordActive.add (chordNote);
                }
            }
            else
            {
                output.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), position);
                arpActiveNote = note;
            }
        }

        arpGateRemaining = gateSamples;
        arpCounter = samplesToNextStep (position);
    };

    // Runs the step clock over [from, to).
    const auto runSteps = [&] (int from, int to)
    {
        auto position = from;

        while (position < to)
        {
            const auto sounding = arpActiveNote >= 0 || ! arpChordActive.isEmpty();

            if (sounding && (arpGateRemaining <= 0 || arpHeldNotes.isEmpty()))
                releaseSounding (position);

            if (arpHeldNotes.isEmpty())
                return;

            if (arpCounter <= 0)
            {
                triggerStep (position);
            }
            else if (arpRatchetsLeft > 0 && arpRatchetCounter <= 0)
            {
                // The next repeat of a ratcheted step.
                releaseSounding (position);
                output.addEvent (juce::MidiMessage::noteOn (1, arpRatchetNote, (juce::uint8) 100), position);
                arpActiveNote = arpRatchetNote;
                arpGateRemaining = juce::jmax (4, (int) ((float) arpRatchetInterval * gate));
                arpRatchetCounter = arpRatchetInterval;
                --arpRatchetsLeft;
            }

            auto advance = juce::jmin (to - position, arpCounter);

            if ((arpActiveNote >= 0 || ! arpChordActive.isEmpty()) && arpGateRemaining > 0)
                advance = juce::jmin (advance, arpGateRemaining);

            if (arpRatchetsLeft > 0)
                advance = juce::jmin (advance, arpRatchetCounter);

            advance = juce::jmax (1, advance);
            arpCounter -= advance;
            arpGateRemaining -= advance;
            arpRatchetCounter -= advance;
            position += advance;
        }
    };

    // Walk the block event by event so releases and new keys act at the
    // sample they arrive on, not at the next block.
    auto position = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventPosition = juce::jlimit (0, lastSample, metadata.samplePosition);
        runSteps (position, eventPosition);
        position = juce::jmax (position, eventPosition);

        const auto message = metadata.getMessage();
        const auto wasEmpty = arpHeldNotes.isEmpty();
        trackKeys (message);

        if (! message.isNoteOnOrOff())
            output.addEvent (message, position);

        if (arpHeldNotes.isEmpty() && ! wasEmpty)
        {
            releaseSounding (position);
            arpCounter = 0;
            engineDisplayStep.store (-1);
        }
        else if (wasEmpty && ! arpHeldNotes.isEmpty())
        {
            arpCounter = 0;
            arpRatchetsLeft = 0;
            engineStepCount = 0;
        }
    }

    runSteps (position, numSamples);
}

// Euclid in Exciter mode: each hit re-strikes the Physical strings of the
// notes held down. The hit travels as a private SysEx marker, so it lands
// on its exact sample inside the synth's render (IlanaSynth::handleMidiEvent).
void IlanaSynthAudioProcessor::addEuclidExciterHits (juce::MidiBuffer& midi, int numSamples)
{
    if (getParam ("euc_on") < 0.5f || (int) getParam ("euc_target") != 1 || numSamples <= 0)
    {
        euclidExciterLastStep = -1;
        return;
    }

    const auto bpm = juce::jmax (20.0, currentBpm.load());
    const auto beats = juce::jmax (0.001, getSyncDivisionBeats ((int) getParam ("euc_div")));
    const auto stepsPerSample = (bpm / 60.0) / beats / currentSampleRate;
    const auto steps = juce::jlimit (2, 32, (int) getParam ("euc_steps"));
    const auto hits = juce::jlimit (0, 32, (int) getParam ("euc_hits"));
    const auto rotate = juce::jlimit (0, 31, (int) getParam ("euc_rotate"));
    const auto start = hostPlaying.load() ? hostPpq.load() / beats : euclidExciterPhase;

    // A jump back (a loop or a relocate) starts counting again.
    if ((double) euclidExciterLastStep > start + 1.0)
        euclidExciterLastStep = (long long) std::floor (start) - 1;

    const auto end = start + (double) numSamples * stepsPerSample;

    for (auto k = (long long) std::ceil (start - 1.0e-9); (double) k < end; ++k)
    {
        if (k <= euclidExciterLastStep)
            continue;

        euclidExciterLastStep = k;
        euclidDisplayStep.store ((int) (((k % steps) + steps) % steps));
        const auto offset = juce::jlimit (0, numSamples - 1, (int) std::ceil (((double) k - start) / stepsPerSample));

        if (euclidHit ((int) (((k % steps) + steps) % steps), hits, steps, rotate))
        {
            const juce::uint8 marker[] { 0xf0, IlanaSynth::exciterMarker[0], IlanaSynth::exciterMarker[1],
                                         IlanaSynth::exciterMarker[2], 100, 0xf7 };
            midi.addEvent (marker, (int) sizeof (marker), offset);
        }
    }

    euclidExciterPhase = std::fmod (end, 4096.0);
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
    if (oscIndex < 0 || oscIndex >= numSampleOscs)
        return false;

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

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        auto& retired = retiredSamples[(size_t) oscIndex];
        auto& index = retiredIndex[(size_t) oscIndex];
        retired[(size_t) index] = std::move (sampleSlots[(size_t) oscIndex]);
        index = (index + 1) % (int) retired.size();
        sampleSlots[(size_t) oscIndex] = std::move (data);
    }

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        samplePaths[(size_t) oscIndex] = file.getFullPathName();
    }

    return true;
}

bool IlanaSynthAudioProcessor::loadUserWavetable (int slot, const juce::File& file, Wavetable::LoadMode mode)
{
    if (slot < 0 || slot >= numUserSlots)
        return false;

    WavetableDoc doc;
    if (! WavetableDoc::loadFromFile (file, doc, mode))
        return false;

    return setUserTable (slot, doc);
}

void IlanaSynthAudioProcessor::swapUserTable (int slot, std::shared_ptr<Wavetable> table)
{
    const juce::SpinLock::ScopedLockType lock (tableLock);
    auto& index = retiredTableIndex[(size_t) slot];
    retiredTables[(size_t) (slot * 3 + index)] = std::move (userTables[(size_t) slot]);
    index = (index + 1) % 3;
    userTables[(size_t) slot] = std::move (table);
}

bool IlanaSynthAudioProcessor::setUserTable (int slot, const WavetableDoc& doc)
{
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

juce::StringArray IlanaSynthAudioProcessor::getFactoryPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : Presets::getFactoryPresets())
        names.add (preset.name);

    return names;
}

juce::StringArray IlanaSynthAudioProcessor::getFactoryPresetCategories() const
{
    return Presets::getFactoryPresetCategories();
}

juce::Array<juce::File> IlanaSynthAudioProcessor::getUserPresetFiles() const
{
    juce::Array<juce::File> files;

    const auto directory = getUserPresetDirectory();

    if (directory.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.ilanapreset"))
            files.add (entry.getFile());
    }

    files.sort();
    return files;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetNames() const
{
    auto names = getFactoryPresetNames();

    for (const auto& file : getUserPresetFiles())
        names.add (file.getFileNameWithoutExtension());

    return names;
}

const IlanaSynthAudioProcessor::UserPresetMeta& IlanaSynthAudioProcessor::getUserPresetMeta (const juce::File& file) const
{
    const auto key = file.getFullPathName();
    const auto modified = file.getLastModificationTime().toMilliseconds();
    auto& entry = userPresetMetaCache[key.toStdString()];

    if (entry.modified != modified)
    {
        entry.modified = modified;
        entry.category = "User";
        entry.tags.clear();

        // Only the root element's attributes are needed.
        if (auto xml = juce::XmlDocument (file).getDocumentElementIfTagMatches (apvts.state.getType().toString()))
        {
            const auto category = xml->getStringAttribute ("presetCategory").trim();

            if (category.isNotEmpty())
                entry.category = category;

            entry.tags = xml->getStringAttribute ("presetTags").trim();
        }
    }

    return entry;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetCategories() const
{
    auto categories = getFactoryPresetCategories();

    for (const auto& file : getUserPresetFiles())
        categories.add (getUserPresetMeta (file).category);

    return categories;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetTags() const
{
    juce::StringArray tags;

    for (int i = 0; i < getFactoryPresetCategories().size(); ++i)
        tags.add ({});

    for (const auto& file : getUserPresetFiles())
        tags.add (getUserPresetMeta (file).tags);

    return tags;
}

int IlanaSynthAudioProcessor::getNumAllPresets() const
{
    return (int) Presets::getFactoryPresets().size() + getUserPresetFiles().size();
}

void IlanaSynthAudioProcessor::loadPresetByIndex (int index)
{
    const auto factoryCount = (int) Presets::getFactoryPresets().size();

    if (index < 0)
        return;

    if (index < factoryCount)
    {
        loadFactoryPreset (index);
        return;
    }

    const auto files = getUserPresetFiles();
    const auto userIndex = index - factoryCount;

    if (userIndex < files.size())
        loadPresetFromFile (files[userIndex]);
}

void IlanaSynthAudioProcessor::handleAsyncUpdate()
{
    const auto program = pendingProgramChange.exchange (-1);

    if (program >= 0)
    {
        const auto count = Presets::getFactoryPresets().size();

        if (count > 0)
            loadFactoryPreset (program % (int) count);
    }

    if (macrosPending.exchange (false))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (auto* parameter = apvts.getParameter ("macro" + juce::String (i + 1)))
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (pendingMacros[i].load()));
        }
    }

    bool reloadSamples = false;
    bool reloadTables = false;
    std::array<bool, (size_t) numSampleOscs> clearSamples {};
    std::array<bool, (size_t) numUserSlots> clearTables {};

    setOversampling (wantedOversamplingFactor());

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        reloadSamples = samplesReloadPending;
        samplesReloadPending = false;
        reloadTables = userTablesReloadPending;
        userTablesReloadPending = false;
        clearSamples = pendingSampleClear;
        pendingSampleClear = {};
        clearTables = pendingUserTableClear;
        pendingUserTableClear = {};
    }

    if (reloadSamples || clearSamples != std::array<bool, (size_t) numSampleOscs> {})
    {
        for (int i = 0; i < numSampleOscs; ++i)
        {
            juce::String path;

            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                path = pendingSamplePaths[(size_t) i];
                pendingSamplePaths[(size_t) i].clear();
            }

            if (clearSamples[(size_t) i])
            {
                {
                    const juce::SpinLock::ScopedLockType lock (sampleLock);
                    auto& retired = retiredSamples[(size_t) i];
                    auto& index = retiredIndex[(size_t) i];
                    retired[(size_t) index] = std::move (sampleSlots[(size_t) i]);
                    index = (index + 1) % (int) retired.size();
                }

                const juce::SpinLock::ScopedLockType lock (stateLock);
                samplePaths[(size_t) i].clear();
                continue;
            }

            if (path.isEmpty())
                continue;

            const juce::File file (path);

            if (file.existsAsFile())
                loadUserSample (i, file);
        }
    }

    if (reloadTables || clearTables != std::array<bool, (size_t) numUserSlots> {})
    {
        for (int i = 0; i < numUserSlots; ++i)
        {
            juce::String path;
            auto mode = 0;

            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                path = pendingUserTablePaths[(size_t) i];
                mode = pendingUserTableModes[(size_t) i];
                pendingUserTablePaths[(size_t) i].clear();
            }

            if (clearTables[(size_t) i])
            {
                resetUserTableToDefault (i);
                continue;
            }

            if (path.isEmpty())
                continue;

            const juce::File file (path);

            if (file.existsAsFile())
                loadUserWavetable (i, file, (Wavetable::LoadMode) juce::jlimit (0, 2, mode));
        }
    }
}

int IlanaSynthAudioProcessor::assignModSlot (int sourceIndex, int destination, float depth)
{
    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto slot = readModSlot (i);

        if (slot.source != Mod::Source::None || slot.destination != 0)
            continue;

        clearModSlot (i);
        setModSlotValue (i, "src", (float) sourceIndex);
        setModSlotValue (i, "dst", (float) destination);
        setModSlotValue (i, "amt", depth);
        return i;
    }

    return -1;
}

void IlanaSynthAudioProcessor::applyDefaultMacros()
{
    using D = Mod::Destination;
    using Targets = std::vector<std::pair<int, float>>;

    const auto map = [this] (int macro, const char* name, const Targets& targets)
    {
        setMacroName (macro, name);

        for (const auto& target : targets)
            assignModSlot ((int) Mod::Source::Macro1 + macro, target.first, target.second);
    };

    const auto on = [this] (const char* id) { return getParam (id) > 0.5f; };

    // 1: tone. Bright patches close down, dark ones open up.
    map (0, "TONE", { { (int) D::Filter1Cutoff, getParam ("f1_cutoff") > 6000.0f ? -0.5f : 0.4f } });

    // 2: timbre, from whatever the main oscillators are.
    const auto osc1Mode = (int) getParam ("osc1_mode");
    const auto osc2Wave = on ("osc2_on") && (int) getParam ("osc2_mode") == 0;

    if (osc1Mode == 0)
    {
        Targets targets { { (int) D::Osc1Frame, 0.4f } };

        if (osc2Wave)
            targets.push_back ({ (int) D::Osc2Frame, 0.4f });

        map (1, "MORPH", targets);
    }
    else if (osc1Mode == 1)
    {
        map (1, "DAMP", { { Mod::destinationForParamId ("osc1_string_damp"), 0.3f } });
    }
    else
    {
        map (1, "START", { { (int) D::Osc1SampleStart, 0.3f } });
    }

    // 3: drive into the filter.
    map (2, "DRIVE", { { (int) D::Filter1Drive, 0.5f } });

    // 4: space if the chain has reverb or delay, else width or swell.
    auto hasReverb = false, hasDelay = false;

    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto type = (int) getParam (fxSlotIds[(size_t) slot - 1].type);
        hasReverb = hasReverb || type == 13;
        hasDelay = hasDelay || type == 9;
    }

    if (hasReverb || hasDelay)
    {
        Targets targets;

        if (hasReverb)
            targets.push_back ({ (int) D::FxReverbMix, 0.3f });

        if (hasDelay)
            targets.push_back ({ (int) D::FxDelayMix, 0.25f });

        map (3, "SPACE", targets);
    }
    else if (getParam ("osc1_unison") > 1.5f)
    {
        map (3, "WIDTH", { { (int) D::Osc1Detune, 0.3f }, { (int) D::Osc1Spread, 0.4f } });
    }
    else
    {
        map (3, "SWELL", { { (int) D::AmpAttack, 0.3f } });
    }
}

void IlanaSynthAudioProcessor::loadFactoryPreset (int index)
{
    liveRetrigger = true;
    const auto& presets = Presets::getFactoryPresets();

    if (index < 0 || index >= (int) presets.size())
        return;

    setCurrentPresetName (presets[(size_t) index].name);
    setPresetMeta (getFactoryPresetCategories()[index], {});

    for (int macro = 0; macro < 4; ++macro)
    {
        const auto& names = presets[(size_t) index].macroNames;
        setMacroName (macro, macro < (int) names.size() ? juce::String (names[(size_t) macro]) : juce::String());
    }

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto& curves = presets[(size_t) index].lfoCurves;
        const auto* text = lfo < (int) curves.size() ? curves[(size_t) lfo] : nullptr;
        setLfoCurve (lfo, text != nullptr ? LfoCurve::fromString (text) : LfoCurve::preset (0));
    }

    // Back to three of each module; anything the preset turns on or routes
    // still shows because it is in use.
    for (auto& mask : revealMasks)
        mask.store (defaultRevealMask);
    ++revealVersion;

    // The rest of the per-patch state a factory preset doesn't carry goes
    // back to its default too, as applyFullState does for a state without it:
    // macro CCs, drawn LFO shapes, loaded samples and user tables.
    for (int macro = 0; macro < 4; ++macro)
        macroCc[macro].store (20 + macro);

    for (int lfo = 0; lfo < numLfos; ++lfo)
        for (int i = 0; i < lfoDrawSteps; ++i)
            setLfoCustomPoint (lfo, i, (float) std::sin (juce::MathConstants<double>::twoPi * (double) i / (double) lfoDrawSteps));

    {
        auto asyncNeeded = false;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);

            for (int i = 0; i < numSampleOscs; ++i)
                if (samplePaths[(size_t) i].isNotEmpty())
                {
                    pendingSamplePaths[(size_t) i].clear();
                    pendingSampleClear[(size_t) i] = true;
                    asyncNeeded = true;
                }

            for (int i = 0; i < numUserSlots; ++i)
                if (userTablePaths[(size_t) i].isNotEmpty())
                {
                    pendingUserTablePaths[(size_t) i].clear();
                    pendingUserTableClear[(size_t) i] = true;
                    asyncNeeded = true;
                }
        }

        if (asyncNeeded)
            triggerAsyncUpdate();

        // Edited patch tables go back to their defaults too.
        for (int i = 0; i < numUserSlots; ++i)
            if (isUserSlotEdited (i))
                resetUserTableToDefault (i);
    }

    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    // The original 80 presets predate the separate sub; move them over.
    std::vector<std::pair<juce::String, float>> values;

    for (const auto& value : presets[(size_t) index].values)
        values.push_back ({ juce::String (value.id), value.value });

    if (presets[(size_t) index].category == nullptr)
    {
        const auto find = [&values] (const juce::String& id) -> std::pair<juce::String, float>*
        {
            for (auto& entry : values)
                if (entry.first == id)
                    return &entry;

            return nullptr;
        };

        migrateLegacyOsc3 ([&find] (const juce::String& id, float fallback)
                           {
                               const auto* entry = find (id);
                               return entry != nullptr ? entry->second : fallback;
                           },
                           [&find, &values] (const juce::String& id, float value)
                           {
                               if (auto* entry = find (id))
                                   entry->second = value;
                               else
                                   values.push_back ({ id, value });
                           });
    }

    for (const auto& value : values)
    {
        if (auto* parameter = apvts.getParameter (value.first))
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                ranged->setValueNotifyingHost (ranged->convertTo0to1 (value.second));
        }
    }

    // Presets written before the rack had slots only enabled modules; if this
    // preset never assigns a slot, restore the classic chain so its effects
    // keep working.
    auto anySlotAssigned = false;

    for (int slot = 1; slot <= numFxSlots; ++slot)
        if (getParam (("fx_slot" + juce::String (slot)).toRawUTF8()) > 0.5f)
            anySlotAssigned = true;

    if (! anySlotAssigned)
    {
        const auto moduleConfigured = [this] (const juce::String& prefix)
        {
            const auto state = apvts.copyState();

            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                const auto id = state.getChild (i).getProperty ("id").toString();

                if (! id.startsWith (prefix))
                    continue;

                if (auto* parameter = apvts.getParameter (id))
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    {
                        const auto current = apvts.getRawParameterValue (id)->load();
                        const auto defaultValue = ranged->convertFrom0to1 (ranged->getDefaultValue());

                        if (std::abs (current - defaultValue) > 0.0001f)
                            return true;
                    }
                }
            }

            return false;
        };

        const std::pair<const char*, int> legacyModules[] {
            { "fx_drive_", 2 }, { "fx_fold", 2 }, { "fx_crush_", 3 }, { "fx_comb_", 5 },
            { "fx_phaser_", 6 }, { "fx_chorus_", 7 }, { "fx_delay_", 9 }, { "fx_taps_", 9 },
            { "fx_stutter_", 10 }, { "fx_smear_", 11 }, { "fx_freeze_", 12 }, { "fx_reverb_", 13 }
        };

        auto nextSlot = 1;
        bool assignedTypes[64] {};

        for (const auto& module : legacyModules)
        {
            if (nextSlot > numFxSlots)
                break;

            if (module.second < 64 && assignedTypes[module.second])
                continue;

            if (moduleConfigured (module.first))
            {
                if (auto* parameter = apvts.getParameter ("fx_slot" + juce::String (nextSlot)))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) module.second));

                if (module.second < 64)
                    assignedTypes[module.second] = true;

                ++nextSlot;
            }
        }
    }

    if (index > 0 && presets[(size_t) index].macroNames.empty())
        applyDefaultMacros();
}

bool IlanaSynthAudioProcessor::savePresetToFile (const juce::File& file)
{
    const auto state = buildFullState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    setCurrentPresetName (file.getFileNameWithoutExtension());
    return true;
}

bool IlanaSynthAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    applyFullState (juce::ValueTree::fromXml (*xml));
    setCurrentPresetName (file.getFileNameWithoutExtension());
    return true;
}

juce::ValueTree IlanaSynthAudioProcessor::buildFullState()
{
    auto state = apvts.copyState();
    state.setProperty ("osc3Schema", 2, nullptr);
    state.setProperty ("destSchema", 2, nullptr);
    state.setProperty ("tableSchema", 2, nullptr);

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        juce::StringArray values;

        {
            const juce::SpinLock::ScopedLockType lock (lfoShapeLock);

            for (int i = 0; i < lfoDrawSteps; ++i)
                values.add (juce::String (lfoCustom[(size_t) lfo][(size_t) i], 5));
        }

        state.setProperty ("lfo" + juce::String (lfo + 1) + "Draw", values.joinIntoString (","), nullptr);
        state.setProperty ("lfo" + juce::String (lfo + 1) + "Curve", getLfoCurve (lfo).toString(), nullptr);
    }

    for (int macro = 0; macro < 4; ++macro)
        state.setProperty ("macroCc" + juce::String (macro), macroCc[macro].load(), nullptr);
    state.setProperty ("oscRevealMask", revealMasks[(size_t) Module::Oscillator].load(), nullptr);
    state.setProperty ("envRevealMask", revealMasks[(size_t) Module::Envelope].load(), nullptr);
    state.setProperty ("lfoRevealMask", revealMasks[(size_t) Module::Lfo].load(), nullptr);

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        for (int i = 0; i < numSampleOscs; ++i)
            if (samplePaths[(size_t) i].isNotEmpty())
                state.setProperty ("osc" + juce::String (i + 1) + "SamplePath", samplePaths[(size_t) i], nullptr);

        for (int i = 0; i < numUserSlots; ++i)
            if (userTablePaths[(size_t) i].isNotEmpty())
            {
                state.setProperty ("userTablePath" + juce::String (i + 1), userTablePaths[(size_t) i], nullptr);
                state.setProperty ("userTableMode" + juce::String (i + 1), userTableModes[(size_t) i], nullptr);
            }
    }

    // M7.4: every edited or loaded patch table travels with the patch (see
    // WavetableDoc), stored once per slot however many oscillators play it.
    state.removeChild (state.getChildWithName ("Wavetables"), nullptr);
    juce::ValueTree tables ("Wavetables");
    for (int i = 0; i < numUserSlots; ++i)
    {
        std::shared_ptr<const WavetableDoc> doc;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            doc = userTableDocs[(size_t) i];
        }
        if (doc != nullptr)
        {
            auto table = doc->toValueTree();
            table.setProperty ("slot", i, nullptr);
            tables.appendChild (table, nullptr);
        }
    }
    if (tables.getNumChildren() > 0)
        state.appendChild (tables, nullptr);

    return state;
}

// Before v1.1, OSC 3 doubled as the sub: its table list began with four sub
// shapes (Shape/Sine/PWM/Analog Saw), it always played an octave or two
// down, and the noise followed its route. A patch that used it as a plain
// sub moves to the dedicated SUB (same table, same level, same octave); any
// other use stays on OSC 3 with the octave drop folded into SEMI and its
// table renumbered. Works on plain stored values, before they reach the
// parameters, so old table numbers aren't clamped by the new list.
void IlanaSynthAudioProcessor::migrateLegacyOsc3 (const std::function<float (const juce::String&, float)>& get,
                                                  const std::function<void (const juce::String&, float)>& set)
{
    const auto table = juce::roundToInt (get ("sub_table", 0.0f));
    const auto shape = juce::roundToInt (get ("sub_shape", 1.0f));
    const auto octave = juce::roundToInt (get ("sub_octave", 0.0f));
    const auto semi = juce::roundToInt (get ("sub_semi", 0.0f));
    const auto route = get ("sub_route", 0.0f);

    auto modulated = false;
    const Mod::Destination osc3Targets[] { Mod::Destination::SubLevel, Mod::Destination::SubPitch, Mod::Destination::SubFrame,
                                           Mod::Destination::SubSampleStart, Mod::Destination::SubSampleEnd,
                                           Mod::Destination::SubDetune, Mod::Destination::SubPan, Mod::Destination::SubWarp,
                                           Mod::Destination::SubBlend, Mod::Destination::SubSpread };

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);

        if (get (prefix + "_src", 0.0f) < 0.5f || std::abs (get (prefix + "_amt", 0.0f)) < 1.0e-6f)
            continue;

        const auto destination = juce::roundToInt (get (prefix + "_dst", 0.0f));

        for (const auto target : osc3Targets)
            modulated = modulated || destination == (int) target;
    }

    const auto plainSub = table <= 3
                          && juce::roundToInt (get ("sub_mode", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_unison", 1.0f)) <= 1
                          && juce::roundToInt (get ("sub_warp", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_chord", 0.0f)) == 0
                          && semi == 0 && std::abs (get ("sub_fine", 0.0f)) < 0.01f
                          && std::abs (get ("sub_pan", 0.0f)) < 0.001f
                          && get ("sub_frame", 0.0f) < 0.001f
                          && ! modulated;

    const auto shapeForTable = table == 0 ? shape : (table == 1 ? 0 : (table == 2 ? 1 : 2));
    set ("subosc_route", route);

    if (plainSub)
    {
        set ("subosc_on", get ("sub_on", 1.0f));
        set ("subosc_level", get ("sub_level", 0.3f));
        set ("sub_shape", (float) shapeForTable);
        set ("sub_octave", (float) octave);
        set ("sub_on", 0.0f);
        set ("sub_table", 0.0f);
        return;
    }

    // OSC 3 was a real oscillator: keep it, in the new table numbering, at
    // the same pitch.
    const int shapeTables[] { 8, 6, 10 };
    set ("subosc_on", 0.0f);
    set ("sub_on", get ("sub_on", 1.0f));
    set ("sub_level", get ("sub_level", 0.3f));
    set ("sub_table", (float) (table >= 4 ? table - 4 : shapeTables[juce::jlimit (0, 2, shapeForTable)]));
    set ("sub_semi", (float) juce::jlimit (-24, 24, semi + (octave == 0 ? -12 : -24)));
}

juce::String IlanaSynthAudioProcessor::fmRouteId (int source, int target)
{
    static const char* ids[3][3] {
        { "fm_feedback", "fm_1to2", "fm_1to3" },
        { "fm_amount", "fm_fb2", "fm_2to3" },
        { "fm_3to1", "fm_3to2", "fm_fb3" }
    };

    if (source < 3 && target < 3)
        return ids[source][target];

    return source == target ? "fm_fb" + juce::String (source + 1)
                            : "fm_" + juce::String (source + 1) + "to" + juce::String (target + 1);
}

double IlanaSynthAudioProcessor::getSnappedRatio (int osc) const
{
    const auto& ids = operatorIds[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)];
    const auto* ratio = apvts.getRawParameterValue (ids.ratio);
    const auto* snap = apvts.getRawParameterValue (ids.snap);
    return OscTuning::snapRatio (ratio != nullptr ? (double) ratio->load() : 1.0,
                                 snap != nullptr ? (int) snap->load() : 0);
}

void IlanaSynthAudioProcessor::applyFmAlgorithm (int index)
{
    if (! juce::isPositiveAndBelow (index, FmAlgorithms::count()))
        return;

    const auto& algorithm = FmAlgorithms::all()[(size_t) index];

    // One undo step for the whole routing.
    undoManager.beginNewTransaction ("FM algorithm: " + juce::String (algorithm.name));

    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (value);

            if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        }
    };

    for (int op = 0; op < algorithm.numOperators; ++op)
        if (! isOscillatorShown (op) || apvts.getRawParameterValue (oscCoreIds[(size_t) op].on)->load() < 0.5f)
            addOscillator (op);

    for (int source = 0; source < OscillatorIds::count; ++source)
        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            const auto id = fmRouteId (source, target);
            const auto current = apvts.getRawParameterValue (id)->load();

            if (! FmAlgorithms::hasRoute (algorithm, source, target))
                set (id, 0.0f);
            else if (current < 0.001f)
                set (id, source == target ? FmAlgorithms::defaultFeedbackAmount : FmAlgorithms::defaultRouteAmount);
        }

    for (int op = 0; op < algorithm.numOperators; ++op)
        set (oscCoreIds[(size_t) op].out, FmAlgorithms::isCarrier (algorithm, op) ? 1.0f : 0.0f);
}

int IlanaSynthAudioProcessor::findMatchingFmAlgorithm() const
{
    // A routing can satisfy a smaller algorithm too (Pair + Sine is a 2-Op
    // Stack plus a carrier), so the match using the most operators wins.
    auto best = -1;

    for (int index = 0; index < FmAlgorithms::count(); ++index)
    {
        const auto& algorithm = FmAlgorithms::all()[(size_t) index];
        auto matches = true;

        for (int source = 0; source < OscillatorIds::count && matches; ++source)
            for (int target = 0; target < OscillatorIds::count && matches; ++target)
                matches = (apvts.getRawParameterValue (fmRouteId (source, target))->load() > 0.001f)
                          == FmAlgorithms::hasRoute (algorithm, source, target);

        for (int op = 0; op < OscillatorIds::count && matches; ++op)
        {
            const auto on = apvts.getRawParameterValue (oscCoreIds[(size_t) op].on)->load() > 0.5f;
            const auto out = apvts.getRawParameterValue (oscCoreIds[(size_t) op].out)->load() > 0.5f;

            if (op < algorithm.numOperators)
                matches = on && out == FmAlgorithms::isCarrier (algorithm, op);
        }

        if (matches && (best < 0 || algorithm.numOperators > FmAlgorithms::all()[(size_t) best].numOperators))
            best = index;
    }

    return best;
}

bool IlanaSynthAudioProcessor::isOscillatorShown (int index) const
{

    if (isRevealed (Module::Oscillator, index))
        return true;

    const auto* on = apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on");
    return on != nullptr && on->load() > 0.5f;
}

bool IlanaSynthAudioProcessor::isLfoShown (int index) const
{
    if (isRevealed (Module::Lfo, index))
        return true;

    const auto source = Mod::lfoSourceFor (index);

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = readModSlot (slot);

        if (routing.destination != 0 && (routing.source == source || routing.aux == source))
            return true;
    }

    return false;
}

juce::Colour IlanaSynthAudioProcessor::lfoColour (int index)
{
    switch (index)
    {
        case 0: return juce::Colour (0xffff8a3b);
        case 1: return juce::Colour (0xff35c8ff);
        case 2: return juce::Colour (0xff6fe3c1);
        case 3: return juce::Colour (0xffe3a56f);
        default: return juce::Colour::fromHSV ((float) (index - 4) / 12.0f + 0.04f, 0.5f, 0.95f, 1.0f);
    }
}

void IlanaSynthAudioProcessor::addOscillator (int index)
{
    setRevealed (Module::Oscillator, index, true);

    if (auto* on = apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on"))
        on->setValueNotifyingHost (1.0f);
}

void IlanaSynthAudioProcessor::removeOscillator (int index)
{
    if (auto* on = apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on"))
        on->setValueNotifyingHost (0.0f);

    setRevealed (Module::Oscillator, index, false);
}

void IlanaSynthAudioProcessor::applyFullState (const juce::ValueTree& stateIn)
{
    liveRetrigger = true;
    auto state = stateIn.createCopy();
    {
        // Pre-mask M3b states saved a count of revealed envelopes.
        auto envMask = (int) state.getProperty ("envRevealMask", defaultRevealMask);
        if (! state.hasProperty ("envRevealMask") && state.hasProperty ("envRevealCount"))
            envMask = (1 << juce::jlimit (3, 16, (int) state.getProperty ("envRevealCount"))) - 1;
        revealMasks[(size_t) Module::Oscillator].store ((int) state.getProperty ("oscRevealMask", defaultRevealMask));
        revealMasks[(size_t) Module::Envelope].store (envMask);
        revealMasks[(size_t) Module::Lfo].store ((int) state.getProperty ("lfoRevealMask", defaultRevealMask));
        ++revealVersion;
    }

    if ((int) state.getProperty ("osc3Schema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);

            return juce::ValueTree();
        };

        migrateLegacyOsc3 ([&find] (const juce::String& id, float fallback)
                           {
                               const auto child = find (id);
                               return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                           },
                           [&find, &state] (const juce::String& id, float value)
                           {
                               auto child = find (id);

                               if (! child.isValid())
                               {
                                   child = juce::ValueTree ("PARAM");
                                   child.setProperty ("id", id, nullptr);
                                   state.appendChild (child, nullptr);
                               }

                               child.setProperty ("value", value, nullptr);
                           });

        state.setProperty ("osc3Schema", 2, nullptr);
    }

    // v1.1 added factory wavetables ahead of the four user slots, so older
    // saved choices of a user slot move up.
    if ((int) state.getProperty ("tableSchema", 1) < 2)
    {
        const auto added = TableFactory::getNumFactoryTables() - 16;

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (id == "osc1_table" || id == "osc2_table" || id == "sub_table")
            {
                const auto table = juce::roundToInt ((float) child.getProperty ("value"));

                if (table >= 16)
                    child.setProperty ("value", table + added, nullptr);
            }
        }

        state.setProperty ("tableSchema", 2, nullptr);
    }

    // v1.1 added explicit (FM) destinations ahead of the parameter
    // destinations, so older saved routings to those move up.
    if ((int) state.getProperty ("destSchema", 1) < 2)
    {
        const auto added = Mod::numExplicitDestinations - Mod::explicitDestinationsV10;

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (id.startsWith ("mod") && id.endsWith ("_dst"))
            {
                const auto destination = juce::roundToInt ((float) child.getProperty ("value"));

                if (destination >= Mod::explicitDestinationsV10)
                    child.setProperty ("value", destination + added, nullptr);
            }
        }

        state.setProperty ("destSchema", 2, nullptr);
    }

    const auto parseDraw = [this] (int lfoIndex, const juce::String& text)
    {
        const auto tokens = juce::StringArray::fromTokens (text, ",", "");

        for (int i = 0; i < lfoDrawSteps && i < tokens.size(); ++i)
            setLfoCustomPoint (lfoIndex, i, tokens[i].getFloatValue());
    };

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto property = "lfo" + juce::String (lfo + 1) + "Draw";
        const auto text = state.getProperty (property).toString();

        if (text.isNotEmpty())
            parseDraw (lfo, text);

        const auto curveText = state.getProperty ("lfo" + juce::String (lfo + 1) + "Curve").toString();
        setLfoCurve (lfo, curveText.isNotEmpty() ? LfoCurve::fromString (curveText) : LfoCurve::preset (0));
    }

    for (int macro = 0; macro < 4; ++macro)
    {
        const auto property = "macroCc" + juce::String (macro);

        if (state.hasProperty (property))
            macroCc[macro].store (juce::jlimit (0, 127, (int) state.getProperty (property)));
    }

    auto asyncNeeded = false;

    // M7.4 patch tables: recipe, then embedded frames, then the file (see
    // WavetableDoc). A slot the state does not mention goes back to its
    // default, unless an old-style path below reloads it.
    std::array<bool, (size_t) numUserSlots> restoredTables {};
    {
        const auto tables = state.getChildWithName ("Wavetables");
        juce::StringArray notices;
        for (int child = 0; child < tables.getNumChildren(); ++child)
        {
            const auto tree = tables.getChild (child);
            const auto slot = (int) tree.getProperty ("slot", -1);
            if (slot < 0 || slot >= numUserSlots || restoredTables[(size_t) slot])
                continue;
            WavetableDoc doc;
            juce::String notice;
            if (WavetableDoc::fromValueTree (tree, doc, notice))
                setUserTable (slot, doc);
            else
                resetUserTableToDefault (slot);
            if (notice.isNotEmpty())
                notices.add (notice);
            restoredTables[(size_t) slot] = true;
        }
        state.removeChild (tables, nullptr);

        for (int i = 0; i < numUserSlots; ++i)
            if (! restoredTables[(size_t) i] && isUserSlotEdited (i)
                && state.getProperty ("userTablePath" + juce::String (i + 1)).toString().isEmpty())
                resetUserTableToDefault (i);

        if (! notices.isEmpty())
        {
            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                tableNotice = notices.joinIntoString ("\n");
            }
            ++tableNoticeVersion;
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        for (int i = 0; i < numSampleOscs; ++i)
        {
            const auto path = state.getProperty ("osc" + juce::String (i + 1) + "SamplePath").toString();

            if (path.isNotEmpty() && path != samplePaths[(size_t) i])
            {
                pendingSamplePaths[(size_t) i] = path;
                pendingSampleClear[(size_t) i] = false;
                samplesReloadPending = true;
                asyncNeeded = true;
            }
            else if (path.isEmpty() && samplePaths[(size_t) i].isNotEmpty())
            {
                pendingSamplePaths[(size_t) i].clear();
                pendingSampleClear[(size_t) i] = true;
                asyncNeeded = true;
            }
        }

        for (int i = 0; i < numUserSlots; ++i)
        {
            if (restoredTables[(size_t) i])
            {
                pendingUserTablePaths[(size_t) i].clear();
                continue;
            }

            const auto path = state.getProperty ("userTablePath" + juce::String (i + 1)).toString();
            const auto mode = (int) state.getProperty ("userTableMode" + juce::String (i + 1), 0);

            if (path.isNotEmpty() && (path != userTablePaths[(size_t) i] || mode != userTableModes[(size_t) i]))
            {
                pendingUserTablePaths[(size_t) i] = path;
                pendingUserTableModes[(size_t) i] = mode;
                pendingUserTableClear[(size_t) i] = false;
                userTablesReloadPending = true;
                asyncNeeded = true;
            }
            else if (path.isEmpty() && userTablePaths[(size_t) i].isNotEmpty())
            {
                pendingUserTablePaths[(size_t) i].clear();
                pendingUserTableClear[(size_t) i] = true;
                asyncNeeded = true;
            }
        }
    }

    if (asyncNeeded)
        triggerAsyncUpdate();

    // JUCE replaceState retains current values for absent parameters. Fill every
    // omitted parameter from its declared default, including future additions.
    // The saved IDs are collected once, so loading stays linear in the
    // parameter count.
    std::set<juce::String> savedIds;

    for (int child = 0; child < state.getNumChildren(); ++child)
        savedIds.insert (state.getChild (child).getProperty ("id").toString());

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const auto id = static_cast<juce::AudioProcessorParameterWithID*> (ranged)->paramID;
            const auto found = savedIds.count (id) > 0;

            if (! found)
            {
                juce::ValueTree missingParameter ("PARAM");
                missingParameter.setProperty ("id", id, nullptr);
                missingParameter.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
                state.appendChild (missingParameter, nullptr);
            }
        }

    apvts.replaceState (state);
}

void IlanaSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = buildFullState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void IlanaSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    applyFullState (juce::ValueTree::fromXml (*xml));
}

void IlanaSynthAudioProcessor::assignFxSlot (int slot, int type)
{
    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    set ("fx_slot" + juce::String (slot), (float) type);

    // Freeze and Stutter are momentary performance triggers, so they are left
    // off; the classic modules below otherwise stay silent until enabled.
    const char* enableId = nullptr;

    switch (type)
    {
        case 2:  enableId = "fx_drive_on"; break;
        case 3:  enableId = "fx_crush_on"; break;
        case 5:  enableId = "fx_comb_on"; break;
        case 6:  enableId = "fx_phaser_on"; break;
        case 7:  enableId = "fx_chorus_on"; break;
        case 9:  enableId = "fx_delay_on"; break;
        case 11: enableId = "fx_smear_on"; break;
        case 13: enableId = "fx_reverb_on"; break;
        default: break;
    }

    if (enableId != nullptr)
        set (enableId, 1.0f);
}

void IlanaSynthAudioProcessor::randomizeFxChain()
{
    juce::Random random;
    constexpr int typeCount = numFxTypes;

    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto type = random.nextFloat() < 0.15f ? 0 : 1 + random.nextInt (typeCount);
        assignFxSlot (slot, type);
        set ("fx_slot" + juce::String (slot) + "_bypass", 0.0f);
        set ("fx_slot" + juce::String (slot) + "_mix", 0.6f + random.nextFloat() * 0.4f);
    }

    set ("fx_drive_on", 1.0f);
    set ("fx_drive_amount", 2.0f + random.nextFloat() * 6.0f);
    set ("fx_delay_on", 1.0f);
    set ("fx_delay_sync", 1.0f);
    set ("fx_delay_div", (float) (2 + random.nextInt (4)));
    set ("fx_delay_feedback", 0.2f + random.nextFloat() * 0.4f);
    set ("fx_reverb_on", 1.0f);
    set ("fx_reverb_type", (float) random.nextInt (6));
    set ("fx_reverb_mix", 0.15f + random.nextFloat() * 0.3f);
}

bool IlanaSynthAudioProcessor::saveFxChainToFile (const juce::File& file)
{
    juce::XmlElement xml ("ilanafxchain");
    const auto tree = apvts.copyState();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        if (! id.startsWith ("fx_"))
            continue;

        if (const auto* value = apvts.getRawParameterValue (id))
        {
            auto* element = xml.createNewChildElement ("p");
            element->setAttribute ("id", id);
            element->setAttribute ("v", (double) value->load());
        }
    }

    return xml.writeTo (file);
}

void IlanaSynthAudioProcessor::loadReverbIr (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);
    reverbIrLoaded.store (true);
}

bool IlanaSynthAudioProcessor::loadFxChainFromFile (const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));

    if (xml == nullptr || ! xml->hasTagName ("ilanafxchain"))
        return false;

    for (auto* element = xml->getFirstChildElement(); element != nullptr; element = element->getNextElement())
    {
        const auto id = element->getStringAttribute ("id");

        if (id.isEmpty())
            continue;

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 ((float) element->getDoubleAttribute ("v")));
    }

    return true;
}

juce::String IlanaSynthAudioProcessor::captureFxChain()
{
    juce::String state;
    const auto tree = apvts.copyState();

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto id = tree.getChild (i).getProperty ("id").toString();

        if (! id.startsWith ("fx_"))
            continue;

        if (const auto* value = apvts.getRawParameterValue (id))
            state += id + "=" + juce::String (value->load(), 6) + ";";
    }

    return state;
}

void IlanaSynthAudioProcessor::applyFxChain (const juce::String& state)
{
    const auto tokens = juce::StringArray::fromTokens (state, ";", "");

    for (const auto& token : tokens)
    {
        const auto id = token.upToFirstOccurrenceOf ("=", false, false);
        const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();

        if (id.isEmpty())
            continue;

        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

void IlanaSynthAudioProcessor::switchFxChain()
{
    if (showingChainA)
    {
        chainA = captureFxChain();

        if (chainBValid)
            applyFxChain (chainB);
        else
        {
            chainB = chainA;
            chainBValid = true;
        }

        showingChainA = false;
    }
    else
    {
        chainB = captureFxChain();
        chainBValid = true;
        applyFxChain (chainA);
        showingChainA = true;
    }
}

void IlanaSynthAudioProcessor::copyFxChainToOtherBank()
{
    if (showingChainA)
    {
        chainA = captureFxChain();
        chainB = chainA;
        chainBValid = true;
    }
    else
    {
        chainB = captureFxChain();
        chainA = chainB;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IlanaSynthAudioProcessor();
}
