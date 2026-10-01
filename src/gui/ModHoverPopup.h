#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"

// Hovering a modulated knob for a moment opens a small card beside it that
// lists every source routed into it: its colour, name and depth, and a live
// bar of what it is adding right now (the source's value, shaped by the
// slot's polarity and curve, times the depth). Vital shows the same on its
// knobs. One card for the whole editor: it lives in the editor's content so
// it is never clipped by a card. Its rows are live controls too: drag one
// sideways (or up and down) to set that routing's depth, shift for fine,
// double-click for zero; right-click to bypass or remove it.
class ModHoverPopup : public juce::Component,
                      private IlanaAnim::FrameTimer
{
public:
    explicit ModHoverPopup (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        setAlwaysOnTop (true);
        setRepaintsOnMouseActivity (true);
        instance() = this;
        modHoverHooks().show = [] (juce::Component& knob, int destination, const juce::String& text) { showFor (knob, destination, text); };
        modHoverHooks().hide = [] (const juce::Component& knob) { hideFor (knob); };
        modHoverHooks().engaged = [] (const juce::Component& knob)
        {
            auto* popup = instance();
            return popup != nullptr && popup->isShowingFor (knob) && popup->isEngaged();
        };
    }

    ~ModHoverPopup() override
    {
        if (instance() == this)
        {
            instance() = nullptr;
            modHoverHooks() = {};
        }
    }

    // Called by a knob as the mouse enters (after its hover delay) and leaves.
    static void showFor (juce::Component& knob, int destination, const juce::String& title)
    {
        if (auto* popup = instance(); popup != nullptr && destination != 0)
            popup->open (knob, destination, title);
    }

    static void hideFor (const juce::Component& knob)
    {
        if (auto* popup = instance(); popup != nullptr && popup->owner.getComponent() == &knob)
            popup->dismiss();
    }

    static ModHoverPopup*& instance()
    {
        static ModHoverPopup* popup = nullptr;
        return popup;
    }

    bool isShowingFor (const juce::Component& knob) const { return isVisible() && owner.getComponent() == &knob; }
    int getNumRows() const { return (int) rows.size(); }
    int getRowSlot (int row) const { return juce::isPositiveAndBelow (row, getNumRows()) ? rows[(size_t) row].slot : -1; }
    bool isEngaged() const { return isMouseOver (true) || dragRow >= 0 || menuOpen; }

    juce::Rectangle<int> getRowBounds (int row) const
    {
        return { 4, padTop + headerHeight + row * rowHeight, getWidth() - 8, rowHeight };
    }

    enum class RowAction { toggleBypass = 1, remove };

    // A row's right-click menu choice (public for the tests).
    void applyRowAction (int row, RowAction action)
    {
        const auto slot = getRowSlot (row);

        if (slot < 0)
            return;

        if (action == RowAction::remove)
            processorRef.clearModSlot (slot);
        else
            withGesture (slot, "byp", [&] (juce::RangedAudioParameter& p)
                         { p.setValueNotifyingHost (p.convertTo0to1 (rows[(size_t) row].bypass ? 0.0f : 1.0f)); });

        refresh();
    }

    void mouseMove (const juce::MouseEvent& event) override { setHoverRow (rowAt (event.position.toInt())); }
    void mouseExit (const juce::MouseEvent&) override { setHoverRow (-1); }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.position.toInt());

        if (row < 0)
            return;

        if (event.mods.isPopupMenu())
        {
            showRowMenu (row);
            return;
        }

        dragRow = row;
        dragSlot = rows[(size_t) row].slot;
        dragStartDepth = rows[(size_t) row].depth;
        dragParameter = processorRef.apvts.getParameter (processorRef.getModSlotParamId (dragSlot, "amt"));

        if (dragParameter != nullptr)
            dragParameter->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragRow < 0 || dragParameter == nullptr)
            return;

        // Sideways along the bar, or up and down like the knob's dots.
        const auto fine = event.mods.isShiftDown() ? 0.2f : 1.0f;
        const auto travel = (float) (event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY());
        setDragDepth (dragStartDepth + travel * 0.006f * fine);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParameter != nullptr)
            dragParameter->endChangeGesture();

        dragParameter = nullptr;
        dragRow = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.position.toInt());

        if (row < 0 || event.mods.isPopupMenu())
            return;

        withGesture (rows[(size_t) row].slot, "amt", [] (juce::RangedAudioParameter& p)
                     { p.setValueNotifyingHost (p.convertTo0to1 (0.0f)); });
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.0f);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

        auto area = getLocalBounds().reduced (10, padTop);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (title.toUpperCase(), area.removeFromTop (headerHeight), juce::Justification::centredLeft);

        const auto names = Mod::getSourceNames();

        for (int index = 0; index < (int) rows.size(); ++index)
        {
            const auto& row = rows[(size_t) index];
            auto line = area.removeFromTop (rowHeight);

            if (index == dragRow || (index == hoverRow && dragRow < 0))
            {
                g.setColour (juce::Colours::white.withAlpha (index == dragRow ? 0.09f : 0.05f));
                g.fillRoundedRectangle (getRowBounds (index).toFloat(), 4.0f);
            }

            const auto colour = modSourceColour (row.source);
            const auto dim = row.bypass ? 0.4f : 1.0f;

            g.setColour (colour.withAlpha (dim));
            g.fillEllipse (line.removeFromLeft (8).toFloat().withSizeKeepingCentre (7.0f, 7.0f));
            line.removeFromLeft (6);

            g.setColour (IlanaTheme::Ui::text.withAlpha (dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            const auto name = juce::isPositiveAndBelow (row.source, names.size()) ? names[row.source] : juce::String ("?");
            g.drawText (name.toUpperCase(), line.removeFromLeft (nameWidth), juce::Justification::centredLeft);

            const auto depthText = (row.depth >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (row.depth * 100.0f)) + "%";
            g.setColour (IlanaTheme::Ui::text2.withAlpha (dim));
            g.drawText (row.bypass ? juce::String ("OFF") : depthText, line.removeFromRight (40), juce::Justification::centredRight);
            line.removeFromRight (6);

            // The live contribution, from the centre: right is up, left down.
            const auto bar = line.toFloat().withSizeKeepingCentre ((float) line.getWidth(), 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (1.0f, 8.0f).withCentre (bar.getCentre()));

            if (! row.bypass)
            {
                const auto amount = juce::jlimit (-1.0f, 1.0f, row.live);
                const auto half = bar.getWidth() * 0.5f;
                const auto x0 = bar.getCentreX() + juce::jmin (0.0f, amount) * half;
                g.setColour (colour);
                g.fillRoundedRectangle (juce::Rectangle<float> (x0, bar.getY(), std::abs (amount) * half, bar.getHeight()), 2.0f);
            }
        }
    }

private:
    struct Row
    {
        int slot = -1;
        int source = 0;
        float depth = 0.0f;
        float live = 0.0f;
        bool bypass = false;
    };

    static constexpr int padTop = 7;
    static constexpr int headerHeight = 16;
    static constexpr int rowHeight = 17;
    static constexpr int nameWidth = 78;
    static constexpr int width = 214;

    void open (juce::Component& knob, int destinationIn, const juce::String& titleIn)
    {
        owner = &knob;
        destination = destinationIn;
        title = titleIn;
        readRows();

        if (rows.empty() || getParentComponent() == nullptr)
        {
            dismiss();
            return;
        }

        // Beside the knob, to the right unless that runs off the content.
        auto* parent = getParentComponent();
        const auto knobArea = parent->getLocalArea (&knob, knob.getLocalBounds());
        const auto height = padTop * 2 + headerHeight + (int) rows.size() * rowHeight;
        auto x = knobArea.getRight() + 4;

        if (x + width > parent->getWidth() - 6)
            x = knobArea.getX() - width - 4;

        const auto y = juce::jlimit (6, juce::jmax (6, parent->getHeight() - height - 6), knobArea.getY());
        setBounds (juce::jmax (6, x), y, width, height);
        setVisible (true);
        toFront (false);
        startTimerHz (30);
        repaint();
    }

    void dismiss()
    {
        if (dragParameter != nullptr)
            dragParameter->endChangeGesture();

        dragParameter = nullptr;
        dragRow = -1;
        hoverRow = -1;
        owner = nullptr;
        setVisible (false);
        stopTimer();
    }

    void readRows()
    {
        rows.clear();

        for (int i = 0; i < Mod::maxSlots && rows.size() < 8; ++i)
        {
            const auto slot = processorRef.readModSlot (i);

            if (slot.destination != destination || slot.source == Mod::Source::None)
                continue;

            Row row;
            row.slot = i;
            row.source = (int) slot.source;
            row.depth = slot.depth;
            row.bypass = slot.bypass;
            row.live = Mod::shape (slot, processorRef.getSourceDisplayValue ((int) slot.source)) * slot.depth;
            rows.push_back (row);
        }
    }

    void timerCallback() override { refresh(); }

    void refresh()
    {
        if (owner == nullptr || ! shownInTree (*owner))
        {
            dismiss();
            return;
        }

        const auto count = rows.size();
        readRows();

        if (rows.empty())
            dismiss();
        else if (rows.size() != count && dragRow < 0)
            open (*owner, destination, title);
        else
            repaint();
    }

    int rowAt (juce::Point<int> position) const
    {
        for (int row = 0; row < (int) rows.size(); ++row)
            if (getRowBounds (row).contains (position))
                return row;
        return -1;
    }

    void setHoverRow (int row)
    {
        if (row != hoverRow)
        {
            hoverRow = row;
            setMouseCursor (row >= 0 ? juce::MouseCursor::UpDownLeftRightResizeCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void setDragDepth (float depth)
    {
        depth = juce::jlimit (-1.0f, 1.0f, depth);
        dragParameter->setValueNotifyingHost (dragParameter->convertTo0to1 (depth));

        if (juce::isPositiveAndBelow (dragRow, getNumRows()))
            rows[(size_t) dragRow].depth = depth;

        repaint();
    }

    template <typename Change>
    void withGesture (int slot, const char* field, Change&& change)
    {
        if (auto* parameter = processorRef.apvts.getParameter (processorRef.getModSlotParamId (slot, field)))
        {
            parameter->beginChangeGesture();
            change (*parameter);
            parameter->endChangeGesture();
        }
    }

    void showRowMenu (int row)
    {
        juce::PopupMenu menu;
        menu.addItem ((int) RowAction::toggleBypass, "Bypass", true, rows[(size_t) row].bypass);
        menu.addItem ((int) RowAction::remove, "Remove");
        menuOpen = true;
        const auto slot = rows[(size_t) row].slot;
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (getRowBounds (row))),
                            [safeThis = juce::Component::SafePointer<ModHoverPopup> (this), slot] (int result)
                            {
                                if (safeThis == nullptr)
                                    return;

                                safeThis->menuOpen = false;

                                // The row may have moved while the menu was open.
                                for (int index = 0; index < safeThis->getNumRows() && result > 0; ++index)
                                    if (safeThis->getRowSlot (index) == slot)
                                        return safeThis->applyRowAction (index, (RowAction) result);
                            });
    }

    // Visible up to the window (a page switch hides the knob's page; the
    // window itself is not asked). Not
    // isShowing(), which is false for an editor rendered off screen.
    static bool shownInTree (const juce::Component& component)
    {
        for (auto* c = &component; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return true;
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::Component::SafePointer<juce::Component> owner;
    int destination = 0;
    juce::String title;
    std::vector<Row> rows;
    int hoverRow = -1;
    int dragRow = -1;
    int dragSlot = -1;
    float dragStartDepth = 0.0f;
    juce::RangedAudioParameter* dragParameter = nullptr;
    bool menuOpen = false;
};
