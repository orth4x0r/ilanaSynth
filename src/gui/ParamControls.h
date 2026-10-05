#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"
#include "ParamInfo.h"
#include "ModulePool.h"

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
        { "fm_1to2", D::Fm1to2, 1.0f }, { "fm_1to3", D::Fm1to3, 1.0f }, { "fm_2to3", D::Fm2to3, 1.0f },
        { "fm_3to1", D::Fm3to1, 1.0f }, { "fm_3to2", D::Fm3to2, 1.0f },
        { "fm_fb2", D::Fm2Feedback, 1.0f }, { "fm_fb3", D::Fm3Feedback, 1.0f },
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
// "first target +N more". Empty when it drives nothing.
inline juce::String describeModTargets (const IlanaSynthAudioProcessor& processor, Mod::Source source,
                                        juce::StringArray targets = {})
{
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.isActive() && (routing.source == source || routing.aux == source))
            targets.addIfNotAlreadyThere (ModNames::destination (routing.destination));
    }

    if (targets.isEmpty())
        return {};

    // "+2 more", not "+2", which read as a depth beside the matrix's "+70%"
    // (UI review 8, S8-18).
    return targets[0] + (targets.size() > 1 ? "  +" + juce::String (targets.size() - 1) + " more" : juce::String());
}

// A small tag naming what a card drives, at the left of `area`'s bottom.
// A narrow card passes `below`, a row under its title: a tag that would be
// cut short on the title line ("Filte...") moves there whole.
inline void paintTargetTag (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour colour,
                            juce::Rectangle<float> below = {})
{
    const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
    const auto wanted = juce::GlyphArrangement::getStringWidth (font, text) + 12.0f;

    if (wanted > area.getWidth() && ! below.isEmpty())
        area = below;

    // A tag that still doesn't fit is left out rather than squeezed: the
    // card's tooltip names its targets (review 8, V8-29).
    if (text.isEmpty() || wanted > area.getWidth())
        return;

    const auto width = juce::jmin (area.getWidth(), wanted);
    const auto tag = juce::Rectangle<float> (area.getX(), area.getBottom() - 15.0f, width, 15.0f);

    g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
    g.fillRoundedRectangle (tag, 7.5f);
    g.setColour (colour.withAlpha (0.9f));
    g.setFont (font);
    IlanaTheme::drawFitted (g, text, tag.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
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

// The source a drag under way carries ("modsource:N" from a chip, a macro,
// an LFO or envelope card), 0 while none is dragged. Knobs poll it to show
// every drop target as soon as a drag starts (review 8, V8-10).
inline int modSourceBeingDragged (juce::Component& component)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (&component);

    if (container == nullptr || ! container->isDragAndDropActive())
        return 0;

    const auto description = container->getCurrentDragDescription().toString();
    return description.startsWith ("modsource:") ? description.fromFirstOccurrenceOf (":", false, false).getIntValue() : 0;
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
        // The Operator Env's: the FM page's amber, and a lighter one.
        case Mod::Source::OpLfo:      return juce::Colour (0xffe3a56f);
        case Mod::Source::OpPitchEnv: return juce::Colour (0xfff0c99a);
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
    // The same card for a source (a macro in the strip, a chip): where it
    // goes, with a warning for targets whose module is off.
    std::function<void (juce::Component&, int source)> showSource;
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

// The badges beside a modulated knob: one per routing into it, a disc in
// the source's colour (a macro's carries its number). They are the legend
// for the knob's rings, shown while the knob is hovered (V7-27), and
// controls too:
// drag one up or down to set that routing's depth, double-click it to zero
// the depth (as knobs, rings and the source card do), right-click it to
// bypass or remove the routing. More routings than fit show as a "+N"
// badge, which opens the knob's source card with all of them.
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
        bool bipolar = false;
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
    // The "+N" badge: show every routing (the knob's source card).
    std::function<void()> onShowAll;

    void setDots (const std::vector<Dot>& newDots)
    {
        if (dragIndex >= 0)
            return; // keep the list stable mid-drag

        dots = newDots;
        updateTooltip();
        repaint();
    }

    // At least 14 px, with a 20 px target (UI review 5/6, S5-5).
    static constexpr int dotSize = 14;
    static constexpr int dotPitch = 16;
    static constexpr int stripWidth = 18;

    // How many badges the strip has room for; past that, "+N".
    void setMaxVisible (int count)
    {
        maxVisible = juce::jmax (1, count);
        repaint();
    }

    int getNumShown() const { return numShown(); }
    bool hasOverflow() const { return (int) dots.size() > maxVisible; }
    int getPreferredHeight() const { return juce::jmax (1, numShown() + (hasOverflow() ? 1 : 0)) * dotPitch; }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < numShown(); ++i)
        {
            const auto& dot = dots[(size_t) i];
            const auto area = dotBounds (i);
            const auto colour = modSourceColour (dot.source);
            const auto active = i == dragIndex || i == hoverIndex;

            g.setColour (colour.withAlpha (active ? 0.35f : 0.16f));
            g.fillEllipse (area.expanded (active ? 2.0f : 1.0f));

            // A plain disc in the source's colour: the ring shows the depth,
            // the badge only which source it is (review 7, V7-27: no pie).
            g.setColour (IlanaTheme::Ui::bg);
            g.fillEllipse (area);
            g.setColour (colour.withAlpha (dot.bypass ? 0.3f : 0.9f));
            g.fillEllipse (area.reduced (1.0f));
            g.setColour (colour.withAlpha (dot.bypass ? 0.4f : 0.95f));
            g.drawEllipse (area.reduced (0.5f), 1.2f);

            // A macro's number, white with a dark outline so it reads on
            // its colour.
            if (const auto macro = Mod::macroIndexFor ((Mod::Source) dot.source); macro >= 0)
                paintOutlinedText (g, juce::String (macro + 1), area, IlanaTheme::TextSize::tiny);
        }

        if (hasOverflow())
        {
            const auto area = dotBounds (numShown());
            const auto active = hoverIndex == overflowIndex;
            g.setColour (IlanaTheme::Ui::raised.interpolatedWith (juce::Colours::white, active ? 0.15f : 0.0f));
            g.fillEllipse (area);
            g.setColour (IlanaTheme::Ui::text2);
            g.drawEllipse (area.reduced (0.5f), 1.0f);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            IlanaTheme::drawFitted (g, "+" + juce::String ((int) dots.size() - numShown()), area.expanded (1.0f, 0.0f).translated (0.0f, 0.5f).toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index != hoverIndex)
        {
            hoverIndex = index;
            setMouseCursor (index >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                       : index == overflowIndex ? juce::MouseCursor::PointingHandCursor
                                                                : juce::MouseCursor::NormalCursor);
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
        const auto index = indexAt (event.position);

        if (index == overflowIndex)
        {
            if (onShowAll != nullptr)
                onShowAll();
            return;
        }

        if (event.mods.isPopupMenu())
        {
            showDotMenu (index);
            return;
        }

        dragIndex = index;

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

    // Rings one routing's badge (a source dropped on a knob it already
    // drives points at the existing routing instead of adding a second).
    void flashSlot (int slot)
    {
        for (int i = 0; i < numShown(); ++i)
            if (dots[(size_t) i].slot == slot)
            {
                hoverIndex = i;
                updateTooltip();
                repaint();
            }
    }

    // White text with a dark outline (a macro's number on a yellow pie).
    static void paintOutlinedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float size)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (juce::Font (IlanaTheme::font (size, true)), text, area.getX(), area.getY() + 0.5f,
                              area.getWidth(), area.getHeight(), juce::Justification::centred, 1);
        juce::Path glyph;
        glyphs.createPath (glyph);
        g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
        g.strokePath (glyph, juce::PathStrokeType (1.8f));
        g.setColour (juce::Colours::white);
        g.fillPath (glyph);
    }

private:
    static constexpr int overflowIndex = -2;

    int numShown() const
    {
        const auto count = (int) dots.size();
        return count > maxVisible ? maxVisible - 1 : count;
    }

    void showDotMenu (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) dots.size()))
            return;

        const auto dot = dots[(size_t) index];
        juce::PopupMenu menu;
        menu.addSectionHeader (ModNames::source (dot.source) + "  (" + juce::String (juce::roundToInt (dot.depth * 100.0f)) + "%)");
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

    // A badge, overflowIndex for "+N", or -1. Each takes a 20 px target
    // (its own row's share of it where they touch).
    int indexAt (juce::Point<float> position) const
    {
        const auto rows = numShown() + (hasOverflow() ? 1 : 0);
        const auto row = (int) std::floor (position.y / (float) dotPitch);

        if (! juce::isPositiveAndBelow (row, rows) || std::abs (position.x - (float) getWidth() * 0.5f) > 10.0f)
            return -1;

        return row < numShown() ? row : overflowIndex;
    }

    void updateTooltip()
    {
        const auto index = hoverIndex >= 0 ? hoverIndex : dragIndex;

        if (hoverIndex == overflowIndex)
        {
            setTooltip ("Show all " + juce::String ((int) dots.size()) + " routings into this knob");
            return;
        }

        if (! juce::isPositiveAndBelow (index, (int) dots.size()))
        {
            setTooltip ("Modulation\nDrag a badge (or the knob's ring) to set its depth, double-click it to zero it, right-click to bypass or remove.");
            return;
        }

        const auto& dot = dots[(size_t) index];
        setTooltip (ModNames::source (dot.source) + "  " + (dot.depth >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (dot.depth * 100.0f))
                    + "%" + (dot.bipolar ? "  (swings both ways)" : "") + (dot.bypass ? "  (bypassed)" : "")
                    + "\nDrag up or down to set the depth (Shift for fine), double-click to zero it, right-click to bypass or remove.");
    }

    std::vector<Dot> dots;
    int dragIndex = -1;
    int hoverIndex = -1;
    int maxVisible = 3;
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

// A one-line note that floats over a control for a moment and fades (a
// source dropped on a knob that can't take it, I7-5). One at a time; it
// lives in the editor's scaled content and goes with it.
class FloatingNote : public juce::Component,
                     private juce::ComponentListener,
                     private juce::Timer
{
public:
    static void show (juce::Component& anchor, const juce::String& text)
    {
        // The anchor's ancestor just under the top level: the editor's
        // content, drawn at the zoom.
        juce::Component* host = &anchor;
        while (host->getParentComponent() != nullptr && host->getParentComponent()->getParentComponent() != nullptr)
            host = host->getParentComponent();
        if (host == &anchor)
            return;

        delete current().getComponent();
        auto* note = new FloatingNote (text, *host);
        current() = note;

        const auto font = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto width = juce::GlyphArrangement::getStringWidthInt (font, text) + 24;
        const auto at = host->getLocalArea (&anchor, anchor.getLocalBounds());
        auto bounds = juce::Rectangle<int> (width, 26).withCentre ({ at.getCentreX(), at.getY() - 15 });
        bounds = bounds.constrainedWithin (host->getLocalBounds().reduced (4));
        note->setBounds (bounds);
    }

    // The note on screen now, for the tests (empty when none).
    static juce::String shownText() { return current() != nullptr ? current()->text : juce::String(); }

    ~FloatingNote() override
    {
        if (host != nullptr)
            host->removeComponentListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (text, getLocalBounds(), juce::Justification::centred, false);
    }

private:
    FloatingNote (const juce::String& textIn, juce::Component& hostIn) : text (textIn), host (&hostIn)
    {
        setInterceptsMouseClicks (false, false);
        host->addAndMakeVisible (this);
        host->addComponentListener (this);
        toFront (false);
        startTimerHz (30);
    }

    static juce::Component::SafePointer<FloatingNote>& current()
    {
        static juce::Component::SafePointer<FloatingNote> note;
        return note;
    }

    void timerCallback() override
    {
        life -= 1.0f / 30.0f;
        setAlpha (juce::jlimit (0.0f, 1.0f, life / 0.4f));
        if (life <= 0.0f)
        {
            stopTimer();
            setVisible (false);
            juce::MessageManager::callAsync ([note = juce::Component::SafePointer<FloatingNote> (this)] { delete note.getComponent(); });
        }
    }

    void componentBeingDeleted (juce::Component&) override
    {
        host->removeComponentListener (this);
        host = nullptr;
        stopTimer();
        delete this;
    }

    juce::String text;
    juce::Component* host = nullptr;
    float life = 2.2f;
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
        dotStrip.onZero = [this] (int slot)
        {
            if (processorRef != nullptr)
                processorRef->performEdit ("Zero " + ModNames::source ((int) processorRef->readModSlot (slot).source) + " depth",
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
                processorRef->beginEdit (ModNames::source (source) + " depth");
        };
        dotStrip.onDragEnd = [this]
        {
            if (processorRef != nullptr)
                processorRef->endEdit();
        };
        dotStrip.onShowAll = [this] { openModCard (true); };
        addChildComponent (dotStrip);
        // Over the slider, so a press on a ring sets that routing's depth
        // (one inside it still turns the knob).
        addChildComponent (ringOverlay);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterID, slider);

        parameter = state.getParameter (parameterID);

        if (parameter != nullptr)
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

            applyModulatableMark();
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
    juce::Label& getNameLabel() { return label; }
    bool isCompact() const { return compact; }
    const juce::String& getParameterId() const { return parameterId; }
    // The modulation destination whose depth the knob's ring shows (0: none).
    int getRingDestination() const { return ringConfig.destination; }
    int getNumRoutings() const { return (int) routings.size(); }
    // The depth rings (one per routing, up to three), for the tests: how
    // many, a point on one (0..1 along its sweep, local to the knob), the
    // component that takes presses on them, and the badge strip.
    int getNumRings() const { return numRings(); }
    juce::Point<float> getRingPoint (int ring, float proportion) const
    {
        const auto angle = ringStart + (ringEnd - ringStart) * proportion;
        return dialCentre().getPointOnCircumference (ringRadius (ring), angle);
    }
    juce::Component& getRingOverlay() { return ringOverlay; }
    // The drawn dial's radius and a ring's, about the dial's centre.
    float getDialRadius() const { return dialRadius(); }
    float getRingRadius (int ring) const { return ringRadius (ring); }
    juce::Point<float> getDialCentre() const { return dialCentre(); }
    // (The timer skips knobs that aren't on screen, as in the offscreen tests.)
    void syncRoutings() { refreshRoutings(); }
    ModDotStrip& getDotStrip() { return dotStrip; }
    // Shows the badges as a hover does (the tests); false leaves them to the mouse.
    void setBadgesShown (bool shown)
    {
        badgesForced = shown;
        badgesShown = shown && ! routings.empty();
        layoutDots();
    }
    bool areBadgesShown() const { return dotStrip.isVisible(); }
    // Whether a mod slot routes this source into the knob, and whether the
    // source pinned by a chip click is one of them (the knob is lit).
    bool isDrivenBy (int source) const { return routesFrom (source); }
    // The source a drag under way carries, as the knob last saw it (V8-10;
    // the tests set it with showDragTarget).
    int getShownDragSource() const { return dragSource; }
    void showDragTarget (int source) { dragSource = source; repaint(); }
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
        applyModulatableMark();
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
            modHoverHooks().show (*this, ringConfig.destination, ModNames::destination (ringConfig.destination));
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
        // thin halo just outside the knob's rings, with no fill, so the
        // knob's label and value stay readable.
        const auto pinned = pinnedModSource();
        const auto highlighted = highlightedModSource() != 0 ? highlightedModSource() : pinned;

        if (highlighted != 0 && routesFrom (highlighted))
        {
            const auto radius = outerRingRadius() + 2.5f;
            const juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (rotaryArea().expanded (12.0f, 1.0f).toNearestInt());
            g.setColour (modSourceColour (highlighted).withAlpha (highlighted == pinned ? 0.95f : 0.75f));
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (dialCentre()), highlighted == pinned ? 2.0f : 1.5f);
        }

        // While a source is dragged, every knob says whether it takes it:
        // a target ring in the source's colour, or dimmed (V8-10). The one
        // under the mouse lights up fully (below).
        if (dragSource != 0 && ! dragHover && ! sourceKnob)
        {
            if (isModulatable())
            {
                const auto radius = outerRingRadius() + 2.5f;
                const juce::Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (rotaryArea().expanded (12.0f, 1.0f).toNearestInt());
                g.setColour (modSourceColour (dragSource).withAlpha (0.5f));
                g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (dialCentre()), 1.5f);
            }
            else
            {
                g.setColour (IlanaTheme::Ui::bg.withAlpha (0.45f));
                g.fillRoundedRectangle (knobBounds.toFloat().reduced (2.0f), 6.0f);
            }
        }

        if (dragHover)
        {
            if (isModulatable() && dragSource != 0)
            {
                const auto radius = outerRingRadius() + 2.5f;
                const juce::Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (rotaryArea().expanded (12.0f, 1.0f).toNearestInt());
                g.setColour (modSourceColour (dragSource));
                g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (dialCentre()), 2.5f);
            }

            // A knob that can't take the source greys and is struck through.
            const auto area = knobBounds.toFloat().reduced (2.0f);
            g.setColour (isModulatable() ? juce::Colours::white.withAlpha (0.16f) : IlanaTheme::Ui::bg.withAlpha (0.55f));
            g.fillRoundedRectangle (area, 6.0f);
            if (! isModulatable())
            {
                const auto cross = juce::Rectangle<float> (16.0f, 16.0f).withCentre (rotaryArea().getCentre());
                g.setColour (IlanaTheme::Ui::text3);
                g.drawEllipse (cross, 1.6f);
                g.drawLine ({ cross.getBottomLeft() + juce::Point<float> (3.0f, -3.0f), cross.getTopRight() + juce::Point<float> (-3.0f, 3.0f) }, 1.6f);
            }
        }

        paintRings (g, highlighted);
    }

    // The knob's modulation rings (Vital and Serum 2 style): one concentric
    // ring per routing, outward from just outside the value arc, in the
    // source's colour. Each spans the range its routing can sweep from the
    // knob's value (both sides for a source that swings both ways), shows
    // what it adds right now as a brighter stretch with a white dot, and
    // ends in a handle at full depth. Drag a ring to set its depth.
    void paintRings (juce::Graphics& g, int highlighted)
    {
        const auto count = numRings();
        const auto knobRadius = knobRadiusFor (knobBounds);

        if (count == 0 || knobRadius < 8.0f)
            return;

        const auto centre = dialCentre();
        const auto knobColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto baseNorm = (float) juce::jlimit (0.0, 1.0, slider.valueToProportionOfLength (slider.getValue()));
        const auto angleOf = [] (float norm) { return ringStart + juce::jlimit (0.0f, 1.0f, norm) * (ringEnd - ringStart); };

        // The outer rings pass behind the knob's name, not over it.
        const juce::Graphics::ScopedSaveState state (g);
        if (label.isVisible() && label.getText().isNotEmpty())
        {
            const auto font = label.getFont();
            const auto width = (float) juce::GlyphArrangement::getStringWidthInt (font, label.getText()) + 6.0f;
            // (4 px of air under the name: a ring's 2.5 px stroke used to touch it, V9-18.)
            g.excludeClipRegion (label.getBounds().withSizeKeepingCentre (juce::roundToInt (width), label.getHeight()).withTrimmedBottom (-4));
        }

        for (int i = count - 1; i >= 0; --i)
        {
            const auto& dot = routings[(size_t) i];
            const auto radius = ringRadius (i);
            const auto hot = i == hoveredRing || i == draggedRing;
            const auto tight = count > 1 && ringRadius (1) - ringRadius (0) < 3.0f;
            const auto width = (tight ? 1.6f : 2.0f) + (hot ? 1.0f : 0.0f);
            const auto quiet = dot.bypass || (highlighted != 0 && highlighted != dot.source);
            const auto colour = modArcColour (modSourceColour (dot.source), knobColour);
            const auto depth = dot.depth * ringConfig.scale;
            const auto low = dot.bipolar ? baseNorm - std::abs (depth) : juce::jmin (baseNorm, baseNorm + depth);
            const auto high = dot.bipolar ? baseNorm + std::abs (depth) : juce::jmax (baseNorm, baseNorm + depth);

            juce::Path range;
            const auto a = angleOf (low), b = angleOf (high);

            // A routing with no depth (or nothing left to sweep) still shows
            // as a short tick at the knob's value, so it can be grabbed.
            if (b - a < 0.06f)
                range.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, (a + b) * 0.5f - 0.03f, (a + b) * 0.5f + 0.03f, true);
            else
                range.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, a, b, true);

            const auto rounded = juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
            g.strokePath (range, juce::PathStrokeType (width + (tight ? 1.0f : 1.6f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (colour.withAlpha (quiet ? 0.22f : (hot ? 0.75f : 0.55f)));
            g.strokePath (range, rounded);

            if (dot.bypass)
                continue;

            // What it adds now: from the knob's value to the live position.
            const auto live = juce::isPositiveAndBelow (i, (int) liveValues.size()) ? liveValues[(size_t) i] : 0.0f;
            const auto liveAngle = angleOf (baseNorm + live * ringConfig.scale);
            const auto baseAngle = angleOf (baseNorm);

            if (std::abs (liveAngle - baseAngle) > 0.01f)
            {
                juce::Path now;
                now.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (baseAngle, liveAngle),
                                   juce::jmax (baseAngle, liveAngle), true);
                g.setColour (colour.withAlpha (quiet ? 0.4f : 1.0f));
                g.strokePath (now, rounded);
            }

            // The depth handle, then the live dot.
            const auto handleAt = centre.getPointOnCircumference (radius, angleOf (baseNorm + depth));
            const auto handle = juce::Rectangle<float> (hot ? 6.5f : 5.0f, hot ? 6.5f : 5.0f).withCentre (handleAt);
            g.setColour (IlanaTheme::Ui::bg);
            g.fillEllipse (handle.expanded (1.0f));
            g.setColour (colour.withAlpha (quiet ? 0.5f : 1.0f));
            g.fillEllipse (handle);

            if (! quiet && std::abs (liveAngle - baseAngle) > 0.01f)
            {
                const auto dotAt = centre.getPointOnCircumference (radius, liveAngle);
                g.setColour (juce::Colours::white.withAlpha (0.95f));
                g.fillEllipse (juce::Rectangle<float> (3.2f, 3.2f).withCentre (dotAt));
            }
        }
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

    // A knob that can't be modulated still takes the drag, to say so: it
    // greys while a source hovers it and a dropped source leaves a note.
    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return processorRef != nullptr && details.description.toString().startsWith ("modsource:");
    }

    // Whether a source can be routed here (else the knob is drawn with a
    // dashed track and refuses drops, I7-5).
    bool isModulatable() const { return ringConfig.destination != 0; }

    // A knob that is itself a source (a macro): no dotted track.
    void setIsSourceKnob()
    {
        sourceKnob = true;
        applyModulatableMark();
    }

    // The note a refused drop leaves ("OSC 1 › Unison can't be modulated").
    juce::String refusalText() const { return displayName() + " can't be modulated"; }

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

        if (! isModulatable())
        {
            FloatingNote::show (*this, refusalText());
            repaint();
            return;
        }

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
    // The rings' geometry: the value arc's sweep, the first ring a little
    // outside the drawn dial (the look-and-feel's arc), the rest 3.5 px
    // apart. Up to three rings; past that the badges (and their "+N") list
    // every routing.
    static constexpr float ringStart = juce::MathConstants<float>::pi * 1.2f;
    static constexpr float ringEnd = juce::MathConstants<float>::pi * 2.8f;
    static constexpr float ringPitch = 3.5f, ringGap = 3.0f;
    static constexpr int maxRings = 3;

    int numRings() const { return juce::jmin ((int) routings.size(), maxRings); }

    // Where the look-and-feel draws the dial: the slider's rotary bounds
    // (above its value box), less its 4 px inset, radius 14-30.
    juce::Rectangle<float> dialBounds() const
    {
        auto& s = const_cast<juce::Slider&> (slider);
        return s.getLookAndFeel().getSliderLayout (s).sliderBounds.toFloat().translated ((float) slider.getX(), (float) slider.getY()).reduced (4.0f);
    }
    juce::Point<float> dialCentre() const { return dialBounds().getCentre(); }
    float dialRadius() const
    {
        const auto area = dialBounds();
        return juce::jlimit (14.0f, 30.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f);
    }

    // A knob in a narrow cell has little room either side of its dial: the
    // rings close up a little (never onto each other or the value arc)
    // rather than run off the knob's edge.
    float ringRadius (int index) const
    {
        const auto count = numRings();
        const auto centreX = dialCentre().x;
        const auto room = juce::jmin (centreX, (float) getWidth() - centreX) - 2.5f; // (the stroke and its outline)
        const auto dial = dialRadius();
        auto base = dial + ringGap;
        auto pitch = ringPitch;

        if (count > 1)
            pitch = juce::jlimit (2.5f, ringPitch, (room - base) / (float) (count - 1));

        base = juce::jmax (dial + 1.5f, juce::jmin (base, room - pitch * (float) (count - 1)));
        return base + (float) index * pitch;
    }
    float outerRingRadius() const { return ringRadius (juce::jmax (0, numRings() - 1)) + 1.0f; }

    // The ring under a point (local to the knob), or -1: within the ring band
    // and the arc's sweep (the gap at the bottom stays the knob's).
    int ringAt (juce::Point<float> position) const
    {
        const auto count = numRings();

        if (count == 0 || knobRadiusFor (knobBounds) < 12.0f)
            return -1;

        const auto centre = dialCentre();
        const auto distance = position.getDistanceFrom (centre);

        // A source pinned (or hovered) in the chip bar owns the whole band,
        // at least 8 px, and with Alt held the whole knob (S7-33).
        const auto focus = highlightedModSource() != 0 ? highlightedModSource() : pinnedModSource();
        auto focusRing = -1;
        for (int i = 0; i < count && focus != 0; ++i)
            if (routings[(size_t) i].source == focus)
                focusRing = i;

        if (focusRing >= 0 && juce::ModifierKeys::currentModifiers.isAltDown() && distance <= ringRadius (count - 1) + 4.0f)
            return focusRing;

        const auto inner = ringRadius (0) - (focusRing >= 0 ? 4.0f : 2.5f);
        const auto outer = ringRadius (count - 1) + (focusRing >= 0 ? 4.0f : 3.0f);
        if (distance < inner || distance > outer)
            return -1;

        // JUCE's angles: 0 at 12 o'clock, clockwise.
        auto angle = std::atan2 (position.x - centre.x, centre.y - position.y);
        if (angle < 0.0f)
            angle += juce::MathConstants<float>::twoPi;
        if (angle > juce::MathConstants<float>::pi * 0.8f && angle < juce::MathConstants<float>::pi * 1.2f)
            return -1;

        if (focusRing >= 0)
            return focusRing;

        auto nearest = 0;
        for (int i = 1; i < count; ++i)
            if (std::abs (distance - ringRadius (i)) < std::abs (distance - ringRadius (nearest)))
                nearest = i;

        return nearest;
    }

    // Catches presses on the rings (over the slider); the rest of the knob
    // passes through to the slider. Drag a ring up or right to deepen its
    // routing (Shift for fine), double-click it to zero the depth,
    // right-click to bypass or remove it.
    class RingOverlay : public juce::Component,
                        public juce::SettableTooltipClient
    {
    public:
        explicit RingOverlay (KnobControl& ownerIn) : owner (ownerIn) { setRepaintsOnMouseActivity (false); }

        bool hitTest (int x, int y) override
        {
            return owner.ringAt (owner.getLocalPoint (this, juce::Point<int> (x, y)).toFloat()) >= 0;
        }

        void mouseMove (const juce::MouseEvent& event) override { setHovered (ringUnder (event)); }
        void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

        void mouseDown (const juce::MouseEvent& event) override
        {
            owner.closeModCard();
            const auto ring = ringUnder (event);

            if (! juce::isPositiveAndBelow (ring, (int) owner.routings.size()))
                return;

            if (event.mods.isPopupMenu())
            {
                showMenu (ring);
                return;
            }

            owner.draggedRing = ring;
            dragSlot = owner.routings[(size_t) ring].slot;
            dragStartDepth = owner.routings[(size_t) ring].depth;

            if (owner.dotStrip.onDragStart != nullptr)
                owner.dotStrip.onDragStart (owner.routings[(size_t) ring].source);
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (owner.draggedRing < 0)
                return;

            const auto fine = event.mods.isShiftDown() ? 0.2f : 1.0f;
            const auto travel = (float) (event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY());
            const auto depth = juce::jlimit (-1.0f, 1.0f, dragStartDepth + travel * 0.006f * fine);

            if (juce::isPositiveAndBelow (owner.draggedRing, (int) owner.routings.size()))
                owner.routings[(size_t) owner.draggedRing].depth = depth;

            if (owner.dotStrip.onDepthChange != nullptr)
                owner.dotStrip.onDepthChange (dragSlot, depth);

            updateTooltip (owner.draggedRing);
            owner.repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (owner.draggedRing >= 0 && owner.dotStrip.onDragEnd != nullptr)
                owner.dotStrip.onDragEnd();

            owner.draggedRing = -1;
            owner.repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            const auto ring = ringUnder (event);

            if (juce::isPositiveAndBelow (ring, (int) owner.routings.size()) && owner.dotStrip.onZero != nullptr)
                owner.dotStrip.onZero (owner.routings[(size_t) ring].slot);
        }

    private:
        int ringUnder (const juce::MouseEvent& event) const
        {
            return owner.ringAt (owner.getLocalPoint (this, event.position));
        }

        void setHovered (int ring)
        {
            if (ring == owner.hoveredRing)
                return;

            owner.hoveredRing = ring;
            setMouseCursor (ring >= 0 ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            updateTooltip (ring);
            owner.repaint();
        }

        void updateTooltip (int ring)
        {
            if (! juce::isPositiveAndBelow (ring, (int) owner.routings.size()))
                return;

            const auto& dot = owner.routings[(size_t) ring];
            setTooltip (ModNames::source (dot.source, owner.processorRef) + "  " + (dot.depth >= 0.0f ? "+" : "")
                        + juce::String (juce::roundToInt (dot.depth * 100.0f)) + "%" + (dot.bipolar ? "  (swings both ways)" : "")
                        + (dot.bypass ? "  (bypassed)" : "")
                        + "\nDrag the ring up or right to deepen it (Shift for fine), double-click to zero it, "
                          "right-click to bypass or remove it.");
        }

        void showMenu (int ring)
        {
            const auto dot = owner.routings[(size_t) ring];
            juce::PopupMenu menu;
            menu.addSectionHeader (ModNames::source (dot.source, owner.processorRef) + "  ("
                                   + juce::String (juce::roundToInt (dot.depth * 100.0f)) + "%)");
            menu.addItem (1, "Bypass", true, dot.bypass);
            menu.addItem (2, "Remove");
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                [safeOwner = juce::Component::SafePointer<KnobControl> (&owner), dot] (int result)
                                {
                                    if (safeOwner == nullptr)
                                        return;

                                    if (result == 1 && safeOwner->dotStrip.onBypass != nullptr)
                                        safeOwner->dotStrip.onBypass (dot.slot, ! dot.bypass);
                                    else if (result == 2 && safeOwner->dotStrip.onRemove != nullptr)
                                        safeOwner->dotStrip.onRemove (dot.slot);
                                });
        }

        KnobControl& owner;
        int dragSlot = -1;
        float dragStartDepth = 0.0f;
    };

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

    // The badges sit in a column right of the rings, as many as the dial's
    // height takes (then "+N"); the ring overlay covers the dial.
    void layoutDots()
    {
        const auto centre = dialCentre();
        const auto dial = dialRadius();
        const auto stripW = ModDotStrip::stripWidth, pitch = ModDotStrip::dotPitch;
        const auto x = (int) std::ceil (centre.x + outerRingRadius() + 1.0f);

        // Beside the rings, a column of badges as tall as the dial, while
        // the knob is hovered (the rings are the legend the rest of the
        // time, V7-27). A knob too narrow for that leaves its routings to
        // the rings (and its card); only those past the three rings get a
        // badge, in its top-right corner: the routing's own, or "+N".
        const auto roomBeside = getWidth() - x >= stripW - 1 && (badgesShown || (int) routings.size() > maxRings);
        juce::Rectangle<int> bounds;

        if (roomBeside)
        {
            dotStrip.setDots (routings);
            dotStrip.setMaxVisible ((int) (dial * 2.0f + 6.0f) / pitch);
            const auto height = juce::jmax (pitch, dotStrip.getPreferredHeight());
            bounds = { x, (int) (centre.y - dial) - 2, stripW, height };
        }
        else
        {
            const auto extra = (int) routings.size() > maxRings ? std::vector<ModDotStrip::Dot> (routings.begin() + maxRings, routings.end())
                                                                 : std::vector<ModDotStrip::Dot>();
            dotStrip.setDots (extra);
            dotStrip.setMaxVisible (1);
            auto y = (int) (centre.y - dial) - pitch + 1;

            if (label.isVisible() && label.getText().isNotEmpty())
            {
                const auto textWidth = juce::GlyphArrangement::getStringWidthInt (label.getFont(), label.getText());
                if (label.getBounds().getCentreX() + textWidth / 2 + 2 > getWidth() - stripW + 2)
                    y = juce::jmax (y, label.getBottom() - 1);
            }

            bounds = { getWidth() - stripW, y, stripW, pitch };
        }

        dotStrip.setBounds (bounds.withY (juce::jmax (0, bounds.getY())));
        dotStrip.setVisible (! compact && (roomBeside ? ! routings.empty() : (int) routings.size() > maxRings));

        ringOverlay.setBounds (getLocalBounds());
        ringOverlay.setVisible (! routings.empty());
        ringOverlay.toFront (false);
        dotStrip.toFront (false);
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

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processorRef->readModSlot (i);

            if (slot.destination != ringConfig.destination || slot.source == Mod::Source::None)
                continue;

            const auto bipolar = slot.polarity == Mod::Polarity::Bipolar
                                 || (slot.polarity == Mod::Polarity::Natural && Mod::isBipolarSource (slot.source));
            found.push_back ({ i, (int) slot.source, slot.depth, slot.bypass, bipolar });

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
                                                       && a.bipolar == b.bipolar && std::abs (a.depth - b.depth) < 1.0e-4f; });

        // Mid-drag the ring keeps its own depth (the parameter catches up).
        if (! changed || draggedRing >= 0)
            return;

        routings = std::move (found);
        layoutDots(); // (and the badges' list)
        repaint();
    }

public:
    // "Modulate with": LFOs, envelopes, macros and the rest, by their
    // ModNames names (the UI test reads it).
    juce::PopupMenu buildModulateWithMenu() const
    {
        juce::PopupMenu sourceMenu, lfoMenu, envMenu, macroMenu, otherMenu;

        // Only the LFOs and envelopes in the pools (a source that has no
        // card isn't offered); "New LFO" / "New envelope" adds the next.
        // The LFOs' B outputs file under LFOs.
        for (const auto i : ModNames::sourcesInMenuOrder())
        {
            const auto source = (Mod::Source) i;
            if (source == Mod::Source::InputEnv && ! IlanaSynthAudioProcessor::isEffectBuild)
                continue;
            if (! modSourceInPatch (*processorRef, source) && ! routesFrom (i))
                continue;
            const auto group = ModNames::groupOf (i);
            auto& target = group == ModNames::SourceGroup::lfo || group == ModNames::SourceGroup::lfoB ? lfoMenu
                         : group == ModNames::SourceGroup::envelope ? envMenu
                         : group == ModNames::SourceGroup::macro ? macroMenu : otherMenu;
            target.addItem (i + 1, ModNames::source (i, processorRef), true, routesFrom (i));
        }

        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (! processorRef->isLfoShown (lfo))
            {
                lfoMenu.addSeparator();
                lfoMenu.addItem (6000 + lfo, "New LFO  (LFO " + juce::String (lfo + 1) + ")");
                break;
            }

        for (int env = 0; env < 16; ++env)
            if (! envelopeShown (*processorRef, env))
            {
                envMenu.addSeparator();
                envMenu.addItem (6100 + env, "New envelope  (" + ModNames::source ((int) envelopeSource (env)) + ")");
                break;
            }

        sourceMenu.addSubMenu ("LFOs", lfoMenu);
        sourceMenu.addSubMenu ("Envelopes", envMenu);
        sourceMenu.addSubMenu ("Macros", macroMenu);
        sourceMenu.addSubMenu ("Performance and more", otherMenu);
        return sourceMenu;
    }

private:
    void showModMenu()
    {
        if (processorRef == nullptr)
            return;

        juce::PopupMenu menu;

        if (ringConfig.destination != 0)
        {
            menu.addSubMenu ("Modulate with", buildModulateWithMenu());

            if (! routings.empty())
            {
                juce::PopupMenu removeMenu;

                for (const auto& dot : routings)
                    removeMenu.addItem (5000 + dot.slot, ModNames::source (dot.source, processorRef) + "  ("
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
                                                  : result >= 6000 ? juce::String ("Add modulation")
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
                                else if (result >= 6100)
                                {
                                    // A new envelope: into the pool, then routed.
                                    processor.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, result - 6100, true);
                                    processor.assignModSlot ((int) envelopeSource (result - 6100), safeThis->ringConfig.destination, 0.35f);
                                }
                                else if (result >= 6000)
                                {
                                    processor.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, result - 6000, true);
                                    processor.assignModSlot ((int) Mod::lfoSourceFor (result - 6000), safeThis->ringConfig.destination, 0.35f);
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

        if (const auto dragged = modSourceBeingDragged (*this); dragged != dragSource)
        {
            dragSource = dragged;
            repaint();
        }

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

        if (const auto wantBadges = ! routings.empty() && (isMouseOverOrDragging (true) || badgesForced); wantBadges != badgesShown)
        {
            badgesShown = wantBadges;
            layoutDots();
        }

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
        auto modValue = processorRef->getModDisplay (ringConfig.destination);

        // Each ring's live stretch: its source now, shaped by the slot, times
        // the depth.
        liveValues.resize ((size_t) numRings());
        for (int i = 0; i < numRings(); ++i)
        {
            const auto slot = processorRef->readModSlot (routings[(size_t) i].slot);
            liveValues[(size_t) i] = slot.bypass ? 0.0f
                                                 : Mod::shape (slot, processorRef->getSourceDisplayValue ((int) slot.source)) * slot.depth;
            modValue += 3.0f * liveValues[(size_t) i] * (float) (i + 2);
        }

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
    RingOverlay ringOverlay { *this };
    std::vector<float> liveValues;
    int hoveredRing = -1, draggedRing = -1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    IlanaSynthAudioProcessor* processorRef = nullptr;
    juce::RangedAudioParameter* parameter = nullptr;
    // The knob's name as the matrix writes it ("OSC 1 › Frame", S7-20),
    // the first line of its tooltip and the hover line's title.
    juce::String displayName() const
    {
        if (ringConfig.destination != 0)
            return processorRef != nullptr ? ModNames::destination (ringConfig.destination, *processorRef)
                                           : ModNames::destination (ringConfig.destination);
        return parameter != nullptr ? ModNames::asLabelled (ModNames::detail::paramName (parameterId, parameter->getName (64))).full()
                                    : parameterId;
    }

    // Tooltip and track for whether the knob takes a source: a dotted
    // track and "Not modulatable" when it doesn't (I7-5).
    void applyModulatableMark()
    {
        slider.getProperties().set ("notModulatable", ! isModulatable() && ! sourceKnob);
        slider.repaint();

        if (parameter == nullptr)
            return;

        const auto description = describeParameter (parameterId);
        const auto modHint = isModulatable() ? juce::String ("  Drop a mod source here, or right-click to modulate.")
                                             : juce::String ("  Not modulatable.");
        baseTooltip = displayName() + "\n" + description + modHint;
        const auto tooltip = baseTooltip + (inactiveNote.isNotEmpty() ? "\n(No effect now: " + inactiveNote + ")" : juce::String());
        slider.setTooltip (tooltip);
        setTooltip (tooltip);
    }

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
    int dragSource = 0; // the source being dragged anywhere (V8-10)
    bool hover = false;
    bool compact = false;
    bool sourceKnob = false;
    bool badgesShown = false; // the mouse is on the knob (or its badges)
    bool badgesForced = false; // (the tests and snapshots)
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
    juce::Label& getNameLabel() { return label; }

    // Replace the drop-down list with something else when clicked.
    void setPopupOverride (std::function<void()> override) { combo.popupOverride = std::move (override); }

    void resized() override
    {
        auto area = getLocalBounds();

        // A menu without a name (PLAY's strips: its value says what it is)
        // is just the box.
        if (label.getText().isEmpty())
        {
            label.setBounds ({});
            combo.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (24, area.getHeight())));
            return;
        }

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
        rules.push_back ({ &control, std::move (hasEffect), why, {} });
    }

    // The same, for a control whose whole module is switched off: drawn at
    // `offAlpha()` (e.g. FilterColours::offAlpha, lighter than one dimmed
    // control) instead of IlanaTheme::dimmedAlpha while it doesn't act.
    void add (juce::Component& control, Condition hasEffect, const juce::String& why, std::function<float()> offAlpha)
    {
        rules.push_back ({ &control, std::move (hasEffect), why, std::move (offAlpha) });
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
            const auto alpha = acts ? 1.0f : rule.offAlpha != nullptr ? rule.offAlpha() : IlanaTheme::dimmedAlpha;

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
        std::function<float()> offAlpha;
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
// under), for a grid read by its labels (SEQ's GENERATE). menuWeight > 1
// gives each menu that much of a column against the others' one, for rows
// whose menus hold long names beside short knob labels (FX algorithms).
inline void layoutRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, bool oneLabelLine = false,
                       float menuWeight = 1.0f)
{
    if (items.empty())
        return;

    auto menus = 0;
    for (auto* item : items)
        menus += dynamic_cast<ComboControl*> (item) != nullptr ? 1 : 0;
    if (menus == (int) items.size())
        menuWeight = 1.0f;

    const auto shares = (float) ((int) items.size() - menus) + (float) menus * menuWeight;
    const auto width = (int) ((float) area.getWidth() / shares);
    const auto menuWidth = (int) ((float) width * menuWeight);
    auto band = 0;

    // One label size for the row (V9-6): the smallest any of its names needs
    // in its cell, so no label is shrunk on its own beside bigger ones.
    {
        std::vector<std::pair<juce::Label*, float>> needs;
        auto smallest = 1000.0f;

        for (auto* item : items)
        {
            juce::Label* name = nullptr;

            if (auto* knob = dynamic_cast<KnobControl*> (item))
                name = knob->isCompact() ? nullptr : &knob->getNameLabel();
            else if (auto* combo = dynamic_cast<ComboControl*> (item))
                name = &combo->getNameLabel();

            if (name == nullptr || name->getText().isEmpty())
                continue;

            const auto cell = (float) (dynamic_cast<ComboControl*> (item) != nullptr ? menuWidth : width) - 6.0f - 2.0f;
            const auto need = IlanaTheme::fittedFont (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body)), name->getText().trim(), cell,
                                                      IlanaTheme::TextSize::minPassive).getHeight();
            needs.push_back ({ name, need });
            smallest = juce::jmin (smallest, need);
        }

        for (auto& entry : needs)
            entry.first->getProperties().set ("fitCap", smallest);
    }

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
        auto cell = area.removeFromLeft (dynamic_cast<ComboControl*> (item) != nullptr ? menuWidth : width).reduced (3);

        if (item == nullptr)
            continue;

        if (dialDrop > 0 && (dynamic_cast<ToggleControl*> (item) != nullptr || dynamic_cast<ComboControl*> (item) != nullptr))
            cell = cell.withTrimmedTop (dialDrop);

        item->setBounds (cell);
    }
}
