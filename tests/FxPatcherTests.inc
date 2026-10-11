// Roadmap step 17: effect instances and the FX patcher (FxPatch.h,
// FxPatcher.cpp). Included by TableTest.cpp after ParallelFxTests.inc (it
// uses its rack helpers); ILANA_FX_PATCHER_TEST=1 runs only these.

namespace FxPatcherLab
{
using namespace ParallelFxLab;

constexpr int drive = 2, delay = 9, reverb = 13, chorus = 7, utility = 19;

inline bool finiteUnder (const std::vector<float>& samples, float limit)
{
    for (const auto sample : samples)
        if (! std::isfinite (sample) || std::abs (sample) > limit)
            return false;
    return true;
}

inline float peakOfSamples (const std::vector<float>& samples, size_t from = 0)
{
    auto peak = 0.0f;
    for (auto i = from; i < samples.size(); ++i)
        peak = juce::jmax (peak, std::abs (samples[i]));
    return peak;
}

// Delays that ring out audibly within the test's 40 blocks.
inline void delaySettings (IlanaSynthAudioProcessor& processor, float time)
{
    set (processor, "fx_delay_on", 1.0f);
    set (processor, "fx_delay_sync", 0.0f);
    set (processor, "fx_delay_time", time);
    set (processor, "fx_delay_feedback", 0.4f);
    set (processor, "fx_delay_mix", 0.5f);
}

inline PatchGraph::NodeId nodeOfSlot (const PatchGraph::Graph& graph, int slot)
{
    for (const auto& node : graph.nodes())
        if (! node.isFixed() && node.ref == slot + 1)
            return node.id;
    return 0;
}
} // namespace FxPatcherLab

void runFxPatcherTests()
{
    using namespace FxPatcherLab;
    const auto input = dry();

    // Every effect parameter belongs to exactly one effect type (an
    // instance copies its type's parameters by these prefixes).
    {
        IlanaSynthAudioProcessor processor;
        juce::StringArray unclaimed, doubled;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            {
                const auto id = withId->paramID;
                if (! id.startsWith ("fx_") || id.startsWith ("fx_slot") || id == "fx_routing" || id == "fx_patcher"
                    || id.startsWith ("fx_split_") || id.startsWith ("fx_xover"))
                    continue;
                auto owners = 0;
                for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
                    owners += FxPatchData::isParamOf (type, id) ? 1 : 0;
                if (owners == 0)
                    unclaimed.add (id);
                if (owners > 1)
                    doubled.add (id);
            }
        check (unclaimed.isEmpty() && doubled.isEmpty(),
               "every effect parameter belongs to one effect type (unclaimed: " + unclaimed.joinIntoString (", ") + "; doubled: "
                   + doubled.joinIntoString (", ") + ")");
        check (processor.apvts.getRawParameterValue ("fx_patcher") != nullptr && processor.apvts.getRawParameterValue ("fx_patcher")->load() == 0.0f,
               "fx_patcher exists and is off by default");
        check (processor.getFxPatch().isDefault(), "a new processor has no instances and no patcher graph");
    }

    // The rack runs through its compiled plan once it is made, and renders
    // as the fixed loop did.
    {
        const auto fixed = rendered ({ drive, utility, reverb }, false, [] (IlanaSynthAudioProcessor& processor) { set (processor, "fx_reverb_on", 1.0f); });
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive, utility, reverb }, false);
        set (processor, "fx_reverb_on", 1.0f);
        processor.prepareToPlay (48000.0, 256);
        check (processor.isFxRackPlanCurrent(), "after prepareToPlay the rack's plan is the rack as it is");
        set (processor, "fx_slot2_bypass", 1.0f);
        check (! processor.isFxRackPlanCurrent(), "a bypass makes the rack's plan stale (the fixed loop stands in until the new one)");
        processor.compileFxPlansNow();
        check (processor.isFxRackPlanCurrent(), "compileFxPlansNow makes the plan current again");
        set (processor, "fx_slot2_bypass", 0.0f);
        const auto again = run (processor);
        check (worstDifference (fixed, again) == 0.0f, "a rack with its plan renders sample for sample as before");
    }

    // PATCHER on, with the graph it starts from (the rack as it is):
    // nothing changes, in series or in parallel.
    for (const auto parallel : { false, true })
    {
        const auto tweak = [] (IlanaSynthAudioProcessor& processor)
        {
            set (processor, "fx_reverb_on", 1.0f);
            delaySettings (processor, 120.0f);
        };
        const auto rack = rendered ({ drive, delay, reverb, chorus }, parallel, tweak);
        const auto patched = rendered ({ drive, delay, reverb, chorus }, parallel, [&] (IlanaSynthAudioProcessor& processor)
        {
            tweak (processor);
            processor.setFxPatcherOn (true);
        });
        check (worstDifference (rack, patched) < 1.0e-6f,
               juce::String ("PATCHER on renders the rack it started from (") + (parallel ? "parallel" : "series") + ", worst "
                   + juce::String (worstDifference (rack, patched), 9) + ")");
    }

    // Two Delays, each its own instance: each sounds as it would alone.
    {
        const auto alone = [] (float time)
        {
            return rendered ({ delay }, false, [time] (IlanaSynthAudioProcessor& processor) { delaySettings (processor, time); });
        };
        const auto first = alone (120.0f), second = alone (310.0f);

        IlanaSynthAudioProcessor processor;
        setUp (processor, { delay }, false);
        delaySettings (processor, 120.0f);
        const auto slot = processor.addFxEffect (delay);
        check (slot == 1 && processor.isFxInstance (1) && processor.isFxSlotInFront (1) && ! processor.isFxSlotInFront (0),
               "adding a Delay the rack has makes a second instance, its card in front");
        set (processor, "fx_delay_time", 310.0f); // the instance in front: slot 2
        const auto patch = processor.getFxPatch();
        auto firstTime = 0.0f;
        for (const auto& [id, value] : patch.slots[0].values)
            if (id == "fx_delay_time")
                firstTime = value;
        check (firstTime == 120.0f, "the first Delay keeps its own copy of its settings (" + juce::String (firstTime) + " ms)");

        set (processor, "fx_slot2_bypass", 1.0f);
        check (worstDifference (run (processor), first) == 0.0f, "the first instance alone renders as a lone Delay with its settings");
        set (processor, "fx_slot2_bypass", 0.0f);
        set (processor, "fx_slot1_bypass", 1.0f);
        check (worstDifference (run (processor), second) == 0.0f, "the second instance alone renders as a lone Delay with its settings, on its own engine");
        set (processor, "fx_slot1_bypass", 0.0f);
        const auto both = run (processor);
        check (worstDifference (both, first) > 1.0e-3f && worstDifference (both, second) > 1.0e-3f, "the two in series are neither one alone");

        // Swapping which card is in front changes nothing heard.
        IlanaSynthAudioProcessor swapped, twin;
        for (auto* each : { &swapped, &twin })
        {
            setUp (*each, { delay }, false);
            delaySettings (*each, 120.0f);
            each->addFxEffect (delay);
            set (*each, "fx_delay_time", 310.0f);
            each->prepareToPlay (48000.0, 256);
        }
        {
        auto& processor = swapped;
        juce::AudioBuffer<float> a (2, 256), b (2, 256);
        auto worst = 0.0f;
        for (int block = 0; block < 40; ++block)
        {
            if (block == 15)
            {
                processor.focusFxInstance (0);
                check (processor.isFxSlotInFront (0) && ! processor.isFxSlotInFront (1)
                           && std::abs (processor.apvts.getRawParameterValue ("fx_delay_time")->load() - 120.0f) < 1.0e-3f,
                       "EDIT on the first instance puts its settings on the controls");
            }
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                    a.setSample (channel, i, ParallelFxLab::input (channel, block * 256 + i)), b.setSample (channel, i, ParallelFxLab::input (channel, block * 256 + i));
            processor.processEffects (a);
            twin.processEffects (b);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                    worst = juce::jmax (worst, std::abs (a.getSample (channel, i) - b.getSample (channel, i)));
        }
        check (worst == 0.0f, "swapping the instance in front is silent (worst " + juce::String (worst, 9) + ")");

        // Undo puts the swap back, settings and all.
        processor.performEdit ("Edit Delay", [&] { processor.focusFxInstance (1); });
        check (processor.isFxSlotInFront (1) && std::abs (processor.apvts.getRawParameterValue ("fx_delay_time")->load() - 310.0f) < 1.0e-3f,
               "EDIT swaps again (" + juce::String ((int) processor.isFxSlotInFront (1)) + ", " + juce::String (processor.apvts.getRawParameterValue ("fx_delay_time")->load(), 6) + ")");
        processor.getUndoManager().undo();
        check (processor.isFxSlotInFront (0) && ! processor.isFxSlotInFront (1)
                   && std::abs (processor.apvts.getRawParameterValue ("fx_delay_time")->load() - 120.0f) < 1.0e-3f,
               "undo puts the first instance back in front with its settings");

        // Removing the card in front brings the other forward with its settings.
        processor.removeFxEffect (0);
        check (processor.isFxSlotInFront (1) && std::abs (processor.apvts.getRawParameterValue ("fx_delay_time")->load() - 310.0f) < 1.0e-3f
                   && processor.getFxPatch().slots[0].isDefault(),
               "removing the instance in front brings the other forward (" + juce::String ((int) processor.isFxSlotInFront (1)) + ", "
                   + juce::String (processor.apvts.getRawParameterValue ("fx_delay_time")->load(), 6) + ", " + juce::String ((int) processor.getFxPatch().slots[0].isDefault()) + ")");
        }
    }

    // A slot whose effect changes loses its instance.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { delay }, false);
        processor.addFxEffect (delay);
        processor.assignFxSlot (2, reverb);
        processor.compileFxPlansNow();
        check (processor.getFxPatch().slots[1].isDefault() && processor.isFxSlotInFront (0), "a slot given another effect drops its instance; the Delay left is in front");
    }

    // Saving and loading: instances, their settings and the patcher graph.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive, delay, reverb }, false);
        delaySettings (processor, 200.0f);
        set (processor, "fx_reverb_on", 1.0f);
        processor.addFxEffect (delay);
        set (processor, "fx_delay_time", 90.0f);
        processor.setFxPatcherOn (true);
        auto graph = processor.getFxPatchGraph();
        graph.connect ({ nodeOfSlot (graph, 2), 0 }, { nodeOfSlot (graph, 0), 0 }); // Reverb back into Drive
        processor.setFxPatchGraph (graph);
        const auto saved = processor.getFxPatch();
        const auto loops = saved.graph.feedbackCables();
        auto loopGain = 0.0f;
        for (size_t c = 0; c < loops.size(); ++c)
            if (loops[c])
                loopGain = saved.graph.cables()[c].gain;
        check (loopGain == 0.5f, "a new loop cable starts at 50 % (" + juce::String (loopGain) + ")");

        juce::MemoryBlock state;
        processor.getStateInformation (state);
        IlanaSynthAudioProcessor loaded;
        loaded.setStateInformation (state.getData(), (int) state.getSize());
        loaded.compileFxPlansNow();
        check (FxPatchData::samePatch (saved, loaded.getFxPatch()) && loaded.isFxPatcherOn(), "instances and the patcher graph survive a save and load");
        const auto before = run (processor), after = run (loaded);
        check (worstDifference (before, after) == 0.0f && finiteUnder (after, 8.0f), "the loaded patch renders the same, loop included");

        // An older state (no FxPatch child) loads with none.
        IlanaSynthAudioProcessor fresh;
        juce::MemoryBlock plain;
        fresh.getStateInformation (plain);
        check (! plain.toString().contains ("FxPatch"), "a patch that uses none of it saves no FxPatch child");
        loaded.setStateInformation (plain.getData(), (int) plain.getSize());
        loaded.compileFxPlansNow();
        check (loaded.getFxPatch().isDefault() && ! loaded.isFxPatcherOn(), "loading a patch without one clears the instances and the graph");

        // Chain 1 / 2 carries it too.
        processor.switchFxChain();
        processor.switchFxChain();
        processor.compileFxPlansNow();
        check (FxPatchData::samePatch (saved, processor.getFxPatch()), "the FX chain switch keeps the instances and the graph");
    }

    // Loops stay bounded: loop cables are held to 70 %, and the ring clamps.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive, delay }, false);
        delaySettings (processor, 30.0f);
        set (processor, "fx_delay_feedback", 0.95f);
        processor.setFxPatcherOn (true);
        auto graph = processor.getFxPatchGraph();
        graph.connect ({ nodeOfSlot (graph, 1), 0 }, { nodeOfSlot (graph, 0), 0 }, 4.0f);
        processor.setFxPatchGraph (graph);
        for (const auto& cable : processor.getFxPatch().graph.cables())
            if (cable.from.node == nodeOfSlot (graph, 1) && cable.to.node == nodeOfSlot (graph, 0))
                check (cable.gain == 0.7f, "a loop cable is held to 70 % (" + juce::String (cable.gain) + ")");
        std::vector<float> loud (input.size() * 3);
        for (size_t i = 0; i < loud.size(); ++i)
            loud[i] = i < input.size() ? input[i] * 2.0f : 0.0f;
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (2, 256);
        std::vector<float> out;
        for (int block = 0; block < 120; ++block)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (channel, i, block < 40 ? 2.0f * ParallelFxLab::input (channel, block * 256 + i) : 0.0f);
            processor.processEffects (buffer);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                    out.push_back (buffer.getSample (channel, i));
        }
        check (finiteUnder (out, 8.0f), "a hot loop through Drive and a 95 % Delay stays finite and under +18 dB (peak "
                                          + juce::String (peakOfSamples (out), 3) + ")");
    }

    // Removing an effect joins the cables around it; adding one wires it in before OUTPUT.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive, chorus, reverb }, false);
        processor.setFxPatcherOn (true);
        const auto graph = processor.getFxPatchGraph();
        const auto a = nodeOfSlot (graph, 0), c = nodeOfSlot (graph, 2);
        processor.removeFxEffect (1);
        const auto after = processor.getFxPatchGraph();
        check (after.findCable ({ a, 0 }, { c, 0 }) >= 0 && nodeOfSlot (after, 1) == 0, "removing the middle effect joins its neighbours");
        const auto slot = processor.addFxEffect (utility);
        const auto added = processor.getFxPatchGraph();
        const auto u = nodeOfSlot (added, slot);
        check (u != 0 && added.findCable ({ c, 0 }, { u, 0 }) >= 0 && added.findCable ({ u, 0 }, { PatchGraph::outputNodeId, 0 }) >= 0,
               "a new effect is wired in before OUTPUT");
    }

    // Parallel branches of two instances in the patcher sum as cables say.
    {
        const auto patched = rendered ({ delay }, false, [] (IlanaSynthAudioProcessor& processor)
        {
            delaySettings (processor, 120.0f);
            processor.addFxEffect (delay);
            set (processor, "fx_delay_time", 310.0f);
            processor.setFxPatcherOn (true);
            auto graph = processor.getFxPatchGraph();
            const auto one = nodeOfSlot (graph, 0), two = nodeOfSlot (graph, 1);
            for (const auto& cable : std::vector<PatchGraph::Cable> (graph.cables()))
                graph.disconnect (cable.from, cable.to);
            graph.connect ({ PatchGraph::inputNodeId, 0 }, { one, 0 });
            graph.connect ({ PatchGraph::inputNodeId, 0 }, { two, 0 });
            graph.connect ({ one, 0 }, { PatchGraph::outputNodeId, 0 }, 0.5f);
            graph.connect ({ two, 0 }, { PatchGraph::outputNodeId, 0 }, 0.5f);
            processor.setFxPatchGraph (graph);
        });
        const auto parallel = rendered ({ delay }, true, [] (IlanaSynthAudioProcessor& processor)
        {
            delaySettings (processor, 120.0f);
            processor.addFxEffect (delay);
            set (processor, "fx_delay_time", 310.0f);
        });
        check (worstDifference (patched, parallel) < 1.0e-6f, "two Delay instances wired side by side at 50 % render as PARALLEL (worst "
                                                                  + juce::String (worstDifference (patched, parallel), 9) + ")");
    }
}
