#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ModNames.h"
#include "ParamControls.h"
#include "AnimationUtils.h"

// A compact knob with its name and value beside it, for the bottom strip.
// Macro units also carry an editable name and act as drag sources.
class StripKnob : public juce::Component,
                  public juce::SettableTooltipClient,
                  private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~StripKnob() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    StripKnob (IlanaSynthAudioProcessor& p, const juce::String& parameterID, const juce::String& title,
               int macroIndexIn = -1, juce::Colour accent = IlanaTheme::accent(), bool followsTheme = true)
        : processorRef (p),
          knob (p.apvts, parameterID, title, accent, followsTheme),
          defaultTitle (title),
          macroIndex (macroIndexIn)
    {
        knob.setCompact (true);
        addAndMakeVisible (knob);
        parameter = p.apvts.getParameter (parameterID);
        if (macroIndex >= 0)
            knob.setIsSourceKnob(); // a macro is a source, not a target

        if (macroIndex >= 0)
        {
            nameEditor.setJustificationType (juce::Justification::centredLeft);
            nameEditor.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            nameEditor.setColour (juce::Label::textColourId, IlanaTheme::Ui::text);
            nameEditor.setColour (juce::Label::textWhenEditingColourId, juce::Colours::white);
            nameEditor.setColour (juce::Label::backgroundWhenEditingColourId, IlanaTheme::Ui::well);
            nameEditor.setColour (juce::Label::outlineWhenEditingColourId, IlanaTheme::accent());
            nameEditor.setEditable (false, true, false);
            nameEditor.setInterceptsMouseClicks (false, false);
            nameEditor.onTextChange = [this]
            {
                auto text = nameEditor.getText().trim().substring (0, 16);
                processorRef.setMacroName (macroIndex, text);
                refreshName();
            };
            addChildComponent (nameEditor);

            setTooltip ("Turn to sweep everything this macro is routed to; rest on it to see where it goes.  "
                        "Drag its name onto any knob to route it there.  Double-click the name to rename it.");
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

        if (macroIndex >= 0)
            refreshTargets();

        startPollingHz (60); // follows its value, no animation of its own
    }

    void refreshName()
    {
        if (macroIndex >= 0)
            nameEditor.setText (processorRef.getMacroName (macroIndex), juce::dontSendNotification);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto text = textArea();
        const auto title = macroIndex >= 0 ? processorRef.getMacroName (macroIndex) : defaultTitle;

        // Drag handle: a little grip, shown on hover, so the name reads as
        // grabbable without crowding it the rest of the time.
        if (macroIndex >= 0 && hover)
        {
            const auto grip = text.removeFromRight (10).toFloat();

            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 2; ++col)
                {
                    g.setColour (juce::Colours::white.withAlpha (hover ? 0.45f : 0.2f));
                    g.fillEllipse (grip.getX() + 2.0f + (float) col * 4.0f,
                                   grip.getCentreY() - 6.0f + (float) row * 5.0f, 2.0f, 2.0f);
                }
        }

        if (! nameEditor.isBeingEdited())
        {
            auto nameArea = text.removeFromTop (text.getHeight() / 2);

            // A target whose module is off (a reverb mix with the reverb
            // switched off) can't be heard: a small amber warning sign after
            // the name says so, its tooltip which (V7-28), and the macro's
            // card (rest on it) the same.
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
            const auto name = title.toUpperCase();
            // The warning is a word in the warning colour, "TALK · OFF" (review 10,
            // I10-14), not a bare triangle.
            // (How many of its targets sit in a module that is off: "· 1 OFF", not a bare
            // "OFF" that does not say what is off; I14-8.)
            const juce::String warning (juce::String::fromUTF8 ("\xc2\xb7 ") + juce::String (juce::jmax (1, idleTargets)) + " OFF");
            const auto warningFont = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            // What the macro does, in a number: "→ 3" routes (review 12, S12-10).
            const auto routeText = routedTargets > 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + juce::String (routedTargets) : juce::String();
            const auto routeWidth = routeText.isEmpty() ? 0 : juce::GlyphArrangement::getStringWidthInt (warningFont, routeText) + 8;
            const auto markWidth = routeWidth + (idleTargets > 0 ? juce::GlyphArrangement::getStringWidthInt (warningFont, warning) + 10 : 0) + (evolving ? 16 : 0);
            const auto nameWidth = juce::jmin (nameArea.getWidth() - markWidth,
                                               juce::GlyphArrangement::getStringWidthInt (font, name) + 1);
            // A macro routed nowhere reads quietly, so the preset's own
            // macros stand out (V8-37).
            const auto quiet = macroIndex >= 0 && ! isAssigned();
            g.setColour (hover ? IlanaTheme::Ui::text : quiet ? IlanaTheme::Ui::text3 : IlanaTheme::Ui::text2);
            g.setFont (font);
            IlanaTheme::drawFitted (g, name, nameArea.removeFromLeft (nameWidth), juce::Justification::bottomLeft, 1);

            if (routeWidth > 0 && nameArea.getWidth() >= routeWidth)
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (warningFont);
                g.drawText (routeText, nameArea.removeFromLeft (routeWidth).withTrimmedLeft (6), juce::Justification::bottomLeft);
            }

            // An evolving macro carries a small drift wave after its name
            // (its EVOLVE is on its card).
            if (evolving)
            {
                const auto capHeight = juce::Font (font).getAscent() * 0.72f;
                const auto wave = juce::Rectangle<float> ((float) nameArea.getX() + 4.0f,
                                                          (float) nameArea.getBottom() - juce::Font (font).getDescent() - capHeight,
                                                          11.0f, capHeight);
                juce::Path path;
                for (int i = 0; i <= 12; ++i)
                {
                    const auto t = (float) i / 12.0f;
                    const auto point = juce::Point<float> (wave.getX() + t * wave.getWidth(),
                                                           wave.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi) * wave.getHeight() * 0.4f);
                    if (i == 0)
                        path.startNewSubPath (point);
                    else
                        path.lineTo (point);
                }
                g.setColour (modSourceColour ((int) Mod::macroSourceFor (macroIndex)));
                g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                nameArea.removeFromLeft (16);
            }

            markBounds = {};
            if (idleTargets > 0)
            {
                // The word after the name, on its baseline.
                const auto width = juce::GlyphArrangement::getStringWidthInt (warningFont, warning) + 2;
                const auto mark = nameArea.removeFromLeft (6 + width).withTrimmedLeft (6);
                g.setColour (juce::Colour (0xffffb020));
                g.setFont (warningFont);
                g.drawText (warning, mark, juce::Justification::bottomLeft);
                markBounds = mark.expanded (3).getIntersection (getLocalBounds());
            }
        }
        else
        {
            text.removeFromTop (text.getHeight() / 2);
        }

        assignBounds = {};

        if (macroIndex >= 0 && ! isAssigned() && (! quietAssign || hover))
        {
            // Nothing to move yet: a small button that adds a routing from
            // this macro in the matrix (review 9, I9-20), so macros 5-8 need
            // no drag.
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const juce::String label ("+ ASSIGN");
            const auto width = juce::jmin (text.getWidth(), juce::GlyphArrangement::getStringWidthInt (font, label) + 14);
            assignBounds = text.withWidth (width).withSizeKeepingCentre (width, 16).withX (text.getX());
            const auto over = hover && assignBounds.contains (getMouseXYRelative());
            g.setColour (over ? IlanaTheme::accent().withAlpha (0.22f) : IlanaTheme::Ui::raised);
            g.fillRoundedRectangle (assignBounds.toFloat(), 4.0f);
            g.setColour (over ? IlanaTheme::accent() : IlanaTheme::Ui::line.brighter (0.2f));
            g.drawRoundedRectangle (assignBounds.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (over ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
            g.setFont (font);
            g.drawText (label, assignBounds, juce::Justification::centred, false);
            return;
        }

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, false, true)); // a live value
        g.drawText (valueText(), text, juce::Justification::topLeft, true);
    }

    // Whether the macro moves anything: a routing, or its own EVOLVE.
    bool isAssigned() const { return routedTargets > 0 || evolving; }
    bool isEvolving() const { return evolving; }

    // With every macro idle, only the first keeps its + ASSIGN showing; the
    // others show it on hover, so four identical pills read as one
    // (review 11, S11-9).
    void setQuietAssign (bool quiet)
    {
        if (quiet != quietAssign)
        {
            quietAssign = quiet;
            repaint();
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        knob.setBounds (area.removeFromLeft (juce::jmin (area.getHeight() + 4, 52)));

        auto text = textArea();
        nameEditor.setBounds (text.removeFromTop (text.getHeight() / 2).withTrimmedRight (10));
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hover = true;

        if (macroIndex >= 0)
            highlightedModSource() = (int) Mod::macroSourceFor (macroIndex);

        repaint();
    }

    void mouseExit (const juce::MouseEvent& event) override
    {
        // (Moving onto the knob inside isn't leaving.)
        if (getLocalBounds().contains (event.getEventRelativeTo (this).getPosition()))
            return;

        hover = false;
        overAssign = false;
        hoverRest = 0.0f;

        if (macroIndex >= 0 && highlightedModSource() == (int) Mod::macroSourceFor (macroIndex))
            highlightedModSource() = 0;

        repaint();
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto over = ! assignBounds.isEmpty() && assignBounds.contains (event.getPosition());

        if (over != overAssign)
        {
            overAssign = over;
            setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
            repaint();
        }
    }

    // + ASSIGN: add a routing from this macro (the editor opens the matrix).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (macroIndex >= 0 && ! assignBounds.isEmpty() && ! event.mouseWasDraggedSinceMouseDown()
            && assignBounds.contains (event.getPosition()) && onAssign != nullptr)
            onAssign (macroIndex);
    }

    // The + ASSIGN button's bounds (empty while the macro moves something).
    juce::Rectangle<int> getAssignBounds() const { return assignBounds; }
    std::function<void (int macro)> onAssign;

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (macroIndex >= 0)
        {
            nameEditor.setVisible (true);
            nameEditor.showEditor();

            if (auto* editor = nameEditor.getCurrentTextEditor())
                editor->onFocusLost = [this] { nameEditor.hideEditor (false); nameEditor.setVisible (false); };
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (macroIndex < 0 || event.getDistanceFromDragStart() < 4)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                auto image = createComponentSnapshot (textArea(), true, 1.0f);
                image.multiplyAllAlphas (0.8f);
                container->startDragging ("modsource:" + juce::String ((int) Mod::macroSourceFor (macroIndex)), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

    KnobControl& getKnob() { return knob; }
    int getMacroIndex() const { return macroIndex; }
    // How many of the macro's targets can't be heard now (their module is
    // off); the tests.
    int getNumIdleTargets() const { return idleTargets; }
    // The warning sign's bounds (empty while every target can be heard).
    juce::Rectangle<int> getIdleMarkBounds() const { return markBounds; }

    // A macro with targets that can't be heard says which first (its
    // warning sign means this).
    juce::String getTooltip() override
    {
        const auto base = juce::SettableTooltipClient::getTooltip();
        if (macroIndex < 0 || idleTargets == 0 || idleText.isEmpty())
            return base;
        // The title says what is wrong in words, the next line which targets
        // and why (review 9, I9-12: the sign alone was a bare triangle).
        const auto name = processorRef.getMacroName (macroIndex).toUpperCase();
        const auto title = idleTargets >= routedTargets ? name + ": no effect now"
                                                         : name + ": " + juce::String (idleTargets) + " of " + juce::String (routedTargets)
                                                               + " targets have no effect now";
        return title + "\n" + idleText + "." + (base.isNotEmpty() ? "  " + base : juce::String());
    }

    // Opens the macro's card: where it goes, with warnings (hover does it
    // after a short rest).
    void openCard()
    {
        if (macroIndex >= 0 && modHoverHooks().showSource != nullptr)
            modHoverHooks().showSource (*this, (int) Mod::macroSourceFor (macroIndex));
    }

private:
    juce::Rectangle<int> textArea() const
    {
        auto area = getLocalBounds();
        area.removeFromLeft (juce::jmin (area.getHeight() + 4, 52) + 2);
        return area.reduced (0, 6);
    }

public:
    // Reads the macro's routings and EVOLVE again (the timer does it a few
    // times a second; the tests call it).
    void refreshTargets()
    {
        juce::String text;
        const auto idle = countIdleTargets (text);
        idleText = text;
        auto routed = 0;
        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processorRef.readModSlot (i);
            routed += slot.source == Mod::macroSourceFor (macroIndex) && slot.destination != 0 ? 1 : 0;
        }
        const auto* evolve = processorRef.apvts.getRawParameterValue ("macro" + juce::String (macroIndex + 1) + "_evolve");
        const auto nowEvolving = evolve != nullptr && evolve->load() > 0.0005f;
        if (idle != idleTargets || routed != routedTargets || nowEvolving != evolving)
        {
            idleTargets = idle;
            routedTargets = routed;
            evolving = nowEvolving;
            knob.setAlpha (isAssigned() ? 1.0f : 0.55f);
            repaint();
        }
    }

private:
    int countIdleTargets (juce::String& text) const
    {
        const auto source = Mod::macroSourceFor (macroIndex);
        auto count = 0;
        text.clear();

        for (int i = 0; i < Mod::maxSlots; ++i)
        {
            const auto slot = processorRef.readModSlot (i);

            if (slot.source == source && slot.isActive())
                if (const auto why = ModNames::whyDestinationIsIdle (processorRef, slot.destination); why.isNotEmpty())
                {
                    text << (count > 0 ? "; " : "") << ModNames::destination (slot.destination, processorRef) << " (" << why << ")";
                    ++count;
                }
        }

        return count;
    }

    juce::String valueText() const
    {
        if (parameter == nullptr)
            return {};

        return parameter->getCurrentValueAsText();
    }

    void timerCallback() override
    {
        if (nameEditor.isVisible() && ! nameEditor.isBeingEdited())
            nameEditor.setVisible (false);

        if (macroIndex >= 0)
        {
            // Resting on the macro (not turning it) opens its card.
            const auto resting = isMouseOver (true) && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
            if (resting && hoverRest < 0.35f && (hoverRest += frameSeconds()) >= 0.35f)
                openCard();
            else if (! resting)
                hoverRest = 0.0f;

            // Its targets and idle targets, a few times a second.
            if ((idleCheck += frameSeconds()) > 0.4f)
            {
                idleCheck = 0.0f;
                refreshTargets();
            }
        }

        const auto value = parameter != nullptr ? parameter->getValue() : 0.0f;

        if (std::abs (value - lastValue) > 1.0e-5f)
        {
            lastValue = value;
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    KnobControl knob;
    juce::Label nameEditor;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::String defaultTitle;
    int macroIndex = -1;
    float lastValue = -1.0f;
    float hoverRest = 0.0f, idleCheck = 1.0f;
    int idleTargets = 0, routedTargets = -1;
    bool evolving = false;
    juce::String idleText;
    juce::Rectangle<int> markBounds;
    bool hover = false, overAssign = false, quietAssign = false;
    juce::Rectangle<int> assignBounds;
};

// GLIDE as a row in the VOICES menu (it left the bottom strip to make room
// for all eight macros): its name, a slider and the time. Turning it leaves
// the menu open.
class GlideMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    explicit GlideMenuItem (IlanaSynthAudioProcessor& p)
        : juce::PopupMenu::CustomComponent (false),
          slider (p.apvts, "glide")
    {
        addAndMakeVisible (slider);
    }

    void getIdealSize (int& idealWidth, int& idealHeight) override
    {
        idealWidth = 280;
        idealHeight = 30;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText ("Glide", getLocalBounds().withTrimmedLeft (12).withWidth (60), juce::Justification::centredLeft);
    }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedLeft (72).reduced (4, 4)); }

private:
    ValueSliderControl slider;
};
