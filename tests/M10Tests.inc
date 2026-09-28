// M10: the preset pack's plumbing: 80 more factory tables (and the patch
// migration that moves User table choices past them), choices read back
// exactly, and presets that bounce another preset at load. Included by
// TableTest.cpp.

namespace M10
{
float rawValue (IlanaSynthAudioProcessor& processor, const juce::String& id)
{
    if (const auto* value = processor.apvts.getRawParameterValue (id))
        return value->load();
    return -1.0f;
}

// A state saved before v1.3 (tableSchema 2) with the given table choices.
std::unique_ptr<IlanaSynthAudioProcessor> loadSchema2 (const std::vector<std::pair<juce::String, float>>& tables)
{
    IlanaSynthAudioProcessor source;
    juce::MemoryBlock block;
    source.getStateInformation (block);
    std::unique_ptr<juce::XmlElement> xml (juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()));
    auto state = juce::ValueTree::fromXml (*xml);
    state.setProperty ("tableSchema", 2, nullptr);
    for (const auto& [id, value] : tables)
        for (int i = 0; i < state.getNumChildren(); ++i)
            if (state.getChild (i).getProperty ("id").toString() == id)
                state.getChild (i).setProperty ("value", value, nullptr);
    juce::MemoryBlock data;
    std::unique_ptr<juce::XmlElement> out (state.createXml());
    juce::AudioProcessor::copyXmlToBinary (*out, data);
    auto target = std::make_unique<IlanaSynthAudioProcessor>();
    target->setStateInformation (data.getData(), (int) data.getSize());
    return target;
}

void runPresetIdTest()
{
    IlanaSynthAudioProcessor processor;
    juce::StringArray unknown;
    for (const auto& preset : Presets::getFactoryPresets())
        for (const auto& value : preset.values)
            if (processor.apvts.getParameter (value.id) == nullptr)
                unknown.addIfNotAlreadyThere (juce::String (preset.name) + ": " + value.id);
    check (unknown.isEmpty(), "every factory preset value names a real parameter" + (unknown.isEmpty() ? juce::String() : " (" + unknown.joinIntoString (", ") + ")"));
}

// The resampled presets bounce another preset when they load.
void runBouncePresetTest()
{
    IlanaSynthAudioProcessor processor;
    const auto names = processor.getFactoryPresetNames();
    processor.loadFactoryPreset (names.indexOf ("Bounced Bell Grains"));
    const auto* sample = processor.getSampleForOsc (0);
    check (processor.isSampleEmbedded (0) && sample != nullptr && sample->getNumSamples() > 48000
               && juce::roundToInt (rawValue (processor, "osc1_mode")) == 3,
           "Bounced Bell Grains loads with Church Bell Shell bounced onto OSC 1, played as grains");

    processor.loadFactoryPreset (names.indexOf ("Tabled Cello"));
    const auto choice = juce::roundToInt (rawValue (processor, "osc1_table"));
    const auto slot = choice - TableFactory::getNumFactoryTables();
    check (slot >= 0 && processor.isUserSlotEdited (slot) && juce::roundToInt (rawValue (processor, "osc1_mode")) == 0,
           "Tabled Cello loads with Cello Section cut into a patch wavetable");

    // Loading a plain preset afterwards clears the bounce.
    processor.loadFactoryPreset (names.indexOf ("Bounced Bell Grains"));
    processor.loadFactoryPreset (0);
    processor.flushAsyncUpdates();
    check (! processor.isSampleEmbedded (0), "a plain preset loaded after a resampled one drops its bounce");
}

// Every parameter ID once (pluginval found osc4-6_out declared twice: a
// host sees two parameters with one ID and state restore sets only one).
void runUniqueIdTest()
{
    IlanaSynthAudioProcessor processor;
    std::set<juce::String> seen;
    juce::StringArray duplicates;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            if (! seen.insert (withId->paramID).second)
                duplicates.add (withId->paramID);
    check (duplicates.isEmpty(), "every parameter ID is declared once" + (duplicates.isEmpty() ? juce::String() : " (" + duplicates.joinIntoString (", ") + ")"));

    // And each restores from a saved state.
    IlanaSynthAudioProcessor source;
    juce::Random random (9);
    for (auto* parameter : source.getParameters())
        parameter->setValueNotifyingHost (random.nextFloat());
    juce::MemoryBlock state;
    source.getStateInformation (state);
    IlanaSynthAudioProcessor target;
    target.setStateInformation (state.getData(), (int) state.getSize());
    juce::StringArray wrong;
    for (int i = 0; i < source.getParameters().size(); ++i)
        if (source.getParameters()[i]->getCurrentValueAsText() != target.getParameters()[i]->getCurrentValueAsText())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (target.getParameters()[i]))
                wrong.add (withId->paramID);
    check (wrong.isEmpty(), "every parameter comes back from a saved state" + (wrong.isEmpty() ? juce::String() : " (" + wrong.joinIntoString (", ") + ")"));
}

void runTests()
{
    runUniqueIdTest();
    runPresetIdTest();
    runBouncePresetTest();
    const auto factory = TableFactory::getNumFactoryTables();
    check (factory == 120 && TableFactory::getFactoryTableNames().size() == factory
               && TableFactory::getFactoryTableCategories().size() == factory,
           "120 factory tables, each with a name and a category");

    // Old patches: a User choice moves past the new tables, on all six
    // oscillators; a factory choice stays.
    {
        auto p = loadSchema2 ({ { "osc1_table", 39.0f }, { "osc2_table", 41.0f }, { "osc4_table", 55.0f }, { "osc6_table", 40.0f } });
        check (juce::roundToInt (rawValue (*p, "osc1_table")) == 39 && juce::roundToInt (rawValue (*p, "osc2_table")) == factory + 1
                   && juce::roundToInt (rawValue (*p, "osc4_table")) == factory + 15 && juce::roundToInt (rawValue (*p, "osc6_table")) == factory,
               "a pre-1.3 patch's User table choices move past the new factory tables (all six oscillators)");
        juce::MemoryBlock again;
        p->getStateInformation (again);
        IlanaSynthAudioProcessor reloaded;
        reloaded.setStateInformation (again.getData(), (int) again.getSize());
        check (juce::roundToInt (rawValue (reloaded, "osc2_table")) == factory + 1, "a migrated patch is not migrated twice");
    }

    // Choices come back exactly (the stored value of index 7 of 136 is
    // 6.9999995): the vector pad's corners read through the same path.
    {
        IlanaSynthAudioProcessor processor;
        auto exact = true;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            setParam (processor, "vec_b", (float) osc);
            exact = exact && processor.getVectorCorner (1) == osc;
        }
        check (exact, "choice parameters read back as their exact index");
    }

    // The new tables: finite, full-scale, 64 frames, and all different.
    {
        auto ok = true;
        std::vector<std::vector<float>> middles;
        for (int i = 40; i < factory; ++i)
        {
            const auto frames = TableFactory::generate (i);
            auto peak = 0.0f;
            auto finite = frames.size() == 64;
            for (const auto& frame : frames)
                for (auto v : frame)
                {
                    finite = finite && std::isfinite (v);
                    peak = juce::jmax (peak, std::abs (v));
                }
            if (! finite || peak < 0.5f || peak > 1.01f)
            {
                ok = false;
                std::cout << "INFO: table " << TableFactory::getFactoryTableNames()[i] << " peak " << peak << std::endl;
            }
            middles.push_back (frames.empty() ? std::vector<float> (2048) : frames[32]);
        }
        check (ok, "the 80 new tables are finite and full-scale, 64 frames each");

        auto duplicates = 0;
        for (size_t a = 0; a < middles.size(); ++a)
            for (size_t b = a + 1; b < middles.size(); ++b)
            {
                double ab = 0, aa = 0, bb = 0;
                for (size_t i = 0; i < 2048; ++i)
                {
                    ab += middles[a][i] * middles[b][i];
                    aa += middles[a][i] * middles[a][i];
                    bb += middles[b][i] * middles[b][i];
                }
                if (std::abs (ab) / std::sqrt (aa * bb + 1e-30) > 0.999)
                {
                    ++duplicates;
                    std::cout << "INFO: tables " << TableFactory::getFactoryTableNames()[(int) a + 40] << " and "
                              << TableFactory::getFactoryTableNames()[(int) b + 40] << " match" << std::endl;
                }
            }
        check (duplicates == 0, "no two new tables are the same wave");

        const auto categories = TableFactory::getFactoryTableCategories();
        const auto order = TableFactory::getCategoryOrder();
        auto known = true;
        for (const auto& c : categories)
            known = known && order.contains (c);
        check (known, "every table's category is one the browser shows");
    }
}
} // namespace M10

void runM10Tests()
{
    M10::runTests();
}
