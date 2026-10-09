#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

class KeyboardStrip : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    explicit KeyboardStrip (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        setTooltip ("Click or drag to audition notes.\n"
                    "Click lower on a key for higher velocity.");
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
                const auto pressed = noteNumber == pressedNote;

                g.setColour (pressed ? IlanaTheme::accent().withAlpha (0.45f + 0.5f * pressedVelocity)
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
                    const auto pressed = noteNumber == pressedNote;

                    g.setColour (pressed ? IlanaTheme::accent().withAlpha (0.55f + 0.45f * pressedVelocity)
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
        startNote (noteAt (event.position), velocityAt (event.position));
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto note = noteAt (event.position);
        const auto velocity = velocityAt (event.position);

        if (note != pressedNote)
            startNote (note, velocity);
        else if (std::abs (velocity - pressedVelocity) > 0.05f)
            pressedVelocity = velocity;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (pressedNote >= 0)
            processorRef.triggerPreviewNote (pressedNote, false);

        pressedNote = -1;
        repaint();
    }

private:
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

    static constexpr int numOctaves = 3;
    static constexpr int firstNote = 36;
    static constexpr int whiteCount = 7 * numOctaves;

    IlanaSynthAudioProcessor& processorRef;
    int pressedNote = -1;
    float pressedVelocity = 0.7f;
};
