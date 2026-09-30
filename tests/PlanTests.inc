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

void runTests()
{
    runInitTest();
    runAlgorithmFeedbackTest();
}
} // namespace Plan

void runPlanTests()
{
    Plan::runTests();
}
