#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <vector>

#include "ParamInfo.h"
#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"
#include "AnimationUtils.h"

// The patch's LFOs at a glance, Phase Plant style: the added and the routed
// ones, then a "+" card for the next. Cards keep one size (four fit the view)
// and the bar scrolls sideways when there are more. Each card shows its waveform, a live
// phase dot, its rate and whether it is routed anywhere. Clicking a card
// selects that LFO for editing; dragging a card onto a knob routes it there;
// right-click removes it.
class LfoThumbBar : public juce::Component,
                    private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~LfoThumbBar() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    LfoThumbBar (IlanaSynthAudioProcessor& p, std::function<juce::Colour (int)> colourForIn)
        : processorRef (p), colourFor (std::move (colourForIn))
    {
        juce::Random random (99);

        for (auto& value : sampleHoldPreview)
            value = random.nextFloat() * 2.0f - 1.0f;

        startTimerHz (20);
    }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;

    // The width the bar is seen through: four cards fill it.
    void setViewWidth (int width) { viewWidth = width; }

    int getPreferredWidth() const
    {
        return juce::jmax (viewWidth, (int) std::ceil ((float) numCards() * (cardWidth() + gap) - gap));
    }

    juce::Rectangle<int> boundsOfCard (int lfo) const
    {
        const auto visible = visibleLfos();
        const auto position = std::find (visible.begin(), visible.end(), lfo);
        return position == visible.end() ? juce::Rectangle<int>()
                                         : cardBounds ((int) (position - visible.begin())).toNearestInt();
    }

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto visible = visibleLfos();

        for (int position = 0; position < (int) visible.size(); ++position)
            paintCard (g, visible[(size_t) position], cardBounds (position));

        if (numCards() > (int) visible.size())
        {
            const auto card = cardBounds ((int) visible.size());
            IlanaTheme::paintWell (g, card, 6.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::display, true));
            g.drawText ("+", card, juce::Justification::centred);
        }
    }

    // Select on release, and only for a click: a drag assigns the source to
    // a knob instead (on MAIN, selecting would switch pages mid-drag).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown() || event.getDistanceFromDragStart() >= 6)
            return;

        const auto index = indexAt (event.getPosition());

        if (index >= 0 && event.mods.isPopupMenu())
        {
            showCardMenu (index);
            return;
        }

        if (index == -2)
        {
            const auto visible = visibleLfos();
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (std::find (visible.begin(), visible.end(), lfo) == visible.end())
                {
                    processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
                    selected = lfo;
                    if (onSelect != nullptr)
                        onSelect (lfo);
                    break;
                }

            layoutChanged();
        }
        else if (index >= 0 && onSelect != nullptr)
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
                const auto visible = visibleLfos();
                const auto position = (int) (std::find (visible.begin(), visible.end(), index) - visible.begin());
                auto image = createComponentSnapshot (cardBounds (position).toNearestInt(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String ((int) Mod::lfoSourceFor (index)), this,
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
            highlightedModSource() = index >= 0 ? (int) Mod::lfoSourceFor (index) : 0;
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
    static constexpr float gap = 8.0f;

    std::vector<int> visibleLfos() const
    {
        std::vector<int> visible;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (processorRef.isLfoShown (lfo))
                visible.push_back (lfo);
        return visible;
    }

    int numCards() const
    {
        const auto visible = (int) visibleLfos().size();
        return visible + (visible < IlanaSynthAudioProcessor::numLfos ? 1 : 0);
    }

    float cardWidth() const
    {
        return ((float) (viewWidth > 0 ? viewWidth : getWidth()) - gap * 3.0f) / 4.0f;
    }

    juce::Rectangle<float> cardBounds (int position) const
    {
        return { (float) position * (cardWidth() + gap), 0.0f, cardWidth(), (float) getHeight() };
    }

    // An LFO index, -2 for the "+" card, or -1.
    int indexAt (juce::Point<int> position) const
    {
        const auto visible = visibleLfos();

        for (int i = 0; i < numCards(); ++i)
            if (cardBounds (i).contains (position.toFloat()))
                return i < (int) visible.size() ? visible[(size_t) i] : -2;

        return -1;
    }

    void layoutChanged()
    {
        lastCardCount = numCards();
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
        repaint();
    }

    void showCardMenu (int lfo)
    {
        juce::PopupMenu menu;
        const auto routed = isRouted (lfo);
        menu.addItem (1, routed ? "Remove (unroute it first)" : "Remove LFO " + juce::String (lfo + 1), ! routed);

        juce::Component::SafePointer<LfoThumbBar> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis, lfo] (int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            safeThis->processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, false);
            if (safeThis->selected == lfo && safeThis->onSelect != nullptr)
            {
                const auto remaining = safeThis->visibleLfos();
                safeThis->selected = remaining.empty() ? 0 : remaining.front();
                safeThis->onSelect (safeThis->selected);
            }
            safeThis->layoutChanged();
        });
    }

    float readParam (int lfo, const juce::String& suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + suffix))
            return value->load();

        return 0.0f;
    }

    // The cards repaint often; what they drive is re-read four times a second.
    juce::String cachedTargets (int lfo)
    {
        const auto now = juce::Time::getMillisecondCounter();

        if (now - targetsStamp > 250 || targetsStamp == 0)
        {
            targetsStamp = now;
            for (int i = 0; i < (int) targetsCache.size(); ++i)
                targetsCache[(size_t) i] = describeModTargets (processorRef, Mod::lfoSourceFor (i));
        }

        return targetsCache[(size_t) juce::jlimit (0, (int) targetsCache.size() - 1, lfo)];
    }

    std::array<juce::String, (size_t) IlanaSynthAudioProcessor::numLfos> targetsCache;
    juce::uint32 targetsStamp = 0;

    bool isRouted (int lfo) const
    {
        const auto source = Mod::lfoSourceFor (lfo);

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.destination != 0 && (routing.source == source || routing.aux == source
                                              || routing.source == Mod::lfoBSourceFor (lfo) || routing.aux == Mod::lfoBSourceFor (lfo)))
                return true;
        }

        return false;
    }

    // M8.1: a simulated shape's picture, a few seconds of it from a fresh
    // start at RATE 1 Hz, rebuilt when its settings change.
    float simValue (int lfo, const LfoSimSettings& settings, double phase) const
    {
        auto& cache = simTraces[(size_t) lfo];
        if (cache.shape != settings.shape || cache.params != settings.p || cache.axis != settings.axis)
        {
            cache.shape = settings.shape;
            cache.params = settings.p;
            cache.axis = settings.axis;
            LfoSim sim;
            sim.sampleRate = 600.0;
            sim.reset (settings, 3);
            const auto seconds = LfoSimShapes::isPhysics (settings.shape) ? 3.0 : 4.0;
            const auto perPoint = juce::jmax (1, (int) (seconds * 600.0 / (double) cache.values.size()));
            for (auto& value : cache.values)
                for (int i = 0; i < perPoint; ++i)
                {
                    float b = 0.0f;
                    sim.next (settings, 1.0 / 600.0, value, b);
                }
        }
        const auto index = juce::jlimit (0, (int) cache.values.size() - 1, (int) (phase * (double) cache.values.size()));
        return cache.values[(size_t) index];
    }

    struct SimTrace
    {
        int shape = -1, axis = 0;
        std::array<float, LfoSimInfo::numParams> params {};
        std::array<float, 128> values {};
    };
    mutable std::array<SimTrace, (size_t) IlanaSynthAudioProcessor::numLfos> simTraces;

    float shapeValue (int lfo, int shape, double phase) const
    {
        if (LfoSimShapes::isSim (shape))
            return simValue (lfo, processorRef.readLfoSimSettings (lfo), juce::jlimit (0.0, 0.999999, phase));

        phase = LfoShapes::isPhysics (shape) ? juce::jlimit (0.0, 0.999999, phase)
                                             : phase - std::floor (phase);

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

        g.setColour (active ? colour : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("LFO " + juce::String (lfo + 1), titleRow, juce::Justification::centredLeft);

        const auto synced = readParam (lfo, "_sync") > 0.5f;
        const juce::StringArray divisions { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
        const auto rateText = synced ? divisions[juce::jlimit (0, divisions.size() - 1, (int) readParam (lfo, "_div"))]
                                     : describeValue ("lfo" + juce::String (lfo + 1) + "_rate", readParam (lfo, "_rate")); // as the RATE knob shows it

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (rateText, titleRow, juce::Justification::centredRight);

        if (isRouted (lfo))
        {
            const auto titleWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true)),
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

        // Unassigned LFOs are drawn faint.
        const auto targets = cachedTargets (lfo);
        g.setColour (colour.withAlpha (active ? 0.95f : (targets.isNotEmpty() ? 0.6f : 0.3f)));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        paintTargetTag (g, inner, targets, colour);

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
        {
            // Routing an LFO elsewhere, or loading a patch, can add a card.
            if (numCards() != lastCardCount)
                layoutChanged();
            if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ lfoPhases()))
                repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    // Free-running LFOs move with nothing sounding: their dots follow.
    juce::uint64 lfoPhases() const
    {
        juce::uint64 signature = 0;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            signature ^= IlanaAnim::phaseSignature (processorRef.getLfoPhase (lfo), lfo);
        return signature;
    }

    IlanaSynthAudioProcessor& processorRef;
    std::function<juce::Colour (int)> colourFor;
    std::array<float, 8> sampleHoldPreview {};
    int selected = 0;
    int hoverIndex = -1;
    int viewWidth = 0;
    int lastCardCount = -1;
};
