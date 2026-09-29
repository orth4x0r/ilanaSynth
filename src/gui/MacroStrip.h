#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
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

            setTooltip (title + "\nTurn to sweep everything this macro is routed to.  Drag its name onto any knob "
                                "to route it there.  Double-click the name to rename it.");
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        }

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

        if (macroIndex >= 0)
        {
            // Drag handle: a little grip so the name reads as grabbable.
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
            g.setColour (hover ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            g.drawText (title.toUpperCase(), text.removeFromTop (text.getHeight() / 2), juce::Justification::bottomLeft, true);
        }
        else
        {
            text.removeFromTop (text.getHeight() / 2);
        }

        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText (valueText(), text, juce::Justification::topLeft, true);
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
            highlightedModSource() = (int) Mod::Source::Macro1 + macroIndex;

        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = false;

        if (macroIndex >= 0 && highlightedModSource() == (int) Mod::Source::Macro1 + macroIndex)
            highlightedModSource() = 0;

        repaint();
    }

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
                container->startDragging ("modsource:" + juce::String ((int) Mod::Source::Macro1 + macroIndex), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

    KnobControl& getKnob() { return knob; }

private:
    juce::Rectangle<int> textArea() const
    {
        auto area = getLocalBounds();
        area.removeFromLeft (juce::jmin (area.getHeight() + 4, 52) + 2);
        return area.reduced (0, 6);
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
    bool hover = false;
};
