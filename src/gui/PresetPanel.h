#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

// The preset browser. It drops down under the preset name: categories (with
// counts) on the left, search and the list on the right.
//   click        load the preset and keep browsing
//   double-click load it and close
//   up / down    step through the list, loading as you go
//   Enter        keep the selected preset and close
//   Esc, or a click anywhere outside, closes
// Clicking the star on a row makes it a favourite. The list is sorted by
// name (SORT: by category, or favourites first). The DX7 ROM voices stay out
// of All unless "Show DX7 voices" is ticked; under DX7, chips pick a bank
// (ROM1A, ROM1B..., Dexed's and imported ones). Chips also filter by the tags
// saved with user presets, when there are any (factory presets have none).
// DOCK keeps it open at the side of the window instead (the window grows by
// the panel's width, as Serum 2's browser does): categories above, rows of
// two lines (name and tag, then the macro names), and loading never closes
// it. FLOAT returns it to the drop-down, the cross closes it.
class PresetPanel : public juce::Component,
                    private juce::ListBoxModel,
                    private juce::KeyListener,
                    private IlanaAnim::FrameTimer
{
public:
    explicit PresetPanel (IlanaSynthAudioProcessor& processor, juce::PropertiesFile* settingsIn)
        : processorRef (processor),
          settings (settingsIn)
    {
        setWantsKeyboardFocus (true);

        search.setTextToShowWhenEmpty ("Search names, categories and tags", juce::Colours::white.withAlpha (0.35f));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
        search.setColour (juce::TextEditor::highlightColourId, IlanaTheme::accent().withAlpha (0.35f));
        search.setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::title)));
        search.setIndents (26, 5);
        search.onTextChange = [this] { rebuild(); };
        search.onReturnKey = [this] { loadRow (juce::jmax (0, list.getSelectedRow()), true); };
        search.onEscapeKey = [this] { close(); };
        search.addKeyListener (this);
        addAndMakeVisible (search);

        sidebar.onSelect = [this] (const juce::String& key)
        {
            filterKey = key;
            rebuild();
        };
        addAndMakeVisible (sidebar);

        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        list.setRowHeight (26);
        list.setModel (this);
        list.addKeyListener (this);
        addAndMakeVisible (list);

        saveAsButton.onClick = [this]
        {
            if (onSaveAs != nullptr)
                onSaveAs();
        };
        deleteButton.onClick = [this] { deleteSelected(); };
        folderButton.onClick = [this] { processorRef.getUserPresetDirectory().revealToUser(); };
        surpriseButton.onClick = [this] { loadRandom(); };
        surpriseButton.setTooltip ("Load a random preset from the list shown");

        for (auto* button : { &surpriseButton, &saveAsButton, &deleteButton, &folderButton })
            addAndMakeVisible (button);

        // SAVE AS is the main action; SURPRISE ME is an ordinary button.
        saveAsButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.35f));

        sortMode = settings != nullptr ? juce::jlimit (0, 2, settings->getIntValue ("presetSort", 0)) : 0;
        sortButton.setTooltip ("Sort the list by name, by category, or with favourites first");
        sortButton.onClick = [this] { showSortMenu(); };
        addAndMakeVisible (sortButton);
        updateSortButton();

        showDx7 = settings != nullptr && settings->getBoolValue ("presetShowDx7", false);
        dx7Toggle.setButtonText ("Show DX7 voices");
        dx7Toggle.setTooltip ("List the 288 DX7 ROM voices under All too (they are always under DX7)");
        dx7Toggle.setToggleState (showDx7, juce::dontSendNotification);
        dx7Toggle.onClick = [this] { setShowDx7 (dx7Toggle.getToggleState()); };
        addAndMakeVisible (dx7Toggle);

        chips.onClick = [this] (const juce::String& key) { chipClicked (key); };
        addChildComponent (chips);

        dockButton.setTooltip ("Keep the browser open at the side of the window");
        dockButton.onClick = [this]
        {
            if (onDockRequest != nullptr)
                onDockRequest (! docked);
        };
        addAndMakeVisible (dockButton);

        closeDockButton.setTooltip ("Close the browser");
        closeDockButton.onClick = [this]
        {
            if (onDockedClose != nullptr)
                onDockedClose();
        };
        addChildComponent (closeDockButton);
    }

    ~PresetPanel() override
    {
        stopWatchingClicks();

        if (auto* parent = scrim.getParentComponent())
            parent->removeChildComponent (&scrim);
    }

    // The area dimmed behind the open browser (in the parent's coordinates),
    // so it reads as on top; a click there closes it.
    void setScrimArea (juce::Rectangle<int> area) { scrimArea = area; }

    // Called with the preset index, and whether the browser should close.
    std::function<void (int, bool)> onLoad;
    std::function<void()> onFavouriteChanged;
    // SAVE AS pressed (the editor opens its save panel).
    std::function<void()> onSaveAs;
    // DOCK / FLOAT pressed (true: dock at the side), and the docked cross.
    std::function<void (bool)> onDockRequest;
    std::function<void()> onDockedClose;

    // Docked: a side panel that stays open (no scrim, outside clicks and Esc
    // leave it alone). Undocked: the drop-down.
    void setDocked (bool shouldDock)
    {
        if (docked == shouldDock)
            return;

        docked = shouldDock;
        dockButton.setButtonText (docked ? "FLOAT" : "DOCK");
        dockButton.setTooltip (docked ? "Back to the drop-down under the preset name"
                                      : "Keep the browser open at the side of the window");
        closeDockButton.setVisible (docked);
        list.setRowHeight (docked ? 40 : 26);

        if (docked)
        {
            stopWatchingClicks();
            scrim.setVisible (false);
            closing = false;
            appear = 1.0f;
            setAlpha (1.0f);
            setTransform ({});
        }

        resized();
        repaint();
    }

    bool isDocked() const { return docked; }

    // The preset name box: clicks on it are left to it (it toggles us).
    void setAnchor (juce::Component* anchorComponent) { anchor = anchorComponent; }

    // Back to the preset that is loaded (a load the user cancelled).
    void selectLoadedPreset() { showCurrentPreset(); list.repaint(); }

    void refresh()
    {
        names = processorRef.getAllPresetNames();
        macroTextCache.clear();
        categories = processorRef.getAllPresetCategories();
        tags = processorRef.getAllPresetTags();
        banks.clear();

        // Factory DX7 voices are "NAME (ROM1A)"; imported banks sit in
        // DX7/<bank>/ in the user folder.
        const auto userFiles = processorRef.getUserPresetFiles();

        for (int i = 0; i < names.size(); ++i)
        {
            juce::String bank;

            if (categories[i] == "DX7")
            {
                if (! isUserPreset (i))
                    bank = names[i].fromLastOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false);
                else if (const auto file = userFiles[i - factoryCount];
                         file.getParentDirectory().getParentDirectory().getFileName() == "DX7")
                    bank = file.getParentDirectory().getFileName();
                else
                    bank = "User";
            }

            banks.add (bank);
        }

        rebuildSidebar();
        rebuild();
    }

    void open()
    {
        refresh();

        if (docked)
        {
            closing = false;
            appear = 1.0f;
            setAlpha (1.0f);
            setTransform ({});
            setVisible (true);
            showCurrentPreset();
            startTimerHz (20);
            return;
        }

        if (! isVisible())
        {
            appear = 0.0f;
            setAlpha (0.0f);

            if (auto* parent = getParentComponent(); parent != nullptr && ! scrimArea.isEmpty())
            {
                if (scrim.getParentComponent() != parent)
                {
                    parent->addChildComponent (scrim);
                    scrim.onClick = [this] { close(); };
                }

                scrim.setBounds (scrimArea);
                scrim.setLevel (0.0f);
                scrim.setVisible (true);
                scrim.toFront (false);
            }

            setVisible (true);
            toFront (false);
        }

        closing = false;
        watchClicks();
        showCurrentPreset();
        search.grabKeyboardFocus();
        startTimerHz (60);
    }

    void close()
    {
        if (! isVisible() || docked)
            return;

        closing = true;
        stopWatchingClicks();
        startTimerHz (60);
    }

    bool isOpen() const { return isVisible() && ! closing; }

    // Sort orders (kept in the settings).
    enum SortMode { sortByName = 0, sortByCategory, sortFavouritesFirst };

    void setSortMode (int mode)
    {
        sortMode = juce::jlimit (0, 2, mode);

        if (settings != nullptr)
        {
            settings->setValue ("presetSort", sortMode);
            settings->saveIfNeeded();
        }

        updateSortButton();
        rebuild();
    }

    int getSortMode() const { return sortMode; }

    // Whether All lists the DX7 ROM voices (kept in the settings).
    void setShowDx7 (bool shouldShow)
    {
        showDx7 = shouldShow;
        dx7Toggle.setToggleState (showDx7, juce::dontSendNotification);

        if (settings != nullptr)
        {
            settings->setValue ("presetShowDx7", showDx7);
            settings->saveIfNeeded();
        }

        rebuildSidebar();
        rebuild();
    }

    bool isShowingDx7() const { return showDx7; }

    // For the tests: the sidebar's choice ("" is All, a category's name,
    // or "*fav" / "*user"), the search, the names listed, the chips, and a
    // click on a row at x (the star is the first 30 px).
    void selectFilter (const juce::String& key)
    {
        filterKey = key;
        sidebar.selected = key;
        sidebar.repaint();
        rebuild();
    }

    void setSearchText (const juce::String& text) { search.setText (text, true); }

    juce::StringArray getListedNames() const
    {
        juce::StringArray shown;

        for (auto index : filtered)
            shown.add (names[index]);

        return shown;
    }

    juce::StringArray getChipKeys() const
    {
        juce::StringArray keys;

        for (const auto& chip : chips.entries)
            keys.add (chip.key);

        return keys;
    }

    void clickChip (const juce::String& key) { chipClicked (key); }

    void clickRow (int row, int x)
    {
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), { (float) x, 10.0f },
                                      {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &list, &list, juce::Time::getCurrentTime(),
                                      { (float) x, 10.0f }, juce::Time::getCurrentTime(), 1, false);
        listBoxItemClicked (row, event);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // A deep shadow so the browser floats over the page.
        for (int i = 0; i < 4 && ! docked; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f + (float) i * 2.0f).reduced ((float) (3 - i)), 9.0f);
        }

        IlanaTheme::paintCard (g, bounds, 9.0f, IlanaTheme::accent().withAlpha (docked ? 0.25f : 0.45f));

        auto header = getLocalBounds().reduced (14, 0).removeFromTop (34);
        g.setColour (IlanaTheme::accent());
        g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("PRESETS", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        const auto count = juce::String (filtered.size()) + " of " + juce::String (names.size());

        if (docked)
        {
            g.drawText (count, header.withTrimmedLeft (84).withTrimmedRight (dockButton.getWidth() + closeDockButton.getWidth() + 10),
                        juce::Justification::centredLeft);
        }
        else
        {
            g.drawText (juce::String (juce::CharPointer_UTF8 ("Up/Down browse  \xc2\xb7  Enter keep  \xc2\xb7  Esc close")),
                        header.withTrimmedRight (dockButton.getWidth() + 40), juce::Justification::centred);
            g.drawText (count, header.withTrimmedRight (dockButton.getWidth() + 8), juce::Justification::centredRight);
        }

        IlanaTheme::paintRecessedPanel (g, search.getBounds().toFloat().expanded (1.0f), 6.0f);

        // Magnifier in the search box.
        {
            const auto box = search.getBounds().toFloat();
            const auto centre = juce::Point<float> (box.getX() + 13.0f, box.getCentreY() - 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (centre), 1.5f);
            g.drawLine (centre.x + 3.2f, centre.y + 3.2f, centre.x + 6.5f, centre.y + 6.5f, 1.6f);
        }

        IlanaTheme::paintRecessedPanel (g, sidebar.getBounds().toFloat().expanded (1.0f), 6.0f);
        IlanaTheme::paintRecessedPanel (g, list.getBounds().toFloat().expanded (1.0f), 6.0f);

        if (filtered.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawFittedText (filterKey == favouritesKey && search.isEmpty()
                                  ? juce::String ("No favourites yet. Click the star on a preset to add it.")
                                  : (hiddenDx7Matches > 0 ? "No presets match. " + juce::String (hiddenDx7Matches)
                                                                + " DX7 voices do: tick Show DX7 voices, or pick DX7."
                                                          : juce::String ("No presets match.")),
                              list.getBounds().reduced (16, 0), juce::Justification::centred, 2);
        }
    }

    void lookAndFeelChanged() override
    {
        search.setColour (juce::TextEditor::highlightColourId, IlanaTheme::accent().withAlpha (0.35f));
        saveAsButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.35f));
        repaint();
    }

    void resized() override
    {
        {
            auto header = getLocalBounds().reduced (12, 0).removeFromTop (34).withSizeKeepingCentre (getWidth() - 24, 20);

            if (docked)
            {
                closeDockButton.setBounds (header.removeFromRight (22));
                header.removeFromRight (4);
            }

            dockButton.setBounds (header.removeFromRight (56));
        }

        if (docked)
        {
            layoutDocked();
            return;
        }

        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (24);

        auto footer = area.removeFromBottom (28);
        area.removeFromBottom (8);

        auto left = area.removeFromLeft (150);
        area.removeFromLeft (10);
        dx7Toggle.setBounds (left.removeFromBottom (22));
        left.removeFromBottom (6);
        sidebar.columns = 1;
        sidebar.setBounds (left.reduced (1));

        auto searchRow = area.removeFromTop (30);
        sortButton.setBounds (searchRow.removeFromRight (128).reduced (1, 2));
        searchRow.removeFromRight (8);
        search.setBounds (searchRow.reduced (1));
        area.removeFromTop (8);
        layoutChips (area);
        list.setBounds (area.reduced (1));

        surpriseButton.setBounds (footer.removeFromLeft (150).reduced (1, 0));
        footer.removeFromLeft (10);
        const auto buttonWidth = footer.getWidth() / 3;
        saveAsButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        deleteButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        folderButton.setBounds (footer.reduced (2, 0));
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (docked)
                unfocusAllComponents();
            else
                close();
            return true;
        }

        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            stepSelection (key == juce::KeyPress::downKey ? 1 : -1);
            return true;
        }

        return false;
    }

    // Loads the next (1) or previous (-1) preset of the list shown, leaving
    // the browser open. Also what the arrow keys do wherever focus is.
    void stepSelection (int delta)
    {
        if (filtered.isEmpty())
            return;

        const auto current = list.getSelectedRow();
        const auto next = current < 0 ? (delta > 0 ? 0 : filtered.size() - 1)
                                      : juce::jlimit (0, filtered.size() - 1, current + delta);
        list.selectRow (next);
    }

    // Clicks anywhere in the window outside the browser close it.
    void mouseDown (const juce::MouseEvent& event) override
    {
        auto* clicked = event.originalComponent;

        if (clicked == this || isParentOf (clicked))
            return;

        if (anchor != nullptr && (clicked == anchor.getComponent() || anchor->isParentOf (clicked)))
            return;

        close();
    }

private:
    void layoutDocked()
    {
        auto area = getLocalBounds().reduced (10);
        area.removeFromTop (26);

        search.setBounds (area.removeFromTop (30).reduced (1));
        area.removeFromTop (6);

        {
            auto row = area.removeFromTop (24);
            sortButton.setBounds (row.removeFromRight (128).reduced (1, 1));
            dx7Toggle.setBounds (row);
        }
        area.removeFromTop (6);

        sidebar.columns = 2;
        sidebar.setBounds (area.removeFromTop (juce::jmin (sidebar.getPreferredHeight(), area.getHeight() / 3)).reduced (1));
        area.removeFromTop (8);

        auto footer = area.removeFromBottom (26);
        area.removeFromBottom (8);
        layoutChips (area);
        list.setBounds (area.reduced (1));

        const auto buttonWidth = footer.getWidth() / 4;
        surpriseButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        saveAsButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        deleteButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        folderButton.setBounds (footer.reduced (2, 0));
    }

    bool docked = false;
    juce::TextButton dockButton { "DOCK" };
    juce::TextButton closeDockButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };

    static constexpr const char* favouritesKey = "*fav";
    static constexpr const char* userKey = "*user";

    // The category column.
    class Sidebar : public juce::Component
    {
    public:
        struct Entry
        {
            juce::String key, label;
            int count = 0;
            juce::Colour colour;
            bool gapBefore = false;
        };

        std::vector<Entry> entries;
        juce::String selected;
        std::function<void (const juce::String&)> onSelect;

        void paint (juce::Graphics& g) override
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                const auto& entry = entries[i];
                const auto row = rowBounds ((int) i).toFloat();
                const auto isSelected = entry.key == selected;

                if (isSelected)
                {
                    g.setColour (IlanaTheme::accent().withAlpha (0.22f));
                    g.fillRoundedRectangle (row, 5.0f);
                    g.setColour (IlanaTheme::accent());
                    g.fillRoundedRectangle (row.withWidth (3.0f).reduced (0.0f, 4.0f), 1.5f);
                }
                else if ((int) i == hovered)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.06f));
                    g.fillRoundedRectangle (row, 5.0f);
                }

                auto text = row.reduced (10.0f, 0.0f);

                if (entry.colour != juce::Colour())
                {
                    g.setColour (entry.colour.withAlpha (isSelected ? 1.0f : 0.75f));
                    g.fillEllipse (text.getX(), text.getCentreY() - 3.0f, 6.0f, 6.0f);
                }

                text.removeFromLeft (14.0f);
                g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.95f : 0.72f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, isSelected));
                g.drawText (entry.label, text, juce::Justification::centredLeft);

                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText (juce::String (entry.count), text, juce::Justification::centredRight);
            }
        }

        void mouseMove (const juce::MouseEvent& event) override { mouseX = event.x; setHovered (rowAt (event.y)); }
        void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            mouseX = event.x;
            const auto row = rowAt (event.y);

            if (juce::isPositiveAndBelow (row, (int) entries.size()))
            {
                selected = entries[(size_t) row].key;
                repaint();

                if (onSelect != nullptr)
                    onSelect (selected);
            }
        }

        // Docked, the categories flow into two columns above the list.
        int columns = 1;

        int getPreferredHeight() const
        {
            return columns > 1 ? 8 + ((int) entries.size() + columns - 1) / columns * rowHeight
                               : rowBounds ((int) entries.size() - 1).getBottom() + 6;
        }

    private:
        static constexpr int rowHeight = 24;
        static constexpr int gap = 9;

        juce::Rectangle<int> rowBounds (int index) const
        {
            if (columns > 1)
            {
                const auto columnWidth = (getWidth() - 8) / columns;
                return { 4 + (index % columns) * columnWidth, 4 + (index / columns) * rowHeight, columnWidth - 2, rowHeight - 2 };
            }

            auto y = 4;

            for (int i = 0; i <= index && i < (int) entries.size(); ++i)
            {
                if (entries[(size_t) i].gapBefore)
                    y += gap;

                if (i < index)
                    y += rowHeight;
            }

            return { 4, y, getWidth() - 8, rowHeight - 2 };
        }

        int rowAt (int y) const
        {
            for (int i = 0; i < (int) entries.size(); ++i)
                if (rowBounds (i).expanded (0, 1).contains (columns > 1 ? rowBounds (i).getCentreX() : getWidth() / 2, y)
                    && (columns == 1 || std::abs (rowBounds (i).getCentreX() - mouseX) <= rowBounds (i).getWidth() / 2 + 1))
                    return i;

            return -1;
        }

        int mouseX = 0;

        void setHovered (int row)
        {
            if (row != hovered)
            {
                hovered = row;
                repaint();
            }
        }

        int hovered = -1;
    };

    // Filter chips: DX7 banks, or user preset tags. Click one to pick it.
    class ChipRow : public juce::Component
    {
    public:
        struct Entry
        {
            juce::String key, label;
            bool selected = false;
        };

        std::vector<Entry> entries;
        std::function<void (const juce::String&)> onClick;

        int getPreferredHeight (int width) const
        {
            const auto boxes = layout (width);
            return boxes.empty() ? 0 : boxes.back().getBottom();
        }

        void paint (juce::Graphics& g) override
        {
            const auto boxes = layout (getWidth());
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

            for (size_t i = 0; i < entries.size(); ++i)
            {
                const auto box = boxes[i].toFloat();
                const auto& entry = entries[i];
                g.setColour (entry.selected ? IlanaTheme::accent().withAlpha (0.3f)
                                            : juce::Colours::white.withAlpha ((int) i == hovered ? 0.1f : 0.05f));
                g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
                g.setColour (entry.selected ? IlanaTheme::accent() : IlanaTheme::Ui::line.brighter (0.3f));
                g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);
                g.setColour (entry.selected ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
                g.drawText (entry.label, boxes[i], juce::Justification::centred);
            }
        }

        void mouseMove (const juce::MouseEvent& event) override { setHovered (chipAt (event.getPosition())); }
        void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            const auto chip = chipAt (event.getPosition());

            if (chip >= 0 && onClick != nullptr)
                onClick (entries[(size_t) chip].key);
        }

    private:
        static constexpr int chipHeight = 20, gap = 5;

        std::vector<juce::Rectangle<int>> layout (int width) const
        {
            std::vector<juce::Rectangle<int>> boxes;
            const juce::Font font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            auto x = 0, y = 0;

            for (const auto& entry : entries)
            {
                const auto w = juce::jmin (width, juce::GlyphArrangement::getStringWidthInt (font, entry.label) + 18);

                if (x > 0 && x + w > width)
                {
                    x = 0;
                    y += chipHeight + gap;
                }

                boxes.push_back ({ x, y, w, chipHeight });
                x += w + gap;
            }

            return boxes;
        }

        int chipAt (juce::Point<int> position) const
        {
            const auto boxes = layout (getWidth());

            for (size_t i = 0; i < boxes.size(); ++i)
                if (boxes[i].contains (position))
                    return (int) i;

            return -1;
        }

        void setHovered (int chip)
        {
            if (chip != hovered)
            {
                hovered = chip;
                repaint();
            }
        }

        int hovered = -1;
    };

    static juce::Colour categoryColour (const juce::String& category)
    {
        if (category == "Bass") return juce::Colour (0xffff6b4a);
        if (category == "Lead") return juce::Colour (0xff5b8cff);
        if (category == "Pluck") return juce::Colour (0xffffd447);
        if (category == "Pad") return juce::Colour (0xffb28aff);
        if (category == "Drone") return juce::Colour (0xff6fe3c1);
        if (category == "FX") return juce::Colour (0xffe3a56f);
        if (category == "Keys") return juce::Colour (0xff8fd14f);
        if (category == "Chords") return juce::Colour (0xff4fd1a5);
        if (category == "Arp") return juce::Colour (0xffff7ac6);
        if (category == "Drums") return juce::Colour (0xffc9ced6);
        if (category == "Generative") return juce::Colour (0xff4fc3ff);
        if (category == "DX7") return juce::Colour (0xffd9765f); // the DX7's membrane-panel red
        return IlanaTheme::Ui::text2;
    }

    static bool isFavouriteName (juce::PropertiesFile* settingsFile, const juce::String& name)
    {
        return settingsFile != nullptr && settingsFile->getValue ("fav_" + name, "0") == "1";
    }

    static void drawStar (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool filled)
    {
        juce::Path star;

        for (int i = 0; i < 10; ++i)
        {
            const auto angle = juce::MathConstants<float>::pi * (float) i / 5.0f - juce::MathConstants<float>::halfPi;
            const auto r = i % 2 == 0 ? radius : radius * 0.45f;
            const juce::Point<float> point (centre.x + std::cos (angle) * r, centre.y + std::sin (angle) * r);

            if (i == 0)
                star.startNewSubPath (point);
            else
                star.lineTo (point);
        }

        star.closeSubPath();
        g.setColour (colour);

        if (filled)
            g.fillPath (star);
        else
            g.strokePath (star, juce::PathStrokeType (1.1f));
    }

    void rebuildSidebar()
    {
        static const juce::StringArray order { "Bass", "Lead", "Pluck", "Pad", "Keys", "Chords", "Arp",
                                               "Drone", "Drums", "FX", "FX Input", "Generative", "DX7", "Other" };
        juce::StringArray present;

        // "User" has its own entry; the Init patch lives under All.
        for (const auto& category : categories)
            if (category.isNotEmpty() && category != "User" && category != "Init" && ! present.contains (category))
                present.add (category);

        juce::StringArray shownCategories;

        for (const auto& category : order)
            if (present.contains (category))
                shownCategories.add (category);

        present.sort (true);

        for (const auto& category : present)
            if (! shownCategories.contains (category))
                shownCategories.add (category);

        auto favourites = 0, users = 0, all = 0;

        for (int i = 0; i < names.size(); ++i)
        {
            favourites += isFavouriteName (settings, names[i]) ? 1 : 0;
            users += isUserPreset (i) ? 1 : 0;
            all += showDx7 || categories[i] != "DX7" ? 1 : 0;
        }

        std::vector<Sidebar::Entry> entries;
        entries.push_back ({ "", "All", all, IlanaTheme::accent(), false });
        entries.push_back ({ favouritesKey, "Favourites", favourites, juce::Colour (0xffffd447), false });
        entries.push_back ({ userKey, "User", users, juce::Colours::white.withAlpha (0.6f), false });

        auto first = true;

        for (const auto& category : shownCategories)
        {
            entries.push_back ({ category, category, countOf (category),
                                 categoryColour (category), first });
            first = false;
        }

        sidebar.entries = std::move (entries);
        sidebar.selected = filterKey;
        sidebar.repaint();
    }

    int countOf (const juce::String& category) const
    {
        auto count = 0;

        for (const auto& item : categories)
            count += item == category ? 1 : 0;

        return count;
    }

    // "TONE  ·  MORPH  ·  DRIVE": the preset's macro names. The loaded one
    // reads them from the patch (so an automatic mapping shows too); the
    // others from the factory table. Cached until the list is rebuilt.
    juce::String macroNamesFor (int presetIndex, bool loaded)
    {
        if (loaded)
        {
            juce::StringArray live;

            for (int macro = 0; macro < 4; ++macro)
            {
                const auto name = processorRef.apvts.state.getProperty ("macroName" + juce::String (macro + 1)).toString();

                if (name.isNotEmpty())
                    live.add (name);
            }

            return live.joinIntoString (juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))).toUpperCase();
        }

        if (isUserPreset (presetIndex))
            return {};

        if (const auto found = macroTextCache.find (presetIndex); found != macroTextCache.end())
            return found->second;

        auto list = processorRef.getFactoryMacroNames (presetIndex);
        list.removeEmptyStrings();
        const auto text = list.joinIntoString (juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))).toUpperCase();
        macroTextCache[presetIndex] = text;
        return text;
    }

    int getNumRows() override { return filtered.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return;

        const auto presetIndex = filtered[row];
        const auto rowBounds = juce::Rectangle<float> (3.0f, 1.0f, (float) width - 6.0f, (float) height - 2.0f);
        const auto name = names[presetIndex];
        const auto isCurrent = name == processorRef.getCurrentPresetName();

        if (selected)
        {
            juce::ColourGradient selection (IlanaTheme::accent().withAlpha (0.30f), 0.0f, 0.0f,
                                            IlanaTheme::accent().withAlpha (0.10f), (float) width, 0.0f, false);
            g.setGradientFill (selection);
            g.fillRoundedRectangle (rowBounds, 5.0f);
        }
        else if (row == hoveredRow)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (rowBounds, 5.0f);
        }

        if (isCurrent)
        {
            g.setColour (IlanaTheme::accent());
            g.fillRoundedRectangle (rowBounds.withWidth (3.0f).reduced (0.0f, 4.0f), 1.5f);
        }

        const auto favourite = isFavouriteName (settings, name);

        // Every row shows its star (a click on it toggles the favourite).
        if (favourite)
            drawStar (g, { 17.0f, (float) height * 0.5f }, 5.5f, juce::Colour (0xffffd447), true);
        else
            drawStar (g, { 17.0f, (float) height * 0.5f }, 5.5f,
                      juce::Colours::white.withAlpha (row == hoveredRow && hoveredX < 30 ? 0.75f : (row == hoveredRow ? 0.4f : 0.18f)), false);

        if (docked)
        {
            paintDockedRow (g, presetIndex, width, height, selected, isCurrent);
            return;
        }

        g.setColour (juce::Colours::white.withAlpha (selected || isCurrent ? 0.97f : 0.8f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, isCurrent));
        g.drawText (name, juce::Rectangle<int> (32, 0, width - 170, height), juce::Justification::centredLeft);
        const auto nameFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::title, isCurrent));
        const auto nameEnd = 32 + juce::GlyphArrangement::getStringWidthInt (nameFont, name);

        const auto isUser = isUserPreset (presetIndex);
        const auto category = categories[presetIndex];
        const auto colour = categoryColour (category);
        auto tagText = category.toUpperCase();

        if (isUser)
            tagText = tagText.isEmpty() ? juce::String ("USER") : juce::String (juce::CharPointer_UTF8 ("USER \xc2\xb7 ")) + tagText;

        const auto tagWidth = tagText.isEmpty() ? 0.0f : juce::jmax (40.0f, (float) tagText.length() * 6.4f + 14.0f);

        // What this preset's macros are called, between the name and the tag.
        {
            const auto macros = macroNamesFor (presetIndex, isCurrent);
            const auto left = juce::jmax (nameEnd + 16, (int) ((float) width * 0.4f));
            const auto right = width - (int) tagWidth - 20;

            if (macros.isNotEmpty() && right - left > 40)
            {
                g.setColour (IlanaTheme::Ui::text2.withAlpha (selected ? 0.95f : 0.75f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawText (macros, juce::Rectangle<int> (left, 0, right - left, height), juce::Justification::centredLeft, true);
            }
        }

        if (tagText.isEmpty())
            return;

        const auto tagBounds = juce::Rectangle<float> ((float) width - tagWidth - 10.0f,
                                                       (float) height * 0.5f - 8.0f, tagWidth, 16.0f);

        g.setColour (colour.withAlpha (selected ? 0.30f : 0.16f));
        g.fillRoundedRectangle (tagBounds, 8.0f);
        g.setColour (colour.withAlpha (selected ? 1.0f : 0.85f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (tagText, tagBounds.toNearestInt(), juce::Justification::centred);
    }

    // Docked rows: the name with its category tag, and the macro names
    // on a second line.
    void paintDockedRow (juce::Graphics& g, int presetIndex, int width, int height, bool selected, bool isCurrent)
    {
        const auto name = names[presetIndex];
        const auto category = categories[presetIndex];
        const auto colour = categoryColour (category);
        const auto isUser = isUserPreset (presetIndex);
        const auto tagText = isUser ? juce::String ("USER") : category.toUpperCase();
        const juce::Font tagFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        const auto tagWidth = tagText.isEmpty() ? 0.0f : (float) juce::GlyphArrangement::getStringWidthInt (tagFont, tagText) + 14.0f;
        const auto top = juce::Rectangle<int> (32, 3, width - 40, height / 2 - 1);
        const auto bottom = juce::Rectangle<int> (32, height / 2, width - 40, height / 2 - 4);

        g.setColour (juce::Colours::white.withAlpha (selected || isCurrent ? 0.97f : 0.82f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, isCurrent));
        g.drawText (name, top.withTrimmedRight ((int) tagWidth + 6), juce::Justification::centredLeft, true);

        if (tagText.isNotEmpty())
        {
            const auto tagBounds = juce::Rectangle<float> ((float) top.getRight() - tagWidth, (float) top.getCentreY() - 7.5f, tagWidth, 15.0f);
            g.setColour (colour.withAlpha (selected ? 0.30f : 0.16f));
            g.fillRoundedRectangle (tagBounds, 7.5f);
            g.setColour (colour.withAlpha (selected ? 1.0f : 0.85f));
            g.setFont (tagFont);
            g.drawText (tagText, tagBounds.toNearestInt(), juce::Justification::centred);
        }

        const auto macros = macroNamesFor (presetIndex, isCurrent);

        if (macros.isNotEmpty())
        {
            g.setColour (IlanaTheme::Ui::text2.withAlpha (selected ? 0.95f : 0.7f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText (macros, bottom, juce::Justification::centredLeft, true);
        }
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (suppressLoad || ! juce::isPositiveAndBelow (lastRowSelected, filtered.size()))
            return;

        // A click on a row's star only toggles the favourite: the selection
        // goes back to the preset that is loaded.
        if (juce::ModifierKeys::currentModifiers.isLeftButtonDown() && list.getMouseXYRelative().x < 30)
        {
            juce::Component::SafePointer<PresetPanel> safeThis (this);
            juce::MessageManager::callAsync ([safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->showCurrentPreset();
            });
            return;
        }

        loadedBySelection = true;
        loadRow (lastRowSelected, false);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& event) override
    {
        const auto justLoaded = std::exchange (loadedBySelection, false);

        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return;

        const auto presetIndex = filtered[row];

        if (event.mods.isPopupMenu())
        {
            showRowMenu (presetIndex);
            return;
        }

        // The star toggles the favourite.
        if (event.x < 30)
        {
            toggleFavourite (presetIndex);
            return;
        }

        if (! justLoaded)
            loadRow (row, false);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override
    {
        if (event.x >= 30)
            loadRow (row, true);
    }

    void returnKeyPressed (int lastRowSelected) override { loadRow (lastRowSelected, true); }

    // Up/down in the search box move through the list.
    bool keyPressed (const juce::KeyPress& key, juce::Component* origin) override
    {
        if (origin == &search && (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey))
        {
            stepSelection (key == juce::KeyPress::downKey ? 1 : -1);
            return true;
        }

        if (key == juce::KeyPress::escapeKey)
        {
            if (docked)
                unfocusAllComponents();
            else
                close();
            return true;
        }

        return false;
    }

    void loadRow (int row, bool closeAfter)
    {
        closeAfter = closeAfter && ! docked;

        if (! juce::isPositiveAndBelow (row, filtered.size()))
        {
            if (closeAfter)
                close();

            return;
        }

        selectedPreset = filtered[row];
        updateDeleteButton();

        if (onLoad != nullptr)
            onLoad (selectedPreset, closeAfter);

        list.repaint();
    }

    void loadRandom()
    {
        if (filtered.isEmpty())
            return;

        auto row = juce::Random::getSystemRandom().nextInt (filtered.size());

        if (filtered.size() > 1 && row == list.getSelectedRow())
            row = (row + 1 + juce::Random::getSystemRandom().nextInt (filtered.size() - 1)) % filtered.size();

        const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
        list.selectRow (row);
        loadRow (row, false);
    }

    // Selects and scrolls to the loaded preset, without reloading it.
    void showCurrentPreset()
    {
        const auto current = names.indexOf (processorRef.getCurrentPresetName());
        const auto row = filtered.indexOf (current);
        const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);

        if (row >= 0)
        {
            list.selectRow (row);
            list.scrollToEnsureRowIsOnscreen (row);
        }
        else
        {
            list.deselectAllRows();
        }

        shownCurrentName = processorRef.getCurrentPresetName();
    }

    void toggleFavourite (int presetIndex)
    {
        if (settings == nullptr || ! juce::isPositiveAndBelow (presetIndex, names.size()))
            return;

        const auto name = names[presetIndex];
        settings->setValue ("fav_" + name, isFavouriteName (settings, name) ? "0" : "1");
        settings->saveIfNeeded();

        if (onFavouriteChanged != nullptr)
            onFavouriteChanged();

        rebuildSidebar();

        if (filterKey == favouritesKey)
            rebuild();
        else
            list.repaint();
    }

    void showRowMenu (int presetIndex)
    {
        if (! juce::isPositiveAndBelow (presetIndex, names.size()))
            return;

        const auto name = names[presetIndex];
        const auto isUser = isUserPreset (presetIndex);
        const auto favourite = isFavouriteName (settings, name);

        juce::PopupMenu menu;
        menu.addItem (1, "Load");
        menu.addItem (2, favourite ? "Remove from favourites" : "Add to favourites");

        if (isUser)
            menu.addItem (3, "Delete");

        juce::Component::SafePointer<PresetPanel> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&list),
                            [safeThis, presetIndex] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                if (result == 1)
                                {
                                    const auto row = safeThis->filtered.indexOf (presetIndex);
                                    safeThis->loadRow (row, true);
                                }
                                else if (result == 2)
                                {
                                    safeThis->toggleFavourite (presetIndex);
                                }
                                else if (result == 3)
                                {
                                    safeThis->deletePresetByIndex (presetIndex);
                                }
                            });
    }

    void deletePresetByIndex (int presetIndex)
    {
        if (! juce::isPositiveAndBelow (presetIndex, names.size()) || ! isUserPreset (presetIndex))
            return;

        const auto name = names[presetIndex];
        const auto file = processorRef.getUserPresetDirectory().getChildFile (name + ".ilanapreset");

        if (! file.existsAsFile())
            return;

        juce::Component::SafePointer<PresetPanel> safeThis (this);

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                            "Delete preset",
                                            "Move '" + name + "' to the recycle bin?",
                                            "Delete", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safeThis, file] (int result)
                                            {
                                                if (result == 1 && safeThis != nullptr)
                                                {
                                                    if (! file.moveToTrash())
                                                        file.deleteFile();

                                                    safeThis->selectedPreset = -1;
                                                    safeThis->refresh();
                                                }
                                            }));
    }

    // The category (and DX7 visibility) filter, before search, bank and tags.
    bool passesCategory (int i) const
    {
        if (filterKey == favouritesKey)
            return isFavouriteName (settings, names[i]);

        if (filterKey == userKey)
            return isUserPreset (i);

        if (filterKey.isNotEmpty())
            return categories[i] == filterKey;

        return showDx7 || categories[i] != "DX7";
    }

    static juce::StringArray tagsOf (const juce::String& text)
    {
        auto list = juce::StringArray::fromTokens (text, ",", "");
        list.trim();
        list.removeEmptyStrings();
        return list;
    }

    // Under DX7: a chip per bank. Elsewhere: a chip per tag the presets
    // shown carry (only user presets have tags), when there are any.
    void rebuildChips()
    {
        std::vector<ChipRow::Entry> entries;

        if (filterKey == "DX7")
        {
            juce::StringArray seen;

            for (int i = 0; i < names.size(); ++i)
                if (categories[i] == "DX7" && banks[i].isNotEmpty() && ! seen.contains (banks[i]))
                    seen.add (banks[i]);

            if (! seen.contains (bankFilter))
                bankFilter = {};

            entries.push_back ({ "bank:", "All banks", bankFilter.isEmpty() });

            for (const auto& bank : seen)
                entries.push_back ({ "bank:" + bank, bank, bank == bankFilter });
        }
        else
        {
            bankFilter = {};
            juce::StringArray seen;

            for (int i = 0; i < names.size(); ++i)
                if (passesCategory (i))
                    for (const auto& tag : tagsOf (tags[i]))
                        if (! seen.contains (tag, true))
                            seen.add (tag);

            seen.sortNatural();

            for (int i = selectedTags.size(); --i >= 0;)
                if (! seen.contains (selectedTags[i], true))
                    selectedTags.remove (i);

            for (const auto& tag : seen)
                entries.push_back ({ "tag:" + tag, tag, selectedTags.contains (tag, true) });
        }

        const auto wasShown = chips.isVisible();
        const auto oldCount = chips.entries.size();
        chips.entries = std::move (entries);
        chips.setVisible (! chips.entries.empty());
        chips.repaint();

        if (wasShown != chips.isVisible() || oldCount != chips.entries.size())
            resized();
    }

    void chipClicked (const juce::String& key)
    {
        if (key.startsWith ("bank:"))
        {
            bankFilter = key.fromFirstOccurrenceOf ("bank:", false, false);
        }
        else if (key.startsWith ("tag:"))
        {
            const auto tag = key.fromFirstOccurrenceOf ("tag:", false, false);

            if (selectedTags.contains (tag, true))
                selectedTags.removeString (tag, true);
            else
                selectedTags.add (tag);
        }

        rebuild();
    }

    // The chips above the list, wrapping onto more lines when they need to.
    void layoutChips (juce::Rectangle<int>& area)
    {
        if (! chips.isVisible())
            return;

        const auto height = chips.getPreferredHeight (area.getWidth());
        chips.setBounds (area.removeFromTop (height));
        area.removeFromTop (6);
    }

    void sortFiltered()
    {
        static const juce::StringArray categoryOrder { "Init", "Bass", "Lead", "Pluck", "Pad", "Keys", "Chords", "Arp",
                                                       "Drone", "Drums", "FX", "FX Input", "Generative", "DX7", "Other", "User" };
        const auto rank = [] (const juce::String& category)
        {
            const auto index = categoryOrder.indexOf (category);
            return index >= 0 ? index : categoryOrder.size();
        };

        std::vector<int> order (filtered.begin(), filtered.end());

        std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
        {
            if (sortMode == sortByCategory && categories[a] != categories[b])
            {
                const auto rankA = rank (categories[a]), rankB = rank (categories[b]);
                return rankA != rankB ? rankA < rankB : categories[a].compareNatural (categories[b], false) < 0;
            }

            if (sortMode == sortFavouritesFirst)
            {
                const auto favA = isFavouriteName (settings, names[a]), favB = isFavouriteName (settings, names[b]);

                if (favA != favB)
                    return favA;
            }

            return names[a].compareNatural (names[b], false) < 0;
        });

        filtered.clearQuick();

        for (auto index : order)
            filtered.add (index);
    }

    void updateSortButton()
    {
        static const char* labels[] { "SORT: NAME", "SORT: CATEGORY", "SORT: FAVOURITES" };
        sortButton.setButtonText (juce::String (labels[sortMode]) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
    }

    void showSortMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Name (A-Z)", true, sortMode == sortByName);
        menu.addItem (2, "Category, then name", true, sortMode == sortByCategory);
        menu.addItem (3, "Favourites first", true, sortMode == sortFavouritesFirst);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sortButton),
                            [safeThis = juce::Component::SafePointer<PresetPanel> (this)] (int result)
                            {
                                if (safeThis != nullptr && result > 0)
                                    safeThis->setSortMode (result - 1);
                            });
    }

    void rebuild()
    {
        filtered.clear();
        rebuildChips();
        hiddenDx7Matches = 0;

        const auto query = search.getText().trim().toLowerCase();
        const auto words = juce::StringArray::fromTokens (query, " ", "");

        for (int i = 0; i < names.size(); ++i)
        {
            // Every word must match the name, category, tags or bank.
            const auto haystack = (names[i] + " " + categories[i] + " " + tags[i] + " " + banks[i]).toLowerCase();
            auto matches = true;

            for (const auto& word : words)
                matches = matches && haystack.contains (word);

            if (! matches)
                continue;

            if (! passesCategory (i))
            {
                // What All leaves out, for the empty list's hint.
                hiddenDx7Matches += filterKey.isEmpty() && words.size() > 0 && categories[i] == "DX7" ? 1 : 0;
                continue;
            }

            if (bankFilter.isNotEmpty() && banks[i] != bankFilter)
                continue;

            auto hasTags = true;

            for (const auto& tag : selectedTags)
                hasTags = hasTags && tagsOf (tags[i]).contains (tag, true);

            if (hasTags)
                filtered.add (i);
        }

        sortFiltered();

        {
            const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
            list.updateContent();
            list.deselectAllRows();

            const auto row = filtered.indexOf (selectedPreset >= 0 ? selectedPreset
                                                                   : names.indexOf (processorRef.getCurrentPresetName()));
            if (row >= 0)
                list.selectRow (row);
            else
                list.scrollToEnsureRowIsOnscreen (0); // a new filter starts at the top
        }

        list.repaint();
        updateDeleteButton();
        repaint();
    }

    void timerCallback() override
    {
        const auto target = closing ? 0.0f : 1.0f;

        if (std::abs (appear - target) > 0.005f)
        {
            appear = IlanaAnim::approach (appear, target, 0.2f, frameTicks());
            const auto eased = IlanaAnim::easeOutCubic (appear);

            setAlpha (eased);
            setTransform (juce::AffineTransform::translation (0.0f, std::round ((1.0f - eased) * -10.0f)));
            scrim.setLevel (eased);
        }
        else if (appear != target)
        {
            appear = target;
            setAlpha (appear);
            setTransform ({});
            scrim.setLevel (appear);

            if (closing)
            {
                closing = false;
                setVisible (false);
                scrim.setVisible (false);
                stopTimer();
                return;
            }
        }

        const auto relative = list.getMouseXYRelative();
        const auto hovered = list.isMouseOver (true)
                                 ? list.getRowContainingPosition (relative.x, relative.y)
                                 : -1;

        if (hovered != hoveredRow || (hovered >= 0 && (relative.x < 30) != (hoveredX < 30)))
        {
            hoveredRow = hovered;
            hoveredX = relative.x;
            list.repaint();
        }

        // The header's next/previous buttons can change the preset meanwhile.
        if (processorRef.getCurrentPresetName() != shownCurrentName)
        {
            shownCurrentName = processorRef.getCurrentPresetName();
            list.repaint();
        }

        if (! closing && appear >= 0.999f && getTimerInterval() != 50)
            startTimerHz (20);
    }

    void watchClicks()
    {
        if (auto* top = getTopLevelComponent(); top != nullptr && top != this && watched == nullptr)
        {
            watched = top;
            top->addMouseListener (this, true);
        }
    }

    void stopWatchingClicks()
    {
        if (watched != nullptr)
            watched->removeMouseListener (this);

        watched = nullptr;
    }

    void updateDeleteButton()
    {
        deleteButton.setEnabled (juce::isPositiveAndBelow (selectedPreset, names.size())
                                 && isUserPreset (selectedPreset));
    }

    void deleteSelected()
    {
        if (juce::isPositiveAndBelow (selectedPreset, names.size()) && isUserPreset (selectedPreset))
            deletePresetByIndex (selectedPreset);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile* settings = nullptr;
    bool isUserPreset (int index) const { return index >= factoryCount; }

    juce::StringArray names, categories, tags, banks;
    const int factoryCount = processorRef.getFactoryPresetNames().size();
    std::map<int, juce::String> macroTextCache;
    juce::Array<int> filtered;
    juce::TextEditor search;
    Sidebar sidebar;
    juce::ListBox list { "presets", this };
    juce::TextButton surpriseButton { "SURPRISE ME" };
    juce::TextButton saveAsButton { "SAVE AS" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton folderButton { "FOLDER" };
    juce::TextButton sortButton;
    juce::ToggleButton dx7Toggle;
    ChipRow chips;
    int sortMode = sortByName;
    bool showDx7 = false;
    juce::String bankFilter;
    juce::StringArray selectedTags;
    int hiddenDx7Matches = 0;
    int hoveredX = -1;
    juce::Component::SafePointer<juce::Component> anchor;
    juce::Component::SafePointer<juce::Component> watched;
    juce::String filterKey;
    juce::String shownCurrentName;
    int selectedPreset = -1;
    int hoveredRow = -1;
    float appear = 0.0f;
    bool closing = false;

    struct Scrim : juce::Component
    {
        std::function<void()> onClick;
        float level = 0.0f;

        void setLevel (float newLevel)
        {
            if (std::abs (newLevel - level) > 0.001f)
            {
                level = newLevel;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black.withAlpha (0.6f * level)); }
        void mouseDown (const juce::MouseEvent&) override { if (onClick != nullptr) onClick(); }
    };

    Scrim scrim;
    juce::Rectangle<int> scrimArea;
    bool suppressLoad = false;
    bool loadedBySelection = false;
};
