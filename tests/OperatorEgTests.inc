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
} // namespace OperatorEgTests

void runOperatorEgTests()
{
    OperatorEgTests::runOldDx7PatchTest();
    OperatorEgTests::runPlainPatchTest();
    OperatorEgTests::runLoadVoiceTest();
}
