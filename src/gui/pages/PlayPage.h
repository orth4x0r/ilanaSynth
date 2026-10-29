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

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    // Stacked (FILTER's SLOPE): the pills one over the other in a knob's cell, the caption under them
    // where a knob's name sits.
    void setStacked (const juce::String& captionText)
    {
        caption = captionText;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = optionArea();
        const auto stacked = caption.isNotEmpty();
        const auto width = bounds.getWidth() / (float) (stacked ? 1 : labels.size());
        const auto height = stacked ? bounds.getHeight() / (float) labels.size() : bounds.getHeight();
        const auto mouse = getMouseXYRelative().toFloat();
        if (stacked)
        {
            g.setColour (IlanaTheme::Ui::text3.withMultipliedAlpha (isEnabled() ? 1.0f : 0.5f));
            g.setFont (stackCaptionFont());
            g.drawText (caption, juce::Rectangle<float> (0.0f, bounds.getBottom() + 1.0f, (float) getWidth(), (float) getHeight() - bounds.getBottom() - 1.0f),
                        juce::Justification::centredTop, false);
        }
        for (int option = 0; option < labels.size(); ++option)
        {
            const auto cellBounds = (stacked ? juce::Rectangle<float> (bounds.getX(), bounds.getY() + height * (float) option, width, height).reduced (0.0f, 1.0f)
                                             : juce::Rectangle<float> (bounds.getX() + width * (float) option, bounds.getY(), width, bounds.getHeight()).reduced (1.5f, 1.0f));
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
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
            g.drawText (labels[option], cellBounds, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto area = optionArea();
        const auto at = caption.isNotEmpty() ? (event.position.y - area.getY()) / area.getHeight() : (event.position.x - area.getX()) / area.getWidth();
        setValue (values[(size_t) juce::jlimit (0, labels.size() - 1, (int) (at * (float) labels.size()))]);
    }

    // A stacked choice in a packed knob's bounds: the pills where its dial is, the caption where its name is.
    static constexpr int stackPadX = 3, stackPadTop = 6, stackDial = 28;

private:
    juce::Rectangle<float> optionArea() const
    {
        if (caption.isEmpty())
            return getLocalBounds().toFloat();
        return juce::Rectangle<float> ((float) stackPadX + 1.0f, (float) stackPadTop, (float) getWidth() - 2.0f * (float) stackPadX - 2.0f, (float) stackDial);
    }

    static juce::Font stackCaptionFont() { return IlanaTheme::font (juce::jlimit (7.6f, 8.6f, 7.6f * (float) stackDial / 26.0f), true); }

    juce::String caption;
    juce::StringArray labels;
    juce::Colour colour;
    std::vector<int> values;
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
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedTitle, true));
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
    // A small dot on a pill that is not shown but is switched on (FILTER's WEST pill while WEST plays).
    std::function<bool (int)> isMarked;
    std::function<juce::Colour (int)> markColour;

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
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true)), text) + 2;
    }

    int idealWidth() const
    {
        auto width = 0;
        for (const auto& label : labels)
            width += wordWidth (label) + 2 * pad;
        return width + pillGap * juce::jmax (0, labels.size() - 1) + (isMarked != nullptr ? 6 : 0);
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
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
            const auto marked = ! on && isMarked != nullptr && isMarked (option);
            g.drawText (labels[option], marked ? cell.withTrimmedLeft (6.0f) : cell, juce::Justification::centred);
            if (marked)
            {
                g.setColour (markColour != nullptr ? markColour (option) : colour);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ cell.getX() + 7.0f, cell.getCentreY() }));
            }
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

    int idealWidth() const { return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true)), getButtonText()) + 25; }

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
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
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
        auto c = bounds.getCentre();
        g.setColour (isEnabled() ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
        // Wide enough, the note says NOTES after it.
        if (bounds.getWidth() > 40.0f)
        {
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::packedBody, true);
            const auto textW = juce::GlyphArrangement::getStringWidth (font, "NOTES");
            const auto left = c.x - (textW + 10.0f) * 0.5f;
            g.setFont (font);
            g.drawText ("NOTES", juce::Rectangle<float> (left + 10.0f, bounds.getY(), textW + 2.0f, bounds.getHeight()), juce::Justification::centredLeft, false);
            c.x = left + 2.5f;
        }
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
        setName (kind == Kind::envelope ? "PLAY envelope card" : "PLAY LFO card");
        previous.setName (getName() + " previous");
        next.setName (getName() + " next");
        edit.onClick = [this] { if (onEdit != nullptr && ! sources.empty()) onEdit (selected, edit); };
        addAndMakeVisible (edit);
        refreshSources();
        startTimerHz (6);
    }

    // The selected source's pop-up editor (EDIT, or a click on the title), by `anchor`.
    std::function<void (int source, juce::Component& anchor)> onEdit;

    static constexpr int headerHeight = 20;

    int getSelectedSource() const { return selected; }

    // Colour coding: the card, its tabs and controls in the source's new colour.
    void refreshColours()
    {
        edit.setColour (sourceTabColour (selected));
        buildBody();
        resized();
        repaint();
        for (auto& tab : tabs)
            tab->repaint();
    }

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
        // (No title: the pills say which envelope or LFO shows, ilana's layout 2026-10-29.)
        IlanaTheme::paintTag (g, { 14.0f, (float) centre }, colour);

        if (! chipBounds.isEmpty())
        {
            const auto chip = chipBounds.toFloat().reduced (0.5f);
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (chip, chip.getHeight() * 0.5f, 1.0f);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ chip.getX() + 8.0f, chip.getCentreY() }));
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
            IlanaTheme::drawFitted (g, chipText, chipBounds.withTrimmedLeft (14).withTrimmedRight (4), juce::Justification::centredLeft, 1);
        }
    }

    void resized() override
    {
        const auto centre = headerHeight / 2;
        auto x = 23;
        previous.setBounds ({ x, centre - 7, 14, 14 });
        x += 17;
        const auto tabWidth = kind == Kind::envelope ? 42 : 20;
        // As many pills as fit before "+", ">" and EDIT; when there are more, the row shows a window of them
        // that always holds the selected one (the arrows scroll it), so every source can be seen and picked.
        const auto room = getWidth() - 14 - edit.idealWidth() - 17 - 17 - 8 - x;
        const auto capacity = juce::jlimit (1, juce::jmax (1, (int) tabs.size()), (room + 3) / (tabWidth + 3));
        const auto selectedIndex = (int) (std::find (sources.begin(), sources.end(), selected) - sources.begin());
        if (selectedIndex < (int) sources.size())
        {
            if (selectedIndex < firstShown)
                firstShown = selectedIndex;
            else if (selectedIndex >= firstShown + capacity)
                firstShown = selectedIndex - capacity + 1;
        }
        firstShown = juce::jlimit (0, juce::jmax (0, (int) tabs.size() - capacity), firstShown);
        juce::String shownList;
        for (int i = 0; i < (int) tabs.size(); ++i)
        {
            auto& tab = tabs[(size_t) i];
            const auto shown = i >= firstShown && i < firstShown + capacity;
            tab->setVisible (shown);
            if (shown)
            {
                tab->setBounds ({ x, centre - 8, tabWidth, 16 });
                x += tabWidth + 3;
                shownList << tab->getSource() << ",";
            }
        }
        // For the UI tests: the selected source and the sources with a pill on screen.
        getProperties().set ("selectedSource", selected);
        getProperties().set ("shownSources", shownList);
        add.setBounds ({ x, centre - 7, 14, 14 });
        next.setBounds ({ x + 17, centre - 7, 14, 14 });
        edit.setBounds ({ getWidth() - 7 - edit.idealWidth(), centre - 8, edit.idealWidth(), 16 });
        layoutBody();
    }

    // A right-click on the title or a pill: Copy, Paste and Init that
    // envelope or LFO (ilana 2026-10-28); on the dot, its colour.
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu() || sources.empty())
            return;

        // Colour coding: on the card's dot, the source's colour instead.
        if (event.eventComponent == this && event.y < headerHeight && event.x < 28)
            if (const auto key = SectionColours::keyForSource (selected); key >= 0)
            {
                SectionColourPicker::show (processorRef, key, ModNames::source (selected, &processorRef), *this, { 4, 0, 20, headerHeight });
                return;
            }

        auto source = selected;
        for (auto& tab : tabs)
            if (event.eventComponent == tab.get())
                source = tab->getSource();

        if (event.eventComponent == this && event.y >= headerHeight)
            return;

        showModuleMenu (source);
    }

    juce::PopupMenu buildModuleMenu (int source)
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (sourceTabName (source, processorRef));
        const auto lfo = Mod::lfoIndexFor ((Mod::Source) source);
        const auto env = SourceEditor::envelopeOf (source);
        juce::Component::SafePointer<SourceCarouselCard> safeThis (this);
        const auto changed = [safeThis] { if (safeThis != nullptr) safeThis->buildBody(); };

        if (kind == Kind::lfo && lfo >= 0)
            ModuleMenu::addItems (menu, processorRef, ModuleMenu::Kind::Lfo, lfo, changed);
        else if (kind == Kind::envelope && env >= 0)
            ModuleMenu::addItems (menu, processorRef, ModuleMenu::Kind::Envelope, env, changed);

        return menu;
    }

    void showModuleMenu (int source)
    {
        buildModuleMenu (source).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
    }

    // A click on the dot, left of the pills, opens the pop-up editor too.
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            return;

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
                tab->addMouseListener (this, false); // (its right-click: the module menu)
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

        // (UI review 25: the graphs grow, the knobs go in a grid at its right.) The body keeps 7 px at the sides, 3 above and below.
        auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (7, 3);
        const auto place = [&] (KnobControl& knob, int x, int y)
        {
            knob.setBounds (KnobControl::packedBoundsFor ({ x, y, cell, (int) dialSize + 10 }));
        };

        if (kind == Kind::envelope)
        {
            // DELAY ATTACK HOLD over DECAY SUSTAIN RELEASE at 45 px, so no two names touch; the graph takes the rest.
            const auto grid = body.removeFromRight (2 * envPitch + cell);
            body.removeFromRight (gapToKnobs);
            graph->setBounds (body);
            for (size_t i = 0; i < knobs.size(); ++i)
                place (*knobs[i], grid.getX() + (int) (i % 3) * envPitch, i < 3 ? grid.getY() : grid.getBottom() - (int) dialSize - 10);
            return;
        }

        // RATE, SMOOTH and START on top at 43 px, SHAPE and SYNC / RETRIG / KEY under them; the graph at the left with
        // what the LFO drives under it.
        auto column = body.removeFromRight (2 * lfoPitch + cell);
        body.removeFromRight (gapToKnobs);
        chipBounds = body.removeFromBottom (rowHeight);
        body.removeFromBottom (4);
        graph->setBounds (body);
        place (rate->getRateKnob(), column.getX(), column.getY());
        rate->matchBounds();
        for (size_t i = 0; i < knobs.size(); ++i)
            place (*knobs[i], column.getX() + (int) (i + 1) * lfoPitch, column.getY());
        const auto switches = column.removeFromBottom (rowHeight);
        column.removeFromBottom (3);
        shape->setBounds (column.removeFromBottom (rowHeight));
        // Three pills at one gap, as wide as the column, the padding round each word the same.
        auto textTotal = 0.0f;
        for (auto& toggle : toggles)
            textTotal += toggle->textWidth();
        const auto count = juce::jmax (1, (int) toggles.size());
        const auto pillGap = 2.0f;
        const auto pad = ((float) switches.getWidth() - pillGap * (float) (count - 1) - textTotal) / (float) (2 * count);
        auto edge = (float) switches.getX();
        for (auto& toggle : toggles)
        {
            const auto right = edge + toggle->textWidth() + 2.0f * pad;
            toggle->setBounds (switches.withLeft (juce::roundToInt (edge)).withRight (juce::roundToInt (right)));
            edge = right + pillGap;
        }
    }

    static constexpr int cell = 35, envPitch = 45, lfoPitch = 43, gapToKnobs = 5, rowHeight = 16;

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
    int selected = 0, firstShown = 0;
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
    IlanaAnim::MorphSource shapeMorph { *this }; // (its old shape fades out over a new one)

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
        // (Off, the whole curve dims as the card's knobs do: the trace sets its own colours, so through a layer.)
        g.beginTransparencyLayer (on);
        IlanaTrace::paintTrace (g, path, colour);
        g.endTransparencyLayer();
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
    IlanaAnim::MorphSource shapeMorph { *this }; // (its old shape fades out over a new one)

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
        IlanaTrace::paintArea (g, path, IlanaTheme::subNoiseColour(), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, IlanaTheme::subNoiseColour());
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
        IlanaTrace::paintTrace (g, path, IlanaTheme::subNoiseColour());
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
        IlanaTrace::paintArea (g, path, IlanaTheme::subNoiseColour(), plot.getBottom(), plot.getX(), plot.getRight());
        IlanaTrace::paintTrace (g, path, IlanaTheme::subNoiseColour());
    }
};

// BOARD: the soundboard's modes as bars across the pitch range (70 Hz to 3.6 kHz), each as tall as it
// rings long. SIZE slides them (a bigger board is lower and rings longer), TONE's lid darkens the top,
// MODEL Dense packs them closer. Drawn from AcousticKeys.h's own tables.
class BoardResponseDisplay : public ParamPicture
{
public:
    explicit BoardResponseDisplay (IlanaSynthAudioProcessor& p)
        : ParamPicture (p, { "sb_model", "sb_size", "sb_tone", "sb_on" },
                        "The board's modes: low pitches left, each bar as tall as that mode rings. SIZE slides them, TONE darkens the top, MODEL Dense packs them closer.") {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 4.0f);
        const auto on = read ("sb_on") > 0.5f ? 1.0f : 0.45f;
        g.setColour (IlanaTheme::Ui::text3.withMultipliedAlpha (on));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedLabel, true).withKerningFactor (0.09f));
        g.drawText ("BOARD RESPONSE", bounds.reduced (6.0f, 3.0f), juce::Justification::topLeft, false);

        const auto plot = bounds.reduced (6.0f, 3.0f).withTrimmedTop (12.0f);
        const auto size = (double) juce::jlimit (0.0f, 1.0f, read ("sb_size"));
        const auto tone = (double) juce::jlimit (0.0f, 1.0f, read ("sb_tone"));
        const auto dense = juce::roundToInt (read ("sb_model")) == 1;
        constexpr double frequencies[] { 92.0, 118.0, 147.0, 181.0, 223.0, 270.0, 330.0, 402.0, 488.0,
                                         590.0, 712.0, 860.0, 1040.0, 1260.0, 1530.0, 1850.0, 2250.0, 2750.0 };
        const auto count = dense ? 36 : (int) std::size (frequencies);
        const auto scale = std::exp2 (0.6 - 1.2 * size);
        const auto corner = 450.0 + 6000.0 * tone * tone;
        constexpr auto lowHz = 70.0, highHz = 3600.0;
        const auto colour = IlanaTheme::subNoiseColour();
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getBottom() - 1.0f, plot.getWidth(), 1.0f));
        for (int m = 0; m < count; ++m)
        {
            // (Dense: the classic set's span with twice the modes, spaced evenly in pitch.)
            const auto base = dense ? 92.0 * std::pow (2750.0 / 92.0, (double) m / (double) (count - 1)) : frequencies[m];
            const auto hz = base * scale;
            if (hz < lowHz || hz > highHz)
                continue;
            const auto t60 = (0.28 - 0.2 * (double) m / (double) count) * (0.6 + 0.9 * size);
            const auto lid = 1.0 / std::sqrt (1.0 + std::pow (hz / corner, 4.0));
            const auto level = juce::jlimit (0.06, 1.0, t60 / 0.42 * (0.35 + 0.65 * lid));
            const auto x = plot.getX() + (float) (std::log (hz / lowHz) / std::log (highHz / lowHz)) * plot.getWidth();
            const auto h = (float) level * plot.getHeight();
            g.setColour (colour.withAlpha ((0.4f + 0.55f * (float) lid) * on));
            g.fillRoundedRectangle (juce::Rectangle<float> (x - 1.2f, plot.getBottom() - h, 2.4f, h), 1.0f);
        }
    }
};

// FILTER's BALANCE while the filters run in parallel: a slider in the header, F1 at the left, F2 at
// the right, the middle marked. Drag it; a double-click centres it.
class BalanceSlider : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    BalanceSlider (juce::AudioProcessorValueTreeState& state, const juce::String& id)
        : parameter (*state.getParameter (id)),
          attachment (parameter, [this] (float) { repaint(); }, nullptr)
    {
        setTooltip ("BALANCE\nThe mix of Filter 1 and Filter 2 while they run in parallel. Double-click: the middle.");
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        attachment.sendInitialUpdate();
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::packedBody, true);
        g.setFont (font);
        g.setColour (IlanaTheme::Ui::text2);
        g.drawText ("F1", bounds.withWidth (labelWidth), juce::Justification::centredLeft, false);
        g.drawText ("F2", bounds.withTrimmedLeft (bounds.getWidth() - labelWidth), juce::Justification::centredRight, false);
        const auto track = trackArea();
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (track.withSizeKeepingCentre (track.getWidth(), 3.0f), 1.5f);
        g.setColour (IlanaTheme::Ui::text3);
        g.fillRect (juce::Rectangle<float> (1.0f, 9.0f).withCentre (track.getCentre()));
        const auto x = track.getX() + parameter.getValue() * track.getWidth();
        const auto thumb = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, track.getCentreY() });
        g.setColour (colour);
        g.fillEllipse (thumb.expanded (2.0f));
        g.setColour (IlanaTheme::Ui::text);
        g.fillEllipse (thumb);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        attachment.beginGesture();
        dragTo (event.position.x);
    }

    void mouseDrag (const juce::MouseEvent& event) override { dragTo (event.position.x); }
    void mouseUp (const juce::MouseEvent&) override { attachment.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (parameter.getDefaultValue())); }

private:
    static constexpr float labelWidth = 14.0f;

    juce::Rectangle<float> trackArea() const { return getLocalBounds().toFloat().reduced (labelWidth + 6.0f, 0.0f); }

    void dragTo (float x)
    {
        const auto track = trackArea();
        attachment.setValueAsPartOfGesture (parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, (x - track.getX()) / track.getWidth())));
    }

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    juce::Colour colour = IlanaTheme::Ui::text2;
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
    // (Packed as the other pop-ups, UI review 26: four 48 px cells, PLAY's knob pitch, not spread over 262 px.)
    static constexpr int cell = 48, width = 4 * cell + 2 * 12 + 6, height = 98, headerHeight = 22;
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
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::packedBody, true));
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
        // (Under LOOSE, the knob it belongs to, its right edge on the knobs'.)
        looseBoth.setBounds (foot.withLeft (juce::jmax (foot.getX(), x - 100)).withRight (x));
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
                 private IlanaTheme::IdentityColourListener,
                 private juce::AudioProcessorValueTreeState::Listener,
                 private juce::AsyncUpdater,
                 private juce::Timer
{
public:
    explicit PlayPage (IlanaSynthAudioProcessor& p)
        : processorRef (p), oscView (p), envelopeCard (p, SourceCarouselCard::Kind::envelope), lfoCard (p, SourceCarouselCard::Kind::lfo),
          filterDisplay (p)
    {
        IlanaTheme::identityColourListeners (&p).add (this);
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
        // (WEST COAST is FILTER's third view, ilana's layout 2026-10-29: its pill carries a dot while it plays.)
        filterTabs.isMarked = [this] (int index) { return index == 2 && readInt ("west_on") != 0; };
        filterTabs.markColour = [] (int) { return westColour(); };
        filterTabs.addMouseListener (this, false); // (its right-click: the filter's module menu)
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
            card.slope = std::make_unique<PillChoice> (state, prefix + "_slope", juce::StringArray { "12 dB", "24 dB" }, colour, "SLOPE\n12 dB is gentler and brighter; 24 dB cuts harder.");
            card.slope->setStacked ("SLOPE");
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
        balance = std::make_unique<BalanceSlider> (state, "filter_balance");
        addChildComponent (*balance);
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
        stringsManual = std::make_unique<PillChoice> (state, "sym_manual", juce::StringArray { "AUTO", "MANUAL" }, stringsColour(),
                                                      "TUNING\nAUTO tunes the strings to what you play; MANUAL to the six NOTES you pick.");
        addAndMakeVisible (*stringsManual);
        tuneButton.setTooltip ("NOTES\nThe sympathetic strings' six notes (MANUAL tuning)");
        tuneButton.onClick = [this] { showTuning (! tuningPopover.isVisible() || tuningGrow.isClosing()); };
        addAndMakeVisible (tuneButton);
        for (const auto& [id, label] : { std::pair<const char*, const char*> { "sym_amount", "AMOUNT" }, { "sym_count", "COUNT" }, { "sym_decay", "DECAY" } })
            stringsKnobs.push_back (knob (id, label, stringsColour()));
        // (One line each: KEY, DAMPER and PEDAL sit under one NOISE label; RING is the pedal's resonance, a word that fits 43 px.)
        for (const auto& [id, label] : { std::pair<const char*, const char*> { "stretch", "STRETCH" }, { "pedal_res", "RING" }, { "mech_key", "KEY" },
                                         { "mech_damper", "DAMPER" }, { "mech_pedal", "PEDAL" } })
            keysKnobs.push_back (knob (id, label, stringsColour()));
        for (int i = 0; i < 6; ++i)
        {
            auto note = std::make_unique<KnobControl> (state, "sym_note" + juce::String (i + 1), "NOTE " + juce::String (i + 1), stringsColour(), false);
            note->setPackedDial (36.0f);
            tuningPopover.addAndMakeVisible (*note);
            tuningNotes.push_back (std::move (note));
        }
        tuningPopover.isOwnButton = [this] (juce::Component* c) { return c == &tuneButton || c == stringsManual.get(); };
        tuningPopover.onClose = [this] { showTuning (false); };
        addChildComponent (tuningPopover);

        // WEST COAST: its switch, GATE, PLACE and STRIKE BY menus in the header, the fold's curve and seven knobs.
        westDisplay = std::make_unique<WestDisplay> (p);
        addAndMakeVisible (*westDisplay);
        westOn = bareSwitch ("west_on", westColour(), "WEST COAST on or off: a wavefolder into a low-pass gate");
        westMode = menuFor ("west_mode", "", "GATE\nHow the low-pass gate opens: Combo, Low Pass or VCA.");
        westPosition = menuFor ("west_pos", "", "PLACE\nWhere WEST sits: after the filters, or in Filter 2's place.");
        {
            // (Full words, UI review 25; the saved choice strings stay.)
            auto& box = westPosition->getComboBox();
            const auto selectedId = box.getSelectedId();
            box.changeItemText (2, "As Filter 2");
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        westSource = menuFor ("west_src", "STRIKE BY", "STRIKE BY\nWhat strikes the gate: each note, or a modulation source.");
        {
            auto& box = westSource->getComboBox();
            const auto selectedId = box.getSelectedId();
            box.changeItemText (1, "Note"); // ("Note Strike": STRIKE BY says the rest)
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        for (const auto& spec : { std::pair<const char*, const char*> { "west_fold", "FOLD" }, { "west_sym", "SYMMETRY" }, { "west_stages", "STAGES" },
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
        bodyCoupling = menuFor ("body_coupling_mode", "COUPLING", "COUPLING\nWhich way the strings and the body push on each other.");
        {
            auto& box = bodyCoupling->getComboBox();
            const auto selectedId = box.getSelectedId();
            const auto arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");
            box.changeItemText (2, "String" + arrow + "body");
            box.changeItemText (3, "Body" + arrow + "string");
            box.setSelectedId (selectedId, juce::dontSendNotification);
        }
        for (const auto& spec : { std::pair<const char*, const char*> { "res_amount", "AMOUNT" }, { "res_decay", "DECAY" }, { "body_coupling", "COUPLE" },
                                  { "body_material", "MATERIAL" }, { "body_size", "SIZE" }, { "res_offset", "OFFSET" }, { "res_keytrack", "KEY TRACK" } })
            bodyKnobs.push_back (knob (spec.first, spec.second, bodyColour()));
        boardModel = menuFor ("sb_model", "MODEL", "SOUNDBOARD MODEL\nClassic, or Dense for a thicker board.");
        for (const auto& spec : { std::pair<const char*, const char*> { "sb_size", "SIZE" }, { "sb_tone", "TONE" }, { "sb_mix", "MIX" } })
            boardKnobs.push_back (knob (spec.first, spec.second, bodyColour()));
        boardResponse = std::make_unique<BoardResponseDisplay> (p);
        addAndMakeVisible (*boardResponse);

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
        // WEST off: its menus and knobs dim like every other card's (UI review 25).
        for (auto* part : std::initializer_list<juce::Component*> { westMode.get(), westPosition.get(), westSource.get() })
            effectRules.add (*part, [this] { return readInt ("west_on") != 0; }, "WEST COAST is off: switch it on");
        for (auto& part : westKnobs)
            effectRules.add (*part, [this] { return readInt ("west_on") != 0; }, "WEST COAST is off: switch it on");
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
        dotClock.tick = [this] { tickDots(); };
        for (const auto* id : dotSwitchIds)
            processorRef.apvts.addParameterListener (id, this);
        startTimerHz (4);
    }

    ~PlayPage() override
    {
        IlanaTheme::identityColourListeners (&processorRef).remove (this);
        for (const auto* id : dotSwitchIds)
            processorRef.apvts.removeParameterListener (id, this);
        cancelPendingUpdate();
        stopTimer();
        dotClock.stopTimer();
    }

    // A right-click on a card's dot picks that section's colour (colour
    // coding); elsewhere on the FILTER header, the filter's menu.
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            return;

        const struct { juce::Rectangle<int> card; int key; const char* name; } tags[]
        {
            { filterBounds, westShown() ? SectionColours::westKey : SectionColours::envKey (filterTabs.getSelected() == 0 ? 1 : 2),
              westShown() ? "West coast" : filterTabs.getSelected() == 0 ? "Filter 1" : "Filter 2" },
            { stringsBounds, SectionColours::physicalKey, "Strings and body" },
            { bodyBounds, SectionColours::physicalKey, "Strings and body" },
            { westBounds, SectionColours::westKey, "West coast" },
            { subBounds, SectionColours::subKey, "Sub and noise" },
            { noiseBounds, SectionColours::subKey, "Sub and noise" },
        };

        for (const auto& tag : tags)
        {
            const auto dot = juce::Rectangle<int> (tag.card.getX() + 4, tag.card.getY(), 22, headerHeight);
            if (event.eventComponent == this && ! tag.card.isEmpty() && dot.contains (event.getPosition()))
            {
                SectionColourPicker::show (processorRef, tag.key, tag.name, *this, dot);
                return;
            }
        }

        showFilterMenuFor (event);
    }

    // Colour coding: the cards whose section was given another colour.
    void identityColoursChanged() override
    {
        envelopeCard.refreshColours();
        lfoCard.refreshColours();

        for (int f = 0; f < 2; ++f)
        {
            const auto colour = filterColour (f);
            filters[(size_t) f].slope->setColour (colour);
            for (auto& knob : filters[(size_t) f].knobs)
                knob->setIdentityColour (colour);
        }
        filterTabs.setColour (filterCardColour());
        if (balance != nullptr && ! westShown())
            balance->setColour (filterColour (filterTabs.getSelected()));

        const auto physical = stringsColour();
        stringsTabs.setColour (physical);
        bodyTab.setColour (physical);
        boardTab.setColour (physical);
        for (auto* toggle : { stringsOn.get(), bodyOn.get(), boardOn.get() })
            toggle->setSwitchColour (physical);
        stringsManual->setColour (physical);
        for (auto* list : { &stringsKnobs, &keysKnobs, &tuningNotes, &bodyKnobs, &boardKnobs })
            for (auto& knob : *list)
                knob->setIdentityColour (physical);

        westOn->setSwitchColour (westColour());
        filterTabs.repaint();
        for (auto& knob : westKnobs)
            knob->setIdentityColour (westColour());

        const auto sub = subColour();
        for (auto* toggle : { subOn.get(), noiseOn.get() })
            toggle->setSwitchColour (sub);
        subOctave->setColour (sub);
        for (auto* knob : { subLevel.get(), subColourKnob.get(), noiseLevel.get(), noiseColour.get() })
            knob->setIdentityColour (sub);

        repaint();
    }

    std::function<void (int)> onEditOperator;
    std::function<void (const juce::String&)> onOpenPage;
    // EDIT on ENVELOPE or LFO: the source's floating editor, by the button.
    std::function<void (int source, juce::Component& anchor)> onEditSource;

    // A right-click on the FILTER card's header (or its F1 / F2 pills): Copy,
    // Paste and Init that filter (ilana 2026-10-28).
    void showFilterMenuFor (const juce::MouseEvent& event)
    {
        auto filter = filterTabs.getSelected();

        if (event.eventComponent == &filterTabs)
            filter = juce::jlimit (0, 2, event.x * 3 / juce::jmax (1, filterTabs.getWidth()));
        else if (event.eventComponent != this || ! filterBounds.withHeight (headerHeight).contains (event.getPosition()))
            return;

        if (filter == 2)
            return; // (WEST has no Copy / Paste / Init)

        buildFilterMenu (filter).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&filterTabs).withMousePosition());
    }

    juce::PopupMenu buildFilterMenu (int filter)
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("FILTER " + juce::String (filter + 1));
        ModuleMenu::addItems (menu, processorRef, ModuleMenu::Kind::Filter, filter);
        return menu;
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (size_t i = 0; i < dots.size(); ++i)
        {
            if (dots[i].lit < 0.0f)
            {
                dots[i].target = dotTarget (i);
                dots[i].lit = dots[i].target ? 1.0f : 0.0f;
            }
        }

        // (A switched-off card's dot dims as an off oscillator's does: UI review 26.)
        paintCardWithHeader (g, filterBounds, "FILTER", filterCardColour(), dots[0]);
        paintCardWithHeader (g, stringsBounds, {}, stringsColour(), dots[1]);
        paintCardWithHeader (g, bodyBounds, {}, bodyColour(), dots[3]);
        paintCardWithHeader (g, subBounds, "SUB", subColour(), dots[4]);
        paintCardWithHeader (g, noiseBounds, "NOISE", subColour(), dots[5]);

        // STRINGS' TUNING over AUTO / MANUAL and NOTES; KEYS' NOISE over KEY, DAMPER and PEDAL.
        const auto strings = stringsTabs.getSelected() == 0;
        paintGroupLabel (g, strings ? tuningGroup : noiseGroup, strings ? "TUNING" : "NOISE", ! strings || readInt ("sym_on") != 0);
    }

    // A group's name centred over its controls, a thin rule either side.
    static void paintGroupLabel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text, bool lit)
    {
        if (area.isEmpty())
            return;
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::packedLabel, true).withKerningFactor (0.09f);
        const auto textW = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 2.0f;
        const auto r = area.toFloat();
        const auto alpha = lit ? 1.0f : IlanaTheme::dimmedAlpha;
        g.setColour (IlanaTheme::Ui::line.withMultipliedAlpha (alpha));
        const auto ruleW = (r.getWidth() - textW) * 0.5f - 5.0f;
        if (ruleW > 2.0f)
        {
            g.fillRect (juce::Rectangle<float> (r.getX(), r.getCentreY(), ruleW, 1.0f));
            g.fillRect (juce::Rectangle<float> (r.getRight() - ruleW, r.getCentreY(), ruleW, 1.0f));
        }
        g.setColour (IlanaTheme::Ui::text3.withMultipliedAlpha (alpha));
        g.setFont (font);
        g.drawText (text, r, juce::Justification::centred, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (sideMargin, 0);
        // The oscillator cards take what they need (all three of them, or the page scrolls them);
        // the two module bands sit under them at one height.
        const auto bottom = area.getBottom() - bottomMargin;
        // (ilana's layout, 2026-10-29: two rows of 108, so the oscillator area is a little shorter and scrolls.)
        // (ilana's layout, 2026-10-29: two rows, one height on every patch. 119 rather than her 108, so two wavetable
        // cards and their add row end right above them, no empty strip; a third card scrolls.)
        const auto bandsHeight = juce::jmin (2 * rowHeight + vGap, juce::jmax (2 * minBand + vGap, area.getHeight() / 2));
        // (Three full oscillator cards: the bands sit right under them, the little slack goes to the foot.)
        const auto oscMin = oscView.getPage() != nullptr ? oscView.getPage()->getReferenceHeight() : 0;
        const auto bandsTop = oscMin > bottom - bandsHeight - 14 ? juce::jmin (bottom - bandsHeight, oscMin) : bottom - bandsHeight;
        oscView.setBounds (getLocalBounds().withHeight (juce::jmax (0, bandsTop)));
        oscBar.setBounds (getWidth() - sideMargin + 1, 4, sideMargin - 2, juce::jmax (0, bandsTop - 8));
        syncOscBar();

        layoutBands ({ area.getX(), bandsTop, area.getWidth(), bandsHeight });
        if (tuningPopover.isVisible())
            tuningPopover.setBounds (tuningBounds());

        // The cards' rectangles, for the layout tests' margin scan (x, y, width, height; name).
        juce::String cards;
        const auto note = [&cards] (const char* name, juce::Rectangle<int> r) { cards << name << "," << r.getX() << "," << r.getY() << "," << r.getWidth() << "," << r.getHeight() << ";"; };
        note ("FILTER", filterBounds);
        note ("STRINGS", stringsBounds);
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
        index = juce::jlimit (0, 2, index);
        filterTabs.setSelected (index, false);
        filterTabs.setColour (filterCardColour());
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
        boardResponse->setVisible (index == 1);
        for (auto& knob : boardKnobs)
            knob->setVisible (index == 1);
    }

    bool isTuningShown() const { return tuningPopover.isVisible(); }

    // (The anchor for a pop-up opened from elsewhere, as OP PITCH from FM.)
    juce::Component& getEnvelopeCard() { return envelopeCard; }

private:
    struct FilterCard
    {
        std::unique_ptr<ComboControl> type;
        std::unique_ptr<PillChoice> slope;
        std::vector<std::unique_ptr<KnobControl>> knobs; // CUTOFF, RESO, DRIVE, KEY TRACK, MORPH
    };

    static constexpr int sideMargin = 7, bottomMargin = 4, hGap = 5, vGap = 5, headerHeight = 20, cell = 35;
    static constexpr int minBand = 62, rowHeight = 119;
    static constexpr float dialSize = 28.0f;

    static juce::Colour filterColour (int index) { return IlanaTheme::envIdentityColour (index == 0 ? 1 : 2); }
    static juce::Colour westColour() { return FilterColours::west(); }
    bool westShown() const { return filterTabs.getSelected() == 2; }
    juce::Colour filterCardColour() const { return westShown() ? westColour() : filterColour (filterTabs.getSelected()); }
    static juce::Colour stringsColour() { return IlanaTheme::physicalColour(); }
    static juce::Colour bodyColour() { return IlanaTheme::physicalColour(); }
    static juce::Colour subColour() { return IlanaTheme::subNoiseColour(); }

    void timerCallback() override
    {
        wakeDots();
        operatorFilterOff = operatorVoiceFilterOff (processorRef);
        filterDisplay.setVoiceOff (operatorFilterOff);
        effectRules.apply();
        if (const auto switches = switchesSignature(); switches != lastSwitches)
        {
            lastSwitches = switches;
            filterTabs.repaint();
            repaint();
        }
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
        if (readInt ("sym_on") != stringsSignature)
        {
            stringsSignature = readInt ("sym_on");
            repaint (tuningGroup);
        }
    }

    bool operatorFilterOff = false;
    int filterSignature = -1, stringsSignature = -1;

    // The shown filter's type, slope and knobs; MORPH only on a type that morphs, BALANCE only in parallel.
    void refreshFilterParts()
    {
        const auto shown = filterTabs.getSelected();
        const auto west = shown == 2;
        filterDisplay.setVisible (! west);
        routing->setVisible (! west);
        westDisplay->setVisible (west);
        for (auto* part : std::initializer_list<juce::Component*> { westOn.get(), westMode.get(), westPosition.get(), westSource.get() })
            part->setVisible (west);
        for (auto& knob : westKnobs)
            knob->setVisible (west);
        for (int f = 0; f < 2; ++f)
        {
            auto& card = filters[(size_t) f];
            const auto on = f == shown;
            card.type->setVisible (on);
            card.slope->setVisible (on && filterHasSlope (f) && ! FilterType::usesMorph (readInt (f == 0 ? "f1_type" : "f2_type")));
            // (Slot 5 holds SLOPE on a classic type, MORPH on one that morphs.)
            for (size_t k = 0; k < card.knobs.size(); ++k)
                card.knobs[k]->setVisible (on && (k < 4 || FilterType::usesMorph (readInt (f == 0 ? "f1_type" : "f2_type"))));
        }
        if (! west)
            balance->setColour (filterColour (shown));
        balance->setVisible (! west && readInt ("filters_parallel") != 0);
        f2Switch.setVisible (shown == 1);
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
    int lastSwitches = -1;

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

    int switchesSignature() const
    {
        return readInt ("sym_on") + 2 * readInt ("west_on") + 4 * readInt ("res_on") + 8 * readInt ("sb_on")
               + 16 * readInt ("subosc_on") + 32 * readInt ("noise_on");
    }

    // The cards' header dots (review 26 flair): a dot fades over 180 ms as
    // its card is switched, glows once as it comes on, and while notes
    // sound its halo breathes with the synth's output level.
    struct HeaderDot
    {
        float lit = -1.0f, glow = 0.0f, breath = 0.0f;
        bool target = false;
    };
    std::array<HeaderDot, 6> dots;

    struct DotClock : IlanaAnim::FrameTimer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } dotClock;

    bool dotTarget (size_t index) const
    {
        switch (index)
        {
            case 0: return ! westShown() || readInt ("west_on") != 0;
            case 1: return stringsTabs.getSelected() != 0 || readInt ("sym_on") != 0;
            case 2: return readInt ("west_on") != 0;
            case 3: return readInt (boardTab.getSelected() == 0 ? "sb_on" : "res_on") != 0;
            case 4: return readInt ("subosc_on") != 0;
            case 5: return readInt ("noise_on") != 0;
            default: return true;
        }
    }

    juce::Rectangle<int> dotArea (size_t index) const
    {
        const std::array<juce::Rectangle<int>, 6> cards { filterBounds, stringsBounds, westBounds, bodyBounds, subBounds, noiseBounds };
        const auto& card = cards[index];
        return juce::Rectangle<int> (28, 28).withCentre ({ card.getX() + 14, card.getY() + headerHeight / 2 });
    }

    // A card's switch moved (from any thread): its dot fades at once, not on the next page tick.
    static constexpr const char* dotSwitchIds[] { "sym_on", "west_on", "sb_on", "res_on", "subosc_on", "noise_on" };
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { wakeDots(); }

    // Starts the dots' clock when a switch moved or notes sound (the 4 Hz page timer asks).
    void wakeDots()
    {
        auto changed = false;
        for (size_t i = 0; i < dots.size(); ++i)
            changed = changed || dots[i].lit < 0.0f || dotTarget (i) != dots[i].target;

        if (changed || (IlanaAnim::fullMotion() && processorRef.getActiveVoiceCount() > 0))
            if (! dotClock.isTimerRunning())
                dotClock.startTimerHz (60);
    }

    void tickDots()
    {
        const auto seconds = dotClock.frameSeconds();
        const auto full = IlanaAnim::fullMotion() && IlanaAnim::showing (*this);
        const auto level = full && processorRef.getActiveVoiceCount() > 0 ? juce::jlimit (0.0f, 1.0f, processorRef.getOutputPeak() * 1.5f) : 0.0f;
        auto busy = level > 0.0f;

        for (size_t i = 0; i < dots.size(); ++i)
        {
            auto& dot = dots[i];
            const auto target = dotTarget (i);
            const auto before = dot;

            if (dot.lit < 0.0f || ! full)
            {
                dot.lit = target ? 1.0f : 0.0f;
                dot.glow = 0.0f;
            }
            else
            {
                if (target && ! dot.target)
                    dot.glow = 1.0f;
                dot.lit += ((target ? 1.0f : 0.0f) - dot.lit) * (1.0f - std::exp (-seconds / 0.05f)); // ~180 ms to settle
                dot.glow *= std::exp (-seconds / 0.18f);
                if (dot.glow < 0.01f)
                    dot.glow = 0.0f;
            }

            dot.target = target;
            const auto breathTarget = target ? level : 0.0f;
            dot.breath += (breathTarget - dot.breath) * (1.0f - std::exp (-seconds / (breathTarget > dot.breath ? 0.03f : 0.25f)));
            if (dot.breath < 0.01f && breathTarget == 0.0f)
                dot.breath = 0.0f;

            const auto moving = std::abs (dot.lit - (target ? 1.0f : 0.0f)) > 0.004f || dot.glow > 0.0f || dot.breath > 0.0f;
            if (! moving)
                dot.lit = target ? 1.0f : 0.0f;
            busy = busy || moving;

            if (std::abs (dot.lit - before.lit) > 0.002f || std::abs (dot.glow - before.glow) > 0.002f || std::abs (dot.breath - before.breath) > 0.004f)
                repaint (dotArea (i));
        }

        if (! busy)
            dotClock.stopTimer();
    }

    void paintCardWithHeader (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title, juce::Colour tint, const HeaderDot& dot) const
    {
        if (card.isEmpty())
            return;

        IlanaTheme::paintCard (g, card.toFloat(), 10.0f, tint);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (card.getX() + 1, card.getY() + headerHeight - 1, card.getWidth() - 2, 1));
        const auto centre = card.getY() + headerHeight / 2;
        const auto lit = dot.lit < 0.0f ? 1.0f : dot.lit;
        const auto dotCentre = juce::Point<float> ((float) card.getX() + 14.0f, (float) centre);
        const auto colour = tint.withMultipliedSaturation (0.6f).withMultipliedBrightness (0.5f).interpolatedWith (tint, lit);
        // (paintTag's dot, its halo bigger and brighter while it glows or breathes.)
        const auto swell = juce::jmax (dot.glow, dot.breath * 0.8f);
        g.setColour (colour.withAlpha (0.2f + 0.3f * swell));
        g.fillEllipse (juce::Rectangle<float> (14.0f + 8.0f * swell, 14.0f + 8.0f * swell).withCentre (dotCentre));
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.35f * dot.glow));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dotCentre));
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

    // A packed menu's width for its name and its longest value (the look-and-feel's text sizes and insets).
    static int packedMenuWidth (const juce::String& prefix, const juce::StringArray& values)
    {
        auto valueW = 0;
        for (const auto& value : values)
            valueW = juce::jmax (valueW, juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaLookAndFeel::packedMenuText), value) + 2);
        const auto prefixW = prefix.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaLookAndFeel::packedNameText, true).withKerningFactor (0.09f), prefix) + 5;
        return 5 + prefixW + valueW + 15;
    }

    // ilana's layout (arranger, 2026-10-29): three columns on two rows. SUB over NOISE; FILTER (F1 | F2 | WEST)
    // over ENVELOPE and LFO; STRINGS · KEYS over BODY · BOARD. Design widths 124 / 600 / 310 of 1034.
    void layoutBands (juce::Rectangle<int> bands)
    {
        const auto total = bands.getWidth() - 2 * hGap;
        const auto leftW = juce::roundToInt ((float) total * 124.0f / 1034.0f);
        const auto rightW = juce::roundToInt ((float) total * 310.0f / 1034.0f);
        const auto middleW = total - leftW - rightW;
        const auto rowH = (bands.getHeight() - vGap) / 2;
        const auto row1 = bands.withHeight (rowH);
        const auto row2 = bands.withTrimmedTop (rowH + vGap).withHeight (rowH);
        const auto x1 = bands.getX() + leftW + hGap, x2 = x1 + middleW + hGap;

        subBounds = row1.withWidth (leftW);
        noiseBounds = row2.withWidth (leftW);
        filterBounds = row1.withX (x1).withWidth (middleW);
        stringsBounds = row1.withX (x2).withWidth (rightW);
        bodyBounds = row2.withX (x2).withWidth (rightW);
        westBounds = {};
        const auto envW = juce::roundToInt ((float) (middleW - hGap) * 324.0f / 595.0f);
        envelopeCard.setBounds (row2.withX (x1).withWidth (envW));
        lfoCard.setBounds (row2.withX (x1 + envW + hGap).withWidth (middleW - envW - hGap));

        layoutFilterCard();
        layoutStringsCard();
        layoutBodyCard();
        layoutSubNoise();
    }

    void layoutFilterCard()
    {
        const auto card = filterBounds;
        const auto centre = card.getY() + headerHeight / 2;
        auto x = card.getX() + 23 + titleWidth ("FILTER") + 4;
        filterTabs.setBounds ({ x, centre - 8, filterTabs.idealWidth(), 16 });
        x += filterTabs.getWidth() + 6;
        const auto headerX = x;
        auto body = card.withTrimmedTop (headerHeight).reduced (0, 3);

        // F1 / F2. Header: TYPE, SERIAL / PARALLEL, BALANCE while parallel, Filter 2's switch.
        for (auto& parts : filters)
            parts.type->setBounds ({ x, centre - 8, 84, 16 });
        x += 84 + 4;
        routing->setBounds ({ x, centre - 8, 104, 16 });
        x += 104 + 6;
        f2Switch.setBounds (switchIn (card));
        balance->setBounds ({ x, centre - 8, juce::jmax (40, f2Switch.getX() - 6 - x), 16 });

        // Body: the graph takes everything left of CUTOFF RESO DRIVE KEY TRACK and a fifth slot (SLOPE on a
        // classic type, MORPH on one that morphs).
        const auto knobsX = card.getRight() - 6 - 5 * filterPitch;
        filterDisplay.setBounds (juce::Rectangle<int> (card.getX() + 6, body.getY() + 1, knobsX - 6 - card.getX() - 6, body.getHeight() - 2));
        // (Room under the dials for KEY TRACK's second line.)
        const auto cellY = body.getY() - 2 + (body.getHeight() + 2 - (int) dialSize - 10 - 13) / 2;
        for (size_t k = 0; k < filters[0].knobs.size(); ++k)
            for (auto& other : filters)
                other.knobs[k]->setBounds (KnobControl::packedBoundsFor ({ knobsX + (int) juce::jmin ((size_t) 4, k) * filterPitch, cellY, filterPitch, (int) dialSize + 10 + 13 }));
        for (auto& other : filters)
            other.slope->setBounds (KnobControl::packedBoundsFor ({ knobsX + 4 * filterPitch, cellY, filterPitch, (int) dialSize + 10 + 13 }));

        // WEST. Header: GATE, PLACE and STRIKE BY as wide as their words, its switch at the right; body: the fold's
        // curve, then seven knobs.
        westOn->setBounds (switchIn (card));
        x = headerX;
        // (+4: at 75 % the text floor makes the words a little wider than at the design size.)
        const auto modeW = packedMenuWidth ({}, { "Combo", "Low Pass", "VCA" }) + 4;
        const auto placeW = packedMenuWidth ({}, { "After Filters", "As Filter 2" }) + 4;
        westMode->setBounds ({ x, centre - 8, modeW, 16 });
        x += modeW + 4;
        westPosition->setBounds ({ x, centre - 8, placeW, 16 });
        x += placeW + 4;
        westSource->setBounds ({ x, centre - 8, juce::jmin (packedMenuWidth ("STRIKE BY", { "Note", "LFO 16 B" }) + 4, westOn->getX() - 6 - x), 16 });
        // (The last ring's ink sits about 8 px inside its cell: the cells reach the card's edge, the picture starts 6 px in.)
        const auto westX = card.getRight() - 6 * westPitch - cell;
        westDisplay->setBounds ({ card.getX() + 6, body.getY() + 1, westX - 6 - card.getX() - 6, body.getHeight() - 2 });
        auto knobX = westX;
        for (auto& knob : westKnobs)
        {
            knob->setBounds (KnobControl::packedBoundsFor ({ knobX, body.getY() + (body.getHeight() - (int) dialSize - 10) / 2, cell, (int) dialSize + 10 }));
            knobX += westPitch;
        }
    }

    // STRINGS and KEYS on one grid: five columns at 43 px, centred in the card, the knobs at one height.
    void layoutStringsCard()
    {
        const auto card = stringsBounds;
        const auto centre = card.getY() + headerHeight / 2;
        stringsTabs.setBounds ({ card.getX() + 22, centre - 8, stringsTabs.idealWidth(), 16 });
        stringsOn->setBounds (switchIn (card));
        const auto gridW = 4 * stringsPitch + cell;
        const auto left = card.getX() + (card.getWidth() - gridW) / 2;
        // (The group name, 10 px, then 4 px, then the knob cell, 38 px: the block centred under the header.)
        const auto blockH = 14 + (int) dialSize + 10;
        const auto top = card.getY() + headerHeight + (card.getHeight() - headerHeight - blockH) / 2;
        const auto column = [left] (int i) { return left + i * stringsPitch; };
        const auto cellY = top + 14;
        const auto place = [cellY] (KnobControl& knob, int x) { knob.setBounds (KnobControl::packedBoundsFor ({ x, cellY, cell, (int) dialSize + 10 })); };
        for (size_t i = 0; i < stringsKnobs.size(); ++i)
            place (*stringsKnobs[i], column ((int) i));
        for (size_t i = 0; i < keysKnobs.size(); ++i)
            place (*keysKnobs[i], column ((int) i));
        const auto groupOver = [&] (int first, int last) { return juce::Rectangle<int> (column (first), top, column (last) + cell - column (first), 10); };
        tuningGroup = groupOver (3, 4);
        noiseGroup = groupOver (2, 4);
        stringsManual->setBounds ({ tuningGroup.getX(), cellY + 1, tuningGroup.getWidth(), 16 });
        tuneButton.setBounds ({ tuningGroup.getX(), cellY + 20, tuningGroup.getWidth(), 16 });
    }

    void layoutBodyCard()
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

        // One grid of five columns at 42 px, centred in the card, two rows; every name inside its menu or under its knob.
        const auto top = card.getY() + headerHeight;
        const auto left = card.getX() + (card.getWidth() - 4 * bodyPitch - cell) / 2;
        const auto column = [left] (int i) { return left + i * bodyPitch; };
        // (Rows from the top and from the bottom: the second row's names end 3 px above the card's edge at any band height.)
        const auto row1 = top + 3, row2 = card.getBottom() - 3 - (int) dialSize - 10;
        const auto place = [] (KnobControl& knob, int knobX, int y) { knob.setBounds (KnobControl::packedBoundsFor ({ knobX, y, cell, (int) dialSize + 10 })); };
        const auto spanTo = [&column] (int last) { return column (last) + cell - column (0); };

        // BODY: TYPE over COUPLING beside AMOUNT and DECAY; then MATERIAL, SIZE, COUPLE, OFFSET, KEY TRK.
        bodyType->setBounds ({ column (0), top + 4, spanTo (2), 16 });
        bodyCoupling->setBounds ({ column (0), top + 23, spanTo (2), 16 });
        place (*bodyKnobs[0], column (3), row1);
        place (*bodyKnobs[1], column (4), row1);
        const std::array<size_t, 5> second { 3, 4, 2, 5, 6 };
        for (size_t i = 0; i < second.size(); ++i)
            place (*bodyKnobs[second[i]], column ((int) i), row2);

        // BOARD: MODEL (as wide as BODY's menus) beside TONE and MIX; SIZE beside the response it slides.
        boardModel->setBounds ({ column (0), row1 + (int) dialSize / 2 - 8, spanTo (2), 16 });
        place (*boardKnobs[1], column (3), row1);
        place (*boardKnobs[2], column (4), row1);
        place (*boardKnobs[0], column (0), row2);
        boardResponse->setBounds (juce::Rectangle<int> (column (1), row2, column (4) + cell - column (1), card.getBottom() - 3 - row2));
    }

    void layoutSubNoise()
    {
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
        if (show == (tuningPopover.isVisible() && ! tuningGrow.isClosing()))
            return;
        if (show)
        {
            tuningPopover.setBounds (tuningBounds());
            tuningPopover.setVisible (true);
            tuningPopover.toFront (false);
            tuningGrow.open (getLocalPoint (&tuneButton, tuneButton.getLocalBounds().getCentre()).toFloat());
        }
        else
        {
            tuningGrow.close ([safe = juce::Component::SafePointer<juce::Component> (&tuningPopover)]
                              {
                                  if (safe != nullptr)
                                      safe->setVisible (false);
                              });
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

    // (UI review 26: FILTER back to mockup v4's width, its graph about 290 px; the knobs at 33 and WEST at 41 give the room.)
    static constexpr int westPitch = 41, stringsPitch = 43, bodyPitch = 42, filterPitch = 33;

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef };
    OscPageViewport oscView;
    OscScrollBar oscBar { true };
    SourceCarouselCard envelopeCard, lfoCard;
    FilterDisplay filterDisplay;
    ViewPills filterTabs { { "F1", "F2", "WEST" }, filterColour (0), "F1 and F2: which filter's type and knobs show (the graph draws both). WEST: the WEST COAST folder and gate" };
    std::array<FilterCard, 2> filters;
    std::unique_ptr<PillChoice> routing;
    std::unique_ptr<BalanceSlider> balance;
    juce::TextButton f2Switch;
    ViewPills stringsTabs { { "STRINGS", "KEYS" }, stringsColour(), "STRINGS: the sympathetic strings. KEYS: the piano's stretch, pedal and key noises." };
    std::unique_ptr<ToggleControl> stringsOn;
    std::unique_ptr<PillChoice> stringsManual;
    NoteButton tuneButton;
    TuningPopover tuningPopover;
    IlanaAnim::PopGrow tuningGrow { tuningPopover }; // (NOTES grows from its button)
    std::vector<std::unique_ptr<KnobControl>> stringsKnobs, keysKnobs, tuningNotes;
    juce::Rectangle<int> filterBounds, stringsBounds, westBounds, bodyBounds, subBounds, noiseBounds, tuningGroup, noiseGroup;
    ViewPills bodyTab { { "BODY" }, bodyColour(), "Show BODY's controls (its switch is beside it)" };
    ViewPills boardTab { { "BOARD" }, bodyColour(), "Show the SOUNDBOARD's controls (its switch is beside it)" };
    std::unique_ptr<ToggleControl> westOn, bodyOn, boardOn, subOn, noiseOn;
    std::unique_ptr<WestDisplay> westDisplay;
    std::unique_ptr<SubWaveDisplay> subWave;
    std::unique_ptr<NoiseColourDisplay> noiseBand;
    std::unique_ptr<BoardResponseDisplay> boardResponse;
    std::unique_ptr<ComboControl> westMode, westPosition, westSource, bodyType, bodyCoupling, boardModel, subShape, noiseType;
    std::unique_ptr<PillChoice> subOctave;
    std::vector<std::unique_ptr<KnobControl>> westKnobs, bodyKnobs, boardKnobs;
    std::unique_ptr<KnobControl> subLevel, subColourKnob, noiseLevel, noiseColour;
};
} // namespace
