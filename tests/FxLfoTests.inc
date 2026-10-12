// The FX patcher's LFO boxes (FxLfo.h, ilana 2026-10-12). Included by
// TableTest.cpp after FxPatcherTests.inc (it uses its helpers);
// ILANA_FX_LFO_TEST=1 runs only these.

namespace FxLfoLab
{
using namespace FxPatcherLab;

// An LFO box on slot `slot` moving every knob it may, at `amount`.
inline int lfoOnEveryKnob (IlanaSynthAudioProcessor& processor, int slot, float amount, const FxLfo::Settings& settings)
{
    const auto id = processor.addFxLfo (0.0f, -100.0f);
    processor.setFxLfoSettings (id, settings);
    processor.setFxLfoCable (id, slot, true);
    auto k = 0;
    for (const auto& [param, name] : processor.getFxLfoTargets (slot))
        processor.assignFxLfo (id, slot, param, true, (k++ % 2 == 0) ? amount : -amount);
    return id;
}

inline FxLfo::Settings fastSine()
{
    FxLfo::Settings s;
    s.shape = 0;
    s.rateHz = FxLfo::maxRateHz;
    s.depth = 1.0f;
    return s;
}
} // namespace FxLfoLab

void runFxLfoTests()
{
    using namespace FxLfoLab;

    // A patch without LFO boxes saves exactly as before (no new children),
    // and boxes, cables and knobs survive a save and load.
    {
        FxPatchData::Patch empty;
        check (empty.isDefault() && FxPatchData::toValueTree (empty).getNumChildren() == 0, "a patch with no LFO box is still the default patch");

        FxPatchData::Patch patch;
        FxPatchData::Lfo lfo;
        lfo.id = 3;
        lfo.x = 120.0f;
        lfo.y = -80.0f;
        lfo.settings.shape = 5;
        lfo.settings.sync = true;
        lfo.settings.division = 1;
        lfo.settings.depth = 0.6f;
        lfo.settings.bipolar = false;
        lfo.settings.phase = 0.25f;
        patch.lfos.push_back (lfo);
        patch.lfoCables.push_back ({ 3, 4 });
        patch.lfoRoutes.push_back ({ 3, 4, "fx_delay_time", -0.4f });
        patch.lfoRoutes.push_back ({ 3, 4, FxPatchData::mixParam, 0.3f });
        const auto loaded = FxPatchData::fromValueTree (juce::ValueTree::fromXml (FxPatchData::toValueTree (patch).toXmlString()));
        check (! patch.isDefault() && FxPatchData::samePatch (patch, loaded), "LFO boxes, cables and knobs survive a save and load");

        // Damaged: out-of-range settings are clamped, a route with no cable or box is dropped.
        auto tree = FxPatchData::toValueTree (patch);
        tree.getChildWithName (FxPatchData::lfoTag).setProperty ("rate", "nan", nullptr);
        tree.getChildWithName (FxPatchData::lfoTag).setProperty ("depth", 7.0f, nullptr);
        juce::ValueTree stray (FxPatchData::lfoRouteTag);
        stray.setProperty ("lfo", 5, nullptr);
        stray.setProperty ("slot", 2, nullptr);
        stray.setProperty ("param", "fx_drive_amount", nullptr);
        tree.appendChild (stray, nullptr);
        const auto damaged = FxPatchData::fromValueTree (tree);
        check (damaged.lfos.size() == 1 && std::isfinite (damaged.lfos[0].settings.rateHz) && damaged.lfos[0].settings.depth == 1.0f
                   && damaged.lfoRoutes.size() == 2,
               "a damaged LFO box loads in range and a stray route is dropped");
    }

    // Shapes stay in -1..1, and a render is the same every time (own random seed).
    {
        juce::StringArray wrong;
        for (int choice = 0; choice < (int) FxLfo::shapes.size(); ++choice)
        {
            FxLfo::State state;
            state.reset (1);
            FxLfo::Settings settings;
            settings.shape = choice;
            settings.rateHz = 7.0f;
            for (int step = 0; step < 4000; ++step)
            {
                const auto value = FxLfo::output (settings, state.shapeValue (FxLfo::shapeOf (choice)));
                if (! std::isfinite (value) || std::abs (value) > 1.0f)
                {
                    wrong.add (FxLfo::shapeNames()[choice]);
                    break;
                }
                state.advance (settings, 7.0 / 48000.0, 128, -1.0);
            }
        }
        check (wrong.isEmpty(), "every LFO shape stays in -1..1 (" + wrong.joinIntoString (", ") + ")");
    }

    // Two Delays: an LFO cable on the second moves only the second; and
    // taking the knob off leaves its offset at 0.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { delay }, false);
        delaySettings (processor, 200.0f);
        processor.addFxEffect (delay);
        const auto id = processor.addFxLfo (0.0f, 0.0f);
        check (id == 1, "the first LFO box is LFO 1");
        processor.setFxLfoSettings (id, fastSine());
        processor.setFxLfoCable (id, 1, true);
        check (processor.getFxLfoTargets (1).size() > 3, "an LFO offers the Delay's continuous knobs and MIX");
        processor.assignFxLfo (id, 1, "fx_delay_time", true, 0.5f);
        processor.prepareToPlay (48000.0, 256);
        auto movedSecond = 0.0f, movedFirst = 0.0f;
        juce::AudioBuffer<float> buffer (2, 256);
        for (int block = 0; block < 20; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (c, i, ParallelFxLab::input (c, block * 256 + i));
            processor.processEffects (buffer);
            movedSecond = juce::jmax (movedSecond, std::abs (processor.getFxLfoOffset (1, "fx_delay_time")));
            movedFirst = juce::jmax (movedFirst, std::abs (processor.getFxLfoOffset (0, "fx_delay_time")));
        }
        check (movedSecond > 0.2f && movedFirst == 0.0f, "an LFO cable moves the Delay it goes to and not the other one ("
                                                             + juce::String (movedSecond, 3) + ", " + juce::String (movedFirst, 3) + ")");

        processor.setFxLfoCable (id, 1, false);
        check (processor.getFxPatch().lfoRoutes.empty(), "taking the cable out drops the knobs it moved");
        processor.processEffects (buffer);
        check (processor.getFxLfoOffset (1, "fx_delay_time") == 0.0f, "with the cable gone the Delay reads its knob as set (no offset)");

        // A slot swap carries the LFO's cable and knobs with the effect.
        processor.assignFxLfo (id, 1, "fx_delay_time", true, 0.5f);
        processor.swapFxSlots (0, 1);
        const auto swapped = processor.getFxPatch();
        check (swapped.hasLfoCable (id, 0) && swapped.lfoRoutes.size() == 1 && swapped.lfoRoutes[0].slot == 0,
               "a slot swap moves the LFO's cable and knobs with the effect");

        // Removing the effect drops its LFO cables; removing the box drops the rest.
        processor.removeFxEffect (0);
        check (processor.getFxPatch().lfoCables.empty() && processor.getFxPatch().lfoRoutes.empty(),
               "removing an effect drops the LFO cables into it");
        processor.removeFxLfo (id);
        check (processor.getFxPatch().lfos.empty(), "an LFO box can be removed");
    }

    // An LFO on a stateless effect: once its cable is gone the effect renders
    // exactly as one that never had it.
    {
        const auto plain = rendered ({ drive }, false);
        IlanaSynthAudioProcessor processor;
        setUp (processor, { drive }, false);
        const auto id = lfoOnEveryKnob (processor, 0, 1.0f, fastSine());
        const auto moved = run (processor);
        check (worstDifference (plain, moved) > 1.0e-3f, "an LFO on Drive changes what it sounds like");
        processor.setFxLfoCable (id, 0, false);
        const auto after = run (processor);
        check (worstDifference (plain, after) < 1.0e-6f, "after the LFO cable is out, Drive renders as if it never had one (worst "
                                                              + juce::String (worstDifference (plain, after), 8) + ")");
    }

    // Every effect type, an LFO at 20 Hz and full depth on every knob it
    // offers (alternately up and down), Sine, Square, Sample & Hold and
    // Chaos: alone it stays finite and bounded; fed back into itself at the
    // loop's 70 % top with the test tones twice as loud it stays at or under
    // 0 dB; and after the LFO and loop cables are out it plays again.
    {
        juce::StringArray runaway;
        auto worstAlone = 0.0f;
        for (int type = 1; type <= IlanaSynthAudioProcessor::numFxTypes; ++type)
            for (const auto shape : { 0, 4, 5, 8 })
            {
                IlanaSynthAudioProcessor processor;
                setUp (processor, { type }, false);
                for (auto* parameter : processor.getParameters())
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                        if (withId->paramID.startsWith ("fx_") && withId->paramID.endsWith ("_on"))
                            set (processor, withId->paramID, 1.0f);
                auto settings = fastSine();
                settings.shape = shape;
                const auto id = lfoOnEveryKnob (processor, 0, 1.0f, settings);
                processor.prepareToPlay (48000.0, 256);
                const auto alone = runPeak (processor, 120, 60, 1.0f);
                worstAlone = std::isfinite (alone) ? juce::jmax (worstAlone, alone) : 1.0e9f;

                processor.setFxPatcherOn (true);
                auto graph = processor.getFxPatchGraph();
                const auto node = nodeOfSlot (graph, 0);
                graph.connect ({ node, 0 }, { node, 0 });
                processor.setFxPatchGraph (graph);
                auto hot = processor.getFxPatchGraph();
                hot.setCableGain (hot.findCable ({ node, 0 }, { node, 0 }), 0.7f);
                processor.setFxPatchGraph (hot);
                processor.compileFxPlansNow();
                const auto looped = runPeak (processor, 300, 40, 2.0f);

                processor.setFxLfoCable (id, 0, false);
                auto unlooped = processor.getFxPatchGraph();
                unlooped.disconnect ({ node, 0 }, { node, 0 });
                processor.setFxPatchGraph (unlooped);
                processor.compileFxPlansNow();
                const auto again = runPeak (processor, 80, 40, 1.0f, 20);
                if (! (alone < 64.0f) || ! (looped <= 1.0f) || ! std::isfinite (again) || again > 1.0e3f)
                    runaway.add ("type " + juce::String (type) + " " + FxLfo::shapeNames()[shape] + " (" + juce::String (alone, 3) + ", "
                                 + juce::String (looped, 3) + ", then " + juce::String (again, 3) + ")");
            }
        check (runaway.isEmpty(), "an LFO on every knob of every effect type stays bounded, alone and in a hot loop, and the effect plays "
                                  "again after (" + runaway.joinIntoString (", ") + "; loudest alone " + juce::String (worstAlone, 3) + ")");
    }

    // A slot whose effect changes keeps only knobs the new effect has.
    {
        IlanaSynthAudioProcessor processor;
        setUp (processor, { delay }, false);
        const auto id = processor.addFxLfo (0.0f, 0.0f);
        processor.assignFxLfo (id, 0, "fx_delay_feedback", true, 0.5f);
        processor.assignFxLfo (id, 0, FxPatchData::mixParam, true, 0.5f);
        processor.assignFxSlot (1, reverb);
        processor.compileFxPlansNow();
        const auto patch = processor.getFxPatch();
        check (patch.lfoRoutes.size() == 1 && patch.lfoRoutes[0].param == FxPatchData::mixParam,
               "a slot that changes effect keeps only its LFO's MIX knob");
    }
}
