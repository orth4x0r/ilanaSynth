#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// A menu box whose click opens a menu of its own instead of ComboBox's list:
// the one click handling shared by every select that does that (ComboControl's
// PopupCombo with its ownMenu set, the sample card's SampleNameBox).
//
// It opens on the press only and never goes through ComboBox's "menu active"
// flag. The base class sets that flag on the press, again on the release, and
// asks showPopup() to open on a drag and on key repeats, and it clears it only
// in its own list's callback. With a menu of our own the flag stayed set, and
// on a quick click a second showPopup() landed inside the debounce and
// returned with it set, so every later click did nothing (the list never
// showed, though the arrow keys still worked: ilana's PC reports for PLAY and
// OSC, 2026-10-10, and the granular SOURCE box, 2026-11-13). Taking the press,
// release, drag and Return key here leaves the flag alone for good.
class OwnMenuBox : public juce::ComboBox
{
public:
    // The menu this box opens. Null: a plain ComboBox (its own list).
    std::function<void()> ownMenu;

    bool opensOwnMenu() const { return ownMenu != nullptr; }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! opensOwnMenu())
        {
            juce::ComboBox::mouseDown (event);
            return;
        }

        // (Asynchronously, as ComboBox does: the press that dismissed another
        // open menu gets to finish first.)
        if (isEnabled() && ! event.mods.isPopupMenu())
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<OwnMenuBox> (this)]
                                             { if (safe != nullptr) safe->openOwnMenu(); });
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! opensOwnMenu())
            juce::ComboBox::mouseDrag (event);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! opensOwnMenu())
            juce::ComboBox::mouseUp (event);
    }

    // Reached by accessibility's "press" action: clear the flag it set before
    // opening our own menu.
    void showPopup() override
    {
        if (! opensOwnMenu())
        {
            showListPopup();
            return;
        }

        hidePopup();
        openOwnMenu();
    }

    // Opens the box's own menu. A press can arrive twice in a few
    // milliseconds (a touchpad tap, a double event from the host); the second
    // would close the first's menu as it opened.
    void openOwnMenu()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (now - lastOwnMenuOpen < 150.0)
            return;

        lastOwnMenuOpen = now;
        aboutToOpenOwnMenu();
        if (ownMenu != nullptr)
            ownMenu();
    }

protected:
    // ComboBox's own list (a box without a menu of its own).
    virtual void showListPopup() { juce::ComboBox::showPopup(); }
    // Called just before the box's own menu opens.
    virtual void aboutToOpenOwnMenu() {}

private:
    double lastOwnMenuOpen = -1.0e9;
};
