#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"
#include "GenerativeWidgets.h"

// The clip sequencer's piano roll: the notes of the clip chosen by CLIP,
// with a velocity lane under it. Double-click empty space to add a note (on
// the GRID, at the last velocity set), click a note to select it (shift-click
// adds), drag on empty space to select a group, drag notes to move them and
// a note's right edge to resize, right-click to delete. Keys: Ctrl+A, Delete,
// Ctrl+C / V / D, arrows nudge (shift: an octave or a bar). The wheel
// scrolls the pitches, Ctrl+wheel zooms in time, shift+wheel scrolls it.
// Edits go straight to the patch's clips (ClipState), one undo step per
// click, drag or key; a note placed or picked plays briefly.
class ClipEditor : public juce::Component,
                   public juce::SettableTooltipClient,
                   private IlanaAnim::FrameTimer
{
public:
    // GRID's choices, straight then triplet; a view setting, not saved.
    static constexpr int numGrids = 8;
    static constexpr int defaultGrid = 2; // 1/16

    static const char* gridName (int index)
    {
        static const char* const names[] = { "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "1/32 T" };
        return names[juce::jlimit (0, numGrids - 1, index)];
    }

    // In beats (quarter notes).
    static float gridBeats (int index)
    {
        index = juce::jlimit (0, numGrids - 1, index);
        return std::ldexp (1.0f, -(index % 4)) * (index >= 4 ? 2.0f / 3.0f : 1.0f);
    }

    ClipEditor (IlanaSynthAudioProcessor& processor, juce::Colour colourIn)
        : processorRef (processor), colour (colourIn)
    {
        setTooltip ("Clip piano roll. Double-click to add a note, click to select (shift adds), drag empty space to select "
                    "several. Drag to move, drag the right edge to resize, right-click to delete. Delete, Ctrl+C / V / D, "
                    "arrows nudge (shift: octave / bar). Drag the lane below for velocity. Wheel: pitch, Ctrl+wheel: zoom.");
        setWantsKeyboardFocus (true);
        reload (true);
        startTimerHz (30);
    }

    ~ClipEditor() override { stopAudition(); }

    void setGrid (int index)
    {
        gridIndex = juce::jlimit (0, numGrids - 1, index);
        repaint();
    }

    int getGrid() const { return gridIndex; }
    float getZoom() const { return zoom; }
    const Clip& getShownClip() const { return clip; }

    int getNumSelected() const { return (int) std::count (selected.begin(), selected.end(), true); }

    void selectAll()
    {
        selected.assign (clip.notes.size(), true);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);

        const auto on = GenerativeWidgets::read (processorRef, "clip_on") > 0.5f;
        const auto grid = gridBounds();
        const auto lane = laneBounds();
        const auto rows = visibleRows();
        const auto rowHeight = grid.getHeight() / (float) rows;
        const auto viewEnd = viewStart + viewBeats();

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

        // The velocity lane's floor and label.
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRect (lane);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("VEL", juce::Rectangle<float> (bounds.getX() + 4.0f, lane.getY(), keyWidth - 6.0f, lane.getHeight()).toNearestInt(),
                    juce::Justification::centredLeft);

        // Grid lines (when they are far enough apart), beats, the bars stronger.
        const auto step = gridStep();

        if (beatToX (step) - beatToX (0.0f) >= 5.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));

            for (auto k = std::ceil (viewStart / step - 1.0e-4f); k * step <= viewEnd + 1.0e-4f; k += 1.0f)
                g.fillRect (beatToX (k * step), grid.getY(), 1.0f, grid.getHeight());
        }

        for (auto beat = (int) std::ceil (viewStart - 1.0e-4f); (float) beat <= viewEnd + 1.0e-4f; ++beat)
        {
            const auto x = juce::jmin (grid.getRight() - 1.0f, beatToX ((float) beat));
            g.setColour (juce::Colours::white.withAlpha (beat % ClipState::beatsPerBar == 0 ? 0.16f : 0.06f));
            g.fillRect (x, grid.getY(), 1.0f, grid.getHeight());
            g.fillRect (x, lane.getY(), 1.0f, lane.getHeight());
        }

        // The root note (C3) plays the clip as written.
        if (ClipState::rootNote >= lowNote && ClipState::rootNote < lowNote + rows)
        {
            g.setColour (colour.withAlpha (0.35f));
            g.fillRect (grid.getX(), grid.getBottom() - (float) (ClipState::rootNote - lowNote) * rowHeight - 0.5f, grid.getWidth(), 1.0f);
        }

        {
            juce::Graphics::ScopedSaveState clipToGrid (g);
            juce::RectangleList<int> region (grid.toNearestInt());
            region.add (lane.expanded (3.0f, 3.0f).withRight (lane.getRight()).toNearestInt()); // a velocity head on beat 0 or at 127
            g.reduceClipRegion (region);

            for (size_t i = 0; i < clip.notes.size(); ++i)
            {
                const auto& n = clip.notes[i];
                const auto isSelected = i < selected.size() && selected[i];
                const auto shade = 0.45f + 0.55f * (float) n.velocity / 127.0f;
                const auto fill = (isSelected ? colour.interpolatedWith (juce::Colours::white, 0.3f) : colour)
                                      .withAlpha (shade * (on ? 1.0f : 0.6f));

                // Its velocity: a stem with a head at the level.
                const auto x = beatToX (n.start);

                if (x >= grid.getX() - 4.0f && x <= grid.getRight())
                {
                    const auto top = lane.getBottom() - (lane.getHeight() - headSize) * (float) n.velocity / 127.0f;
                    g.setColour (fill);
                    g.fillRect (x, top, 1.5f, lane.getBottom() - top);
                    g.fillEllipse (x + 0.75f - headSize * 0.5f, top - headSize * 0.5f, headSize, headSize);

                    if (isSelected)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.9f));
                        g.drawEllipse (x + 0.75f - headSize * 0.5f, top - headSize * 0.5f, headSize, headSize, 1.0f);
                    }
                }

                const auto r = noteBounds (n);

                if (! r.intersects (grid))
                    continue;

                g.setColour (fill);
                g.fillRoundedRectangle (r.reduced (0.0f, 0.5f), 2.0f);
                g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.95f : 0.25f));
                g.drawRoundedRectangle (r.reduced (0.0f, 0.5f), 2.0f, isSelected ? 1.5f : 1.0f);
            }
        }

        if (const auto playhead = processorRef.getClipPlayhead(); on && playhead >= viewStart && playhead <= viewEnd)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (beatToX (playhead), grid.getY(), 1.5f, grid.getHeight());
        }

        if (dragMode == Drag::select)
        {
            const auto band = juce::Rectangle<float> (bandStart, bandEnd).getIntersection (grid);
            g.setColour (colour.withAlpha (0.12f));
            g.fillRect (band);
            g.setColour (colour.withAlpha (0.7f));
            g.drawRect (band, 1.0f);
        }

        // Zoomed in: where the view sits in the clip, along the top edge.
        if (zoom > 1.001f)
        {
            const auto length = (float) lengthBeats();
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (grid.getX(), bounds.getY() + 1.0f, grid.getWidth(), 2.0f);
            g.setColour (colour.withAlpha (0.8f));
            g.fillRect (grid.getX() + grid.getWidth() * viewStart / length, bounds.getY() + 1.0f,
                        grid.getWidth() * viewBeats() / length, 2.0f);
        }

        // The keyboard strip's edge.
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (grid.getX() - 1.0f, grid.getY(), 1.0f, lane.getBottom() - grid.getY());

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
            g.drawText ("EMPTY: double-click to add notes", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto hit = noteAt (event.position);
        setMouseCursor (laneBounds().contains (event.position)          ? juce::MouseCursor::UpDownResizeCursor
                        : hit >= 0 && onResizeEdge (hit, event.position) ? juce::MouseCursor::LeftRightResizeCursor
                                                                          : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        reload (false);
        endGesture();

        if (isShowing())
            grabKeyboardFocus();

        const auto position = event.position;
        const auto grid = gridBounds();

        // The velocity lane: drag across the bars to set them (only the
        // selected notes when the bar grabbed is one of them).
        if (laneBounds().expanded (0.0f, 2.0f).contains (position) && position.x >= grid.getX())
        {
            onlySelected = false;

            for (size_t i = 0; i < clip.notes.size(); ++i)
                onlySelected = onlySelected || (selected[i] && std::abs (beatToX (clip.notes[i].start) - position.x) <= 4.0f);

            startGesture (Drag::velocity, "Clip velocity");
            lastVelocityX = position.x;
            paintVelocity (position.x, position.x, position.y);
            return;
        }

        const auto hit = noteAt (position);

        if (event.mods.isPopupMenu())
        {
            if (hit >= 0)
            {
                if (! selected[(size_t) hit])
                    selectOnly (hit);

                deleteSelected();
            }

            return;
        }

        if (hit >= 0)
        {
            if (event.mods.isShiftDown())
            {
                selected[(size_t) hit] = ! selected[(size_t) hit];

                if (! selected[(size_t) hit])
                {
                    repaint();
                    return;
                }
            }
            else if (! selected[(size_t) hit])
            {
                selectOnly (hit);
            }

            const auto& n = clip.notes[(size_t) hit];
            const auto resizing = onResizeEdge (hit, position);
            startGesture (resizing ? Drag::resize : Drag::move,
                          getNumSelected() > 1 ? (resizing ? "Resize clip notes" : "Move clip notes")
                                               : (resizing ? "Resize clip note" : "Move clip note"));
            dragHit = hit;
            dragOrigin = clip;
            grabBeat = xToBeat (position.x) - n.start;
            dragNote = n.note;
            audition (n.note, n.velocity);
            repaint();
            return;
        }

        if (! grid.contains (position))
            return;

        // A double-click on empty space adds a note in the grid cell under
        // it; dragging on moves it.
        if (event.getNumberOfClicks() >= 2)
        {
            if ((int) clip.notes.size() >= ClipState::maxNotes)
                return;

            startGesture (Drag::move, "Add clip note");
            ClipNote n;
            n.start = floorToGrid (xToBeat (position.x));
            n.length = lastLength;
            n.note = yToNote (position.y);
            n.velocity = lastVelocity;
            constrain (n);
            clip.notes.push_back (n);
            selectOnly ((int) clip.notes.size() - 1);
            dragHit = (int) clip.notes.size() - 1;
            dragOrigin = clip;
            grabBeat = xToBeat (position.x) - n.start;
            dragNote = n.note;
            commit();
            audition (n.note, n.velocity);
            return;
        }

        // A drag on empty space selects what it covers (shift adds to the
        // selection); a plain click clears it.
        dragMode = Drag::select;
        bandStart = bandEnd = position;
        bandBase = event.mods.isShiftDown() ? selected : std::vector<bool> (clip.notes.size(), false);
        selected = bandBase;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto position = event.position;

        if (dragMode == Drag::velocity)
        {
            paintVelocity (lastVelocityX, position.x, position.y);
            lastVelocityX = position.x;
            return;
        }

        if (dragMode == Drag::select)
        {
            bandEnd = position;
            const auto band = juce::Rectangle<float> (bandStart, bandEnd);

            for (size_t i = 0; i < clip.notes.size(); ++i)
                selected[i] = bandBase[i] || band.intersects (noteBounds (clip.notes[i]));

            repaint();
            return;
        }

        if ((dragMode != Drag::move && dragMode != Drag::resize) || dragHit < 0 || dragHit >= (int) dragOrigin.notes.size())
            return;

        const auto& origin = dragOrigin.notes[(size_t) dragHit];
        const auto step = gridStep();
        clip = dragOrigin;

        if (dragMode == Drag::resize)
        {
            const auto length = juce::jmax (step, snap (xToBeat (position.x) - origin.start + step * 0.5f));
            const auto delta = length - origin.length;

            for (size_t i = 0; i < clip.notes.size(); ++i)
                if (selected[i])
                {
                    clip.notes[i].length = juce::jmax (step, dragOrigin.notes[i].length + delta);
                    constrain (clip.notes[i]);
                }

            lastLength = clip.notes[(size_t) dragHit].length;
        }
        else
        {
            // The grabbed note lands on the grid; the others keep their
            // distance from it. Less than half a step from where it was
            // leaves it there (an imported note may sit off the grid).
            const auto raw = xToBeat (position.x) - grabBeat;
            auto beats = std::abs (raw - origin.start) < step * 0.5f ? 0.0f : snap (raw) - origin.start;
            auto notes = yToNote (position.y) - origin.note;
            limitShift (dragOrigin, beats, notes);

            for (size_t i = 0; i < clip.notes.size(); ++i)
                if (selected[i])
                {
                    clip.notes[i].start += beats;
                    clip.notes[i].note += notes;
                    constrain (clip.notes[i]);
                }

            if (origin.note + notes != dragNote)
            {
                dragNote = origin.note + notes;
                audition (dragNote, origin.velocity);
            }
        }

        commit();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endGesture();
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        const auto delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
        const auto direction = delta > 0.0f ? 1 : delta < 0.0f ? -1 : 0;

        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            // Zoom in time around the pointer.
            const auto anchor = xToBeat (event.position.x);
            const auto fraction = (event.position.x - gridBounds().getX()) / gridBounds().getWidth();
            zoom *= direction > 0 ? 1.25f : direction < 0 ? 0.8f : 1.0f;
            clampView();
            viewStart = anchor - fraction * viewBeats();
            clampView();
        }
        else if (event.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
        {
            viewStart -= (float) direction * viewBeats() * 0.125f;
            clampView();
        }
        else
        {
            const auto rows = visibleRows();
            lowNote = juce::jlimit (0, 128 - rows, lowNote + direction * 3);
        }

        repaint();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        reload (false);
        const auto modifiers = key.getModifiers();
        const auto code = key.getKeyCode();
        const auto letter = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) code);
        const auto any = getNumSelected() > 0;

        if (modifiers.isCommandDown() || modifiers.isCtrlDown())
        {
            if (letter == 'A')
                selectAll();
            else if (letter == 'C')
                copySelection();
            else if (letter == 'V')
                paste();
            else if (letter == 'D')
                duplicateSelection();
            else
                return false; // Ctrl+Z and the rest belong to the editor

            return true;
        }

        if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        {
            deleteSelected();
            return any;
        }

        if (code == juce::KeyPress::escapeKey && any)
        {
            selected.assign (clip.notes.size(), false);
            repaint();
            return true;
        }

        const auto shift = modifiers.isShiftDown();

        if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
            return nudge (0.0f, (code == juce::KeyPress::upKey ? 1 : -1) * (shift ? 12 : 1));

        if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
            return nudge ((code == juce::KeyPress::rightKey ? 1.0f : -1.0f) * (shift ? (float) ClipState::beatsPerBar : gridStep()), 0);

        return false;
    }

    // The clip shown follows the CLIP choice; a new clip centres on its notes.
    void reload (bool force)
    {
        if (dragMode != Drag::none && ! force)
            return;

        const auto index = juce::jlimit (0, ClipState::numClips - 1, juce::roundToInt (GenerativeWidgets::read (processorRef, "clip_index")));
        const auto epoch = processorRef.getDataEpoch();

        if (! force && index == shownIndex && epoch == shownEpoch)
            return;

        const auto changedClip = index != shownIndex;
        shownIndex = index;
        shownEpoch = epoch;
        clip = processorRef.getClipState().getClip (index);
        selected.assign (clip.notes.size(), false); // the order may have changed
        clampView();

        if (changedClip || force)
            centreOnNotes();

        repaint();
    }

    // Edits of the selection, one undo step each (the keys call them).
    void deleteSelected()
    {
        if (getNumSelected() == 0)
            return;

        processorRef.performEdit (getNumSelected() > 1 ? "Delete clip notes" : "Delete clip note", [this]
        {
            Clip kept = clip;
            kept.notes.clear();

            for (size_t i = 0; i < clip.notes.size(); ++i)
                if (! selected[i])
                    kept.notes.push_back (clip.notes[i]);

            clip = std::move (kept);
            selected.assign (clip.notes.size(), false);
            commit();
        });
    }

    void copySelection()
    {
        if (getNumSelected() == 0)
            return;

        const auto range = selectionRange();
        clipboard().notes.clear();

        for (size_t i = 0; i < clip.notes.size(); ++i)
            if (selected[i])
            {
                auto n = clip.notes[i];
                n.start -= range.first;
                clipboard().notes.push_back (n);
            }

        clipboard().end = range.second;
    }

    // At the playhead while the clip plays, else just after the selection
    // (or after where the notes were copied from).
    void paste()
    {
        if (clipboard().notes.empty())
            return;

        const auto playhead = processorRef.getClipPlayhead();
        const auto on = GenerativeWidgets::read (processorRef, "clip_on") > 0.5f;
        const auto at = on && playhead >= 0.0f ? floorToGrid (playhead)
                      : getNumSelected() > 0  ? selectionRange().second
                                              : clipboard().end;
        insert (clipboard().notes, at, "Paste clip notes");
    }

    // A copy of the selection right after it.
    void duplicateSelection()
    {
        if (getNumSelected() == 0)
            return;

        const auto range = selectionRange();
        std::vector<ClipNote> notes;

        for (size_t i = 0; i < clip.notes.size(); ++i)
            if (selected[i])
            {
                auto n = clip.notes[i];
                n.start -= range.first;
                notes.push_back (n);
            }

        insert (notes, range.second, "Duplicate clip notes");
    }

    // Moves the selection by beats and semitones, as far as it fits.
    bool nudge (float beats, int notes)
    {
        if (getNumSelected() == 0)
            return false;

        limitShift (clip, beats, notes);

        if (beats == 0.0f && notes == 0)
            return true;

        processorRef.performEdit ("Nudge clip notes", [&]
        {
            for (size_t i = 0; i < clip.notes.size(); ++i)
                if (selected[i])
                {
                    clip.notes[i].start += beats;
                    clip.notes[i].note += notes;
                    constrain (clip.notes[i]);
                }

            commit();
        });

        if (notes != 0)
            for (size_t i = 0; i < clip.notes.size(); ++i)
                if (selected[i])
                {
                    audition (clip.notes[i].note, clip.notes[i].velocity);
                    break;
                }

        return true;
    }

private:
    static constexpr float keyWidth = 34.0f;
    static constexpr float headSize = 5.0f; // a velocity stem's head
    static constexpr int auditionMs = 250;
    enum class Drag { none, move, resize, velocity, select };

    struct Clipboard
    {
        std::vector<ClipNote> notes; // starts from the copied range's start
        float end = 0.0f;            // where the copied range ended
    };

    // Shared by every piano roll, so notes copy between clips.
    static Clipboard& clipboard()
    {
        static Clipboard shared;
        return shared;
    }

    void timerCallback() override
    {
        if (auditionNote >= 0 && juce::Time::getMillisecondCounter() >= auditionEnd)
            stopAudition();

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
    float gridStep() const { return gridBeats (gridIndex); }
    float viewBeats() const { return (float) lengthBeats() / zoom; }

    // Zoom stops at one beat across; the view stays inside the clip.
    void clampView()
    {
        const auto length = (float) lengthBeats();
        zoom = juce::jlimit (1.0f, juce::jmax (1.0f, length), zoom);
        viewStart = juce::jlimit (0.0f, length - viewBeats(), viewStart);
    }

    float laneHeight() const { return juce::jlimit (18.0f, 44.0f, ((float) getHeight() - 8.0f) * 0.22f); }

    juce::Rectangle<float> gridBounds() const
    {
        auto area = getLocalBounds().toFloat().reduced (0.0f, 4.0f);
        return area.withTrimmedLeft (keyWidth).withTrimmedRight (6.0f).withTrimmedBottom (laneHeight() + 4.0f);
    }

    juce::Rectangle<float> laneBounds() const
    {
        const auto grid = gridBounds();
        return { grid.getX(), grid.getBottom() + 4.0f, grid.getWidth(), laneHeight() };
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

    float beatToX (float beat) const { return gridBounds().getX() + gridBounds().getWidth() * (beat - viewStart) / viewBeats(); }
    float xToBeat (float x) const { return viewStart + (x - gridBounds().getX()) / gridBounds().getWidth() * viewBeats(); }
    float snap (float beat) const { return std::floor (beat / gridStep() + 0.5f) * gridStep(); }
    float floorToGrid (float beat) const { return std::floor (beat / gridStep() + 1.0e-3f) * gridStep(); }
    float ceilToGrid (float beat) const { return std::ceil (beat / gridStep() - 1.0e-3f) * gridStep(); }

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
        if (! gridBounds().contains (position))
            return -1;

        for (int i = (int) clip.notes.size() - 1; i >= 0; --i)
            if (noteBounds (clip.notes[(size_t) i]).expanded (0.0f, 0.5f).contains (position))
                return i;

        return -1;
    }

    bool onResizeEdge (int index, juce::Point<float> position) const
    {
        const auto r = noteBounds (clip.notes[(size_t) index]);
        return position.x > r.getRight() - juce::jmin (6.0f, r.getWidth() * 0.4f);
    }

    // The shortest note: a grid step, at most a sixteenth.
    float shortestNote() const { return juce::jmin (gridStep(), 0.25f); }

    void constrain (ClipNote& n) const
    {
        const auto length = (float) lengthBeats();
        n.note = juce::jlimit (0, 127, n.note);
        n.velocity = juce::jlimit (1, 127, n.velocity);
        n.start = juce::jlimit (0.0f, length - shortestNote(), n.start);
        n.length = juce::jlimit (shortestNote(), length - n.start, n.length);
    }

    void selectOnly (int index)
    {
        selected.assign (clip.notes.size(), false);
        selected[(size_t) index] = true;
        repaint();
    }

    // The selected notes' span, out to the grid on both sides.
    std::pair<float, float> selectionRange() const
    {
        auto from = (float) lengthBeats(), to = 0.0f;

        for (size_t i = 0; i < clip.notes.size(); ++i)
            if (selected[i])
            {
                from = juce::jmin (from, clip.notes[i].start);
                to = juce::jmax (to, clip.notes[i].start + clip.notes[i].length);
            }

        from = floorToGrid (from);
        return { from, juce::jmax (from + gridStep(), ceilToGrid (to)) };
    }

    // Shrinks a shift of the selected notes so none leaves the clip or the keyboard.
    void limitShift (const Clip& from, float& beats, int& notes) const
    {
        const auto latest = (float) lengthBeats() - shortestNote();

        for (size_t i = 0; i < from.notes.size(); ++i)
            if (selected[i])
            {
                const auto& n = from.notes[i];
                beats = juce::jlimit (-n.start, juce::jmax (0.0f, latest - n.start), beats);
                notes = juce::jlimit (-n.note, 127 - n.note, notes);
            }
    }

    // Adds notes (starts relative to `at`) as one step and selects them;
    // those past the clip's end are left out.
    void insert (const std::vector<ClipNote>& notes, float at, const juce::String& name)
    {
        processorRef.performEdit (name, [&]
        {
            selected.assign (clip.notes.size(), false);

            for (auto n : notes)
            {
                n.start += at;

                if (n.start >= (float) lengthBeats() - 1.0e-3f || (int) clip.notes.size() >= ClipState::maxNotes)
                    continue;

                constrain (n);
                clip.notes.push_back (n);
                selected.push_back (true);
            }

            commit();
        });
    }

    // Sets the velocity of the bars between two x positions from a y in the lane.
    void paintVelocity (float x0, float x1, float y)
    {
        const auto lane = laneBounds();
        const auto velocity = juce::jlimit (1, 127, juce::roundToInt ((lane.getBottom() - y) / (lane.getHeight() - headSize) * 127.0f));
        const auto low = juce::jmin (x0, x1) - 4.0f, high = juce::jmax (x0, x1) + 4.0f;
        auto changed = false;

        for (size_t i = 0; i < clip.notes.size(); ++i)
        {
            const auto x = beatToX (clip.notes[i].start);

            if (x < low || x > high || (onlySelected && ! selected[i]))
                continue;

            clip.notes[i].velocity = velocity;
            changed = true;
        }

        lastVelocity = velocity;

        if (changed)
            commit();
    }

    void startGesture (Drag mode, const juce::String& name)
    {
        dragMode = mode;
        processorRef.beginEdit (name); // closed on mouse-up: one undo step
        editOpen = true;
    }

    void endGesture()
    {
        dragMode = Drag::none;
        dragHit = -1;

        if (editOpen)
        {
            editOpen = false;
            processorRef.endEdit();
        }
    }

    // A short preview through the on-screen keyboard's queue. Not in Key
    // transpose while the clip is on: there a held key starts the clip.
    void audition (int note, int velocity)
    {
        stopAudition();

        if (GenerativeWidgets::read (processorRef, "clip_on") > 0.5f
            && juce::roundToInt (GenerativeWidgets::read (processorRef, "clip_mode")) == 0)
            return;

        processorRef.triggerPreviewNote (note, true, (float) velocity / 127.0f);
        auditionNote = note;
        auditionEnd = juce::Time::getMillisecondCounter() + (juce::uint32) auditionMs;
    }

    void stopAudition()
    {
        if (auditionNote < 0)
            return;

        processorRef.triggerPreviewNote (auditionNote, false);
        auditionNote = -1;
    }

    // Publishes the edit; the notes stay in the order they were made while
    // the editor shows them (ClipState sorts its own copy).
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
    Clip clip, dragOrigin;
    std::vector<bool> selected, bandBase; // one per note of `clip`
    int shownIndex = -1;
    unsigned shownEpoch = 0;
    int lowNote = 48;
    int gridIndex = defaultGrid;
    float zoom = 1.0f, viewStart = 0.0f;
    Drag dragMode = Drag::none;
    bool editOpen = false, onlySelected = false;
    int dragHit = -1, dragNote = -1;
    float grabBeat = 0.0f, lastLength = 1.0f, lastVelocityX = 0.0f;
    int lastVelocity = 100;
    juce::Point<float> bandStart, bandEnd;
    int auditionNote = -1;
    juce::uint32 auditionEnd = 0;
};

// GRID: the piano roll's snap, a menu like the parameter combos (a view
// setting of the roll, not part of the patch).
class ClipGridControl : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ClipGridControl (ClipEditor& editorIn, const juce::String& labelText)
        : editor (editorIn)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        addAndMakeVisible (label);

        for (int i = 0; i < ClipEditor::numGrids; ++i)
        {
            if (i == 4)
                combo.addSeparator();

            combo.addItem (ClipEditor::gridName (i), i + 1);
        }

        combo.setSelectedId (editor.getGrid() + 1, juce::dontSendNotification);
        combo.onChange = [this] { editor.setGrid (combo.getSelectedId() - 1); };
        addAndMakeVisible (combo);
        setTooltip ("Grid\nWhere the piano roll's notes snap, and how far the left / right arrows nudge them (T: triplets).");
        combo.setTooltip (getTooltip());
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (13));
        combo.setBounds (area.removeFromTop (24));
    }

    juce::ComboBox& getCombo() { return combo; }

private:
    ClipEditor& editor;
    juce::ComboBox combo;
    juce::Label label;
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
