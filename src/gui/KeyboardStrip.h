#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

#include <array>

class KeyboardStrip : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    explicit KeyboardStrip (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        setTooltip (generalTip());
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        // Every note the synth plays lights its key (the host, the arp, SEQ,
        // spray), not only the ones played here; a key let go fades out over
        // about 250 ms. While nothing changes this only polls a counter.
        startPollingHz (30);
    }

    // No note is left sounding when the strip goes away.
    ~KeyboardStrip() override { releaseAll(); }

    // The hovered key's name leads the tooltip.
    juce::String getTooltip() override
    {
        return hoverNote >= 0 ? noteName (hoverNote) + "\n" + generalTip() : generalTip();
    }

    // The computer keyboard (the editor forwards its keys here): a note
    // sounds, and shows pressed, until its key is released. Any number at once.
    void keyNoteOn (int note, float velocity = 0.8f)
    {
        if (! juce::isPositiveAndBelow (note, 128) || sounding[(size_t) note])
            return;

        sounding[(size_t) note] = true;
        processorRef.triggerPreviewNote (note, true, velocity);
        shownEpoch = ~0u; // (the lights follow on the next frame)
        repaint();
    }

    void keyNoteOff (int note)
    {
        if (! juce::isPositiveAndBelow (note, 128) || ! sounding[(size_t) note])
            return;

        sounding[(size_t) note] = false;
        processorRef.triggerPreviewNote (note, false);
        shownEpoch = ~0u; // (the lights follow on the next frame)
        repaint();
    }

    void releaseAll()
    {
        for (int note = 0; note < 128; ++note)
            if (sounding[(size_t) note])
            {
                sounding[(size_t) note] = false;
                processorRef.triggerPreviewNote (note, false);
            }

        if (pressedNote >= 0)
            processorRef.triggerPreviewNote (pressedNote, false);

        pressedNote = -1;
        shownEpoch = ~0u; // (the lights follow on the next frame)
        repaint();
    }

    void visibilityChanged() override
    {
        if (! isShowing())
            releaseAll();
    }

    // How lit a key is drawn (0..1), for the tests.
    float getKeyLight (int note) const { return juce::isPositiveAndBelow (note, 128) ? light[(size_t) note] : 0.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();

        IlanaTheme::paintWell (g, area, 4.0f);

        const auto whiteWidth = area.getWidth() / (float) whiteCount;

        int whiteIndex = 0;

        for (int octave = 0; octave < numOctaves; ++octave)
        {
            for (int note = 0; note < 12; ++note)
            {
                if (isBlackKey (note))
                    continue;

                const auto noteNumber = firstNote + octave * 12 + note;
                const auto x = area.getX() + (float) whiteIndex * whiteWidth;
                const auto lit = light[(size_t) noteNumber];
                const auto pressed = lit > 0.3f;
                const auto hovered = noteNumber == hoverNote && lit <= 0.0f;
                const juce::Rectangle<float> key (x + 1.0f, area.getY() + 2.0f, whiteWidth - 2.0f, area.getHeight() - 4.0f);

                g.setColour (hovered ? IlanaTheme::Ui::text.interpolatedWith (IlanaTheme::accent(), 0.25f) : IlanaTheme::Ui::text);
                g.fillRect (key);
                paintLit (g, key, lit);

                // Octaves: each C named at the foot of its key (C3 = MIDI 60,
                // as on the parameter readouts).
                // Left out when a key is too narrow for it at the type floor.
                if (note == 0 && area.getHeight() >= 18.0f && whiteWidth >= 14.0f)
                {
                    g.setColour (juce::Colours::black.withAlpha (pressed ? 0.7f : 0.45f));
                    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                    g.drawText ("C" + juce::String (noteNumber / 12 - 2),
                                juce::Rectangle<float> (x + 1.0f, area.getBottom() - 15.0f, whiteWidth - 2.0f, 12.0f),
                                juce::Justification::centred);
                }
                ++whiteIndex;
            }
        }

        whiteIndex = 0;

        for (int octave = 0; octave < numOctaves; ++octave)
        {
            for (int note = 0; note < 12; ++note)
            {
                if (isBlackKey (note))
                {
                    const auto noteNumber = firstNote + octave * 12 + note;
                    const auto x = area.getX() + (float) whiteIndex * whiteWidth - whiteWidth * 0.3f;
                    const auto lit = light[(size_t) noteNumber];
                    const juce::Rectangle<float> key (x, area.getY() + 2.0f, whiteWidth * 0.6f, area.getHeight() * 0.62f);

                    g.setColour (noteNumber == hoverNote && lit <= 0.0f ? IlanaTheme::Ui::bg.interpolatedWith (IlanaTheme::accent(), 0.3f)
                                                                        : IlanaTheme::Ui::bg);
                    g.fillRect (key);
                    paintLit (g, key, lit);
                }
                else
                {
                    ++whiteIndex;
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        // Only the primary button plays (a right-click is for a menu).
        if (event.mods.isPopupMenu())
            return;

        // Shift- or Ctrl/Cmd-click holds a note on (click it again to let it
        // go), so a chord can be built one key at a time.
        if (event.mods.isShiftDown() || event.mods.isCommandDown())
        {
            const auto note = noteAt (event.position);
            latching = true;

            if (sounding[(size_t) note])
                keyNoteOff (note);
            else
                keyNoteOn (note, velocityAt (event.position));

            return;
        }

        startNote (noteAt (event.position), velocityAt (event.position));
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (latching || event.mods.isPopupMenu() || pressedNote < 0)
            return;

        // Sliding to another key plays it; the velocity is the one the key
        // was pressed with (a new value would show but not sound).
        const auto note = noteAt (event.position);

        if (note != pressedNote)
            startNote (note, velocityAt (event.position));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        latching = false;

        if (pressedNote >= 0)
            processorRef.triggerPreviewNote (pressedNote, false);

        pressedNote = -1;
        shownEpoch = ~0u; // (the lights follow on the next frame)
        repaint();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (const auto note = noteAt (event.position); note != hoverNote)
        {
            hoverNote = note;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverNote = -1;
        repaint();
    }

private:
    // A lit key: the accent over it as strong as the note was hit, a
    // brighter top edge, and a soft wash down from it.
    static void paintLit (juce::Graphics& g, juce::Rectangle<float> key, float lit)
    {
        if (lit <= 0.002f)
            return;

        const auto accent = IlanaTheme::accent();
        g.setColour (accent.withAlpha (0.92f * lit));
        g.fillRect (key);
        g.setGradientFill (juce::ColourGradient (accent.interpolatedWith (juce::Colours::white, 0.45f).withAlpha (0.55f * lit), key.getX(), key.getY(),
                                                 accent.withAlpha (0.0f), key.getX(), key.getY() + key.getHeight() * 0.6f, false));
        g.fillRect (key);
        g.setColour (accent.interpolatedWith (juce::Colours::white, 0.7f).withAlpha (lit));
        g.fillRect (key.withHeight (2.0f));
    }

    // A key's own column (white keys hold the black keys beside them in
    // their repaint, since a black key sits over two whites).
    juce::Rectangle<int> keyColumn (int note) const
    {
        const auto whiteWidth = (float) getWidth() / (float) whiteCount;
        static const int whiteBefore[] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
        const auto offset = note - firstNote;
        const auto index = (offset / 12) * 7 + whiteBefore[offset % 12];
        const auto x = (float) index * whiteWidth;
        return juce::Rectangle<float> (x - whiteWidth * 0.5f, 0.0f, whiteWidth * 2.0f, (float) getHeight()).getSmallestIntegerContainer();
    }

    float targetLight (int note) const
    {
        if (isDown (note))
            return 0.3f + 0.7f * (note == pressedNote ? pressedVelocity : 0.8f);

        const auto velocity = processorRef.getUiNoteVelocity (note);
        return velocity > 0 ? 0.3f + 0.7f * (float) velocity / 127.0f : 0.0f; // (a soft note faint, a hard one full)
    }

    void timerCallback() override
    {
        if (! IlanaAnim::showing (*this))
            return;

        const auto epoch = processorRef.getUiNoteEpoch();
        if (epoch == shownEpoch && ! fading)
            return;

        shownEpoch = epoch;
        const auto step = IlanaAnim::fullMotion() ? frameSeconds() * (float) IlanaAnim::motionClockRate() / 0.25f : 1.0f;
        auto anyFading = false;

        for (int note = firstNote; note < firstNote + numOctaves * 12; ++note)
        {
            auto& shown = light[(size_t) note];
            const auto target = targetLight (note);
            const auto before = shown;

            // On at once, as hard as it was hit; off in a 250 ms fade.
            shown = target >= shown ? target : juce::jmax (target, shown - step);
            anyFading = anyFading || shown > target;

            if (shown != before)
                repaint (keyColumn (note));
        }

        if (anyFading != fading)
        {
            fading = anyFading;
            if (fading)
                startTimerHz (60);
            else
                startPollingHz (30);
        }
    }

    static juce::String generalTip()
    {
        return "Click or drag to audition notes.\n"
               "Click lower on a key for higher velocity.\n"
               "Shift-click holds a note, to build a chord.\n"
               "Computer keys A to K play notes, Z and X shift the octave.";
    }

    // C3 = MIDI 60, as on the parameter readouts.
    static juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    bool isDown (int note) const
    {
        return note == pressedNote || (juce::isPositiveAndBelow (note, 128) && sounding[(size_t) note]);
    }

    static bool isBlackKey (int note)
    {
        return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
    }

    int noteAt (juce::Point<float> position) const
    {
        const auto area = getLocalBounds().toFloat();
        const auto whiteWidth = area.getWidth() / (float) whiteCount;
        const auto blackZone = area.getY() + area.getHeight() * 0.62f;

        int whiteIndex = 0;

        for (int octave = 0; octave < numOctaves; ++octave)
        {
            for (int note = 0; note < 12; ++note)
            {
                if (isBlackKey (note))
                {
                    const auto x = area.getX() + (float) whiteIndex * whiteWidth - whiteWidth * 0.3f;

                    if (position.y < blackZone && position.x >= x && position.x <= x + whiteWidth * 0.6f)
                        return firstNote + octave * 12 + note;
                }
                else
                {
                    ++whiteIndex;
                }
            }
        }

        static const int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
        const auto index = juce::jlimit (0, whiteCount - 1,
                                         (int) ((position.x - area.getX()) / whiteWidth));

        return firstNote + (index / 7) * 12 + whiteNotes[index % 7];
    }

    float velocityAt (juce::Point<float> position) const
    {
        const auto area = getLocalBounds().toFloat();
        const auto proportion = juce::jlimit (0.0f, 1.0f, (position.y - area.getY()) / juce::jmax (1.0f, area.getHeight()));
        return juce::jlimit (0.25f, 1.0f, 0.35f + 0.65f * proportion);
    }

    void startNote (int note, float velocity)
    {
        if (pressedNote >= 0)
            processorRef.triggerPreviewNote (pressedNote, false);

        pressedNote = note;
        pressedVelocity = velocity;
        processorRef.triggerPreviewNote (note, true, velocity);
        shownEpoch = ~0u; // (the lights follow on the next frame)
        repaint();
    }

    static constexpr int numOctaves = 6; // C1 to C6 (design pass: doubled)
    static constexpr int firstNote = 36;
    static constexpr int whiteCount = 7 * numOctaves;

    IlanaSynthAudioProcessor& processorRef;
    std::array<bool, 128> sounding {}; // held by the computer keyboard or latched
    bool latching = false;
    int hoverNote = -1;
    int pressedNote = -1;
    float pressedVelocity = 0.7f;
    std::array<float, 128> light {};
    juce::uint32 shownEpoch = ~0u;
    bool fading = false;
};
