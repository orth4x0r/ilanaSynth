#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <algorithm>
#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"
#include "AnimationUtils.h"
#include "FmOperatorInfo.h"
#include "LfoThumbs.h"
#include "ModulePool.h"

// The patch's envelopes at a glance, Phase Plant style: one card per
// envelope in the patch (added or in use), then a slim "+" for the next.
// Cards keep one size (four to the view, narrower before they scroll). Each
// card draws its shape and says what the envelope drives. Click to edit;
// drag a card onto a knob to route it there; the "x" on a hovered card
// removes it (asking first when it is in use).
class EnvThumbBar : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~EnvThumbBar() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    struct Env
    {
        juce::String title;
        juce::String prefix;
        Mod::Source source;
        juce::Colour colour;
    };

    explicit EnvThumbBar (IlanaSynthAudioProcessor& p, std::vector<Env> envsIn)
        : processorRef (p), envs (std::move (envsIn))
    {
        startTimerHz (10);
    }

    // An envelope edited beside these but not one of ENV 1-16 (the FM page's
    // Operator EG, say). Its card follows the "+"; selecting it calls
    // onSelect with envs.size() + its position in the order added.
    struct ExtraCard
    {
        juce::String title;
        Mod::Source source {};                                     // what dragging the card routes (None: no drag)
        juce::Colour colour;
        std::function<bool()> isShown;                             // null: always
        std::function<void (juce::Graphics&, juce::Rectangle<float>, bool active)> paintShape;
        std::function<juce::String()> targets;                     // what it drives, for the title row
        std::function<bool()> isActive;                            // null: always; else greyed and "unused" while false
        juce::String tooltip;                                      // empty: the default
        bool pinnedFirst = false;                                  // before the envelopes (the DX7's, where it plays)
    };

    void addExtraCard (ExtraCard card) { extras.push_back (std::move (card)); }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;

    // A second item for the "+" menu (UI review 8, S8-1 / V8-1: the Operator
    // Env's cards are absent from a patch that doesn't play it, and offered
    // here instead). Empty: "+" adds the next envelope straight away.
    std::function<juce::String()> plusOffer;
    std::function<void()> onPlusOffer;

    static constexpr int plusId = -2;

    // Where an envelope's card sits, for scrolling it into view.
    juce::Rectangle<int> boundsOfCard (int env) const
    {
        for (const auto& item : layoutItems())
            if (item.id == env)
                return item.bounds.toNearestInt();

        return {};
    }

    // The width the bar is seen through: four cards fill it. It never needs
    // more (cards past what fits fold into the overflow card).
    void setViewWidth (int width) { viewWidth = width; }

    int getPreferredWidth() const { return viewWidth; }

    // The cards folded into the overflow card right now (the UI test).
    std::vector<int> getFoldedCards() const
    {
        std::vector<int> folded;
        layoutItems (folded);
        return folded;
    }

    // Whether the envelope plays a part, and a relayout after a card is added.
    bool isEnvelopeInUse (int env) const { return isInUse (env); }
    void refreshLayout() { layoutChanged(); }
    bool isCardShown (int env) const { return ! boundsOfCard (env).isEmpty(); }

    // In the pool, on screen or folded into the overflow card.
    bool isCardInPool (int id) const
    {
        if (id >= 0 && id < (int) envs.size())
            return envelopeShown (processorRef, id);
        const auto extra = id - (int) envs.size();
        return extra >= 0 && extra < (int) extras.size() && (extras[(size_t) extra].isShown == nullptr || extras[(size_t) extra].isShown());
    }

    // For the UI test: what a click on a card's "x" does, and where it is
    // (empty for a card that can't be removed).
    void requestRemove (int env) { removeEnvelope (env); }
    juce::Rectangle<int> removeButtonOf (int env) const
    {
        return canRemove (env) ? PoolCards::removeBounds (boundsOfCard (env).toFloat()).toNearestInt() : juce::Rectangle<int>();
    }

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        std::vector<int> folded;
        for (const auto& item : layoutItems (folded))
        {
            if (item.id == plusId)
                PoolCards::paintPlus (g, item.bounds, hoverIndex == plusId);
            else if (item.id == PoolCards::overflowId)
                PoolCards::paintOverflow (g, item.bounds, (int) folded.size(), hoverIndex == PoolCards::overflowId);
            else if (item.id >= (int) envs.size())
                paintExtraCard (g, item.id - (int) envs.size(), item.bounds);
            else
                paintCard (g, item.id, item.bounds);
        }
    }

    // Select on release, and only for a click: a drag assigns the source to
    // a knob instead (on MAIN, selecting would switch pages mid-drag).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown() || event.getDistanceFromDragStart() >= 6)
            return;

        const auto index = indexAt (event.getPosition());

        if (index >= 0 && index < (int) envs.size()
            && (event.mods.isPopupMenu() || (canRemove (index) && PoolCards::removeBounds (boundsOfCard (index).toFloat()).contains (event.position))))
        {
            removeEnvelope (index);
            return;
        }

        if (index == PoolCards::overflowId)
        {
            std::vector<int> folded;
            layoutItems (folded);
            juce::Component::SafePointer<EnvThumbBar> safeThis (this);
            PoolCards::showOverflowMenu (*this, boundsOfCard (PoolCards::overflowId), folded,
                                         [this] (int id) { return id < (int) envs.size() ? envs[(size_t) id].title
                                                                                         : extras[(size_t) (id - (int) envs.size())].title; },
                                         [safeThis] (int id)
                                         {
                                             if (safeThis == nullptr)
                                                 return;
                                             safeThis->selected = id;
                                             if (safeThis->onSelect != nullptr)
                                                 safeThis->onSelect (id);
                                             safeThis->layoutChanged();
                                         });
            return;
        }

        if (index == plusId)
        {
            const auto offer = plusOffer != nullptr ? plusOffer() : juce::String();
            if (offer.isEmpty())
            {
                addNextEnvelope();
                return;
            }

            const auto next = nextHiddenEnvelope();
            juce::PopupMenu menu;
            menu.addItem (1, "Add " + (next >= 0 ? envs[(size_t) next].title : juce::String ("an envelope")), next >= 0);
            menu.addItem (2, offer);
            juce::Component::SafePointer<EnvThumbBar> safeThis (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (boundsOfCard (plusId))),
                                [safeThis] (int picked)
                                {
                                    if (safeThis == nullptr)
                                        return;
                                    if (picked == 1)
                                        safeThis->addNextEnvelope();
                                    else if (picked == 2 && safeThis->onPlusOffer != nullptr)
                                        safeThis->onPlusOffer();
                                });
        }
        else if (index >= 0 && onSelect != nullptr)
        {
            selected = index;
            onSelect (index);
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getMouseDownPosition());

        if (index < 0 || event.getDistanceFromDragStart() < 6)
            return;

        const auto source = index < (int) envs.size() ? envs[(size_t) index].source : extras[(size_t) (index - (int) envs.size())].source;

        if (source == Mod::Source::None)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (boundsOfCard (index), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String ((int) source), this, juce::ScaledImage (image), true);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());
        const auto overRemove = index >= 0 && index < (int) envs.size() && canRemove (index)
                                && PoolCards::removeBounds (boundsOfCard (index).toFloat()).contains (event.position);

        if (index != hoverIndex || overRemove != hoverRemove)
        {
            hoverIndex = index;
            hoverRemove = overRemove;
            highlightedModSource() = index >= 0 && index < (int) envs.size() ? (int) envs[(size_t) index].source
                                     : index >= (int) envs.size()           ? (int) extras[(size_t) (index - (int) envs.size())].source
                                                                            : 0;
            setTooltip (tooltipFor (index));
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        hoverRemove = false;
        highlightedModSource() = 0;
        repaint();
    }

private:
    static constexpr float gap = PoolCards::gap;

    using Item = PoolCards::Placed;  // an envelope, envs.size() + an extra card, plusId or the overflow

    std::vector<int> visibleEnvelopes() const
    {
        std::vector<int> visible;
        for (int env = 0; env < (int) envs.size(); ++env)
            if (envelopeShown (processorRef, env))
                visible.push_back (env);
        return visible;
    }

    std::vector<int> visibleExtras() const
    {
        std::vector<int> shown;
        for (int extra = 0; extra < (int) extras.size(); ++extra)
            if (extras[(size_t) extra].isShown == nullptr || extras[(size_t) extra].isShown())
                shown.push_back (extra);
        return shown;
    }

    // The pinned extra cards (the Operator Env's, on a voice that plays it:
    // UI review 8, I8-5), the envelopes, the other extra cards, then the "+"
    // (as the LFO pool), with the overflow card before it when they don't
    // fit; the cards fold from the right, so the pinned ones stay.
    std::vector<Item> layoutItems (std::vector<int>& folded) const
    {
        const auto envelopes = visibleEnvelopes();
        const auto withPlus = envelopes.size() < envs.size();
        std::vector<int> ids;
        for (const auto extra : visibleExtras())
            if (extras[(size_t) extra].pinnedFirst)
                ids.push_back ((int) envs.size() + extra);
        ids.insert (ids.end(), envelopes.begin(), envelopes.end());
        for (const auto extra : visibleExtras())
            if (! extras[(size_t) extra].pinnedFirst)
                ids.push_back ((int) envs.size() + extra);
        return PoolCards::layout (ids, selected, withPlus, plusId, (float) (viewWidth > 0 ? viewWidth : getWidth()), (float) getHeight(),
                                  folded);
    }

    std::vector<Item> layoutItems() const
    {
        std::vector<int> folded;
        return layoutItems (folded);
    }

    // Changes when a card comes or goes (not with what fits).
    int numCards() const { return (int) (visibleEnvelopes().size() + visibleExtras().size()); }

    int indexAt (juce::Point<int> position) const
    {
        for (const auto& item : layoutItems())
            if (item.bounds.contains (position.toFloat()))
                return item.id;

        return -1;
    }

    // The first envelope the pool doesn't show, or -1.
    int nextHiddenEnvelope() const
    {
        for (int env = 0; env < (int) envs.size(); ++env)
            if (! envelopeShown (processorRef, env))
                return env;
        return -1;
    }

    void addNextEnvelope()
    {
        if (const auto env = nextHiddenEnvelope(); env >= 0)
        {
            processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, true);
            selected = env;
            if (onSelect != nullptr)
                onSelect (env);
        }

        layoutChanged();
    }

    juce::String tooltipFor (int index) const
    {
        if (index == plusId)
            return plusOffer != nullptr && plusOffer().isNotEmpty() ? "Add an envelope, or the Operator Env (DX7)" : "Add an envelope";
        if (index == PoolCards::overflowId)
            return "More cards than fit: click for the rest";
        if (index < 0)
            return {};
        if (index >= (int) envs.size())
        {
            const auto& info = extras[(size_t) (index - (int) envs.size())];
            return info.tooltip.isNotEmpty() ? info.tooltip : info.title + "\nClick to edit it below.";
        }
        const auto& title = envs[(size_t) index].title;
        if (hoverRemove)
            return isInUse (index) ? "Remove " + title + " (asks first: it is in use)" : "Remove " + title;
        return title + "\nClick to edit it below; drag it onto a knob to modulate that knob."
               + (canRemove (index) ? juce::String (" The x removes it.") : juce::String());
    }

    void layoutChanged()
    {
        lastCardCount = numCards();
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
        repaint();
    }

    // The amp envelope is the default every oscillator plays: it stays
    // (greyed on a DX7 voice, whose operators play the Operator Env).
    bool canRemove (int env) const
    {
        return env >= 0 && env < (int) envs.size() && envs[(size_t) env].source != Mod::Source::AmpEnv;
    }

    // The "x": an unused envelope goes at once; one in use asks first, then
    // lets go of everything it does (its filter's env amount, oscillators
    // playing or warping with it, its routes) in one undo step.
    void removeEnvelope (int env)
    {
        if (! canRemove (env))
            return;

        const auto& info = envs[(size_t) env];
        const auto slots = modSlotsUsing (processorRef, { info.source });
        juce::StringArray ties;

        if (info.source == Mod::Source::FilterEnv && std::abs (readParam ("f1_env")) > 0.001f)
            ties.add ("Filter 1's ENV AMT goes to 0");
        if (info.source == Mod::Source::FilterEnv2 && std::abs (readParam ("f2_env")) > 0.001f)
            ties.add ("Filter 2's ENV AMT goes to 0");

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            if (env > 0 && (int) readParam (prefix + "_amp_env") == env)
                ties.add ("OSC " + juce::String (osc + 1) + " goes back to AMP ENV");
            if ((int) readParam (prefix + "_pd_env") == env + 1)
                ties.add ("OSC " + juce::String (osc + 1) + "'s warp envelope goes Off");
        }

        if (! slots.empty())
            ties.add (slots.size() == 1 ? juce::String ("its route goes") : juce::String ((int) slots.size()) + " routes go");

        juce::Component::SafePointer<EnvThumbBar> safeThis (this);
        const auto remove = [safeThis, env]
        {
            if (safeThis == nullptr)
                return;

            auto& self = *safeThis;
            const auto& entry = self.envs[(size_t) env];

            if (self.isInUse (env))
                self.processorRef.performEdit ("Remove " + entry.title, [&self, env, &entry]
                {
                    const auto set = [&self] (const juce::String& id, float value)
                    {
                        if (auto* parameter = self.processorRef.apvts.getParameter (id))
                            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                    };

                    if (entry.source == Mod::Source::FilterEnv)
                        set ("f1_env", 0.0f);
                    if (entry.source == Mod::Source::FilterEnv2)
                        set ("f2_env", 0.0f);

                    for (const auto* prefix : OscillatorIds::prefixes)
                    {
                        if (env > 0 && (int) self.readParam (juce::String (prefix) + "_amp_env") == env)
                            set (juce::String (prefix) + "_amp_env", 0.0f);
                        if ((int) self.readParam (juce::String (prefix) + "_pd_env") == env + 1)
                            set (juce::String (prefix) + "_pd_env", 0.0f);
                    }

                    removeModRoutes (self.processorRef, { entry.source });
                });

            self.processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, false);

            if (self.selected == env && self.onSelect != nullptr)
            {
                // The neighbour on the left, else the first card left.
                const auto remaining = self.visibleEnvelopes();
                auto next = remaining.empty() ? 0 : remaining.front();
                for (const auto other : remaining)
                    if (other < env)
                        next = other;
                self.selected = next;
                self.onSelect (next);
            }
            self.layoutChanged();
        };

        if (ties.isEmpty())
        {
            remove();
            return;
        }

        const auto drives = targetsText (env);
        confirmPoolRemoval (*this, info.title + (drives.isNotEmpty() ? " drives " + drives : juce::String (" is in use")),
                            "Remove " + info.title + " (" + ties.joinIntoString (", ") + ")", remove);
    }

    float readParam (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    bool isInUse (int env) const { return envelopeInUse (processorRef, env); }

    void paintFrame (juce::Graphics& g, juce::Rectangle<float> card, juce::Colour colour, bool active, bool hovered) const
    {
        IlanaTheme::paintWell (g, card, 6.0f);

        if (active)
        {
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (card, 6.0f);
            g.setColour (colour.withAlpha (0.9f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.4f);
        }
        else if (hovered)
        {
            g.setColour (colour.withAlpha (0.4f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.0f);
        }
    }

    void paintTitle (juce::Graphics& g, juce::Rectangle<float> titleRow, const juce::String& title, const juce::String& targets,
                     juce::Colour colour, bool active, bool inUse) const
    {
        const auto titleFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, title);

        g.setColour (active ? colour : juce::Colours::white.withAlpha (inUse ? 0.75f : 0.45f));
        g.setFont (titleFont);
        g.drawText (title, titleRow, juce::Justification::centredLeft);

        // What it drives, on the title line (in the lower corner it sat on
        // the curve). No dot after the name: the tag says it is in use, and
        // the on dot is kept for switches (UI review 8, V8-40).
        paintTargetTag (g, titleRow.withTrimmedLeft (titleWidth + 10.0f), targets, colour);
    }

    void paintCard (juce::Graphics& g, int env, juce::Rectangle<float> card)
    {
        const auto& info = envs[(size_t) env];
        const auto colour = info.colour;
        const auto active = env == selected;
        const auto hovered = env == hoverIndex;
        const auto inUse = isInUse (env);

        paintFrame (g, card, colour, active, hovered);

        auto inner = card.reduced (8.0f, 5.0f);
        auto titleRow = inner.removeFromTop (16.0f);
        const auto removable = hovered && canRemove (env);
        // The amp envelope on a DX7 voice: a short "unused" tag that fits
        // the narrowest card (UI review 7, I7-8), the shape greyed.
        const auto unusedAmp = info.source == Mod::Source::AmpEnv && ! inUse;
        if (unusedAmp)
            titleRow.removeFromRight (PoolCards::paintUnusedTag (g, titleRow));
        paintTitle (g, removable ? titleRow.withTrimmedRight (18.0f) : titleRow, info.title, unusedAmp ? juce::String() : cachedTargets (env),
                    colour, active, inUse);

        // ADSR outline on a compressed time axis so long and short stages
        // both stay readable.
        const auto prefix = juce::String (info.prefix);
        const auto attack = std::sqrt (readParam (prefix + "_attack"));
        const auto decay = std::sqrt (readParam (prefix + "_decay"));
        const auto sustain = juce::jlimit (0.0f, 1.0f, readParam (prefix + "_sustain"));
        const auto release = std::sqrt (readParam (prefix + "_release"));
        const auto delay = std::sqrt (readParam (prefix + "_delay"));
        const auto peakHold = std::sqrt (readParam (prefix + "_hold"));
        const auto hold = 0.35f;
        const auto total = juce::jmax (0.001f, delay + attack + peakHold + decay + hold + release);

        const auto plot = inner.reduced (0.0f, 3.0f);
        const auto xAt = [&plot, total] (float t) { return plot.getX() + plot.getWidth() * t / total; };
        const auto yAt = [&plot] (float level) { return plot.getBottom() - level * plot.getHeight(); };

        juce::Path path;
        path.startNewSubPath (xAt (0.0f), yAt (0.0f));
        path.lineTo (xAt (delay), yAt (0.0f));
        path.lineTo (xAt (delay + attack), yAt (1.0f));
        path.lineTo (xAt (delay + attack + peakHold), yAt (1.0f));
        path.lineTo (xAt (delay + attack + peakHold + decay), yAt (sustain));
        path.lineTo (xAt (delay + attack + peakHold + decay + hold), yAt (sustain));
        path.lineTo (xAt (total), yAt (0.0f));

        auto fill = path;
        fill.closeSubPath();
        g.setColour (colour.withAlpha (active ? 0.16f : 0.08f));
        g.fillPath (fill);

        g.setColour (colour.withAlpha (active ? 0.95f : (inUse ? 0.6f : 0.35f)));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (removable)
            PoolCards::paintRemoveButton (g, PoolCards::removeBounds (card), hoverRemove);
    }

    void paintExtraCard (juce::Graphics& g, int extra, juce::Rectangle<float> card)
    {
        const auto& info = extras[(size_t) extra];
        const auto id = (int) envs.size() + extra;
        const auto active = id == selected;
        const auto targets = info.targets != nullptr ? info.targets() : juce::String();

        const auto inUse = info.isActive == nullptr || info.isActive();

        paintFrame (g, card, info.colour, active, id == hoverIndex);
        auto inner = card.reduced (8.0f, 5.0f);
        auto titleRow = inner.removeFromTop (16.0f);

        // Greyed, with "unused", while nothing it shapes plays.
        if (! inUse)
            titleRow.removeFromRight (PoolCards::paintUnusedTag (g, titleRow));
        paintTitle (g, titleRow, info.title, inUse ? targets : juce::String(), inUse ? info.colour : IlanaTheme::Ui::text3, active && inUse,
                    inUse && targets.isNotEmpty());

        if (info.paintShape != nullptr)
        {
            if (! inUse)
                g.beginTransparencyLayer (0.35f);
            info.paintShape (g, inner.reduced (0.0f, 3.0f), active);
            if (! inUse)
                g.endTransparencyLayer();
        }
    }

    // The cards repaint often; what they drive is re-read four times a second.
    juce::String cachedTargets (int env)
    {
        const auto now = juce::Time::getMillisecondCounter();

        if (now - targetsStamp > 250 || targetsStamp == 0)
        {
            targetsStamp = now;
            targetsCache.clear();
            for (int i = 0; i < (int) envs.size(); ++i)
                targetsCache.add (targetsText (i));
        }

        return targetsCache[env];
    }

    juce::StringArray targetsCache;
    juce::uint32 targetsStamp = 0;

    // What this envelope drives: its built-in jobs, then the matrix.
    juce::String targetsText (int env) const
    {
        const auto& info = envs[(size_t) env];
        juce::StringArray fixed;

        if (info.source == Mod::Source::AmpEnv && FmOperatorInfo::ampEnvelopeInUse (processorRef))
            fixed.add ("Amp");
        else if (info.source == Mod::Source::AmpEnv)
            fixed.add ("unused");
        if (info.source == Mod::Source::FilterEnv && std::abs (readParam ("f1_env")) > 0.001f)
            fixed.add ("Filter 1");
        if (info.source == Mod::Source::FilterEnv2 && std::abs (readParam ("f2_env")) > 0.001f)
            fixed.add ("Filter 2");

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);

            if (env > 0 && processorRef.isOscillatorShown (osc) && (int) readParam (prefix + "_amp_env") == env)
                fixed.add ("Osc" + juce::String (osc + 1) + " Amp");
            if ((int) readParam (prefix + "_pd_env") == env + 1)
                fixed.add ("Osc" + juce::String (osc + 1) + " Warp");
        }

        return describeModTargets (processorRef, info.source, fixed);
    }

    void timerCallback() override
    {
        if (isShowing())
        {
            // Assigning an envelope elsewhere, or loading a patch, can add a card.
            if (numCards() != lastCardCount)
                layoutChanged();
            if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this)))
                repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    std::vector<Env> envs;
    std::vector<ExtraCard> extras;
    int selected = 0;
    int hoverIndex = -1;
    bool hoverRemove = false;
    int viewWidth = 0;
    int lastCardCount = -1;
};
