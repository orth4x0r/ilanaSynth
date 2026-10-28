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
    static constexpr const char* whatsNewVersion = "1.4 BETA";

    struct NewFeature
    {
        juce::String label, page, tip;
    };

    static const std::vector<NewFeature>& whatsNew()
    {
        static const std::vector<NewFeature> list {
            { "ONE-TAB PLAY", "MAIN", "The oscillators, filters, strings, west coast, envelopes, LFOs and the body are all on PLAY. "
                                      "The OSC, FILTER and MOD tabs are gone." },
            { "EDIT POP-UPS", "MAIN", "EDIT on the ENVELOPE or LFO card (or a click on a dock chip) opens that source's whole editor, on any page." },
            { "FRAME BLEND", "MAIN", "In a wavetable card's header: Crossfade between frames, Spectral (the harmonics glide) or Stepped." },
            { "FULL-RANGE MODULATION", "MATRIX", "A route at 100 % now sweeps the knob's whole range. Old routes were rescaled and sound the same." },
            { "CLICK + LOOSE", "MAIN", "CLICK takes the slap out of a pluck (the EXCITER row); LOOSE, in the dock's STEREO pop-up, detunes every note a little." },
            { "447 WAVETABLES", "MAIN", "The Surge XT packs are factory tables now; LIBRARY in the table browser loads your own." },
            { "DX7 BANKS + OPERATOR ENV", "FM", "288 DX7 voices load as normal presets (find them by sound in the browser, or under its DX7 chip); "
                                                "any oscillator can use the DX7's envelope (ENVELOPE: Operator Env). Import more .syx banks from the browser." },
            { "AIRWINDOWS", "FX", "Airwindows effect modules (their delays sync to tempo), and character filters in the FILTER band's TYPE menu." },
            { juce::String (FilterType::Count) + " FILTERS", "MAIN", "The filter list, grouped by family in the FILTER band's type menu." },
            { "CLIP SEQUENCER", "ARP/SEQ", "Piano-roll clips under SEQ > CLIP, with MIDI file import." },
            { "SF2 / SFZ", "MAIN", "Drop an .sf2 or .sfz on an oscillator in Sample mode to play it across the keys." },
            { "VOCODER", "FX", "A vocoder effect module." },
            { "PATCH + EFFECTS", "PATCH", "Cable the signal path by hand, the body included, and drag effects in from the carousel underneath." },
            { "FELT HAMMER BOARD", "MAIN", "The reworked piano exciter: try the Felt Hammer Board preset." },
            { "VECTOR + EVOLVE", "VECTOR", "Morph four oscillators from a pad; EVOLVE is on each macro's card." }
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
                 "Every edit can be undone (" + commandKey() + "+Z), preset loads too, so try things: RANDOMISE (the dice) rolls a fresh patch." };
    }

    // The hidden mouse moves, for MOUSE MOVES (Impeccable re-score, H7: the
    // help for what the screen doesn't show). Key caps, then what it does.
    struct Move
    {
        juce::StringArray keys;
        juce::String what;
    };

    static std::vector<Move> moves()
    {
        return { { { "Double-click" }, "a knob: back to its default" },
                 { { "Shift", "drag" }, "a knob, ring or slider: fine moves" },
                 { { "Right-click" }, "a knob: modulate, type a value, copy, MIDI learn" },
                 { { "Drag" }, "a ring around a knob: how far its source moves it (double-click: zero)" },
                 { { "Right-click" }, "a ring: one way up, one way down or both sides" },
                 { { "Right-click" }, "a source chip: where it goes, each route with Remove" },
                 { { "Right-click" }, "a card's name: copy, paste or init that module" },
                 { { commandKey(), "wheel" }, "turn a knob on a page that scrolls" } };
    }

    // For the tests: whether "Don't show this again" is ticked.
    bool isDontShowTicked() const { return dontShowAgain.getToggleState(); }

    void captureBackdrop()
    {
        backdrop = {};

        if (auto* parent = getParentComponent())
            backdrop = IlanaAnim::blurredSnapshot (*parent, 2.0f);
    }

    // The panel is as tall as its content (the tips as tall as their lines:
    // review 7 found a fixed allowance left a gap above NEW IN), centred.
    juce::Rectangle<int> panelBounds() const
    {
        // A reading column, not the window's width (review 11, S11-16).
        auto area = getLocalBounds().reduced (70);
        area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 720), area.getHeight());
        const auto tipsWidth = area.getWidth() - 56;
        const auto contentHeight = 28 + 34 + 18 + 14 + tipColumnHeight (tipsWidth, getTips()) + 10 + movesBandHeight() + 6 + newBandHeight() + 12 + 18 + 44 + 28;
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
        newOpen = false;
        movesOpen = false;
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
            g.drawText ("A one-minute tour.  Reopen it any time with the ? at the top right.",
                        area.removeFromTop (18), juce::Justification::centredLeft);

            area.removeFromTop (14);
            area.removeFromBottom (44); // GOT IT and the checkbox

            // Shortcuts, then what's new, along the bottom; the tips fill the rest.
            auto shortcuts = area.removeFromBottom (18);
            area.removeFromBottom (12);
            auto newBand = area.removeFromBottom (newBandHeight());
            area.removeFromBottom (6);
            auto movesBand = area.removeFromBottom (movesBandHeight());
            area.removeFromBottom (10);

            drawMoves (g, movesBand);

            {
                // A disclosure: the pills are behind it (review 9, S9-20).
                drawDisclosure (g, newBand.removeFromTop (22), "WHAT'S NEW IN " + juce::String (whatsNewVersion),
                                newOpen ? "click one to open its page" : "click to show", newOpen, headingHover);

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

            drawTipColumn (g, area, "START HERE", getTips());

            // The shortcuts as key caps and a word each (V12-29); the settings
            // menu, which has no key, is named last.
            {
                struct Shortcut { juce::StringArray keys; juce::String what; };
                const auto cmd = commandKey();
                const std::vector<Shortcut> shortcuts_ {
                    { { cmd, "1-8" }, "tabs" }, { { cmd, "Shift", "1-3" }, "pages" }, { { cmd, "Z" }, "undo" },
                    { { cmd, "Shift", "Z" }, "redo" }, { { cmd, "S" }, "save" }, { {}, "gear: settings" } };
                const auto capFont = IlanaTheme::font (IlanaTheme::TextSize::label, true);
                const auto wordFont = IlanaTheme::font (IlanaTheme::TextSize::body);
                auto x = (float) shortcuts.getX();
                const auto cy = (float) shortcuts.getCentreY();
                for (const auto& shortcut : shortcuts_)
                {
                    for (int k = 0; k < shortcut.keys.size(); ++k)
                    {
                        const auto width = juce::GlyphArrangement::getStringWidth (capFont, shortcut.keys[k]) + 12.0f;
                        const auto cap = juce::Rectangle<float> (x, cy - 9.0f, width, 18.0f);
                        g.setColour (IlanaTheme::Ui::raised);
                        g.fillRoundedRectangle (cap, 4.0f);
                        g.setColour (IlanaTheme::Ui::line.brighter (0.2f));
                        g.drawRoundedRectangle (cap.reduced (0.5f), 4.0f, 1.0f);
                        g.setColour (IlanaTheme::Ui::text2);
                        g.setFont (capFont);
                        g.drawText (shortcut.keys[k], cap, juce::Justification::centred);
                        x += width + 3.0f;
                    }
                    const auto width = juce::GlyphArrangement::getStringWidth (wordFont, shortcut.what);
                    g.setColour (IlanaTheme::Ui::text3);
                    g.setFont (wordFont);
                    g.drawText (shortcut.what, juce::Rectangle<float> (x + 3.0f, cy - 9.0f, width + 6.0f, 18.0f), juce::Justification::centredLeft);
                    x += width + 22.0f;
                }
            }
        }
    }

    // The chips, packed in rows under the NEW IN heading (as many as fit).
    std::vector<juce::Rectangle<float>> chipBounds() const
    {
        auto area = panelBounds().reduced (28);
        area.removeFromTop (34 + 18 + 14);
        area.removeFromBottom (44 + 18 + 12);
        auto band = area.removeFromBottom (newBandHeight());
        band.removeFromTop (22);

        if (! newOpen)
            return {};

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

    // The WHAT'S NEW line: its pills show only while it is open.
    juce::Rectangle<int> headingBounds() const
    {
        auto area = panelBounds().reduced (28);
        area.removeFromBottom (44 + 18 + 12);
        return area.removeFromBottom (newBandHeight()).removeFromTop (22);
    }

    bool isWhatsNewOpen() const { return newOpen; }

    // One of the two opens at a time, so the panel fits a 720 px window.
    void setWhatsNewOpen (bool open)
    {
        if (open == newOpen)
            return;

        newOpen = open;
        movesOpen = movesOpen && ! open;
        resized();
        repaint();
    }

    // The MOUSE MOVES line, above WHAT'S NEW.
    juce::Rectangle<int> movesHeadingBounds() const
    {
        auto area = panelBounds().reduced (28);
        area.removeFromBottom (44 + 18 + 12 + newBandHeight() + 6);
        return area.removeFromBottom (movesBandHeight()).removeFromTop (22);
    }

    bool isMovesOpen() const { return movesOpen; }

    void setMovesOpen (bool open)
    {
        if (open == movesOpen)
            return;

        movesOpen = open;
        newOpen = newOpen && ! open;
        resized();
        repaint();
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
        const auto onHeading = headingBounds().contains (event.getPosition());
        const auto onMoves = movesHeadingBounds().contains (event.getPosition());

        if (chip != hoverChip || onHeading != headingHover || onMoves != movesHover)
        {
            hoverChip = chip;
            headingHover = onHeading;
            movesHover = onMoves;
            setMouseCursor (chip >= 0 || onHeading || onMoves ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverChip >= 0 || headingHover || movesHover)
        {
            hoverChip = -1;
            headingHover = false;
            movesHover = false;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (appear >= 1.0f && headingBounds().contains (event.getPosition()))
            setWhatsNewOpen (! newOpen);
        else if (appear >= 1.0f && movesHeadingBounds().contains (event.getPosition()))
            setMovesOpen (! movesOpen);
        else if (const auto chip = chipAt (event.position); chip >= 0 && appear >= 1.0f)
            openChip (chip);
    }

    juce::String getTooltip() override
    {
        const auto chip = chipAt (getMouseXYRelative().toFloat());
        return chip >= 0 ? whatsNew()[(size_t) chip].tip : juce::String();
    }

private:
    static constexpr float chipHeight = 26.0f, chipGap = 6.0f;
    // The heading alone, or with two rows of chips under it.
    // (As many rows as the chips need at the column's width: review 11, S11-16.)
    int newBandHeight() const { return newOpen ? 22 + 3 + chipRows() * 26 + (chipRows() - 1) * 6 + 2 : 22; }

    static constexpr int moveRowHeight = 24;
    int movesBandHeight() const { return movesOpen ? 22 + 4 + (int) moves().size() * moveRowHeight : 22; }

    // A disclosure heading: the triangle, the title and a hint after it.
    static void drawDisclosure (juce::Graphics& g, juce::Rectangle<int> heading, const juce::String& title,
                                const juce::String& hint, bool open, bool hover)
    {
        const auto titleFont = IlanaTheme::font (IlanaTheme::TextSize::body, true);
        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (titleFont, title);
        const auto arrow = juce::Rectangle<float> (14.0f, 8.0f).withCentre ({ (float) heading.getX() + 7.0f, (float) heading.getCentreY() });
        juce::Path triangle;
        if (open)
            triangle.addTriangle (arrow.getX() + 1.0f, arrow.getY() + 1.0f, arrow.getRight() - 1.0f, arrow.getY() + 1.0f,
                                  arrow.getCentreX(), arrow.getBottom());
        else
            triangle.addTriangle (arrow.getX() + 3.0f, arrow.getY() - 1.0f, arrow.getX() + 3.0f, arrow.getBottom() + 1.0f,
                                  arrow.getRight() - 2.0f, arrow.getCentreY());
        g.setColour (hover ? IlanaTheme::Ui::text : IlanaTheme::accent());
        g.fillPath (triangle);
        g.setColour (IlanaTheme::accent());
        g.setFont (titleFont);
        g.drawText (title, heading.withTrimmedLeft (18), juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (hint, heading.withTrimmedLeft (18 + titleWidth + 12), juce::Justification::centredLeft);
    }

    // A key cap, as the shortcut line draws them; returns its width.
    static float drawCap (juce::Graphics& g, const juce::String& key, float x, float cy)
    {
        const auto capFont = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto width = juce::GlyphArrangement::getStringWidth (capFont, key) + 12.0f;
        const auto cap = juce::Rectangle<float> (x, cy - 9.0f, width, 18.0f);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (IlanaTheme::Ui::line.brighter (0.2f));
        g.drawRoundedRectangle (cap.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (capFont);
        g.drawText (key, cap, juce::Justification::centred);
        return width;
    }

    // MOUSE MOVES: the heading, and while open one move a row, the caps in
    // one column and what each does on one left edge after them.
    void drawMoves (juce::Graphics& g, juce::Rectangle<int> band) const
    {
        drawDisclosure (g, band.removeFromTop (22), "MOUSE MOVES",
                        movesOpen ? "what right-click, double-click and Shift do" : "click to show", movesOpen, movesHover);

        if (! movesOpen)
            return;

        band.removeFromTop (4);
        const auto capFont = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto all = moves();
        auto capsWidth = 0.0f;

        for (const auto& move : all)
        {
            auto width = 0.0f;
            for (const auto& key : move.keys)
                width += juce::GlyphArrangement::getStringWidth (capFont, key) + 12.0f + 3.0f;
            capsWidth = juce::jmax (capsWidth, width);
        }

        const auto textX = (float) band.getX() + 18.0f + capsWidth + 9.0f;

        for (const auto& move : all)
        {
            const auto row = band.removeFromTop (moveRowHeight);
            const auto cy = (float) row.getCentreY();
            auto x = (float) row.getX() + 18.0f;

            for (const auto& key : move.keys)
                x += drawCap (g, key, x, cy) + 3.0f;

            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText (move.what, juce::Rectangle<float> (textX, (float) row.getY(), (float) row.getRight() - textX, (float) row.getHeight()),
                        juce::Justification::centredLeft);
        }
    }

    int chipRows() const
    {
        const auto width = (float) juce::jmin (getWidth() - 140, 720) - 56.0f;
        const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        auto rows = 1;
        auto x = 0.0f;

        for (const auto& feature : whatsNew())
        {
            const auto chip = (float) juce::GlyphArrangement::getStringWidthInt (font, feature.label) + 22.0f;

            if (x + chip > width && x > 0.0f)
            {
                ++rows;
                x = 0.0f;
            }

            x += chip + 8.0f;
        }

        return rows;
    }

    // The heading and the tips, as drawTipColumn lays them out.
    static int tipColumnHeight (int width, const juce::StringArray& tips)
    {
        const juce::Font font (IlanaTheme::font (IlanaTheme::TextSize::body));
        auto y = 20.0f + 2.0f;

        for (const auto& tip : tips)
        {
            juce::GlyphArrangement lines;
            lines.addJustifiedText (font, tip, 14.0f, y + font.getAscent(), (float) width - 14.0f, juce::Justification::left);
            y = lines.getBoundingBox (0, -1, true).getBottom() + 12.0f;
        }

        return (int) std::ceil (y);
    }

    static void drawTipColumn (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& heading,
                               const juce::StringArray& tips)
    {
        g.setColour (IlanaTheme::accent());
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (heading, area.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));

        // Each tip as tall as its lines, the same gap after each (a fixed
        // row left a one-line tip with a gap under it that a wrapped one
        // didn't have), never shrunk to fit.
        const juce::Font font (IlanaTheme::font (IlanaTheme::TextSize::body));
        auto y = (float) area.getY() + 2.0f;

        for (const auto& tip : tips)
        {
            juce::GlyphArrangement lines;
            lines.addJustifiedText (font, tip, (float) area.getX() + 14.0f, y + font.getAscent(), (float) area.getWidth() - 14.0f,
                                    juce::Justification::left);

            g.setColour (IlanaTheme::accent().withAlpha (0.9f));
            g.fillEllipse ((float) area.getX(), y + font.getAscent() * 0.5f, 5.0f, 5.0f);

            g.setColour (IlanaTheme::Ui::text);
            lines.draw (g);
            y = lines.getBoundingBox (0, -1, true).getBottom() + 12.0f;
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
    bool newOpen = false, headingHover = false;
    bool movesOpen = false, movesHover = false;
    bool captureAttempted = false;
    juce::Image backdrop;
};
