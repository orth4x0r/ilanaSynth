#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/TableFactory.h"
#include "IlanaLookAndFeel.h"

// Where the editor keeps its settings (set while an editor exists): the
// browser's favourites live there, as the preset browser's do.
inline juce::PropertiesFile*& tableBrowserSettings()
{
    static juce::PropertiesFile* file = nullptr;
    return file;
}

// Every wavetable at a glance, grouped by category, each drawn from its
// first and middle frames. Click one to load it into the oscillator; the
// star keeps it under FAVOURITES at the top, and HEAR plays the table under
// the pointer (restored when you leave without picking). The search field
// above narrows it to the tables whose name or group match.
class TableBrowser : public juce::Component,
                     private juce::Timer
{
public:
    ~TableBrowser() override
    {
        restoreOriginal();
    }

    TableBrowser (IlanaSynthAudioProcessor& p, juce::String parameterIdIn, juce::Colour colourIn)
        : processorRef (p), parameterId (std::move (parameterIdIn)), colour (colourIn)
    {
        search.setTextToShowWhenEmpty ("Search tables", IlanaTheme::Ui::text3);
        search.setColour (juce::TextEditor::backgroundColourId, IlanaTheme::Ui::well);
        search.setColour (juce::TextEditor::outlineColourId, IlanaTheme::Ui::line);
        search.setColour (juce::TextEditor::focusedOutlineColourId, colour.withAlpha (0.6f));
        search.setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
        search.setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body)));
        search.setIndents (8, 5);
        search.onTextChange = [this] { buildLayout(); };
        search.onReturnKey = [this]
        {
            // Return takes the only match, or the first one.
            if (! items.empty())
                pick (items.front().choice);
        };
        search.onEscapeKey = [this]
        {
            if (! search.isEmpty())
                search.setText ({}, true);
            else if (auto* callOut = findParentComponentOfClass<juce::CallOutBox>())
                callOut->dismiss();
        };
        addAndMakeVisible (search);

        // HEAR: hovering a table plays it (review 9, S9-19).
        hearButton.setButtonText ("HEAR");
        hearButton.setClickingTogglesState (true);
        hearButton.setTooltip ("Hear tables\nOn: resting the pointer on a table plays a note with it, in this oscillator. "
                               "Leaving without picking puts the old table back.");
        hearButton.onClick = [this]
        {
            audition = hearButton.getToggleState();

            if (! audition)
                restoreOriginal();
        };
        addAndMakeVisible (hearButton);

        content.owner = this;
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
        buildLayout();
        setSize (740, 520);
    }

    // Opens the browser in a call-out next to the given component.
    static void show (IlanaSynthAudioProcessor& processor, const juce::String& parameterId, juce::Colour colour,
                      juce::Component& target)
    {
        auto browser = std::make_unique<TableBrowser> (processor, parameterId, colour);
        auto* parent = target.getTopLevelComponent();
        // The call-out is not guaranteed to sit in the editor's tree, so the
        // browser takes the editor's look itself (HEAR and the scroll bar
        // were the stock widgets: V10-12).
        browser->setLookAndFeel (&target.getLookAndFeel());

        // Fill most of the plugin window so the previews are readable at any
        // UI zoom (the call-out is not scaled with the editor's content).
        if (parent != nullptr)
            browser->setSize (juce::jmax (740, juce::roundToInt ((float) parent->getWidth() * 0.86f)),
                              juce::jmax (520, juce::roundToInt ((float) parent->getHeight() * 0.8f)));

        auto& callOut = juce::CallOutBox::launchAsynchronously (std::move (browser),
                                                                parent != nullptr ? parent->getLocalArea (&target, target.getLocalBounds())
                                                                                  : target.getScreenBounds(),
                                                                parent);
        callOut.setLookAndFeel (&target.getLookAndFeel());
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto searchRow = area.removeFromTop (34).reduced (6, 4);
        hearButton.setBounds (searchRow.removeFromRight (70));
        searchRow.removeFromRight (6);
        search.setBounds (searchRow);
        viewport.setBounds (area);
        buildLayout();
    }

    void paint (juce::Graphics& g) override { g.fillAll (IlanaTheme::Ui::panel); }

    // Typing goes straight to the search once the browser is up.
    void parentHierarchyChanged() override
    {
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<TableBrowser> (this)]
        {
            if (safeThis != nullptr && safeThis->isShowing())
                safeThis->search.grabKeyboardFocus();
        });
    }

    // How a factory table's name shows here: spaced Title Case ("DriveSaw"
    // reads "Drive Saw"), and ANALOG's "Basic" reads "Analog Saw" so it isn't
    // taken for the BASIC group. Display only: the stored names (and the
    // patches that keep them) don't change.
    static juce::String displayName (int index, const juce::String& name)
    {
        if (index == 0 && name == "Basic")
            return "Analog Saw";

        juce::String spaced;
        juce::juce_wchar previous = 0;

        for (auto pointer = name.getCharPointer(); ! pointer.isEmpty();)
        {
            const auto c = pointer.getAndAdvance();

            if (juce::CharacterFunctions::isUpperCase (c) && juce::CharacterFunctions::isLowerCase (previous))
                spaced << " ";

            spaced << juce::String::charToString (c);
            previous = c;
        }

        auto words = juce::StringArray::fromTokens (spaced, " ", "");

        for (auto& word : words)
            word = word.substring (0, 1).toUpperCase() + word.substring (1);

        return words.joinIntoString (" ");
    }

    // For the tests: the search, and the tables shown.
    void setSearchText (const juce::String& text) { search.setText (text, true); }

    juce::StringArray getShownNames() const
    {
        juce::StringArray shown;

        for (const auto& item : items)
            shown.add (item.name);

        return shown;
    }

private:
    struct Item
    {
        int choice = 0;
        juce::String name;
        juce::Rectangle<int> bounds;
    };

    struct Heading
    {
        juce::String text;
        juce::Rectangle<int> bounds;
    };

    struct Content : public juce::Component
    {
        TableBrowser* owner = nullptr;

        void paint (juce::Graphics& g) override { owner->paintContent (g); }
        void mouseMove (const juce::MouseEvent& event) override { owner->hoverAt (event.getPosition()); }
        void mouseExit (const juce::MouseEvent&) override { owner->hoverAt ({ -1, -1 }); }
        void mouseUp (const juce::MouseEvent& event) override { owner->pickAt (event.getPosition()); }
    };

    void buildLayout()
    {
        items.clear();
        headings.clear();

        const auto names = TableFactory::getFactoryTableNames();
        const auto categories = TableFactory::getFactoryTableCategories();
        auto order = TableFactory::getCategoryOrder();
        order.add ("User");

        const auto width = juce::jmax (200, viewport.getWidth() - 12);
        const auto words = juce::StringArray::fromTokens (search.getText().trim().toLowerCase(), " ", "");
        const auto matches = [&words] (const juce::String& name, const juce::String& category)
        {
            const auto haystack = (name + " " + category).toLowerCase();

            for (const auto& word : words)
                if (! haystack.contains (word))
                    return false;

            return true;
        };
        // Cells grow with the browser; ~140 px wide at the smallest size.
        scale = juce::jlimit (1.0f, 2.5f, (float) width / 728.0f);
        const auto columns = juce::jlimit (4, 6, width / juce::roundToInt (140.0f * scale));
        const auto cellWidth = width / columns;
        const auto cellHeight = juce::roundToInt ((float) cellWidth * 0.55f);
        const auto headingHeight = juce::roundToInt (22.0f * scale);
        auto y = 6;

        // FAVOURITES first, when the editor keeps settings and any is starred.
        auto groups = order;
        std::vector<Item> favourites;

        if (tableBrowserSettings() != nullptr)
        {
            for (int i = 0; i < names.size(); ++i)
                if (isFavourite (names[i]) && matches (displayName (i, names[i]) + " " + names[i], "favourites"))
                    favourites.push_back ({ i, displayName (i, names[i]), {} });

            if (! favourites.empty())
                groups.insert (0, "Favourites");
        }

        for (const auto& category : groups)
        {
            std::vector<Item> group;

            if (category == "Favourites")
            {
                group = favourites;
            }
            else if (category == "User")
            {
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numUserSlots; ++slot)
                    if (matches ("User " + juce::String (slot + 1), category))
                        group.push_back ({ names.size() + slot, "User " + juce::String (slot + 1), {} });
            }
            else
            {
                for (int i = 0; i < names.size(); ++i)
                    if (categories[i] == category && matches (displayName (i, names[i]) + " " + names[i], category))
                        group.push_back ({ i, displayName (i, names[i]), {} });
            }

            if (group.empty())
                continue;

            headings.push_back ({ category.toUpperCase(), { 8, y, width, headingHeight } });
            y += headingHeight + 2;

            for (size_t i = 0; i < group.size(); ++i)
            {
                const auto column = (int) i % columns;

                if (column == 0 && i > 0)
                    y += cellHeight + 6;

                group[i].bounds = { 4 + column * cellWidth, y, cellWidth - 6, cellHeight };
                items.push_back (group[i]);
            }

            y += cellHeight + juce::roundToInt (14.0f * scale);
        }

        content.setSize (width, juce::jmax (y, viewport.getHeight()));
        content.repaint();
    }

    int current() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (parameterId))
            return (int) value->load();

        return 0;
    }

    void paintContent (juce::Graphics& g)
    {
        g.fillAll (IlanaTheme::Ui::panel);
        const auto selected = current();

        if (items.empty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body * scale));
            g.drawText ("No tables match \"" + search.getText().trim() + "\".", content.getLocalBounds().withHeight (80),
                        juce::Justification::centred);
        }

        for (const auto& heading : headings)
        {
            g.setColour (colour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body * scale, true));
            g.drawText (heading.text, heading.bounds, juce::Justification::centredLeft);
        }

        for (const auto& item : items)
        {
            const auto bounds = item.bounds.toFloat();
            const auto isSelected = item.choice == selected;
            const auto isHovered = item.choice == hovered;

            g.setColour (isSelected ? colour.withAlpha (0.2f) : juce::Colours::white.withAlpha (isHovered ? 0.07f : 0.03f));
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (isSelected ? colour : juce::Colours::white.withAlpha (isHovered ? 0.3f : 0.1f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, isSelected ? 1.5f : 1.0f);

            auto area = bounds.reduced (8.0f * scale, 5.0f * scale);
            const auto label = area.removeFromBottom (17.0f * scale);
            paintWave (g, item.choice, area, isSelected || isHovered);

            g.setColour (isSelected ? colour : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body * scale, isSelected));
            IlanaTheme::drawFitted (g, item.name, label.toNearestInt(), juce::Justification::centred, 1);

            // The favourite star, top right: faint until the pointer is on
            // the cell, solid once starred.
            if (const auto star = starBounds (item); ! star.isEmpty() && item.choice < TableFactory::getFactoryTableNames().size())
            {
                const auto starred = isFavourite (TableFactory::getFactoryTableNames()[item.choice]);
                if (starred || isHovered)
                {
                    juce::Path shape;
                    shape.addStar (star.getCentre().toFloat(), 5, star.getWidth() * 0.22f, star.getWidth() * 0.5f);
                    g.setColour (starred ? juce::Colour (0xffffd447)
                                         : juce::Colours::white.withAlpha (hoveredStar == item.choice ? 0.85f : 0.4f));
                    if (starred)
                        g.fillPath (shape);
                    else
                        g.strokePath (shape, juce::PathStrokeType (1.2f));
                }
            }
        }
    }

    void paintWave (juce::Graphics& g, int choice, juce::Rectangle<float> area, bool bright)
    {
        const auto* table = processorRef.getWavetable (choice);

        if (table == nullptr || table->getNumFrames() == 0)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label * scale));
            g.drawText ("EMPTY", area, juce::Justification::centred);
            return;
        }

        // First frame bright, middle frame faint behind it.
        const int frames[] { table->getNumFrames() / 2, 0 };

        for (int layer = 0; layer < 2; ++layer)
        {
            const auto* data = table->getFrameData (0, frames[layer]);
            juce::Path path;

            for (int i = 0; i <= 96; ++i)
            {
                const auto index = juce::jmin (Wavetable::frameSize - 1, i * Wavetable::frameSize / 96);
                const auto x = area.getX() + area.getWidth() * (float) i / 96.0f;
                const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, data[index]) * area.getHeight() * 0.45f;

                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            g.setColour (layer == 0 ? colour.withAlpha (0.3f) : colour.withAlpha (bright ? 1.0f : 0.75f));
            g.strokePath (path, juce::PathStrokeType ((layer == 0 ? 1.0f : 1.6f) * scale));
        }
    }

    int itemAt (juce::Point<int> position) const
    {
        for (const auto& item : items)
            if (item.bounds.contains (position))
                return item.choice;

        return -1;
    }

    bool isFavourite (const juce::String& name) const
    {
        return tableBrowserSettings() != nullptr && tableBrowserSettings()->getValue ("tablefav_" + name) == "1";
    }

    // The star's hit area, in a cell's top right corner (factory tables).
    juce::Rectangle<int> starBounds (const Item& item) const
    {
        if (tableBrowserSettings() == nullptr || item.choice >= TableFactory::getFactoryTableNames().size())
            return {};

        const auto size = juce::roundToInt (18.0f * scale);
        return juce::Rectangle<int> (size, size).withPosition (item.bounds.getRight() - size - 3, item.bounds.getY() + 3);
    }

    void hoverAt (juce::Point<int> position)
    {
        const auto choice = itemAt (position);
        auto star = -1;

        for (const auto& item : items)
            if (item.choice == choice && starBounds (item).contains (position))
                star = choice;

        if (choice != hovered || star != hoveredStar)
        {
            hovered = choice;
            hoveredStar = star;
            content.repaint();

            // HEAR: a short rest on a table plays it.
            stopTimer();
            if (audition && choice >= 0 && choice != previewed && star < 0)
                startTimer (160);
        }
    }

    void timerCallback() override
    {
        stopTimer();

        if (audition && hovered >= 0 && hovered != previewed)
            preview (hovered);
    }

    // Sets the table for a listen (no undo step, no EDITED beyond the
    // value) and plays a note; the old value is kept to put back.
    void preview (int choice)
    {
        auto* parameter = processorRef.apvts.getParameter (parameterId);

        if (parameter == nullptr)
            return;

        if (! hasOriginal)
        {
            originalChoice = current();
            hasOriginal = true;
        }

        previewed = choice;
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) choice));
        processorRef.triggerPreviewNote (60, true, 0.7f);
        juce::Timer::callAfterDelay (700, [&processor = processorRef] { processor.triggerPreviewNote (60, false); });
        content.repaint();
    }

    // Leaving without picking (or HEAR off) puts the old table back.
    void restoreOriginal()
    {
        if (! hasOriginal)
            return;

        hasOriginal = false;
        previewed = -1;

        if (auto* parameter = processorRef.apvts.getParameter (parameterId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) originalChoice));
    }

    void pickAt (juce::Point<int> position)
    {
        const auto choice = itemAt (position);

        if (choice < 0)
            return;

        // The star only toggles the favourite.
        for (const auto& item : items)
            if (item.choice == choice && starBounds (item).contains (position))
            {
                const auto key = "tablefav_" + TableFactory::getFactoryTableNames()[choice];
                tableBrowserSettings()->setValue (key, isFavourite (TableFactory::getFactoryTableNames()[choice]) ? "0" : "1");
                tableBrowserSettings()->saveIfNeeded();
                buildLayout();
                return;
            }

        pick (choice);
    }

    void pick (int choice)
    {
        // The pick is the edit: the old value goes back first, so the undo
        // step records old to new, not the table already being heard.
        restoreOriginal();

        if (auto* parameter = processorRef.apvts.getParameter (parameterId))
        {
            processorRef.performEdit (parameter->getName (64), [parameter, choice]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) choice));
                parameter->endChangeGesture();
            });
        }

        if (auto* callOut = findParentComponentOfClass<juce::CallOutBox>())
            callOut->dismiss();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String parameterId;
    juce::Colour colour;
    juce::TextEditor search;
    juce::Viewport viewport;
    Content content;
    std::vector<Item> items;
    std::vector<Heading> headings;
    int hovered = -1, hoveredStar = -1, previewed = -1, originalChoice = 0;
    bool audition = false, hasOriginal = false;
    juce::TextButton hearButton;
    float scale = 1.0f;
};
