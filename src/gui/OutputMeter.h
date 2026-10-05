#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// A stereo peak meter for the master output, named OUT: bars fall
// smoothly, a peak line holds for a moment, ticks mark 0, -12 and -24 dB, and
// the clip light beside the name lights red after a clip until clicked. The tooltip
// gives the loudest peak since the last click.
class OutputMeter : public juce::Component,
                    public juce::TooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    explicit OutputMeter (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        startTimerHz (30);
    }

    juce::String getTooltip() override
    {
        const auto db = juce::Decibels::gainToDecibels (loudest, -100.0f);
        return juce::String ("Output level (peak, ticks at 0, -12 and -24 dB).\n")
               + (loudest < 1.0e-5f ? juce::String ("Silent since the last reset.")
                                    : "Loudest peak: " + juce::String (db, 1) + " dB" + (clipped ? " (clipped)." : "."))
               + "\nThe light above lights red if the output went over 0 dB; click to reset it.";
    }

    bool isClipLit() const { return clipped; }

    void paint (juce::Graphics& g) override
    {
        // A stereo instrument, not a sliver (UI review 13, V13-5): a name
        // column on the left ("OUT", the loudest hold in dB, the clip light)
        // and two bars about 14 px tall, left over right, with the 0, -12 and
        // -24 dB ticks drawn across them.
        auto whole = getLocalBounds().toFloat();
        auto names = whole.removeFromLeft (58.0f);
        whole.removeFromLeft (4.0f);

        auto top = names.removeFromTop (names.getHeight() * 0.5f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText ("OUT", top.withTrimmedLeft (2.0f), juce::Justification::bottomLeft);

        // The clip light, a dot at the right of the name.
        const auto light = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ top.getRight() - 6.0f, top.getBottom() - 7.0f });
        g.setColour (clipped ? juce::Colour (0xffff4f5e) : IlanaTheme::Ui::raised);
        g.fillEllipse (light);
        g.setColour (clipped ? juce::Colour (0xffff4f5e).brighter (0.4f) : IlanaTheme::Ui::line);
        g.drawEllipse (light.reduced (0.5f), 1.0f);

        // The loudest hold in dB, so the meter has a number (V11-10).
        {
            const auto peak = juce::jmax (holds[0], holds[1]);
            g.setColour (IlanaTheme::Ui::text2.withAlpha (peak > 0.001f ? 1.0f : 0.6f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, false, true));
            IlanaTheme::drawFitted (g, peak > 0.001f ? juce::String (juce::Decibels::gainToDecibels (peak, -60.0f), 1) + " dB" : juce::String ("-- dB"),
                                    names.withTrimmedLeft (2.0f).toNearestInt(), juce::Justification::topLeft);
        }

        const auto bounds = whole.reduced (0.0f, 1.0f);
        IlanaTheme::paintWell (g, bounds, 3.0f);
        const auto inner = bounds.reduced (2.0f, 2.0f);
        const auto barHeight = (inner.getHeight() - 2.0f) * 0.5f;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto bar = juce::Rectangle<float> (inner.getX(), inner.getY() + (float) channel * (barHeight + 2.0f),
                                                     inner.getWidth(), barHeight);
            const auto level = proportion (levels[(size_t) channel]);
            const auto filled = bar.withWidth (bar.getWidth() * level);

            juce::ColourGradient gradient (juce::Colour (0xff4fd1a5), bar.getX(), bar.getY(),
                                           juce::Colour (0xffff4f5e), bar.getRight(), bar.getY(), false);
            gradient.addColour (proportion (juce::Decibels::decibelsToGain (-6.0f)), juce::Colour (0xffffd447));
            g.setGradientFill (gradient);
            g.fillRect (filled);

            const auto hold = proportion (holds[(size_t) channel]);

            if (hold > 0.01f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRect (bar.withX (bar.getX() + bar.getWidth() * hold - 1.0f).withWidth (1.0f));
            }
        }

        // dB ticks across both bars, with the 0 and -12 marks named.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        for (const auto db : { 0.0f, -12.0f, -24.0f })
        {
            const auto x = inner.getX() + inner.getWidth() * proportion (juce::Decibels::decibelsToGain (db));
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (x - 0.5f, inner.getY(), 1.0f, inner.getHeight()));
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        clipped = false;
        loudest = 0.0f;
        repaint();
    }

private:
    // -48 dB .. +3 dB onto 0..1.
    static float proportion (float gain)
    {
        const auto db = juce::Decibels::gainToDecibels (gain, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f);
    }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        auto changed = false;

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto peak = processorRef.takeOutputPeak (channel);
            auto& level = levels[(size_t) channel];
            auto& hold = holds[(size_t) channel];
            auto& holdSeconds = holdTimes[(size_t) channel];
            const auto before = level;

            // Rise at once, fall about 20 dB per second.
            level = juce::jmax (peak, IlanaAnim::decay (level, 0.925f, frameTicks()));

            if (level < 0.0005f)
                level = 0.0f;

            if (peak >= hold)
            {
                hold = peak;
                holdSeconds = 1.2f;
            }
            else if ((holdSeconds -= frameSeconds()) <= 0.0f)
            {
                hold = IlanaAnim::decay (hold, 0.8f, frameTicks());
            }

            clipped = clipped || peak > 1.0f;
            loudest = juce::jmax (loudest, peak);
            changed = changed || std::abs (level - before) > 1.0e-4f || hold > 0.0005f;
        }

        if (changed || clipped)
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    std::array<float, 2> levels {}, holds {};
    std::array<float, 2> holdTimes {};
    float loudest = 0.0f;
    bool clipped = false;
};
