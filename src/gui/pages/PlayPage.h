// Included only from PluginEditor.cpp, after OscPage.h and ModSourcePanel.h; the
// contents stay in the same anonymous namespace.
#pragma once

namespace
{
// A small selector of two or three solid-fill pills (the slope, SUB's octave).
class PillChoice : public ParamBoundComponent
{
public:
    PillChoice (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::StringArray labelsIn, juce::Colour colourIn, const juce::String& tip, std::vector<int> valuesIn = {})
        : ParamBoundComponent (state, id), labels (std::move (labelsIn)), colour (colourIn), values (std::move (valuesIn))
    {
        if (values.empty())
            for (int i = 0; i < labels.size(); ++i)
                values.push_back (i);
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto width = bounds.getWidth() / (float) labels.size();
        const auto mouse = getMouseXYRelative().toFloat();
        for (int option = 0; option < labels.size(); ++option)
        {
            const auto cellBounds = juce::Rectangle<float> (bounds.getX() + width * (float) option, bounds.getY(), width, bounds.getHeight()).reduced (1.5f, 1.0f);
            const auto on = values[(size_t) option] == current;
            const auto hovered = isMouseOver() && cellBounds.contains (mouse);
            g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, hovered ? 0.12f : 0.0f));
            g.fillRoundedRectangle (cellBounds, 4.0f);
            if (! on)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (cellBounds.reduced (0.5f), 4.0f, 1.0f);
            }
            g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (8.5f, true));
            g.drawText (labels[option], cellBounds, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        setValue (values[(size_t) juce::jlimit (0, labels.size() - 1, (int) (event.position.x / (float) getWidth() * (float) labels.size()))]);
    }

private:
    juce::StringArray labels;
    juce::Colour colour;
    std::vector<int> values;
};

// An on/off as one pill (the LFO's SYNC, RETRIG and KEY): solid while on.
class PillToggle : public ParamBoundComponent
{
public:
    PillToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text, juce::Colour colourIn, const juce::String& tip)
        : ParamBoundComponent (state, id), label (text), colour (colourIn)
    {
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const auto on = current > 0;
        g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, isMouseOver() ? 0.12f : 0.0f));
        const auto radius = bounds.getHeight() * 0.5f;
        g.fillRoundedRectangle (bounds, radius);
        if (! on)
        {
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
        }
        g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (8.5f, true));
        g.drawText (label, bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { setValue (current > 0 ? 0 : 1); }

    // The text's width, so a row of pills can be sized to what they say.
    float textWidth() const { return juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (8.5f, true)), label) + 2.0f; }

private:
    juce::String label;
    juce::Colour colour;
};

// A small round glyph button (the carousel's arrows and +).
class GlyphButton : public juce::Button
{
public:
    explicit GlyphButton (const juce::String& glyphIn) : juce::Button (glyphIn), glyph (glyphIn)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (IlanaTheme::Ui::raised.interpolatedWith (IlanaTheme::Ui::text, down ? 0.2f : hovered ? 0.1f : 0.0f));
        g.fillEllipse (bounds);
        g.setColour (IlanaTheme::Ui::line);
        g.drawEllipse (bounds.reduced (0.5f), 1.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (10.0f, true));
        g.drawText (glyph, bounds.translated (0.0f, -0.5f), juce::Justification::centred);
    }

private:
    juce::String glyph;
};

// Pills that pick a view inside a card (F1 / F2, STRINGS / KEYS, BODY, BOARD): no parameter, solid while chosen,
// as wide as their words.
class ViewPills : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    ViewPills (juce::StringArray labelsIn, juce::Colour colourIn, const juce::String& tip)
        : labels (std::move (labelsIn)), colour (colourIn)
    {
        setTooltip (tip);
        setRepaintsOnMouseActivity (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (int)> onPick;

    int getSelected() const { return selected; }

    // -1: none chosen (a lone tab whose card shows the other view).
    void setSelected (int index, bool notify)
    {
        if (index == selected)
            return;
        selected = index;
        repaint();
        if (notify && onPick != nullptr)
            onPick (selected);
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    static constexpr int pad = 6, pillGap = 2;

    static int wordWidth (const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (8.5f, true)), text) + 2;
    }

    int idealWidth() const
    {
        auto width = 0;
        for (const auto& label : labels)
            width += wordWidth (label) + 2 * pad;
        return width + pillGap * juce::jmax (0, labels.size() - 1);
    }

    void paint (juce::Graphics& g) override
    {
        const auto mouse = getMouseXYRelative().toFloat();
        for (int option = 0; option < labels.size(); ++option)
        {
            const auto cell = cellBounds (option).toFloat().reduced (0.5f, 0.0f);
            const auto on = option == selected;
            const auto hovered = isMouseOver() && cell.contains (mouse);
            g.setColour (on ? colour : IlanaTheme::Ui::raised.interpolatedWith (colour, hovered ? 0.12f : 0.0f));
            g.fillRoundedRectangle (cell, 4.0f);
            if (! on)
            {
                g.setColour (IlanaTheme::Ui::line);
                g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);
            }
            g.setColour (on ? IlanaTheme::Ui::header : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (8.5f, true));
            g.drawText (labels[option], cell, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int option = 0; option < labels.size(); ++option)
            if (cellBounds (option).contains (event.getPosition()))
                setSelected (option, true);
    }

private:
    // Each pill as wide as its word; the leftover width shared out evenly.
    juce::Rectangle<int> cellBounds (int option) const
    {
        const auto spare = labels.isEmpty() ? 0 : (getWidth() - idealWidth()) / labels.size();
        auto x = 0;
        for (int i = 0; i < option; ++i)
            x += wordWidth (labels[i]) + 2 * pad + spare + pillGap;
        return { x, 0, wordWidth (labels[option]) + 2 * pad + spare, getHeight() };
    }

    juce::StringArray labels;
    juce::Colour colour;
    int selected = 0;
};

// A small outlined header button that opens a pop-up editor (ENVELOPE and LFO's EDIT, the dock's STEREO):
// its word and an arrow pointing up and out.
class PopButton : public juce::Button
{
public:
    PopButton (const juce::String& text, juce::Colour colourIn) : juce::Button (text), colour (colourIn)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    // Filled in the colour (the dock's STEREO tag), not only outlined.
    void setTagStyle (bool tag) { tagStyle = tag; }

    int idealWidth() const { return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (8.5f, true)), getButtonText()) + 25; }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto lit = hovered || down || getToggleState();
        const auto edge = tagStyle ? colour : lit ? colour : IlanaTheme::Ui::line.brighter (0.2f);
        g.setColour (tagStyle ? colour.withAlpha (lit ? 0.3f : 0.14f) : IlanaTheme::Ui::raised.interpolatedWith (colour, lit ? 0.12f : 0.0f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        const auto ink = tagStyle ? colour.brighter (0.2f) : IlanaTheme::Ui::text;
        g.setColour (ink);
        g.setFont (IlanaTheme::font (8.5f, true));
        auto text = bounds.reduced (6.0f, 0.0f);
        const auto arrow = text.removeFromRight (7.0f).withSizeKeepingCentre (6.0f, 6.0f);
        g.drawText (getButtonText(), text, juce::Justification::centred);
        // (Manrope has no arrow glyph: a short stroke and its two-line head.)
        juce::Path path;
        path.startNewSubPath (arrow.getBottomLeft());
        path.lineTo (arrow.getTopRight());
        path.startNewSubPath (arrow.getX() + 1.5f, arrow.getY());
        path.lineTo (arrow.getTopRight());
        path.lineTo (arrow.getRight(), arrow.getBottom() - 1.5f);
        g.setColour (tagStyle ? ink : colour.brighter (0.15f));
        g.strokePath (path, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::Colour colour;
    bool tagStyle = false;
};

// STRINGS' tuning button: a drawn eighth note (the six notes open over the page).
class NoteButton : public juce::Button
{
public:
    NoteButton() : juce::Button ("Tuning") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool hovered, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto lit = getToggleState() || down;
        g.setColour (IlanaTheme::Ui::raised.interpolatedWith (IlanaTheme::Ui::text, lit ? 0.2f : hovered ? 0.1f : 0.0f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        const auto c = bounds.getCentre();
        g.setColour (isEnabled() ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
        g.fillEllipse (juce::Rectangle<float> (4.6f, 3.6f).withCentre ({ c.x - 1.6f, c.y + 3.0f }));
        g.fillRect (juce::Rectangle<float> (c.x + 0.3f, c.y - 4.5f, 1.1f, 7.6f));
        juce::Path flag;
        flag.startNewSubPath (c.x + 0.9f, c.y - 4.5f);
        flag.quadraticTo (c.x + 2.0f, c.y - 1.8f, c.x + 3.8f, c.y - 1.2f);
        g.strokePath (flag, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// A carousel card for one kind of source (PLAY's ENVELOPE and LFO): the sources
// of that kind as pills (each one still drags onto a knob), the selected one's
// graph and main controls under them. The pills' "+" adds the next one to the
// patch, the arrows step through them.
class SourceCarouselCard : public juce::Component,
                           private juce::Timer
{
public:
    enum class Kind { envelope, lfo };

    SourceCarouselCard (IlanaSynthAudioProcessor& p, Kind kindIn) : processorRef (p), kind (kindIn)
    {
        for (auto* arrow : { &previous, &next })
            addAndMakeVisible (*arrow);
        previous.setTooltip ("The previous one");
        next.setTooltip ("The next one");
        previous.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        add.setTooltip (kind == Kind::envelope ? "Add an envelope to the patch" : "Add an LFO to the patch");
        add.onClick = [this] { showAddMenu(); };
        addAndMakeVisible (add);
        edit.setTooltip (kind == Kind::envelope ? "Edit\nThis envelope's pop-up editor, with its rarer settings a click away"
                                                : "Edit\nThis LFO's pop-up editor, with its rarer settings a click away");
        edit.setName (kind == Kind::envelope ? "PLAY envelope EDIT" : "PLAY LFO EDIT");
        edit.onClick = [this] { if (onEdit != nullptr && ! sources.empty()) onEdit (selected, edit); };
        addAndMakeVisible (edit);
        refreshSources();
        startTimerHz (6);
    }

    // The selected source's pop-up editor (EDIT, or a click on the title), by `anchor`.
    std::function<void (int source, juce::Component& anchor)> onEdit;

    static constexpr int headerHeight = 20;

    int getSelectedSource() const { return selected; }

    void select (int source)
    {
        selected = source;
        for (auto& tab : tabs)
            tab->setSelected (tab->getSource() == selected);
        edit.setColour (sourceTabColour (selected));
        buildBody();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto colour = sourceTabColour (selected);
        IlanaTheme::paintCard (g, getLocalBounds().toFloat(), 10.0f, colour);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (1, headerHeight - 1, getWidth() - 2, 1));
        const auto centre = headerHeight / 2;
        IlanaTheme::paintTag (g, { 14.0f, (float) centre }, colour);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (13.0f, true));
        IlanaTheme::drawFitted (g, kind == Kind::envelope ? "ENVELOPE" : "LFO", juce::Rectangle<int> (23, centre - 9, 60, 18), juce::Justification::centredLeft, 1);

        if (! chipBounds.isEmpty())
        {
            const auto chip = chipBounds.toFloat().reduced (0.5f);
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (chip, chip.getHeight() * 0.5f, 1.0f);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ chip.getX() + 8.0f, chip.getCentreY() }));
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (8.5f, true));
            IlanaTheme::drawFitted (g, chipText, chipBounds.withTrimmedLeft (14).withTrimmedRight (4), juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        const auto centre = headerHeight / 2;
        auto x = kind == Kind::envelope ? 78 : 44;
        previous.setBounds ({ x, centre - 7, 14, 14 });
        x += 17;
        const auto tabWidth = kind == Kind::envelope ? 42 : 20;
        hiddenCount = 0;
        for (auto& tab : tabs)
        {
            const auto fits = x + tabWidth + 17 + 17 + 8 <= getWidth() - 14 - edit.idealWidth();
            tab->setVisible (fits);
            if (fits)
            {
                tab->setBounds ({ x, centre - 8, tabWidth, 16 });
                x += tabWidth + 3;
            }
            else
            {
                ++hiddenCount;
            }
        }
        add.setBounds ({ x, centre - 7, 14, 14 });
        next.setBounds ({ x + 17, centre - 7, 14, 14 });
        edit.setBounds ({ getWidth() - 7 - edit.idealWidth(), centre - 8, edit.idealWidth(), 16 });
        layoutBody();
    }

    // The title opens the pop-up editor too.
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked() && event.y < headerHeight && event.x < previous.getX() && onEdit != nullptr && ! sources.empty())
            onEdit (selected, edit);
    }

    // The sources the card lists: the envelopes (or LFOs) in the patch.
    std::vector<int> wantedSources() const
    {
        std::vector<int> wanted;
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (envelopeShown (processorRef, env))
                    wanted.push_back ((int) envelopeSource (env));
        }
        else
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (processorRef.isLfoShown (lfo))
                    wanted.push_back ((int) Mod::lfoSourceFor (lfo));
        }
        return wanted;
    }

private:
    // The pill's name: "AMP ENV" is AMP, "LFO 2" is 2.
    juce::String compactName (int source) const
    {
        auto name = sourceTabName (source, processorRef);
        if (kind == Kind::lfo)
            return name.fromLastOccurrenceOf (" ", false, false);
        return name.endsWith (" ENV") ? name.dropLastCharacters (4) : name;
    }

    void step (int direction)
    {
        if (sources.empty())
            return;
        auto index = (int) (std::find (sources.begin(), sources.end(), selected) - sources.begin());
        index = (index + direction + (int) sources.size()) % (int) sources.size();
        select (sources[(size_t) index]);
    }

    bool anyToAdd() const
    {
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (! envelopeShown (processorRef, env))
                    return true;
            return false;
        }
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processorRef.isLfoShown (lfo))
                return true;
        return false;
    }

    // "+" adds the next free one, selected.
    void showAddMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (kind == Kind::envelope ? "Add an envelope" : "Add an LFO");
        if (kind == Kind::envelope)
        {
            for (int env = 0; env < 16; ++env)
                if (! envelopeShown (processorRef, env))
                    menu.addItem (100 + env, ModNames::source ((int) envelopeSource (env)));
        }
        else
        {
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (! processorRef.isLfoShown (lfo))
                    menu.addItem (1 + lfo, "LFO " + juce::String (lfo + 1));
        }
        juce::Component::SafePointer<SourceCarouselCard> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add), [safeThis] (int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;
            auto& p = safeThis->processorRef;
            const auto isLfo = safeThis->kind == Kind::lfo;
            const auto index = isLfo ? result - 1 : result - 100;
            p.setRevealed (isLfo ? IlanaSynthAudioProcessor::Module::Lfo : IlanaSynthAudioProcessor::Module::Envelope, index, true);
            safeThis->refreshSources();
            safeThis->select (isLfo ? (int) Mod::lfoSourceFor (index) : (int) envelopeSource (index));
        });
    }

    static juce::String destinationShortName (int destination)
    {
        const auto parts = ModNames::destinationParts (destination);
        return parts.control.isEmpty() ? parts.module : parts.control;
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
                tab->setCompactLabel (compactName (source));
                tab->onSelect = [this] (int picked) { select (picked); };
                addAndMakeVisible (*tab);
                tabs.push_back (std::move (tab));
            }
            if (std::find (sources.begin(), sources.end(), selected) == sources.end())
                selected = sources.empty() ? 0 : sources.front();
            select (selected);
        }

        for (auto& tab : tabs)
            tab->setRouteCount (modRouteCount (processorRef, tab->getSource()));
        add.setVisible (anyToAdd());

        if (kind == Kind::lfo && ! sources.empty())
        {
            const auto slots = modSlotsDrivenBy (processorRef, selected);
            auto text = slots.empty() ? juce::String ("Drives nothing")
                                      : destinationShortName (processorRef.readModSlot (slots.front()).destination)
                                            + (slots.size() > 1 ? " +" + juce::String ((int) slots.size() - 1) : juce::String());
            if (text != chipText)
            {
                chipText = text;
                repaint();
            }
        }
    }

    // The selected source's graph and controls.
    void buildBody()
    {
        graph.reset();
        knobs.clear();
        toggles.clear();
        shape.reset();
        rate.reset();
        if (sources.empty())
            return;

        const auto colour = sourceTabColour (selected);
        auto& state = processorRef.apvts;
        const auto dial = [&] (const juce::String& id, const juce::String& label)
        {
            auto knob = std::make_unique<KnobControl> (state, id, label, colour, false);
            knob->setPackedDial (dialSize);
            knob->setPackedValueRoom (12);
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        };

        if (kind == Kind::envelope)
        {
            const auto env = SourceEditor::envelopeOf (selected);
            const auto prefix = SourceEditor::envelopePrefix (juce::jmax (0, env));
            auto display = std::make_unique<EnvelopeDisplay> (processorRef, prefix, colour, false);
            display->setSlim (true);
            display->setHandlesOnHover (true);
            graph = std::move (display);
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_delay", "DELAY" }, { "_attack", "ATTACK" }, { "_hold", "HOLD" },
                                                 { "_decay", "DECAY" }, { "_sustain", "SUSTAIN" }, { "_release", "RELEASE" } })
                dial (prefix + suffix, label);
        }
        else
        {
            const auto lfo = juce::jmax (0, Mod::lfoIndexFor ((Mod::Source) selected));
            const auto prefix = "lfo" + juce::String (lfo + 1);
            auto display = std::make_unique<LfoDisplay> (processorRef, lfo, colour);
            display->setMinimal (true);
            graph = std::move (display);
            rate = std::make_unique<LfoRateControl> (processorRef, lfo, colour, false);
            rate->addTo (*this);
            rate->getRateKnob().setPackedDial (dialSize);
            rate->getRateKnob().setPackedValueRoom (12);
            rate->getDivisionKnob().setPackedDial (dialSize);
            rate->setShown (true);
            dial (prefix + "_smooth", "SMOOTH");
            dial (prefix + "_phase", "START");
            for (const auto& [suffix, label] : { std::pair<const char*, const char*> { "_sync", "SYNC" }, { "_retrig", "RETRIG" }, { "_key", "KEY" } })
            {
                auto toggle = std::make_unique<PillToggle> (state, prefix + suffix, label, colour, juce::String (label) + "\nThis LFO's " + juce::String (label).toLowerCase() + " setting.");
                addAndMakeVisible (*toggle);
                toggles.push_back (std::move (toggle));
            }
            shape = std::make_unique<ComboControl> (state, prefix + "_shape", "SHAPE");
            LfoShapeMenu::apply (*shape, processorRef);
            shape->setPackedMenu (true);
            addAndMakeVisible (*shape);
        }
        addAndMakeVisible (*graph);
    }

    void layoutBody()
    {
        if (graph == nullptr)
            return;

        // (Equal above and below the graph, and at both sides: the envelope's last dial and the LFO's buttons end as far from the card as the graph starts.)
        auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (7, 3);
        body.setRight (body.getRight() + (kind == Kind::envelope ? 4 : 0));
        // (The LFO's three dials sit in 34 px cells so its pills keep their padding; the graph keeps its size.)
        const auto cell = kind == Kind::envelope ? 35 : 33;
        // The graphs take every pixel the knobs and the controls leave, the card's full height (bigger than mockup fit8's, ilana 2026-10-19).
        const auto lfoColumn = 91;
        const auto graphWidth = kind == Kind::envelope ? juce::jmax (60, body.getWidth() - 6 * cell - 3)
                                                        : juce::jmax (50, body.getWidth() - 3 * cell - lfoColumn - 9);
        graph->setBounds (body.removeFromLeft (graphWidth));
        // (The LFO's graph keeps its size; the buttons' column takes its width from the two gaps beside the knobs, which stay equal.)
        const auto gap = kind == Kind::envelope ? 3 : 1;
        body.removeFromLeft (gap);
        const auto place = [&] (KnobControl& knob, int x)
        {
            knob.setBounds (KnobControl::packedBoundsFor ({ x, body.getY() + (body.getHeight() - (int) dialSize - 10) / 2, cell, (int) dialSize + 10 }));
        };

        if (kind == Kind::envelope)
        {
            auto x = body.getX() + (body.getWidth() - (int) knobs.size() * cell) / 2;
            for (auto& knob : knobs)
            {
                place (*knob, x);
                x += cell;
            }
            return;
        }

        // RATE, SMOOTH, START as knobs; SYNC / RETRIG / KEY over SHAPE in a column.
        auto x = body.getX();
        place (rate->getRateKnob(), x);
        rate->matchBounds();
        x += cell;
        for (auto& knob : knobs)
        {
            place (*knob, x);
            x += cell;
        }
        // The column runs the graph's full height: three rows, the same height, the leftover pixels shared by the two gaps,
        // so the top row starts where the graph starts and the chip ends where it ends.
        auto column = body.withTrimmedLeft (x - body.getX() + gap);
        // (Gaps near 9: the first one that lets the three rows come out whole, so rows and gaps are each equal.)
        auto gapA = 9;
        for (const auto candidate : { 9, 8, 10, 7, 11, 6, 12 })
            if ((column.getHeight() - 2 * candidate) % 3 == 0)
            {
                gapA = candidate;
                break;
            }
        const auto gapB = gapA;
        const auto rowH = juce::jmax (11, (column.getHeight() - 2 * gapA) / 3);
        auto switches = column.removeFromTop (rowH);
        column.removeFromTop (gapA);
        // Three pills at one gap (each draws 1 px inside its bounds, so the row reaches 1 px past the column on both sides).
        const auto count = juce::jmax (1, (int) toggles.size());
        const auto across = switches.getWidth() + 2;
        // One gap between the pills (2 design px, each pill also draws 1 px inside its bounds); the padding each side of every word is the same on all
        // three and takes the rest, so the row ends flush with the column on both edges (no under 4 design px of padding at the 8.5 text).
        auto textTotal = 0.0f;
        for (auto& toggle : toggles)
            textTotal += toggle->textWidth();
        const auto pillGap = 2.0f;
        const auto pad = ((float) across - pillGap * (float) (count - 1) - textTotal) / (float) (2 * count);
        auto pills = switches.expanded (1, 0).withTrimmedBottom (-1);
        auto edge = (float) pills.getX();
        for (size_t i = 0; i < toggles.size(); ++i)
        {
            const auto right = edge + toggles[i]->textWidth() + 2.0f * pad;
            toggles[i]->setBounds (pills.withLeft (juce::roundToInt (edge)).withRight (juce::roundToInt (right)));
            edge = right + pillGap;
        }
        shape->setBounds (column.removeFromTop (rowH));
        column.removeFromTop (gapB);
        chipBounds = column.removeFromTop (rowH);
    }

    void timerCallback() override
    {
        // (Visible up the tree: an offscreen card, as in the UI tests, follows the pool too.)
        for (auto* c = static_cast<juce::Component*> (this); c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return;
        refreshSources();
    }

    static constexpr float dialSize = 28.0f;

    IlanaSynthAudioProcessor& processorRef;
    Kind kind;
    std::vector<int> sources;
    std::vector<std::unique_ptr<SourceTab>> tabs;
    GlyphButton previous { juce::String::fromUTF8 ("\xe2\x80\xb9") }, next { juce::String::fromUTF8 ("\xe2\x80\xba") }, add { "+" };
    PopButton edit { "EDIT", IlanaTheme::accent() };
    int selected = 0, hiddenCount = 0;
    juce::Rectangle<int> chipBounds;
    juce::String chipText;
    std::unique_ptr<juce::Component> graph;
    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::vector<std::unique_ptr<PillToggle>> toggles;
    std::unique_ptr<ComboControl> shape;
    std::unique_ptr<LfoRateControl> rate;
};

// WEST COAST's live picture: the folder's transfer curve (what one input level comes out as),
// computed as WestCoast.h's Wavefolder does it, so FOLD, BIAS and STAGES redraw it.
class WestDisplay : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit WestDisplay (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setInterceptsMouseClicks (false, false);
        setTooltip ("The wavefolder's curve: input level across, output level up. FOLD bends it, BIAS tilts it, STAGES fold it again.");
        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);
        const auto plot = bounds.reduced (6.0f, 6.0f);
        const auto colour = FilterColours::west();
        const auto on = read ("west_on") > 0.5f ? 1.0f : 0.4f;

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre (plot.getCentre()));
        g.fillRect (juce::Rectangle<float> (1.0f, plot.getHeight()).withCentre (plot.getCentre()));

        const auto fold = read ("west_fold"), symmetry = read ("west_sym");
        const auto stages = juce::jlimit (1, 4, juce::roundToInt (read ("west_stages")));
        const auto gain = 0.35 + 11.65 * (double) (fold * fold);
        const auto bias = 0.5 * (double) symmetry;
        const auto stretch = 1.0 + 0.35 * gain / (double) stages;

        juce::Path path;
        const auto steps = juce::jmax (8, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto input = -1.0 + 2.0 * (double) i / (double) steps;
            auto v = input * gain + bias;
            for (int stage = 0; stage < stages; ++stage)
            {
                const auto y = std::sin (juce::MathConstants<double>::halfPi * v);
                v = stage + 1 < stages ? y * stretch : y;
            }
            const auto point = juce::Point<float> (plot.getX() + (float) (i) / (float) steps * plot.getWidth(),
                                                   plot.getCentreY() - (float) juce::jlimit (-1.0, 1.0, v) * plot.getHeight() * 0.5f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (on);
        IlanaTrace::paintTrace (g, path, colour);
    }

private:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;
        const auto signature = read ("west_fold") * 7.0f + read ("west_sym") * 13.0f + read ("west_stages") * 31.0f + read ("west_on");
        if (juce::approximatelyEqual (signature, lastSignature))
            return;
        lastSignature = signature;
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    float lastSignature = -1.0e9f;
};

// PLAY (ilana, 2026-10-19, mockup "fit8"): the oscillators as the OSC page's own
// cards on top, the voice's modules in two bands under them: FILTER 1, FILTER 2
// and WEST COAST; then ENVELOPE, LFO, BODY and SUB + NOISE. One knob size and
// pitch in every card, the menus in the headers.
// A small live picture of a few parameters (BODY's resonance, SUB's wave, NOISE's colour): it
// repaints when one of them moves, in the same well and trace as the other pictures.
class ParamPicture : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    ParamPicture (IlanaSynthAudioProcessor& p, std::vector<juce::String> idsIn, const juce::String& tip)
        : processorRef (p), ids (std::move (idsIn))
    {
        setInterceptsMouseClicks (false, false);
        setTooltip (tip);
        startTimerHz (10);
    }

protected:
    float read (const char* id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;

private:
    void timerCallback() override
    {
        auto signature = 0.0;
        auto weight = 1.0;
        for (const auto& id : ids)
        {
            signature += (double) read (id.toRawUTF8()) * weight;
            weight *= 1.7;
        }
        if (signature != lastSignature)
        {
            lastSignature = signature;
            repaint();
        }
    }

    std::vector<juce::String> ids;
    double lastSignature = -1.0e9;
};

// BODY: where its modes sit and how long they ring, from TYPE, MATERIAL and SIZE (the engine's own tuning tables).
class BodyDisplay : public ParamPicture
{
public:
    explicit BodyDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "body_type", "body_material", "body_size", "res_on" },
                        "The body's resonance: its modes across the pitch range. TYPE picks their spacing, MATERIAL how long they ring, SIZE slides them.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto type = juce::jlimit (0, 4, juce::roundToInt (read ("body_type")));
        const auto material = juce::jlimit (0.0f, 1.0f, read ("body_material"));
        const auto size = juce::jlimit (0.0f, 1.0f, read ("body_size"));
        const auto on = read ("res_on") > 0.5f ? 1.0f : 0.45f;
        const auto metal = juce::jlimit (0.0f, 1.0f, (material - 0.15f) / 0.55f);
        const auto glass = juce::jlimit (0.0f, 1.0f, (material - 0.8f) / 0.2f);
        const auto lifeScale = 0.9f + 0.6f * metal - 0.75f * glass;

        // The first mode sits at 160 Hz at the middle SIZE, an octave per half turn; the axis is 40 Hz to 8 kHz.
        const auto root = 160.0 * std::pow (2.0, (0.5 - (double) size) * 2.0);
        constexpr auto lowHz = 40.0, highHz = 8000.0;
        std::array<double, BodyTuning::modes> hz {}, height {}, width {};
        for (int mode = 0; mode < BodyTuning::modes; ++mode)
        {
            const auto m = (size_t) mode;
            const auto ratio = type == 0 ? (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].ratio[m];
            const auto gain = type == 0 ? 1.0 / (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].gain[m];
            const auto life = type == 0 ? 1.0 / (double) (mode + 1) : (double) BodyTuning::shapes[type - 1].lifetime[m];
            hz[m] = root * ratio;
            height[m] = gain * (1.0 - 0.5 * (1.0 - (double) material) * mode / 7.0);
            width[m] = hz[m] * (0.012 + 0.07 / (1.0 + 5.0 * life * (double) lifeScale));
        }

        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto f = lowHz * std::pow (highHz / lowHz, (double) i / (double) steps);
            auto level = 0.0;
            for (size_t m = 0; m < hz.size(); ++m)
            {
                const auto d = (f - hz[m]) / width[m];
                level += height[m] / (1.0 + d * d);
            }
            const auto point = juce::Point<float> (plot.getX() + (float) i / (float) steps * plot.getWidth(),
                                                   plot.getBottom() - (float) juce::jlimit (0.0, 1.0, std::sqrt (level)) * plot.getHeight());
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (on);
        IlanaTrace::paintArea (g, path, juce::Colour (0xffff6a2a), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// SUB: one wave shape, two cycles at -1 octave and one at -2.
class SubWaveDisplay : public ParamPicture
{
public:
    explicit SubWaveDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "sub_shape", "sub_octave", "subosc_on" }, "The sub oscillator's wave: sine, square or saw, an octave or two below the note.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto shape = juce::roundToInt (read ("sub_shape"));
        const auto cycles = juce::roundToInt (read ("sub_octave")) == 1 ? 1.0 : 2.0;
        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth() * 2);
        for (int i = 0; i <= steps; ++i)
        {
            const auto phase = std::fmod ((double) i / (double) steps * cycles, 1.0);
            const auto v = shape == 1 ? (phase < 0.5 ? 1.0 : -1.0)
                           : shape == 2 ? 1.0 - 2.0 * phase
                                        : std::sin (juce::MathConstants<double>::twoPi * phase);
            const auto point = juce::Point<float> (plot.getX() + (float) i / (float) steps * plot.getWidth(), plot.getCentreY() - (float) v * plot.getHeight() * 0.5f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (read ("subosc_on") > 0.5f ? 1.0f : 0.45f);
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// NOISE: its spectrum's slope, low pitches on the left: white is flat, pink falls 3 dB an octave, brown 6,
// blue and violet rise, grey dips in the middle like the ear's own curve.
class NoiseColourDisplay : public ParamPicture
{
public:
    explicit NoiseColourDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "noise_type", "noise_on" }, "The noise colour's spectrum: low pitches left, high right.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto plot = bounds.reduced (4.0f, 2.0f);
        const auto type = juce::jlimit (0, 5, juce::roundToInt (read ("noise_type")));
        const std::array<double, 6> slope { 0.0, -3.0, -6.0, 3.0, 6.0, 0.0 };
        juce::Path path;
        const auto steps = juce::jmax (16, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const auto x = (double) i / (double) steps; // 0..1 across ten octaves
            auto db = slope[(size_t) type] * (x - 0.5) * 10.0;
            if (type == 5)
                db = 9.0 * (std::cos (juce::MathConstants<double>::twoPi * x * 0.9 + 0.2) * 0.5 - 0.1);
            const auto y = plot.getCentreY() - (float) juce::jlimit (-1.0, 1.0, db / 15.0) * plot.getHeight() * 0.5f;
            const auto point = juce::Point<float> (plot.getX() + (float) x * plot.getWidth(), y);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setOpacity (read ("noise_on") > 0.5f ? 1.0f : 0.45f);
        IlanaTrace::paintArea (g, path, juce::Colour (0xffff6a2a), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, juce::Colour (0xffff6a2a));
    }
};

// The OSC area's thin scroll bar: a track and a thumb that read on the dark page.
class OscScrollBar : public juce::ScrollBar
{
public:
    using juce::ScrollBar::ScrollBar;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, 2.5f);

        const auto total = getMaximumRangeLimit() - getMinimumRangeLimit();
        if (total <= 0.0)
            return;
        const auto top = (float) ((getCurrentRangeStart() - getMinimumRangeLimit()) / total) * bounds.getHeight();
        const auto height = juce::jmax (16.0f, (float) (getCurrentRangeSize() / total) * bounds.getHeight());
        g.setColour (IlanaTheme::Ui::text2);
        g.fillRoundedRectangle (bounds.getX(), bounds.getY() + top, bounds.getWidth(), juce::jmin (height, bounds.getHeight() - top), 2.5f);
    }
};

// The dock's VOICES pop-up (ilana's mockup v3, 2026-10-25): the stereo settings that were the OSC tab's STEREO card,
// SPREAD, PHASE, DRIFT and LOOSE with LOOSE's "Sharp and flat" switch. It floats over the page above the dock;
// the cross, Escape or a click elsewhere closes it.
class StereoPopover : public juce::Component
{
public:
    explicit StereoPopover (IlanaSynthAudioProcessor& p)
        : looseBoth (p.apvts, "tune_loose_both", "Sharp and flat")
    {
        for (const auto& [id, label] : { std::pair<const char*, const char*> { "voice_spread", "SPREAD" }, { "unison_random", "PHASE" },
                                         { "drift", "DRIFT" }, { "tune_loose", "LOOSE" } })
        {
            auto knob = std::make_unique<KnobControl> (p.apvts, id, label, colour(), false);
            knob->setPackedDial (dialSize);
            knob->setPackedValueRoom (12);
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }
        looseBoth.setPackedName (ToggleControl::NamePlace::right);
        looseBoth.setRightNameSize (8.5f);
        looseBoth.setSwitchColour (colour());
        addAndMakeVisible (looseBoth);
        closeButton.setTooltip ("Close (Escape)");
        closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };
        addAndMakeVisible (closeButton);
        setWantsKeyboardFocus (true);
    }

    std::function<void()> onClose;
    std::function<bool (juce::Component*)> isOwnButton; // (the dock's button toggles it itself)

    ~StereoPopover() override { juce::Desktop::getInstance().removeGlobalMouseListener (this); }

    void visibilityChanged() override
    {
        if (isVisible())
            juce::Desktop::getInstance().addGlobalMouseListener (this);
        else
            juce::Desktop::getInstance().removeGlobalMouseListener (this);
    }

    // A click anywhere else closes it.
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! isVisible() || event.eventComponent == nullptr || event.eventComponent == this || isParentOf (event.eventComponent))
            return;
        if (isOwnButton != nullptr && isOwnButton (event.eventComponent))
            return;
        // (A menu or a value box the pop-up opened is not "elsewhere".)
        if (juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0)
            return;
        if (onClose != nullptr)
            onClose();
    }

    static juce::Colour colour() { return juce::Colour (0xff3fd0e6); }
    static constexpr int width = 262, height = 98, headerHeight = 22, cell = 60;
    static constexpr float dialSize = 28.0f;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (3.0f);
        for (int ring = 4; ring >= 1; --ring)
        {
            g.setColour (juce::Colours::black.withAlpha (0.14f));
            g.fillRoundedRectangle (bounds.expanded ((float) ring).translated (0.0f, 2.0f), 9.0f + (float) ring);
        }
        IlanaTheme::paintCard (g, bounds, 9.0f, colour());
        const auto header = bounds.toNearestInt().removeFromTop (headerHeight);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (header.getX() + 1, header.getBottom() - 1, header.getWidth() - 2, 1);
        IlanaTheme::paintTag (g, { (float) header.getX() + 12.0f, (float) header.getCentreY() }, colour());
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (13.0f, true));
        const auto title = juce::String ("VOICE");
        g.drawText (title, header.withTrimmedLeft (21), juce::Justification::centredLeft, false);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (8.5f, true));
        g.drawText ("STEREO SETTINGS", header.withTrimmedLeft (21 + juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (13.0f, true)), title) + 8),
                    juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (3);
        closeButton.setBounds (juce::Rectangle<int> (16, 16).withCentre ({ area.getRight() - 13, area.getY() + headerHeight / 2 }));
        area.removeFromTop (headerHeight);
        auto foot = area.removeFromBottom (19).reduced (10, 2);
        auto x = area.getX() + (area.getWidth() - (int) knobs.size() * cell) / 2;
        for (auto& knob : knobs)
        {
            knob->setBounds (KnobControl::packedBoundsFor ({ x, area.getY() + 3, cell, (int) dialSize + 10 }));
            x += cell;
        }
        // (Under LOOSE, the knob it belongs to.)
        looseBoth.setBounds (foot.withLeft (knobs.back()->getBounds().getCentreX() - 50).withWidth (100));
    }

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
    std::vector<std::unique_ptr<KnobControl>> knobs;
    ToggleControl looseBoth;
    GlyphButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
};

class PlayPage : public juce::Component,
                 private juce::ScrollBar::Listener,
                 private juce::Timer
{
public:
    explicit PlayPage (IlanaSynthAudioProcessor& p)
        : processorRef (p), oscView (p), envelopeCard (p, SourceCarouselCard::Kind::envelope), lfoCard (p, SourceCarouselCard::Kind::lfo),
          filterDisplay (p)
    {
        if (auto* page = oscView.getPage())
            page->setEmbedded (true);
        oscView.setMaximumScale (1.0f);
        oscView.setOverlayScroll (true);
        oscView.onViewMoved = [this] { syncOscBar(); };
        addAndMakeVisible (oscView);
        oscBar.setAutoHide (false);
        oscBar.addListener (this);
        addChildComponent (oscBar);

        auto& state = p.apvts;
        const auto knob = [&] (const juce::String& id, const juce::String& label, juce::Colour colour)
        {
            auto control = std::make_unique<KnobControl> (state, id, label, colour, false);
            control->setPackedDial (dialSize);
            control->setPackedValueRoom (12);
            addAndMakeVisible (*control);
            return control;
        };
        const auto bareSwitch = [&] (const juce::String& id, juce::Colour colour, const juce::String& tip)
        {
            auto toggle = std::make_unique<ToggleControl> (state, id, "ON");
            toggle->setBareSwitch (true);
            toggle->setSwitchColour (colour);
            toggle->getButton().setTooltip (tip);
            addAndMakeVisible (*toggle);
            return toggle;
        };
        const auto menuFor = [&] (const juce::String& id, const juce::String& prefix, const juce::String& tip)
        {
            auto menu = std::make_unique<ComboControl> (state, id, prefix);
            menu->setTooltip (tip);
            menu->getComboBox().setTooltip (tip);
            menu->setPackedMenu (true);
            addAndMakeVisible (*menu);
            return menu;
        };

        // FILTER (mockup v3): Filter 1 and 2 in one card. F1 / F2 pick whose type, slope and knobs show; the
        // response draws both and the spectrum. SERIAL / PARALLEL in the header, BALANCE while parallel,
        // MORPH on the types that morph, Filter 2's switch in its view.
        addAndMakeVisible (filterDisplay);
        filterTabs.onPick = [this] (int index) { showFilter (index); };
        addAndMakeVisible (filterTabs);
        for (int f = 0; f < 2; ++f)
        {
            const juce::String prefix (f == 0 ? "f1" : "f2");
            const auto colour = filterColour (f);
            auto& card = filters[(size_t) f];
            card.type = std::make_unique<ComboControl> (state, prefix + "_type", "");
            {
                // (Same item ids, so the attachment still matches them.)
                auto& box = card.type->getComboBox();
                const auto shortNames = FilterTypeGrid::shortNames();
                const auto names = FilterType::getNames();
                box.clear (juce::dontSendNotification);
                for (int index = 0; index < names.size(); ++index)
                    box.addItem (index < shortNames.size() ? shortNames[index] : names[index], index + 1);
                box.setSelectedId (readInt (prefix + "_type") + 1, juce::dontSendNotification);
            }
            const juce::String typeTip ("FILTER " + juce::String (f + 1) + " TYPE\nThe filter model, grouped by family.");
            card.type->setTooltip (typeTip);
            card.type->getComboBox().setTooltip (typeTip);
            card.type->setPopupOverride ([this, combo = &card.type->getComboBox(), id = prefix + "_type", f] { showFilterTypeMenu (*combo, id, f); });
            card.type->setPackedMenu (true);
            addAndMakeVisible (*card.type);
            card.slope = std::make_unique<PillChoice> (state, prefix + "_slope", juce::StringArray { "12", "24" }, colour, "Slope\n12 dB is gentler and brighter; 24 dB cuts harder.");
            addAndMakeVisible (*card.slope);
            for (const auto& spec : { std::pair<const char*, const char*> { "_cutoff", "CUTOFF" }, { "_reso", "RESO" }, { "_drive", "DRIVE" },
                                      { "_keytrack", "KEY TRACK" }, { "_morph", "MORPH" } })
            {
                card.knobs.push_back (knob (prefix + spec.first, spec.second, colour));
                card.knobs.back()->setPackedNameWrap (true);
                // A DX7 voice has no filter until CUTOFF comes down: the rest dims (I14-3).
                if (juce::String (spec.first) != "_cutoff")
                    effectRules.add (*card.knobs.back(), [this] { return ! operatorFilterOff; },
                                     "a DX7 voice has no filter: turn CUTOFF down to bring one in");
            }
        }
        routing = std::make_unique<PillChoice> (state, "filters_parallel", juce::StringArray { "SERIAL", "PARALLEL" }, IlanaTheme::Ui::text2,
                                                "Filter routing\nSERIAL runs Filter 1 into Filter 2; PARALLEL runs them side by side and BALANCE mixes them.");
        addAndMakeVisible (*routing);
        balance = knob ("filter_balance", "BALANCE", IlanaTheme::Ui::text);
        // Filter 2 has no on parameter: OFF parks its CUTOFF wide open (as the FILTER page's switch did).
        f2Switch.setClickingTogglesState (false);
        f2Switch.getProperties().set ("switch", true);
        f2Switch.setTooltip ("Filter 2 on or off. Off is wide open (CUTOFF at the top), so it passes everything; ON brings its CUTOFF back.");
        f2Switch.onClick = [this] { toggleFilter2(); };
        addChildComponent (f2Switch);

        // STRINGS · KEYS: the sympathetic strings (their switch, Manual tuning and its six notes, three knobs)
        // or the piano KEYS knobs.
        stringsTabs.onPick = [this] (int index) { showStrings (index); };
        addAndMakeVisible (stringsTabs);
        stringsOn = bareSwitch ("sym_on", stringsColour(), "Sympathetic strings: shared drone strings that ring with everything you play");
        stringsManual = std::make_unique<ToggleControl> (state, "sym_manual", "Manual");
        stringsManual->setPackedName (ToggleControl::NamePlace::right);
        stringsManual->setRightNameSize (8.5f);
        stringsManual->setSwitchColour (stringsColour());
        stringsManual->getButton().onClick = [this] { showTuning (readInt ("sym_manual") != 0); };
        addAndMakeVisible (*stringsManual);
        tuneButton.setTooltip ("The sympathetic strings' six notes (with Manual tuning on)");
        tuneButton.onClick = [this] { showTuning (! tuningPopover.isVisible()); };
        addAndMakeVisible (tuneButton);
        for (const auto& [id, label] : { std::pair<const char*, const char*> { "sym_amount", "AMOUNT" }, { "sym_count", "COUNT" }, { "sym_decay", "DECAY" } })
            stringsKnobs.push_back (knob (id, label, stringsColour()));
        for (const auto& [id, label] : { std::pair<const char*, const char*> { "stretch", "STRETCH" }, { "pedal_res", "PEDAL RES" }, { "mech_key", "KEY NOISE" },
                                         { "mech_damper", "DAMPER NOISE" }, { "mech_pedal", "PEDAL NOISE" } })
        {
            keysKnobs.push_back (knob (id, label, stringsColour()));
            keysKnobs.back()->setPackedNameWrap (true);
        }
        for (int i = 0; i < 6; ++i)
        {
            auto note = std::make_unique<KnobControl> (state, "sym_note" + juce::String (i + 1), "NOTE " + juce::String (i + 1), stringsColour(), false);
            note->setPackedDial (36.0f);
            tuningPopover.addAndMakeVisible (*note);
            tuningNotes.push_back (std::move (note));
        }
        tuningPopover.isOwnButton = [this] (juce::Component* c) { return c == &tuneButton || c == stringsManual.get() || stringsManual->isParentOf (c); };
        tuningPopover.onClose = [this] { showTuning (false); };
        addChildComponent (tuningPopover);

        // WEST COAST: its switch, GATE, PLACE and STRIKE BY menus in the header, the fold's curve and seven knobs.
        westDisplay = std::make_unique<WestDisplay> (p);
        addAndMakeVisible (*westDisplay);
        westOn = bareSwitch ("west_on", westColour(), "WEST COAST on or off: a wavefolder into a low-pass gate");
        westMode = menuFor ("west_mode", "", "GATE\nHow the low-pass gate opens: Combo, Low Pass or VCA.");
        westPosition = menuFor ("west_pos", "", "PLACE\nWhere WEST sits: after the filters, or in Filter 2's place.");
        {
            // (Short words for the header; the saved choice strings stay.)
            auto& box = westPosition->getComboBox();
            const auto selectedId = box.getSelectedId();
            box.changeItemText (1, "After F");
            box.changeItemText (2, "As F2");
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        westSource = menuFor ("west_src", "BY", "STRIKE BY\nWhat strikes the gate: each note, or a modulation source.");
        {
            auto& box = westSource->getComboBox();
            const auto selectedId = box.getSelectedId();
            box.changeItemText (1, "Note"); // ("Note Strike": BY says the rest)
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        for (const auto& spec : { std::pair<const char*, const char*> { "west_fold", "FOLD" }, { "west_sym", "BIAS" }, { "west_stages", "STAGES" },
                                  { "west_decay", "DECAY" }, { "west_res", "RESO" }, { "west_strike", "STRIKE" }, { "west_open", "OPEN" } })
            westKnobs.push_back (knob (spec.first, spec.second, westColour()));

        // BODY · BOARD: the physical body (the FILTER tab's) and the soundboard (the OSC tab's), each with its
        // own switch in the header, so either, both or none play; the tabs pick whose controls show.
        bodyTab.onPick = [this] (int) { showBody (0); };
        boardTab.onPick = [this] (int) { showBody (1); };
        addAndMakeVisible (bodyTab);
        addAndMakeVisible (boardTab);
        bodyOn = bareSwitch ("res_on", bodyColour(), "BODY on or off: the oscillator mix excites a resonant body");
        boardOn = bareSwitch ("sb_on", bodyColour(), "SOUNDBOARD on or off: a wooden board under the whole voice");
        bodyType = menuFor ("body_type", "TYPE", "BODY TYPE\nClassic, bar, plate, bell, shell or guitar.");
        bodyCoupling = menuFor ("body_coupling_mode", "", "COUPLING\nWhich way the strings and the body push on each other.");
        {
            auto& box = bodyCoupling->getComboBox();
            const auto selectedId = box.getSelectedId();
            const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");
            box.changeItemText (2, "String" + arrow + "body");
            box.changeItemText (3, "Body" + arrow + "string");
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        for (const auto& spec : { std::pair<const char*, const char*> { "res_amount", "AMOUNT" }, { "res_decay", "DECAY" }, { "body_coupling", "COUPLE" },
                                  { "body_material", "MATERIAL" }, { "body_size", "SIZE" }, { "res_offset", "OFFSET" }, { "res_keytrack", "KEY TRK" } })
            bodyKnobs.push_back (knob (spec.first, spec.second, bodyColour()));
        boardModel = menuFor ("sb_model", "MODEL", "SOUNDBOARD MODEL\nClassic, or Dense for a thicker board.");
        for (const auto& spec : { std::pair<const char*, const char*> { "sb_size", "SIZE" }, { "sb_tone", "TONE" }, { "sb_mix", "MIX" } })
            boardKnobs.push_back (knob (spec.first, spec.second, bodyColour()));

        // SUB and NOISE: each its own card, switch, menu, picture and two knobs.
        subOn = bareSwitch ("subosc_on", subColour(), "SUB on or off: a sine, square or saw an octave or two down");
        noiseOn = bareSwitch ("noise_on", subColour(), "NOISE on or off");
        subShape = menuFor ("sub_shape", "WAVE", "SUB WAVE\nSine for clean weight, Square or Saw for grit.");
        noiseType = menuFor ("noise_type", "TYPE", "NOISE TYPE\nWhite, pink, brown, blue, violet or grey.");
        subOctave = std::make_unique<PillChoice> (state, "sub_octave", juce::StringArray { "-2", "-1" }, subColour(), "SUB OCTAVE\n-1 or -2 octaves below the note.", std::vector<int> { 1, 0 });
        addAndMakeVisible (*subOctave);
        subLevel = knob ("subosc_level", "LEVEL", subColour());
        subColourKnob = knob ("sub_colour", "COLOUR", subColour());
        noiseLevel = knob ("noise_level", "LEVEL", subColour());
        noiseColour = knob ("noise_color", "COLOUR", subColour());
        subWave = std::make_unique<SubWaveDisplay> (p);
        addAndMakeVisible (*subWave);
        noiseBand = std::make_unique<NoiseColourDisplay> (p);
        addAndMakeVisible (*noiseBand);

        // What doesn't act dims in place; the cards keep their size.
        for (auto* part : std::initializer_list<juce::Component*> { subShape.get(), subOctave.get(), subLevel.get(), subColourKnob.get() })
            effectRules.add (*part, [this] { return readInt ("subosc_on") != 0; }, "SUB is off: switch it on");
        for (auto* part : std::initializer_list<juce::Component*> { noiseType.get(), noiseLevel.get(), noiseColour.get() })
            effectRules.add (*part, [this] { return readInt ("noise_on") != 0; }, "NOISE is off: switch it on");
        for (auto& part : stringsKnobs)
            effectRules.add (*part, [this] { return readInt ("sym_on") != 0; }, "STRINGS is off: switch it on");
        effectRules.add (*stringsManual, [this] { return readInt ("sym_on") != 0; }, "STRINGS is off: switch it on");
        for (auto* part : std::initializer_list<juce::Component*> { boardModel.get(), boardKnobs[0].get(), boardKnobs[1].get(), boardKnobs[2].get() })
            effectRules.add (*part, [this] { return readInt ("sb_on") != 0; }, "SOUNDBOARD is off: switch it on");
        // (As on the FILTER tab: off, the body's controls dim, but the coupling while it couples the strings, which works without it.)
        const auto stringsCoupling = [this] { return readInt ("body_coupling_mode") == 3; };
        const auto modal = [this] { return readInt ("body_type") > 0; };
        effectRules.add (*bodyType, [this] { return readInt ("res_on") != 0; }, "BODY is off: switch it on");
        effectRules.add (*bodyKnobs[0], [this] { return readInt ("res_on") != 0; }, "BODY is off: switch it on");
        effectRules.add (*bodyCoupling, [this, stringsCoupling] { return readInt ("res_on") != 0 || stringsCoupling(); }, "BODY is off: switch it on");
        // Knobs: AMOUNT DECAY COUPLE / MATERIAL SIZE OFFSET KEY TRK.
        effectRules.add (*bodyKnobs[2], [this, stringsCoupling, modal]
                         { return stringsCoupling() || (readInt ("body_coupling_mode") != 0 && modal() && readInt ("res_on") != 0); },
                         "COUPLING is Off, or needs a modal BODY");
        for (const size_t i : std::initializer_list<size_t> { 3, 4 })
            effectRules.add (*bodyKnobs[i], [this, modal] { return readInt ("res_on") != 0 && modal(); }, "BODY is off, or Classic");
        for (const size_t i : std::initializer_list<size_t> { 1, 5, 6 })
            effectRules.add (*bodyKnobs[i], [this] { return readInt ("res_on") != 0; }, "BODY is off: switch it on");
        effectRules.add (*balance, [this] { return readInt ("filters_parallel") != 0; }, "the filters are in SERIAL");

        // ENVELOPE and LFO: EDIT (or the title) opens the source's pop-up editor.
        envelopeCard.onEdit = [this] (int source, juce::Component& anchor) { if (onEditSource != nullptr) onEditSource (source, anchor); };
        lfoCard.onEdit = [this] (int source, juce::Component& anchor) { if (onEditSource != nullptr) onEditSource (source, anchor); };
        addAndMakeVisible (envelopeCard);
        addAndMakeVisible (lfoCard);

        showFilter (0);
        showStrings (0);
        showBody (0);
        startTimerHz (4);
    }

    ~PlayPage() override { stopTimer(); }

    std::function<void (int)> onEditOperator;
    std::function<void (const juce::String&)> onOpenPage;
    // EDIT on ENVELOPE or LFO: the source's floating editor, by the button.
    std::function<void (int source, juce::Component& anchor)> onEditSource;

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        paintCardWithHeader (g, filterBounds, "FILTER", filterColour (filterTabs.getSelected()));
        paintCardWithHeader (g, stringsBounds, {}, stringsColour());
        paintCardWithHeader (g, westBounds, "WEST COAST", westColour());
        paintCardWithHeader (g, bodyBounds, {}, bodyColour());
        paintCardWithHeader (g, subBounds, "SUB", subColour());
        paintCardWithHeader (g, noiseBounds, "NOISE", subColour());

        // STRINGS: how many strings, and how they are tuned.
        if (stringsTabs.getSelected() == 0 && ! stringsCaption.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text2.withAlpha (readInt ("sym_on") != 0 ? 1.0f : IlanaTheme::dimmedAlpha));
            g.setFont (IlanaTheme::font (8.5f));
            g.drawText (juce::String (readInt ("sym_count")) + (readInt ("sym_count") == 1 ? " string" : " strings") + juce::String::fromUTF8 (" \xc2\xb7 ")
                            + (readInt ("sym_manual") != 0 ? "manual" : "auto-tuned"),
                        stringsCaption, juce::Justification::centredRight, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (sideMargin, 0);
        // The oscillator cards take what they need (all three of them, or the page scrolls them);
        // the two module bands sit under them at one height.
        const auto bottom = area.getBottom() - bottomMargin;
        const auto bandsHeight = juce::jlimit (2 * minBand + gap, 2 * maxBand + gap, bandsHeightFor (area.getHeight()));
        // (Three full oscillator cards: the bands sit right under them, the little slack goes to the foot.)
        const auto oscMin = oscView.getPage() != nullptr ? oscView.getPage()->getReferenceHeight() : 0;
        const auto bandsTop = oscMin > bottom - bandsHeight - 14 ? juce::jmin (bottom - bandsHeight, oscMin) : bottom - bandsHeight;
        oscView.setBounds (getLocalBounds().withHeight (juce::jmax (0, bandsTop)));
        oscBar.setBounds (getWidth() - sideMargin + 1, 4, sideMargin - 2, juce::jmax (0, bandsTop - 8));
        syncOscBar();

        const auto band1Height = juce::jlimit (minBand, 84, bandsHeight - gap - 90);
        const auto bandHeight = bandsHeight - gap - band1Height;
        layoutBand1 ({ area.getX(), bandsTop, area.getWidth(), band1Height });
        layoutBand2 ({ area.getX(), bandsTop + band1Height + gap, area.getWidth(), bandHeight });
        if (tuningPopover.isVisible())
            tuningPopover.setBounds (tuningBounds());

        // The cards' rectangles, for the layout tests' margin scan (x, y, width, height; name).
        juce::String cards;
        const auto note = [&cards] (const char* name, juce::Rectangle<int> r) { cards << name << "," << r.getX() << "," << r.getY() << "," << r.getWidth() << "," << r.getHeight() << ";"; };
        note ("FILTER", filterBounds);
        note ("STRINGS", stringsBounds);
        note ("WEST", westBounds);
        note ("ENVELOPE", envelopeCard.getBounds());
        note ("LFO", lfoCard.getBounds());
        note ("BODY", bodyBounds);
        note ("SUB", subBounds);
        note ("NOISE", noiseBounds);
        getProperties().set ("playCards", cards);
    }

    // The views the tests and the snapshots open.
    void showFilter (int index)
    {
        index = juce::jlimit (0, 1, index);
        filterTabs.setSelected (index, false);
        filterTabs.setColour (filterColour (index));
        refreshFilterParts();
        resized();
        repaint();
    }

    void showStrings (int index)
    {
        stringsTabs.setSelected (juce::jlimit (0, 1, index), false);
        const auto strings = stringsTabs.getSelected() == 0;
        for (auto* part : std::initializer_list<juce::Component*> { stringsOn.get(), stringsManual.get(), &tuneButton })
            part->setVisible (strings);
        for (auto& knob : stringsKnobs)
            knob->setVisible (strings);
        for (auto& knob : keysKnobs)
            knob->setVisible (! strings);
        if (! strings)
            showTuning (false);
        repaint();
    }

    void showBody (int index)
    {
        bodyTab.setSelected (index == 0 ? 0 : -1, false);
        boardTab.setSelected (index == 1 ? 0 : -1, false);
        bodyType->setVisible (index == 0);
        bodyCoupling->setVisible (index == 0);
        for (auto& knob : bodyKnobs)
            knob->setVisible (index == 0);
        boardModel->setVisible (index == 1);
        for (auto& knob : boardKnobs)
            knob->setVisible (index == 1);
    }

    bool isTuningShown() const { return tuningPopover.isVisible(); }

private:
    struct FilterCard
    {
        std::unique_ptr<ComboControl> type;
        std::unique_ptr<PillChoice> slope;
        std::vector<std::unique_ptr<KnobControl>> knobs; // CUTOFF, RESO, DRIVE, KEY TRACK, MORPH
    };

    static constexpr int sideMargin = 7, bottomMargin = 4, gap = 0, hGap = 5, headerHeight = 20, cell = 35;
    static constexpr int minBand = 62, maxBand = 96;
    static constexpr float dialSize = 28.0f;

    static juce::Colour filterColour (int index) { return IlanaTheme::envIdentityColour (index == 0 ? 1 : 2); }
    static juce::Colour westColour() { return FilterColours::west(); }
    static juce::Colour stringsColour() { return juce::Colour (0xffff8a5c); }
    static juce::Colour bodyColour() { return juce::Colour (0xffff6a2a); }
    static juce::Colour subColour() { return juce::Colour (0xffff6a2a); }

    void timerCallback() override
    {
        operatorFilterOff = operatorVoiceFilterOff (processorRef);
        filterDisplay.setVoiceOff (operatorFilterOff);
        effectRules.apply();
        // A type or the routing changed: the filter card's knobs follow (MORPH, BALANCE, the slope).
        const auto signature = readInt ("f1_type") * 7 + readInt ("f2_type") * 131 + readInt ("filters_parallel") * 30011;
        if (signature != filterSignature)
        {
            filterSignature = signature;
            refreshFilterParts();
            resized();
        }
        const auto f2Open = FilterDisplay::isPassThrough (processorRef, 1, operatorFilterOff);
        if (f2Switch.getToggleState() == f2Open)
            f2Switch.setToggleState (! f2Open, juce::dontSendNotification);
        tuneButton.setEnabled (readInt ("sym_manual") != 0);
        if (readInt ("sym_manual") == 0 && tuningPopover.isVisible())
            showTuning (false);
        const auto stringsKey = readInt ("sym_count") * 4 + readInt ("sym_manual") * 2 + readInt ("sym_on");
        if (stringsKey != stringsSignature)
        {
            stringsSignature = stringsKey;
            repaint (stringsCaption);
        }
    }

    bool operatorFilterOff = false;
    int filterSignature = -1, stringsSignature = -1;

    // The shown filter's type, slope and knobs; MORPH only on a type that morphs, BALANCE only in parallel.
    void refreshFilterParts()
    {
        const auto shown = filterTabs.getSelected();
        for (int f = 0; f < 2; ++f)
        {
            auto& card = filters[(size_t) f];
            const auto on = f == shown;
            card.type->setVisible (on);
            card.slope->setVisible (on && filterHasSlope (f));
            for (size_t k = 0; k < card.knobs.size(); ++k)
                card.knobs[k]->setVisible (on && (k < 4 || FilterType::usesMorph (readInt (f == 0 ? "f1_type" : "f2_type"))));
        }
        balance->setVisible (readInt ("filters_parallel") != 0);
        f2Switch.setVisible (shown == 1);
    }

    std::vector<KnobControl*> filterKnobRow() const
    {
        std::vector<KnobControl*> row;
        for (auto& knob : filters[(size_t) filterTabs.getSelected()].knobs)
            if (knob->isVisible())
                row.push_back (knob.get());
        if (balance->isVisible())
            row.push_back (balance.get());
        return row;
    }

    // As the FILTER tab's switch: off parks Filter 2's CUTOFF at the top, on brings it back.
    void toggleFilter2()
    {
        if (auto* parameter = processorRef.apvts.getParameter ("f2_cutoff"))
        {
            const auto open = FilterDisplay::isPassThrough (processorRef, 1, operatorFilterOff);
            const auto current = parameter->convertFrom0to1 (parameter->getValue());
            const auto target = open ? juce::jmin (rememberedCutoff, 19000.0f) : 20000.0f;
            if (! open)
                rememberedCutoff = current;
            processorRef.performEdit (parameter->getName (64), [parameter, target]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (target));
                parameter->endChangeGesture();
            });
        }
    }

    float rememberedCutoff = 8000.0f;

    // The OSC area's own scroll bar (thin, over the cards' side margin), shown only while the cards overflow it.
    void syncOscBar()
    {
        auto* page = oscView.getViewedComponent();
        if (page == nullptr)
            return;
        oscBar.setRangeLimits (0.0, (double) page->getHeight());
        oscBar.setCurrentRange ((double) oscView.getViewPositionY(), (double) oscView.getHeight());
        oscBar.setVisible (page->getHeight() > oscView.getHeight() + 1);
    }

    void scrollBarMoved (juce::ScrollBar*, double newStart) override
    {
        oscView.jumpTo (juce::roundToInt (newStart));
    }

    int readInt (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    // What the bands may take: the oscillator cards' own height (their page's
    // minimum, with its add row) first.
    int bandsHeightFor (int pageHeight) const
    {
        auto oscHeight = 0;
        if (auto* page = oscView.getPage())
            oscHeight = page->getReferenceHeight();
        return pageHeight - bottomMargin - oscHeight;
    }

    void paintCardWithHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint) const
    {
        if (card.isEmpty())
            return;

        IlanaTheme::paintCard (g, card.toFloat(), 10.0f, tint);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (card.getX() + 1, card.getY() + headerHeight - 1, card.getWidth() - 2, 1));
        const auto centre = card.getY() + headerHeight / 2;
        IlanaTheme::paintTag (g, { (float) card.getX() + 14.0f, (float) centre }, tint);
        if (title.isEmpty())
            return;
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (13.0f, true));
        IlanaTheme::drawFitted (g, title, juce::Rectangle<int> (card.getX() + 23, centre - 9, titleWidth (title), 18), juce::Justification::centredLeft, 1);
    }

    static int titleWidth (const juce::String& title)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (13.0f, true)), title) + 4;
    }

    // Knobs in one row at `pitch`, starting at `x` (or centred in `area` when x < 0).
    static void placeKnobRow (juce::Rectangle<int> area, const std::vector<KnobControl*>& row, int pitch = cell, int extraBelow = 0, int x = -1)
    {
        const auto width = (int) row.size() * pitch;
        if (x < 0)
            x = area.getX() + (area.getWidth() - width) / 2;
        for (auto* knob : row)
        {
            knob->setBounds (KnobControl::packedBoundsFor ({ x, area.getY() + (area.getHeight() - (int) dialSize - 10 - extraBelow) / 2, pitch, (int) dialSize + 10 + extraBelow }));
            x += pitch;
        }
    }

    static std::vector<KnobControl*> pointers (const std::vector<std::unique_ptr<KnobControl>>& knobs)
    {
        std::vector<KnobControl*> row;
        for (auto& knob : knobs)
            row.push_back (knob.get());
        return row;
    }

    // The switch at the right of a header, as the OSC cards': 30 x 17.
    static juce::Rectangle<int> switchIn (juce::Rectangle<int> card)
    {
        return juce::Rectangle<int> (30, 17).withCentre ({ card.getRight() - 6 - 15, card.getY() + headerHeight / 2 });
    }

    // The mockup's widths (v3, 1060 px), FILTER 520, STRINGS · KEYS 222, WEST COAST 294, with 30 px moved from
    // FILTER's graph to WEST: its three header menus need it at the real fonts (STRIKE BY read "Note Str...").
    void layoutBand1 (juce::Rectangle<int> band)
    {
        const auto total = band.getWidth() - 2 * hGap;
        auto row = band;
        filterBounds = row.removeFromLeft (juce::roundToInt ((float) total * 490.0f / 1036.0f));
        row.removeFromLeft (hGap);
        stringsBounds = row.removeFromLeft (juce::roundToInt ((float) total * 222.0f / 1036.0f));
        row.removeFromLeft (hGap);
        westBounds = row;

        {
            const auto card = filterBounds;
            const auto centre = card.getY() + headerHeight / 2;
            const auto shown = filterTabs.getSelected();
            auto x = card.getX() + 23 + titleWidth ("FILTER") + 4;
            filterTabs.setBounds ({ x, centre - 8, filterTabs.idealWidth(), 16 });
            x += filterTabs.getWidth() + 6;
            for (auto& parts : filters)
                parts.type->setBounds ({ x, centre - 8, 84, 16 });
            x += 84 + 4;
            for (auto& parts : filters)
                parts.slope->setBounds ({ x, centre - 8, 42, 16 });
            x += filters[(size_t) shown].slope->isVisible() ? 42 + 4 : 0;
            routing->setBounds ({ x, centre - 8, 104, 16 });
            f2Switch.setBounds (switchIn (card));

            // (The graph's edge is 5 from the card on the left; the last dial's ring is 5 from it on the right.)
            auto body = card.withTrimmedTop (headerHeight).withTrimmedLeft (5).withTrimmedRight (1).reduced (0, 3);
            const auto row = filterKnobRow();
            const auto knobsWidth = (int) row.size() * cell;
            filterDisplay.setBounds (body.removeFromLeft (body.getWidth() - knobsWidth - 2));
            // (Room under the dials for KEY TRACK's second line.)
            placeKnobRow (body.withTrimmedTop (-2), row, cell, 13);
        }

        {
            const auto card = stringsBounds;
            const auto centre = card.getY() + headerHeight / 2;
            stringsTabs.setBounds ({ card.getX() + 22, centre - 8, stringsTabs.idealWidth(), 16 });
            stringsOn->setBounds (switchIn (card));
            const auto top = card.getY() + headerHeight + 5;
            stringsManual->setBounds ({ card.getX() + 6, top, 64, 15 });
            tuneButton.setBounds ({ card.getX() + 72, top, 18, 15 });
            stringsCaption = juce::Rectangle<int> (card.getX() + 94, top, card.getWidth() - 94 - 7, 15);
            const auto knobArea = card.withTop (top + 15).withBottom (card.getBottom() - 2);
            placeKnobRow (knobArea, pointers (stringsKnobs), 44);
            placeKnobRow (card.withTrimmedTop (headerHeight).reduced (0, 3), pointers (keysKnobs), 42, 13);
        }

        {
            const auto card = westBounds;
            const auto centre = card.getY() + headerHeight / 2;
            westOn->setBounds (switchIn (card));
            auto x = card.getX() + 23 + titleWidth ("WEST COAST") + 4;
            const auto end = westOn->getX() - 4;
            // Three menus share what the title and the switch leave, GATE and PLACE as wide as their words.
            const auto modeW = 52, placeW = 54;
            westMode->setBounds ({ x, centre - 8, modeW, 16 });
            x += modeW + 2;
            westPosition->setBounds ({ x, centre - 8, placeW, 16 });
            x += placeW + 2;
            westSource->setBounds ({ x, centre - 8, juce::jmax (40, end - x), 16 });
            // The picture at the left, the seven knobs at a 31 pitch to its right.
            auto body = card.withTrimmedTop (headerHeight).withTrimmedLeft (5).withTrimmedRight (1).reduced (0, 3);
            const auto knobsWidth = (int) westKnobs.size() * westCell;
            westDisplay->setBounds (body.removeFromLeft (body.getWidth() - knobsWidth - 2));
            placeKnobRow (body, pointers (westKnobs), westCell, 9);
        }
    }

    // ENVELOPE 321, LFO 272, BODY · BOARD 185, SUB 124, NOISE 124.
    void layoutBand2 (juce::Rectangle<int> band)
    {
        const auto total = band.getWidth() - 4 * hGap;
        const auto widthOf = [total] (float design) { return juce::roundToInt ((float) total * design / 1026.0f); };
        auto row = band;
        envelopeCard.setBounds (row.removeFromLeft (widthOf (321.0f)));
        row.removeFromLeft (hGap);
        lfoCard.setBounds (row.removeFromLeft (widthOf (272.0f)));
        row.removeFromLeft (hGap);
        bodyBounds = row.removeFromLeft (widthOf (185.0f));
        row.removeFromLeft (hGap);
        subBounds = row.removeFromLeft ((row.getWidth() - hGap) / 2);
        row.removeFromLeft (hGap);
        noiseBounds = row;

        {
            const auto card = bodyBounds;
            const auto centre = card.getY() + headerHeight / 2;
            auto x = card.getX() + 22;
            bodyTab.setBounds ({ x, centre - 8, bodyTab.idealWidth(), 16 });
            x += bodyTab.getWidth() + 4;
            bodyOn->setBounds ({ x, centre - 8, 28, 16 });
            x += 28 + 6;
            boardTab.setBounds ({ x, centre - 8, boardTab.idealWidth(), 16 });
            x += boardTab.getWidth() + 4;
            boardOn->setBounds ({ x, centre - 8, 28, 16 });
            const auto top = card.getY() + headerHeight + 5;
            const auto inner = card.reduced (6, 0);
            boardModel->setBounds ({ inner.getX(), top, inner.getWidth(), 16 });
            placeKnobRow (card.withTop (top + 18).withBottom (card.getBottom() - 3), pointers (boardKnobs), 43);

            // BODY: two rows. TYPE over COUPLING beside AMOUNT DECAY COUPLE, then MATERIAL SIZE OFFSET KEY TRK.
            const auto rows = card.withTop (card.getY() + headerHeight + 1).withBottom (card.getBottom() - 2);
            const auto row1 = rows.withHeight (rows.getHeight() / 2);
            const auto row2 = rows.withTop (row1.getBottom());
            constexpr int menuWidth = 72, bodyPitch = 33;
            const auto menuX = inner.getX() + (inner.getWidth() - menuWidth - 3 * bodyPitch) / 2;
            bodyType->setBounds ({ menuX, row1.getCentreY() - 18, menuWidth, 16 });
            bodyCoupling->setBounds ({ menuX, row1.getCentreY() + 2, menuWidth, 16 });
            placeKnobRow (row1, { bodyKnobs[0].get(), bodyKnobs[1].get(), bodyKnobs[2].get() }, bodyPitch, 0, menuX + menuWidth);
            placeKnobRow (row2, { bodyKnobs[3].get(), bodyKnobs[4].get(), bodyKnobs[5].get(), bodyKnobs[6].get() }, 43);
        }

        for (const auto isNoise : { false, true })
        {
            const auto card = isNoise ? noiseBounds : subBounds;
            (isNoise ? noiseOn : subOn)->setBounds (switchIn (card));
            const auto top = card.getY() + headerHeight + 5;
            const auto inner = card.reduced (5, 0);
            if (isNoise)
            {
                noiseType->setBounds ({ inner.getX(), top, inner.getWidth(), 16 });
            }
            else
            {
                subShape->setBounds ({ inner.getX(), top, inner.getWidth() - 46, 16 });
                subOctave->setBounds ({ inner.getRight() - 42, top, 42, 16 });
            }
            const auto graphTop = top + 16 + 4;
            const auto graph = juce::Rectangle<int> (inner.getX(), graphTop, 44, card.getBottom() - 4 - graphTop);
            (isNoise ? static_cast<juce::Component*> (noiseBand.get()) : subWave.get())->setBounds (graph);
            const auto knobArea = juce::Rectangle<int> (graph.getRight(), graphTop, inner.getRight() - graph.getRight(), graph.getHeight());
            placeKnobRow (knobArea, isNoise ? std::vector<KnobControl*> { noiseLevel.get(), noiseColour.get() }
                                            : std::vector<KnobControl*> { subLevel.get(), subColourKnob.get() }, cell);
        }
    }

    // The strings' six notes, over the oscillator cards right above the STRINGS card.
    juce::Rectangle<int> tuningBounds() const
    {
        const auto width = TuningPopover::widthFor (6), height = TuningPopover::height;
        const auto left = juce::jlimit (sideMargin, getWidth() - sideMargin - width, stringsBounds.getCentreX() - width / 2);
        return { left, stringsBounds.getY() - height - 4, width, height };
    }

    void showTuning (bool show)
    {
        show = show && readInt ("sym_manual") != 0;
        if (show == tuningPopover.isVisible())
            return;
        if (show)
        {
            tuningPopover.setBounds (tuningBounds());
            tuningPopover.setVisible (true);
            tuningPopover.toFront (false);
        }
        else
        {
            tuningPopover.setVisible (false);
        }
        tuneButton.setToggleState (show, juce::dontSendNotification);
    }

    // PLAY's TYPE list, grouped as the FILTER page's grid: its pages, then
    // its families, under the tiles' names.
    void showFilterTypeMenu (juce::ComboBox& combo, const juce::String& id, int filterIndex)
    {
        auto* parameter = processorRef.apvts.getParameter (id);

        if (parameter == nullptr)
            return;

        const auto current = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
        const auto names = FilterTypeGrid::shortNames();
        juce::PopupMenu menu;

        for (int page = 0; page < FilterTypeGrid::getNumPages(); ++page)
        {
            juce::PopupMenu pageMenu;
            auto holdsCurrent = false;

            for (const auto& family : FilterTypeGrid::getFamilies (page))
            {
                pageMenu.addSectionHeader (family.first);

                for (const auto type : family.second)
                {
                    pageMenu.addItem (type + 1, names[type], true, type == current);
                    holdsCurrent = holdsCurrent || type == current;
                }
            }

            menu.addSubMenu (FilterTypeGrid::getPageName (page), pageMenu, true, nullptr, holdsCurrent);
        }

        juce::Component::SafePointer<PlayPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&combo),
                            [safe, parameter, filterIndex] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;

                                safe->processorRef.performEdit ("Filter " + juce::String (filterIndex + 1) + " type", [parameter, result]
                                {
                                    parameter->beginChangeGesture();
                                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (result - 1)));
                                    parameter->endChangeGesture();
                                });
                            });
    }

    // As on the FILTER page: only the classic models have a slope.
    bool filterHasSlope (int filterIndex) const
    {
        const auto type = juce::jlimit (0, FilterType::Count - 1, readInt (filterIndex == 0 ? "f1_type" : "f2_type"));
        return type != FilterType::CombPlus && type != FilterType::CombMinus && type != FilterType::Formant
               && FilterType::usesSlope (type);
    }

    static constexpr int westCell = 31;

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    OscPageViewport oscView;
    OscScrollBar oscBar { true };
    SourceCarouselCard envelopeCard, lfoCard;
    FilterDisplay filterDisplay;
    ViewPills filterTabs { { "F1", "F2" }, filterColour (0), "Which filter's type and knobs show; the graph draws both" };
    std::array<FilterCard, 2> filters;
    std::unique_ptr<PillChoice> routing;
    std::unique_ptr<KnobControl> balance;
    juce::TextButton f2Switch;
    ViewPills stringsTabs { { "STRINGS", "KEYS" }, stringsColour(), "STRINGS: the sympathetic strings. KEYS: the piano's stretch, pedal and key noises." };
    std::unique_ptr<ToggleControl> stringsOn, stringsManual;
    NoteButton tuneButton;
    TuningPopover tuningPopover;
    std::vector<std::unique_ptr<KnobControl>> stringsKnobs, keysKnobs, tuningNotes;
    juce::Rectangle<int> filterBounds, stringsBounds, westBounds, bodyBounds, subBounds, noiseBounds, stringsCaption;
    ViewPills bodyTab { { "BODY" }, bodyColour(), "Show BODY's controls (its switch is beside it)" };
    ViewPills boardTab { { "BOARD" }, bodyColour(), "Show the SOUNDBOARD's controls (its switch is beside it)" };
    std::unique_ptr<ToggleControl> westOn, bodyOn, boardOn, subOn, noiseOn;
    std::unique_ptr<WestDisplay> westDisplay;
    std::unique_ptr<SubWaveDisplay> subWave;
    std::unique_ptr<NoiseColourDisplay> noiseBand;
    std::unique_ptr<ComboControl> westMode, westPosition, westSource, bodyType, bodyCoupling, boardModel, subShape, noiseType;
    std::unique_ptr<PillChoice> subOctave;
    std::vector<std::unique_ptr<KnobControl>> westKnobs, bodyKnobs, boardKnobs;
    std::unique_ptr<KnobControl> subLevel, subColourKnob, noiseLevel, noiseColour;
};
} // namespace
