#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "../dsp/TableFactory.h"
#include "IlanaLookAndFeel.h"

// Every wavetable at a glance, grouped by category, each drawn from its
// first and middle frames. Click one to load it into the oscillator.
class TableBrowser : public juce::Component
{
public:
    TableBrowser (IlanaSynthAudioProcessor& p, juce::String parameterIdIn, juce::Colour colourIn)
        : processorRef (p), parameterId (std::move (parameterIdIn)), colour (colourIn)
    {
        content.owner = this;
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
        buildLayout();
        setSize (740, 520);
    }

    // Opens the browser in a call-out next to the given component.
    static void show (IlanaSynthAudioProcessor& processor, const juce::String& parameterId, juce::Colour colour,
                      juce::Component& target)
    {
        auto browser = std::make_unique<TableBrowser> (processor, parameterId, colour);
        auto* parent = target.getTopLevelComponent();

        // Fill most of the plugin window so the previews are readable at any
        // UI zoom (the call-out is not scaled with the editor's content).
        if (parent != nullptr)
            browser->setSize (juce::jmax (740, juce::roundToInt ((float) parent->getWidth() * 0.86f)),
                              juce::jmax (520, juce::roundToInt ((float) parent->getHeight() * 0.8f)));

        juce::CallOutBox::launchAsynchronously (std::move (browser),
                                                parent != nullptr ? parent->getLocalArea (&target, target.getLocalBounds())
                                                                  : target.getScreenBounds(),
                                                parent);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        buildLayout();
    }

private:
    struct Item
    {
        int choice = 0;
        juce::String name;
        juce::Rectangle<int> bounds;
    };

    struct Heading
    {
        juce::String text;
        juce::Rectangle<int> bounds;
    };

    struct Content : public juce::Component
    {
        TableBrowser* owner = nullptr;

        void paint (juce::Graphics& g) override { owner->paintContent (g); }
        void mouseMove (const juce::MouseEvent& event) override { owner->hoverAt (event.getPosition()); }
        void mouseExit (const juce::MouseEvent&) override { owner->hoverAt ({ -1, -1 }); }
        void mouseUp (const juce::MouseEvent& event) override { owner->pickAt (event.getPosition()); }
    };

    void buildLayout()
    {
        items.clear();
        headings.clear();

        const auto names = TableFactory::getFactoryTableNames();
        const auto categories = TableFactory::getFactoryTableCategories();
        auto order = TableFactory::getCategoryOrder();
        order.add ("User");

        const auto width = juce::jmax (200, getWidth() - 12);
        // Cells grow with the browser; ~140 px wide at the smallest size.
        scale = juce::jlimit (1.0f, 2.5f, (float) width / 728.0f);
        const auto columns = juce::jlimit (4, 6, width / juce::roundToInt (140.0f * scale));
        const auto cellWidth = width / columns;
        const auto cellHeight = juce::roundToInt ((float) cellWidth * 0.55f);
        const auto headingHeight = juce::roundToInt (22.0f * scale);
        auto y = 6;

        for (const auto& category : order)
        {
            std::vector<Item> group;

            if (category == "User")
            {
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numUserSlots; ++slot)
                    group.push_back ({ names.size() + slot, "User " + juce::String (slot + 1), {} });
            }
            else
            {
                for (int i = 0; i < names.size(); ++i)
                    if (categories[i] == category)
                        group.push_back ({ i, names[i], {} });
            }

            if (group.empty())
                continue;

            headings.push_back ({ category.toUpperCase(), { 8, y, width, headingHeight } });
            y += headingHeight + 2;

            for (size_t i = 0; i < group.size(); ++i)
            {
                const auto column = (int) i % columns;

                if (column == 0 && i > 0)
                    y += cellHeight + 6;

                group[i].bounds = { 4 + column * cellWidth, y, cellWidth - 6, cellHeight };
                items.push_back (group[i]);
            }

            y += cellHeight + juce::roundToInt (14.0f * scale);
        }

        content.setSize (width, y);
        content.repaint();
    }

    int current() const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (parameterId))
            return (int) value->load();

        return 0;
    }

    void paintContent (juce::Graphics& g)
    {
        g.fillAll (IlanaTheme::Ui::panel);
        const auto selected = current();

        for (const auto& heading : headings)
        {
            g.setColour (colour);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body * scale, true));
            g.drawText (heading.text, heading.bounds, juce::Justification::centredLeft);
        }

        for (const auto& item : items)
        {
            const auto bounds = item.bounds.toFloat();
            const auto isSelected = item.choice == selected;
            const auto isHovered = item.choice == hovered;

            g.setColour (isSelected ? colour.withAlpha (0.2f) : juce::Colours::white.withAlpha (isHovered ? 0.07f : 0.03f));
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (isSelected ? colour : juce::Colours::white.withAlpha (isHovered ? 0.3f : 0.1f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, isSelected ? 1.5f : 1.0f);

            auto area = bounds.reduced (8.0f * scale, 5.0f * scale);
            const auto label = area.removeFromBottom (17.0f * scale);
            paintWave (g, item.choice, area, isSelected || isHovered);

            g.setColour (isSelected ? colour : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body * scale, isSelected));
            g.drawFittedText (item.name, label.toNearestInt(), juce::Justification::centred, 1, 0.8f);
        }
    }

    void paintWave (juce::Graphics& g, int choice, juce::Rectangle<float> area, bool bright)
    {
        const auto* table = processorRef.getWavetable (choice);

        if (table == nullptr || table->getNumFrames() == 0)
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label * scale));
            g.drawText ("empty", area, juce::Justification::centred);
            return;
        }

        // First frame bright, middle frame faint behind it.
        const int frames[] { table->getNumFrames() / 2, 0 };

        for (int layer = 0; layer < 2; ++layer)
        {
            const auto* data = table->getFrameData (0, frames[layer]);
            juce::Path path;

            for (int i = 0; i <= 96; ++i)
            {
                const auto index = juce::jmin (Wavetable::frameSize - 1, i * Wavetable::frameSize / 96);
                const auto x = area.getX() + area.getWidth() * (float) i / 96.0f;
                const auto y = area.getCentreY() - juce::jlimit (-1.0f, 1.0f, data[index]) * area.getHeight() * 0.45f;

                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            g.setColour (layer == 0 ? colour.withAlpha (0.3f) : colour.withAlpha (bright ? 1.0f : 0.75f));
            g.strokePath (path, juce::PathStrokeType ((layer == 0 ? 1.0f : 1.6f) * scale));
        }
    }

    int itemAt (juce::Point<int> position) const
    {
        for (const auto& item : items)
            if (item.bounds.contains (position))
                return item.choice;

        return -1;
    }

    void hoverAt (juce::Point<int> position)
    {
        const auto choice = itemAt (position);

        if (choice != hovered)
        {
            hovered = choice;
            content.repaint();
        }
    }

    void pickAt (juce::Point<int> position)
    {
        const auto choice = itemAt (position);

        if (choice < 0)
            return;

        if (auto* parameter = processorRef.apvts.getParameter (parameterId))
        {
            processorRef.performEdit (parameter->getName (64), [parameter, choice]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) choice));
                parameter->endChangeGesture();
            });
        }

        if (auto* callOut = findParentComponentOfClass<juce::CallOutBox>())
            callOut->dismiss();
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String parameterId;
    juce::Colour colour;
    juce::Viewport viewport;
    Content content;
    std::vector<Item> items;
    std::vector<Heading> headings;
    int hovered = -1;
    float scale = 1.0f;
};
