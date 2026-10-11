// The FILTER tab's ROUTING sub-page (roadmap step 16): the oscillators,
// Filter 1, Filter 2 and WEST as boxes on the cable view (PatchCableView.h).
// A cable from an oscillator sends it to a filter, to WEST or straight OUT
// (several cables play it in parallel); cables between the filters and WEST
// set the order they run in. The cables are parameters (route_cables,
// cable_*; dsp/VoiceRouting.h). While they are off the page shows the fixed
// chain as cables, and the first cable edit hands the routing to them.
// Included only from PluginEditor.cpp, after the FILTER page.
#pragma once

namespace
{
class OscRoutingPage : public juce::Component,
                       private juce::Timer
{
public:
    explicit OscRoutingPage (IlanaSynthAudioProcessor& p) : processorRef (p), view (shown)
    {
        chainButton.setTooltip ("Chain\nThe fixed chain: each oscillator's route menu, SERIAL / PARALLEL and WEST's position decide.");
        cablesButton.setTooltip ("Cables\nThe cables below decide where each oscillator goes and the order of the filters and WEST.");
        chainButton.setDescription ("Oscillator routing: chain");
        cablesButton.setDescription ("Oscillator routing: cables");
        for (auto* button : { &chainButton, &cablesButton })
        {
            IlanaTheme::makePill (*button, IlanaTheme::accent());
            button->setRadioGroupId (0x5e71a3);
            addAndMakeVisible (*button);
        }
        chainButton.onClick = [this] { setCables (false); };
        cablesButton.onClick = [this] { setCables (true); };

        resetButton.setTooltip ("Cables back to the fixed chain's routing (each oscillator's route, SERIAL / PARALLEL, WEST's position)");
        resetButton.onClick = [this] { resetToChain(); };
        fitButton.setTooltip ("Fit the routing to the view");
        fitButton.onClick = [this] { view.fitContent (maxZoom); };
        for (auto* quiet : { &resetButton, &fitButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
            addAndMakeVisible (*quiet);
        }

        view.setName ("OSC ROUTING"); // (the UI test finds it by name)
        view.showInputNode = false;
        view.nodesRemovable = false;
        view.cableLevels = false;
        view.colourOf = [] (const PatchGraph::Node& node) { return colourOf (node.kind); };
        view.statusOf = [this] (const PatchGraph::Node& node) { return statusOf (node); };
        view.acceptGraph = [] (const PatchGraph::Graph& graph) { return ! VoiceRouting::hasLoop (patchOf (graph)); };
        view.onRefused = [this]
        {
            refusedUntil = juce::Time::getMillisecondCounter() + 3000;
            repaint (hint);
        };
        view.onEdited = [this] (PatchCableView::Edit edit) { commit (edit); };
        view.onOpen = [this] (const PatchGraph::Node& node) { open (node.kind); };
        view.nodeMenu = [this] (juce::PopupMenu& menu, const PatchGraph::Node& node) { addNodeItems (menu, node); };
        addAndMakeVisible (view);

        openButton.onClick = [this] { open (inspectedKind); };
        IlanaTheme::makePill (openButton, IlanaTheme::accent());
        IlanaTheme::makePill (onButton, IlanaTheme::accent());
        onButton.setClickingTogglesState (true);
        addChildComponent (openButton);
        addChildComponent (onButton);

        refresh (true);
        startTimerHz (10);
    }

    // The page an OPEN button shows (set by the editor): "OSC" or "FILTER".
    std::function<void (const juce::String&)> onShowPage;

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        paintPageBackground (g, getLocalBounds());
        paintWell (g, routingSeg.toFloat(), (float) routingSeg.getHeight() * 0.5f);
        const auto refused = juce::Time::getMillisecondCounter() < refusedUntil;
        g.setColour (refused ? Ui::accent2 : Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (refused ? juce::String ("That cable would close a loop: the filters and WEST run one after another, never in a circle.")
                            : hintText(),
                    hint, juce::Justification::centredLeft, true);
        paintInspector (g);
    }

    void resized() override
    {
        // The FX PATCHER's frame: 8 px above and below, a 12 px gutter at the right.
        auto area = getLocalBounds().reduced (0, 8).withTrimmedRight (12);
        auto toolbar = area.removeFromTop (toolbarHeight);
        routingSeg = toolbar.removeFromLeft (152);
        chainButton.setBounds (routingSeg.withTrimmedLeft (3).removeFromLeft (70).reduced (0, 2));
        cablesButton.setBounds (routingSeg.withTrimmedLeft (3 + 72).removeFromLeft (74).reduced (0, 2));
        toolbar.removeFromLeft (10);
        fitButton.setBounds (toolbar.removeFromRight (70).reduced (0, 2));
        toolbar.removeFromRight (6);
        resetButton.setBounds (toolbar.removeFromRight (80).reduced (0, 2));
        hint = toolbar.withTrimmedRight (10);
        area.removeFromTop (10);
        inspector = area.removeFromBottom (inspectorHeight);
        area.removeFromBottom (10);
        view.setBounds (area);
        view.fitContent (maxZoom);
        layoutInspector();
    }

    // Tests: the graph the view shows.
    const PatchGraph::Graph& getShownGraph() const { return shown; }
    PatchCableView& getView() { return view; }

    // Node kinds: the sources by their VoiceRouting index (0-5 OSC 1-6,
    // 6 SUB + NOISE), then the stages.
    static constexpr int stageKind = 10; // + VoiceRouting::Stage

    // The patch a graph's cables make.
    static VoiceRouting::Patch patchOf (const PatchGraph::Graph& graph)
    {
        VoiceRouting::Patch patch;
        for (const auto& cable : graph.cables())
        {
            const auto* from = graph.find (cable.from.node);
            const auto* to = graph.find (cable.to.node);
            if (from == nullptr || to == nullptr)
                continue;
            int destination = -1;
            if (to->isOutput())
                destination = VoiceRouting::out;
            else if (to->kind >= stageKind && to->kind < stageKind + VoiceRouting::numStages)
                destination = to->kind - stageKind;
            if (destination < 0)
                continue;
            const auto bit = VoiceRouting::bitOf (destination);
            if (juce::isPositiveAndBelow (from->kind, VoiceRouting::numSources))
                patch.source[(size_t) from->kind] = (std::uint8_t) (patch.source[(size_t) from->kind] | bit);
            else if (from->kind >= stageKind && from->kind < stageKind + VoiceRouting::numStages)
                patch.stage[(size_t) (from->kind - stageKind)] = (std::uint8_t) (patch.stage[(size_t) (from->kind - stageKind)] | bit);
        }
        return patch;
    }

private:
    static constexpr int toolbarHeight = 28;
    static constexpr int inspectorHeight = 70;
    static constexpr float maxZoom = 1.3f;
    // The 10 px grid: columns 210 apart (a 130 px box and room for the
    // cables), rows 70 apart (a 50 px box and a 20 px gap).
    static constexpr float columnStep = 210.0f, rowStep = 70.0f;

    static juce::Colour colourOf (int kind)
    {
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

    static juce::String nameOf (int kind)
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

    juce::String onParamOf (int kind) const
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

    // A box's second line: what it is, or why it isn't heard.
    juce::String statusOf (const PatchGraph::Node& node) const
    {
        const auto kind = node.kind;
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
        if (! processorRef.areRouteCablesOn())
            return "The fixed chain as cables. Change a cable and CABLES takes over the routing.";
        return "Drag an oscillator to a filter, WEST or OUTPUT; more cables play it in parallel. Cables between the filters set their order.";
    }

    // The order the used stages run in, as a line: "FILTER 1 > WEST > OUT".
    juce::String orderText() const
    {
        juce::StringArray names;
        for (int k = 0; k < table.numOrdered; ++k)
            names.add (nameOf (stageKind + table.order[(size_t) k]));
        names.add ("OUT");
        return names.joinIntoString ("  >  ");
    }

    void paintInspector (juce::Graphics& g)
    {
        using namespace IlanaTheme;
        if (inspector.isEmpty())
            return;
        paintCard (g, inspector.toFloat(), 8.0f, Ui::line);
        auto text = inspectorText;
        if (inspectedKind < 0)
        {
            g.setColour (Ui::text);
            g.setFont (font (TextSize::body, true));
            g.drawText (orderText(), text.removeFromTop (text.getHeight() / 2), juce::Justification::bottomLeft, true);
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::label));
            g.drawText ("The stages in the order they run. Click a box to see where it goes; double-click it to edit it.",
                        text.withTrimmedTop (4), juce::Justification::topLeft, true);

            // At the right: how the patch is wired.
            auto stats = inspector.reduced (20, 10).removeFromRight (300);
            auto parallel = 0;
            for (int source = 0; source < VoiceRouting::numSources; ++source)
                if (sourceOn (source) && table.busOf[(size_t) source] != VoiceRouting::mutedBus)
                {
                    auto destinations = 0;
                    for (int node = 0; node <= VoiceRouting::numStages; ++node)
                        destinations += (table.busMask[(size_t) table.busOf[(size_t) source]] & VoiceRouting::bitOf (node)) != 0 ? 1 : 0;
                    parallel += destinations > 1 ? 1 : 0;
                }
            for (const auto& [name, value] : { std::pair<juce::String, juce::String> { "ROUTING", processorRef.areRouteCablesOn() ? "CABLES" : "CHAIN" },
                                               { "SPLIT SOURCES", juce::String (parallel) } })
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
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colourOf (inspectedKind));
        g.setColour (Ui::text);
        g.setFont (font (TextSize::large, true));
        g.drawText (nameOf (inspectedKind), title.withTrimmedLeft (16), juce::Justification::bottomLeft, true);
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (destinationText (inspectedKind), text.withTrimmedTop (4).withTrimmedLeft (16), juce::Justification::topLeft, true);
        if (! onArea.isEmpty())
        {
            g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
            g.drawText ("ON", juce::Rectangle<int> (onArea.getX(), onArea.getY() - 16, 60, 14), juce::Justification::centredLeft, false);
        }
    }

    // "Goes to FILTER 1 and OUT", from the shown cables.
    juce::String destinationText (int kind) const
    {
        juce::StringArray to;
        for (const auto& cable : shown.cables())
            if (const auto* from = shown.find (cable.from.node); from != nullptr && from->kind == kind)
                if (const auto* target = shown.find (cable.to.node))
                    to.add (target->isOutput() ? juce::String ("OUT") : nameOf (target->kind));
        if (to.isEmpty())
            return "No cable: not heard";
        const auto last = to[to.size() - 1];
        to.remove (to.size() - 1);
        return "Goes to " + (to.isEmpty() ? last : to.joinIntoString (", ") + " and " + last)
               + (to.isEmpty() ? juce::String() : juce::String (", in parallel"));
    }

    void layoutInspector()
    {
        auto row = inspector.reduced (20, 10);
        inspectorText = row.removeFromLeft (juce::jmin (420, row.getWidth() / 2));
        onArea = {};
        if (inspectedKind < 0)
        {
            inspectorText = inspector.reduced (20, 10);
            return;
        }
        auto actions = row.removeFromRight (160);
        openButton.setBounds (actions.removeFromRight (160).withSizeKeepingCentre (160, 28));
        if (onButton.isVisible())
        {
            row.removeFromLeft (20);
            const auto centreY = row.getY() + 32;
            onArea = juce::Rectangle<int> (row.getX(), centreY - 13, 50, 26);
            onButton.setBounds (onArea);
        }
    }

    void inspect (int kind)
    {
        if (kind == inspectedKind)
            return;
        inspectedKind = kind;
        onAttachment.reset();
        const auto onId = kind >= 0 ? onParamOf (kind) : juce::String();
        if (onId.isNotEmpty())
            if (auto* parameter = processorRef.apvts.getParameter (onId))
                onAttachment = std::make_unique<juce::ButtonParameterAttachment> (*parameter, onButton, nullptr);
        onButton.setVisible (onAttachment != nullptr);
        openButton.setVisible (kind >= 0);
        openButton.setButtonText (kind >= stageKind ? "EDIT ON FILTER" : "EDIT ON OSC");
        openButton.setTooltip (kind >= stageKind ? "Open the FILTER page" : "Open the OSC page");
        layoutInspector();
        repaint (inspector);
    }

    void open (int kind)
    {
        if (onShowPage && kind >= 0)
            onShowPage (kind >= stageKind ? "FILTER" : "OSC");
    }

    void addNodeItems (juce::PopupMenu& menu, const PatchGraph::Node& node)
    {
        const auto kind = node.kind;
        menu.addItem (kind >= stageKind ? "Edit on the FILTER page" : "Edit on the OSC page", [this, kind] { open (kind); });
        if (kind != stageKind + VoiceRouting::f1 && kind != stageKind + VoiceRouting::f2)
            menu.addItem ("Remove its cables", [this, id = node.id]
            {
                for (const auto& cable : std::vector<PatchGraph::Cable> (shown.cables()))
                    if (cable.from.node == id)
                        shown.disconnect (cable.from, cable.to);
                commit (PatchCableView::Edit::cables);
            });
    }

    void setCables (bool on)
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

    void resetToChain()
    {
        const auto chain = processorRef.getChainAsCables();
        processorRef.performEdit ("Routing: cables from the chain", [this, chain] { processorRef.setRouteCables (chain, processorRef.areRouteCablesOn()); });
        refresh (true);
    }

    // A view edit: the cables go to the processor (CABLES on, if it wasn't:
    // a changed cable means the cables now decide). One undo step.
    void commit (PatchCableView::Edit edit)
    {
        if (edit == PatchCableView::Edit::layout)
            return; // (the boxes are placed by the routing, not saved)
        const auto patch = patchOf (shown);
        processorRef.performEdit ("Routing cables", [this, patch] { processorRef.setRouteCables (patch, true); });
        refresh (true);
    }

    // The graph for the processor's routing: sources down the left, each
    // stage in the column after the stages feeding it, OUTPUT last.
    void refresh (bool force)
    {
        const auto on = processorRef.areRouteCablesOn();
        chainButton.setToggleState (! on, juce::dontSendNotification);
        cablesButton.setToggleState (on, juce::dontSendNotification);

        const auto patch = VoiceRouting::withoutLoops (on ? processorRef.getRoutePatch() : processorRef.getChainAsCables());
        juce::String signature;
        signature << (on ? "C" : "-");
        for (const auto mask : patch.source)
            signature << "," << (int) mask;
        for (const auto mask : patch.stage)
            signature << "," << (int) mask;
        std::vector<int> sources;
        for (int osc = 0; osc < VoiceRouting::numOscSources; ++osc)
            if (processorRef.isOscillatorShown (osc))
                sources.push_back (osc);
        sources.push_back (VoiceRouting::subNoiseSource);
        for (const auto source : sources)
            signature << "/" << source;

        if (! force && signature == lastSignature)
            return;
        lastSignature = signature;
        table = VoiceRouting::compile (patch);

        // Columns: a stage sits one column right of the furthest stage
        // feeding it (unfed stages in the first stage column).
        std::array<int, VoiceRouting::numStages> column { 1, 1, 1 };
        std::array<int, VoiceRouting::numStages> order {};
        VoiceRouting::topologicalOrder (patch, order);
        for (const auto stage : order)
            for (int from = 0; from < VoiceRouting::numStages; ++from)
                if (from != stage && (patch.stage[(size_t) from] & VoiceRouting::bitOf (stage)) != 0)
                    column[(size_t) stage] = juce::jmax (column[(size_t) stage], column[(size_t) from] + 1);
        // A stage not in use that only goes OUT waits in the last stage
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
        const auto outColumn = *std::max_element (column.begin(), column.end()) + 1;

        PatchGraph::Graph graph;
        std::array<PatchGraph::NodeId, VoiceRouting::numSources + VoiceRouting::numStages> ids {};
        const auto sourceRows = (int) sources.size();
        const auto middle = (float) (sourceRows - 1) * rowStep * 0.5f;
        for (int row = 0; row < sourceRows; ++row)
        {
            const auto kind = sources[(size_t) row];
            ids[(size_t) kind] = graph.addNode (kind, 0, 1, 0.0f, (float) row * rowStep, nameOf (kind).toStdString(), kind);
        }
        for (int col = 1; col < outColumn; ++col)
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
                const auto kind = stageKind + here[k];
                ids[(size_t) (VoiceRouting::numSources + here[k])]
                    = graph.addNode (kind, 1, 1, (float) col * columnStep, y, nameOf (kind).toStdString(), kind);
            }
        }
        graph.moveNode (PatchGraph::outputNodeId, (float) outColumn * columnStep, std::round ((middle) / 10.0f) * 10.0f);
        if (auto* output = graph.find (PatchGraph::outputNodeId))
            output->label = "OUT";

        const auto targetOf = [&] (int destination) -> PatchGraph::NodeId
        {
            return destination == VoiceRouting::out ? PatchGraph::outputNodeId : ids[(size_t) (VoiceRouting::numSources + destination)];
        };
        for (const auto source : sources)
            for (int destination = 0; destination <= VoiceRouting::numStages; ++destination)
                if ((patch.source[(size_t) source] & VoiceRouting::bitOf (destination)) != 0)
                    graph.connect ({ ids[(size_t) source], 0 }, { targetOf (destination), 0 });
        for (int stage = 0; stage < VoiceRouting::numStages; ++stage)
            for (int destination = 0; destination <= VoiceRouting::numStages; ++destination)
                if (destination != stage && (patch.stage[(size_t) stage] & VoiceRouting::bitOf (destination)) != 0)
                    graph.connect ({ ids[(size_t) (VoiceRouting::numSources + stage)], 0 }, { targetOf (destination), 0 });

        // Sources switched off draw dimmed.
        for (const auto source : sources)
            if (! sourceOn (source))
                graph.setBypassed (ids[(size_t) source], true);
        if (! table.used[VoiceRouting::west] || read ("west_on") < 0.5f)
            graph.setBypassed (ids[(size_t) (VoiceRouting::numSources + VoiceRouting::west)], true);

        shown = graph;
        view.graphChanged();
        view.fitContent (maxZoom);
        repaint();
    }

    void timerCallback() override
    {
        // (Not under a drag: the view is editing its copy.)
        if (! isVisible() || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            return;
        // Switches and types the boxes show (cheap: the signature decides).
        juce::String state;
        for (int kind = 0; kind < VoiceRouting::numSources; ++kind)
            state << (sourceOn (kind) ? "1" : "0");
        state << read ("west_on") << read ("f1_type") << read ("f2_type");
        refresh (state != lastState);
        lastState = state;
        const auto* node = shown.find (view.getSelectedNode());
        inspect (node != nullptr && ! node->isFixed() ? node->kind : -1);
        if (juce::Time::getMillisecondCounter() >= refusedUntil && refusedUntil != 0)
        {
            refusedUntil = 0;
            repaint (hint);
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    PatchGraph::Graph shown;
    PatchCableView view;
    VoiceRouting::Table table;
    juce::TextButton chainButton { "CHAIN" }, cablesButton { "CABLES" };
    juce::TextButton resetButton { "RESET" }, fitButton { "FIT" };
    juce::TextButton openButton { "EDIT ON OSC" }, onButton { "ON" };
    std::unique_ptr<juce::ButtonParameterAttachment> onAttachment;
    juce::Rectangle<int> routingSeg, hint, inspector, inspectorText, onArea;
    int inspectedKind = -1;
    juce::String lastSignature, lastState;
    juce::uint32 refusedUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscRoutingPage)
};
} // namespace
