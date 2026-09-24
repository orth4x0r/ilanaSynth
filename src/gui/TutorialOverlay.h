#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

class TutorialOverlay : public juce::Component,
                        private juce::Timer
{
public:
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
        dontShowAgain.setColour (juce::ToggleButton::textColourId, juce::Colours::white.withAlpha (0.6f));

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

            g.setColour (juce::Colour (0xff18181c));
            g.fillRoundedRectangle (panel, 10.0f);
            g.setColour (IlanaTheme::accent().withAlpha (0.5f + 0.3f * (1.0f - appear)));
            g.drawRoundedRectangle (panel.reduced (0.5f), 10.0f, 1.5f);

            auto area = panel.toNearestInt().reduced (28);

            g.setColour (IlanaTheme::accent());
            g.setFont (IlanaTheme::font (26.0f, true));
            g.drawText ("Welcome to ilanaSynth", area.removeFromTop (34), juce::Justification::centredLeft);

            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (IlanaTheme::font (13.5f));
            g.drawText ("A one-minute tour.  Reopen it any time with the ? button at the bottom.",
                        area.removeFromTop (18), juce::Justification::centredLeft);

            area.removeFromTop (14);
            area.removeFromBottom (10);

            auto left = area.removeFromLeft (area.getWidth() / 2 - 12);
            auto right = area.removeFromRight (area.getWidth() - 12);

            const juce::StringArray playTips {
                "Right-click an oscillator's waveform to pick a factory sample, or drag in your own wav - it switches to Sample mode.",
                "ENV and LFO tabs start with one unit; press + to reveal more envelopes and up to four LFOs.",
                "Drag a coloured source chip onto any knob to create a modulation; right-click knobs to edit or MIDI-learn.",
                "Every unit has its own colour - envelope curves, LFO traces and the chips all match.",
                "MATRIX has 8 slots with live sources: mod wheel, aftertouch, macros, clocked S&H and MSEG."
            };

            const juce::StringArray workflowTips {
                "The FX rack starts empty - click an empty slot to add an effect, then drag rows to reorder.",
                "A/B keeps two FX chains; COPY A/B clones the current bank onto the other.",
                "DICE rolls a fresh patch, INIT resets, Ctrl+Z undoes everything - HIST lists your history.",
                "Presets: click to audition, chips filter by category, SAVE AS stores your own in the browser.",
                "XTRA holds the voice engine: cross mod, arp, resonator and the 2x oversampling switch."
            };

            drawTipColumn (g, left, "PLAY", playTips);
            drawTipColumn (g, right, "WORKFLOW", workflowTips);

            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.setFont (IlanaTheme::font (12.5f));
            g.drawText ("1-9 switch tabs    Ctrl+Z / Ctrl+Shift+Z undo / redo    ? reopens this tour",
                        area.removeFromBottom (18), juce::Justification::centredLeft);
        }
    }

private:
    static void drawTipColumn (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& heading,
                               const juce::StringArray& tips)
    {
        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (13.5f, true));
        g.drawText (heading, area.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (IlanaTheme::font (13.5f));

        for (const auto& tip : tips)
        {
            auto row = area.removeFromTop (52).reduced (0, 2);

            g.setColour (IlanaTheme::accent().withAlpha (0.9f));
            g.fillEllipse ((float) row.getX(), (float) row.getY() + 5.0f, 5.0f, 5.0f);

            g.setColour (juce::Colours::white.withAlpha (0.8f));
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
