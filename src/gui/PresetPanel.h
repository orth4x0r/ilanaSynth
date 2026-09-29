#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
// Clicking the star on a row makes it a favourite.
class PresetPanel : public juce::Component,
                    private juce::ListBoxModel,
                    private juce::KeyListener,
                    private juce::Timer
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

        saveAsButton.onClick = [this] { saveCurrentAs(); };
        deleteButton.onClick = [this] { deleteSelected(); };
        folderButton.onClick = [this] { processorRef.getUserPresetDirectory().revealToUser(); };
        surpriseButton.onClick = [this] { loadRandom(); };
        surpriseButton.setTooltip ("Load a random preset from the list shown");

        for (auto* button : { &surpriseButton, &saveAsButton, &deleteButton, &folderButton })
            addAndMakeVisible (button);

        surpriseButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.35f));
    }

    ~PresetPanel() override
    {
        stopWatchingClicks();
    }

    // Called with the preset index, and whether the browser should close.
    std::function<void (int, bool)> onLoad;
    std::function<void()> onFavouriteChanged;

    // The preset name box: clicks on it are left to it (it toggles us).
    void setAnchor (juce::Component* anchorComponent) { anchor = anchorComponent; }

    void refresh()
    {
        names = processorRef.getAllPresetNames();
        categories = processorRef.getAllPresetCategories();
        tags = processorRef.getAllPresetTags();
        rebuildSidebar();
        rebuild();
    }

    void open()
    {
        refresh();

        if (! isVisible())
        {
            appear = 0.0f;
            setAlpha (0.0f);
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
        if (! isVisible())
            return;

        closing = true;
        stopWatchingClicks();
        startTimerHz (60);
    }

    bool isOpen() const { return isVisible() && ! closing; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        // A deep shadow so the browser floats over the page.
        for (int i = 0; i < 4; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f + (float) i * 2.0f).reduced ((float) (3 - i)), 9.0f);
        }

        IlanaTheme::paintCard (g, bounds, 9.0f, IlanaTheme::accent().withAlpha (0.45f));

        auto header = getLocalBounds().reduced (14, 0).removeFromTop (34);
        g.setColour (IlanaTheme::accent());
        g.fillEllipse ((float) header.getX(), (float) header.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("PRESETS", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Up/Down browse  \xc2\xb7  Enter keep  \xc2\xb7  Esc close")),
                    header, juce::Justification::centred);
        g.drawText (juce::String (filtered.size()) + " of " + juce::String (names.size()),
                    header, juce::Justification::centredRight);

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
            g.drawText (filterKey == favouritesKey && search.isEmpty()
                            ? "No favourites yet. Click the star on a preset to add it."
                            : "No presets match.",
                        list.getBounds(), juce::Justification::centred);
        }
    }

    void lookAndFeelChanged() override
    {
        search.setColour (juce::TextEditor::highlightColourId, IlanaTheme::accent().withAlpha (0.35f));
        surpriseButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.35f));
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        area.removeFromTop (24);

        auto footer = area.removeFromBottom (28);
        area.removeFromBottom (8);

        auto left = area.removeFromLeft (150);
        area.removeFromLeft (10);
        sidebar.setBounds (left.reduced (1));

        search.setBounds (area.removeFromTop (30).reduced (1));
        area.removeFromTop (8);
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
            close();
            return true;
        }

        return false;
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
        if (category == "Drums") return juce::Colour (0xffff4f6d);
        if (category == "Generative") return juce::Colour (0xff4fc3ff);
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
                                               "Drone", "Drums", "FX", "Generative", "Other" };
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
        entries.push_back ({ "", "All", names.size(), IlanaTheme::accent(), false });
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

        if (favourite)
            drawStar (g, { 17.0f, (float) height * 0.5f }, 5.5f, juce::Colour (0xffffd447), true);
        else if (row == hoveredRow)
            drawStar (g, { 17.0f, (float) height * 0.5f }, 5.5f, juce::Colours::white.withAlpha (0.35f), false);

        g.setColour (juce::Colours::white.withAlpha (selected || isCurrent ? 0.97f : 0.8f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, isCurrent));
        g.drawText (name, juce::Rectangle<int> (32, 0, width - 170, height), juce::Justification::centredLeft);

        const auto isUser = isUserPreset (presetIndex);
        const auto category = categories[presetIndex];
        const auto colour = categoryColour (category);
        auto tagText = category.toUpperCase();

        if (isUser)
            tagText = tagText.isEmpty() ? juce::String ("USER") : juce::String (juce::CharPointer_UTF8 ("USER \xc2\xb7 ")) + tagText;

        if (tagText.isEmpty())
            return;

        const auto tagWidth = juce::jmax (40.0f, (float) tagText.length() * 6.4f + 14.0f);
        const auto tagBounds = juce::Rectangle<float> ((float) width - tagWidth - 10.0f,
                                                       (float) height * 0.5f - 8.0f, tagWidth, 16.0f);

        g.setColour (colour.withAlpha (selected ? 0.30f : 0.16f));
        g.fillRoundedRectangle (tagBounds, 8.0f);
        g.setColour (colour.withAlpha (selected ? 1.0f : 0.85f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (tagText, tagBounds.toNearestInt(), juce::Justification::centred);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (suppressLoad || ! juce::isPositiveAndBelow (lastRowSelected, filtered.size()))
            return;

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
            if (filtered.isEmpty())
                return true;

            const auto current = list.getSelectedRow();
            const auto next = current < 0 ? 0
                                          : juce::jlimit (0, filtered.size() - 1,
                                                          current + (key == juce::KeyPress::downKey ? 1 : -1));
            list.selectRow (next);
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
                                            "Delete Preset",
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

    void rebuild()
    {
        filtered.clear();

        const auto query = search.getText().trim().toLowerCase();
        const auto words = juce::StringArray::fromTokens (query, " ", "");

        for (int i = 0; i < names.size(); ++i)
        {
            if (filterKey == favouritesKey)
            {
                if (! isFavouriteName (settings, names[i]))
                    continue;
            }
            else if (filterKey == userKey)
            {
                if (! isUserPreset (i))
                    continue;
            }
            else if (filterKey.isNotEmpty() && categories[i] != filterKey)
            {
                continue;
            }

            // Every word must match the name, category or tags.
            const auto haystack = (names[i] + " " + categories[i] + " " + tags[i]).toLowerCase();
            auto matches = true;

            for (const auto& word : words)
                matches = matches && haystack.contains (word);

            if (matches)
                filtered.add (i);
        }

        {
            const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
            list.updateContent();
            list.deselectAllRows();

            const auto row = filtered.indexOf (selectedPreset >= 0 ? selectedPreset
                                                                   : names.indexOf (processorRef.getCurrentPresetName()));
            if (row >= 0)
                list.selectRow (row);
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
            appear = IlanaAnim::approach (appear, target, 0.2f);
            const auto eased = IlanaAnim::easeOutCubic (appear);

            setAlpha (eased);
            setTransform (juce::AffineTransform::translation (0.0f, (1.0f - eased) * -10.0f));
        }
        else if (appear != target)
        {
            appear = target;
            setAlpha (appear);
            setTransform ({});

            if (closing)
            {
                closing = false;
                setVisible (false);
                stopTimer();
                return;
            }
        }

        const auto relative = list.getMouseXYRelative();
        const auto hovered = list.isMouseOver (true)
                                 ? list.getRowContainingPosition (relative.x, relative.y)
                                 : -1;

        if (hovered != hoveredRow)
        {
            hoveredRow = hovered;
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

    void saveCurrentAs()
    {
        juce::Component::SafePointer<PresetPanel> safeThis (this);
        showSaveDialog (processorRef, [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->refresh();
        });
    }

public:
    // Name, category and tags, then writes into the user preset folder.
    // Shared by the browser's SAVE AS and the header's save button.
    static void showSaveDialog (IlanaSynthAudioProcessor& processor, std::function<void()> onSaved)
    {
        auto* window = new juce::AlertWindow ("Save Preset", "Saved to your user preset folder.",
                                              juce::AlertWindow::NoIcon);

        auto currentName = processor.getCurrentPresetName();

        if (currentName.isEmpty() || currentName == "Init")
            currentName = "My Preset";

        auto choices = IlanaSynthAudioProcessor::getPresetCategoryChoices();
        auto currentCategory = choices.indexOf (processor.getPresetCategory());

        if (currentCategory < 0)
            currentCategory = choices.size() - 1;

        window->addTextEditor ("name", currentName, "Name");
        window->addComboBox ("category", choices, "Category");
        window->getComboBoxComponent ("category")->setSelectedItemIndex (currentCategory, juce::dontSendNotification);
        window->addTextEditor ("tags", processor.getPresetTags(), "Tags (comma separated)");
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        auto* processorPointer = &processor;

        window->enterModalState (true, juce::ModalCallbackFunction::create (
            [processorPointer, window, onSaved] (int result)
            {
                std::unique_ptr<juce::AlertWindow> owner (window);

                if (result != 1)
                    return;

                auto name = window->getTextEditorContents ("name").trim()
                                .retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");

                if (name.isEmpty())
                    name = "Preset";

                const auto category = window->getComboBoxComponent ("category")->getText();
                auto tagList = juce::StringArray::fromTokens (window->getTextEditorContents ("tags"), ",", "");
                tagList.trim();
                tagList.removeEmptyStrings();

                // Names stay unique across factory and user presets:
                // favourites and the browser find presets by name.
                const auto factoryNames = processorPointer->getFactoryPresetNames();
                const auto directory = processorPointer->getUserPresetDirectory();
                directory.createDirectory();
                auto finalName = name;
                auto suffix = 1;

                while ((directory.getChildFile (finalName + ".ilanapreset").existsAsFile()
                        || factoryNames.contains (finalName, true))
                       && suffix < 1000)
                    finalName = name + " " + juce::String (++suffix);

                processorPointer->setPresetMeta (category, tagList.joinIntoString (", "));
                processorPointer->savePresetToFile (directory.getChildFile (finalName + ".ilanapreset"));

                if (onSaved != nullptr)
                    onSaved();
            }), true);
    }

private:
    void deleteSelected()
    {
        if (juce::isPositiveAndBelow (selectedPreset, names.size()) && isUserPreset (selectedPreset))
            deletePresetByIndex (selectedPreset);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::PropertiesFile* settings = nullptr;
    bool isUserPreset (int index) const { return index >= factoryCount; }

    juce::StringArray names, categories, tags;
    const int factoryCount = processorRef.getFactoryPresetNames().size();
    juce::Array<int> filtered;
    juce::TextEditor search;
    Sidebar sidebar;
    juce::ListBox list { "presets", this };
    juce::TextButton surpriseButton { "SURPRISE ME" };
    juce::TextButton saveAsButton { "SAVE AS" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton folderButton { "FOLDER" };
    juce::Component::SafePointer<juce::Component> anchor;
    juce::Component::SafePointer<juce::Component> watched;
    juce::String filterKey;
    juce::String shownCurrentName;
    int selectedPreset = -1;
    int hoveredRow = -1;
    float appear = 0.0f;
    bool closing = false;
    bool suppressLoad = false;
    bool loadedBySelection = false;
};
