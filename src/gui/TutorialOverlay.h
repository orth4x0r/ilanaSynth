#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

class TutorialOverlay : public juce::Component,
                        private juce::Timer
{
public:
    // The shortcut modifier as the platform names it.
    static juce::String commandKey()
    {
       #if JUCE_MAC
        return "Cmd";
       #else
        return "Ctrl";
       #endif
    }

    TutorialOverlay()
    {
        closeButton.setButtonText ("GOT IT");
        closeButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent().withAlpha (0.85f));
        closeButton.onClick = [this]
        {
            const auto dontShow = dontShowAgain.getToggleState();
            setVisible (false);

            if (onDismiss != nullptr)
                onDismiss (dontShow);
        };

        dontShowAgain.setButtonText ("Don't show this again");
        dontShowAgain.setToggleState (true, juce::dontSendNotification);
        dontShowAgain.setColour (juce::ToggleButton::textColourId, IlanaTheme::Ui::text2);

        addAndMakeVisible (closeButton);
        addAndMakeVisible (dontShowAgain);
    }

    std::function<void (bool dontShowAgain)> onDismiss;

    void captureBackdrop()
    {
        backdrop = {};

        if (auto* parent = getParentComponent())
            backdrop = IlanaAnim::blurredSnapshot (*parent, 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (70).reduced (28);
        auto bottom = area.removeFromBottom (40);
        dontShowAgain.setBounds (bottom.removeFromLeft (220).reduced (0, 8));
        closeButton.setBounds (bottom.removeFromRight (140).reduced (0, 2));
    }

    void visibilityChanged() override
    {
        if (! isVisible())
        {
            backdrop = {};
            return;
        }

        appear = 0.0f;
        closeButton.setAlpha (0.0f);
        dontShowAgain.setAlpha (0.0f);

        if (! backdrop.isValid() && ! captureAttempted)
        {
            captureAttempted = true;
            setVisible (false);
            captureBackdrop();
            setVisible (true);
            return;
        }

        captureAttempted = false;
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override
    {
        const auto eased = IlanaAnim::easeOutBack (appear);
        const auto panel = getLocalBounds().reduced (70).toFloat();

        if (backdrop.isValid())
            g.drawImage (backdrop, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

        g.setColour (juce::Colours::black.withAlpha (0.5f + 0.15f * appear));
        g.fillAll();

        {
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - IlanaAnim::easeOutCubic (appear)) * 26.0f)
                                .scaled (0.96f + 0.04f * eased, 0.96f + 0.04f * eased,
                                         panel.getCentreX(), panel.getCentreY()));
            g.setOpacity (juce::jlimit (0.0f, 1.0f, appear * 1.4f));

            g.setColour (IlanaTheme::Ui::panel);
            g.fillRoundedRectangle (panel, 10.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.5f + 0.3f * (1.0f - appear)));
            g.drawRoundedRectangle (panel.reduced (0.5f), 10.0f, 1.5f);

            auto area = panel.toNearestInt().reduced (28);

            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::hero, true));
            g.drawText ("Welcome to ilanaSynth", area.removeFromTop (34), juce::Justification::centredLeft);

            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("A one-minute tour.  Reopen it any time with the ? button at the bottom.",
                        area.removeFromTop (18), juce::Justification::centredLeft);

            area.removeFromTop (14);
            area.removeFromBottom (44); // GOT IT and the checkbox

            // Shortcuts, then what's new, along the bottom; the tips fill the rest.
            auto shortcuts = area.removeFromBottom (18);
            area.removeFromBottom (12);
            auto newBand = area.removeFromBottom (58);
            area.removeFromBottom (10);

            {
                g.setColour (IlanaTheme::accent());
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
                g.drawText ("NEW SINCE 1.2", newBand.removeFromTop (22), juce::Justification::centredLeft);

                const juce::StringArray features { "PHYSICAL PAGE", "GRAND PIANO", "CHAOS LFOs", "WEST COAST", "25 FILTERS",
                                                   "FEEDBACK GUITAR", "VECTOR + EVOLVE", "BOUNCE", "WAVETABLE EDITOR" };
                const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
                auto x = (float) newBand.getX();

                for (int i = 0; i < features.size(); ++i)
                {
                    const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, features[i]) + 22.0f;
                    const auto chip = juce::Rectangle<float> (x, (float) newBand.getY() + 4.0f, width, 26.0f);

                    if (chip.getRight() > (float) newBand.getRight())
                        break;

                    const auto hue = std::fmod (0.02f + (float) i * 0.11f, 1.0f);
                    const auto colour = juce::Colour::fromHSV (hue, 0.6f, 1.0f, 1.0f);
                    g.setColour (colour.withAlpha (0.14f));
                    g.fillRoundedRectangle (chip, 13.0f);
                    g.setColour (colour.withAlpha (0.7f));
                    g.drawRoundedRectangle (chip.reduced (0.5f), 13.0f, 1.0f);
                    g.setColour (colour.brighter (0.3f));
                    g.setFont (font);
                    g.drawText (features[i], chip, juce::Justification::centred);
                    x = chip.getRight() + 8.0f;
                }
            }

            auto left = area.removeFromLeft (area.getWidth() / 2 - 12);
            auto right = area.removeFromRight (area.getWidth() - 12);

            const juce::StringArray playTips {
                "Each oscillator is Wavetable, Physical, Sample, Granular or Live (ilanaSynth FX's input). Click TABLE for the visual browser; drop a wav on the display to sample it.",
                "MAIN puts the oscillators, filter, amp envelope and LFOs on one screen; ENV/LFO shows every envelope and LFO as a card.",
                "Drag a source chip or an LFO/envelope card onto any knob to modulate it, then drag its dot to set the depth.",
                "Try the simulated LFO shapes (Lorenz, Bounce, Pendulum...), the WEST card on FILTER, or PHYSICAL to watch a string move.",
                "MATRIX has 64 slots, each with a curve, polarity and a Via source that scales it (e.g. mod wheel fading in an LFO)."
            };

            const juce::StringArray workflowTips {
                "The FX rack starts empty - use the quick-add buttons or click a slot, then drag rows to reorder.",
                "BOUNCE on an oscillator card renders the patch into it as a sample or a wavetable; VECTOR mixes four oscillators and lets the macros EVOLVE.",
                "DICE rolls a fresh patch, INIT resets, " + commandKey() + "+Z undoes everything - HIST lists your history.",
                "371 presets, all with named macros. SAVE stores your own with a category and tags; search finds tags too.",
                "FM has six operators, 16 one-click algorithms and a noise operator. ARP/SEQ has the arp, step LFOs and GENERATE: scale snap and note spray."
            };

            drawTipColumn (g, left, "PLAY", playTips);
            drawTipColumn (g, right, "WORKFLOW", workflowTips);

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("1-9, 0 switch tabs    " + commandKey() + "+Z / " + commandKey() + "+Shift+Z undo / redo    ? reopens this tour",
                        shortcuts, juce::Justification::centredLeft);
        }
    }

private:
    static void drawTipColumn (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& heading,
                               const juce::StringArray& tips)
    {
        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (heading, area.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));

        for (const auto& tip : tips)
        {
            auto row = area.removeFromTop (52).reduced (0, 2);

            g.setColour (IlanaTheme::accent().withAlpha (0.9f));
            g.fillEllipse ((float) row.getX(), (float) row.getY() + 5.0f, 5.0f, 5.0f);

            g.setColour (IlanaTheme::Ui::text);
            g.drawFittedText (tip, row.withTrimmedLeft (14), juce::Justification::topLeft, 3, 1.0f);
        }
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.07f);
        const auto controlAlpha = juce::jlimit (0.0f, 1.0f, (appear - 0.5f) * 2.0f);
        closeButton.setAlpha (controlAlpha);
        dontShowAgain.setAlpha (controlAlpha);
        repaint();

        if (appear >= 1.0f)
        {
            closeButton.setAlpha (1.0f);
            dontShowAgain.setAlpha (1.0f);
            stopTimer();
        }
    }

    juce::TextButton closeButton;
    juce::ToggleButton dontShowAgain;
    float appear = 0.0f;
    bool captureAttempted = false;
    juce::Image backdrop;
};
