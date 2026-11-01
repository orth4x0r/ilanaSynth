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
    // The output level (peak, 1 = 0 dB): while notes sound the wave grows
    // with it, and a loud hit glows round the badge.
    std::function<float()> outputLevel;
    // Room left round the badge for that glow (the component is this much
    // bigger than the mark on every side but the right).
    static constexpr float glowMargin = 8.0f;
    // Shown small right after the name.
    juce::String version;
    // The name's centre line, in this component's coordinates (level with
    // the header buttons').
    float nameCentreY = 0.0f;

    LogoComponent()
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        // The mockup's mark: a 34 px badge in an accent gradient with the
        // waves in it, the name and, small beneath it, the version.
        const auto bounds = getLocalBounds().toFloat().withTrimmedLeft (glowMargin).withTrimmedTop (glowMargin).withTrimmedBottom (glowMargin);
        const auto badge = bounds.withSizeKeepingCentre (34.0f, 34.0f).withX (bounds.getX());
        const auto radius = 9.0f;

        // A loud hit: a soft accent glow round the badge that dies away.
        if (halo > 0.01f)
        {
            for (int i = 4; i >= 1; --i)
            {
                const auto spread = (float) i * 1.8f;
                g.setColour (IlanaTheme::accent().withAlpha (0.16f * halo / (float) i));
                g.fillRoundedRectangle (badge.expanded (spread), radius + spread);
            }
        }

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
            const auto amplitude = badge.getHeight() * waveHeight;
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
        if (! IlanaAnim::showing (*this))
            return;

        const auto sounding = isSounding != nullptr && isSounding();
        const auto full = IlanaAnim::fullMotion();
        const auto ticks = frameTicks() * (float) IlanaAnim::motionClockRate(); // (the flair's clock)
        // The level as the header dots read it, held a little: up at once,
        // down over about 300 ms, so the wave breathes and doesn't flicker.
        const auto peak = sounding && full && outputLevel != nullptr ? juce::jlimit (0.0f, 1.0f, outputLevel() * 1.5f) : 0.0f;
        const auto rise = peak - level; // (how far above the held level: a hit)
        level = peak > level ? peak : IlanaAnim::approach (level, peak, 0.1f, ticks);

        // Silent, the mark is the mockup's (a fifth of the badge); sounding,
        // it runs from a quiet 0.12 to a loud 0.27 (taller clips the badge).
        const auto targetHeight = sounding && full ? 0.12f + 0.15f * level : 0.2f;
        // The glow marks hits, not loudness: a jump in level lights it, and
        // it dies away over about half a second even while the note holds.
        const auto targetHalo = rise > 0.15f ? juce::jlimit (0.0f, 1.0f, peak) : 0.0f;
        const auto before = std::make_pair (waveHeight, halo);
        waveHeight = full ? IlanaAnim::approach (waveHeight, targetHeight, 0.35f, ticks) : targetHeight;
        halo = targetHalo > halo ? targetHalo : IlanaAnim::approach (halo, targetHalo, 0.09f, ticks);

        if (std::abs (waveHeight - targetHeight) < 0.001f)
            waveHeight = targetHeight;
        if (halo < 0.005f)
            halo = 0.0f;

        if (sounding)
            phaseOffset += 0.006f * frameTicks();

        if (sounding || before != std::make_pair (waveHeight, halo))
            repaint();
    }

    float phaseOffset = 0.0f, level = 0.0f, waveHeight = 0.2f, halo = 0.0f;
};
