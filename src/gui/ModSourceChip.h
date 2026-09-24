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
    ModSourceChip (const juce::String& sourceName, int sourceIndex)
        : name (sourceName),
          index (sourceIndex)
    {
        setTooltip ("Drag onto any knob to create a modulation");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        if (hover > 0.01f)
        {
            g.setColour (modSourceColour (index).withAlpha (0.3f * hover));
            g.fillRoundedRectangle (bounds.expanded (2.0f), 4.0f);
        }

        g.setColour (juce::Colour (0xff1d1d22).brighter (0.12f * hover));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (modSourceColour (index).withAlpha (0.45f + 0.5f * hover));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f + hover);

        g.setColour (juce::Colours::white.withAlpha (0.75f + 0.25f * hover));
        g.setFont (IlanaTheme::font (11.5f, true));
        g.drawText (name, getLocalBounds(), juce::Justification::centred);
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
        const auto target = isMouseOver() ? 1.0f : 0.0f;

        if (std::abs (hover - target) < 0.005f)
            return;

        hover = IlanaAnim::approach (hover, target, 0.22f);
        repaint();
    }

    juce::String name;
    int index = 0;
    float hover = 0.0f;
};
