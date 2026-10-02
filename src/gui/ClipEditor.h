#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"
#include "GenerativeWidgets.h"

// The clip sequencer's piano roll: the notes of the clip chosen by CLIP.
// Click empty space to add a note, drag a note to move it, drag its right
// edge to resize it, right-click to delete; the wheel scrolls the pitches.
// Edits go straight to the patch's clips (ClipState), one undo step per
// click or drag.
class ClipEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   private IlanaAnim::FrameTimer
{
public:
    ClipEditor (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Clip piano roll. Click to add a note, drag to move it, drag its right edge to resize, "
                    "right-click to delete. The wheel scrolls the pitches.");
        reload (true);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = GenerativeWidgets::read (processorRef, "clip_on") > 0.5f;
        const auto grid = gridBounds();
        const auto rows = visibleRows();
        const auto rowHeight = grid.getHeight() / (float) rows;
        const auto beats = (float) lengthBeats();

        // Rows: black keys darker, C rows labelled on the keyboard strip.
        for (int row = 0; row < rows; ++row)
        {
            const auto note = lowNote + row;
            const auto y = grid.getBottom() - (float) (row + 1) * rowHeight;
            const auto black = juce::MidiMessage::isMidiNoteBlack (note);
            g.setColour (juce::Colours::white.withAlpha (black ? 0.0f : 0.035f));
            g.fillRect (juce::Rectangle<float> (grid.getX(), y, grid.getWidth(), rowHeight));

            if (note % 12 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.fillRect (grid.getX(), y + rowHeight - 0.5f, grid.getWidth(), 1.0f);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                            juce::Rectangle<float> (bounds.getX() + 4.0f, y, keyWidth - 6.0f, rowHeight).toNearestInt(),
                            juce::Justification::centredLeft);
            }
        }

        // Beat lines, the bars stronger.
        for (int beat = 0; beat <= (int) beats; ++beat)
        {
            const auto x = grid.getX() + grid.getWidth() * (float) beat / beats;
            g.setColour (juce::Colours::white.withAlpha (beat % ClipState::beatsPerBar == 0 ? 0.16f : 0.06f));
            g.fillRect (x, grid.getY(), 1.0f, grid.getHeight());
        }

        // The root note (C3) plays the clip as written.
        if (ClipState::rootNote >= lowNote && ClipState::rootNote < lowNote + rows)
        {
            g.setColour (colour.withAlpha (0.35f));
            g.fillRect (grid.getX(), grid.getBottom() - (float) (ClipState::rootNote - lowNote) * rowHeight - 0.5f, grid.getWidth(), 1.0f);
        }

        for (const auto& n : clip.notes)
        {
            const auto r = noteBounds (n);

            if (! r.intersects (grid))
                continue;

            const auto shade = 0.45f + 0.55f * (float) n.velocity / 127.0f;
            g.setColour (colour.withAlpha (shade * (on ? 1.0f : 0.6f)));
            g.fillRoundedRectangle (r.reduced (0.0f, 0.5f), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawRoundedRectangle (r.reduced (0.0f, 0.5f), 2.0f, 1.0f);
        }

        if (const auto playhead = processorRef.getClipPlayhead(); on && playhead >= 0.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (grid.getX() + grid.getWidth() * playhead / beats, grid.getY(), 1.5f, grid.getHeight());
        }

        // The keyboard strip's edge.
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (grid.getX() - 1.0f, grid.getY(), 1.0f, grid.getHeight());

        if (! on)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("CLIP OFF", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
        }
        else if (clip.notes.empty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("EMPTY: click to add notes", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        reload (false);
        dragIndex = -1;
        const auto hit = noteAt (event.position);

        // The whole click or drag is one undo step (ended in mouseUp).
        processorRef.beginEdit (event.mods.isPopupMenu() ? "Delete clip note"
                                : hit < 0               ? "Add clip note"
                                : event.position.x > noteBounds (clip.notes[(size_t) hit]).getRight() - 6.0f ? "Resize clip note"
                                                                                                              : "Move clip note");

        if (event.mods.isPopupMenu())
        {
            if (hit >= 0)
            {
                clip.notes.erase (clip.notes.begin() + hit);
                commit();
            }

            return;
        }

        if (hit >= 0)
        {
            dragIndex = hit;
            const auto& n = clip.notes[(size_t) hit];
            mode = event.position.x > noteBounds (n).getRight() - 6.0f ? Drag::resize : Drag::move;
            grabBeat = xToBeat (event.position.x) - n.start;
            lastLength = n.length;
            return;
        }

        if (! gridBounds().contains (event.position) || (int) clip.notes.size() >= ClipState::maxNotes)
            return;

        // A new note where the click is, snapped to the grid; dragging moves it.
        ClipNote n;
        n.start = snap (xToBeat (event.position.x));
        n.length = lastLength;
        n.note = yToNote (event.position.y);
        n.velocity = 100;
        clip.notes.push_back (n);
        dragIndex = (int) clip.notes.size() - 1;
        mode = Drag::move;
        grabBeat = xToBeat (event.position.x) - n.start;
        constrain (clip.notes.back());
        commit();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragIndex < 0 || dragIndex >= (int) clip.notes.size())
            return;

        auto& n = clip.notes[(size_t) dragIndex];

        if (mode == Drag::resize)
        {
            n.length = juce::jmax (snapStep, snap (xToBeat (event.position.x) - n.start + snapStep * 0.5f));
            lastLength = n.length;
        }
        else
        {
            n.start = snap (xToBeat (event.position.x) - grabBeat);
            n.note = yToNote (event.position.y);
        }

        constrain (n);
        commit();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragIndex = -1;
        processorRef.endEdit();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const auto rows = visibleRows();
        lowNote = juce::jlimit (0, 128 - rows, lowNote + (wheel.deltaY > 0.0f ? 3 : wheel.deltaY < 0.0f ? -3 : 0));
        repaint();
    }

    // The clip shown follows the CLIP choice; a new clip centres on its notes.
    void reload (bool force)
    {
        if (dragIndex >= 0 && ! force)
            return;

        const auto index = juce::jlimit (0, ClipState::numClips - 1, juce::roundToInt (GenerativeWidgets::read (processorRef, "clip_index")));
        const auto epoch = processorRef.getDataEpoch();

        if (! force && index == shownIndex && epoch == shownEpoch)
            return;

        const auto changedClip = index != shownIndex;
        shownIndex = index;
        shownEpoch = epoch;
        clip = processorRef.getClipState().getClip (index);

        if (changedClip || force)
            centreOnNotes();

        repaint();
    }

private:
    static constexpr float keyWidth = 34.0f;
    static constexpr float snapStep = 0.25f; // a sixteenth
    enum class Drag { move, resize };

    void timerCallback() override
    {
        if (! isShowing())
            return;

        reload (false);
        const auto playhead = processorRef.getClipPlayhead();
        const auto signature = processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)
                             ^ ((juce::uint64) (playhead * 64.0f + 64.0f) << 40) ^ ((juce::uint64) lowNote << 52);

        if (changeGate.check (signature))
            repaint();
    }

    double lengthBeats() const { return (double) (clip.bars * ClipState::beatsPerBar); }

    juce::Rectangle<float> gridBounds() const
    {
        auto area = getLocalBounds().toFloat().reduced (0.0f, 4.0f);
        return area.withTrimmedLeft (keyWidth).withTrimmedRight (6.0f);
    }

    int visibleRows() const { return juce::jlimit (12, 48, juce::roundToInt (gridBounds().getHeight() / 7.0f)); }

    void centreOnNotes()
    {
        auto centre = 60;

        if (! clip.notes.empty())
        {
            auto lowest = 127, highest = 0;

            for (const auto& n : clip.notes)
            {
                lowest = juce::jmin (lowest, n.note);
                highest = juce::jmax (highest, n.note);
            }

            centre = (lowest + highest) / 2;
        }

        const auto rows = visibleRows();
        lowNote = juce::jlimit (0, 128 - rows, centre - rows / 2);
    }

    float beatToX (float beat) const { return gridBounds().getX() + gridBounds().getWidth() * beat / (float) lengthBeats(); }
    float xToBeat (float x) const { return (x - gridBounds().getX()) / gridBounds().getWidth() * (float) lengthBeats(); }
    static float snap (float beat) { return std::floor (beat / snapStep + 0.5f) * snapStep; }

    int yToNote (float y) const
    {
        const auto grid = gridBounds();
        const auto rowHeight = grid.getHeight() / (float) visibleRows();
        return juce::jlimit (0, 127, lowNote + (int) std::floor ((grid.getBottom() - y) / rowHeight));
    }

    juce::Rectangle<float> noteBounds (const ClipNote& n) const
    {
        const auto grid = gridBounds();
        const auto rowHeight = grid.getHeight() / (float) visibleRows();
        const auto y = grid.getBottom() - (float) (n.note - lowNote + 1) * rowHeight;
        return { beatToX (n.start), y, juce::jmax (3.0f, beatToX (n.start + n.length) - beatToX (n.start)), rowHeight };
    }

    int noteAt (juce::Point<float> position) const
    {
        for (int i = (int) clip.notes.size() - 1; i >= 0; --i)
            if (noteBounds (clip.notes[(size_t) i]).expanded (0.0f, 0.5f).contains (position))
                return i;

        return -1;
    }

    void constrain (ClipNote& n) const
    {
        const auto length = (float) lengthBeats();
        n.note = juce::jlimit (0, 127, n.note);
        n.start = juce::jlimit (0.0f, length - snapStep, n.start);
        n.length = juce::jlimit (snapStep, length - n.start, n.length);
    }

    // Publishes the edit; the notes stay in the order they were made while
    // a drag is on (ClipState sorts its own copy).
    void commit()
    {
        processorRef.getClipState().setClip (shownIndex, clip);
        processorRef.clipsEdited();
        shownEpoch = processorRef.getDataEpoch();
        repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    juce::Colour colour;
    Clip clip;
    int shownIndex = -1;
    unsigned shownEpoch = 0;
    int lowNote = 48;
    int dragIndex = -1;
    Drag mode = Drag::move;
    float grabBeat = 0.0f, lastLength = 1.0f;
};

// LENGTH: the clip's length in bars, a menu like the parameter combos (it is
// part of the clip, so it is saved with the clips, not a parameter).
class ClipBarsControl : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ClipBarsControl (IlanaSynthAudioProcessor& processor, const juce::String& labelText)
        : processorRef (processor)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        addAndMakeVisible (label);

        for (int bars = 1; bars <= ClipState::maxBars; ++bars)
            combo.addItem (juce::String (bars) + (bars == 1 ? " bar" : " bars"), bars);

        addAndMakeVisible (combo);
        combo.onChange = [this]
        {
            if (updating)
                return;

            processorRef.performEdit ("Clip length", [this]
            {
                auto clip = processorRef.getClipState().getClip (currentIndex());
                clip.bars = combo.getSelectedId();
                processorRef.getClipState().setClip (currentIndex(), clip);
                processorRef.clipsEdited();
            });
        };
        setTooltip ("Length\nHow many bars (of four beats) the clip lasts before it loops.");
        combo.setTooltip (getTooltip());
        refresh();
    }

    // Shows the chosen clip's length (called on a timer by the page).
    void refresh()
    {
        const auto bars = processorRef.getClipState().getClip (currentIndex()).bars;

        if (combo.getSelectedId() != bars)
        {
            updating = true;
            combo.setSelectedId (bars, juce::dontSendNotification);
            updating = false;
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (13));
        combo.setBounds (area.removeFromTop (24));
    }

private:
    int currentIndex() const
    {
        return juce::jlimit (0, ClipState::numClips - 1, juce::roundToInt (GenerativeWidgets::read (processorRef, "clip_index")));
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::ComboBox combo;
    juce::Label label;
    bool updating = false;
};
