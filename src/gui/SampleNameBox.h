#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "OwnMenuBox.h"

// The sample's name in the card header: a packed menu box ("SAMPLE  Rhodes C3.wav") whose click
// opens the load menu instead of a list (OwnMenuBox::ownMenu: the click handling
// every select with a menu of its own shares).
class SampleNameBox : public OwnMenuBox
{
public:
    SampleNameBox()
    {
        setInterceptsMouseClicks (true, false);
        getProperties().set ("packedMenu", true);
    }

    // (Always the load menu, never a list: with no menu set a click does nothing.)
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (opensOwnMenu())
            OwnMenuBox::mouseDown (event);
    }
    void mouseDrag (const juce::MouseEvent&) override {}
    void mouseUp (const juce::MouseEvent&) override {}

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
        {
            openOwnMenu();
            return true;
        }
        return false; // (one item: nothing to step through)
    }

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        juce::Component::mouseWheelMove (event, wheel); // (the page scrolls; no item to step to)
    }

    void showPopup() override
    {
        hidePopup();
        openOwnMenu();
    }

    void setCaptionAndName (const juce::String& caption, const juce::String& name)
    {
        getProperties().set ("textPrefix", caption);
        if (getText() != name)
        {
            clear (juce::dontSendNotification);
            addItem (name, 1);
            setSelectedId (1, juce::dontSendNotification);
        }
        repaint();
    }
};
