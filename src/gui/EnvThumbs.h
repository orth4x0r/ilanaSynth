#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <algorithm>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "ParamControls.h"
#include "AnimationUtils.h"

// The patch's envelopes at a glance, Phase Plant style: the added and the
// assigned ones, then a "+" card for the next. Cards keep one size (five fit
// the view) and the bar scrolls sideways when there are more. Each card draws
// its ADSR shape and marks whether the envelope is doing anything in the patch.
// Click to edit; drag a card onto a knob to route it there; right-click to remove.
class EnvThumbBar : public juce::Component,
                    private IlanaAnim::FrameTimer
{
public:
    // A card rebuilt under the mouse never gets its mouseExit; don't leave
    // knobs lit for a source nobody is hovering.
    ~EnvThumbBar() override
    {
        if (isMouseOver (true))
            highlightedModSource() = 0;
    }

    struct Env
    {
        juce::String title;
        juce::String prefix;
        Mod::Source source;
        juce::Colour colour;
    };

    explicit EnvThumbBar (IlanaSynthAudioProcessor& p, std::vector<Env> envsIn)
        : processorRef (p), envs (std::move (envsIn))
    {
        startTimerHz (10);
    }

    std::function<void (int)> onSelect;
    std::function<void()> onLayoutChanged;

    // Where an envelope's card sits, for scrolling it into view.
    juce::Rectangle<int> boundsOfCard (int env) const
    {
        const auto visible = visibleEnvelopes();
        const auto position = std::find (visible.begin(), visible.end(), env);
        if (position == visible.end())
            return {};
        return cardBounds ((int) (position - visible.begin())).toNearestInt();
    }

    // The width the bar is seen through: five cards fill it.
    void setViewWidth (int width) { viewWidth = width; }

    int getPreferredWidth() const
    {
        const auto count = numCards();
        return juce::jmax (viewWidth, (int) std::ceil ((float) count * (cardWidth() + gap) - gap));
    }

    void setSelected (int index)
    {
        selected = index;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto visible = visibleEnvelopes();
        const auto count = numCards();
        for (int position = 0; position < (int) visible.size(); ++position)
            paintCard (g, visible[(size_t) position], cardBounds (position));
        if (count > (int) visible.size())
        {
            const auto card = cardBounds (count - 1);
            IlanaTheme::paintWell (g, card, 6.0f);
            g.setColour (IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::display, true));
            g.drawText ("+", card, juce::Justification::centred);
        }
    }

    // Select on release, and only for a click: a drag assigns the source to
    // a knob instead (on MAIN, selecting would switch pages mid-drag).
    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasDraggedSinceMouseDown() || event.getDistanceFromDragStart() >= 6)
            return;

        const auto index = indexAt (event.getPosition());

        if (index >= 0 && event.mods.isPopupMenu())
        {
            showCardMenu (index);
            return;
        }

        if (index == -2)
        {
            const auto visible = visibleEnvelopes();
            for (int env = 0; env < (int) envs.size(); ++env)
                if (std::find (visible.begin(), visible.end(), env) == visible.end())
                {
                    processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, true);
                    selected = env;
                    if (onSelect != nullptr)
                        onSelect (env);
                    break;
                }

            layoutChanged();
        }
        else if (index >= 0 && onSelect != nullptr)
        {
            selected = index;
            onSelect (index);
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getMouseDownPosition());

        if (index < 0 || event.getDistanceFromDragStart() < 6)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! container->isDragAndDropActive())
            {
                const auto visible = visibleEnvelopes();
                const auto position = (int) (std::find (visible.begin(), visible.end(), index) - visible.begin());
                auto image = createComponentSnapshot (cardBounds (position).toNearestInt(), true, 1.0f);
                image.multiplyAllAlphas (0.75f);
                container->startDragging ("modsource:" + juce::String ((int) envs[(size_t) index].source), this,
                                          juce::ScaledImage (image), true);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index != hoverIndex)
        {
            hoverIndex = index;
            highlightedModSource() = index >= 0 ? (int) envs[(size_t) index].source : 0;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        highlightedModSource() = 0;
        repaint();
    }

private:
    static constexpr float gap = 8.0f;

    std::vector<int> visibleEnvelopes() const
    {
        std::vector<int> visible;
        for (int env = 0; env < (int) envs.size(); ++env)
            if (processorRef.isRevealed (IlanaSynthAudioProcessor::Module::Envelope, env) || isInUse (env))
                visible.push_back (env);
        return visible;
    }

    int numCards() const
    {
        const auto visible = visibleEnvelopes().size();
        return (int) visible + (visible < envs.size() ? 1 : 0);
    }

    float cardWidth() const
    {
        const auto width = viewWidth > 0 ? viewWidth : getWidth();
        return ((float) width - gap * 4.0f) / 5.0f;
    }

    juce::Rectangle<float> cardBounds (int position) const
    {
        return { (float) position * (cardWidth() + gap), 0.0f, cardWidth(), (float) getHeight() };
    }

    int indexAt (juce::Point<int> position) const
    {
        const auto visible = visibleEnvelopes();
        const auto count = numCards();
        for (int i = 0; i < count; ++i)
            if (cardBounds (i).contains (position.toFloat()))
                return i < (int) visible.size() ? visible[(size_t) i] : -2;

        return -1;
    }

    void layoutChanged()
    {
        lastCardCount = numCards();
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
        repaint();
    }

    void showCardMenu (int env)
    {
        juce::PopupMenu menu;
        const auto inUse = isInUse (env);
        menu.addItem (1, inUse ? "Remove (unassign it first)" : "Remove " + envs[(size_t) env].title, ! inUse);

        juce::Component::SafePointer<EnvThumbBar> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis, env] (int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            safeThis->processorRef.setRevealed (IlanaSynthAudioProcessor::Module::Envelope, env, false);
            if (safeThis->selected == env && safeThis->onSelect != nullptr)
            {
                safeThis->selected = 0;
                safeThis->onSelect (0);
            }
            safeThis->layoutChanged();
        });
    }

    float readParam (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();

        return 0.0f;
    }

    // The amp envelope always plays; the filter envelopes count when their
    // filter's env amount is set; any envelope counts when routed in the matrix.
    bool isInUse (int env) const
    {
        const auto& info = envs[(size_t) env];

        if (info.source == Mod::Source::AmpEnv)
            return true;

        if (info.source == Mod::Source::FilterEnv && std::abs (readParam ("f1_env")) > 0.001f)
            return true;

        if (info.source == Mod::Source::FilterEnv2 && std::abs (readParam ("f2_env")) > 0.001f)
            return true;

        // As an oscillator's amp envelope or its warp (DCW) envelope, whose
        // choices start with Off.
        for (const auto* prefix : OscillatorIds::prefixes)
            if ((int) readParam (juce::String (prefix) + "_amp_env") == env
                || (int) readParam (juce::String (prefix) + "_pd_env") == env + 1)
                return true;

        for (int slot = 0; slot < Mod::maxSlots; ++slot)
        {
            const auto routing = processorRef.readModSlot (slot);

            if (routing.destination != 0 && (routing.source == info.source || routing.aux == info.source))
                return true;
        }

        return false;
    }

    void paintCard (juce::Graphics& g, int env, juce::Rectangle<float> card)
    {
        const auto& info = envs[(size_t) env];
        const auto colour = info.colour;
        const auto active = env == selected;
        const auto hovered = env == hoverIndex;
        const auto inUse = isInUse (env);

        IlanaTheme::paintWell (g, card, 6.0f);

        if (active)
        {
            g.setColour (colour.withAlpha (0.12f));
            g.fillRoundedRectangle (card, 6.0f);
            g.setColour (colour.withAlpha (0.9f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.4f);
        }
        else if (hovered)
        {
            g.setColour (colour.withAlpha (0.4f));
            g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.0f);
        }

        auto inner = card.reduced (8.0f, 5.0f);
        auto titleRow = inner.removeFromTop (14.0f);

        g.setColour (active ? colour : juce::Colours::white.withAlpha (inUse ? 0.75f : 0.45f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText (info.title, titleRow, juce::Justification::centredLeft);

        if (inUse)
        {
            const auto titleWidth = juce::GlyphArrangement::getStringWidth (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::body, true)),
                                                                            info.title);
            g.setColour (colour);
            g.fillEllipse (titleRow.getX() + titleWidth + 6.0f, titleRow.getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        // ADSR outline on a compressed time axis so long and short stages
        // both stay readable.
        const auto prefix = juce::String (info.prefix);
        const auto attack = std::sqrt (readParam (prefix + "_attack"));
        const auto decay = std::sqrt (readParam (prefix + "_decay"));
        const auto sustain = juce::jlimit (0.0f, 1.0f, readParam (prefix + "_sustain"));
        const auto release = std::sqrt (readParam (prefix + "_release"));
        const auto delay = std::sqrt (readParam (prefix + "_delay"));
        const auto peakHold = std::sqrt (readParam (prefix + "_hold"));
        const auto hold = 0.35f;
        const auto total = juce::jmax (0.001f, delay + attack + peakHold + decay + hold + release);

        const auto plot = inner.reduced (0.0f, 3.0f);
        const auto xAt = [&plot, total] (float t) { return plot.getX() + plot.getWidth() * t / total; };
        const auto yAt = [&plot] (float level) { return plot.getBottom() - level * plot.getHeight(); };

        juce::Path path;
        path.startNewSubPath (xAt (0.0f), yAt (0.0f));
        path.lineTo (xAt (delay), yAt (0.0f));
        path.lineTo (xAt (delay + attack), yAt (1.0f));
        path.lineTo (xAt (delay + attack + peakHold), yAt (1.0f));
        path.lineTo (xAt (delay + attack + peakHold + decay), yAt (sustain));
        path.lineTo (xAt (delay + attack + peakHold + decay + hold), yAt (sustain));
        path.lineTo (xAt (total), yAt (0.0f));

        auto fill = path;
        fill.closeSubPath();
        g.setColour (colour.withAlpha (active ? 0.16f : 0.08f));
        g.fillPath (fill);

        g.setColour (colour.withAlpha (active ? 0.95f : (inUse ? 0.6f : 0.35f)));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        paintTargetTag (g, inner, cachedTargets (env), colour);
    }

    // The cards repaint often; what they drive is re-read four times a second.
    juce::String cachedTargets (int env)
    {
        const auto now = juce::Time::getMillisecondCounter();

        if (now - targetsStamp > 250 || targetsStamp == 0)
        {
            targetsStamp = now;
            targetsCache.clear();
            for (int i = 0; i < (int) envs.size(); ++i)
                targetsCache.add (targetsText (i));
        }

        return targetsCache[env];
    }

    juce::StringArray targetsCache;
    juce::uint32 targetsStamp = 0;

    // What this envelope drives: its built-in jobs, then the matrix.
    juce::String targetsText (int env) const
    {
        const auto& info = envs[(size_t) env];
        juce::StringArray fixed;

        if (info.source == Mod::Source::AmpEnv)
            fixed.add ("Amp");
        if (info.source == Mod::Source::FilterEnv && std::abs (readParam ("f1_env")) > 0.001f)
            fixed.add ("Filter 1");
        if (info.source == Mod::Source::FilterEnv2 && std::abs (readParam ("f2_env")) > 0.001f)
            fixed.add ("Filter 2");

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);

            if (env > 0 && processorRef.isOscillatorShown (osc) && (int) readParam (prefix + "_amp_env") == env)
                fixed.add ("Osc" + juce::String (osc + 1) + " Amp");
            if ((int) readParam (prefix + "_pd_env") == env + 1)
                fixed.add ("Osc" + juce::String (osc + 1) + " Warp");
        }

        return describeModTargets (processorRef, info.source, fixed);
    }

    void timerCallback() override
    {
        if (isShowing())
        {
            // Assigning an envelope elsewhere, or loading a patch, can add a card.
            if (numCards() != lastCardCount)
                layoutChanged();
            if (isMouseOver (true) || changeGate.check (processorRef.getUiEpoch()))
                repaint();
        }
    }

    IlanaAnim::ChangeGate changeGate;

    IlanaSynthAudioProcessor& processorRef;
    std::vector<Env> envs;
    int selected = 0;
    int hoverIndex = -1;
    int viewWidth = 0;
    int lastCardCount = -1;
};
