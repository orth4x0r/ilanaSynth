#pragma once

// M8.5: the vector pad. An XY square whose corners are four of the six
// oscillators; the puck sets how much of each you hear (equal-power
// bilinear). Drag the puck, or switch on PATH and drag its eight points: the
// puck then travels the path at RATE. The live position (after the path,
// Evolve's drift and any modulation) is drawn as a ring.

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// One cycle (128 points) of the wavetable frame an oscillator plays, for the
// pictures on the VECTOR page; false while the oscillator isn't a plain
// wavetable one (a sample, an operator or a physical string has no table).
inline bool vectorCornerCycle (IlanaSynthAudioProcessor& processor, int osc, std::array<float, 128>& out)
{
    if (osc < 0 || osc >= OscillatorIds::count || ! processor.isOscillatorShown (osc))
        return false;

    const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
    const auto choice = [&] (const juce::String& id)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (id)))
            return param->getIndex();
        return 0;
    };

    if (choice (prefix + "_mode") != 0)
        return false;

    const auto* table = processor.getWavetable (choice (prefix + "_table"));

    if (table == nullptr || table->getNumFrames() == 0)
        return false;

    const auto* frameParam = processor.apvts.getParameter (prefix + "_frame");
    const auto frame = juce::jlimit (0, table->getNumFrames() - 1,
                                     juce::roundToInt ((frameParam != nullptr ? frameParam->getValue() : 0.0f) * (float) (table->getNumFrames() - 1)));
    const auto* data = table->getFrameData (0, frame);

    for (int i = 0; i < (int) out.size(); ++i)
        out[(size_t) i] = data[juce::jlimit (1, Wavetable::frameSize, i * Wavetable::frameSize / (int) out.size() + 1)];

    return true;
}

// The picture of one corner's oscillator in its own colour, for the CORNERS
// box under the corner's menu (V14-2: the page shows what it mixes).
class VectorCornerWave : public juce::Component,
                         private IlanaAnim::FrameTimer
{
public:
    VectorCornerWave (IlanaSynthAudioProcessor& p, int cornerIn)
        : processorRef (p), corner (cornerIn)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, area, 5.0f);
        const auto osc = processorRef.getVectorCorner (corner);
        std::array<float, 128> cycle {};
        const auto plot = area.reduced (8.0f, 8.0f);

        // A quarter grid behind the cycle, as the design draws these wells.
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 4; ++i)
        {
            g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * (float) i / 4.0f), area.getX() + 1.0f, area.getRight() - 1.0f);
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / 4.0f), area.getY() + 1.0f, area.getBottom() - 1.0f);
        }
        g.setColour (IlanaTheme::Ui::line);
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

        if (! vectorCornerCycle (processorRef, osc, cycle))
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, false));
            const auto shown = osc >= 0 && osc < OscillatorIds::count && processorRef.isOscillatorShown (osc);
            g.drawText (shown ? "NOT A WAVETABLE" : "NOT ADDED", area, juce::Justification::centred);
            return;
        }

        juce::Path path;
        for (int i = 0; i < (int) cycle.size(); ++i)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (cycle.size() - 1);
            const auto y = plot.getCentreY() - cycle[(size_t) i] * plot.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }

        const auto on = processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on")->load() > 0.5f;
        g.setColour (IlanaTheme::oscColour (osc).withAlpha (on ? 1.0f : 0.4f));
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

private:
    void timerCallback() override
    {
        if (isShowing() && changeGate.check (processorRef.getUiEpoch()))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    IlanaSynthAudioProcessor& processorRef;
    int corner;
};

class VectorPadDisplay : public juce::Component,
                         public juce::SettableTooltipClient,
                         private IlanaAnim::FrameTimer
{
public:
    explicit VectorPadDisplay (IlanaSynthAudioProcessor& p)
        : processorRef (p)
    {
        setTooltip ("Vector pad: drag the dot to mix the four corner oscillators. With PATH on, drag the numbered "
                    "points; the dot then travels the path. Double-click: centre.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = square();
        IlanaTheme::paintWell (g, area, 6.0f);
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (area.toNearestInt());
        const auto accent = IlanaTheme::accent();

        // Grid and the four corners' shares, as soft glows.
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 4; ++i)
        {
            const auto t = (float) i / 4.0f;
            g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * t), area.getX(), area.getRight());
            g.drawVerticalLine ((int) (area.getX() + area.getWidth() * t), area.getY(), area.getBottom());
        }

        const auto live = processorRef.getVectorPosition();
        const auto weights = IlanaSynthAudioProcessor::vectorWeights (live.x, live.y);
        const juce::Point<float> corners[4] { area.getTopLeft(), area.getTopRight(), area.getBottomLeft(), area.getBottomRight() };
        for (int c = 0; c < 4; ++c)
        {
            // A corner whose oscillator is off (or not on the page) adds
            // nothing: it is greyed and says so (UI review 4, S27).
            const auto sounding = isCornerSounding (c);

            if (sounding && read ("vec_on") > 0.5f)
            {
                const auto radius = area.getWidth() * (0.12f + 0.3f * weights[(size_t) c]);
                juce::ColourGradient glow (accent.withAlpha (0.35f * weights[(size_t) c] + 0.05f), corners[c],
                                           accent.withAlpha (0.0f), corners[c].translated (radius, 0.0f), true);
                g.setGradientFill (glow);
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (corners[c]));
            }

            // Each corner names its oscillator on a small pill with the
            // oscillator's dot, as the design draws it.
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
            const auto label = getCornerLabel (c, weights[(size_t) c]);
            const auto osc = processorRef.getVectorCorner (c);
            const auto tint = osc >= 0 && osc < OscillatorIds::count ? IlanaTheme::oscColour (osc) : IlanaTheme::Ui::text3;
            const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (font), label);
            auto pill = juce::Rectangle<float> (textWidth + 28.0f, 20.0f);
            pill.setPosition (c % 2 == 0 ? area.getX() + 6.0f : area.getRight() - pill.getWidth() - 6.0f,
                              c < 2 ? area.getY() + 6.0f : area.getBottom() - 26.0f);
            g.setColour (IlanaTheme::Ui::raised.withAlpha (0.85f));
            g.fillRoundedRectangle (pill, 10.0f);
            g.setColour ((sounding ? tint : IlanaTheme::Ui::line).withAlpha (sounding ? 0.7f : 1.0f));
            g.drawRoundedRectangle (pill.reduced (0.5f), 10.0f, 1.0f);
            g.setColour (sounding ? tint : IlanaTheme::Ui::text3);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ pill.getX() + 11.0f, pill.getCentreY() }));
            g.setColour (sounding ? IlanaTheme::Ui::text : IlanaTheme::Ui::text3);
            g.setFont (font);
            IlanaTheme::drawFitted (g, label, pill.withTrimmedLeft (18.0f).withTrimmedRight (4.0f).toNearestInt(),
                                    juce::Justification::centredLeft, 1);
        }

        // Where the puck sits, over the pad's top edge (the design's readout).
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("X " + juce::String (juce::roundToInt (live.x * 100.0f)) + "%  "
                            + juce::String::fromUTF8 ("\xc2\xb7") + "  Y "
                            + juce::String (juce::roundToInt (live.y * 100.0f)) + "%",
                        area.withHeight (16.0f).translated (0.0f, 8.0f).toNearestInt(), juce::Justification::centred);
        }

        // The mix itself: the four corners' cycles at the puck's weights,
        // one trace across the pad, so the pad is a picture of what it
        // makes (V14-2).
        {
            std::array<float, 128> mix {}, one {};
            auto any = false;
            for (int c = 0; c < 4; ++c)
                if (isCornerSounding (c) && vectorCornerCycle (processorRef, processorRef.getVectorCorner (c), one))
                {
                    any = true;
                    for (size_t i = 0; i < mix.size(); ++i)
                        mix[i] += one[i] * weights[(size_t) c];
                }

            if (any)
            {
                const auto plot = area.reduced (24.0f, 0.0f);
                juce::Path trace;
                for (int i = 0; i < (int) mix.size(); ++i)
                {
                    const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (mix.size() - 1);
                    // (Kept inside the pad: a loud mix used to run off its foot.)
                    const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, mix[(size_t) i]) * area.getHeight() * 0.3f;
                    if (i == 0) trace.startNewSubPath (x, y); else trace.lineTo (x, y);
                }
                g.setColour (accent.withAlpha (read ("vec_on") > 0.5f ? 0.55f : 0.4f));
                g.strokePath (trace, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
            }
        }

        // While VECTOR is off the pad does nothing (the page dims it too), but
        // it still shows what it would mix: each corner in its oscillator's
        // colour, dim (V11-24).
        if (read ("vec_on") < 0.5f)
        {
            for (int c = 0; c < 4; ++c)
            {
                const auto osc = processorRef.getVectorCorner (c);
                if (osc < 0 || osc >= OscillatorIds::count || ! processorRef.isOscillatorShown (osc))
                    continue;
                const auto tint = IlanaTheme::oscColour (osc);
                juce::ColourGradient glow (tint.withAlpha (0.22f), corners[c], tint.withAlpha (0.0f),
                                           corners[c].translated (area.getWidth() * 0.4f, 0.0f), true);
                g.setGradientFill (glow);
                g.fillEllipse (juce::Rectangle<float> (area.getWidth() * 0.8f, area.getWidth() * 0.8f).withCentre (corners[c]));
            }

            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (juce::String::fromUTF8 ("VECTOR OFF  \xc2\xb7  switch on to mix the corners"),
                        area.withSizeKeepingCentre (area.getWidth(), 18.0f).withY (area.getBottom() - 44.0f),
                        juce::Justification::centred);
        }

        // The path.
        if (processorRef.isVectorPathOn())
        {
            juce::Path path;
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
            {
                const auto p = toScreen (processorRef.getVectorPathPoint (i));
                if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
            }
            path.closeSubPath();
            g.setColour (accent.withAlpha (0.5f));
            g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved), juce::AffineTransform());
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
            {
                const auto p = toScreen (processorRef.getVectorPathPoint (i));
                g.setColour (i == dragPoint ? juce::Colours::white : accent);
                g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (9.0f, 9.0f).withCentre (p), juce::Justification::centred);
            }
        }

        // The knob position (set by hand) and the live one (after the path,
        // drift and modulation).
        const auto set = toScreen ({ read ("vec_x"), read ("vec_y") });
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (set), 1.2f);
        const auto now = toScreen ({ live.x, live.y });
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (now));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (now));
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragPoint = -1;
        if (processorRef.isVectorPathOn())
            for (int i = 0; i < IlanaSynthAudioProcessor::numVectorPoints; ++i)
                if (toScreen (processorRef.getVectorPathPoint (i)).getDistanceFrom (event.position) < 9.0f)
                    dragPoint = i;
        processorRef.beginEdit (dragPoint >= 0 ? "Vector path point " + juce::String (dragPoint + 1) : juce::String ("Vector position"));
        beginGesture();
        mouseDrag (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto p = fromScreen (event.position);
        if (dragPoint >= 0)
        {
            setParam ("vec_px" + juce::String (dragPoint + 1), p.x);
            setParam ("vec_py" + juce::String (dragPoint + 1), p.y);
        }
        else
        {
            setParam ("vec_x", p.x);
            setParam ("vec_y", p.y);
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endGesture();
        processorRef.endEdit();
        dragPoint = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        processorRef.performEdit ("Centre vector position", [this]
        {
            setParam ("vec_x", 0.5f);
            setParam ("vec_y", 0.5f);
        });
    }

    // A corner's oscillator is on the page and switched on.
    bool isCornerSounding (int corner) const
    {
        const auto osc = processorRef.getVectorCorner (corner);

        if (osc < 0 || osc >= OscillatorIds::count || ! processorRef.isOscillatorShown (osc))
            return false;

        return read (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_on") > 0.5f;
    }

    // "OSC 1  25%" while the vector plays; "OSC 4: none" for an
    // oscillator the patch doesn't have, ": off" for one switched off; no
    // share while the vector is off (UI review 6, V29).
    juce::String getCornerLabel (int corner, float weight) const
    {
        const auto osc = processorRef.getVectorCorner (corner);
        const auto name = "OSC " + juce::String (osc + 1);

        if (osc < 0 || osc >= OscillatorIds::count || ! processorRef.isOscillatorShown (osc))
            return name + ": none";

        if (! isCornerSounding (corner))
            return name + ": off";

        return read ("vec_on") > 0.5f ? name + "  " + juce::String (juce::roundToInt (weight * weight * 100.0f)) + "%" : name;
    }

private:
    juce::Rectangle<float> square() const
    {
        const auto side = (float) juce::jmin (getWidth(), getHeight()) - 4.0f;
        return juce::Rectangle<float> (side, side).withCentre (getLocalBounds().toFloat().getCentre());
    }

    juce::Point<float> toScreen (juce::Point<float> p) const
    {
        const auto area = square().reduced (10.0f);
        return { area.getX() + p.x * area.getWidth(), area.getBottom() - p.y * area.getHeight() };
    }

    juce::Point<float> fromScreen (juce::Point<float> p) const
    {
        const auto area = square().reduced (10.0f);
        return { juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth()),
                 juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight()) };
    }

    float read (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    void setParam (const juce::String& id, float value)
    {
        if (auto* parameter = processorRef.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    void beginGesture()
    {
        for (const auto* id : { "vec_x", "vec_y" })
            if (auto* parameter = processorRef.apvts.getParameter (id))
                parameter->beginChangeGesture();
    }

    void endGesture()
    {
        for (const auto* id : { "vec_x", "vec_y" })
            if (auto* parameter = processorRef.apvts.getParameter (id))
                parameter->endChangeGesture();
    }

    void timerCallback() override
    {
        if (isShowing() && (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    int dragPoint = -1;
};
