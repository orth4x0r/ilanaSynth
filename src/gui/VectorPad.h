#pragma once

// M8.5: the vector pad. An XY square whose corners are four of the six
// oscillators; the puck sets how much of each you hear (equal-power
// bilinear). Drag the puck, or switch on PATH and drag its eight points: the
// puck then travels the path at RATE. The live position (after the path,
// Evolve's drift and any modulation) is drawn as a ring.

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

class VectorPadDisplay : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    explicit VectorPadDisplay (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        setTooltip ("Vector pad: drag the dot to mix the four corner oscillators. With PATH on, drag the numbered "
                    "points; the dot then travels the path. Double-click: centre.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = square();
        IlanaTheme::paintWell (g, area, 6.0f);
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (area.toNearestInt());
        const auto accent = IlanaTheme::accent();

        // Grid and the four corners' shares, as soft glows.
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 4; ++i)
        {
            const auto t = (float) i / 4.0f;
            g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * t), area.getX(), area.getRight());
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * t), area.getY(), area.getBottom());
        }

        const auto live = processorRef.getVectorPosition();
        const auto weights = IlanaSynthAudioProcessor::vectorWeights (live.x, live.y);
        const juce::Point<float> corners[4] { area.getTopLeft(), area.getTopRight(), area.getBottomLeft(), area.getBottomRight() };
        for (int c = 0; c < 4; ++c)
        {
            const auto radius = area.getWidth() * (0.12f + 0.3f * weights[(size_t) c]);
            juce::ColourGradient glow (accent.withAlpha (0.35f * weights[(size_t) c] + 0.05f), corners[c],
                                       accent.withAlpha (0.0f), corners[c].translated (radius, 0.0f), true);
            g.setGradientFill (glow);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (corners[c]));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (IlanaTheme::font (11.0f, true));
            const auto label = "OSC " + juce::String (processorRef.getVectorCorner (c) + 1)
                               + "  " + juce::String (juce::roundToInt (weights[(size_t) c] * weights[(size_t) c] * 100.0f)) + "%";
            auto box = juce::Rectangle<float> (area.getWidth() * 0.5f - 8.0f, 16.0f);
            box.setPosition (c % 2 == 0 ? area.getX() + 6.0f : area.getRight() - box.getWidth() - 6.0f,
                             c < 2 ? area.getY() + 4.0f : area.getBottom() - 20.0f);
            g.drawText (label, box, c % 2 == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }

        // The path.
        if (processorRef.isVectorPathOn())
        {
            juce::Path path;
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
            {
                const auto p = toScreen (processorRef.getVectorPathPoint (i));
                if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
            }
            path.closeSubPath();
            g.setColour (accent.withAlpha (0.5f));
            g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved), juce::AffineTransform());
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
            {
                const auto p = toScreen (processorRef.getVectorPathPoint (i));
                g.setColour (i == dragPoint ? juce::Colours::white : accent);
                g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (8.5f, true));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (9.0f, 9.0f).withCentre (p), juce::Justification::centred);
            }
        }

        // The knob position (set by hand) and the live one (after the path,
        // drift and modulation).
        const auto set = toScreen ({ read ("vec_x"), read ("vec_y") });
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (set), 1.2f);
        const auto now = toScreen ({ live.x, live.y });
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (now));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (now));
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragPoint = -1;
        if (processorRef.isVectorPathOn())
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
                if (toScreen (processorRef.getVectorPathPoint (i)).getDistanceFrom (event.position) < 9.0f)
                    dragPoint = i;
        beginGesture();
        mouseDrag (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto p = fromScreen (event.position);
        if (dragPoint >= 0)
        {
            setParam ("vec_px" + juce::String (dragPoint + 1), p.x);
            setParam ("vec_py" + juce::String (dragPoint + 1), p.y);
        }
        else
        {
            setParam ("vec_x", p.x);
            setParam ("vec_y", p.y);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endGesture();
        dragPoint = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParam ("vec_x", 0.5f);
        setParam ("vec_y", 0.5f);
    }

private:
    juce::Rectangle<float> square() const
    {
        const auto side = (float) juce::jmin (getWidth(), getHeight()) - 4.0f;
        return juce::Rectangle<float> (side, side).withCentre (getLocalBounds().toFloat().getCentre());
    }

    juce::Point<float> toScreen (juce::Point<float> p) const
    {
        const auto area = square().reduced (10.0f);
        return { area.getX() + p.x * area.getWidth(), area.getBottom() - p.y * area.getHeight() };
    }

    juce::Point<float> fromScreen (juce::Point<float> p) const
    {
        const auto area = square().reduced (10.0f);
        return { juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth()),
                 juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight()) };
    }

    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    void setParam (const juce::String& id, float value)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    void beginGesture()
    {
        for (const auto* id : { "vec_x", "vec_y" })
            if (auto* parameter = processorRef.apvts.getParameter (id))
                parameter->beginChangeGesture();
    }

    void endGesture()
    {
        for (const auto* id : { "vec_x", "vec_y" })
            if (auto* parameter = processorRef.apvts.getParameter (id))
                parameter->endChangeGesture();
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    int dragPoint = -1;
};
