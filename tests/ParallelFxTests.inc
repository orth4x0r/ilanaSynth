// FX routing (fx_routing, design round 2): SERIES is the old rack (each
// effect feeds the next one); PARALLEL runs every working slot on the rack's
// input and averages the branches. Included by TableTest.cpp;
// ILANA_PARALLEL_FX_TEST=1 runs only these.

namespace ParallelFxLab
{
inline void set (IlanaSynthAudioProcessor& processor, const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// A processor whose rack holds the given types in slots 1, 2, ... (the
// rest empty), Drive on at x5, Utility at -6 dB, every MIX 100 %.
inline void setUp (IlanaSynthAudioProcessor& processor, std::initializer_list<int> types, bool parallel)
{
    auto slot = 1;
    for (const auto type : types)
        processor.assignFxSlot (slot++, type);
    for (; slot <= IlanaSynthAudioProcessor::numFxSlots; ++slot)
        processor.assignFxSlot (slot, 0);
    for (int s = 1; s <= IlanaSynthAudioProcessor::numFxSlots; ++s)
    {
        set (processor, "fx_slot" + juce::String (s) + "_mix", 1.0f);
        set (processor, "fx_slot" + juce::String (s) + "_band", 0.0f);
        set (processor, "fx_slot" + juce::String (s) + "_solo", 0.0f);
        set (processor, "fx_slot" + juce::String (s) + "_bypass", 0.0f);
    }
    set (processor, "fx_drive_on", 1.0f);
    set (processor, "fx_drive_amount", 5.0f);
    set (processor, "fx_drive_mix", 1.0f);
    set (processor, "fx_fold", 0.0f);
    set (processor, "fx_util_gain", -6.0f);
    set (processor, "fx_routing", parallel ? 1.0f : 0.0f);
}

// A stereo test signal: two tones, a little different per channel.
inline float input (int channel, int n)
{
    const auto t = (double) n / 48000.0;
    return (float) (0.4 * std::sin (juce::MathConstants<double>::twoPi * (110.0 + 3.0 * channel) * t)
                    + 0.2 * std::sin (juce::MathConstants<double>::twoPi * 1700.0 * t));
}

// Runs the rack over 40 blocks of 256 samples; returns both channels in a row.
inline std::vector<float> run (IlanaSynthAudioProcessor& processor, const std::vector<float>* source = nullptr)
{
    processor.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buffer (2, 256);
    std::vector<float> out;
    for (int block = 0; block < 40; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (channel, i, source != nullptr ? (*source)[(size_t) ((block * 2 + channel) * 256 + i)]
                                                                : input (channel, block * 256 + i));
        processor.processEffects (buffer);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                out.push_back (buffer.getSample (channel, i));
    }
    return out;
}

inline std::vector<float> dry()
{
    std::vector<float> out;
    for (int block = 0; block < 40; ++block)
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                out.push_back (input (channel, block * 256 + i));
    return out;
}

inline float worstDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size())
        return 1.0e9f;
    auto worst = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        worst = juce::jmax (worst, std::abs (a[i] - b[i]));
    return worst;
}

inline std::vector<float> rendered (std::initializer_list<int> types, bool parallel, const std::function<void (IlanaSynthAudioProcessor&)>& tweak = {},
                                    const std::vector<float>* source = nullptr)
{
    IlanaSynthAudioProcessor processor;
    setUp (processor, types, parallel);
    if (tweak)
        tweak (processor);
    return run (processor, source);
}
} // namespace ParallelFxLab

void runParallelFxTests()
{
    using namespace ParallelFxLab;
    constexpr int drive = 2, utility = 19;
    const auto input = dry();

    // A new processor and every state without the parameter are series.
    {
        IlanaSynthAudioProcessor processor;
        check (processor.apvts.getRawParameterValue ("fx_routing") != nullptr
                   && processor.apvts.getRawParameterValue ("fx_routing")->load() == 0.0f,
               "fx_routing exists and defaults to series");
    }

    // SERIES is the old rack: Drive then Utility is Drive's output run through
    // Utility, sample for sample.
    {
        const auto driveOnly = rendered ({ drive }, false);
        const auto thenUtility = rendered ({ utility }, false, {}, &driveOnly);
        const auto chain = rendered ({ drive, utility }, false);
        const auto reversed = rendered ({ utility, drive }, false);
        check (worstDifference (chain, thenUtility) == 0.0f, "series: Drive -> Utility equals Drive's output through Utility, bit for bit");
        check (worstDifference (chain, reversed) > 0.01f, "series: the order matters (Utility -> Drive differs)");
    }

    // PARALLEL: the branches averaged, each one the slot alone on the input.
    {
        const auto driveOnly = rendered ({ drive }, false);
        const auto utilityOnly = rendered ({ utility }, false);
        const auto both = rendered ({ drive, utility }, true);
        const auto reversed = rendered ({ utility, drive }, true);
        auto worst = 0.0f;
        for (size_t i = 0; i < both.size(); ++i)
            worst = juce::jmax (worst, std::abs (both[i] - 0.5f * (driveOnly[i] + utilityOnly[i])));
        check (worst < 1.0e-6f, "parallel: Drive + Utility is the average of each alone (worst " + juce::String (worst, 8) + ")");
        check (worstDifference (both, reversed) < 1.0e-6f, "parallel: the order of the slots does not matter");

        // One working slot sounds the same either way.
        check (worstDifference (rendered ({ drive }, true), driveOnly) == 0.0f, "parallel with one working slot equals series");

        // Branches that leave the sound alone give it back unchanged.
        const auto unity = [] (IlanaSynthAudioProcessor& p) { set (p, "fx_util_gain", 0.0f); set (p, "fx_drive_mix", 0.0f); };
        check (worstDifference (rendered ({ drive, utility }, true, unity), input) < 1.0e-6f,
               "parallel: two effects that leave the sound alone give back the input");
    }

    // Bypass, solo and MIX keep working in parallel.
    {
        const auto utilityOnly = rendered ({ utility }, false);
        const auto driveOnly = rendered ({ drive }, false);
        const auto bypassed = rendered ({ drive, utility }, true, [] (IlanaSynthAudioProcessor& p) { set (p, "fx_slot1_bypass", 1.0f); });
        check (worstDifference (bypassed, utilityOnly) < 1.0e-6f, "parallel: a bypassed slot leaves the sum (Utility alone, at full level)");

        const auto soloed = rendered ({ drive, utility }, true, [] (IlanaSynthAudioProcessor& p) { set (p, "fx_slot2_solo", 1.0f); });
        check (worstDifference (soloed, utilityOnly) < 1.0e-6f, "parallel: a soloed slot is heard alone");

        const auto halfMix = rendered ({ drive, utility }, true, [] (IlanaSynthAudioProcessor& p) { set (p, "fx_slot1_mix", 0.5f); });
        const auto halfDrive = rendered ({ drive }, false, [] (IlanaSynthAudioProcessor& p) { set (p, "fx_slot1_mix", 0.5f); });
        auto worst = 0.0f;
        for (size_t i = 0; i < halfMix.size(); ++i)
            worst = juce::jmax (worst, std::abs (halfMix[i] - 0.5f * (halfDrive[i] + utilityOnly[i])));
        check (worst < 1.0e-6f && worstDifference (halfMix, rendered ({ drive, utility }, true)) > 0.01f,
               "parallel: a slot's MIX blends its own branch (worst " + juce::String (worst, 8) + ")");

        const auto silent = rendered ({ drive, utility }, true, [] (IlanaSynthAudioProcessor& p)
                                      { set (p, "fx_slot1_bypass", 1.0f); set (p, "fx_slot2_bypass", 1.0f); });
        check (worstDifference (silent, input) < 1.0e-6f && driveOnly.size() == input.size(),
               "parallel: a rack with every slot bypassed passes the input");
    }

    // The level meters (the rail's dB chips) read each branch's own gain.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive, utility }, true);
        run (processor);
        const auto utilityDb = juce::Decibels::gainToDecibels (processor.getFxSlotOutLevel (1) / processor.getFxSlotInLevel (1));
        const auto sameInput = std::abs (processor.getFxSlotInLevel (0) - processor.getFxSlotInLevel (1)) < 1.0e-5f;
        check (std::abs (utilityDb + 6.0f) < 0.05f && sameInput,
               "parallel: both branches hear the same input, Utility's chip reads -6 dB (" + juce::String (utilityDb, 2) + ")");
    }

    // State: parallel survives a save and load; a state without the
    // parameter (every older patch) loads as series, also over a parallel one.
    {
        IlanaSynthAudioProcessor source;
        setUp (source, { drive, utility }, true);
        juce::MemoryBlock saved;
        source.getStateInformation (saved);

        IlanaSynthAudioProcessor loaded;
        loaded.setStateInformation (saved.getData(), (int) saved.getSize());
        check (loaded.apvts.getRawParameterValue ("fx_routing")->load() == 1.0f, "state: PARALLEL survives a save and load");
        check (worstDifference (run (loaded), rendered ({ drive, utility }, true)) < 1.0e-6f, "state: the loaded rack renders as the saved one");

        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        auto removed = 0;
        if (xml != nullptr)
            for (auto* child = xml->getFirstChildElement(); child != nullptr;)
            {
                auto* next = child->getNextElement();
                if (child->getStringAttribute ("id") == "fx_routing")
                {
                    xml->removeChildElement (child, true);
                    ++removed;
                }
                child = next;
            }
        juce::MemoryBlock old;
        if (xml != nullptr)
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
        loaded.setStateInformation (old.getData(), (int) old.getSize());
        check (removed == 1 && loaded.apvts.getRawParameterValue ("fx_routing")->load() == 0.0f,
               "state: a patch saved before fx_routing loads as series, even over a parallel rack");

        // CHAIN 1 and CHAIN 2 keep their own routing.
        set (loaded, "fx_routing", 1.0f);
        loaded.switchFxChain();
        set (loaded, "fx_routing", 0.0f);
        loaded.switchFxChain();
        const auto chainOne = loaded.apvts.getRawParameterValue ("fx_routing")->load();
        loaded.switchFxChain();
        const auto chainTwo = loaded.apvts.getRawParameterValue ("fx_routing")->load();
        check (chainOne == 1.0f && chainTwo == 0.0f, "CHAIN 1 and CHAIN 2 each keep their routing");

        set (loaded, "fx_routing", 1.0f);
        loaded.loadFactoryPreset (0);
        check (loaded.apvts.getRawParameterValue ("fx_routing")->load() == 0.0f, "a factory preset loads as series");
    }
}
