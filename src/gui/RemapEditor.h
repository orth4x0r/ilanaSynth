#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"

// A mod slot's remap curve (Vital's per-route remap), drawn as the LFO curve
// shape is: the source's range runs left to right, the amount it sends from
// bottom to top. Click empty space to add a point, drag points, drag a
// segment's middle dot to bend it, double-click to remove a point or
// straighten a segment; SHAPES (or a right-click) for presets, flip and
// reverse. The live source shows as a line across the plot and a dot where
// it meets the curve. The matrix docks it under its row; X closes it.
class RemapEditor : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    RemapEditor (IlanaSynthAudioProcessor& p, int slotIndexIn, juce::Colour colourIn)
        : processorRef (p), slotIndex (slotIndexIn), colour (colourIn)
    {
        curve = processorRef.getModRemap (slotIndex);
        // The LFO curve's gestures and words.
        setTooltip ("Remap\nThe source's range runs left to right, what the routing sends bottom to top.  Click to add a point, "
                    "drag it, drag the dot on a line to curve it, double-click to delete (or straighten); right-click for "
                    "shapes.  The value shows while you drag.");

        shapesButton.setButtonText (juce::String::fromUTF8 ("SHAPES  \xe2\x96\xbe"));
        shapesButton.setTooltip ("Curve presets, flip and reverse");
        shapesButton.onClick = [this] { showMenu (&shapesButton); };
        addAndMakeVisible (shapesButton);

        closeButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
        closeButton.setTooltip ("Close the remap editor");
        closeButton.onClick = [this]
        {
            if (onClose != nullptr)
                onClose();
        };
        // Only an editor someone can close shows the X.
        addChildComponent (closeButton);

        setSize (300, 220);
        startTimerHz (30);
    }

    // Set by the owner (the matrix); shows the close button.
    void setOnClose (std::function<void()> callback)
    {
        onClose = std::move (callback);
        closeButton.setVisible (onClose != nullptr);
    }

    int getSlotIndex() const { return slotIndex; }

    // Esc in the editor: closes it as X does, when its owner lets it close.
    bool close()
    {
        if (onClose == nullptr)
            return false;

        auto callback = onClose; // the owner may delete us
        callback();
        return true;
    }

    // The live input's position along the curve (0..1), or -1 with no source.
    float getLiveInput() const { return liveInput; }

    void resized() override
    {
        auto top = getLocalBounds().removeFromTop (22).reduced (4, 2);
        if (closeButton.isVisible() || onClose != nullptr)
            closeButton.setBounds (top.removeFromRight (22));
        top.removeFromRight (4);
        shapesButton.setBounds (top.removeFromRight (78));
    }

    // Replaces the curve with one of the presets (SHAPES, or the matrix's
    // quick shape tiles), as one undo step.
    void applyShape (int index)
    {
        processorRef.beginEdit (editName());
        curve = shape (index);
        commit();
        processorRef.endEdit();
    }

    const LfoCurve& getCurve() const { return curve; }

    static juce::StringArray getShapeNames()
    {
        return { "Straight", "Invert", "Ease In", "Ease Out", "S-Curve", "Dead Zone", "Peak", "Steps 4", "Gate" };
    }

    // The tiles' own names: short enough for a 64 px tile at full size.
    static juce::StringArray getShortShapeNames()
    {
        return { "Line", "Invert", "Ease in", "Ease out", "S-curve", "Dead", "Peak", "Steps 4", "Gate" };
    }

    static LfoCurve shape (int index)
    {
        LfoCurve result;

        switch (index)
        {
            case 1:  result.points = { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 2:  result.points = { { 0.0f, -1.0f, 0.6f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 3:  result.points = { { 0.0f, -1.0f, -0.6f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 4:  result.points = { { 0.0f, -1.0f, 0.5f }, { 0.5f, 0.0f, -0.5f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 5:  result.points = { { 0.0f, -1.0f, 0.0f }, { 0.35f, 0.0f, 0.0f }, { 0.65f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 6:  result.points = { { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } }; break;
            case 7:  result.points = { { 0.0f, -1.0f, 0.0f }, { 0.249f, -1.0f, 0.0f }, { 0.25f, -0.33f, 0.0f },
                                       { 0.499f, -0.33f, 0.0f }, { 0.5f, 0.33f, 0.0f }, { 0.749f, 0.33f, 0.0f },
                                       { 0.75f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 8:  result.points = { { 0.0f, -1.0f, 0.0f }, { 0.499f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } }; break;
            default: result = IlanaSynthAudioProcessor::identityRemap(); break;
        }

        return result;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        IlanaTheme::drawFitted (g, "REMAP  " + titleText(), bounds.removeFromTop (22.0f).reduced (6.0f, 0.0f).toNearestInt(),
                          juce::Justification::centredLeft, 1);

        const auto plot = plotArea();
        IlanaTheme::paintWell (g, plot.expanded (6.0f), 5.0f);

        // Quarter grid.
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 4; ++i)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) i / 4.0f;
            const auto y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        }

        // The live source, across the source range.
        const auto slot = processorRef.readModSlot (slotIndex);
        if (slot.source != Mod::Source::None)
        {
            const auto bipolar = slot.polarity == Mod::Polarity::Bipolar
                                 || (slot.polarity == Mod::Polarity::Natural && Mod::isBipolarSource (slot.source));
            auto raw = processorRef.getSourceDisplayValue ((int) slot.source);
            if (slot.polarity == Mod::Polarity::Unipolar && Mod::isBipolarSource (slot.source))
                raw = 0.5f * (raw + 1.0f);
            else if (slot.polarity == Mod::Polarity::Bipolar && ! Mod::isBipolarSource (slot.source))
                raw = 2.0f * raw - 1.0f;
            const auto x = juce::jlimit (0.0f, 1.0f, bipolar ? 0.5f * (raw + 1.0f) : raw);
            liveInput = x;
            g.setColour (colour.withAlpha (0.35f));
            g.drawVerticalLine (juce::roundToInt (plot.getX() + x * plot.getWidth()), plot.getY(), plot.getBottom());
        }
        else
        {
            liveInput = -1.0f;
        }

        juce::Path path;
        for (int i = 0; i <= 128; ++i)
        {
            const auto x = (float) i / 128.0f;
            const auto y = i == 128 ? curve.points.back().y : curve.valueAt ((double) x);
            const juce::Point<float> point (plot.getX() + x * plot.getWidth(), yToScreen (y));
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));

        for (int i = 0; i + 1 < (int) curve.points.size(); ++i)
            if (curve.points[(size_t) i + 1].x - curve.points[(size_t) i].x > 0.02f)
            {
                g.setColour (colour.withAlpha (0.5f));
                g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tensionHandle (i)), 1.2f);
            }

        for (const auto& point : curve.points)
        {
            g.setColour (colour);
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (pointToScreen (point)));
        }

        // The live input where it meets the curve: what the routing sends now.
        if (liveInput >= 0.0f)
        {
            const juce::Point<float> at (plot.getX() + liveInput * plot.getWidth(), yToScreen (curve.valueAt ((double) liveInput)));
            g.setColour (IlanaTheme::Ui::bg);
            g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (at));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (at));
        }

        // The dragged point or curve's value beside it, as on the LFO graph.
        if (dragPoint >= 0 && dragPoint < (int) curve.points.size())
        {
            const auto& point = curve.points[(size_t) dragPoint];
            paintReadout (g, pointToScreen (point), "IN " + juce::String (juce::roundToInt (point.x * 100.0f)) + "%  OUT "
                                                        + (point.y >= 0.0f ? "+" : "") + juce::String (point.y, 2), plot);
        }
        else if (dragTension >= 0 && dragTension + 1 < (int) curve.points.size())
        {
            const auto tension = curve.points[(size_t) dragTension].tension;
            paintReadout (g, tensionHandle (dragTension), "CURVE  " + juce::String (tension >= 0.0f ? "+" : "") + juce::String (tension, 2), plot);
        }

        // Axis ends: the source's range left to right, what it sends bottom to top.
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText ("IN", juce::Rectangle<float> (plot.getRight() - 30.0f, plot.getBottom() + 1.0f, 30.0f, 10.0f), juce::Justification::centredRight);
        g.drawText ("OUT", juce::Rectangle<float> (plot.getX() - 10.0f, plot.getY() - 12.0f, 30.0f, 10.0f), juce::Justification::centredLeft);
    }

    // The title names the routing by what it joins ("REMAP  ·  LFO 1 →
    // Filter 1 › Cutoff", V7-40), not by a row or slot number; a caller
    // can set its own instead.
    void setTitle (const juce::String& text)
    {
        titleSuffix = text;
        repaint();
    }

    juce::String titleText() const
    {
        if (titleSuffix.isNotEmpty())
            return titleSuffix;
        const auto slot = processorRef.readModSlot (slotIndex);
        return juce::String::fromUTF8 ("\xc2\xb7  ") + ModNames::source ((int) slot.source, &processorRef)
               + (slot.destination != 0 ? juce::String::fromUTF8 (" \xe2\x86\x92 ") + ModNames::destination (slot.destination) : juce::String());
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragPoint = dragTension = -1;
        curve = processorRef.getModRemap (slotIndex); // an undo may have changed it

        if (event.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        processorRef.beginEdit (editName());

        dragPoint = hitPoint (event.position);
        if (dragPoint < 0)
            dragTension = hitTension (event.position);

        if (dragPoint < 0 && dragTension < 0)
        {
            const auto plot = plotArea();
            LfoCurve::Point point;
            point.x = juce::jlimit (0.001f, 0.999f, (event.position.x - plot.getX()) / plot.getWidth());
            point.y = yFromScreen (event.position.y);
            auto insertAt = curve.points.begin();
            while (insertAt != curve.points.end() && insertAt->x <= point.x)
                ++insertAt;
            dragPoint = (int) std::distance (curve.points.begin(), curve.points.insert (insertAt, point));
            commit();
        }

        if (dragTension >= 0)
            dragStartTension = curve.points[(size_t) dragTension].tension;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto plot = plotArea();

        if (dragPoint >= 0 && dragPoint < (int) curve.points.size())
        {
            auto& point = curve.points[(size_t) dragPoint];
            const auto last = (int) curve.points.size() - 1;
            if (dragPoint > 0 && dragPoint < last)
                point.x = juce::jlimit (curve.points[(size_t) dragPoint - 1].x, curve.points[(size_t) dragPoint + 1].x,
                                        (event.position.x - plot.getX()) / plot.getWidth());
            point.y = yFromScreen (event.position.y);
            commit();
        }
        else if (dragTension >= 0)
        {
            const auto& a = curve.points[(size_t) dragTension];
            const auto& b = curve.points[(size_t) dragTension + 1];
            const auto direction = b.y >= a.y ? 1.0f : -1.0f;
            const auto delta = (float) event.getDistanceFromDragStartY() * 0.012f * direction;
            curve.points[(size_t) dragTension].tension = juce::jlimit (-1.0f, 1.0f, dragStartTension + delta);
            commit();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragPoint = dragTension = -1;
        processorRef.endEdit();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        processorRef.performEdit (editName(), [this, &event]
        {
            curve = processorRef.getModRemap (slotIndex);
            const auto hit = hitPoint (event.position);

            if (hit > 0 && hit < (int) curve.points.size() - 1)
            {
                curve.points.erase (curve.points.begin() + hit);
                commit();
            }
            else if (const auto segment = hitTension (event.position); segment >= 0)
            {
                curve.points[(size_t) segment].tension = 0.0f;
                commit();
            }
        });
    }

private:
    void paintReadout (juce::Graphics& g, juce::Point<float> anchor, const juce::String& text, juce::Rectangle<float> area) const
    {
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, text) + 14.0f;
        auto box = juce::Rectangle<float> (width, 17.0f).withCentre ({ anchor.x, anchor.y - 18.0f });
        if (box.getY() < area.getY())
            box.setY (anchor.y + 10.0f);
        box.setX (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - width), box.getX()));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.3f));
        g.setFont (font);
        g.drawText (text, box, juce::Justification::centred);
    }

    juce::String editName() const { return "Remap curve " + juce::String (slotIndex + 1); }

    void timerCallback() override
    {
        // Follows undo and redo between gestures.
        if (! isMouseButtonDown())
            curve = processorRef.getModRemap (slotIndex);

        repaint();
    }

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().withTrimmedTop (24.0f).reduced (14.0f, 12.0f); }

    float yToScreen (float y) const
    {
        const auto plot = plotArea();
        return plot.getCentreY() - y * plot.getHeight() * 0.5f;
    }

    float yFromScreen (float screenY) const
    {
        const auto plot = plotArea();
        return juce::jlimit (-1.0f, 1.0f, (plot.getCentreY() - screenY) / (plot.getHeight() * 0.5f));
    }

    juce::Point<float> pointToScreen (const LfoCurve::Point& point) const
    {
        const auto plot = plotArea();
        return { plot.getX() + point.x * plot.getWidth(), yToScreen (point.y) };
    }

    juce::Point<float> tensionHandle (int segment) const
    {
        const auto& a = curve.points[(size_t) segment];
        const auto& b = curve.points[(size_t) segment + 1];
        const auto plot = plotArea();
        const auto y = a.y + (b.y - a.y) * LfoCurve::bend (0.5f, a.tension);
        return { plot.getX() + 0.5f * (a.x + b.x) * plot.getWidth(), yToScreen (y) };
    }

    int hitPoint (juce::Point<float> position) const
    {
        for (int i = 0; i < (int) curve.points.size(); ++i)
            if (pointToScreen (curve.points[(size_t) i]).getDistanceFrom (position) < 8.0f)
                return i;
        return -1;
    }

    int hitTension (juce::Point<float> position) const
    {
        for (int i = 0; i + 1 < (int) curve.points.size(); ++i)
            if (curve.points[(size_t) i + 1].x - curve.points[(size_t) i].x > 0.02f
                && tensionHandle (i).getDistanceFrom (position) < 7.0f)
                return i;
        return -1;
    }

    void commit()
    {
        processorRef.setModRemap (slotIndex, curve);
        // Keep editing the unsanitised copy's order: setModRemap sorts too.
        repaint();
    }

    void showMenu (juce::Component* target = nullptr)
    {
        juce::PopupMenu menu;
        const auto names = getShapeNames();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (100 + i, names[i]);
        menu.addSeparator();
        menu.addItem (300, "Flip vertically");
        menu.addItem (301, "Reverse");

        juce::Component::SafePointer<RemapEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target != nullptr ? target : this), [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            auto& self = *safeThis;
            self.processorRef.beginEdit (self.editName());

            if (result >= 100 && result < 200)
            {
                self.curve = shape (result - 100);
            }
            else if (result == 300)
            {
                for (auto& point : self.curve.points)
                    point.y = -point.y;
            }
            else if (result == 301)
            {
                LfoCurve reversed;
                reversed.points.clear();
                const auto& points = self.curve.points;
                for (auto i = (int) points.size() - 1; i >= 0; --i)
                {
                    auto point = points[(size_t) i];
                    point.x = 1.0f - point.x;
                    // A segment's bend belongs to its start point; reversed, it
                    // starts at the other end and bends the other way.
                    point.tension = i > 0 ? -points[(size_t) i - 1].tension : 0.0f;
                    reversed.points.push_back (point);
                }
                self.curve = reversed;
            }
            self.commit();
            self.processorRef.endEdit();
        });
    }

    IlanaSynthAudioProcessor& processorRef;
    int slotIndex;
    juce::Colour colour;
    LfoCurve curve;
    int dragPoint = -1, dragTension = -1;
    float dragStartTension = 0.0f;
    float liveInput = -1.0f;
    juce::String titleSuffix;
    juce::TextButton shapesButton, closeButton;
    std::function<void()> onClose;
};

// A preset remap curve as a small tile (the matrix's dock lists them beside
// the open editor): its curve over a well, its name under it. A click
// applies it.
class RemapShapeTile : public juce::Button
{
public:
    explicit RemapShapeTile (int shapeIndexIn)
        : juce::Button (RemapEditor::getShapeNames()[shapeIndexIn]), shapeIndex (shapeIndexIn)
    {
        setTooltip (getName() + "\nUse this curve for the open routing.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    int getShapeIndex() const { return shapeIndex; }
    juce::Colour colour = IlanaTheme::accent();

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto nameArea = bounds.removeFromBottom (15.0f);
        const auto plot = bounds.reduced (2.0f);
        IlanaTheme::paintWell (g, plot, 4.0f);
        if (highlighted || down)
        {
            g.setColour (colour.withAlpha (down ? 0.25f : 0.12f));
            g.fillRoundedRectangle (plot, 4.0f);
        }

        const auto curve = RemapEditor::shape (shapeIndex);
        const auto inner = plot.reduced (4.0f, 4.0f);
        juce::Path path;
        for (int i = 0; i <= 32; ++i)
        {
            const auto x = juce::jmin (0.9999f, (float) i / 32.0f);
            const auto point = juce::Point<float> (inner.getX() + inner.getWidth() * (float) i / 32.0f,
                                                   inner.getCentreY() - curve.valueAt (x) * inner.getHeight() * 0.5f);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setColour (colour.withAlpha (highlighted || down ? 1.0f : 0.8f));
        g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (highlighted || down ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        // (A narrow tile, at 75 % with the dock open, takes the last word: "EASE OUT" reads "OUT".)
        auto name = RemapEditor::getShortShapeNames()[shapeIndex].toUpperCase();
        if (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name) > nameArea.getWidth() - 2.0f)
            name = name.fromLastOccurrenceOf (" ", false, false);
        IlanaTheme::drawFitted (g, name, nameArea.toNearestInt(), juce::Justification::centred, 1);
    }

private:
    int shapeIndex;
};
