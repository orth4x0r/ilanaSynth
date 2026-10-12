// The PATCH tab's LFO boxes (FxLfo.h; ilana, 2026-10-12: "add lfo effects
// module to modulate fx in the patcher ... first you add an lfo module and
// connect it with a cable to the module you are trying to modulate, then you
// right click in the small window and assign to that lfo"). The box's
// controls, its picture, its floating window, and the row a selected LFO
// cable shows (how far it moves each knob). Included only from
// PluginEditor.cpp, before PatchPage.h.
#pragma once

#include <map>
#include <optional>

namespace
{
inline juce::Colour fxLfoColour (int id) { return IlanaSynthAudioProcessor::lfoColour (juce::jlimit (0, 15, id - 1)); }
inline juce::String fxLfoName (int id) { return "FX LFO " + juce::String (id); }

//==============================================================================
// One LFO box's settings as controls: the usual KnobControl / ComboControl /
// ToggleControl on shadow parameters of their own, written into the FX patch.
// A knob's drag is one undo step (its gesture); a menu or switch is one each.
class FxLfoControls : private juce::AudioProcessorParameter::Listener
{
public:
    FxLfoControls (IlanaSynthAudioProcessor& p, int lfoId) : processorRef (p), id (lfoId)
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_shape", 1 }, "Shape", FxLfo::shapeNames(), 0));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "lfo_sync", 1 }, "Sync", false));
        NormalisableRange<float> rateRange (FxLfo::minRateHz, FxLfo::maxRateHz);
        rateRange.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_rate", 1 }, "Rate", rateRange, 0.5f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                           { return v < 1.0f ? String (v, 2) + " Hz" : String (v, 1) + " Hz"; })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_div", 1 }, "Division", FxLfo::divisionNames(), FxLfo::defaultDivision));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_depth", 1 }, "Depth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                           { return String (roundToInt (v)) + " %"; })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lfo_phase", 1 }, "Phase", NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                           { return String (roundToInt (v)) + juce::String::fromUTF8 ("\xc2\xb0"); })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfo_polarity", 1 }, "Polarity", StringArray { "Bipolar", "Unipolar" }, 0));
        state = std::make_unique<AudioProcessorValueTreeState> (holder, nullptr, "FxLfo", std::move (layout));

        const auto colour = fxLfoColour (id);
        shape = std::make_unique<ComboControl> (*state, "lfo_shape", "SHAPE");
        sync = std::make_unique<ToggleControl> (*state, "lfo_sync", "SYNC");
        rate = std::make_unique<KnobControl> (*state, "lfo_rate", "RATE", colour, false);
        division = std::make_unique<ComboControl> (*state, "lfo_div", "RATE");
        depth = std::make_unique<KnobControl> (*state, "lfo_depth", "DEPTH", colour, false);
        phase = std::make_unique<KnobControl> (*state, "lfo_phase", "PHASE", colour, false);
        polarity = std::make_unique<ComboControl> (*state, "lfo_polarity", "POLARITY");
        for (auto* knob : { rate.get(), depth.get(), phase.get() })
            knob->setInlineKnob (true);
        shape->setTooltip ("SHAPE: the LFO's wave. Sample & Hold, Smooth Random, Drunk and Chaos are random.");
        sync->setTooltip ("SYNC: the rate follows the song's tempo, from 8 bars to 1/32 (with the transport playing, in step with the bar).");
        rate->setTooltip ("RATE: how fast it moves, 0.02 to 20 Hz.");
        division->setTooltip ("RATE: one cycle per this much of the bar.");
        depth->setTooltip ("DEPTH: how far it moves every knob it is assigned to (each knob has its own amount on the cable).");
        phase->setTooltip ("PHASE: where in its cycle it starts (with SYNC, against the bar).");
        polarity->setTooltip ("POLARITY: Bipolar moves a knob both ways round where it is set; Unipolar moves it one way from there.");

        show();
        for (auto* parameter : state->processor.getParameters())
            parameter->addListener (this);
    }

    ~FxLfoControls() override
    {
        for (auto* parameter : state->processor.getParameters())
            parameter->removeListener (this);
        // (The controls hold attachments to the parameters: they go first.)
        shape.reset();
        sync.reset();
        rate.reset();
        division.reset();
        depth.reset();
        phase.reset();
        polarity.reset();
        if (gestureOpen)
            processorRef.endEdit();
    }

    // The box's settings onto the controls (not heard back as an edit).
    void show()
    {
        if (gestureOpen)
            return;
        const auto patch = processorRef.getFxPatch();
        const auto* lfo = patch.findLfo (id);
        if (lfo == nullptr)
            return;
        const juce::ScopedValueSetter<bool> quiet (syncing, true);
        const auto& s = lfo->settings;
        set ("lfo_shape", (float) s.shape);
        set ("lfo_sync", s.sync ? 1.0f : 0.0f);
        set ("lfo_rate", s.rateHz);
        set ("lfo_div", (float) s.division);
        set ("lfo_depth", s.depth * 100.0f);
        set ("lfo_phase", s.phase * 360.0f);
        set ("lfo_polarity", s.bipolar ? 0.0f : 1.0f);
        shown = s;
    }

    FxLfo::Settings settings() const { return shown; }
    bool isSynced() const { return shown.sync; }

    // The controls a window lays out, in order (RATE is the knob or the division menu).
    std::vector<juce::Component*> windowItems() const
    {
        return { shape.get(), sync.get(), shown.sync ? (juce::Component*) division.get() : (juce::Component*) rate.get(), depth.get(),
                 phase.get(), polarity.get() };
    }

    std::vector<juce::Component*> allItems() const
    {
        return { shape.get(), sync.get(), rate.get(), division.get(), depth.get(), phase.get(), polarity.get() };
    }

    std::function<void()> onLayoutChanged; // SYNC flipped: RATE becomes a menu or a knob

    const int id;

private:
    void set (const char* paramId, float value)
    {
        auto* parameter = state->getParameter (paramId);
        const auto normalised = parameter->convertTo0to1 (value);
        if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
            parameter->setValueNotifyingHost (normalised);
    }

    float get (const char* paramId) const
    {
        const auto* parameter = state->getParameter (paramId);
        return parameter->convertFrom0to1 (parameter->getValue());
    }

    FxLfo::Settings fromControls() const
    {
        FxLfo::Settings s;
        s.shape = juce::roundToInt (get ("lfo_shape"));
        s.sync = get ("lfo_sync") > 0.5f;
        s.rateHz = get ("lfo_rate");
        s.division = juce::roundToInt (get ("lfo_div"));
        s.depth = get ("lfo_depth") / 100.0f;
        s.phase = juce::jlimit (0.0f, 1.0f, get ("lfo_phase") / 360.0f);
        s.bipolar = get ("lfo_polarity") < 0.5f;
        return s.sanitised();
    }

    void parameterValueChanged (int, float) override
    {
        if (syncing)
            return;
        const auto next = fromControls();
        if (next == shown)
            return;
        const auto layoutChanged = next.sync != shown.sync;
        shown = next;
        if (gestureOpen)
            processorRef.setFxLfoSettings (id, next);
        else
            processorRef.performEdit (fxLfoName (id) + " settings", [this, next] { processorRef.setFxLfoSettings (id, next); });
        if (layoutChanged && onLayoutChanged)
            juce::MessageManager::callAsync ([cb = onLayoutChanged] { cb(); });
    }

    void parameterGestureChanged (int, bool starting) override
    {
        if (starting && ! gestureOpen)
        {
            gestureOpen = true;
            processorRef.beginEdit (fxLfoName (id) + " settings");
        }
        else if (! starting && gestureOpen)
        {
            gestureOpen = false;
            processorRef.endEdit();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    std::unique_ptr<ComboControl> shape, division, polarity;
    std::unique_ptr<ToggleControl> sync;
    std::unique_ptr<KnobControl> rate, depth, phase;
    FxLfo::Settings shown;
    bool syncing = false, gestureOpen = false;
};

//==============================================================================
// The wave an LFO box makes: two cycles of its shape in the shared trace
// style (DisplayStyle.h), DEPTH and POLARITY applied, with a dot riding it.
// The random shapes show a representative picture, as the LFO page does.
inline float fxLfoPictureValue (int shape, double phase)
{
    if (shape == LfoShapes::SampleHold)
    {
        // (One held value per cycle, as it plays: four cycles shown.)
        static constexpr float steps[] { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, 0.5f, -0.6f, -0.1f };
        const auto cycle = (int) std::floor (phase * 2.0);
        return steps[(size_t) (((cycle % 8) + 8) % 8)];
    }
    return lfoShapeValue (shape, phase);
}

inline void paintFxLfoWave (juce::Graphics& g, juce::Rectangle<float> plot, const FxLfo::Settings& settings, juce::Colour colour,
                            float livePhase, bool showDot, bool withArea)
{
    const auto shape = FxLfo::shapeOf (settings.shape);
    const auto cycles = 2.0;
    const auto centreY = settings.bipolar ? plot.getCentreY() : plot.getBottom();
    const auto half = settings.bipolar ? plot.getHeight() * 0.5f : plot.getHeight();
    const auto yOf = [&] (float shapeValue) { return centreY - FxLfo::output (settings, shapeValue) * half; };

    // The zero line (where the knob is set) under the trace.
    g.setColour (IlanaTheme::Ui::line);
    g.fillRect (plot.getX(), std::round (centreY) - 0.5f, plot.getWidth(), 1.0f);

    juce::Path path;
    const auto width = juce::jmax (2, (int) plot.getWidth());
    for (int x = 0; x <= width; ++x)
    {
        const auto phase = (double) x / (double) width * cycles + (double) settings.phase;
        const auto px = plot.getX() + (float) x * plot.getWidth() / (float) width;
        const auto py = yOf (fxLfoPictureValue (shape, phase));
        if (x == 0)
            path.startNewSubPath (px, py);
        else
            path.lineTo (px, py);
    }
    if (withArea)
        IlanaTrace::paintArea (g, path, colour, centreY, plot.getX(), plot.getRight());
    IlanaTrace::paintTrace (g, path, colour);

    if (showDot)
    {
        // The live phase (it already carries PHASE) on the first cycle.
        const auto phase = (double) livePhase;
        const auto x = plot.getX() + (float) (phase - (double) settings.phase - std::floor (phase - (double) settings.phase)) / (float) cycles * plot.getWidth();
        const auto y = yOf (fxLfoPictureValue (shape, phase));
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
        g.setColour (IlanaTheme::Ui::panel);
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }), 1.0f);
    }
}

class FxLfoDisplay : public juce::Component
{
public:
    FxLfoDisplay (IlanaSynthAudioProcessor& p, const FxLfoControls& c) : processorRef (p), controls (c) {}

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);
        paintFxLfoWave (g, bounds.reduced (10.0f, 8.0f), controls.settings(), fxLfoColour (controls.id), processorRef.getFxLfoPhase (controls.id),
                        true, true);
    }

private:
    IlanaSynthAudioProcessor& processorRef;
    const FxLfoControls& controls;
};

//==============================================================================
// An LFO box's floating window (double-click its box), as an effect's is:
// its wave, then its controls three across. Dragged by its title, closed with x.
class FxLfoWindow : public juce::Component
{
public:
    FxLfoWindow (IlanaSynthAudioProcessor& p, int lfoId) : id (lfoId), controls (p, lfoId), display (p, controls)
    {
        setName ("FX WINDOW " + fxLfoName (lfoId)); // (the UI test finds it by name)
        constrainer.owner = this;
        closeButton.setTooltip ("Close this window");
        closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        closeButton.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text2);
        closeButton.onClick = [this] { if (onClose) onClose (*this); };
        addAndMakeVisible (closeButton);
        addAndMakeVisible (display);
        for (auto* item : controls.allItems())
            addChildComponent (item);
        controls.onLayoutChanged = [safe = juce::Component::SafePointer<FxLfoWindow> (this)] { if (safe != nullptr) safe->resized(); };
        const auto rows = 2;
        setSize (3 * cellWidth + 2 * gap + 2 * pad, titleHeight + displayHeight + gap + rows * cellHeight + (rows - 1) * gap + pad);
    }

    const int id;
    std::function<void (FxLfoWindow&)> onClose, onMoved;

    // The patch's settings onto the controls, and the dot on.
    void refresh()
    {
        const auto synced = controls.isSynced();
        controls.show();
        if (synced != controls.isSynced())
            resized();
        display.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace IlanaTheme;
        const auto colour = fxLfoColour (id);
        const auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f).reduced (2.0f), 10.0f);
        paintCard (g, bounds.reduced (1.0f), 10.0f, colour.withAlpha (0.55f));
        auto title = getLocalBounds().removeFromTop (titleHeight).reduced (pad, 0);
        paintTag (g, { (float) title.getX() + 4.0f, (float) title.getCentreY() }, colour);
        g.setColour (Ui::text);
        g.setFont (font (TextSize::body, true));
        g.drawText (fxLfoName (id), title.withTrimmedLeft (16).withTrimmedRight (60), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto title = area.removeFromTop (titleHeight).reduced (pad, 0);
        closeButton.setBounds (title.removeFromRight (28).withSizeKeepingCentre (28, 28));
        area = area.withTrimmedLeft (pad).withTrimmedRight (pad).withTrimmedBottom (pad);
        display.setBounds (area.removeFromTop (displayHeight));
        area.removeFromTop (gap);
        for (auto* item : controls.allItems())
            item->setVisible (false);
        const auto items = controls.windowItems();
        for (size_t k = 0; k < items.size(); ++k)
        {
            const auto column = (int) k % 3, row = (int) k / 3;
            items[k]->setBounds (area.getX() + column * (cellWidth + gap), area.getY() + row * (cellHeight + gap), cellWidth, cellHeight);
            items[k]->setVisible (true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        toFront (false);
        dragging = e.y < titleHeight;
        if (dragging)
            dragger.startDraggingComponent (this, e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging)
            dragger.dragComponent (this, e, &constrainer);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && onMoved)
            onMoved (*this);
        dragging = false;
    }

    void setLimits (juce::Rectangle<int> area)
    {
        limits = area;
        setBounds (getBounds().constrainedWithin (limits));
    }

private:
    // (The effect windows' grid: FxMiniWindow.)
    static constexpr int pad = 12, gap = 8, titleHeight = 40, displayHeight = 64, cellWidth = 116, cellHeight = 44;

    struct Constrainer : juce::ComponentBoundsConstrainer
    {
        FxLfoWindow* owner = nullptr;
        void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>&, const juce::Rectangle<int>&, bool, bool, bool, bool) override
        {
            if (owner != nullptr && ! owner->limits.isEmpty())
                bounds = bounds.constrainedWithin (owner->limits);
        }
    };

    FxLfoControls controls;
    FxLfoDisplay display;
    juce::TextButton closeButton { juce::String::fromUTF8 ("\xc3\x97") };
    juce::ComponentDragger dragger;
    Constrainer constrainer;
    juce::Rectangle<int> limits;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxLfoWindow)
};

//==============================================================================
// A selected LFO cable's row: how far it moves each knob it is assigned to
// (AMOUNT knobs named after the knobs, -100 to 100 %), at most four shown.
class FxLfoAmounts : private juce::AudioProcessorParameter::Listener
{
public:
    static constexpr int maxShown = 4;

    explicit FxLfoAmounts (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int k = 0; k < maxShown; ++k)
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "amt" + juce::String (k), 1 }, "Amount",
                                                                     juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 50.0f,
                                                                     juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                                         [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
        state = std::make_unique<juce::AudioProcessorValueTreeState> (holder, nullptr, "FxLfoAmounts", std::move (layout));
        for (int k = 0; k < maxShown; ++k)
        {
            knobs[(size_t) k] = std::make_unique<KnobControl> (*state, "amt" + juce::String (k), "AMOUNT", IlanaTheme::accent(), false);
            knobs[(size_t) k]->setInlineKnob (true);
            knobs[(size_t) k]->setTooltip ("How far the LFO moves this knob at full DEPTH, either way. Not modulatable.");
        }
        for (auto* parameter : state->processor.getParameters())
            parameter->addListener (this);
    }

    ~FxLfoAmounts() override
    {
        for (auto* parameter : state->processor.getParameters())
            parameter->removeListener (this);
        for (auto& knob : knobs)
            knob.reset();
        if (gestureOpen)
            processorRef.endEdit();
    }

    // The cable from LFO `lfo` into `slot`: which knobs, named, and their amounts.
    void show (int lfo, int slot, const std::vector<std::pair<juce::String, juce::String>>& targets, const juce::String& effectName,
               juce::Colour colour)
    {
        if (gestureOpen)
            return;
        lfoId = lfo;
        slotIndex = slot;
        params.clear();
        const auto patch = processorRef.getFxPatch();
        const juce::ScopedValueSetter<bool> quiet (syncing, true);
        for (const auto& route : patch.lfoRoutes)
            if (route.lfo == lfo && route.slot == slot && (int) params.size() < maxShown)
            {
                const auto k = (int) params.size();
                params.push_back (route.param);
                auto name = route.param;
                for (const auto& [param, label] : targets)
                    if (param == route.param)
                        name = FxLfo::shortName (label, effectName);
                auto& knob = *knobs[(size_t) k];
                if (knob.getLabelText() != name.toUpperCase())
                    knob.setLabelText (name.toUpperCase());
                knob.setIdentityColour (colour);
                auto* parameter = state->getParameter ("amt" + juce::String (k));
                const auto normalised = parameter->convertTo0to1 (route.amount * 100.0f);
                if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
                    parameter->setValueNotifyingHost (normalised);
            }
    }

    int numShown() const { return (int) params.size(); }
    int totalAssigned() const
    {
        const auto patch = processorRef.getFxPatch();
        return (int) std::count_if (patch.lfoRoutes.begin(), patch.lfoRoutes.end(), [this] (const auto& r) { return r.lfo == lfoId && r.slot == slotIndex; });
    }

    std::array<std::unique_ptr<KnobControl>, maxShown> knobs;

private:
    void parameterValueChanged (int index, float) override
    {
        if (syncing || ! juce::isPositiveAndBelow (index, (int) params.size()))
            return;
        auto* parameter = state->getParameter ("amt" + juce::String (index));
        const auto amount = parameter->convertFrom0to1 (parameter->getValue()) / 100.0f;
        const auto param = params[(size_t) index];
        if (gestureOpen)
            processorRef.setFxLfoAmount (lfoId, slotIndex, param, amount);
        else
            processorRef.performEdit ("LFO amount", [this, param, amount] { processorRef.setFxLfoAmount (lfoId, slotIndex, param, amount); });
    }

    void parameterGestureChanged (int, bool starting) override
    {
        if (starting && ! gestureOpen)
        {
            gestureOpen = true;
            processorRef.beginEdit ("LFO amount");
        }
        else if (! starting && gestureOpen)
        {
            gestureOpen = false;
            processorRef.endEdit();
        }
    }

    IlanaSynthAudioProcessor& processorRef;
    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    std::vector<juce::String> params;
    int lfoId = 0, slotIndex = -1;
    bool syncing = false, gestureOpen = false;
};
} // namespace
