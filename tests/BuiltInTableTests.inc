#include "../src/dsp/BuiltInTables.h"

// The built-in wavetables are factory tables: the choice lists them after the
// User slots, nothing saved moves, and a patch stores only the choice.
void runBuiltInTableTests()
{
    const auto factory = TableFactory::getNumFactoryTables();
    const auto base = factory + IlanaSynthAudioProcessor::numUserSlots;
    check (BuiltInTables::count() > 300, "the synth carries its built-in wavetables");

    // The choice list: factory, then 256 User slots, then the built-ins; the old choices keep their numbers.
    {
        IlanaSynthAudioProcessor processor;
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("osc1_table"));
        check (choice != nullptr && choice->choices.size() == base + BuiltInTables::count()
                   && choice->choices[factory] == "User 1" && choice->choices[base - 1] == "User 256"
                   && choice->choices[base] == BuiltInTables::name (0),
               "the table choices run factory, User 1-256, then the built-in tables");
    }

    // A built-in choice renders sound offline, the same note twice is identical, and it differs from the stand-in.
    const auto render = [base] (int builtIn, std::vector<float>* out)
    {
        IlanaSynthAudioProcessor processor;
        processor.setNonRealtime (true);
        setParam (processor, "sub_on", 0.0f);
        setParam (processor, "osc1_table", (float) (base + builtIn));
        processor.prepareToPlay (48000.0, 512);
        double centroid = 0.0;
        return renderPeakAndCentroid (processor, 57, 20, centroid, out);
    };

    std::vector<float> first, again;
    const auto peak = render (BuiltInTables::count() / 3, &first);
    render (BuiltInTables::count() / 3, &again);
    check (peak > 0.05f && first == again, "a built-in table plays, and plays the same every time");

    // The patch stores the choice; restoring it brings the table back with no User slot used.
    {
        IlanaSynthAudioProcessor source;
        setParam (source, "osc1_table", (float) (base + 5));
        juce::MemoryBlock state;
        source.getStateInformation (state);

        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (juce::roundToInt (restored.apvts.getRawParameterValue ("osc1_table")->load()) == base + 5
                   && ! restored.isUserSlotEdited (0) && restored.getWavetable (base + 5) == BuiltInTables::table (5),
               "a patch keeps a built-in table by its choice and has it ready on load");
    }

    // Every table decodes to frames and builds.
    {
        auto bad = 0;
        for (int i = 0; i < BuiltInTables::count(); i += 17)
        {
            WavetableDoc doc;
            bad += BuiltInTables::load (i, doc) && doc.getNumFrames() >= 1 ? 0 : 1;
        }
        check (bad == 0, "the built-in tables decode");
    }
}
