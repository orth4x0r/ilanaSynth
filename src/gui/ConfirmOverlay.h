#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "IlanaLookAndFeel.h"

// A small confirm over the whole window, in the theme (UI review 4, S1):
// a title, one line of text, a "Don't show this again" tick and two
// buttons, or three with an alternative ("SAVE AND LOAD", review 6). Button
// names are upper case, as everywhere in the UI (review 8, S8-23). Return confirms,
// Esc or a click outside the panel cancels. The editor shows it before a
// preset load or a new random patch replaces an edited patch, and to report
// a DX7 bank import.
class ConfirmOverlay : public juce::Component
{
public:
    ConfirmOverlay()
    {
        confirmButton.onClick = [this] { finish (true); };
        cancelButton.onClick = [this] { finish (false); };
        cancelButton.setButtonText ("CANCEL");
        alternativeButton.onClick = [this]
        {
            auto done = std::move (alternative);
            alternative = nullptr;
            callback = nullptr;
            setVisible (false);

            if (done != nullptr)
                done();
        };

        // The tour's words for the same tick (review 8, S8-23).
        dontAskAgain.setButtonText ("Don't show this again");
        dontAskAgain.setColour (juce::ToggleButton::textColourId, IlanaTheme::Ui::text2);

        for (auto* component : std::initializer_list<juce::Component*> { &confirmButton, &cancelButton, &dontAskAgain })
            addAndMakeVisible (component);

        addChildComponent (alternativeButton);

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
        ask (titleText, messageText, { confirmText }, std::move (callbackIn));
    }

    // The buttons: what confirms, what cancels, and an optional third
    // choice (with what it does) between them; offerDontAsk shows the tick.
    struct Choices
    {
        juce::String confirmText, cancelText = "Cancel", alternativeText;
        std::function<void()> onAlternative;
        bool offerDontAsk = true;
    };

    void ask (const juce::String& titleText, const juce::String& messageText, const Choices& choices, Callback callbackIn)
    {
        if (callback != nullptr)
            finish (false);

        title = titleText;
        message = messageText;
        callback = std::move (callbackIn);
        alternative = choices.onAlternative;
        confirmButton.setButtonText (choices.confirmText.toUpperCase());
        cancelButton.setButtonText (choices.cancelText.toUpperCase());
        alternativeButton.setButtonText (choices.alternativeText.toUpperCase());
        alternativeButton.setVisible (choices.alternativeText.isNotEmpty() && alternative != nullptr);
        dontAskAgain.setVisible (choices.offerDontAsk);
        dontAskAgain.setToggleState (false, juce::dontSendNotification);
        resized();

        // The way on, solid in the accent as the tour's GOT IT is; with a
        // safe alternative (SAVE AND LOAD), that one is solid and the
        // destructive way on is plain (UI review 9, V9-15).
        const auto solid = alternativeButton.isVisible() ? &alternativeButton : &confirmButton;
        const auto plain = alternativeButton.isVisible() ? &confirmButton : nullptr;
        solid->setColour (juce::TextButton::buttonColourId, IlanaTheme::accent());
        solid->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        if (plain != nullptr)
        {
            plain->setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
            plain->setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text);
        }

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
        alternative = nullptr;
        setVisible (false);

        if (done != nullptr)
            done (confirmed, dontAskAgain.getToggleState());
    }

    // The third choice, for the tests (does what its button does).
    bool hasAlternative() const { return alternativeButton.isVisible(); }
    juce::String getAlternativeText() const { return alternativeButton.getButtonText(); }
    void chooseAlternative() { alternativeButton.onClick(); }
    juce::String getTitle() const { return title; }

    // As tall as what it says (review 7: a fixed height left an empty band
    // under a one-line message). Three buttons need the tick on a line of
    // its own.
    juce::Rectangle<int> panelBounds() const
    {
        const auto three = alternativeButton.isVisible();
        const auto width = juce::jmin (three ? 470 : 420, getWidth() - 32);
        const auto tickLine = three && dontAskAgain.isVisible() ? 24 + 8 : 0;
        return getLocalBounds().withSizeKeepingCentre (width, 16 + 24 + 6 + messageHeight (width - 40) + 14 + tickLine + 30 + 16);
    }

    // As many lines as the message wraps to (up to 8; a report from tell() can be long).
    int messageHeight (int width) const
    {
        return 20 * messageLines (width);
    }

    int messageLines (int width) const
    {
        const juce::Font font (IlanaTheme::font (IlanaTheme::TextSize::body));
        auto lines = 0;
        for (const auto& paragraph : juce::StringArray::fromLines (message))
            lines += juce::jmax (1, (int) std::ceil ((double) juce::GlyphArrangement::getStringWidthInt (font, paragraph) / juce::jmax (1.0, width * 0.95)));
        return juce::jlimit (1, 8, lines);
    }

    void resized() override
    {
        auto area = panelBounds().reduced (20, 16);
        auto bottom = area.removeFromBottom (30);
        confirmButton.setBounds (bottom.removeFromRight (124));
        bottom.removeFromRight (8);

        if (alternativeButton.isVisible())
        {
            alternativeButton.setBounds (bottom.removeFromRight (124));
            bottom.removeFromRight (8);
        }

        cancelButton.setBounds (bottom.removeFromRight (90));
        cancelButton.setVisible (cancelButton.getButtonText().isNotEmpty());

        if (alternativeButton.isVisible())
        {
            area.removeFromBottom (8);
            dontAskAgain.setBounds (area.removeFromBottom (24).withWidth (200));
        }
        else
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
        const auto lines = messageLines (area.getWidth());
        IlanaTheme::drawFitted (g, message, area.removeFromTop (20 * lines), juce::Justification::topLeft, lines);
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
        {
            // Return takes the solid button.
            if (alternativeButton.isVisible())
                alternativeButton.onClick();
            else
                finish (true);
        }

        return true; // nothing reaches the page underneath while asking
    }

    // A message (an error, a result) inside the editor instead of a desktop
    // alert box, which neither followed the Interface size nor the skin and
    // could open away from the plugin window (Fable U10). Shown on the overlay
    // of the editor `near` sits in, else the most recent editor's; with no
    // editor (a test tool) it falls back to the alert box.
    static void tell (juce::Component* near, const juce::String& titleText, const juce::String& messageText)
    {
        ConfirmOverlay* overlay = nullptr;
        for (auto* c = near; c != nullptr && overlay == nullptr; c = c->getParentComponent())
            for (auto* child : c->getChildren())
                if (auto* found = dynamic_cast<ConfirmOverlay*> (child))
                    overlay = found;
        if (overlay == nullptr)
            overlay = latest().getComponent();
        if (overlay == nullptr || overlay->isAsking())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, titleText, messageText, {}, near);
            return;
        }
        Choices choices;
        choices.confirmText = "OK";
        choices.cancelText = {};
        choices.offerDontAsk = false;
        overlay->ask (titleText, messageText, choices, [] (bool, bool) {});
    }

    void parentHierarchyChanged() override
    {
        if (getParentComponent() != nullptr)
            latest() = this;
    }

private:
    static juce::Component::SafePointer<ConfirmOverlay>& latest()
    {
        static juce::Component::SafePointer<ConfirmOverlay> overlay;
        return overlay;
    }

    juce::TextButton confirmButton, cancelButton, alternativeButton;
    juce::ToggleButton dontAskAgain;
    juce::String title, message;
    Callback callback;
    std::function<void()> alternative;
};
