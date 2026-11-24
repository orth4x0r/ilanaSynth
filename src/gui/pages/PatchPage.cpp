#include "PatchPage.h"

#include "PageHelpers.h"

LoopKnobs::LoopKnobs (float shortestMs)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "loop_level", 1 }, "Loop Level",
                                                             juce::NormalisableRange<float> (0.0f, 70.0f, 1.0f), 20.0f,
                                                             juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                 [] (float value, int) { return juce::String (juce::roundToInt (value)) + "%"; })));
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

LoopKnobs::~LoopKnobs ()
{
    // (The knobs hold attachments to the parameters: they go first.)
    level.reset();
    delay.reset();
    for (const auto* id : { "loop_level", "loop_delay" })
        state->getParameter (id)->removeListener (this);
}

void LoopKnobs::show (const PatchGraph::Cable& cable)
{
    const juce::ScopedValueSetter<bool> quiet (syncing, true);
    set ("loop_level", cable.gain * 100.0f);
    set ("loop_delay", juce::jmax (1.0f, cable.delayMs));
}

void LoopKnobs::set (const char* id, float value)
{
    auto* parameter = state->getParameter (id);
    const auto normalised = parameter->convertTo0to1 (value);
    if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
        parameter->setValueNotifyingHost (normalised);
}

float LoopKnobs::get (const char* id) const
{
    const auto* parameter = state->getParameter (id);
    return parameter->convertFrom0to1 (parameter->getValue());
}

void LoopKnobs::parameterValueChanged (int, float)
{
    if (! syncing && onChange)
        onChange (get ("loop_level") / 100.0f, get ("loop_delay"));
}

void LoopKnobs::parameterGestureChanged (int, bool starting)
{
    if (onGesture)
        onGesture (starting);
}

FxMiniWindow::FxMiniWindow (IlanaSynthAudioProcessor& p, FxPage& rack, int slotIndex, int fxType, const juce::String& title)
    : slot (slotIndex), type (fxType), processorRef (p), name (title), colour (fxColour (fxType)), power (p, slotIndex), display (p)
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
        display.setSlot (slot);
        display.setType (type, colour);
        addAndMakeVisible (display);
    }

    // A copy of an effect whose settings aren't the ones in front (its
    // own, kept in the FX patch): the controls work on a shadow of them,
    // as its card on the RACK page does (FxInstanceControls.h).
    if (! p.isFxSlotInFront (slot))
    {
        std::vector<juce::String> ids;
        const auto patch = p.getFxPatch(); // (a copy: a range-for over the temporary's member read freed memory)
        for (const auto& entry : patch.slots[(size_t) slot].values)
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
    updateDimming();
}

FxMiniWindow::~FxMiniWindow ()
{
    // (The controls hold attachments to the shadow's parameters: they go first.)
    display.setValueSource (nullptr);
    controls.clear();
}

void FxMiniWindow::setLfoHooks (std::function<void (juce::PopupMenu&, const juce::String&)> menu, std::function<juce::Colour (const juce::String&)> marker)
{
    lfoMarker = std::move (marker);
    const auto mixId = "fx_slot" + juce::String (slot + 1) + "_mix";
    for (auto& control : controls)
        if (auto* knob = dynamic_cast<KnobControl*> (control.get()))
        {
            const auto param = knob->getParameterId() == mixId ? FxPatchData::mixParam : knob->getParameterId();
            knob->extraMenu = [menu, param] (juce::PopupMenu& items) { if (menu) menu (items, param); };
        }
    repaint();
}

void FxMiniWindow::paintOverChildren (juce::Graphics& g)
{
    if (! lfoMarker)
        return;
    const auto mixId = "fx_slot" + juce::String (slot + 1) + "_mix";
    for (auto& control : controls)
        if (auto* knob = dynamic_cast<KnobControl*> (control.get()))
        {
            const auto colour = lfoMarker (knob->getParameterId() == mixId ? FxPatchData::mixParam : knob->getParameterId());
            if (colour.isTransparent())
                continue;
            const auto at = knob->getBounds().toFloat();
            // (On the dial's top right corner, by the knob it moves.)
            const auto dial = juce::jmin (at.getHeight(), at.getWidth());
            const auto dot = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ at.getX() + dial * 0.86f, at.getY() + dial * 0.14f });
            g.setColour (colour);
            g.fillEllipse (dot);
            g.setColour (IlanaTheme::Ui::panel);
            g.drawEllipse (dot, 1.0f);
        }
}

void FxMiniWindow::refresh ()
{
    if (shadow != nullptr)
        shadow->refresh();
    updateDimming();
}

void FxMiniWindow::paint (juce::Graphics& g)
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
    g.drawText (name, title.withTrimmedLeft (Space::s16).withTrimmedRight (100), juce::Justification::centredLeft, true); // off-grid
}

void FxMiniWindow::resized ()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (titleHeight).reduced (pad, 0);
    closeButton.setBounds (title.removeFromRight (28).withSizeKeepingCentre (28, 28)); // off-grid
    title.removeFromRight (10); // off-grid
    power.setBounds (title.removeFromRight (40).withSizeKeepingCentre (40, 20)); // off-grid
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

void FxMiniWindow::mouseDown (const juce::MouseEvent& e)
{
    toFront (false);
    dragging = e.y < titleHeight;
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void FxMiniWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        dragger.dragComponent (this, e, &constrainer);
}

void FxMiniWindow::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onMoved)
        onMoved (*this);
    dragging = false;
}

void FxMiniWindow::setLimits (juce::Rectangle<int> area)
{
    limits = area;
    constrainer.setMinimumOnscreenAmounts (getHeight(), getWidth(), getHeight(), getWidth());
    setBounds (getBounds().constrainedWithin (limits));
}

void FxMiniWindow::updateDimming ()
{
    auto& state = shadow != nullptr ? shadow->getState() : processorRef.apvts;
    const auto* bypass = processorRef.apvts.getRawParameterValue ("fx_slot" + juce::String (slot + 1) + "_bypass");
    auto off = bypass != nullptr && bypass->load() > 0.5f;
    if (const auto* id = FxPage::enableParamFor (type))
        if (const auto* value = state.getRawParameterValue (id))
            off = off || value->load() < 0.5f;

    const auto alpha = off ? FilterColours::offAlpha : 1.0f;
    if (display.getAlpha() != alpha)
        display.setAlpha (alpha);
    for (auto& control : controls)
    {
        auto itemAlpha = alpha;
        if (! off && FxPage::isIdleControl (type, FxPage::parameterIdOf (control.get()), state))
            itemAlpha = IlanaTheme::dimmedAlpha;
        if (control->getAlpha() != itemAlpha)
            control->setAlpha (itemAlpha);
    }
}

PatchPage::PatchPage (IlanaSynthAudioProcessor& p, FxPage& rackPage)
    : processorRef (p), rack (rackPage), view (shown), hasVoice (! IlanaSynthAudioProcessor::isEffectBuild)
{
    addButton.setTooltip ("Add an effect: it is wired in just before OUT. One the rack has already is a new instance with its own settings.");
    addButton.onClick = [this] { rack.openLibrary (addButton); };
    lfoButton.setTooltip ("Add an LFO box. Cable it to an effect, then right-click a knob in that effect's window to assign it.");
    lfoButton.onClick = [this] { addLfo (std::nullopt); };
    addAndMakeVisible (lfoButton);
    resetButton.setTooltip ("The oscillator and filter cables back to the standard routing (each oscillator's route, "
                            "SERIAL / PARALLEL and WEST's position, as PLAY sets them)");
    resetButton.onClick = [this] { resetVoiceToChain(); };
    fitButton.setTooltip ("Fit the whole patch in view (F, or double-click empty space)");
    fitButton.onClick = [this] { fit (true); };
    // The zoom: out, the percentage (click: 100 %), in.
    const auto navigation = juce::String ("Scroll or drag empty space to move around; pinch or ctrl + scroll to zoom. "
                                          "Keys: F fits, 0 is 100 %, + and - zoom.");
    zoomOutButton.setTooltip ("Zoom out (-)\n" + navigation);
    zoomInButton.setTooltip ("Zoom in (+)\n" + navigation);
    zoomLabel.setTooltip ("Back to 100 % (0)\n" + navigation);
    zoomOutButton.setDescription ("Zoom out");
    zoomInButton.setDescription ("Zoom in");
    zoomOutButton.onClick = [this] { view.zoomStep (-1); };
    zoomInButton.onClick = [this] { view.zoomStep (1); };
    zoomLabel.onClick = [this] { view.zoomToActualSize(); };
    for (auto* quiet : { &resetButton, &fitButton, &zoomOutButton, &zoomInButton, &zoomLabel })
    {
        quiet->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        quiet->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
    }
    for (auto* inWell : { &zoomOutButton, &zoomInButton, &zoomLabel })
        inWell->setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    addAndMakeVisible (addButton);
    addAndMakeVisible (fitButton);
    for (auto* zoomControl : { &zoomOutButton, &zoomLabel, &zoomInButton })
        addAndMakeVisible (*zoomControl);
    if (hasVoice)
        addAndMakeVisible (resetButton);

    view.setName ("PATCH"); // (the UI tests find it by name)
    view.colourOf = [] (const PatchGraph::Node& node) { return isLfo (node) ? fxLfoColour (lfoOf (node)) : colourOf (node); };
    view.isModSource = [] (const PatchGraph::Node& node) { return isLfo (node); };
    view.modTagOf = [this] (const PatchGraph::Cable& cable) { return modTagOf (cable); };
    view.decorateNode = [this] (juce::Graphics& g, const PatchGraph::Node& node, juce::Rectangle<float> box) { decorateLfo (g, node, box); };
    view.bypassable = [] (const PatchGraph::Node& node) { return ! isLfo (node); };
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
        return from != nullptr && ! isVoice (*from) && ! isLfo (*from);
    };
    view.acceptGraph = [this] (const PatchGraph::Graph& graph) { return accept (graph); };
    view.onRefused = [this]
    {
        refusedUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint (hint);
    };
    view.onConnectRefused = [this] (PatchGraph::ConnectResult result)
    {
        refusal = result == PatchGraph::ConnectResult::duplicate ? "Those two are already joined by a cable."
                                                                 : "That's the most cables the patch can hold.";
        refusedUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint (hint);
    };
    view.onEdited = [this] (PatchCableView::Edit edit) { commit (edit); };
    view.onBypass = [this] (const PatchGraph::Node& node) { toggleBypass (slotOf (&node)); };
    view.onRemove = [this] (const PatchGraph::Node& node)
    {
        if (isLfo (node))
            return removeLfo (lfoOf (node));
        removeEffect (slotOf (&node));
    };
    view.onOpen = [this] (const PatchGraph::Node& node) { open (node); };
    view.onCopy = [this] (const std::vector<PatchGraph::NodeId>& ids) { copyEffects (ids); };
    view.onPaste = [this] (juce::Point<float> at) { pasteEffects (at); };
    view.onRemoveMany = [this] (const std::vector<PatchGraph::NodeId>& ids) { removeMany (ids); };
    view.nodeMenu = [this] (juce::PopupMenu& menu, const PatchGraph::Node& node) { addNodeItems (menu, node); };
    view.addMenu = [this] (juce::PopupMenu& menu, juce::Point<float> at) { addEffectItems (menu, at); };
    view.paintBackdrop = [this] (juce::Graphics& g) { paintVoiceBand (g); };
    view.loopGuardOf = [this] (const PatchGraph::Cable& cable)
    {
        return processorRef.isFxPatcherOn() ? processorRef.getFxLoopGuard (cable.from.node, cable.from.port, cable.to.node, cable.to.port) : 1.0f;
    };
    view.onGesture = [this] (bool starting) { loopGesture (starting, "Loop level"); };
    // The user moved the view: it stays where they put it (FIT hands it back).
    view.onNavigated = [this] { autoFit = false; };
    view.onFitRequested = [this] { fit (true); };
    // Only effects drop into cables, and only after VOICES (accept() vets the rest).
    view.canSplice = [this] (const PatchGraph::Node& node, const PatchGraph::Cable& cable)
    {
        // (Not a voice box, nor an LFO box, nor into an LFO's cable.)
        const auto* from = shown.find (cable.from.node);
        return ! isVoice (node) && ! isLfo (node) && (from == nullptr || ! isLfo (*from));
    };
    addAndMakeVisible (view);

    // The row of effects under the canvas: a click adds one, a drag picks
    // it up onto the canvas (the box is added under the pointer and the
    // canvas takes the drag from there).
    carousel.isFull = [this] { return rackFull(); };
    carousel.onPick = [this] (const FxCarousel::Tile& tile) { addFromCarousel (tile, std::nullopt); };
    carousel.onGrab = [this] (const FxCarousel::Tile& tile, juce::Point<float> at)
    {
        const auto where = view.graphPointAt (view.getLocalPoint (&carousel, at)) - juce::Point<float> (PatchCableView::nodeWidth, PatchCableView::nodeHeight) * 0.5f;
        return addFromCarousel (tile, where);
    };
    carousel.onGrabDrag = [this] (const juce::MouseEvent& e) { view.mouseDrag (e.getEventRelativeTo (&view)); };
    carousel.onGrabEnd = [this] (const juce::MouseEvent& e)
    {
        view.mouseUp (e.getEventRelativeTo (&view));
        carouselDrag = false;
        if (autoFit && ! view.contentFits())
            fit (true);
    };
    addAndMakeVisible (carousel);

    // The selected loop's own row: LEVEL and DELAY, and remove.
    loopKnobs = std::make_unique<LoopKnobs> (shortestLoopMs());
    loopKnobs->onGesture = [this] (bool starting) { loopGesture (starting, "Loop settings"); };
    loopKnobs->onChange = [this] (float level, float delayMs) { setLoop (level, delayMs); };
    addChildComponent (*loopKnobs->level);
    addChildComponent (*loopKnobs->delay);
    removeCableButton.setTooltip ("Take this loop cable out");
    removeCableButton.onClick = [this]
    {
        if (const auto* cable = selectedLoop() != nullptr ? selectedLoop() : selectedModCable())
        {
            shown.disconnect (cable->from, cable->to);
            commit (PatchCableView::Edit::cables);
        }
    };
    removeCableButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeCableButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
    addChildComponent (removeCableButton);

    // A selected LFO cable: AMOUNT per knob it moves, the effect's window, remove.
    lfoAmounts = std::make_unique<FxLfoAmounts> (processorRef);
    for (auto& knob : lfoAmounts->knobs)
        addChildComponent (*knob);
    IlanaTheme::makePill (openTargetButton, IlanaTheme::accent());
    openTargetButton.setTooltip ("Open the effect's window: right-click a knob there to assign it to this LFO");
    openTargetButton.onClick = [this]
    {
        if (const auto* cable = selectedModCable())
            if (const auto slot = slotOf (shown.find (cable->to.node)); slot >= 0)
                openWindow (slot);
    };
    addChildComponent (openTargetButton);
    // A selected LFO box: its wave, its main controls, its window, remove.
    IlanaTheme::makePill (openLfoButton, IlanaTheme::accent());
    openLfoButton.setTooltip ("Open this LFO's window (double-click its box does too)");
    openLfoButton.onClick = [this] { if (inspectedLfo > 0) openLfoWindow (inspectedLfo); };
    removeLfoButton.setTooltip ("Take this LFO out (the knobs it moved go back to where they are set)");
    removeLfoButton.onClick = [this] { removeLfo (inspectedLfo); };
    removeLfoButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    removeLfoButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
    addChildComponent (openLfoButton);
    addChildComponent (removeLfoButton);

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

PatchPage::~PatchPage ()
{
    if (loopGestureOpen)
        processorRef.endEdit();
}

void PatchPage::paint (juce::Graphics& g)
{
    using namespace IlanaTheme;
    paintPageBackground (g, getLocalBounds());
    // The zoom's well: one pill round out, the percentage and in.
    paintWell (g, zoomWell.toFloat(), (float) zoomWell.getHeight() * 0.5f);

    const auto refused = juce::Time::getMillisecondCounter() < refusedUntil;
    g.setColour (refused ? Ui::accent2 : Ui::text3);
    g.setFont (font (TextSize::label));
    paintInspector (g);
}

void PatchPage::paintOverChildren (juce::Graphics& g)
{
    using namespace IlanaTheme;
    if (! windows.empty() || ! lfoWindows.empty())
        return; // (an open effect window covers the corner)
    const auto refused = juce::Time::getMillisecondCounter() < refusedUntil;
    g.setColour (refused ? Ui::accent2 : Ui::text3);
    g.setFont (font (TextSize::label));
    g.drawText (refused ? refusal : hintText(), hint, juce::Justification::centredRight, true);
}

bool PatchPage::inspectorIsIdle () const
{
    return inspectedVoice < 0 && inspectedSlot < 0 && inspectedLfo <= 0 && selectedModCable() == nullptr && selectedLoop() == nullptr;
}

void PatchPage::resized ()
{
    // The FX RACK page's frame: 8 px above and below, a 12 px gutter at the right.
    auto area = getLocalBounds().reduced (0, Space::s8).withTrimmedRight (Space::s12);
    auto toolbar = area.removeFromTop (toolbarHeight);
    // Right: + ADD EFFECT, RESET, the zoom and FIT.
    toolbar.removeFromLeft (10); // off-grid
    fitButton.setBounds (toolbar.removeFromRight (70).reduced (0, Space::s2)); // off-grid
    toolbar.removeFromRight (Space::s6);
    // (The well is the buttons' height, 24 px, its parts 2 px inside it.)
    zoomWell = toolbar.removeFromRight (132).reduced (0, Space::s2); // off-grid
    {
        auto well = zoomWell.reduced (Space::s2, Space::s2);
        zoomOutButton.setBounds (well.removeFromLeft (32)); // off-grid
        zoomInButton.setBounds (well.removeFromRight (32)); // off-grid
        zoomLabel.setBounds (well);
    }
    toolbar.removeFromRight (Space::s16);
    if (hasVoice)
    {
        resetButton.setBounds (toolbar.removeFromRight (80).reduced (0, Space::s2)); // off-grid
        toolbar.removeFromRight (Space::s6);
    }
    addButton.setBounds (toolbar.removeFromRight (112).reduced (0, Space::s2)); // off-grid
    toolbar.removeFromRight (Space::s6);
    lfoButton.setBounds (toolbar.removeFromRight (70).reduced (0, Space::s2)); // off-grid
    area.removeFromTop (10); // off-grid
    inspector = area.removeFromBottom (inspectorIsIdle() ? idleInspectorHeight : inspectorHeight);
    area.removeFromBottom (10); // off-grid
    // The row of effects under the canvas.
    carousel.setBounds (area.removeFromBottom (FxCarousel::stripHeight));
    area.removeFromBottom (10); // off-grid
    view.setBounds (area);
    // The canvas's hint: in its empty lower-right corner.
    hint = area.reduced (Space::s8).removeFromBottom (Space::s24).withTrimmedLeft (area.getWidth() / 3);
    if (autoFit)
        fit();
    layoutInspector();
    for (auto& window : windows)
        window->setLimits (view.getBounds());
    for (auto& window : lfoWindows)
        window->setLimits (view.getBounds());
}

VoiceRouting::Patch PatchPage::voicePatchOf (const PatchGraph::Graph& graph)
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

int PatchPage::slotOf (const PatchGraph::Node* node) const
{
    if (node == nullptr || node->isFixed() || isVoice (*node) || ! juce::isPositiveAndBelow (node->ref - 1, IlanaSynthAudioProcessor::numFxSlots))
        return -1;
    return node->ref - 1;
}

juce::Colour PatchPage::colourOf (const PatchGraph::Node& node)
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
    if (kind == stageKind + VoiceRouting::body)
        return FilterColours::body();
    return IlanaTheme::subNoiseColour(); // SUB + NOISE, as SIGNAL FLOW and PLAY draw it
}

juce::String PatchPage::voiceName (int kind)
{
    if (juce::isPositiveAndBelow (kind, VoiceRouting::numOscSources))
        return "OSC " + juce::String (kind + 1);
    if (kind == VoiceRouting::subNoiseSource)
        return "SUB + NOISE";
    if (kind == stageKind + VoiceRouting::f1)
        return "FILTER 1";
    if (kind == stageKind + VoiceRouting::f2)
        return "FILTER 2";
    if (kind == stageKind + VoiceRouting::body)
        return "BODY";
    return "WEST";
}

float PatchPage::read (const juce::String& id) const
{
    if (const auto* value = processorRef.apvts.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

juce::String PatchPage::onParamOf (int kind)
{
    if (juce::isPositiveAndBelow (kind, VoiceRouting::numOscSources))
        return juce::String (OscillatorIds::prefixes[(size_t) kind]) + "_on";
    if (kind == stageKind + VoiceRouting::west)
        return "west_on";
    return {};
}

bool PatchPage::sourceOn (int kind) const
{
    if (kind == VoiceRouting::subNoiseSource)
        return read ("subosc_on") > 0.5f || (read ("noise_on") > 0.5f && read ("noise_level") > 0.001f);
    return read (onParamOf (kind)) > 0.5f;
}

juce::String PatchPage::statusOf (const PatchGraph::Node& node) const
{
        if (isLfo (node))
        {
            const auto* lfo = lfoPatch.findLfo (lfoOf (node));
            return lfo != nullptr ? FxLfo::rateText (lfo->settings).toUpperCase() : juce::String();
        }
        if (! isVoice (node))
        {
            if (node.bypassed)
                return "BYPASSED";
            const auto slot = slotOf (&node);
            if (slot < 0)
                return {};
            const auto cpu = processorRef.getFxSlotCpu (slot);
            return juce::String (cpu * 100.0f, cpu < 0.001f ? 2 : 1) + "% CPU";
        }
        const auto kind = localKind (node);
        if (kind >= stageKind)
        {
            const auto stage = kind - stageKind;
            if (! table.used[(size_t) stage])
                return "NOT IN USE";
            if (stage == VoiceRouting::west)
                return read ("west_on") > 0.5f ? "FOLD + GATE" : "OFF: PASSES THROUGH";
            if (stage == VoiceRouting::body)
                return read ("res_on") > 0.5f ? (read ("body_type") > 0.5f ? "MATERIAL BODY" : "RESONATOR") : "OFF: PASSES THROUGH";
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

juce::String PatchPage::hintText () const
{
    const auto loops = processorRef.getFxLoopLatencyMs();
    if (loops > 0.0f)
        return "Drag output to input. Drag a loop's tag for its level.";
    return "Drag output to input. Drag over empty space to pick boxes; Ctrl+C, Ctrl+V copy them.";
}

juce::String PatchPage::voiceOrderText () const
{
    juce::StringArray names;
    for (int k = 0; k < table.numOrdered; ++k)
        names.add (voiceName (stageKind + table.order[(size_t) k]));
    return names.joinIntoString ("  >  ");
}

juce::String PatchPage::fxSummaryText () const
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

juce::String PatchPage::destinationText (const PatchGraph::Node& node) const
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

void PatchPage::paintVoiceBand (juce::Graphics& g) const
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
        // (As tall as the boxes after VOICES need, an LFO box above or below the row inside: the
        // frame hugs the chain rather than standing as tall as EACH NOTE, mostly empty, UI review 26.)
        auto top = std::numeric_limits<float>::max(), bottom = std::numeric_limits<float>::lowest();
        for (const auto& node : shown.nodes())
            if (! isVoice (node))
            {
                top = juce::jmin (top, node.y - 40.0f);
                bottom = juce::jmax (bottom, node.y + PatchCableView::nodeHeight + 20.0f);
            }
        if (top > bottom)
        {
            top = voiceArea.getY();
            bottom = voiceArea.getBottom();
        }
        const auto all = juce::Rectangle<float> (band.getRight() + 10.0f, top + view.getOrigin().y,
                                                 content.getRight() + 20.0f - band.getRight() - 10.0f, bottom - top);
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

void PatchPage::paintInspector (juce::Graphics& g)
{
    using namespace IlanaTheme;
    if (inspector.isEmpty())
        return;
    paintCard (g, inspector.toFloat(), 8.0f, Ui::line);
    auto text = inspectorText;
    const auto twoLines = [&g, &text] (const juce::String& name, const juce::String& detail, juce::Colour colour)
    {
        auto title = text.removeFromTop (text.getHeight() / 2);
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colour);
        g.setColour (Ui::text);
        g.setFont (font (TextSize::large, true));
        drawFitted (g, name, title.withTrimmedLeft (Space::s16), juce::Justification::bottomLeft, 1, TextSize::minPassive);
        g.setColour (Ui::text3);
        g.setFont (font (TextSize::label));
        drawFitted (g, detail, text.withTrimmedTop (Space::s4).withTrimmedLeft (Space::s16), juce::Justification::topLeft, 2);
    };
    if (const auto* mod = selectedModCable())
    {
        const auto* from = shown.find (mod->from.node);
        const auto* to = shown.find (mod->to.node);
        const auto id = from != nullptr ? lfoOf (*from) : 0;
        const auto target = to != nullptr ? juce::String (to->label) : juce::String ("?");
        const auto assigned = lfoAmounts->totalAssigned();
        juce::String detail;
        if (assigned == 0)
            detail = "Right-click a knob in " + target + "'s window to assign it.";
        else
            detail << "Moves " << assigned << (assigned == 1 ? " knob" : " knobs")
                   << (assigned > lfoAmounts->numShown() ? " (first " + juce::String (lfoAmounts->numShown()) + " here)" : juce::String())
                   << ". Right-click a knob in its window for more.";
        twoLines (fxLfoName (id).toUpperCase() + "  >  " + target, detail, fxLfoColour (id));
        return;
    }
    if (inspectedLfo > 0)
    {
        juce::StringArray to;
        for (const auto& cable : shown.cables())
            if (cable.from.node == lfoNodeId + (PatchGraph::NodeId) inspectedLfo)
                if (const auto* target = shown.find (cable.to.node))
                    to.add (target->label);
        twoLines (fxLfoName (inspectedLfo).toUpperCase(),
                  to.isEmpty() ? "Drag from its output onto an effect's box, then assign knobs in that effect's window."
                               : "Cabled to " + to.joinIntoString (", ") + ".",
                  fxLfoColour (inspectedLfo));
        return;
    }
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
        drawFitted (g, "LOOP  " + name (from) + " > " + name (to), title.withTrimmedLeft (Space::s16), juce::Justification::bottomLeft, 1, TextSize::minPassive);
        const auto ringing = view.loopGuardOf && view.loopGuardOf (*loop) < 0.98f;
        g.setColour (ringing ? accent() : Ui::text3);
        g.setFont (font (TextSize::label));
        g.drawText (ringing ? "Ringing by itself: held at 0 dB. Turn LEVEL down to tame it."
                            : "Feeds " + name (from) + " back into " + name (to) + ".",
                    text.withTrimmedTop (Space::s4).withTrimmedLeft (Space::s16), juce::Justification::topLeft, true);
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
        // Nothing selected: one line. The patch's chain at the left, what it costs and how it is wired at the right
        // as label-value pairs. (How to start is the canvas's corner hint.)
        auto line = inspector.reduced (Space::s16, 0);
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
            { "EFFECTS CPU", juce::String (cpu * 100.0f, 2) + "%" },
            { "LOOP GUARD", guard }
        };
        if (hasVoice)
            cells.push_back ({ "SPLIT SOURCES", juce::String (splitSources()) });
        const auto nameFont = font (TextSize::tiny, true).withKerningFactor (0.08f);
        const auto valueFont = font (TextSize::body, true, true);
        for (const auto& [name, value] : cells)
        {
            const auto valueWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (valueFont), value) + Space::s2;
            const auto nameWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (nameFont), name) + Space::s2;
            g.setColour (Ui::text);
            g.setFont (valueFont);
            g.drawText (value, line.removeFromRight (valueWidth), juce::Justification::centredRight, false);
            line.removeFromRight (Space::s6);
            g.setColour (Ui::text3);
            g.setFont (nameFont);
            g.drawText (name, line.removeFromRight (nameWidth), juce::Justification::centredRight, false);
            line.removeFromRight (Space::s24);
        }
        juce::String chain;
        if (hasVoice)
        {
            chain << voiceOrderText();
            chain << (chain.isEmpty() ? "" : "  >  ") << "VOICES  >  ";
        }
        chain << fxSummaryText() << "  >  OUT";
        g.setColour (Ui::text);
        g.setFont (font (TextSize::body, true));
        g.drawText (chain, line, juce::Justification::centredLeft, true);
        return;
    }

    auto title = text.removeFromTop (text.getHeight() / 2);
    paintTag (g, { (float) title.getX() + 4.0f, (float) title.getBottom() - 10.0f }, colourOf (*node));
    g.setColour (Ui::text);
    g.setFont (font (TextSize::large, true));
    g.drawText (isVoice (*node) ? voiceName (localKind (*node)) : juce::String (node->label), title.withTrimmedLeft (Space::s16),
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
        detail << ", " << juce::String (processorRef.getFxSlotCpu (slot) * 100.0f, 2) << "% CPU";
    }
    g.setColour (Ui::text3);
    g.setFont (font (TextSize::label));
    g.drawText (detail, text.withTrimmedTop (Space::s4).withTrimmedLeft (Space::s16), juce::Justification::topLeft, true);
    label ("ON", onButton.isVisible() ? onArea : powerArea);
    label ("SOLO", soloArea);
    label ("BAND", bandArea);
}

int PatchPage::splitSources () const
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

std::vector<juce::Component*> PatchPage::inspectorItems ()
{
    return { &onButton, &soloButton, &band, &editButton, &duplicateButton, &removeButton };
}

void PatchPage::placeStripControl (juce::Component& control, juce::Rectangle<int> cell)
{
    if (auto* knob = dynamic_cast<KnobControl*> (&control))
    {
        knob->setInlineKnob (false);
        knob->placeAsRing (cell, 34);
        return;
    }
    control.setBounds (cell.withSizeKeepingCentre (116, 44)); // off-grid
}

void PatchPage::layoutInspector ()
{
    // The bar is one line while nothing is selected and a strip of controls when something is.
    if (! inspector.isEmpty() && inspector.getHeight() != (inspectorIsIdle() ? idleInspectorHeight : inspectorHeight))
    {
        resized();
        repaint();
        return;
    }
    auto row = inspector.reduced (20, 10); // off-grid
    inspectorText = row.removeFromLeft (juce::jmin (inspectedVoice >= 0 ? 420 : inspectedLfo > 0 ? 200 : selectedModCable() != nullptr ? 260 : 300,
                                                   row.getWidth() / 3));
    onArea = powerArea = soloArea = bandArea = {};
    const auto loop = selectedLoop() != nullptr;
    for (juce::Component* item : { (juce::Component*) loopKnobs->level.get(), (juce::Component*) loopKnobs->delay.get(),
                                   (juce::Component*) &removeCableButton })
        item->setVisible (loop);
    if (loop)
    {
        // Name and detail, LEVEL, DELAY (the MIX knob's size), REMOVE CABLE at the right.
        auto actions = row.removeFromRight (340); // off-grid
        removeCableButton.setBounds (actions.removeFromRight (130).withSizeKeepingCentre (130, 28)); // off-grid
        row.removeFromLeft (20); // off-grid
        placeStripControl (*loopKnobs->level, row.removeFromLeft (120)); // off-grid
        row.removeFromLeft (20); // off-grid
        placeStripControl (*loopKnobs->delay, row.removeFromLeft (120)); // off-grid
        return;
    }
    // A selected LFO cable: name and detail, an AMOUNT per knob it moves,
    // OPEN WINDOW and REMOVE CABLE at the right.
    const auto* mod = selectedModCable();
    for (auto& knob : lfoAmounts->knobs)
        knob->setVisible (false);
    openTargetButton.setVisible (mod != nullptr);
    if (mod != nullptr)
    {
        auto actions = row.removeFromRight (290); // off-grid
        removeCableButton.setVisible (true);
        removeCableButton.setBounds (actions.removeFromRight (130).withSizeKeepingCentre (130, 28)); // off-grid
        actions.removeFromRight (10); // off-grid
        openTargetButton.setBounds (actions.removeFromRight (140).withSizeKeepingCentre (140, 28)); // off-grid
        row.removeFromLeft (20); // off-grid
        for (int k = 0; k < lfoAmounts->numShown(); ++k)
        {
            placeStripControl (*lfoAmounts->knobs[(size_t) k], row.removeFromLeft (120)); // off-grid
            lfoAmounts->knobs[(size_t) k]->setVisible (true);
            row.removeFromLeft (20); // off-grid
        }
        return;
    }
    // A selected LFO box: name and detail, its wave, SHAPE, SYNC, RATE,
    // DEPTH (the rest in its window), OPEN WINDOW and REMOVE at the right.
    if (inspectedLfo > 0 && lfoControls != nullptr)
    {
        auto actions = row.removeFromRight (250); // off-grid
        removeLfoButton.setBounds (actions.removeFromRight (90).withSizeKeepingCentre (90, 28)); // off-grid
        actions.removeFromRight (10); // off-grid
        openLfoButton.setBounds (actions.removeFromRight (140).withSizeKeepingCentre (140, 28)); // off-grid
        row.removeFromLeft (20); // off-grid
        lfoDisplay->setBounds (row.removeFromLeft (140).withSizeKeepingCentre (140, 44)); // off-grid
        row.removeFromLeft (20); // off-grid
        for (auto* item : lfoControls->allItems())
            item->setVisible (false);
        const auto items = lfoControls->windowItems();
        for (size_t k = 0; k < 4 && row.getWidth() >= 116; ++k)
        {
            placeStripControl (*items[k], row.removeFromLeft (116)); // off-grid
            items[k]->setVisible (true);
            row.removeFromLeft (Space::s8); // (the windows' cell gap)
        }
        return;
    }
    if (inspectedVoice < 0 && inspectedSlot < 0)
    {
        inspectorText = inspector.reduced (20, 10); // off-grid
        return;
    }
    const auto centreY = row.getY() + 32;
    if (inspectedVoice >= 0)
    {
        auto actions = row.removeFromRight (160); // off-grid
        editButton.setBounds (actions.withSizeKeepingCentre (160, 28)); // off-grid
        if (onButton.isVisible())
        {
            row.removeFromLeft (20); // off-grid
            onArea = juce::Rectangle<int> (row.getX(), centreY - 13, 50, 26);
            onButton.setBounds (onArea);
        }
        return;
    }
    auto actions = row.removeFromRight (340); // off-grid
    removeButton.setBounds (actions.removeFromRight (90).withSizeKeepingCentre (90, 28)); // off-grid
    actions.removeFromRight (10); // off-grid
    duplicateButton.setBounds (actions.removeFromRight (100).withSizeKeepingCentre (100, 28)); // off-grid
    actions.removeFromRight (10); // off-grid
    editButton.setBounds (actions.removeFromRight (130).withSizeKeepingCentre (130, 28)); // off-grid
    row.removeFromLeft (20); // off-grid
    const auto controlRow = [&] (int width) { auto cell = row.removeFromLeft (width); row.removeFromLeft (20); return cell; }; // off-grid
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
        placeStripControl (*mixKnob, controlRow (120));
}

void PatchPage::inspect (const PatchGraph::Node* node)
{
    const auto voice = node != nullptr && isVoice (*node) ? localKind (*node) : -1;
    const auto slot = slotOf (node);
    const auto lfo = node != nullptr ? lfoOf (*node) : 0;
    if (voice == inspectedVoice && slot == inspectedSlot && lfo == inspectedLfo)
        return;
    inspectedVoice = voice;
    inspectedSlot = slot;
    inspectedLfo = lfo;
    lfoDisplay.reset();
    lfoControls.reset();
    openLfoButton.setVisible (lfo > 0);
    removeLfoButton.setVisible (lfo > 0);
    if (lfo > 0)
    {
        lfoControls = std::make_unique<FxLfoControls> (processorRef, lfo);
        lfoDisplay = std::make_unique<FxLfoDisplay> (processorRef, *lfoControls);
        addAndMakeVisible (*lfoDisplay);
        for (auto* item : lfoControls->allItems())
            addChildComponent (item);
        lfoControls->onLayoutChanged = [safe = juce::Component::SafePointer<PatchPage> (this)] { if (safe != nullptr) safe->layoutInspector(); };
    }
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
        editButton.setButtonText ("EDIT ON PLAY");
        editButton.setTooltip (voice >= stageKind ? "Open PLAY, where its FILTER or WEST COAST card is" : "Open PLAY, where its oscillator card is");
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

void PatchPage::fit (bool animated)
{
    if (animated)
        autoFit = true;
    view.fitContentInto (view.getLocalBounds(), fitMaxZoom, minZoom, fitPadX, fitPadY, 0.05f, animated);

    // Review 20, R20-1 (polish pass 4): the fit is limited by the height
    // or the width. The rows of the per-note column spread (to 1.8 x) when
    // the width limits, so the graph fills the height; they close up (to
    // 70 px apart) when the height limits, so the boxes stay as big as
    // the width allows, 100 % at the most, instead of opening at 70 %.
    if (! hasVoice || ! autoFit || stretching || view.getHeight() <= 0 || view.getWidth() <= 0)
        return;
    const auto content = view.getContentArea();
    const auto bandHeight = 100.0f; // (the per-note band: 40 px above the boxes, 60 below)
    const auto spreadNow = content.getHeight() - bandHeight - PatchCableView::nodeHeight;
    if (spreadNow <= 1.0f)
        return;
    const auto widthZoom = std::floor ((float) view.getWidth() / (content.getWidth() + fitPadX) / 0.05f) * 0.05f;
    const auto target = juce::jlimit (minZoom, fitMaxZoom, widthZoom);
    const auto spreadWanted = (float) view.getHeight() / target - fitPadY - bandHeight - PatchCableView::nodeHeight;
    const auto lowest = 70.0f / rowStepFor (lastSourceCount); // (70 px is a box and a 20 px gap)
    const auto wanted = juce::jlimit (juce::jmin (1.0f, lowest), 1.8f, rowStretch * spreadWanted / spreadNow);
    if (stepOf (lastSourceCount, wanted) == stepOf (lastSourceCount, rowStretch))
        return;
    rowStretch = wanted;
    stretching = true;
    refresh (true);
    stretching = false;
    view.fitContentInto (view.getLocalBounds(), fitMaxZoom, minZoom, fitPadX, fitPadY, 0.05f, animated);
}

bool PatchPage::accept (const PatchGraph::Graph& graph)
{
    for (const auto& cable : graph.cables())
    {
        const auto* from = graph.find (cable.from.node);
        const auto* to = graph.find (cable.to.node);
        if (from == nullptr || to == nullptr)
            continue;
        if (isLfo (*from))
        {
            if (slotOf (to) < 0)
            {
                refusal = "An LFO's cable goes to an effect's box.";
                return false;
            }
            continue;
        }
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
    // A graph the compiler can't make would play as the rack instead
    // (too many loops, too many buffers): say so now, not by ear.
    if (const auto plan = PatchGraph::compile (fxGraphOf (graph)); ! plan.ok)
    {
        refusal = "The effects can't play that: " + juce::String (plan.error) + ".";
        return false;
    }
    return true;
}

PatchGraph::Graph PatchPage::fxGraphOf (const PatchGraph::Graph& graph) const
{
    auto fx = fxShown;
    for (const auto& cable : std::vector<PatchGraph::Cable> (fx.cables()))
        fx.disconnect (cable.from, cable.to);
    for (const auto& cable : graph.cables())
    {
        const auto* from = graph.find (cable.from.node);
        if (from != nullptr && ! isVoice (*from) && ! isLfo (*from))
        {
            fx.connect (cable.from, cable.to, cable.gain);
            fx.setCableDelay (fx.findCable (cable.from, cable.to), cable.delayMs);
        }
    }
    for (const auto& node : graph.nodes())
        if (! isVoice (node) && ! isLfo (node) && ! node.isInput())
            fx.moveNode (node.id, node.x - fxShift.x, node.y - fxShift.y);
    return fx;
}

bool PatchPage::sameCables (const PatchGraph::Graph& a, const PatchGraph::Graph& b)
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

void PatchPage::commit (PatchCableView::Edit edit)
{
    if (commitLfos())
        return;
    const auto fx = fxGraphOf (shown);
    if (edit == PatchCableView::Edit::loop && loopGestureOpen)
    {
        // (Live while a loop's tag or knob turns; the gesture is one undo step.)
        processorRef.setFxPatchGraph (fx);
        if (! processorRef.isFxPatcherOn())
            processorRef.setFxPatcherOn (true);
        fxShown = fx;
        nameEffects (fxShown);
        view.repaint();
        return;
    }
    if (edit == PatchCableView::Edit::layout)
    {
        // (The rack's routing as cables, now with the box where it was put:
        // the same sound, so it plays the cables from here on.)
        processorRef.performEdit ("Move effect", [this, fx]
        {
            processorRef.setFxPatchGraph (fx);
            if (! processorRef.isFxPatcherOn())
                processorRef.setFxPatcherOn (true);
        });
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
            // (The graph first: turning the patcher on keeps a graph it already has.)
            processorRef.setFxPatchGraph (fx);
            if (! processorRef.isFxPatcherOn())
                processorRef.setFxPatcherOn (true);
        }
    });
    refresh (true);
}

const PatchGraph::Cable* PatchPage::selectedModCable () const
{
    const auto index = view.getSelectedCable();
    if (! juce::isPositiveAndBelow (index, (int) shown.cables().size()))
        return nullptr;
    const auto& cable = shown.cables()[(size_t) index];
    const auto* from = shown.find (cable.from.node);
    return from != nullptr && isLfo (*from) ? &cable : nullptr;
}

const PatchGraph::Cable* PatchPage::selectedLoop () const
{
    const auto index = view.getSelectedCable();
    if (! juce::isPositiveAndBelow (index, (int) shown.cables().size()) || ! shown.feedbackCables()[(size_t) index])
        return nullptr;
    return &shown.cables()[(size_t) index];
}

float PatchPage::shortestLoopMs () const
{
    return juce::jmax (0.1f, (float) (64000.0 / juce::jmax (1.0, processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 48000.0)));
}

void PatchPage::loopGesture (bool starting, const juce::String& name)
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

void PatchPage::setLoop (float level, float delayMs)
{
    const auto index = view.getSelectedCable();
    if (selectedLoop() == nullptr)
        return;
    shown.setCableGain (index, juce::jlimit (0.0f, view.maxLoopLevel, level));
    shown.setCableDelay (index, delayMs <= shortestLoopMs() ? 0.0f : delayMs);
    commit (PatchCableView::Edit::loop);
}

void PatchPage::resetVoiceToChain ()
{
    const auto chain = processorRef.getChainAsCables();
    processorRef.performEdit ("Routing: cables from the chain", [this, chain] { processorRef.setRouteCables (chain, processorRef.areRouteCablesOn()); });
    refresh (true);
}

void PatchPage::playRackAsCables ()
{
    if (processorRef.isFxPatcherOn())
        return;
    processorRef.setFxPatchGraph (processorRef.getFxRackGraph (true)); // (bypass stays the slot's own)
    processorRef.setFxPatcherOn (true);
}

void PatchPage::toggleBypass (int slot)
{
    if (slot < 0)
        return;
    if (auto* bypass = processorRef.apvts.getParameter ("fx_slot" + juce::String (slot + 1) + "_bypass"))
        processorRef.performEdit ("Bypass effect", [bypass] { bypass->setValueNotifyingHost (bypass->getValue() > 0.5f ? 0.0f : 1.0f); });
    refresh (true);
}

void PatchPage::removeEffect (int slot)
{
    if (slot < 0)
        return;
    processorRef.performEdit ("Remove effect", [this, slot] { processorRef.removeFxEffect (slot); });
    refresh (true);
}

bool PatchPage::rackFull () const
{
    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        if (read ("fx_slot" + juce::String (slot + 1)) < 0.5f)
            return false;
    return true;
}

void PatchPage::duplicateEffect (int slot)
{
    if (rackFull())
    {
        // (Refused before any edit: nothing changes, not even the routing.)
        refusal = "Every slot is in use.";
        refusedUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint (hint);
        return;
    }
    const PatchGraph::Node* node = nullptr;
    for (const auto& box : shown.nodes())
        if (slotOf (&box) == slot)
            node = &box;
    if (node == nullptr)
        return;
    const auto type = node->kind;
    const juce::Point<float> at (node->x, node->y + 70.0f);
    auto added = -1;
    processorRef.performEdit ("Duplicate effect", [this, slot, type, at, &added]
    {
        playRackAsCables();
        if (! processorRef.isFxSlotInFront (slot))
            processorRef.focusFxInstance (slot);
        added = processorRef.addFxEffect (type);
        if (added >= 0)
            placeNodeNow (added, at); // (inside the edit, so redo keeps the place)
    });
    refresh (true);
}

void PatchPage::copyEffects (const std::vector<PatchGraph::NodeId>& ids)
{
    Clip copied;
    std::map<PatchGraph::NodeId, int> index;
    for (const auto id : ids)
        if (const auto* node = shown.find (id); node != nullptr && slotOf (node) >= 0 && ! isLfo (*node))
        {
            const auto slot = slotOf (node);
            index[id] = (int) copied.boxes.size();
            copied.boxes.push_back ({ slot, node->kind, { node->x, node->y }, processorRef.snapshotFxSlot (slot) });
        }
    if (copied.boxes.empty())
        return;
    for (const auto& cable : shown.cables())
        if (index.count (cable.from.node) > 0 && index.count (cable.to.node) > 0)
            copied.wires.push_back ({ index[cable.from.node], cable.from.port, index[cable.to.node], cable.to.port, cable.gain, cable.delayMs });
    clip = std::move (copied);
}

int PatchPage::freeSlots () const
{
    auto free = 0;
    for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
        free += read ("fx_slot" + juce::String (slot + 1)) < 0.5f ? 1 : 0;
    return free;
}

void PatchPage::pasteEffects (juce::Point<float> at)
{
    if (clip.boxes.empty())
        return;
    if (freeSlots() < (int) clip.boxes.size())
    {
        refusal = "Not enough free slots for " + juce::String ((int) clip.boxes.size()) + " effects.";
        refusedUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint (hint);
        return;
    }
    auto topLeft = clip.boxes.front().at;
    auto bottom = topLeft.y;
    for (const auto& box : clip.boxes)
    {
        topLeft = { juce::jmin (topLeft.x, box.at.x), juce::jmin (topLeft.y, box.at.y) };
        bottom = juce::jmax (bottom, box.at.y);
    }
    auto offset = std::isnan (at.x) ? juce::Point<float> (0.0f, bottom - topLeft.y + 70.0f) : at - topLeft;
    // (Never on top of a box already there: a row further down until the copies land in free space.)
    const auto overlaps = [this] (juce::Point<float> by)
    {
        for (const auto& box : clip.boxes)
        {
            const juce::Rectangle<float> area (box.at.x + by.x - 10.0f, box.at.y + by.y - 10.0f, PatchCableView::nodeWidth + 20.0f, PatchCableView::nodeHeight + 20.0f);
            for (const auto& node : shown.nodes())
                if (area.intersects (juce::Rectangle<float> { node.x, node.y, node.isFixed() ? PatchCableView::fixedWidth : PatchCableView::nodeWidth, PatchCableView::nodeHeight }))
                    return true;
            if (voiceArea.intersects (area))
                return true;
        }
        return false;
    };
    for (int tries = 0; tries < 24 && overlaps (offset); ++tries)
        offset.y += 70.0f;

    std::vector<int> added;
    processorRef.performEdit (clip.boxes.size() == 1 ? "Paste effect" : "Paste effects", [this, offset, &added]
    {
        playRackAsCables();
        const auto before = processorRef.getFxPatchGraph();
        for (const auto& box : clip.boxes)
        {
            // (The copy takes its snapshotted settings even if the source slot moved or was removed.)
            if (box.values.empty() && read ("fx_slot" + juce::String (box.slot + 1)) == (float) box.type && ! processorRef.isFxSlotInFront (box.slot))
                processorRef.focusFxInstance (box.slot);
            const auto addedSlot = processorRef.addFxEffect (box.type);
            if (addedSlot >= 0 && ! box.values.empty())
                processorRef.applyFxSlotValues (addedSlot, box.values);
            added.push_back (addedSlot);
        }
        // The cables as they were, and the copies' own between them: nothing else is wired.
        auto graph = processorRef.getFxPatchGraph();
        for (const auto& cable : std::vector<PatchGraph::Cable> (graph.cables()))
            graph.disconnect (cable.from, cable.to);
        for (const auto& cable : before.cables())
        {
            graph.connect (cable.from, cable.to, cable.gain);
            graph.setCableDelay (graph.findCable (cable.from, cable.to), cable.delayMs);
        }
        std::vector<PatchGraph::NodeId> ids (added.size(), 0);
        for (size_t i = 0; i < added.size(); ++i)
            for (const auto& node : graph.nodes())
                if (added[i] >= 0 && node.ref == added[i] + 1 && ! node.isFixed())
                {
                    ids[i] = node.id;
                    const auto to = clip.boxes[i].at + offset - fxShift;
                    graph.moveNode (node.id, std::round (to.x / 10.0f) * 10.0f, std::round (to.y / 10.0f) * 10.0f);
                }
        for (const auto& wire : clip.wires)
            if (ids[(size_t) wire.from] != 0 && ids[(size_t) wire.to] != 0)
            {
                const PatchGraph::Port from { ids[(size_t) wire.from], wire.fromPort }, to { ids[(size_t) wire.to], wire.toPort };
                graph.connect (from, to, wire.gain);
                graph.setCableDelay (graph.findCable (from, to), wire.delayMs);
            }
        processorRef.setFxPatchGraph (graph);
    });
    refresh (true);
    std::vector<PatchGraph::NodeId> pasted;
    for (const auto& node : shown.nodes())
        for (const auto slot : added)
            if (slot >= 0 && slotOf (&node) == slot)
                pasted.push_back (node.id);
    view.setSelection (pasted);
}

void PatchPage::removeMany (const std::vector<PatchGraph::NodeId>& ids)
{
    std::vector<int> slots, lfos;
    for (const auto id : ids)
        if (const auto* node = shown.find (id))
        {
            if (isLfo (*node))
                lfos.push_back (lfoOf (*node));
            else if (const auto slot = slotOf (node); slot >= 0)
                slots.push_back (slot);
        }
    if (slots.empty() && lfos.empty())
        return;
    std::sort (slots.rbegin(), slots.rend());
    processorRef.performEdit ("Remove effects", [this, slots, lfos]
    {
        for (const auto slot : slots)
            processorRef.removeFxEffect (slot);
        for (const auto lfo : lfos)
            processorRef.removeFxLfo (lfo);
    });
    for (const auto lfo : lfos)
        for (auto& window : lfoWindows)
            if (window->id == lfo)
            {
                closeLfoWindow (window.get());
                break;
            }
    refresh (true);
}

void PatchPage::restoreCables (const PatchGraph::Graph& before)
{
    auto graph = processorRef.getFxPatchGraph();
    for (const auto& cable : std::vector<PatchGraph::Cable> (graph.cables()))
        graph.disconnect (cable.from, cable.to);
    for (const auto& cable : before.cables())
    {
        graph.connect (cable.from, cable.to, cable.gain);
        graph.setCableDelay (graph.findCable (cable.from, cable.to), cable.delayMs);
    }
    processorRef.setFxPatchGraph (graph);
}

void PatchPage::open (const PatchGraph::Node& node)
{
    if (isLfo (node))
        return openLfoWindow (lfoOf (node));
    if (isVoice (node))
    {
        if (onShowPage)
            onShowPage ("MAIN"); // (the oscillators and the filters are on PLAY)
    }
    else if (const auto slot = slotOf (&node); slot >= 0)
    {
        openWindow (slot);
    }
}

void PatchPage::openWindow (int slot)
{
    const PatchGraph::Node* node = nullptr;
    for (const auto& box : shown.nodes())
        if (slotOf (&box) == slot)
            node = &box;
    if (node == nullptr)
        return;
    for (auto& window : windows)
        if (window->slot == slot && window->isVisible())
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
    window->setLfoHooks ([this, slot] (juce::PopupMenu& menu, const juce::String& param) { addKnobLfoItems (menu, slot, param); },
                         [this, slot] (const juce::String& param) { return knobLfoColour (slot, param); });
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
    window->setBounds (place);
    addAndMakeVisible (*window);
    window->setLimits (view.getBounds());
    windows.push_back (std::move (window));
}

void PatchPage::closeWindow (FxMiniWindow* window)
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
    });
}

void PatchPage::checkWindows ()
{
    for (auto& window : lfoWindows)
        if (window->isVisible())
        {
            if (lfoPatch.findLfo (window->id) == nullptr)
                closeLfoWindow (window.get());
            else
                window->refresh();
        }
    // (So does one whose settings moved to or from the front: its
    // controls would be on the wrong ones.)
    for (auto& window : windows)
        if (window->isVisible())
        {
            if (juce::roundToInt (read ("fx_slot" + juce::String (window->slot + 1))) != window->type
                || window->isOwnSettings() == processorRef.isFxSlotInFront (window->slot))
                closeWindow (window.get());
            else
            {
                window->refresh();
                if (std::any_of (lfoPatch.lfoCables.begin(), lfoPatch.lfoCables.end(),
                                                                             [&window] (const auto& c) { return c.slot == window->slot; }))
                    window->repaint();
            }
        }
}

void PatchPage::openSelected ()
{
    if (const auto* node = shown.find (view.getSelectedNode()))
        open (*node);
}

void PatchPage::addNodeItems (juce::PopupMenu& menu, const PatchGraph::Node& node)
{
    juce::Component::SafePointer<PatchPage> safeThis (this);
    if (isLfo (node))
    {
        menu.addItem ("Open its window", [safeThis, id = lfoOf (node)] { if (safeThis != nullptr) safeThis->openLfoWindow (id); });
        return;
    }
    if (isVoice (node))
    {
        const auto kind = localKind (node);
        menu.addItem ("Edit on PLAY", [safeThis, node] { if (safeThis != nullptr) safeThis->open (node); });
        if (kind != stageKind + VoiceRouting::f1 && kind != stageKind + VoiceRouting::f2)
            menu.addItem ("Remove its cables", [safeThis, id = node.id]
            {
                if (safeThis == nullptr)
                    return;
                for (const auto& cable : std::vector<PatchGraph::Cable> (safeThis->shown.cables()))
                    if (cable.from.node == id)
                        safeThis->shown.disconnect (cable.from, cable.to);
                safeThis->commit (PatchCableView::Edit::cables);
            });
        return;
    }
    const auto slot = slotOf (&node);
    if (slot < 0)
        return;
    menu.addItem ("Open its window", [safeThis, slot] { if (safeThis != nullptr) safeThis->openWindow (slot); });
    menu.addItem ("Edit on the RACK page", [safeThis, slot] { if (safeThis != nullptr && safeThis->onOpenSlot) safeThis->onOpenSlot (slot); });
    menu.addItem ("Duplicate (its own settings)", ! rackFull(), false, [safeThis, slot] { if (safeThis != nullptr) safeThis->duplicateEffect (slot); });
}

void PatchPage::addEffectItems (juce::PopupMenu& menu, juce::Point<float> at)
{
    juce::Component::SafePointer<PatchPage> safeThis (this);
    menu.addItem ("Add an LFO here", (int) lfoPatch.lfos.size() < FxLfo::maxLfos, false, [safeThis, at] { if (safeThis != nullptr) safeThis->addLfo (at); });
    menu.addSeparator();
    if (rackFull())
    {
        menu.addItem (-1, "Every slot is in use", false);
        return;
    }
    for (const auto& group : fxLibraryGroups())
    {
        juce::PopupMenu sub;
        for (const auto& entry : group.entries)
        {
            const auto add = [safeThis, at] (int type, const juce::String& name)
            {
                if (safeThis == nullptr)
                    return;
                auto added = -1;
                safeThis->processorRef.performEdit ("Add " + name, [safeThis, type, at, &added]
                {
                    safeThis->playRackAsCables();
                    added = safeThis->processorRef.addFxEffect (type);
                    if (added >= 0)
                        safeThis->placeNodeNow (added, at); // (inside the edit, so redo keeps the place)
                });
                safeThis->refresh (true);
            };
            sub.addItem (entry.name, [add, type = entry.type, name = juce::String (entry.name)] { add (type, name); });
            if (entry.twin > 0)
                sub.addItem (juce::String (entry.twinName) + " (Airwindows)",
                             [add, type = entry.twin, name = juce::String (entry.twinName)] { add (type, name); });
        }
        menu.addSubMenu (juce::String (group.title).toLowerCase().replace ("&", "and").toUpperCase(), sub);
    }
}

void PatchPage::placeNodeNow (int slot, juce::Point<float> at)
{
    auto graph = processorRef.getFxPatchGraph();
    for (const auto& node : std::vector<PatchGraph::Node> (graph.nodes()))
        if (node.ref == slot + 1 && ! node.isFixed())
        {
            graph.moveNode (node.id, std::round ((at.x - fxShift.x) / 10.0f) * 10.0f, std::round ((at.y - fxShift.y) / 10.0f) * 10.0f);
            if (processorRef.isFxPatcherOn())
                processorRef.setFxPatchGraph (graph);
        }
}

bool PatchPage::addFromCarousel (const FxCarousel::Tile& tile, std::optional<juce::Point<float>> at)
{
    if (rackFull())
        return false;
    // (A box placed by hand: the view stays where the user has it until it is dropped.)
    if (at.has_value())
        carouselDrag = true;
    auto added = -1;
    processorRef.performEdit ("Add " + tile.name, [this, &tile, at, &added]
    {
        playRackAsCables();
        const auto before = processorRef.getFxPatchGraph();
        added = processorRef.addFxEffect (tile.type);
        // (Dragged out: it lands where it is dropped, wired to nothing, or into the cable it is dropped on;
        // ilana 2026-11-04. A click still wires it in before OUT.)
        if (added >= 0 && at.has_value())
        {
            restoreCables (before);
            placeNodeNow (added, *at);
        }
    });
    if (added < 0)
    {
        carouselDrag = false;
        return false;
    }
    refresh (true);
    if (at.has_value())
    {
        for (const auto& node : shown.nodes())
            if (node.ref == added + 1 && ! node.isFixed() && ! isVoice (node))
            {
                if (view.beginNodeDrag (node.id))
                    return true;
                break;
            }
        carouselDrag = false;
        return false;
    }
    return true;
}

void PatchPage::refresh (bool force)
{
    const auto cablesOn = hasVoice && processorRef.areRouteCablesOn();
    const auto patcher = processorRef.isFxPatcherOn();
    const auto parallel = read ("fx_routing") > 0.5f;

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
    // (While the rack plays, its routing as it is now: a graph saved by an
    // earlier patcher session is not what is heard.)
    fxShown = patcher ? processorRef.getFxPatchGraph() : processorRef.getFxRackGraph();
    if (! patcher)
        if (const auto* rackInput = fxShown.find (PatchGraph::inputNodeId))
        {
            // (On the voice's column step, measured from INPUT.)
            const auto from = rackInput->x;
            for (const auto& node : std::vector<PatchGraph::Node> (fxShown.nodes()))
                if (node.id != PatchGraph::inputNodeId)
                    fxShown.moveNode (node.id, from + std::round ((node.x - from) * columnStep / rackStep / 10.0f) * 10.0f, node.y);
        }
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
    addLfoBoxes (graph);

    shown = graph;
    view.setLoopLatencyMs (shortestLoopMs());
    view.graphChanged();
    // A new box or one outside the view: the patch is fitted again, unless
    // the user has moved the view (then only a new patch that is off
    // screen is, as after loading a preset).
    const auto countChanged = (int) shown.nodes().size() != shownNodes;
    if (! carouselDrag && (autoFit ? (countChanged || ! view.contentFits()) : (countChanged && ! view.contentFits())))
    {
        autoFit = true;
        fit();
    }
    shownNodes = (int) shown.nodes().size();
    repaint();
}

void PatchPage::nameEffects (PatchGraph::Graph& fx) const
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

juce::Point<float> PatchPage::addVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources)
{
    std::array<int, VoiceRouting::numStages> column { 1, 1, 1, 1 };
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
    lastSourceCount = sources.size();
    const auto rowStep = stepOf (sources.size(), rowStretch);
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
    // (BODY dims the same way: NOT IN USE beside a bright cable read as playing, UI review 27.)
    if (! table.used[VoiceRouting::body] || read ("res_on") < 0.5f)
        graph.setBypassed (voiceId + (PatchGraph::NodeId) (stageKind + VoiceRouting::body), true);

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

void PatchPage::connectVoice (PatchGraph::Graph& graph, const VoiceRouting::Patch& patch, const std::vector<int>& sources)
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

const std::vector<std::pair<juce::String, juce::String>>& PatchPage::targetsOf (int slot)
{
    auto& targets = lfoTargets[slot];
    if (targets.empty())
        targets = processorRef.getFxLfoTargets (slot);
    return targets;
}

void PatchPage::addLfoBoxes (PatchGraph::Graph& graph)
{
    lfoPatch = processorRef.getFxPatch();
    lfoTargets.clear();
    modTags.clear();
    for (const auto& lfo : lfoPatch.lfos)
    {
        PatchGraph::Node node;
        node.id = lfoNodeId + (PatchGraph::NodeId) lfo.id;
        node.kind = lfoKind;
        node.numInputs = 0;
        node.numOutputs = 1;
        node.x = lfo.x + fxShift.x;
        node.y = lfo.y + fxShift.y;
        node.label = fxLfoName (lfo.id).toUpperCase().toStdString();
        graph.restoreNode (node);
    }
    for (const auto& cable : lfoPatch.lfoCables)
    {
        const PatchGraph::Node* target = nullptr;
        for (const auto& node : graph.nodes())
            if (slotOf (&node) == cable.slot)
                target = &node;
        if (target == nullptr)
            continue;
        graph.connect ({ lfoNodeId + (PatchGraph::NodeId) cable.lfo, 0 }, { target->id, 0 });

        juce::StringArray names;
        for (const auto& route : lfoPatch.lfoRoutes)
            if (route.lfo == cable.lfo && route.slot == cable.slot)
                for (const auto& [param, name] : targetsOf (cable.slot))
                    if (param == route.param)
                        names.add (FxLfo::shortName (name, fxTypeName (target->kind)));
        juce::String tag;
        if (names.isEmpty())
            tag = "ASSIGN A KNOB";
        else if (names.size() <= 2)
            tag = names.joinIntoString (", ");
        else
            tag = names[0] + ", " + names[1] + " +" + juce::String (names.size() - 2);
        modTags[{ cable.lfo, cable.slot }] = tag;
    }
}

juce::String PatchPage::modTagOf (const PatchGraph::Cable& cable) const
{
    const auto* from = shown.find (cable.from.node);
    const auto slot = slotOf (shown.find (cable.to.node));
    if (from == nullptr || slot < 0)
        return {};
    const auto found = modTags.find ({ lfoOf (*from), slot });
    return found != modTags.end() ? found->second : juce::String();
}

void PatchPage::decorateLfo (juce::Graphics& g, const PatchGraph::Node& node, juce::Rectangle<float> box) const
{
    if (! isLfo (node))
        return;
    const auto* lfo = lfoPatch.findLfo (lfoOf (node));
    if (lfo == nullptr)
        return;
    const auto plot = juce::Rectangle<float> (box.getRight() - 54.0f, box.getY() + 13.0f, 40.0f, 24.0f);
    paintFxLfoWave (g, plot, lfo->settings, fxLfoColour (lfo->id), processorRef.getFxLfoPhase (lfo->id), true, false);
}

bool PatchPage::commitLfos ()
{
    const auto patchPointer = processorRef.getFxPatchShared(); // (no copy)
    const auto& patch = *patchPointer;
    std::vector<FxPatchData::LfoCable> cables;
    for (const auto& cable : shown.cables())
        if (const auto* from = shown.find (cable.from.node); from != nullptr && isLfo (*from))
            if (const auto slot = slotOf (shown.find (cable.to.node)); slot >= 0)
                cables.push_back ({ lfoOf (*from), slot });
    std::vector<FxPatchData::LfoCable> added, removed;
    for (const auto& cable : cables)
        if (! patch.hasLfoCable (cable.lfo, cable.slot))
            added.push_back (cable);
    for (const auto& cable : patch.lfoCables)
        if (std::find (cables.begin(), cables.end(), cable) == cables.end())
            removed.push_back (cable);
    std::vector<FxPatchData::Lfo> moved;
    for (const auto& node : shown.nodes())
        if (const auto* lfo = isLfo (node) ? patch.findLfo (lfoOf (node)) : nullptr)
            if (lfo->x != node.x - fxShift.x || lfo->y != node.y - fxShift.y)
            {
                auto at = *lfo;
                at.x = node.x - fxShift.x;
                at.y = node.y - fxShift.y;
                moved.push_back (at);
            }
    if (added.empty() && removed.empty() && moved.empty())
        return false;
    const auto name = ! moved.empty() && added.empty() && removed.empty() ? "Move LFO" : removed.empty() ? "LFO cable" : "Remove LFO cable";
    processorRef.performEdit (name, [this, added, removed, moved]
    {
        for (const auto& cable : removed)
            processorRef.setFxLfoCable (cable.lfo, cable.slot, false);
        for (const auto& cable : added)
            processorRef.setFxLfoCable (cable.lfo, cable.slot, true);
        for (const auto& lfo : moved)
            processorRef.moveFxLfo (lfo.id, lfo.x, lfo.y);
    });
    refresh (true);
    return true;
}

int PatchPage::addLfo (std::optional<juce::Point<float>> at)
{
    if ((int) lfoPatch.lfos.size() >= FxLfo::maxLfos)
    {
        refusal = "Every LFO box is in use (" + juce::String (FxLfo::maxLfos) + ").";
        refusedUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint (hint);
        return 0;
    }
    juce::Point<float> place;
    if (at.has_value())
    {
        place = *at;
    }
    else
    {
        // Above the effects' row, left to right as they are added.
        auto top = std::numeric_limits<float>::max(), left = std::numeric_limits<float>::max();
        for (const auto& node : shown.nodes())
            if (! isVoice (node) && ! isLfo (node) && ! node.isOutput())
            {
                top = juce::jmin (top, node.y);
                left = juce::jmin (left, node.x + (node.isInput() ? PatchCableView::fixedWidth + 50.0f : 0.0f));
            }
        if (top == std::numeric_limits<float>::max())
            top = left = 0.0f;
        const auto row = (float) lfoPatch.lfos.size();
        place = { left + row * (PatchCableView::nodeWidth + 20.0f), top - 110.0f };
    }
    const auto x = std::round ((place.x - fxShift.x) / 10.0f) * 10.0f, y = std::round ((place.y - fxShift.y) / 10.0f) * 10.0f;
    auto id = 0;
    processorRef.performEdit ("Add LFO", [this, x, y, &id] { id = processorRef.addFxLfo (x, y); });
    refresh (true);
    return id;
}

void PatchPage::removeLfo (int id)
{
    if (id <= 0)
        return;
    processorRef.performEdit ("Remove " + fxLfoName (id), [this, id] { processorRef.removeFxLfo (id); });
    for (auto& window : lfoWindows)
        if (window->id == id)
            closeLfoWindow (window.get());
    refresh (true);
}

void PatchPage::openLfoWindow (int id)
{
    const auto* node = shown.find (lfoNodeId + (PatchGraph::NodeId) id);
    if (node == nullptr)
        return;
    for (auto& window : lfoWindows)
        if (window->id == id && window->isVisible())
        {
            window->toFront (false);
            return;
        }
    auto window = std::make_unique<FxLfoWindow> (processorRef, id);
    window->onClose = [this] (FxLfoWindow& closing) { closeLfoWindow (&closing); };
    // Beside its box, where it covers the fewest boxes: right, left, below, above.
    const auto box = (view.nodeBounds (*node) * view.getZoom()).toNearestInt() + view.getPosition();
    const auto size = window->getLocalBounds();
    const std::vector<juce::Rectangle<int>> places { size.withPosition (box.getRight() + 20, box.getY()),
                                                     size.withPosition (box.getX() - 20 - size.getWidth(), box.getY()),
                                                     size.withPosition (box.getCentreX() - size.getWidth() / 2, box.getBottom() + 20),
                                                     size.withPosition (box.getCentreX() - size.getWidth() / 2, box.getY() - 20 - size.getHeight()) };
    auto place = places[0].constrainedWithin (view.getBounds());
    auto least = std::numeric_limits<juce::int64>::max();
    for (const auto& candidate : places)
    {
        const auto inside = candidate.constrainedWithin (view.getBounds());
        juce::int64 covered = view.getBounds().contains (candidate) ? 0 : 1;
        for (const auto& other : shown.nodes())
        {
            const auto overlap = ((view.nodeBounds (other) * view.getZoom()).toNearestInt() + view.getPosition()).getIntersection (inside);
            covered += (juce::int64) overlap.getWidth() * overlap.getHeight();
        }
        for (const auto& other : windows)
            if (other->isVisible())
            {
                const auto overlap = other->getBounds().getIntersection (inside);
                covered += 100 * (juce::int64) overlap.getWidth() * overlap.getHeight();
            }
        if (covered < least)
        {
            least = covered;
            place = inside;
        }
    }
    window->setBounds (place);
    addAndMakeVisible (*window);
    window->setLimits (view.getBounds());
    lfoWindows.push_back (std::move (window));
}

void PatchPage::closeLfoWindow (FxLfoWindow* window)
{
    if (window == nullptr)
        return;
    window->setVisible (false);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PatchPage> (this), window]
    {
        if (safe == nullptr)
            return;
        auto& list = safe->lfoWindows;
        list.erase (std::remove_if (list.begin(), list.end(), [window] (const auto& w) { return w.get() == window; }), list.end());
    });
}

void PatchPage::addKnobLfoItems (juce::PopupMenu& menu, int slot, const juce::String& param)
{
    const auto patchPointer = processorRef.getFxPatchShared(); // (no copy)
    const auto& patch = *patchPointer;
    const auto targets = processorRef.getFxLfoTargets (slot);
    if (std::none_of (targets.begin(), targets.end(), [&param] (const auto& t) { return t.first == param; }))
        return;
    auto cabled = 0;
    for (const auto& cable : patch.lfoCables)
    {
        if (cable.slot != slot)
            continue;
        ++cabled;
        const auto id = cable.lfo;
        const auto name = fxLfoName (id);
        const auto route = std::find_if (patch.lfoRoutes.begin(), patch.lfoRoutes.end(), [&] (const FxPatchData::LfoRoute& r)
                                         { return r.lfo == id && r.slot == slot && r.param == param; });
        juce::Component::SafePointer<PatchPage> safeThis (this);
        if (route == patch.lfoRoutes.end())
        {
            menu.addItem ("Assign to " + name, [safeThis, id, slot, param, name]
            {
                if (safeThis == nullptr)
                    return;
                safeThis->processorRef.performEdit ("Assign to " + name, [safeThis, id, slot, param] { safeThis->processorRef.assignFxLfo (id, slot, param, true, 0.5f); });
                safeThis->refresh (true);
            });
            continue;
        }
        juce::PopupMenu amounts;
        for (const auto percent : { 100, 50, 25, -25, -50, -100 })
            amounts.addItem (juce::String (percent) + "%", true, juce::roundToInt (route->amount * 100.0f) == percent, [safeThis, id, slot, param, percent]
            {
                if (safeThis == nullptr)
                    return;
                safeThis->processorRef.performEdit ("LFO amount", [safeThis, id, slot, param, percent] { safeThis->processorRef.setFxLfoAmount (id, slot, param, (float) percent / 100.0f); });
                safeThis->refresh (true);
            });
        menu.addSubMenu (name + " amount  (" + juce::String (juce::roundToInt (route->amount * 100.0f)) + "%)", amounts);
        menu.addItem ("Remove from " + name, [safeThis, id, slot, param, name]
        {
            if (safeThis == nullptr)
                return;
            safeThis->processorRef.performEdit ("Remove from " + name, [safeThis, id, slot, param] { safeThis->processorRef.assignFxLfo (id, slot, param, false); });
            safeThis->refresh (true);
        });
    }
    juce::Component::SafePointer<PatchPage> safeThis (this);
    if (cabled == 0 && (int) patch.lfos.size() < FxLfo::maxLfos)
        menu.addItem ("Assign to a new LFO", [safeThis, slot, param]
        {
            if (safeThis == nullptr)
                return;
            // The new box above this effect's, cabled to it.
            juce::Point<float> at;
            for (const auto& node : safeThis->shown.nodes())
                if (safeThis->slotOf (&node) == slot)
                    at = { node.x, node.y - 110.0f };
            const auto x = std::round ((at.x - safeThis->fxShift.x) / 10.0f) * 10.0f, y = std::round ((at.y - safeThis->fxShift.y) / 10.0f) * 10.0f;
            safeThis->processorRef.performEdit ("Assign to a new LFO", [safeThis, slot, param, x, y]
            {
                if (const auto id = safeThis->processorRef.addFxLfo (x, y); id > 0)
                    safeThis->processorRef.assignFxLfo (id, slot, param, true, 0.5f);
            });
            safeThis->refresh (true);
        });
}

juce::Colour PatchPage::knobLfoColour (int slot, const juce::String& param) const
{
    for (const auto& route : lfoPatch.lfoRoutes)
        if (route.slot == slot && route.param == param)
            return fxLfoColour (route.lfo);
    return juce::Colours::transparentBlack;
}

void PatchPage::timerCallback ()
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
    const auto* mod = selectedModCable();
    if (mod != nullptr)
    {
        const auto slot = slotOf (shown.find (mod->to.node));
        const auto* from = shown.find (mod->from.node);
        const auto shownBefore = lfoAmounts->numShown();
        const auto* to = shown.find (mod->to.node);
        lfoAmounts->show (from != nullptr ? lfoOf (*from) : 0, slot, targetsOf (slot), to != nullptr ? fxTypeName (to->kind) : juce::String(),
                          from != nullptr ? fxLfoColour (lfoOf (*from)) : IlanaTheme::accent());
        if (shownBefore != lfoAmounts->numShown() || ! openTargetButton.isVisible())
            layoutInspector();
    }
    else if (openTargetButton.isVisible())
    {
        layoutInspector();
    }
    if (lfoControls != nullptr)
    {
        const auto synced = lfoControls->isSynced();
        lfoControls->show();
        if (synced != lfoControls->isSynced())
            layoutInspector();
        if (lfoDisplay != nullptr)
            lfoDisplay->repaint();
    }
    checkWindows();
    if (juce::Time::getMillisecondCounter() >= refusedUntil && refusedUntil != 0)
    {
        refusedUntil = 0;
        repaint (hint);
    }
    // The boxes' live lines (CPU, an LFO's rate, a stage's state), the loop guards and the LFO waves: the canvas
    // redraws only when one of them moved. (It used to redraw every tick: ~14 ms a paint over 72 % of the window
    // at 175 % in software, while the page sat idle.)
    {
        auto live = false;
        juce::String readouts;
        for (const auto& node : shown.nodes())
        {
            live = live || isLfo (node); // (an LFO box draws its wave and moving dot)
            readouts << statusOf (node) << '|';
        }
        if (processorRef.isFxPatcherOn())
            for (const auto& cable : shown.cables())
                readouts << (processorRef.getFxLoopGuard (cable.from.node, cable.from.port, cable.to.node, cable.to.port) < 0.98f ? '!' : '.');
        if (live || readouts != lastReadouts)
        {
            lastReadouts = readouts;
            view.repaint();
        }
    }
    repaint (inspector);
    updateZoomLabel();
}

void PatchPage::updateZoomLabel ()
{
    const auto text = juce::String (juce::roundToInt (view.getTargetZoom() * 100.0f)) + "%";
    if (zoomLabel.getButtonText() != text)
        zoomLabel.setButtonText (text);
    zoomOutButton.setEnabled (view.getTargetZoom() > PatchCableView::minViewZoom + 0.001f);
    zoomInButton.setEnabled (view.getTargetZoom() < PatchCableView::maxViewZoom - 0.001f);
}
