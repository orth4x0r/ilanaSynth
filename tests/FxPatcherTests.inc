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

// Runs `blocks` blocks of 256 samples (the test tones at `level` for the
// first `loudBlocks`, then silence); returns the peak from `from` on.
inline float runPeak (IlanaSynthAudioProcessor& processor, int blocks, int loudBlocks, float level, int from = 0)
{
    juce::AudioBuffer<float> buffer (2, 256);
    auto peak = 0.0f;
    for (int block = 0; block < blocks; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (channel, i, block < loudBlocks ? level * ParallelFxLab::input (channel, block * 256 + i) : 0.0f);
        processor.processEffects (buffer);
        if (block >= from)
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 256; ++i)
                {
                    const auto x = buffer.getSample (channel, i);
                    peak = std::isfinite (x) ? juce::jmax (peak, std::abs (x)) : 1.0e9f;
                }
    }
    return peak;
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
        check (loopGain == 0.2f, "a new loop cable starts at 20 % (" + juce::String (loopGain) + ")");

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


    // ilana's patch (2026-10-11): Reverb into Smear and EQ, Smear into EQ
    // too, and an EQ to Reverb loop. Two branches feed the Reverb's output
    // into EQ, so the loop's gain is over 1 at 50 %: it grew to +24 dB and
    // never stopped. The loop guard and the output guard hold it at 0 dB
    // even at the loop's 70 % top; at a new loop's level it dies away; and
    // once the loop cable is gone the patch sounds as before.
    for (const auto loopLevel : { 0.0f, 0.7f })
    {
        IlanaSynthAudioProcessor processor;
        constexpr int smear = 11, eq = 29;
        setUp (processor, { reverb, smear, eq }, false);
        set (processor, "fx_reverb_on", 1.0f);
        set (processor, "fx_smear_on", 1.0f);
        processor.setFxPatcherOn (true);
        auto graph = processor.getFxPatchGraph();
        const auto r = nodeOfSlot (graph, 0), e = nodeOfSlot (graph, 2);
        graph.connect ({ r, 0 }, { e, 0 });
        graph.connect ({ e, 0 }, { r, 0 }); // (drawn: starts at a new loop's level)
        processor.setFxPatchGraph (graph);
        if (loopLevel > 0.0f)
        {
            auto hot = processor.getFxPatchGraph();
            hot.setCableGain (hot.findCable ({ e, 0 }, { r, 0 }), loopLevel);
            processor.setFxPatchGraph (hot);
        }
        processor.compileFxPlansNow();
        processor.prepareToPlay (48000.0, 256);
        const auto during = runPeak (processor, 400, 40, 1.0f);
        const auto late = runPeak (processor, 200, 0, 0.0f, 100);
        auto unlooped = processor.getFxPatchGraph();
        unlooped.disconnect ({ e, 0 }, { r, 0 });
        processor.setFxPatchGraph (unlooped);
        processor.compileFxPlansNow();
        const auto after = runPeak (processor, 600, 0, 0.0f, 400);
        const auto fresh = runPeak (processor, 100, 40, 1.0f);
        const auto name = loopLevel > 0.0f ? juce::String ("at 70 %") : juce::String ("at a new loop's level");
        check (during <= 1.0f && late <= 1.0f, "ilana's Reverb, Smear, EQ loop " + name + " stays at or under 0 dB (peak "
                                                   + juce::String (juce::jmax (during, late), 3) + ")");
        if (loopLevel == 0.0f)
            check (late < 0.01f, "at a new loop's level it dies away (2 s after the input stops: " + juce::String (late, 5) + ")");
        check (after < 1.0e-3f && std::isfinite (fresh) && fresh > 0.1f && fresh < 4.0f,
               "with the loop unpatched " + name + " it falls silent and plays again (" + juce::String (after, 5) + ", "
                   + juce::String (fresh, 3) + ")");
    }

    // ilana's second patch: Reverb and Smear in series with a Smear to
    // Reverb loop. At 50 % it "kept on feeding back without cutting off";
    // at a new loop's level it dies away.
    {
        IlanaSynthAudioProcessor processor;
        constexpr int smear = 11;
        setUp (processor, { reverb, smear }, false);
        set (processor, "fx_reverb_on", 1.0f);
        set (processor, "fx_smear_on", 1.0f);
        processor.setFxPatcherOn (true);
        auto graph = processor.getFxPatchGraph();
        graph.connect ({ nodeOfSlot (graph, 1), 0 }, { nodeOfSlot (graph, 0), 0 });
        processor.setFxPatchGraph (graph);
        processor.compileFxPlansNow();
        processor.prepareToPlay (48000.0, 256);
        runPeak (processor, 400, 40, 1.0f);
        const auto late = runPeak (processor, 200, 0, 0.0f, 100);
        check (late < 0.01f, "a Smear to Reverb loop at a new loop's level dies away (2 s after the input stops: " + juce::String (late, 5) + ")");
    }

    // A loop on every effect type: an effect fed back into itself at the
    // loop's 70 % top, with the test tones twice as loud, stays finite and
    // at or under 0 dB, and after the loop cable is removed the effect plays
    // again (finite, not silent).
    {
        juce::StringArray runaway;
        for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
        {
            IlanaSynthAudioProcessor processor;
            setUp (processor, { type }, false);
            for (auto* parameter : processor.getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                    if (withId->paramID.startsWith ("fx_") && withId->paramID.endsWith ("_on"))
                        set (processor, withId->paramID, 1.0f);
            processor.setFxPatcherOn (true);
            auto graph = processor.getFxPatchGraph();
            const auto node = nodeOfSlot (graph, 0);
            graph.connect ({ node, 0 }, { node, 0 });
            processor.setFxPatchGraph (graph);
            auto hot = processor.getFxPatchGraph();
            hot.setCableGain (hot.findCable ({ node, 0 }, { node, 0 }), 0.7f);
            processor.setFxPatchGraph (hot);
            processor.compileFxPlansNow();
            processor.prepareToPlay (48000.0, 256);
            const auto looped = runPeak (processor, 300, 40, 2.0f);
            auto unlooped = processor.getFxPatchGraph();
            unlooped.disconnect ({ node, 0 }, { node, 0 });
            processor.setFxPatchGraph (unlooped);
            processor.compileFxPlansNow();
            const auto again = runPeak (processor, 80, 40, 1.0f, 20);
            if (! (looped <= 1.0f) || ! std::isfinite (again) || again > 1.0e3f)
                runaway.add ("type " + juce::String (type) + " (" + juce::String (looped, 3) + ", then " + juce::String (again, 3) + ")");
        }
        check (runaway.isEmpty(), "a hot loop on every effect type stays at or under 0 dB and the effect plays again after it ("
                                      + runaway.joinIntoString (", ") + ")");
    }

    // FILTER (type 42, ilana 2026-10-11: an effect fed back into a filter).
    // Its low pass passes the 110 Hz tone and cuts 5 kHz; every filter model
    // at full resonance and drive, in a hot loop with a Reverb (Reverb >
    // FILTER > back into Reverb at 70 %), stays finite and at or under 0 dB,
    // and plays again after the loop is unpatched.
    {
        constexpr int filterFx = 42;
        const auto tonePeak = [] (IlanaSynthAudioProcessor& processor, double hz)
        {
            processor.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> buffer (2, 256);
            auto peak = 0.0f;
            for (int block = 0; block < 60; ++block)
            {
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 256; ++i)
                        buffer.setSample (channel, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (block * 256 + i) / 48000.0));
                processor.processEffects (buffer);
                if (block >= 30)
                    peak = juce::jmax (peak, buffer.getMagnitude (0, 0, 256));
            }
            return peak;
        };
        IlanaSynthAudioProcessor processor;
        setUp (processor, { filterFx }, false);
        set (processor, "fx_filter_type", 0.0f);
        set (processor, "fx_filter_cutoff", 300.0f);
        set (processor, "fx_filter_reso", 0.0f);
        const auto low = tonePeak (processor, 110.0), high = tonePeak (processor, 5000.0);
        check (low > 0.4f && low < 0.7f && high < 0.05f,
               "FILTER's low pass at 300 Hz passes 110 Hz and cuts 5 kHz (" + juce::String (low, 3) + ", " + juce::String (high, 4) + ")");

        juce::StringArray runaway;
        for (int model = 0; model < FilterType::Count; ++model)
        {
            IlanaSynthAudioProcessor looped;
            setUp (looped, { reverb, filterFx }, false);
            set (looped, "fx_reverb_on", 1.0f);
            set (looped, "fx_filter_type", (float) model);
            set (looped, "fx_filter_cutoff", 800.0f);
            set (looped, "fx_filter_reso", 1.0f);
            set (looped, "fx_filter_drive", 10.0f);
            set (looped, "fx_filter_morph", 0.5f);
            looped.setFxPatcherOn (true);
            auto graph = looped.getFxPatchGraph();
            const auto verb = nodeOfSlot (graph, 0), filter = nodeOfSlot (graph, 1);
            graph.connect ({ filter, 0 }, { verb, 0 });
            looped.setFxPatchGraph (graph);
            auto hot = looped.getFxPatchGraph();
            hot.setCableGain (hot.findCable ({ filter, 0 }, { verb, 0 }), 0.7f);
            looped.setFxPatchGraph (hot);
            looped.compileFxPlansNow();
            looped.prepareToPlay (48000.0, 256);
            const auto peak = runPeak (looped, 300, 40, 2.0f);
            auto unlooped = looped.getFxPatchGraph();
            unlooped.disconnect ({ filter, 0 }, { verb, 0 });
            looped.setFxPatchGraph (unlooped);
            looped.compileFxPlansNow();
            const auto again = runPeak (looped, 80, 40, 1.0f, 20);
            if (! (peak <= 1.0f) || ! std::isfinite (again) || again > 1.0e3f || again <= 0.0f)
                runaway.add (FilterType::getNames()[model] + " (" + juce::String (peak, 3) + ", then " + juce::String (again, 3) + ")");
        }
        check (runaway.isEmpty(), "a Reverb > FILTER > Reverb loop at 70 % on every filter model (full reso and drive) stays at or under 0 dB "
                                  "and plays again unpatched (" + runaway.joinIntoString (", ") + ")");
    }

    // The loop delay: a loop cable's own delay, from one chunk up to
    // maxLoopDelayMs; it saves and loads with the patch.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { utility }, false);
        set (processor, "fx_util_gain", 0.0f);
        processor.setFxPatcherOn (true);
        auto graph = processor.getFxPatchGraph();
        const auto node = nodeOfSlot (graph, 0);
        graph.connect ({ node, 0 }, { node, 0 });
        processor.setFxPatchGraph (graph);
        auto delayed = processor.getFxPatchGraph();
        const auto loop = delayed.findCable ({ node, 0 }, { node, 0 });
        delayed.setCableGain (loop, 0.5f);
        delayed.setCableDelay (loop, 100.0f);
        processor.setFxPatchGraph (delayed);
        processor.compileFxPlansNow();
        processor.prepareToPlay (48000.0, 256);
        // One click in, then where its first echo lands.
        juce::AudioBuffer<float> buffer (2, 256);
        auto echoAt = -1;
        for (int block = 0; block < 40 && echoAt < 0; ++block)
        {
            buffer.clear();
            if (block == 0)
                buffer.setSample (0, 0, 0.5f);
            processor.processEffects (buffer);
            for (int i = 0; i < 256 && echoAt < 0; ++i)
                if ((block > 0 || i > 0) && std::abs (buffer.getSample (0, i)) > 0.05f)
                    echoAt = block * 256 + i;
        }
        check (echoAt == 4800, "a loop cable set to 100 ms echoes 4800 samples later at 48 kHz (" + juce::String (echoAt) + ")");

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        IlanaSynthAudioProcessor loaded;
        loaded.setStateInformation (saved.getData(), (int) saved.getSize());
        const auto& cables = loaded.getFxPatch().graph.cables();
        const auto back = loaded.getFxPatch().graph.findCable ({ node, 0 }, { node, 0 });
        check (back >= 0 && cables[(size_t) back].delayMs == 100.0f && cables[(size_t) back].gain == 0.5f, "a loop's delay and level save and load");
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
