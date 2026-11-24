// The PATCH tab's LFO boxes (FxLfo.h; ilana, 2026-10-12: "add lfo effects
// module to modulate fx in the patcher ... first you add an lfo module and
// connect it with a cable to the module you are trying to modulate, then you
// right click in the small window and assign to that lfo"). The box's
// controls, its picture, its floating window, and the row a selected LFO
// cable shows (how far it moves each knob). The classes are declared here;
// their member functions are in PatchLfo.cpp.
#pragma once

#include <map>
#include <optional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../PluginProcessor.h"
#include "../DisplayStyle.h"
#include "../FxInstanceControls.h"
#include "../IlanaLookAndFeel.h"
#include "../OperatorEnvDisplay.h"
#include "../ParamControls.h"
#include "PageHelpers.h"

inline juce::Colour fxLfoColour (int id) { return IlanaSynthAudioProcessor::lfoColour (juce::jlimit (0, 15, id - 1)); }
inline juce::String fxLfoName (int id) { return "FX LFO " + juce::String (id); }

//==============================================================================
// One LFO box's settings as controls: the usual KnobControl / ComboControl /
// ToggleControl on shadow parameters of their own, written into the FX patch.
// A knob's drag is one undo step (its gesture); a menu or switch is one each.
class FxLfoControls : private juce::AudioProcessorParameter::Listener
{
public:
    FxLfoControls (IlanaSynthAudioProcessor& p, int lfoId);

    ~FxLfoControls() override;

    // The box's settings onto the controls (not heard back as an edit).
    void show();

    FxLfo::Settings settings() const { return shown; }
    bool isSynced() const { return shown.sync; }

    // The controls a window lays out, in order (RATE is the knob or the division menu).
    std::vector<juce::Component*> windowItems() const;

    std::vector<juce::Component*> allItems() const;

    std::function<void()> onLayoutChanged; // SYNC flipped: RATE becomes a menu or a knob

    const int id;

private:
    void set (const char* paramId, float value);

    float get (const char* paramId) const;

    FxLfo::Settings fromControls() const;

    void parameterValueChanged (int, float) override;

    void parameterGestureChanged (int, bool starting) override;

    IlanaSynthAudioProcessor& processorRef;
    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    std::unique_ptr<ComboControl> shape, division, polarity;
    std::unique_ptr<ToggleControl> sync, retrig;
    std::unique_ptr<KnobControl> rate, depth, phase, smooth;
    FxLfo::Settings shown;
    bool syncing = false, gestureOpen = false;
};

//==============================================================================
// The wave an LFO box makes: two cycles of its shape in the shared trace
// style (DisplayStyle.h), DEPTH and POLARITY applied, with a dot riding it.
// The random shapes show a representative picture, as the LFO page does.
inline float fxLfoPictureValue (const FxLfo::Settings& settings, int shape, double phase)
{
    if (FxLfo::isDrawnShape (shape))
        return FxLfo::drawnValue (settings.drawn, phase, shape == LfoShapes::Steps);
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
                            float livePhase, bool showDot, bool withArea, bool detailed = false)
{
    const auto shape = FxLfo::shapeOf (settings.shape);
    const auto cycles = 2.0;
    const auto centreY = settings.bipolar ? plot.getCentreY() : plot.getBottom();
    const auto half = settings.bipolar ? plot.getHeight() * 0.5f : plot.getHeight();
    const auto yOf = [&] (float shapeValue) { return centreY - FxLfo::output (settings, shapeValue) * half; };

    // SMOOTH glides the wave (as the engine does: a glide of SMOOTH x a cycle), so the
    // picture shows it: a one-pole over points a pixel apart, run over the cycle before.
    const auto pixels = juce::jmax (2, (int) plot.getWidth());
    const auto perPixel = cycles / (double) pixels;
    const auto glideBy = settings.smooth > 0.0f ? 1.0 - std::exp (-perPixel / (double) settings.smooth) : 1.0;
    const auto valueAt = [&] (double phase)
    {
        if (settings.smooth <= 0.0f)
            return fxLfoPictureValue (settings, shape, phase);
        auto glided = (double) fxLfoPictureValue (settings, shape, phase - 1.0);
        for (int k = (int) ((double) pixels / cycles); k >= 0; --k)
            glided += ((double) fxLfoPictureValue (settings, shape, phase - (double) k * perPixel) - glided) * glideBy;
        return (float) glided;
    };

    // The zero line (where the knob is set) under the trace.
    g.setColour (IlanaTheme::Ui::line);
    g.fillRect (plot.getX(), std::round (centreY) - 0.5f, plot.getWidth(), 1.0f);

    // The big picture (the LFO window, as the MOD page's LFO: ilana
    // 2026-10-15, "visualization parity"): +1 / 0 / -1 against the zero line
    // with faint rails, the live reading above and the time under it.
    if (detailed)
    {
        const auto tiny = IlanaTheme::font (IlanaTheme::TextSize::tiny);
        g.setFont (tiny);
        const auto label = [&] (const juce::String& text, float y)
        {
            g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
            g.drawText (text, juce::Rectangle<float> (plot.getX() - 26.0f, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredRight);
        };
        const auto rail = [&] (float y)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (plot.getX(), std::round (y) - 0.5f, plot.getWidth(), 1.0f);
        };
        rail (centreY - half);
        label ("+1", centreY - half);
        if (settings.bipolar)
        {
            rail (centreY + half);
            label ("0", centreY);
            label ("-1", centreY + half);
        }
        else
            label ("0", centreY);

        const auto cycleSeconds = settings.sync ? 0.0 : 1.0 / (double) settings.rateHz;
        for (int tick = 0; tick <= 4; ++tick)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) tick / 4.0f;
            juce::String text;
            if (settings.sync)
                text = tick == 0 ? juce::String ("0") : tick == 2 ? FxLfo::rateText (settings) : tick == 4 ? "2 x " + FxLfo::rateText (settings) : juce::String();
            else
            {
                const auto ms = cycleSeconds * 2.0 * 1000.0 * (double) tick / 4.0;
                text = ms >= 1000.0 ? juce::String (ms / 1000.0, 1) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
            }
            if (text.isNotEmpty())
            {
                g.setColour (IlanaTheme::Ui::text3.withAlpha (0.8f));
                g.drawText (text, juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 3.0f, 60.0f, 12.0f), juce::Justification::centred);
            }
        }

        const auto phase = (double) livePhase;
        const auto value = FxLfo::output (settings, valueAt (phase));
        const auto readout = juce::String ("PHASE ") + juce::String (phase - std::floor (phase), 2) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  VALUE "))
                             + (value >= 0.0f ? "+" : "") + juce::String (value, 2);
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.35f));
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        g.drawText (readout, juce::Rectangle<float> (plot.getX(), plot.getY() - 15.0f, plot.getWidth(), 12.0f), juce::Justification::centredLeft);
        if (FxLfo::isDrawnShape (shape))
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText ("DRAW HERE  |  DOUBLE-CLICK RESETS", juce::Rectangle<float> (plot.getX(), plot.getY() - 15.0f, plot.getWidth(), 12.0f),
                        juce::Justification::centredRight);
        }
    }

    juce::Path path;
    const auto width = juce::jmax (2, (int) plot.getWidth());

    auto glided = 0.0;
    if (settings.smooth > 0.0f)
        glided = (double) valueAt ((double) settings.phase - perPixel);
    for (int x = 0; x <= width; ++x)
    {
        const auto phase = (double) x / (double) width * cycles + (double) settings.phase;
        const auto px = plot.getX() + (float) x * plot.getWidth() / (float) width;
        glided += ((double) fxLfoPictureValue (settings, shape, phase) - glided) * glideBy;
        const auto py = yOf (settings.smooth > 0.0f ? (float) glided : fxLfoPictureValue (settings, shape, phase));
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
        const auto y = yOf (valueAt (phase));
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
        g.setColour (IlanaTheme::Ui::panel);
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }), 1.0f);
    }
}

class FxLfoDisplay : public juce::Component
{
public:
    FxLfoDisplay (IlanaSynthAudioProcessor& p, FxLfoControls& c) : processorRef (p), controls (c) {}
    ~FxLfoDisplay() override
    {
        if (editing)
        {
            processorRef.endEdit();
            editing = false;
        }
    }

    void paint (juce::Graphics& g) override;

    // Drawn and Steps are drawn on the picture (the big one in the window):
    // a drag sets the points under the pointer, a double-click gives back the sine.
    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;

    // (Not drawable: the click goes through to the window or the page under it.)
    bool hitTest (int x, int y) override { return canDraw() && juce::Component::hitTest (x, y); }

    juce::MouseCursor getMouseCursor() override;

private:
    bool canDraw() const { return getHeight() >= 100 && FxLfo::isDrawnShape (FxLfo::shapeOf (controls.settings().shape)); }

    juce::Rectangle<float> plotArea() const;

    // The pointer's place as a point of the drawn table and a value, as the picture shows them.
    void drawAt (juce::Point<float> position);

    IlanaSynthAudioProcessor& processorRef;
    FxLfoControls& controls;
    std::optional<FxLfo::Settings> live;
    bool editing = false;
    int lastIndex = -1;
    float lastValue = 0.0f;
};

//==============================================================================
// An LFO box's floating window (double-click its box), as an effect's is:
// its wave, then its controls three across. Dragged by its title, closed with x.
class FxLfoWindow : public juce::Component
{
public:
    FxLfoWindow (IlanaSynthAudioProcessor& p, int lfoId);

    const int id;
    std::function<void (FxLfoWindow&)> onClose, onMoved;

    // The patch's settings onto the controls, and the dot on.
    void refresh();

    void paint (juce::Graphics& g) override;

    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;

    void setLimits (juce::Rectangle<int> area);

private:
    // (The effect windows' grid: FxMiniWindow.)
    static constexpr int pad = Space::windowPad, gap = Space::windowGap, titleHeight = Space::windowTitleHeight, displayHeight = 124, cellWidth = Space::windowCellWidth, cellHeight = Space::windowCellHeight;

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

    explicit FxLfoAmounts (IlanaSynthAudioProcessor& p);

    ~FxLfoAmounts() override;

    // The cable from LFO `lfo` into `slot`: which knobs, named, and their amounts.
    void show (int lfo, int slot, const std::vector<std::pair<juce::String, juce::String>>& targets, const juce::String& effectName,
               juce::Colour colour);

    int numShown() const { return (int) params.size(); }
    int totalAssigned() const;

    std::array<std::unique_ptr<KnobControl>, maxShown> knobs;

private:
    void parameterValueChanged (int index, float) override;

    void parameterGestureChanged (int, bool starting) override;

    IlanaSynthAudioProcessor& processorRef;
    FxInstance::ShadowProcessor holder;
    std::unique_ptr<juce::AudioProcessorValueTreeState> state;
    std::vector<juce::String> params;
    int lfoId = 0, slotIndex = -1;
    bool syncing = false, gestureOpen = false;
};
