#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "ParamInfo.h"
#include "../PluginProcessor.h"
#include "../dsp/LfoShape.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"
#include "LfoSimView.h"
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

// A pool never scrolls sideways (UI review 7, V7-17 / S7-40): the cards
// narrow to minimumWidth, and past that the ones that don't fit fold into a
// "N MORE" card (a menu of them) before the "+", as the chip bar folds its
// chips. The selected card always keeps a place.
inline constexpr float overflowWidth = 62.0f;
inline constexpr int overflowId = -3;

struct Placed
{
    int id = -1;  // a card, overflowId or the bar's plus id
    juce::Rectangle<float> bounds;
};

// `ids` in their order; `folded` gets the ones in the overflow menu.
// `compact` names the cards that play no part now (an unused envelope):
// they shrink to compactWidth and the others take the room (I13-11).
inline constexpr float compactWidth = 104.0f;
inline std::vector<Placed> layout (const std::vector<int>& ids, int selected, bool withPlus, int plusId, float viewWidth,
                                   float height, std::vector<int>& folded, const std::vector<int>& compact = {})
{
    folded.clear();
    const auto view = juce::jmax (1.0f, viewWidth);
    const auto count = (int) ids.size();
    const auto plusPart = withPlus ? plusWidth + gap : 0.0f;
    auto width = cardWidth ((int) view, count, withPlus);
    auto shown = ids;

    if (count > 1 && (float) count * width + (float) (count - 1) * gap + plusPart > view + 0.5f)
    {
        const auto room = view - plusPart - overflowWidth - gap;
        const auto fit = juce::jlimit (1, count - 1, (int) std::floor ((room + gap) / (minimumWidth + gap)));
        shown.assign (ids.begin(), ids.begin() + fit);
        if (std::find (ids.begin(), ids.end(), selected) != ids.end() && std::find (shown.begin(), shown.end(), selected) == shown.end())
            shown.back() = selected;
        for (const auto id : ids)
            if (std::find (shown.begin(), shown.end(), id) == shown.end())
                folded.push_back (id);
        width = juce::jmax (40.0f, (room - gap * (float) (fit - 1)) / (float) fit);
    }

    auto shrunk = 0;
    for (const auto id : shown)
        shrunk += folded.empty() && std::find (compact.begin(), compact.end(), id) != compact.end() ? 1 : 0;
    const auto others = (int) shown.size() - shrunk;
    if (shrunk > 0 && others > 0)
    {
        const auto fitted = (view - plusPart - gap * (float) ((int) shown.size() - 1) - compactWidth * (float) shrunk) / (float) others;
        width = juce::jmax (width, juce::jmin (fitted, (view - gap * 3.0f) / 4.0f * 1.25f));
    }

    std::vector<Placed> items;
    auto x = 0.0f;
    for (const auto id : shown)
    {
        const auto w = shrunk > 0 && others > 0 && folded.empty() && std::find (compact.begin(), compact.end(), id) != compact.end()
                           ? juce::jmin (width, compactWidth) : width;
        items.push_back ({ id, { x, 0.0f, w, height } });
        x += w + gap;
    }
    if (! folded.empty())
    {
        items.push_back ({ overflowId, { x, 0.0f, overflowWidth, height } });
        x += overflowWidth + gap;
    }
    if (withPlus)
        items.push_back ({ plusId, { x, 0.0f, plusWidth, height } });
    return items;
}

// The cards sharing the whole view evenly (the design's pools: PLAY's and
// MOD's), the "+" keeping `plusW`.
inline void fillEvenly (std::vector<Placed>& items, int plusId, float view, float plusW)
{
    auto cards = 0;
    auto hasPlus = false;
    for (const auto& item : items)
    {
        cards += item.id == plusId ? 0 : 1;
        hasPlus = hasPlus || item.id == plusId;
    }

    if (cards == 0)
        return;

    const auto width = (view - (hasPlus ? plusW + gap : 0.0f) - gap * (float) (cards - 1)) / (float) cards;
    auto x = 0.0f;
    for (auto& item : items)
    {
        item.bounds.setX (x);
        item.bounds.setWidth (item.id == plusId ? plusW : width);
        x += item.bounds.getWidth() + gap;
    }
}

inline void paintOverflow (juce::Graphics& g, juce::Rectangle<float> card, int count, bool hovered)
{
    IlanaTheme::paintWell (g, card, 6.0f);
    if (hovered)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (card, 6.0f);
    }
    g.setColour (hovered ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
    auto area = card.reduced (4.0f, 6.0f);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
    g.drawText (juce::String (count) + " MORE", area.removeFromTop (area.getHeight() * 0.55f), juce::Justification::centredBottom);
    g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")), area, juce::Justification::centredTop);
}

// The overflow card's menu: the folded cards by name; `picked` gets the id.
inline void showOverflowMenu (juce::Component& target, juce::Rectangle<int> card, const std::vector<int>& folded,
                              std::function<juce::String (int)> titleOf, std::function<void (int)> picked)
{
    juce::PopupMenu menu;
    for (size_t i = 0; i < folded.size(); ++i)
        menu.addItem ((int) i + 1, titleOf (folded[i]));
    juce::Component::SafePointer<juce::Component> safeTarget (&target);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (target.localAreaToGlobal (card)),
                        [safeTarget, folded, picked = std::move (picked)] (int result)
                        {
                            if (safeTarget != nullptr && result > 0 && result <= (int) folded.size() && picked != nullptr)
                                picked (folded[(size_t) result - 1]);
                        });
}

// A card that is in the pool but plays no part now (the Operator Env's
// cards on a patch without an operator on it): a small "unused" tag at the
// right of its title line. Returns the width it took.
inline float paintUnusedTag (juce::Graphics& g, juce::Rectangle<float> titleRow, const juce::String& title = {})
{
    const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
    // On a card too narrow for its name and the word, the tag says "off"
    // rather than cutting the name (review 14, V14-17: "AMP E... unused").
    auto word = juce::String ("unused");
    if (title.isNotEmpty()
        && juce::GlyphArrangement::getStringWidth (IlanaTheme::font (IlanaTheme::TextSize::body, true), title)
                   + juce::GlyphArrangement::getStringWidth (font, word) + 10.0f + 14.0f > titleRow.getWidth())
        word = "off";
    const auto width = juce::GlyphArrangement::getStringWidth (font, word) + 10.0f;
    const auto box = titleRow.removeFromRight (width).withSizeKeepingCentre (width, 13.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (box, 6.5f);
    g.setColour (IlanaTheme::Ui::text3);
    g.setFont (font);
    g.drawText (word, box, juce::Justification::centred);
    return width + 6.0f;
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
        std::function<bool()> isActive;                // null: always; else greyed and "unused" while false
        juce::String tooltip;                          // empty: the default
        bool pinnedFirst = false;                      // before the LFOs (the DX7's, where it plays)
    };

    void addExtraCard (ExtraCard card) { extras.push_back (std::move (card)); }
    int getNumExtraCards() const { return (int) extras.size(); }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;

    // The width the bar is seen through: four cards fill it. It never needs
    // more (cards past what fits fold into the overflow card).
    void setViewWidth (int width) { viewWidth = width; }

    int getPreferredWidth() const { return viewWidth; }

    // PLAY: the cards share the whole bar evenly (the design's thumbnails)
    // instead of keeping a quarter each.
    void setFillWidth (bool fill, float plusWidth = PoolCards::plusWidth) { fillWidth = fill; plusW = plusWidth; repaint(); }

    // The cards folded into the overflow card right now (the UI test).
    std::vector<int> getFoldedCards() const
    {
        std::vector<int> folded;
        layoutItems (folded);
        return folded;
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

    // In the pool, on screen or folded into the overflow card.
    bool isCardInPool (int id) const
    {
        if (id >= 0 && id < IlanaSynthAudioProcessor::numLfos)
            return processorRef.isLfoShown (id);
        const auto extra = id - IlanaSynthAudioProcessor::numLfos;
        const auto shown = visibleExtras();
        return std::find (shown.begin(), shown.end(), extra) != shown.end();
    }

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
        IlanaAnim::countPaint ("lfoThumbs");
        std::vector<int> folded;
        for (const auto& item : layoutItems (folded))
        {
            if (item.id == plusId)
                PoolCards::paintPlus (g, item.bounds, hoverIndex == plusId);
            else if (item.id == PoolCards::overflowId)
                PoolCards::paintOverflow (g, item.bounds, (int) folded.size(), hoverIndex == PoolCards::overflowId);
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

        if (index == PoolCards::overflowId)
        {
            std::vector<int> folded;
            layoutItems (folded);
            juce::Component::SafePointer<LfoThumbBar> safeThis (this);
            PoolCards::showOverflowMenu (*this, boundsOfCard (PoolCards::overflowId), folded,
                                         [this] (int id) { return titleOf (id); },
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

        if (index < 0 || event.getDistanceFromDragStart() < 6)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                // The card's "OUT 2" tag drags the second output.
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
            highlightedModSource() = index >= 0 ? (int) (overB ? Mod::lfoBSourceFor (index) : sourceOf (index)) : 0;
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

    using Item = PoolCards::Placed;  // an LFO, numLfos + an extra card, plusId or the overflow

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

    // Cards left to right: the pinned extra cards (OP LFO on a voice that
    // plays the Operator Env: UI review 8, I8-5), the LFOs, the other extra
    // cards, then the "+" (UI review 7, I7-30), with the overflow card before
    // it when they don't fit (cards fold from the right).
    std::vector<Item> layoutItems (std::vector<int>& folded) const
    {
        const auto lfos = visibleLfos();
        const auto withPlus = (int) lfos.size() < IlanaSynthAudioProcessor::numLfos;
        std::vector<int> ids;
        for (const auto extra : visibleExtras())
            if (extras[(size_t) extra].pinnedFirst)
                ids.push_back (IlanaSynthAudioProcessor::numLfos + extra);
        ids.insert (ids.end(), lfos.begin(), lfos.end());
        for (const auto extra : visibleExtras())
            if (! extras[(size_t) extra].pinnedFirst)
                ids.push_back (IlanaSynthAudioProcessor::numLfos + extra);
        auto items = PoolCards::layout (ids, selected, withPlus, plusId, (float) (viewWidth > 0 ? viewWidth : getWidth()), (float) getHeight(),
                                        folded);

        if (fillWidth && folded.empty() && ! ids.empty())
            PoolCards::fillEvenly (items, plusId, (float) (viewWidth > 0 ? viewWidth : getWidth()), plusW);

        return items;
    }

    std::vector<Item> layoutItems() const
    {
        std::vector<int> folded;
        return layoutItems (folded);
    }

    // Changes when a card comes or goes (not with what fits: folding needs
    // only a repaint).
    int numCards() const { return (int) (visibleLfos().size() + visibleExtras().size()); }

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

    juce::String titleOf (int id) const
    {
        return id >= IlanaSynthAudioProcessor::numLfos ? extras[(size_t) (id - IlanaSynthAudioProcessor::numLfos)].title
                                                       : "LFO " + juce::String (id + 1);
    }

    bool isExtraActive (int extra) const
    {
        const auto& info = extras[(size_t) extra];
        return info.isActive == nullptr || info.isActive();
    }

    juce::String tooltipFor (int id) const
    {
        if (id == plusId)
            return "Add an LFO";
        if (id == PoolCards::overflowId)
            return "More cards than fit: click for the rest";
        if (id < 0)
            return {};
        if (id >= IlanaSynthAudioProcessor::numLfos)
        {
            const auto& info = extras[(size_t) (id - IlanaSynthAudioProcessor::numLfos)];
            return info.tooltip.isNotEmpty() ? info.tooltip
                                             : info.title + "\nClick to edit it below; drag it onto a knob to modulate that knob.";
        }
        if (hoverRemove)
            return isRouted (id) ? "Remove LFO " + juce::String (id + 1) + " (asks first: it is routed)" : "Remove LFO " + juce::String (id + 1);
        if (hoverB)
            return "LFO " + juce::String (id + 1) + " \xc2\xb7 OUT 2\nThis shape has two outputs: the card drags OUT 1, this tag "
                   "OUT 2 (" + juce::String (LfoSimInfo::get ((int) readParam (id, "_shape")).outB).toLowerCase()
                   + "). Drag it onto a knob to modulate that knob with it.";
        const auto drives = describeModTargets (processorRef, Mod::lfoSourceFor (id));
        return "LFO " + juce::String (id + 1) + (drives.isNotEmpty() ? " (drives " + drives + ")" : juce::String())
               + "\nClick to edit it below; drag it onto a knob to modulate that knob. The x removes it.";
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

    // M8.1: a simulated shape (attractor, physics, random walk) has no fixed
    // cycle to draw: its card shows the live trace of the same simulation the
    // MOD page's picture runs (LfoSimPreview), a rolling window of about two
    // cycles that moves with the frames. The preview is created on first use
    // and only advanced while the card is on screen.
    LfoSimPreview& simPreviewOf (int lfo) const
    {
        auto& slot = simPreviews[(size_t) lfo];
        if (slot == nullptr)
        {
            // A first look starts part-way along (the trace is never empty).
            slot = std::make_unique<LfoSimPreview>();
            const auto settings = processorRef.readLfoSimSettings (lfo);
            for (int i = 0; i < 4; ++i)
                slot->advance (settings, lfoRateHz (lfo), 2.0);
        }
        return *slot;
    }

    double lfoRateHz (int lfo) const
    {
        if (readParam (lfo, "_sync") > 0.5f)
        {
            static const double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
            return (processorRef.getCurrentBpm() / 60.0) / beats[juce::jlimit (0, 10, (int) readParam (lfo, "_div"))];
        }
        return (double) readParam (lfo, "_rate");
    }

    // The preview, stepped by the real time since this card last drew it
    // (so a card that was hidden catches up, and one not drawn costs nothing).
    LfoSimPreview& liveSimPreview (int lfo) const
    {
        auto& preview = simPreviewOf (lfo);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        auto& stamp = simStamps[(size_t) lfo];
        if (stamp > 0.0)
            preview.advance (processorRef.readLfoSimSettings (lfo), lfoRateHz (lfo), juce::jlimit (0.0, 0.25, (now - stamp) * 0.001));
        stamp = now;
        return preview;
    }

    bool anySimShown() const
    {
        for (const auto lfo : visibleLfos())
            if (LfoSimShapes::isSim ((int) readParam (lfo, "_shape")))
                return true;
        return false;
    }

    mutable std::array<double, (size_t) IlanaSynthAudioProcessor::numLfos> simStamps {};
    std::array<IlanaAnim::BlockSmoother, (size_t) IlanaSynthAudioProcessor::numLfos> phaseSmoothers;
    mutable std::array<std::unique_ptr<LfoSimPreview>, (size_t) IlanaSynthAudioProcessor::numLfos> simPreviews;

    float shapeValue (int lfo, int shape, double phase) const
    {
        if (LfoSimShapes::isSim (shape))
            return simPreviewOf (lfo).historyAt (juce::jlimit (0.0, 0.999999, phase));

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

    // A simulated shape's second output, as a small "OUT 2" tag on the
    // title line right after the name, that drags LFO n's second output (UI
    // review 6, I6-24; named, not a bare "B" that read as an A/B state:
    // review 8, S8-16 / V8-28; off the trace, in the header row: review 9,
    // V9-16 / V9-20). Empty for the other shapes.
    static constexpr float outputTagWidth = 40.0f;

    juce::Rectangle<float> outputBTag (int lfo, juce::Rectangle<float> card) const
    {
        if (! LfoSimShapes::isSim ((int) readParam (lfo, "_shape")))
            return {};

        const auto titleFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, "LFO " + juce::String (lfo + 1));
        const auto titleRow = card.reduced (8.0f, 5.0f).withHeight (16.0f);
        return { titleRow.getX() + titleWidth + 8.0f, titleRow.getY() + 1.0f, outputTagWidth, 14.0f };
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

    // Title, what it drives, the rate. No dot after the name: the target tag
    // says it is routed, and the on dot is kept for switches (UI review 8,
    // V8-40). A tag too long for the title line drops onto the trace only
    // when `dropTag` asks (no card does since review 9, V9-16: the trace
    // stays clear and the tooltip names the targets).
    void paintTitleRow (juce::Graphics& g, juce::Rectangle<float> titleRow, const juce::String& title, const juce::String& rateText,
                        const juce::String& targets, juce::Colour colour, bool active, bool dropTag, float afterTitle = 0.0f) const
    {
        const auto titleFont = juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, title);

        g.setColour (active ? colour : IlanaTheme::Ui::text2);
        g.setFont (titleFont);
        g.drawText (title, titleRow, juce::Justification::centredLeft);

        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        g.drawText (rateText, titleRow, juce::Justification::centredRight);

        // What it drives, on the title line between the name and the rate
        // (in the lower corner it sat on the curve).
        const auto rateWidth = rateText.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label)), rateText);
        paintTargetTag (g, titleRow.withTrimmedLeft (titleWidth + 10.0f + afterTitle).withTrimmedRight (rateWidth + 8.0f), targets, colour,
                        dropTag ? titleRow.translated (0.0f, titleRow.getHeight() + 3.0f) : juce::Rectangle<float>());
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

        // The wave filled down to the card's foot (the design's thumbnails).
        auto filled (path);
        filled.lineTo (plot.getRight(), plot.getBottom());
        filled.lineTo (plot.getX(), plot.getBottom());
        filled.closeSubPath();
        g.setColour (colour.withAlpha (alpha * 0.16f));
        g.fillPath (filled);

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
        auto titleRow = inner.removeFromTop (16.0f);

        const auto synced = readParam (lfo, "_sync") > 0.5f;
        const juce::StringArray divisions { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8D", "1/16D" };
        const auto rateText = synced ? divisions[juce::jlimit (0, divisions.size() - 1, (int) readParam (lfo, "_div"))]
                                     : describeValue ("lfo" + juce::String (lfo + 1) + "_rate", readParam (lfo, "_rate")); // as the RATE knob shows it

        const auto targets = cachedTargets (lfo);
        // (The trace sits below the title line, clear of the routing tag: review 11, S11-11.)
        const auto plot = inner.withTrimmedTop (7.0f).reduced (0.0f, 3.0f);
        const auto shape = (int) readParam (lfo, "_shape");

        if (LfoSimShapes::isSim (shape))
            liveSimPreview (lfo);

        // Unassigned LFOs are drawn faint.
        paintTrace (g, plot, colour, active ? 0.95f : (targets.isNotEmpty() ? 0.6f : 0.3f), false,
                    [this, lfo, shape] (double phase) { return shapeValue (lfo, shape, phase); });

        // A simulated shape's dot rides the live end of its trace; the others
        // follow the voice's phase along the cycle.
        if (LfoSimShapes::isSim (shape))
            paintDot (g, plot, colour, 1.0, simPreviewOf (lfo).latestA());
        else
        {
            const auto phase = (double) phaseSmoothers[(size_t) lfo].get (processorRef.getLfoPhase (lfo), true);
            paintDot (g, plot, colour, phase, shapeValue (lfo, shape, phase));
        }

        // The hovered card's "x" takes the rate's corner. Nothing is drawn
        // over the trace (UI review 9, V9-16): a target tag too long for the
        // title line is left to the tooltip.
        const auto tag = outputBTag (lfo, card);
        paintTitleRow (g, hovered ? titleRow.withTrimmedRight (18.0f) : titleRow, "LFO " + juce::String (lfo + 1), rateText, targets,
                       colour, active, false, tag.isEmpty() ? 0.0f : outputTagWidth + 6.0f);

        if (! tag.isEmpty())
        {
            const auto hot = hovered && hoverB;
            g.setColour (IlanaTheme::Ui::bg.withAlpha (0.85f));
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (colour.withAlpha (hot ? 1.0f : 0.6f));
            g.drawRoundedRectangle (tag.reduced (0.5f), 4.0f, 1.0f);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("OUT 2", tag, juce::Justification::centred);
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
        const auto titleRow = inner.removeFromTop (16.0f);
        const auto targets = extraTargets[extra];
        const auto plot = inner.withTrimmedTop (7.0f).reduced (0.0f, 3.0f);

        const auto inUse = isExtraActive (extra);

        if (info.valueAt != nullptr)
        {
            paintTrace (g, plot, info.colour, ! inUse ? 0.22f : active ? 0.95f : (targets.isNotEmpty() ? 0.6f : 0.3f), info.stepped,
                        info.valueAt);

            if (info.phase != nullptr && inUse)
            {
                const auto phase = juce::jlimit (0.0, 0.999999, info.phase());
                paintDot (g, plot, info.colour, phase, info.valueAt (phase));
            }
        }

        // Greyed, with "unused" in place of its rate, while nothing it
        // drives plays.
        if (! inUse)
        {
            const auto taken = PoolCards::paintUnusedTag (g, titleRow, info.title);
            paintTitleRow (g, titleRow.withTrimmedRight (taken), info.title, {}, {}, IlanaTheme::Ui::text3, false, false);
            return;
        }

        paintTitleRow (g, titleRow, info.title, info.rateText != nullptr ? info.rateText() : juce::String(), targets, info.colour,
                       active, false);
    }

    void timerCallback() override
    {
        if (isShowing())
        {
            // Routing an LFO elsewhere, or loading a patch, can add a card.
            if (numCards() != lastCardCount)
                layoutChanged();
            // Simulated shapes move all the time: they redraw every frame
            // (each card steps its simulation by the time since its last draw).
            const auto simulating = anySimShown();
            if (changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this) ^ lfoPhases()) || simulating)
                repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    // Free-running LFOs move with nothing sounding: their dots follow.
    juce::uint64 lfoPhases() const
    {
        juce::uint64 signature = 0;
        for (int lfo = 0; lfo < IlanaSynthAudioProcessor::numLfos; ++lfo)
            signature ^= IlanaAnim::phaseSignature (phaseSmoothers[(size_t) lfo].get (processorRef.getLfoPhase (lfo), true), lfo);
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
    bool fillWidth = false;
    float plusW = PoolCards::plusWidth;
    int lastCardCount = -1;
};
