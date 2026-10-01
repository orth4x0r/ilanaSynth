#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

// A mod slot's remap curve (Vital's per-route remap), drawn as the LFO curve
// shape is: the source's range runs left to right, the amount it sends from
// bottom to top. Click empty space to add a point, drag points, drag a
// segment's middle dot to bend it, double-click to remove a point or
// straighten a segment, right-click for shapes. The live source shows as a
// line across the plot.
class RemapEditor : public juce::Component,
                    private juce::Timer
{
public:
    RemapEditor (IlanaSynthAudioProcessor& p, int slotIndexIn, juce::Colour colourIn)
        : processorRef (p), slotIndex (slotIndexIn), colour (colourIn)
    {
        curve = processorRef.getModRemap (slotIndex);
        setSize (300, 220);
        startTimerHz (30);
    }

    static juce::StringArray getShapeNames()
    {
        return { "Straight", "Invert", "Ease In", "Ease Out", "S-Curve", "Dead Zone", "Peak", "Steps 4", "Gate" };
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
        g.drawText ("REMAP  " + juce::String (slotIndex + 1), bounds.removeFromTop (18.0f).reduced (4.0f, 0.0f),
                    juce::Justification::centredLeft);

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
            g.setColour (colour.withAlpha (0.35f));
            g.drawVerticalLine (juce::roundToInt (plot.getX() + x * plot.getWidth()), plot.getY(), plot.getBottom());
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
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragPoint = dragTension = -1;

        if (event.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

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

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
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
    }

private:
    void timerCallback() override { repaint(); }

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().withTrimmedTop (24.0f).reduced (12.0f, 10.0f); }

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

    void showMenu()
    {
        juce::PopupMenu menu;
        const auto names = getShapeNames();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (100 + i, names[i]);
        menu.addSeparator();
        menu.addItem (300, "Flip vertically");
        menu.addItem (301, "Reverse");

        juce::Component::SafePointer<RemapEditor> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            auto& self = *safeThis;
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
        });
    }

    IlanaSynthAudioProcessor& processorRef;
    int slotIndex;
    juce::Colour colour;
    LfoCurve curve;
    int dragPoint = -1, dragTension = -1;
    float dragStartTension = 0.0f;
};
