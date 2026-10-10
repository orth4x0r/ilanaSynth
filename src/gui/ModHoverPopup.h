#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"
#include "ParamControls.h"

// Hovering a modulated knob for a moment opens a small card beside it that
// lists every source routed into it: its colour, name and depth, and a live
// bar of what it is adding right now (the source's value, shaped by the
// slot's polarity and curve, times the depth). Vital shows the same on its
// knobs. One card for the whole editor: it lives in the editor's content so
// it is never clipped by a card. Its rows are live controls too: drag one
// sideways (or up and down) to set that routing's depth, shift for fine,
// double-click for zero; right-click to bypass or remove it.
//
// The same card opens for a source (a macro in the bottom strip, a source
// chip): then it lists where the source goes, and warns about targets whose
// module is off ("FX Reverb is switched off"), which such a routing can't be
// heard through (UI review 6, V6-26 / S6-15). A macro's card also holds
// its EVOLVE (the macro drifting on its own within a range, and how fast)
// and FREEZE: per-macro behaviour lives on the macro (review 8, I8-13).
class ModHoverPopup : public juce::Component,
                      private IlanaAnim::FrameTimer
{
public:
    explicit ModHoverPopup (IlanaSynthAudioProcessor& processor) : processorRef (processor)
    {
        freezeButton.setButtonText ("FREEZE");
        freezeButton.setTooltip ("Freeze\nKeeps every evolving macro where it has drifted to, and stops the drift (all macros).");
        freezeButton.onClick = [this] { processorRef.performEdit ("Freeze Evolve", [this] { processorRef.freezeEvolve(); }); };
        addChildComponent (freezeButton);

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
        modHoverHooks().showSource = [] (juce::Component& anchor, int source)
        {
            if (auto* popup = instance(); popup != nullptr && source > 0)
                popup->openForSource (anchor, source);
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
    bool isShowingSource() const { return isVisible() && sourceMode; }
    int getNumRows() const { return (int) rows.size(); }
    int getRowSlot (int row) const { return juce::isPositiveAndBelow (row, getNumRows()) ? rows[(size_t) row].slot : -1; }
    // Why a row's target can't be heard now (empty when it can); the tests.
    juce::String getRowWarning (int row) const { return juce::isPositiveAndBelow (row, getNumRows()) ? rows[(size_t) row].idle : juce::String(); }
    bool isEngaged() const { return isMouseOver (true) || dragRow >= 0 || menuOpen; }
    // A macro's EVOLVE controls on its card (null on other cards); the tests.
    ValueSliderControl* getEvolveAmount() const { return evolveAmount.get(); }
    ValueSliderControl* getEvolveRate() const { return evolveRate.get(); }
    juce::TextButton& getFreezeButton() { return freezeButton; }
    void close() { dismiss(); }

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

        const auto bypassed = rows[(size_t) row].bypass;
        processorRef.performEdit (action == RowAction::remove ? "Remove modulation"
                                  : bypassed                  ? "Enable modulation"
                                                              : "Bypass modulation",
                                  [&]
                                  {
                                      if (action == RowAction::remove)
                                          processorRef.clearModSlot (slot);
                                      else
                                          withGesture (slot, "byp", [&] (juce::RangedAudioParameter& p)
                                                       { p.setValueNotifyingHost (p.convertTo0to1 (bypassed ? 0.0f : 1.0f)); });
                                  });

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
        {
            processorRef.beginEdit (ModNames::source (rows[(size_t) row].source) + " depth");
            dragParameter->beginChangeGesture();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragRow < 0 || dragParameter == nullptr)
            return;

        // Sideways along the bar, or up and down like the knob's rings.
        const auto fine = event.mods.isShiftDown() ? 0.2f : 1.0f;
        const auto travel = (float) (event.getDistanceFromDragStartX() - event.getDistanceFromDragStartY());
        setDragDepth (dragStartDepth + travel * 0.006f * fine);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragParameter != nullptr)
        {
            dragParameter->endChangeGesture();
            processorRef.endEdit();
        }

        dragParameter = nullptr;
        dragRow = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto row = rowAt (event.position.toInt());

        if (row < 0 || event.mods.isPopupMenu())
            return;

        processorRef.performEdit ("Zero " + ModNames::source (rows[(size_t) row].source) + " depth", [&]
        {
            withGesture (rows[(size_t) row].slot, "amt", [] (juce::RangedAudioParameter& p)
                         { p.setValueNotifyingHost (p.convertTo0to1 (0.0f)); });
        });
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.0f);
        g.setColour (IlanaTheme::Ui::raised);
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (sourceMode ? modSourceColour (cardSource).withAlpha (0.55f) : IlanaTheme::Ui::line);
        g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

        auto area = getLocalBounds().reduced (10, padTop);
        auto header = area.removeFromTop (headerHeight);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));

        if (sourceMode)
        {
            g.setColour (modSourceColour (cardSource));
            g.fillEllipse (header.removeFromLeft (8).toFloat().withSizeKeepingCentre (7.0f, 7.0f));
            header.removeFromLeft (6);
            g.setColour (IlanaTheme::Ui::text);
            g.drawText (title, header, juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.drawText (rows.empty() ? juce::String ("ROUTED NOWHERE")
                                     : juce::String ((int) rows.size()) + (rows.size() == 1 ? " TARGET" : " TARGETS"),
                        header, juce::Justification::centredRight);
        }
        else
        {
            g.setColour (IlanaTheme::Ui::text2);
            g.drawText (title, header, juce::Justification::centredLeft);
        }

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
            const auto idle = row.idle.isNotEmpty();
            const auto dim = row.bypass || idle ? 0.45f : 1.0f;

            // A target that can't be heard gets an amber mark in place of the
            // colour dot.
            const auto marker = line.removeFromLeft (8).toFloat();
            if (idle)
            {
                g.setColour (warningColour());
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                g.drawText ("!", marker.withSizeKeepingCentre (8.0f, (float) rowHeight), juce::Justification::centred);
            }
            else
            {
                g.setColour (colour.withAlpha (dim));
                g.fillEllipse (marker.withSizeKeepingCentre (7.0f, 7.0f));
            }
            line.removeFromLeft (6);

            g.setColour (IlanaTheme::Ui::text.withAlpha (dim));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            const auto name = sourceMode ? ModNames::destination (row.destination, processorRef) : ModNames::sourceUpper (row.source, &processorRef);
            IlanaTheme::drawFitted (g, name, line.removeFromLeft (nameWidth()), juce::Justification::centredLeft, 1);

            const auto depthText = (row.depth >= 0.0f ? "+" : "") + juce::String (juce::roundToInt (row.depth * 100.0f)) + "%";
            g.setColour (IlanaTheme::Ui::text2.withAlpha (dim));
            g.drawText (row.bypass ? juce::String ("OFF") : depthText, line.removeFromRight (40), juce::Justification::centredRight);
            line.removeFromRight (6);

            // The depth, dim, from the centre (right is up, left down), and
            // over it the live contribution.
            const auto bar = line.toFloat().withSizeKeepingCentre ((float) line.getWidth(), 4.0f);
            const auto half = bar.getWidth() * 0.5f;
            const auto span = [&bar, half] (float amount)
            {
                amount = juce::jlimit (-1.0f, 1.0f, amount);
                return juce::Rectangle<float> (bar.getCentreX() + juce::jmin (0.0f, amount) * half, bar.getY(), std::abs (amount) * half,
                                               bar.getHeight());
            };
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (colour.withAlpha ((row.bypass ? 0.12f : 0.3f) * dim));
            g.fillRoundedRectangle (span (row.depth), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (1.0f, 8.0f).withCentre (bar.getCentre()));

            if (! row.bypass)
            {
                g.setColour (colour.withAlpha (idle ? 0.5f : 1.0f));
                g.fillRoundedRectangle (span (row.live), 2.0f);
            }
        }

        // Why the amber rows do nothing, one line each.
        if (! warnings.isEmpty())
        {
            area.removeFromTop (3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.setColour (warningColour());

            for (const auto& warning : warnings)
                IlanaTheme::drawFitted (g, "! " + warning.substring (0, 1).toUpperCase() + warning.substring (1) + ": no effect now",
                                  area.removeFromTop (warningHeight), juce::Justification::centredLeft, 1);
        }

        if (sourceMode && rows.empty())
        {
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
            g.setColour (IlanaTheme::Ui::text2);
            IlanaTheme::drawFitted (g, "Drag it onto any knob to route it there.", area.removeFromTop (rowHeight),
                              juce::Justification::centredLeft, 1);
        }

        if (evolveMacro >= 0)
            paintEvolve (g);
    }

    // A macro's EVOLVE: a hairline, its name, where the macro is set and
    // where it has drifted to, then its two sliders' labels.
    void paintEvolve (juce::Graphics& g)
    {
        const auto section = evolveArea();
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (section.getX(), section.getY(), section.getWidth(), 1);

        auto header = section.withTrimmedTop (5).withHeight (evolveHeaderHeight);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text);
        g.drawText ("EVOLVE", header, juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        const auto evolving = readParam ("macro" + juce::String (evolveMacro + 1) + "_evolve") > 0.0005f;
        g.drawText (evolving ? "drifts in range" : "off: set AMOUNT", header.withTrimmedLeft (52), juce::Justification::centredLeft);

        if (evolving)
        {
            // Set (a tick) and drifted to (a dot), left of FREEZE.
            const auto bar = juce::Rectangle<float> ((float) freezeButton.getX() - 76.0f, (float) header.getCentreY() - 2.0f, 66.0f, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (bar, 2.0f);
            const auto set = readParam ("macro" + juce::String (evolveMacro + 1));
            const auto now = processorRef.macroValue (evolveMacro);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.fillRect (bar.getX() + bar.getWidth() * set - 1.0f, bar.getY() - 3.0f, 2.0f, bar.getHeight() + 6.0f);
            g.setColour (modSourceColour (cardSource));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ bar.getX() + bar.getWidth() * now, bar.getCentreY() }));
        }

        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.setColour (IlanaTheme::Ui::text2);
        for (auto* slider : { evolveAmount.get(), evolveRate.get() })
            if (slider != nullptr)
                g.drawText (slider == evolveAmount.get() ? "AMOUNT" : "RATE",
                            juce::Rectangle<int> (section.getX(), slider->getY(), evolveLabelWidth, slider->getHeight()),
                            juce::Justification::centredLeft);
    }

    void resized() override
    {
        if (evolveMacro < 0)
            return;

        auto section = evolveArea().withTrimmedTop (5);
        auto header = section.removeFromTop (evolveHeaderHeight);
        freezeButton.setBounds (header.removeFromRight (64).reduced (0, 1));
        section.removeFromTop (3);
        for (auto* slider : { evolveAmount.get(), evolveRate.get() })
            if (slider != nullptr)
                slider->setBounds (section.removeFromTop (evolveRowHeight).withTrimmedLeft (evolveLabelWidth).reduced (0, 2));
    }

    static juce::Colour warningColour() { return juce::Colour (0xffffb020); }

private:
    struct Row
    {
        int slot = -1;
        int source = 0;
        int destination = 0;
        float depth = 0.0f;
        float live = 0.0f;
        bool bypass = false;
        juce::String idle;
    };

    static constexpr int padTop = 7;
    static constexpr int headerHeight = 16;
    static constexpr int rowHeight = 17;
    static constexpr int warningHeight = 14;
    static constexpr int maxRows = 12;

    // A knob's card is as wide as its title and its sources' names need,
    // so it covers no more of the page than it must (S8-35).
    int nameWidth() const { return sourceMode ? 150 : knobNameWidth; }
    int cardWidth() const { return sourceMode ? 290 : knobCardWidth; }

    void fitKnobCard()
    {
        const auto labelFont = IlanaTheme::font (IlanaTheme::TextSize::label);
        auto widest = 0;
        for (const auto& row : rows)
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidthInt (labelFont, ModNames::sourceUpper (row.source, &processorRef)));
        knobNameWidth = juce::jlimit (48, 120, widest + 6);
        const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (IlanaTheme::font (IlanaTheme::TextSize::tiny, true), title);
        knobCardWidth = juce::jlimit (170, 260, juce::jmax (titleWidth + 24, 20 + 14 + knobNameWidth + 64 + 46));
    }

    static constexpr int evolveHeaderHeight = 18, evolveRowHeight = 24, evolveLabelWidth = 56;
    static constexpr int evolveHeight = 6 + evolveHeaderHeight + 3 + 2 * evolveRowHeight;

    int cardHeight() const
    {
        const auto shownRows = juce::jmax (sourceMode ? 1 : 0, (int) rows.size());
        return padTop * 2 + headerHeight + shownRows * rowHeight + (warnings.isEmpty() ? 0 : 3 + warnings.size() * warningHeight)
               + (evolveMacro >= 0 ? evolveHeight : 0);
    }

    // The EVOLVE section: the bottom of a macro's card.
    juce::Rectangle<int> evolveArea() const
    {
        return getLocalBounds().reduced (10, padTop).removeFromBottom (evolveHeight);
    }

    float readParam (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    // A macro's card carries its EVOLVE sliders; other cards none.
    void setEvolveMacro (int macro)
    {
        if (macro == evolveMacro)
            return;

        evolveAmount.reset();
        evolveRate.reset();
        evolveMacro = macro;
        freezeButton.setVisible (macro >= 0);

        if (macro < 0)
            return;

        const auto prefix = "macro" + juce::String (macro + 1);
        evolveAmount = std::make_unique<ValueSliderControl> (processorRef.apvts, prefix + "_evolve");
        evolveRate = std::make_unique<ValueSliderControl> (processorRef.apvts, prefix + "_evolve_rate");
        for (auto* slider : { evolveAmount.get(), evolveRate.get() })
        {
            slider->getSlider().setColour (juce::Slider::trackColourId, modSourceColour (cardSource));
            addAndMakeVisible (*slider);
        }
    }

    void open (juce::Component& knob, int destinationIn, const juce::String& titleIn)
    {
        owner = &knob;
        sourceMode = false;
        setEvolveMacro (-1);
        destination = destinationIn;
        title = titleIn;
        readRows();
        fitKnobCard();

        if (rows.empty() || getParentComponent() == nullptr)
        {
            dismiss();
            return;
        }

        // Beside the knob, to the right unless that runs off the content.
        auto* parent = getParentComponent();
        const auto knobArea = parent->getLocalArea (&knob, knob.getLocalBounds());
        const auto height = cardHeight();
        auto x = knobArea.getRight() + 4;

        if (x + cardWidth() > parent->getWidth() - 6)
            x = knobArea.getX() - cardWidth() - 4;

        const auto y = juce::jlimit (6, juce::jmax (6, parent->getHeight() - height - 6), knobArea.getY());
        setBounds (juce::jmax (6, x), y, cardWidth(), height);
        setVisible (true);
        toFront (false);
        startTimerHz (30);
        repaint();
    }

    // A source's card: above its macro or chip (they sit at the bottom).
    void openForSource (juce::Component& anchor, int source)
    {
        if (getParentComponent() == nullptr)
            return;

        owner = &anchor;
        sourceMode = true;
        cardSource = source;
        title = ModNames::sourceUpper (source, &processorRef);
        leaveSeconds = 0.0f;
        setEvolveMacro (Mod::macroIndexFor ((Mod::Source) source));
        readRows();
        placeAboveAnchor();
        setVisible (true);
        toFront (false);
        startTimerHz (30);
        repaint();
    }

    void placeAboveAnchor()
    {
        auto* parent = getParentComponent();

        if (parent == nullptr || owner == nullptr)
            return;

        const auto anchorArea = parent->getLocalArea (owner.getComponent(), owner->getLocalBounds());
        const auto height = cardHeight();
        const auto x = juce::jlimit (6, juce::jmax (6, parent->getWidth() - cardWidth() - 6), anchorArea.getX());
        const auto y = juce::jmax (6, anchorArea.getY() - height - 4);
        setBounds (x, y, cardWidth(), height);
        resized(); // (the same size for another macro still moves its sliders)
    }

    void dismiss()
    {
        if (dragParameter != nullptr)
        {
            dragParameter->endChangeGesture();
            processorRef.endEdit();
        }

        dragParameter = nullptr;
        dragRow = -1;
        hoverRow = -1;
        owner = nullptr;
        sourceMode = false;
        setVisible (false);
        stopTimer();
        // (Not from inside a slider's own callback: the card is hidden and
        // its sliders go when it next opens for something else.)
    }

    void readRows()
    {
        rows.clear();
        warnings.clear();

        for (int i = 0; i < Mod::maxSlots && (int) rows.size() < maxRows; ++i)
        {
            const auto slot = processorRef.readModSlot (i);

            if (slot.source == Mod::Source::None || slot.destination == 0)
                continue;

            if (sourceMode ? (int) slot.source != cardSource : slot.destination != destination)
                continue;

            Row row;
            row.slot = i;
            row.source = (int) slot.source;
            row.destination = slot.destination;
            row.depth = slot.depth;
            row.bypass = slot.bypass;
            row.live = Mod::shape (slot, processorRef.getSourceDisplayValue ((int) slot.source)) * slot.depth;

            // (A knob's card is about the knob: its module's state shows on
            // the page already.)
            if (sourceMode)
                row.idle = ModNames::whyDestinationIsIdle (processorRef, slot.destination);

            if (row.idle.isNotEmpty() && ! row.bypass && warnings.size() < 3)
                warnings.addIfNotAlreadyThere (row.idle);

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

        // A source's card closes a moment after the mouse leaves both it and
        // its macro or chip (a knob closes its own). A held card (the tests,
        // snapshots) stays.
        if (sourceMode && ! menuOpen && dragRow < 0 && ! held)
        {
            // (A slider being dragged keeps it, wherever the mouse goes.)
            if (owner->isMouseOver (true) || isMouseOver (true) || isMouseButtonDown (true))
                leaveSeconds = 0.0f;
            else if ((leaveSeconds += frameSeconds()) > 0.3f)
            {
                dismiss();
                return;
            }
        }

        const auto count = rows.size();
        const auto warningCount = warnings.size();
        readRows();

        if (rows.empty() && ! sourceMode)
            dismiss();
        else if ((rows.size() != count || warnings.size() != warningCount) && dragRow < 0)
        {
            if (sourceMode)
            {
                placeAboveAnchor();
                repaint();
            }
            else
                open (*owner, destination, title);
        }
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
    // IlanaAnim::showing (*this), which is false for an editor rendered off screen.
    static bool shownInTree (const juce::Component& component)
    {
        for (auto* c = &component; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return true;
    }

public:
    // A source card that stays until closed (tests and snapshots).
    void holdOpen (bool shouldHold) { held = shouldHold; }

private:
    IlanaSynthAudioProcessor& processorRef;
    juce::Component::SafePointer<juce::Component> owner;
    int destination = 0;
    int cardSource = 0;
    bool sourceMode = false, held = false;
    float leaveSeconds = 0.0f;
    juce::String title;
    std::vector<Row> rows;
    juce::StringArray warnings;
    int hoverRow = -1;
    int dragRow = -1;
    int dragSlot = -1;
    float dragStartDepth = 0.0f;
    juce::RangedAudioParameter* dragParameter = nullptr;
    bool menuOpen = false;
    int evolveMacro = -1;
    int knobNameWidth = 92, knobCardWidth = 230;
    std::unique_ptr<ValueSliderControl> evolveAmount, evolveRate;
    juce::TextButton freezeButton;
};
