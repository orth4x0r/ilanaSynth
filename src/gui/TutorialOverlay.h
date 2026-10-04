#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "../dsp/FilterUnit.h"

class TutorialOverlay : public juce::Component,
                        public juce::TooltipClient,
                        private IlanaAnim::FrameTimer
{
public:
    // What's new, for the tour's chips: one per feature, with the page a
    // click opens (an id for the editor's showPage) and a line for its
    // tooltip. Update the list and its version together with the changelog;
    // the UI test checks the version against the header's and that every
    // page exists.
    static constexpr const char* whatsNewVersion = "1.3";

    struct NewFeature
    {
        juce::String label, page, tip;
    };

    static const std::vector<NewFeature>& whatsNew()
    {
        static const std::vector<NewFeature> list {
            { "DX7 BANKS + OPERATOR EG", "FM", "288 DX7 voices load as normal patches (find them by sound in the browser, or under its DX7 chip); "
                                                "any oscillator can use the DX7's envelope (ENVELOPE: Operator EG). Import more .syx banks from the browser." },
            { "AIRWINDOWS", "FX", "Airwindows effect modules, and character filters on the FILTER page." },
            { juce::String (FilterType::Count) + " FILTERS", "FILTER", "The filter list, grouped by family on the FILTER page." },
            { "CLIP SEQUENCER", "ARP/SEQ", "Piano-roll clips under SEQ > CLIP, with MIDI file import." },
            { "SF2 / SFZ", "OSC", "Drop an .sf2 or .sfz on an oscillator in Sample mode to play it across the keys." },
            { "VOCODER", "FX", "A vocoder effect module." },
            { "PHYSICAL PAGE", "PHYSICAL", "Strings, plates and tubes struck, plucked or bowed." },
            { "FELT HAMMER BOARD", "PHYSICAL", "The reworked piano exciter: try the Felt Hammer Board preset." },
            { "FEEDBACK GUITAR", "PHYSICAL", "A string that feeds back through its amp." },
            { "CHAOS LFO SHAPES", "ENV/LFO", "LFOs that run a simulation: Lorenz, pendulums, bouncing balls." },
            { "WEST COAST", "FILTER", "A wavefolder and low-pass gate after (or in place of) the filters." },
            { "VECTOR + EVOLVE", "VECTOR", "Morph four oscillators from a pad, or let EVOLVE move them." },
            { "BOUNCE", "OSC", "Resample the whole patch into an oscillator." },
            { "WAVETABLE EDITOR", "OSC", "Draw and edit wavetables from an oscillator's table." }
        };
        return list;
    }

    // A chip's click: the editor shows that page (the tour closes first).
    std::function<void (const juce::String& pageId)> onShowPage;

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
        // Unticked: GOT IT alone brings the tour back next time; ticking
        // it is a choice (UI review 5).
        dontShowAgain.setToggleState (false, juce::dontSendNotification);
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

    // Three tips: the few things that aren't obvious from the screen.
    juce::StringArray getTips() const
    {
        return { "PLAY has the essentials on one screen; the other tabs hold the detail. Click the preset name to browse "
                     + juce::String (presetCount) + " presets by sound, tag or bank.",
                 "Drag a source chip (the row above the macros) onto any knob to modulate it; the knob shows how far it moves.",
                 "Every edit can be undone (" + commandKey() + "+Z), preset loads too, so try things: the dice rolls a fresh patch." };
    }

    // For the tests: whether "Don't show this again" is ticked.
    bool isDontShowTicked() const { return dontShowAgain.getToggleState(); }

    void captureBackdrop()
    {
        backdrop = {};

        if (auto* parent = getParentComponent())
            backdrop = IlanaAnim::blurredSnapshot (*parent, 2.0f);
    }

    // The panel is as tall as its content (one line per tip), centred.
    juce::Rectangle<int> panelBounds() const
    {
        constexpr int contentHeight = 28 + 34 + 18 + 14 + 20 + 3 * 40 + 10 + newBandHeight + 12 + 18 + 44 + 28;
        const auto area = getLocalBounds().reduced (70);
        return area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), contentHeight));
    }

    void resized() override
    {
        auto area = panelBounds().reduced (28);
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
        const auto panel = panelBounds().toFloat();

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
            g.drawText ("A one-minute tour.  Reopen it any time with the ? beside SCOPE.",
                        area.removeFromTop (18), juce::Justification::centredLeft);

            area.removeFromTop (14);
            area.removeFromBottom (44); // GOT IT and the checkbox

            // Shortcuts, then what's new, along the bottom; the tips fill the rest.
            auto shortcuts = area.removeFromBottom (18);
            area.removeFromBottom (12);
            auto newBand = area.removeFromBottom (newBandHeight);
            area.removeFromBottom (10);

            {
                g.setColour (IlanaTheme::accent());
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
                const auto heading = newBand.removeFromTop (22);
                const auto title = "NEW IN " + juce::String (whatsNewVersion);
                g.drawText (title, heading, juce::Justification::centredLeft);
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText ("click one to open its page",
                            heading.withTrimmedLeft (juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::body, true), title) + 12),
                            juce::Justification::centredLeft);

                const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
                const auto& features = whatsNew();
                const auto chips = chipBounds();

                for (size_t i = 0; i < chips.size(); ++i)
                {
                    const auto chip = chips[i];
                    const auto hover = (int) i == hoverChip;

                    // Neutral chips with an accent edge (source colours
                    // mean sources everywhere else); a hovered one lights.
                    g.setColour (hover ? IlanaTheme::Ui::raised.brighter (0.15f) : IlanaTheme::Ui::raised);
                    g.fillRoundedRectangle (chip, chip.getHeight() * 0.5f);
                    g.setColour (IlanaTheme::accent().withAlpha (hover ? 1.0f : 0.6f));
                    g.drawRoundedRectangle (chip.reduced (0.5f), chip.getHeight() * 0.5f, 1.0f);
                    g.setColour (IlanaTheme::Ui::text);
                    g.setFont (font);
                    g.drawText (features[i].label, chip, juce::Justification::centred);
                }
            }

            drawTipColumn (g, area.withTrimmedRight (area.getWidth() / 5), "START HERE", getTips());

            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText (commandKey() + "+1-7 switch tabs    " + commandKey() + "+Z / " + commandKey() + "+Shift+Z undo / redo    "
                            + commandKey() + "+S save",
                        shortcuts, juce::Justification::centredLeft);
        }
    }

    // The chips, packed in rows under the NEW IN heading (as many as fit).
    std::vector<juce::Rectangle<float>> chipBounds() const
    {
        auto area = panelBounds().reduced (28);
        area.removeFromTop (34 + 18 + 14);
        area.removeFromBottom (44 + 18 + 12);
        auto band = area.removeFromBottom (newBandHeight);
        band.removeFromTop (22);

        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        std::vector<juce::Rectangle<float>> chips;
        auto x = (float) band.getX();
        auto y = (float) band.getY() + 3.0f;

        for (const auto& feature : whatsNew())
        {
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, feature.label) + 22.0f;

            if (x + width > (float) band.getRight())
            {
                x = (float) band.getX();
                y += chipHeight + chipGap;
            }

            if (y + chipHeight > (float) band.getBottom())
                break;

            chips.push_back ({ x, y, width, chipHeight });
            x += width + 8.0f;
        }

        return chips;
    }

    int chipAt (juce::Point<float> position) const
    {
        const auto chips = chipBounds();

        for (size_t i = 0; i < chips.size(); ++i)
            if (chips[i].contains (position))
                return (int) i;

        return -1;
    }

    // What a chip's click does (the UI test calls it too).
    void openChip (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) whatsNew().size()))
            return;

        const auto page = whatsNew()[(size_t) index].page;
        dismiss();

        if (onShowPage != nullptr)
            onShowPage (page);
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto chip = chipAt (event.position);

        if (chip != hoverChip)
        {
            hoverChip = chip;
            setMouseCursor (chip >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverChip >= 0)
        {
            hoverChip = -1;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (const auto chip = chipAt (event.position); chip >= 0 && appear >= 1.0f)
            openChip (chip);
    }

    juce::String getTooltip() override
    {
        const auto chip = chipAt (getMouseXYRelative().toFloat());
        return chip >= 0 ? whatsNew()[(size_t) chip].tip : juce::String();
    }

private:
    static constexpr float chipHeight = 26.0f, chipGap = 6.0f;
    static constexpr int newBandHeight = 22 + 3 + 2 * 26 + 6 + 2; // two rows of chips

    static void drawTipColumn (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& heading,
                               const juce::StringArray& tips)
    {
        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (heading, area.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));

        // The tips share the space evenly (no dead band under the last one).
        const auto rowHeight = juce::jlimit (34, 48, area.getHeight() / juce::jmax (1, tips.size()));

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
    int hoverChip = -1;
    bool captureAttempted = false;
    juce::Image backdrop;
};
