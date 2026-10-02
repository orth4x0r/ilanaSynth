#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

// A small confirm over the whole window, in the theme (UI review 4, S1):
// a title, one line of text, a "Don't ask again" tick and two buttons.
// Return confirms, Esc or a click outside the panel cancels. The editor
// shows it before a preset load or a new random patch replaces an edited
// patch.
class ConfirmOverlay : public juce::Component
{
public:
    ConfirmOverlay()
    {
        confirmButton.onClick = [this] { finish (true); };
        cancelButton.onClick = [this] { finish (false); };
        cancelButton.setButtonText ("Cancel");

        dontAskAgain.setButtonText ("Don't ask again");
        dontAskAgain.setColour (juce::ToggleButton::textColourId, IlanaTheme::Ui::text2);

        for (auto* component : std::initializer_list<juce::Component*> { &confirmButton, &cancelButton, &dontAskAgain })
            addAndMakeVisible (component);

        setWantsKeyboardFocus (true);
        setVisible (false);
    }

    // Called with whether the user went ahead, and the tick's state.
    using Callback = std::function<void (bool confirmed, bool dontAskAgain)>;

    // Asks; a question already showing is answered "no" first (a second
    // load request replaces it).
    void ask (const juce::String& titleText, const juce::String& messageText, const juce::String& confirmText,
              Callback callbackIn)
    {
        if (callback != nullptr)
            finish (false);

        title = titleText;
        message = messageText;
        callback = std::move (callbackIn);
        confirmButton.setButtonText (confirmText);
        dontAskAgain.setToggleState (false, juce::dontSendNotification);

        // The way on, solid in the accent as the tour's GOT IT is.
        confirmButton.setColour (juce::TextButton::buttonColourId, IlanaTheme::accent());
        confirmButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);

        if (auto* parent = getParentComponent())
            setBounds (parent->getLocalBounds());

        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
        repaint();
    }

    bool isAsking() const { return isVisible() && callback != nullptr; }

    // For the buttons, the keys and the tests.
    void finish (bool confirmed)
    {
        auto done = std::move (callback);
        callback = nullptr;
        setVisible (false);

        if (done != nullptr)
            done (confirmed, dontAskAgain.getToggleState());
    }

    void setDontAskAgain (bool shouldTick) { dontAskAgain.setToggleState (shouldTick, juce::dontSendNotification); }

    juce::Rectangle<int> panelBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre (juce::jmin (420, getWidth() - 32), 150);
    }

    void resized() override
    {
        auto area = panelBounds().reduced (20, 16);
        auto bottom = area.removeFromBottom (30);
        confirmButton.setBounds (bottom.removeFromRight (124));
        bottom.removeFromRight (8);
        cancelButton.setBounds (bottom.removeFromRight (90));
        dontAskAgain.setBounds (bottom.withTrimmedRight (8));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));

        const auto panel = panelBounds().toFloat();
        g.setColour (IlanaTheme::Ui::panel);
        g.fillRoundedRectangle (panel, 10.0f);
        g.setColour (IlanaTheme::accent().withAlpha (0.6f));
        g.drawRoundedRectangle (panel.reduced (0.5f), 10.0f, 1.5f);

        auto area = panel.toNearestInt().reduced (20, 16);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::large, true));
        g.drawText (title, area.removeFromTop (24), juce::Justification::centredLeft);

        area.removeFromTop (6);
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawFittedText (message, area.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! panelBounds().contains (event.getPosition()))
            finish (false);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
            finish (false);
        else if (key == juce::KeyPress::returnKey)
            finish (true);

        return true; // nothing reaches the page underneath while asking
    }

private:
    juce::TextButton confirmButton, cancelButton;
    juce::ToggleButton dontAskAgain;
    juce::String title, message;
    Callback callback;
};
