#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

#include "../PluginProcessor.h"
#include "../PresetNames.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

// The preset browser. It drops down under the preset name: categories (with
// counts) on the left, search, filter chips and the list on the right.
//   click        load the preset and keep browsing
//   double-click load it and close
//   up / down    step through the list, loading as you go
//   Enter        keep the selected preset and close
//   Esc, or a click anywhere outside, closes
// Clicking the star on a row makes it a favourite. The list is sorted by
// name (SORT: by category, or favourites first). "All" is every preset.
// The 288 DX7 voices sit in the categories of what they sound like (Keys,
// Bass, Pad...); the DX7 chip shows only them, with a chip per bank (ROM1A,
// ROM1B..., Dexed's and imported ones). Each preset carries 3-6 descriptive
// tags (factory ones from tools/tag_presets.py), shown as chips on its row
// and above the list, where a click filters by them (several narrow it);
// a tag chip on a row filters too. A row's tooltip names its macros.
// DOCK opens the browser over the page area instead (the window keeps its
// size): categories on the left, the list in the middle, and the selected
// preset's details (author, comment, tags, macros) on the right. The keys
// are the same (review 7): Enter keeps the preset and closes it, Esc
// closes it; a click or double-click only loads, and picking a tab closes
// it too. FLOAT returns it to the drop-down, the cross closes it. A voice a
// later DX7 cartridge repeats is listed once ("also in ROM4A"), and under
// its own bank's chip.
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
        Presets::migrateRenamedPresetSettings (settings);
        setWantsKeyboardFocus (true);

        search.setTextToShowWhenEmpty ("Search names, categories, tags and banks", juce::Colours::white.withAlpha (0.35f));
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
        list.setRowHeight (rowHeight);
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
        folderButton.setTooltip ("Open your user preset folder");
        surpriseButton.onClick = [this] { loadRandom(); };
        surpriseButton.setTooltip ("Random preset\nLoads a random preset from the list shown.");
        importButton.onClick = [this]
        {
            if (onImportSyx != nullptr)
                onImportSyx();
        };
        importButton.setTooltip ("Import a DX7 or Dexed bank (.syx): its voices become your presets, under the DX7 chip");

        for (auto* button : { &surpriseButton, &saveAsButton, &deleteButton, &folderButton, &importButton })
            addAndMakeVisible (button);

        // SAVE AS is the main action; RANDOM PRESET is an ordinary button.
        saveAsButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.35f));
        updateDeleteButton();

        sortMode = settings != nullptr ? juce::jlimit (0, 3, settings->getIntValue ("presetSort", 0)) : 0;
        sortButton.setTooltip ("Sort the list by name, by category, with favourites first, or with the ones you loaded last first");
        sortButton.onClick = [this] { showSortMenu(); };
        addAndMakeVisible (sortButton);
        updateSortButton();

        chips.onClick = [this] (const juce::String& key) { chipClicked (key); };
        addChildComponent (chips);

        dockButton.setTooltip ("Open the browser over the page, with each preset's details, and keep it open while you load");
        dockButton.onClick = [this]
        {
            if (onDockRequest != nullptr)
                onDockRequest (! docked);
        };
        addAndMakeVisible (dockButton);

        // Audition (S15-1): PLAY sounds C3 through the loaded preset (a
        // click about a second, held for as long as it is down); AUTO-PLAY
        // does it after every pick. Space does the same in the list.
        playButton.setTooltip ("Play C3\nHolds the note while pressed, about a second on a click. Space does the same in the list.");
        playButton.onStateChange = [this]
        {
            if (playButton.isDown())
                startAudition();
            else if (auditioning && juce::Time::getMillisecondCounter() - auditionStarted >= auditionMs)
                stopAudition();
        };
        addChildComponent (playButton);
        autoPlayButton.setClickingTogglesState (true);
        autoPlayButton.setToggleState (settings != nullptr && settings->getBoolValue ("presetAutoPlay", false), juce::dontSendNotification);
        autoPlayButton.setTooltip ("Auto-play\nPlay C3 each time you pick a preset.");
        autoPlayButton.onClick = [this]
        {
            if (settings != nullptr)
                settings->setValue ("presetAutoPlay", autoPlayButton.getToggleState());
        };
        addChildComponent (autoPlayButton);

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
        stopAudition();
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
    // IMPORT .SYX pressed (the editor asks for the file and imports it).
    std::function<void()> onImportSyx;
    // DOCK / FLOAT pressed (true: dock over the page), and the docked cross.
    std::function<void (bool)> onDockRequest;
    std::function<void()> onDockedClose;
    // The drop-down finished closing (a browsing session ended).
    std::function<void()> onClosed;

    // Docked: a panel over the page area that stays open (no scrim, outside
    // clicks and Esc leave it alone). Undocked: the drop-down.
    void setDocked (bool shouldDock)
    {
        if (docked == shouldDock)
            return;

        docked = shouldDock;
        dockButton.setButtonText (docked ? "FLOAT" : "DOCK");
        dockButton.setTooltip (docked ? "Back to the drop-down under the preset name"
                                      : "Open the browser over the page, with each preset's details, and keep it open while you load");
        closeDockButton.setVisible (docked);

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
        infoCache.clear();
        categories = processorRef.getAllPresetCategories();
        tags = processorRef.getAllPresetTags();
        banks = processorRef.getAllPresetBanks();
        repeats = processorRef.getPresetRepeats();
        shownNames.clear();
        tagLists.clear();
        alsoIn.assign ((size_t) names.size(), {});

        for (int i = 0; i < names.size(); ++i)
        {
            shownNames.add (banks[i].isNotEmpty() ? Presets::dx7DisplayName (names[i]) : names[i]);
            auto tagList = tagsOf (tags[i]);

            // Every DX7 voice is FM: the tag would tell nothing (review 7).
            if (banks[i].isNotEmpty())
            {
                tagList.removeString ("FM", true);
                tagList.removeString ("DX7", true);
            }

            tagLists.push_back (tagList);
        }

        // A voice a later cartridge repeats lists that bank ("also in ROM4A").
        for (int i = 0; i < names.size(); ++i)
            if (const auto original = repeats[i]; juce::isPositiveAndBelow (original, names.size()))
                alsoIn[(size_t) original].add (banks[i] + (shownNames[i] != shownNames[original] ? " (as " + shownNames[i] + ")" : juce::String()));

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
            toFront (false);
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

    // Docked, closing is the editor's (it gives the page back).
    void close()
    {
        if (docked)
        {
            if (isVisible() && onDockedClose != nullptr)
                onDockedClose();

            return;
        }

        stopAudition();

        if (! isVisible())
            return;

        closing = true;
        stopWatchingClicks();
        startTimerHz (60);
    }

    bool isOpen() const { return isVisible() && ! closing; }

    // Sort orders (kept in the settings).
    enum SortMode { sortByName = 0, sortByCategory, sortFavouritesFirst, sortRecentFirst };

    void setSortMode (int mode)
    {
        sortMode = juce::jlimit (0, 3, mode);

        if (settings != nullptr)
        {
            settings->setValue ("presetSort", sortMode);
            settings->saveIfNeeded();
        }

        updateSortButton();
        rebuild();
    }

    int getSortMode() const { return sortMode; }

    // Shows only the DX7 voices, of one bank ("" for every bank); after an
    // import, the editor opens the browser on the new bank this way.
    void showDx7Bank (const juce::String& bank)
    {
        filterKey = {};
        sidebar.selected = {};
        sidebar.repaint();
        dx7Only = true;
        bankFilter = bank;
        rebuild();
    }

    // For the tests: the sidebar's choice ("" is All, a category's name,
    // or "*fav" / "*user"), the search, the names listed (as saved, and as
    // shown), the chips, and a click on a row at x (the star is the first
    // 30 px; tag chips on the row filter).
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

    juce::StringArray getListedDisplayNames() const
    {
        juce::StringArray shown;

        for (auto index : filtered)
            shown.add (shownNames[index]);

        return shown;
    }

    juce::StringArray getSidebarKeys() const
    {
        juce::StringArray keys;

        for (const auto& entry : sidebar.entries)
            keys.add (entry.key);

        return keys;
    }

    juce::StringArray getChipKeys() const
    {
        juce::StringArray keys;

        for (const auto& chip : chips.entries)
            keys.add (chip.key);

        return keys;
    }

    // The tag chips drawn on a preset's row (in the list shown).
    juce::StringArray getRowTags (int row) const
    {
        juce::StringArray shown;

        if (juce::isPositiveAndBelow (row, filtered.size()))
            for (const auto& box : rowTagBoxes (filtered[row], list.getVisibleRowWidth(), list.getRowHeight()))
                shown.add (box.second);

        return shown;
    }

    juce::String getRowTooltip (int row) { return getTooltipForRow (row); }
    bool isDeleteEnabled() const { return deleteButton.isEnabled(); }
    juce::String getDeleteTooltip() { return deleteButton.getTooltip(); }
    juce::Rectangle<int> getListBounds() const { return list.getBounds(); }
    int getListRowHeight() const { return list.getRowHeight(); }

    void clickChip (const juce::String& key) { chipClicked (key); }
    juce::Rectangle<int> getChipBounds (const juce::String& key) const { return chips.getChipBounds (key); }
    int getChipRowWidth() const { return chips.getWidth(); }

    juce::String getChipLabel (const juce::String& key) const
    {
        for (const auto& chip : chips.entries)
            if (chip.key == key)
                return chip.label;

        return {};
    }

    void clickRow (int row, int x)
    {
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), { (float) x, (float) rowHeight * 0.5f },
                                      {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &list, &list, juce::Time::getCurrentTime(),
                                      { (float) x, (float) rowHeight * 0.5f }, juce::Time::getCurrentTime(), 1, false);
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

        IlanaTheme::paintCard (g, bounds, 9.0f, IlanaTheme::accent().withAlpha (docked ? 0.3f : 0.45f));

        auto header = getLocalBounds().reduced (14, 0).removeFromTop (34);
        g.setColour (IlanaTheme::accent());
        g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("PRESETS", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        const auto count = juce::String (filtered.size()) + " of " + juce::String (names.size() - repeatCount());
        // The same keys docked and floating (review 7).
        const auto keys = juce::String (juce::CharPointer_UTF8 ("Up/Down browse  \xc2\xb7  Space play  \xc2\xb7  Enter keep  \xc2\xb7  Esc close"));

        // One layout floating and docked: the count after the title, the keys
        // centred (review 11, S11-15).
        g.drawText (count, header.withTrimmedLeft (84), juce::Justification::centredLeft);
        g.drawText (keys, header.withTrimmedRight (dockButton.getWidth() + (docked ? closeDockButton.getWidth() + 20 : 40)), juce::Justification::centred);

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
        IlanaTheme::paintRecessedPanel (g, listFrame.toFloat(), 6.0f);

        if (filtered.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            IlanaTheme::drawFitted (g, filterKey == favouritesKey && search.isEmpty() && selectedTags.isEmpty()
                                  ? juce::String ("No favourites yet. Click the star on a preset to add it.")
                                  : filterKey == recentKey && search.isEmpty() && selectedTags.isEmpty()
                                  ? juce::String ("Nothing loaded from this browser yet.")
                                  : juce::String ("No presets match. Clear the search or a chip to see more."),
                              list.getBounds().reduced (16, 0), juce::Justification::centred, 2);
        }

        if (docked || ! detailsArea.isEmpty())
            paintDetails (g);
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
            // The same 28 px as the buttons along the bottom (S15-10).
            auto header = getLocalBounds().reduced (12, 0).removeFromTop (34).withSizeKeepingCentre (getWidth() - 24, 28);

            if (docked)
            {
                closeDockButton.setBounds (header.removeFromRight (28));
                header.removeFromRight (4);
            }

            dockButton.setBounds (header.removeFromRight (64));
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
        sidebar.setBounds (left.reduced (1));

        // On a wide drop-down the preset's details sit beside the list, as
        // docked (review 10, S10-5).
        detailsArea = {};
        if (getWidth() >= wideWidth)
        {
            detailsArea = area.removeFromRight (250);
            area.removeFromRight (10);
        }
        layoutAudition();

        auto searchRow = area.removeFromTop (30);
        sortButton.setBounds (searchRow.removeFromRight (128).reduced (1, 2));
        searchRow.removeFromRight (8);
        search.setBounds (searchRow.reduced (1));
        area.removeFromTop (8);
        layoutChips (area);
        layoutList (area);

        surpriseButton.setBounds (footer.removeFromLeft (150).reduced (1, 0));
        footer.removeFromLeft (10);
        layoutFooter (footer, { &saveAsButton, &deleteButton, &folderButton, &importButton });
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }

        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            stepSelection (key == juce::KeyPress::downKey ? 1 : -1);
            return true;
        }

        if (key == juce::KeyPress::spaceKey)
        {
            startAudition();
            return true;
        }

        return false;
    }

    // Audition: C3 (60) through the loaded preset, by the processor's
    // preview-note path (the keyboard's: a lock-free hand-over to the audio
    // thread). Public for the UI test.
    void startAudition()
    {
        if (auditioning)
            processorRef.triggerPreviewNote (auditionNote, false);

        processorRef.triggerPreviewNote (auditionNote, true, 0.7f);
        auditioning = true;
        auditionStarted = juce::Time::getMillisecondCounter();
        const auto generation = ++auditionGeneration;
        playButton.setToggleState (true, juce::dontSendNotification); // (lit while the note sounds; the look-and-feel fades it)

        juce::Timer::callAfterDelay (auditionMs, [safeThis = juce::Component::SafePointer<PresetPanel> (this), generation]
        {
            if (safeThis != nullptr && safeThis->auditionGeneration == generation && ! safeThis->playButton.isDown())
                safeThis->stopAudition();
        });
        repaint (playButton.getBounds());
    }

    void stopAudition()
    {
        if (! auditioning)
            return;

        auditioning = false;
        ++auditionGeneration;
        playButton.setToggleState (false, juce::dontSendNotification);
        processorRef.triggerPreviewNote (auditionNote, false);
        repaint (playButton.getBounds());
    }

    bool isAuditioning() const { return auditioning; }
    bool isPlayLit() const { return playButton.getToggleState(); } // (PLAY lights while the note sounds; the UI test reads it)
    juce::String getAuditionCaption() const { return deleteButton.isEnabled() ? juce::String() : deleteReason(); }
    juce::Rectangle<int> getPlayBounds() const { return playButton.isVisible() ? playButton.getBounds() : juce::Rectangle<int>(); }
    juce::Rectangle<int> getDockBounds() const { return dockButton.getBounds(); }
    juce::Rectangle<int> getFooterButtonBounds() const { return surpriseButton.getBounds(); }

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
    static constexpr int rowHeight = 28, wideWidth = 900;

    // The list sits in a recessed frame; its height is a whole number of
    // rows, so the last row is never cut in half (the frame keeps the
    // spare few pixels under it).
    void layoutList (juce::Rectangle<int> area)
    {
        listFrame = area.expanded (1);
        auto inner = area.reduced (1);
        const auto rows = juce::jmax (1, inner.getHeight() / rowHeight);
        list.setBounds (inner.withHeight (rows * rowHeight));
    }

    static void layoutFooter (juce::Rectangle<int> footer, std::initializer_list<juce::Button*> buttons)
    {
        const auto width = footer.getWidth() / (int) buttons.size();

        for (auto* button : buttons)
            button->setBounds (footer.removeFromLeft (width).reduced (2, 0));
    }

    void layoutDocked()
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (26);

        auto footer = area.removeFromBottom (28);
        area.removeFromBottom (8);

        auto left = area.removeFromLeft (170);
        area.removeFromLeft (10);
        sidebar.setBounds (left.reduced (1));

        detailsArea = area.removeFromRight (250);
        area.removeFromRight (10);
        layoutAudition();

        auto searchRow = area.removeFromTop (30);
        sortButton.setBounds (searchRow.removeFromRight (140).reduced (1, 2));
        searchRow.removeFromRight (8);
        search.setBounds (searchRow.reduced (1));
        area.removeFromTop (8);
        layoutChips (area);
        layoutList (area);

        surpriseButton.setBounds (footer.removeFromLeft (170).reduced (1, 0));
        footer.removeFromLeft (10);
        layoutFooter (footer, { &saveAsButton, &deleteButton, &folderButton, &importButton });
    }

    // Docked: the selected preset (or the one playing) in full on the right.
    void paintDetails (juce::Graphics& g)
    {
        IlanaTheme::paintRecessedPanel (g, detailsArea.toFloat(), 6.0f);
        const auto index = juce::isPositiveAndBelow (selectedPreset, names.size()) ? selectedPreset
                                                                                  : names.indexOf (processorRef.getCurrentPresetName());

        auto area = detailsArea.reduced (14, 12);

        // The foot: PLAY, with the reason DELETE is off above it.
        area.removeFromBottom (28 + 8);
        {
            const auto reason = deleteReason();

            if (reason.isNotEmpty())
            {
                const auto caption = area.removeFromBottom (32);
                area.removeFromBottom (4);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                IlanaTheme::drawFitted (g, reason, caption, juce::Justification::bottomLeft, 2);
            }
        }

        if (! juce::isPositiveAndBelow (index, names.size()))
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            IlanaTheme::drawFitted (g, "Pick a preset to see who made it, its tags and what its macros do.", area,
                              juce::Justification::centredTop, 3);
            return;
        }

        const auto colour = categoryColour (categories[index]);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
        {
            const juce::Font titleFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
            const auto oneLine = juce::GlyphArrangement::getStringWidthInt (titleFont, shownNames[index]) <= area.getWidth();
            IlanaTheme::drawFitted (g, shownNames[index], area.removeFromTop (oneLine ? 26 : 46), juce::Justification::topLeft, 2);
        }

        {
            auto line = area.removeFromTop (20);
            const auto text = (isUserPreset (index) ? juce::String (juce::CharPointer_UTF8 ("USER  \xc2\xb7  ")) : juce::String())
                              + categories[index].toUpperCase();
            g.setColour (colour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (text, line, juce::Justification::centredLeft);

            if (banks[index].isNotEmpty())
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText ("DX7  " + banks[index], line, juce::Justification::centredRight);
            }
        }

        if (! alsoIn[(size_t) index].isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, "Also in " + alsoIn[(size_t) index].joinIntoString (", "), area.removeFromTop (18),
                              juce::Justification::centredLeft, 1);
        }

        const auto& info = infoFor (index);
        area.removeFromTop (6);

        if (info.author.isNotEmpty())
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("by " + info.author, area.removeFromTop (18), juce::Justification::centredLeft, true);
        }

        if (info.comment.isNotEmpty())
        {
            area.removeFromTop (4);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, info.comment, area.removeFromTop (64), juce::Justification::topLeft, 4);
        }

        const auto heading = [&g, &area] (const juce::String& text)
        {
            area.removeFromTop (10);
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (text, area.removeFromTop (16), juce::Justification::centredLeft);
        };

        if (! tagLists[(size_t) index].isEmpty())
        {
            heading ("TAGS");
            const auto font = tagChipFont();
            auto x = area.getX(), y = area.getY();

            for (const auto& tag : tagLists[(size_t) index])
            {
                const auto w = juce::GlyphArrangement::getStringWidthInt (font, tag) + 16;

                if (x > area.getX() && x + w > area.getRight())
                {
                    x = area.getX();
                    y += 24;
                }

                paintTagChip (g, juce::Rectangle<int> (x, y, w, 20).toFloat(), tag, selectedTags.contains (tag, true));
                x += w + 5;
            }

            area.setTop (y + 24);
        }

        // The macros that have a name (all eight on the loaded preset),
        // each with its number.
        const auto macros = macroListFor (index, names[index] == processorRef.getCurrentPresetName());
        auto named = 0;

        for (const auto& macro : macros)
            named += macro.isNotEmpty() ? 1 : 0;

        // The oscillators and their types, read from the patch (the preset
        // is loaded when picked).
        if (names[index] == processorRef.getCurrentPresetName() && area.getHeight() > 60)
        {
            juce::StringArray lines;

            for (int osc = 0; osc < OscillatorIds::count; ++osc)
            {
                if (! processorRef.isOscillatorShown (osc))
                    continue;

                const auto* mode = processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_mode");
                const auto type = juce::jlimit (0, OscMode::count - 1, mode != nullptr ? (int) mode->load() : 0);
                lines.add ("OSC " + juce::String (osc + 1) + "|" + juce::String (OscMode::names[(size_t) type]));
            }

            if (! lines.isEmpty())
            {
                heading ("OSCILLATORS");
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));

                for (const auto& entry : lines)
                {
                    if (area.getHeight() < 18 + 18)
                        break;

                    auto line = area.removeFromTop (18);
                    g.setColour (IlanaTheme::Ui::text3);
                    g.drawText (entry.upToFirstOccurrenceOf ("|", false, false), line.removeFromLeft (46), juce::Justification::centredLeft);
                    g.setColour (IlanaTheme::Ui::text2);
                    IlanaTheme::drawFitted (g, entry.fromFirstOccurrenceOf ("|", false, false), line, juce::Justification::centredLeft, 1);
                }
            }
        }

        if (named > 0 && area.getHeight() >= 16 + 10 + 18)
        {
            heading ("MACROS");
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));

            for (int m = 0; m < macros.size(); ++m)
            {
                if (macros[m].isEmpty())
                    continue;

                if (area.getHeight() < 18)
                    break;

                auto line = area.removeFromTop (18);
                g.setColour (IlanaTheme::Ui::text3);
                g.drawText (juce::String (m + 1), line.removeFromLeft (16), juce::Justification::centredLeft);
                g.setColour (IlanaTheme::Ui::text2);
                IlanaTheme::drawFitted (g, macros[m], line, juce::Justification::centredLeft, 1);
            }
        }
    }

    // PLAY and AUTO-PLAY sit at the foot of the details (S15-1).
    void layoutAudition()
    {
        const auto show = ! detailsArea.isEmpty();
        playButton.setVisible (show);
        autoPlayButton.setVisible (show);

        if (! show)
            return;

        auto foot = detailsArea.reduced (14, 12).removeFromBottom (28);
        autoPlayButton.setBounds (foot.removeFromRight (juce::jmin (112, foot.getWidth() / 2)));
        foot.removeFromRight (6);
        playButton.setBounds (foot);
    }

    juce::String deleteReason() const
    {
        return juce::isPositiveAndBelow (selectedPreset, names.size()) && ! isUserPreset (selectedPreset)
                   ? juce::String ("DELETE is for your own presets; factory presets can't be deleted.")
                   : juce::String();
    }

    static constexpr int auditionNote = 60, auditionMs = 1000;
    bool auditioning = false;
    juce::uint32 auditionStarted = 0;
    int auditionGeneration = 0;
    juce::TextButton playButton { "PLAY C3" };
    juce::TextButton autoPlayButton { "AUTO-PLAY" };

    bool docked = false;
    juce::TextButton dockButton { "DOCK" };
    juce::TextButton closeDockButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };

    static constexpr const char* favouritesKey = "*fav";
    static constexpr const char* recentKey = "*recent";
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

        void mouseMove (const juce::MouseEvent& event) override { setHovered (rowAt (event.y)); }
        void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            const auto row = rowAt (event.y);

            if (juce::isPositiveAndBelow (row, (int) entries.size()))
            {
                selected = entries[(size_t) row].key;
                repaint();

                if (onSelect != nullptr)
                    onSelect (selected);
            }
        }

    private:
        static constexpr int rowHeight = 24;
        static constexpr int gap = 9;

        juce::Rectangle<int> rowBounds (int index) const
        {
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
                if (rowBounds (i).expanded (0, 1).contains (getWidth() / 2, y))
                    return i;

            return -1;
        }

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

    // The filter chips above the list, in groups: the DX7 chip, then its
    // banks while it is on, then (on a line of their own) the tags of the
    // presets shown (most used first). Tags keep to one line; the "+N" chip
    // opens the rest, "Fewer" folds them again.
    class ChipRow : public juce::Component
    {
    public:
        enum Kind { source, bank, tag };

        struct Entry
        {
            juce::String key, label;
            bool selected = false;
            Kind kind = tag;
            juce::Colour colour;
        };

        std::vector<Entry> entries;
        bool expanded = false;
        std::function<void (const juce::String&)> onClick;
        static constexpr const char* moreKey = "*more";
        // The banks fold into one BANK chip that opens a menu (review 9,
        // S9-8: four rows of chips above eleven rows of list).
        static constexpr const char* bankKey = "*bank";

        int getPreferredHeight (int width) const
        {
            const auto boxes = layout (width);
            auto bottom = 0;

            for (const auto& box : boxes)
                bottom = juce::jmax (bottom, box.bounds.getBottom());

            return bottom;
        }

        // Where a chip is drawn (empty when it isn't), for the UI test.
        juce::Rectangle<int> getChipBounds (const juce::String& key) const
        {
            for (const auto& box : layout (getWidth()))
                if (box.key == key)
                    return box.bounds;

            return {};
        }

        // The keys of the chips drawn (tags folded away aren't).
        juce::StringArray getShownKeys() const
        {
            juce::StringArray keys;

            for (const auto& box : layout (getWidth()))
                keys.add (box.key);

            return keys;
        }

        void paint (juce::Graphics& g) override
        {
            const auto boxes = layout (getWidth());
            g.setFont (font());

            // The DX7 pack and its banks are a source filter, not a tag
            // (S15-11): named, and ruled off from the tags under them.
            if (hasSourceRow())
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("SOURCE", juce::Rectangle<int> (0, 0, sourceLabelWidth - 4, chipHeight), juce::Justification::centredLeft);
                g.setFont (font());

                for (const auto& box : boxes)
                    if (box.entry != nullptr && box.entry->kind == tag && box.bounds.getY() > 0)
                    {
                        g.setColour (IlanaTheme::Ui::line);
                        g.fillRect (0, box.bounds.getY() - sourceRuleGap / 2 - 1, getWidth(), 1);
                        break;
                    }
            }

            for (size_t i = 0; i < boxes.size(); ++i)
            {
                const auto& box = boxes[i];
                const auto bounds = box.bounds.toFloat();
                const auto hover = (int) i == hovered;

                if (box.entry == nullptr)
                {
                    // "+N" / "Fewer": a chip that opens or folds the rest
                    // (review 7: it was bare text), not a filter.
                    g.setColour (juce::Colours::white.withAlpha (hover ? 0.12f : 0.06f));
                    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
                    g.setColour (IlanaTheme::Ui::line.brighter (hover ? 0.6f : 0.3f));
                    g.drawRoundedRectangle (bounds.reduced (0.5f), bounds.getHeight() * 0.5f, 1.0f);
                    g.setColour (hover ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
                    g.drawText (box.label, box.bounds, juce::Justification::centred);
                    continue;
                }

                const auto& entry = *box.entry;
                const auto tint = entry.colour != juce::Colour() ? entry.colour : IlanaTheme::accent();

                if (box.dividerBefore)
                {
                    g.setColour (IlanaTheme::Ui::line.brighter (0.4f));
                    g.fillRect (juce::Rectangle<float> (1.0f, bounds.getHeight() - 6.0f).withCentre ({ bounds.getX() - 6.0f, bounds.getCentreY() }));
                }

                const auto isBankChip = box.key == bankKey;
                const auto selected = isBankChip ? box.bankPicked : entry.selected;
                g.setColour (selected ? tint.withAlpha (0.3f) : juce::Colours::white.withAlpha (hover ? 0.1f : 0.04f));
                g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
                g.setColour (selected ? tint : (entry.kind == tag ? IlanaTheme::Ui::line.brighter (0.3f) : tint.withAlpha (0.55f)));
                g.drawRoundedRectangle (bounds.reduced (0.5f), bounds.getHeight() * 0.5f, 1.0f);
                g.setColour (selected ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
                g.drawText (isBankChip ? box.label : entry.label, box.bounds, juce::Justification::centred);
            }
        }

        void mouseMove (const juce::MouseEvent& event) override { setHovered (chipAt (event.getPosition())); }
        void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            const auto boxes = layout (getWidth());
            const auto chip = chipAt (event.getPosition());

            if (chip >= 0 && onClick != nullptr)
                onClick (boxes[(size_t) chip].key);
        }

    private:
        static constexpr int chipHeight = 22, gap = 5, groupGap = 13, sourceLabelWidth = 56, sourceRuleGap = 10;

        bool hasSourceRow() const { return ! entries.empty() && entries.front().kind == source; }

        static juce::Font font() { return juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive, true)); }

        struct Box
        {
            juce::Rectangle<int> bounds;
            juce::String key, label;
            const Entry* entry = nullptr;
            bool dividerBefore = false, bankPicked = false;
        };

        std::vector<Box> layout (int width) const
        {
            std::vector<Box> boxes;
            const auto f = font();
            const auto widthOf = [&f, width] (const juce::String& text)
            {
                return juce::jmin (width, juce::GlyphArrangement::getStringWidthInt (f, text) + 20);
            };

            // "SOURCE" before the pack chip: the first row is not tags.
            auto x = hasSourceRow() ? sourceLabelWidth - gap : 0, y = 0;
            auto previousKind = -1;
            auto tagCount = 0;

            for (const auto& entry : entries)
                tagCount += entry.kind == tag ? 1 : 0;

            auto tagsPlaced = 0;

            auto bankChipPlaced = false;

            for (const auto& entry : entries)
            {
                // The banks are one chip, "BANK" or the bank picked.
                if (entry.kind == bank)
                {
                    if (bankChipPlaced)
                        continue;

                    bankChipPlaced = true;
                    auto picked = juce::String();
                    for (const auto& other : entries)
                        if (other.kind == bank && other.selected)
                            picked = other.label;

                    const auto label = (picked.isEmpty() ? juce::String ("BANK") : picked) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe"));
                    const auto bankW = widthOf (label);
                    auto bankX = x + (x > 0 ? (previousKind != (int) bank ? groupGap : gap) : 0);

                    if (bankX + bankW > width && x > 0)
                    {
                        x = 0;
                        y += chipHeight + gap;
                        bankX = 0;
                    }

                    boxes.push_back ({ { bankX, y, bankW, chipHeight }, bankKey, label, &entry, previousKind >= 0 && bankX > 0, picked.isNotEmpty() });
                    x = bankX + bankW;
                    previousKind = (int) bank;
                    continue;
                }

                const auto w = widthOf (entry.label);
                const auto newGroup = previousKind >= 0 && previousKind != (int) entry.kind;

                // The tags get a line of their own under the DX7 and bank
                // chips (review 7: one row mixed the two).
                if (entry.kind == tag && newGroup && x > 0)
                {
                    x = 0;
                    y += chipHeight + (hasSourceRow() ? sourceRuleGap : gap);
                }

                auto startX = x + (x > 0 ? (newGroup ? groupGap : gap) : 0);

                if (entry.kind == tag && ! expanded)
                {
                    // Folded: the tags stay on the line they start on, with
                    // room kept for "+N" while more are left.
                    const auto left = tagCount - tagsPlaced - 1;
                    const auto moreWidth = left > 0 ? widthOf ("+" + juce::String (left)) + gap : 0;

                    if (startX + w + moreWidth > width)
                    {
                        const auto label = "+" + juce::String (tagCount - tagsPlaced);
                        const auto moreW = widthOf (label);

                        if (startX + moreW > width)
                        {
                            startX = 0;
                            y += chipHeight + gap;
                        }

                        boxes.push_back ({ { startX, y, moreW, chipHeight }, moreKey, label, nullptr, false });
                        return boxes;
                    }
                }
                else if (startX + w > width && x > 0)
                {
                    x = 0;
                    y += chipHeight + gap;
                    startX = 0;
                }

                boxes.push_back ({ { startX, y, w, chipHeight }, entry.key, entry.label, &entry, newGroup && startX > 0 });
                x = startX + w;
                previousKind = (int) entry.kind;
                tagsPlaced += entry.kind == tag ? 1 : 0;
            }

            if (expanded && tagCount > 0)
            {
                const auto w = widthOf ("Fewer");
                auto startX = x + gap;

                if (startX + w > width)
                {
                    startX = 0;
                    y += chipHeight + gap;
                }

                boxes.push_back ({ { startX, y, w, chipHeight }, moreKey, "Fewer", nullptr, false });
            }

            return boxes;
        }

        int chipAt (juce::Point<int> position) const
        {
            const auto boxes = layout (getWidth());

            for (size_t i = 0; i < boxes.size(); ++i)
                if (boxes[i].bounds.contains (position))
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
        if (category == "Brass") return juce::Colour (0xffe8b04a);
        if (category == "Wind") return juce::Colour (0xff7fc8d8);
        if (category == "Pluck") return juce::Colour (0xffffd447);
        if (category == "Pad") return juce::Colour (0xffb28aff);
        if (category == "Drone") return juce::Colour (0xff6fe3c1);
        if (category == "FX") return juce::Colour (0xffe3a56f);
        if (category == "Keys") return juce::Colour (0xff8fd14f);
        if (category == "Chords") return juce::Colour (0xff4fd1a5);
        if (category == "Arp") return juce::Colour (0xffff7ac6);
        if (category == "Drums") return juce::Colour (0xffc9ced6);
        if (category == "Generative") return juce::Colour (0xff4fc3ff);
        return IlanaTheme::Ui::text2;
    }

    // The DX7's membrane-panel red, for the DX7 chip and bank text.
    static juce::Colour dx7Colour() { return juce::Colour (0xffd9765f); }

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

    // Tag chips filter when clicked, so their text is interactive size.
    static juce::Font tagChipFont() { return juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)); }

    static void paintTagChip (juce::Graphics& g, juce::Rectangle<float> box, const juce::String& text, bool selected)
    {
        g.setColour (selected ? IlanaTheme::accent().withAlpha (0.28f) : juce::Colours::white.withAlpha (0.045f));
        g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
        g.setColour (selected ? IlanaTheme::accent() : IlanaTheme::Ui::line.brighter (0.35f));
        g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);
        g.setColour (selected ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.setFont (tagChipFont());
        g.drawText (text, box.toNearestInt(), juce::Justification::centred);
    }

    void rebuildSidebar()
    {
        static const juce::StringArray order { "Bass", "Lead", "Brass", "Wind", "Pluck", "Pad", "Keys", "Chords", "Arp",
                                               "Drone", "Drums", "FX", "FX Input", "Generative", "Other" };
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

        auto favourites = 0, users = 0;

        for (int i = 0; i < names.size(); ++i)
        {
            favourites += isFavouriteName (settings, names[i]) ? 1 : 0;
            users += isUserPreset (i) ? 1 : 0;
        }

        std::vector<Sidebar::Entry> entries;
        entries.push_back ({ "", "All", names.size() - repeatCount(), IlanaTheme::accent(), false });
        entries.push_back ({ favouritesKey, "Favourites", favourites, juce::Colour (0xffffd447), false });
        entries.push_back ({ userKey, "User", users, juce::Colours::white.withAlpha (0.6f), false });
        {
            // The last presets loaded from this browser (review 11, S11-8).
            auto recent = 0;
            const auto list = recentNames();
            for (int i = 0; i < names.size(); ++i)
                recent += list.contains (names[i]) ? 1 : 0;
            entries.push_back ({ recentKey, "Recent", recent, juce::Colour (0xff7fd1ff), false });
        }

        auto first = true;

        for (const auto& category : shownCategories)
        {
            entries.push_back ({ category, category, countOf (category),
                                 categoryColour (category), first });
            first = false;
        }

        if (! filterKey.isEmpty() && filterKey != favouritesKey && filterKey != userKey && filterKey != recentKey && ! shownCategories.contains (filterKey))
            filterKey = {};

        sidebar.entries = std::move (entries);
        sidebar.selected = filterKey;
        sidebar.repaint();
    }

    int countOf (const juce::String& category) const
    {
        auto count = 0;

        for (int i = 0; i < categories.size(); ++i)
            count += categories[i] == category && repeats[i] < 0 ? 1 : 0;

        return count;
    }

    // The voices a cartridge repeats, left out of the counts.
    int repeatCount() const
    {
        auto count = 0;

        for (auto original : repeats)
            count += original >= 0 ? 1 : 0;

        return count;
    }

    // A repeated voice is listed only under its own bank's chip, or when
    // its first copy is filtered away.
    bool isHiddenRepeat (int i) const
    {
        return bankFilter.isEmpty() && repeatsACandidate (i);
    }

    // A voice whose first copy is among the candidates: the DX7 chip never
    // counts it, with or without a bank picked (review 8, S8-27 / V8-25: the
    // chip read 270 in one view and 288 in the other).
    bool repeatsACandidate (int i) const
    {
        const auto original = repeats[i];
        return original >= 0 && (size_t) original < candidateMask.size() && candidateMask[(size_t) original];
    }

    // The preset's macro names. The loaded one reads them from the patch
    // (so an automatic mapping shows too); the others from the factory
    // table. Cached until the list is rebuilt.
    juce::StringArray macroListFor (int presetIndex, bool loaded)
    {
        if (loaded)
        {
            juce::StringArray live;

            for (int macro = 0; macro < Mod::numMacros; ++macro)
                live.add (processorRef.apvts.state.getProperty ("macroName" + juce::String (macro + 1)).toString());

            return live;
        }

        if (isUserPreset (presetIndex))
            return {};

        if (const auto found = macroTextCache.find (presetIndex); found != macroTextCache.end())
            return found->second;

        const auto names4 = processorRef.getFactoryMacroNames (presetIndex);
        macroTextCache[presetIndex] = names4;
        return names4;
    }

    const IlanaSynthAudioProcessor::PresetInfo& infoFor (int presetIndex)
    {
        if (const auto found = infoCache.find (presetIndex); found != infoCache.end())
            return found->second;

        return infoCache[presetIndex] = processorRef.getPresetInfo (presetIndex);
    }

    int getNumRows() override { return filtered.size(); }

    // Where a row's tag chips go: after the name (and its bank), at most
    // three, as many as fit before the category pill.
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> rowTagBoxes (int presetIndex, int width, int height) const
    {
        std::vector<std::pair<juce::Rectangle<float>, juce::String>> boxes;

        if (! juce::isPositiveAndBelow (presetIndex, names.size()))
            return boxes;

        const auto layout = rowLayout (presetIndex, width);
        const auto font = tagChipFont();
        auto x = (float) layout.tagsLeft;

        for (const auto& tag : tagLists[(size_t) presetIndex])
        {
            if (boxes.size() >= 3)
                break;

            const auto w = (float) juce::GlyphArrangement::getStringWidthInt (font, tag) + 16.0f;

            if (x + w > (float) layout.tagsRight)
                break;

            boxes.push_back ({ { x, (float) height * 0.5f - 9.5f, w, 19.0f }, tag });
            x += w + 4.0f;
        }

        return boxes;
    }

    struct RowLayout
    {
        int nameRight = 0, bankLeft = 0, tagsLeft = 0, tagsRight = 0;
        juce::Rectangle<int> author; // empty unless the list is wide (review 9, S9-23)
        juce::Rectangle<float> pill;
        juce::String pillText;
    };

    // A DX7 voice's bank beside its name: only while the name is shared by
    // voices of several banks, and no bank chip is on.
    bool showsBankText (int presetIndex) const
    {
        return banks[presetIndex].isNotEmpty() && bankFilter.isEmpty()
               && sharedNames.contains (shownNames[presetIndex].toLowerCase());
    }

    RowLayout rowLayout (int presetIndex, int width) const
    {
        RowLayout layout;
        const auto isUser = isUserPreset (presetIndex);
        auto pillText = categories[presetIndex].toUpperCase();

        if (isUser)
            pillText = pillText.isEmpty() || pillText == "USER" ? juce::String ("USER")
                                                                : juce::String (juce::CharPointer_UTF8 ("USER \xc2\xb7 ")) + pillText;

        const juce::Font pillFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        const auto pillWidth = pillText.isEmpty() ? 0.0f : (float) juce::GlyphArrangement::getStringWidthInt (pillFont, pillText) + 16.0f;
        layout.pill = { (float) width - pillWidth - 10.0f, 0.0f, pillWidth, 16.0f };
        layout.pillText = pillText;

        const juce::Font nameFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        const auto nameWidth = juce::GlyphArrangement::getStringWidthInt (nameFont, shownNames[presetIndex]);
        // Names share one column so the tags line up (long names push them).
        const auto column = docked ? 300 : 230;
        layout.nameRight = juce::jmin (32 + nameWidth, width - (int) pillWidth - 30);
        layout.bankLeft = layout.nameRight + 8;
        auto after = layout.nameRight;

        // The bank only where the name alone is ambiguous (the same voice in
        // two cartridges); otherwise it is in the tooltip (review 9, S9-24).
        if (showsBankText (presetIndex))
        {
            const juce::Font bankFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            after = layout.bankLeft + juce::GlyphArrangement::getStringWidthInt (bankFont, banks[presetIndex]);
        }

        layout.tagsLeft = juce::jmax (after + 14, column);
        layout.tagsRight = (int) layout.pill.getX() - 10;

        // Who made it, in a column of its own while the list is wide.
        if (width >= 760)
        {
            layout.author = juce::Rectangle<int> (layout.tagsRight - 96, 0, 96, 28);
            layout.tagsRight -= 104;
        }

        return layout;
    }

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

        const auto layout = rowLayout (presetIndex, width);

        g.setColour (juce::Colours::white.withAlpha (selected || isCurrent ? 0.97f : 0.8f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, isCurrent));
        g.drawText (shownNames[presetIndex], juce::Rectangle<int> (32, 0, layout.nameRight - 32, height), juce::Justification::centredLeft, true);

        // A DX7 voice's bank, dim beside its name (in text3, not the DX7
        // red: review 7), and not at all while its bank chip is on.
        if (showsBankText (presetIndex))
        {
            const auto shared = sharedNames.contains (shownNames[presetIndex].toLowerCase());
            g.setColour (selected || shared ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (banks[presetIndex], juce::Rectangle<int> (layout.bankLeft, 0, 80, height), juce::Justification::centredLeft);
        }

        // The tag chips only on the row you point at, the selected and the loaded
        // one, or while a tag filters the list: the list rests on names, a star
        // and the category pill (review 12, S12-6).
        if (selected || isCurrent || row == hoveredRow || ! selectedTags.isEmpty())
            for (const auto& [box, tag] : rowTagBoxes (presetIndex, width, height))
                paintTagChip (g, box, tag, selectedTags.contains (tag, true));

        if (! layout.author.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText (infoFor (presetIndex).author, layout.author.withHeight (height), juce::Justification::centredRight, true);
        }

        if (layout.pillText.isEmpty())
            return;

        const auto colour = isUserPreset (presetIndex) && categories[presetIndex] == "User" ? IlanaTheme::Ui::text2
                                                                                           : categoryColour (categories[presetIndex]);
        const auto pill = layout.pill.withY ((float) height * 0.5f - 8.0f);
        g.setColour (colour.withAlpha (selected ? 0.30f : 0.16f));
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (colour.withAlpha (selected ? 1.0f : 0.85f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (layout.pillText, pill.toNearestInt(), juce::Justification::centred);
    }

    // Hovering a row names its macros (and its author's comment).
    juce::String getTooltipForRow (int row) override
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return {};

        const auto presetIndex = filtered[row];
        auto macros = macroListFor (presetIndex, names[presetIndex] == processorRef.getCurrentPresetName());
        macros.removeEmptyStrings();
        juce::String text = shownNames[presetIndex];

        if (! macros.isEmpty())
            text << "\nMacros: " << macros.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")));

        if (banks[presetIndex].isNotEmpty())
            text << "\nDX7 bank " << banks[presetIndex];

        if (! alsoIn[(size_t) presetIndex].isEmpty())
            text << "\nAlso in " << alsoIn[(size_t) presetIndex].joinIntoString (", ");

        const auto& info = infoFor (presetIndex);

        if (info.comment.isNotEmpty())
            text << "\n" << info.comment;
        else if (info.author.isNotEmpty() && isUserPreset (presetIndex))
            text << "\nby " << info.author;

        return text;
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (suppressLoad || ! juce::isPositiveAndBelow (lastRowSelected, filtered.size()))
            return;

        // A click on a row's star or on one of its tag chips doesn't load:
        // the selection goes back to the preset that is loaded.
        if (juce::ModifierKeys::currentModifiers.isLeftButtonDown())
        {
            const auto mouse = list.getMouseXYRelative();

            if (mouse.x < 30 || tagAt (lastRowSelected, mouse.x).isNotEmpty())
            {
                juce::Component::SafePointer<PresetPanel> safeThis (this);
                juce::MessageManager::callAsync ([safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->showCurrentPreset();
                });
                return;
            }
        }

        loadedBySelection = true;
        loadRow (lastRowSelected, false);

        if (autoPlayButton.getToggleState())
        {
            const auto name = names[filtered[lastRowSelected]];
            juce::Timer::callAfterDelay (80, [safeThis = juce::Component::SafePointer<PresetPanel> (this), name]
            {
                if (safeThis != nullptr && safeThis->processorRef.getCurrentPresetName() == name)
                    safeThis->startAudition();
            });
        }
    }

    juce::String tagAt (int row, int x) const
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return {};

        for (const auto& [box, tag] : rowTagBoxes (filtered[row], list.getVisibleRowWidth(), list.getRowHeight()))
            if (box.getX() <= (float) x && (float) x < box.getRight())
                return tag;

        return {};
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

        // A tag chip on the row filters by that tag.
        if (const auto tag = tagAt (row, event.x); tag.isNotEmpty())
        {
            chipClicked ("tag:" + tag);
            return;
        }

        if (! justLoaded)
            loadRow (row, false);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override
    {
        // Docked, a double-click only loads: the panel is there to stay.
        if (event.x >= 30 && tagAt (row, event.x).isEmpty())
            loadRow (row, ! docked);
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

        if (key == juce::KeyPress::spaceKey && origin == &list)
        {
            startAudition();
            return true;
        }

        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }

        return false;
    }

    void loadRow (int row, bool closeAfter)
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
        {
            if (closeAfter)
                close();

            return;
        }

        stopAudition();
        selectedPreset = filtered[row];
        updateDeleteButton();
        noteRecent (names[selectedPreset]);

        if (onLoad != nullptr)
            onLoad (selectedPreset, closeAfter);

        list.repaint();
        repaint (detailsArea);
    }

    // The presets loaded from the browser, newest first (kept in the settings).
    juce::StringArray recentNames() const
    {
        return settings != nullptr ? juce::StringArray::fromLines (settings->getValue ("presetRecent")) : juce::StringArray();
    }

    void noteRecent (const juce::String& name)
    {
        if (settings == nullptr || name.isEmpty())
            return;

        auto list = recentNames();
        list.removeString (name);
        list.insert (0, name);
        while (list.size() > 24)
            list.remove (list.size() - 1);
        settings->setValue ("presetRecent", list.joinIntoString ("\n"));
        settings->saveIfNeeded();
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

        selectedPreset = current;
        updateDeleteButton();
        shownCurrentName = processorRef.getCurrentPresetName();
        repaint (detailsArea);
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
        const auto files = processorRef.getUserPresetFiles();
        const auto file = files[presetIndex - factoryCount];

        if (! file.existsAsFile())
            return;

        juce::Component::SafePointer<PresetPanel> safeThis (this);

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                            "Delete preset",
                                            "Move '" + name + "' to the recycle bin?",
                                            "DELETE", "CANCEL", this,
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

    // The category filter (the sidebar), before search, chips and tags.
    bool passesCategory (int i) const
    {
        if (filterKey == favouritesKey)
            return isFavouriteName (settings, names[i]);

        if (filterKey == userKey)
            return isUserPreset (i);

        if (filterKey == recentKey)
            return recentNames().contains (names[i]);

        if (filterKey.isNotEmpty())
            return categories[i] == filterKey;

        return true;
    }

    static juce::StringArray tagsOf (const juce::String& text)
    {
        auto tagList = juce::StringArray::fromTokens (text, ",", "");
        tagList.trim();
        tagList.removeEmptyStrings();
        tagList.removeDuplicates (true);
        return tagList;
    }

    bool matchesSearch (int i, const juce::StringArray& words) const
    {
        // Every word must match the name (as saved or shown), category,
        // tags or bank.
        const auto haystack = (names[i] + " " + shownNames[i] + " " + categories[i] + " " + tags[i] + " " + banks[i]
                               + (banks[i].isNotEmpty() ? " dx7" : ""))
                                  .toLowerCase();

        for (const auto& word : words)
            if (! haystack.contains (word))
                return false;

        return true;
    }

    // The chips for what the sidebar and search leave: DX7 (when any DX7
    // voice is among them) and its banks, then the tags by how many of the
    // presets carry them.
    void rebuildChips (const juce::Array<int>& candidates)
    {
        std::vector<ChipRow::Entry> entries;
        auto dx7Count = 0;
        juce::StringArray seenBanks;

        for (auto i : candidates)
            if (banks[i].isNotEmpty())
            {
                dx7Count += repeatsACandidate (i) ? 0 : 1;

                if (! seenBanks.contains (banks[i]))
                    seenBanks.add (banks[i]);
            }

        if (dx7Count == 0)
            dx7Only = false;

        if (! seenBanks.contains (bankFilter))
            bankFilter = {};

        if (dx7Count > 0)
        {
            entries.push_back ({ "pack:dx7", "DX7  " + juce::String (dx7Count), dx7Only, ChipRow::source, dx7Colour() });

            if (dx7Only)
                for (const auto& bank : seenBanks)
                    entries.push_back ({ "bank:" + bank, bank, bank == bankFilter, ChipRow::bank, dx7Colour() });
        }

        std::map<juce::String, int> counts;
        std::map<juce::String, juce::String> spelling;

        for (auto i : candidates)
            if (! dx7Only || banks[i].isNotEmpty())
                for (const auto& tag : tagLists[(size_t) i])
                {
                    const auto key = tag.toLowerCase();

                    if (key == "dx7") // the DX7 chip already
                        continue;

                    ++counts[key];

                    if (spelling.find (key) == spelling.end())
                        spelling[key] = tag;
                }

        for (int i = selectedTags.size(); --i >= 0;)
            if (counts.find (selectedTags[i].toLowerCase()) == counts.end())
                selectedTags.remove (i);

        std::vector<std::pair<juce::String, int>> ranked;

        for (const auto& [key, count] : counts)
            ranked.push_back ({ spelling[key], count });

        // A tag on most of the list ("Warm", "Sustained") filters little, so
        // those go after the ones that narrow it down.
        auto shownCount = 0;

        for (auto i : candidates)
            shownCount += ! dx7Only || banks[i].isNotEmpty() ? 1 : 0;

        const auto broad = [shownCount] (int count) { return count * 4 > shownCount; };

        std::stable_sort (ranked.begin(), ranked.end(), [this, &broad] (const auto& a, const auto& b)
        {
            const auto selectedA = selectedTags.contains (a.first, true), selectedB = selectedTags.contains (b.first, true);

            if (selectedA != selectedB)
                return selectedA;

            if (broad (a.second) != broad (b.second))
                return ! broad (a.second);

            if (a.second != b.second)
                return a.second > b.second;

            return a.first.compareNatural (b.first, false) < 0;
        });

        for (const auto& [tag, count] : ranked)
            entries.push_back ({ "tag:" + tag, tag, selectedTags.contains (tag, true), ChipRow::tag, {} });

        const auto oldHeight = chips.isVisible() ? chips.getHeight() : -1;
        chips.entries = std::move (entries);
        chips.setVisible (! chips.entries.empty());
        chips.repaint();

        if (oldHeight != (chips.isVisible() ? chips.getPreferredHeight (chips.getWidth()) : -1))
            resized();
    }

    void chipClicked (const juce::String& key)
    {
        if (key == ChipRow::moreKey)
        {
            chips.expanded = ! chips.expanded;
            resized();
            chips.repaint();
            return;
        }

        if (key == ChipRow::bankKey)
        {
            showBankMenu();
            return;
        }

        if (key == "pack:dx7")
        {
            dx7Only = ! dx7Only;
            bankFilter = {};
        }
        else if (key.startsWith ("bank:"))
        {
            const auto bank = key.fromFirstOccurrenceOf ("bank:", false, false);
            bankFilter = bankFilter == bank ? juce::String() : bank;
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

    // BANK: the DX7 banks in a menu, the one picked ticked (picking it again,
    // or ALL BANKS, shows every bank).
    void showBankMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "All banks", true, bankFilter.isEmpty());
        menu.addSeparator();
        juce::StringArray keys;

        for (const auto& entry : chips.entries)
            if (entry.kind == ChipRow::bank)
            {
                keys.add (entry.key);
                menu.addItem (1 + keys.size(), entry.label, true, entry.selected);
            }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chips),
                            [safeThis = juce::Component::SafePointer<PresetPanel> (this), keys] (int result)
                            {
                                if (safeThis == nullptr || result <= 0)
                                    return;

                                if (result == 1)
                                {
                                    safeThis->bankFilter = {};
                                    safeThis->rebuild();
                                }
                                else
                                    safeThis->chipClicked (keys[result - 2]);
                            });
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
        static const juce::StringArray categoryOrder { "Init", "Bass", "Lead", "Brass", "Wind", "Pluck", "Pad", "Keys", "Chords", "Arp",
                                                       "Drone", "Drums", "FX", "FX Input", "Generative", "Other", "User" };
        const auto rank = [] (const juce::String& category)
        {
            const auto index = categoryOrder.indexOf (category);
            return index >= 0 ? index : categoryOrder.size();
        };

        std::vector<int> order (filtered.begin(), filtered.end());
        const auto recentList = sortMode == sortRecentFirst ? recentNames() : juce::StringArray();

        std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
        {
            if (sortMode == sortByCategory && categories[a] != categories[b])
            {
                const auto rankA = rank (categories[a]), rankB = rank (categories[b]);
                return rankA != rankB ? rankA < rankB : categories[a].compareNatural (categories[b], false) < 0;
            }

            if (sortMode == sortRecentFirst)
            {
                const auto rankA = recentList.indexOf (names[a]), rankB = recentList.indexOf (names[b]);

                if (rankA != rankB)
                    return rankA >= 0 && (rankB < 0 || rankA < rankB);
            }

            if (sortMode == sortFavouritesFirst)
            {
                const auto favA = isFavouriteName (settings, names[a]), favB = isFavouriteName (settings, names[b]);

                if (favA != favB)
                    return favA;
            }

            // By the name shown; same names (a DX7 voice in two banks) by bank.
            const auto byName = shownNames[a].compareNatural (shownNames[b], false);
            return byName != 0 ? byName < 0 : banks[a].compareNatural (banks[b], false) < 0;
        });

        filtered.clearQuick();

        for (auto index : order)
            filtered.add (index);
    }

    void updateSortButton()
    {
        static const char* labels[] { "SORT: NAME", "SORT: CATEGORY", "SORT: FAVOURITES", "SORT: RECENT" };
        sortButton.setButtonText (juce::String (labels[sortMode]) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
    }

    void showSortMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Name (A-Z)", true, sortMode == sortByName);
        menu.addItem (2, "Category, then name", true, sortMode == sortByCategory);
        menu.addItem (3, "Favourites first", true, sortMode == sortFavouritesFirst);
        menu.addItem (4, "Recently loaded first", true, sortMode == sortRecentFirst);

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

        const auto query = search.getText().trim().toLowerCase();
        const auto words = juce::StringArray::fromTokens (query, " ", "");
        juce::Array<int> candidates;

        candidateMask.assign ((size_t) names.size(), false);

        for (int i = 0; i < names.size(); ++i)
            if (passesCategory (i) && matchesSearch (i, words))
            {
                candidates.add (i);
                candidateMask[(size_t) i] = true;
            }

        rebuildChips (candidates);

        for (auto i : candidates)
        {
            if (dx7Only && banks[i].isEmpty())
                continue;

            if (isHiddenRepeat (i))
                continue;

            if (bankFilter.isNotEmpty() && banks[i] != bankFilter)
                continue;

            auto hasTags = true;

            for (const auto& tag : selectedTags)
                hasTags = hasTags && tagLists[(size_t) i].contains (tag, true);

            if (hasTags)
                filtered.add (i);
        }

        sortFiltered();

        // Names listed more than once (different DX7 voices with one name,
        // "Oboe" in ROM2A and ROM4A): their rows show the bank brighter, so
        // they read apart (review 8, V8-25).
        {
            std::map<juce::String, int> seen;
            for (auto i : filtered)
                ++seen[shownNames[i].toLowerCase()];
            sharedNames.clear();
            for (const auto& [name, count] : seen)
                if (count > 1)
                    sharedNames.add (name);
        }

        {
            const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
            list.updateContent();
            list.deselectAllRows();

            const auto row = filtered.indexOf (selectedPreset >= 0 ? selectedPreset
                                                                   : names.indexOf (processorRef.getCurrentPresetName()));
            if (row >= 0)
            {
                list.selectRow (row);
                list.scrollToEnsureRowIsOnscreen (row);
            }
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

                if (onClosed != nullptr)
                    onClosed();

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
            repaint (detailsArea);
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

    // DELETE works on your own presets; the tooltip says why it is off.
    void updateDeleteButton()
    {
        const auto canDelete = juce::isPositiveAndBelow (selectedPreset, names.size()) && isUserPreset (selectedPreset);
        deleteButton.setEnabled (canDelete);
        deleteButton.setTooltip (canDelete ? "Move '" + names[selectedPreset] + "' to the recycle bin"
                                           : juce::String ("Delete\nFactory presets can't be deleted: select one of your own (User) to delete it."));
        repaint (detailsArea);
    }

    void deleteSelected()
    {
        if (juce::isPositiveAndBelow (selectedPreset, names.size()) && isUserPreset (selectedPreset))
            deletePresetByIndex (selectedPreset);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile* settings = nullptr;
    bool isUserPreset (int index) const { return index >= factoryCount; }

    juce::StringArray names, shownNames, categories, tags, banks;
    std::vector<juce::StringArray> tagLists, alsoIn;
    juce::Array<int> repeats;
    std::vector<bool> candidateMask;
    juce::StringArray sharedNames;
    const int factoryCount = processorRef.getFactoryPresetNames().size();
    std::map<int, juce::StringArray> macroTextCache;
    std::map<int, IlanaSynthAudioProcessor::PresetInfo> infoCache;
    juce::Array<int> filtered;
    juce::TextEditor search;
    Sidebar sidebar;
    juce::ListBox list { "presets", this };
    juce::Rectangle<int> listFrame, detailsArea;
    juce::TextButton surpriseButton { "RANDOM PRESET" };
    juce::TextButton saveAsButton { "SAVE AS" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton folderButton { "FOLDER" };
    juce::TextButton importButton { "IMPORT .SYX" };
    juce::TextButton sortButton;
    ChipRow chips;
    int sortMode = sortByName;
    bool dx7Only = false;
    juce::String bankFilter;
    juce::StringArray selectedTags;
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
