#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "FilterWidgets.h"
#include "IlanaLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../SectionColours.h"

namespace SectionColours
{
    // The colour a section shows now (`key` as in SectionColours.h).
    inline juce::Colour current (int key)
    {
        if (key < subKey)
            return IlanaTheme::oscColour (key);
        if (key == subKey)
            return IlanaTheme::subNoiseColour();
        if (key == physicalKey)
            return IlanaTheme::physicalColour();
        if (key == westKey)
            return FilterColours::west();
        if (key < lfoKey (0))
            return IlanaTheme::envIdentityColour (key - envKey (0));
        return IlanaSynthAudioProcessor::lfoColour (key - lfoKey (0));
    }

    // The key of a modulation source that can be given a colour (the five
    // main envelopes and the LFOs), or -1.
    inline int keyForSource (int source)
    {
        if (const auto lfo = Mod::lfoIndexFor ((Mod::Source) source); lfo >= 0 && lfo < numLfos)
            return lfoKey (lfo);

        switch ((Mod::Source) source)
        {
            case Mod::Source::AmpEnv:     return envKey (0);
            case Mod::Source::FilterEnv:  return envKey (1);
            case Mod::Source::FilterEnv2: return envKey (2);
            case Mod::Source::ModEnv:     return envKey (3);
            case Mod::Source::Env4:       return envKey (4);
            default:                      return -1;
        }
    }

    // The colour it has when the patch gives it none.
    inline juce::Colour standard (int key)
    {
        auto& table = shown();
        const auto picked = table[(size_t) key];
        table[(size_t) key] = 0;
        const auto colour = current (key);
        table[(size_t) key] = picked;
        return colour;
    }
}

// Colour coding (ilana, 2026-10-27/28): right-click a section's dot (or
// "Colour..." in its menu) for twelve swatches and Reset. The colour is the
// patch's, saved with it, and every page shows the section in it.
class SectionColourPicker : public juce::Component
{
public:
    SectionColourPicker (IlanaSynthAudioProcessor& p, int keyIn, const juce::String& nameIn)
        : processorRef (p), key (keyIn), name (nameIn.toUpperCase()), defaultColour (SectionColours::standard (keyIn))
    {
        setSize (columns * (swatch + gap) - gap + 2 * pad, titleHeight + 2 * (swatch + gap) + resetHeight + 2 * pad);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // Opens the picker by `area` of `anchor`, inside the editor so it takes
    // the editor's zoom.
    static void show (IlanaSynthAudioProcessor& p, int key, const juce::String& name, juce::Component& anchor, juce::Rectangle<int> area)
    {
        auto* parent = anchor.getTopLevelComponent();
        juce::CallOutBox::launchAsynchronously (std::make_unique<SectionColourPicker> (p, key, name),
                                                parent->getLocalArea (&anchor, area), parent);
    }

    // Sets a section's colour (0: its default) and shows it on every page now.
    static void apply (IlanaSynthAudioProcessor& p, int key, juce::uint32 argb)
    {
        p.setSectionColour (key, argb);
        SectionColours::shown() = p.getSectionColours();
        ++SectionColours::version();
        IlanaTheme::identityColourListeners (&p).call ([] (IlanaTheme::IdentityColourListener& listener) { listener.identityColoursChanged(); });

        for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
            if (auto* window = juce::Desktop::getInstance().getComponent (i))
                window->repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true).withKerningFactor (0.1f));
        g.drawText (name + " COLOUR", juce::Rectangle<int> (pad, pad, getWidth() - 2 * pad, titleHeight - 6), juce::Justification::centredLeft, false);

        const auto current = processorRef.getSectionColour (key);

        for (int i = 0; i < SectionColours::numSwatches; ++i)
        {
            const auto dot = swatchBounds (i).toFloat();
            const auto chosen = current == SectionColours::swatches[i];
            g.setColour (juce::Colour (SectionColours::swatches[i]));
            g.fillEllipse (dot.reduced (chosen ? 3.0f : (i == hovered ? 0.0f : 1.0f)));

            if (chosen)
            {
                g.setColour (IlanaTheme::Ui::text);
                g.drawEllipse (dot.reduced (0.75f), 1.5f);
            }
        }

        const auto reset = resetBounds();
        g.setColour (IlanaTheme::Ui::line);
        g.fillRect (reset.withHeight (1));
        g.setColour (hovered == resetIndex ? IlanaTheme::Ui::text : IlanaTheme::Ui::text2);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body));
        g.drawText ("Reset to default", reset.withTrimmedLeft (18), juce::Justification::centredLeft, false);
        g.setColour (defaultColour);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ (float) reset.getX() + 5.0f, (float) reset.getCentreY() + 0.5f }));
        g.setColour (current == 0 ? IlanaTheme::Ui::text2 : IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText (current == 0 ? "in use" : "", reset, juce::Justification::centredRight, false);
    }

    void mouseMove (const juce::MouseEvent& event) override { setHovered (indexAt (event.getPosition())); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index < 0)
            return;

        apply (processorRef, key, index == resetIndex ? 0u : SectionColours::swatches[index]);

        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

private:
    static constexpr int columns = 6, swatch = 22, gap = 8, pad = 12, titleHeight = 24, resetHeight = 32;
    static constexpr int resetIndex = 100;

    juce::Rectangle<int> swatchBounds (int i) const
    {
        return { pad + (i % columns) * (swatch + gap), pad + titleHeight + (i / columns) * (swatch + gap), swatch, swatch };
    }

    juce::Rectangle<int> resetBounds() const
    {
        return { pad, getHeight() - pad - resetHeight + 4, getWidth() - 2 * pad, resetHeight - 4 };
    }

    int indexAt (juce::Point<int> point) const
    {
        for (int i = 0; i < SectionColours::numSwatches; ++i)
            if (swatchBounds (i).expanded (gap / 2).contains (point))
                return i;

        return resetBounds().contains (point) ? resetIndex : -1;
    }

    void setHovered (int index)
    {
        if (index != hovered)
        {
            hovered = index;
            repaint();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    const int key;
    const juce::String name;
    const juce::Colour defaultColour;
    int hovered = -1;
};
