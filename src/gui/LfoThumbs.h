#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

// All four LFOs at a glance: each card shows its waveform, a live phase dot,
// its rate and whether it is routed anywhere. Clicking a card selects that
// LFO for editing; dragging a card onto a knob routes it there.
class LfoThumbBar : public juce::Component,
                    private juce::Timer
{
public:
    LfoThumbBar (IlanaSynthAudioProcessor& p, std::function<juce::Colour (int)> colourForIn)
        : processorRef (p), colourFor (std::move (colourForIn))
    {
        juce::Random random (99);

        for (auto& value : sampleHoldPreview)
            value = random.nextFloat() * 2.0f - 1.0f;

        startTimerHz (20);
    }

    std::function<void (int)> onSelect;

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            paintCard (g, lfo, cardBounds (lfo));
    }

    // Select on release, and only for a click: a drag assigns the source to
    // a knob instead (on MAIN, selecting would switch pages mid-drag).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown() || event.getDistanceFromDragStart() >= 6)
            return;

        const auto index = indexAt (event.getPosition());

        if (index >= 0 && onSelect != nullptr)
        {
            selected = index;
            onSelect (index);
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getMouseDownPosition());

        if (index < 0 || event.getDistanceFromDragStart() < 6)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (cardBounds (index).toNearestInt(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                const Mod::Source sources[] { Mod::Source::Lfo1, Mod::Source::Lfo2, Mod::Source::Lfo3, Mod::Source::Lfo4 };
                container->startDragging ("modsource:" + juce::String ((int) sources[index]), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index != hoverIndex)
        {
            hoverIndex = index;
            const Mod::Source sources[] { Mod::Source::Lfo1, Mod::Source::Lfo2, Mod::Source::Lfo3, Mod::Source::Lfo4 };
            highlightedModSource() = index >= 0 ? (int) sources[index] : 0;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        highlightedModSource() = 0;
        repaint();
    }

private:
    juce::Rectangle<float> cardBounds (int index) const
    {
        const auto gap = 8.0f;
        const auto width = ((float) getWidth() - gap * 3.0f) / 4.0f;
        return { (float) index * (width + gap), 0.0f, width, (float) getHeight() };
    }

    int indexAt (juce::Point<int> position) const
    {
        for (int i = 0; i < IlanaSynthAudioProcessor::numLfos; ++i)
            if (cardBounds (i).contains (position.toFloat()))
                return i;

        return -1;
    }

    float readParam (int lfo, const juce::String& suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + suffix))
            return value->load();

        return 0.0f;
    }

    bool isRouted (int lfo) const
    {
        const Mod::Source sources[] { Mod::Source::Lfo1, Mod::Source::Lfo2, Mod::Source::Lfo3, Mod::Source::Lfo4 };

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.isActive() && routing.source == sources[lfo])
                return true;
        }

        return false;
    }

    float shapeValue (int lfo, int shape, double phase) const
    {
        phase -= std::floor (phase);

        switch (shape)
        {
            case 5: return sampleHoldPreview[(size_t) juce::jlimit (0, 7, (int) (phase * 8.0))];
            case 6: return processorRef.getLfoCustomPoint (lfo, juce::jlimit (0, IlanaSynthAudioProcessor::lfoDrawSteps - 1,
                                                                              (int) (phase * IlanaSynthAudioProcessor::lfoDrawSteps)));
            case 7: return readParam (lfo, "_step" + juce::String (juce::jlimit (0, 15, (int) (phase * 16.0)) + 1));
            case IlanaSynthAudioProcessor::curveShape: return processorRef.getLfoCurveValue (lfo, phase);
            default: return lfoShapeValue (shape, phase);
        }
    }

    void paintCard (juce::Graphics& g, int lfo, juce::Rectangle<float> card)
    {
        const auto colour = colourFor (lfo);
        const auto active = lfo == selected;
        const auto hovered = lfo == hoverIndex;

        IlanaTheme::paintWell (g, card, 6.0f);

        if (active)
        {
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (card, 6.0f);
            g.setColour (colour.withAlpha (0.9f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.4f);
        }
        else if (hovered)
        {
            g.setColour (colour.withAlpha (0.4f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.0f);
        }

        auto inner = card.reduced (8.0f, 5.0f);
        auto titleRow = inner.removeFromTop (14.0f);

        g.setColour (active ? colour : juce::Colours::white.withAlpha (0.7f));
        g.setFont (IlanaTheme::font (12.0f, true));
        g.drawText ("LFO " + juce::String (lfo + 1), titleRow, juce::Justification::centredLeft);

        const auto synced = readParam (lfo, "_sync") > 0.5f;
        const juce::StringArray divisions { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
        const auto rateText = synced ? divisions[juce::jlimit (0, divisions.size() - 1, (int) readParam (lfo, "_div"))]
                                     : juce::String (readParam (lfo, "_rate"), 2) + " Hz";

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (IlanaTheme::font (11.0f));
        g.drawText (rateText, titleRow, juce::Justification::centredRight);

        if (isRouted (lfo))
        {
            const auto titleWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (12.0f, true)),
                                                                            "LFO " + juce::String (lfo + 1));
            g.setColour (colour);
            g.fillEllipse (titleRow.getX() + titleWidth + 6.0f, titleRow.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        const auto plot = inner.reduced (0.0f, 3.0f);
        const auto shape = (int) readParam (lfo, "_shape");

        juce::Path path;
        constexpr int points = 96;

        for (int i = 0; i <= points; ++i)
        {
            const auto phase = (double) i / (double) points;
            const auto value = shapeValue (lfo, shape, phase);
            const auto x = plot.getX() + plot.getWidth() * (float) phase;
            const auto y = plot.getCentreY() - value * plot.getHeight() * 0.45f;

            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        g.setColour (colour.withAlpha (active ? 0.95f : 0.6f));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto phase = (double) processorRef.getLfoPhase (lfo);
        const auto dotValue = shapeValue (lfo, shape, phase);
        const juce::Point<float> dot (plot.getX() + plot.getWidth() * (float) phase,
                                      plot.getCentreY() - dotValue * plot.getHeight() * 0.45f);

        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (dot));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::function<juce::Colour (int)> colourFor;
    std::array<float, 8> sampleHoldPreview {};
    int selected = 0;
    int hoverIndex = -1;
};
