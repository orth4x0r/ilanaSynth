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

// The source pinned by clicking its chip in the bottom bar (0: none). Knobs
// it drives stay lit until the chip is clicked again. Message thread only.
inline int& pinnedModSource()
{
    static int source = 0;
    return source;
}

// One colour per modulation source, used by its chip, its card (LFO and
// envelope pages), its tabs, the matrix and the rings on knobs it moves.
// Every source has its own: macros step from yellow to amber (and carry
// their number on the knob's dot), the performance sources each take a soft
// hue of their own, and ENV 6-16 come from a fixed list that keeps clear of
// the main envelopes' and LFO 1-4's hues.
inline juce::Colour modSourceColour (int sourceIndex)
{
    // Every LFO (A and B outputs) in the LFO palette.
    if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) sourceIndex); lfo >= 0)
        return IlanaSynthAudioProcessor::lfoColour (lfo);

    if (sourceIndex >= (int) Mod::Source::Lfo1B && sourceIndex <= (int) Mod::Source::Lfo16B)
        return IlanaSynthAudioProcessor::lfoColour (sourceIndex - (int) Mod::Source::Lfo1B);

    if (const auto macro = Mod::macroIndexFor ((Mod::Source) sourceIndex); macro >= 0)
    {
        // Yellow (macro 1) to amber (macro 8), alternately lighter and
        // deeper so neighbours differ by more than the hue step.
        const auto t = (float) macro / (float) (Mod::numMacros - 1);
        return juce::Colour::fromHSV (0.155f - 0.07f * t, 0.70f + 0.12f * t + (macro % 2 == 0 ? 0.0f : 0.06f),
                                      macro % 2 == 0 ? 1.0f : 0.90f, 1.0f);
    }

    switch ((Mod::Source) sourceIndex)
    {
        case Mod::Source::AmpEnv:     return juce::Colour (0xffff5a4a); // not the accent: it would clash with FILT ENV or LFO 1
        case Mod::Source::FilterEnv:  return juce::Colour (0xffc86bff);
        case Mod::Source::FilterEnv2: return juce::Colour (0xff8f9dff);
        case Mod::Source::ModEnv:     return juce::Colour (0xff8fff3b);
        case Mod::Source::Env4:       return juce::Colour (0xff5b8cff);
        // The performance sources: soft tints, each its own hue.
        case Mod::Source::Velocity:   return juce::Colour::fromHSV (0.03f, 0.34f, 0.97f, 1.0f); // peach
        case Mod::Source::KeyTrack:   return juce::Colour::fromHSV (0.31f, 0.34f, 0.92f, 1.0f); // sage
        case Mod::Source::Random:     return juce::Colour::fromHSV (0.85f, 0.34f, 0.95f, 1.0f); // orchid
        case Mod::Source::ClockSh:    return juce::Colour::fromHSV (0.70f, 0.30f, 0.97f, 1.0f); // lavender
        case Mod::Source::ModWheel:   return juce::Colour::fromHSV (0.52f, 0.34f, 0.93f, 1.0f); // pale aqua
        case Mod::Source::Aftertouch: return juce::Colour::fromHSV (0.95f, 0.30f, 0.97f, 1.0f); // blush
        case Mod::Source::Expression: return juce::Colour::fromHSV (0.21f, 0.38f, 0.92f, 1.0f); // pale lime
        case Mod::Source::Mseg:       return juce::Colour (0xffe0e6f0);
        case Mod::Source::InputEnv:   return juce::Colour (0xffc9b79c); // sand
        case Mod::Source::VectorX:    return juce::Colour (0xff7fe0d8);
        case Mod::Source::VectorY:    return juce::Colour (0xff6fb8ff);
        default: break;
    }

    // ENV 6-16: a fixed list, deeper than the LFO pool's pastels, with hues
    // between AMP (red), MOD (lime), ENV 5 / FILT 2 (blues), FILT (violet)
    // and LFO 1-4 (rose, cyan, teal, yellow-green).
    if (sourceIndex >= (int) Mod::Source::Env6 && sourceIndex <= (int) Mod::Source::Env16)
    {
        static constexpr float hues[] { 0.05f, 0.22f, 0.32f, 0.37f, 0.42f, 0.50f, 0.70f, 0.74f, 0.84f, 0.89f, 0.975f };
        return juce::Colour::fromHSV (hues[sourceIndex - (int) Mod::Source::Env6], 0.80f, 0.86f, 1.0f);
    }

    return IlanaTheme::accent();
}

// How a knob opens and closes the editor's modulation card (ModHoverPopup,
// which sets these while it exists): the knob, its destination and title.
struct ModHoverHooks
{
    std::function<void (juce::Component&, int, const juce::String&)> show;
    std::function<void (const juce::Component&)> hide;
    // True while the mouse is on the knob's card (or dragging a row, or its
    // menu is open), so the card stays when the mouse moves onto it.
    std::function<bool (const juce::Component&)> engaged;
};

inline ModHoverHooks& modHoverHooks()
{
    static ModHoverHooks hooks;
    return hooks;
}

inline juce::String& knobClipboard()
{
    static juce::String value;
    return value;
}

// The coloured dots beside a modulated knob: one per routing into it.
// Drag a dot up or down to change that routing's depth, double-click it to
// zero the depth (as knobs and the source card do), right-click it to
// bypass or remove the routing. Hovering shows which source it is; a
// macro's dot carries the macro's number.
class ModDotStrip : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    struct Dot
    {
        int slot = -1;
        int source = 0;
        float depth = 0.0f;
        bool bypass = false;
    };

    std::function<void (int slot, float depth)> onDepthChange;
    // Double-click: the depth to zero, one undo step.
    std::function<void (int slot)> onZero;
    // The right-click menu's choices.
    std::function<void (int slot)> onRemove;
    std::function<void (int slot, bool bypass)> onBypass;
    // A depth drag's start and end (one undo step).
    std::function<void (int source)> onDragStart;
    std::function<void()> onDragEnd;

    void setDots (const std::vector<Dot>& newDots)
    {
        if (dragIndex >= 0)
            return; // keep the list stable mid-drag

        dots = newDots;
        updateTooltip();
        repaint();
    }

    static constexpr int dotSize = 10;
    static constexpr int dotPitch = 13;

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

            g.setColour (colour.withAlpha (dot.bypass ? 0.4f : 0.9f));
            g.drawEllipse (area, 1.0f);

            // A macro's number, white with a dark outline so it reads on
            // both the yellow and the dark part of the pie.
            if (const auto macro = Mod::macroIndexFor ((Mod::Source) dot.source); macro >= 0)
            {
                juce::GlyphArrangement digit;
                digit.addFittedText (juce::Font (IlanaTheme::font (8.5f, true)), juce::String (macro + 1), area.getX(), area.getY() + 0.5f,
                                     area.getWidth(), area.getHeight(), juce::Justification::centred, 1);
                juce::Path glyph;
                digit.createPath (glyph);
                g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
                g.strokePath (glyph, juce::PathStrokeType (1.6f));
                g.setColour (juce::Colours::white);
                g.fillPath (glyph);
            }
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
        if (event.mods.isPopupMenu())
        {
            showDotMenu (indexAt (event.position));
            return;
        }

        dragIndex = indexAt (event.position);

        if (dragIndex >= 0)
        {
            dragStartDepth = dots[(size_t) dragIndex].depth;

            if (onDragStart != nullptr)
                onDragStart (dots[(size_t) dragIndex].source);
        }
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

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragIndex >= 0 && onDragEnd != nullptr)
            onDragEnd();

        dragIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index >= 0 && ! event.mods.isPopupMenu() && onZero != nullptr)
            onZero (dots[(size_t) index].slot);
    }

    // Rings one routing's dot (a source dropped on a knob it already drives
    // points at the existing routing instead of adding a second).
    void flashSlot (int slot)
    {
        for (int i = 0; i < (int) dots.size(); ++i)
            if (dots[(size_t) i].slot == slot)
            {
                hoverIndex = i;
                updateTooltip();
                repaint();
            }
    }

private:
    void showDotMenu (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) dots.size()))
            return;

        const auto dot = dots[(size_t) index];
        juce::PopupMenu menu;
        menu.addSectionHeader (Mod::getSourceNames()[dot.source] + "  (" + juce::String (juce::roundToInt (dot.depth * 100.0f)) + "%)");
        menu.addItem (1, "Bypass", true, dot.bypass);
        menu.addItem (2, "Remove");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                .withTargetScreenArea (localAreaToGlobal (dotBounds (index).expanded (2.0f).getSmallestIntegerContainer())),
                            [safeThis = juce::Component::SafePointer<ModDotStrip> (this), dot] (int result)
                            {
                                if (safeThis == nullptr)
                                    return;

                                if (result == 1 && safeThis->onBypass != nullptr)
                                    safeThis->onBypass (dot.slot, ! dot.bypass);
                                else if (result == 2 && safeThis->onRemove != nullptr)
                                    safeThis->onRemove (dot.slot);
                            });
    }

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
            setTooltip ("Modulation\nDrag a dot to set its depth, double-click it to zero it, right-click to bypass or remove.");
            return;
        }

        const auto& dot = dots[(size_t) index];
        setTooltip (Mod::getSourceNames()[dot.source] + "  " + juce::String (juce::roundToInt (dot.depth * 100.0f))
                    + "%" + (dot.bypass ? "  (bypassed)" : "")
                    + "\nDrag up or down to set the depth (Shift for fine), double-click to zero it, right-click to bypass or remove.");
    }

    std::vector<Dot> dots;
    int dragIndex = -1;
    int hoverIndex = -1;
    float dragStartDepth = 0.0f;
};

// The colour a knob draws a modulation arc in: the source's own, unless it
// is too close in hue to the knob's colour, then lifted towards white.
inline juce::Colour modArcColour (juce::Colour source, juce::Colour knob)
{
    auto hueGap = std::abs (source.getHue() - knob.getHue());
    hueGap = juce::jmin (hueGap, 1.0f - hueGap);
    const auto bothColoured = source.getSaturation() > 0.3f && knob.getSaturation() > 0.3f;
    return bothColoured && hueGap < 0.06f ? source.interpolatedWith (juce::Colours::white, 0.6f) : source;
}

// MIDI learn for any automatable control (macros keep their own CC slots):
// the next controller moved drives the parameter; saved with the patch.
// Menu item IDs 4001-4003 are taken by these.
inline void addMidiLearnItems (juce::PopupMenu& menu, const IlanaSynthAudioProcessor& processor, const juce::String& parameterId)
{
    const auto cc = processor.getParamCc (parameterId);

    if (processor.getParamLearnTarget() == parameterId)
        menu.addItem (4003, "Cancel MIDI Learn  (waiting for a controller)");
    else
        menu.addItem (4001, cc >= 0 ? "MIDI Learn  (CC " + juce::String (cc) + ")" : juce::String ("MIDI Learn"));

    if (cc >= 0)
        menu.addItem (4002, "Clear MIDI  (CC " + juce::String (cc) + ")");
}

// True when the menu result was one of addMidiLearnItems' (and is done).
inline bool handleMidiLearnResult (int result, IlanaSynthAudioProcessor& processor, const juce::String& parameterId)
{
    if (result == 4001)
        processor.startParamLearn (parameterId);
    else if (result == 4002)
        processor.clearParamCc (parameterId);
    else if (result == 4003)
        processor.cancelParamLearn();
    else
        return false;

    return true;
}

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
        dotStrip.onZero = [this] (int slot)
        {
            if (processorRef != nullptr)
                processorRef->performEdit ("Zero " + Mod::getSourceNames()[(int) processorRef->readModSlot (slot).source] + " depth",
                                           [this, slot] { processorRef->setModSlotValue (slot, "amt", 0.0f); });

            refreshRoutings();
        };
        dotStrip.onRemove = [this] (int slot)
        {
            if (processorRef != nullptr)
                processorRef->performEdit ("Remove modulation", [this, slot] { processorRef->clearModSlot (slot); });

            refreshRoutings();
        };
        dotStrip.onBypass = [this] (int slot, bool bypass)
        {
            if (processorRef != nullptr)
                processorRef->performEdit (bypass ? "Bypass modulation" : "Enable modulation",
                                           [this, slot, bypass] { processorRef->setModSlotValue (slot, "byp", bypass ? 1.0f : 0.0f); });

            refreshRoutings();
        };
        dotStrip.onDragStart = [this] (int source)
        {
            if (processorRef != nullptr)
                processorRef->beginEdit (Mod::getSourceNames()[source] + " depth");
        };
        dotStrip.onDragEnd = [this]
        {
            if (processorRef != nullptr)
                processorRef->endEdit();
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
            baseTooltip = tooltip;
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
        closeModCard();

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
    // The modulation destination whose depth the knob's ring shows (0: none).
    int getRingDestination() const { return ringConfig.destination; }
    int getNumRoutings() const { return (int) routings.size(); }
    // Whether a mod slot routes this source into the knob, and whether the
    // source pinned by a chip click is one of them (the knob is lit).
    bool isDrivenBy (int source) const { return routesFrom (source); }
    bool isLitByPinnedSource() const { return pinnedModSource() != 0 && routesFrom (pinnedModSource()); }

    // Compact knobs (bottom strip) have no label or value box: the owner
    // draws those next to the knob.
    void setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        label.setVisible (! compact);
        slider.setTextBoxStyle (compact ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 64, 14);
        resized();
    }

    // The knob's role sets its largest dial (IlanaTheme::KnobSize: main,
    // small or mini); layoutRow and preferredControlHeight follow it.
    void setSizeRole (int largestDial)
    {
        maxDial = juce::jmax (IlanaTheme::KnobSize::minimum, largestDial);
        resized();
    }

    int getMaxDial() const { return maxDial; }
    // The dial's drawn size right now (the UI test checks roles with it).
    int getDialSize() const { return knobBounds.getWidth() > 0 ? juce::jmin (knobBounds.getWidth(), (int) rotaryArea().getHeight()) : 0; }

    // A knob standing in for another parameter's modulation: a synced LFO's
    // division knob shows RATE's ring and dots, and a source dropped on it
    // (or picked from its menu) routes to RATE, which still scales the
    // synced rate.
    void setModulationTarget (const juce::String& targetParameterId)
    {
        ringConfig = modRingConfigFor (targetParameterId);
        routings.clear();
        dotStrip.setDots (routings);
        refreshRoutings();
        layoutDots();
        repaint();
    }

    // Why the knob does nothing right now (EffectRules), added to its
    // tooltip; empty when it acts.
    void setInactiveNote (const juce::String& note)
    {
        if (note == inactiveNote || baseTooltip.isEmpty())
            return;

        inactiveNote = note;
        const auto tooltip = baseTooltip + (note.isNotEmpty() ? "\n(No effect now: " + note + ")" : juce::String());
        slider.setTooltip (tooltip);
        setTooltip (tooltip);
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

    ~KnobControl() override { closeModCard(); }

    // Opens the card listing this knob's sources (hover does it after a
    // short rest). Held: it stays until closeModCard (tests, snapshots).
    void openModCard (bool held = false)
    {
        modCardHeld = held;
        if (modHoverHooks().show != nullptr && ! routings.empty())
        {
            modHoverHooks().show (*this, ringConfig.destination, parameter != nullptr ? parameter->getName (40) : label.getText());
            modCardOpen = true;
        }
    }

    bool isModCardOpen() const { return modCardOpen; }

    void closeModCard()
    {
        if (modCardOpen && modHoverHooks().hide != nullptr)
            modHoverHooks().hide (*this);

        modCardOpen = false;
        modCardHeld = false;
        hoverRest = 0.0f;
        cardLeave = 0.0f;
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

        // A hovered or pinned source lights up every knob it modulates: a
        // thin ring just outside the mod arc, with no fill, so the knob's
        // label and value stay readable.
        const auto pinned = pinnedModSource();
        const auto highlighted = highlightedModSource() != 0 ? highlightedModSource() : pinned;

        if (highlighted != 0 && routesFrom (highlighted))
        {
            const auto radius = knobRadius + 3.0f;
            const juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (rotaryArea().expanded (12.0f, 1.0f).toNearestInt());
            g.setColour (modSourceColour (highlighted).withAlpha (highlighted == pinned ? 0.95f : 0.75f));
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), highlighted == pinned ? 2.0f : 1.5f);
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

        // A dark underlay keeps the arc apart from the knob's own value arc,
        // and a source whose colour is close to the knob's (a macro on an
        // OSC 1 knob: yellow on gold) is drawn paler so it still reads.
        const auto arcColour = modArcColour (modSourceColour (dominantSource),
                                             slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (lineWidth + 2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour (arcColour.withAlpha (0.92f));
        g.strokePath (arc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // A marker where the modulation currently puts the knob.
        const auto markerRadius = juce::jlimit (2.5f, 3.6f, knobRadius * 0.14f);
        const auto markerCentre = centre.getPointOnCircumference (arcRadius, angleB);
        const auto marker = juce::Rectangle<float> (markerRadius * 2.0f, markerRadius * 2.0f).withCentre (markerCentre);
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.9f));
        g.fillEllipse (marker.expanded (1.2f));
        g.setColour (arcColour);
        g.fillEllipse (marker);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (marker, 1.0f);
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
        constexpr int labelHeight = 13, valueHeight = 16;
        const auto hasLabel = label.getText().isNotEmpty();
        const auto dial = juce::jlimit (IlanaTheme::KnobSize::minimum, maxDial,
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

        // One routing per source and knob: a source that already drives
        // this knob points at its routing (dot and card) instead of adding
        // a second one.
        if (! showExistingRouting (sourceIndex))
            processorRef->performEdit ("Add modulation",
                                       [this, sourceIndex] { processorRef->assignModSlot (sourceIndex, ringConfig.destination, 0.35f); });

        refreshRoutings();
        repaint();
    }

    // The slot already routing this source into the knob, or -1.
    int findRoutingFrom (int source) const
    {
        for (const auto& dot : routings)
            if (dot.source == source)
                return dot.slot;

        return -1;
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

    bool routesFrom (int source) const { return findRoutingFrom (source) >= 0; }

    // A source that already drives this knob: ring its dot and open the
    // knob's card so its depth can be set there. False when it doesn't.
    bool showExistingRouting (int source)
    {
        refreshRoutings();
        const auto slot = findRoutingFrom (source);

        if (slot < 0)
            return false;

        dotStrip.flashSlot (slot);
        openModCard();
        return true;
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

            found.push_back ({ i, (int) slot.source, slot.depth, slot.bypass });

            if (! slot.bypass && std::abs (slot.depth) > strongest)
            {
                strongest = std::abs (slot.depth);
                dominantSource = (int) slot.source;
            }
        }

        const auto changed = found.size() != routings.size()
                             || ! std::equal (found.begin(), found.end(), routings.begin(),
                                              [] (const auto& a, const auto& b)
                                              { return a.slot == b.slot && a.source == b.source && a.bypass == b.bypass
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
                                                             + juce::String (juce::roundToInt (dot.depth * 100.0f)) + "%)");

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

        const auto isMacro = parameterId.startsWith ("macro") && ! parameterId.containsChar ('_');

        if (isMacro)
        {
            const auto macroIndex = parameterId.getTrailingIntValue() - 1;

            menu.addSeparator();
            menu.addItem (4000, "MIDI Learn  (CC " + juce::String (processorRef->getMacroCc (macroIndex)) + ")");
        }
        else if (parameter != nullptr && parameter->isAutomatable())
        {
            menu.addSeparator();
            addMidiLearnItems (menu, *processorRef, parameterId);
        }

        juce::Component::SafePointer<KnobControl> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safeThis] (int result)
                            {
                                if (safeThis == nullptr || result == 0)
                                    return;

                                auto& processor = *safeThis->processorRef;

                                // Each edit here is one named undo step.
                                const auto name = result == 1000   ? juce::String ("Clear modulation")
                                                  : result == 2000 ? "Reset " + safeThis->label.getText()
                                                  : result == 3001 ? "Paste " + safeThis->label.getText()
                                                  : result >= 5000 ? juce::String ("Remove modulation")
                                                                   : juce::String ("Add modulation");

                                if (handleMidiLearnResult (result, processor, safeThis->parameterId))
                                    return;

                                // "Modulate with" a source that already drives
                                // the knob edits that routing instead.
                                if (result > 0 && result < 1000 && safeThis->showExistingRouting (result - 1))
                                    return;

                                if (result != 3000 && result != 4000)
                                    processor.beginEdit (name);

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

                                processor.endEdit();
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

        // Resting on a modulated knob (not turning it) opens its source card.
        // It stays while the mouse is on the card, after a moment's grace to
        // cross the gap between them.
        const auto overKnob = isMouseOver (true) && ! slider.isMouseButtonDown();
        const auto cardEngaged = modCardOpen && modHoverHooks().engaged != nullptr && modHoverHooks().engaged (*this);

        if (! routings.empty() && (overKnob || cardEngaged))
        {
            cardLeave = 0.0f;

            if (! modCardOpen && overKnob && (hoverRest += frameSeconds()) >= 0.35f)
                openModCard();
        }
        else if (modCardOpen && ! modCardHeld)
        {
            if ((cardLeave += frameSeconds()) >= 0.25f)
                closeModCard();
        }
        else if (hoverRest > 0.0f)
        {
            closeModCard();
        }

        const auto highlighted = highlightedModSource() * 1000 + pinnedModSource();
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
    float hoverRest = 0.0f;
    float cardLeave = 0.0f;
    bool modCardOpen = false;
    bool modCardHeld = false;
    int lastHighlighted = 0;
    float lastModValue = 0.0f;
    float glow = 0.0f;
    float activity = 0.0f;
    double lastSliderValue = 0.0;
    bool dragHover = false;
    bool hover = false;
    bool compact = false;
    int maxDial = IlanaTheme::KnobSize::main;
    juce::String baseTooltip, inactiveNote;
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
        : parameterId (parameterID)
    {
        processorRef = dynamic_cast<IlanaSynthAudioProcessor*> (&state.processor);
        button.onPopupMenu = [this] { showMenu(); };
        button.setButtonText (labelText);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, IlanaTheme::accent().withAlpha (0.85f));
        addAndMakeVisible (button);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterID, button);

        // Every on/off is drawn as a sliding switch (a lit and a dark button
        // were easy to misread): a card's bare "ON" switch, or a named one
        // with its name centred above it, like a knob's.
        switchAmount = button.getToggleState() ? 1.0f : 0.0f;
        button.getProperties().set ("switch", true);
        button.getProperties().set ("switchAmount", switchAmount);

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

    // (Every toggle is a switch now; kept for the call sites that ask.)
    void showAsSwitch()
    {
        switchAmount = button.getToggleState() ? 1.0f : 0.0f;
        button.getProperties().set ("switch", true);
        button.getProperties().set ("switchAmount", switchAmount);
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
            g.drawText (button.getButtonText(), getLocalBounds().withHeight (13), juce::Justification::centred, true);
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

    // Right-click opens the switch's menu (MIDI learn) instead of flipping it.
    struct SwitchButton : public juce::TextButton
    {
        std::function<void()> onPopupMenu;

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu() && onPopupMenu != nullptr)
                onPopupMenu();
            else
                juce::TextButton::mouseDown (event);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            if (! (event.mods.isPopupMenu() && onPopupMenu != nullptr))
                juce::TextButton::mouseUp (event);
        }
    };

    void showMenu()
    {
        auto* parameter = processorRef != nullptr ? processorRef->apvts.getParameter (parameterId) : nullptr;

        if (parameter == nullptr || ! parameter->isAutomatable())
            return;

        juce::PopupMenu menu;
        addMidiLearnItems (menu, *processorRef, parameterId);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                            [safeThis = juce::Component::SafePointer<ToggleControl> (this)] (int result)
                            {
                                if (safeThis != nullptr && safeThis->processorRef != nullptr)
                                    handleMidiLearnResult (result, *safeThis->processorRef, safeThis->parameterId);
                            });
    }

    SwitchButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    IlanaSynthAudioProcessor* processorRef = nullptr;
    juce::String parameterId;
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

// One dimming rule for every page (UI review 4, V12 and S10). A control
// declares when it has an effect (SPEC AMT while SPECTRAL is on, DETUNE
// with more than one unison voice...), and the page calls apply() on its
// timer: a control with no effect is drawn at IlanaTheme::dimmedAlpha (its
// arc goes grey) and says why in its tooltip, but stays usable, so it can be
// set up before it is switched in. A control the page has disabled (its
// section is off) is left as the page set it.
class EffectRules
{
public:
    explicit EffectRules (const IlanaSynthAudioProcessor& p) : processor (p) {}

    using Condition = std::function<bool()>;

    // `hasEffect` decides; `why` (shown in a knob's tooltip while it doesn't
    // act) names what to change, e.g. "SPECTRAL is Off".
    void add (juce::Component& control, Condition hasEffect, const juce::String& why = {})
    {
        rules.push_back ({ &control, std::move (hasEffect), why });
    }

    // Conditions on a parameter's plain value.
    Condition isOn (const juce::String& id) const { return [this, id] { return read (id) > 0.5f; }; }
    Condition isOff (const juce::String& id) const { return [this, id] { return read (id) < 0.5f; }; }
    Condition isAbove (const juce::String& id, float threshold) const { return [this, id, threshold] { return read (id) > threshold; }; }
    Condition choiceIsNot (const juce::String& id, int index) const
    {
        return [this, id, index] { return juce::roundToInt (read (id)) != index; };
    }

    void apply()
    {
        for (auto& rule : rules)
        {
            if (! rule.control->isEnabled())
                continue;

            const auto acts = rule.hasEffect();
            const auto alpha = acts ? 1.0f : IlanaTheme::dimmedAlpha;

            if (rule.control->getAlpha() != alpha)
                rule.control->setAlpha (alpha);

            if (auto* knob = dynamic_cast<KnobControl*> (rule.control))
                knob->setInactiveNote (acts ? juce::String() : rule.why);
        }
    }

    // How many controls are dimmed by a rule right now (the UI test).
    int numInactive() const
    {
        auto count = 0;
        for (const auto& rule : rules)
            count += rule.control->isEnabled() && ! rule.hasEffect() ? 1 : 0;
        return count;
    }

private:
    float read (const juce::String& id) const
    {
        const auto* value = processor.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    struct Rule
    {
        juce::Component* control;
        Condition hasEffect;
        juce::String why;
    };

    const IlanaSynthAudioProcessor& processor;
    std::vector<Rule> rules;
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
        return labelHeight + juce::jlimit (IlanaTheme::KnobSize::minimum, knob->getMaxDial(), width) + 16;
    }

    if (dynamic_cast<ComboControl*> (item) != nullptr || dynamic_cast<ToggleControl*> (item) != nullptr)
        return 13 + 24;

    return -1;
}

// Controls side by side in equal columns. When the row is taller than its
// controls need, they sit as one band centred in it (labels on one line)
// rather than hugging the top with the spare space below. oneLabelLine keeps
// menus' and switches' names on the knobs' label line too (their boxes right
// under), for a grid read by its labels (SEQ's GENERATE).
inline void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, bool oneLabelLine = false)
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

    // Menus and switches beside full-size knobs sit with their box level
    // with the dials' centres (their names drop with them).
    auto hasKnob = false;
    auto largestDial = IlanaTheme::KnobSize::minimum;
    for (auto* item : items)
        if (auto* knob = dynamic_cast<KnobControl*> (item))
        {
            hasKnob = hasKnob || (! knob->isCompact() && knob->getLabelText().isNotEmpty());
            largestDial = juce::jmax (largestDial, knob->getMaxDial());
        }

    // (The dial is sized as KnobControl sizes it: by the cell's width or
    // height, whichever is tighter, up to its role's size; a small dial
    // barely drops anything.)
    const auto dialSize = juce::jlimit (IlanaTheme::KnobSize::minimum, largestDial, juce::jmin (width - 6, area.getHeight() - 6 - 13 - 16));
    const auto dialDrop = hasKnob && ! oneLabelLine ? juce::jmax (0, dialSize / 2 - 12) : 0;

    for (auto* item : items)
    {
        auto cell = area.removeFromLeft (width).reduced (3);

        if (item == nullptr)
            continue;

        if (dialDrop > 0 && (dynamic_cast<ToggleControl*> (item) != nullptr || dynamic_cast<ComboControl*> (item) != nullptr))
            cell = cell.withTrimmedTop (dialDrop);

        item->setBounds (cell);
    }
}
