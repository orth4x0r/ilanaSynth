#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/BuiltInTables.h"
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
// the pointer (restored when you leave without picking). After the User
// tables come the built-in packs (BuiltInTables); picking one copies it into
// a free User slot. The search field
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
        hearButton.setTooltip ("Hear tables\nOn: resting the pointer on a table plays a note with it, in this oscillator.\n"
                               "Leaving without picking puts the old table back.");
        hearButton.onClick = [this]
        {
            audition = hearButton.getToggleState();

            if (! audition)
                restoreOriginal();
        };
        addAndMakeVisible (hearButton);

        // LOAD...: the oscillator's own loader, one step from here (review
        // 11, S11-7); shown when the caller gives one.
        loadButton.setButtonText ("LOAD...");
        loadButton.setTooltip ("Load a wavetable (.wav of single-cycle frames), or turn any recording into a wavetable");
        loadButton.onClick = [this]
        {
            auto load = onLoad;
            if (auto* callOut = findParentComponentOfClass<juce::CallOutBox>())
                callOut->dismiss();
            if (load != nullptr)
                juce::MessageManager::callAsync (load);
        };
        addChildComponent (loadButton);

        // LIBRARY: every table of Documents/ilanaSynth Wavetables in folders
        // (packs, families), one pick loads it into a free User slot.
        libraryButton.setButtonText ("LIBRARY");
        libraryButton.setTooltip ("Your wavetable library (Documents/ilanaSynth Wavetables), in folders: pick one to load it into a free User slot");
        libraryButton.onClick = [this]
        {
            auto pick = onLibraryPick;
            const auto anchorArea = anchor;
            if (auto* callOut = findParentComponentOfClass<juce::CallOutBox>())
                callOut->dismiss();
            if (pick == nullptr)
                return;
            juce::MessageManager::callAsync ([pick, anchorArea]
            {
                auto files = std::make_shared<std::vector<juce::File>>();
                juce::PopupMenu menu;
                buildLibraryMenu (WavetableDoc::getLibraryFolder(), menu, *files);
                if (files->empty())
                    menu.addItem (-1, "The library is empty: run tools/import_surge_wavetables.py or save tables from the editor", false);
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (anchorArea),
                                    [pick, files] (int result)
                {
                    if (result >= libraryItemBase && (size_t) (result - libraryItemBase) < files->size())
                        pick ((*files)[(size_t) (result - libraryItemBase)]);
                });
            });
        };
        addChildComponent (libraryButton);

        content.owner = this;
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
        buildLayout();
        setSize (740, 520);
    }

    // The LOAD... and LIBRARY buttons show when their callbacks are given.
    void setLoaders (std::function<void()> onLoadIn, std::function<void (const juce::File&)> onLibraryPickIn)
    {
        onLoad = std::move (onLoadIn);
        onLibraryPick = std::move (onLibraryPickIn);
        loadButton.setVisible (onLoad != nullptr);
        libraryButton.setVisible (onLibraryPick != nullptr);
        resized();
    }

    // Opens the browser in a call-out next to the given component.
    static void show (IlanaSynthAudioProcessor& processor, const juce::String& parameterId, juce::Colour colour,
                      juce::Component& target, std::function<void()> onLoadIn = {},
                      std::function<void (const juce::File&)> onLibraryPickIn = {})
    {
        auto browser = std::make_unique<TableBrowser> (processor, parameterId, colour);
        browser->setLoaders (std::move (onLoadIn), std::move (onLibraryPickIn));
        browser->anchor = target.getScreenBounds();
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
        // The top bar scales with the tiles (the call-out is not scaled with the editor's content).
        scale = juce::jlimit (1.0f, 2.5f, (float) getWidth() / 728.0f);
        const auto scaled = [this] (int units) { return juce::roundToInt ((float) units * scale); };
        const auto place = [this] (juce::Component& button, juce::Rectangle<int> at)
        {
            button.setTransform (juce::AffineTransform::scale (scale));
            // (The transform scales the position too, so the bounds are given in the scaled units.)
            button.setBounds (juce::roundToInt ((float) at.getX() / scale), juce::roundToInt ((float) at.getY() / scale),
                              juce::roundToInt ((float) at.getWidth() / scale), juce::roundToInt ((float) at.getHeight() / scale));
        };

        auto area = getLocalBounds();
        auto searchRow = area.removeFromTop (scaled (34)).reduced (scaled (6), scaled (4));
        place (hearButton, searchRow.removeFromRight (scaled (70)));
        searchRow.removeFromRight (scaled (6));
        if (loadButton.isVisible())
        {
            place (loadButton, searchRow.removeFromRight (scaled (80)));
            searchRow.removeFromRight (scaled (6));
        }
        if (libraryButton.isVisible())
        {
            place (libraryButton, searchRow.removeFromRight (scaled (80)));
            searchRow.removeFromRight (scaled (6));
        }
        search.setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body * scale)));
        search.setIndents (scaled (8), scaled (5));
        search.setBounds (searchRow);
        // The category chips stay put above the scrolling tiles.
        chipRow = area.removeFromTop (scaled (28)).reduced (scaled (6), scaled (2));
        viewport.setBounds (area);
        buildLayout();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (IlanaTheme::Ui::panel);

        for (const auto& chip : chips)
        {
            const auto box = chip.bounds.toFloat();
            g.setColour (IlanaTheme::Ui::raised);
            g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
            g.setColour (IlanaTheme::Ui::line);
            g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::pillFont().withHeight (IlanaTheme::pillFont().getHeight() * scale));
            g.drawText (chip.text, box, juce::Justification::centred);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        for (const auto& chip : chips)
            if (chip.bounds.contains (event.getPosition()))
                viewport.setViewPosition (0, juce::jmax (0, chip.y - 4));
    }

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

    // For the tests: the search, the tables shown, and picking a built-in table.
    void pickBuiltInTable (int index) { pickBuiltIn (index); }

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
        juce::String chip; // the jump chip's label; empty: none (a built-in pack's folders share its chip)
    };

    struct Chip
    {
        juce::String text;
        juce::Rectangle<int> bounds;
        int y = 0;
    };

    struct Content : public juce::Component,
                     public juce::TooltipClient
    {
        TableBrowser* owner = nullptr;

        // A long table name is cut in its tile: the tip carries all of it.
        juce::String getTooltip() override { return owner != nullptr ? owner->hoveredName() : juce::String(); }

        void paint (juce::Graphics& g) override { owner->paintContent (g); }
        void mouseMove (const juce::MouseEvent& event) override { owner->hoverAt (event.getPosition()); }
        void mouseExit (const juce::MouseEvent&) override { owner->hoverAt ({ -1, -1 }); }
        void mouseUp (const juce::MouseEvent& event) override { owner->pickAt (event.getPosition()); }
    };

    // One heading and its tiles, laid out from y down.
    void addGroup (std::vector<Item>& group, const juce::String& heading, const juce::String& chip, int& y, int width,
                   int columns, int cellWidth, int cellHeight, int headingHeight)
    {
        headings.push_back ({ heading, { 8, y, width, headingHeight }, chip });
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

    void buildLayout()
    {
        items.clear();
        headings.clear();
        lastPack = {};

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
                // Only the slots holding a table (or playing on an oscillator):
                // the other 200+ are empty names.
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numUserSlots; ++slot)
                    if ((processorRef.isUserSlotEdited (slot) || slot < 4) && matches ("User " + juce::String (slot + 1), category))
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

            addGroup (group, category.toUpperCase(), category.toUpperCase(), y, width, columns, cellWidth, cellHeight, headingHeight);
        }

        // The built-in packs, a heading per folder, one chip per pack.
        if (BuiltInTables::count() > 0)
        {
            const auto builtInBase = names.size() + IlanaSynthAudioProcessor::numUserSlots;
            std::vector<Item> group;
            juce::String groupName;

            const auto flush = [&]
            {
                if (group.empty())
                    return;

                const auto pack = groupName.upToFirstOccurrenceOf (" / ", false, false);
                addGroup (group, groupName.toUpperCase(), lastPack == pack ? juce::String() : pack.toUpperCase(),
                          y, width, columns, cellWidth, cellHeight, headingHeight);
                lastPack = pack;
                group.clear();
            };

            for (int k = 0; k < BuiltInTables::count(); ++k)
            {
                if (! matches (BuiltInTables::name (k) + " " + BuiltInTables::group (k), "built-in"))
                    continue;

                if (BuiltInTables::group (k) != groupName)
                {
                    flush();
                    groupName = BuiltInTables::group (k);
                }

                group.push_back ({ builtInBase + k, BuiltInTables::name (k), {} });
            }

            flush();
        }

        content.setSize (width, juce::jmax (y, viewport.getHeight()));
        content.repaint();

        // A chip per heading shown, jumping to it.
        chips.clear();
        auto chipX = chipRow.getX();
        for (const auto& heading : headings)
        {
            if (heading.chip.isEmpty())
                continue;

            const auto chipWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::pillFont().withHeight (IlanaTheme::pillFont().getHeight() * scale)), heading.chip) + juce::roundToInt (20.0f * scale);
            if (chipX + chipWidth > chipRow.getRight())
                break;
            chips.push_back ({ heading.chip, { chipX, chipRow.getY(), chipWidth, chipRow.getHeight() }, heading.bounds.getY() });
            chipX += chipWidth + juce::roundToInt (5.0f * scale);
        }
        repaint (chipRow.expanded (4));
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

        const auto visible = g.getClipBounds();

        for (const auto& item : items)
        {
            if (! item.bounds.expanded (2).intersects (visible))
                continue;

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

    int builtInBase() const { return TableFactory::getFactoryTableNames().size() + IlanaSynthAudioProcessor::numUserSlots; }

    void paintBuiltInWave (juce::Graphics& g, int index, juce::Rectangle<float> area, bool bright)
    {
        const auto& preview = BuiltInTables::preview (index);
        const std::vector<float>* layers[] { &preview.middle, &preview.first };

        for (int layer = 0; layer < 2; ++layer)
        {
            const auto& points = *layers[layer];
            if (points.size() < 2)
                continue;

            juce::Path path;
            for (size_t i = 0; i < points.size(); ++i)
            {
                const auto x = area.getX() + area.getWidth() * (float) i / (float) (points.size() - 1);
                const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, points[i]) * area.getHeight() * 0.45f;
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            g.setColour (layer == 0 ? colour.withAlpha (0.3f) : colour.withAlpha (bright ? 1.0f : 0.75f));
            g.strokePath (path, juce::PathStrokeType ((layer == 0 ? 1.0f : 1.6f) * scale));
        }
    }

    void paintWave (juce::Graphics& g, int choice, juce::Rectangle<float> area, bool bright)
    {
        if (choice >= builtInBase())
        {
            paintBuiltInWave (g, choice - builtInBase(), area, bright);
            return;
        }

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

    juce::String hoveredName() const
    {
        for (const auto& item : items)
            if (item.choice == hovered && hovered >= 0)
                return item.name;

        return {};
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
        auto value = choice;

        // A built-in table is heard from a User slot, given back on leaving.
        if (choice >= builtInBase())
        {
            const auto slot = placeBuiltIn (choice - builtInBase(), true);
            if (slot < 0)
                return;
            value = TableFactory::getNumFactoryTables() + slot;
        }

        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
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

        if (previewSlot >= 0)
        {
            processorRef.clearUserTable (previewSlot);
            previewSlot = -1;
        }
    }

    // Copies built-in table index into a User slot and returns it (-1: none
    // free). A slot already holding that table is reused. For a listen the
    // slot is remembered in previewSlot, so leaving without a pick frees it.
    int placeBuiltIn (int index, bool forListen)
    {
        WavetableDoc doc;
        if (! BuiltInTables::load (index, doc))
            return -1;

        for (int slot = 0; slot < IlanaSynthAudioProcessor::numUserSlots; ++slot)
            if (slot != previewSlot)
                if (const auto held = processorRef.getUserTableDocPtr (slot);
                    held != nullptr && held->name == doc.name && held->frames == doc.frames)
                {
                    if (previewSlot >= 0)
                    {
                        processorRef.clearUserTable (previewSlot);
                        previewSlot = -1;
                    }
                    return slot;
                }

        auto slot = forListen && previewSlot >= 0 ? previewSlot : processorRef.findFreeUserSlot();

        if (slot < 0 || ! processorRef.setUserTable (slot, doc))
            return -1;

        if (forListen)
            previewSlot = slot;

        return slot;
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

        if (choice >= builtInBase())
        {
            pickBuiltIn (choice - builtInBase());
            return;
        }

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

    // The table and the choice in one undo step, as LOAD... does.
    void pickBuiltIn (int index)
    {
        processorRef.beginEdit ("Load wavetable");
        const auto slot = placeBuiltIn (index, false);

        if (slot >= 0)
        {
            if (auto* parameter = processorRef.apvts.getParameter (parameterId))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (TableFactory::getNumFactoryTables() + slot)));
        }

        processorRef.endEdit();

        if (slot < 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Wavetable",
                                                    "All " + juce::String (IlanaSynthAudioProcessor::numUserSlots) + " patch tables are in use. Pick a User table to replace it.");

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
    // The library as nested menus, folders first; every file gets the next id
    // from libraryItemBase, in the order of files.
    static constexpr int libraryItemBase = 1000;

    static void buildLibraryMenu (const juce::File& folder, juce::PopupMenu& menu, std::vector<juce::File>& files)
    {
        juce::Array<juce::File> folders (folder.findChildFiles (juce::File::findDirectories, false));
        folders.sort();
        for (const auto& sub : folders)
        {
            // The synth carries these itself now (the built-in packs).
            if (sub.getFileName() == "Surge Packs" && BuiltInTables::count() > 0)
                continue;

            juce::PopupMenu inner;
            const auto before = files.size();
            buildLibraryMenu (sub, inner, files);
            if (files.size() > before)
                menu.addSubMenu (sub.getFileName(), inner);
        }

        juce::Array<juce::File> tables (folder.findChildFiles (juce::File::findFiles, false, "*.wav"));
        tables.sort();
        for (const auto& file : tables)
        {
            menu.addItem (libraryItemBase + (int) files.size(), file.getFileNameWithoutExtension());
            files.push_back (file);
        }
    }

    juce::TextButton hearButton, loadButton, libraryButton;
    std::function<void()> onLoad;
    std::function<void (const juce::File&)> onLibraryPick;
    juce::Rectangle<int> anchor;
    std::vector<Chip> chips;
    juce::Rectangle<int> chipRow;
    float scale = 1.0f;
    juce::String lastPack;
    int previewSlot = -1; // a User slot holding a built-in table only for a HEAR listen
};
