#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// The FM matrix as operators: the patch's oscillators, with an arrow for every
// route (thicker = deeper) and a loop for feedback. Drag from one oscillator
// to another to add that route (or remove it if it's already there); drag
// onto the same oscillator for feedback. Click an oscillator to switch its
// output on or off (off = a silent modulator).
class FmDiagram : public juce::Component,
                  public juce::SettableTooltipClient,
                  private IlanaAnim::FrameTimer
{
public:
    explicit FmDiagram (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip (defaultTooltip());
        startTimerHz (30);
    }

    static juce::String defaultTooltip()
    {
        return "Drag from one oscillator to another to add or remove an FM route; onto itself for feedback. "
               "Click an oscillator to mute its output (it still modulates).";
    }

    // [source][target] parameter ids.
    static juce::String routeId (int source, int target)
    {
        static const char* ids[3][3] {
            { "fm_feedback", "fm_1to2", "fm_1to3" },
            { "fm_amount", "fm_fb2", "fm_2to3" },
            { "fm_3to1", "fm_3to2", "fm_fb3" }
        };

        if (source < 3 && target < 3)
            return ids[source][target];
        return source == target ? "fm_fb" + juce::String (source + 1)
                                : "fm_" + juce::String (source + 1) + "to" + juce::String (target + 1);
    }

    // Whether FM into an oscillator does anything: a wavetable always takes
    // it; a string only when its exciter is "Osc In" (the incoming signal
    // drives the string rather than bending its phase); sample and granular
    // oscillators never read it. Every mode can still modulate others.
    static bool receivesFm (const IlanaSynthAudioProcessor& p, int osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)]);
        const auto choice = [&p] (const juce::String& id)
        {
            const auto* value = p.apvts.getRawParameterValue (id);
            return value != nullptr ? juce::roundToInt (value->load()) : 0;
        };

        const auto mode = choice (prefix + "_mode");

        if (mode == 2 || mode == 3)
            return false;

        return mode != 1 || choice (prefix + "_excite") == 6;
    }

    // Why an oscillator ignores FM, for tooltips and labels.
    static juce::String fmInputNote (const IlanaSynthAudioProcessor& p, int osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)]);
        const auto* value = p.apvts.getRawParameterValue (prefix + "_mode");
        const auto mode = value != nullptr ? juce::roundToInt (value->load()) : 0;

        if (mode == 1)
            return "A string only takes FM when its Excite is set to Osc In (the incoming signal then drives the string).";

        if (mode == 2)
            return "Sample oscillators don't take FM. They can still modulate other oscillators.";

        if (mode == 3)
            return "Granular oscillators don't take FM. They can still modulate other oscillators.";

        return {};
    }

    static juce::Colour oscColour (int osc) { return IlanaTheme::oscColour (osc); }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintWell (g, getLocalBounds().toFloat(), 8.0f);
        const auto centres = operatorCentres();
        const auto radius = operatorRadius();
        const auto shown = shownOscillators();

        // Routes between different oscillators.
        for (const auto source : shown)
        {
            for (const auto target : shown)
            {
                const auto amount = read (routeId (source, target));

                if (amount < 0.001f)
                    continue;

                // A route into an oscillator that ignores FM stays in the
                // patch but is drawn faint, without the flowing energy.
                const auto live = receivesFm (processorRef, target);
                const auto colour = oscColour (source).withAlpha ((0.35f + 0.6f * amount) * (live ? 1.0f : 0.3f));
                const auto thickness = 1.5f + amount * 6.0f;

                if (source == target)
                {
                    // Feedback: a loop on the outside of the operator.
                    const auto centre = centres[(size_t) source];
                    auto outward = centre - getLocalBounds().toFloat().getCentre();
                    const auto length = outward.getDistanceFromOrigin();
                    outward = length > 1.0f ? outward / length : juce::Point<float> (0.0f, -1.0f);
                    const auto loop = juce::Rectangle<float> (radius * 1.1f, radius * 1.1f)
                                          .withCentre (centre + outward * radius * 1.25f);
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

                // Energy flowing along the route: faster and brighter while notes play.
                if (live)
                {
                    const auto playing = processorRef.getActiveVoiceCount() > 0;
                    const auto now = liveSeconds;
                    const auto speed = (playing ? 0.9 : 0.3) * (0.5 + amount);

                    for (int dot = 0; dot < 3; ++dot)
                    {
                        const auto t = (float) std::fmod (now * speed + dot / 3.0, 1.0);
                        const auto position = from + (to - from) * (t * 0.88f);
                        const auto alpha = std::sin (t * juce::MathConstants<float>::pi) * (playing ? 0.95f : 0.5f);
                        g.setColour (juce::Colours::white.withAlpha (alpha));
                        g.fillEllipse (juce::Rectangle<float> (4.0f + thickness * 0.4f, 4.0f + thickness * 0.4f).withCentre (position));
                    }
                }

                g.setColour (juce::Colours::white.withAlpha (live ? 0.75f : 0.3f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + "%",
                            juce::Rectangle<float> (40.0f, 14.0f).withCentre ((from + to) * 0.5f + normal * 12.0f),
                            juce::Justification::centred);
            }
        }

        // The noise operator: a small node in the corner, drawn only while it
        // modulates something.
        {
            const auto node = noiseNode();
            auto anyNoise = false;

            for (const auto target : shown)
            {
                const auto amount = read ("fm_noise" + juce::String (target + 1));

                if (amount < 0.001f)
                    continue;

                anyNoise = true;
                const auto to = centres[(size_t) target];
                const auto direction = (to - node.getCentre()) / juce::jmax (1.0f, node.getCentre().getDistanceFrom (to));
                juce::Path arrow;
                arrow.addArrow ({ node.getCentre() + direction * node.getWidth() * 0.5f, to - direction * (radius + 6.0f) },
                                1.2f + amount * 4.0f, 8.0f + amount * 4.0f, 10.0f);
                g.setColour (IlanaTheme::Ui::text2.withAlpha (0.3f + 0.5f * amount));
                g.fillPath (arrow);
            }

            if (anyNoise)
            {
                g.setColour (IlanaTheme::Ui::panel);
                g.fillEllipse (node);
                g.setColour (IlanaTheme::Ui::text2);
                g.drawEllipse (node.reduced (1.0f), 1.6f);

                // A few speckles for noise.
                juce::Random speckle (7);
                for (int dot = 0; dot < 14; ++dot)
                    g.fillEllipse (juce::Rectangle<float> (1.6f, 1.6f).withCentre (
                        node.getCentre() + juce::Point<float> (speckle.nextFloat() - 0.5f, speckle.nextFloat() - 0.5f) * node.getWidth() * 0.55f));

                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                // Beside the node: under it, the small node's width cut the
                // name short ("NOI...": UI review 4, S14).
                g.drawText ("NOISE", juce::Rectangle<float> (node.getRight() + 5.0f, node.getCentreY() - 7.0f, 48.0f, 14.0f).toNearestInt(),
                            juce::Justification::centredLeft);
            }
        }

        auto anyRoute = false;

        for (const auto source : shown)
            for (const auto target : shown)
                anyRoute = anyRoute || read (routeId (source, target)) > 0.001f;

        if (! anyRoute)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText ("Drag from one oscillator to another to add FM, or pick an algorithm above",
                        getLocalBounds().removeFromBottom ((int) hintHeight + 4), juce::Justification::centred);
        }

        // Drag in progress.
        if (dragSource >= 0)
        {
            const auto blocked = hoverOsc >= 0 && ! receivesFm (processorRef, hoverOsc)
                                 && read (routeId (dragSource, hoverOsc)) < 0.001f;
            g.setColour (blocked ? juce::Colours::white.withAlpha (0.2f) : oscColour (dragSource).withAlpha (0.6f));
            g.drawLine ({ centres[(size_t) dragSource], dragPosition }, 2.0f);
        }

        // Operators.
        for (const auto osc : shown)
        {
            const auto centre = centres[(size_t) osc];
            const auto colour = oscColour (osc);
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
            const auto out = read (prefix + "_out") > 0.5f;
            const auto on = read (prefix + "_on") > 0.5f;
            const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

            // A soft halo that breathes while this operator sounds.
            if (on && processorRef.getActiveVoiceCount() > 0)
            {
                const auto breath = 0.5f + 0.5f * std::sin ((float) liveSeconds * 4.0f + (float) osc);
                g.setColour (colour.withAlpha (0.10f + 0.12f * breath));
                g.fillEllipse (circle.expanded (6.0f + 4.0f * breath));
            }

            g.setColour (IlanaTheme::Ui::panel);
            g.fillEllipse (circle);
            g.setColour (colour.withAlpha (on ? 0.25f : 0.08f));
            g.fillEllipse (circle.reduced (3.0f));
            g.setColour (colour.withAlpha (on ? 1.0f : 0.35f));
            g.drawEllipse (circle.reduced (1.0f), hoverOsc == osc ? 2.6f : 1.8f);

            g.setColour (on ? juce::Colours::white : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (osc + 1), circle.withTrimmedBottom (radius * 0.4f), juce::Justification::centred);

            g.setColour (out ? colour : IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (out ? "OUT" : "MOD", circle.withTrimmedTop (radius * 0.9f), juce::Justification::centred);

            // Its tuning and level under it, so the graph reads without
            // opening each operator (UI review 4, S14).
            g.setColour (on ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
            g.drawText (getNodeCaption (osc), juce::Rectangle<float> (radius * 4.0f, 12.0f).withCentre ({ centre.x, circle.getBottom() + 9.0f }),
                        juce::Justification::centred);

            // Oscillators that ignore FM: a dashed ring and a tag under them.
            if (! receivesFm (processorRef, osc))
            {
                juce::Path ring;
                ring.addEllipse (circle.expanded (5.0f));
                juce::Path dashed;
                const float dashes[] { 3.0f, 4.0f };
                juce::PathStrokeType (1.2f).createDashedStroke (dashed, ring, dashes, 2);
                g.setColour (juce::Colours::white.withAlpha (0.28f));
                g.fillPath (dashed);

                g.setColour (IlanaTheme::Ui::text2);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText ("NO FM IN", juce::Rectangle<float> (radius * 3.0f, 12.0f)
                                           .withCentre ({ centre.x, circle.getBottom() + 21.0f }),
                            juce::Justification::centred);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto osc = oscAt (event.position);

        if (osc != hoverOsc)
        {
            hoverOsc = osc;
            const auto note = osc >= 0 ? fmInputNote (processorRef, osc) : juce::String();
            setTooltip (note.isNotEmpty() && ! receivesFm (processorRef, osc) ? note : defaultTooltip());
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
        hoverOsc = oscAt (event.position);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto source = dragSource;
        dragSource = -1;

        if (source < 0)
            return;

        const auto target = oscAt (event.position);

        if (target >= 0)
            processorRef.getUndoManager().beginNewTransaction ("FM routing");

        if (target >= 0 && event.getDistanceFromDragStart() < 6)
        {
            // A click: toggle the operator's output.
            toggle (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_out");
        }
        else if (target >= 0)
        {
            // A drag: add the route, or remove it if it's already there.
            // A new route into an oscillator that ignores FM isn't added.
            auto* parameter = processorRef.apvts.getParameter (routeId (source, target));

            if (parameter != nullptr && (parameter->getValue() > 0.001f || receivesFm (processorRef, target)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getValue() > 0.001f ? 0.0f : 0.5f);
                parameter->endChangeGesture();
            }
        }

        repaint();
    }

private:
    // Only the oscillators the patch has added take part.
    std::vector<int> shownOscillators() const
    {
        std::vector<int> shown;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                shown.push_back (osc);
        return shown;
    }

    static constexpr float hintHeight = 24.0f;

public:
    // "x1.00  50%": an operator's tuning (ratio, fixed Hz or semitones) and
    // its level.
    juce::String getNodeCaption (int osc) const
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)]);
        const auto tune = juce::roundToInt (read (prefix + "_tune"));
        juce::String tuning;

        if (tune == OscTuning::Ratio)
            tuning = "x" + juce::String (processorRef.getSnappedRatio (osc), 2);
        else if (tune == OscTuning::Fixed)
            tuning = juce::String (juce::roundToInt (read (prefix + "_fixed_hz"))) + " Hz";
        else
        {
            const auto semi = juce::roundToInt (read (prefix + "_semi"));
            tuning = (semi > 0 ? "+" : "") + juce::String (semi) + " st";
        }

        return tuning + "  " + juce::String (juce::roundToInt (read (prefix + "_level") * 100.0f)) + "%";
    }

private:

    float operatorRadius() const
    {
        const auto size = (float) juce::jmin (getWidth(), getHeight() - (int) hintHeight);
        return shownOscillators().size() <= 3 ? juce::jlimit (26.0f, 44.0f, size * 0.11f)
                                              : juce::jlimit (20.0f, 36.0f, size * 0.085f);
    }

    std::array<juce::Point<float>, OscillatorIds::count> operatorCentres() const
    {
        const auto shown = shownOscillators();
        const auto count = (int) shown.size();
        // The bottom strip holds the hint text and the noise node.
        const auto bounds = getLocalBounds().toFloat().withTrimmedBottom (hintHeight).reduced (operatorRadius() * 1.6f);
        std::array<juce::Point<float>, OscillatorIds::count> centres {};

        if (count <= 3)
        {
            // An upside-down triangle: OSC 1 and 2 on top, OSC 3 below.
            // It spans 1.5 x its vertical spread top to bottom; in a wide,
            // short area it widens (up to 2.2 x) rather than shrinking.
            const auto spreadY = juce::jmin (bounds.getHeight() / 1.5f, bounds.getWidth() * 0.5f);
            const auto spreadX = juce::jmin (bounds.getWidth() * 0.46f, spreadY * 2.2f);
            const auto centre = juce::Point<float> (bounds.getCentreX(),
                                                    bounds.getY() + 0.5f * spreadY + (bounds.getHeight() - 1.5f * spreadY) * 0.5f);
            const float angles[] { -150.0f, -30.0f, 90.0f };

            for (int i = 0; i < count; ++i)
            {
                const auto radians = juce::degreesToRadians (count == 1 ? 90.0f : angles[i]);
                centres[(size_t) shown[(size_t) i]] = count == 1 ? bounds.getCentre()
                    : centre + juce::Point<float> (std::cos (radians) * spreadX, std::sin (radians) * spreadY);
            }

            return centres;
        }

        // More operators sit around a ring, which leaves room for arrows both
        // ways. It starts half a step before the top, so four make a square
        // and six a flat hexagon that uses a wide, short area well.
        for (int i = 0; i < count; ++i)
        {
            const auto angle = juce::MathConstants<float>::twoPi * ((float) i - 0.5f) / (float) count
                               - juce::MathConstants<float>::halfPi;
            centres[(size_t) shown[(size_t) i]] = bounds.getCentre()
                                                  + juce::Point<float> (std::cos (angle) * bounds.getWidth() * 0.45f,
                                                                        std::sin (angle) * bounds.getHeight() * 0.5f);
        }


        return centres;
    }

    juce::Rectangle<float> noiseNode() const
    {
        const auto size = operatorRadius() * 1.1f;
        return juce::Rectangle<float> (size, size).withPosition (14.0f, (float) getHeight() - size - 22.0f);
    }

    int oscAt (juce::Point<float> position) const
    {
        const auto centres = operatorCentres();

        for (const auto osc : shownOscillators())
            if (centres[(size_t) osc].getDistanceFrom (position) <= operatorRadius() + 4.0f)
                return osc;

        return -1;
    }

    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    void toggle (const juce::String& id)
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
        // The flow along the routes moves only while notes sound.
        if (processorRef.getActiveVoiceCount() > 0)
            liveSeconds += (double) frameSeconds();

        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    double liveSeconds = 0.0;

    IlanaSynthAudioProcessor& processorRef;
    int dragSource = -1;
    int hoverOsc = -1;
    juce::Point<float> dragPosition;
};
