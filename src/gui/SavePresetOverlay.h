#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

// Preset names and the files they live in (UI review 4, V3).
namespace PresetFiles
{
// The name as a file can hold it: every character but / \ : * ? " < > |
// and control characters, trimmed of spaces and of trailing dots (Windows
// drops those). removed gets the characters left out, once each.
inline juce::String legalName (const juce::String& typed, juce::String* removed = nullptr)
{
    static const juce::String forbidden ("/\\:*?\"<>|");
    juce::String kept, dropped;

    for (auto pointer = typed.getCharPointer(); ! pointer.isEmpty();)
    {
        const auto c = pointer.getAndAdvance();

        if (c < 0x20 || c == 0x7f || forbidden.containsChar (c))
        {
            if (c >= 0x20 && c != 0x7f && ! dropped.containsChar (c))
                dropped << juce::String::charToString (c);

            continue;
        }

        kept << juce::String::charToString (c);
    }

    const auto trimmed = kept.trim().trimCharactersAtEnd (". ");

    if (removed != nullptr)
        *removed = dropped;

    return trimmed.substring (0, 120);
}

// The user preset called name (in any folder, any case), or File().
inline juce::File findUserPreset (const IlanaSynthAudioProcessor& processor, const juce::String& name)
{
    for (const auto& file : processor.getUserPresetFiles())
        if (file.getFileNameWithoutExtension().equalsIgnoreCase (name))
            return file;

    return {};
}

// The loaded patch's own user preset file, or File() when it is a factory
// preset or was never saved.
inline juce::File loadedUserPreset (const IlanaSynthAudioProcessor& processor)
{
    const auto name = processor.getCurrentPresetName();

    if (name.isEmpty() || processor.getFactoryPresetNames().contains (name, true))
        return {};

    for (const auto& file : processor.getUserPresetFiles())
        if (file.getFileNameWithoutExtension() == name)
            return file;

    return {};
}
} // namespace PresetFiles

// SAVE AS, in the theme: name, category and tags over the whole window.
// A name already used by a user preset asks before overwriting it; a factory
// preset's name can't be taken (favourites and the browser find presets by
// name). Characters a file name can't hold are left out, with a note saying
// which. Return saves, Esc or a click outside cancels.
class SavePresetOverlay : public juce::Component
{
public:
    explicit SavePresetOverlay (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        for (auto* field : { &nameField, &tagsField })
        {
            field->setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::title)));
            field->setColour (juce::TextEditor::backgroundColourId, IlanaTheme::Ui::well);
            field->setColour (juce::TextEditor::outlineColourId, IlanaTheme::Ui::line);
            field->setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
            field->setIndents (8, 6);
            field->onReturnKey = [this] { save(); };
            field->onEscapeKey = [this] { cancel(); };
            addAndMakeVisible (field);
        }

        nameField.onTextChange = [this] { updateNote(); };
        tagsField.setTextToShowWhenEmpty ("Optional, separated by commas", IlanaTheme::Ui::text3.withAlpha (0.7f));

        categoryBox.addItemList (IlanaSynthAudioProcessor::getPresetCategoryChoices(), 1);
        addAndMakeVisible (categoryBox);

        saveButton.onClick = [this] { asking ? finishOverwrite (true) : save(); };
        cancelButton.onClick = [this] { asking ? finishOverwrite (false) : cancel(); };
        addAndMakeVisible (saveButton);
        addAndMakeVisible (cancelButton);

        setWantsKeyboardFocus (true);
        setVisible (false);
    }

    // Called with the saved preset's file once it is written.
    std::function<void (const juce::File&)> onSaved;

    // Opens over the parent, filled in from the loaded patch.
    void show()
    {
        auto name = processorRef.getCurrentPresetName();

        if (name.isEmpty() || name == "Init")
            name = "My Preset";

        const auto choices = IlanaSynthAudioProcessor::getPresetCategoryChoices();
        auto category = choices.indexOf (processorRef.getPresetCategory());

        categoryBox.setSelectedItemIndex (category >= 0 ? category : choices.size() - 1, juce::dontSendNotification);
        tagsField.setText (processorRef.getPresetTags(), false);
        nameField.setText (name, false);
        setAsking (false);
        updateNote();

        if (auto* parent = getParentComponent())
            setBounds (parent->getLocalBounds());

        setVisible (true);
        toFront (true);
        nameField.grabKeyboardFocus();
        nameField.selectAll();
    }

    void cancel()
    {
        setAsking (false);
        setVisible (false);
    }

    bool isShowing() const { return isVisible(); }
    bool isAskingOverwrite() const { return isVisible() && asking; }
    juce::String getNote() const { return note; }
    juce::TextEditor& getNameField() { return nameField; }
    juce::TextEditor& getTagsField() { return tagsField; }
    juce::ComboBox& getCategoryBox() { return categoryBox; }

    // SAVE: writes a new preset, or asks first when the name is taken.
    void save()
    {
        auto name = PresetFiles::legalName (nameField.getText());

        if (name.isEmpty())
        {
            setNote ("Type a name first.", true);
            return;
        }

        if (processorRef.getFactoryPresetNames().contains (name, true))
        {
            setNote ("A factory preset is called '" + name + "'. Pick another name.", true);
            return;
        }

        if (const auto existing = PresetFiles::findUserPreset (processorRef, name); existing.existsAsFile())
        {
            target = existing;
            setAsking (true);
            return;
        }

        const auto directory = processorRef.getUserPresetDirectory();
        directory.createDirectory();
        write (directory.getChildFile (name + ".ilanapreset"));
    }

    // The overwrite question's answer.
    void finishOverwrite (bool overwrite)
    {
        setAsking (false);

        // The file keeps its folder (an imported DX7 bank's, say) and, on a
        // case-blind file system, its spelling.
        if (overwrite && target != juce::File())
            write (target);
        else
            nameField.grabKeyboardFocus();
    }

    void resized() override
    {
        auto area = panelBounds().reduced (22, 18);
        area.removeFromTop (48);

        auto row = [&area] (int height)
        {
            auto r = area.removeFromTop (height);
            area.removeFromTop (8);
            return r;
        };

        nameField.setBounds (row (32).withTrimmedLeft (labelWidth));
        auto second = row (28).withTrimmedLeft (labelWidth);
        categoryBox.setBounds (second.removeFromLeft (150));
        tagsField.setBounds (row (28).withTrimmedLeft (labelWidth));

        auto bottom = panelBounds().reduced (22, 18).removeFromBottom (30);
        saveButton.setBounds (bottom.removeFromRight (120));
        bottom.removeFromRight (8);
        cancelButton.setBounds (bottom.removeFromRight (90));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));

        const auto panel = panelBounds().toFloat();
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (panel, 10.0f);
        g.setColour (IlanaTheme::accent().withAlpha (0.6f));
        g.drawRoundedRectangle (panel.reduced (0.5f), 10.0f, 1.5f);

        auto area = panel.toNearestInt().reduced (22, 18);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));

        if (asking)
        {
            g.drawText ("Overwrite '" + target.getFileNameWithoutExtension() + "'?", area.removeFromTop (24),
                        juce::Justification::centredLeft, true);
            area.removeFromTop (8);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawFittedText ("You already have a preset with this name. Overwriting replaces it with the sound playing now; "
                              "Cancel goes back to change the name.",
                              area.removeFromTop (60), juce::Justification::topLeft, 3, 1.0f);
            return;
        }

        g.drawText ("Save preset as", area.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText ("To your user preset folder", area.removeFromTop (16), juce::Justification::centredLeft);
        area.removeFromTop (8);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

        for (auto* field : std::initializer_list<juce::Component*> { &nameField, &categoryBox, &tagsField })
            g.drawText (field == &nameField ? "NAME" : (field == &categoryBox ? "CATEGORY" : "TAGS"),
                        juce::Rectangle<int> (area.getX(), field->getY(), labelWidth - 8, field->getHeight()),
                        juce::Justification::centredLeft);

        if (note.isNotEmpty())
        {
            g.setColour (noteIsWarning ? juce::Colour (0xffffb35c) : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            g.drawFittedText (note, juce::Rectangle<int> (area.getX() + labelWidth, tagsField.getBottom() + 6,
                                                          area.getWidth() - labelWidth, 30),
                              juce::Justification::topLeft, 2, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! panelBounds().contains (event.getPosition()))
            asking ? finishOverwrite (false) : cancel();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
            asking ? finishOverwrite (false) : cancel();
        else if (key == juce::KeyPress::returnKey)
            asking ? finishOverwrite (true) : save();

        return true; // nothing reaches the page underneath
    }

private:
    static constexpr int labelWidth = 86;

    juce::Rectangle<int> panelBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre (juce::jmin (460, getWidth() - 32), 252);
    }

    void write (const juce::File& file)
    {
        auto tagList = juce::StringArray::fromTokens (tagsField.getText(), ",", "");
        tagList.trim();
        tagList.removeEmptyStrings();

        processorRef.setPresetMeta (categoryBox.getText(), tagList.joinIntoString (", "));

        if (! processorRef.savePresetToFile (file))
        {
            setNote ("Couldn't write " + file.getFullPathName() + ".", true);
            return;
        }

        setVisible (false);

        if (onSaved != nullptr)
            onSaved (file);
    }

    void setAsking (bool shouldAsk)
    {
        asking = shouldAsk;
        nameField.setVisible (! asking);
        tagsField.setVisible (! asking);
        categoryBox.setVisible (! asking);
        saveButton.setButtonText (asking ? "Overwrite" : "Save");
        cancelButton.setButtonText ("Cancel");
        saveButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent());
        saveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);

        if (asking)
            grabKeyboardFocus();

        repaint();
    }

    void setNote (const juce::String& text, bool warning)
    {
        note = text;
        noteIsWarning = warning;
        repaint();
    }

    // Says which characters the name will lose, as it is typed.
    void updateNote()
    {
        juce::String removed;
        PresetFiles::legalName (nameField.getText(), &removed);

        if (removed.isEmpty())
        {
            setNote ({}, false);
            return;
        }

        juce::StringArray shown;

        for (auto pointer = removed.getCharPointer(); ! pointer.isEmpty();)
            shown.add (juce::String::charToString (pointer.getAndAdvance()));

        setNote ("Saved without  " + shown.joinIntoString ("  ") + "  (a file name can't hold "
                     + (shown.size() == 1 ? "it)." : "them)."),
                 true);
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::TextEditor nameField, tagsField;
    juce::ComboBox categoryBox;
    juce::TextButton saveButton { "Save" }, cancelButton { "Cancel" };
    juce::File target;
    juce::String note;
    bool noteIsWarning = false;
    bool asking = false;
};
