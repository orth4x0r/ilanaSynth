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

// A small tag in a card's lower-left corner naming what it drives.
inline void paintTargetTag (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour colour)
{
    if (text.isEmpty())
        return;

    const auto font = juce::Font (IlanaTheme::font (9.5f, true));
    const auto width = juce::jmin (area.getWidth(), juce::GlyphArrangement::getStringWidth (font, text) + 12.0f);
    const auto tag = juce::Rectangle<float> (area.getX(), area.getBottom() - 13.0f, width, 13.0f);

    g.setColour (juce::Colour (0xff111115).withAlpha (0.85f));
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

inline juce::Colour modSourceColour (int sourceIndex)
{
    if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) sourceIndex); lfo >= 4)
        return IlanaSynthAudioProcessor::lfoColour (lfo);

    switch (sourceIndex)
    {
        case 1:  return juce::Colour (0xffff8a3b);
        case 2:  return juce::Colour (0xff35c8ff);
        case 3:  return juce::Colour (0xff8fff3b);
        case 4:  return juce::Colour (0xffff4fd8);
        case 5:  return juce::Colour (0xff5b8cff);
        case 6:
        case 7:
        case 8:  return juce::Colour (0xffbbbbbb);
        case 9:
        case 10:
        case 11: return juce::Colour (0xffb28aff);
        case 12:
        case 13:
        case 14:
        case 15: return juce::Colour (0xffffd447);
        case 16: return juce::Colour (0xffbbbbbb);
        case 17: return juce::Colour (0xff6fe3c1);
        case 18: return juce::Colour (0xffffd447);
        case 19: return juce::Colour (0xffb28aff);
        case 20: return juce::Colour (0xff6fe3c1);
        case 21: return juce::Colour (0xffe3a56f);
        default: return IlanaTheme::accent();
    }
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
            g.setColour (juce::Colour (0xff141418));
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
                    public IlanaAnim::PageAnimated,
                    private juce::Timer
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
        label.setFont (IlanaTheme::font (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
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

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            showModMenu();
    }

    juce::Slider& getSlider() { return slider; }
    void setLabelText (const juce::String& text) { label.setText (text, juce::dontSendNotification); }
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

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

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

        if (! compact)
            label.setBounds (area.removeFromTop (13));

        knobBounds = area;
        slider.setBounds (area);
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
        const auto targetGlow = hover ? 1.0f : 0.0f;
        glow += (targetGlow - glow) * 0.22f;
        appear = juce::jmin (1.0f, appear + 0.12f);
        slider.setAlpha (appear);
        label.setAlpha (appear);
        dotStrip.setAlpha (appear);

        const auto value = slider.getValue();

        if (std::abs (value - lastSliderValue) > 1.0e-6)
        {
            lastSliderValue = value;
            activity = 1.0f;
        }

        activity *= 0.88f;

        if (processorRef == nullptr || ringConfig.destination == 0)
        {
            if (glow > 0.01f || activity > 0.01f || appear < 0.999f || isMouseOver())
                repaint();

            return;
        }

        if (++routingCheck >= 6)
        {
            routingCheck = 0;
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
        else if (glow > 0.01f || activity > 0.01f || appear < 0.999f || isMouseOver())
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
    int routingCheck = 0;
    int lastHighlighted = 0;
    float lastModValue = 0.0f;
    float glow = 0.0f;
    float activity = 0.0f;
    double lastSliderValue = 0.0;
    bool dragHover = false;
    bool hover = false;
    bool compact = false;
    float appear = 1.0f;
};

class ComboControl : public juce::Component,
                     public juce::SettableTooltipClient,
                     public IlanaAnim::PageAnimated,
                     private juce::Timer
{
public:
    ComboControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID,
                  const juce::String& labelText)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setFont (IlanaTheme::font (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
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

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

private:
    void timerCallback() override
    {
        hover = IlanaAnim::approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.22f);
        appear = juce::jmin (1.0f, appear + 0.12f);

        combo.setAlpha (appear);
        label.setAlpha (appear);

        const auto scale = 1.0f + 0.05f * hover;
        combo.setTransform (juce::AffineTransform::scale (scale, scale,
                                                          (float) combo.getX() + (float) combo.getWidth() * 0.5f,
                                                          (float) combo.getY() + (float) combo.getHeight() * 0.5f));

        if (hover > 0.01f || appear < 0.999f)
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
    float appear = 1.0f;
    float hover = 0.0f;
};

class ToggleControl : public juce::Component,
                      public juce::SettableTooltipClient,
                      public IlanaAnim::PageAnimated,
                      private juce::Timer
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

    void paint (juce::Graphics& g) override
    {
        if (hover > 0.01f)
        {
            g.setColour (IlanaTheme::accent().withAlpha (0.18f * hover));
            g.fillRoundedRectangle (button.getBounds().toFloat().expanded (2.0f), 5.0f);
        }
    }

    // On/off reads from the button itself (lit accent when on); a soft
    // pulse under a lit button keeps it alive.
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! button.getToggleState())
            return;

        const auto pulse = 0.5f + 0.5f * std::sin (pulsePhase);
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.05f * pulse));
        g.fillRoundedRectangle (button.getBounds().toFloat().reduced (2.0f).withTrimmedTop ((float) button.getHeight() * 0.55f), 3.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (13);
        button.setBounds (area.removeFromTop (juce::jmin (24, juce::jmax (16, area.getHeight()))));
    }

    void visibilityChanged() override
    {
        if (isVisible())
            appear = 0.0f;
    }

    void replayAppear() override { appear = 0.0f; }

private:
    void timerCallback() override
    {
        pulsePhase += 0.16f;
        hover = IlanaAnim::approach (hover, isMouseOver() ? 1.0f : 0.0f, 0.22f);
        appear = juce::jmin (1.0f, appear + 0.12f);

        const auto scale = 1.0f + 0.05f * hover;
        button.setTransform (juce::AffineTransform::scale (scale, scale,
                                                           (float) button.getX() + (float) button.getWidth() * 0.5f,
                                                           (float) button.getY() + (float) button.getHeight() * 0.5f));
        button.setAlpha (appear);

        const auto on = button.getToggleState();

        if (on || on != lastOn || hover > 0.01f || appear < 0.999f || isMouseOver())
            repaint();

        lastOn = on;
    }

    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    float pulsePhase = 0.0f;
    float appear = 1.0f;
    float hover = 0.0f;
    bool lastOn = false;
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

inline void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    if (items.empty())
        return;

    const auto width = area.getWidth() / (int) items.size();

    for (auto* item : items)
        item->setBounds (area.removeFromLeft (width).reduced (3));
}
