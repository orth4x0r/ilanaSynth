#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <functional>
#include <vector>

#include "ParamInfo.h"
#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"
#include "AnimationUtils.h"

// The mod slots a source takes part in (as the source or the VIA aux), and
// a one-step removal of those routes: a slot the source drives is cleared,
// one it only scales (VIA) loses its aux. Shared by the LFO and envelope
// pools' remove buttons.
inline std::vector<int> modSlotsUsing (const IlanaSynthAudioProcessor& processor, std::initializer_list<Mod::Source> sources)
{
    std::vector<int> slots;

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = processor.readModSlot (slot);

        if (routing.destination == 0)
            continue;

        for (const auto source : sources)
            if (routing.source == source || routing.aux == source)
            {
                slots.push_back (slot);
                break;
            }
    }

    return slots;
}

inline void removeModRoutes (IlanaSynthAudioProcessor& processor, std::initializer_list<Mod::Source> sources)
{
    for (const auto slot : modSlotsUsing (processor, sources))
    {
        const auto routing = processor.readModSlot (slot);
        const auto drives = std::find (sources.begin(), sources.end(), routing.source) != sources.end();

        if (drives)
            processor.clearModSlot (slot);
        else
            processor.setModSlotValue (slot, "aux", 0.0f);
    }
}

// Asks before a pool card goes when it is still in use: a small menu at the
// card naming what it drives, with the removal as its one action (UI review
// 6, S6-35). `onRemove` runs only when the user picks it.
// The UI test answers the question through this instead of a menu.
inline std::function<void (const juce::String& heading, const juce::String& action, std::function<void()> onRemove)>& poolRemovalHook()
{
    static std::function<void (const juce::String&, const juce::String&, std::function<void()>)> hook;
    return hook;
}

inline void confirmPoolRemoval (juce::Component& target, const juce::String& heading, const juce::String& action,
                                std::function<void()> onRemove)
{
    if (poolRemovalHook() != nullptr)
    {
        poolRemovalHook() (heading, action, std::move (onRemove));
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader (heading);
    menu.addItem (1, action);
    menu.addItem (2, "Keep it");
    juce::Component::SafePointer<juce::Component> safeTarget (&target);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
                        [safeTarget, onRemove = std::move (onRemove)] (int result)
                        {
                            if (safeTarget != nullptr && result == 1 && onRemove != nullptr)
                                onRemove();
                        });
}

// The geometry the LFO and envelope pools share (UI review 6, V6-8 and
// S6-34): one card per module in the patch plus a slim "+" for the next, no
// numbered ruler above. Cards keep a quarter of the view while they fit and
// narrow before the bar has to scroll; past that it scrolls sideways.
namespace PoolCards
{
inline constexpr float gap = 8.0f;
inline constexpr float plusWidth = 36.0f;
inline constexpr float minimumWidth = 112.0f;

// The width of one card for `cards` full cards (plus the "+" when shown) in
// a view `viewWidth` wide.
inline float cardWidth (int viewWidth, int cards, bool withPlus)
{
    const auto view = (float) juce::jmax (1, viewWidth);
    const auto quarter = (view - gap * 3.0f) / 4.0f;
    const auto plusPart = withPlus ? plusWidth + gap : 0.0f;
    const auto fitted = cards > 0 ? (view - plusPart - gap * (float) (cards - 1)) / (float) cards : quarter;
    return juce::jmax (minimumWidth, juce::jmin (quarter, fitted));
}

// The small "x" at a card's top right, shown while the card is hovered.
inline juce::Rectangle<float> removeBounds (juce::Rectangle<float> card)
{
    return { card.getRight() - 21.0f, card.getY() + 4.0f, 16.0f, 16.0f };
}

inline void paintRemoveButton (juce::Graphics& g, juce::Rectangle<float> box, bool hot)
{
    g.setColour (juce::Colours::black.withAlpha (hot ? 0.6f : 0.4f));
    g.fillEllipse (box);
    g.setColour (juce::Colours::white.withAlpha (hot ? 1.0f : 0.7f));
    const auto cross = box.reduced (5.0f);
    g.drawLine ({ cross.getTopLeft(), cross.getBottomRight() }, 1.4f);
    g.drawLine ({ cross.getBottomLeft(), cross.getTopRight() }, 1.4f);
}

inline void paintPlus (juce::Graphics& g, juce::Rectangle<float> card, bool hovered)
{
    IlanaTheme::paintWell (g, card, 6.0f);
    if (hovered)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (card, 6.0f);
    }
    g.setColour (hovered ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::display, true));
    g.drawText ("+", card, juce::Justification::centred);
}
} // namespace PoolCards

// The patch's LFOs at a glance, Phase Plant style: one card per LFO in the
// patch (added or routed), a slim "+" for the next, then the fixed
// modulators edited in the same place (the MSEG; Clocked S&H once routed).
// Each card shows its waveform, a live phase dot, its rate and what it
// drives. Clicking a card selects it for editing; dragging a card onto a
// knob routes it there; the "x" on a hovered card removes it (asking first
// when it is routed).
class LfoThumbBar : public juce::Component,
                    public juce::SettableTooltipClient,
                    private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~LfoThumbBar() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    LfoThumbBar (IlanaSynthAudioProcessor& p, std::function<juce::Colour (int)> colourForIn)
        : processorRef (p), colourFor (std::move (colourForIn))
    {
        juce::Random random (99);

        for (auto& value : sampleHoldPreview)
            value = random.nextFloat() * 2.0f - 1.0f;

        startTimerHz (20);
    }

    // A modulator edited beside the LFOs but not one of them: the MSEG, the
    // Clocked S&H, later the FM page's voice LFO. Its card follows the
    // LFOs'; selecting it calls onSelect with numLfos + its position in the
    // order added.
    struct ExtraCard
    {
        juce::String title;
        Mod::Source source {};                         // what dragging the card routes
        juce::Colour colour;
        std::function<bool()> isShown;                 // null: always
        std::function<float (double)> valueAt;         // its picture over one cycle, -1..1
        std::function<double()> phase;                 // the live dot (null: none)
        std::function<juce::String()> rateText;        // right of the title (null: none)
        std::function<juce::StringArray()> fixedUses;  // what it drives outside the matrix
        bool stepped = false;                          // draws as held steps
    };

    void addExtraCard (ExtraCard card) { extras.push_back (std::move (card)); }
    int getNumExtraCards() const { return (int) extras.size(); }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;

    // The width the bar is seen through: four cards fill it.
    void setViewWidth (int width) { viewWidth = width; }

    int getPreferredWidth() const
    {
        const auto items = layoutItems();
        return items.empty() ? viewWidth : juce::jmax (viewWidth, (int) std::ceil (items.back().bounds.getRight()));
    }

    // Where a card sits (an LFO index, or numLfos + an extra card's position).
    juce::Rectangle<int> boundsOfCard (int id) const
    {
        for (const auto& item : layoutItems())
            if (item.id == id)
                return item.bounds.toNearestInt();

        return {};
    }

    bool isCardShown (int id) const { return ! boundsOfCard (id).isEmpty(); }

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    // Whether a mod slot uses this LFO (A or B), and a relayout after a card
    // is added.
    bool isLfoRouted (int lfo) const { return isRouted (lfo); }
    void refreshLayout() { layoutChanged(); }

    // For the UI test: what a click on the "x" of an LFO's card does.
    void requestRemove (int lfo) { removeLfo (lfo); }
    juce::Rectangle<int> removeButtonOf (int lfo) const { return PoolCards::removeBounds (boundsOfCard (lfo).toFloat()).toNearestInt(); }

    void paint (juce::Graphics& g) override
    {
        for (const auto& item : layoutItems())
        {
            if (item.id == plusId)
                PoolCards::paintPlus (g, item.bounds, hoverIndex == plusId);
            else if (item.id >= IlanaSynthAudioProcessor::numLfos)
                paintExtraCard (g, item.id - IlanaSynthAudioProcessor::numLfos, item.bounds);
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

        if (index >= 0 && index < IlanaSynthAudioProcessor::numLfos
            && (event.mods.isPopupMenu() || PoolCards::removeBounds (boundsOfCard (index).toFloat()).contains (event.position)))
        {
            removeLfo (index);
            return;
        }

        if (index == plusId)
        {
            const auto visible = visibleLfos();
            for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
                if (std::find (visible.begin(), visible.end(), lfo) == visible.end())
                {
                    processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, true);
                    selected = lfo;
                    if (onSelect != nullptr)
                        onSelect (lfo);
                    break;
                }

            layoutChanged();
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

        if (index < 0 || index == plusId || event.getDistanceFromDragStart() < 6)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                // The card's "B" tag drags output B.
                const auto fromB = index < IlanaSynthAudioProcessor::numLfos
                                   && outputBTag (index, boundsOfCard (index).toFloat()).contains (event.getMouseDownPosition().toFloat());
                auto image = createComponentSnapshot (boundsOfCard (index), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String ((int) (fromB ? Mod::lfoBSourceFor (index) : sourceOf (index))), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());
        const auto overRemove = index >= 0 && index < IlanaSynthAudioProcessor::numLfos
                                && PoolCards::removeBounds (boundsOfCard (index).toFloat()).contains (event.position);
        const auto overB = index >= 0 && index < IlanaSynthAudioProcessor::numLfos
                           && outputBTag (index, boundsOfCard (index).toFloat()).contains (event.position);

        if (index != hoverIndex || overRemove != hoverRemove || overB != hoverB)
        {
            hoverIndex = index;
            hoverRemove = overRemove;
            hoverB = overB;
            highlightedModSource() = index >= 0 && index != plusId ? (int) (overB ? Mod::lfoBSourceFor (index) : sourceOf (index)) : 0;
            setTooltip (tooltipFor (index));
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        hoverRemove = hoverB = false;
        highlightedModSource() = 0;
        repaint();
    }

    static constexpr int plusId = -2;

private:
    static constexpr float gap = PoolCards::gap;

    struct Item
    {
        int id = -1;  // an LFO, numLfos + an extra card, or plusId
        juce::Rectangle<float> bounds;
    };

    std::vector<int> visibleLfos() const
    {
        std::vector<int> visible;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            if (processorRef.isLfoShown (lfo))
                visible.push_back (lfo);
        return visible;
    }

    std::vector<int> visibleExtras() const
    {
        std::vector<int> visible;
        for (int extra = 0; extra < (int) extras.size(); ++extra)
            if (extras[(size_t) extra].isShown == nullptr || extras[(size_t) extra].isShown())
                visible.push_back (extra);
        return visible;
    }

    // Cards left to right: the LFOs, the "+", then the extra cards.
    std::vector<Item> layoutItems() const
    {
        const auto lfos = visibleLfos();
        const auto shownExtras = visibleExtras();
        const auto withPlus = (int) lfos.size() < IlanaSynthAudioProcessor::numLfos;
        const auto width = PoolCards::cardWidth (viewWidth > 0 ? viewWidth : getWidth(), (int) (lfos.size() + shownExtras.size()), withPlus);
        const auto height = (float) getHeight();
        std::vector<Item> items;
        auto x = 0.0f;

        for (const auto lfo : lfos)
        {
            items.push_back ({ lfo, { x, 0.0f, width, height } });
            x += width + gap;
        }

        if (withPlus)
        {
            items.push_back ({ plusId, { x, 0.0f, PoolCards::plusWidth, height } });
            x += PoolCards::plusWidth + gap;
        }

        // The extras sit at the right end of the view when there's room, so
        // they read as apart from the LFOs and don't move as LFOs come and go.
        const auto extrasWidth = (float) shownExtras.size() * (width + gap) - gap;
        x = juce::jmax (x, (float) (viewWidth > 0 ? viewWidth : getWidth()) - extrasWidth);

        for (const auto extra : shownExtras)
        {
            items.push_back ({ IlanaSynthAudioProcessor::numLfos + extra, { x, 0.0f, width, height } });
            x += width + gap;
        }

        return items;
    }

    int numCards() const { return (int) layoutItems().size(); }

    // An LFO index, numLfos + an extra card, plusId for the "+" card, or -1.
    int indexAt (juce::Point<int> position) const
    {
        for (const auto& item : layoutItems())
            if (item.bounds.contains (position.toFloat()))
                return item.id;

        return -1;
    }

    Mod::Source sourceOf (int id) const
    {
        return id < IlanaSynthAudioProcessor::numLfos ? Mod::lfoSourceFor (id) : extras[(size_t) (id - IlanaSynthAudioProcessor::numLfos)].source;
    }

    juce::String tooltipFor (int id) const
    {
        if (id == plusId)
            return "Add an LFO";
        if (id < 0)
            return {};
        if (id >= IlanaSynthAudioProcessor::numLfos)
            return extras[(size_t) (id - IlanaSynthAudioProcessor::numLfos)].title + "\nClick to edit it below; drag it onto a knob to modulate that knob.";
        if (hoverRemove)
            return isRouted (id) ? "Remove LFO " + juce::String (id + 1) + " (asks first: it is routed)" : "Remove LFO " + juce::String (id + 1);
        if (hoverB)
            return "LFO " + juce::String (id + 1) + " B\nThis shape's second output: drag it onto a knob to modulate that knob with it.";
        return "LFO " + juce::String (id + 1) + "\nClick to edit it below; drag it onto a knob to modulate that knob. The x removes it.";
    }

    void layoutChanged()
    {
        lastCardCount = numCards();
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
        repaint();
    }

    // The "x": an unrouted LFO goes at once, a routed one asks first and
    // takes its routes with it (one undo step).
    void removeLfo (int lfo)
    {
        const auto slots = modSlotsUsing (processorRef, { Mod::lfoSourceFor (lfo), Mod::lfoBSourceFor (lfo) });
        juce::Component::SafePointer<LfoThumbBar> safeThis (this);
        const auto remove = [safeThis, lfo]
        {
            if (safeThis == nullptr)
                return;

            auto& self = *safeThis;
            const auto name = "LFO " + juce::String (lfo + 1);
            if (self.isRouted (lfo))
                self.processorRef.performEdit ("Remove " + name, [&self, lfo]
                {
                    removeModRoutes (self.processorRef, { Mod::lfoSourceFor (lfo), Mod::lfoBSourceFor (lfo) });
                });
            self.processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Lfo, lfo, false);

            if (self.selected == lfo && self.onSelect != nullptr)
            {
                // The neighbour on the left, else the first card left.
                const auto remaining = self.visibleLfos();
                auto next = remaining.empty() ? -1 : remaining.front();
                for (const auto other : remaining)
                    if (other < lfo)
                        next = other;
                self.selected = next >= 0 ? next : IlanaSynthAudioProcessor::numLfos;
                self.onSelect (self.selected);
            }
            self.layoutChanged();
        };

        if (slots.empty())
        {
            remove();
            return;
        }

        const auto name = "LFO " + juce::String (lfo + 1);
        auto drives = describeModTargets (processorRef, Mod::lfoSourceFor (lfo));
        if (const auto viaB = describeModTargets (processorRef, Mod::lfoBSourceFor (lfo)); viaB.isNotEmpty())
            drives << (drives.isNotEmpty() ? ", " : "") << "(B) " << viaB;
        confirmPoolRemoval (*this, name + " drives " + drives,
                            "Remove " + name + " and its " + (slots.size() == 1 ? juce::String ("route") : juce::String ((int) slots.size()) + " routes"),
                            remove);
    }

    float readParam (int lfo, const juce::String& suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + suffix))
            return value->load();

        return 0.0f;
    }

    // The cards repaint often; what they drive is re-read four times a second.
    juce::String cachedTargets (int lfo)
    {
        const auto now = juce::Time::getMillisecondCounter();

        if (now - targetsStamp > 250 || targetsStamp == 0)
        {
            targetsStamp = now;
            for (int i = 0; i < (int) targetsCache.size(); ++i)
                targetsCache[(size_t) i] = describeModTargets (processorRef, Mod::lfoSourceFor (i));
            extraTargets.clear();
            for (const auto& extra : extras)
                extraTargets.add (describeModTargets (processorRef, extra.source, extra.fixedUses != nullptr ? extra.fixedUses() : juce::StringArray()));
        }

        return targetsCache[(size_t) juce::jlimit (0, (int) targetsCache.size() - 1, lfo)];
    }

    std::array<juce::String, (size_t) IlanaSynthAudioProcessor::numLfos> targetsCache;
    juce::StringArray extraTargets;
    juce::uint32 targetsStamp = 0;

    bool isRouted (int lfo) const
    {
        return ! modSlotsUsing (processorRef, { Mod::lfoSourceFor (lfo), Mod::lfoBSourceFor (lfo) }).empty();
    }

    // M8.1: a simulated shape's picture, a few seconds of it from a fresh
    // start at RATE 1 Hz, rebuilt when its settings change.
    float simValue (int lfo, const LfoSimSettings& settings, double phase) const
    {
        auto& cache = simTraces[(size_t) lfo];
        if (cache.shape != settings.shape || cache.params != settings.p || cache.axis != settings.axis)
        {
            cache.shape = settings.shape;
            cache.params = settings.p;
            cache.axis = settings.axis;
            LfoSim sim;
            sim.sampleRate = 600.0;
            sim.reset (settings, 3);
            const auto seconds = LfoSimShapes::isPhysics (settings.shape) ? 3.0 : 4.0;
            const auto perPoint = juce::jmax (1, (int) (seconds * 600.0 / (double) cache.values.size()));
            for (auto& value : cache.values)
                for (int i = 0; i < perPoint; ++i)
                {
                    float b = 0.0f;
                    sim.next (settings, 1.0 / 600.0, value, b);
                }
        }
        const auto index = juce::jlimit (0, (int) cache.values.size() - 1, (int) (phase * (double) cache.values.size()));
        return cache.values[(size_t) index];
    }

    struct SimTrace
    {
        int shape = -1, axis = 0;
        std::array<float, LfoSimInfo::numParams> params {};
        std::array<float, 128> values {};
    };
    mutable std::array<SimTrace, (size_t) IlanaSynthAudioProcessor::numLfos> simTraces;

    float shapeValue (int lfo, int shape, double phase) const
    {
        if (LfoSimShapes::isSim (shape))
            return simValue (lfo, processorRef.readLfoSimSettings (lfo), juce::jlimit (0.0, 0.999999, phase));

        phase = LfoShapes::isPhysics (shape) ? juce::jlimit (0.0, 0.999999, phase)
                                             : phase - std::floor (phase);

        switch (shape)
        {
            case 5: return sampleHoldPreview[(size_t) juce::jlimit (0, 7, (int) (phase * 8.0))];
            case 6: return processorRef.getLfoCustomPoint (lfo, juce::jlimit (0, IlanaSynthAudioProcessor::lfoDrawSteps - 1,
                                                                              (int) (phase * IlanaSynthAudioProcessor::lfoDrawSteps)));
            case 7: return readParam (lfo, "_step" + juce::String (juce::jlimit (0, 15, (int) (phase * 16.0)) + 1));
            case IlanaSynthAudioProcessor::curveShape: return processorRef.getLfoCurveValue (lfo, phase);
            default: return lfoShapeValue (shape, phase);
        }
    }

    // A simulated shape's second output, as a small "B" tag at the card's
    // bottom right that drags LFO n B (UI review 6, I6-24). Empty for the
    // other shapes.
    juce::Rectangle<float> outputBTag (int lfo, juce::Rectangle<float> card) const
    {
        if (! LfoSimShapes::isSim ((int) readParam (lfo, "_shape")))
            return {};

        return { card.getRight() - 24.0f, card.getBottom() - 19.0f, 18.0f, 14.0f };
    }

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

    // Title, a dot when it drives something, what it drives, the rate.
    void paintTitleRow (juce::Graphics& g, juce::Rectangle<float> titleRow, const juce::String& title, const juce::String& rateText,
                        const juce::String& targets, juce::Colour colour, bool active, bool routed) const
    {
        const auto titleFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, title);

        g.setColour (active ? colour : IlanaTheme::Ui::text2);
        g.setFont (titleFont);
        g.drawText (title, titleRow, juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (rateText, titleRow, juce::Justification::centredRight);

        if (routed)
        {
            g.setColour (colour);
            g.fillEllipse (titleRow.getX() + titleWidth + 6.0f, titleRow.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        // What it drives, on the title line between the name and the rate
        // (in the lower corner it sat on the curve).
        const auto rateWidth = rateText.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label)), rateText);
        paintTargetTag (g, titleRow.withTrimmedLeft (titleWidth + 16.0f).withTrimmedRight (rateWidth + 8.0f), targets, colour);
    }

    void paintTrace (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour colour, float alpha, bool stepped,
                     const std::function<float (double)>& valueAt) const
    {
        juce::Path path;
        constexpr int points = 96;

        for (int i = 0; i <= points; ++i)
        {
            const auto phase = (double) i / (double) points;
            const auto value = valueAt (juce::jmin (0.999999, phase));
            const auto x = plot.getX() + plot.getWidth() * (float) phase;
            const auto y = plot.getCentreY() - value * plot.getHeight() * 0.45f;

            if (i == 0)
                path.startNewSubPath (x, y);
            else if (stepped)
            {
                path.lineTo (x, path.getCurrentPosition().y);
                path.lineTo (x, y);
            }
            else
                path.lineTo (x, y);
        }

        g.setColour (colour.withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void paintDot (juce::Graphics& g, juce::Rectangle<float> plot, juce::Colour colour, double phase, float value) const
    {
        const juce::Point<float> dot (plot.getX() + plot.getWidth() * (float) phase,
                                      plot.getCentreY() - value * plot.getHeight() * 0.45f);

        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (dot));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
    }

    void paintCard (juce::Graphics& g, int lfo, juce::Rectangle<float> card)
    {
        const auto colour = colourFor (lfo);
        const auto active = lfo == selected;
        const auto hovered = lfo == hoverIndex;

        paintFrame (g, card, colour, active, hovered);

        auto inner = card.reduced (8.0f, 5.0f);
        auto titleRow = inner.removeFromTop (14.0f);

        const auto synced = readParam (lfo, "_sync") > 0.5f;
        const juce::StringArray divisions { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
        const auto rateText = synced ? divisions[juce::jlimit (0, divisions.size() - 1, (int) readParam (lfo, "_div"))]
                                     : describeValue ("lfo" + juce::String (lfo + 1) + "_rate", readParam (lfo, "_rate")); // as the RATE knob shows it

        // The hovered card's "x" takes the rate's corner.
        const auto targets = cachedTargets (lfo);
        paintTitleRow (g, hovered ? titleRow.withTrimmedRight (18.0f) : titleRow, "LFO " + juce::String (lfo + 1), rateText, targets,
                       colour, active, isRouted (lfo));

        const auto plot = inner.reduced (0.0f, 3.0f);
        const auto shape = (int) readParam (lfo, "_shape");

        // Unassigned LFOs are drawn faint.
        paintTrace (g, plot, colour, active ? 0.95f : (targets.isNotEmpty() ? 0.6f : 0.3f), false,
                    [this, lfo, shape] (double phase) { return shapeValue (lfo, shape, phase); });

        const auto phase = (double) processorRef.getLfoPhase (lfo);
        paintDot (g, plot, colour, phase, shapeValue (lfo, shape, phase));

        if (const auto tag = outputBTag (lfo, card); ! tag.isEmpty())
        {
            const auto hot = hovered && hoverB;
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (colour.withAlpha (hot ? 1.0f : 0.6f));
            g.drawRoundedRectangle (tag.reduced (0.5f), 4.0f, 1.0f);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("B", tag, juce::Justification::centred);
        }

        if (hovered)
            PoolCards::paintRemoveButton (g, PoolCards::removeBounds (card), hoverRemove);
    }

    void paintExtraCard (juce::Graphics& g, int extra, juce::Rectangle<float> card)
    {
        const auto& info = extras[(size_t) extra];
        const auto id = IlanaSynthAudioProcessor::numLfos + extra;
        const auto active = id == selected;
        const auto hovered = id == hoverIndex;

        paintFrame (g, card, info.colour, active, hovered);
        cachedTargets (0);

        auto inner = card.reduced (8.0f, 5.0f);
        const auto titleRow = inner.removeFromTop (14.0f);
        const auto targets = extraTargets[extra];
        paintTitleRow (g, titleRow, info.title, info.rateText != nullptr ? info.rateText() : juce::String(), targets, info.colour,
                       active, targets.isNotEmpty());

        const auto plot = inner.reduced (0.0f, 3.0f);

        if (info.valueAt != nullptr)
        {
            paintTrace (g, plot, info.colour, active ? 0.95f : (targets.isNotEmpty() ? 0.6f : 0.3f), info.stepped, info.valueAt);

            if (info.phase != nullptr)
            {
                const auto phase = juce::jlimit (0.0, 0.999999, info.phase());
                paintDot (g, plot, info.colour, phase, info.valueAt (phase));
            }
        }
    }

    void timerCallback() override
    {
        if (isShowing())
        {
            // Routing an LFO elsewhere, or loading a patch, can add a card.
            if (numCards() != lastCardCount)
                layoutChanged();
            if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ lfoPhases()))
                repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    // Free-running LFOs move with nothing sounding: their dots follow.
    juce::uint64 lfoPhases() const
    {
        juce::uint64 signature = 0;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            signature ^= IlanaAnim::phaseSignature (processorRef.getLfoPhase (lfo), lfo);
        for (int extra = 0; extra < (int) extras.size(); ++extra)
            if (extras[(size_t) extra].phase != nullptr)
                signature ^= IlanaAnim::phaseSignature ((float) extras[(size_t) extra].phase(), 100 + extra);
        return signature;
    }

    IlanaSynthAudioProcessor& processorRef;
    std::function<juce::Colour (int)> colourFor;
    std::vector<ExtraCard> extras;
    std::array<float, 8> sampleHoldPreview {};
    int selected = 0;
    int hoverIndex = -1;
    bool hoverRemove = false, hoverB = false;
    int viewWidth = 0;
    int lastCardCount = -1;
};
