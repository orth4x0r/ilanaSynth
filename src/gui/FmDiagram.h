#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"

// The FM matrix as operators: three oscillators, with an arrow for every
// route (thicker = deeper) and a loop for feedback. Drag from one oscillator
// to another to add that route (or remove it if it's already there); drag
// onto the same oscillator for feedback. Click an oscillator to switch its
// output on or off (off = a silent modulator).
class FmDiagram : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    explicit FmDiagram (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("Drag from one oscillator to another to add or remove an FM route; onto itself for feedback. "
                    "Click an oscillator to mute its output (it still modulates).");
        startTimerHz (15);
    }

    // [source][target] parameter ids.
    static const char* routeId (int source, int target)
    {
        static const char* ids[3][3] {
            { "fm_feedback", "fm_1to2", "fm_1to3" },
            { "fm_amount", "fm_fb2", "fm_2to3" },
            { "fm_3to1", "fm_3to2", "fm_fb3" }
        };

        return ids[source][target];
    }

    static juce::Colour oscColour (int osc)
    {
        const juce::Colour colours[] { IlanaTheme::accent(), juce::Colour (0xff5b8cff), juce::Colour (0xffffd447) };
        return colours[juce::jlimit (0, 2, osc)];
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 8.0f);
        const auto centres = operatorCentres();
        const auto radius = operatorRadius();

        // Routes between different oscillators.
        for (int source = 0; source < 3; ++source)
        {
            for (int target = 0; target < 3; ++target)
            {
                const auto amount = read (routeId (source, target));

                if (amount < 0.001f)
                    continue;

                const auto colour = oscColour (source).withAlpha (0.35f + 0.6f * amount);
                const auto thickness = 1.5f + amount * 6.0f;

                if (source == target)
                {
                    // Feedback: a loop above (or below) the operator.
                    const auto centre = centres[(size_t) source];
                    const auto up = source == 2 ? 1.0f : -1.0f;
                    const auto loop = juce::Rectangle<float> (radius * 1.1f, radius * 1.1f)
                                          .withCentre ({ centre.x, centre.y + up * radius * 1.25f });
                    g.setColour (colour);
                    g.drawEllipse (loop, thickness);
                    continue;
                }

                // Offset the two directions of a pair so both are visible.
                auto from = centres[(size_t) source];
                auto to = centres[(size_t) target];
                const auto direction = (to - from) / juce::jmax (1.0f, from.getDistanceFrom (to));
                const juce::Point<float> normal (-direction.y, direction.x);
                const auto offset = normal * 7.0f;
                from = from + direction * radius + offset;
                to = to - direction * (radius + 6.0f) + offset;

                juce::Path arrow;
                arrow.addArrow ({ from, to }, thickness, 10.0f + thickness, 12.0f);
                g.setColour (colour);
                g.fillPath (arrow);

                g.setColour (juce::Colours::white.withAlpha (0.75f));
                g.setFont (IlanaTheme::font (10.0f, true));
                g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + "%",
                            juce::Rectangle<float> (40.0f, 14.0f).withCentre ((from + to) * 0.5f + normal * 12.0f),
                            juce::Justification::centred);
            }
        }

        auto anyRoute = false;

        for (int source = 0; source < 3; ++source)
            for (int target = 0; target < 3; ++target)
                anyRoute = anyRoute || read (routeId (source, target)) > 0.001f;

        if (! anyRoute)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (12.5f));
            g.drawText ("Drag from one oscillator to another to add FM", getLocalBounds().removeFromBottom (30),
                        juce::Justification::centred);
        }

        // Drag in progress.
        if (dragSource >= 0)
        {
            g.setColour (oscColour (dragSource).withAlpha (0.6f));
            g.drawLine ({ centres[(size_t) dragSource], dragPosition }, 2.0f);
        }

        // Operators.
        for (int osc = 0; osc < 3; ++osc)
        {
            const auto centre = centres[(size_t) osc];
            const auto colour = oscColour (osc);
            const auto out = read (osc == 0 ? "osc1_out" : (osc == 1 ? "osc2_out" : "sub_out")) > 0.5f;
            const auto on = read (osc == 0 ? "osc1_on" : (osc == 1 ? "osc2_on" : "sub_on")) > 0.5f;
            const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

            g.setColour (juce::Colour (0xff17171b));
            g.fillEllipse (circle);
            g.setColour (colour.withAlpha (on ? 0.25f : 0.08f));
            g.fillEllipse (circle.reduced (3.0f));
            g.setColour (colour.withAlpha (on ? 1.0f : 0.35f));
            g.drawEllipse (circle.reduced (1.0f), hoverOsc == osc ? 2.6f : 1.8f);

            g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.4f));
            g.setFont (IlanaTheme::font (13.0f, true));
            g.drawText ("OSC " + juce::String (osc + 1), circle.withTrimmedBottom (radius * 0.4f), juce::Justification::centred);

            g.setColour (out ? colour : juce::Colours::white.withAlpha (0.35f));
            g.setFont (IlanaTheme::font (9.5f, true));
            g.drawText (out ? "OUT" : "MOD ONLY", circle.withTrimmedTop (radius * 0.9f), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto osc = oscAt (event.position);

        if (osc != hoverOsc)
        {
            hoverOsc = osc;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragSource = oscAt (event.position);
        dragPosition = event.position;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        dragPosition = event.position;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto source = dragSource;
        dragSource = -1;

        if (source < 0)
            return;

        const auto target = oscAt (event.position);

        if (target >= 0 && event.getDistanceFromDragStart() < 6)
        {
            // A click: toggle the operator's output.
            toggle (source == 0 ? "osc1_out" : (source == 1 ? "osc2_out" : "sub_out"));
        }
        else if (target >= 0)
        {
            // A drag: add the route, or remove it if it's already there.
            if (auto* parameter = processorRef.apvts.getParameter (routeId (source, target)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getValue() > 0.001f ? 0.0f : 0.5f);
                parameter->endChangeGesture();
            }
        }

        repaint();
    }

private:
    float operatorRadius() const { return juce::jlimit (26.0f, 44.0f, (float) juce::jmin (getWidth(), getHeight()) * 0.11f); }

    std::array<juce::Point<float>, 3> operatorCentres() const
    {
        const auto area = getLocalBounds().toFloat().reduced (operatorRadius() * 1.9f);
        return { juce::Point<float> { area.getX() + area.getWidth() * 0.12f, area.getY() + area.getHeight() * 0.1f },
                 juce::Point<float> { area.getRight() - area.getWidth() * 0.12f, area.getY() + area.getHeight() * 0.1f },
                 juce::Point<float> { area.getCentreX(), area.getBottom() - area.getHeight() * 0.05f } };
    }

    int oscAt (juce::Point<float> position) const
    {
        const auto centres = operatorCentres();

        for (int osc = 0; osc < 3; ++osc)
            if (centres[(size_t) osc].getDistanceFrom (position) <= operatorRadius() + 4.0f)
                return osc;

        return -1;
    }

    float read (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void toggle (const char* id)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getValue() > 0.5f ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    int dragSource = -1;
    int hoverOsc = -1;
    juce::Point<float> dragPosition;
};
