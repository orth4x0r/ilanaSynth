// The FM / DX7 oscillator type (2026-10-06): an operator is its own type
// (oscN_mode 5, appended), rendered by the wavetable engine exactly as a
// wavetable oscillator with the same settings. Included by TableTest.cpp;
// ILANA_OSCMODE_TEST=1 runs these alone (ILANA_OSCMODE_STATS=1 also lists
// the factory operators that use wavetable-only controls).

namespace OscModeTests
{
constexpr double sr = 48000.0;

std::vector<float> render (IlanaSynthAudioProcessor& processor)
{
    processor.prepareToPlay (sr, 256);
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, 256);
    for (int block = 0; block < (int) (1.5 * sr / 256); ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 80), 0);
        }
        if (block == (int) (0.8 * sr / 256))
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
        }
        buffer.clear();
        processor.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                out.push_back (buffer.getSample (ch, i));
    }
    return out;
}

float value (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    const auto* raw = processor.apvts.getRawParameterValue (id);
    return raw != nullptr ? raw->load() : -1.0f;
}

int modeOf (IlanaSynthAudioProcessor& processor, int osc)
{
    return juce::roundToInt (value (processor, juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode"));
}

void setRaw (IlanaSynthAudioProcessor& processor, const juce::String& id, float newValue)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (newValue));
}

juce::MemoryBlock stateOf (IlanaSynthAudioProcessor& processor)
{
    juce::MemoryBlock block;
    processor.getStateInformation (block);
    return block;
}

// What the version before FM / DX7 saved: every FM / DX7 oscillator as a
// Wavetable (mode 0). Returns how many it changed.
int toOldState (juce::MemoryBlock& block)
{
    auto xml = std::unique_ptr<juce::XmlElement> (juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()));
    if (xml == nullptr)
        return 0;
    auto changed = 0;
    for (auto* child : xml->getChildIterator())
        if (child->getStringAttribute ("id").endsWith ("_mode") && juce::roundToInt (child->getDoubleAttribute ("value")) == OscMode::fmOperator)
        {
            child->setAttribute ("value", 0.0);
            ++changed;
        }
    block.reset();
    juce::AudioProcessor::copyXmlToBinary (*xml, block);
    return changed;
}

bool sameSound (const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0;
}

// Every factory preset: an operator (a wavetable tuned by ratio or fixed Hz,
// or on the Operator EG) is FM / DX7; no Wavetable keeps an operator's tuning
// or envelope.
void runFactoryTest()
{
    IlanaSynthAudioProcessor processor;
    const auto names = processor.getFactoryPresetNames();
    auto operators = 0, stray = 0, presetsWithOperators = 0;
    const auto stats = juce::SystemStats::getEnvironmentVariable ("ILANA_OSCMODE_STATS", "").isNotEmpty();
    juce::StringArray strayNames;

    for (int index = 0; index < names.size(); ++index)
    {
        processor.loadFactoryPreset (index);
        auto here = 0;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            const auto mode = modeOf (processor, osc);
            const auto tuned = juce::roundToInt (value (processor, prefix + "_tune")) != OscTuning::Semitones;
            const auto onEg = juce::roundToInt (value (processor, prefix + "_amp_env")) == OperatorEg::envelopeChoice;
            if (mode == OscMode::fmOperator)
            {
                ++operators;
                ++here;
                if (stats)
                {
                    juce::StringArray used;
                    for (const auto* suffix : { "_warp", "_warp2", "_spectral", "_pd_env", "_chord" })
                        if (juce::roundToInt (value (processor, prefix + suffix)) != 0)
                            used.add (suffix);
                    if (value (processor, prefix + "_unison") > 1.5f)
                        used.add ("_unison");
                    if (value (processor, prefix + "_frame") > 0.001f)
                        used.add ("_frame");
                    if (juce::roundToInt (value (processor, prefix + "_fb_type")) != 0)
                        used.add ("_fb_type");
                    if (used.size() > 0 && processor.isOscillatorShown (osc))
                        std::cout << "STATS: " << names[index] << " OSC " << (osc + 1) << " " << used.joinIntoString (" ") << std::endl;
                }
            }
            if (mode == OscMode::wavetable && (tuned || onEg))
            {
                ++stray;
                strayNames.add (names[index] + " OSC " + juce::String (osc + 1));
            }
        }
        presetsWithOperators += here > 0 ? 1 : 0;
    }

    std::cout << "OSC MODE: " << operators << " FM / DX7 oscillators in " << presetsWithOperators << " factory presets" << std::endl;
    check (operators > 288 * 4, "osc type: the DX7 banks' operators load as FM / DX7");
    check (stray == 0, "osc type: no factory Wavetable keeps an operator's tuning or Operator EG " + strayNames.joinIntoString (", "));
}

// A state saved before the type existed (operators as Wavetable) loads as
// FM / DX7 and sounds bit-identical; FM / DX7 renders as Wavetable would.
void runOldStateTest (const juce::String& presetName)
{
    IlanaSynthAudioProcessor source;
    const auto index = source.getFactoryPresetNames().indexOf (presetName);
    check (index >= 0, "osc type: preset " + presetName + " is there");
    if (index < 0)
        return;
    source.loadFactoryPreset (index);
    auto saved = stateOf (source);
    auto old = saved;
    const auto changed = toOldState (old);
    check (changed > 0, "osc type: " + presetName + " has FM / DX7 oscillators (" + juce::String (changed) + ")");

    IlanaSynthAudioProcessor fresh, migrated, forced;
    fresh.setStateInformation (saved.getData(), (int) saved.getSize());
    migrated.setStateInformation (old.getData(), (int) old.getSize());
    forced.setStateInformation (old.getData(), (int) old.getSize());

    auto modesMatch = true;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        modesMatch = modesMatch && modeOf (migrated, osc) == modeOf (fresh, osc);
    check (modesMatch, "osc type: " + presetName + " saved as Wavetable loads its operators as FM / DX7");

    // The engine: the same patch with its operators forced back to mode 0
    // (past the load's migration) plays the same.
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (modeOf (forced, osc) == OscMode::fmOperator)
            setRaw (forced, juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode", 0.0f);

    const auto expected = render (fresh);
    check (sameSound (expected, render (migrated)), "osc type: " + presetName + " from an old state renders bit-identical");
    check (sameSound (expected, render (forced)), "osc type: " + presetName + " as FM / DX7 renders as the Wavetable engine did");

    auto peak = 0.0f;
    for (const auto sample : expected)
        peak = juce::jmax (peak, std::abs (sample));
    check (peak > 1.0e-3f, "osc type: " + presetName + " makes sound (" + juce::String (peak) + ")");
}

// Picking a type: leaving FM / DX7 drops what only an operator has; picking
// it changes nothing else; adding one starts a sine operator.
void runModeChoiceTest()
{
    IlanaSynthAudioProcessor processor;
    setRaw (processor, "osc2_on", 1.0f);
    setRaw (processor, "osc2_tune", (float) OscTuning::Ratio);
    setRaw (processor, "osc2_ratio", 3.0f);
    setRaw (processor, "osc2_amp_env", (float) OperatorEg::envelopeChoice);
    processor.migrateOperatorModes();
    check (modeOf (processor, 1) == OscMode::fmOperator, "osc type: a ratio-tuned Wavetable on the Operator EG becomes FM / DX7");

    processor.setOscillatorMode (1, OscMode::wavetable);
    check (modeOf (processor, 1) == OscMode::wavetable
               && juce::roundToInt (value (processor, "osc2_tune")) == OscTuning::Semitones
               && juce::roundToInt (value (processor, "osc2_amp_env")) == 0,
           "osc type: FM / DX7 to Wavetable goes back to semitones and the Amp Env");

    setRaw (processor, "osc2_amp_env", 4.0f);
    processor.setOscillatorMode (1, OscMode::fmOperator);
    check (modeOf (processor, 1) == OscMode::fmOperator && juce::roundToInt (value (processor, "osc2_amp_env")) == 4
               && juce::roundToInt (value (processor, "osc2_tune")) == OscTuning::Semitones,
           "osc type: Wavetable to FM / DX7 keeps the sound's settings");

    setRaw (processor, "osc2_tune", (float) OscTuning::Fixed);
    processor.setOscillatorMode (1, OscMode::physical);
    check (modeOf (processor, 1) == OscMode::physical && juce::roundToInt (value (processor, "osc2_tune")) == OscTuning::Semitones,
           "osc type: FM / DX7 to Physical goes back to semitones");

    processor.addOscillator (4, OscMode::fmOperator);
    check (modeOf (processor, 4) == OscMode::fmOperator && value (processor, "osc5_on") > 0.5f && processor.isOscillatorShown (4)
               && juce::roundToInt (value (processor, "osc5_tune")) == OscTuning::Ratio
               && juce::roundToInt (value (processor, "osc5_amp_env")) == OperatorEg::envelopeChoice
               && juce::roundToInt (value (processor, "osc5_table")) == 8,
           "osc type: an added FM / DX7 oscillator is a sine operator by ratio on the Operator EG");

    // Saved and loaded, the type stays.
    auto saved = stateOf (processor);
    IlanaSynthAudioProcessor loaded;
    loaded.setStateInformation (saved.getData(), (int) saved.getSize());
    check (modeOf (loaded, 4) == OscMode::fmOperator && modeOf (loaded, 1) == OscMode::physical, "osc type: a saved FM / DX7 oscillator loads as one");

    // The parameter's list names it, at the end.
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("sub_mode"));
    check (choice != nullptr && choice->choices.size() == OscMode::count && choice->choices[OscMode::fmOperator] == "FM / DX7"
               && choice->choices[4] == "Live",
           "osc type: FM / DX7 is appended to the type list (index 5)");
}

void run()
{
    runModeChoiceTest();
    for (const auto* name : { "FM E-Piano", "E.PIANO 1 (ROM1A)", "Operator Bell" })
        runOldStateTest (name);
    runFactoryTest();
}
} // namespace OscModeTests

void runOscModeTests() { OscModeTests::run(); }
