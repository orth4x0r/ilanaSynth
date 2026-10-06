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
        // The mockup's mark: a 34 px badge in an accent gradient with the
        // waves in it, the name and, small beneath it, the version.
        const auto bounds = getLocalBounds().toFloat();
        const auto badge = bounds.withSizeKeepingCentre (34.0f, 34.0f).withX (bounds.getX());
        const auto radius = 9.0f;

        g.setGradientFill (juce::ColourGradient (IlanaTheme::accent().brighter (0.3f), badge.getX(), badge.getY(),
                                                 IlanaTheme::accent().darker (0.05f), badge.getRight(), badge.getBottom(), false));
        g.fillRoundedRectangle (badge, radius);

        juce::Path clip;
        clip.addRoundedRectangle (badge, radius);

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
        const auto nameFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::title, true));
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (nameFont);
        g.drawText ("ilanaSynth", textArea.removeFromTop (textArea.getHeight() * 0.56f).withTrimmedTop (1.0f), juce::Justification::bottomLeft);

        if (version.isNotEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.08f));
            g.drawText (version.toUpperCase(), textArea, juce::Justification::topLeft);
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
