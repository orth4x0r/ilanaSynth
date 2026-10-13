#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

#include <array>

class KeyboardStrip : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit KeyboardStrip (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        setTooltip (generalTip());
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
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
        repaint();
    }

    void keyNoteOff (int note)
    {
        if (! juce::isPositiveAndBelow (note, 128) || ! sounding[(size_t) note])
            return;

        sounding[(size_t) note] = false;
        processorRef.triggerPreviewNote (note, false);
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
        repaint();
    }

    void visibilityChanged() override
    {
        if (! isShowing())
            releaseAll();
    }

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
                const auto pressed = isDown (noteNumber);
                const auto hovered = noteNumber == hoverNote && ! pressed;

                g.setColour (pressed ? IlanaTheme::accent().withAlpha (0.45f + 0.5f * pressedVelocity)
                                     : hovered ? IlanaTheme::Ui::text.interpolatedWith (IlanaTheme::accent(), 0.25f)
                                     : IlanaTheme::Ui::text);
                g.fillRect (juce::Rectangle<float> (x + 1.0f, area.getY() + 2.0f,
                                                    whiteWidth - 2.0f, area.getHeight() - 4.0f));

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
                    const auto pressed = isDown (noteNumber);

                    g.setColour (pressed ? IlanaTheme::accent().withAlpha (0.55f + 0.45f * pressedVelocity)
                                         : noteNumber == hoverNote ? IlanaTheme::Ui::bg.interpolatedWith (IlanaTheme::accent(), 0.3f)
                                                                   : IlanaTheme::Ui::bg);
                    g.fillRect (juce::Rectangle<float> (x, area.getY() + 2.0f,
                                                        whiteWidth * 0.6f, area.getHeight() * 0.62f));
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
};
