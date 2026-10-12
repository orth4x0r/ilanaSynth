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
// The selected loop cable's LEVEL and DELAY knobs (ilana 2026-10-11): the
// usual KnobControl on two parameters of their own, written into the cable.
class LoopKnobs : private juce::AudioProcessorParameter::Listener
{
public:
    explicit LoopKnobs (float shortestMs)
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "loop_level", 1 }, "Loop Level",
                                                                 juce::NormalisableRange<float> (0.0f, 70.0f, 1.0f), 20.0f,
                                                                 juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                     [] (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; })));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "loop_delay", 1 }, "Loop Delay",
                                                                 juce::NormalisableRange<float> (1.0f, PatchGraph::maxLoopDelayMs, 0.0f, 0.3f), 1.0f,
                                                                 juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                     [shortestMs] (float value, int)
                                                                     {
                                                                         const auto ms = juce::jmax (shortestMs, value);
                                                                         return ms < 10.0f ? juce::String (ms, 1) + " ms" : ms < 1000.0f ? juce::String (juce::roundToInt (ms)) + " ms"
                                                                                                                                        : juce::String (ms / 1000.0f, 2) + " s";
                                                                     })));
        state = std::make_unique<juce::AudioProcessorValueTreeState> (holder, nullptr, "Loop", std::move (layout));
        level = std::make_unique<KnobControl> (*state, "loop_level", "LEVEL", IlanaTheme::accent(), false);
        delay = std::make_unique<KnobControl> (*state, "loop_delay", "DELAY", IlanaTheme::accent(), false);
        level->setTooltip ("LEVEL: how much of the loop comes back each time round. Turn it down if the loop rings by itself.  Not modulatable.");
        delay->setTooltip ("DELAY: how long the loop takes to come round, from the shortest (one render chunk) to 1 s.  Not modulatable.");
        for (auto* knob : { level.get(), delay.get() })
            knob->setInlineKnob (true);
        for (const auto* id : { "loop_level", "loop_delay" })
            state->getParameter (id)->addListener (this);
    }

    ~LoopKnobs() override
    {
        // (The knobs hold attachments to the parameters: they go first.)
        level.reset();
        delay.reset();
        for (const auto* id : { "loop_level", "loop_delay" })
            state->getParameter (id)->removeListener (this);
    }

    // The cable's values onto the knobs (not heard back as an edit).
    void show (const PatchGraph::Cable& cable)
    {
        const juce::ScopedValueSetter<bool> quiet (syncing, true);
        set ("loop_level", cable.gain * 100.0f);
        set ("loop_delay", juce::jmax (1.0f, cable.delayMs));
    }

    std::function<void (float level, float delayMs)> onChange; // a turn (inside onGesture, or one step)
    std::function<void (bool)> onGesture;

    std::unique_ptr<KnobControl> level, delay;

private:
    void set (const char* id, float value)
    {
        auto* parameter = state->getParameter (id);
        const auto normalised = parameter->convertTo0to1 (value);
        if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
            parameter->setValueNotifyingHost (normalised);
    }

    float get (const char* id) const
    {
        const auto* parameter = state->getParameter (id);
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    void parameterValueChanged (int, float) override
    {
        if (! syncing && onChange)
            onChange (get ("loop_level") / 100.0f, get ("loop_delay"));
    }

    void parameterGestureChanged (int, bool starting) override
    {
        if (onGesture)
            onGesture (starting);
    }

    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    bool syncing = false;
};

// An effect's mini window on the PATCH tab (ilana, 2026-10-11: "open mini
// windowed versions of effects inside the patcher"): its picture and its
// card's controls, built here as copies on the same parameters (the RACK
// page keeps its own), so it can be edited without leaving the cables.
// Dragged by its title, closed with x; the page keeps it inside the canvas.
class FxMiniWindow : public juce::Component
{
public:
    FxMiniWindow (IlanaSynthAudioProcessor& p, FxPage& rack, int slotIndex, int fxType, const juce::String& title)
        : slot (slotIndex), type (fxType), name (title), colour (fxColour (fxType)), power (p, slotIndex), display (p)
    {
        setName ("FX WINDOW " + title); // (the UI test finds it by name)
        constrainer.owner = this;
        power.setBare (true);
        addAndMakeVisible (power);
        closeButton.setTooltip ("Close this window");
        closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        closeButton.onClick = [this] { if (onClose) onClose (*this); };
        addAndMakeVisible (closeButton);
        if (FxDisplay::hasDisplay (type))
        {
            display.setType (type, colour);
            addAndMakeVisible (display);
        }

        // A copy of an effect whose settings aren't the ones in front (its
        // own, kept in the FX patch): the controls work on a shadow of them,
        // as its card on the RACK page does (FxInstanceControls.h).
        if (! p.isFxSlotInFront (slot))
        {
            std::vector<juce::String> ids;
            for (const auto& entry : p.getFxPatch().slots[(size_t) slot].values)
                ids.push_back (entry.first);
            shadow = std::make_unique<FxInstance::Shadow> (p, slot, type, ids);
            display.setValueSource (&shadow->getState());
        }
        auto& state = shadow != nullptr ? shadow->getState() : p.apvts;
        for (auto* item : rack.windowControls (type))
        {
            std::unique_ptr<juce::Component> copy;
            if (auto* knob = dynamic_cast<KnobControl*> (item); knob != nullptr && state.getParameter (knob->getParameterId()) != nullptr)
            {
                auto made = std::make_unique<KnobControl> (state, knob->getParameterId(), knob->getLabelText(), colour, false);
                made->setInlineKnob (true);
                copy = std::move (made);
            }
            else if (auto* toggle = dynamic_cast<ToggleControl*> (item); toggle != nullptr && state.getParameter (toggle->getParameterId()) != nullptr)
            {
                copy = std::make_unique<ToggleControl> (state, toggle->getParameterId(), toggle->getButton().getButtonText());
            }
            else if (auto* combo = dynamic_cast<ComboControl*> (item); combo != nullptr && state.getParameter (combo->getParameterId()) != nullptr)
            {
                copy = std::make_unique<ComboControl> (state, combo->getParameterId(), combo->getLabelText());
            }
            if (copy != nullptr)
            {
                addAndMakeVisible (*copy);
                controls.push_back (std::move (copy));
            }
        }
        if (const auto mixName = rack.windowMixName (slot, type); mixName.isNotEmpty())
        {
            auto mix = std::make_unique<KnobControl> (p.apvts, "fx_slot" + juce::String (slot + 1) + "_mix", mixName, colour, false);
            mix->setInlineKnob (true);
            addAndMakeVisible (*mix);
            controls.push_back (std::move (mix));
        }

        const auto count = (int) controls.size();
        // (At most three across: a window is a small thing beside its box.)
        columns = juce::jlimit (1, 3, count == 4 ? 2 : count);
        rows = juce::jmax (1, (count + columns - 1) / columns);
        const auto width = juce::jmax (300, columns * cellWidth + (columns - 1) * gap + 2 * pad);
        const auto height = titleHeight + (display.isVisible() ? displayHeight + gap : 0) + rows * cellHeight + (rows - 1) * gap + pad;
        setSize (width, height);
    }

    ~FxMiniWindow() override
    {
        // (The controls hold attachments to the shadow's parameters: they go first.)
        display.setValueSource (nullptr);
        controls.clear();
    }

    const int slot, type;
    std::function<void (FxMiniWindow&)> onClose, onMoved;

    // Whether its controls are on a copy's own settings (or on the front ones).
    bool isOwnSettings() const { return shadow != nullptr; }

    // Changes made elsewhere (its RACK card, undo) onto a copy's controls.
    void refresh()
    {
        if (shadow != nullptr)
            shadow->refresh();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f).reduced (2.0f), 10.0f);
        paintCard (g, bounds.reduced (1.0f), 10.0f, colour.withAlpha (0.55f));
        auto title = getLocalBounds().removeFromTop (titleHeight).reduced (pad, 0);
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getCentreY() }, colour);
        g.setColour (Ui::text);
        g.setFont (font (TextSize::body, true));
        g.drawText (name, title.withTrimmedLeft (16).withTrimmedRight (100), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto title = area.removeFromTop (titleHeight).reduced (pad, 0);
        closeButton.setBounds (title.removeFromRight (28).withSizeKeepingCentre (28, 28));
        title.removeFromRight (10);
        power.setBounds (title.removeFromRight (40).withSizeKeepingCentre (40, 20));
        area = area.withTrimmedLeft (pad).withTrimmedRight (pad).withTrimmedBottom (pad);
        if (display.isVisible())
        {
            display.setBounds (area.removeFromTop (displayHeight));
            area.removeFromTop (gap);
        }
        for (size_t k = 0; k < controls.size(); ++k)
        {
            const auto column = (int) k % columns, row = (int) k / columns;
            controls[k]->setBounds (area.getX() + column * (cellWidth + gap), area.getY() + row * (cellHeight + gap), cellWidth, cellHeight);
        }
    }

    // Dragged by its title bar.
    void mouseDown (const juce::MouseEvent& e) override
    {
        toFront (false);
        dragging = e.y < titleHeight;
        if (dragging)
            dragger.startDraggingComponent (this, e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging)
            dragger.dragComponent (this, e, &constrainer);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && onMoved)
            onMoved (*this);
        dragging = false;
    }

    // Keeps it inside this area of its parent (the canvas).
    void setLimits (juce::Rectangle<int> area)
    {
        limits = area;
        constrainer.setMinimumOnscreenAmounts (getHeight(), getWidth(), getHeight(), getWidth());
        setBounds (getBounds().constrainedWithin (limits));
    }

private:
    static constexpr int pad = 12, gap = 8, titleHeight = 40, displayHeight = 64, cellWidth = 116, cellHeight = 44;

    struct Constrainer : juce::ComponentBoundsConstrainer
    {
        FxMiniWindow* owner = nullptr;
        void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>&, const juce::Rectangle<int>&, bool, bool, bool, bool) override
        {
            if (owner != nullptr && ! owner->limits.isEmpty())
                bounds = bounds.constrainedWithin (owner->limits);
        }
    };

    juce::String name;
    juce::Colour colour;
    SlotSwitch power;
    FxDisplay display;
    std::unique_ptr<FxInstance::Shadow> shadow;
    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::vector<std::unique_ptr<juce::Component>> controls;
    int columns = 1, rows = 1;
    juce::ComponentDragger dragger;
    Constrainer constrainer;
    juce::Rectangle<int> limits;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxMiniWindow)
};

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
        view.loopGuardOf = [this] (const PatchGraph::Cable& cable)
        {
            return processorRef.isFxPatcherOn() ? processorRef.getFxLoopGuard (cable.from.node, cable.from.port, cable.to.node, cable.to.port) : 1.0f;
        };
        view.onGesture = [this] (bool starting) { loopGesture (starting, "Loop level"); };
        addAndMakeVisible (view);

        // The selected loop's own row: LEVEL and DELAY, and remove.
        loopKnobs = std::make_unique<LoopKnobs> (shortestLoopMs());
        loopKnobs->onGesture = [this] (bool starting) { loopGesture (starting, "Loop settings"); };
        loopKnobs->onChange = [this] (float level, float delayMs) { setLoop (level, delayMs); };
        addChildComponent (*loopKnobs->level);
        addChildComponent (*loopKnobs->delay);
        removeCableButton.setTooltip ("Take this loop cable out");
        removeCableButton.onClick = [this]
        {
            if (const auto* cable = selectedLoop())
            {
                shown.disconnect (cable->from, cable->to);
                commit (PatchCableView::Edit::cables);
            }
        };
        removeCableButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        removeCableButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        addChildComponent (removeCableButton);

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

    ~PatchPage() override
    {
        if (loopGestureOpen)
            processorRef.endEdit();
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
        for (auto& window : windows)
            window->setLimits (view.getBounds());
    }

    // Tests: the graph the view shows, and the voice's cables as a patch.
    const PatchGraph::Graph& getShownGraph() const { return shown; }
    int getNumWindows() const { return (int) windows.size(); }
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
            return "Chain and rack as cables. Edit one to play them.";
        const auto loops = processorRef.getFxLoopLatencyMs();
        if (loops > 0.0f)
            return "Drag output to input. Drag a loop's tag for its level.";
        return "Drag output to input. More cables play in parallel.";
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
        // The rest, after VOICES, hears every note summed: framed the same way.
        const auto content = view.getContentArea().translated (view.getOrigin().x, view.getOrigin().y);
        if (content.getRight() > band.getRight() + 40.0f)
        {
            const auto all = juce::Rectangle<float> (band.getRight() + 10.0f, band.getY(), content.getRight() + 20.0f - band.getRight() - 10.0f, band.getHeight());
            g.setColour (Ui::line2.withAlpha (0.2f));
            g.fillRoundedRectangle (all, 12.0f);
            g.setColour (Ui::line.withAlpha (0.7f));
            g.drawRoundedRectangle (all.reduced (0.5f), 12.0f, 1.0f);
            g.setColour (Ui::text3);
            g.drawText ("ALL NOTES", all.reduced (16.0f, 10.0f).removeFromTop (14.0f), juce::Justification::centredLeft, false);
        }
        else
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
        if (const auto* loop = selectedLoop())
        {
            // A loop: where it runs, and whether the guard is holding it.
            const auto* from = shown.find (loop->from.node);
            const auto* to = shown.find (loop->to.node);
            const auto colour = from != nullptr ? colourOf (*from) : Ui::text2;
            auto title = text.removeFromTop (text.getHeight() / 2);
            paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colour);
            g.setColour (Ui::text);
            g.setFont (font (TextSize::large, true));
            const auto name = [] (const PatchGraph::Node* box) { return box != nullptr ? juce::String (box->label) : juce::String ("?"); };
            drawFitted (g, "LOOP  " + name (from) + " > " + name (to), title.withTrimmedLeft (16), juce::Justification::bottomLeft, 1, TextSize::minPassive);
            const auto ringing = view.loopGuardOf && view.loopGuardOf (*loop) < 0.98f;
            g.setColour (ringing ? accent() : Ui::text3);
            g.setFont (font (TextSize::label));
            g.drawText (ringing ? "Ringing by itself: held at 0 dB. Turn LEVEL down to tame it."
                                : "Feeds " + name (from) + " back into " + name (to) + ".",
                        text.withTrimmedTop (4).withTrimmedLeft (16), juce::Justification::topLeft, true);
            return;
        }
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
            // Nothing selected: the patch in one line, and how to start
            // (left of the stats at the right).
            text = text.withTrimmedRight (hasVoice ? 470 : 320);
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
            g.drawText ("Click a box or a loop's tag to set it here; double-click a box to open it. Right-click to add an effect.",
                        text.withTrimmedTop (4), juce::Justification::topLeft, true);

            // At the right: how the patch is wired and what its effects cost.
            auto stats = inspector.reduced (20, 10).removeFromRight (450);
            auto cpu = 0.0f;
            for (const auto& box : fxShown.nodes())
                if (! box.isFixed() && juce::isPositiveAndBelow (box.ref - 1, IlanaSynthAudioProcessor::numFxSlots))
                    cpu += processorRef.getFxSlotCpu (box.ref - 1);
            const auto loops = shown.feedbackCables();
            auto loopCount = 0, ringing = 0;
            for (size_t c = 0; c < loops.size(); ++c)
                if (loops[c])
                {
                    ++loopCount;
                    ringing += view.loopGuardOf && view.loopGuardOf (shown.cables()[c]) < 0.98f ? 1 : 0;
                }
            const auto guard = loopCount == 0 ? juce::String ("no loops") : ringing == 0 ? juce::String ("idle")
                                                                                         : juce::String ("holding ") + juce::String (ringing) + (ringing == 1 ? " loop" : " loops");
            std::vector<std::pair<juce::String, juce::String>> cells {
                { "EFFECTS CPU", juce::String (cpu * 100.0f, 2) + " %" },
                { "LOOP GUARD", guard }
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
        const auto loop = selectedLoop() != nullptr;
        for (juce::Component* item : { (juce::Component*) loopKnobs->level.get(), (juce::Component*) loopKnobs->delay.get(),
                                       (juce::Component*) &removeCableButton })
            item->setVisible (loop);
        if (loop)
        {
            // Name and detail, LEVEL, DELAY (the MIX knob's size), REMOVE CABLE at the right.
            auto actions = row.removeFromRight (340);
            removeCableButton.setBounds (actions.removeFromRight (130).withSizeKeepingCentre (130, 28));
            row.removeFromLeft (20);
            loopKnobs->level->setBounds (row.removeFromLeft (120).withSizeKeepingCentre (116, 44));
            row.removeFromLeft (20);
            loopKnobs->delay->setBounds (row.removeFromLeft (120).withSizeKeepingCentre (116, 44));
            return;
        }
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
            editButton.setButtonText ("OPEN WINDOW");
            editButton.setTooltip ("Open this effect's controls in a small window over the cables (double-click its box does too)");
        }
        layoutInspector();
        repaint (inspector);
    }

    //==========================================================================
    // Edits.
    // The graph fills what the open windows leave free: tighter margins and
    // 5 % zoom steps than the default fit, so it keeps its size (R17-2).
    void fit() { view.fitContentInto (freeArea(), maxZoom, minZoom, 60.0f, 90.0f, 0.05f); }

    juce::Rectangle<int> freeArea() const
    {
        auto free = view.getLocalBounds();
        for (const auto& window : windows)
            if (window->isVisible())
            {
                const auto at = window->getBounds().translated (-view.getX(), -view.getY());
                // (Only one docked at the right edge; a window dragged elsewhere floats.)
                if (at.getRight() >= free.getRight() - 2 && at.getX() > free.getX() + 300)
                    free.setRight (at.getX() - 10);
            }
        return free;
    }

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
            {
                fx.connect (cable.from, cable.to, cable.gain);
                fx.setCableDelay (fx.findCable (cable.from, cable.to), cable.delayMs);
            }
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
            if (index < 0 || b.cables()[(size_t) index].gain != cable.gain || b.cables()[(size_t) index].delayMs != cable.delayMs)
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
        if (edit == PatchCableView::Edit::loop && loopGestureOpen)
        {
            // (Live while a loop's tag or knob turns; the gesture is one undo step.)
            if (! processorRef.isFxPatcherOn())
                processorRef.setFxPatcherOn (true);
            processorRef.setFxPatchGraph (fx);
            fxShown = fx;
            nameEffects (fxShown);
            view.repaint();
            return;
        }
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
        processorRef.performEdit (edit == PatchCableView::Edit::loop ? "Loop settings" : "Patch cables", [this, fx, voice, voiceChanged, fxChanged]
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

    // The loop cable the view has selected, or null.
    const PatchGraph::Cable* selectedLoop() const
    {
        const auto index = view.getSelectedCable();
        if (! juce::isPositiveAndBelow (index, (int) shown.cables().size()) || ! shown.feedbackCables()[(size_t) index])
            return nullptr;
        return &shown.cables()[(size_t) index];
    }

    float shortestLoopMs() const
    {
        return juce::jmax (0.1f, (float) (64000.0 / juce::jmax (1.0, processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 48000.0)));
    }

    void loopGesture (bool starting, const juce::String& name)
    {
        if (starting && ! loopGestureOpen)
        {
            loopGestureOpen = true;
            processorRef.beginEdit (name);
        }
        else if (! starting && loopGestureOpen)
        {
            loopGestureOpen = false;
            processorRef.endEdit();
            refresh (true);
        }
    }

    // A turn of the selected loop's LEVEL or DELAY knob.
    void setLoop (float level, float delayMs)
    {
        const auto index = view.getSelectedCable();
        if (selectedLoop() == nullptr)
            return;
        shown.setCableGain (index, juce::jlimit (0.0f, view.maxLoopLevel, level));
        shown.setCableDelay (index, delayMs <= shortestLoopMs() ? 0.0f : delayMs);
        commit (PatchCableView::Edit::loop);
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
        else if (const auto slot = slotOf (&node); slot >= 0)
        {
            openWindow (slot);
        }
    }

    //==========================================================================
    // Mini windows: at most four, one per box (a copy of an effect with its
    // own settings gets controls on those, not on the front ones).
    static constexpr size_t maxWindows = 4;

    void openWindow (int slot)
    {
        const PatchGraph::Node* node = nullptr;
        for (const auto& box : shown.nodes())
            if (slotOf (&box) == slot)
                node = &box;
        if (node == nullptr)
            return;
        for (auto& window : windows)
            if (window->slot == slot)
            {
                window->toFront (false);
                return;
            }
        const auto type = node->kind;
        const auto label = juce::String (node->label);
        if (std::count_if (windows.begin(), windows.end(), [] (const auto& w) { return w->isVisible(); }) >= (std::ptrdiff_t) maxWindows)
            for (auto& window : windows)
                if (window->isVisible())
                {
                    closeWindow (window.get());
                    break;
                }

        auto window = std::make_unique<FxMiniWindow> (processorRef, rack, slot, type, label);
        window->onClose = [this] (FxMiniWindow& closing) { closeWindow (&closing); };
        // Beside the box where it fits the canvas and covers no other
        // window: right of it, left, below, above; else right of it.
        const auto box = (view.nodeBounds (*node) * view.getZoom()).toNearestInt() + view.getPosition();
        const auto size = window->getLocalBounds();
        std::vector<juce::Rectangle<int>> places {
            size.withPosition (box.getRight() + 20, box.getY()),
            size.withPosition (box.getX() - 20 - size.getWidth(), box.getY()),
            size.withPosition (box.getCentreX() - size.getWidth() / 2, box.getBottom() + 20),
            size.withPosition (box.getCentreX() - size.getWidth() / 2, box.getY() - 20 - size.getHeight())
        };
        // (Or beside a window already open, so they tile rather than stack.)
        for (const auto& other : windows)
            if (other->isVisible())
            {
                const auto at = other->getBounds();
                places.push_back (size.withPosition (at.getX() - 10 - size.getWidth(), at.getY()));
                places.push_back (size.withPosition (at.getRight() + 10, at.getY()));
                places.push_back (size.withPosition (at.getX(), at.getBottom() + 10));
                places.push_back (size.withPosition (at.getX(), at.getY() - 10 - size.getHeight()));
            }
        // Docked first: against the canvas's right edge while that leaves the
        // graph room; the graph refits into what is left (R17-3), so a window
        // no longer covers its own boxes. Further windows tile as before.
        std::vector<juce::Rectangle<int>> docks { size.withPosition (view.getRight() - size.getWidth(), view.getY()) };
        auto docked = false;
        for (const auto& dock : docks)
        {
            auto clear = view.getBounds().contains (dock) && (float) (dock.getX() - view.getX()) >= view.getContentArea().getWidth() * 0.8f + 60.0f;
            for (const auto& other : windows)
                clear = clear && (! other->isVisible() || ! other->getBounds().intersects (dock));
            if (clear)
            {
                places.insert (places.begin(), dock);
                docked = true;
                break;
            }
        }

        // The one covering the least: another window counts far more than the
        // boxes (which stay reachable round it); a place that fits as it is
        // wins a tie.
        auto place = places[0].constrainedWithin (view.getBounds());
        auto least = std::numeric_limits<juce::int64>::max();
        for (const auto& candidate : places)
        {
            const auto inside = candidate.constrainedWithin (view.getBounds());
            juce::int64 covered = view.getBounds().contains (candidate) ? 0 : 1;
            for (const auto& other : windows)
                if (other->isVisible())
                {
                    const auto overlap = other->getBounds().getIntersection (inside);
                    covered += 100 * (juce::int64) overlap.getWidth() * overlap.getHeight();
                }
            for (const auto& node : shown.nodes())
            {
                const auto overlap = ((view.nodeBounds (node) * view.getZoom()).toNearestInt() + view.getPosition()).getIntersection (inside);
                covered += (juce::int64) overlap.getWidth() * overlap.getHeight();
            }
            if (covered < least)
            {
                least = covered;
                place = inside;
            }
        }
        window->setBounds (docked ? places.front() : place);
        addAndMakeVisible (*window);
        window->setLimits (view.getBounds());
        windows.push_back (std::move (window));
        if (docked)
            fit();
    }

    // (Deferred: a window closes from its own button's click.)
    void closeWindow (FxMiniWindow* window)
    {
        if (window == nullptr)
            return;
        window->setVisible (false);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PatchPage> (this), window]
        {
            if (safe == nullptr)
                return;
            auto& list = safe->windows;
            list.erase (std::remove_if (list.begin(), list.end(), [window] (const auto& w) { return w.get() == window; }), list.end());
            safe->fit();
        });
    }

    // A window whose slot changed effect or was emptied closes.
    void checkWindows()
    {
        // (So does one whose settings moved to or from the front: its
        // controls would be on the wrong ones.)
        for (auto& window : windows)
            if (window->isVisible())
            {
                if (juce::roundToInt (read ("fx_slot" + juce::String (window->slot + 1))) != window->type
                    || window->isOwnSettings() == processorRef.isFxSlotInFront (window->slot))
                    closeWindow (window.get());
                else
                    window->refresh();
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
        menu.addItem ("Open its window", [this, slot] { openWindow (slot); });
        menu.addItem ("Edit on the RACK page", [this, slot] { if (onOpenSlot) onOpenSlot (slot); });
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
        {
            graph.connect (cable.from, cable.to, cable.gain);
            graph.setCableDelay (graph.findCable (cable.from, cable.to), cable.delayMs);
        }
        if (hasVoice)
            connectVoice (graph, patch, sources);

        shown = graph;
        view.setLoopLatencyMs (shortestLoopMs());
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
        const auto* loop = selectedLoop();
        if (loop != nullptr && ! loopGestureOpen)
            loopKnobs->show (*loop);
        if ((loop != nullptr) != loopKnobs->level->isVisible())
            layoutInspector();
        checkWindows();
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

    std::unique_ptr<LoopKnobs> loopKnobs;
    juce::TextButton removeCableButton { "REMOVE CABLE" };
    bool loopGestureOpen = false;

    std::vector<std::unique_ptr<FxMiniWindow>> windows;
    juce::String lastSignature, lastState, refusal;
    juce::uint32 refusedUntil = 0;
    int shownNodes = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchPage)
};
} // namespace
