#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "AnimationUtils.h"
#include "FmOperatorInfo.h"
#include "IlanaLookAndFeel.h"
#include "LfoShapeMenu.h"
#include "OperatorEnvDisplay.h"
#include "ParamControls.h"

// The Operator Env in the MOD page's pools (UI review 7, I7-7, V7-5, S7-6,
// S7-27): OP ENV and OP PITCH are cards in the envelope pool and OP LFO one
// in the LFO pool, the same size as the others, edited in place below them
// with the editors the FM page uses (the Operator Env graph, its stage knobs,
// the LFO's controls). They are always in the pool; while no oscillator
// plays the Operator Env they are greyed and say why. OP PITCH and OP LFO
// drag onto a knob like any pool source (the OP PITCH and OP LFO
// sources); OP ENV shapes its operators only, so it doesn't drag.
namespace OperatorPool
{
// The FM page's colour, which the Operator Env wears everywhere.
inline juce::Colour colour() { return juce::Colour (0xffe3a56f); }

// Why the Operator Env's cards are greyed.
inline const char* unusedReason() { return "no oscillator plays the Operator Env (an oscillator's ENVELOPE picks it)"; }

// The share of the row the graph takes (as the LFO and envelope panels).
inline constexpr int graphPercent = 47;

// The DX7 LFO's waves (opeg_lfo_wave, DX7 order) at a phase, -1..1.
inline float lfoWaveValue (int wave, double phase)
{
    phase -= std::floor (phase);
    const auto p = (float) phase;
    switch (wave)
    {
        case 0: return 1.0f - 2.0f * std::abs (2.0f * p - 1.0f);
        case 1: return 1.0f - 2.0f * p;
        case 2: return 2.0f * p - 1.0f;
        case 3: return p < 0.5f ? 1.0f : -1.0f;
        case 4: return std::sin (p * juce::MathConstants<float>::twoPi);
        default:
        {
            static const float held[] { 0.6f, -0.3f, 0.9f, -0.8f, 0.1f, -0.5f, 0.4f, -0.1f };
            return held[juce::jlimit (0, 7, (int) (p * 8.0f))];
        }
    }
}

// The oscillators that play the Operator Env now.
inline std::vector<int> operatorsOnEnv (const IlanaSynthAudioProcessor& p)
{
    std::vector<int> list;
    for (int osc = 0; osc < OscillatorIds::count; ++osc)
        if (FmOperatorInfo::isPlaying (p, osc) && FmOperatorInfo::usesOperatorEnv (p, osc))
            list.push_back (osc);
    return list;
}

// An envelope's shape for a card: samples 0..1 over its run (an operator's
// level, or the pitch with a little room around the note), rebuilt only when
// its settings change.
struct ShapeCache
{
    OperatorEnv::Settings settings {};
    bool valid = false;
    std::vector<float> values;
    bool moves = false; // the pitch envelope moves the pitch
};

inline const ShapeCache& envelopeShape (const IlanaSynthAudioProcessor& p, const juce::String& prefix, ShapeCache& cache)
{
    const auto settings = OperatorEnv::read (p, prefix);
    if (cache.valid && settings == cache.settings)
        return cache;

    cache.settings = settings;
    cache.valid = true;
    cache.values.clear();
    const auto run = OperatorEnv::run (settings);
    auto low = 1.0e9, high = -1.0e9;
    for (const auto value : run.values)
    {
        low = std::min (low, value);
        high = std::max (high, value);
    }
    cache.moves = high - low > 0.11;
    if (prefix.isEmpty())
        low = std::min (low, -0.05), high = std::max (high, 0.05);
    const auto step = juce::jmax<size_t> (1, run.values.size() / 48);
    for (size_t i = 0; i < run.values.size(); i += step)
        cache.values.push_back ((float) ((run.values[i] - low) / std::max (1.0e-6, high - low)));
    return cache;
}

// A card's picture of an envelope shape, as the pool's other cards draw theirs.
inline void paintEnvelopeShape (juce::Graphics& g, juce::Rectangle<float> plot, const std::vector<float>& values, juce::Colour colour,
                                bool active)
{
    if (values.size() < 2)
        return;
    juce::Path path;
    for (size_t i = 0; i < values.size(); ++i)
    {
        const juce::Point<float> point (plot.getX() + plot.getWidth() * (float) i / (float) (values.size() - 1),
                                        plot.getBottom() - plot.getHeight() * values[i]);
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    auto fill = path;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (active ? 0.16f : 0.08f));
    g.fillPath (fill);
    g.setColour (colour.withAlpha (active ? 0.95f : 0.6f));
    g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// One of the Operator Env's stage knobs, as the FM page makes them: a rate
// reads as its stage's time (the rest of the envelope as set), and "250 ms"
// or "1.5 s" typed sets that time; its 0-99 is in the tooltip. Every knob of
// the pool's Operator Env editors is made here, so their behaviour has one
// place to follow the FM page's.
inline std::unique_ptr<KnobControl> makeStageKnob (IlanaSynthAudioProcessor& p, const juce::String& prefix, const juce::String& suffix,
                                                   const juce::String& label, juce::Colour colour)
{
    auto knob = std::make_unique<KnobControl> (p.apvts, prefix + suffix, label, colour, false);
    const auto rateStage = suffix.endsWith ("_r1") ? 0 : suffix.endsWith ("_r2") ? 1 : suffix.endsWith ("_r3") ? 2 : suffix.endsWith ("_r4") ? 3 : -1;
    if (rateStage >= 0)
    {
        // The pitch envelope's prefix is empty (its ids are whole).
        const auto envPrefix = prefix.startsWith ("osc") ? prefix : juce::String();
        auto& slider = knob->getSlider();
        // Clockwise is longer, as on the FM page (review 7, I7-3).
        FmOperatorInfo::reverseRateKnob (slider);
        const auto original = slider.valueFromTextFunction;
        slider.textFromValueFunction = [&p, envPrefix, rateStage] (double value)
        {
            auto settings = OperatorEnv::read (p, envPrefix);
            settings.rates[(size_t) rateStage] = juce::jlimit (0, 99, juce::roundToInt (value));
            const auto run = OperatorEnv::run (settings, rateStage);
            return OperatorEnv::formatSeconds (run.stageSeconds (rateStage), run.endless[(size_t) rateStage]);
        };
        slider.valueFromTextFunction = [&p, envPrefix, rateStage, original] (const juce::String& text)
        {
            const auto trimmed = text.trim().toLowerCase();
            if (trimmed.endsWith ("ms") || trimmed.endsWith ("s"))
            {
                const auto seconds = trimmed.getDoubleValue() * (trimmed.endsWith ("ms") ? 0.001 : 1.0);
                return (double) OperatorEnv::rateForSeconds (OperatorEnv::read (p, envPrefix), rateStage, seconds);
            }
            return original != nullptr ? original (text) : trimmed.getDoubleValue();
        };
        slider.updateText();
    }
    return knob;
}
} // namespace OperatorPool

// Rate knobs' times follow the levels around them, and every knob's tooltip
// carries its DX7 value: kept current on the editors' timers.
class OperatorKnobTexts
{
public:
    void add (KnobControl& knob, const juce::String& envPrefix, bool rate)
    {
        entries.push_back ({ &knob, envPrefix, knob.getTooltip(), -1, rate, {} });
    }

    void refresh (const IlanaSynthAudioProcessor& p)
    {
        for (auto& entry : entries)
        {
            if (! entry.knob->isShowing())
                continue;
            const auto value = juce::roundToInt (entry.knob->getSlider().getValue());
            if (value != entry.lastValue)
            {
                entry.lastValue = value;
                const auto tooltip = entry.baseTooltip + "\nDX7 value: " + juce::String (value);
                entry.knob->setTooltip (tooltip);
                entry.knob->getSlider().setTooltip (tooltip);
            }
            if (entry.rate)
            {
                const auto settings = OperatorEnv::read (p, entry.prefix);
                if (settings != entry.settings)
                {
                    entry.settings = settings;
                    entry.knob->getSlider().updateText();
                }
            }
        }
    }

private:
    struct Entry
    {
        KnobControl* knob;
        juce::String prefix, baseTooltip;
        int lastValue;
        bool rate;
        OperatorEnv::Settings settings;
    };
    std::vector<Entry> entries;
};

// The panel card beside a pool editor's graph: its title, a line about it
// and, at the right of the header, a small "OPEN ON FM" link.
inline void paintOperatorPanel (juce::Graphics& g, juce::Rectangle<int> panel, const juce::String& title, const juce::String& subtitle,
                                int reserveRight)
{
    IlanaTheme::paintCard (g, panel.toFloat(), 7.0f, OperatorPool::colour().withAlpha (0.35f));
    IlanaTheme::paintCardHeader (g, panel.reduced (12, 0).withHeight (26), title, subtitle, OperatorPool::colour(), reserveRight);
}

inline void styleFmLink (juce::TextButton& button)
{
    button.setButtonText (juce::String (juce::CharPointer_UTF8 ("FM \xe2\x80\xba")));
    button.getProperties().set ("pill", true);
    button.setColour (juce::TextButton::buttonOnColourId, OperatorPool::colour());
}

// OP ENV and OP PITCH, edited in the envelope pool's place: the Operator Env
// graph (draggable, as on FM) and its stage knobs. OP ENV picks the operator
// on the header line (the oscillators on the Operator Env); OP PITCH is the
// voice's pitch envelope.
class OperatorEnvEditor : public juce::Component,
                          private IlanaAnim::FrameTimer
{
public:
    explicit OperatorEnvEditor (IlanaSynthAudioProcessor& p) : processorRef (p), graph (p), effectRules (p)
    {
        addAndMakeVisible (graph);
        styleFmLink (fmLink);
        fmLink.setTooltip ("Open this on the FM page, beside the algorithm.");
        fmLink.onClick = [this] { FmOperatorInfo::openOperator (pitch ? -1 : selectedOperator); };
        addAndMakeVisible (fmLink);

        const auto anyOperator = [this] { return FmOperatorInfo::anyOperatorEnv (processorRef); };

        // Per oscillator: the times, then the levels, the key rate and the
        // operator's output LEVEL (I7-2's words).
        static const char* const suffixes[] { "_eg_r1", "_eg_r2", "_eg_r3", "_eg_r4", "_eg_rate_key",
                                               "_eg_l1", "_eg_l2", "_eg_l3", "_eg_l4", "_eg_out" };
        static const char* const labels[] { "ATTACK", "DECAY 1", "DECAY 2", "RELEASE", "KEY RATE", "PEAK", "MID", "SUSTAIN", "END", "LEVEL" };
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto prefix = FmOperatorInfo::prefixOf (osc);
            for (size_t i = 0; i < 10; ++i)
            {
                auto knob = OperatorPool::makeStageKnob (p, prefix, suffixes[i], labels[i], IlanaTheme::oscColour (osc));
                addChildComponent (*knob);
                texts.add (*knob, prefix, i < 4);
                effectRules.add (*knob, [this, osc] { return FmOperatorInfo::isPlaying (processorRef, osc) && FmOperatorInfo::usesOperatorEnv (processorRef, osc); },
                                 "OSC " + juce::String (osc + 1) + " doesn't play the Operator Env");
                operatorKnobs[(size_t) osc][i] = std::move (knob);
            }

            auto pill = std::make_unique<juce::TextButton> ("OSC " + juce::String (osc + 1));
            pill->getProperties().set ("pill", true);
            pill->setColour (juce::TextButton::buttonOnColourId, IlanaTheme::oscColour (osc));
            pill->setTooltip ("Edit OSC " + juce::String (osc + 1) + "'s Operator Env");
            pill->onClick = [this, osc] { selectOperator (osc); };
            addChildComponent (*pill);
            pills[(size_t) osc] = std::move (pill);
        }

        static const char* const pitchSuffixes[] { "opeg_pitch_r1", "opeg_pitch_r2", "opeg_pitch_r3", "opeg_pitch_r4",
                                                    "opeg_pitch_l1", "opeg_pitch_l2", "opeg_pitch_l3", "opeg_pitch_l4" };
        static const char* const pitchLabels[] { "ATTACK", "DECAY 1", "DECAY 2", "RELEASE", "PEAK", "MID", "SUSTAIN", "END" };
        for (size_t i = 0; i < 8; ++i)
        {
            pitchKnobs[i] = OperatorPool::makeStageKnob (p, {}, pitchSuffixes[i], pitchLabels[i], OperatorPool::colour());
            addChildComponent (*pitchKnobs[i]);
            texts.add (*pitchKnobs[i], {}, i < 4);
            effectRules.add (*pitchKnobs[i], anyOperator, OperatorPool::unusedReason());
        }

        startTimerHz (8);
    }

    // OP ENV (false) or OP PITCH (true).
    void setPitch (bool shouldShowPitch)
    {
        pitch = shouldShowPitch;
        refreshOperators();
        updateVisibility();
    }

    bool isPitch() const { return pitch; }
    int getSelectedOperator() const { return selectedOperator; }

    void selectOperator (int osc)
    {
        selectedOperator = juce::jlimit (0, OscillatorIds::count - 1, osc);
        updateVisibility();
    }

    // The UI test reaches these.
    OperatorEnvDisplay& getGraph() { return graph; }
    KnobControl* getKnob (int osc, int index) { return operatorKnobs[(size_t) juce::jlimit (0, 5, osc)][(size_t) juce::jlimit (0, 9, index)].get(); }

    void paint (juce::Graphics& g) override
    {
        const auto active = FmOperatorInfo::anyOperatorEnv (processorRef);
        const auto title = pitch ? juce::String ("OP PITCH") : juce::String ("OP ENV");
        const auto subtitle = ! active ? juce::String ("unused: ") + OperatorPool::unusedReason()
                              : pitch  ? juce::String ("the pitch of every oscillator on the Operator Env")
                                       : "OSC " + juce::String (selectedOperator + 1) + "'s level; drag the graph or the knobs";
        paintOperatorPanel (g, panel, title, subtitle, panel.getRight() - headerLeft + 8);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        graph.setBounds (area.removeFromLeft (area.getWidth() * OperatorPool::graphPercent / 100).reduced (2));
        area.removeFromLeft (8);
        panel = area;

        // The header: FM link at the right, the operator pills before it.
        auto header = panel.reduced (12, 0).withHeight (26).withSizeKeepingCentre (panel.getWidth() - 24, 20);
        fmLink.setBounds (header.removeFromRight (46));
        header.removeFromRight (8);
        headerLeft = fmLink.getX();
        if (! pitch)
            for (int i = OscillatorIds::count - 1; i >= 0; --i)
                if (pills[(size_t) i]->isVisible())
                {
                    pills[(size_t) i]->setBounds (header.removeFromRight (54));
                    header.removeFromRight (4);
                    headerLeft = pills[(size_t) i]->getX();
                }

        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        std::vector<juce::Component*> first, second;
        if (pitch)
        {
            for (size_t i = 0; i < 8; ++i)
                (i < 4 ? first : second).push_back (pitchKnobs[i].get());
        }
        else
        {
            for (size_t i = 0; i < 10; ++i)
                (i < 5 ? first : second).push_back (operatorKnobs[(size_t) selectedOperator][i].get());
        }

        // Two rows (times over levels) when both fit full-size knobs, else
        // one row of all of them, as the envelope panel does.
        constexpr int fullRow = 13 + 58 + 16 + 6;
        if (inner.getHeight() < fullRow * 2)
        {
            auto all = first;
            all.insert (all.end(), second.begin(), second.end());
            layoutRow (inner, all);
            return;
        }
        auto rows = inner.withSizeKeepingCentre (inner.getWidth(), fullRow * 2);
        layoutRow (rows.removeFromTop (fullRow), first);
        layoutRow (rows, second);
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;
        refreshOperators();
        effectRules.apply();
        texts.refresh (processorRef);
        if (changeGate.check (processorRef.getUiEpoch()))
            repaint (panel.withHeight (26));
    }

    // The pills follow the oscillators on the Operator Env; the selection
    // moves to one of them when its operator leaves.
    void refreshOperators()
    {
        const auto onEnv = OperatorPool::operatorsOnEnv (processorRef);
        auto changed = false;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto wanted = ! pitch && std::find (onEnv.begin(), onEnv.end(), osc) != onEnv.end();
            if (pills[(size_t) osc]->isVisible() != wanted)
            {
                pills[(size_t) osc]->setVisible (wanted);
                changed = true;
            }
            pills[(size_t) osc]->setToggleState (osc == selectedOperator, juce::dontSendNotification);
        }
        if (! onEnv.empty() && std::find (onEnv.begin(), onEnv.end(), selectedOperator) == onEnv.end())
        {
            selectedOperator = onEnv.front();
            changed = true;
        }
        if (changed)
            updateVisibility();
    }

    void updateVisibility()
    {
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            for (auto& knob : operatorKnobs[(size_t) osc])
                knob->setVisible (! pitch && osc == selectedOperator);
        for (auto& knob : pitchKnobs)
            knob->setVisible (pitch);
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            pills[(size_t) osc]->setToggleState (osc == selectedOperator, juce::dontSendNotification);
        graph.setSource (pitch ? juce::String() : FmOperatorInfo::prefixOf (selectedOperator),
                         pitch ? OperatorPool::colour() : IlanaTheme::oscColour (selectedOperator));
        resized();
        repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    OperatorEnvDisplay graph;
    juce::TextButton fmLink;
    std::array<std::array<std::unique_ptr<KnobControl>, 10>, (size_t) OscillatorIds::count> operatorKnobs;
    std::array<std::unique_ptr<KnobControl>, 8> pitchKnobs;
    std::array<std::unique_ptr<juce::TextButton>, (size_t) OscillatorIds::count> pills;
    OperatorKnobTexts texts;
    EffectRules effectRules;
    IlanaAnim::ChangeGate changeGate;
    juce::Rectangle<int> panel;
    int headerLeft = 0;
    int selectedOperator = 0;
    bool pitch = false;
};

// The Op LFO's picture: one cycle of its wave, as the pool LFOs' graphs
// draw theirs, with its live value at the right edge.
class OperatorLfoView : public juce::Component,
                        public juce::SettableTooltipClient,
                        private IlanaAnim::FrameTimer
{
public:
    explicit OperatorLfoView (IlanaSynthAudioProcessor& p) : processorRef (p)
    {
        setTooltip ("OP LFO\nOne cycle of its shape. Its DELAY fades it in after each note (with RETRIG on, it restarts too); "
                    "the tick at the right is where it is now.");
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 6.0f);
        const auto plot = bounds.reduced (12.0f, 16.0f);
        const auto centreY = plot.getCentreY();
        const auto half = plot.getHeight() * 0.42f;
        const auto active = FmOperatorInfo::anyOperatorEnv (processorRef);
        const auto colour = OperatorPool::colour().withAlpha (active ? 1.0f : 0.35f);

        g.setColour (juce::Colours::white.withAlpha (0.04f));
        for (const auto level : { -1.0f, -0.5f, 0.5f, 1.0f })
            g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withPosition (plot.getX(), centreY - level * half));
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (plot.getWidth(), 1.0f).withCentre ({ plot.getCentreX(), centreY }));

        const auto wave = juce::roundToInt (FmOperatorInfo::read (processorRef, "opeg_lfo_wave"));
        juce::Path path;
        const auto points = juce::jmax (2, (int) plot.getWidth());
        for (int i = 0; i <= points; ++i)
        {
            const auto phase = (double) i / (double) points;
            const auto x = plot.getX() + plot.getWidth() * (float) phase;
            const auto y = centreY - OperatorPool::lfoWaveValue (wave, juce::jmin (0.99999, phase)) * half;
            if (i == 0)
                path.startNewSubPath (x, y);
            else if (wave == 3 || wave == 5)
            {
                path.lineTo (x, path.getCurrentPosition().y);
                path.lineTo (x, y);
            }
            else
                path.lineTo (x, y);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.6f));

        if (active)
        {
            const auto now = juce::jlimit (-1.0f, 1.0f, processorRef.getSourceDisplayValue ((int) Mod::Source::OpLfo));
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float> (8.0f, 2.0f).withPosition (plot.getRight() + 2.0f, centreY - now * half - 1.0f));
        }

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        g.drawText (active ? "one cycle at " + describeValue ("opeg_lfo_speed", FmOperatorInfo::read (processorRef, "opeg_lfo_speed"))
                           : juce::String ("unused: no oscillator plays the Operator Env"),
                    bounds.reduced (12.0f, 2.0f).removeFromBottom (13.0f), juce::Justification::centredLeft);
    }

private:
    void timerCallback() override
    {
        if (isShowing() && changeGate.check (processorRef.getUiEpoch()
                                             ^ IlanaAnim::phaseSignature (processorRef.getSourceDisplayValue ((int) Mod::Source::OpLfo), 41)))
            repaint();
    }

    IlanaSynthAudioProcessor& processorRef;
    IlanaAnim::ChangeGate changeGate;
};

// OP LFO, edited in the LFO pool's place, laid out as the pool LFOs' panel:
// SHAPE and RETRIG on the top row, RATE and DELAY below, then what it moves
// on the operators (the DX7's PITCH DEPTH, PITCH SENS and AMP DEPTH).
class OperatorLfoEditor : public juce::Component,
                          private IlanaAnim::FrameTimer
{
public:
    explicit OperatorLfoEditor (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          view (p),
          shape (p.apvts, "opeg_lfo_wave", "SHAPE"),
          retrig (p.apvts, "opeg_lfo_sync", "RETRIG"),
          rate (p.apvts, "opeg_lfo_speed", "RATE", OperatorPool::colour(), false),
          delay (p.apvts, "opeg_lfo_delay", "DELAY", OperatorPool::colour(), false),
          pitchDepth (p.apvts, "opeg_lfo_pmd", "PITCH DEPTH", OperatorPool::colour(), false),
          pitchSens (p.apvts, "opeg_lfo_pms", "PITCH SENS", OperatorPool::colour(), false),
          ampDepth (p.apvts, "opeg_lfo_amd", "AMP DEPTH", OperatorPool::colour(), false),
          effectRules (p)
    {
        retrig.showAsSwitch();
        LfoShapeMenu::applyOperatorLfo (shape);
        styleFmLink (fmLink);
        fmLink.setTooltip ("Open this on the FM page, beside the algorithm.");
        fmLink.onClick = [] { FmOperatorInfo::openOperator (-1); };
        for (auto* child : std::initializer_list<juce::Component*> { &view, &shape, &retrig, &rate, &delay, &pitchDepth, &pitchSens, &ampDepth, &fmLink })
            addAndMakeVisible (child);
        const auto anyOperator = [this] { return FmOperatorInfo::anyOperatorEnv (processorRef); };
        for (auto* knob : { &rate, &delay, &pitchDepth, &pitchSens, &ampDepth })
            effectRules.add (*knob, anyOperator, OperatorPool::unusedReason());
        startTimerHz (8);
    }

    // The UI test reaches these.
    ComboControl& getShape() { return shape; }
    juce::Rectangle<int> getPanelBounds() const { return panel; }

    void paint (juce::Graphics& g) override
    {
        const auto active = FmOperatorInfo::anyOperatorEnv (processorRef);
        paintOperatorPanel (g, panel, "OP LFO",
                            active ? "the DX7 LFO: every oscillator on the Operator Env follows it"
                                   : juce::String ("unused: ") + OperatorPool::unusedReason(),
                            panel.getRight() - fmLink.getX() + 8);
        // The operator half of the controls, under its own small heading.
        if (! operatorHeading.isEmpty())
        {
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.drawText ("ON THE OPERATORS", operatorHeading, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        view.setBounds (area.removeFromLeft (area.getWidth() * OperatorPool::graphPercent / 100).reduced (2));
        area.removeFromLeft (8);
        panel = area;
        fmLink.setBounds (panel.reduced (12, 0).withHeight (26).withSizeKeepingCentre (panel.getWidth() - 24, 20).removeFromRight (46));

        auto inner = panel.reduced (10, 6);
        inner.removeFromTop (20);
        // As the LFO panel: SHAPE and the switch on one row, the knobs
        // filling the rest.
        auto options = inner.removeFromTop (13 + 24 + 14);
        shape.setBounds (options.removeFromLeft (options.getWidth() / 2).reduced (3, 1));
        retrig.setBounds (options.removeFromLeft (options.getWidth() / 3).reduced (3, 1));
        inner.removeFromTop (4);
        operatorHeading = inner.withLeft (inner.getX() + inner.getWidth() * 2 / 5 + 3).withHeight (12);
        inner.removeFromTop (12);
        layoutRow (inner, { &rate, &delay, &pitchDepth, &pitchSens, &ampDepth });
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;
        effectRules.apply();
        if (changeGate.check (processorRef.getUiEpoch()))
            repaint (panel.withHeight (26));
    }

    IlanaSynthAudioProcessor& processorRef;
    OperatorLfoView view;
    ComboControl shape;
    ToggleControl retrig;
    KnobControl rate, delay, pitchDepth, pitchSens, ampDepth;
    juce::TextButton fmLink;
    EffectRules effectRules;
    IlanaAnim::ChangeGate changeGate;
    juce::Rectangle<int> panel, operatorHeading;
};
