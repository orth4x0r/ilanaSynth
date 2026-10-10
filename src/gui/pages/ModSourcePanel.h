// Included only from PluginEditor.cpp, after EnvLfoPages.h; the contents stay
// in the same anonymous namespace.
#pragma once

namespace
{
// A modulation source edited where you are (ilana, 2026-10-09, after Vital
// and Serum 2: "dragging works everywhere, but the sources are spread over
// tabs"): PLAY's MODULATION card and the dock's pop-out editor both show the
// selected source's graph, its main controls and what it drives, so a
// source can be dragged, shaped and its depths set without a trip to MOD.

// The matrix slots a source drives: as the slot's source, its LFO's second
// output, or its VIA. Bypassed and zero-depth routes count (they are listed,
// dimmed), so a route set to 0 % doesn't vanish under the mouse.
inline std::vector<int> modSlotsDrivenBy (const IlanaSynthAudioProcessor& processor, int source)
{
    std::vector<int> slots;
    const auto s = (Mod::Source) source;
    const auto lfo = Mod::lfoIndexFor (s);
    const auto sourceB = lfo >= 0 ? Mod::lfoBSourceFor (lfo) : Mod::Source::None;

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);
        if (routing.destination == 0 || routing.source == Mod::Source::None)
            continue;
        if (routing.source == s || routing.aux == s || (sourceB != Mod::Source::None && routing.source == sourceB))
            slots.push_back (slot);
    }
    return slots;
}

// How many knobs a source moves now (the badge on its tab and chip).
inline int modRouteCount (const IlanaSynthAudioProcessor& processor, int source)
{
    auto count = 0;
    for (const auto slot : modSlotsDrivenBy (processor, source))
        count += processor.readModSlot (slot).isActive() ? 1 : 0;
    return count;
}

inline juce::Colour sourceTabColour (int source)
{
    return source == opEnvSourceId ? OperatorPool::colour() : modSourceColour (source);
}

inline juce::String sourceTabName (int source, const IlanaSynthAudioProcessor& processor)
{
    return source == opEnvSourceId ? juce::String ("OP ENV") : ModNames::sourceUpper (source, &processor);
}

// OP ENV's list of what plays it: each oscillator's OUTPUT as a bar, under a
// header that folds it away.
class OperatorEnvList : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit OperatorEnvList (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("What plays the Operator Env: each oscillator's OUTPUT, as a bar. Click to open or close the list.");
        startTimerHz (4);
    }

    std::function<void()> onToggle;
    bool isOpen() const { return open; }
    static constexpr int headerHeight = 24, rowHeight = 16;
    int getIdealHeight() const { return headerHeight + (open ? 4 + rowHeight * juce::jmax (1, (int) OperatorPool::operatorsOnEnv (processorRef).size()) + 4 : 0); }

    void paint (juce::Graphics& g) override
    {
        const auto operators = OperatorPool::operatorsOnEnv (processorRef);
        const auto count = (int) operators.size();
        auto area = getLocalBounds();
        auto header = area.removeFromTop (headerHeight);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (header.toFloat().reduced (0.5f), 6.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        g.drawText (juce::String (count) + (count == 1 ? " OSCILLATOR PLAYS IT  " : " OSCILLATORS PLAY IT  ")
                        + juce::String::fromUTF8 (open ? "\xe2\x96\xb4" : "\xe2\x96\xbe"),
                    header.reduced (10, 0), juce::Justification::centredLeft);
        if (! open)
            return;

        area.removeFromTop (4);
        for (int i = 0; i < count && area.getHeight() >= rowHeight; ++i)
        {
            const auto osc = operators[(size_t) i];
            const auto* parameter = processorRef.apvts.getParameter (OscRole::prefix (osc) + "_eg_out");
            if (parameter == nullptr)
                continue;
            auto row = area.removeFromTop (rowHeight).reduced (4, 0);
            g.setColour (IlanaTheme::oscColour (osc));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText ("OSC " + juce::String (osc + 1), row.removeFromLeft (52), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawText (parameter->getCurrentValueAsText(), row.removeFromRight (62), juce::Justification::centredRight);
            const auto bar = row.withSizeKeepingCentre (row.getWidth() - 8, 6).toFloat();
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (IlanaTheme::oscColour (osc).withAlpha (0.85f));
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * parameter->getValue())), 3.0f);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.y < headerHeight && ! event.mouseWasDraggedSinceMouseDown())
        {
            open = ! open;
            if (onToggle != nullptr)
                onToggle();
            repaint();
        }
    }

private:
    void timerCallback() override
    {
        if (open && isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    bool open = true; // (open, as PLAY's OP ENV card listed them: ilana, 2026-10-09)
};

// "DRIVES": each route of one source, its target, a bar of its depth and the
// number. Drag a bar sideways to set the depth (shift for fine), double-click
// it to zero, click the target to open its matrix row, right-click for
// bypass and remove. Collapsible to its header line (PLAY folds it when the
// card is short: the controls come first).
class SourceRouteList : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit SourceRouteList (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("What this source drives. Drag a bar sideways to set that route's depth (shift: fine), double-click "
                    "it for 0 %, click a target to open its row in MATRIX, right-click to bypass or remove it.");
        startTimerHz (12);
    }

    void setSource (int newSource, juce::Colour newColour)
    {
        source = newSource;
        colour = newColour;
        refresh (true);
    }

    // Folded: the header line only, with the count; a click on it opens it.
    void setCollapsed (bool shouldCollapse)
    {
        if (collapsed != shouldCollapse)
        {
            collapsed = shouldCollapse;
            if (onCollapsedChange != nullptr)
                onCollapsedChange (collapsed);
            repaint();
        }
    }

    bool isCollapsed() const { return collapsed; }
    std::function<void (bool)> onCollapsedChange;
    // The header clicked (folded or opened by hand).
    std::function<void (bool collapsed)> onUserToggle;

    static constexpr int headerHeight = 22, rowHeight = 18;

    // The height that lists every route (at least one row, for the empty note).
    int getIdealHeight() const { return headerHeight + 6 + juce::jmax (1, (int) slots.size()) * rowHeight + 6; }
    int getNumRoutes() const { return (int) slots.size(); }
    int getRouteSlot (int row) const { return juce::isPositiveAndBelow (row, (int) slots.size()) ? slots[(size_t) row] : -1; }
    juce::Rectangle<int> getRowBounds (int row) const { return rowArea (row); }
    juce::Rectangle<int> getBarBounds (int row) const { return barArea (rowArea (row)); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::well);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        auto header = headerArea();
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
        g.setColour (colour);
        const auto caret = juce::String::fromUTF8 (collapsed ? "\xe2\x96\xb8  " : "\xe2\x96\xbe  ");
        g.drawText (caret + "DRIVES", header, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text3);
        const auto count = (int) slots.size();
        g.drawText (juce::String (count) + (count == 1 ? " ROUTE" : " ROUTES"), header, juce::Justification::centredRight);

        if (collapsed)
            return;

        if (slots.empty())
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.setColour (IlanaTheme::Ui::text3);
            IlanaTheme::drawFitted (g, "Nothing yet. Drag " + ModNames::sourceUpper (source, &processorRef)
                                           + " onto any knob; its routes and depths show here.",
                                    rowArea (0), juce::Justification::centredLeft, 1);
            return;
        }

        for (int row = 0; row < count; ++row)
        {
            const auto area = rowArea (row);
            if (area.getBottom() > getHeight() - 2)
                break;

            const auto routing = processorRef.readModSlot (slots[(size_t) row]);
            const auto live = routing.isActive();
            const auto hot = row == hoverRow || row == dragRow;
            const auto tint = colour.withAlpha (live ? (hot ? 1.0f : 0.85f) : 0.4f);
            const auto lfo = Mod::lfoIndexFor ((Mod::Source) source);
            const auto viaB = lfo >= 0 && routing.source == Mod::lfoBSourceFor (lfo);
            const auto prefix = viaB ? juce::String ("OUT 2  ") : routing.source != (Mod::Source) source ? juce::String ("VIA  ") : juce::String();

            auto cells = area;
            const auto name = cells.removeFromLeft (nameWidth (area));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.setColour (tint);
            IlanaTheme::drawFitted (g, prefix + ModNames::destination (routing.destination, processorRef), name.withTrimmedRight (8),
                                    juce::Justification::centredLeft, 1);
            if (hot && hoverName)
                g.fillRect (name.getX(), name.getBottom() - 3, juce::jmin (name.getWidth() - 8, juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), prefix + ModNames::destination (routing.destination, processorRef))), 1);

            const auto depth = juce::roundToInt (routing.depth * 100.0f);
            g.setColour (live ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (routing.bypass ? juce::String ("OFF") : (depth > 0 ? "+" : "") + juce::String (depth) + "%", cells.removeFromRight (valueWidth),
                        juce::Justification::centredRight);

            // The depth as a bar from the centre: right for positive, left
            // for negative (as the knob rings swing).
            const auto bar = barArea (area).toFloat();
            g.setColour (juce::Colours::white.withAlpha (hot ? 0.1f : 0.06f));
            g.fillRoundedRectangle (bar, 2.5f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
            const auto half = bar.getWidth() * 0.5f * juce::jlimit (0.0f, 1.0f, std::abs (routing.depth));
            const auto fill = routing.depth >= 0.0f ? juce::Rectangle<float> (bar.getCentreX(), bar.getY(), half, bar.getHeight())
                                                    : juce::Rectangle<float> (bar.getCentreX() - half, bar.getY(), half, bar.getHeight());
            g.setColour (colour.withAlpha (live ? (hot ? 0.95f : 0.7f) : 0.3f));
            g.fillRoundedRectangle (fill, 2.5f);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override { updateHover (event.getPosition()); }
    void mouseExit (const juce::MouseEvent&) override { updateHover ({ -1, -1 }); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragRow = -1;

        if (headerArea().expanded (0, 3).contains (event.getPosition()))
        {
            setCollapsed (! collapsed);
            if (onUserToggle != nullptr)
                onUserToggle (collapsed);
            return;
        }

        const auto row = rowAt (event.getPosition());
        if (row < 0)
            return;

        const auto slot = slots[(size_t) row];

        if (event.mods.isPopupMenu())
        {
            showMenu (slot);
            return;
        }

        if (event.x < rowArea (row).getX() + nameWidth (rowArea (row)))
        {
            if (ModNames::openMatrixRow() != nullptr)
                ModNames::openMatrixRow() (slot);
            return;
        }

        dragRow = row;
        dragStartDepth = processorRef.readModSlot (slot).depth;
        processorRef.beginEdit (ModNames::source (source, &processorRef) + " depth");
        editing = true;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragRow < 0 || ! juce::isPositiveAndBelow (dragRow, (int) slots.size()))
            return;

        const auto bar = barArea (rowArea (dragRow));
        const auto scale = (event.mods.isShiftDown() ? 0.25f : 1.0f) * 2.0f / (float) juce::jmax (20, bar.getWidth());
        const auto depth = juce::jlimit (-1.0f, 1.0f, dragStartDepth + (float) event.getDistanceFromDragStartX() * scale);
        processorRef.setModSlotValue (slots[(size_t) dragRow], "amt", depth);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (editing)
            processorRef.endEdit();
        editing = false;
        dragRow = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.getPosition());
        if (row < 0 || event.x < rowArea (row).getX() + nameWidth (rowArea (row)))
            return;
        const auto slot = slots[(size_t) row];
        processorRef.performEdit ("Zero " + ModNames::source (source, &processorRef) + " depth",
                                  [this, slot] { processorRef.setModSlotValue (slot, "amt", 0.0f); });
        repaint();
    }

private:
    juce::Rectangle<int> headerArea() const { return getLocalBounds().reduced (10, 0).removeFromTop (headerHeight).withTrimmedTop (4); }

    juce::Rectangle<int> rowArea (int row) const
    {
        return { 10, headerHeight + 4 + row * rowHeight, getWidth() - 20, rowHeight };
    }

    // The targets' column: as wide as the longest target (no wider than a
    // third of the row), so the bars start right after the names.
    int nameWidth (juce::Rectangle<int> row) const { return juce::jlimit (80, juce::jmax (80, row.getWidth() / 3), longestName + 18); }
    static constexpr int valueWidth = 46;

    juce::Rectangle<int> barArea (juce::Rectangle<int> row) const
    {
        row.removeFromLeft (nameWidth (row));
        row.removeFromRight (valueWidth + 8);
        return row.withSizeKeepingCentre (row.getWidth(), 6);
    }

    int rowAt (juce::Point<int> position) const
    {
        if (collapsed)
            return -1;
        for (int row = 0; row < (int) slots.size(); ++row)
            if (rowArea (row).contains (position))
                return row;
        return -1;
    }

    void updateHover (juce::Point<int> position)
    {
        const auto row = rowAt (position);
        const auto onName = row >= 0 && position.x < rowArea (row).getX() + nameWidth (rowArea (row));
        if (row != hoverRow || onName != hoverName)
        {
            hoverRow = row;
            hoverName = onName;
            setMouseCursor (headerArea().contains (position) || onName ? juce::MouseCursor::PointingHandCursor
                            : row >= 0                               ? juce::MouseCursor::LeftRightResizeCursor
                                                                     : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void showMenu (int slot)
    {
        const auto routing = processorRef.readModSlot (slot);
        juce::PopupMenu menu;
        menu.addSectionHeader (ModNames::destination (routing.destination, processorRef));
        menu.addItem (1, "Bypass", true, routing.bypass);
        menu.addItem (2, "Open in MATRIX");
        menu.addItem (3, "Remove");
        juce::Component::SafePointer<SourceRouteList> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis, slot, bypass = routing.bypass] (int result)
        {
            if (safeThis == nullptr)
                return;
            auto& p = safeThis->processorRef;
            if (result == 1)
                p.performEdit (bypass ? "Enable modulation" : "Bypass modulation",
                               [&p, slot, bypass] { p.setModSlotValue (slot, "byp", bypass ? 0.0f : 1.0f); });
            else if (result == 2 && ModNames::openMatrixRow() != nullptr)
                ModNames::openMatrixRow() (slot);
            else if (result == 3)
                p.performEdit ("Remove modulation", [&p, slot] { p.clearModSlot (slot); });
            safeThis->refresh (true);
        });
    }

    // Repaint only when a route of this source changed (added, removed,
    // moved, its depth set from a knob ring).
    void refresh (bool force)
    {
        auto next = modSlotsDrivenBy (processorRef, source);
        auto signature = (juce::int64) next.size();
        for (const auto slot : next)
        {
            const auto routing = processorRef.readModSlot (slot);
            signature = signature * 31 + slot * 977 + routing.destination * 13 + juce::roundToInt (routing.depth * 1000.0f) + (routing.bypass ? 7 : 0);
        }
        if (force || signature != lastSignature)
        {
            lastSignature = signature;
            const auto countChanged = next.size() != slots.size();
            slots = std::move (next);
            longestName = 0;
            const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label));
            for (const auto slot : slots)
            {
                const auto routing = processorRef.readModSlot (slot);
                const auto lfo = Mod::lfoIndexFor ((Mod::Source) source);
                const auto prefix = lfo >= 0 && routing.source == Mod::lfoBSourceFor (lfo) ? juce::String ("OUT 2  ")
                                    : routing.source != (Mod::Source) source                  ? juce::String ("VIA  ")
                                                                                              : juce::String();
                longestName = juce::jmax (longestName, juce::GlyphArrangement::getStringWidthInt (font, prefix + ModNames::destination (routing.destination, processorRef)));
            }
            if (countChanged && onRoutesChanged != nullptr)
                onRoutesChanged();
            repaint();
        }
    }

    void timerCallback() override
    {
        if (isShowing() && dragRow < 0)
            refresh (false);
    }

public:
    // The number of routes changed (the owner may re-lay itself out).
    std::function<void()> onRoutesChanged;

private:
    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    juce::Colour colour { IlanaTheme::accent() };
    std::vector<int> slots;
    juce::int64 lastSignature = -1;
    bool collapsed = false, editing = false, hoverName = false;
    int hoverRow = -1, dragRow = -1, longestName = 0;
    float dragStartDepth = 0.0f;
};

// One source's editor: its graph and main controls side by side over its
// DRIVES list. An LFO: the drawn shape, SHAPE, RATE, SMOOTH, SYNC and
// RETRIG. An envelope: the graph and ATTACK, DECAY, SUSTAIN and RELEASE.
// Anything else (velocity, the wheel, a macro...) has no settings: a line
// on what it is, its live value, and its routes. Its rarer settings stay
// on MOD (onOpenInMod).
class SourceEditor : public juce::Component,
                     private juce::Timer
{
public:
    explicit SourceEditor (IlanaSynthAudioProcessor& p) : processorRef (p), routes (p), operatorList (p)
    {
        addAndMakeVisible (routes);
        addChildComponent (operatorList);
        operatorList.onToggle = [this] { resized(); };
        routes.onRoutesChanged = [this] { resized(); };
        routes.onCollapsedChange = [this] (bool) { resized(); };
        // An unused AMP ENV on an operator voice: the way to what plays instead.
        styleJumpLink (opEnvLink, "OP ENV");
        opEnvLink.setTooltip ("Each operator plays its own copy of the Operator Env; AMP ENV is unused. Opens it on FM.");
        opEnvLink.onClick = [this] { if (onEditOperatorEnv != nullptr) onEditOperatorEnv(); };
        addChildComponent (opEnvLink);
        startTimerHz (20);
    }

    // The envelope (0-15, the pool's order) a source is, or -1.
    static int envelopeOf (int source)
    {
        for (int env = 0; env < 16; ++env)
            if ((int) envelopeSource (env) == source)
                return env;
        return -1;
    }

    static juce::String envelopePrefix (int env)
    {
        const char* const prefixes[] { "amp", "fe", "f2e", "me", "e4" };
        return env < 5 ? juce::String (prefixes[juce::jlimit (0, 4, env)]) : "env" + juce::String (env + 1);
    }

    int getSource() const { return source; }
    SourceRouteList& getRouteList() { return routes; }

    // OP ENV's picture: a click opens the operator's envelope on FM.
    std::function<void()> onEditOperatorEnv;

    void setSource (int newSource)
    {
        if (newSource == source && built)
            return;

        source = newSource;
        built = true;
        graph.reset();
        controls.clear();
        rate.reset();
        colour = sourceTabColour (source);

        const auto lfo = Mod::lfoIndexFor ((Mod::Source) source);
        const auto env = envelopeOf (source);
        auto& state = processorRef.apvts;

        if (source == opEnvSourceId)
        {
            auto overview = std::make_unique<OperatorEnvOverview> (processorRef);
            overview->onClick = [this] { if (onEditOperatorEnv != nullptr) onEditOperatorEnv(); };
            graph = std::move (overview);
            colour = OperatorPool::colour();
            kind = Kind::opEnv;
        }
        else if (lfo >= 0)
        {
            const auto prefix = "lfo" + juce::String (lfo + 1);
            graph = std::make_unique<LfoDisplay> (processorRef, lfo, colour);
            auto shape = std::make_unique<ComboControl> (state, prefix + "_shape", "SHAPE");
            LfoShapeMenu::apply (*shape, processorRef);
            shape->setTint (colour);
            controls.push_back (std::move (shape));
            rate = std::make_unique<LfoRateControl> (processorRef, lfo, colour, false);
            rate->addTo (*this);
            rate->getRateKnob().setSizeRole (IlanaTheme::KnobSize::compact);
            rate->getDivisionKnob().setSizeRole (IlanaTheme::KnobSize::compact);
            auto smooth = std::make_unique<KnobControl> (state, prefix + "_smooth", "SMOOTH", colour, false);
            smooth->setSizeRole (IlanaTheme::KnobSize::compact);
            controls.push_back (std::move (smooth));
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_sync", "SYNC" }, { "_retrig", "RETRIG" } })
            {
                auto toggle = std::make_unique<ToggleControl> (state, prefix + suffix, label);
                toggle->setSwitchColour (colour);
                controls.push_back (std::move (toggle));
            }
            kind = Kind::lfo;
        }
        else if (env >= 0)
        {
            const auto prefix = envelopePrefix (env);
            auto display = std::make_unique<EnvelopeDisplay> (processorRef, prefix, colour, false);
            display->setSlim (true);
            graph = std::move (display);
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_attack", "ATTACK" }, { "_decay", "DECAY" },
                                                 { "_sustain", "SUSTAIN" }, { "_release", "RELEASE" } })
            {
                auto knob = std::make_unique<KnobControl> (state, prefix + suffix, label, colour, false);
                knob->setSizeRole (IlanaTheme::KnobSize::compact);
                controls.push_back (std::move (knob));
            }
            kind = Kind::envelope;
        }
        else
        {
            kind = Kind::other;
        }

        if (graph != nullptr)
            addAndMakeVisible (*graph);
        for (auto& control : controls)
            addAndMakeVisible (*control);
        if (rate != nullptr)
            rate->setShown (true);

        routes.setVisible (kind != Kind::opEnv);
        operatorList.setVisible (kind == Kind::opEnv);
        if (kind != Kind::opEnv)
            routes.setSource (source, colour);
        history.fill (0.0f);
        unusedNote = "-"; // (refreshed below)
        refreshUnused();
        resized();
        repaint();
    }

    // Lay out with the DRIVES list folded to its header line (PLAY on a short card).
    void setRoutesFolded (bool fold)
    {
        if (fold != routes.isCollapsed())
        {
            routes.setCollapsed (fold);
            resized();
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (kind != Kind::other || graphArea.isEmpty())
            return;

        // No settings to show: what the source is, and its live value as a trace.
        IlanaTheme::paintWell (g, graphArea.toFloat(), 8.0f);
        auto area = graphArea.reduced (12, 10);
        g.setColour (colour);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        g.drawText (ModNames::sourceUpper (source, &processorRef), area.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        IlanaTheme::drawFitted (g, describeOther(), area.removeFromTop (34), juce::Justification::topLeft, 2);

        const auto plot = area.reduced (0, 4).toFloat();
        if (plot.getHeight() > 12.0f)
        {
            juce::Path trace;
            for (size_t i = 0; i < history.size(); ++i)
            {
                const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (history.size() - 1);
                const auto y = plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, history[(i + historyPos) % history.size()]);
                if (i == 0)
                    trace.startNewSubPath (x, y);
                else
                    trace.lineTo (x, y);
            }
            g.setColour (colour.withAlpha (0.85f));
            g.strokePath (trace, juce::PathStrokeType (2.0f));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (kind == Kind::opEnv)
        {
            // The picture, and under it what plays it (a drop-down).
            operatorList.setBounds (area.removeFromBottom (juce::jmin (operatorList.getIdealHeight(), area.getHeight() - minGraphHeight)));
            area.removeFromBottom (gap);
            graphArea = area;
            if (graph != nullptr)
                graph->setBounds (area);
            return;
        }

        // The DRIVES list takes what it needs (its whole list, up to half the
        // height); folded, one line. The graph and controls get the rest.
        const auto routesHeight = routes.isCollapsed() ? SourceRouteList::headerHeight + 6
                                                       : juce::jmin (routes.getIdealHeight(), juce::jmax (SourceRouteList::headerHeight + 6, area.getHeight() / 2));
        routes.setBounds (area.removeFromBottom (routesHeight));
        area.removeFromBottom (gap);

        // The graph on the left, the controls in a grid of equal cells on the
        // right, both the same height, so their edges line up.
        // An LFO's controls on a low area go in one row under SHAPE, which
        // wants a wider column.
        const auto lowLfo = kind == Kind::lfo && area.getHeight() < menuHeight + 6 + 2 * knobRowHeight;
        const auto controlsWidth = kind == Kind::other || kind == Kind::opEnv ? 0
                                   : lowLfo ? juce::jlimit (220, 300, area.getWidth() * 50 / 100)
                                            : juce::jlimit (180, 280, area.getWidth() * 40 / 100);
        auto controlArea = area.removeFromRight (controlsWidth);
        if (controlsWidth > 0)
            area.removeFromRight (gap);
        graphArea = area;

        if (graph != nullptr)
            graph->setBounds (graphArea);
        opEnvLink.setBounds (graphArea.getRight() - 8 - 104, graphArea.getY() + 8, 104, 20);

        if (kind == Kind::lfo && controls.size() == 4 && rate != nullptr && lowLfo)
        {
            // SHAPE across the top; RATE, SMOOTH, SYNC and RETRIG in one row
            // at the foot, level with the graph's bottom.
            controls[0]->setBounds (controlArea.removeFromTop (menuHeight));
            auto row = controlArea.removeFromBottom (knobRowHeight);
            const auto cell = row.getWidth() / 4;
            rate->setBounds (row.removeFromLeft (cell));
            controls[1]->setBounds (row.removeFromLeft (cell));
            // (The switches' names level with the knobs'.)
            layoutRow (row.withHeight (13 + 30), { controls[2].get(), controls[3].get() });
        }
        else if (kind == Kind::lfo && controls.size() == 4 && rate != nullptr)
        {
            // SHAPE across the top; RATE and SMOOTH; SYNC and RETRIG.
            controls[0]->setBounds (controlArea.removeFromTop (menuHeight));
            controlArea.removeFromTop (6);
            const auto rowHeight = controlArea.getHeight() / 2;
            auto knobs = controlArea.removeFromTop (rowHeight);
            auto switches = controlArea;
            rate->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
            controls[1]->setBounds (knobs);
            layoutRow (switches.withSizeKeepingCentre (switches.getWidth(), juce::jmin (switches.getHeight(), 13 + 30)),
                       { controls[2].get(), controls[3].get() });
        }
        else if (kind == Kind::envelope && controls.size() == 4)
        {
            // ATTACK DECAY over SUSTAIN RELEASE: a 2 x 2 grid.
            const auto top = controlArea.removeFromTop (controlArea.getHeight() / 2);
            layoutRow (top, { controls[0].get(), controls[1].get() });
            layoutRow (controlArea, { controls[2].get(), controls[3].get() });
        }
    }

private:
    enum class Kind { lfo, envelope, opEnv, other };
    static constexpr int gap = 10, menuHeight = 13 + 4 + 28, knobRowHeight = 70, minGraphHeight = 90;

    juce::String describeOther() const
    {
        using S = Mod::Source;
        const auto s = (S) source;
        if (s == S::Velocity) return "How hard each note is played, 0 to 1. Set once per note.";
        if (s == S::KeyTrack) return "The note's pitch: low keys low, high keys high (C3 in the middle).";
        if (s == S::ModWheel) return "The mod wheel (CC 1), for the whole voice.";
        if (s == S::Aftertouch) return "Pressure: channel or polyphonic aftertouch, and MPE pressure.";
        if (s == S::Random) return "A new random value for each note.";
        if (Mod::macroIndexFor (s) >= 0) return "A macro knob in the dock: one knob that moves every route it drives.";
        return "A modulation source. Its settings, if it has any, are on MOD.";
    }

    // An envelope nothing plays dims, and says so over its graph (UI review
    // 6, I6-2; one wording with MOD: S8-17).
    void paintOverChildren (juce::Graphics& g) override
    {
        if (unusedNote.isEmpty() || graphArea.isEmpty())
            return;
        // (Clear of the LFO graph's GRID menu and of EDIT OP ENV.)
        auto pill = graphArea.reduced (8).withHeight (20);
        pill.setRight (opEnvLink.isVisible() ? opEnvLink.getX() - 6 : pill.getRight());
        // An LFO's hint sits across the middle of its graph, where the whole
        // sentence fits (A16-6: it was cut by the GRID menu's corner).
        if (kind == Kind::lfo)
            pill = pill.withY (graphArea.getCentreY() - 10);
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label);
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
        g.fillRoundedRectangle (pill.toFloat(), 4.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (font);
        IlanaTheme::drawFitted (g, unusedNote, pill.reduced (8, 0), juce::Justification::centredLeft, 1);
    }

    void refreshUnused()
    {
        const auto env = kind == Kind::envelope ? envelopeOf (source) : -1;
        const auto lfo = kind == Kind::lfo ? Mod::lfoIndexFor ((Mod::Source) source) : -1;
        const auto inUse = env >= 0 ? envelopeInUse (processorRef, env)
                         : lfo >= 0 ? modSourceRouted (processorRef, Mod::lfoSourceFor (lfo)) || modSourceRouted (processorRef, Mod::lfoBSourceFor (lfo))
                                    : true;
        const auto ampOnOperators = env == 0 && ! inUse && FmOperatorInfo::anyOperatorEnv (processorRef);
        const auto note = inUse            ? juce::String()
                          : ampOnOperators ? juce::String (EnvSection::ampUnusedText())
                          : lfo >= 0       ? juce::String ("unused: drag it onto a knob to use it")
                                           : juce::String ("unused: nothing plays this envelope yet");
        if (note == unusedNote)
            return;
        unusedNote = note;
        const auto alpha = inUse ? 1.0f : IlanaTheme::dimmedAlpha;
        if (graph != nullptr && (kind == Kind::envelope || kind == Kind::lfo))
            graph->setAlpha (alpha);
        for (auto& control : controls)
            control->setAlpha (alpha);
        if (rate != nullptr)
        {
            rate->getRateKnob().setAlpha (alpha);
            rate->getDivisionKnob().setAlpha (alpha);
        }
        opEnvLink.setVisible (ampOnOperators);
        repaint();
    }

    void timerCallback() override
    {
        if ((kind == Kind::envelope || kind == Kind::lfo) && isShowing())
            refreshUnused();
        if (kind != Kind::other || ! isShowing())
            return;
        history[historyPos] = std::abs (processorRef.getSourceDisplayValue (source));
        historyPos = (historyPos + 1) % history.size();
        repaint (graphArea);
    }

    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    bool built = false;
    Kind kind = Kind::other;
    juce::Colour colour { IlanaTheme::accent() };
    std::unique_ptr<juce::Component> graph;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<LfoRateControl> rate;
    SourceRouteList routes;
    OperatorEnvList operatorList;
    juce::TextButton opEnvLink;
    juce::String unusedNote;
    juce::Rectangle<int> graphArea;
    std::array<float, 80> history {};
    size_t historyPos = 0;
};

// The dock's pop-out editor: a click on a source chip opens it above the
// dock on whatever page is showing, so a source is shaped and its depths
// set beside the knobs it drives. The header drags it anywhere in the page
// area; OPEN IN MOD jumps to the full editor; the cross, Escape or a second
// click on the chip closes it. While it is open its source is pinned (the
// knobs it drives stay lit).
class SourcePopover : public juce::Component
{
public:
    explicit SourcePopover (IlanaSynthAudioProcessor& p) : processorRef (p), editor (p)
    {
        // The assignment bars wait behind DRIVES, as on PLAY.
        editor.setRoutesFolded (true);
        addAndMakeVisible (editor);
        closeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")));
        closeButton.setTooltip ("Close (Escape, or click the source's chip again)");
        closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
        openButton.setButtonText (juce::String::fromUTF8 ("OPEN IN MOD \xe2\x80\xba"));
        openButton.setTooltip ("This source's full editor on MOD > ENV / LFO, with its rarer settings");
        openButton.onClick = [this] { if (onOpenInMod != nullptr) onOpenInMod (editor.getSource()); };
        addAndMakeVisible (closeButton);
        addAndMakeVisible (openButton);
        setWantsKeyboardFocus (true);
    }

    std::function<void()> onClose;
    std::function<void (int source)> onOpenInMod;

    int getSource() const { return editor.getSource(); }
    SourceEditor& getEditor() { return editor; }

    // The source's full editor lives on MOD only for LFOs and envelopes.
    void setSource (int source)
    {
        editor.setSource (source);
        openButton.setVisible (Mod::lfoIndexFor ((Mod::Source) source) >= 0 || SourceEditor::envelopeOf (source) >= 0);
        resized();
        repaint();
    }

    static constexpr int preferredWidth = 620, preferredHeight = 340, headerHeight = 38;

    // Above the chip it came from, inside the page area; where it was
    // dragged to once moved.
    juce::Rectangle<int> placeIn (juce::Rectangle<int> area, juce::Rectangle<int> chip)
    {
        pageArea = area;
        const auto size = juce::Point<int> (juce::jmin (preferredWidth, area.getWidth() - 16), juce::jmin (preferredHeight, area.getHeight() - 16));
        if (! moved)
            topLeft = { chip.getX() - 6, area.getBottom() - 8 - size.y };
        return juce::Rectangle<int> (topLeft.x, topLeft.y, size.x, size.y).constrainedWithin (area.reduced (4));
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (4.0f);
        for (int ring = 5; ring >= 1; --ring)
        {
            g.setColour (juce::Colours::black.withAlpha (0.13f));
            g.fillRoundedRectangle (bounds.expanded ((float) ring).translated (0.0f, 3.0f), 10.0f + (float) ring);
        }
        const auto colour = modSourceColour (editor.getSource());
        IlanaTheme::paintCard (g, bounds, 10.0f, colour.withAlpha (0.55f));

        auto header = bounds.toNearestInt().reduced (16, 0).removeFromTop (headerHeight);
        const auto count = modRouteCount (processorRef, editor.getSource());
        IlanaTheme::paintCardHeader (g, header, ModNames::sourceUpper (editor.getSource(), &processorRef),
                                     count == 0 ? juce::String ("drag its chip onto a knob") : juce::String (count) + (count == 1 ? " route" : " routes"),
                                     colour, 200);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        auto header = area.removeFromTop (headerHeight).reduced (12, 0).withSizeKeepingCentre (area.getWidth() - 24, 22);
        closeButton.setBounds (header.removeFromRight (24));
        header.removeFromRight (6);
        openButton.setBounds (header.removeFromRight (124));
        editor.setBounds (area.reduced (14, 0).withTrimmedBottom (14));
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (event.y < headerHeight + 4 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragging = event.y < headerHeight + 4;
        dragStart = getPosition();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! dragging)
            return;
        auto next = getBounds().withPosition (dragStart + event.getOffsetFromDragStart());
        if (! pageArea.isEmpty())
            next = next.constrainedWithin (pageArea.reduced (4));
        topLeft = next.getPosition();
        moved = true;
        setBounds (next);
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey && onClose != nullptr)
        {
            onClose();
            return true;
        }
        return false;
    }

private:
    IlanaSynthAudioProcessor& processorRef;
    SourceEditor editor;
    juce::TextButton closeButton, openButton;
    juce::Rectangle<int> pageArea;
    juce::Point<int> topLeft, dragStart;
    bool moved = false, dragging = false;
};

// PLAY's MODULATION card tabs: one per source in the patch (the envelopes
// and LFOs in their pools, the MSEG and the Operator Env's while used),
// in equal columns, so the row reads as a grid rather than ragged pills.
// Each tab is the source's drag handle (the grip at its right), says how
// many knobs it moves, and a click shows it in the card's editor.
class SourceTab : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    SourceTab (IlanaSynthAudioProcessor& p, int sourceIndex) : processorRef (p), source (sourceIndex)
    {
        setTooltip (source == opEnvSourceId
                        ? juce::String ("OP ENV\nThe Operator Env: the DX7 envelope each oscillator on it plays (its level). It shapes its "
                                        "operators only, so it isn't a modulation source: nothing to drag.")
                        : ModNames::source (source, &processorRef) + "\nClick to edit it here; drag it onto any knob to modulate that knob.");
    }

    std::function<void (int)> onSelect;
    int getSource() const { return source; }

    void setSelected (bool shouldSelect)
    {
        if (selected != shouldSelect)
        {
            selected = shouldSelect;
            repaint();
        }
    }

    void setRouteCount (int count)
    {
        if (count != routes)
        {
            routes = count;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto colour = sourceTabColour (source);
        const auto hot = isMouseOver() || selected;
        g.setColour (selected ? IlanaTheme::Ui::panel.interpolatedWith (colour, 0.2f) : IlanaTheme::Ui::raised.interpolatedWith (colour, isMouseOver() ? 0.06f : 0.0f));
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (selected ? colour.withAlpha (0.8f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);

        auto area = getLocalBounds().reduced (9, 0);
        // The count badge (or a plain dot while it drives nothing).
        const auto badge = area.removeFromLeft (18);
        if (routes > 0)
        {
            const auto circle = juce::Rectangle<float> (16.0f, 16.0f).withCentre (badge.getCentre().toFloat());
            g.setColour (colour);
            g.fillEllipse (circle);
            g.setColour (IlanaTheme::Ui::header);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (juce::String (routes), circle, juce::Justification::centred);
        }
        else
        {
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (badge.getCentre().toFloat()));
        }

        // The grip: two columns of dots, the drag handle (none on OP ENV).
        const auto grip = area.removeFromRight (10);
        if (source != opEnvSourceId)
        g.setColour ((hot ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3).withAlpha (0.9f));
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column)
                g.fillEllipse ((float) grip.getX() + 2.0f + (float) column * 4.0f, (float) grip.getCentreY() - 5.0f + (float) row * 4.0f, 2.2f, 2.2f);

        g.setColour (hot ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true));
        IlanaTheme::drawFitted (g, sourceTabName (source, processorRef), area.reduced (6, 0), juce::Justification::centred, 1);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        if (source != opEnvSourceId)
            highlightedModSource() = source;
        repaint();
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (highlightedModSource() == source)
            highlightedModSource() = 0;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasDraggedSinceMouseDown() && ! event.mods.isPopupMenu() && onSelect != nullptr)
            onSelect (source);
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (source == opEnvSourceId)
            return;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String (source), this, juce::ScaledImage (image), true);
            }
    }

private:
    IlanaSynthAudioProcessor& processorRef;
    int source = 0;
    int routes = 0;
    bool selected = false;
};

// PLAY's MODULATION card (option 1 of the 2026-10-09 plan, after Serum 2's
// mod section): the ENVELOPE and LFO cards in one. A grid of source tabs
// (the envelopes and LFOs in the patch, then "+"), all one width so the rows
// line up with the card's edges; under them the selected source's editor
// and its DRIVES list, closed to its header line until opened (ilana's rule
// for PLAY: settings before assignments), and folded again on a card too
// short for both.
class ModulationCard : public juce::Component,
                       public ModulationCardView,
                       private juce::Timer
{
public:
    explicit ModulationCard (IlanaSynthAudioProcessor& p) : processorRef (p), editor (p)
    {
        addAndMakeVisible (editor);
        editor.onEditOperatorEnv = [this] { if (onEditOperatorEnv != nullptr) onEditOperatorEnv(); };
        editor.getRouteList().onUserToggle = [this] (bool collapsed) { routesOpen = ! collapsed; };
        addTab.setButtonText ("+");
        addTab.setTooltip ("Add an LFO or envelope to the patch");
        addTab.onClick = [this] { showAddMenu(); };
        addAndMakeVisible (addTab);
        moreTab.setTooltip ("The sources that don't fit");
        moreTab.onClick = [this] { showMoreMenu(); };
        addChildComponent (moreTab);
        refreshSources();
        startTimerHz (6);
    }

    // EDIT ›: the selected source's full editor on MOD (OP ENV's: on FM).
    std::function<void (int source)> onOpenInMod;

    // OP ENV's editor (FM, the first operator on it).
    std::function<void()> onEditOperatorEnv;

    int getSelectedSource() const override { return selected; }
    const std::vector<int>& getSources() const { return sources; }
    std::vector<int> getSourceList() const override { return sources; }
    void selectSource (int source) override { select (source); }
    int sourceOfTab (const juce::Component& tab) const override
    {
        const auto* sourceTab = dynamic_cast<const SourceTab*> (&tab);
        return sourceTab != nullptr ? sourceTab->getSource() : 0;
    }
    std::vector<juce::Component*> getSourceTabs() const override
    {
        std::vector<juce::Component*> shown;
        for (auto* tab : getTabs())
            shown.push_back (tab);
        return shown;
    }
    SourceEditor& getEditor() { return editor; }
    std::vector<SourceTab*> getTabs() const
    {
        std::vector<SourceTab*> shown;
        for (auto& tab : tabs)
            if (tab->isVisible())
                shown.push_back (tab.get());
        std::sort (shown.begin(), shown.end(), [] (const SourceTab* a, const SourceTab* b)
                   { return a->getY() != b->getY() ? a->getY() < b->getY() : a->getX() < b->getX(); });
        return shown;
    }

    void select (int source)
    {
        selected = source;
        for (auto& tab : tabs)
            tab->setSelected (tab->getSource() == selected);
        editor.setSource (selected);
        resized(); // (the selected tab comes into view)
        repaint();
    }

    static constexpr int headerHeight = 30, pad = 10, tabHeight = 28, tabGap = 6, minTabWidth = 104, maxRows = 2;

    void paint (juce::Graphics& g) override
    {
        const auto colour = sourceTabColour (selected);
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 6.0f, colour);
        auto header = getLocalBounds().withHeight (headerHeight).withTrimmedLeft (12).withTrimmedRight (12);
        IlanaTheme::paintCardHeader (g, header, "MODULATION", "drag a tab onto any knob", colour, editWidth + 8);

        const auto link = editLinkArea();
        g.setColour (hoverEdit ? colour.brighter (0.25f) : colour);
        g.setFont (IlanaTheme::linkFont());
        g.drawText (juce::String ("EDIT ") + juce::String::fromUTF8 ("\xe2\x80\xba"), link, juce::Justification::centredRight);
    }

    void resized() override
    {
        // The tab grid: as many equal columns as fit (the "+" is a cell), on
        // at most two rows; past that the last cell is "N MORE".
        auto area = getLocalBounds().withTrimmedTop (headerHeight).reduced (pad, 0).withTrimmedBottom (pad);
        const auto cells = (int) sources.size() + 1;
        const auto columns = juce::jlimit (1, cells, (area.getWidth() + tabGap) / (minTabWidth + tabGap));
        const auto rows = juce::jlimit (1, maxRows, (cells + columns - 1) / columns);
        const auto capacity = columns * rows;
        const auto overflow = cells > capacity;
        const auto shownTabs = overflow ? capacity - 2 : (int) sources.size(); // (room for "N MORE" and "+")
        const auto cellWidth = (float) (area.getWidth() - (columns - 1) * tabGap) / (float) columns;
        const auto cell = [&] (int index)
        {
            const auto row = index / columns, column = index % columns;
            return juce::Rectangle<float> ((float) area.getX() + (float) column * (cellWidth + (float) tabGap),
                                           (float) (area.getY() + row * (tabHeight + tabGap)), cellWidth, (float) tabHeight).toNearestInt();
        };

        // The selected source always has its tab in view: picked from
        // "N MORE", it takes the last shown cell.
        std::vector<SourceTab*> order;
        for (auto& tab : tabs)
            order.push_back (tab.get());
        if (shownTabs > 0)
            for (size_t i = (size_t) shownTabs; i < order.size(); ++i)
                if (order[i]->getSource() == selected)
                    std::swap (order[i], order[(size_t) shownTabs - 1]);

        hiddenSources.clear();
        for (size_t i = 0; i < order.size(); ++i)
        {
            const auto shown = (int) i < shownTabs;
            order[i]->setVisible (shown);
            if (shown)
                order[i]->setBounds (cell ((int) i));
            else
                hiddenSources.push_back (order[i]->getSource());
        }
        auto next = shownTabs;
        moreTab.setVisible (overflow);
        if (overflow)
        {
            moreTab.setButtonText (juce::String ((int) hiddenSources.size()) + " MORE");
            moreTab.setBounds (cell (next++));
        }
        addTab.setBounds (cell (next));
        addTab.setVisible (anyToAdd());

        area.removeFromTop (rows * tabHeight + (rows - 1) * tabGap + pad);
        editorArea = area;
        layoutEditor();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto over = editLinkArea().contains (event.getPosition());
        if (over != hoverEdit)
        {
            hoverEdit = over;
            setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint (editLinkArea());
        }
    }

    void mouseExit (const juce::MouseEvent& event) override { mouseMove (event.withNewPosition (juce::Point<int> (-1, -1))); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (editLinkArea().contains (event.getPosition()))
            openSelectedInMod();
    }

    void openSelectedInMod() override
    {
        if (selected == opEnvSourceId)
        {
            if (onEditOperatorEnv != nullptr)
                onEditOperatorEnv();
        }
        else if (onOpenInMod != nullptr)
        {
            onOpenInMod (selected);
        }
    }

private:
    static constexpr int editWidth = 60;
    // The graph and controls keep at least this; the DRIVES list folds to
    // its header line before they get less (controls first).
    static constexpr int minControlsHeight = 130;

    juce::Rectangle<int> editLinkArea() const { return { getWidth() - 12 - editWidth, headerHeight / 2 - 8, editWidth, 16 }; }

    void layoutEditor()
    {
        editor.setRoutesFolded (! routesOpen || editorArea.getHeight() - editor.getRouteList().getIdealHeight() - 10 < minControlsHeight);
        editor.setBounds (editorArea);
    }

    // The sources the card lists: the envelopes and LFOs in their pools, the
    // patch's MSEG and the Operator Env's while it uses them.
    std::vector<int> wantedSources() const
    {
        std::vector<int> wanted;
        if (operatorPoolShown (processorRef))
            wanted.push_back (opEnvSourceId);
        if (operatorSourceShown (processorRef, Mod::Source::OpPitchEnv))
            wanted.push_back ((int) Mod::Source::OpPitchEnv);
        for (int env = 0; env < 16; ++env)
            if (envelopeShown (processorRef, env))
                wanted.push_back ((int) envelopeSource (env));
        if (operatorSourceShown (processorRef, Mod::Source::OpLfo))
            wanted.push_back ((int) Mod::Source::OpLfo);
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (processorRef.isLfoShown (lfo))
                wanted.push_back ((int) Mod::lfoSourceFor (lfo));
        if (msegModuleInUse (processorRef))
            wanted.push_back ((int) Mod::Source::Mseg);
        return wanted;
    }

    bool anyToAdd() const
    {
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processorRef.isLfoShown (lfo))
                return true;
        for (int env = 0; env < 16; ++env)
            if (! envelopeShown (processorRef, env))
                return true;
        return false;
    }

    void refreshSources()
    {
        auto wanted = wantedSources();
        if (wanted != sources)
        {
            sources = std::move (wanted);
            tabs.clear();
            for (const auto source : sources)
            {
                auto tab = std::make_unique<SourceTab> (processorRef, source);
                tab->onSelect = [this] (int picked) { select (picked); };
                addAndMakeVisible (*tab);
                tabs.push_back (std::move (tab));
            }
            // A selection that left the patch, or a patch that just became an
            // operator voice: its first LFO, else its first source.
            const auto opEnvArrived = ! hadOpEnv && ! sources.empty() && sources.front() == opEnvSourceId;
            hadOpEnv = ! sources.empty() && sources.front() == opEnvSourceId;
            if (opEnvArrived || std::find (sources.begin(), sources.end(), selected) == sources.end())
            {
                // (A DX7 voice opens on OP ENV, as its envelope card did: I8-18.)
                selected = sources.empty() ? (int) Mod::lfoSourceFor (0) : sources.front();
                if (selected != opEnvSourceId)
                    for (const auto source : sources)
                        if (Mod::lfoIndexFor ((Mod::Source) source) >= 0)
                        {
                            selected = source;
                            break;
                        }
            }
            resized();
            select (selected);
        }

        for (auto& tab : tabs)
            tab->setRouteCount (modRouteCount (processorRef, tab->getSource()));
    }

    void showAddMenu()
    {
        juce::PopupMenu lfos, envelopes;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processorRef.isLfoShown (lfo))
                lfos.addItem (1 + lfo, "LFO " + juce::String (lfo + 1));
        for (int env = 0; env < 16; ++env)
            if (! envelopeShown (processorRef, env))
                envelopes.addItem (100 + env, ModNames::source ((int) envelopeSource (env)));
        juce::PopupMenu menu;
        menu.addSectionHeader ("Add a source");
        if (lfos.getNumItems() > 0)
            menu.addSubMenu ("LFOs", lfos);
        if (envelopes.getNumItems() > 0)
            menu.addSubMenu ("Envelopes", envelopes);
        juce::Component::SafePointer<ModulationCard> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTab), [safeThis] (int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;
            auto& p = safeThis->processorRef;
            const auto isLfo = result < 100;
            const auto index = isLfo ? result - 1 : result - 100;
            p.setRevealed (isLfo ? IlanaSynthAudioProcessor::Module::Lfo : IlanaSynthAudioProcessor::Module::Envelope, index, true);
            safeThis->refreshSources();
            safeThis->select (isLfo ? (int) Mod::lfoSourceFor (index) : (int) envelopeSource (index));
        });
    }

    void showMoreMenu()
    {
        juce::PopupMenu menu;
        for (size_t i = 0; i < hiddenSources.size(); ++i)
            menu.addItem ((int) i + 1, sourceTabName (hiddenSources[i], processorRef), true, hiddenSources[i] == selected);
        juce::Component::SafePointer<ModulationCard> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreTab), [safeThis] (int result)
        {
            if (safeThis != nullptr && juce::isPositiveAndNotGreaterThan (result, (int) safeThis->hiddenSources.size()) && result > 0)
                safeThis->select (safeThis->hiddenSources[(size_t) result - 1]);
        });
    }

    void timerCallback() override
    {
        // (Visible up the tree: an offscreen editor, as in the UI tests, follows the pool too.)
        for (auto* c = static_cast<juce::Component*> (this); c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return;
        refreshSources();
    }

    IlanaSynthAudioProcessor& processorRef;
    SourceEditor editor;
    std::vector<int> sources, hiddenSources;
    std::vector<std::unique_ptr<SourceTab>> tabs;
    juce::TextButton addTab, moreTab;
    juce::Rectangle<int> editorArea;
    int selected = (int) Mod::lfoSourceFor (0);
    bool hadOpEnv = false;
    bool hoverEdit = false;
    // The DRIVES list is closed until opened (ilana, 2026-10-09: what a
    // source drives is a drop-down, the controls come first); opened, it
    // stays open from source to source.
    bool routesOpen = false;
};
} // namespace
