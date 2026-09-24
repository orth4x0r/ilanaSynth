#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

class LogoComponent : public juce::Component,
                      private juce::Timer
{
public:
    LogoComponent()
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto badge = bounds.withWidth (bounds.getHeight()).reduced (2.0f);

        const auto glow = 0.55f + 0.45f * std::sin (phaseOffset * 2.4f);
        g.setColour (IlanaTheme::accent().withAlpha (0.08f + 0.12f * glow));
        g.fillRoundedRectangle (badge.expanded (4.0f + 2.0f * glow), badge.getWidth() * 0.33f);

        juce::ColourGradient badgeGradient (IlanaTheme::accent().brighter (0.25f), badge.getTopLeft(),
                                            IlanaTheme::accent().darker (0.7f), badge.getBottomRight(), false);
        g.setGradientFill (badgeGradient);
        g.fillRoundedRectangle (badge, badge.getWidth() * 0.28f);

        juce::Path clip;
        clip.addRoundedRectangle (badge, badge.getWidth() * 0.28f);

        {
            juce::Graphics::ScopedSaveState saveState (g);
            g.reduceClipRegion (clip);

            const auto centreY = badge.getCentreY();

            for (int line = 0; line < 3; ++line)
            {
                const auto amplitude = badge.getHeight() * (0.09f + 0.045f * (float) line);
                const auto phase = phaseOffset + (float) line * 0.9f;

                juce::Path wave;

                for (int x = 0; x <= (int) badge.getWidth(); ++x)
                {
                    const auto t = (float) x / juce::jmax (1.0f, badge.getWidth());
                    const auto y = centreY + std::sin ((t * 1.5f + phase) * juce::MathConstants<float>::twoPi) * amplitude;

                    if (x == 0)
                        wave.startNewSubPath (badge.getX() + (float) x, y);
                    else
                        wave.lineTo (badge.getX() + (float) x, y);
                }

                g.setColour (juce::Colours::white.withAlpha (0.22f + 0.16f * (float) line));
                g.strokePath (wave, juce::PathStrokeType (1.5f));
            }
        }

        const auto textArea = bounds.withTrimmedLeft (badge.getWidth() + 10.0f);

        juce::ColourGradient textGradient (juce::Colours::white, textArea.getTopLeft(),
                                           IlanaTheme::accent(), textArea.getBottomRight(), false);
        g.setGradientFill (textGradient);
        g.setFont (IlanaTheme::font (27.0f, true));
        g.drawText ("ilanaSynth", textArea, juce::Justification::centredLeft);
    }

private:
    void timerCallback() override
    {
        phaseOffset += 0.006f;
        repaint();
    }

    float phaseOffset = 0.0f;
};
