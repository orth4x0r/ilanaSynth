#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class InfoStrip : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    InfoStrip()
    {
        helpButton.setButtonText ("?");
        helpButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
        helpButton.onClick = [this]
        {
            if (onHelp != nullptr)
                onHelp();
        };

        addAndMakeVisible (helpButton);
        startTimerHz (20);
    }

    std::function<void()> onHelp;

    void setToolbarButton (juce::Component* button)
    {
        toolbarButton = button;

        if (toolbarButton != nullptr)
            addAndMakeVisible (*toolbarButton);

        resized();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();

        IlanaTheme::paintWell (g, bounds, 4.0f);

        g.setOpacity (juce::jlimit (0.0f, 1.0f, textAppear));

        auto textArea = getLocalBounds().reduced (10, 0).withTrimmedRight (toolbarButton != nullptr ? 122 : 34);

        if (title.isNotEmpty())
        {
            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText (title, textArea.removeFromLeft (230), juce::Justification::centredLeft);

            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText (description, textArea, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Hover any control for details.  Drag source chips onto knobs to modulate.  "
                        "Right-click knobs for quick modulation.",
                        textArea, juce::Justification::centredLeft);
        }

        // Long text trails off into the strip instead of being chopped off.
        const auto fadeWidth = juce::jmin (30.0f, (float) textArea.getWidth() * 0.5f);
        const auto fadeX = (float) textArea.getRight() - fadeWidth;

        if (fadeWidth > 4.0f)
        {
            juce::ColourGradient fade (IlanaTheme::Ui::well.withAlpha (0.0f), fadeX, 0.0f,
                                       IlanaTheme::Ui::well, (float) textArea.getRight(), 0.0f, false);
            g.setGradientFill (fade);
            g.fillRect (juce::Rectangle<float> (fadeX, 0.0f, fadeWidth, (float) getHeight()));
        }
    }

    void resized() override
    {
        const auto right = getWidth() - 28;
        helpButton.setBounds (right, 2, 24, getHeight() - 4);

        if (toolbarButton != nullptr)
            toolbarButton->setBounds (right - 6 - 78, 2, 78, getHeight() - 4);
    }

private:
    void timerCallback() override
    {
        juce::String newTitle, newDescription;

        if (auto* component = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
        {
            auto* current = component;

            while (current != nullptr)
            {
                if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*> (current))
                {
                    const auto tooltip = tooltipClient->getTooltip();

                    if (tooltip.isNotEmpty())
                    {
                        const auto lines = juce::StringArray::fromLines (tooltip);
                        newTitle = lines[0];

                        for (int i = 1; i < lines.size(); ++i)
                            newDescription += (i > 1 ? "  " : "") + lines[i];

                        break;
                    }
                }

                current = current->getParentComponent();
            }
        }

        if (newTitle != title || newDescription != description)
        {
            title = newTitle;
            description = newDescription;
            textAppear = 0.0f;
        }

        const auto wasFading = textAppear < 1.0f;
        textAppear = juce::jmin (1.0f, textAppear + 0.16f * frameTicks());

        if (wasFading)
            repaint();
    }

    juce::String title, description;
    float textAppear = 1.0f;
    juce::TextButton helpButton;
    juce::Component* toolbarButton = nullptr;
};
