#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

class LogoComponent : public juce::Component,
                      private IlanaAnim::FrameTimer
{
public:
    // The waves in the badge scroll while this says the synth is sounding.
    std::function<bool()> isSounding;
    // Shown small right after the name.
    juce::String version;
    // The name's centre line, in this component's coordinates (level with
    // the header buttons').
    float nameCentreY = 0.0f;

    LogoComponent()
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto badge = bounds.withWidth (bounds.getHeight()).reduced (2.0f);

        constexpr auto glow = 0.6f;
        g.setColour (IlanaTheme::accent().withAlpha (0.08f + 0.12f * glow));
        g.fillRoundedRectangle (badge.expanded (4.0f + 2.0f * glow), badge.getWidth() * 0.33f);

        g.setColour (IlanaTheme::accent());
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

                g.setColour (juce::Colours::white.withAlpha (0.35f + 0.25f * (float) line));
                g.strokePath (wave, juce::PathStrokeType (1.5f));
            }
        }

        auto textArea = bounds.withTrimmedLeft (badge.getWidth() + 10.0f);
        if (nameCentreY > 0.0f)
            textArea = textArea.withSizeKeepingCentre (textArea.getWidth(), 28.0f).withCentre ({ textArea.getCentreX(), nameCentreY });

        const auto nameFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::display, true));
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (nameFont);
        g.drawText ("ilanaSynth", textArea, juce::Justification::centredLeft);

        if (version.isNotEmpty())
        {
            // The version beside the name, on its upper half.
            const auto nameWidth = juce::GlyphArrangement::getStringWidth (nameFont, "ilanaSynth");
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.drawText (version, textArea.withTrimmedLeft (nameWidth + 5.0f).withHeight (textArea.getHeight() * 0.55f),
                        juce::Justification::centredLeft);
        }
    }

private:
    void timerCallback() override
    {
        if (isSounding != nullptr && isSounding() && isShowing())
        {
            phaseOffset += 0.006f * frameTicks();
            repaint();
        }
    }

    float phaseOffset = 0.0f;
};
