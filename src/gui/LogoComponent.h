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

        // (The sheet's 145 degree gradient, #ff8a52 to #ff5530 on the ember accent.)
        const auto accent = IlanaTheme::accent();
        g.setGradientFill (juce::ColourGradient (accent.interpolatedWith (juce::Colours::white, 0.12f).withRotatedHue (0.008f), badge.getX() + badge.getWidth() * 0.2f, badge.getY(),
                                                 accent.withRotatedHue (-0.01f).withMultipliedSaturation (1.08f), badge.getRight() - badge.getWidth() * 0.2f, badge.getBottom(), false));
        g.fillRoundedRectangle (badge, radius);

        juce::Path clip;
        clip.addRoundedRectangle (badge, radius);

        {
            juce::Graphics::ScopedSaveState saveState (g);
            g.reduceClipRegion (clip);

            const auto centreY = badge.getCentreY();

            // The mockup's mark: one sine, white, with round ends.
            const auto amplitude = badge.getHeight() * 0.2f;
            const auto inner = badge.reduced (7.0f, 0.0f);
            juce::Path wave;

            for (int x = 0; x <= (int) inner.getWidth(); ++x)
            {
                const auto t = (float) x / juce::jmax (1.0f, inner.getWidth());
                const auto y = centreY + std::sin ((t * 1.0f + phaseOffset) * juce::MathConstants<float>::twoPi) * amplitude;

                if (x == 0)
                    wave.startNewSubPath (inner.getX() + (float) x, y);
                else
                    wave.lineTo (inner.getX() + (float) x, y);
            }

            g.setColour (juce::Colours::white);
            g.strokePath (wave, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        auto textArea = bounds.withTrimmedLeft (badge.getWidth() + 10.0f);
        const auto nameFont = juce::Font (IlanaTheme::font (20.5f, true)); // (15 px at 800)
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (nameFont);
        g.drawText ("ilanaSynth", textArea.removeFromTop (textArea.getHeight() * 0.56f).withTrimmedTop (1.0f), juce::Justification::bottomLeft);

        if (version.isNotEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny).withKerningFactor (0.08f)); // (9 px at 600)
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
