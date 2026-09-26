#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

class ModSourceChip : public juce::Component,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~ModSourceChip() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    ModSourceChip (const juce::String& sourceName, int sourceIndex)
        : name (sourceName),
          index (sourceIndex)
    {
        setTooltip (sourceName + "\nDrag onto any knob to modulate it.  Knobs it already modulates light up while you hover.");
        startTimerHz (30);
    }

    // The source's live value (LFO position, envelope level, wheel...); the
    // chip glows with it. Optional.
    std::function<float()> valueProvider;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.5f);
        const auto colour = modSourceColour (index);
        const auto radius = bounds.getHeight() * 0.5f;
        const auto glow = juce::jlimit (0.0f, 1.0f, activity);

        if (hover > 0.01f || glow > 0.02f)
        {
            g.setColour (colour.withAlpha (0.22f * hover + 0.16f * glow));
            g.fillRoundedRectangle (bounds.expanded (2.0f), radius + 2.0f);
        }

        g.setColour (juce::Colour (0xff17171c));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (colour.withAlpha (0.10f + 0.10f * hover + 0.22f * glow));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (colour.withAlpha (0.5f + 0.45f * juce::jmax (hover, glow)));
        g.drawRoundedRectangle (bounds, radius, 1.0f + hover);

        // Grip dots: this chip is dragged, not clicked.
        g.setColour (colour.withAlpha (0.55f + 0.4f * hover));
        const auto gripX = bounds.getX() + radius * 0.9f;

        for (int row = -1; row <= 1; ++row)
            for (int column = 0; column < 2; ++column)
                g.fillEllipse (gripX + (float) column * 3.6f, bounds.getCentreY() + (float) row * 3.6f - 1.0f, 2.0f, 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.78f + 0.22f * juce::jmax (hover, glow)));
        g.setFont (IlanaTheme::font (11.5f, true));
        g.drawText (name, getLocalBounds().withTrimmedLeft (juce::roundToInt (radius * 0.9f + 6.0f)),
                    juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { highlightedModSource() = index; }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (highlightedModSource() == index)
            highlightedModSource() = 0;
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String (index), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        auto changed = false;
        const auto target = isMouseOver() ? 1.0f : 0.0f;

        if (std::abs (hover - target) >= 0.005f)
        {
            hover = IlanaAnim::approach (hover, target, 0.22f);
            changed = true;
        }

        if (valueProvider != nullptr)
        {
            const auto value = std::abs (valueProvider());
            const auto next = IlanaAnim::approach (activity, juce::jlimit (0.0f, 1.0f, value), 0.35f);

            if (std::abs (next - activity) > 0.01f)
            {
                activity = next;
                changed = true;
            }
        }

        if (changed)
            repaint();
    }

    juce::String name;
    int index = 0;
    float hover = 0.0f;
    float activity = 0.0f;
};
