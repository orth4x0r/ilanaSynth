// The Operator EG (2026-10-02): DX7 voices as ordinary parameters. Included
// by TableTest.cpp; ILANA_OPEG_TEST=1 runs these alone. That the 288 DX7
// presets still sound as DX7 mode played them is the fingerprint check
// (bit-identical when this landed).

#include "Dx7Presets.h"

namespace OperatorEgTests
{
constexpr double sr = 48000.0;

std::vector<float> play (IlanaSynthAudioProcessor& processor, int note, double seconds, double releaseAt)
{
    processor.prepareToPlay (sr, 256);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 256);
    const auto releaseBlock = (int) (releaseAt * sr / 256);
    for (int block = 0; block < (int) (seconds * sr / 256); ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (block == releaseBlock)
            midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 256; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

float peak (const std::vector<float>& samples, size_t from, size_t to)
{
    auto result = 0.0f;
    for (auto i = from; i < juce::jmin (to, samples.size()); ++i)
        result = juce::jmax (result, std::abs (samples[i]));
    return result;
}

float read (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    const auto* value = processor.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : -1.0f;
}

// A patch the old DX7 mode saved (a "Dx7" child, Filtered feedback, the
// operators' ENVELOPE left at Amp Env) loads as the same sound.
void runOldDx7PatchTest()
{
    const auto names = IlanaSynthAudioProcessor().getFactoryPresetNames();
    const auto index = names.indexOf ("E.PIANO 1 (ROM1A)");
    check (index > 0, "operator EG: the DX7 bank presets are there (E.PIANO 1)");
    if (index <= 0)
        return;

    IlanaSynthAudioProcessor source;
    source.prepareToPlay (sr, 256);
    source.loadFactoryPreset (index);
    check (juce::roundToInt (read (source, "osc1_amp_env")) == OperatorEg::envelopeChoice
               && juce::roundToInt (read (source, "osc1_fb_type")) == FmFeedback::Dx7,
           "operator EG: a DX7 preset's operators play the Operator EG with DX7 feedback");
    const auto expected = play (source, 60, 2.0, 1.0);

    juce::MemoryBlock saved;
    source.getStateInformation (saved);
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()));
    check (xml != nullptr && xml->getChildByName ("Dx7") == nullptr, "operator EG: a saved patch carries no Dx7 child");
    if (xml == nullptr)
        return;

    // Turn it back into what DX7 mode saved: no EG parameters, ENVELOPE at
    // Amp Env, Filtered feedback, and the voice as a Dx7 child.
    for (auto* child = xml->getFirstChildElement(); child != nullptr;)
    {
        auto* next = child->getNextElement();
        const auto id = child->getStringAttribute ("id");
        if (id.contains ("_eg_") || id.startsWith ("opeg_"))
            xml->removeChildElement (child, true);
        else if (id.endsWith ("_amp_env"))
            child->setAttribute ("value", 0.0);
        else if (id.endsWith ("_fb_type") && juce::roundToInt (child->getDoubleAttribute ("value")) == FmFeedback::Dx7)
            child->setAttribute ("value", (double) FmFeedback::Filtered);
        child = next;
    }
    auto voice = Dx7::unpack (Dx7Banks::banks[0].data);
    for (int i = 0; i < 32; ++i)
        if (Dx7::name (Dx7::unpack (Dx7Banks::banks[0].data + i * 128)) == "E.PIANO 1")
            voice = Dx7::unpack (Dx7Banks::banks[0].data + i * 128);
    auto* dx7 = xml->createNewChildElement ("Dx7");
    dx7->setAttribute ("voice", juce::Base64::toBase64 (voice.data(), voice.size()));

    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);
    IlanaSynthAudioProcessor loaded;
    loaded.prepareToPlay (sr, 256);
    loaded.setStateInformation (oldState.getData(), (int) oldState.getSize());

    check (juce::roundToInt (read (loaded, "osc4_amp_env")) == OperatorEg::envelopeChoice
               && juce::roundToInt (read (loaded, "osc1_eg_out")) == (int) Dx7::op (voice, 1)[16]
               && juce::roundToInt (read (loaded, "osc6_eg_r1")) == (int) Dx7::op (voice, 6)[0],
           "operator EG: an old DX7-mode patch's voice becomes the Operator EG parameters");

    const auto rendered = play (loaded, 60, 2.0, 1.0);
    auto worst = 0.0f;
    for (size_t i = 0; i < juce::jmin (expected.size(), rendered.size()); ++i)
        worst = juce::jmax (worst, std::abs (expected[i] - rendered[i]));
    check (peak (expected, 0, expected.size()) > 0.01f && worst == 0.0f,
           "operator EG: an old DX7-mode patch renders as the preset does (max difference " + juce::String (worst) + ")");
}

// Any oscillator can use it: OSC 1's saw on the Operator EG beside the
// usual amp envelope's patch, and a released note ends.
void runPlainPatchTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (sr, 256);
    processor.loadFactoryPreset (0);
    setParam (processor, "osc2_on", 0.0f);
    setParam (processor, "sub_on", 0.0f);
    setParam (processor, "osc1_amp_env", (float) OperatorEg::envelopeChoice);
    // A slow attack to L1, a dip, then a rise again to L3: a shape an ADSR
    // can't make.
    setParam (processor, "osc1_eg_r1", 60.0f);
    setParam (processor, "osc1_eg_l1", 99.0f);
    setParam (processor, "osc1_eg_r2", 70.0f);
    setParam (processor, "osc1_eg_l2", 60.0f);
    setParam (processor, "osc1_eg_r3", 50.0f);
    setParam (processor, "osc1_eg_l3", 90.0f);
    setParam (processor, "osc1_eg_r4", 70.0f);
    const auto out = play (processor, 60, 3.0, 1.5);
    const auto block = [&out] (double from, double to) { return peak (out, (size_t) (from * sr), (size_t) (to * sr)); };
    check (block (0.0, 0.003) < block (0.1, 0.2), "operator EG: R1 is an attack (" + juce::String (block (0.0, 0.003)) + " then "
                                                      + juce::String (block (0.1, 0.2)) + ")");
    check (block (1.2, 1.5) > 0.01f, "operator EG: the note holds at L3 (" + juce::String (block (1.2, 1.5)) + ")");
    check (block (2.8, 3.0) < 1.0e-4f, "operator EG: a released note dies (" + juce::String (block (2.8, 3.0)) + ")");
}

// Loading a voice (as the .syx import does) gives ordinary parameters.
void runLoadVoiceTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (sr, 256);
    const auto voice = Dx7::unpack (Dx7Banks::banks[0].data); // BRASS 1
    processor.loadDx7Voice (voice, "01 BRASS 1");
    auto allOnEg = true;
    for (const auto* prefix : OscillatorIds::prefixes)
        allOnEg = allOnEg && juce::roundToInt (read (processor, juce::String (prefix) + "_amp_env")) == OperatorEg::envelopeChoice;
    check (allOnEg && juce::roundToInt (read (processor, "opeg_pitch_l1")) == (int) voice[130]
               && juce::roundToInt (read (processor, "opeg_lfo_speed")) == (int) voice[137]
               && juce::roundToInt (read (processor, OperatorEg::keyOffsetId)) == (int) voice[144] - 24,
           "operator EG: a loaded DX7 voice sets the Operator EG, pitch EG and LFO parameters");
    const auto out = play (processor, 60, 1.0, 0.5);
    check (peak (out, 0, out.size()) > 0.01f, "operator EG: a loaded DX7 voice plays");
}

// Renders a note on a DX7 preset, with `during` called on each block's MIDI
// (to move the wheel, say) and `edit` once at `editAt` seconds.
std::vector<float> playEdited (IlanaSynthAudioProcessor& processor, double seconds,
                               const std::function<void (int, juce::MidiBuffer&)>& during = {},
                               const std::function<void()>& edit = {}, double editAt = 0.0)
{
    processor.prepareToPlay (sr, 256);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 256);
    const auto editBlock = (int) (editAt * sr / 256);
    for (int block = 0; block < (int) (seconds * sr / 256); ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        if (during != nullptr)
            during (block, midi);
        if (edit != nullptr && block == editBlock)
            edit();
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int i = 0; i < 256; ++i)
            out.push_back (buffer.getSample (0, i));
    }
    return out;
}

float difference (const std::vector<float>& a, const std::vector<float>& b)
{
    auto worst = 0.0f;
    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
        worst = juce::jmax (worst, std::abs (a[i] - b[i]));
    return worst;
}

int presetIndex (const juce::String& name)
{
    return IlanaSynthAudioProcessor().getFactoryPresetNames().indexOf (name);
}

// UI review 6: every DX7 voice names its algorithm (the stored number
// matches its routing), and each of the 32 sets the routing it names.
void runDx7AlgorithmTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (sr, 256);
    const auto names = processor.getFactoryPresetNames();
    auto voices = 0, matched = 0;
    juce::String firstMiss;
    for (int index = 0; index < names.size(); ++index)
    {
        if (! names[index].contains ("(ROM") && ! names[index].contains ("(BANK"))
            continue;
        processor.loadFactoryPreset (index);
        const auto stored = juce::roundToInt (read (processor, OperatorEg::dx7AlgorithmId));
        if (stored <= 0)
            continue;
        ++voices;
        if (processor.findMatchingDx7Algorithm() == stored)
            ++matched;
        else if (firstMiss.isEmpty())
            firstMiss = names[index] + " (stored " + juce::String (stored) + ", matched "
                        + juce::String (processor.findMatchingDx7Algorithm()) + ")";
    }
    check (voices >= 32 && matched == voices, "operator EG: every DX7 voice lights its algorithm (" + juce::String (matched) + " of "
                                                  + juce::String (voices) + (firstMiss.isNotEmpty() ? ", first miss " + firstMiss : juce::String()) + ")");

    const auto epiano = presetIndex ("E.PIANO 1 (ROM1A)");
    if (epiano < 0)
        return;
    auto allSet = true;
    juce::String miss;
    for (int number = 1; number <= 32; ++number)
    {
        processor.loadFactoryPreset (epiano);
        processor.applyDx7Algorithm (number);
        const auto found = processor.findMatchingDx7Algorithm();
        if (found != number && miss.isEmpty())
            miss = juce::String (number) + " matched " + juce::String (found);
        allSet = allSet && found == number
                 && juce::roundToInt (read (processor, OperatorEg::dx7AlgorithmId)) == number
                 && juce::roundToInt (read (processor, "osc6_fb_type")) == FmFeedback::Dx7;
    }
    check (allSet, "operator EG: each DX7 algorithm sets its routing, keeps DX7 feedback, and is found again" + (miss.isNotEmpty() ? " (" + miss + ")" : juce::String()));
}

// Turning a knob while a note sounds moves the Operator Env at once, as
// on a DX7; touching nothing renders the same note twice.
void runLiveEditTest()
{
    const auto epiano = presetIndex ("E.PIANO 1 (ROM1A)");
    if (epiano < 0)
        return;
    IlanaSynthAudioProcessor a, b, c;
    for (auto* processor : { &a, &b, &c })
        processor->loadFactoryPreset (epiano);
    const auto plain = playEdited (a, 1.5);
    const auto again = playEdited (b, 1.5);
    const auto edited = playEdited (c, 1.5, {}, [&c]
    {
        for (const auto* id : { "osc1_eg_out", "osc3_eg_out", "osc5_eg_out" })
            setParam (c, id, 60.0f);
    }, 0.5);
    check (difference (plain, again) == 0.0f, "operator EG: the same note renders the same");
    check (difference (plain, edited) > 0.01f && peak (edited, (size_t) (1.2 * sr), (size_t) (1.5 * sr)) < peak (plain, (size_t) (1.2 * sr), (size_t) (1.5 * sr)),
           "operator EG: lowering the carriers' LEVEL while a note holds lowers it at once");
}

// The wheel and pressure add vibrato on a DX7 voice, as on the hardware,
// and do nothing until moved (the voice's own LFO depth is untouched).
void runWheelTest()
{
    const auto index = presetIndex ("E.PIANO 1 (ROM1A)");
    if (index < 0)
        return;
    IlanaSynthAudioProcessor still, atZero, moved, pressed;
    for (auto* processor : { &still, &atZero, &moved, &pressed })
        processor->loadFactoryPreset (index);
    const auto wheel = [] (int value)
    {
        return [value] (int block, juce::MidiBuffer& midi)
        {
            if (block == 1)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, value), 0);
        };
    };
    const auto plain = playEdited (still, 2.0);
    const auto zero = playEdited (atZero, 2.0, wheel (0));
    const auto full = playEdited (moved, 2.0, wheel (127));
    const auto pressure = playEdited (pressed, 2.0, [] (int block, juce::MidiBuffer& midi)
    {
        if (block == 1)
            midi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
    });
    check (difference (plain, zero) == 0.0f, "operator EG: the wheel at rest leaves a DX7 voice as it was");
    check (difference (plain, full) > 0.001f, "operator EG: the wheel adds vibrato to a DX7 voice ("
                                                  + juce::String (difference (plain, full)) + ")");
    check (difference (plain, pressure) > 0.001f, "operator EG: pressure adds vibrato to a DX7 voice ("
                                                      + juce::String (difference (plain, pressure)) + ")");
}

// The Op LFO and Op Pitch Env are modulation sources like any other.
void runSourcesTest()
{
    const auto index = presetIndex ("E.PIANO 1 (ROM1A)");
    if (index < 0)
        return;
    IlanaSynthAudioProcessor plain, routed;
    for (auto* processor : { &plain, &routed })
    {
        processor->loadFactoryPreset (index);
        setParam (*processor, "opeg_lfo_speed", 70.0f);
    }
    const auto destination = Mod::getDestinationNames().indexOf ("Osc1 Level");
    setParam (routed, "mod16_src", (float) (int) Mod::Source::OpLfo);
    setParam (routed, "mod16_dst", (float) destination);
    setParam (routed, "mod16_amt", 1.0f);
    const auto names = Mod::getSourceNames();
    check (names[(int) Mod::Source::OpLfo] == "Op LFO" && names[(int) Mod::Source::OpPitchEnv] == "Op Pitch Env",
           "operator EG: Op LFO and Op Pitch Env are named sources");
    check (destination > 0 && difference (playEdited (plain, 1.0), playEdited (routed, 1.0)) > 0.001f,
           "operator EG: the Op LFO modulates a destination through the matrix");
    for (const auto* name : { "Osc1 Op Env Level", "Osc6 Op Env Attack Rate", "Op LFO Rate", "Op LFO Pitch Depth", "Op Pitch Env Level 1" })
        check (Mod::getDestinationNames().contains (name), juce::String ("operator EG: \"") + name + "\" is a destination");
}
} // namespace OperatorEgTests

void runOperatorEgTests()
{
    OperatorEgTests::runOldDx7PatchTest();
    OperatorEgTests::runPlainPatchTest();
    OperatorEgTests::runLoadVoiceTest();
    OperatorEgTests::runDx7AlgorithmTest();
    OperatorEgTests::runLiveEditTest();
    OperatorEgTests::runWheelTest();
    OperatorEgTests::runSourcesTest();
}
