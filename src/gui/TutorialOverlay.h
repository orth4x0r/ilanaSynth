#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"

class TutorialOverlay : public juce::Component,
                        private IlanaAnim::FrameTimer
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
        closeButton.onClick = [this] { dismiss(); };

        dontShowAgain.setButtonText ("Don't show this again");
        dontShowAgain.setToggleState (true, juce::dontSendNotification);
        dontShowAgain.setColour (juce::ToggleButton::textColourId, IlanaTheme::Ui::text2);

        addAndMakeVisible (closeButton);
        addAndMakeVisible (dontShowAgain);
    }

    std::function<void (bool dontShowAgain)> onDismiss;

    // GOT IT, or Esc: close, honouring "Don't show this again".
    void dismiss()
    {
        const auto dontShow = dontShowAgain.getToggleState();
        setVisible (false);

        if (onDismiss != nullptr)
            onDismiss (dontShow);
    }

    // How many factory presets there are, for the tips.
    void setPresetCount (int count) { presetCount = count; }

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

        // The one button here: solid in the current accent, with white text,
        // so it reads as the way on.
        closeButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent());
        closeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);

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

                    // Neutral chips with an accent edge (source colours
                    // mean sources everywhere else).
                    g.setColour (IlanaTheme::Ui::raised);
                    g.fillRoundedRectangle (chip, 13.0f);
                    g.setColour (IlanaTheme::accent().withAlpha (0.6f));
                    g.drawRoundedRectangle (chip.reduced (0.5f), 13.0f, 1.0f);
                    g.setColour (IlanaTheme::Ui::text);
                    g.setFont (font);
                    g.drawText (features[i], chip, juce::Justification::centred);
                    x = chip.getRight() + 8.0f;
                }
            }

            // Five tips: the few things that aren't obvious from the screen.
            const juce::StringArray tips {
                "PLAY has the essentials on one screen; the other tabs hold the detail. Number keys switch tabs.",
                "Drag a source chip (bottom row) onto any knob to modulate it, then drag its coloured dot to set the depth.",
                "Each oscillator can be a wavetable, a physical string, a sample or grains: pick it in MODE. TABLE opens the browser.",
                "The FX rack starts empty: click an effect to add it, drag slots to reorder, switch a module off in its header.",
                "The dice rolls a fresh patch and " + commandKey() + "+Z undoes anything; " + juce::String (presetCount) + " factory presets are one click away in the name box."
            };

            drawTipColumn (g, area.withTrimmedRight (area.getWidth() / 5), "START HERE", tips);

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Number keys switch tabs    " + commandKey() + "+Z / " + commandKey() + "+Shift+Z undo / redo    ? reopens this tour",
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

        // The tips share the space evenly (no dead band under the last one).
        const auto rowHeight = juce::jlimit (34, 64, area.getHeight() / juce::jmax (1, tips.size()));

        for (const auto& tip : tips)
        {
            auto row = area.removeFromTop (rowHeight).reduced (0, 2);

            g.setColour (IlanaTheme::accent().withAlpha (0.9f));
            g.fillEllipse ((float) row.getX(), (float) row.getY() + 5.0f, 5.0f, 5.0f);

            g.setColour (IlanaTheme::Ui::text);
            g.drawFittedText (tip, row.withTrimmedLeft (14), juce::Justification::topLeft, 3, 1.0f);
        }
    }

    void timerCallback() override
    {
        appear = juce::jmin (1.0f, appear + 0.07f * frameTicks());
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
    int presetCount = 0;
    bool captureAttempted = false;
    juce::Image backdrop;
};
