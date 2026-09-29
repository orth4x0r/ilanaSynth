#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "IlanaLookAndFeel.h"

// The pages inside one top-level tab (MOD holds ENV / LFO, STEPS and
// MATRIX, for example), picked with a segmented switch whose lit pill
// slides to the chosen page.
class SectionSwitcher : public juce::Component,
                        private juce::Timer
{
public:
    std::function<void (int)> onSelect;

    void setItems (const juce::StringArray& newItems)
    {
        items = newItems;
        repaint();
    }

    void setSelected (int index, bool animate)
    {
        selected = juce::jlimit (0, juce::jmax (0, items.size() - 1), index);

        if (! animate)
            pill = (float) selected;
        else
            startTimerHz (60);

        repaint();
    }

    int getSelected() const { return selected; }

    int getIdealWidth() const
    {
        auto width = 6;

        for (const auto& item : items)
            width += segmentWidth (item);

        return width;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;

        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto radius = bounds.getHeight() * 0.5f;
        g.setColour (Ui::panel);
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (Ui::line);
        g.drawRoundedRectangle (bounds, radius, 1.0f);

        // The lit pill sits between segments while it slides.
        const auto lower = juce::jlimit (0, items.size() - 1, (int) std::floor (pill));
        const auto upper = juce::jlimit (0, items.size() - 1, lower + 1);
        const auto t = pill - (float) lower;
        const auto a = segmentBounds (lower), b = segmentBounds (upper);
        const auto lit = juce::Rectangle<float> (a.getX() + (b.getX() - a.getX()) * t, a.getY(),
                                                 a.getWidth() + (b.getWidth() - a.getWidth()) * t, a.getHeight()).reduced (2.0f);

        paintGlow (g, lit, lit.getHeight() * 0.5f, accent(), 1.0f);
        g.setColour (Ui::raised.interpolatedWith (accent(), 0.3f));
        g.fillRoundedRectangle (lit, lit.getHeight() * 0.5f);
        g.setColour (accent().withAlpha (0.8f));
        g.drawRoundedRectangle (lit, lit.getHeight() * 0.5f, 1.0f);

        g.setFont (font (TextSize::label, true));

        for (int i = 0; i < items.size(); ++i)
        {
            const auto near = 1.0f - juce::jlimit (0.0f, 1.0f, std::abs (pill - (float) i));
            auto colour = Ui::text2.interpolatedWith (juce::Colours::white, near);

            if (i == hover && i != selected)
                colour = colour.interpolatedWith (Ui::text, 0.6f);

            g.setColour (colour);
            g.drawText (items[i], segmentBounds (i), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index != hover)
        {
            hover = index;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.position);

        if (index >= 0 && index != selected)
        {
            setSelected (index, true);

            if (onSelect != nullptr)
                onSelect (index);
        }
    }

private:
    static int segmentWidth (const juce::String& item)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::label, true)), item) + 26;
    }

    juce::Rectangle<float> segmentBounds (int index) const
    {
        auto x = 3.0f;

        for (int i = 0; i < index; ++i)
            x += (float) segmentWidth (items[i]);

        return { x, 0.0f, (float) segmentWidth (items[juce::jlimit (0, juce::jmax (0, items.size() - 1), index)]), (float) getHeight() };
    }

    int indexAt (juce::Point<float> position) const
    {
        for (int i = 0; i < items.size(); ++i)
            if (segmentBounds (i).contains (position))
                return i;

        return -1;
    }

    void timerCallback() override
    {
        const auto target = (float) selected;
        pill += (target - pill) * 0.25f;

        if (std::abs (target - pill) < 0.005f)
        {
            pill = target;
            stopTimer();
        }

        repaint();
    }

    juce::StringArray items;
    int selected = 0, hover = -1;
    float pill = 0.0f;
};

class SectionPage : public juce::Component
{
public:
    // Called after a different page is shown (for its entrance animation).
    std::function<void (juce::Component&)> onPageShown;

    SectionPage()
    {
        switcher.onSelect = [this] (int index) { show (index, true); };
    }

    // Takes ownership of the page. `id` is how the rest of the editor
    // names it ("MATRIX"); `label` is what the switch shows.
    void addPage (const juce::String& id, const juce::String& label, juce::Component* page)
    {
        pages.push_back ({ id, std::unique_ptr<juce::Component> (page) });
        labels.add (label);
        addChildComponent (page);
        page->setBounds (getLocalBounds());
        switcher.setItems (labels);

        if (pages.size() == 1)
            show (0, false);
    }

    int indexOf (const juce::String& id) const
    {
        for (int i = 0; i < (int) pages.size(); ++i)
            if (pages[(size_t) i].id == id)
                return i;

        return -1;
    }

    int getNumPages() const { return (int) pages.size(); }
    juce::String getPageId (int index) const { return pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, index)].id; }
    juce::String getCurrentId() const { return pages.empty() ? juce::String() : pages[(size_t) current].id; }
    juce::Component* getCurrentPage() const { return pages.empty() ? nullptr : pages[(size_t) current].page.get(); }
    juce::Component* getPage (int index) const { return pages[(size_t) juce::jlimit (0, (int) pages.size() - 1, index)].page.get(); }

    void show (int index, bool animate)
    {
        if (pages.empty())
            return;

        index = juce::jlimit (0, (int) pages.size() - 1, index);
        const auto changed = index != current || ! pages[(size_t) index].page->isVisible();
        current = index;

        for (int i = 0; i < (int) pages.size(); ++i)
            pages[(size_t) i].page->setVisible (i == current);

        switcher.setSelected (current, animate);

        if (changed && animate && onPageShown != nullptr)
            onPageShown (*pages[(size_t) current].page);
    }

    // Pages refresh when they become visible; a tab switch changes the
    // section's visibility, not theirs, so pass it on.
    void visibilityChanged() override
    {
        if (isVisible())
            if (auto* page = getCurrentPage())
                page->visibilityChanged();
    }

    void resized() override
    {
        for (auto& entry : pages)
            entry.page->setBounds (getLocalBounds());
    }

    SectionSwitcher switcher;

private:
    struct Entry
    {
        juce::String id;
        std::unique_ptr<juce::Component> page;
    };

    std::vector<Entry> pages;
    juce::StringArray labels;
    int current = 0;
};
