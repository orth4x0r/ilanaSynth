// Regressions for the work in the 2026-09-29 product review's plan.
// Included by TableTest.cpp; ILANA_PLAN_TEST=1 runs only these.

namespace Plan
{
// Init opens the filter fully and uses Filtered FM feedback.
void runInitTest()
{
    IlanaSynthAudioProcessor processor;
    processor.loadFactoryPreset (0);
    const auto cutoff = processor.apvts.getRawParameterValue ("f1_cutoff")->load();
    auto filtered = true;
    for (const auto* prefix : OscillatorIds::prefixes)
        filtered = filtered
                   && (int) processor.apvts.getRawParameterValue (juce::String (prefix) + "_fb_type")->load() == FmFeedback::Filtered;
    check (processor.getCurrentPresetName() == "Init" && cutoff > 19999.0f && filtered,
           "Init has the filter open and Filtered FM feedback (cutoff "
               + juce::String (cutoff, 0) + " Hz)");
}

// An FM algorithm's new feedback starts Filtered; feedback the patch
// already had keeps its type.
void runAlgorithmFeedbackTest()
{
    auto withFeedback = -1;
    for (int index = 0; index < FmAlgorithms::count() && withFeedback < 0; ++index)
    {
        const auto& algorithm = FmAlgorithms::all()[(size_t) index];
        for (int op = 0; op < algorithm.numOperators && withFeedback < 0; ++op)
            if (FmAlgorithms::hasRoute (algorithm, op, op))
                withFeedback = index;
    }
    if (withFeedback < 0)
    {
        check (false, "an FM algorithm with feedback exists");
        return;
    }

    const auto& algorithm = FmAlgorithms::all()[(size_t) withFeedback];
    auto op = 0;
    while (! FmAlgorithms::hasRoute (algorithm, op, op))
        ++op;
    const auto typeId = juce::String (OscillatorIds::prefixes[(size_t) op]) + "_fb_type";

    IlanaSynthAudioProcessor fresh;
    setParam (fresh, typeId, (float) FmFeedback::Plain);
    for (int s = 0; s < OscillatorIds::count; ++s)
        for (int t = 0; t < OscillatorIds::count; ++t)
            setParam (fresh, IlanaSynthAudioProcessor::fmRouteId (s, t), 0.0f);
    fresh.applyFmAlgorithm (withFeedback);
    const auto newType = (int) fresh.apvts.getRawParameterValue (typeId)->load();

    IlanaSynthAudioProcessor kept;
    setParam (kept, typeId, (float) FmFeedback::Cross);
    setParam (kept, IlanaSynthAudioProcessor::fmRouteId (op, op), 0.3f);
    kept.applyFmAlgorithm (withFeedback);
    const auto keptType = (int) kept.apvts.getRawParameterValue (typeId)->load();

    check (newType == FmFeedback::Filtered && keptType == FmFeedback::Cross,
           "an FM algorithm's new feedback is Filtered, existing feedback keeps its type");
}

// No choice parameter is host-automatable (a list that grows would move
// recorded lanes); the continuous knobs still are.
void runAutomationTest()
{
    IlanaSynthAudioProcessor processor;
    auto choices = 0, automatableChoices = 0, floats = 0, automatableFloats = 0;
    for (auto* parameter : processor.getParameters())
    {
        if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            ++choices;
            automatableChoices += parameter->isAutomatable() ? 1 : 0;
        }
        else if (dynamic_cast<juce::AudioParameterFloat*> (parameter) != nullptr)
        {
            ++floats;
            automatableFloats += parameter->isAutomatable() ? 1 : 0;
        }
    }
    check (choices > 50 && automatableChoices == 0 && automatableFloats == floats,
           "no choice parameter is automatable (" + juce::String (choices) + " choices, " + juce::String (automatableChoices)
               + " automatable); all " + juce::String (floats) + " knobs are");
}

// 32-voice polyphony: 32 held notes sound at once by default.
void runPolyphonyTest()
{
    IlanaSynthAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;
    for (int note = 0; note < 32; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (1, 36 + note, (juce::uint8) 100), 0);
    processor.processBlock (buffer, midi);
    juce::MidiBuffer none;
    buffer.clear();
    processor.processBlock (buffer, none);
    check (processor.getActiveVoiceCount() == 32, "32 held notes sound on 32 voices ("
                                                    + juce::String (processor.getActiveVoiceCount()) + ")");
}

// The DC blocker before the effects: Warp Envelope Pad left an offset of
// 0.44 of full scale before it.
void runDcBlockTest()
{
    IlanaSynthAudioProcessor processor;
    processor.loadFactoryPreset (processor.getFactoryPresetNames().indexOf ("Warp Envelope Pad"));
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    auto sum = 0.0, count = 0.0;
    for (int block = 0; block < 400; ++block)
    {
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        if (block >= 200) // from about 1 s on
            for (int i = 0; i < 256; ++i)
            {
                sum += 0.5 * (buffer.getSample (0, i) + buffer.getSample (1, i));
                count += 1.0;
            }
    }
    const auto dc = std::abs (sum / juce::jmax (1.0, count));
    check (dc < 0.005, "no DC offset reaches the output (Warp Envelope Pad: " + juce::String (dc, 5) + ")");
}

void runTests()
{
    runDcBlockTest();
    runPolyphonyTest();
    runAutomationTest();
    runInitTest();
    runAlgorithmFeedbackTest();
}
} // namespace Plan

void runPlanTests()
{
    Plan::runTests();
}
