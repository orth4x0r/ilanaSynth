// The FX tab's PATCHER sub-page (roadmap step 17): the rack's effects as
// boxes on the cable view (PatchCableView.h), wired from an output to an
// input as FL Studio's Patcher does: parallel branches, loops (each cable
// that closes one has a 1.3 ms delay and stays under 100 %) and several
// instances of one effect. Included only from PluginEditor.cpp, after
// FxPage.h, in the same anonymous namespace.
#pragma once

namespace
{
class FxPatcherPage : public juce::Component,
                      private juce::Timer
{
public:
    FxPatcherPage (IlanaSynthAudioProcessor& p, FxPage& rackPage)
        : processorRef (p), rack (rackPage), view (shown)
    {
        // The same three-way switch as the RACK page's top bar.
        seriesButton.setTooltip ("Series\nEach effect feeds the next one, in the rack's order.");
        parallelButton.setTooltip ("Parallel\nEvery effect hears the rack's input; their outputs are averaged.");
        patcherButton.setTooltip ("Patcher\nThe cables below decide what feeds what.");
        seriesButton.setDescription ("FX routing: series");
        parallelButton.setDescription ("FX routing: parallel");
        patcherButton.setDescription ("FX routing: patcher");
        for (auto* button : { &seriesButton, &parallelButton, &patcherButton })
        {
            IlanaTheme::makePill (*button, IlanaTheme::accent());
            button->setRadioGroupId (0x5e71a2);
            addAndMakeVisible (*button);
        }
        seriesButton.onClick = [this] { setRouting (0); };
        parallelButton.onClick = [this] { setRouting (1); };
        patcherButton.onClick = [this] { setRouting (2); };

        addButton.setTooltip ("Add an effect: it is wired in just before OUTPUT. One the rack has already is a new instance with its own settings.");
        addButton.onClick = [this] { rack.openLibrary (addButton); };
        centreButton.setTooltip ("Fit the patch to the view");
        centreButton.onClick = [this] { view.fitContent (maxZoom); };
        for (auto* quiet : { &centreButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        }
        addAndMakeVisible (addButton);
        addAndMakeVisible (centreButton);

        view.setName ("FX PATCHER"); // (the UI test finds it by name)
        view.colourOf = [] (const PatchGraph::Node& node) { return fxColour (node.kind); };
        view.cpuOf = [this] (PatchGraph::NodeId id) -> float
        {
            const auto* node = shown.find (id);
            if (node == nullptr || node->isFixed() || ! juce::isPositiveAndBelow (node->ref - 1, IlanaSynthAudioProcessor::numFxSlots))
                return -1.0f;
            return processorRef.getFxSlotCpu (node->ref - 1);
        };
        view.onEdited = [this] (PatchCableView::Edit edit) { commit (edit); };
        view.onBypass = [this] (const PatchGraph::Node& node) { toggleBypass (node.ref - 1); };
        view.onRemove = [this] (const PatchGraph::Node& node) { removeEffect (node.ref - 1); };
        view.onOpen = [this] (const PatchGraph::Node& node) { openEffect (node.ref - 1); };
        view.nodeMenu = [this] (juce::PopupMenu& menu, const PatchGraph::Node& node) { addNodeItems (menu, node); };
        view.addMenu = [this] (juce::PopupMenu& menu, juce::Point<float> at) { addEffectItems (menu, at); };
        addAndMakeVisible (view);

        // The selected box's own row: its switch, solo, band and MIX, and
        // what to do with it.
        soloButton.setClickingTogglesState (true);
        soloButton.setTooltip ("SOLO: hear only this effect's output (wet only)");
        IlanaTheme::makePill (soloButton, IlanaTheme::accent());
        band.addItemList ({ "Full Band", "Low Band", "Mid Band", "High Band", "M/S Mid", "M/S Side" }, 1);
        band.setTooltip ("Band: the part of the signal this effect works on; the rest passes around it.");
        editButton.setTooltip ("Open this effect's card on the RACK tab, with its settings on the controls");
        duplicateButton.setTooltip ("A new instance of this effect with a copy of its settings, wired in before OUTPUT");
        removeButton.setTooltip ("Take this effect out (the cables around it are joined up)");
        editButton.onClick = [this] { openEffect (inspected); };
        duplicateButton.onClick = [this] { duplicateEffect (inspected); };
        removeButton.onClick = [this] { removeEffect (inspected); };
        IlanaTheme::makePill (editButton, IlanaTheme::accent());
        for (auto* quiet : { &duplicateButton, &removeButton })
        {
            quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        }
        for (juce::Component* item : { (juce::Component*) &soloButton, (juce::Component*) &band, (juce::Component*) &editButton,
                                       (juce::Component*) &duplicateButton, (juce::Component*) &removeButton })
            addChildComponent (item);

        refresh (true);
        startTimerHz (15);
    }

    // The RACK page's card for a slot (set by the editor: it switches the sub-page).
    std::function<void (int)> onOpenSlot;

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());
        IlanaTheme::paintWell (g, routingSeg.toFloat(), (float) routingSeg.getHeight() * 0.5f);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (hintText(), hint, juce::Justification::centredLeft, true);
        paintInspector (g);
    }

    void paintInspector (juce::Graphics& g)
    {
        using namespace IlanaTheme;
        if (inspector.isEmpty())
            return;
        paintCard (g, inspector.toFloat(), 8.0f, Ui::line);
        auto text = inspectorText;
        const auto* node = selectedNode();
        if (node == nullptr)
        {
            // Nothing selected: the patch in one line, and how to start.
            g.setColour (Ui::text);
            g.setFont (font (TextSize::body, true));
            g.drawText (summaryText(), text.removeFromTop (text.getHeight() / 2), juce::Justification::bottomLeft, true);
            g.setColour (Ui::text3);
            g.setFont (font (TextSize::label));
            g.drawText ("Click a box to set its MIX, band and solo here. Right-click the canvas to add an effect where you clicked.",
                        text.withTrimmedTop (4), juce::Justification::topLeft, true);

            // At the right: what the patch costs, and its loops' delay.
            auto stats = inspector.reduced (20, 10).removeFromRight (300);
            auto cpu = 0.0f;
            for (const auto& box : shown.nodes())
                if (! box.isFixed() && juce::isPositiveAndBelow (box.ref - 1, IlanaSynthAudioProcessor::numFxSlots))
                    cpu += processorRef.getFxSlotCpu (box.ref - 1);
            const auto loopMs = processorRef.getFxLoopLatencyMs();
            for (const auto& [name, value] : { std::pair<juce::String, juce::String> { "LOOP DELAY", loopMs > 0.0f ? juce::String (loopMs, 1) + " ms" : juce::String ("none") },
                                               { "EFFECTS CPU", juce::String (cpu * 100.0f, 2) + " %" } })
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
        const auto colour = fxColour (node->kind);
        auto title = text.removeFromTop (text.getHeight() / 2);
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colour);
        g.setColour (Ui::text);
        g.setFont (font (TextSize::large, true));
        g.drawText (juce::String (node->label), title.withTrimmedLeft (16), juce::Justification::bottomLeft, true);
        const auto slot = node->ref - 1;
        juce::String detail = "Slot " + juce::String (slot + 1);
        if (processorRef.isFxInstance (slot) || ! processorRef.isFxSlotInFront (slot))
            detail << (processorRef.isFxSlotInFront (slot) ? ", own settings, open on its RACK card" : ", own settings");
        detail << ", " << juce::String (processorRef.getFxSlotCpu (slot) * 100.0f, 2) << " % CPU";
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (detail, text.withTrimmedTop (4).withTrimmedLeft (16), juce::Justification::topLeft, true);
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::tiny, true).withKerningFactor (0.08f));
        // (The names on one line, 14 px above the controls' line.)
        const auto labelY = inspector.getY() + 10 + 32 - 13 - 16;
        for (const auto& [area, name] : { std::pair<juce::Rectangle<int>, const char*> { powerArea, "ON" }, { soloArea, "SOLO" }, { bandArea, "BAND" } })
            if (! area.isEmpty())
                g.drawText (name, juce::Rectangle<int> (area.getX(), labelY, 60, 14), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        // The RACK page's frame: 8 px above and below, a 12 px gutter at the right.
        auto area = getLocalBounds().reduced (0, 8).withTrimmedRight (12);
        auto toolbar = area.removeFromTop (toolbarHeight);
        routingSeg = toolbar.removeFromLeft (232);
        seriesButton.setBounds (routingSeg.withTrimmedLeft (3).removeFromLeft (66).reduced (0, 2));
        parallelButton.setBounds (routingSeg.withTrimmedLeft (3 + 68).removeFromLeft (78).reduced (0, 2));
        patcherButton.setBounds (routingSeg.withTrimmedLeft (3 + 68 + 80).removeFromLeft (78).reduced (0, 2));
        toolbar.removeFromLeft (10);
        addButton.setBounds (toolbar.removeFromRight (112).reduced (0, 2));
        toolbar.removeFromRight (6);
        centreButton.setBounds (toolbar.removeFromRight (70).reduced (0, 2));
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

private:
    static constexpr int toolbarHeight = 28;
    static constexpr float maxZoom = 1.4f;

    static constexpr int inspectorHeight = 70;

    const PatchGraph::Node* selectedNode() const
    {
        const auto* node = shown.find (view.getSelectedNode());
        return node != nullptr && ! node->isFixed() && juce::isPositiveAndBelow (node->ref - 1, IlanaSynthAudioProcessor::numFxSlots) ? node : nullptr;
    }

    juce::String summaryText() const
    {
        auto effects = 0, merges = 0;
        for (const auto& node : shown.nodes())
        {
            if (! node.isFixed())
                ++effects;
            auto inputs = 0;
            for (const auto& cable : shown.cables())
                inputs += cable.to.node == node.id ? 1 : 0;
            merges += inputs > 1 ? 1 : 0;
        }
        const auto loops = shown.feedbackCables();
        const auto loopCount = (int) std::count (loops.begin(), loops.end(), true);
        juce::String text;
        text << effects << (effects == 1 ? " effect" : " effects");
        if (merges > 0)
            text << ", " << merges << (merges == 1 ? " mix point" : " mix points");
        if (loopCount > 0)
            text << ", " << loopCount << (loopCount == 1 ? " loop" : " loops");
        return text;
    }

    // The selected box's controls, attached to its slot's parameters.
    void inspect (int slot)
    {
        if (slot == inspected)
            return;
        inspected = slot;
        power.reset();
        mixKnob.reset();
        soloAttachment.reset();
        bandAttachment.reset();
        if (slot >= 0)
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
        }
        for (juce::Component* item : { (juce::Component*) &soloButton, (juce::Component*) &band, (juce::Component*) &editButton,
                                       (juce::Component*) &duplicateButton, (juce::Component*) &removeButton })
            item->setVisible (slot >= 0);
        layoutInspector();
        repaint (inspector);
    }

    // The card's row on the 10 px grid: name and detail, ON, SOLO, BAND,
    // MIX, then the actions at the right.
    void layoutInspector()
    {
        auto row = inspector.reduced (20, 10);
        inspectorText = row.removeFromLeft (juce::jmin (300, row.getWidth() / 3));
        powerArea = soloArea = bandArea = {};
        if (inspected < 0)
        {
            inspectorText = inspector.reduced (20, 10);
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
        // One label line above one control line (centred 32 px down the row).
        const auto centreY = row.getY() + 32;
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

    void duplicateEffect (int slot)
    {
        const auto* node = selectedNode();
        if (node == nullptr || node->ref - 1 != slot)
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

    juce::String hintText() const
    {
        if (! processorRef.isFxPatcherOn())
            return processorRef.getFxPatch().hasGraph ? "PATCHER is off, so the rack runs in order. Click PATCHER to play these cables again."
                                                      : "The rack as cables. Change a cable and PATCHER takes over the routing.";
        const auto loops = processorRef.getFxLoopLatencyMs();
        return loops > 0.0f ? "Drag an output to an input. Each loop adds " + juce::String (loops, 1) + " ms. Double-click an effect to edit it."
                            : "Drag an output to an input; cables into one input are summed. Double-click an effect to edit it.";
    }

    void setRouting (int mode)
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

    // A view edit: the graph goes to the processor (PATCHER on, if it wasn't:
    // a changed cable means the cables now decide). One undo step.
    void commit (PatchCableView::Edit edit)
    {
        const auto name = edit == PatchCableView::Edit::layout ? "Move effect" : "Patch cables";
        const auto graph = shown;
        processorRef.performEdit (name, [this, graph, edit]
        {
            if (edit != PatchCableView::Edit::layout && ! processorRef.isFxPatcherOn())
                processorRef.setFxPatcherOn (true);
            processorRef.setFxPatchGraph (graph);
        });
        refresh (true);
    }

    void toggleBypass (int slot)
    {
        if (auto* bypass = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
            processorRef.performEdit ("Bypass effect", [bypass] { bypass->setValueNotifyingHost (bypass->getValue() > 0.5f ? 0.0f : 1.0f); });
        refresh (true);
    }

    void removeEffect (int slot)
    {
        processorRef.performEdit ("Remove effect", [this, slot] { processorRef.removeFxEffect (slot); });
        refresh (true);
    }

    void openEffect (int slot)
    {
        if (onOpenSlot)
            onOpenSlot (slot);
    }

    void addNodeItems (juce::PopupMenu& menu, const PatchGraph::Node& node)
    {
        const auto slot = node.ref - 1;
        if (! juce::isPositiveAndBelow (slot, IlanaSynthAudioProcessor::numFxSlots))
            return;
        menu.addItem ("Edit settings", [this, slot] { openEffect (slot); });
        menu.addItem ("Duplicate (its own settings)", [this, slot] { duplicateEffect (slot); });
    }

    void addEffectItems (juce::PopupMenu& menu, juce::Point<float> at)
    {
        const auto full = [this]
        {
            for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                if (processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1))->load() < 0.5f)
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
    // before OUTPUT).
    void placeNode (int slot, juce::Point<float> at)
    {
        auto graph = processorRef.getFxPatchGraph();
        for (const auto& node : std::vector<PatchGraph::Node> (graph.nodes()))
            if (node.ref == slot + 1 && ! node.isFixed())
            {
                graph.moveNode (node.id, std::round (at.x / 10.0f) * 10.0f, std::round (at.y / 10.0f) * 10.0f);
                if (processorRef.isFxPatcherOn())
                    processorRef.setFxPatchGraph (graph);
            }
        refresh (true);
    }

    // The processor's graph, with each box named as the library names it
    // (numbered when the rack has two of one effect).
    void refresh (bool force)
    {
        juce::String signature;
        signature << (processorRef.isFxPatcherOn() ? "P" : "-") << processorRef.getPatchDataHash();
        for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        {
            const auto prefix = "fx_slot" + juce::String (slot + 1);
            signature << "," << (int) processorRef.apvts.getRawParameterValue (prefix)->load()
                      << (processorRef.apvts.getRawParameterValue (prefix + "_bypass")->load() > 0.5f ? "b" : "")
                      << (processorRef.apvts.getRawParameterValue (prefix + "_solo")->load() > 0.5f ? "s" : "");
        }
        signature << (processorRef.apvts.getRawParameterValue ("fx_routing")->load() > 0.5f ? "p" : "");

        const auto patcher = processorRef.isFxPatcherOn();
        const auto parallel = processorRef.apvts.getRawParameterValue ("fx_routing")->load() > 0.5f;
        seriesButton.setToggleState (! patcher && ! parallel, juce::dontSendNotification);
        parallelButton.setToggleState (! patcher && parallel, juce::dontSendNotification);
        patcherButton.setToggleState (patcher, juce::dontSendNotification);

        if (! force && signature == lastSignature)
            return;
        lastSignature = signature;

        shown = processorRef.getFxPatchGraph();
        std::array<int, 64> count {}, seen {};
        for (const auto& node : shown.nodes())
            if (! node.isFixed() && juce::isPositiveAndBelow (node.kind, 64))
                ++count[(size_t) node.kind];
        std::vector<PatchGraph::Node> ordered (shown.nodes().begin(), shown.nodes().end());
        std::sort (ordered.begin(), ordered.end(), [] (const auto& a, const auto& b) { return a.ref < b.ref; });
        for (const auto& node : ordered)
        {
            if (node.isFixed() || ! juce::isPositiveAndBelow (node.kind, 64))
                continue;
            auto label = fxTypeName (node.kind).toUpperCase();
            if (count[(size_t) node.kind] > 1)
                label << " " << ++seen[(size_t) node.kind];
            if (auto* editable = shown.find (node.id))
                editable->label = label.toStdString();
        }
        view.setLoopLatencyMs (juce::jmax (0.1f, (float) (64000.0 / juce::jmax (1.0, processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 48000.0))));
        view.graphChanged();
        // A new box or one outside the view: the patch is fitted again.
        if ((int) shown.nodes().size() != shownNodes || ! view.contentFits())
            view.fitContent (maxZoom);
        shownNodes = (int) shown.nodes().size();
        repaint (hint);
    }

    void timerCallback() override
    {
        // (Not under a drag: the view is editing its copy.)
        if (! isVisible() || juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            return;
        refresh (false);
        const auto* node = selectedNode();
        inspect (node != nullptr ? node->ref - 1 : -1);
        view.repaint(); // the CPU readouts
        repaint (inspector);
    }

    IlanaSynthAudioProcessor& processorRef;
    FxPage& rack;
    PatchGraph::Graph shown;
    PatchCableView view;
    juce::TextButton seriesButton { "SERIES" }, parallelButton { "PARALLEL" }, patcherButton { "PATCHER" };
    juce::TextButton addButton { "+ ADD EFFECT" }, centreButton { "FIT" };
    juce::Rectangle<int> routingSeg, hint, inspector, inspectorText, powerArea, soloArea, bandArea;
    int inspected = -1;
    std::unique_ptr<SlotSwitch> power;
    std::unique_ptr<KnobControl> mixKnob;
    juce::TextButton soloButton { "S" };
    juce::ComboBox band;
    std::unique_ptr<juce::ButtonParameterAttachment> soloAttachment;
    std::unique_ptr<juce::ComboBoxParameterAttachment> bandAttachment;
    juce::TextButton editButton { "EDIT SETTINGS" }, duplicateButton { "DUPLICATE" }, removeButton { "REMOVE" };
    juce::String lastSignature;
    int shownNodes = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPatcherPage)
};
} // namespace
