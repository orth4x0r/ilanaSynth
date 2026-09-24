#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

class PresetPanel : public juce::Component,
                    private juce::ListBoxModel,
                    private juce::Timer
{
public:
    explicit PresetPanel (IlanaSynthAudioProcessor& processor, juce::PropertiesFile* settingsIn)
        : processorRef (processor),
          settings (settingsIn)
    {
        search.setTextToShowWhenEmpty ("Search presets...", juce::Colours::white.withAlpha (0.35f));
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::textColourId, juce::Colours::white.withAlpha (0.85f));
        search.setColour (juce::TextEditor::highlightColourId, IlanaTheme::accent().withAlpha (0.35f));
        search.onTextChange = [this] { rebuild(); };
        addAndMakeVisible (search);

        const juce::StringArray chipNames { "ALL", "BASS", "LEAD", "PLUCK", "PAD", "KEYS", "ARP", "DRONE", "FX", "USER" };

        for (int i = 0; i < chipNames.size(); ++i)
        {
            auto button = std::make_unique<juce::TextButton> (chipNames[i]);
            button->setClickingTogglesState (true);
            button->setRadioGroupId (0x1001);
            button->setColour (juce::TextButton::buttonOnColourId, IlanaTheme::accent().withAlpha (0.85f));
            button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d1d22));
            button->onClick = [this, index = i]
            {
                categoryFilter = index;
                rebuild();
            };
            addAndMakeVisible (*button);
            chips.push_back (std::move (button));
        }

        chips[0]->setToggleState (true, juce::dontSendNotification);

        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        list.setRowHeight (24);
        list.setModel (this);
        addAndMakeVisible (list);

        saveAsButton.onClick = [this] { saveCurrentAs(); };
        deleteButton.onClick = [this] { deleteSelected(); };
        folderButton.onClick = [this] { processorRef.getUserPresetDirectory().revealToUser(); };

        addAndMakeVisible (saveAsButton);
        addAndMakeVisible (deleteButton);
        addAndMakeVisible (folderButton);

        rebuild();
    }

    std::function<void (int)> onLoad;
    std::function<void()> onFavouriteChanged;

    void refresh()
    {
        names = processorRef.getAllPresetNames();
        categories = processorRef.getAllPresetCategories();
        tags = processorRef.getAllPresetTags();
        rebuild();
    }

    void open()
    {
        refresh();

        if (! isVisible())
        {
            appear = 0.0f;
            setVisible (true);
            toFront (false);
        }

        closing = false;
        startTimerHz (60);
    }

    void close()
    {
        if (! isVisible())
            return;

        closing = true;
        startTimerHz (60);
    }

    bool isOpen() const { return isVisible() && ! closing; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 7.0f);

        IlanaTheme::paintCard (g, bounds, 7.0f, IlanaTheme::accent().withAlpha (0.4f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (IlanaTheme::font (12.5f, true));
        g.drawText ("PRESETS", juce::Rectangle<int> (12, 8, 120, 16), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (IlanaTheme::font (11.5f));
        g.drawText (juce::String (filtered.size()) + " / " + juce::String (names.size()),
                    juce::Rectangle<int> (getWidth() - 100, 8, 88, 16), juce::Justification::centredRight);

        IlanaTheme::paintRecessedPanel (g, search.getBounds().toFloat().expanded (1.0f), 5.0f);

        const auto listBounds = list.getBounds().toFloat();
        IlanaTheme::paintRecessedPanel (g, listBounds.expanded (1.0f), 5.0f);
    }

    void lookAndFeelChanged() override
    {
        for (auto& chip : chips)
            chip->setColour (juce::TextButton::buttonOnColourId, IlanaTheme::accent().withAlpha (0.85f));

        search.setColour (juce::TextEditor::highlightColourId, IlanaTheme::accent().withAlpha (0.35f));
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromTop (16);
        search.setBounds (area.removeFromTop (26).reduced (2, 0));
        area.removeFromTop (8);

        auto chipRowA = area.removeFromTop (22);
        area.removeFromTop (2);
        auto chipRowB = area.removeFromTop (22);
        area.removeFromTop (8);

        const auto halfCount = (int) chips.size() / 2;

        for (int i = 0; i < halfCount; ++i)
            chips[(size_t) i]->setBounds (chipRowA.removeFromLeft (chipRowA.getWidth()
                                                                   / (halfCount - i)).reduced (1));

        for (int i = halfCount; i < (int) chips.size(); ++i)
            chips[(size_t) i]->setBounds (chipRowB.removeFromLeft (chipRowB.getWidth()
                                                                   / ((int) chips.size() - i)).reduced (1));

        auto footer = area.removeFromBottom (24);
        area.removeFromBottom (6);
        list.setBounds (area.reduced (1, 0));

        const auto buttonWidth = footer.getWidth() / 3;
        saveAsButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        deleteButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
        folderButton.setBounds (footer.removeFromLeft (buttonWidth).reduced (2, 0));
    }

private:
    static juce::Colour categoryColour (const juce::String& category)
    {
        if (category == "Bass") return juce::Colour (0xffff6b4a);
        if (category == "Lead") return juce::Colour (0xff5b8cff);
        if (category == "Pluck") return juce::Colour (0xffffd447);
        if (category == "Pad") return juce::Colour (0xffb28aff);
        if (category == "Drone") return juce::Colour (0xff6fe3c1);
        if (category == "FX") return juce::Colour (0xffe3a56f);
        if (category == "Keys" || category == "Chords") return juce::Colour (0xff8fd14f);
        if (category == "Arp") return juce::Colour (0xffff7ac6);
        return IlanaTheme::accent();
    }

    static bool isFavouriteName (juce::PropertiesFile* settingsFile, const juce::String& name)
    {
        return settingsFile != nullptr && settingsFile->getValue ("fav_" + name, "0") == "1";
    }

    void drawFavStar (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour) const
    {
        juce::Path star;
        star.startNewSubPath (centre.x, centre.y - radius);
        star.lineTo (centre.x + radius * 0.26f, centre.y - radius * 0.26f);
        star.lineTo (centre.x + radius, centre.y);
        star.lineTo (centre.x + radius * 0.26f, centre.y + radius * 0.26f);
        star.lineTo (centre.x, centre.y + radius);
        star.lineTo (centre.x - radius * 0.26f, centre.y + radius * 0.26f);
        star.lineTo (centre.x - radius, centre.y);
        star.lineTo (centre.x - radius * 0.26f, centre.y - radius * 0.26f);
        star.closeSubPath();
        g.setColour (colour);
        g.fillPath (star);
    }

    int getNumRows() override { return filtered.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return;

        const auto presetIndex = filtered[row];
        const auto rowBounds = juce::Rectangle<float> (2.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f);

        if (selected)
        {
            juce::ColourGradient selection (IlanaTheme::accent().withAlpha (0.28f), 0.0f, 0.0f,
                                            IlanaTheme::accent().withAlpha (0.12f), 0.0f, (float) height, false);
            g.setGradientFill (selection);
            g.fillRoundedRectangle (rowBounds, 4.0f);

            g.setColour (IlanaTheme::accent());
            g.fillRoundedRectangle (rowBounds.withWidth (3.0f).reduced (0.0f, 3.0f), 1.5f);
        }
        else if (row == hoveredRow)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (rowBounds, 4.0f);
        }
        else if (row % 2 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.02f));
            g.fillRoundedRectangle (rowBounds, 4.0f);
        }

        const auto name = names[presetIndex];
        const auto favourite = isFavouriteName (settings, name);

        if (favourite)
            drawFavStar (g, { 14.0f, (float) height * 0.5f }, 4.0f, juce::Colour (0xffffd447));

        g.setColour (selected ? juce::Colours::white.withAlpha (0.95f)
                              : juce::Colours::white.withAlpha (0.82f));
        g.setFont (IlanaTheme::font (13.5f));
        g.drawText (name, juce::Rectangle<int> (favourite ? 24 : 12, 0, width - 110, height),
                    juce::Justification::centredLeft);

        const auto isUser = isUserPreset (presetIndex);
        const auto colour = categoryColour (categories[presetIndex]);
        auto tagText = categories[presetIndex].toUpperCase();

        if (isUser && categories[presetIndex] != "User")
            tagText = juce::String (juce::CharPointer_UTF8 ("USER \xc2\xb7 ")) + tagText;
        const auto tagWidth = juce::jmax (34.0f, (float) tagText.length() * 6.0f + 12.0f);
        const auto tagBounds = juce::Rectangle<float> ((float) width - tagWidth - 8.0f,
                                                       (float) height * 0.5f - 7.0f, tagWidth, 14.0f);

        g.setColour (colour.withAlpha (selected ? 0.32f : 0.18f));
        g.fillRoundedRectangle (tagBounds, 7.0f);

        g.setColour (colour.withAlpha (selected ? 1.0f : 0.8f));
        g.setFont (IlanaTheme::font (10.5f, true));
        g.drawText (tagText, tagBounds.toNearestInt(), juce::Justification::centred);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& event) override
    {
        if (! juce::isPositiveAndBelow (row, filtered.size()))
            return;

        const auto presetIndex = filtered[row];

        if (event.mods.isPopupMenu())
        {
            showRowMenu (presetIndex);
            return;
        }

        selectedPreset = presetIndex;
        updateDeleteButton();

        if (onLoad != nullptr)
            onLoad (selectedPreset);
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
                            [safeThis, name, presetIndex] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                if (result == 1)
                                {
                                    safeThis->selectedPreset = presetIndex;
                                    safeThis->updateDeleteButton();

                                    if (safeThis->onLoad != nullptr)
                                        safeThis->onLoad (presetIndex);
                                }
                                else if (result == 2 && safeThis->settings != nullptr)
                                {
                                    const auto was = isFavouriteName (safeThis->settings, name);
                                    safeThis->settings->setValue ("fav_" + name, was ? "0" : "1");
                                    safeThis->settings->saveIfNeeded();

                                    if (safeThis->onFavouriteChanged != nullptr)
                                        safeThis->onFavouriteChanged();

                                    safeThis->list.repaint();
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
                                            "Delete '" + name + "' permanently?",
                                            "Delete", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safeThis, file] (int result)
                                            {
                                                if (result == 1 && safeThis != nullptr)
                                                {
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
        const juce::String categoryQuery (chipCategories[(size_t) juce::jlimit (0, (int) chipCategories.size() - 1, categoryFilter)]);

        for (int i = 0; i < names.size(); ++i)
        {
            if (categoryQuery == "User")
            {
                if (! isUserPreset (i))
                    continue;
            }
            else if (categoryQuery == "Keys")
            {
                if (categories[i] != "Keys" && categories[i] != "Chords")
                    continue;
            }
            else if (categoryQuery.isNotEmpty() && ! categories[i].equalsIgnoreCase (categoryQuery))
            {
                continue;
            }

            if (query.isEmpty()
                || names[i].toLowerCase().contains (query)
                || categories[i].toLowerCase().contains (query)
                || tags[i].toLowerCase().contains (query))
                filtered.add (i);
        }

        list.updateContent();
        list.repaint();
        updateDeleteButton();
    }

    void timerCallback() override
    {
        const auto target = closing ? 0.0f : 1.0f;

        if (std::abs (appear - target) > 0.005f)
        {
            appear = IlanaAnim::approach (appear, target, 0.18f);
            const auto eased = IlanaAnim::easeOutCubic (appear);

            setAlpha (eased);
            setTransform (juce::AffineTransform::translation (0.0f, (1.0f - eased) * -14.0f));
        }
        else if (appear != target)
        {
            appear = target;
            setAlpha (appear);

            if (closing)
            {
                closing = false;
                setTransform ({});
                setVisible (false);
                stopTimer();
                return;
            }

            setTransform ({});
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

        if (! closing && appear >= 0.999f && getTimerInterval() != 20)
            startTimerHz (20);
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

        const auto choices = IlanaSynthAudioProcessor::getPresetCategoryChoices();
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

                const auto directory = processorPointer->getUserPresetDirectory();
                directory.createDirectory();
                auto file = directory.getChildFile (name + ".ilanapreset");
                auto suffix = 1;

                while (file.existsAsFile() && suffix < 100)
                    file = directory.getChildFile (name + " " + juce::String (++suffix) + ".ilanapreset");

                processorPointer->setPresetMeta (category, tagList.joinIntoString (", "));
                processorPointer->savePresetToFile (file);

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
    juce::ListBox list { "presets", this };
    juce::TextButton saveAsButton { "SAVE AS" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton folderButton { "FOLDER" };
    std::vector<std::unique_ptr<juce::TextButton>> chips;
    int categoryFilter = 0;
    static constexpr std::array<const char*, 10> chipCategories {
        "", "Bass", "Lead", "Pluck", "Pad", "Keys", "Arp", "Drone", "FX", "User"
    };
    int selectedPreset = -1;
    int hoveredRow = -1;
    float appear = 1.0f;
    bool closing = false;
};
