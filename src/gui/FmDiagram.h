#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"
#include "FmOperatorInfo.h"

// The FM matrix as operators: the patch's oscillators, with an arrow for every
// route (thicker = deeper) and a loop for feedback. Drag from one oscillator
// to another to add that route (or remove it if it's already there); drag
// onto the same oscillator for feedback. Click an oscillator to switch its
// output on or off (off = a silent modulator). Operators stand in DX stacks
// (UI review 6): carriers along the bottom, each modulator one row above the
// deepest operator it drives, as a DX7's algorithm chart. Review 7: every
// patch is drawn this way (one node style: "OSC n" and its role inside, its
// tuning and level to the right); a stack too tall for the height climbs
// up and to the left as a staircase, so nodes stay a readable size.
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

        const auto amountLabels = amountLabelLayout();

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
                    // Feedback: a loop with its arrowhead at the node's upper
                    // left, clear of it, as the matrix's glyph (I7-11).
                    const auto box = feedbackLoop (source, centres, radius);
                    const auto loopCentre = box.getCentre();
                    const auto loopRadius = box.getWidth() * 0.5f;
                    juce::Path loop;
                    loop.addCentredArc (loopCentre.x, loopCentre.y, loopRadius, loopRadius, 0.0f, 0.6f,
                                        juce::MathConstants<float>::twoPi - 0.3f, true);
                    g.setColour (colour);
                    g.strokePath (loop, juce::PathStrokeType (juce::jmin (thickness, 3.0f), juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
                    const auto tip = loopCentre + juce::Point<float> (std::sin (-0.3f), -std::cos (-0.3f)) * loopRadius;
                    juce::Path head;
                    head.addTriangle (tip.x - 4.0f, tip.y - 3.4f, tip.x + 1.5f, tip.y, tip.x - 4.0f, tip.y + 3.4f);
                    g.fillPath (head);
                    continue;
                }

                // Offset the two directions of a pair so both are visible.
                auto from = centres[(size_t) source];
                auto to = centres[(size_t) target];
                const auto direction = (to - from) / juce::jmax (1.0f, from.getDistanceFrom (to));
                const juce::Point<float> normal (-direction.y, direction.x);
                const auto both = read (routeId (target, source)) > 0.001f;
                const auto offset = normal * (both ? 7.0f : 0.0f);
                // From above, a route lands on the node's top, clear of
                // the caption on its right (V7-3).
                const auto landing = landingPoint (source, target, centres, radius);
                from = from + direction * radius + offset;
                to = landing.has_value() ? *landing - ((*landing - from) / juce::jmax (1.0f, from.getDistanceFrom (*landing))) * 6.0f
                                         : to - direction * (radius + 6.0f) + offset;

                // A route that would cross another operator bends around it.
                const auto bend = bendFor (source, target, centres, radius);
                juce::Path arrow;
                if (bend == 0.0f)
                    arrow.addArrow ({ from, to }, thickness, 10.0f + thickness, 12.0f);
                else
                    addBentArrow (arrow, centres[(size_t) source], landing.value_or (centres[(size_t) target]), bend, radius,
                                  landing.has_value() ? 6.0f : radius + 6.0f, thickness);
                g.setColour (colour);
                g.fillPath (arrow);
                if (bend != 0.0f)
                {
                    // Its flow rides the bend's middle.
                    const auto bentEnd = landing.value_or (centres[(size_t) target]);
                    const auto bentDirection = (bentEnd - centres[(size_t) source]) / juce::jmax (1.0f, centres[(size_t) source].getDistanceFrom (bentEnd));
                    const auto middle = (centres[(size_t) source] + bentEnd) * 0.5f
                                        + juce::Point<float> (-bentDirection.y, bentDirection.x) * bend * 0.5f;
                    from = middle - direction * 4.0f;
                    to = middle + direction * 4.0f;
                }

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
                // Beside the arrow where it crosses no node or caption.
                for (const auto& label : amountLabels)
                    if (label.first == std::make_pair (source, target))
                        g.drawText (juce::String (juce::roundToInt (amount * 100.0f)) + "%", label.second, juce::Justification::centred);
            }
        }

        // The OSC card's WARP FM and Ring: OSC 2 into OSC 1, OSC 1 into the
        // others. Dashed, labelled, so all the FM in the patch shows here
        // (UI review 6, I6-13).
        for (const auto target : shown)
        {
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) target]);
            const auto warp = juce::roundToInt (read (prefix + "_warp"));
            const auto source = target == 0 ? 1 : 0;
            if ((warp != Warp::Fm && warp != Warp::Ring) || read (prefix + "_warp_amt") < 0.001f
                || std::find (shown.begin(), shown.end(), source) == shown.end())
                continue;
            const auto from = centres[(size_t) source], to = centres[(size_t) target];
            const auto direction = (to - from) / juce::jmax (1.0f, from.getDistanceFrom (to));
            auto normal = juce::Point<float> (-direction.y, direction.x);
            // A bow on the left of the pair (the captions are on the right),
            // clear of any matrix route between them.
            if (normal.x > 0.0f || (normal.x == 0.0f && normal.y > 0.0f))
                normal = -normal;
            const auto control = (from + to) * 0.5f + normal * radius * 2.2f;
            const auto startDirection = (control - from) / juce::jmax (1.0f, from.getDistanceFrom (control));
            const auto endDirection = (to - control) / juce::jmax (1.0f, to.getDistanceFrom (control));
            const auto start = from + startDirection * radius, end = to - endDirection * (radius + 4.0f);
            juce::Path shaft, dashed;
            shaft.startNewSubPath (start);
            shaft.quadraticTo (control, end - endDirection * 6.0f);
            const float dashes[] { 5.0f, 4.0f };
            juce::PathStrokeType (1.6f).createDashedStroke (dashed, shaft, dashes, 2);
            juce::Path head;
            head.addArrow ({ end - endDirection * 7.0f, end }, 0.0f, 9.0f, 8.0f);
            g.setColour (oscColour (source).withAlpha (0.75f));
            g.fillPath (dashed);
            g.fillPath (head);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            const auto apex = (from + to) * 0.25f + control * 0.5f;
            const auto labelBox = juce::Rectangle<float> (60.0f, 12.0f);
            g.drawText (warp == Warp::Fm ? "WARP FM" : "WARP RING",
                        normal.x < -0.5f ? labelBox.withRightX (apex.x - 9.0f).withY (apex.y - 6.0f)
                                         : labelBox.withCentre (apex + normal * 10.0f),
                        normal.x < -0.5f ? juce::Justification::centredRight : juce::Justification::centred);
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
            g.setColour (colour.withAlpha (on ? 0.25f : 0.06f));
            g.fillEllipse (circle.reduced (3.0f));
            if (on)
            {
                g.setColour (colour);
                g.drawEllipse (circle.reduced (1.0f), hoverOsc == osc ? 2.6f : 1.8f);
            }
            else
            {
                // Switched off: dashed and dim (S7-14).
                juce::Path ring, dashed;
                ring.addEllipse (circle.reduced (1.0f));
                const float dashes[] { 4.0f, 3.0f };
                juce::PathStrokeType (hoverOsc == osc ? 2.2f : 1.5f).createDashedStroke (dashed, ring, dashes, 2);
                g.setColour (colour.withAlpha (0.4f));
                g.fillPath (dashed);
            }

            // One style for every node (V7-13, I7-11): its name, and under
            // it whether it is heard (OUT), modulates (MOD) or is off.
            const auto nameFont = IlanaTheme::font (radius < 26.0f ? IlanaTheme::TextSize::label : IlanaTheme::TextSize::body, true);
            g.setColour (on ? juce::Colours::white : IlanaTheme::Ui::text3);
            g.setFont (nameFont);
            g.drawText ("OSC " + juce::String (osc + 1), circle.withTrimmedBottom (radius * 0.97f).withTrimmedTop (radius * 0.2f),
                        juce::Justification::centredBottom, false);

            g.setColour (! on ? IlanaTheme::Ui::text3 : out ? colour : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText (! on ? "OFF" : out ? "OUT" : "MOD", circle.withTrimmedTop (radius * 1.1f), juce::Justification::centredTop, false);

            // Its tuning and level to its right, so the graph reads without
            // opening each operator (UI review 4, S14).
            g.setColour (on ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
            const auto caption = getCaptionBounds (osc, centres, radius);
            g.drawText (FmOperatorInfo::tuningText (processorRef, osc), caption.withTrimmedBottom (caption.getHeight() * 0.5f).toNearestInt(),
                        juce::Justification::bottomLeft);
            g.drawText (FmOperatorInfo::levelText (processorRef, osc), caption.withTrimmedTop (caption.getHeight() * 0.5f).toNearestInt(),
                        juce::Justification::topLeft);

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
                g.drawText ("NO FM IN", caption.withY (caption.getBottom()).withHeight (12.0f).toNearestInt(),
                            juce::Justification::topLeft);
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
    // "×1.00  50%": an operator's tuning (ratio, fixed Hz or semitones) and
    // its level (on the Operator Env, its output in dB), as PLAY and OSC
    // read it too (FmOperatorInfo).
    juce::String getNodeCaption (int osc) const
    {
        return FmOperatorInfo::tuningText (processorRef, osc) + "  " + FmOperatorInfo::levelText (processorRef, osc);
    }

    // Every patch stands in DX stacks (review 7: one layout, one node style).
    bool isStacked() const { return true; }

    // How many rows the stacks take.
    int getStackRows() const
    {
        const auto levels = stackLevels();
        auto rows = 1;
        for (const auto osc : shownOscillators())
            rows = juce::jmax (rows, levels[(size_t) osc] + 1);
        return rows;
    }

    // Whether the stacks climb to the right (too tall to stand straight at a
    // readable size).
    bool isStaircase() const { return geometry().staircase; }

    // The height under which nodes, captions and arrows would crowd.
    int getMinimumHeight() const
    {
        const auto rows = getStackRows();
        return juce::roundToInt ((float) minimumRadius * (float) (rows + 1) + stairGap * (float) (rows - 1)) + 16;
    }

    // Where each shown operator is drawn, and how big (for the UI test).
    std::vector<juce::Rectangle<float>> getNodeBounds() const
    {
        const auto centres = operatorCentres();
        const auto radius = operatorRadius();
        std::vector<juce::Rectangle<float>> nodes;
        for (const auto osc : shownOscillators())
            nodes.push_back (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centres[(size_t) osc]));
        return nodes;
    }

    std::vector<juce::Rectangle<float>> getCaptionBoundsList() const
    {
        const auto centres = operatorCentres();
        const auto radius = operatorRadius();
        std::vector<juce::Rectangle<float>> captions;
        for (const auto osc : shownOscillators())
            captions.push_back (getCaptionBounds (osc, centres, radius));
        return captions;
    }

    // Each route's amount label (for the UI test: none sits on a node or a
    // caption).
    std::vector<juce::Rectangle<float>> getAmountLabelBoundsList() const
    {
        std::vector<juce::Rectangle<float>> labels;
        for (const auto& label : amountLabelLayout())
            labels.push_back (label.second);
        return labels;
    }

    float getOperatorRadius() const { return operatorRadius(); }

private:
    // Node sizes: never smaller than minimumRadius (its name fits inside),
    // never larger than maximumRadius (deep and shallow algorithms look
    // alike). Straight stacks keep stackGap between rows; a staircase rises
    // a radius and stairGap a row (each caption clears the node under it)
    // and steps stairStep radii to the left, room for the arrow between.
    static constexpr int minimumRadius = 21;
    static constexpr float maximumRadius = 30.0f, stackGap = 16.0f, stairGap = 20.0f, stairStep = 2.3f;
    static constexpr float captionWidth = 58.0f;

    struct Geometry
    {
        float radius = 26.0f;
        bool staircase = false;
    };

    Geometry geometry() const
    {
        const auto area = layoutArea();
        const auto rows = (float) getStackRows();
        const auto slot = area.getWidth() / (float) widestRow();
        const auto byWidth = (slot - captionWidth - 10.0f) * 0.5f;

        // A staircase only when the height is what is short: it saves
        // height, not width.
        const auto byHeight = (area.getHeight() / rows - stackGap) * 0.5f;
        Geometry geo;
        geo.radius = juce::jmin (maximumRadius, byWidth, byHeight);
        if (byHeight < 24.0f && byHeight < byWidth && rows > 1.0f)
        {
            geo.staircase = true;
            geo.radius = juce::jmin (maximumRadius, byWidth, (area.getHeight() - 8.0f - stairGap * (rows - 1.0f)) / (rows + 1.0f));
        }
        geo.radius = juce::jmax ((float) juce::jmin (minimumRadius, 16), geo.radius);
        return geo;
    }

    float operatorRadius() const { return geometry().radius; }

    // The stacks' rows: 0 for the operators heard (and modulators that
    // reach nothing shown), then each modulator one above the deepest it
    // drives. Loops stop at the number of operators.
    std::array<int, OscillatorIds::count> stackLevels() const
    {
        const auto shown = shownOscillators();
        std::array<int, OscillatorIds::count> level;
        level.fill (-1);
        const auto isShown = [&shown] (int osc) { return std::find (shown.begin(), shown.end(), osc) != shown.end(); };
        const auto routed = [&] (int source, int target)
        {
            return source != target && isShown (source) && isShown (target) && read (routeId (source, target)) > 0.001f;
        };

        for (const auto osc : shown)
        {
            auto drivesAny = false;
            for (const auto target : shown)
                drivesAny = drivesAny || routed (osc, target);
            if (read (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_out") > 0.5f || ! drivesAny)
                level[(size_t) osc] = 0;
        }

        for (size_t pass = 0; pass < shown.size(); ++pass)
            for (const auto source : shown)
                for (const auto target : shown)
                    if (routed (source, target) && level[(size_t) target] >= 0 && level[(size_t) source] != 0)
                        level[(size_t) source] = juce::jmin ((int) shown.size() - 1,
                                                             juce::jmax (level[(size_t) source], level[(size_t) target] + 1));

        for (const auto osc : shown)
            level[(size_t) osc] = juce::jmax (0, level[(size_t) osc]);
        return level;
    }

    int widestRow() const
    {
        const auto levels = stackLevels();
        std::array<int, OscillatorIds::count> perRow {};
        auto widest = 1;
        for (const auto osc : shownOscillators())
            widest = juce::jmax (widest, ++perRow[(size_t) levels[(size_t) osc]]);
        return widest;
    }

    // The area operators are placed in: the hint's strip at the bottom goes
    // while there are no routes, and the noise node's corner while it plays.
    juce::Rectangle<float> layoutArea() const
    {
        auto area = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
        if (! anyRouteShown())
            area.removeFromBottom (hintHeight);
        if (anyNoiseShown())
            area.removeFromLeft (60.0f);
        return area;
    }

    bool anyRouteShown() const
    {
        const auto shown = shownOscillators();
        for (const auto source : shown)
            for (const auto target : shown)
                if (read (routeId (source, target)) > 0.001f)
                    return true;
        return false;
    }

    bool anyNoiseShown() const
    {
        for (const auto target : shownOscillators())
            if (read ("fm_noise" + juce::String (target + 1)) > 0.001f)
                return true;
        return false;
    }

    std::array<juce::Point<float>, OscillatorIds::count> operatorCentres() const
    {
        const auto shown = shownOscillators();
        std::array<juce::Point<float>, OscillatorIds::count> centres {};
        if (shown.empty())
            return centres;

        // DX stacks. Each row's slots are as wide as the widest row allows;
        // a node sits left of its slot's centre, its caption on the right.
        const auto geo = geometry();
        const auto radius = geo.radius;
        const auto levels = stackLevels();
        const auto rows = getStackRows();
        const auto area = layoutArea();
        const auto slot = area.getWidth() / (float) widestRow();
        const auto captionShift = juce::jmin ((captionWidth + 6.0f) * 0.5f, slot * 0.25f);
        const auto step = geo.staircase ? -radius * stairStep : 0.0f;
        const auto rise = geo.staircase ? radius + stairGap : area.getHeight() / (float) rows;
        const auto bottomY = geo.staircase ? area.getCentreY() + ((float) (rows - 1) * rise) * 0.5f
                                           : area.getBottom() - rise * 0.5f;

        for (int row = 0; row < rows; ++row)
        {
            std::vector<std::pair<float, int>> placed; // desired slot centre, operator
            for (const auto osc : shown)
            {
                if (levels[(size_t) osc] != row)
                    continue;
                // The bottom row in order; above it, over what each drives
                // (a step to the left of it on a staircase).
                auto desired = 0.0f;
                auto targets = 0;
                if (row > 0)
                    for (const auto target : shown)
                        if (target != osc && levels[(size_t) target] < row && read (routeId (osc, target)) > 0.001f)
                        {
                            desired += centres[(size_t) target].x + captionShift + step;
                            ++targets;
                        }
                placed.push_back ({ targets > 0 ? desired / (float) targets : -1.0f, osc });
            }
            if (placed.empty())
                continue;

            const auto rowWidth = slot * (float) placed.size();
            auto x = area.getCentreX() - rowWidth * 0.5f + slot * 0.5f;
            for (auto& item : placed)
            {
                if (item.first < 0.0f)
                    item.first = x;
                x += slot;
            }
            std::stable_sort (placed.begin(), placed.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

            // Keep a slot apart, as close to the wishes as fits: operators
            // that want one place share it, spread evenly around it.
            struct Cluster { float left; int count; };
            std::vector<Cluster> clusters;
            for (const auto& item : placed)
            {
                clusters.push_back ({ item.first, 1 });
                while (clusters.size() > 1)
                {
                    const auto last = clusters.back();
                    auto& previous = clusters[clusters.size() - 2];
                    if (previous.left + slot * (float) previous.count <= last.left)
                        break;
                    const auto count = previous.count + last.count;
                    previous.left = (previous.left * (float) previous.count
                                     + (last.left - slot * (float) previous.count) * (float) last.count) / (float) count;
                    previous.count = count;
                    clusters.pop_back();
                }
            }
            std::vector<float> xs;
            for (const auto& cluster : clusters)
                for (int i = 0; i < cluster.count; ++i)
                    xs.push_back (cluster.left + slot * (float) i);

            const auto y = bottomY - rise * (float) row;
            for (size_t i = 0; i < placed.size(); ++i)
                centres[(size_t) placed[i].second] = { xs[i] - captionShift, y };
        }

        // The whole picture (nodes, captions and feedback loops) centred
        // across the area, and pulled inside it.
        auto left = std::numeric_limits<float>::max(), right = std::numeric_limits<float>::lowest();
        for (const auto osc : shown)
        {
            left = juce::jmin (left, centres[(size_t) osc].x - radius * (read (routeId (osc, osc)) > 0.001f ? 2.1f : 1.1f));
            right = juce::jmax (right, centres[(size_t) osc].x + radius + 6.0f + captionWidth);
        }
        auto shift = area.getCentreX() - (left + right) * 0.5f;
        if (right - left <= area.getWidth())
            shift = juce::jlimit (area.getX() - left, area.getRight() - right, shift);
        else
            shift = area.getX() - left;
        for (const auto osc : shown)
            centres[(size_t) osc].x += shift;

        return centres;
    }

    // Where a route from a row above, more or less straight over, ends: the
    // top of the node it drives (a staircase's diagonal steps aim at the
    // centre, which keeps them under the source's caption).
    std::optional<juce::Point<float>> landingPoint (int source, int target, const std::array<juce::Point<float>, OscillatorIds::count>& centres,
                                                    float radius) const
    {
        if (centres[(size_t) source].y < centres[(size_t) target].y - radius * 0.5f
            && std::abs (centres[(size_t) source].x - centres[(size_t) target].x) < radius * 1.5f)
            return centres[(size_t) target] - juce::Point<float> (0.0f, radius);
        return std::nullopt;
    }

    // A route's sideways bend (0: straight) when the straight line would run
    // through another operator, or along its own row (it then arcs over the
    // top). Around an operator it bows over it where it can, by enough to
    // clear it.
    float bendFor (int source, int target, const std::array<juce::Point<float>, OscillatorIds::count>& centres, float radius) const
    {
        const auto start = centres[(size_t) source];
        const auto end = landingPoint (source, target, centres, radius).value_or (centres[(size_t) target]);
        const juce::Line<float> line (start, end);
        const auto direction = (line.getEnd() - line.getStart()) / juce::jmax (1.0f, line.getLength());
        const juce::Point<float> normal (-direction.y, direction.x);
        const auto up = normal.y > 0.0f ? -1.0f : 1.0f;
        const auto sameRow = std::abs (start.y - centres[(size_t) target].y) < 1.0f;

        // The first bend, straight first, then over the top, then under,
        // each further out, whose curve misses every other node, every
        // caption (its own included, V7-3) and the diagram's edge.
        const auto shown = shownOscillators();
        const auto clear = [&] (float bend)
        {
            const auto control = (start + end) * 0.5f + normal * bend;
            for (int step = 1; step < 16; ++step)
            {
                const auto t = (float) step / 16.0f;
                const auto point = start * ((1.0f - t) * (1.0f - t)) + control * (2.0f * t * (1.0f - t)) + end * (t * t);
                if (! getLocalBounds().toFloat().reduced (3.0f).contains (point))
                    return false;
                for (const auto osc : shown)
                {
                    if (osc != source && osc != target && point.getDistanceFrom (centres[(size_t) osc]) < radius + 5.0f)
                        return false;
                    if (getCaptionBounds (osc, centres, radius).expanded (3.0f).contains (point))
                        return false;
                }
            }
            return true;
        };
        if (! sameRow && clear (0.0f))
            return 0.0f;
        for (const auto amount : { 1.0f, 1.7f, 2.4f, 3.2f, 4.0f })
            for (const auto side : { up, -up })
                if (clear (side * radius * amount))
                    return side * radius * amount;

        if (sameRow)
            return up * radius * 1.7f;
        for (const auto osc : shown)
        {
            if (osc == source || osc == target)
                continue;
            juce::Point<float> nearest;
            if (line.getDistanceFromPoint (centres[(size_t) osc], nearest) < radius + 5.0f)
            {
                // The curve's middle sits half the bend off the line.
                const auto offset = (centres[(size_t) osc] - nearest).getDotProduct (normal);
                return 2.0f * (offset + up * (radius + 10.0f));
            }
        }
        return 0.0f;
    }

    // A route's path as a few straight pieces (for keeping labels off it).
    std::vector<juce::Point<float>> routePoints (int source, int target, const std::array<juce::Point<float>, OscillatorIds::count>& centres,
                                                 float radius) const
    {
        const auto from = centres[(size_t) source], to = centres[(size_t) target];
        const auto end = landingPoint (source, target, centres, radius).value_or (to);
        const auto direction = (end - from) / juce::jmax (1.0f, from.getDistanceFrom (end));
        const juce::Point<float> normal (-direction.y, direction.x);
        const auto control = (from + end) * 0.5f + normal * bendFor (source, target, centres, radius);
        std::vector<juce::Point<float>> points;
        for (int step = 0; step <= 8; ++step)
        {
            const auto t = (float) step / 8.0f;
            points.push_back (from * ((1.0f - t) * (1.0f - t)) + control * (2.0f * t * (1.0f - t)) + end * (t * t));
        }
        return points;
    }

    // Where each route's amount is written: beside its arrow's middle, on the
    // first side that covers no node, caption, loop, route or earlier label.
    std::vector<std::pair<std::pair<int, int>, juce::Rectangle<float>>> amountLabelLayout() const
    {
        const auto centres = operatorCentres();
        const auto radius = operatorRadius();
        const auto shown = shownOscillators();
        std::vector<std::pair<int, int>> routes;
        std::vector<std::vector<juce::Point<float>>> paths;
        for (const auto source : shown)
            for (const auto target : shown)
                if (source != target && read (routeId (source, target)) >= 0.001f)
                {
                    routes.push_back ({ source, target });
                    paths.push_back (routePoints (source, target, centres, radius));
                }

        std::vector<std::pair<std::pair<int, int>, juce::Rectangle<float>>> placed;
        for (const auto& route : routes)
        {
            const auto source = route.first, target = route.second;
            const auto from = centres[(size_t) source], to = centres[(size_t) target];
            const auto direction = (to - from) / juce::jmax (1.0f, from.getDistanceFrom (to));
            const juce::Point<float> normal (-direction.y, direction.x);
            const auto bend = bendFor (source, target, centres, radius);
            const auto both = read (routeId (target, source)) > 0.001f;
            const auto end = landingPoint (source, target, centres, radius).value_or (to - direction * (radius + 6.0f));
            auto middle = (from + direction * radius + end) * 0.5f + normal * (both ? 7.0f : 0.0f);
            if (bend != 0.0f)
            {
                const auto bentEnd = landingPoint (source, target, centres, radius).value_or (to);
                const auto bentDirection = (bentEnd - from) / juce::jmax (1.0f, from.getDistanceFrom (bentEnd));
                middle = (from + bentEnd) * 0.5f + juce::Point<float> (-bentDirection.y, bentDirection.x) * bend * 0.5f;
            }

            const auto size = juce::Rectangle<float> (32.0f, 12.0f);
            std::vector<juce::Rectangle<float>> candidates;
            for (const auto away : { 0.55f, 0.8f, 1.1f, 1.5f, 1.9f, 2.4f })
            {
                candidates.push_back (size.withCentre (middle + juce::Point<float> (radius * away, -radius * away * 0.6f)));
                candidates.push_back (size.withCentre (middle + juce::Point<float> (-radius * away, radius * away * 0.6f)));
                candidates.push_back (size.withCentre (middle - normal * radius * away));
                candidates.push_back (size.withCentre (middle + normal * radius * away));
                candidates.push_back (size.withCentre (middle + juce::Point<float> (radius * away + 12.0f, 0.0f)));
                candidates.push_back (size.withCentre (middle - juce::Point<float> (radius * away + 12.0f, 0.0f)));
                candidates.push_back (size.withCentre (middle - juce::Point<float> (0.0f, radius * away)));
                candidates.push_back (size.withCentre (middle + juce::Point<float> (0.0f, radius * away)));
            }

            // How badly a place covers things: nodes, captions, loops and
            // earlier labels count by the area covered, routes a little, so
            // a crowded diagram still gets the least bad place.
            const auto cost = [&] (juce::Rectangle<float> box)
            {
                auto total = getLocalBounds().toFloat().reduced (2.0f).contains (box) ? 0.0f : 1.0e6f;
                const auto covered = [&box] (juce::Rectangle<float> other) { return box.getIntersection (other).getWidth() * box.getIntersection (other).getHeight(); };
                for (const auto osc : shown)
                {
                    const auto node = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centres[(size_t) osc]);
                    total += 100.0f * (covered (node.reduced (radius * 0.12f)) + covered (getCaptionBounds (osc, centres, radius)));
                    if (read (routeId (osc, osc)) > 0.001f)
                        total += 100.0f * covered (feedbackLoop (osc, centres, radius));
                }
                for (const auto& label : placed)
                    total += 100.0f * covered (label.second.expanded (2.0f));
                for (const auto& path : paths)
                    for (size_t i = 1; i < path.size(); ++i)
                        if (box.expanded (2.0f).intersects (juce::Line<float> (path[i - 1], path[i])))
                            total += 1.0f;
                return total;
            };

            auto chosen = candidates.front();
            auto best = std::numeric_limits<float>::max();
            for (const auto& box : candidates)
                if (const auto score = cost (box); score < best)
                {
                    best = score;
                    chosen = box;
                }
            placed.push_back ({ route, chosen });
        }
        return placed;
    }

    static void addBentArrow (juce::Path& arrow, juce::Point<float> from, juce::Point<float> to, float bend, float radius,
                              float endGap, float thickness)
    {
        const auto direction = (to - from) / juce::jmax (1.0f, from.getDistanceFrom (to));
        const juce::Point<float> normal (-direction.y, direction.x);
        const auto control = (from + to) * 0.5f + normal * bend;
        const auto startDirection = (control - from) / juce::jmax (1.0f, from.getDistanceFrom (control));
        const auto endDirection = (to - control) / juce::jmax (1.0f, to.getDistanceFrom (control));
        const auto start = from + startDirection * radius;
        const auto end = to - endDirection * endGap;
        const auto head = 10.0f + thickness;
        const auto shaftEnd = end - endDirection * head * 0.8f;

        juce::Path shaft;
        shaft.startNewSubPath (start);
        shaft.quadraticTo (control, shaftEnd);
        juce::PathStrokeType (thickness).createStrokedPath (arrow, shaft);
        arrow.addArrow ({ shaftEnd - endDirection * 0.5f, end }, 0.0f, head, head);
    }

    // The feedback loop's box: beside the node, clear of it, of every other
    // node and of the captions: its upper left first (free over a stack's
    // top), else its left, above it, or its lower left.
    juce::Rectangle<float> feedbackLoop (int osc, const std::array<juce::Point<float>, OscillatorIds::count>& centres, float radius) const
    {
        const auto size = juce::jmax (12.0f, radius * 0.7f);
        const auto centre = centres[(size_t) osc];
        const auto side = radius + size * 0.5f + 3.0f;
        const juce::Point<float> offsets[] { { -side * 0.82f, -side * 0.6f }, { -side, 0.0f }, { 0.0f, -side }, { -side * 0.82f, side * 0.6f } };
        const auto shown = shownOscillators();
        for (const auto offset : offsets)
        {
            const auto box = juce::Rectangle<float> (size, size).withCentre (centre + offset);
            auto clear = getLocalBounds().toFloat().reduced (2.0f).contains (box);
            for (const auto other : shown)
                clear = clear && ! box.intersects (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centres[(size_t) other]).reduced (radius * 0.12f))
                        && ! box.intersects (getCaptionBounds (other, centres, radius));
            if (clear)
                return box;
        }
        return juce::Rectangle<float> (size, size).withCentre (centre + offsets[0]);
    }

    juce::Rectangle<float> getCaptionBounds (int osc, const std::array<juce::Point<float>, OscillatorIds::count>& centres, float radius) const
    {
        const auto centre = centres[(size_t) osc];
        return { centre.x + radius + 5.0f, centre.y - 13.0f, captionWidth, 26.0f };
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
