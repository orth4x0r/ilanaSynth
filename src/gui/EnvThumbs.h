#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <algorithm>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

// Revealed and assigned envelopes at a glance: each card draws its
// ADSR shape and marks whether the envelope is doing anything in the patch.
// Click to edit; drag a card onto a knob to route it there.
class EnvThumbBar : public juce::Component,
                    private juce::Timer
{
public:
    struct Env
    {
        juce::String title;
        juce::String prefix;
        Mod::Source source;
        juce::Colour colour;
    };

    explicit EnvThumbBar (IlanaSynthAudioProcessor& p, std::vector<Env> envsIn)
        : processorRef (p), envs (std::move (envsIn))
    {
        startTimerHz (10);
    }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;
    int getPreferredHeight() const
    {
        const auto visible = visibleEnvelopes().size();
        return visible + (visible < envs.size() ? 1 : 0) > 8 ? 96 : 48;
    }

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto visible = visibleEnvelopes();
        const auto count = (int) visible.size() + (visible.size() < envs.size() ? 1 : 0);
        for (int position = 0; position < (int) visible.size(); ++position)
            paintCard (g, visible[(size_t) position], cardBounds (position, count));
        if (count > (int) visible.size())
        {
            const auto card = cardBounds (count - 1, count);
            IlanaTheme::paintWell (g, card, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (IlanaTheme::font (21.0f, true));
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

        if (index == -2)
        {
            const auto previous = visibleEnvelopes().size();
            do
            {
                processorRef.revealNextEnvelope();
            } while (visibleEnvelopes().size() == previous && processorRef.getRevealedEnvelopeCount() < 16);
            if (onLayoutChanged != nullptr)
                onLayoutChanged();
            repaint();
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
                const auto visible = visibleEnvelopes();
                const auto position = (int) (std::find (visible.begin(), visible.end(), index) - visible.begin());
                auto image = createComponentSnapshot (cardBounds (position, (int) visible.size() + (visible.size() < envs.size() ? 1 : 0)).toNearestInt(), true, 1.0f);
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
    std::vector<int> visibleEnvelopes() const
    {
        std::vector<int> visible;
        for (int env = 0; env < (int) envs.size(); ++env)
            if (env < processorRef.getRevealedEnvelopeCount() || isInUse (env))
                visible.push_back (env);
        return visible;
    }

    juce::Rectangle<float> cardBounds (int position, int count) const
    {
        const auto columns = count > 8 ? 8 : juce::jmax (1, count);
        const auto gap = 6.0f;
        const auto width = ((float) getWidth() - gap * (float) (columns - 1)) / (float) columns;
        const auto rows = count > 8 ? 2 : 1;
        const auto height = ((float) getHeight() - gap * (float) (rows - 1)) / (float) rows;
        return { (float) (position % columns) * (width + gap),
                 (float) (position / columns) * (height + gap), width, height };
    }

    int indexAt (juce::Point<int> position) const
    {
        const auto visible = visibleEnvelopes();
        const auto count = (int) visible.size() + (visible.size() < envs.size() ? 1 : 0);
        for (int i = 0; i < count; ++i)
            if (cardBounds (i, count).contains (position.toFloat()))
                return i < (int) visible.size() ? visible[(size_t) i] : -2;

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

        for (const auto* prefix : OscillatorIds::prefixes)
            if ((int) readParam (juce::String (prefix) + "_amp_env") == env)
                return true;

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.destination != 0 && (routing.source == info.source || routing.aux == info.source))
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
        {
            const auto preferred = getPreferredHeight();
            if (preferred != lastPreferredHeight && onLayoutChanged != nullptr)
                onLayoutChanged();
            lastPreferredHeight = preferred;
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    std::vector<Env> envs;
    int selected = 0;
    int hoverIndex = -1;
    int lastPreferredHeight = 48;
};
