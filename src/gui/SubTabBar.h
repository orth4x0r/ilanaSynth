#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class SubTabBar : public juce::Component,
                  private IlanaAnim::FrameTimer
{
public:
    SubTabBar()
    {
        addButton.setButtonText ("+");
        addButton.setTooltip ("Reveal the next unit");
        addButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.75f));
        addButton.onClick = [this]
        {
            if (onAdd != nullptr)
                onAdd();
        };

        addAndMakeVisible (addButton);
        startTimerHz (30);
    }

    void setItems (const juce::StringArray& names, int numRevealed, int selected)
    {
        if (initialised && numRevealed > revealed && numRevealed <= juce::jmax (1, names.size()))
        {
            revealFlash = 1.0f;
            flashIndex = numRevealed - 1;
        }

        initialised = true;
        items = names;
        revealed = juce::jlimit (1, juce::jmax (1, items.size()), numRevealed);
        selectedIndex = juce::jlimit (0, revealed - 1, selected);
        addButton.setVisible (revealed < items.size());
        repaint();
    }

    int getNumRevealed() const { return revealed; }

    std::function<void (int)> onSelect;
    std::function<void()> onAdd;

    void resized() override
    {
        addButton.setBounds (getWidth() - 30, 2, 26, 22);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index >= 0 && index < revealed && index != selectedIndex)
        {
            selectedIndex = index;
            repaint();

            if (onSelect != nullptr)
                onSelect (index);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index != hoverIndex)
        {
            hoverIndex = index;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverIndex != -1)
        {
            hoverIndex = -1;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < revealed && i < items.size(); ++i)
        {
            auto pill = pillBounds (i);
            const auto active = i == selectedIndex;

            if (i == flashIndex && revealFlash > 0.01f)
            {
                pill = pill.expanded (2.0f * revealFlash);
                g.setColour (IlanaTheme::accent().withAlpha (0.35f * revealFlash));
                g.drawRoundedRectangle (pill, 5.0f, 2.0f);
                g.setColour (IlanaTheme::accent().withAlpha (0.15f * revealFlash));
                g.fillRoundedRectangle (pill.expanded (3.0f), 6.0f);
            }

            // The choice and the hover fade (the shared animator).
            const auto chosen = IlanaTheme::fade (*this, i, active ? 1.0f : 0.0f);
            const auto hovered = IlanaTheme::fade (*this, 1000 + i, i == hoverIndex && ! active ? 1.0f : 0.0f, IlanaTheme::FadeRate::hover);
            g.setColour (IlanaTheme::Ui::panel.interpolatedWith (juce::Colours::white, 0.08f * hovered)
                             .interpolatedWith (IlanaTheme::accent().withAlpha (0.18f), chosen));
            g.fillRoundedRectangle (pill, 4.0f);

            g.setColour (IlanaTheme::Ui::text2.interpolatedWith (IlanaTheme::accent(), chosen));
            g.setFont (active ? IlanaTheme::font (IlanaTheme::TextSize::body, true)
                              : IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText (items[i], pill, juce::Justification::centred);

            if (chosen > 0.01f)
            {
                g.setColour (IlanaTheme::accent().withAlpha (chosen));
                g.fillRoundedRectangle (pill.withHeight (2.0f).withY (pill.getBottom() - 2.0f), 1.0f);
            }
        }

        if (revealed < items.size())
        {
            const auto alpha = 0.3f;

            g.setColour (IlanaTheme::accent().withAlpha (alpha));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText ("PRESS + TO ADD", juce::Rectangle<int> (getWidth() - 150, 6, 114, 14),
                        juce::Justification::centredRight);
        }
    }

private:
    void timerCallback() override
    {
        revealFlash = IlanaAnim::decay (revealFlash, 0.9f, frameTicks());

        if (revealFlash > 0.01f)
            repaint();
    }

    juce::Rectangle<float> pillBounds (int index) const
    {
        constexpr float pillWidth = 96.0f;
        constexpr float gap = 6.0f;
        return { (float) index * (pillWidth + gap), 2.0f, pillWidth, 22.0f };
    }

    int indexAt (juce::Point<int> position) const
    {
        for (int i = 0; i < revealed && i < items.size(); ++i)
            if (pillBounds (i).contains (position.toFloat()))
                return i;

        return -1;
    }

    juce::StringArray items;
    int revealed = 1;
    int selectedIndex = 0;
    int hoverIndex = -1;
    bool initialised = false;
    float revealFlash = 0.0f;
    int flashIndex = -1;
    juce::TextButton addButton;
};
