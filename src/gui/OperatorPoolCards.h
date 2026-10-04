#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "FmOperatorInfo.h"
#include "IlanaLookAndFeel.h"
#include "OperatorEnvDisplay.h"
#include "ParamControls.h"

// The Operator Env in the MOD page's pools (UI review 6, I6-2, I6-3): while
// an oscillator plays it, the ENVELOPES heading carries an OP ENV card and an
// OP PITCH card and the LFO heading an OP LFO card, each with its shape. A
// click opens it on the FM page; OP PITCH and OP LFO drag onto a knob like
// any pool source (they are the Op Pitch Env and Op LFO sources).
class OperatorPoolCard : public juce::Component,
                         public juce::SettableTooltipClient,
                         private IlanaAnim::FrameTimer
{
public:
    enum class Kind { envelope, pitch, lfo };

    OperatorPoolCard (IlanaSynthAudioProcessor& p, Kind kindIn) : processorRef (p), kind (kindIn)
    {
        setTooltip (kind == Kind::envelope ? "The Operator Env: the DX7 envelope each operator on it plays. Click to edit it on FM."
                    : kind == Kind::pitch  ? "The Operator Env's pitch envelope (source: Op Pitch Env). Click to edit it on FM; "
                                             "drag it onto a knob to modulate that too."
                                           : "The Operator Env's LFO (source: Op LFO). Click to edit it on FM; drag it onto a "
                                             "knob to modulate that too.");
        setRepaintsOnMouseActivity (true);
        refresh();
        startTimerHz (4);
    }

    ~OperatorPoolCard() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    // Shown while some oscillator plays the Operator Env; the owner lays
    // out around it.
    std::function<void()> onShownChanged;
    bool isWanted() const { return wanted; }

    // Wide enough for its detail ("5.4 Hz Sine  -> FM") in full.
    int getIdealWidth() const
    {
        const auto text = detail + juce::String (juce::CharPointer_UTF8 ("  \xe2\x86\x92 FM"));
        const auto textWidth = juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::tiny), text);
        return juce::jlimit (214, 340, 16 + 12 + 66 + 52 + 8 + textWidth + 6);
    }

    Mod::Source getSource() const { return kind == Kind::lfo ? Mod::Source::OpLfo : Mod::Source::OpPitchEnv; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto colour = kind == Kind::envelope ? opColour : modSourceColour ((int) getSource());
        IlanaTheme::paintWell (g, bounds, 6.0f);
        if (isMouseOver())
        {
            g.setColour (colour.withAlpha (0.45f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
        }

        auto inner = bounds.reduced (8.0f, 2.0f);
        IlanaTheme::paintTag (g, { inner.getX() + 3.0f, inner.getCentreY() }, colour);
        inner.removeFromLeft (12.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
        const auto title = kind == Kind::envelope ? "OP ENV" : kind == Kind::pitch ? "OP PITCH" : "OP LFO";
        g.drawText (title, inner.removeFromLeft (66.0f), juce::Justification::centredLeft);

        // Its shape, small.
        const auto plot = inner.removeFromLeft (52.0f).reduced (0.0f, 4.0f);
        juce::Path path;
        for (size_t i = 0; i < shape.size(); ++i)
        {
            const auto point = juce::Point<float> (plot.getX() + plot.getWidth() * (float) i / (float) juce::jmax<size_t> (1, shape.size() - 1),
                                                   plot.getBottom() - plot.getHeight() * shape[i]);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        inner.removeFromLeft (8.0f);
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText (detail + juce::String (juce::CharPointer_UTF8 ("  \xe2\x86\x92 FM")), inner, juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.getDistanceFromDragStart() >= 6)
            return;
        if (kind != Kind::envelope)
        {
            FmOperatorInfo::openOperator (-1);
            return;
        }
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (FmOperatorInfo::isPlaying (processorRef, osc) && FmOperatorInfo::usesOperatorEnv (processorRef, osc))
            {
                FmOperatorInfo::openOperator (osc);
                return;
            }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (kind == Kind::envelope || event.getDistanceFromDragStart() < 6)
            return;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this); container != nullptr && ! container->isDragAndDropActive())
        {
            auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            image.multiplyAllAlphas (0.75f);
            container->startDragging ("modsource:" + juce::String ((int) getSource()), this, juce::ScaledImage (image), true);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        if (kind != Kind::envelope)
            highlightedModSource() = (int) getSource();
    }

    void mouseExit (const juce::MouseEvent&) override { highlightedModSource() = 0; }

private:
    void timerCallback() override { refresh(); }

    void refresh()
    {
        const auto nowWanted = FmOperatorInfo::anyOperatorEnv (processorRef);
        if (nowWanted != wanted)
        {
            wanted = nowWanted;
            if (onShownChanged != nullptr)
                onShownChanged();
        }
        // (Visible, not showing: an offscreen editor keeps it current too.)
        if (! wanted || ! isVisible())
            return;

        std::vector<float> nowShape;
        juce::String nowDetail;
        if (kind == Kind::lfo)
        {
            const auto wave = juce::roundToInt (FmOperatorInfo::read (processorRef, "opeg_lfo_wave"));
            for (int i = 0; i <= 24; ++i)
            {
                const auto phase = (float) i / 24.0f;
                const auto value = wave == 0 ? 1.0f - std::abs (phase * 2.0f - 1.0f)
                                   : wave == 1 ? 1.0f - phase
                                   : wave == 2 ? phase
                                   : wave == 3 ? (phase < 0.5f ? 1.0f : 0.0f)
                                   : wave == 4 ? 0.5f + 0.5f * std::sin (phase * juce::MathConstants<float>::twoPi)
                                               : (float) ((i / 4 * 7) % 5) / 4.0f;
                nowShape.push_back (value);
            }
            nowDetail = describeValue ("opeg_lfo_speed", FmOperatorInfo::read (processorRef, "opeg_lfo_speed")) + " "
                        + OperatorEg::lfoWaveNames[(size_t) juce::jlimit (0, 5, wave)];
        }
        else
        {
            juce::String prefix;
            auto count = 0;
            for (int osc = 0; osc < OscillatorIds::count; ++osc)
                if (FmOperatorInfo::isPlaying (processorRef, osc) && FmOperatorInfo::usesOperatorEnv (processorRef, osc))
                {
                    prefix = prefix.isEmpty() ? FmOperatorInfo::prefixOf (osc) : prefix;
                    ++count;
                }
            const auto settings = OperatorEnv::read (processorRef, kind == Kind::pitch ? juce::String() : prefix);
            const auto run = OperatorEnv::run (settings);
            const auto step = juce::jmax<size_t> (1, run.values.size() / 32);
            auto low = 1.0e9, high = -1.0e9;
            for (const auto value : run.values)
            {
                low = std::min (low, value);
                high = std::max (high, value);
            }
            if (kind == Kind::pitch)
                low = std::min (low, -0.05), high = std::max (high, 0.05);
            for (size_t i = 0; i < run.values.size(); i += step)
                nowShape.push_back ((float) ((run.values[i] - low) / std::max (1.0e-6, high - low)));
            nowDetail = kind == Kind::pitch ? juce::String (high - low > 0.11 ? "moves the pitch" : "no pitch movement")
                                            : juce::String (count) + (count == 1 ? " operator" : " operators");
        }

        if (nowShape != shape || nowDetail != detail)
        {
            const auto widthBefore = getIdealWidth();
            shape = std::move (nowShape);
            detail = nowDetail;
            repaint();
            if (getIdealWidth() != widthBefore && onShownChanged != nullptr)
                onShownChanged();
        }
    }

    static inline const juce::Colour opColour { 0xffe3a56f };

    IlanaSynthAudioProcessor& processorRef;
    Kind kind;
    bool wanted = false;
    std::vector<float> shape;
    juce::String detail;
};
