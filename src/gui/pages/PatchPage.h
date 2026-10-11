// The PATCH tab: roadmap steps 16 and 17 as one view (ilana, 2026-10-11:
// "unify reroutable oscillators and patcher"). One canvas runs from the
// oscillators and SUB + NOISE through Filter 1, Filter 2 and WEST (per note,
// on a band of their own) to VOICES, where every note sums, then through the
// rack's effects to OUT. Under the hood it is still two patches, so each part
// costs what it did: the voice's cables (route_cables and cable_*,
// dsp/VoiceRouting.h) and the FX patch (FxPatch.h). The page merges them into
// one graph for the cable view (PatchCableView.h) and splits each edit back.
// Included only from PluginEditor.cpp, after FxPage.h and the FILTER page.
#pragma once

namespace
{
class PatchPage : public juce::Component,
                  private juce::Timer
{
public:
    // The voice's boxes in the merged graph: id voiceId + kind, node kind
    // voiceKind + kind, where kind is 0-5 for OSC 1-6, 6 for SUB + NOISE and
    // stageKind + VoiceRouting::Stage for the stages. The effects keep their
    // FX patch ids and types; the FX patch's INPUT is VOICES.
    static constexpr PatchGraph::NodeId voiceId = 1u << 20;
    static constexpr int voiceKind = 1000;
    static constexpr int stageKind = 10;

    PatchPage (IlanaSynthAudioProcessor& p, FxPage& rackPage)
        : processorRef (p), rack (rackPage), view (shown), hasVoice (! IlanaSynthAudioProcessor::isEffectBuild)
    {
        // VOICE: chain or cables; FX: the RACK page's three-way switch.
        chainButton.setTooltip ("Chain\nThe fixed chain: each oscillator's route menu, SERIAL / PARALLEL and WEST's position decide.");
        cablesButton.setTooltip ("Cables\nThe cables below decide where each oscillator goes and the order of the filters and WEST.");
        chainButton.setDescription ("Oscillator routing: chain");
        cablesButton.setDescription ("Oscillator routing: cables");
        chainButton.onClick = [this] { setVoiceCables (false); };
        cablesButton.onClick = [this] { setVoiceCables (true); };
        seriesButton.setTooltip ("Series\nEach effect feeds the next one, in the rack's order.");
        parallelButton.setTooltip ("Parallel\nEvery effect hears VOICES; their outputs are averaged.");
        patcherButton.setTooltip ("Patcher\nThe cables after VOICES decide what feeds what.");
        seriesButton.setDescription ("FX routing: series");
        parallelButton.setDescription ("FX routing: parallel");
        patcherButton.setDescription ("FX routing: patcher");
        seriesButton.onClick = [this] { setFxRouting (0); };
        parallelButton.onClick = [this] { setFxRouting (1); };
        patcherButton.onClick = [this] { setFxRouting (2); };
        for (auto* button : { &chainButton, &cablesButton })
        {
            IlanaTheme::makePill (*button, IlanaTheme::accent());
            button->setRadioGroupId (0x5e71a3);
            if (hasVoice)
                addAndMakeVisible (*button);
        }
        for (auto* button : { &seriesButton, &parallelButton, &patcherButton })
        {
            IlanaTheme::makePill (*button, IlanaTheme::accent());
            button->setRadioGroupId (0x5e71a2);
            addAndMakeVisible (*button);
        }

        addButton.setTooltip ("Add an effect: it is wired in just before OUT. One the rack has already is a new instance with its own settings.");
        addButton.onClick = [this] { rack.openLibrary (addButton); };
        resetButton.setTooltip ("Voice cables back to the fixed chain's routing (each oscillator's route, SERIAL / PARALLEL, WEST's position)");
        resetButton.onClick = [this] { resetVoiceToChain(); };
        fitButton.setTooltip ("Fit the patch to the view");
        fitButton.onClick = [this] { fit(); };
        for (auto* quiet : { &resetButton, &fitButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        }
        addAndMakeVisible (addButton);
        addAndMakeVisible (fitButton);
        if (hasVoice)
            addAndMakeVisible (resetButton);

        view.setName ("PATCH"); // (the UI tests find it by name)
        view.colourOf = [] (const PatchGraph::Node& node) { return colourOf (node); };
        view.cpuOf = [this] (PatchGraph::NodeId id) -> float
        {
            const auto slot = slotOf (shown.find (id));
            return slot >= 0 ? processorRef.getFxSlotCpu (slot) : -1.0f;
        };
        view.statusOf = [this] (const PatchGraph::Node& node) { return statusOf (node); };
        view.removable = [] (const PatchGraph::Node& node) { return ! isVoice (node); };
        view.movable = [] (const PatchGraph::Node& node) { return ! isVoice (node) && ! node.isInput(); };
        view.hasLevel = [this] (const PatchGraph::Cable& cable)
        {
            const auto* from = shown.find (cable.from.node);
            return from != nullptr && ! isVoice (*from);
        };
        view.acceptGraph = [this] (const PatchGraph::Graph& graph) { return accept (graph); };
        view.onRefused = [this]
        {
            refusedUntil = juce::Time::getMillisecondCounter() + 3000;
            repaint (hint);
        };
        view.onEdited = [this] (PatchCableView::Edit edit) { commit (edit); };
        view.onBypass = [this] (const PatchGraph::Node& node) { toggleBypass (slotOf (&node)); };
        view.onRemove = [this] (const PatchGraph::Node& node) { removeEffect (slotOf (&node)); };
        view.onOpen = [this] (const PatchGraph::Node& node) { open (node); };
        view.nodeMenu = [this] (juce::PopupMenu& menu, const PatchGraph::Node& node) { addNodeItems (menu, node); };
        view.addMenu = [this] (juce::PopupMenu& menu, juce::Point<float> at) { addEffectItems (menu, at); };
        view.paintBackdrop = [this] (juce::Graphics& g) { paintVoiceBand (g); };
        addAndMakeVisible (view);

        // The selected box's own row. A voice box: its ON switch and where to
        // edit it. An effect: its switch, solo, band and MIX, and what to do
        // with it.
        IlanaTheme::makePill (onButton, IlanaTheme::accent());
        onButton.setClickingTogglesState (true);
        soloButton.setClickingTogglesState (true);
        soloButton.setTooltip ("SOLO: hear only this effect's output (wet only)");
        IlanaTheme::makePill (soloButton, IlanaTheme::accent());
        band.addItemList ({ "Full Band", "Low Band", "Mid Band", "High Band", "M/S Mid", "M/S Side" }, 1);
        band.setTooltip ("Band: the part of the signal this effect works on; the rest passes around it.");
        editButton.onClick = [this] { openSelected(); };
        duplicateButton.setTooltip ("A new instance of this effect with a copy of its settings, wired in before OUT");
        removeButton.setTooltip ("Take this effect out (the cables around it are joined up)");
        duplicateButton.onClick = [this] { duplicateEffect (inspectedSlot); };
        removeButton.onClick = [this] { removeEffect (inspectedSlot); };
        IlanaTheme::makePill (editButton, IlanaTheme::accent());
        for (auto* quiet : { &duplicateButton, &removeButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        }
        for (juce::Component* item : inspectorItems())
            addChildComponent (item);

        refresh (true);
        startTimerHz (15);
    }

    // Set by the editor: show a page ("OSC", "FILTER"), or an effect's card
    // on the RACK page.
    std::function<void (const juce::String&)> onShowPage;
    std::function<void (int)> onOpenSlot;

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        paintPageBackground (g, getLocalBounds());
        const auto caption = [&g] (const char* name, juce::Rectangle<int> area)
        {
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
            g.drawText (name, area, juce::Justification::centredLeft, false);
        };
        if (hasVoice)
        {
            caption ("VOICE", voiceCaption);
            paintWell (g, voiceSeg.toFloat(), (float) voiceSeg.getHeight() * 0.5f);
        }
        caption ("FX", fxCaption);
        paintWell (g, fxSeg.toFloat(), (float) fxSeg.getHeight() * 0.5f);

        const auto refused = juce::Time::getMillisecondCounter() < refusedUntil;
        g.setColour (refused ? Ui::accent2 : Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (refused ? refusal : hintText(), hint, juce::Justification::centredLeft, true);
        paintInspector (g);
    }

    void resized() override
    {
        // The FX RACK page's frame: 8 px above and below, a 12 px gutter at the right.
        auto area = getLocalBounds().reduced (0, 8).withTrimmedRight (12);
        auto toolbar = area.removeFromTop (toolbarHeight);
        if (hasVoice)
        {
            toolbar.removeFromLeft (10);
            voiceCaption = toolbar.removeFromLeft (44);
            voiceSeg = toolbar.removeFromLeft (152);
            chainButton.setBounds (voiceSeg.withTrimmedLeft (3).removeFromLeft (70).reduced (0, 2));
            cablesButton.setBounds (voiceSeg.withTrimmedLeft (3 + 72).removeFromLeft (74).reduced (0, 2));
            toolbar.removeFromLeft (20);
        }
        else
        {
            toolbar.removeFromLeft (10);
        }
        fxCaption = toolbar.removeFromLeft (28);
        fxSeg = toolbar.removeFromLeft (232);
        seriesButton.setBounds (fxSeg.withTrimmedLeft (3).removeFromLeft (66).reduced (0, 2));
        parallelButton.setBounds (fxSeg.withTrimmedLeft (3 + 68).removeFromLeft (78).reduced (0, 2));
        patcherButton.setBounds (fxSeg.withTrimmedLeft (3 + 68 + 80).removeFromLeft (78).reduced (0, 2));
        toolbar.removeFromLeft (20);
        fitButton.setBounds (toolbar.removeFromRight (70).reduced (0, 2));
        toolbar.removeFromRight (6);
        if (hasVoice)
        {
            resetButton.setBounds (toolbar.removeFromRight (80).reduced (0, 2));
            toolbar.removeFromRight (6);
        }
        addButton.setBounds (toolbar.removeFromRight (112).reduced (0, 2));
        hint = toolbar.withTrimmedRight (10);
        area.removeFromTop (10);
        inspector = area.removeFromBottom (inspectorHeight);
        area.removeFromBottom (10);
        view.setBounds (area);
        fit();
        layoutInspector();
    }

    // Tests: the graph the view shows, and the voice's cables as a patch.
    const PatchGraph::Graph& getShownGraph() const { return shown; }
    PatchCableView& getView() { return view; }

    static bool isVoice (const PatchGraph::Node& node) { return node.id >= voiceId; }

    // The voice's patch a merged graph's cables make (a cable into VOICES
    // is VoiceRouting's OUT).
    static VoiceRouting::Patch voicePatchOf (const PatchGraph::Graph& graph)
    {
        VoiceRouting::Patch patch;
        for (const auto& cable : graph.cables())
        {
            const auto* from = graph.find (cable.from.node);
            const auto* to = graph.find (cable.to.node);
            if (from == nullptr || to == nullptr || ! isVoice (*from))
                continue;
            int destination = -1;
            if (to->isInput())
                destination = VoiceRouting::out;
            else if (isVoice (*to) && localKind (*to) >= stageKind)
                destination = localKind (*to) - stageKind;
            if (destination < 0)
                continue;
            const auto bit = VoiceRouting::bitOf (destination);
            const auto kind = localKind (*from);
            if (juce::isPositiveAndBelow (kind, VoiceRouting::numSources))
                patch.source[(size_t) kind] = (std::uint8_t) (patch.source[(size_t) kind] | bit);
            else if (kind >= stageKind && kind < stageKind + VoiceRouting::numStages)
                patch.stage[(size_t) (kind - stageKind)] = (std::uint8_t) (patch.stage[(size_t) (kind - stageKind)] | bit);
        }
        return patch;
    }

private:
    static constexpr int toolbarHeight = 28;
    static constexpr int inspectorHeight = 70;
    static constexpr float maxZoom = 1.3f, minZoom = 0.6f;
    // The 10 px grid: voice columns 180 apart (a 130 px box and room for the
    // cables, the rack's step), rows 70 apart (a 50 px box and a 20 px gap).
    static constexpr float columnStep = 180.0f, rowStep = 70.0f;

    static int localKind (const PatchGraph::Node& node) { return node.kind - voiceKind; }

    int slotOf (const PatchGraph::Node* node) const
    {
        if (node == nullptr || node->isFixed() || isVoice (*node) || ! juce::isPositiveAndBelow (node->ref - 1, IlanaSynthAudioProcessor::numFxSlots))
            return -1;
        return node->ref - 1;
    }

    //==========================================================================
    // Names, colours and the boxes' second lines.
    static juce::Colour colourOf (const PatchGraph::Node& node)
    {
        if (! isVoice (node))
            return fxColour (node.kind);
        const auto kind = localKind (node);
        if (juce::isPositiveAndBelow (kind, VoiceRouting::numOscSources))
            return IlanaTheme::oscColour (kind);
        if (kind == stageKind + VoiceRouting::f1)
            return FilterColours::filter (0);
        if (kind == stageKind + VoiceRouting::f2)
            return FilterColours::filter (1);
        if (kind == stageKind + VoiceRouting::west)
            return FilterColours::west();
        return IlanaTheme::Ui::text2; // SUB + NOISE, as SIGNAL FLOW draws it
    }

    static juce::String voiceName (int kind)
    {
        if (juce::isPositiveAndBelow (kind, VoiceRouting::numOscSources))
            return "OSC " + juce::String (kind + 1);
        if (kind == VoiceRouting::subNoiseSource)
            return "SUB + NOISE";
        if (kind == stageKind + VoiceRouting::f1)
            return "FILTER 1";
        if (kind == stageKind + VoiceRouting::f2)
            return "FILTER 2";
        return "WEST";
    }

    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    static juce::String onParamOf (int kind)
    {
        if (juce::isPositiveAndBelow (kind, VoiceRouting::numOscSources))
            return juce::String (OscillatorIds::prefixes[(size_t) kind]) + "_on";
        if (kind == stageKind + VoiceRouting::west)
            return "west_on";
        return {};
    }

    bool sourceOn (int kind) const
    {
        if (kind == VoiceRouting::subNoiseSource)
            return read ("subosc_on") > 0.5f || (read ("noise_on") > 0.5f && read ("noise_level") > 0.001f);
        return read (onParamOf (kind)) > 0.5f;
    }

    juce::String statusOf (const PatchGraph::Node& node) const
    {
        if (! isVoice (node))
        {
            if (node.bypassed)
                return "BYPASSED";
            const auto slot = slotOf (&node);
            if (slot < 0)
                return {};
            const auto cpu = processorRef.getFxSlotCpu (slot);
            return juce::String (cpu * 100.0f, cpu < 0.001f ? 2 : 1) + " % CPU";
        }
        const auto kind = localKind (node);
        if (kind >= stageKind)
        {
            const auto stage = kind - stageKind;
            if (! table.used[(size_t) stage])
                return "NOT IN USE";
            if (stage == VoiceRouting::west)
                return read ("west_on") > 0.5f ? "FOLD + GATE" : "OFF: PASSES THROUGH";
            return FilterTypes::displayName (juce::roundToInt (read (stage == VoiceRouting::f1 ? "f1_type" : "f2_type"))).toUpperCase();
        }
        if (! sourceOn (kind))
            return "OFF";
        if (kind < VoiceRouting::numOscSources && read (juce::String (OscillatorIds::prefixes[(size_t) kind]) + "_out") < 0.5f)
            return "FM ONLY";
        if (table.busOf[(size_t) kind] == VoiceRouting::mutedBus)
            return "NOT HEARD";
        if (kind == VoiceRouting::subNoiseSource)
            return "SUB, NOISE";
        if (auto* mode = processorRef.apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) kind]) + "_mode"))
            return mode->getCurrentValueAsText().toUpperCase();
        return {};
    }

    juce::String hintText() const
    {
        if (hasVoice && ! processorRef.areRouteCablesOn() && ! processorRef.isFxPatcherOn())
            return "The chain and rack as cables. Change one to play the cables.";
        const auto loops = processorRef.getFxLoopLatencyMs();
        if (loops > 0.0f)
            return "Drag an output to an input. Each loop adds " + juce::String (loops, 1) + " ms.";
        return "Drag an output to an input. More cables play in parallel.";
    }

    // "WEST > FILTER 1 > FILTER 2", the voice's stages in the order they run.
    juce::String voiceOrderText() const
    {
        juce::StringArray names;
        for (int k = 0; k < table.numOrdered; ++k)
            names.add (voiceName (stageKind + table.order[(size_t) k]));
        return names.joinIntoString ("  >  ");
    }

    juce::String fxSummaryText() const
    {
        auto effects = 0, merges = 0;
        for (const auto& node : fxShown.nodes())
        {
            if (! node.isFixed())
                ++effects;
            auto inputs = 0;
            for (const auto& cable : fxShown.cables())
                inputs += cable.to.node == node.id ? 1 : 0;
            merges += inputs > 1 ? 1 : 0;
        }
        const auto loops = fxShown.feedbackCables();
        const auto loopCount = (int) std::count (loops.begin(), loops.end(), true);
        juce::String text;
        text << effects << (effects == 1 ? " effect" : " effects");
        if (merges > 0)
            text << ", " << merges << (merges == 1 ? " mix point" : " mix points");
        if (loopCount > 0)
            text << ", " << loopCount << (loopCount == 1 ? " loop" : " loops");
        return text;
    }

    // "Goes to FILTER 1 and VOICES", from the shown cables.
    juce::String destinationText (const PatchGraph::Node& node) const
    {
        juce::StringArray to;
        for (const auto& cable : shown.cables())
            if (cable.from.node == node.id)
                if (const auto* target = shown.find (cable.to.node))
                    to.add (target->isInput() ? juce::String ("VOICES") : voiceName (localKind (*target)));
        if (to.isEmpty())
            return "No cable: not heard";
        const auto last = to[to.size() - 1];
        to.remove (to.size() - 1);
        return "Goes to " + (to.isEmpty() ? last : to.joinIntoString (", ") + " and " + last)
               + (to.isEmpty() ? juce::String() : juce::String (", in parallel"));
    }

    //==========================================================================
    // The voice's band: behind every box that runs once per note, up to VOICES.
    void paintVoiceBand (juce::Graphics& g) const
    {
        using namespace IlanaTheme;
        if (! hasVoice || voiceArea.isEmpty())
            return;
        const auto band = voiceArea.translated (view.getOrigin().x, view.getOrigin().y);
        g.setColour (Ui::line2.withAlpha (0.35f));
        g.fillRoundedRectangle (band, 12.0f);
        g.setColour (Ui::line);
        g.drawRoundedRectangle (band.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
        g.drawText ("EACH NOTE", band.reduced (16.0f, 10.0f).removeFromTop (14.0f), juce::Justification::centredLeft, false);
        // The rest, after VOICES, hears every note summed.
        g.drawText ("ALL NOTES", juce::Rectangle<float> (band.getRight() + 16.0f, band.getY() + 10.0f, 120.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    void paintInspector (juce::Graphics& g)
    {
        using namespace IlanaTheme;
        if (inspector.isEmpty())
            return;
        paintCard (g, inspector.toFloat(), 8.0f, Ui::line);
        auto text = inspectorText;
        const auto* node = shown.find (view.getSelectedNode());
        const auto label = [&g] (const char* name, juce::Rectangle<int> area)
        {
            if (area.isEmpty())
                return;
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
            g.drawText (name, juce::Rectangle<int> (area.getX(), area.getY() - 16, 60, 14), juce::Justification::centredLeft, false);
        };

        if (node == nullptr || node->isFixed())
        {
            // Nothing selected: the patch in one line, and how to start.
            juce::String line;
            if (hasVoice)
            {
                line << voiceOrderText();
                line << (line.isEmpty() ? "" : "  >  ") << "VOICES  >  ";
            }
            line << fxSummaryText() << "  >  OUT";
            g.setColour (Ui::text);
            g.setFont (font (TextSize::body, true));
            g.drawText (line, text.removeFromTop (text.getHeight() / 2), juce::Justification::bottomLeft, true);
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::label));
            g.drawText ("Click a box to set it here; double-click it to open it. Right-click the canvas to add an effect where you clicked.",
                        text.withTrimmedTop (4), juce::Justification::topLeft, true);

            // At the right: how the patch is wired and what its effects cost.
            auto stats = inspector.reduced (20, 10).removeFromRight (450);
            auto cpu = 0.0f;
            for (const auto& box : fxShown.nodes())
                if (! box.isFixed() && juce::isPositiveAndBelow (box.ref - 1, IlanaSynthAudioProcessor::numFxSlots))
                    cpu += processorRef.getFxSlotCpu (box.ref - 1);
            const auto loopMs = processorRef.getFxLoopLatencyMs();
            std::vector<std::pair<juce::String, juce::String>> cells {
                { "EFFECTS CPU", juce::String (cpu * 100.0f, 2) + " %" },
                { "LOOP DELAY", loopMs > 0.0f ? juce::String (loopMs, 1) + " ms" : juce::String ("none") }
            };
            if (hasVoice)
                cells.push_back ({ "SPLIT SOURCES", juce::String (splitSources()) });
            for (const auto& [name, value] : cells)
            {
                auto cell = stats.removeFromRight (140);
                g.setColour (Ui::text3);
                g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
                g.drawText (name, cell.removeFromTop (cell.getHeight() / 2), juce::Justification::bottomLeft, false);
                g.setColour (Ui::text);
                g.setFont (font (TextSize::body, true, true));
                g.drawText (value, cell.withTrimmedTop (4), juce::Justification::topLeft, false);
                stats.removeFromRight (10);
            }
            return;
        }

        auto title = text.removeFromTop (text.getHeight() / 2);
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colourOf (*node));
        g.setColour (Ui::text);
        g.setFont (font (TextSize::large, true));
        g.drawText (isVoice (*node) ? voiceName (localKind (*node)) : juce::String (node->label), title.withTrimmedLeft (16),
                    juce::Justification::bottomLeft, true);
        juce::String detail;
        if (isVoice (*node))
        {
            detail = destinationText (*node);
        }
        else if (const auto slot = slotOf (node); slot >= 0)
        {
            detail = "Slot " + juce::String (slot + 1);
            if (processorRef.isFxInstance (slot) || ! processorRef.isFxSlotInFront (slot))
                detail << (processorRef.isFxSlotInFront (slot) ? ", own settings, open on its RACK card" : ", own settings");
            detail << ", " << juce::String (processorRef.getFxSlotCpu (slot) * 100.0f, 2) << " % CPU";
        }
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (detail, text.withTrimmedTop (4).withTrimmedLeft (16), juce::Justification::topLeft, true);
        label ("ON", onButton.isVisible() ? onArea : powerArea);
        label ("SOLO", soloArea);
        label ("BAND", bandArea);
    }

    int splitSources() const
    {
        auto parallel = 0;
        for (int source = 0; source < VoiceRouting::numSources; ++source)
            if (sourceOn (source) && table.busOf[(size_t) source] != VoiceRouting::mutedBus)
            {
                auto destinations = 0;
                for (int stage = 0; stage <= VoiceRouting::numStages; ++stage)
                    destinations += (table.busMask[(size_t) table.busOf[(size_t) source]] & VoiceRouting::bitOf (stage)) != 0 ? 1 : 0;
                parallel += destinations > 1 ? 1 : 0;
            }
        return parallel;
    }

    std::vector<juce::Component*> inspectorItems()
    {
        return { &onButton, &soloButton, &band, &editButton, &duplicateButton, &removeButton };
    }

    // The selected box's row on the 10 px grid: name and detail, the
    // controls (one label line above one control line), the actions at the
    // right.
    void layoutInspector()
    {
        auto row = inspector.reduced (20, 10);
        inspectorText = row.removeFromLeft (juce::jmin (inspectedVoice >= 0 ? 420 : 300, row.getWidth() / 3));
        onArea = powerArea = soloArea = bandArea = {};
        if (inspectedVoice < 0 && inspectedSlot < 0)
        {
            inspectorText = inspector.reduced (20, 10);
            return;
        }
        const auto centreY = row.getY() + 32;
        if (inspectedVoice >= 0)
        {
            auto actions = row.removeFromRight (160);
            editButton.setBounds (actions.withSizeKeepingCentre (160, 28));
            if (onButton.isVisible())
            {
                row.removeFromLeft (20);
                onArea = juce::Rectangle<int> (row.getX(), centreY - 13, 50, 26);
                onButton.setBounds (onArea);
            }
            return;
        }
        auto actions = row.removeFromRight (340);
        removeButton.setBounds (actions.removeFromRight (90).withSizeKeepingCentre (90, 28));
        actions.removeFromRight (10);
        duplicateButton.setBounds (actions.removeFromRight (100).withSizeKeepingCentre (100, 28));
        actions.removeFromRight (10);
        editButton.setBounds (actions.removeFromRight (130).withSizeKeepingCentre (130, 28));
        row.removeFromLeft (20);
        const auto controlRow = [&] (int width) { auto cell = row.removeFromLeft (width); row.removeFromLeft (20); return cell; };
        powerArea = controlRow (40);
        powerArea = juce::Rectangle<int> (powerArea.getX(), centreY - 10, 40, 20);
        soloArea = controlRow (30);
        soloArea = juce::Rectangle<int> (soloArea.getX(), centreY - 13, 30, 26);
        bandArea = controlRow (130);
        bandArea = juce::Rectangle<int> (bandArea.getX(), centreY - 13, 130, 26);
        if (power != nullptr)
            power->setBounds (powerArea);
        soloButton.setBounds (soloArea);
        band.setBounds (bandArea);
        if (mixKnob != nullptr)
            mixKnob->setBounds (controlRow (120).withSizeKeepingCentre (116, 44));
    }

    // The inspector follows the selected box.
    void inspect (const PatchGraph::Node* node)
    {
        const auto voice = node != nullptr && isVoice (*node) ? localKind (*node) : -1;
        const auto slot = slotOf (node);
        if (voice == inspectedVoice && slot == inspectedSlot)
            return;
        inspectedVoice = voice;
        inspectedSlot = slot;
        onAttachment.reset();
        power.reset();
        mixKnob.reset();
        soloAttachment.reset();
        bandAttachment.reset();
        for (auto* item : inspectorItems())
            item->setVisible (false);

        if (voice >= 0)
        {
            if (const auto onId = onParamOf (voice); onId.isNotEmpty())
                if (auto* parameter = processorRef.apvts.getParameter (onId))
                    onAttachment = std::make_unique<juce::ButtonParameterAttachment> (*parameter, onButton, nullptr);
            onButton.setVisible (onAttachment != nullptr);
            editButton.setVisible (true);
            editButton.setButtonText (voice >= stageKind ? "EDIT ON FILTER" : "EDIT ON OSC");
            editButton.setTooltip (voice >= stageKind ? "Open the FILTER page" : "Open the OSC page");
        }
        else if (slot >= 0)
        {
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            power = std::make_unique<SlotSwitch> (processorRef, slot);
            power->setBare (true);
            addAndMakeVisible (*power);
            mixKnob = std::make_unique<KnobControl> (processorRef.apvts, prefix + "_mix", "MIX", IlanaTheme::accent(), false);
            mixKnob->setInlineKnob (true);
            addAndMakeVisible (*mixKnob);
            if (auto* solo = processorRef.apvts.getParameter (prefix + "_solo"))
                soloAttachment = std::make_unique<juce::ButtonParameterAttachment> (*solo, soloButton, nullptr);
            if (auto* parameter = processorRef.apvts.getParameter (prefix + "_band"))
                bandAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*parameter, band, nullptr);
            for (juce::Component* item : { (juce::Component*) &soloButton, (juce::Component*) &band, (juce::Component*) &editButton,
                                           (juce::Component*) &duplicateButton, (juce::Component*) &removeButton })
                item->setVisible (true);
            editButton.setButtonText ("EDIT SETTINGS");
            editButton.setTooltip ("Open this effect's card on the RACK tab, with its settings on the controls");
        }
        layoutInspector();
        repaint (inspector);
    }

    //==========================================================================
    // Edits.
    void fit() { view.fitContent (maxZoom, minZoom); }

    // A cable may not cross the line at VOICES: the per-note boxes feed only
    // each other and VOICES, the effects never feed back into a note.
    bool accept (const PatchGraph::Graph& graph)
    {
        for (const auto& cable : graph.cables())
        {
            const auto* from = graph.find (cable.from.node);
            const auto* to = graph.find (cable.to.node);
            if (from == nullptr || to == nullptr)
                continue;
            if (isVoice (*from) && ! isVoice (*to) && ! to->isInput())
            {
                refusal = "Notes meet at VOICES before any effect.";
                return false;
            }
            if (! isVoice (*from) && (isVoice (*to) || to->isInput()))
            {
                refusal = "Effects come after VOICES: they can't feed a note.";
                return false;
            }
        }
        if (VoiceRouting::hasLoop (voicePatchOf (graph)))
        {
            refusal = "That would close a loop: the filters run in a line.";
            return false;
        }
        return true;
    }

    // The FX patch a merged graph makes: the effects' cables and places.
    PatchGraph::Graph fxGraphOf (const PatchGraph::Graph& graph) const
    {
        auto fx = fxShown;
        for (const auto& cable : std::vector<PatchGraph::Cable> (fx.cables()))
            fx.disconnect (cable.from, cable.to);
        for (const auto& cable : graph.cables())
        {
            const auto* from = graph.find (cable.from.node);
            if (from != nullptr && ! isVoice (*from))
                fx.connect (cable.from, cable.to, cable.gain);
        }
        for (const auto& node : graph.nodes())
            if (! isVoice (node) && ! node.isInput())
                fx.moveNode (node.id, node.x - fxShift.x, node.y - fxShift.y);
        return fx;
    }

    static bool sameCables (const PatchGraph::Graph& a, const PatchGraph::Graph& b)
    {
        if (a.cables().size() != b.cables().size())
            return false;
        for (const auto& cable : a.cables())
        {
            const auto index = b.findCable (cable.from, cable.to);
            if (index < 0 || b.cables()[(size_t) index].gain != cable.gain)
                return false;
        }
        return true;
    }

    // A view edit: each side that changed goes to the processor (and plays
    // its cables, if it didn't: a changed cable means the cables now
    // decide). One undo step.
    void commit (PatchCableView::Edit edit)
    {
        const auto fx = fxGraphOf (shown);
        if (edit == PatchCableView::Edit::layout)
        {
            processorRef.performEdit ("Move effect", [this, fx] { processorRef.setFxPatchGraph (fx); });
            refresh (true);
            return;
        }
        // (Oscillators the OSC page hides have no box: their cables stay.)
        auto voice = voicePatchOf (shown);
        for (int source = 0; source < VoiceRouting::numSources; ++source)
            if (shown.find (voiceId + (PatchGraph::NodeId) source) == nullptr)
                voice.source[(size_t) source] = voiceShown.source[(size_t) source];
        const auto voiceChanged = hasVoice && ! (voice == voiceShown);
        const auto fxChanged = ! sameCables (fx, fxShown);
        if (! voiceChanged && ! fxChanged)
            return refresh (true);
        processorRef.performEdit ("Patch cables", [this, fx, voice, voiceChanged, fxChanged]
        {
            if (voiceChanged)
                processorRef.setRouteCables (voice, true);
            if (fxChanged)
            {
                if (! processorRef.isFxPatcherOn())
                    processorRef.setFxPatcherOn (true);
                processorRef.setFxPatchGraph (fx);
            }
        });
        refresh (true);
    }

    void setVoiceCables (bool on)
    {
        if (on == processorRef.areRouteCablesOn())
            return;
        // The first time (the cables still the default), they start as the
        // chain plays, so switching to CABLES changes nothing heard.
        auto patch = processorRef.getRoutePatch();
        if (on && patch == VoiceRouting::defaultPatch())
            patch = processorRef.getChainAsCables();
        processorRef.performEdit (on ? "Routing: cables" : "Routing: chain", [this, patch, on] { processorRef.setRouteCables (patch, on); });
        refresh (true);
    }

    void resetVoiceToChain()
    {
        const auto chain = processorRef.getChainAsCables();
        processorRef.performEdit ("Routing: cables from the chain", [this, chain] { processorRef.setRouteCables (chain, processorRef.areRouteCablesOn()); });
        refresh (true);
    }

    void setFxRouting (int mode)
    {
        const auto patcher = processorRef.isFxPatcherOn();
        if (mode == 2)
        {
            if (! patcher)
                processorRef.performEdit ("FX routing: patcher", [this] { processorRef.setFxPatcherOn (true); });
        }
        else
        {
            processorRef.performEdit (mode == 1 ? "FX routing: parallel" : "FX routing: series", [this, mode, patcher]
            {
                if (patcher)
                    processorRef.setFxPatcherOn (false);
                if (auto* routing = processorRef.apvts.getParameter ("fx_routing"))
                    routing->setValueNotifyingHost ((float) mode);
            });
        }
        refresh (true);
    }

    void toggleBypass (int slot)
    {
        if (slot < 0)
            return;
        if (auto* bypass = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
            processorRef.performEdit ("Bypass effect", [bypass] { bypass->setValueNotifyingHost (bypass->getValue() > 0.5f ? 0.0f : 1.0f); });
        refresh (true);
    }

    void removeEffect (int slot)
    {
        if (slot < 0)
            return;
        processorRef.performEdit ("Remove effect", [this, slot] { processorRef.removeFxEffect (slot); });
        refresh (true);
    }

    void duplicateEffect (int slot)
    {
        const PatchGraph::Node* node = nullptr;
        for (const auto& box : shown.nodes())
            if (slotOf (&box) == slot)
                node = &box;
        if (node == nullptr)
            return;
        const auto type = node->kind;
        const juce::Point<float> at (node->x, node->y + 70.0f);
        auto added = -1;
        processorRef.performEdit ("Duplicate effect", [this, slot, type, &added]
        {
            if (! processorRef.isFxSlotInFront (slot))
                processorRef.focusFxInstance (slot);
            added = processorRef.addFxEffect (type);
        });
        if (added >= 0)
            placeNode (added, at);
    }

    void open (const PatchGraph::Node& node)
    {
        if (isVoice (node))
        {
            if (onShowPage)
                onShowPage (localKind (node) >= stageKind ? "FILTER" : "OSC");
        }
        else if (const auto slot = slotOf (&node); slot >= 0 && onOpenSlot)
        {
            onOpenSlot (slot);
        }
    }

    void openSelected()
    {
        if (const auto* node = shown.find (view.getSelectedNode()))
            open (*node);
    }

    void addNodeItems (juce::PopupMenu& menu, const PatchGraph::Node& node)
    {
        if (isVoice (node))
        {
            const auto kind = localKind (node);
            menu.addItem (kind >= stageKind ? "Edit on the FILTER page" : "Edit on the OSC page", [this, node] { open (node); });
            if (kind != stageKind + VoiceRouting::f1 && kind != stageKind + VoiceRouting::f2)
                menu.addItem ("Remove its cables", [this, id = node.id]
                {
                    for (const auto& cable : std::vector<PatchGraph::Cable> (shown.cables()))
                        if (cable.from.node == id)
                            shown.disconnect (cable.from, cable.to);
                    commit (PatchCableView::Edit::cables);
                });
            return;
        }
        const auto slot = slotOf (&node);
        if (slot < 0)
            return;
        menu.addItem ("Edit settings", [this, node] { open (node); });
        menu.addItem ("Duplicate (its own settings)", [this, slot] { duplicateEffect (slot); });
    }

    void addEffectItems (juce::PopupMenu& menu, juce::Point<float> at)
    {
        const auto full = [this]
        {
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                if (read ("fx_slot" + juce::String (slot + 1)) < 0.5f)
                    return false;
            return true;
        }();
        if (full)
        {
            menu.addItem (-1, "Every slot is in use", false);
            return;
        }
        for (const auto& group : fxLibraryGroups())
        {
            juce::PopupMenu sub;
            for (const auto& entry : group.entries)
            {
                const auto add = [this, at] (int type, const juce::String& name)
                {
                    auto added = -1;
                    processorRef.performEdit ("Add " + name, [this, type, &added] { added = processorRef.addFxEffect (type); });
                    if (added >= 0)
                        placeNode (added, at);
                };
                sub.addItem (entry.name, [add, type = entry.type, name = juce::String (entry.name)] { add (type, name); });
                if (entry.twin > 0)
                    sub.addItem (juce::String (entry.twinName) + " (Airwindows)",
                                 [add, type = entry.twin, name = juce::String (entry.twinName)] { add (type, name); });
            }
            menu.addSubMenu (juce::String (group.title).toLowerCase().replace ("&", "and").toUpperCase(), sub);
        }
    }

    // A new effect's box where the menu was opened (the processor wired it in
    // before OUT); at is a merged-graph position.
    void placeNode (int slot, juce::Point<float> at)
    {
        auto graph = processorRef.getFxPatchGraph();
        for (const auto& node : std::vector<PatchGraph::Node> (graph.nodes()))
            if (node.ref == slot + 1 && ! node.isFixed())
            {
                graph.moveNode (node.id, std::round ((at.x - fxShift.x) / 10.0f) * 10.0f, std::round ((at.y - fxShift.y) / 10.0f) * 10.0f);
                if (processorRef.isFxPatcherOn())
                    processorRef.setFxPatchGraph (graph);
            }
        refresh (true);
    }

    //==========================================================================
    // The merged graph: the voice's boxes in columns at the left (sources,
    // then each stage one column right of the furthest stage feeding it),
    // VOICES after them, and the FX patch moved so its INPUT is VOICES.
    void refresh (bool force)
    {
        const auto cablesOn = hasVoice && processorRef.areRouteCablesOn();
        const auto patcher = processorRef.isFxPatcherOn();
        const auto parallel = read ("fx_routing") > 0.5f;
        chainButton.setToggleState (! cablesOn, juce::dontSendNotification);
        cablesButton.setToggleState (cablesOn, juce::dontSendNotification);
        seriesButton.setToggleState (! patcher && ! parallel, juce::dontSendNotification);
        parallelButton.setToggleState (! patcher && parallel, juce::dontSendNotification);
        patcherButton.setToggleState (patcher, juce::dontSendNotification);

        const auto patch = hasVoice ? VoiceRouting::withoutLoops (cablesOn ? processorRef.getRoutePatch() : processorRef.getChainAsCables())
                                    : VoiceRouting::Patch {};
        std::vector<int> sources;
        if (hasVoice)
        {
            for (int osc = 0; osc < VoiceRouting::numOscSources; ++osc)
                if (processorRef.isOscillatorShown (osc))
                    sources.push_back (osc);
            sources.push_back (VoiceRouting::subNoiseSource);
        }

        juce::String signature;
        signature << (cablesOn ? "C" : "-") << (patcher ? "P" : "-") << (parallel ? "p" : "-") << processorRef.getPatchDataHash();
        for (const auto mask : patch.source)
            signature << "," << (int) mask;
        for (const auto mask : patch.stage)
            signature << "," << (int) mask;
        for (const auto source : sources)
            signature << "/" << source;
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            signature << "," << (int) read (prefix) << (read (prefix + "_bypass") > 0.5f ? "b" : "") << (read (prefix + "_solo") > 0.5f ? "s" : "");
        }
        if (! force && signature == lastSignature)
            return;
        lastSignature = signature;
        table = VoiceRouting::compile (patch);
        voiceShown = patch;

        PatchGraph::Graph graph;
        auto voicesAt = juce::Point<float> (0.0f, 0.0f);
        voiceArea = {};
        if (hasVoice)
            voicesAt = addVoice (graph, patch, sources);

        // The FX patch, each box named as the library names it (numbered when
        // the rack has two of one effect), moved so INPUT sits at VOICES.
        fxShown = processorRef.getFxPatchGraph();
        nameEffects (fxShown);
        const auto* input = fxShown.find (PatchGraph::inputNodeId);
        fxShift = input != nullptr ? voicesAt - juce::Point<float> (input->x, input->y) : juce::Point<float>();
        for (const auto& node : fxShown.nodes())
        {
            if (node.isFixed())
                continue;
            auto moved = node;
            moved.x += fxShift.x;
            moved.y += fxShift.y;
            graph.restoreNode (moved);
        }
        if (const auto* output = fxShown.find (PatchGraph::outputNodeId))
            graph.moveNode (PatchGraph::outputNodeId, output->x + fxShift.x, output->y + fxShift.y);
        graph.moveNode (PatchGraph::inputNodeId, voicesAt.x, voicesAt.y);
        if (auto* voices = graph.find (PatchGraph::inputNodeId))
        {
            voices->label = hasVoice ? "VOICES" : "INPUT";
            voices->numInputs = hasVoice ? 1 : 0;
        }
        if (auto* output = graph.find (PatchGraph::outputNodeId))
            output->label = "OUT";
        for (const auto& cable : fxShown.cables())
            graph.connect (cable.from, cable.to, cable.gain);
        if (hasVoice)
            connectVoice (graph, patch, sources);

        shown = graph;
        view.setLoopLatencyMs (juce::jmax (0.1f, (float) (64000.0 / juce::jmax (1.0, processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 48000.0))));
        view.graphChanged();
        // A new box or one outside the view: the patch is fitted again.
        if ((int) shown.nodes().size() != shownNodes || ! view.contentFits())
            fit();
        shownNodes = (int) shown.nodes().size();
        repaint();
    }

    void nameEffects (PatchGraph::Graph& fx) const
    {
        std::array<int, 64> count {}, seen {};
        for (const auto& node : fx.nodes())
            if (! node.isFixed() && juce::isPositiveAndBelow (node.kind, 64))
                ++count[(size_t) node.kind];
        std::vector<PatchGraph::Node> ordered (fx.nodes().begin(), fx.nodes().end());
        std::sort (ordered.begin(), ordered.end(), [] (const auto& a, const auto& b) { return a.ref < b.ref; });
        for (const auto& node : ordered)
        {
            if (node.isFixed() || ! juce::isPositiveAndBelow (node.kind, 64))
                continue;
            auto label = fxTypeName (node.kind).toUpperCase();
            if (count[(size_t) node.kind] > 1)
                label << " " << ++seen[(size_t) node.kind];
            if (auto* editable = fx.find (node.id))
                editable->label = label.toStdString();
        }
    }

    // The voice's boxes; returns where VOICES goes.
    juce::Point<float> addVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources)
    {
        std::array<int, VoiceRouting::numStages> column { 1, 1, 1 };
        std::array<int, VoiceRouting::numStages> order {};
        VoiceRouting::topologicalOrder (patch, order);
        for (const auto stage : order)
            for (int from = 0; from < VoiceRouting::numStages; ++from)
                if (from != stage && (patch.stage[(size_t) from] & VoiceRouting::bitOf (stage)) != 0)
                    column[(size_t) stage] = juce::jmax (column[(size_t) stage], column[(size_t) from] + 1);
        // A stage not in use that only goes to VOICES waits in the last stage
        // column (under the others), so its cable doesn't cross the chain.
        auto lastColumn = 1;
        for (int stage = 0; stage < VoiceRouting::numStages; ++stage)
            if (table.used[(size_t) stage])
                lastColumn = juce::jmax (lastColumn, column[(size_t) stage]);
        for (int stage = 0; stage < VoiceRouting::numStages; ++stage)
            if (! table.used[(size_t) stage] && (patch.stage[(size_t) stage] & ~VoiceRouting::toOut) == 0)
            {
                auto fed = false;
                for (int from = 0; from < VoiceRouting::numStages; ++from)
                    fed = fed || (from != stage && (patch.stage[(size_t) from] & VoiceRouting::bitOf (stage)) != 0);
                if (! fed)
                    column[(size_t) stage] = lastColumn;
            }
        const auto voicesColumn = *std::max_element (column.begin(), column.end()) + 1;

        const auto addBox = [&graph] (int kind, int inputs, float x, float y)
        {
            PatchGraph::Node node;
            node.id = voiceId + (PatchGraph::NodeId) kind;
            node.kind = voiceKind + kind;
            node.ref = 0;
            node.numInputs = inputs;
            node.numOutputs = 1;
            node.x = x;
            node.y = y;
            node.label = voiceName (kind).toStdString();
            graph.restoreNode (node);
        };
        const auto middle = (float) ((int) sources.size() - 1) * rowStep * 0.5f;
        for (size_t row = 0; row < sources.size(); ++row)
        {
            addBox (sources[row], 0, 0.0f, (float) row * rowStep);
            if (! sourceOn (sources[row]))
                graph.setBypassed (voiceId + (PatchGraph::NodeId) sources[row], true);
        }
        for (int col = 1; col < voicesColumn; ++col)
        {
            std::vector<int> here;
            for (const auto stage : order)
                if (column[(size_t) stage] == col)
                    here.push_back (stage);
            // Filters first, WEST below them; the column centred on the sources.
            std::sort (here.begin(), here.end());
            const auto step = rowStep + 30.0f;
            for (size_t k = 0; k < here.size(); ++k)
            {
                const auto y = std::round ((middle - (float) (here.size() - 1) * step * 0.5f + (float) k * step) / 10.0f) * 10.0f;
                addBox (stageKind + here[k], 1, (float) col * columnStep, y);
            }
        }
        if (! table.used[VoiceRouting::west] || read ("west_on") < 0.5f)
            graph.setBypassed (voiceId + (PatchGraph::NodeId) (stageKind + VoiceRouting::west), true);

        // The band behind them: 20 px round the boxes, room for its name above.
        auto bottom = (float) ((int) sources.size() - 1) * rowStep + PatchCableView::nodeHeight;
        auto top = 0.0f;
        for (const auto& node : graph.nodes())
            if (isVoice (node))
            {
                top = juce::jmin (top, node.y);
                bottom = juce::jmax (bottom, node.y + PatchCableView::nodeHeight);
            }
        const auto voicesX = (float) voicesColumn * columnStep;
        voiceArea = { -20.0f, top - 40.0f, voicesX - 25.0f + 20.0f, bottom - top + 60.0f };
        return { voicesX, std::round (middle / 10.0f) * 10.0f };
    }

    void connectVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources)
    {
        const auto targetOf = [] (int destination) -> PatchGraph::NodeId
        {
            return destination == VoiceRouting::out ? PatchGraph::inputNodeId : voiceId + (PatchGraph::NodeId) (stageKind + destination);
        };
        for (const auto source : sources)
            for (int destination = 0; destination <= VoiceRouting::numStages; ++destination)
                if ((patch.source[(size_t) source] & VoiceRouting::bitOf (destination)) != 0)
                    graph.connect ({ voiceId + (PatchGraph::NodeId) source, 0 }, { targetOf (destination), 0 });
        for (int stage = 0; stage < VoiceRouting::numStages; ++stage)
            for (int destination = 0; destination <= VoiceRouting::numStages; ++destination)
                if (destination != stage && (patch.stage[(size_t) stage] & VoiceRouting::bitOf (destination)) != 0)
                    graph.connect ({ voiceId + (PatchGraph::NodeId) (stageKind + stage), 0 }, { targetOf (destination), 0 });
    }

    void timerCallback() override
    {
        // (Not under a drag: the view is editing its copy.)
        if (! isVisible() || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            return;
        // Switches and types the voice's boxes show (cheap: the signature decides).
        juce::String state;
        if (hasVoice)
        {
            for (int kind = 0; kind < VoiceRouting::numSources; ++kind)
                state << (sourceOn (kind) ? "1" : "0");
            state << read ("west_on") << read ("f1_type") << read ("f2_type");
        }
        refresh (state != lastState);
        lastState = state;
        inspect (shown.find (view.getSelectedNode()));
        if (juce::Time::getMillisecondCounter() >= refusedUntil && refusedUntil != 0)
        {
            refusedUntil = 0;
            repaint (hint);
        }
        view.repaint(); // the CPU readouts
        repaint (inspector);
    }

    IlanaSynthAudioProcessor& processorRef;
    FxPage& rack;
    PatchGraph::Graph shown, fxShown;
    PatchCableView view;
    const bool hasVoice;
    VoiceRouting::Table table;
    VoiceRouting::Patch voiceShown;
    juce::Point<float> fxShift;
    juce::Rectangle<float> voiceArea;

    juce::TextButton chainButton { "CHAIN" }, cablesButton { "CABLES" };
    juce::TextButton seriesButton { "SERIES" }, parallelButton { "PARALLEL" }, patcherButton { "PATCHER" };
    juce::TextButton addButton { "+ ADD EFFECT" }, resetButton { "RESET" }, fitButton { "FIT" };
    juce::Rectangle<int> voiceCaption, voiceSeg, fxCaption, fxSeg, hint, inspector, inspectorText, onArea, powerArea, soloArea, bandArea;

    int inspectedVoice = -1, inspectedSlot = -1;
    juce::TextButton onButton { "ON" };
    std::unique_ptr<juce::ButtonParameterAttachment> onAttachment;
    std::unique_ptr<SlotSwitch> power;
    std::unique_ptr<KnobControl> mixKnob;
    juce::TextButton soloButton { "S" };
    juce::ComboBox band;
    std::unique_ptr<juce::ButtonParameterAttachment> soloAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> bandAttachment;
    juce::TextButton editButton { "EDIT SETTINGS" }, duplicateButton { "DUPLICATE" }, removeButton { "REMOVE" };

    juce::String lastSignature, lastState, refusal;
    juce::uint32 refusedUntil = 0;
    int shownNodes = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchPage)
};
} // namespace
