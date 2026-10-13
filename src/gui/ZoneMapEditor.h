#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/OscillatorIds.h"
#include "../dsp/SamplePlayer.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

// Roadmap step 36: the zone map. A modal overlay (shown like WavetableEditor)
// that edits the zones of one oscillator's multisample: keys run across
// (0..127, a keyboard strip along the bottom), velocity runs up (1..127), and
// each zone is a box. Drag a box's edges to resize it, drag inside to move
// it; the readouts and buttons under the map set the selected zone's root
// note, level and sample, and three parameters set what plays outside the
// zones, how alternates are chosen and how much velocity layers blend.
//
// Edits are made on a local copy while the mouse is down and reach the sound
// in one setSampleZones call when the gesture ends (one undo step each).
//
// Wiring: construct with the processor, the oscillator's index and its colour;
// size it like WavetableEditor (the editor's content bounds, reduced), add it
// as a child, set onClose to remove it (called asynchronously from the CLOSE
// button and Escape), and give it keyboard focus.
class ZoneMapEditor : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    ZoneMapEditor (IlanaSynthAudioProcessor& p, int oscIndex, juce::Colour colourIn)
        : processorRef (p),
          osc (juce::jlimit (0, OscillatorIds::count - 1, oscIndex)),
          colour (colourIn),
          prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)])
    {
        for (auto* button : { &closeButton, &auditionButton, &deleteButton, &addButton })
            addAndMakeVisible (*button);

        closeButton.onClick = [this] { close(); };
        deleteButton.onClick = [this] { deleteSelected(); };
        addButton.onClick = [this] { chooseSample(); };
        auditionButton.onStateChange = [this]
        {
            if (auditionButton.isDown())
                startPreview (selected);
            else
                stopPreview();
        };
        closeButton.setTooltip ("Close the zone map (Escape)");
        auditionButton.setTooltip ("Hold to hear the selected zone at its root note");
        deleteButton.setTooltip ("Remove the selected zone (Delete)");
        addButton.setTooltip ("Add an audio file as a zone: over the selected zone's keys, or over the free keys around middle C");

        for (auto* field : { &keysField, &velocityField, &rootField, &levelField })
        {
            field->owner = this;
            addAndMakeVisible (*field);
        }
        keysField.name = "KEYS";
        velocityField.name = "VELOCITY";
        rootField.name = "ROOT";
        levelField.name = "LEVEL";

        rootField.setTooltip ("The note the sample plays at its own pitch. Drag or scroll for a semitone, hold Shift for cents, double-click to type a note");
        levelField.setTooltip ("This zone's level in dB. Drag or scroll, hold Shift for fine steps, double-click to type");
        keysField.setTooltip ("The zone's lowest and highest key. Drag its left and right edges on the map");
        velocityField.setTooltip ("The zone's lowest and highest velocity. Drag its bottom and top edges on the map");

        rootField.adjustable = true;
        rootField.onStep = [this] (int steps, bool fine) { editSelected ([&] (SampleZone& z) { z.rootNote = juce::jlimit (0.0, 127.0, z.rootNote + (fine ? 0.01 : 1.0) * steps); }); };
        rootField.onGestureEnd = [this] { commit ("Change zone root"); };
        rootField.getEditText = [this] { return selectedValid() ? rootText (zones[(size_t) selected].rootNote) : juce::String(); };
        rootField.onTextEntered = [this] (const juce::String& text)
        {
            double value = 0.0;
            if (parseNote (text, value))
            {
                editSelected ([&] (SampleZone& z) { z.rootNote = value; });
                commit ("Change zone root");
            }
        };

        levelField.adjustable = true;
        levelField.onStep = [this] (int steps, bool fine)
        {
            editSelected ([&] (SampleZone& z)
            {
                const auto db = juce::jlimit (-48.0f, 12.0f, juce::Decibels::gainToDecibels (z.gain, -100.0f) + (fine ? 0.1f : 0.5f) * (float) steps);
                z.gain = juce::Decibels::decibelsToGain (db);
            });
        };
        levelField.onGestureEnd = [this] { commit ("Change zone level"); };
        levelField.getEditText = [this] { return selectedValid() ? juce::String (juce::Decibels::gainToDecibels (zones[(size_t) selected].gain, -100.0f), 1) : juce::String(); };
        levelField.onTextEntered = [this] (const juce::String& text)
        {
            const auto db = juce::jlimit (-48.0f, 12.0f, text.retainCharacters ("-+.0123456789").getFloatValue());
            editSelected ([&] (SampleZone& z) { z.gain = juce::Decibels::decibelsToGain (db); });
            commit ("Change zone level");
        };

        auto& state = processorRef.apvts;
        const auto hasParameter = [&state] (const juce::String& id) { return state.getParameter (id) != nullptr; };

        if (hasParameter (prefix + "_sample_unmapped"))
        {
            unmappedControl = std::make_unique<ComboControl> (state, prefix + "_sample_unmapped", "OUTSIDE ZONES");
            addAndMakeVisible (*unmappedControl);
        }
        if (hasParameter (prefix + "_sample_round_robin"))
        {
            roundRobinControl = std::make_unique<ComboControl> (state, prefix + "_sample_round_robin", "ROUND ROBIN");
            addAndMakeVisible (*roundRobinControl);
        }
        if (hasParameter (prefix + "_sample_vel_blend"))
        {
            blendControl = std::make_unique<KnobControl> (state, prefix + "_sample_vel_blend", "VELOCITY BLEND", colour, false);
            blendControl->setInlineKnob (true);
            addAndMakeVisible (*blendControl);
        }

        setWantsKeyboardFocus (true);
        reload (true);
        startTimerHz (10);
    }

    ~ZoneMapEditor() override
    {
        stopTimer();
        stopPreview();
    }

    std::function<void()> onClose;

    int getOscIndex() const { return osc; }
    const std::vector<SampleZone>& getZones() const { return zones; }
    int getSelectedZone() const { return selected; }

    void selectZone (int index)
    {
        selected = (index >= 0 && index < (int) zones.size()) ? index : -1;
        refreshFields();
        repaint();
    }

    // ---------------------------------------------------------------- paint

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xf0101014));
        g.fillRoundedRectangle (bounds, 8.0f);
        IlanaTheme::paintCard (g, bounds.reduced (2.0f), 8.0f, colour);

        const auto dot = juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "));
        g.setColour (colour);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        const auto title = juce::String ("ZONES") + dot + "OSC " + juce::String (osc + 1) + dot + sampleName;
        IlanaTheme::drawFitted (g, title, titleArea, juce::Justification::centredLeft, 1);

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        IlanaTheme::drawFitted (g, "Drag edges to resize, drag a box to move it, drop audio files on the map to add zones",
                                hintArea, juce::Justification::centredRight, 2);

        IlanaTheme::paintWell (g, wellBounds, 6.0f);
        paintMap (g);
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

        // The controls: a row of readouts and buttons, and a row of the
        // three parameters. Every cell is one control high (13 name + 24 box).
        const auto cell = 37;
        const auto hasParameters = unmappedControl != nullptr || roundRobinControl != nullptr || blendControl != nullptr;

        juce::Rectangle<int> parameterRow;
        if (hasParameters)
        {
            parameterRow = area.removeFromBottom (cell);
            area.removeFromBottom (gap);
        }

        auto row1 = area.removeFromBottom (cell);
        area.removeFromBottom (gap);

        const auto room = row1.getWidth() - gap * 6;
        const auto fieldWidth = juce::roundToInt ((float) room * 0.15f);
        for (auto* field : { &keysField, &velocityField, &rootField, &levelField })
        {
            field->setBounds (row1.removeFromLeft (fieldWidth));
            row1.removeFromLeft (gap);
        }
        auditionButton.setBounds (row1.removeFromLeft (juce::roundToInt ((float) room * 0.11f)).removeFromBottom (24));
        row1.removeFromLeft (gap);
        deleteButton.setBounds (row1.removeFromLeft (juce::roundToInt ((float) room * 0.09f)).removeFromBottom (24));
        row1.removeFromLeft (gap);
        addButton.setBounds (row1.removeFromBottom (24));

        // The map and its keyboard strip share one well.
        wellBounds = area.toFloat();
        auto inner = wellBounds.reduced (8.0f);
        stripBounds = inner.removeFromBottom (stripHeight);
        inner.removeFromBottom (4.0f);
        mapBounds = inner;
        stripBounds = stripBounds.withX (mapBounds.getX()).withWidth (mapBounds.getWidth());

        layoutParameterRow (parameterRow);
    }

    // ---------------------------------------------------------------- mouse

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto hit = hitZone (event.position, selected);
        if (hit.zone < 0)
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            return;
        }
        const bool horizontal = (hit.mode & (modeLeft | modeRight)) != 0;
        const bool vertical = (hit.mode & (modeTop | modeBottom)) != 0;
        setMouseCursor (horizontal && vertical ? juce::MouseCursor::UpDownLeftRightResizeCursor
                        : horizontal ? juce::MouseCursor::LeftRightResizeCursor
                        : vertical ? juce::MouseCursor::UpDownResizeCursor
                                   : juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        dragMode = 0;
        dragMoved = false;

        // Every zone under the mouse, the topmost first.
        std::vector<int> hits;
        for (int i = (int) zones.size() - 1; i >= 0; --i)
            if (zoneBox (zones[(size_t) i]).expanded (3.0f).contains (event.position))
                hits.push_back (i);

        if (hits.empty())
        {
            selectZone (-1);
            lastClick = { -100.0f, -100.0f };
            return;
        }

        int chosen = hits.front();
        const auto selectedHit = std::find (hits.begin(), hits.end(), selected);
        if (selectedHit != hits.end())
        {
            const auto onEdge = (hitZone (event.position, selected).mode & edgeMask) != 0;
            if (onEdge)
                chosen = selected;
            else if (event.position.getDistanceFrom (lastClick) <= 4.0f && hits.size() > 1)
                chosen = *(std::next (selectedHit) == hits.end() ? hits.begin() : std::next (selectedHit)); // the next one down
        }

        lastClick = event.position;
        selectZone (chosen);

        const auto hit = hitZone (event.position, chosen);
        dragMode = hit.zone == chosen ? hit.mode : modeMove;
        dragStart = event.position;
        dragOrigin = zones[(size_t) chosen];
        startPreview (chosen);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! selectedValid() || dragMode == 0)
            return;
        if (! dragMoved && event.getDistanceFromDragStart() < 3)
            return;
        dragMoved = true;

        const auto stepsX = (int) std::lround ((event.position.x - dragStart.x) / keyWidth());
        const auto stepsY = (int) std::lround (-(event.position.y - dragStart.y) / velocityHeight());
        auto z = dragOrigin;

        if (dragMode == modeMove)
        {
            const auto keys = dragOrigin.hiKey - dragOrigin.loKey;
            const auto vels = dragOrigin.hiVel - dragOrigin.loVel;
            z.loKey = juce::jlimit (0, 127 - keys, dragOrigin.loKey + stepsX);
            z.hiKey = z.loKey + keys;
            z.loVel = juce::jlimit (1, 127 - vels, dragOrigin.loVel + stepsY);
            z.hiVel = z.loVel + vels;
        }
        else
        {
            if (dragMode & modeLeft)   z.loKey = juce::jlimit (0, dragOrigin.hiKey, dragOrigin.loKey + stepsX);
            if (dragMode & modeRight)  z.hiKey = juce::jlimit (dragOrigin.loKey, 127, dragOrigin.hiKey + stepsX);
            if (dragMode & modeTop)    z.hiVel = juce::jlimit (dragOrigin.loVel, 127, dragOrigin.hiVel + stepsY);
            if (dragMode & modeBottom) z.loVel = juce::jlimit (1, dragOrigin.hiVel, dragOrigin.loVel + stepsY);
        }

        zones[(size_t) selected] = z;
        refreshFields();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        stopPreview();
        if (dragMoved)
            commit ("Edit zone");
        dragMoved = false;
        dragMode = 0;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        for (int i = (int) zones.size() - 1; i >= 0; --i)
            if (zoneBox (zones[(size_t) i]).contains (event.position))
            {
                auditionFor (i, 700);
                return;
            }
    }

    // ------------------------------------------------------------- keyboard

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            deleteSelected();
            return true;
        }

        const auto left = key.isKeyCode (juce::KeyPress::leftKey), right = key.isKeyCode (juce::KeyPress::rightKey),
                   up = key.isKeyCode (juce::KeyPress::upKey), down = key.isKeyCode (juce::KeyPress::downKey);
        if (! (left || right || up || down))
            return false;
        if (! selectedValid())
            return true;

        const auto sign = (right || up) ? 1 : -1;
        const auto shift = key.getModifiers().isShiftDown();
        if (shift)
        {
            editSelected ([&] (SampleZone& z) { z.rootNote = juce::jlimit (0.0, 127.0, z.rootNote + ((left || right) ? 1.0 : 0.1) * sign); });
            commit ("Change zone root");
        }
        else if (left || right)
        {
            editSelected ([&] (SampleZone& z)
            {
                const auto step = juce::jlimit (-z.loKey, 127 - z.hiKey, sign);
                z.loKey += step;
                z.hiKey += step;
            });
            commit ("Move zone");
        }
        else
        {
            editSelected ([&] (SampleZone& z)
            {
                const auto step = juce::jlimit (1 - z.loVel, 127 - z.hiVel, sign);
                z.loVel += step;
                z.hiVel += step;
            });
            commit ("Move zone");
        }
        return true;
    }

    // ----------------------------------------------------------- file drops

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& path : files)
            if (isAudioFile (juce::File (path)))
                return true;
        return false;
    }

    void fileDragEnter (const juce::StringArray&, int x, int) override { dropKey = keyAtX ((float) x); repaint(); }
    void fileDragMove (const juce::StringArray&, int x, int) override
    {
        const auto key = keyAtX ((float) x);
        if (key != dropKey)
        {
            dropKey = key;
            repaint();
        }
    }
    void fileDragExit (const juce::StringArray&) override { dropKey = -1; repaint(); }

    void filesDropped (const juce::StringArray& files, int x, int) override
    {
        const auto first = keyAtX ((float) x);
        dropKey = -1;

        int added = 0;
        for (const auto& path : files)
        {
            const juce::File file (path);
            if (! isAudioFile (file))
                continue;
            const auto key = juce::jmin (127, first + added);
            if (! addFile (file, key, key, key))
                break;
            ++added;
        }
        reload (false);
        if (added > 0)
            selectZone ((int) zones.size() - 1);
        repaint();
    }

private:
    static constexpr int modeLeft = 1, modeRight = 2, modeTop = 4, modeBottom = 8, modeMove = 16;
    static constexpr int edgeMask = modeLeft | modeRight | modeTop | modeBottom;
    static constexpr float stripHeight = 18.0f;

    // ---------------------------------------------------------------- field

    // A caption over a value box: the same name style as ComboControl, so a
    // row of fields and menus reads as one row. ROOT and LEVEL adjust by drag,
    // wheel and typing.
    struct Field : juce::Component, juce::SettableTooltipClient
    {
        ZoneMapEditor* owner = nullptr;
        juce::String name, text;
        bool adjustable = false;
        std::function<void (int steps, bool fine)> onStep;
        std::function<void()> onGestureEnd;
        std::function<juce::String()> getEditText;
        std::function<void (const juce::String&)> onTextEntered;

        Field()
        {
            editor.setJustification (juce::Justification::centredLeft);
            editor.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, false, true));
            editor.setIndents (8, 0);
            editor.setBorder ({ 0, 0, 0, 0 });
            editor.setColour (juce::TextEditor::backgroundColourId, IlanaTheme::Ui::well);
            editor.setColour (juce::TextEditor::textColourId, IlanaTheme::Ui::text);
            editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            editor.onReturnKey = [this] { finishEditing (true); };
            editor.onEscapeKey = [this] { finishEditing (false); };
            editor.onFocusLost = [this] { finishEditing (false); };
            addChildComponent (editor);
        }

        void setText (const juce::String& newText)
        {
            if (text == newText)
                return;
            text = newText;
            repaint();
        }

        juce::Rectangle<int> boxBounds() const { return getLocalBounds().withTrimmedTop (13); }

        void resized() override { editor.setBounds (boxBounds()); }

        void paint (juce::Graphics& g) override
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true).withKerningFactor (0.07f));
            g.drawText (name, getLocalBounds().removeFromTop (13).withTrimmedLeft (4), juce::Justification::centredLeft, false);

            const auto box = boxBounds().toFloat();
            const auto live = text.isNotEmpty();
            g.setColour (IlanaTheme::Ui::raised);
            g.fillRoundedRectangle (box, IlanaLookAndFeel::buttonRadius (box.getHeight()));
            g.setColour (adjustable && isMouseOver() ? IlanaTheme::Ui::hover : IlanaTheme::Ui::line);
            g.drawRoundedRectangle (box.reduced (0.5f), IlanaLookAndFeel::buttonRadius (box.getHeight()), 1.0f);
            g.setColour (live ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, false, true));
            IlanaTheme::drawFitted (g, live ? text : juce::String ("-"), boxBounds().reduced (8, 0), juce::Justification::centredLeft, 1);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            owner->grabKeyboardFocus();
            lastY = event.position.y;
            accumulated = 0.0f;
            moved = false;
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (! adjustable || onStep == nullptr || text.isEmpty())
                return;
            accumulated += lastY - event.position.y;
            lastY = event.position.y;
            const auto pixels = event.mods.isShiftDown() ? 2.0f : 6.0f;
            while (std::abs (accumulated) >= pixels)
            {
                const auto direction = accumulated > 0.0f ? 1 : -1;
                onStep (direction, event.mods.isShiftDown());
                accumulated -= (float) direction * pixels;
                moved = true;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (moved && onGestureEnd != nullptr)
                onGestureEnd();
            moved = false;
        }

        void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
        {
            if (! adjustable || onStep == nullptr || text.isEmpty() || wheel.deltaY == 0.0f)
                return;
            onStep (wheel.deltaY > 0.0f ? 1 : -1, event.mods.isShiftDown());
            if (onGestureEnd != nullptr)
                onGestureEnd();
        }

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (getEditText == nullptr || onTextEntered == nullptr)
                return;
            const auto current = getEditText();
            if (current.isEmpty())
                return;
            editor.setText (current, false);
            editor.setVisible (true);
            editor.grabKeyboardFocus();
            editor.selectAll();
        }

    private:
        void finishEditing (bool accept)
        {
            if (! editor.isVisible())
                return;
            const auto entered = editor.getText();
            editor.setVisible (false);
            owner->grabKeyboardFocus();
            if (accept && onTextEntered != nullptr)
                onTextEntered (entered);
        }

        juce::TextEditor editor;
        float lastY = 0.0f, accumulated = 0.0f;
        bool moved = false;
    };

    struct Hit
    {
        int zone = -1;
        int mode = 0;
    };

    // ---------------------------------------------------------------- state

    bool selectedValid() const { return selected >= 0 && selected < (int) zones.size(); }

    template <typename Change>
    void editSelected (Change&& change)
    {
        if (! selectedValid())
            return;
        change (zones[(size_t) selected]);
        refreshFields();
        repaint();
    }

    // Sends the local zones to the processor: one call per finished gesture.
    void commit (const juce::String& undoName)
    {
        if (processorRef.setSampleZones (osc, zones, undoName))
            lastEpoch = processorRef.getSampleEpoch();
        else
            reload (true);
    }

    void reload (bool force)
    {
        const auto epoch = processorRef.getSampleEpoch();
        if (! force && epoch == lastEpoch)
            return;
        lastEpoch = epoch;

        const auto previousCount = zones.size();
        zones = processorRef.getSampleZones (osc);
        if (zones.size() != previousCount && selected >= (int) zones.size())
            selected = (int) zones.size() - 1;
        if (selected >= (int) zones.size())
            selected = (int) zones.size() - 1;
        if (selected < 0 && ! zones.empty() && zones.size() != previousCount)
            selected = 0;

        const auto* sample = processorRef.getSampleForOsc (osc);
        sampleName = sample != nullptr && sample->name.isNotEmpty() ? sample->name : juce::String ("no sample");
        refreshFields();
        repaint();
    }

    void refreshFields()
    {
        const auto valid = selectedValid();
        if (valid)
        {
            const auto& z = zones[(size_t) selected];
            const auto to = juce::String (" to ");
            keysField.setText (noteName (z.loKey) + to + noteName (z.hiKey));
            velocityField.setText (juce::String (z.loVel) + to + juce::String (z.hiVel));
            rootField.setText (rootText (z.rootNote));
            levelField.setText (juce::String (juce::Decibels::gainToDecibels (z.gain, -100.0f), 1) + " dB");
        }
        else
        {
            for (auto* field : { &keysField, &velocityField, &rootField, &levelField })
                field->setText ({});
        }
        auditionButton.setEnabled (valid);
        deleteButton.setEnabled (valid);
    }

    void deleteSelected()
    {
        if (! selectedValid())
            return;
        zones.erase (zones.begin() + selected);
        selected = juce::jmin (selected, (int) zones.size() - 1);
        refreshFields();
        repaint();
        commit ("Delete zone");
    }

    void close()
    {
        stopPreview();
        if (onClose != nullptr)
        {
            auto callback = onClose;
            juce::MessageManager::callAsync (callback);
        }
    }

    void timerCallback() override
    {
        if (dragMoved || chooser != nullptr)
            return;
        reload (false);
    }

    // -------------------------------------------------------------- preview

    void startPreview (int zoneIndex)
    {
        stopPreview();
        if (zoneIndex < 0 || zoneIndex >= (int) zones.size())
            return;
        const auto& z = zones[(size_t) zoneIndex];
        previewNote = juce::jlimit (0, 127, juce::roundToInt ((float) z.rootNote));
        processorRef.triggerPreviewNote (previewNote, true, (float) (z.loVel + z.hiVel) * 0.5f / 127.0f);
    }

    void stopPreview()
    {
        if (previewNote >= 0)
            processorRef.triggerPreviewNote (previewNote, false);
        previewNote = -1;
    }

    // A note held for a moment (double-click).
    void auditionFor (int zoneIndex, int milliseconds)
    {
        if (zoneIndex < 0 || zoneIndex >= (int) zones.size())
            return;
        startPreview (zoneIndex);
        const auto held = previewNote;
        juce::Component::SafePointer<ZoneMapEditor> safe (this);
        juce::Timer::callAfterDelay (milliseconds, [safe, held]
        {
            if (safe != nullptr && safe->previewNote == held)
                safe->stopPreview();
        });
    }

    // ----------------------------------------------------------- add sample

    static bool isAudioFile (const juce::File& file)
    {
        return file.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
    }

    bool addFile (const juce::File& file, int loKey, int hiKey, int rootKey)
    {
        juce::String error;
        if (processorRef.addSampleZone (osc, file, loKey, hiKey, 1, 127, rootKey, error))
        {
            lastEpoch = processorRef.getSampleEpoch() - 1; // (the next reload takes the new zones)
            return true;
        }

        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Add sample",
                                                error.isNotEmpty() ? error : "Could not read " + file.getFileName() + ".");
        return false;
    }

    void chooseSample()
    {
        chooser = std::make_unique<juce::FileChooser> ("Add a sample zone",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<ZoneMapEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fileChooser)
        {
            if (safe == nullptr)
                return;
            const auto file = fileChooser.getResult();
            safe->chooser.reset();
            if (file.existsAsFile())
                safe->addChosen (file);
        });
    }

    void addChosen (const juce::File& file)
    {
        int lo = 0, hi = 127, root = 60;
        if (selectedValid())
        {
            lo = zones[(size_t) selected].loKey;
            hi = zones[(size_t) selected].hiKey;
            root = juce::jlimit (lo, hi, juce::roundToInt ((float) zones[(size_t) selected].rootNote));
        }
        else if (! zones.empty())
        {
            std::array<bool, 128> taken {};
            for (const auto& z : zones)
                for (int k = z.loKey; k <= z.hiKey; ++k)
                    taken[(size_t) k] = true;

            // The free keys around middle C: the nearest free key, grown both ways.
            int centre = -1;
            for (int distance = 0; distance < 128 && centre < 0; ++distance)
                for (const auto candidate : { 60 - distance, 60 + distance })
                    if (candidate >= 0 && candidate < 128 && ! taken[(size_t) candidate])
                    {
                        centre = candidate;
                        break;
                    }

            if (centre >= 0)
            {
                lo = hi = centre;
                while (lo > 0 && ! taken[(size_t) lo - 1])
                    --lo;
                while (hi < 127 && ! taken[(size_t) hi + 1])
                    ++hi;
                root = juce::jlimit (lo, hi, 60);
            }
            else
            {
                lo = hi = root = 60;
            }
        }

        if (addFile (file, lo, hi, root))
        {
            reload (true);
            selectZone ((int) zones.size() - 1);
        }
    }

    // --------------------------------------------------------------- notes

    static juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, note), true, true, 4);
    }

    // "A3" or "A3 -50 cents" (the minus is a true minus sign).
    static juce::String rootText (double root)
    {
        const auto note = juce::roundToInt ((float) root);
        const auto cents = juce::roundToInt ((float) ((root - note) * 100.0));
        auto text = noteName (note);
        if (cents != 0)
            text << " " << (cents < 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) : juce::String ("+"))
                 << juce::String (std::abs (cents)) << " cents";
        return text;
    }

    // A note name ("C#4", "db3", "a-1") or a MIDI number, with optional cents
    // ("A3 -50", "60 +20").
    static bool parseNote (const juce::String& textIn, double& result)
    {
        auto text = textIn.trim().replace (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")), "-").replace ("cents", "").replace ("cent", "").trim();
        if (text.isEmpty())
            return false;

        double note = 0.0;
        auto rest = text;
        if (juce::CharacterFunctions::isDigit (text[0]))
        {
            const auto digits = text.initialSectionContainingOnly ("0123456789.");
            note = digits.getDoubleValue();
            rest = text.substring (digits.length()).trim();
        }
        else
        {
            const auto letter = juce::CharacterFunctions::toUpperCase (text[0]);
            static const int semitones[] { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
            if (letter < 'A' || letter > 'G')
                return false;
            auto pitchClass = semitones[letter - 'A'];
            auto index = 1;
            while (index < text.length() && (text[index] == '#' || text[index] == 'b' || text[index] == 'B'))
            {
                // ("b" after a letter is a flat; "B" is not accepted as one.)
                if (text[index] == 'B')
                    break;
                pitchClass += text[index] == '#' ? 1 : -1;
                ++index;
            }
            const auto sign = text[index] == '-' ? -1 : 1;
            if (text[index] == '-')
                ++index;
            if (index >= text.length() || ! juce::CharacterFunctions::isDigit (text[index]))
                return false;
            auto octave = 0;
            while (index < text.length() && juce::CharacterFunctions::isDigit (text[index]))
                octave = octave * 10 + (int) (text[index++] - '0');
            note = (double) ((sign * octave + 1) * 12 + pitchClass); // C4 = 60
            rest = text.substring (index).trim();
        }

        if (rest.isNotEmpty())
            note += rest.retainCharacters ("-+.0123456789").getDoubleValue() * 0.01;

        if (note < 0.0 || note > 127.0)
            return false;
        result = note;
        return true;
    }

    // ------------------------------------------------------------- geometry

    float keyWidth() const { return juce::jmax (1.0f, mapBounds.getWidth() / 128.0f); }
    float velocityHeight() const { return juce::jmax (1.0f, mapBounds.getHeight() / 127.0f); }
    float keyX (int key) const { return mapBounds.getX() + (float) key * keyWidth(); }
    float velocityY (int velocity) const { return mapBounds.getBottom() - mapBounds.getHeight() * (float) velocity / 127.0f; }

    int keyAtX (float x) const { return juce::jlimit (0, 127, (int) std::floor ((x - mapBounds.getX()) / keyWidth())); }

    juce::Rectangle<float> zoneBox (const SampleZone& z) const
    {
        return { keyX (z.loKey), velocityY (z.hiVel),
                 (float) (z.hiKey - z.loKey + 1) * keyWidth(),
                 mapBounds.getHeight() * (float) (z.hiVel - z.loVel + 1) / 127.0f };
    }

    // What the point grabs on a zone: its edges (corners count as two), else
    // the zone itself. The preferred zone is tried first, then the topmost.
    Hit hitZone (juce::Point<float> point, int preferred) const
    {
        const auto test = [this, point] (int index) -> Hit
        {
            if (index < 0 || index >= (int) zones.size())
                return {};
            const auto box = zoneBox (zones[(size_t) index]);
            if (! box.expanded (3.0f).contains (point))
                return {};
            const auto edgeX = juce::jmin (6.0f, box.getWidth() / 3.0f), edgeY = juce::jmin (6.0f, box.getHeight() / 3.0f);
            int mode = 0;
            if (point.x <= box.getX() + edgeX)           mode |= modeLeft;
            else if (point.x >= box.getRight() - edgeX)  mode |= modeRight;
            if (point.y <= box.getY() + edgeY)           mode |= modeTop;
            else if (point.y >= box.getBottom() - edgeY) mode |= modeBottom;
            return { index, mode != 0 ? mode : modeMove };
        };

        auto hit = test (preferred);
        if (hit.zone >= 0)
            return hit;
        for (int i = (int) zones.size() - 1; i >= 0; --i)
        {
            hit = test (i);
            if (hit.zone >= 0)
                return hit;
        }
        return {};
    }

    void layoutParameterRow (juce::Rectangle<int> row)
    {
        std::vector<juce::Component*> cells;
        for (juce::Component* control : { (juce::Component*) unmappedControl.get(), (juce::Component*) roundRobinControl.get(), (juce::Component*) blendControl.get() })
            if (control != nullptr)
                cells.push_back (control);
        if (cells.empty())
            return;

        const auto gap = 10;
        const auto count = (int) cells.size();
        const auto width = (row.getWidth() - gap * (count - 1)) / count;
        for (int i = 0; i < count; ++i)
        {
            auto bounds = juce::Rectangle<int> (row.getX() + i * (width + gap), row.getY(), width, row.getHeight());
            if (i == count - 1)
                bounds.setRight (row.getRight());
            cells[(size_t) i]->setBounds (bounds);
        }
    }

    // ---------------------------------------------------------------- paint

    static bool isBlackKey (int key)
    {
        const auto pitchClass = key % 12;
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
    }

    void paintMap (juce::Graphics& g) const
    {
        const auto map = mapBounds;
        const auto keyW = keyWidth();

        // Octave lines under the zones, a C named at the top of each.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (int key = 0; key < 128; key += 12)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (keyX (key), map.getY(), 1.0f, map.getHeight());
        }
        // Velocity lines.
        for (const auto velocity : { 32, 64, 96 })
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (map.getX(), velocityY (velocity), map.getWidth(), 1.0f);
            g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
            g.drawText (juce::String (velocity), juce::Rectangle<float> (map.getX() + 3.0f, velocityY (velocity) - 13.0f, 30.0f, 13.0f),
                        juce::Justification::centredLeft, false);
        }

        if (dropKey >= 0)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillRect (keyX (dropKey), map.getY(), keyW, map.getHeight());
        }

        if (zones.empty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            IlanaTheme::drawFitted (g, "This sample is one zone over every key. Add samples to make a multisample.",
                                    map.reduced (24.0f, 0.0f).toNearestInt(), juce::Justification::centred, 2);
        }

        // The zones, later ones on top. A box that overlaps earlier ones has
        // its outline inset by a step for each, so layers and alternates stay
        // readable.
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            const auto box = zoneBox (z);
            auto overlaps = 0;
            for (size_t j = 0; j < i; ++j)
            {
                const auto& other = zones[j];
                if (other.loKey <= z.hiKey && other.hiKey >= z.loKey && other.loVel <= z.hiVel && other.hiVel >= z.loVel)
                    ++overlaps;
            }
            const auto isSelected = (int) i == selected;

            g.setColour (colour.withAlpha (isSelected ? 0.42f : 0.2f));
            g.fillRect (box);
            g.setColour (colour.withAlpha (isSelected ? 1.0f : 0.8f));
            g.drawRect (box.reduced (0.5f + 1.5f * (float) juce::jmin (3, overlaps)), 1.0f);

            // The root note: where the sample plays at its own pitch.
            const auto root = (float) z.rootNote;
            if (root >= (float) z.loKey && root <= (float) z.hiKey + 1.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.95f : 0.6f));
                g.fillEllipse (keyX (0) + (root + 0.5f) * keyW - 2.0f, box.getBottom() - 6.0f, 4.0f, 4.0f);
            }
        }

        if (selectedValid())
        {
            // The selected zone on top: a bright outline and a handle on each edge.
            const auto& z = zones[(size_t) selected];
            const auto box = zoneBox (z);
            g.setColour (colour.brighter (0.5f));
            g.drawRect (box, 1.5f);
            g.setColour (juce::Colours::white);
            for (const auto point : { juce::Point<float> (box.getX(), box.getCentreY()), { box.getRight(), box.getCentreY() },
                                      { box.getCentreX(), box.getY() }, { box.getCentreX(), box.getBottom() } })
                g.fillRoundedRectangle (juce::Rectangle<float> (6.0f, 6.0f).withCentre (point), 1.5f);
        }

        paintStrip (g);
    }

    void paintStrip (juce::Graphics& g) const
    {
        const auto keyW = keyWidth();
        const auto strip = stripBounds;

        for (int key = 0; key < 128; ++key)
        {
            const auto cell = juce::Rectangle<float> (keyX (key), strip.getY(), keyW, strip.getHeight());
            const auto black = isBlackKey (key);
            g.setColour (black ? IlanaTheme::Ui::track : IlanaTheme::Ui::text2.withAlpha (0.32f));
            g.fillRect (cell);
            for (const auto& z : zones)
                if (key >= z.loKey && key <= z.hiKey)
                {
                    g.setColour (colour.withAlpha (black ? 0.3f : 0.4f));
                    g.fillRect (cell);
                    break;
                }
            if (selectedValid() && key >= zones[(size_t) selected].loKey && key <= zones[(size_t) selected].hiKey)
            {
                g.setColour (colour.withAlpha (black ? 0.3f : 0.4f));
                g.fillRect (cell);
            }
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.7f));
            g.fillRect (cell.getX(), cell.getY(), 1.0f, cell.getHeight());
        }

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.setColour (IlanaTheme::Ui::text);
        for (int key = 0; key < 128; key += 12)
            g.drawText (noteName (key), juce::Rectangle<float> (keyX (key) + 2.0f, strip.getY(), 40.0f, strip.getHeight()),
                        juce::Justification::centredLeft, false);

        // Each zone's root on the strip; the selected one a bright triangle.
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto x = keyX (0) + ((float) zones[i].rootNote + 0.5f) * keyW;
            if ((int) i == selected)
                continue;
            g.setColour (colour.withAlpha (0.9f));
            g.fillRect (x - 0.5f, strip.getBottom() - 5.0f, 1.0f, 5.0f);
        }
        if (selectedValid())
        {
            const auto x = keyX (0) + ((float) zones[(size_t) selected].rootNote + 0.5f) * keyW;
            juce::Path triangle;
            triangle.addTriangle (x - 4.0f, strip.getBottom(), x + 4.0f, strip.getBottom(), x, strip.getBottom() - 8.0f);
            g.setColour (juce::Colours::white);
            g.fillPath (triangle);
        }
    }

    // --------------------------------------------------------------- members

    IlanaSynthAudioProcessor& processorRef;
    int osc = 0;
    juce::Colour colour;
    juce::String prefix;

    std::vector<SampleZone> zones;
    int selected = -1;
    unsigned lastEpoch = 0;
    juce::String sampleName;

    // Mouse gesture
    int dragMode = 0;
    bool dragMoved = false;
    juce::Point<float> dragStart, lastClick { -100.0f, -100.0f };
    SampleZone dragOrigin;
    int previewNote = -1;
    int dropKey = -1;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> titleArea, hintArea;
    juce::Rectangle<float> wellBounds, mapBounds, stripBounds;

    juce::TextButton closeButton { "CLOSE" }, auditionButton { "AUDITION" }, deleteButton { "DELETE" },
        addButton { "ADD SAMPLE..." };
    Field keysField, velocityField, rootField, levelField;
    std::unique_ptr<ComboControl> unmappedControl, roundRobinControl;
    std::unique_ptr<KnobControl> blendControl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZoneMapEditor)
};
