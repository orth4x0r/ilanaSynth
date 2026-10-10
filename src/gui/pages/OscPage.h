// Split verbatim from PluginEditor.cpp. Included only from PluginEditor.cpp,
// after its includes; the contents stay in the same anonymous namespace.
#pragma once

namespace
{
// What an oscillator is in the patch's FM, read the same way by PLAY's
// strips and OSC's cards, so an operator reads as one (its ratio, its
// Operator EG, what it modulates) wherever it shows (UI review 6, V3, S3,
// I6-5).
namespace OscRole
{
inline juce::String prefix (int osc) { return OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)]; }

inline float read (const IlanaSynthAudioProcessor& p, const juce::String& id)
{
    const auto* value = p.apvts.getRawParameterValue (id);
    return value != nullptr ? value->load() : 0.0f;
}

// ENVELOPE choice 17 (appended): the Operator EG.
constexpr int operatorEgChoice = 17;

inline bool usesOperatorEg (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::roundToInt (read (p, prefix (osc) + "_amp_env")) == operatorEgChoice;
}

inline int tuning (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::jlimit (0, OscTuning::Count - 1, juce::roundToInt (read (p, prefix (osc) + "_tune")));
}

// The oscillators this one modulates (its feedback aside).
inline std::vector<int> targets (const IlanaSynthAudioProcessor& p, int osc)
{
    std::vector<int> result;

    for (int target = 0; target < OscillatorIds::count; ++target)
        if (target != osc && p.isOscillatorShown (target) && read (p, FmDiagram::routeId (osc, target)) > 0.001f)
            result.push_back (target);

    return result;
}

// The oscillators that modulate this one.
inline std::vector<int> sources (const IlanaSynthAudioProcessor& p, int osc)
{
    std::vector<int> result;

    for (int source = 0; source < OscillatorIds::count; ++source)
        if (source != osc && p.isOscillatorShown (source) && read (p, FmDiagram::routeId (source, osc)) > 0.001f)
            result.push_back (source);

    return result;
}

inline int mode (const IlanaSynthAudioProcessor& p, int osc)
{
    return juce::jlimit (0, OscMode::count - 1, juce::roundToInt (read (p, prefix (osc) + "_mode")));
}

// An operator is an oscillator of the FM / DX7 type (2026-10-06; before, a
// wavetable tuned by ratio or fixed Hz or on the Operator EG, which now loads
// as one). One in an FM route but of another type stays an oscillator with
// FM, as on Vital (UI review 7, V7-15).
inline bool isOperator (const IlanaSynthAudioProcessor& p, int osc)
{
    return mode (p, osc) == OscMode::fmOperator;
}

// The type menu's order: FM / DX7 next to Wavetable (both play tables);
// the saved index is unchanged.
inline const std::array<int, OscMode::count>& modeMenuOrder()
{
    static const std::array<int, OscMode::count> order { OscMode::wavetable, OscMode::fmOperator, OscMode::physical,
                                                         OscMode::sample, OscMode::granular, OscMode::live };
    return order;
}

inline juce::String modeName (int mode)
{
    return OscMode::names[(size_t) juce::jlimit (0, OscMode::count - 1, mode)];
}

// Picks an oscillator's type as one undo step, through the processor (so
// leaving FM / DX7 drops the operator's tuning and Operator EG).
inline void chooseMode (IlanaSynthAudioProcessor& p, int osc, int newMode)
{
    if (newMode < 0 || newMode >= OscMode::count || newMode == mode (p, osc))
        return;
    p.performEdit ("OSC " + juce::String (osc + 1) + " type " + modeName (newMode),
                   [&p, osc, newMode] { p.setOscillatorMode (osc, newMode); });
}

// The type menu's items in its order (the UI tests read them).
inline juce::StringArray modeMenuItems()
{
    juce::StringArray items;
    for (const auto entry : modeMenuOrder())
        items.add (modeName (entry));
    return items;
}

// A type menu (oscN_mode) that picks through chooseMode: its list in the
// menu's order, the arrow keys step through the same order.
inline void bindModeMenu (ComboControl& control, IlanaSynthAudioProcessor& p, int osc)
{
    auto* box = &control.getComboBox();
    control.setPopupOverride ([box, &p, osc]
    {
        juce::PopupMenu menu;
        const auto current = mode (p, osc);
        for (const auto entry : modeMenuOrder())
            menu.addItem (entry + 1, modeName (entry), true, entry == current);

        juce::Component::SafePointer<juce::ComboBox> safe (box);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (box), [safe, &p, osc] (int result)
        {
            if (safe != nullptr && result > 0)
                chooseMode (p, osc, result - 1);
        });
    });
    control.setNudgeOverride ([&p, osc] (int step)
    {
        const auto& order = modeMenuOrder();
        const auto at = (int) (std::find (order.begin(), order.end(), mode (p, osc)) - order.begin());
        const auto next = juce::jlimit (0, OscMode::count - 1, at + step);
        chooseMode (p, osc, order[(size_t) next]);
    });
}

// The operator's own choices in an oscillator's TUNING and ENVELOPE menus:
// Ratio, Fixed Hz and OP ENV belong to FM / DX7. Elsewhere TUNING keeps only
// Semitones (Ratio and Fixed Hz greyed, so the menu says where they are),
// and a Wavetable's ENVELOPE has no OP ENV. The saved indices are unchanged.
// `current`: the ENVELOPE parameter's index, shown once the menu is rebuilt
// (its attachment could not show OP ENV while the item was missing).
inline void showOperatorChoices (juce::ComboBox* tune, juce::ComboBox* envelope, bool asOperator, bool envelopeChoice, int current)
{
    if (tune != nullptr)
        for (const auto choice : { OscTuning::Ratio, OscTuning::Fixed })
            if (tune->isItemEnabled (choice + 1) != asOperator)
                tune->setItemEnabled (choice + 1, asOperator);

    if (envelope == nullptr)
        return;

    const auto id = OperatorEg::envelopeChoice + 1;
    const auto has = envelope->indexOfItemId (id) >= 0;
    if (has == envelopeChoice)
    {
        if (envelope->getSelectedId() != current + 1 && envelope->indexOfItemId (current + 1) >= 0)
            envelope->setSelectedId (current + 1, juce::dontSendNotification);
        return;
    }

    // Rebuilt in place, with any section headings, so the saved index keeps
    // its item.
    struct Entry { juce::String text; int itemId; bool heading; };
    std::vector<Entry> entries;
    for (juce::PopupMenu::MenuItemIterator it (*envelope->getRootMenu(), false); it.next();)
    {
        const auto& item = it.getItem();
        if (item.itemID == id || (item.isSectionHeader && item.text == "FM PAGE"))
            continue;
        entries.push_back ({ item.text, item.itemID, item.isSectionHeader });
    }
    const auto sectioned = std::any_of (entries.begin(), entries.end(), [] (const Entry& e) { return e.heading; });
    envelope->clear (juce::dontSendNotification);
    for (const auto& entry : entries)
    {
        if (entry.heading)
            envelope->addSectionHeading (entry.text);
        else if (entry.itemId != 0)
            envelope->addItem (entry.text, entry.itemId);
    }
    if (envelopeChoice)
    {
        if (sectioned)
            envelope->addSectionHeading ("FM PAGE");
        envelope->addItem ("OP ENV", id);
    }
    envelope->setSelectedId (current + 1, juce::dontSendNotification);
}

inline juce::String oscList (const std::vector<int>& oscs, const juce::String& before)
{
    juce::StringArray names;
    for (const auto osc : oscs)
        names.add (before + juce::String (osc + 1));
    return names.joinIntoString (", ");
}

// "OSC 2 30 %": the FM routes' depths named beside the oscillators, so the
// caption on OSC carries the number the FM page and the matrix edit (review
// 11, I11-4). `from`: the routes into osc, else out of it.
inline juce::String depthList (const IlanaSynthAudioProcessor& p, int osc, const std::vector<int>& others, bool from)
{
    juce::StringArray names;
    for (const auto other : others)
        names.add ("OSC " + juce::String (other + 1) + " "
                   + juce::String (juce::roundToInt (read (p, from ? FmDiagram::routeId (other, osc) : FmDiagram::routeId (osc, other)) * 100.0f)) + " %");
    return names.joinIntoString (", ");
}

// "OUT", "MOD → 1, 3", "OUT, MOD → 2" or "SILENT": an oscillator's part in
// the FM routing, in the FM diagram's words, whether or not it is an
// operator (I8-19); empty for an oscillator in no FM route.
inline juce::String describe (const IlanaSynthAudioProcessor& p, int osc)
{
    const auto modulated = targets (p, osc);
    if (! isOperator (p, osc) && modulated.empty() && sources (p, osc).empty())
        return {};

    const auto out = read (p, prefix (osc) + "_out") > 0.5f;
    if (modulated.empty())
        return out ? "OUT" : "SILENT";

    juce::StringArray numbers;
    for (const auto target : modulated)
        numbers.add (juce::String (target + 1));

    // The arrow the matrix uses, not a greater-than sign (review 11, I11-16).
    return juce::String (out ? "OUT, MOD " : "MOD ") + juce::String::fromUTF8 ("\xe2\x86\x92 ") + numbers.joinIntoString (", ");
}

// A modulator: it feeds other oscillators and is not itself heard. Its
// A modulator's level is still its OUTPUT: a DX7 operator shows one level, named
// OUTPUT everywhere (UI-CONVENTIONS, review 15 I15-1).
inline bool isModulator (const IlanaSynthAudioProcessor& p, int osc)
{
    return read (p, prefix (osc) + "_out") <= 0.5f && ! targets (p, osc).empty();
}

inline const char* outputKnobName (const IlanaSynthAudioProcessor& p, int osc)
{
    juce::ignoreUnused (p, osc);
    return "OUTPUT";
}

// PLAY's strip role line: what the oscillator does with its OUTPUT. A carrier
// goes "TO OUTPUT", a modulator "MODULATES 1, 3" (I12-3); the tabs keep the
// short form of describe().
inline juce::String roleLine (const IlanaSynthAudioProcessor& p, int osc)
{
    if (describe (p, osc).isEmpty())
        return {};

    const auto out = read (p, prefix (osc) + "_out") > 0.5f;
    const auto modulated = targets (p, osc);
    if (modulated.empty())
        return out ? "TO OUTPUT" : "SILENT";

    juce::StringArray numbers;
    for (const auto target : modulated)
        numbers.add (juce::String (target + 1));

    return juce::String (out ? "TO OUTPUT, MODULATES " : "MODULATES ") + numbers.joinIntoString (", ");
}

// The same in words, for tooltips and the OSC card's header.
inline juce::String describeLong (const IlanaSynthAudioProcessor& p, int osc)
{
    if (! isOperator (p, osc))
    {
        juce::StringArray parts;
        if (const auto from = sources (p, osc); ! from.empty())
            parts.add ("FM FROM " + depthList (p, osc, from, true)); // upper case as on the tab (review 8, S8-30)
        if (const auto into = targets (p, osc); ! into.empty())
            parts.add ("FM INTO " + depthList (p, osc, into, false));
        return parts.joinIntoString (", ");
    }

    const auto out = read (p, prefix (osc) + "_out") > 0.5f;
    const auto modulated = targets (p, osc);
    juce::StringArray parts;
    parts.add ("FM operator");

    if (out)
        parts.add (modulated.empty() ? "a carrier (heard)" : "heard");
    else if (modulated.empty())
        parts.add ("silent (OUT off, modulates nothing)");

    if (! modulated.empty())
    {
        juce::StringArray names;
        for (const auto target : modulated)
            names.add ("OSC " + juce::String (target + 1));
        parts.add ("modulates " + names.joinIntoString (", "));
    }

    if (usesOperatorEg (p, osc))
        parts.add ("plays its OP ENV");

    return parts.joinIntoString (", ");
}
} // namespace OscRole

// A frame scrubber under the wave picture (review 12, S12-2): 64 ticks across
// the table, the thumb on the oscillator's FRAME; a drag sets it.
class FrameScrubber : public juce::Slider
{
public:
    FrameScrubber()
    {
        setSliderStyle (juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setTooltip ("Frame\nDrag across the table's frames: the same as the picture's own drag and the FRAME knob.");
    }

    void setColour (juce::Colour newColour) { colour = newColour; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
        const auto position = (float) valueToProportionOfLength (getValue());
        constexpr int ticks = 64;
        for (int i = 0; i < ticks; ++i)
        {
            const auto t = (float) i / (float) (ticks - 1);
            const auto x = area.getX() + t * area.getWidth();
            const auto major = i % 8 == 0;
            g.setColour ((t <= position ? colour : IlanaTheme::Ui::text3).withAlpha (t <= position ? 0.9f : 0.5f));
            g.fillRect (juce::Rectangle<float> (1.0f, major ? 8.0f : 5.0f).withCentre ({ x, area.getCentreY() }));
        }
        const auto thumb = juce::Rectangle<float> (4.0f, (float) getHeight() - 4.0f).withCentre ({ area.getX() + position * area.getWidth(), area.getCentreY() });
        g.setColour (juce::Colours::white.withAlpha (isMouseOverOrDragging() ? 1.0f : 0.85f));
        g.fillRoundedRectangle (thumb, 2.0f);
    }

private:
    juce::Colour colour = IlanaTheme::accent();
};

// An operator's Operator Env as a small picture, where a plain oscillator
// shows its wave (PLAY's strips: UI review 7, I7-19): the engine's own run
// at C3, time on the graphs' square-root scale, levels down to -60 dB.
struct OperatorEnvThumb
{
    // Re-reads the operator; true when its shape changed.
    bool update (const IlanaSynthAudioProcessor& p, int osc)
    {
        const auto now = OperatorEnv::read (p, OscRole::prefix (osc));

        if (valid && now == settings)
            return false;

        settings = now;
        valid = true;
        const auto run = OperatorEnv::run (settings);
        const auto total = juce::jmax (1.0, OperatorEnv::blocksToSeconds ((int) run.values.size() - 1));
        const auto step = juce::jmax<size_t> (1, run.values.size() / 400);
        points.clear();

        for (size_t i = 0; i < run.values.size(); i += step)
            points.push_back ({ (float) std::sqrt (OperatorEnv::blocksToSeconds ((int) i) / total),
                                juce::jlimit (0.0f, 1.0f, 1.0f + (float) run.values[i] / 10.0f) });

        keyUp = (float) std::sqrt (OperatorEnv::blocksToSeconds (run.stageEnd[2]) / total);
        return true;
    }

    void paint (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, bool lit) const
    {
        IlanaTheme::paintWell (g, area, 6.0f);
        const auto plot = area.reduced (8.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        const float dashes[] { 3.0f, 3.0f };
        const auto x = plot.getX() + keyUp * plot.getWidth();
        g.drawDashedLine ({ x, plot.getY(), x, plot.getBottom() }, dashes, 2, 1.0f);

        juce::Path line, fill;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const juce::Point<float> point (plot.getX() + points[i].x * plot.getWidth(), plot.getBottom() - points[i].y * plot.getHeight());
            if (i == 0)
            {
                line.startNewSubPath (point);
                fill.startNewSubPath (point.x, plot.getBottom());
            }
            else
                line.lineTo (point);
            fill.lineTo (point);
        }
        if (points.empty())
            return;
        fill.lineTo (plot.getX() + points.back().x * plot.getWidth(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (colour.withAlpha (lit ? 0.16f : 0.06f));
        g.fillPath (fill);
        g.setColour (colour.withAlpha (lit ? 0.9f : 0.3f));
        g.strokePath (line, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    OperatorEnv::Settings settings;
    bool valid = false;
    std::vector<juce::Point<float>> points;
    float keyUp = 0.5f;
};

// EXCITE's menu in groups, under the one set of exciter names (UI review 7,
// I7-27). The items keep their ids, so the saved choice is unchanged.
inline void groupExciteMenu (ComboControl& control)
{
    auto& box = control.getComboBox();
    // (Read before renaming: getSelectedId() only matches while the shown
    // text equals the item's text. Re-selected so the box shows the new name.)
    const auto selectedId = box.getSelectedId();

    for (int excite = 0; excite < juce::jmin (box.getNumItems(), Exciters::names().size()); ++excite)
        box.changeItemText (excite + 1, Exciters::name (excite));

    box.setSelectedId (0, juce::dontSendNotification);
    box.setSelectedId (selectedId, juce::dontSendNotification);

    control.setPopupOverride ([combo = &box]
    {
        juce::PopupMenu menu;

        for (const auto& [group, members] : Exciters::groups())
        {
            menu.addSectionHeader (group);

            for (const auto excite : members)
                menu.addItem (excite + 1, Exciters::name (excite), true, combo->getSelectedId() == excite + 1);
        }

        juce::Component::SafePointer<juce::ComboBox> safe (combo);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (combo), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->setSelectedId (result);
        });
    });
}

// Opens the PHYSICAL page on an oscillator's string (defined after
// PhysicalPage, in FilterVectorPhysicalPages.h).
void showPhysicalString (juce::Component& from, int osc);


// The unison block's little picture (the OSC cards' UNISON row): one bar per
// voice, spread across the width as far as DETUNE reaches, the outer voices
// shorter. A picture only; it takes no clicks.
class UnisonSpreadView : public juce::Component
{
public:
    UnisonSpreadView (IlanaSynthAudioProcessor& p, int index)
        : processorRef (p), prefix (OscillatorIds::prefixes[(size_t) index])
    {
        setInterceptsMouseClicks (false, false);
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    // Re-reads the voices and the detune; repaints when they changed.
    void update()
    {
        const auto voiceCount = juce::jlimit (1, 16, juce::roundToInt (read ("_unison")));
        const auto detune = read ("_detune");
        const auto active = read ("_unison") > 1.5f;

        if (voiceCount != voices || ! juce::approximatelyEqual (detune, spread) || active != isActive)
        {
            voices = voiceCount;
            spread = detune;
            isActive = active;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        const auto count = voices;
        const auto detuneNorm = [this]
        {
            if (auto* parameter = processorRef.apvts.getParameter (prefix + "_detune"))
                return parameter->convertTo0to1 (spread);
            return 0.5f;
        }();
        const auto half = area.getWidth() * 0.5f - 6.0f;
        const auto reach = half * (0.18f + 0.82f * juce::jlimit (0.0f, 1.0f, detuneNorm));

        for (int i = 0; i < count; ++i)
        {
            const auto position = count == 1 ? 0.0f : ((float) i - (float) (count - 1) * 0.5f) / ((float) (count - 1) * 0.5f);
            const auto closeness = 1.0f - std::abs (position);
            const auto height = juce::jmin (area.getHeight() - 4.0f, 8.0f + 14.0f * closeness);
            const auto bar = juce::Rectangle<float> (4.0f, height).withCentre ({ area.getCentreX() + position * reach, area.getCentreY() });
            g.setColour (colour.withAlpha (0.45f + 0.5f * closeness));
            g.fillRoundedRectangle (bar, 2.0f);
        }
    }

private:
    float read (const char* suffix) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (prefix + suffix);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour = IlanaTheme::accent();
    int voices = 0;
    float spread = -1.0f;
    bool isActive = false;
};

// A Physical oscillator's STRING row, right of its knobs: the string's
// partials as bars (their levels from the strike point, DAMP and STIFF, as
// the PHYSICAL page's PARTIALS read-out), so the row reads as the wavetable
// card's, full to its end, without a second copy of the string's controls
// (those stay on PHYSICAL: UI review 9, I9-3). A picture only.
class StringPartialsView : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    StringPartialsView (IlanaSynthAudioProcessor& p, int index)
        : processorRef (p), prefix (OscillatorIds::prefixes[(size_t) index])
    {
        setTooltip ("The string's partials: where it is struck (EXCITE POS), DAMP and STIFF set how strong each is. OSC > PHYSICAL edits the string.");
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    std::function<void()> onClick;

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (onClick != nullptr && getLocalBounds().contains (event.getPosition()))
            onClick();
    }

    // Re-reads the settings; repaints when they changed.
    void update()
    {
        const std::array<float, 3> now { read ("_string_damp"), read ("_string_stiffness"), read ("_string_excite_pos") };
        if (now != shown)
        {
            shown = now;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
        const auto title = area.removeFromLeft (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), "PARTIALS") + 2.0f);
        g.drawText ("PARTIALS", title.toNearestInt(), juce::Justification::centredLeft, false);
        area.removeFromLeft (8.0f);
        const auto plot = area.reduced (0.0f, 2.0f);

        const auto damp = shown[0], stiff = shown[1];
        const auto pos = juce::jlimit (0.05f, 0.5f, shown[2] > 0.001f ? shown[2] : 0.25f);
        constexpr int count = 16;
        const auto slot = plot.getWidth() / (float) count;
        for (int n = 1; n <= count; ++n)
        {
            const auto comb = 0.3f + 0.7f * std::abs (std::sin (juce::MathConstants<float>::pi * (float) n * pos));
            // (Drawn on a square-root scale, so the upper partials still show in a 28 px row.)
            const auto level = std::sqrt (juce::jlimit (0.02f, 1.0f, comb / std::pow ((float) n, 0.55f + 1.4f * damp) * (1.0f + 0.4f * stiff * (float) (n % 3))));
            const auto bar = juce::Rectangle<float> (juce::jmin (4.0f, slot * 0.6f), plot.getHeight() * level)
                                 .withCentre ({ plot.getX() + slot * ((float) n - 0.5f), plot.getBottom() - plot.getHeight() * level * 0.5f });
            g.setColour (colour.withAlpha (0.45f + 0.5f * level));
            g.fillRoundedRectangle (bar, 2.0f);
        }
    }

private:
    float read (const char* suffix) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (prefix + suffix);
        return value != nullptr ? value->load() : 0.0f;
    }

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour = IlanaTheme::accent();
    std::array<float, 3> shown { -1.0f, -1.0f, -1.0f };
};

// The right-hand cells of an oscillator's PITCH row: an oscillator in an FM
// route says so in a chip in its source's colour (a link to the FM page), one
// in none shows its output level as a bar.
class OscStatusView : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    OscStatusView (IlanaSynthAudioProcessor& p, int index)
        : processorRef (p), osc (index), colour (IlanaTheme::oscColour (index)) {}

    std::function<void()> onClick;

    // Re-reads the role and the level; repaints when they changed.
    void update (const juce::String& chipTextIn, juce::Colour chipColourIn, bool isLink, const juce::String& tip)
    {
        const auto* levelValue = processorRef.apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_level");
        const auto newLevel = levelValue != nullptr ? juce::jlimit (0.0f, 1.0f, levelValue->load()) : 0.0f;

        if (chipTextIn != chipText || chipColourIn != chipColour || isLink != link || ! juce::approximatelyEqual (newLevel, level))
        {
            chipText = chipTextIn;
            chipColour = chipColourIn;
            link = isLink;
            level = newLevel;
            setMouseCursor (link ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            setTooltip (tip);
            repaint();
        }
    }

    void setColour (juce::Colour newColour)
    {
        colour = newColour;
        repaint();
    }

    bool hitTest (int, int) override { return link && chipText.isNotEmpty(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (link && onClick != nullptr && ! event.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains (event.getPosition()))
            onClick();
    }

    // The chip's text as drawn (the UI test reads it).
    const juce::String& getChipText() const { return chipText; }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();

        if (chipText.isNotEmpty())
        {
            const auto font = IlanaTheme::font (IlanaTheme::TextSize::tiny, true);
            const auto width = juce::jmin (area.getWidth(), (float) juce::GlyphArrangement::getStringWidthInt (juce::Font (font), chipText) + 28.0f);
            const auto chip = juce::Rectangle<float> (width, 20.0f).withCentre ({ area.getX() + width * 0.5f, area.getCentreY() });
            g.setColour (chipColour.withAlpha (0.2f));
            g.fillRoundedRectangle (chip, 10.0f);
            g.setColour (chipColour.withAlpha (0.75f));
            g.drawRoundedRectangle (chip.reduced (0.5f), 10.0f, 1.0f);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ chip.getX() + 11.0f, chip.getCentreY() }));
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (font);
            IlanaTheme::drawFitted (g, chipText, chip.withTrimmedLeft (20.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, 1);
            return;
        }

        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny));
        IlanaTheme::drawFitted (g, "OUT", area.withWidth (30.0f), juce::Justification::centredLeft, 1);
        const auto track = juce::Rectangle<float> (area.getX() + 34.0f, area.getCentreY() - 3.0f, juce::jmax (0.0f, area.getWidth() - 40.0f), 6.0f);
        g.setColour (IlanaTheme::Ui::track);
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * level), 3.0f);
    }

private:
    IlanaSynthAudioProcessor& processorRef;
    int osc;
    juce::Colour colour, chipColour;
    juce::String chipText;
    bool link = false;
    float level = -1.0f;
};

class OscPage : public juce::Component,
                private juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater,
                private juce::Timer
{
    struct PhysicalControls
    {
        PhysicalControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix)
            : stiffness (state, prefix + "_string_stiffness", "STIFF"),
              pickup (state, prefix + "_string_pickup", "PICKUP"),
              excitePos (state, prefix + "_string_excite_pos", "EXCITE POS"),
              hardness (state, prefix + "_string_pick_hardness", "HARDNESS"),
              pickPos (state, prefix + "_string_pick_pos", "PICK POS"),
              slap (state, prefix + "_string_slap", "SLAP"),
              bowPressure (state, prefix + "_bow_pressure", "BOW PRESS"),
              bowSpeed (state, prefix + "_bow_speed", "BOW SPEED"),
              bridgeBuzz (state, prefix + "_bridge_buzz", "BRIDGE BUZZ"),
              fretRattle (state, prefix + "_fret_rattle", "FRET RATTLE"),
              hammer (state, prefix + "_hammer_hard", "HAMMER"),
              couple (state, prefix + "_couple", "COUPLING"), // the section says STRING (I8-25)
              damper (state, prefix + "_damper", "DAMPER"),
              registerMap (state, prefix + "_register", "REGISTER"),
              epDistance (state, prefix + "_ep_distance", "DISTANCE"),
              epPosition (state, prefix + "_ep_position", "OFFSET"),
              fbGain (state, prefix + "_fb_gain", "AMP GAIN"),
              fbDistance (state, prefix + "_fb_distance", "DISTANCE") {}

        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &stiffness, &pickup, &excitePos, &hardness, &pickPos, &bowPressure, &bowSpeed,
                                &bridgeBuzz, &fretRattle, &hammer, &couple, &damper, &registerMap,
                                &epDistance, &epPosition, &fbGain, &fbDistance })
                knob->setIdentityColour (colour);
        }

        KnobControl stiffness, pickup, excitePos, hardness, pickPos;
        KnobControl bowPressure, bowSpeed, bridgeBuzz, fretRattle;
        KnobControl hammer, couple, damper, registerMap;
        KnobControl epDistance, epPosition; // M7.3 Tine / Reed pickup
        KnobControl fbGain, fbDistance;     // M8.5 feedback amp
        ToggleControl slap;
    };

    struct OscControls
    {
        OscControls (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, int index)
            : on (state, prefix + "_on", "ON"),
              mode (state, prefix + "_mode", ""),
              table (state, prefix + "_table", "TABLE"),
              excite (state, prefix + "_excite", "EXCITE"),
              frame (state, prefix + "_frame", "FRAME"),
              level (state, prefix + "_level", "LEVEL"),
              pan (state, prefix + "_pan", "PAN"),
              semi (state, prefix + "_semi", "SEMI"),
              fine (state, prefix + "_fine", "FINE"),
              unison (state, prefix + "_unison", "UNISON"),
              detune (state, prefix + "_detune", "DETUNE"),
              spread (state, prefix + "_spread", "SPREAD"),
              stringDecay (state, prefix + "_string_decay", "DECAY"),
              stringDamp (state, prefix + "_string_damp", "DAMP"),
              stringSustain (state, prefix + "_string_sustain", "SUSTAIN"),
              sampleTuned (state, prefix + "_sample_tuned", "TUNED"),
              sampleLoop (state, prefix + "_sample_loop", "LOOP"),
              sampleReverse (state, prefix + "_sample_reverse", "REVERSE"),
              sampleStart (state, prefix + "_sample_start", "START"),
              sampleEnd (state, prefix + "_sample_end", "END"),
              sampleFadeIn (state, prefix + "_sample_fade_in", "FADE IN"),
              sampleFadeOut (state, prefix + "_sample_fade_out", "FADE OUT"),
              chord (state, prefix + "_chord", "CHORD"),
              ampEnv (state, prefix + "_amp_env", "ENVELOPE"),
              warp (state, prefix + "_warp", "WARP"),
              uniMode (state, prefix + "_uni_mode", "UNI MODE"),
              warpAmt (state, prefix + "_warp_amt", "WARP AMT"),
              uniBlend (state, prefix + "_uni_blend", "BLEND"),
              uniFrame (state, prefix + "_uni_frame", "FRM SPR"),
              uniWarp (state, prefix + "_uni_warp", "WRP SPR"),
              spectral (state, prefix + "_spectral", "SPECTRAL"),
              scale (state, prefix + "_scale", "SCALE"),
              scaleRoot (state, prefix + "_scale_root", "ROOT"),
              spectralAmt (state, prefix + "_spectral_amt", "SPEC AMT"),
              grainPosition (state, prefix + "_sample_start", "POSITION"),
              grainSize (state, prefix + "_grain_size", "SIZE"),
              grainDensity (state, prefix + "_grain_density", "DENSITY"),
              grainSpray (state, prefix + "_grain_spray", "SPRAY"),
              grainPitch (state, prefix + "_grain_pitch", "PITCH RND"),
              grainSpread (state, prefix + "_grain_spread", "STEREO"),
              grainLive (state, prefix + "_grain_live", "LIVE"),
              warp2 (state, prefix + "_warp2", "WARP 2"),
              pdEnv (state, prefix + "_pd_env", "WARP ENV"),
              warp2Amt (state, prefix + "_warp2_amt", "WARP 2 AMT"),
              pdEnvAmt (state, prefix + "_pd_env_amt", "ENV AMT"),
              tune (state, prefix + "_tune", "TUNING"),
              ratio (state, prefix + "_ratio", "RATIO"),
              fixedHz (state, prefix + "_fixed_hz", "FIXED"),
              egOut (state, prefix + "_eg_out", "OUTPUT"), // one level name (UI review 9, I9-7)
              trim (state, prefix + "_level", "VOICE LEVEL"),
              feedback (state, FmDiagram::routeId (index, index), "FEEDBACK"),
              feedbackType (state, prefix + "_fb_type", "FB TYPE") {}

        // Every knob in the oscillator's own colour, as on PLAY and PHYSICAL.
        void setColour (juce::Colour colour)
        {
            for (auto* knob : { &egOut, &trim, &feedback, &ratio, &fixedHz, &warp2Amt, &pdEnvAmt, &frame, &level, &pan, &semi, &fine, &unison, &detune, &spread,
                                &stringDecay, &stringDamp, &stringSustain, &sampleStart, &sampleEnd, &sampleFadeIn,
                                &sampleFadeOut, &warpAmt, &uniBlend, &uniFrame, &uniWarp, &spectralAmt, &grainPosition, &grainSize,
                                &grainDensity, &grainSpray, &grainPitch, &grainSpread })
                knob->setIdentityColour (colour);
        }

        ToggleControl on, sampleTuned, sampleLoop, sampleReverse;
        ComboControl mode, table, excite, chord, ampEnv, warp, uniMode, spectral, scale, scaleRoot;
        // M6: the PD chain's second stage and the warp (DCW) envelope.
        ComboControl warp2, pdEnv;
        KnobControl warp2Amt, pdEnvAmt;
        KnobControl frame, level, pan, semi, fine, unison, detune, spread;
        KnobControl stringDecay, stringDamp, stringSustain;
        KnobControl sampleStart, sampleEnd, sampleFadeIn, sampleFadeOut;
        KnobControl warpAmt, uniBlend, uniFrame, uniWarp, spectralAmt;
        KnobControl grainPosition, grainSize, grainDensity, grainSpray, grainPitch, grainSpread;
        ToggleControl grainLive; // M7.5: grains from the live input (ilanaSynth FX)
        // An FM operator's tuning, as on the FM page (UI review 6, I6-5).
        ComboControl tune;
        KnobControl ratio, fixedHz;
        // An operator on the Operator Env: its OUTPUT (dB) and the
        // oscillator's LEVEL, as the FM card names them, and its
        // feedback (UI review 7, I7-2, I7-20).
        KnobControl egOut, trim, feedback;
        ComboControl feedbackType;
    };

public:
    std::function<void()> onModeChanged;

    explicit OscPage (IlanaSynthAudioProcessor& p)
        : processorRef (p),
          subShape (p.apvts, "sub_shape", "WAVE"),
          subOctave (p.apvts, "sub_octave", "OCT")
          , symOn (p.apvts, "sym_on", "ON"), symManual (p.apvts, "sym_manual", "MANUAL")
          , symAmount (p.apvts, "sym_amount", "AMT"), symDecay (p.apvts, "sym_decay", "DECAY")
          , symCount (p.apvts, "sym_count", "COUNT")
          , sbOn (p.apvts, "sb_on", "SOUNDBOARD"), sbModel (p.apvts, "sb_model", "MODEL"), sbMix (p.apvts, "sb_mix", "MIX"), sbTone (p.apvts, "sb_tone", "TONE")
          , sbSize (p.apvts, "sb_size", "SIZE"), stretch (p.apvts, "stretch", "STRETCH")
          , pedalRes (p.apvts, "pedal_res", "PEDAL RES"), mechKey (p.apvts, "mech_key", "KEY NOISE")
          , mechDamper (p.apvts, "mech_damper", "DAMPER NOISE"), mechPedal (p.apvts, "mech_pedal", "PEDAL NOISE")
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            controls[(size_t) i] = std::make_unique<OscControls> (p.apvts, prefix, i);
            controls[(size_t) i]->setColour (oscColour (i));
            groupExciteMenu (controls[(size_t) i]->excite);
            waveDisplays[(size_t) i] = std::make_unique<WaveDisplay> (
                p, prefix + "_table", prefix + "_frame", prefix + "_unison",
                prefix + "_spread", prefix + "_detune", false, juce::String {},
                prefix + "_mode", i, oscColour (i), false);
            // The card's well opens on the cycle, as the design draws it; its
            // corner chip still steps to every frame at once (3D) and the
            // harmonics (UI review 5, V6; design round 2).
            waveDisplays[(size_t) i]->setViewMode (0);
            loadButtons[(size_t) i] = std::make_unique<juce::TextButton> ("LOAD...");
            editButtons[(size_t) i] = std::make_unique<juce::TextButton> ("EDIT");
            styleJumpLink (*editButtons[(size_t) i], "TABLE");
            // RESAMPLE: one name for the resampler, apart from the Bounce
            // LFO shapes (UI review 7, I7-25).
            bounceButtons[(size_t) i] = std::make_unique<juce::TextButton> ("RESAMPLE");
            aux[(size_t) i] = std::make_unique<CardAux> (p, i);
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            physical[(size_t) i] = std::make_unique<PhysicalControls> (p.apvts, prefix);
            physical[(size_t) i]->setColour (oscColour (i));
            auto& physicalControls = *physical[(size_t) i];
            addChildComponents (physicalControls.stiffness, physicalControls.pickup, physicalControls.excitePos,
                                physicalControls.hardness, physicalControls.pickPos, physicalControls.slap,
                                physicalControls.bowPressure, physicalControls.bowSpeed,
                                physicalControls.bridgeBuzz, physicalControls.fretRattle,
                                physicalControls.hammer, physicalControls.couple,
                                physicalControls.damper, physicalControls.registerMap,
                                physicalControls.epDistance, physicalControls.epPosition,
                                physicalControls.fbGain, physicalControls.fbDistance);

            // The card's controls for each entry of the shared physical list.
            auto& osc = *controls[(size_t) i];
            auto& lookup = physicalLookup[(size_t) i];
            lookup["_string_decay"] = &osc.stringDecay;
            lookup["_string_damp"] = &osc.stringDamp;
            lookup["_string_sustain"] = &osc.stringSustain;
            lookup["_excite"] = &osc.excite;
            lookup["_string_stiffness"] = &physicalControls.stiffness;
            lookup["_register"] = &physicalControls.registerMap;
            lookup["_damper"] = &physicalControls.damper;
            lookup["_couple"] = &physicalControls.couple;
            lookup["_string_slap"] = &physicalControls.slap;
            lookup["_string_excite_pos"] = &physicalControls.excitePos;
            lookup["_string_pick_hardness"] = &physicalControls.hardness;
            lookup["_string_pick_pos"] = &physicalControls.pickPos;
            lookup["_hammer_hard"] = &physicalControls.hammer;
            lookup["_bow_pressure"] = &physicalControls.bowPressure;
            lookup["_bow_speed"] = &physicalControls.bowSpeed;
            lookup["_fb_gain"] = &physicalControls.fbGain;
            lookup["_fb_distance"] = &physicalControls.fbDistance;
            lookup["_string_pickup"] = &physicalControls.pickup;
            lookup["_bridge_buzz"] = &physicalControls.bridgeBuzz;
            lookup["_fret_rattle"] = &physicalControls.fretRattle;
            lookup["_ep_distance"] = &physicalControls.epDistance;
            lookup["_ep_position"] = &physicalControls.epPosition;
        }

        for (int i = 0; i < 6; ++i)
            symNotes[(size_t) i] = std::make_unique<KnobControl> (p.apvts, "sym_note" + juce::String (i + 1),
                                                                    "NOTE " + juce::String (i + 1));

        for (auto& item : controls)
        {
            auto& osc = *item;
            addChildComponents (osc.grainPosition, osc.grainSize, osc.grainDensity,
                                osc.grainSpray, osc.grainPitch, osc.grainSpread, osc.grainLive,
                                osc.spectral, osc.spectralAmt, osc.warp, osc.uniMode, osc.scale, osc.scaleRoot,
                                osc.warpAmt, osc.uniBlend, osc.uniFrame, osc.uniWarp, osc.warp2, osc.pdEnv, osc.warp2Amt, osc.pdEnvAmt);
            addChildComponents (osc.on, osc.mode, osc.table, osc.excite,
                                osc.frame, osc.level, osc.pan, osc.semi, osc.fine,
                                osc.unison, osc.detune, osc.spread, osc.stringDecay,
                                osc.stringDamp, osc.stringSustain, osc.sampleTuned,
                                osc.sampleLoop, osc.sampleReverse, osc.sampleStart,
                                osc.sampleEnd, osc.sampleFadeIn, osc.sampleFadeOut,
                                osc.chord, osc.ampEnv, osc.tune, osc.ratio, osc.fixedHz,
                                osc.egOut, osc.trim, osc.feedback, osc.feedbackType);
        }

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            auto& osc = *controls[(size_t) i];
            auto& card = *aux[(size_t) i];

            // The card's grid: knobs with their name and value to the right
            // of the dial, menus 18 px high under a small name.
            for (auto* knob : cardKnobs (osc))
                knob->setInlineKnob (true);
            for (auto* menu : { &osc.warp, &osc.warp2, &osc.spectral, &osc.pdEnv, &osc.tune, &osc.ampEnv, &osc.uniMode, &osc.scale, &osc.scaleRoot,
                                &osc.chord, &osc.excite, &osc.table, &osc.feedbackType, &osc.mode })
                menu->setCompactLayout (true);
            for (auto* toggle : { &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.grainLive })
                toggle->setInlineLabel (true);
            osc.on.setBareSwitch (true);
            osc.on.setSwitchColour (oscColour (i));
            // The type menu picks through the processor (one undo step; an
            // operator's tuning goes when it leaves FM / DX7).
            OscRole::bindModeMenu (osc.mode, processorRef, i);

            // The short names the grid has room for (the tooltips keep the full ones).
            osc.warpAmt.setLabelText ("AMT");
            osc.warp2Amt.setLabelText ("AMT");
            osc.spectralAmt.setLabelText ("AMT");
            osc.pdEnvAmt.setLabelText ("AMT");
            osc.unison.setLabelText ("VOICES");
            osc.uniMode.setLabelText ("MODE");

            // An amount whose stage is Off keeps its place and shows a faded dash.
            osc.warpAmt.setDashWhen ([this, prefix] { return readChoice (prefix + "_warp") == 0; });
            osc.warp2Amt.setDashWhen ([this, prefix] { return readChoice (prefix + "_warp2") == 0; });
            osc.pdEnvAmt.setDashWhen ([this, prefix] { return readChoice (prefix + "_pd_env") == 0; });
            osc.spectralAmt.setDashWhen ([this, prefix] { return readChoice (prefix + "_spectral") == 0; });

            addChildComponent (card.stringView);
            card.stringView.setColour (oscColour (i));
            card.stringView.setCompact (true);
            addChildComponent (card.opEnvGraph);
            card.opEnvGraph.setReadOnly (true);
            card.opEnvGraph.onOpen = [this, i]
            {
                if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                    editor->showOperatorEnvelope (i);
            };
            card.spreadView.setColour (oscColour (i));
            card.partials.setColour (oscColour (i));
            card.partials.onClick = [this, i] { showPhysicalString (*this, i); };
            addChildComponent (card.partials);
            card.status.setColour (oscColour (i));
            card.status.onClick = [i]
            {
                if (FmOperatorInfo::hooks().openOperator != nullptr)
                    FmOperatorInfo::hooks().openOperator (i);
            };
            addChildComponent (card.spreadView);
            addChildComponent (card.status);

            addChildComponent (waveDisplay (i));
            scrubbers[(size_t) i].setColour (oscColour (i));
            scrubberAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                p.apvts, prefix + "_frame", scrubbers[(size_t) i]));
            addChildComponent (scrubbers[(size_t) i]);
            setupLoadButton (loadButton (i), prefix + "_table", 0);
            addChildComponent (loadButton (i));
            auto& edit = *editButtons[(size_t) i];
            edit.setTooltip ("Edit this wavetable: draw frames, set harmonics, formulas and morphs.\n"
                             "A factory table is copied into one of the patch's 16 tables first.");
            edit.onClick = [this, i] { openTableEditor (i); };
            addChildComponent (edit);
            auto& bounce = *bounceButtons[(size_t) i];
            bounce.setTooltip ("Resample: play the whole patch (one note, optionally with its effects) and put the "
                               "result on this oscillator, as a tuned sample or cut into a wavetable. "
                               "The result is saved inside the patch.");
            bounce.onClick = [this, i] { showBounceMenu (i); };
            addChildComponent (bounce);

            // The header's actions read as buttons, not as tabs (UI review 6,
            // V40): a raised box at the controls' height.
            for (auto* button : { &loadButton (i), &edit, &bounce })
                styleHeaderButton (*button);

            // The Operator Env has one editor, on FM's operator card; here a
            // picture of it and a link there (UI review 8, S8-4).
            styleJumpLink (card.opEnvButton, "OP ENV");
            card.opEnvButton.setTooltip ("This operator plays its OP ENV: edit it on the FM page");
            card.opEnvButton.onClick = [this, i]
            {
                if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                    editor->showOperatorEnvelope (i);
            };
            addChildComponent (card.opEnvButton);

            // A Physical oscillator's string has one editor, the PHYSICAL page;
            // here its picture, its main knobs and a link there (UI review 9,
            // I9-3).
            styleJumpLink (card.stringButton, "STRING");
            card.stringButton.setTooltip ("Every control of this oscillator's string, exciter and body is on the PHYSICAL page");
            card.stringButton.onClick = [this, i] { showPhysicalString (*this, i); };
            addChildComponent (card.stringButton);

            card.sampleLoadButton.setButtonText ("LOAD...");
            card.sampleLoadButton.setTooltip ("Load a sample or an SF2 / SFZ multisample, or pick a factory sample.  You can also drop a .wav or .sfz file on the picture");
            card.sampleLoadButton.onClick = [this, i] { waveDisplay (i).showSampleMenu (aux[(size_t) i]->sampleLoadButton); };
            styleHeaderButton (card.sampleLoadButton);
            addChildComponent (card.sampleLoadButton);
        }

        // The TABLE lists open the wavetable browser.
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            auto* control = &controls[(size_t) i]->table;
            const auto id = juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table";
            const auto colour = oscColour (i);
            control->setPopupOverride ([this, i, control, id, colour]
            {
                TableBrowser::show (processorRef, id, colour, control->getComboBox(), [this, i] { loadButton (i).triggerClick(); });
            });
        }

        // The strip under the oscillators: voice, stereo, noise, sub, strings
        // and soundboard in one card, no tabs.
        voiceSpread = std::make_unique<KnobControl> (p.apvts, "voice_spread", "SPREAD");
        unisonRandom = std::make_unique<KnobControl> (p.apvts, "unison_random", "PHASE");
        // The oscillators' analogue drift (the vector pad's drift is WANDER:
        // UI review 6, I6-25).
        drift = std::make_unique<KnobControl> (p.apvts, "drift", "DRIFT");
        // VOICE: how the notes are shared out (review 9, S9-1): the header's
        // VOICES button opens the same settings.
        voiceMode = std::make_unique<ComboControl> (p.apvts, "voice_mode", "MODE");
        voiceCount = std::make_unique<KnobControl> (p.apvts, "poly_voices", "VOICES", IlanaTheme::Ui::text2, false);
        bendRange = std::make_unique<KnobControl> (p.apvts, "bend_range", "BEND", IlanaTheme::Ui::text2, false);
        glideTime = std::make_unique<KnobControl> (p.apvts, "glide", "GLIDE", IlanaTheme::Ui::text2, false);
        glideLegato = std::make_unique<ToggleControl> (p.apvts, "glide_legato", "LEGATO");
        // The switch is the sub's alone, and says so; the noise has its
        // own level and colour beside it (V8-14, V8-15).
        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "SUB");
        subOscLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "LEVEL", IlanaTheme::accent(), true);
        noiseStrip = std::make_unique<KnobControl> (p.apvts, "noise_level", "LEVEL", IlanaTheme::Ui::text2, false);
        noiseColourStrip = std::make_unique<KnobControl> (p.apvts, "noise_color", "COLOUR", IlanaTheme::Ui::text2, false);
        addChildComponents (subShape, subOctave, *subOscLevel, *noiseStrip, *noiseColourStrip, *subOscOn, *voiceSpread, *unisonRandom, *drift,
                            *voiceMode, *voiceCount, *bendRange, *glideTime, *glideLegato,
                            symOn, symAmount, symDecay, symCount, symManual,
                            sbOn, sbModel, sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal);
        for (auto& note : symNotes)
            addChildComponent (*note);
        sbOn.showAsSwitch();

        for (auto* knob : stripKnobs())
            knob->setInlineKnob (true);
        for (auto* menu : { &subShape, &subOctave, &sbModel, voiceMode.get() })
            menu->setCompactLayout (true);
        symManual.setInlineLabel (true);
        glideLegato->setSmallName (true);
        subOscOn->setGroupName ("SUB", IlanaTheme::accent());
        symOn.setGroupName ("STRINGS", oscColour (4));
        sbOn.setGroupName ("SOUNDBOARD", oscColour (4));
        subOscOn->getButton().setTooltip ("Switch the sub oscillator on or off");
        symOn.getButton().setTooltip ("Sympathetic strings: shared drone strings that ring with everything you play");
        sbOn.getButton().setTooltip ("The acoustic keys' body: soundboard, stretch tuning, sustain pedal (CC64) resonance and the action's noises");

        moreButton.setTooltip ("Manual tuning of the sympathetic strings, and the keys' stretch tuning, pedal resonance and mechanical noises");
        moreButton.onClick = [this]
        {
            stripOpen = ! stripOpen;
            updateModeVisibility();
        };
        styleHeaderButton (moreButton);
        addChildComponent (moreButton);

        // (Not drawn: the tests pick an oscillator by this list, as they did by the old tabs.)
        oscTabs.setName ("OSC tabs");
        oscTabs.onPick = [this] (int osc) { selectOscillator (osc); };
        addChildComponent (oscTabs);

        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : listenedSuffixes)
                processorRef.apvts.addParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : listenedIds)
            processorRef.apvts.addParameterListener (id, this);

        // Controls that do nothing in the oscillator's current settings dim
        // (one rule for every page, EffectRules).
        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) i]);
            auto& osc = *controls[(size_t) i];
            effectRules.add (osc.spectralAmt, effectRules.choiceIsNot (prefix + "_spectral", 0), "SPECTRAL is Off");
            effectRules.add (osc.warpAmt, effectRules.choiceIsNot (prefix + "_warp", 0), "WARP is Off");
            effectRules.add (osc.scaleRoot, effectRules.choiceIsNot (prefix + "_scale", 0), "SCALE is Off");

            effectRules.add (osc.detune, effectRules.isAbove (prefix + "_unison", 1.5f), "UNISON is 1");
            // Grains have no unison blend or spread: the knobs keep their
            // columns, dimmed, with a dash for the value.
            for (auto* knob : { &osc.uniBlend, &osc.uniFrame, &osc.uniWarp, &osc.spread })
            {
                effectRules.add (*knob, [this, i, voices = effectRules.isAbove (prefix + "_unison", 1.5f)] { return voices() && getMode (i) != 3; },
                                 "UNISON is 1 (a Granular oscillator has no unison blend or spread)");
                knob->setDashWhen ([this, i] { return getMode (i) == 3; });
            }

            // The sample's own controls dim until there is a sample (S14-3).
            for (juce::Component* control : { (juce::Component*) &osc.sampleLoop, (juce::Component*) &osc.sampleReverse, (juce::Component*) &osc.sampleStart,
                                              (juce::Component*) &osc.sampleEnd, (juce::Component*) &osc.sampleFadeIn, (juce::Component*) &osc.sampleFadeOut })
                effectRules.add (*control, [this, i]
                                 {
                                     const auto* sample = processorRef.getSampleForOsc (i);
                                     return sample != nullptr && sample->getNumSamples() >= 2;
                                 },
                                 "load a sample first");
        }

        // One way to add an oscillator here: the dashed row under the last card
        // (UI review 6, S33).
        addButton.setTooltip ("Add the next oscillator, switched on");
        addButton.onClick = [this] { addOscillatorOfType (-1); };
        addChildComponent (addButton);
        // FM / DX7 has its own way in (an operator: a sine tuned by ratio on
        // the Operator EG), beside the plain one.
        addFmButton.setTooltip ("Add the next oscillator as an FM / DX7 operator: a sine tuned by ratio, on its OP ENV");
        addFmButton.onClick = [this] { addOscillatorOfType (OscMode::fmOperator); };
        addChildComponent (addFmButton);

        lastRevealVersion = processorRef.getRevealVersion();
        updateModeVisibility();
        updateEnabled();
        startTimerHz (5);
    }

    ~OscPage() override
    {
        for (const auto* prefix : OscillatorIds::prefixes)
            for (const auto* suffix : listenedSuffixes)
                processorRef.apvts.removeParameterListener (juce::String (prefix) + suffix, this);

        for (const auto* id : listenedIds)
            processorRef.apvts.removeParameterListener (id, this);
    }

    void parameterChanged (const juce::String&, float) override
    {
        // Parameter changes can arrive on the audio thread (host automation),
        // so defer the GUI work to the message thread.
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        updateModeVisibility();
        updateEnabled();
    }

    // Patch loads change which oscillators are shown; FM routes (polled)
    // change what the cards say.
    void timerCallback() override
    {
        if (bouncingOsc >= 0)
            updateBounce();

        if (const auto version = processorRef.getRevealVersion(); version != lastRevealVersion)
        {
            lastRevealVersion = version;
            updateModeVisibility();
            updateEnabled();
        }
        else
        {
            updateTabItems();

            if (layoutKey() != shownKey)
                updateModeVisibility();
            else
                updateCardPictures();
        }

        effectRules.apply();
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        for (const auto& card : cards)
            paintOscCard (g, card);

        paintStrip (g);
    }

    // A right-click on a card's header: switch it off or remove it (the old
    // tab's menu).
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
            return;

        for (const auto& card : cards)
            if (card.header.contains (event.getPosition()))
            {
                showOscMenu (card.osc);
                return;
            }
    }

    static juce::Colour oscColour (int index) { return IlanaTheme::oscColour (index); }

    // The height the page needs: every shown card at its smallest, the add
    // row and the strip.
    int getMinimumHeight() const
    {
        return contentHeight (minimumHeights());
    }

    void resized() override
    {
        layoutPage();
    }

    // The oscillator the UI test or the FM page's links pick: its card is
    // scrolled into view.
    void selectOscillator (int osc)
    {
        if (osc < 0 || osc >= OscillatorIds::count || ! processorRef.isOscillatorShown (osc))
            return;

        selected = osc;
        updateModeVisibility();
        updateEnabled();
        scrollTo (osc);
    }

    int getSelectedOscillator() const { return selected; }

    // Adds the next oscillator (one undo step): as it was (-1: a Wavetable
    // unless it had a type), or as that type. The add row and the UI tests.
    void addOscillatorOfType (int mode)
    {
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (! processorRef.isOscillatorShown (i))
            {
                processorRef.performEdit ((mode == OscMode::fmOperator ? "Add FM / DX7 OSC " : "Add OSC ") + juce::String (i + 1), [this, i, mode]
                {
                    if (mode >= 0)
                        processorRef.addOscillator (i, mode);
                    else
                        processorRef.addOscillator (i);
                });
                selected = i;
                break;
            }

        updateModeVisibility();
        updateEnabled();
    }

    // The strip under the cards. VOICE and the keys' extras (stretch, pedal
    // resonance, noises) are both in it; the second sits under MORE, which
    // this opens.
    enum SharedTab { sharedVoice = 0, sharedSubNoise, sharedSympathetic, sharedKeys };

    void selectShared (int index)
    {
        if (index == sharedKeys || index == sharedSympathetic)
            stripOpen = true;

        updateModeVisibility();
        scrollTo (-1);
    }

    // The UI test reads the menu's items.
    juce::StringArray getOscMenuItems (int band) const
    {
        return { isOff (band) ? "Switch on" : "Switch off", "Remove oscillator" };
    }

    // The card's rectangle in the page (the UI tests).
    juce::Rectangle<int> getCardBounds (int osc) const
    {
        for (const auto& card : cards)
            if (card.osc == osc)
                return card.bounds;

        return {};
    }

private:
    static constexpr const char* listenedSuffixes[] { "_mode", "_on", "_excite", "_warp", "_warp2", "_pd_env", "_spectral", "_tune", "_amp_env" };
    static constexpr const char* listenedIds[] { "sym_on", "sym_manual", "sym_count", "sb_on", "subosc_on", "noise_level", "voice_mode" };

    template <typename... Components>
    void addChildComponents (Components&... components)
    {
        (addChildComponent (components), ...);
    }

    static void styleHeaderButton (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, IlanaTheme::Ui::raised);
        button.setColour (juce::TextButton::textColourOffId, IlanaTheme::Ui::text);
    }

    // The page's measures, in design pixels. Every card is the same width; a
    // wavetable card is the one compact height; the page scrolls once the
    // cards and the strip no longer fit.
    static constexpr int sideMargin = 14, topMargin = 10, bottomMargin = 10, gap = 8;
    static constexpr int cardHeaderHeight = 30, compactCardMin = 130, compactCardMax = 142, liveCardHeight = 84;
    static constexpr int addRowHeight = 18, stripRowHeight = 36, stripBaseHeight = 92, stripRowsExtra = 80;
    static constexpr int wellWidth = 150, rowLabelWidth = 50, colGap = 6, rowGap = 4;
    static constexpr int sampleRowHeight = 48, sampleLabelWidth = 96, sampleColumns = 7, sampleWaveMin = 190;

    // fm: the FM / DX7 type, the operator's card (its tuning, the Operator EG
    // or another envelope, feedback); wavetable: the table's card.
    enum class Kind { wavetable, fm, physical, sample, grain, live };

    Kind kindOf (int osc) const
    {
        switch (getMode (osc))
        {
            case 1: return Kind::physical;
            case 2: return Kind::sample;
            case 3: return Kind::grain;
            case 4: return Kind::live;
            case OscMode::fmOperator: return Kind::fm;
            default: return Kind::wavetable;
        }
    }

    static bool isCompactKind (Kind kind) { return kind != Kind::sample; }

    // One cell of a card's grid: the control, its row, first column and width in columns.
    struct Slot
    {
        juce::Component* item;
        int row, col, span;
    };

    // The compact cards' three rows on one nine-column grid (the sample card's
    // two or three on seven): what stands in each cell. The same column holds
    // the same kind of control from card to card.
    std::vector<Slot> slotsFor (int index) const
    {
        auto& osc = *controls[(size_t) index];
        const auto kind = kindOf (index);
        const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
        std::vector<Slot> slots;
        const auto add = [&slots] (juce::Component& item, int row, int col, int span = 1) { slots.push_back ({ &item, row, col, span }); };
        const auto unisonRow = [&] (int row, bool blend, bool spreads = false)
        {
            add (osc.uniMode, row, 0);
            add (osc.unison, row, 1);
            add (osc.detune, row, 2);
            if (blend)
            {
                add (osc.uniBlend, row, 3);
                add (osc.spread, row, 4);
                if (spreads)
                {
                    add (osc.uniFrame, row, 6);
                    add (osc.uniWarp, row, 7);
                }
            }
            add (osc.chord, row, 5);
        };

        switch (kind)
        {
            case Kind::wavetable:
            {
                add (osc.frame, 0, 0);
                add (osc.warp, 0, 1);
                add (osc.warpAmt, 0, 2);
                add (osc.warp2, 0, 3);
                add (osc.warp2Amt, 0, 4);
                add (osc.spectral, 0, 5);
                add (osc.spectralAmt, 0, 6);
                add (osc.pdEnv, 0, 7);
                add (osc.pdEnvAmt, 0, 8);
                add (osc.level, 1, 0);
                add (osc.pan, 1, 1);
                add (osc.tune, 1, 2);
                const auto tuning = OscRole::tuning (processorRef, index);
                add (tuning == OscTuning::Ratio ? (juce::Component&) osc.ratio
                                                : tuning == OscTuning::Fixed ? (juce::Component&) osc.fixedHz : (juce::Component&) osc.semi, 1, 3);
                add (osc.fine, 1, 4);
                add (osc.ampEnv, 1, 5);
                add (osc.scale, 1, 6, 2);
                add (osc.scaleRoot, 1, 8);
                unisonRow (2, true, true);
                break;
            }
            case Kind::fm:
            {
                // The operator's own order (RATIO, SEMI, FINE, OUTPUT; review 8, V8-5),
                // its wave and envelope menu under it, then the unison row as every card has it.
                // OUTPUT is the Operator EG's level; on another envelope the
                // oscillator's LEVEL stands there.
                const auto tuning = OscRole::tuning (processorRef, index);
                add (osc.tune, 0, 0);
                if (tuning == OscTuning::Ratio)
                    add (osc.ratio, 0, 1);
                if (tuning == OscTuning::Fixed)
                    add (osc.fixedHz, 0, 1);
                add (osc.semi, 0, 2);
                add (osc.fine, 0, 3);
                add (OscRole::usesOperatorEg (processorRef, index) ? (juce::Component&) osc.egOut : (juce::Component&) osc.level, 0, 4);
                add (osc.pan, 0, 5);
                add (osc.table, 1, 0, 2);
                add (osc.ampEnv, 1, 2, 2);
                add (osc.feedback, 1, 4, 2);
                add (osc.feedbackType, 1, 6, 2);
                // An operator has no unison spread (review 8, I8-15): its UNISON
                // shows only once it is raised, on the third row; so do a
                // table's shapers, only while one is in use (no factory
                // operator uses them; an old patch's still shows what plays).
                {
                    auto col = 0;
                    if (readFloat (prefix + "_unison") > 1.5f)
                        add (osc.unison, 2, col++);
                    const std::pair<ComboControl*, KnobControl*> shapers[] { { &osc.warp, &osc.warpAmt }, { &osc.warp2, &osc.warp2Amt },
                                                                            { &osc.spectral, &osc.spectralAmt }, { &osc.pdEnv, &osc.pdEnvAmt } };
                    for (const auto& [menu, amount] : shapers)
                        if (col <= 4 && readChoice (prefix + juce::String (menu == &osc.warp ? "_warp" : menu == &osc.warp2 ? "_warp2"
                                                                            : menu == &osc.spectral ? "_spectral" : "_pd_env")) != 0)
                        {
                            add (*menu, 2, col++);
                            add (*amount, 2, col++);
                        }
                }
                break;
            }
            case Kind::physical:
                // The string is edited on PHYSICAL only (UI review 9, I9-3): the
                // card keeps its exciter, DECAY and DAMP (PLAY's two).
                add (osc.excite, 0, 0, 2);
                add (osc.stringDecay, 0, 2);
                add (osc.stringDamp, 0, 3);
                add (osc.level, 1, 0);
                add (osc.pan, 1, 1);
                add (osc.semi, 1, 3);
                add (osc.fine, 1, 4);
                add (osc.ampEnv, 1, 5);
                unisonRow (2, true);
                break;
            case Kind::grain:
            {
                int col = 0;
                if (IlanaSynthAudioProcessor::isEffectBuild)
                    add (osc.grainLive, 0, col++);
                add (osc.sampleTuned, 0, col++);
                add (osc.sampleReverse, 0, col++);
                add (osc.grainPosition, 0, col++);
                add (osc.grainSize, 0, col++);
                add (osc.grainDensity, 0, col++);
                add (osc.grainSpray, 0, col++);
                // (PITCH RND is the longest name: it takes two cells when the row has one to spare.)
                const auto wide = col <= 6;
                add (osc.grainPitch, 0, col, wide ? 2 : 1);
                col += wide ? 2 : 1;
                add (osc.grainSpread, 0, col);
                add (osc.level, 1, 0);
                add (osc.pan, 1, 1);
                add (osc.semi, 1, 3);
                add (osc.fine, 1, 4);
                add (osc.ampEnv, 1, 5);
                // BLEND and SPREAD keep their columns, dimmed with a dash:
                // grains have no unison blend or spread (controls dim in place).
                unisonRow (2, true);
                break;
            }
            case Kind::live:
                // The input itself: no pitch, shape or unison.
                add (osc.level, 0, 0);
                add (osc.pan, 0, 1);
                add (osc.ampEnv, 0, 5);
                break;
            case Kind::sample:
                // A sample's wave takes the card's width above its rows, as in a
                // sampler (S14-3, S14-4); the rows sit under it on seven columns.
                add (osc.sampleTuned, 0, 0);
                add (osc.sampleLoop, 0, 1);
                add (osc.sampleReverse, 0, 2);
                add (osc.sampleStart, 0, 3);
                add (osc.sampleEnd, 0, 4);
                add (osc.sampleFadeIn, 0, 5);
                add (osc.sampleFadeOut, 0, 6);
                add (osc.level, 1, 0);
                add (osc.pan, 1, 1);
                add (osc.semi, 1, 2);
                add (osc.fine, 1, 3);
                add (osc.unison, 1, 4);
                add (osc.ampEnv, 1, 5);
                add (osc.chord, 1, 6);
                // More than one voice: the unison's own row (review 11, I11-11).
                if (readFloat (prefix + "_unison") > 1.5f)
                {
                    add (osc.uniMode, 2, 0);
                    add (osc.detune, 2, 1);
                    add (osc.uniBlend, 2, 2);
                    add (osc.spread, 2, 3);
                }
                break;
        }

        return slots;
    }

    bool unisonShown (int osc) const
    {
        return readFloat (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_unison") > 1.5f;
    }

    static juce::String rowName (Kind kind, int row)
    {
        switch (kind)
        {
            case Kind::wavetable: return row == 0 ? "SHAPE" : row == 1 ? "PITCH" : "UNISON";
            case Kind::fm: return row == 0 ? "PITCH" : row == 1 ? "WAVE" : "UNISON";
            // (Its third row is the wavetable's UNISON row, named as on every
            // card; its tooltip says they are copies of the string, I11-9.)
            case Kind::physical: return row == 0 ? "STRING" : row == 1 ? "PITCH" : "UNISON";
            case Kind::grain: return row == 0 ? "GRAINS" : row == 1 ? "PITCH" : "UNISON";
            case Kind::live: return "LEVEL";
            case Kind::sample: return row == 0 ? "SAMPLE" : row == 1 ? "PITCH & LEVEL" : "UNISON";
        }

        return {};
    }

    static int numRows (Kind kind, bool unisonRow)
    {
        return kind == Kind::live ? 1 : kind == Kind::sample ? (unisonRow ? 3 : 2) : 3;
    }

    // One card's measures, laid out by layoutPage and read by paint.
    struct CardGeometry
    {
        int osc = 0;
        Kind kind = Kind::wavetable;
        juce::Rectangle<int> bounds, header, well, wave, group, captionArea;
        int rows = 3, columns = 9, rowHeight = 28, gridX = 0, gridWidth = 0, labelX = 0, labelWidth = rowLabelWidth;
        std::array<int, 3> rowY {};
        std::vector<int> separatorY;
    };

    std::vector<CardGeometry> cards;
    juce::Rectangle<int> stripArea, addArea;

    // What a card needs at least, by what it holds.
    int minimumCardHeight (int osc) const
    {
        switch (kindOf (osc))
        {
            case Kind::live: return liveCardHeight;
            case Kind::sample:
            {
                const auto rows = numRows (Kind::sample, readFloat (juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_unison") > 1.5f);
                return 1 + cardHeaderHeight + 8 + sampleWaveMin + 8 + sampleGroupHeight (rows) + 8 + 1;
            }
            default: return compactCardMin;
        }
    }

    static int sampleGroupHeight (int rows) { return 8 + rows * sampleRowHeight + (rows - 1); }

    std::vector<int> shownList() const
    {
        std::vector<int> shown;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                shown.push_back (osc);
        return shown;
    }

    std::vector<int> minimumHeights() const
    {
        std::vector<int> heights;
        for (const auto osc : shownList())
            heights.push_back (minimumCardHeight (osc));
        return heights;
    }

    bool canAdd() const { return numShown() < OscillatorIds::count; }

    int stripHeight() const { return stripBaseHeight + (stripOpen ? stripRowsExtra : 0); }

    int contentHeight (const std::vector<int>& heights) const
    {
        auto total = topMargin + stripHeight() + bottomMargin;
        for (const auto height : heights)
            total += height + gap;
        if (canAdd())
            total += addRowHeight + gap;
        return total;
    }

    void layoutPage()
    {
        cards.clear();
        const auto shown = shownList();
        auto heights = minimumHeights();
        const auto slack = getHeight() - contentHeight (heights);

        // Spare height goes to a sample's waveform, else to the wavetable
        // cards up to their roomy size; the strip stays at the foot.
        if (slack > 0 && ! heights.empty())
        {
            std::vector<size_t> samples, compacts;
            for (size_t i = 0; i < shown.size(); ++i)
            {
                const auto kind = kindOf (shown[i]);
                if (kind == Kind::sample)
                    samples.push_back (i);
                else if (kind != Kind::live)
                    compacts.push_back (i);
            }

            if (! samples.empty())
                for (const auto i : samples)
                    heights[i] += slack / (int) samples.size();
            else if (! compacts.empty())
                for (const auto i : compacts)
                    heights[i] = juce::jmin (compactCardMax, heights[i] + slack / (int) compacts.size());
        }

        const auto width = getWidth() - sideMargin * 2;
        auto y = topMargin;

        for (size_t i = 0; i < shown.size(); ++i)
        {
            CardGeometry card;
            card.osc = shown[i];
            card.kind = kindOf (shown[i]);
            card.bounds = { sideMargin, y, width, heights[i] };
            layoutCard (card);
            cards.push_back (card);
            y += heights[i] + gap;
        }

        addArea = {};
        addButton.setVisible (canAdd());
        addFmButton.setVisible (canAdd());

        if (canAdd())
        {
            for (int i = 0; i < OscillatorIds::count; ++i)
                if (! processorRef.isOscillatorShown (i))
                {
                    addButton.setLabel ("+  ADD OSC " + juce::String (i + 1));
                    break;
                }

            // The row adds a Wavetable; its right end the same as FM / DX7.
            addArea = { sideMargin, y, width, addRowHeight };
            auto row = addArea;
            addFmButton.setBounds (row.removeFromRight (addFmWidth));
            row.removeFromRight (gap);
            addButton.setBounds (row);
            y += addRowHeight + gap;
        }

        const auto stripY = juce::jmax (y, getHeight() - bottomMargin - stripHeight());
        stripArea = { sideMargin, stripY, width, stripHeight() };
        layoutStrip();
    }

    // A grid cell of a card, in the page.
    juce::Rectangle<int> cellRect (const CardGeometry& card, int row, int col, int span) const
    {
        const auto cellWidth = (float) (card.gridWidth - (card.columns - 1) * colGap) / (float) card.columns;
        const auto left = card.gridX + juce::roundToInt ((float) col * (cellWidth + (float) colGap));
        const auto right = card.gridX + juce::roundToInt ((float) (col + span) * (cellWidth + (float) colGap) - (float) colGap);
        return { left, card.rowY[(size_t) row], right - left, card.rowHeight };
    }

    void placeSlot (const CardGeometry& card, const Slot& slot) const
    {
        const auto cell = cellRect (card, slot.row, slot.col, slot.span);

        if (dynamic_cast<KnobControl*> (slot.item) != nullptr)
            slot.item->setBounds (cell.expanded (0, 3)); // (room for the modulation rings)
        else if (dynamic_cast<ComboControl*> (slot.item) != nullptr)
            slot.item->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 28));
        else
            slot.item->setBounds (cell);
    }

    void layoutCard (CardGeometry& card)
    {
        const auto index = card.osc;
        auto& osc = *controls[(size_t) index];
        auto& pieces = *aux[(size_t) index];
        const auto inner = card.bounds.reduced (1);
        card.header = inner.withHeight (cardHeaderHeight);
        const auto body = inner.withTrimmedTop (cardHeaderHeight);

        // Header: the power switch at the right, the actions and the engine
        // menu before it; the name and the engine's name at the left.
        {
            const auto centre = card.header.getCentreY();
            osc.on.setBounds (juce::Rectangle<int> (40, 22).withCentre ({ card.header.getRight() - 8 - 20, centre }));
            auto right = card.header.withRight (osc.on.getX() - 8);

            for (auto* button : { &loadButton (index), &pieces.sampleLoadButton, &pieces.opEnvButton, &pieces.stringButton,
                                  editButtons[(size_t) index].get(), bounceButtons[(size_t) index].get() })
                if (button != nullptr && button->isVisible())
                {
                    const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)),
                                                                                   button->getButtonText()) + 24;
                    button->setBounds (right.removeFromRight (width).withSizeKeepingCentre (width, 22));
                    right.removeFromRight (6);
                }

            osc.mode.setBounds (right.removeFromRight (100).withSizeKeepingCentre (100, 20));
            right.removeFromRight (6);
            card.captionArea = right.withTrimmedLeft (80);
        }

        if (card.kind == Kind::sample)
        {
            card.columns = sampleColumns;
            card.rows = numRows (Kind::sample, readFloat (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_unison") > 1.5f);
            card.rowHeight = sampleRowHeight;
            card.labelWidth = sampleLabelWidth;
            auto content = body.reduced (10, 8);
            card.group = content.removeFromBottom (sampleGroupHeight (card.rows));
            content.removeFromBottom (8);
            card.wave = content;
            waveDisplay (index).setBounds (card.wave);
            card.labelX = card.group.getX() + 10;
            card.gridX = card.labelX + sampleLabelWidth + 8;
            card.gridWidth = card.group.getRight() - 10 - card.gridX;
            card.separatorY.clear();

            for (int r = 0; r < card.rows; ++r)
            {
                card.rowY[(size_t) r] = card.group.getY() + 4 + r * (sampleRowHeight + 1);
                if (r > 0)
                    card.separatorY.push_back (card.rowY[(size_t) r] - 1);
            }

            for (const auto& slot : slotsFor (index))
                placeSlot (card, slot);

            // The unison row's picture fills what the row leaves.
            if (card.rows == 3)
                pieces.spreadView.setBounds (cellRect (card, 2, 4, 3));
            return;
        }

        card.rows = numRows (card.kind, true);
        card.columns = 9;
        card.rowHeight = juce::jlimit (26, 28, (body.getHeight() - 2 * rowGap - 8) / 3);
        const auto rowsHeight = card.rowHeight * card.rows + rowGap * (card.rows - 1);
        const auto padTop = juce::jmax (4, (body.getHeight() - rowsHeight) / 2);
        card.well = { body.getX() + 10, body.getY() + juce::jmin (5, padTop), wellWidth, body.getHeight() - 2 * juce::jmin (5, padTop) };
        card.labelX = card.well.getRight() + 10;
        card.gridX = card.labelX + rowLabelWidth + 8;
        card.gridWidth = inner.getRight() - 10 - card.gridX;

        for (int r = 0; r < 3; ++r)
            card.rowY[(size_t) r] = body.getY() + padTop + r * (card.rowHeight + rowGap);

        for (const auto& slot : slotsFor (index))
            placeSlot (card, slot);

        // The well: the wavetable (with its slim line), the operator's envelope
        // or the string.
        const auto electric = card.kind == Kind::physical && isElectric (index);
        waveDisplay (index).setBounds (card.well);
        pieces.opEnvGraph.setBounds (card.well);
        pieces.stringView.setBounds (card.well);
        for (auto& item : scrubbers)
            item.setBounds ({});
        juce::ignoreUnused (electric);

        // The right-hand cells: the FM chip or the output bar in PITCH's, the
        // unison picture in UNISON's.
        if (card.kind == Kind::physical)
            pieces.partials.setBounds (cellRect (card, 0, 4, 5).reduced (4, 0));

        if (card.kind != Kind::live)
        {
            pieces.status.setBounds (cellRect (card, 1 - (card.kind == Kind::fm ? 1 : 0), 6, 3));
            pieces.spreadView.setBounds (cellRect (card, 2, 6, 3));
        }
        else
            pieces.status.setBounds (cellRect (card, 0, 6, 3));
    }

    void paintOscCard (juce::Graphics& g, const CardGeometry& card) const
    {
        const auto index = card.osc;
        const auto tint = oscColour (index);
        const auto off = isOff (index);

        if (off)
            g.beginTransparencyLayer (0.55f);

        IlanaTheme::paintCard (g, card.bounds.toFloat(), 10.0f, tint);
        g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (card.bounds.getX() + 1, card.header.getBottom() - 1, card.bounds.getWidth() - 2, 1));

        const auto centre = card.header.getCentreY();
        IlanaTheme::paintTag (g, { (float) card.bounds.getX() + 17.0f, (float) centre }, tint);
        g.setColour (IlanaTheme::Ui::text);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
        g.drawText ("OSC " + juce::String (index + 1), juce::Rectangle<int> (card.bounds.getX() + 30, centre - 9, 56, 18), juce::Justification::centredLeft);

        // The engine's name after it, quietly.
        g.setColour (IlanaTheme::Ui::text3);
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
        IlanaTheme::drawFitted (g, engineName (index), card.captionArea.withHeight (18).withCentre (card.captionArea.getCentre()),
                                juce::Justification::centredLeft, 1);

        // Each row named at its left, in the oscillator's colour.
        if (card.kind == Kind::sample)
            IlanaTheme::paintRecessedPanel (g, card.group.toFloat(), 8.0f);

        for (int r = 0; r < card.rows; ++r)
        {
            g.setColour (tint.withAlpha (0.8f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            // (An operator's third row is there only for UNISON or a shaper in use.)
            auto name = rowName (card.kind, r);
            if (card.kind == Kind::fm && r == 2)
            {
                auto shaper = false;
                for (const auto& slot : slotsFor (index))
                    shaper = shaper || (slot.row == 2 && slot.item != &controls[(size_t) index]->unison);
                if (! shaper && ! unisonShown (index))
                    continue;
                if (shaper)
                    name = unisonShown (index) ? "MORE" : "SHAPE";
            }
            IlanaTheme::drawFitted (g, name,
                                    juce::Rectangle<int> (card.labelX, card.rowY[(size_t) r], card.labelWidth, card.rowHeight), juce::Justification::centredLeft, 1);
        }

        for (const auto y : card.separatorY)
        {
            g.setColour (IlanaTheme::Ui::line.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<int> (card.group.getX() + 8, y, card.group.getWidth() - 16, 1));
        }

        if (off)
            g.endTransparencyLayer();
    }

    // The engine's name in the header: an FM / DX7 oscillator says it is an
    // operator, and on what envelope.
    juce::String engineName (int index) const
    {
        if (getMode (index) == OscMode::fmOperator)
            return OscRole::usesOperatorEg (processorRef, index) ? "FM operator, OP ENV" : "FM operator";
        return OscRole::modeName (getMode (index));
    }

    // The strip: one card, no tabs. VOICE, STEREO and NOISE on its first row, SUB,
    // STRINGS and SOUNDBOARD on its second; MORE opens the strings' manual
    // tuning and the keys' extras under them.
    struct StripEntry
    {
        juce::Component* item;
        int width;
    };

    struct StripGroup
    {
        juce::String name;
        juce::Colour colour;
        ToggleControl* nameSwitch = nullptr; // a group that is its own switch
        std::vector<StripEntry> entries;
        int nameWidth = 0, width = 0;
    };

    std::vector<std::tuple<juce::Rectangle<int>, juce::String, juce::Colour>> stripNames;
    std::vector<juce::Rectangle<int>> stripSeparators;

    static int textWidth (const juce::String& text, float size, bool bold = false)
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (size, bold)), text) + 2;
    }

    // A knob cell: the dial, then its name and value.
    static int knobCellWidth (KnobControl& knob)
    {
        // (The widest of its name and its values: "COUNT" over "6 strings".)
        auto& slider = knob.getSlider();
        auto widest = juce::jmax (textWidth (knob.getLabelText(), IlanaTheme::TextSize::label), 34);
        for (const auto value : { slider.getMinimum(), slider.getValue(), slider.getMaximum() })
            widest = juce::jmax (widest, textWidth (slider.getTextFromValue (value), IlanaTheme::TextSize::body) + 2);
        return 28 + 4 + widest + 1;
    }

    StripGroup makeGroup (const juce::String& name, juce::Colour colour, ToggleControl* nameSwitch,
                          std::initializer_list<StripEntry> entries) const
    {
        StripGroup group;
        group.name = name;
        group.colour = colour;
        group.nameSwitch = nameSwitch;
        group.entries = entries;
        group.nameWidth = nameSwitch != nullptr ? 13 + textWidth (name, IlanaTheme::TextSize::label, true) + 4
                                                : textWidth (name, IlanaTheme::TextSize::label, true) + 2;
        group.width = group.nameWidth;
        for (auto& entry : group.entries)
        {
            if (entry.width <= 0)
                if (auto* knob = dynamic_cast<KnobControl*> (entry.item))
                    entry.width = knobCellWidth (*knob);
            group.width += 5 + entry.width;
        }
        return group;
    }

    void placeStripEntry (const StripEntry& entry, int x, juce::Rectangle<int> row) const
    {
        const auto cell = juce::Rectangle<int> (x, row.getY(), entry.width, row.getHeight());

        if (dynamic_cast<KnobControl*> (entry.item) != nullptr)
            entry.item->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 30));
        else if (dynamic_cast<ComboControl*> (entry.item) != nullptr)
            entry.item->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 28));
        else if (auto* toggle = dynamic_cast<ToggleControl*> (entry.item); toggle != nullptr && toggle->getButton().getButtonText() == "MANUAL")
            entry.item->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 24));
        else
            entry.item->setBounds (cell);
    }

    // Groups across a row, parted by thin rules; `justify` spreads them over the row.
    void flowRow (juce::Rectangle<int> row, std::vector<StripGroup>& groups, bool justify, int reserveRight = 0)
    {
        auto total = 0;
        for (const auto& group : groups)
            total += group.width;

        const auto separators = juce::jmax (0, (int) groups.size() - 1);
        const auto freeWidth = row.getWidth() - reserveRight - total - separators;
        const auto space = justify && separators > 0 ? juce::jlimit (4, 60, freeWidth / (separators * 2)) : 14;
        auto x = row.getX();

        for (size_t gi = 0; gi < groups.size(); ++gi)
        {
            auto& group = groups[gi];
            const auto nameArea = juce::Rectangle<int> (x, row.getY(), group.nameWidth, row.getHeight());

            if (group.nameSwitch != nullptr)
                group.nameSwitch->setBounds (nameArea);
            else
                stripNames.push_back ({ nameArea, group.name, group.colour });

            x += group.nameWidth;

            for (const auto& entry : group.entries)
            {
                x += 5;
                placeStripEntry (entry, x, row);
                x += entry.width;
            }

            if (gi + 1 < groups.size())
            {
                x += space;
                stripSeparators.push_back ({ x, row.getCentreY() - 13, 1, 26 });
                x += 1 + space;
            }
        }
    }

    void layoutStrip()
    {
        stripNames.clear();
        stripSeparators.clear();

        auto inner = stripArea.reduced (12, 6);
        auto rowA = inner.removeFromTop (stripRowHeight);
        inner.removeFromTop (4);
        auto rowB = inner.removeFromTop (stripRowHeight);

        const auto accent = IlanaTheme::accent();
        std::vector<StripGroup> first { makeGroup ("VOICE", accent, nullptr, { { voiceMode.get(), 68 }, { voiceCount.get(), 0 }, { bendRange.get(), 0 },
                                                                             { glideTime.get(), 0 }, { glideLegato.get(), 48 } }),
                                        makeGroup ("STEREO", accent, nullptr, { { voiceSpread.get(), 0 }, { unisonRandom.get(), 0 }, { drift.get(), 0 } }),
                                        makeGroup ("NOISE", accent, nullptr, { { noiseStrip.get(), 0 }, { noiseColourStrip.get(), 0 } }) };
        const auto moreWidth = 54;
        moreButton.setBounds (juce::Rectangle<int> (moreWidth, 22).withCentre ({ rowA.getRight() - moreWidth / 2, rowA.getCentreY() }));
        flowRow (rowA, first, true, moreWidth + 10);

        std::vector<StripGroup> second { makeGroup ("SUB", accent, subOscOn.get(), { { &subShape, 68 }, { subOscLevel.get(), 0 }, { &subOctave, 62 } }),
                                         makeGroup ("STRINGS", oscColour (4), &symOn, { { &symAmount, 0 }, { &symCount, 0 }, { &symDecay, 0 } }),
                                         makeGroup ("SOUNDBOARD", oscColour (4), &sbOn, { { &sbModel, 66 }, { &sbSize, 0 }, { &sbTone, 0 }, { &sbMix, 0 } }) };
        flowRow (rowB, second, true);

        // Under MORE: the strings' manual tuning, then the keys' extras.
        if (stripOpen)
        {
            inner.removeFromTop (4);
            auto rowC = inner.removeFromTop (stripRowHeight);
            inner.removeFromTop (4);
            auto rowD = inner.removeFromTop (stripRowHeight);
            std::vector<StripGroup> third { makeGroup ("STRING TUNING", oscColour (4), nullptr,
                                                       { { &symManual, 96 }, { symNotes[0].get(), 0 }, { symNotes[1].get(), 0 }, { symNotes[2].get(), 0 },
                                                         { symNotes[3].get(), 0 }, { symNotes[4].get(), 0 }, { symNotes[5].get(), 0 } }) };
            flowRow (rowC, third, false);
            std::vector<StripGroup> fourth { makeGroup ("KEYS", accent, nullptr, { { &stretch, 0 }, { &pedalRes, 0 }, { &mechKey, 0 }, { &mechDamper, 0 }, { &mechPedal, 0 } }) };
            flowRow (rowD, fourth, false);
        }
    }

    void paintStrip (juce::Graphics& g) const
    {
        IlanaTheme::paintCard (g, stripArea.toFloat(), 10.0f, IlanaTheme::accent());

        for (const auto& [area, name, colour] : stripNames)
        {
            g.setColour (colour.withAlpha (0.9f));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            IlanaTheme::drawFitted (g, name, area, juce::Justification::centredLeft, 1);
        }

        for (const auto& separator : stripSeparators)
        {
            g.setColour (IlanaTheme::Ui::line);
            g.fillRect (separator);
        }
    }

    void scrollTo (int osc)
    {
        auto* viewport = findParentComponentOfClass<juce::Viewport>();

        if (viewport == nullptr)
            return;

        const auto area = osc >= 0 ? getCardBounds (osc) : stripArea;

        if (area.isEmpty())
            return;

        const auto top = viewport->getViewPositionY();
        const auto height = viewport->getViewHeight();

        if (area.getY() < top)
            viewport->setViewPosition (0, juce::jmax (0, area.getY() - topMargin));
        else if (area.getBottom() + bottomMargin > top + height)
            viewport->setViewPosition (0, area.getBottom() + bottomMargin - height);
    }

    juce::String layoutKey() const
    {
        juce::String key;
        for (const auto osc : shownList())
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
            key << osc << ":" << getMode (osc) << (OscRole::usesOperatorEg (processorRef, osc) ? "o" : "-") << OscRole::tuning (processorRef, osc)
                << (readFloat (prefix + "_unison") > 1.5f ? "u" : "-") << (isElectric (osc) ? "e" : "-") << ";";
        }
        key << (stripOpen ? "open" : "shut") << (readBool ("sym_manual") ? "m" : "-") << (canAdd() ? "+" : "=");
        return key;
    }

    juce::String shownKey;

    // What a card holds besides the controls every oscillator has: the pictures
    // the modes need (each card has its own, so all can be live at once) and
    // their buttons.
    struct CardAux
    {
        CardAux (IlanaSynthAudioProcessor& p, int index)
            : stringView (p, juce::String (OscillatorIds::prefixes[(size_t) index])), opEnvGraph (p),
              spreadView (p, index), partials (p, index), status (p, index) {}

        PhysicalView stringView;
        OperatorEnvDisplay opEnvGraph;
        UnisonSpreadView spreadView;
        StringPartialsView partials;
        OscStatusView status;
        juce::TextButton opEnvButton, stringButton, sampleLoadButton;
    };

    // Every knob a card can show, for the one inline style.
    static std::vector<KnobControl*> cardKnobs (OscControls& osc)
    {
        return { &osc.frame, &osc.level, &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune, &osc.spread,
                 &osc.stringDecay, &osc.stringDamp, &osc.stringSustain, &osc.sampleStart, &osc.sampleEnd,
                 &osc.sampleFadeIn, &osc.sampleFadeOut, &osc.warpAmt, &osc.uniBlend, &osc.uniFrame, &osc.uniWarp, &osc.spectralAmt,
                 &osc.grainPosition, &osc.grainSize, &osc.grainDensity, &osc.grainSpray, &osc.grainPitch,
                 &osc.grainSpread, &osc.warp2Amt, &osc.pdEnvAmt, &osc.ratio, &osc.fixedHz, &osc.egOut,
                 &osc.trim, &osc.feedback };
    }

    std::vector<KnobControl*> stripKnobs()
    {
        std::vector<KnobControl*> knobs { voiceCount.get(), bendRange.get(), glideTime.get(), voiceSpread.get(), unisonRandom.get(),
                                          drift.get(), noiseStrip.get(), noiseColourStrip.get(), subOscLevel.get(), &symAmount,
                                          &symDecay, &symCount, &sbMix, &sbTone, &sbSize, &stretch, &pedalRes, &mechKey,
                                          &mechDamper, &mechPedal };
        for (auto& note : symNotes)
            knobs.push_back (note.get());
        return knobs;
    }

    bool readBool (const juce::String& id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load() > 0.5f;

        return true;
    }

    float readFloat (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    }

    bool isOff (int index) const
    {
        return ! readBool (juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]) + "_on");
    }

    int numShown() const
    {
        auto count = 0;
        for (int i = 0; i < OscillatorIds::count; ++i)
            count += processorRef.isOscillatorShown (i) ? 1 : 0;
        return count;
    }

    // The hidden picker's order: the shown oscillators.
    int oscForTab (int tab) const
    {
        for (int osc = 0, index = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc) && index++ == tab)
                return osc;

        return selected;
    }

    // The list the tests pick an oscillator from (not drawn), each entry's role
    // in its tooltip; also the OUTPUT name of an operator's level.
    void updateTabItems()
    {

        const auto shown = shownList();

        oscTabs.setOscillators (shown, [this] (int osc) { return ! isOff (osc); },
                                [this] (int osc)
                                {
                                    const auto role = OscRole::describe (processorRef, osc);
                                    return "OSC " + juce::String (osc + 1) + ": " + OscRole::modeName (getMode (osc))
                                           + (role.isNotEmpty() ? ", " + OscRole::describeLong (processorRef, osc) : juce::String())
                                           + (isOff (osc) ? ", switched off" : "") + ".  Right-click its header to switch it off or remove it.";
                                },
                                [this] (int osc)
                                {
                                    if (osc != selected || isOff (osc))
                                        return juce::String();
                                    // (The tag is the type: FM / DX7 for an operator.)
                                    return OscRole::modeName (getMode (osc)).toUpperCase();
                                });
        oscTabs.setSelectedOsc (selected);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (const auto name = juce::String (OscRole::outputKnobName (processorRef, osc)); controls[(size_t) osc]->egOut.getLabelText() != name)
                controls[(size_t) osc]->egOut.setLabelText (name);
    }

    // What the PITCH row's right-hand cells say: the FM role in a chip as
    // long as the cells can hold, else shorter, else none (the output bar).
    void updateStatus (int index)
    {
        auto& view = aux[(size_t) index]->status;
        const auto tint = oscColour (index);
        juce::StringArray candidates;
        auto colour = tint;
        const auto sources = OscRole::sources (processorRef, index);
        const auto targets = OscRole::targets (processorRef, index);

        if (! sources.empty())
            colour = oscColour (sources.front());
        else if (! targets.empty())
            colour = oscColour (targets.front());

        if (OscRole::isOperator (processorRef, index))
        {
            const auto role = OscRole::describe (processorRef, index);
            candidates.add ("OPERATOR  " + role);
            candidates.add (role.isNotEmpty() ? role : juce::String ("OPERATOR"));
            candidates.add ("OP");
        }
        else if (const auto role = OscRole::describeLong (processorRef, index); role.isNotEmpty())
        {
            candidates.add (role);
            candidates.add (OscRole::describe (processorRef, index));
            candidates.add ("FM");
        }

        juce::String chip;
        const auto room = juce::jmax (40, view.getWidth() - 28);
        for (const auto& candidate : candidates)
            if (candidate.isNotEmpty() && textWidth (candidate, IlanaTheme::TextSize::tiny, true) <= room)
            {
                chip = candidate;
                break;
            }

        const auto link = chip.isNotEmpty() && FmOperatorInfo::hooks().openOperator != nullptr;
        const auto full = OscRole::describeLong (processorRef, index);
        view.update (chip, colour, link, chip.isEmpty() ? "The oscillator's output level" : full + (link ? "\nClick to open it on the FM page" : ""));
    }

    // The pictures that follow the sound: the unison's spread, the FM chip and
    // the output bar (polled with the timer).
    void updateCardPictures()
    {
        for (const auto index : shownList())
        {
            aux[(size_t) index]->spreadView.update();
            aux[(size_t) index]->partials.update();
            updateStatus (index);
        }

        getProperties().set ("caption", OscRole::describeLong (processorRef, selected)); // for the UI test
    }

    // An oscillator card's right-click menu.
    void showOscMenu (int band)
    {
        const auto on = ! isOff (band);
        juce::PopupMenu menu;
        menu.addSectionHeader ("OSC " + juce::String (band + 1));
        menu.addItem (1, on ? "Switch off" : "Switch on");
        menu.addItem (2, "Remove oscillator", numShown() > 1);
        juce::Component::SafePointer<OscPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                            [safe, band, on] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;

                                auto& p = safe->processorRef;
                                const auto name = "OSC " + juce::String (band + 1);

                                if (result == 2)
                                    p.performEdit ("Remove " + name, [&p, band] { p.removeOscillator (band); });
                                else if (auto* parameter = p.apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) band]) + "_on"))
                                    p.performEdit (name + (on ? " off" : " on"), [parameter, on]
                                    {
                                        parameter->beginChangeGesture();
                                        parameter->setValueNotifyingHost (on ? 0.0f : 1.0f);
                                        parameter->endChangeGesture();
                                    });

                                safe->updateModeVisibility();
                                safe->updateEnabled();
                            });
    }

    WaveDisplay& waveDisplay (int index)
    {
        return *waveDisplays[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    juce::TextButton& loadButton (int index)
    {
        return *loadButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)];
    }

    int getMode (int oscIndex) const
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, oscIndex)]) + "_mode";
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? (int) value->load() : 0;
    }

    void setupLoadButton (juce::TextButton& button, const juce::String& tableId, int tableChoiceOffset)
    {
        button.setTooltip ("Load a wavetable (.wav of single-cycle frames), or turn any recording into a wavetable");
        button.onClick = [this, &button, tableId, tableChoiceOffset]
        {
            if (chooserOpen)
                return;

            juce::PopupMenu menu;
            menu.addItem (1, "Load wavetable file...");
            menu.addItem (2, "Make a wavetable from any audio...");
            menu.addItem (4, "Vocode: harmonics follow the audio's spectrum...");
            menu.addItem (5, "Time slice: equal slices of the audio, one cycle each...");
            menu.addSeparator();
            menu.addItem (3, "(Any audio: the pitch is detected and one cycle per frame is taken across the file)", false);

            juce::Component::SafePointer<OscPage> safeMenu (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                                [safeMenu, tableId, tableChoiceOffset] (int result)
                                {
                                    if (safeMenu != nullptr && (result == 1 || result == 2 || result == 4 || result == 5))
                                        safeMenu->chooseTable (tableId, tableChoiceOffset,
                                                               result == 2 ? Wavetable::LoadMode::Resynthesize
                                                               : result == 4 ? Wavetable::LoadMode::Vocode
                                                               : result == 5 ? Wavetable::LoadMode::TimeSlice
                                                                             : Wavetable::LoadMode::Automatic);
                                });
        };
    }

    void chooseTable (const juce::String& tableId, int tableChoiceOffset, Wavetable::LoadMode mode)
    {
        chooserOpen = true;

        for (int index = 0; index < OscillatorIds::count; ++index)
            loadButton (index).setEnabled (false);

        if (tableChooser == nullptr)
            tableChooser = std::make_unique<juce::FileChooser> (
                "Load Wavetable or Audio",
                juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

        juce::Component::SafePointer<OscPage> safeThis (this);

        tableChooser->launchAsync (juce::FileBrowserComponent::openMode
                                       | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis, tableId, tableChoiceOffset, mode] (const juce::FileChooser& chooser)
                                   {
                                       if (safeThis == nullptr)
                                           return;

                                       safeThis->chooserOpen = false;
                                       safeThis->updateEnabled();

                                       const auto file = chooser.getResult();

                                       if (! file.existsAsFile())
                                           return;

                                       const auto factoryCount = TableFactory::getNumFactoryTables();
                                       const auto domain = juce::jmax (0, safeThis->readTableChoiceIndex (tableId) - tableChoiceOffset);
                                       const auto slot = domain >= factoryCount
                                                             ? juce::jlimit (0, IlanaSynthAudioProcessor::numUserSlots - 1,
                                                                             domain - factoryCount)
                                                             : 0;

                                       if (safeThis->processorRef.loadUserWavetable (slot, file, mode))
                                       {
                                           if (auto* parameter = safeThis->processorRef.apvts.getParameter (tableId))
                                               parameter->setValueNotifyingHost (
                                                   parameter->convertTo0to1 ((float) (tableChoiceOffset + factoryCount + slot)));
                                       }
                                   });
    }

    // Every component of one oscillator's card, to hide before the shown ones
    // are switched on.
    std::vector<juce::Component*> componentsOf (int i)
    {
        auto& osc = *controls[(size_t) i];
        auto& phys = *physical[(size_t) i];
        auto& pieces = *aux[(size_t) i];
        return { &osc.on, &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.mode, &osc.table,
                 &osc.excite, &osc.chord, &osc.ampEnv, &osc.warp, &osc.uniMode, &osc.scale, &osc.scaleRoot, &osc.spectral, &osc.frame,
                 &osc.level, &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune, &osc.spread,
                 &osc.stringDecay, &osc.stringDamp, &osc.stringSustain, &osc.sampleStart, &osc.sampleEnd,
                 &osc.sampleFadeIn, &osc.sampleFadeOut, &osc.warpAmt, &osc.uniBlend, &osc.uniFrame, &osc.uniWarp, &osc.spectralAmt,
                 &osc.grainPosition, &osc.grainSize, &osc.grainDensity, &osc.grainSpray, &osc.grainPitch,
                 &osc.grainSpread, &osc.grainLive, &osc.warp2, &osc.pdEnv, &osc.warp2Amt, &osc.pdEnvAmt,
                 &osc.tune, &osc.ratio, &osc.fixedHz, &osc.egOut, &osc.trim, &osc.feedback, &osc.feedbackType,
                 &phys.stiffness, &phys.pickup, &phys.excitePos, &phys.hardness,
                 &phys.pickPos, &phys.bowPressure, &phys.bowSpeed, &phys.bridgeBuzz, &phys.fretRattle,
                 &phys.hammer, &phys.couple, &phys.damper, &phys.registerMap, &phys.slap,
                 &phys.epDistance, &phys.epPosition, &phys.fbGain, &phys.fbDistance, &waveDisplay (i), &scrubbers[(size_t) i], &loadButton (i), editButtons[(size_t) i].get(),
                 bounceButtons[(size_t) i].get(), &pieces.stringView, &pieces.opEnvGraph, &pieces.spreadView, &pieces.partials, &pieces.status,
                 &pieces.opEnvButton, &pieces.stringButton, &pieces.sampleLoadButton };
    }

    // The cards show every shown oscillator with its own controls; the strip
    // its groups (the extras only while MORE is open).
    void updateModeVisibility()
    {
        if (! processorRef.isOscillatorShown (selected))
            for (int i = 0; i < OscillatorIds::count; ++i)
                if (processorRef.isOscillatorShown (i))
                {
                    selected = i;
                    break;
                }

        for (int i = 0; i < OscillatorIds::count; ++i)
            for (auto* component : componentsOf (i))
                component->setVisible (false);

        for (const auto index : shownList())
        {
            const auto kind = kindOf (index);
            auto& osc = *controls[(size_t) index];
            auto& pieces = *aux[(size_t) index];
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            osc.stringSustain.setLabelText (juce::roundToInt (readFloat (prefix + "_excite")) == 10 ? "FEEDBACK" : "SUSTAIN");

            for (const auto& slot : slotsFor (index))
                slot.item->setVisible (true);

            osc.on.setVisible (true);
            osc.mode.setVisible (true);
            // An FM / DX7 oscillator plays a plain cycle: no table tools
            // (UI review 8, I8-15, S8-9, V8-16); on the Operator EG no
            // resampling either.
            const auto opEnv = kind == Kind::fm && OscRole::usesOperatorEg (processorRef, index);
            // An FM operator reads as one (UI review 9, I9-6): its type says
            // FM / DX7 and its table is its WAVE. Ratio, Fixed Hz and OP ENV
            // are its own: a Wavetable's menus don't offer them.
            osc.table.setLabelText (kind == Kind::fm ? "WAVE" : "TABLE");
            OscRole::showOperatorChoices (&osc.tune.getComboBox(), &osc.ampEnv.getComboBox(), kind == Kind::fm,
                                          getMode (index) != OscMode::wavetable, readChoice (prefix + "_amp_env"));

            editButtons[(size_t) index]->setVisible (kind == Kind::wavetable);
            // LOAD loads what the mode plays: a wavetable (LOAD...), or a
            // sample or SF2 / SFZ multisample (LOAD, UI review 4, V30; review
            // 7, I7-24).
            loadButton (index).setVisible (kind == Kind::wavetable);
            pieces.sampleLoadButton.setVisible (kind == Kind::sample || kind == Kind::grain);
            bounceButtons[(size_t) index]->setVisible (kind != Kind::live && ! opEnv);
            pieces.opEnvButton.setVisible (opEnv);
            pieces.stringButton.setVisible (kind == Kind::physical);

            // The well: the table (slim line), the string, the operator's
            // envelope, the sample, the live meter.
            const auto electric = kind == Kind::physical && isElectric (index);
            auto& wave = waveDisplay (index);
            wave.setVisible (! opEnv && (kind != Kind::physical || electric));
            if (wave.isCompact() != (kind != Kind::sample))
                wave.setCompact (kind != Kind::sample);
            wave.setSlim (kind == Kind::wavetable);
            pieces.stringView.setVisible (kind == Kind::physical && ! electric);
            pieces.opEnvGraph.setVisible (opEnv);
            pieces.opEnvGraph.setSource (prefix, oscColour (index));
            pieces.partials.setVisible (kind == Kind::physical && ! electric);
            pieces.status.setVisible (kind != Kind::sample);
            pieces.spreadView.setVisible (kind != Kind::live && ((kind != Kind::sample && kind != Kind::fm) || readFloat (prefix + "_unison") > 1.5f));
        }

        // The strip.
        for (auto* item : std::initializer_list<juce::Component*> { voiceMode.get(), voiceCount.get(), bendRange.get(), glideTime.get(), glideLegato.get(),
                                                                    voiceSpread.get(), unisonRandom.get(), drift.get(), noiseStrip.get(), noiseColourStrip.get(),
                                                                    subOscOn.get(), &subShape, subOscLevel.get(), &subOctave, &symOn, &symAmount, &symCount,
                                                                    &symDecay, &sbOn, &sbModel, &sbSize, &sbTone, &sbMix, &moreButton })
            item->setVisible (true);

        for (auto* item : std::initializer_list<juce::Component*> { &symManual, &stretch, &pedalRes, &mechKey, &mechDamper, &mechPedal })
            item->setVisible (stripOpen);

        for (auto& note : symNotes)
            note->setVisible (stripOpen && readBool ("sym_manual"));

        moreButton.setButtonText (stripOpen ? juce::String ("LESS ") + juce::String::fromUTF8 ("\xe2\x96\xb4")
                                            : juce::String ("MORE ") + juce::String::fromUTF8 ("\xe2\x96\xbe"));

        updateTabItems();
        shownKey = layoutKey();
        getProperties().set ("caption", OscRole::describeLong (processorRef, selected)); // for the UI test
        resized();
        updateCardPictures();
        repaint();
        if (onModeChanged != nullptr)
            onModeChanged();
    }

    // M7.3: the Tine and Reed excites (7, 8) have their own controls.
    bool isElectric (int index) const
    {
        const auto excite = juce::roundToInt (readFloat (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_excite"));
        return getMode (index) == 1 && (excite == 7 || excite == 8);
    }

    static void setGroupEnabled (std::initializer_list<juce::Component*> controls, bool enabled)
    {
        for (auto* control : controls)
        {
            control->setEnabled (enabled);
            control->setAlpha (enabled ? 1.0f : IlanaTheme::dimmedAlpha);
        }
    }

    void updateEnabled()
    {
        // The sub's controls follow its switch; noise has its own level.
        const auto subIsOn = readBool ("subosc_on");
        for (auto* control : { static_cast<juce::Component*> (&subShape), static_cast<juce::Component*> (&subOctave),
                               static_cast<juce::Component*> (subOscLevel.get()) })
            if (control != nullptr && control->getAlpha() != (subIsOn ? 1.0f : IlanaTheme::dimmedAlpha))
                control->setAlpha (subIsOn ? 1.0f : IlanaTheme::dimmedAlpha);
        // COLOUR does nothing while there is no noise.
        const auto colourAlpha = readFloat ("noise_level") > 0.0005f ? 1.0f : IlanaTheme::dimmedAlpha;
        if (noiseColourStrip->getAlpha() != colourAlpha)
            noiseColourStrip->setAlpha (colourAlpha);

        const auto boardOn = readBool ("sb_on");
        sbModel.setAlpha (boardOn ? 1.0f : IlanaTheme::dimmedAlpha);
        for (auto* control : { &sbMix, &sbTone, &sbSize })
            control->setAlpha (boardOn ? 1.0f : IlanaTheme::dimmedAlpha);

        // The strings' controls dim while the strings are off; manual notes
        // past STRINGS are not sounding.
        const auto stringsOn = readBool ("sym_on");
        for (auto* control : { &symAmount, &symDecay, &symCount })
            control->setAlpha (stringsOn ? 1.0f : IlanaTheme::dimmedAlpha);
        symManual.setAlpha (stringsOn ? 1.0f : IlanaTheme::dimmedAlpha);
        const auto stringCount = juce::roundToInt (readFloat ("sym_count"));
        for (int i = 0; i < (int) symNotes.size(); ++i)
            symNotes[(size_t) i]->setAlpha (stringsOn && i < stringCount ? 1.0f : IlanaTheme::dimmedAlpha);

        for (int index = 0; index < OscillatorIds::count; ++index)
        {
            const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
            const auto enabled = readBool (prefix + "_on");
            auto& physicalControls = *physical[(size_t) index];
            setGroupEnabled ({ &physicalControls.stiffness, &physicalControls.pickup,
                               &physicalControls.excitePos, &physicalControls.hardness,
                               &physicalControls.pickPos, &physicalControls.slap,
                               &physicalControls.bowPressure, &physicalControls.bowSpeed,
                               &physicalControls.bridgeBuzz, &physicalControls.fretRattle,
                               &physicalControls.hammer, &physicalControls.couple,
                               &physicalControls.damper, &physicalControls.registerMap,
                               &physicalControls.epDistance, &physicalControls.epPosition,
                               &physicalControls.fbGain, &physicalControls.fbDistance }, enabled);

            auto& osc = *controls[(size_t) index];
            setGroupEnabled ({ &osc.mode, &osc.table, &osc.excite, &osc.frame, &osc.level,
                               &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune,
                               &osc.spread, &osc.stringDecay, &osc.stringDamp, &osc.stringSustain,
                               &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse,
                               &osc.sampleStart, &osc.sampleEnd, &osc.sampleFadeIn, &osc.sampleFadeOut,
                               &osc.chord, &osc.warp, &osc.warpAmt, &osc.spectral, &osc.spectralAmt,
                               &osc.grainPosition, &osc.grainSize, &osc.grainDensity,
                               &osc.grainSpray, &osc.grainPitch, &osc.grainSpread, &osc.grainLive,
                               &osc.uniMode, &osc.scale, &osc.scaleRoot, &osc.uniBlend, &osc.uniFrame, &osc.uniWarp, &osc.ampEnv,
                               &osc.warp2, &osc.warp2Amt, &osc.pdEnv, &osc.pdEnvAmt,
                               &osc.tune, &osc.ratio, &osc.fixedHz, &osc.egOut, &osc.trim, &osc.feedback,
                               &osc.feedbackType }, enabled);

            // An amount whose stage or envelope is Off does nothing: dim it
            // (it keeps its place and shows a dash).
            if (enabled)
            {
                osc.warp2Amt.setAlpha (readChoice (prefix + "_warp2") > 0 ? 1.0f : IlanaTheme::dimmedAlpha);
                osc.pdEnvAmt.setAlpha (readChoice (prefix + "_pd_env") > 0 ? 1.0f : IlanaTheme::dimmedAlpha);
            }

            for (auto* knob : { &osc.warpAmt, &osc.warp2Amt, &osc.pdEnvAmt, &osc.spectralAmt, &osc.uniBlend, &osc.uniFrame, &osc.uniWarp, &osc.spread })
                knob->refreshValueText();

            // A switched-off oscillator dims in place, the same size.
            const auto alpha = enabled ? 1.0f : 0.55f;
            auto& pieces = *aux[(size_t) index];
            waveDisplay (index).setAlpha (alpha);
            waveDisplay (index).setEnabled (enabled);
            loadButton (index).setEnabled (! chooserOpen && enabled);
            loadButton (index).setAlpha (alpha);
            editButtons[(size_t) index]->setEnabled (enabled);
            editButtons[(size_t) index]->setAlpha (alpha);
            bounceButtons[(size_t) index]->setAlpha (alpha);
            pieces.stringView.setAlpha (alpha);
            pieces.opEnvGraph.setAlpha (alpha);
            pieces.spreadView.setAlpha (alpha);
            pieces.partials.setAlpha (alpha);
            pieces.status.setAlpha (alpha);
            pieces.opEnvButton.setAlpha (alpha);
            pieces.stringButton.setAlpha (alpha);
            pieces.sampleLoadButton.setAlpha (alpha);
        }

        effectRules.apply();
    }

    // The VOICE group's quiet state: the mode, so the tab says what lives in it.
    juce::String voiceModeName() const
    {
        static const char* const names[] { "POLY", "MONO", "LEGATO" };
        return names[juce::jlimit (0, 2, readChoice ("voice_mode"))];
    }

    int readChoice (const juce::String& id) const
    {
        const auto* value = processorRef.apvts.getRawParameterValue (id);
        return value != nullptr ? juce::roundToInt (value->load()) : 0;
    }

public:
    // M7.4: EDIT. A user table is edited in place; a factory table is first
    // copied into a free patch table, which the oscillator then plays.
    // M8.6: the BOUNCE menu. The choices stay set for the next bounce.
    void showBounceMenu (int index)
    {
        if (bouncingOsc >= 0)
            return;
        juce::PopupMenu menu;
        menu.addSectionHeader ("Resample the patch into OSC " + juce::String (index + 1));
        menu.addItem (1, "As a sample (tuned, one note)");
        menu.addItem (2, "As a wavetable (cut into single cycles)");
        menu.addSeparator();
        menu.addItem (3, "Include the effects", true, bounceRequest.withFx);
        menu.addItem (4, "Mute the other oscillators", true, bounceRequest.muteOthers);
        juce::PopupMenu notes, lengths;
        for (int note : { 36, 48, 60, 72 })
            notes.addItem (100 + note, juce::MidiMessage::getMidiNoteName (note, true, true, 4), true, bounceRequest.note == note);
        for (double hold : { 0.5, 1.0, 2.0, 4.0, 8.0 })
            lengths.addItem (300 + (int) (hold * 2.0), juce::String (hold, hold < 1.0 ? 1 : 0) + " s held + 2 s release",
                             true, std::abs (bounceRequest.holdSeconds - hold) < 1.0e-3);
        menu.addSubMenu ("Note", notes);
        menu.addSubMenu ("Length", lengths);

        juce::Component::SafePointer<OscPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bounceButtons[(size_t) index].get()),
                            [safe, index] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;
                                auto& request = safe->bounceRequest;
                                if (result == 1 || result == 2)
                                {
                                    request.targetOsc = index;
                                    request.toTable = result == 2;
                                    request.tailSeconds = 2.0;
                                    if (safe->processorRef.startBounce (request))
                                    {
                                        safe->bouncingOsc = index;
                                        safe->updateBounce();
                                    }
                                    return;
                                }
                                if (result == 3) request.withFx = ! request.withFx;
                                if (result == 4) request.muteOthers = ! request.muteOthers;
                                if (result >= 100 && result < 300) request.note = result - 100;
                                if (result >= 300) request.holdSeconds = (result - 300) / 2.0;
                                safe->showBounceMenu (index); // keep choosing
                            });
    }

    void updateBounce()
    {
        const auto state = processorRef.getBounceState();
        auto& button = *bounceButtons[(size_t) juce::jlimit (0, OscillatorIds::count - 1, bouncingOsc)];
        if (state == IlanaSynthAudioProcessor::BounceState::Rendering)
        {
            button.setButtonText ("RESAMPLING " + juce::String (juce::roundToInt (processorRef.getBounceProgress() * 100.0f)) + "%");
            if (bounceButtonWide != bouncingOsc)
            {
                bounceButtonWide = bouncingOsc;
                resized();
            }
            for (auto& other : bounceButtons)
                other->setEnabled (false);
            return;
        }
        button.setButtonText ("RESAMPLE");
        for (auto& other : bounceButtons)
            other->setEnabled (true);
        if (state == IlanaSynthAudioProcessor::BounceState::Done)
            selected = bouncingOsc; // the bounced oscillator stays shown
        bouncingOsc = -1;
        bounceButtonWide = -1;
        updateModeVisibility();
        const auto message = processorRef.getBounceMessage();
        if (state == IlanaSynthAudioProcessor::BounceState::Failed)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Resample", message);
        else
            button.setTooltip (message);
        updateModeVisibility();
        updateEnabled();
    }

    void openTableEditor (int index)
    {
        const auto id = juce::String (OscillatorIds::prefixes[(size_t) index]) + "_table";
        const auto factoryCount = TableFactory::getNumFactoryTables();
        const auto choice = readTableChoiceIndex (id);
        auto slot = choice - factoryCount;

        if (slot < 0)
        {
            slot = processorRef.findFreeUserSlot();
            if (slot < 0)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Wavetable editor",
                                                        "All 16 patch tables are in use. Pick a User table to edit it.");
                return;
            }
            auto doc = WavetableDoc::fromFactory (choice);
            doc.name << " edit";
            processorRef.setUserTable (slot, doc);
            if (auto* parameter = processorRef.apvts.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (factoryCount + slot)));
                parameter->endChangeGesture();
            }
        }

        if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
            editor->openWavetableEditor (slot, oscColour (index));
    }

private:
    int readTableChoiceIndex (const juce::String& tableId) const
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (tableId)))
            return param->getIndex();

        return 0;
    }

    IlanaSynthAudioProcessor& processorRef;
    EffectRules effectRules { processorRef }; // after processorRef, which it reads
    std::array<std::unique_ptr<WaveDisplay>, OscillatorIds::count> waveDisplays;
    std::array<FrameScrubber, OscillatorIds::count> scrubbers;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> scrubberAttachments;
    int lastRevealVersion = -1;
    int selected = 0;
    bool stripOpen = false;

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, editButtons, bounceButtons;
    std::array<std::unique_ptr<CardAux>, OscillatorIds::count> aux;
    IlanaSynthAudioProcessor::BounceRequest bounceRequest;
    int bouncingOsc = -1, bounceButtonWide = -1;
    DashedAddButton addButton { "+  ADD OSC", "+  ADD OSC" };
    DashedAddButton addFmButton { "+  ADD FM / DX7", "+  ADD FM / DX7" };
    static constexpr int addFmWidth = 150;
    juce::TextButton moreButton { "MORE" };
    OscPicker oscTabs;
    std::unique_ptr<juce::FileChooser> tableChooser;
    std::array<std::unique_ptr<PhysicalControls>, OscillatorIds::count> physical;
    // Each oscillator's controls by parameter suffix, for the shared
    // physical list.
    std::array<std::map<juce::String, juce::Component*>, OscillatorIds::count> physicalLookup;
    bool chooserOpen = false;

    // Voice-wide settings that shape how the oscillators stack and drift.
    std::unique_ptr<KnobControl> voiceSpread, unisonRandom, drift, voiceCount, bendRange, glideTime;
    std::unique_ptr<ComboControl> voiceMode;
    std::unique_ptr<ToggleControl> glideLegato;
    ToggleControl symOn, symManual;
    KnobControl symAmount, symDecay, symCount;

    // Acoustic keys (M4): soundboard, stretch tuning, pedal resonance and
    // the mechanism's noises, shared by every voice.
    ToggleControl sbOn;
    ComboControl sbModel;
    KnobControl sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal;
    std::array<std::unique_ptr<KnobControl>, 6> symNotes;

    // The dedicated sub and the noise.
    std::unique_ptr<ToggleControl> subOscOn;
    std::unique_ptr<KnobControl> subOscLevel, noiseStrip, noiseColourStrip;

    std::array<std::unique_ptr<OscControls>, OscillatorIds::count> controls;
    ComboControl subShape, subOctave;
};

// The OSC page scrolls only when the card's rows cannot fit at their
// smallest.
class OscPageViewport : public juce::Viewport
{
public:
    explicit OscPageViewport (IlanaSynthAudioProcessor& processor)
    {
        setScrollBarsShown (true, false);
        auto* page = new OscPage (processor);
        page->onModeChanged = [this] { resized(); };
        setViewedComponent (page, true);
    }

    OscPage* getPage() const { return dynamic_cast<OscPage*> (getViewedComponent()); }

    void resized() override
    {
        juce::Viewport::resized();
        if (auto* page = getPage())
        {
            const auto needed = page->getMinimumHeight();
            const auto scrolls = needed > getHeight();
            page->setSize (juce::jmax (1, getWidth() - (scrolls ? getScrollBarThickness() : 0)),
                           juce::jmax (getHeight(), needed));
        }
    }
};
} // namespace
