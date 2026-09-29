#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamInfo.h"

struct ModRingConfig
{
    int destination = 0;
    float scale = 1.0f;
};

// Which modulation destination a knob drives, and how far a full-depth
// modulation moves the knob (as a fraction of its travel) for the ring.
inline ModRingConfig modRingConfigFor (const juce::String& id)
{
    using D = Mod::Destination;
    const auto explicitDest = [] (D d, float scale) { return ModRingConfig { (int) d, scale }; };

    struct OscIds { const char* prefix; D pitch, frame, level, pan, detune, spread, warp, blend, start, end; };
    static const OscIds oscs[] {
        { "osc1", D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1Pan, D::Osc1Detune, D::Osc1Spread, D::Osc1Warp,
          D::Osc1Blend, D::Osc1SampleStart, D::Osc1SampleEnd },
        { "osc2", D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2Pan, D::Osc2Detune, D::Osc2Spread, D::Osc2Warp,
          D::Osc2Blend, D::Osc2SampleStart, D::Osc2SampleEnd },
        { "sub", D::SubPitch, D::SubFrame, D::SubLevel, D::SubPan, D::SubDetune, D::SubSpread, D::SubWarp,
          D::SubBlend, D::SubSampleStart, D::SubSampleEnd },
        { "osc4", D::Osc4Pitch, D::Osc4Frame, D::Osc4Level, D::Osc4Pan, D::Osc4Detune, D::Osc4Spread, D::Osc4Warp,
          D::Osc4Blend, D::Osc4SampleStart, D::Osc4SampleEnd },
        { "osc5", D::Osc5Pitch, D::Osc5Frame, D::Osc5Level, D::Osc5Pan, D::Osc5Detune, D::Osc5Spread, D::Osc5Warp,
          D::Osc5Blend, D::Osc5SampleStart, D::Osc5SampleEnd },
        { "osc6", D::Osc6Pitch, D::Osc6Frame, D::Osc6Level, D::Osc6Pan, D::Osc6Detune, D::Osc6Spread, D::Osc6Warp,
          D::Osc6Blend, D::Osc6SampleStart, D::Osc6SampleEnd },
    };

    for (const auto& osc : oscs)
    {
        const juce::String prefix (osc.prefix);

        if (! id.startsWith (prefix + "_"))
            continue;

        const auto field = id.fromFirstOccurrenceOf (prefix + "_", false, false);

        if (field == "semi" || field == "fine") return explicitDest (osc.pitch, 1.0f);
        if (field == "frame")        return explicitDest (osc.frame, 1.0f);
        if (field == "level")        return explicitDest (osc.level, 1.0f);
        if (field == "pan")          return explicitDest (osc.pan, 0.5f);
        if (field == "detune")       return explicitDest (osc.detune, 1.0f);
        if (field == "spread")       return explicitDest (osc.spread, 1.0f);
        if (field == "warp_amt")     return explicitDest (osc.warp, 1.0f);
        if (field == "uni_blend")    return explicitDest (osc.blend, 1.0f);
        if (field == "sample_start") return explicitDest (osc.start, 0.5f);
        if (field == "sample_end")   return explicitDest (osc.end, 0.5f);
    }

    struct Entry { const char* id; D destination; float scale; };
    static const Entry entries[] {
        { "noise_level", D::NoiseLevel, 1.0f },
        { "f1_cutoff", D::Filter1Cutoff, 0.5f }, { "f1_reso", D::Filter1Reso, 1.0f },
        { "f1_drive", D::Filter1Drive, 1.0f }, { "f1_env", D::Filter1Env, 0.5f }, { "f1_fm", D::Filter1Fm, 0.5f },
        { "f1_morph", D::Filter1Morph, 1.0f },
        { "f2_cutoff", D::Filter2Cutoff, 0.5f }, { "f2_reso", D::Filter2Reso, 1.0f },
        { "f2_drive", D::Filter2Drive, 1.0f }, { "f2_env", D::Filter2Env, 0.5f }, { "f2_fm", D::Filter2Fm, 0.5f },
        { "f2_morph", D::Filter2Morph, 1.0f },
        { "fm_amount", D::FmAmount, 1.0f }, { "fm_feedback", D::FmFeedback, 1.0f },
        { "ring_mod", D::RingMod, 1.0f }, { "drift", D::Drift, 1.0f },
        { "lfo1_rate", D::Lfo1Rate, 0.3f }, { "lfo2_rate", D::Lfo2Rate, 0.3f },
        { "lfo3_rate", D::Lfo3Rate, 0.3f }, { "lfo4_rate", D::Lfo4Rate, 0.3f }, { "mseg_rate", D::MsegRate, 0.3f },
        { "res_amount", D::ResAmount, 1.0f }, { "res_decay", D::ResDecay, 1.0f }, { "res_offset", D::ResOffset, 0.5f },
        { "amp_attack", D::AmpAttack, 0.5f }, { "amp_decay", D::AmpDecay, 0.5f },
        { "amp_sustain", D::AmpSustain, 1.0f }, { "amp_release", D::AmpRelease, 0.5f },
        { "fe_attack", D::FeAttack, 0.5f }, { "fe_decay", D::FeDecay, 0.5f },
        { "fe_sustain", D::FeSustain, 1.0f }, { "fe_release", D::FeRelease, 0.5f },
        { "me_attack", D::MeAttack, 0.5f }, { "me_decay", D::MeDecay, 0.5f },
        { "me_sustain", D::MeSustain, 1.0f }, { "me_release", D::MeRelease, 0.5f },
        { "f2e_attack", D::F2eAttack, 0.5f }, { "f2e_decay", D::F2eDecay, 0.5f },
        { "f2e_sustain", D::F2eSustain, 1.0f }, { "f2e_release", D::F2eRelease, 0.5f },
        { "e4_attack", D::E4Attack, 0.5f }, { "e4_decay", D::E4Decay, 0.5f },
        { "e4_sustain", D::E4Sustain, 1.0f }, { "e4_release", D::E4Release, 0.5f },
        { "fx_drive_amount", D::FxDriveAmount, 1.0f }, { "fx_crush_mix", D::FxCrushMix, 1.0f },
        { "fx_comb_freq", D::FxCombFreq, 0.6f }, { "fx_phaser_rate", D::FxPhaserRate, 0.55f },
        { "fx_chorus_depth", D::FxChorusDepth, 1.0f }, { "fx_delay_mix", D::FxDelayMix, 1.0f },
        { "fx_delay_feedback", D::FxDelayFeedback, 0.95f }, { "fx_smear_mix", D::FxSmearMix, 1.0f },
        { "fx_freeze_mix", D::FxFreezeMix, 1.0f }, { "fx_reverb_mix", D::FxReverbMix, 1.0f },
        { "fx_reverb_size", D::FxReverbSize, 1.0f },
    };

    for (const auto& entry : entries)
        if (id == entry.id)
            return explicitDest (entry.destination, entry.scale);

    for (int lfo = 4; lfo < Mod::numLfoSources; ++lfo)
        if (id == "lfo" + juce::String (lfo + 1) + "_rate")
            return explicitDest (Mod::lfoRateDestinationFor (lfo), 0.3f);

    // Everything else that can be modulated is a plain-parameter destination,
    // offset in the knob's own normalised range.
    return { Mod::destinationForParamId (id), 1.0f };
}

// What a mod source drives, for the envelope and LFO pool cards: "fixed"
// (built-in uses such as "Amp") first, then the matrix destinations, as
// "first target +N". Empty when it drives nothing.
inline juce::String describeModTargets (const IlanaSynthAudioProcessor& processor, Mod::Source source,
                                        juce::StringArray targets = {})
{
    static const auto names = Mod::getDestinationNames();

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.isActive() && (routing.source == source || routing.aux == source)
            && juce::isPositiveAndBelow (routing.destination, names.size()))
            targets.addIfNotAlreadyThere (names[routing.destination]);
    }

    if (targets.isEmpty())
        return {};

    return targets[0] + (targets.size() > 1 ? "  +" + juce::String (targets.size() - 1) : juce::String());
}

// A small tag naming what a card drives, at the left of `area`'s bottom.
inline void paintTargetTag (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour colour)
{
    if (text.isEmpty() || area.getWidth() < 28.0f)
        return;

    const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    const auto width = juce::jmin (area.getWidth(), juce::GlyphArrangement::getStringWidth (font, text) + 12.0f);
    const auto tag = juce::Rectangle<float> (area.getX(), area.getBottom() - 13.0f, width, 13.0f);

    g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
    g.fillRoundedRectangle (tag, 6.5f);
    g.setColour (colour.withAlpha (0.9f));
    g.setFont (font);
    g.drawFittedText (text, tag.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
}

// The source currently hovered (chip, macro or LFO card), so knobs it
// modulates can light up. Message thread only.
inline int& highlightedModSource()
{
    static int source = 0;
    return source;
}

// One colour per modulation source, used by its chip, its card (LFO and
// envelope pages), its tabs, the matrix and the rings on knobs it moves.
inline juce::Colour modSourceColour (int sourceIndex)
{
    // Every LFO (A and B outputs) in the LFO palette.
    if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) sourceIndex); lfo >= 0)
        return IlanaSynthAudioProcessor::lfoColour (lfo);

    if (sourceIndex >= (int) Mod::Source::Lfo1B && sourceIndex <= (int) Mod::Source::Lfo16B)
        return IlanaSynthAudioProcessor::lfoColour (sourceIndex - (int) Mod::Source::Lfo1B);

    switch ((Mod::Source) sourceIndex)
    {
        case Mod::Source::AmpEnv:     return juce::Colour (0xffff5a4a); // not the accent: it would clash with FILT ENV or LFO 1
        case Mod::Source::FilterEnv:  return juce::Colour (0xffc86bff);
        case Mod::Source::FilterEnv2: return juce::Colour (0xff8f9dff);
        case Mod::Source::ModEnv:     return juce::Colour (0xff8fff3b);
        case Mod::Source::Env4:       return juce::Colour (0xff5b8cff);
        case Mod::Source::Velocity:
        case Mod::Source::KeyTrack:
        case Mod::Source::Random:
        case Mod::Source::ClockSh:    return IlanaTheme::Ui::text2;
        case Mod::Source::ModWheel:
        case Mod::Source::Aftertouch:
        case Mod::Source::Expression: return juce::Colour (0xff9fb3c8);
        case Mod::Source::Macro1:
        case Mod::Source::Macro2:
        case Mod::Source::Macro3:
        case Mod::Source::Macro4:     return juce::Colour (0xffffd447);
        case Mod::Source::Mseg:       return juce::Colour (0xffe0e6f0);
        default: break;
    }

    // ENV 6-16 spread from red to violet, short of the pinks the accent and
    // AMP ENV use.
    if (sourceIndex >= (int) Mod::Source::Env6 && sourceIndex <= (int) Mod::Source::Env16)
        return juce::Colour::fromHSV (0.04f + 0.7f * (float) (sourceIndex - (int) Mod::Source::Env6) / 10.0f, 0.55f, 0.95f, 1.0f);

    return IlanaTheme::accent();
}

inline juce::String& knobClipboard()
{
    static juce::String value;
    return value;
}

// The coloured dots beside a modulated knob: one per routing into it.
// Drag a dot up or down to change that routing's depth, double-click it to
// remove the routing. Hovering shows which source it is.
class ModDotStrip : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    struct Dot
    {
        int slot = -1;
        int source = 0;
        float depth = 0.0f;
    };

    std::function<void (int slot, float depth)> onDepthChange;
    std::function<void (int slot)> onRemove;

    void setDots (const std::vector<Dot>& newDots)
    {
        if (dragIndex >= 0)
            return; // keep the list stable mid-drag

        dots = newDots;
        updateTooltip();
        repaint();
    }

    static constexpr int dotSize = 8;
    static constexpr int dotPitch = 11;

    int getPreferredHeight() const { return (int) dots.size() * dotPitch; }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < (int) dots.size(); ++i)
        {
            const auto& dot = dots[(size_t) i];
            const auto area = dotBounds (i);
            const auto colour = modSourceColour (dot.source);
            const auto active = i == dragIndex || i == hoverIndex;

            g.setColour (colour.withAlpha (active ? 0.35f : 0.18f));
            g.fillEllipse (area.expanded (active ? 2.5f : 1.5f));

            // Fill shows the depth: a pie from 12 o'clock, clockwise for
            // positive and anticlockwise for negative.
            g.setColour (IlanaTheme::Ui::bg);
            g.fillEllipse (area);

            juce::Path pie;
            const auto angle = juce::jlimit (-1.0f, 1.0f, dot.depth) * juce::MathConstants<float>::twoPi;
            pie.addPieSegment (area, 0.0f, angle, 0.0f);
            g.setColour (colour);
            g.fillPath (pie);

            g.setColour (colour.withAlpha (0.9f));
            g.drawEllipse (area, 1.0f);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index != hoverIndex)
        {
            hoverIndex = index;
            updateTooltip();
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragIndex = indexAt (event.position);

        if (dragIndex >= 0)
            dragStartDepth = dots[(size_t) dragIndex].depth;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragIndex < 0)
            return;

        const auto fine = event.mods.isShiftDown() ? 0.2f : 1.0f;
        const auto depth = juce::jlimit (-1.0f, 1.0f, dragStartDepth - (float) event.getDistanceFromDragStartY() * 0.008f * fine);
        dots[(size_t) dragIndex].depth = depth;
        updateTooltip();
        repaint();

        if (onDepthChange != nullptr)
            onDepthChange (dots[(size_t) dragIndex].slot, depth);
    }

    void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index >= 0 && onRemove != nullptr)
            onRemove (dots[(size_t) index].slot);
    }

private:
    juce::Rectangle<float> dotBounds (int index) const
    {
        return juce::Rectangle<float> ((float) dotSize, (float) dotSize)
            .withCentre ({ (float) getWidth() * 0.5f, (float) (index * dotPitch) + (float) dotPitch * 0.5f });
    }

    int indexAt (juce::Point<float> position) const
    {
        for (int i = 0; i < (int) dots.size(); ++i)
            if (dotBounds (i).expanded (2.0f).contains (position))
                return i;

        return -1;
    }

    void updateTooltip()
    {
        const auto index = hoverIndex >= 0 ? hoverIndex : dragIndex;

        if (! juce::isPositiveAndBelow (index, (int) dots.size()))
        {
            setTooltip ("Modulation\nDrag a dot to set its depth, double-click it to remove the routing.");
            return;
        }

        const auto& dot = dots[(size_t) index];
        setTooltip (Mod::getSourceNames()[dot.source] + "  " + juce::String (juce::roundToInt (dot.depth * 100.0f))
                    + " %\nDrag up or down to set the depth (Shift for fine), double-click to remove.");
    }

    std::vector<Dot> dots;
    int dragIndex = -1;
    int hoverIndex = -1;
    float dragStartDepth = 0.0f;
};

class KnobControl : public juce::Component,
                    public juce::DragAndDropTarget,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    KnobControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                 const juce::String& labelText, juce::Colour accent = IlanaTheme::accent(),
                 bool followsThemeIn = true)
        : ringConfig (modRingConfigFor (parameterID)),
          parameterId (parameterID),
          knobAccent (accent),
          followsTheme (followsThemeIn)
    {
        processorRef = dynamic_cast<IlanaSynthAudioProcessor*> (&state.processor);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 14);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        addAndMakeVisible (slider);
        slider.addMouseListener (this, false);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        addAndMakeVisible (label);

        dotStrip.onDepthChange = [this] (int slot, float depth)
        {
            if (processorRef != nullptr)
                processorRef->setModSlotValue (slot, "amt", depth);
        };
        dotStrip.onRemove = [this] (int slot)
        {
            if (processorRef != nullptr)
                processorRef->clearModSlot (slot);

            refreshRoutings();
        };
        addChildComponent (dotStrip);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

        parameter = state.getParameter (parameterID);

        if (parameter != nullptr)
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

            const auto description = describeParameter (parameterID);
            const auto modHint = ringConfig.destination != 0
                                     ? juce::String ("  Drop a mod source here, or right-click to modulate.")
                                     : juce::String();
            const auto tooltip = parameter->getName (64)
                                 + (description.isNotEmpty() || modHint.isNotEmpty() ? "\n" + description + modHint : "");
            slider.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        lastSliderValue = slider.getValue();
        refreshRoutings();
        startTimerHz (30);
    }

    void lookAndFeelChanged() override
    {
        if (followsTheme)
            slider.setColour (juce::Slider::rotarySliderFillColourId, IlanaTheme::accent());
    }

    // Gives the knob a fixed identity colour (an oscillator's, say) in place
    // of the theme accent.
    void setIdentityColour (juce::Colour colour)
    {
        knobAccent = colour;
        followsTheme = false;
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            showModMenu();
    }

    juce::Slider& getSlider() { return slider; }
    void setLabelText (const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        resized(); // a knob with a label lays out differently
    }
    juce::String getLabelText() const { return label.getText(); }
    bool isCompact() const { return compact; }
    const juce::String& getParameterId() const { return parameterId; }
    int getNumRoutings() const { return (int) routings.size(); }

    // Compact knobs (bottom strip) have no label or value box: the owner
    // draws those next to the knob.
    void setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        label.setVisible (! compact);
        slider.setTextBoxStyle (compact ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 64, 14);
        resized();
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hover = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = false;
        repaint();
    }



    void paint (juce::Graphics& g) override
    {
        const auto mod = processorRef != nullptr && ringConfig.destination != 0
                             ? processorRef->getModDisplay (ringConfig.destination)
                             : 0.0f;
        const auto modActive = std::abs (mod) > 0.001f;
        const auto glowColour = modActive ? modSourceColour (dominantSource) : IlanaTheme::accent();
        const auto glowIntensity = juce::jmax (glow * 0.09f, activity * 0.2f);
        const auto centre = rotaryArea().getCentre();
        const auto knobRadius = knobRadiusFor (knobBounds);

        if (glowIntensity > 0.005f)
        {
            for (int ring = 2; ring >= 1; --ring)
            {
                const auto radius = knobRadius + (float) ring * 3.5f;
                g.setColour (glowColour.withAlpha (glowIntensity * (ring == 2 ? 0.4f : 0.7f)));
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            }
        }

        // A hovered source lights up every knob it modulates.
        const auto highlighted = highlightedModSource();

        if (highlighted != 0 && routesFrom (highlighted))
        {
            const auto radius = knobRadius + 5.0f;
            g.setColour (modSourceColour (highlighted).withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
            g.setColour (modSourceColour (highlighted).withAlpha (0.8f));
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.5f);
        }

        if (dragHover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.fillRoundedRectangle (knobBounds.toFloat().reduced (2.0f), 6.0f);
        }

        if (! modActive || knobRadius < 8.0f)
            return;

        const auto lineWidth = 2.0f;
        const auto arcRadius = knobRadius - lineWidth * 0.5f;
        const auto startAngle = juce::MathConstants<float>::pi * 1.2f;
        const auto endAngle = juce::MathConstants<float>::pi * 2.8f;

        const auto baseNorm = (float) juce::jlimit (0.0, 1.0, slider.valueToProportionOfLength (slider.getValue()));
        const auto displayNorm = juce::jlimit (0.0f, 1.0f, baseNorm + mod * ringConfig.scale);

        if (std::abs (displayNorm - baseNorm) < 0.001f)
            return;

        const auto angleA = startAngle + baseNorm * (endAngle - startAngle);
        const auto angleB = startAngle + displayNorm * (endAngle - startAngle);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                           juce::jmin (angleA, angleB), juce::jmax (angleA, angleB), true);

        g.setColour (modSourceColour (dominantSource).withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (compact)
        {
            knobBounds = area;
            slider.setBounds (area);
            layoutDots();
            return;
        }

        // Label, dial and value as one tight group, so the gaps between them
        // are the same whatever size cell a page gives the knob (a tall cell
        // used to push the label up and the value down). The group sits at
        // the top, where combo and toggle labels in the same row sit; a knob
        // without a label (a matrix cell) is centred instead.
        constexpr int labelHeight = 13, valueHeight = 16, minDial = 28, maxDial = 58;
        const auto hasLabel = label.getText().isNotEmpty();
        const auto dial = juce::jlimit (minDial, maxDial,
                                        juce::jmin (area.getWidth(), area.getHeight() - (hasLabel ? labelHeight : 0) - valueHeight));
        const auto groupHeight = juce::jmin (area.getHeight(), (hasLabel ? labelHeight : 0) + dial + valueHeight);
        auto group = hasLabel ? area.removeFromTop (groupHeight) : area.withSizeKeepingCentre (area.getWidth(), groupHeight);

        label.setBounds (hasLabel ? group.removeFromTop (labelHeight) : juce::Rectangle<int>());
        knobBounds = group;
        slider.setBounds (group);
        layoutDots();
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return processorRef != nullptr
               && ringConfig.destination != 0
               && details.description.toString().startsWith ("modsource:");
    }

    void itemDragEnter (const SourceDetails&) override
    {
        dragHover = true;
        repaint();
    }

    void itemDragExit (const SourceDetails&) override
    {
        dragHover = false;
        repaint();
    }

    void itemDropped (const SourceDetails& details) override
    {
        dragHover = false;

        if (processorRef == nullptr)
            return;

        const auto sourceIndex = details.description.toString()
                                     .fromFirstOccurrenceOf ("modsource:", false, false)
                                     .getIntValue();

        processorRef->assignModSlot (sourceIndex, ringConfig.destination, 0.35f);
        refreshRoutings();
        repaint();
    }

private:
    float knobRadiusFor (juce::Rectangle<int>) const
    {
        // Deliberately not reduced like the knob itself: the mod ring sits
        // just outside the value arc.
        const auto area = rotaryArea();
        return juce::jlimit (14.0f, 30.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f);
    }

    // The rotary is drawn above the value text box, so glow and mod ring must
    // use the same area or they appear off-centre.
    juce::Rectangle<float> rotaryArea() const
    {
        auto area = knobBounds.toFloat();

        if (! compact)
            area.setHeight (juce::jmax (8.0f, area.getHeight() - 16.0f));

        return area;
    }

    bool routesFrom (int source) const
    {
        for (const auto& dot : routings)
            if (dot.source == source)
                return true;

        return false;
    }

    void layoutDots()
    {
        const auto area = rotaryArea();
        const auto radius = knobRadiusFor (knobBounds);
        const auto height = juce::jmax (ModDotStrip::dotPitch, dotStrip.getPreferredHeight());
        const auto x = (int) (area.getCentreX() + radius + 3.0f);
        const auto y = (int) (area.getCentreY() - radius + 2.0f);

        dotStrip.setBounds (juce::jmin (x, getWidth() - 12), y, 12, height);
        dotStrip.setVisible (! routings.empty() && ! compact);
    }

    // Re-reads which mod slots route into this knob. Cheap (raw parameter
    // reads), so it runs on the timer.
    void refreshRoutings()
    {
        if (processorRef == nullptr || ringConfig.destination == 0)
            return;

        std::vector<ModDotStrip::Dot> found;
        auto strongest = 0.0f;
        dominantSource = 0;

        for (int i = 0; i < Mod::maxSlots && found.size() < 6; ++i)
        {
            const auto slot = processorRef->readModSlot (i);

            if (slot.destination != ringConfig.destination || slot.source == Mod::Source::None)
                continue;

            found.push_back ({ i, (int) slot.source, slot.depth });

            if (! slot.bypass && std::abs (slot.depth) > strongest)
            {
                strongest = std::abs (slot.depth);
                dominantSource = (int) slot.source;
            }
        }

        const auto changed = found.size() != routings.size()
                             || ! std::equal (found.begin(), found.end(), routings.begin(),
                                              [] (const auto& a, const auto& b)
                                              { return a.slot == b.slot && a.source == b.source
                                                       && std::abs (a.depth - b.depth) < 1.0e-4f; });

        if (! changed)
            return;

        routings = std::move (found);
        dotStrip.setDots (routings);
        layoutDots();
        repaint();
    }

    void showModMenu()
    {
        if (processorRef == nullptr)
            return;

        juce::PopupMenu menu;

        if (ringConfig.destination != 0)
        {
            const auto sources = Mod::getSourceNames();
            juce::PopupMenu sourceMenu, lfoMenu, envMenu, otherMenu;

            for (int i = 1; i < sources.size(); ++i)
            {
                const auto source = (Mod::Source) i;
                const auto isEnvelope = source == Mod::Source::AmpEnv || source == Mod::Source::FilterEnv
                                        || source == Mod::Source::FilterEnv2 || source == Mod::Source::ModEnv
                                        || source == Mod::Source::Env4
                                        || (source >= Mod::Source::Env6 && source <= Mod::Source::Env16);
                auto& target = Mod::lfoIndexFor (source) >= 0 ? lfoMenu : (isEnvelope ? envMenu : otherMenu);
                target.addItem (i + 1, sources[i], true, routesFrom (i));
            }

            sourceMenu.addSubMenu ("LFOs", lfoMenu);
            sourceMenu.addSubMenu ("Envelopes", envMenu);
            sourceMenu.addSubMenu ("Performance and more", otherMenu);
            menu.addSubMenu ("Modulate with", sourceMenu);

            if (! routings.empty())
            {
                juce::PopupMenu removeMenu;

                for (const auto& dot : routings)
                    removeMenu.addItem (5000 + dot.slot, sources[dot.source] + "  ("
                                                             + juce::String (juce::roundToInt (dot.depth * 100.0f)) + " %)");

                menu.addSubMenu ("Remove modulation", removeMenu);
                menu.addItem (1000, "Clear all modulation to this knob");
            }

            menu.addSeparator();
        }

        if (parameter != nullptr)
            menu.addItem (2000, "Reset to default");

        menu.addSeparator();
        menu.addItem (3000, "Copy value");

        if (knobClipboard().isNotEmpty())
            menu.addItem (3001, "Paste value");

        if (parameterId.startsWith ("macro"))
        {
            const auto macroIndex = parameterId.getTrailingIntValue() - 1;

            menu.addSeparator();
            menu.addItem (4000, "MIDI Learn  (CC " + juce::String (processorRef->getMacroCc (macroIndex)) + ")");
        }

        juce::Component::SafePointer<KnobControl> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                auto& processor = *safeThis->processorRef;

                                if (result == 1000)
                                    processor.clearModSlotsForTarget (safeThis->ringConfig.destination);
                                else if (result == 2000)
                                {
                                    if (safeThis->parameter != nullptr)
                                        safeThis->parameter->setValueNotifyingHost (safeThis->parameter->getDefaultValue());
                                }
                                else if (result == 3000)
                                {
                                    knobClipboard() = juce::String (safeThis->slider.getValue(), 6);
                                }
                                else if (result == 3001)
                                {
                                    const auto value = knobClipboard().getDoubleValue();
                                    safeThis->slider.setValue (value, juce::sendNotificationSync);
                                }
                                else if (result == 4000)
                                {
                                    processor.startMacroLearn (safeThis->parameterId.getTrailingIntValue() - 1);
                                }
                                else if (result >= 5000)
                                {
                                    processor.clearModSlot (result - 5000);
                                }
                                else
                                {
                                    processor.assignModSlot (result - 1, safeThis->ringConfig.destination, 0.35f);
                                }

                                safeThis->refreshRoutings();
                            });
    }

    void timerCallback() override
    {
        // Knobs on hidden pages skip the frame (hundreds of them).
        if (! isShowing())
            return;

        const auto targetGlow = hover ? 1.0f : 0.0f;
        glow = IlanaAnim::approach (glow, targetGlow, 0.22f, frameTicks());

        const auto value = slider.getValue();

        // Only the user's own moves light the knob up, not a preset load,
        // undo or automation.
        if (std::abs (value - lastSliderValue) > 1.0e-6)
        {
            lastSliderValue = value;

            if (slider.isMouseOverOrDragging())
                activity = 1.0f;
        }

        activity = IlanaAnim::decay (activity, 0.88f, frameTicks());

        if (processorRef == nullptr || ringConfig.destination == 0)
        {
            if ((glow > 0.01f && glow < 0.99f) || activity > 0.01f)
                repaint();

            return;
        }

        if ((routingCheck += frameSeconds()) >= 0.2f)
        {
            routingCheck = 0.0f;
            refreshRoutings();
        }

        const auto highlighted = highlightedModSource();
        const auto modValue = processorRef->getModDisplay (ringConfig.destination);

        if (std::abs (modValue - lastModValue) > 0.002f || highlighted != lastHighlighted)
        {
            lastModValue = modValue;
            lastHighlighted = highlighted;
            repaint();
        }
        else if ((glow > 0.01f && glow < 0.99f) || activity > 0.01f)
        {
            repaint();
        }
    }

    juce::Slider slider;
    juce::Label label;
    ModDotStrip dotStrip;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    IlanaSynthAudioProcessor* processorRef = nullptr;
    juce::RangedAudioParameter* parameter = nullptr;
    ModRingConfig ringConfig;
    juce::String parameterId;
    juce::Colour knobAccent;
    bool followsTheme = false;
    juce::Rectangle<int> knobBounds;
    std::vector<ModDotStrip::Dot> routings;
    int dominantSource = 0;
    float routingCheck = 0.0f;
    int lastHighlighted = 0;
    float lastModValue = 0.0f;
    float glow = 0.0f;
    float activity = 0.0f;
    double lastSliderValue = 0.0;
    bool dragHover = false;
    bool hover = false;
    bool compact = false;
};

class ComboControl : public juce::Component,
                     public juce::SettableTooltipClient,
                     private IlanaAnim::FrameTimer
{
public:
    ComboControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                  const juce::String& labelText)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        label.setColour (juce::Label::textColourId, IlanaTheme::Ui::text2);
        addAndMakeVisible (label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (parameterID)))
            combo.addItemList (choice->getAllValueStrings(), 1);

        addAndMakeVisible (combo);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, parameterID, combo);

        if (auto* parameter = state.getParameter (parameterID))
        {
            const auto description = describeParameter (parameterID);
            const auto tooltip = parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : "");
            combo.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        startTimerHz (30);
    }

    juce::ComboBox& getComboBox() { return combo; }

    // Replace the drop-down list with something else when clicked.
    void setPopupOverride (std::function<void()> override) { combo.popupOverride = std::move (override); }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (13));
        combo.setBounds (area.removeFromTop (24));
    }

    void paint (juce::Graphics& g) override
    {
        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.2f * hover));
            g.fillRoundedRectangle (combo.getBounds().toFloat().expanded (2.0f), 5.0f);
        }
    }



private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        const auto target = isMouseOver() ? 1.0f : 0.0f;

        if (std::abs (hover - target) < 0.005f)
            return;

        hover = std::abs (hover - target) < 0.01f ? target : IlanaAnim::approach (hover, target, 0.22f, frameTicks());
        repaint();
    }

    // A ComboBox whose popup can be replaced (e.g. by the wavetable browser).
    struct PopupCombo : public juce::ComboBox
    {
        std::function<void()> popupOverride;

        void showPopup() override
        {
            if (popupOverride != nullptr)
            {
                popupOverride();

                // ComboBox marks its menu active on click and only clears that
                // when its own menu closes; without this, later clicks are ignored.
                hidePopup();
            }
            else
                juce::ComboBox::showPopup();
        }
    };

    PopupCombo combo;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    float hover = 0.0f;
};

class ToggleControl : public juce::Component,
                      public juce::SettableTooltipClient,
                      private IlanaAnim::FrameTimer
{
public:
    ToggleControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                   const juce::String& labelText)
    {
        button.setButtonText (labelText);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, IlanaTheme::accent().withAlpha (0.85f));
        addAndMakeVisible (button);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterID, button);

        // Every on/off is drawn as a sliding switch (a lit and a dark button
        // were easy to misread): a card's bare "ON" switch, or a named one
        // with its name as a label above it, like a knob's or a menu's.
        switchAmount = button.getToggleState() ? 1.0f : 0.0f;
        button.getProperties().set ("switch", true);
        button.getProperties().set ("switchAmount", switchAmount);

        if (labelText != "ON")
            button.getProperties().set ("switchLeft", true);

        if (auto* parameter = state.getParameter (parameterID))
        {
            const auto description = describeParameter (parameterID);
            const auto tooltip = parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : "");
            button.setTooltip (tooltip);
            setTooltip (tooltip);
        }

        startTimerHz (30);
    }

    juce::TextButton& getButton() { return button; }

    bool isSwitch() const { return button.getProperties().contains ("switch"); }
    // A switch with its name above it, rather than a card's bare ON.
    bool isNamedSwitch() const { return isSwitch() && button.getButtonText() != "ON"; }

    // (Every toggle is a switch now; kept for the call sites that ask.)
    void showAsSwitch()
    {
        switchAmount = button.getToggleState() ? 1.0f : 0.0f;
        button.getProperties().set ("switch", true);
        button.getProperties().set ("switchAmount", switchAmount);
        button.getProperties().set ("switchLeft", true);
        repaint();
    }

    // A button that lights up flares, breathes once or twice, then holds a
    // steady glow (a glow that never settles keeps the whole UI busy).
    void paint (juce::Graphics& g) override
    {
        // A named switch shows its name where other controls show a label.
        if (isSwitch() && button.getButtonText() != "ON")
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
            g.drawText (button.getButtonText(), getLocalBounds().withHeight (13), juce::Justification::centredLeft, true);
            return;
        }

        if (isSwitch() || ! button.getToggleState())
            return;

        const auto settle = juce::jlimit (0.0f, 1.0f, litSeconds / breathSeconds);
        const auto breath = 0.5f + 0.5f * std::cos (litSeconds * 4.2f);
        const auto pulse = juce::jmap (settle, breath, 0.6f);
        IlanaTheme::paintGlow (g, button.getBounds().toFloat().reduced (0.5f), 5.0f,
                               button.findColour (juce::TextButton::buttonOnColourId).withAlpha (1.0f), 0.4f + 0.6f * pulse);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (13);
        button.setBounds (area.removeFromTop (juce::jmin (24, juce::jmax (16, area.getHeight()))));
    }



private:
    void timerCallback() override
    {
        // Hidden: no animation, but a switch keeps its position, so it isn't
        // shown in a stale state (or slides) when its page opens.
        if (! isShowing())
        {
            if (isSwitch())
            {
                const auto target = button.getToggleState() ? 1.0f : 0.0f;

                if (switchAmount != target)
                {
                    switchAmount = target;
                    button.getProperties().set ("switchAmount", switchAmount);
                }
            }

            return;
        }

        hover = IlanaAnim::approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.22f, frameTicks());

        const auto on = button.getToggleState();
        auto switchMoving = false;
        const auto breathing = on && ! isSwitch() && litSeconds < breathSeconds;

        // Only a button the user switches on breathes, not one lit by a
        // preset load, undo or automation.
        if (on && ! lastOn && ticked && button.isMouseOverOrDragging())
            litSeconds = 0.0f;
        else if (breathing)
            litSeconds += frameSeconds();

        if (isSwitch())
        {
            const auto target = on ? 1.0f : 0.0f;
            switchMoving = std::abs (switchAmount - target) > 0.001f;
            switchAmount = switchMoving ? IlanaAnim::approach (switchAmount, target, 0.3f, frameTicks()) : target;
            button.getProperties().set ("switchAmount", switchAmount);

            if (switchMoving)
                button.repaint();
        }

        if (breathing || on != lastOn || switchMoving || (hover > 0.01f && hover < 0.99f))
            repaint();

        lastOn = on;
        ticked = true;
    }

    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    static constexpr float breathSeconds = 2.2f;
    float litSeconds = breathSeconds;
    float hover = 0.0f;
    float switchAmount = 0.0f;
    bool lastOn = false, ticked = false;
};

class ValueSliderControl : public juce::Component
{
public:
    ValueSliderControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 16);
        slider.setColour (juce::Slider::rotarySliderFillColourId, IlanaTheme::accent());
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

        if (auto* parameter = state.getParameter (parameterID))
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

            const auto description = describeParameter (parameterID);
            slider.setTooltip (parameter->getName (64) + (description.isNotEmpty() ? "\n" + description : ""));
        }
    }

    void resized() override { slider.setBounds (getLocalBounds()); }

    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// The height a control needs at a given width (label, then dial and value,
// or the box/button), or -1 when it takes whatever it's given.
inline int preferredControlHeight (juce::Component* item, int width)
{
    if (auto* knob = dynamic_cast<KnobControl*> (item))
    {
        if (knob->isCompact())
            return -1;

        const auto labelHeight = knob->getLabelText().isNotEmpty() ? 13 : 0;
        return labelHeight + juce::jlimit (28, 58, width) + 16;
    }

    if (dynamic_cast<ComboControl*> (item) != nullptr || dynamic_cast<ToggleControl*> (item) != nullptr)
        return 13 + 24;

    return -1;
}

// Controls side by side in equal columns. When the row is taller than its
// controls need, they sit as one band centred in it (labels on one line)
// rather than hugging the top with the spare space below.
inline void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    if (items.empty())
        return;

    const auto width = area.getWidth() / (int) items.size();
    auto band = 0;

    // (A null item is an empty column, so rows can share one grid.)
    for (auto* item : items)
    {
        if (item == nullptr)
            continue;

        const auto height = preferredControlHeight (item, width - 6);

        if (height < 0)
        {
            band = -1;
            break;
        }

        band = juce::jmax (band, height);
    }

    if (band > 0 && band + 6 < area.getHeight())
        area = area.withSizeKeepingCentre (area.getWidth(), band + 6);

    // A bare on/off switch beside knobs sits level with the dials' centres
    // (it has no label to line up with theirs); a named one keeps its name
    // on the labels' line, like a menu.
    auto hasKnob = false;
    for (auto* item : items)
        if (auto* knob = dynamic_cast<KnobControl*> (item))
            hasKnob = hasKnob || (! knob->isCompact() && knob->getLabelText().isNotEmpty());

    const auto dialDrop = hasKnob ? juce::jlimit (28, 58, width - 6) / 2 - 12 : 0;

    for (auto* item : items)
    {
        auto cell = area.removeFromLeft (width).reduced (3);

        if (item == nullptr)
            continue;

        if (auto* toggle = dynamic_cast<ToggleControl*> (item); toggle != nullptr && toggle->isSwitch() && ! toggle->isNamedSwitch() && dialDrop > 0)
            cell = cell.withTrimmedTop (dialDrop);

        item->setBounds (cell);
    }
}
