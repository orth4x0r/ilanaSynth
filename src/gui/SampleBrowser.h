#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/OscillatorIds.h"
#include "IlanaLookAndFeel.h"
#include "SampleBrowserModel.h"

// Roadmap step 42: the sample browser. A modal overlay (shown like the zone
// map and the wavetable editor) for one oscillator. The left column holds the
// places: FAVOURITES, RECENT, the Music, Documents and Desktop folders and any
// folder added with ADD FOLDER. The main list is the folder shown (subfolders
// first, then the audio files) or the favourites or recent files; the search
// box narrows it, the star on a row toggles its favourite, a click previews
// the file (level matched, see IlanaSynthAudioProcessor::startSamplePreview)
// and a double click or Return loads it into the oscillator and adds it to
// RECENT. Favourites, recent files and added folders are kept in the editor's
// settings as portable paths (SampleBrowserModel).
//
// Wiring: construct with the processor, the oscillator's index, its colour, the
// editor's settings file (may be null) and a function that loads a file into the
// oscillator and returns whether it worked; size it like the zone map, add it as
// a child, set onClose to remove it (called asynchronously from CLOSE, Escape
// and after a load) and give it keyboard focus.
class SampleBrowser : public juce::Component,
                      public juce::ListBoxModel,
                      public juce::KeyListener,
                      private juce::Timer
{
public:
    using LoadFunction = std::function<bool (const juce::File&)>;

    SampleBrowser (IlanaSynthAudioProcessor& p, int oscIndex, juce::Colour colourIn, juce::PropertiesFile* settingsIn,
                   LoadFunction loadIn)
        : processorRef (p),
          osc (juce::jlimit (0, OscillatorIds::count - 1, oscIndex)),
          colour (colourIn),
          settings (settingsIn),
          loadFile (std::move (loadIn)),
          places (*this)
    {
        if (settings != nullptr)
        {
            favourites = SampleBrowserModel::parseList (settings->getValue ("sampleFavourites"));
            recent = SampleBrowserModel::parseList (settings->getValue ("sampleRecent"));
            addedFolders = SampleBrowserModel::parseList (settings->getValue ("sampleFolders"));
            previewOn = settings->getBoolValue ("sampleBrowserPreview", true);
            const juce::File last (settings->getValue ("sampleBrowserFolder"));
            if (last.isDirectory())
                folder = last;
        }

        if (! folder.isDirectory())
            folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (! folder.isDirectory())
            folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

        if (SampleBrowserModel::dropMissing (recent))
            saveList ("sampleRecent", recent);

        search.setTextToShowWhenEmpty ("Search this list", IlanaTheme::Ui::text3);
        search.setColour (juce::TextEditor::backgroundColourId, IlanaTheme::Ui::well);
        search.setColour (juce::TextEditor::outlineColourId, IlanaTheme::Ui::line);
        search.setColour (juce::TextEditor::focusedOutlineColourId, colour.withAlpha (0.6f));
        search.setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
        search.setFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body)));
        search.setIndents (8, 5);
        search.onTextChange = [this] { refresh(); };
        search.onEscapeKey = [this] { escape(); };
        search.onFocusLost = [this] { repaint (search.getBounds()); };
        search.addKeyListener (this);
        addAndMakeVisible (search);

        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        list.setRowHeight (rowHeight);
        list.setModel (this);
        list.addKeyListener (this);
        list.addMouseListener (this, true);
        addAndMakeVisible (list);
        placesView.setViewedComponent (&places, false);
        placesView.setScrollBarsShown (true, false);
        placesView.setScrollBarThickness (8);
        addAndMakeVisible (placesView);

        for (auto* button : { &closeButton, &upButton, &addFolderButton, &previewButton, &loadButton })
            addAndMakeVisible (*button);

        closeButton.onClick = [this] { close(); };
        upButton.onClick = [this] { goUp(); };
        addFolderButton.onClick = [this] { chooseFolder(); };
        loadButton.onClick = [this] { activate (list.getSelectedRow()); };
        previewButton.setClickingTogglesState (true);
        previewButton.setToggleState (previewOn, juce::dontSendNotification);
        previewButton.onClick = [this]
        {
            previewOn = previewButton.getToggleState();
            if (settings != nullptr)
            {
                settings->setValue ("sampleBrowserPreview", previewOn);
                settings->saveIfNeeded();
            }
            if (previewOn)
                previewSelected();
            else
                stopPreview();
        };

        closeButton.setTooltip ("Close the sample browser (Escape)");
        upButton.setTooltip ("Go to the folder that holds this one (Backspace)");
        addFolderButton.setTooltip ("Add a folder to the places on the left");
        previewButton.setTooltip ("Preview\nOn: selecting a sample plays it, matched to the same level as every other preview.");
        loadButton.setTooltip ("Load the selected sample into this oscillator (double click or Return)");

        setWantsKeyboardFocus (true);
        rebuildPlaces();
        showFolder (folder);
    }

    ~SampleBrowser() override
    {
        stopTimer();
        processorRef.stopSamplePreview();
    }

    std::function<void()> onClose;

    // ---------------------------------------------------------------- for the tests

    enum class View { folder, favourites, recent };

    View getView() const { return view; }
    juce::File getFolder() const { return folder; }
    const juce::StringArray& getFavourites() const { return favourites; }
    const juce::StringArray& getRecent() const { return recent; }
    const juce::StringArray& getAddedFolders() const { return addedFolders; }
    void setSearchText (const juce::String& text) { search.setText (text, true); }
    int getNumShown() const { return (int) rows.size(); }
    juce::File getShownFile (int row) const { return juce::isPositiveAndBelow (row, (int) rows.size()) ? rows[(size_t) row].file : juce::File(); }
    void openFolder (const juce::File& target) { showFolder (target); }
    void openPlace (int index) { showPlace (index); }
    void selectShown (int row) { list.selectRow (row); }
    void activateShown (int row) { activate (row); }
    void toggleShownFavourite (int row) { toggleFavourite (row); }
    juce::String getLevelNote() const { return levelNote; }

    // ---------------------------------------------------------------- paint

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xf0101014));
        g.fillRoundedRectangle (bounds, 8.0f);
        IlanaTheme::paintCard (g, bounds.reduced (2.0f), 8.0f, colour);

        g.setColour (colour);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
        IlanaTheme::drawFitted (g, juce::String ("SAMPLE BROWSER") + dot + "OSC " + juce::String (osc + 1), titleArea,
                                juce::Justification::centredLeft, 1);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        IlanaTheme::drawFitted (g, "Click to hear a sample, double click or Return to load it, the star keeps it under FAVOURITES",
                                hintArea, juce::Justification::centredRight, 2);

        IlanaTheme::paintWell (g, placesWell, 6.0f);
        IlanaTheme::paintWell (g, listWell, 6.0f);

        // Where we are: the path as links to each folder, or the list's name.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        if (view != View::folder)
        {
            g.setColour (IlanaTheme::Ui::text);
            g.drawText (view == View::favourites ? "FAVOURITES" : "RECENT", crumbArea, juce::Justification::centredLeft);
        }
        else
        {
            for (size_t i = 0; i < crumbs.size(); ++i)
            {
                const auto last = i + 1 == crumbs.size();
                g.setColour (last ? IlanaTheme::Ui::text : (hoveredCrumb == (int) i ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2));
                g.drawText (crumbs[i].text, crumbs[i].bounds, juce::Justification::centredLeft);
            }
        }

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        IlanaTheme::drawFitted (g, levelNote, noteArea, juce::Justification::centredLeft, 1);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // The box's own hint hides while it has the keyboard focus, and it
        // has the focus the moment the browser opens, so it read as an empty
        // gold outline (review 21): draw the hint ourselves in that case.
        if (search.getText().isEmpty() && search.hasKeyboardFocus (false))
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Search this list", search.getBounds().withTrimmedLeft (9), juce::Justification::centredLeft);
        }

        if (! rows.empty())
            return;

        juce::String text;
        const auto filtering = search.getText().trim().isNotEmpty();

        if (filtering)
            text = "Nothing here matches \"" + search.getText().trim() + "\".";
        else if (view == View::favourites)
            text = "No favourites yet. Click the star on a sample to add it.";
        else if (view == View::recent)
            text = "No recent samples yet. Samples you load from here appear in this list.";
        else
            text = "This folder holds no folders or audio files.";

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (text, listWell.toNearestInt().withHeight (80), juce::Justification::centred);
    }

    void resized() override
    {
        const auto gap = 10;
        auto area = getLocalBounds().reduced (14, 10);

        auto header = area.removeFromTop (30);
        closeButton.setBounds (header.removeFromRight (70).reduced (2));
        header.removeFromRight (gap);
        titleArea = header.removeFromLeft (juce::jmax (170, header.getWidth() * 2 / 5));
        hintArea = header;
        area.removeFromTop (gap);

        auto footer = area.removeFromBottom (30);
        area.removeFromBottom (gap);
        previewButton.setBounds (footer.removeFromLeft (100).reduced (0, 2));
        footer.removeFromLeft (gap);
        loadButton.setBounds (footer.removeFromRight (90).reduced (0, 2));
        footer.removeFromRight (gap);
        noteArea = footer;

        auto left = area.removeFromLeft (190);
        area.removeFromLeft (gap);
        addFolderButton.setBounds (left.removeFromBottom (30).reduced (0, 2));
        left.removeFromBottom (gap);
        placesWell = left.toFloat();
        placesView.setBounds (left.reduced (2));
        layoutPlaces();

        auto bar = area.removeFromTop (30);
        area.removeFromTop (gap);
        upButton.setBounds (bar.removeFromLeft (100).reduced (0, 2));
        bar.removeFromLeft (gap);
        search.setBounds (bar.removeFromRight (juce::jlimit (160, 260, bar.getWidth() / 3)).reduced (0, 2));
        bar.removeFromRight (gap);
        crumbArea = bar;

        listWell = area.toFloat();
        list.setBounds (area.reduced (2));
        layoutCrumbs();
    }

    // ---------------------------------------------------------------- keys and mouse

    bool keyPressed (const juce::KeyPress& key) override
    {
        return handleKey (key);
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        return handleKey (key);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (event.eventComponent == this)
        {
            auto crumb = -1;
            for (size_t i = 0; view == View::folder && i < crumbs.size(); ++i)
                if (crumbs[i].bounds.contains (event.getPosition()))
                    crumb = (int) i;
            if (crumb != hoveredCrumb)
            {
                hoveredCrumb = crumb;
                setMouseCursor (crumb >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
                repaint (crumbArea);
            }
            return;
        }

        const auto position = event.getEventRelativeTo (&list).getPosition();
        const auto row = list.getRowContainingPosition (position.x, position.y);
        if (row != hoveredRow || position.x != hoveredX)
        {
            hoveredRow = row;
            hoveredX = position.x;
            list.repaint();
        }
    }

    void mouseExit (const juce::MouseEvent& event) override
    {
        if (event.eventComponent == this)
        {
            hoveredCrumb = -1;
            repaint (crumbArea);
        }
        else if (hoveredRow != -1)
        {
            hoveredRow = -1;
            list.repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.eventComponent != this || view != View::folder)
            return;

        for (const auto& crumb : crumbs)
            if (crumb.bounds.contains (event.getPosition()))
            {
                showFolder (crumb.file);
                return;
            }
    }

    // ---------------------------------------------------------------- list model

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        const auto& entry = rows[(size_t) row];
        const auto rowBounds = juce::Rectangle<float> (3.0f, 1.0f, (float) width - 6.0f, (float) height - 2.0f);
        const auto missing = ! entry.file.exists();

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

        const auto centre = juce::Point<float> (17.0f, (float) height * 0.5f);

        if (entry.isFolder)
        {
            // A folder glyph where a file has its star.
            g.setColour (IlanaTheme::Ui::text2);
            g.fillRoundedRectangle (centre.x - 7.0f, centre.y - 4.0f, 14.0f, 9.5f, 1.5f);
            g.fillRoundedRectangle (centre.x - 7.0f, centre.y - 6.0f, 6.0f, 3.0f, 1.0f);
        }
        else
        {
            const auto favourite = favourites.contains (SampleBrowserModel::toPortablePath (entry.file));
            drawStar (g, centre, 5.5f, favourite ? juce::Colour (0xffffd447)
                                                  : juce::Colours::white.withAlpha (row == hoveredRow && hoveredX < 30 ? 0.75f
                                                                                    : (row == hoveredRow ? 0.4f : 0.18f)),
                      favourite);
        }

        auto nameRight = width - 12;

        if (! entry.isFolder)
        {
            // The kind of file as a pill, at the right.
            const auto text = entry.file.getFileExtension().removeCharacters (".").toUpperCase();
            const auto pillWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true)), text) + 14;
            const auto pill = juce::Rectangle<float> ((float) width - 12.0f - (float) pillWidth, (float) height * 0.5f - 8.0f, (float) pillWidth, 16.0f);
            g.setColour (colour.withAlpha (selected ? 0.30f : 0.16f));
            g.fillRoundedRectangle (pill, 8.0f);
            g.setColour (colour.withAlpha (selected ? 1.0f : 0.85f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (text, pill.toNearestInt(), juce::Justification::centred);
            nameRight = (int) pill.getX() - 10;

            // Favourites and recent files say which folder they are in.
            if (view != View::folder && width > 420)
            {
                const auto where = entry.file.getParentDirectory().getFileName();
                const auto whereWidth = juce::jmin (nameRight / 3, 180);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
                g.drawText (where, juce::Rectangle<int> (nameRight - whereWidth, 0, whereWidth, height), juce::Justification::centredRight, true);
                nameRight -= whereWidth + 10;
            }
        }

        g.setColour (missing ? IlanaTheme::Ui::text3 : juce::Colours::white.withAlpha (selected ? 0.97f : 0.8f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title));
        g.drawText (entry.isFolder ? entry.file.getFileName() : entry.file.getFileNameWithoutExtension(),
                    juce::Rectangle<int> (32, 0, juce::jmax (20, nameRight - 32), height), juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& event) override
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        if (event.x < 30 && ! rows[(size_t) row].isFolder)
        {
            stopTimer();
            toggleFavourite (row);
        }
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override
    {
        if (event.x >= 30)
            activate (row);
    }

    void returnKeyPressed (int row) override { activate (row); }
    void deleteKeyPressed (int) override { goUp(); }
    void selectedRowsChanged (int) override { schedulePreview(); }

    juce::String getTooltipForRow (int row) override
    {
        return juce::isPositiveAndBelow (row, (int) rows.size()) ? rows[(size_t) row].file.getFullPathName() : juce::String();
    }

private:
    static constexpr int rowHeight = 28;

    struct Place
    {
        View kind = View::folder;
        juce::String label;
        juce::File folder;
        bool removable = false;
        juce::String stored; // an added folder's entry in the list
    };

    struct Crumb
    {
        juce::String text;
        juce::File file;
        juce::Rectangle<int> bounds;
    };

    // The places column: rows like the list's, under a small heading.
    struct PlacesList : public juce::Component,
                        public juce::TooltipClient
    {
        explicit PlacesList (SampleBrowser& ownerIn) : owner (ownerIn) {}

        static constexpr int headingHeight = 26;

        int rowAt (int y) const
        {
            const auto row = (y - headingHeight) / rowHeight;
            return y >= headingHeight && row < (int) owner.placeList.size() ? row : -1;
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("PLACES", juce::Rectangle<int> (10, 0, getWidth() - 20, headingHeight), juce::Justification::centredLeft);

            for (int i = 0; i < (int) owner.placeList.size(); ++i)
            {
                const auto& place = owner.placeList[(size_t) i];
                const auto bounds = juce::Rectangle<float> (3.0f, (float) (headingHeight + i * rowHeight) + 1.0f,
                                                            (float) getWidth() - 6.0f, (float) rowHeight - 2.0f);
                const auto current = owner.isCurrentPlace (place);

                if (current)
                {
                    juce::ColourGradient selection (IlanaTheme::accent().withAlpha (0.30f), 0.0f, 0.0f,
                                                    IlanaTheme::accent().withAlpha (0.10f), (float) getWidth(), 0.0f, false);
                    g.setGradientFill (selection);
                    g.fillRoundedRectangle (bounds, 5.0f);
                }
                else if (i == hovered)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.06f));
                    g.fillRoundedRectangle (bounds, 5.0f);
                }

                if (place.kind == View::favourites)
                    SampleBrowser::drawStar (g, { bounds.getX() + 14.0f, bounds.getCentreY() }, 5.5f, juce::Colour (0xffffd447), true);

                g.setColour (current ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, current));
                g.drawText (place.label, bounds.toNearestInt().withTrimmedLeft (place.kind == View::favourites ? 30 : 12).withTrimmedRight (8),
                            juce::Justification::centredLeft, true);
            }
        }

        void mouseMove (const juce::MouseEvent& event) override
        {
            const auto row = rowAt (event.y);
            if (row != hovered)
            {
                hovered = row;
                repaint();
            }
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            hovered = -1;
            repaint();
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            const auto row = rowAt (event.y);
            if (row < 0)
                return;

            if (event.mods.isPopupMenu())
            {
                owner.removePlaceMenu (row);
                return;
            }

            owner.showPlace (row);
        }

        juce::String getTooltip() override
        {
            if (! juce::isPositiveAndBelow (hovered, (int) owner.placeList.size()))
                return {};

            const auto& place = owner.placeList[(size_t) hovered];
            return place.removable ? place.folder.getFullPathName() + "\nRight click to remove this folder from the places"
                                   : place.folder.getFullPathName();
        }

        SampleBrowser& owner;
        int hovered = -1;
    };

    struct Row
    {
        juce::File file;
        bool isFolder = false;
    };

    static void drawStar (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour starColour, bool filled)
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
        g.setColour (starColour);

        if (filled)
            g.fillPath (star);
        else
            g.strokePath (star, juce::PathStrokeType (1.1f));
    }

    bool isCurrentPlace (const Place& place) const
    {
        if (place.kind != View::folder)
            return place.kind == view;

        return view == View::folder && place.folder == folder;
    }

    void rebuildPlaces()
    {
        placeList.clear();
        placeList.push_back ({ View::favourites, "FAVOURITES", {}, false, {} });
        placeList.push_back ({ View::recent, "RECENT", {}, false, {} });

        const std::pair<const char*, juce::File::SpecialLocationType> systemFolders[] {
            { "MUSIC", juce::File::userMusicDirectory },
            { "DOCUMENTS", juce::File::userDocumentsDirectory },
            { "DESKTOP", juce::File::userDesktopDirectory } };

        for (const auto& [label, type] : systemFolders)
        {
            const auto directory = juce::File::getSpecialLocation (type);
            if (directory.isDirectory())
                placeList.push_back ({ View::folder, label, directory, false, {} });
        }

        for (const auto& stored : addedFolders)
        {
            const auto directory = SampleBrowserModel::fromPortablePath (stored);
            if (directory.isDirectory())
                placeList.push_back ({ View::folder, directory.getFileName(), directory, true, stored });
        }

        layoutPlaces();
        places.repaint();
    }

    void layoutPlaces()
    {
        places.setSize (placesView.getMaximumVisibleWidth(),
                        juce::jmax (placesView.getHeight(), PlacesList::headingHeight + (int) placeList.size() * rowHeight));
    }

    void showPlace (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) placeList.size()))
            return;

        const auto place = placeList[(size_t) index];

        if (place.kind == View::folder)
        {
            showFolder (place.folder);
            return;
        }

        view = place.kind;
        search.setText ({}, false);
        refresh();
    }

    void removePlaceMenu (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) placeList.size()) || ! placeList[(size_t) index].removable)
            return;

        juce::PopupMenu menu;
        menu.addItem (1, "Remove Folder From Places");
        const auto stored = placeList[(size_t) index].stored;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&places),
                            [safe = juce::Component::SafePointer<SampleBrowser> (this), stored] (int result)
                            {
                                if (safe == nullptr || result != 1)
                                    return;

                                safe->addedFolders.removeString (stored);
                                safe->saveList ("sampleFolders", safe->addedFolders);
                                safe->rebuildPlaces();
                            });
    }

    void chooseFolder()
    {
        folderChooser = std::make_unique<juce::FileChooser> ("Add a folder of samples", folder);
        folderChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safe = juce::Component::SafePointer<SampleBrowser> (this)] (const juce::FileChooser& chooser)
                                    {
                                        if (safe == nullptr || ! chooser.getResult().isDirectory())
                                            return;

                                        safe->addFolder (chooser.getResult());
                                    });
    }

    void addFolder (const juce::File& directory)
    {
        const auto stored = SampleBrowserModel::toPortablePath (directory);

        if (! addedFolders.contains (stored))
        {
            addedFolders.add (stored);
            saveList ("sampleFolders", addedFolders);
        }

        rebuildPlaces();
        showFolder (directory);
    }

    void showFolder (const juce::File& target)
    {
        if (! target.isDirectory())
            return;

        folder = target;
        view = View::folder;
        search.setText ({}, false);

        if (settings != nullptr)
        {
            settings->setValue ("sampleBrowserFolder", folder.getFullPathName());
            settings->saveIfNeeded();
        }

        refresh();
    }

    void goUp()
    {
        if (view != View::folder)
            return;

        const auto parent = folder.getParentDirectory();
        if (parent != folder)
            showFolder (parent);
    }

    void refresh()
    {
        stopTimer();
        stopPreview();
        rows.clear();

        if (view == View::folder)
        {
            for (const auto& entry : SampleBrowserModel::listFolder (folder, search.getText()))
                rows.push_back ({ entry.file, entry.isFolder });
        }
        else
        {
            if (view == View::recent && SampleBrowserModel::dropMissing (recent))
                saveList ("sampleRecent", recent);

            const auto words = juce::StringArray::fromTokens (search.getText().trim().toLowerCase(), " ", "");

            for (const auto& stored : view == View::favourites ? favourites : recent)
            {
                const auto file = SampleBrowserModel::fromPortablePath (stored);
                const auto name = file.getFileName().toLowerCase();
                auto matches = true;

                for (const auto& word : words)
                    matches = matches && name.contains (word);

                if (matches)
                    rows.push_back ({ file, false });
            }

            if (view == View::favourites)
                std::sort (rows.begin(), rows.end(), [] (const Row& a, const Row& b)
                           { return a.file.getFileName().compareNatural (b.file.getFileName()) < 0; });
        }

        list.updateContent();
        list.deselectAllRows();
        list.scrollToEnsureRowIsOnscreen (0);
        list.repaint();
        upButton.setEnabled (view == View::folder && folder.getParentDirectory() != folder);
        loadButton.setEnabled (false);
        setLevelNote ({});
        layoutCrumbs();
        places.repaint();
        repaint();
    }

    // The path as clickable pieces; the front ones are dropped (and shown as
    // dots) when it is too long for the bar.
    void layoutCrumbs()
    {
        crumbs.clear();

        if (view != View::folder)
            return;

        const juce::Font font (IlanaTheme::font (IlanaTheme::TextSize::body));
        const auto separator = juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xba "));
        std::vector<Crumb> chain;

        for (auto file = folder;; file = file.getParentDirectory())
        {
            auto name = file.getFileName();
            if (name.isEmpty())
                name = file.getFullPathName();
            chain.insert (chain.begin(), { name, file, {} });

            if (file.getParentDirectory() == file)
                break;
        }

        const auto widthOf = [&font] (const juce::String& text) { return juce::GlyphArrangement::getStringWidthInt (font, text); };
        auto first = 0;
        const auto total = [&]
        {
            auto width = first > 0 ? widthOf ("..." + separator) : 0;
            for (size_t i = (size_t) first; i < chain.size(); ++i)
                width += widthOf (chain[i].text) + (i + 1 < chain.size() ? widthOf (separator) : 0);
            return width;
        };

        while (first < (int) chain.size() - 1 && total() > crumbArea.getWidth())
            ++first;

        auto x = crumbArea.getX();
        if (first > 0)
        {
            const auto dots = "..." + separator;
            crumbs.push_back ({ dots, chain[(size_t) first - 1].file, { x, crumbArea.getY(), widthOf (dots), crumbArea.getHeight() } });
            x += widthOf (dots);
        }

        for (size_t i = (size_t) first; i < chain.size(); ++i)
        {
            const auto text = chain[i].text + (i + 1 < chain.size() ? separator : juce::String());
            crumbs.push_back ({ text, chain[i].file, { x, crumbArea.getY(), widthOf (text), crumbArea.getHeight() } });
            x += widthOf (text);
        }

        repaint (crumbArea);
    }

    // ---------------------------------------------------------------- favourites, recent, load

    void saveList (const char* key, const juce::StringArray& values)
    {
        if (settings == nullptr)
            return;

        settings->setValue (key, SampleBrowserModel::serialiseList (values));
        settings->saveIfNeeded();
    }

    void toggleFavourite (int row)
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].isFolder)
            return;

        SampleBrowserModel::toggleEntry (favourites, SampleBrowserModel::toPortablePath (rows[(size_t) row].file));
        saveList ("sampleFavourites", favourites);

        // A file un-starred in the favourites list leaves it.
        if (view == View::favourites)
        {
            const auto selected = list.getSelectedRow();
            const auto file = rows[(size_t) row].file;
            if (! favourites.contains (SampleBrowserModel::toPortablePath (file)))
            {
                rows.erase (rows.begin() + row);
                list.updateContent();
                list.selectRow (selected == row ? -1 : (selected > row ? selected - 1 : selected));
            }
        }

        list.repaint();
    }

    void activate (int row)
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;

        const auto entry = rows[(size_t) row];

        if (entry.isFolder)
        {
            showFolder (entry.file);
            return;
        }

        stopTimer();
        stopPreview();

        if (! entry.file.existsAsFile() || loadFile == nullptr || ! loadFile (entry.file))
        {
            const auto why = processorRef.getLastSampleError();
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load sample",
                                                    why.isNotEmpty() ? why : "Could not load \"" + entry.file.getFileName() + "\".");
            return;
        }

        SampleBrowserModel::addRecent (recent, SampleBrowserModel::toPortablePath (entry.file));
        saveList ("sampleRecent", recent);
        close();
    }

    // ---------------------------------------------------------------- preview

    void schedulePreview()
    {
        const auto row = list.getSelectedRow();
        const auto selected = juce::isPositiveAndBelow (row, (int) rows.size()) && ! rows[(size_t) row].isFolder;
        loadButton.setEnabled (selected);
        stopTimer();

        if (! selected)
        {
            stopPreview();
            setLevelNote ({});
            return;
        }

        if (previewOn)
            startTimer (120);
        else
            setLevelNote ("Preview is off");
    }

    void timerCallback() override
    {
        stopTimer();
        previewSelected();
    }

    void previewSelected()
    {
        const auto row = list.getSelectedRow();

        if (! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].isFolder)
        {
            setLevelNote ({});
            return;
        }

        const auto& file = rows[(size_t) row].file;
        stopPreview();

        if (! previewOn)
            setLevelNote ("Preview is off");
        else if (! SampleBrowserModel::isPreviewable (file))
            setLevelNote ("SoundFonts and SFZ instruments have no preview");
        else if (processorRef.startSamplePreview (file))
            setLevelNote (SampleBrowserModel::levelNote (processorRef.getSamplePreviewGain()));
        else
            setLevelNote ("This file cannot be previewed");
    }

    void stopPreview() { processorRef.stopSamplePreview(); }

    void setLevelNote (const juce::String& text)
    {
        levelNote = text;
        repaint (noteArea);
    }

    // ---------------------------------------------------------------- keys

    bool handleKey (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            escape();
            return true;
        }

        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            const auto count = (int) rows.size();
            if (count > 0)
            {
                const auto row = list.getSelectedRow();
                const auto next = row < 0 ? (key == juce::KeyPress::downKey ? 0 : count - 1)
                                          : juce::jlimit (0, count - 1, row + (key == juce::KeyPress::downKey ? 1 : -1));
                list.selectRow (next);
            }
            return true;
        }

        if (key == juce::KeyPress::returnKey)
        {
            activate (juce::jmax (0, list.getSelectedRow()));
            return true;
        }

        if (key == juce::KeyPress::backspaceKey && ! search.hasKeyboardFocus (false))
        {
            goUp();
            return true;
        }

        return false;
    }

    // Escape clears the search first, then closes.
    void escape()
    {
        if (search.getText().isNotEmpty())
            search.setText ({}, true);
        else
            close();
    }

    void close()
    {
        stopTimer();
        stopPreview();

        if (onClose != nullptr)
        {
            auto callback = onClose;
            juce::MessageManager::callAsync (callback);
        }
    }

    void parentHierarchyChanged() override
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SampleBrowser> (this)]
        {
            if (safe != nullptr && safe->isShowing())
            {
                safe->search.grabKeyboardFocus();
                safe->repaint (safe->search.getBounds());
            }
        });
    }

    IlanaSynthAudioProcessor& processorRef;
    int osc = 0;
    juce::Colour colour;
    juce::PropertiesFile* settings = nullptr;
    LoadFunction loadFile;

    juce::StringArray favourites, recent, addedFolders;
    View view = View::folder;
    juce::File folder;
    std::vector<Row> rows;
    std::vector<Place> placeList;
    std::vector<Crumb> crumbs;
    bool previewOn = true;
    int hoveredRow = -1, hoveredX = 0, hoveredCrumb = -1;
    juce::String levelNote;

    PlacesList places;
    juce::Viewport placesView;
    juce::ListBox list { "samples", this };
    juce::TextEditor search;
    juce::TextButton closeButton { "CLOSE" }, upButton { "FOLDER UP" }, addFolderButton { "ADD FOLDER..." },
        previewButton { "PREVIEW" }, loadButton { "LOAD" };
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::Rectangle<int> titleArea, hintArea, crumbArea, noteArea;
    juce::Rectangle<float> placesWell, listWell;
};
