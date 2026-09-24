#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

// All five envelopes at a glance, like the LFO cards: each card draws its
// ADSR shape and marks whether the envelope is doing anything in the patch.
// Click to edit; drag a card onto a knob to route it there.
class EnvThumbBar : public juce::Component,
                    private juce::Timer
{
public:
    struct Env
    {
        const char* title;
        const char* prefix;
        Mod::Source source;
        juce::Colour colour;
    };

    explicit EnvThumbBar (IlanaSynthAudioProcessor& p, std::array<Env, 5> envsIn)
        : processorRef (p), envs (envsIn)
    {
        startTimerHz (10);
    }

    static constexpr int numEnvs = 5;

    std::function<void (int)> onSelect;

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (int env = 0; env < numEnvs; ++env)
            paintCard (g, env, cardBounds (env));
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
                container->startDragging ("modsource:" + juce::String ((int) envs[(size_t) index].source), this,
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
            highlightedModSource() = index >= 0 ? (int) envs[(size_t) index].source : 0;
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
        const auto width = ((float) getWidth() - gap * (float) (numEnvs - 1)) / (float) numEnvs;
        return { (float) index * (width + gap), 0.0f, width, (float) getHeight() };
    }

    int indexAt (juce::Point<int> position) const
    {
        for (int i = 0; i < numEnvs; ++i)
            if (cardBounds (i).contains (position.toFloat()))
                return i;

        return -1;
    }

    float readParam (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    // The amp envelope always plays; the filter envelopes count when their
    // filter's env amount is set; any envelope counts when routed in the matrix.
    bool isInUse (int env) const
    {
        const auto& info = envs[(size_t) env];

        if (info.source == Mod::Source::AmpEnv)
            return true;

        if (info.source == Mod::Source::FilterEnv && std::abs (readParam ("f1_env")) > 0.001f)
            return true;

        if (info.source == Mod::Source::FilterEnv2 && std::abs (readParam ("f2_env")) > 0.001f)
            return true;

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.isActive() && (routing.source == info.source || routing.aux == info.source))
                return true;
        }

        return false;
    }

    void paintCard (juce::Graphics& g, int env, juce::Rectangle<float> card)
    {
        const auto& info = envs[(size_t) env];
        const auto colour = info.colour;
        const auto active = env == selected;
        const auto hovered = env == hoverIndex;
        const auto inUse = isInUse (env);

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

        g.setColour (active ? colour : juce::Colours::white.withAlpha (inUse ? 0.75f : 0.45f));
        g.setFont (IlanaTheme::font (12.0f, true));
        g.drawText (info.title, titleRow, juce::Justification::centredLeft);

        if (inUse)
        {
            const auto titleWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (12.0f, true)),
                                                                            info.title);
            g.setColour (colour);
            g.fillEllipse (titleRow.getX() + titleWidth + 6.0f, titleRow.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        // ADSR outline on a compressed time axis so long and short stages
        // both stay readable.
        const auto prefix = juce::String (info.prefix);
        const auto attack = std::sqrt (readParam (prefix + "_attack"));
        const auto decay = std::sqrt (readParam (prefix + "_decay"));
        const auto sustain = juce::jlimit (0.0f, 1.0f, readParam (prefix + "_sustain"));
        const auto release = std::sqrt (readParam (prefix + "_release"));
        const auto hold = 0.35f;
        const auto total = juce::jmax (0.001f, attack + decay + hold + release);

        const auto plot = inner.reduced (0.0f, 3.0f);
        const auto xAt = [&plot, total] (float t) { return plot.getX() + plot.getWidth() * t / total; };
        const auto yAt = [&plot] (float level) { return plot.getBottom() - level * plot.getHeight(); };

        juce::Path path;
        path.startNewSubPath (xAt (0.0f), yAt (0.0f));
        path.lineTo (xAt (attack), yAt (1.0f));
        path.lineTo (xAt (attack + decay), yAt (sustain));
        path.lineTo (xAt (attack + decay + hold), yAt (sustain));
        path.lineTo (xAt (total), yAt (0.0f));

        auto fill = path;
        fill.closeSubPath();
        g.setColour (colour.withAlpha (active ? 0.16f : 0.08f));
        g.fillPath (fill);

        g.setColour (colour.withAlpha (active ? 0.95f : (inUse ? 0.6f : 0.35f)));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<Env, numEnvs> envs;
    int selected = 0;
    int hoverIndex = -1;
};
