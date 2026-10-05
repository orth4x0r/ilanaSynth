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

// A wavetable oscillator tuned by ratio or fixed Hz, or on the Operator EG:
// an operator. One in an FM route but tuned in semitones stays an
// oscillator with FM, as on Vital (UI review 7, V7-15).
inline bool isOperator (const IlanaSynthAudioProcessor& p, int osc)
{
    if (juce::roundToInt (read (p, prefix (osc) + "_mode")) != 0)
        return false;

    return tuning (p, osc) != OscTuning::Semitones || usesOperatorEg (p, osc);
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
// OUTPUT is a modulation depth, so the knob says DEPTH (review 12, I12-3).
inline bool isModulator (const IlanaSynthAudioProcessor& p, int osc)
{
    return read (p, prefix (osc) + "_out") <= 0.5f && ! targets (p, osc).empty();
}

inline const char* outputKnobName (const IlanaSynthAudioProcessor& p, int osc)
{
    return isModulator (p, osc) ? "DEPTH" : "OUTPUT";
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
              spectral (state, prefix + "_spectral", "SPECTRAL"),
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
                                &sampleFadeOut, &warpAmt, &uniBlend, &spectralAmt, &grainPosition, &grainSize,
                                &grainDensity, &grainSpray, &grainPitch, &grainSpread })
                knob->setIdentityColour (colour);
        }

        ToggleControl on, sampleTuned, sampleLoop, sampleReverse;
        ComboControl mode, table, excite, chord, ampEnv, warp, uniMode, spectral;
        // M6: the PD chain's second stage and the warp (DCW) envelope.
        ComboControl warp2, pdEnv;
        KnobControl warp2Amt, pdEnvAmt;
        KnobControl frame, level, pan, semi, fine, unison, detune, spread;
        KnobControl stringDecay, stringDamp, stringSustain;
        KnobControl sampleStart, sampleEnd, sampleFadeIn, sampleFadeOut;
        KnobControl warpAmt, uniBlend, spectralAmt;
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
          subShape (p.apvts, "sub_shape", "SHAPE"),
          subOctave (p.apvts, "sub_octave", "OCTAVE")
          , symOn (p.apvts, "sym_on", "ON"), symManual (p.apvts, "sym_manual", "MANUAL")
          , symAmount (p.apvts, "sym_amount", "AMOUNT"), symDecay (p.apvts, "sym_decay", "DECAY")
          , symCount (p.apvts, "sym_count", "STRINGS")
          , sbOn (p.apvts, "sb_on", "SOUNDBOARD"), sbModel (p.apvts, "sb_model", "MODEL"), sbMix (p.apvts, "sb_mix", "SOUNDBOARD MIX"), sbTone (p.apvts, "sb_tone", "TONE")
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
            // There is room here for every frame at once (UI review 5, V6).
            waveDisplays[(size_t) i]->setViewMode (1);
            loadButtons[(size_t) i] = std::make_unique<juce::TextButton> ("LOAD...");
            editButtons[(size_t) i] = std::make_unique<juce::TextButton> ("EDIT");
            styleJumpLink (*editButtons[(size_t) i], "TABLE");
            // RESAMPLE: one name for the resampler, apart from the Bounce
            // LFO shapes (UI review 7, I7-25).
            bounceButtons[(size_t) i] = std::make_unique<juce::TextButton> ("RESAMPLE");
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
                                osc.spectral, osc.spectralAmt, osc.warp, osc.uniMode,
                                osc.warpAmt, osc.uniBlend, osc.warp2, osc.pdEnv, osc.warp2Amt, osc.pdEnvAmt);
            addChildComponents (osc.on, osc.mode, osc.table, osc.excite,
                                osc.frame, osc.level, osc.pan, osc.semi, osc.fine,
                                osc.unison, osc.detune, osc.spread, osc.stringDecay,
                                osc.stringDamp, osc.stringSustain, osc.sampleTuned,
                                osc.sampleLoop, osc.sampleReverse, osc.sampleStart,
                                osc.sampleEnd, osc.sampleFadeIn, osc.sampleFadeOut,
                                osc.chord, osc.ampEnv, osc.tune, osc.ratio, osc.fixedHz,
                                osc.egOut, osc.trim, osc.feedback, osc.feedbackType);
        }

        addChildComponent (stringView);
        addChildComponent (opEnvGraph);
        opEnvGraph.setReadOnly (true);
        opEnvGraph.onOpen = [this]
        {
            if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                editor->showOperatorEnvelope (selected);
        };

        for (int i = 0; i < OscillatorIds::count; ++i)
        {
            addChildComponent (waveDisplay (i));
            setupLoadButton (loadButton (i),
                             juce::String (OscillatorIds::prefixes[(size_t) i]) + "_table", 0);
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
        }

        // The Operator Env has one editor, on FM's operator card; here a
        // picture of it and a link there (UI review 8, S8-4).
        styleJumpLink (opEnvButton, "OP ENV");
        opEnvButton.setTooltip ("This operator plays its OP ENV: edit it on the FM page");
        opEnvButton.onClick = [this]
        {
            if (auto* editor = findParentComponentOfClass<IlanaSynthAudioProcessorEditor>())
                editor->showOperatorEnvelope (selected);
        };
        addChildComponent (opEnvButton);

        // A Physical oscillator's string has one editor, the PHYSICAL page;
        // here its picture, its main knobs and a link there (UI review 9,
        // I9-3).
        styleJumpLink (stringButton, "STRING");
        stringButton.setTooltip ("Every control of this oscillator's string, exciter and body is on the PHYSICAL page");
        stringButton.onClick = [this] { showPhysicalString (*this, selected); };
        addChildComponent (stringButton);

        sampleLoadButton.setButtonText ("LOAD...");
        sampleLoadButton.setTooltip ("Load a sample or an SF2 / SFZ multisample, or pick a factory sample.  You can also drop a .wav or .sfz file on the picture");
        sampleLoadButton.onClick = [this] { waveDisplay (selected).showSampleMenu (sampleLoadButton); };
        styleHeaderButton (sampleLoadButton);
        addChildComponent (sampleLoadButton);

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

        // The shared sections under the oscillator, one at a time behind
        // their tabs (each tab lit while its part is on).
        voiceSpread = std::make_unique<KnobControl> (p.apvts, "voice_spread", "SPREAD");
        unisonRandom = std::make_unique<KnobControl> (p.apvts, "unison_random", "UNI PHASE");
        // The oscillators' analogue drift (the vector pad's drift is WANDER:
        // UI review 6, I6-25).
        drift = std::make_unique<KnobControl> (p.apvts, "drift", "ANALOG DRIFT");
        // VOICE: how the notes are shared out (review 9, S9-1): the header's
        // VOICES button opens the same settings.
        voiceMode = std::make_unique<ComboControl> (p.apvts, "voice_mode", "MODE");
        voiceCount = std::make_unique<KnobControl> (p.apvts, "poly_voices", "VOICES", IlanaTheme::Ui::text2, false);
        bendRange = std::make_unique<KnobControl> (p.apvts, "bend_range", "BEND RANGE", IlanaTheme::Ui::text2, false);
        glideTime = std::make_unique<KnobControl> (p.apvts, "glide", "GLIDE", IlanaTheme::Ui::text2, false);
        glideLegato = std::make_unique<ToggleControl> (p.apvts, "glide_legato", "LEGATO ONLY");
        // The switch is the sub's alone, and says so; the noise has its
        // own level and colour beside it (V8-14, V8-15).
        subOscOn = std::make_unique<ToggleControl> (p.apvts, "subosc_on", "ON");
        subOscLevel = std::make_unique<KnobControl> (p.apvts, "subosc_level", "SUB", IlanaTheme::accent(), true);
        noiseStrip = std::make_unique<KnobControl> (p.apvts, "noise_level", "NOISE", IlanaTheme::Ui::text2, false);
        noiseColourStrip = std::make_unique<KnobControl> (p.apvts, "noise_color", "COLOUR", IlanaTheme::Ui::text2, false);
        addChildComponents (subShape, subOctave, *subOscLevel, *noiseStrip, *noiseColourStrip, *subOscOn, *voiceSpread, *unisonRandom, *drift,
                            *voiceMode, *voiceCount, *bendRange, *glideTime, *glideLegato,
                            symOn, symAmount, symDecay, symCount, symManual,
                            sbOn, sbModel, sbMix, sbTone, sbSize, stretch, pedalRes, mechKey, mechDamper, mechPedal);
        for (auto& note : symNotes)
            addChildComponent (*note);
        sbOn.showAsSwitch();

        oscTabs.setName ("OSC tabs"); // (the UI test finds them by it)
        oscTabs.onPick = [this] (int osc) { selectOscillator (osc); };
        oscTabs.onMenu = [this] (int tab) { showOscMenu (oscForTab (tab)); };
        addAndMakeVisible (oscTabs);
        sharedTabs.onSelect = [this] (int tab) { selectShared (tab); };
        addAndMakeVisible (sharedTabs);

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

            for (auto* knob : { &osc.uniBlend, &osc.spread, &osc.detune })
                effectRules.add (*knob, effectRules.isAbove (prefix + "_unison", 1.5f), "UNISON is 1");
        }

        // One way to add an oscillator here: the tab row's last button
        // (UI review 6, S33).
        addButton.setTooltip ("Add the next oscillator, switched on");
        addButton.onClick = [this]
        {
            for (int i = 0; i < OscillatorIds::count; ++i)
                if (! processorRef.isOscillatorShown (i))
                {
                    processorRef.performEdit ("Add OSC " + juce::String (i + 1), [this, i] { processorRef.addOscillator (i); });
                    selected = i;
                    break;
                }

            updateModeVisibility();
            updateEnabled();
        };
        addChildComponent (addButton);

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
    // change what the tabs say.
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
            updateTabs();

            if (OscRole::describeLong (processorRef, selected) != shownRole || rowsKey (selected) != shownRows)
                updateModeVisibility();
        }

        effectRules.apply();
    }

    void paint (juce::Graphics& g) override
    {
        IlanaTheme::paintPageBackground (g, getLocalBounds());

        // The chosen oscillator's card: its tag, name and what it is, after
        // the MODE menu.
        if (! oscCard.isEmpty())
        {
            const auto tint = oscColour (selected);
            IlanaTheme::paintCard (g, oscCard.toFloat(), 6.0f, tint);
            const auto stripe = juce::Rectangle<float> ((float) oscCard.getX() + 2.0f, (float) oscCard.getY() + 6.0f,
                                                        3.0f, (float) oscCard.getHeight() - 12.0f);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (stripe, 1.5f);

            const auto headerY = oscCard.getY() + headerHeight / 2;
            IlanaTheme::paintTag (g, { (float) oscCard.getX() + 17.0f, (float) headerY }, tint);
            g.setColour (IlanaTheme::Ui::text);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::body, true));
            g.drawText ("OSC " + juce::String (selected + 1), juce::Rectangle<int> (oscCard.getX() + 28, headerY - 8, 60, 16),
                        juce::Justification::centredLeft);

            // An FM-modulated oscillator says so in a pill in its source's colour (review 12, S12-8).
            if (captionIsLink())
            {
                const auto from = OscRole::sources (processorRef, selected);
                const auto pillColour = from.empty() ? tint : oscColour (from.front());
                const auto text = shownRole + juce::String::fromUTF8 ("  \xe2\x80\xba");
                const auto font = IlanaTheme::font (IlanaTheme::TextSize::label, true);
                const auto width = juce::jmin (subtitleArea.getWidth(), juce::GlyphArrangement::getStringWidthInt (juce::Font (font), text) + 18);
                const auto pill = subtitleArea.withWidth (width).withSizeKeepingCentre (width, 20).toFloat();
                g.setColour (pillColour.withAlpha (0.18f));
                g.fillRoundedRectangle (pill, 10.0f);
                g.setColour (pillColour.withAlpha (0.8f));
                g.drawRoundedRectangle (pill.reduced (0.5f), 10.0f, 1.0f);
                g.setColour (IlanaTheme::Ui::text);
                g.setFont (font);
                g.drawText (text, pill.toNearestInt().reduced (9, 0), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (IlanaTheme::Ui::text3);
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
                g.drawText (isOff (selected) ? juce::String::fromUTF8 ("off  \xc2\xb7  switch on to hear it")
                                             : shownRole + (shownRole.contains ("FM") ? juce::String::fromUTF8 ("  \xe2\x80\xba") : juce::String()), subtitleArea,
                            juce::Justification::centredLeft, true);
            }

            if (! controlBay.isEmpty())
                IlanaTheme::paintRecessedPanel (g, controlBay.toFloat(), 6.0f);

            // Each row named at its left.
            for (const auto& [area, name] : rowLabels)
            {
                g.setColour (tint.withAlpha (0.8f));
                g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
                IlanaTheme::drawFitted (g, name, area, juce::Justification::topLeft, 1);
            }
        }

        if (! sharedCard.isEmpty())
            IlanaTheme::paintRecessedPanel (g, sharedCard.toFloat(), 6.0f);

        // Folded: what is off, and what opens it (the sub's switch).
        if (shownSharedFolded)
        {
            const auto left = sharedTabs.getRight() + 14 + 44 + 10;
            g.setColour (IlanaTheme::Ui::text3);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label));
            IlanaTheme::drawFitted (g, "sub off, no noise: switch the sub on to set them",
                                    juce::Rectangle<int> (left, sharedCard.getY(), sharedCard.getRight() - 20 - left, sharedHeaderHeight),
                                    juce::Justification::centredLeft, 1);
        }

        // SUB + NOISE's two halves, each named at its left.
        for (const auto& [area, name] : sharedLabels)
        {
            g.setColour (name == "SUB" ? IlanaTheme::accent().withAlpha (0.8f) : IlanaTheme::Ui::text2);
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));
            IlanaTheme::drawFitted (g, name, area, juce::Justification::topLeft, 1);
        }
        for (const auto& divider : sharedDividers)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (divider);
        }
    }

    // The caption's "FM FROM OSC 2 30 % ›" is a link to the FM page (review
    // 11, I11-4).
    bool captionIsLink() const { return ! isOff (selected) && shownRole.contains ("FM") && FmOperatorInfo::hooks().openOperator != nullptr; }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (captionIsLink() && subtitleArea.contains (event.getPosition()) ? juce::MouseCursor::PointingHandCursor
                                                                                        : juce::MouseCursor::NormalCursor);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (captionIsLink() && subtitleArea.contains (event.getPosition()) && ! event.mouseWasDraggedSinceMouseDown())
            FmOperatorInfo::hooks().openOperator (selected);
    }

    static juce::Colour oscColour (int index) { return IlanaTheme::oscColour (index); }

    // The height the page needs: the tab row, the card with its rows at the
    // smallest knobs, and the shared card.
    int getMinimumHeight() const
    {
        auto units = 0;
        for (const auto& row : rowsFor (selected))
            units += rowUnits (row);
        return pageMargin * 2 + tabRowHeight + gap * 2 + headerHeight + 10 + units * minRowHeight + sharedHeight;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, pageMargin);
        area.removeFromTop (tabRowHeight + gap);

        // SUB + NOISE with the sub off and no noise folds to its header (the
        // tabs and the switch), as WEST and BODY do on FILTER (V10-9).
        shownSharedFolded = sharedFolded();
        sharedCard = area.removeFromBottom (shownSharedFolded ? sharedHeaderHeight + 6 : sharedHeight);
        area.removeFromBottom (gap);
        oscCard = area;
        layoutTabs();
        layoutCard();
        layoutShared();
    }

    // The oscillator the card shows (the UI test and the FM page's links
    // pick one).
    void selectOscillator (int osc)
    {
        if (osc < 0 || osc >= OscillatorIds::count)
            return;

        selected = osc;
        updateModeVisibility();
        updateEnabled();
    }

    int getSelectedOscillator() const { return selected; }

    // The shared section below the card: SUB + NOISE, VOICE, SPREAD & DRIFT,
    // STRINGS (sympathetic) or SOUNDBOARD.
    // The tabs, in their order (the groups: the sub and noise, then the
    // global drawer).
    // VOICE first and open by default: polyphony is found where the voices are
    // set (review 11, S11-1).
    enum SharedTab { sharedVoice = 0, sharedSubNoise, sharedSpread, sharedSympathetic, sharedKeys };

    void selectShared (int index)
    {
        sharedSelected = juce::jlimit (0, shownShared - 1, index);
        sharedTabs.setSelected (sharedSelected);
        updateModeVisibility();
        // SUB + NOISE folds while off: picking it (or leaving it) re-lays the card.
        if (sharedFolded() != shownSharedFolded)
        {
            resized();
            repaint();
        }
    }

    // The UI test reads the menu's items.
    juce::StringArray getOscMenuItems (int band) const
    {
        return { isOff (band) ? "Switch on" : "Switch off", "Remove oscillator" };
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

    static constexpr int pageMargin = 6, gap = 6, tabRowHeight = 30, headerHeight = 32;
    static constexpr int sharedHeaderHeight = 30, sharedHeight = sharedHeaderHeight + 74;
    static constexpr int minRowHeight = 62, maxRowHeight = 104, rowLabelWidth = 96;

    // What the rows hold, to lay them out again when it changes.
    juce::String rowsKey (int index) const
    {
        juce::String key;
        for (const auto& row : rowsFor (index))
        {
            key << row.first << ":" << (int) row.second.size();
            for (auto* item : row.second)
                key << (item == nullptr ? '-' : '+'); // (an amount hides while its stage is Off)
            key << ";";
        }
        return key;
    }

    juce::String shownRows;

    // A row's height in rows: the Operator Env's graph takes two.
    int rowUnits (const std::pair<juce::String, std::vector<juce::Component*>>& row) const
    {
        return std::find (row.second.begin(), row.second.end(), &opEnvGraph) != row.second.end() ? 2 : 1;
    }
    static constexpr int numShared = 5;
    int shownShared = numShared; // the tabs drawn: three on an operator voice (I12-6)

    // An operator voice (an Operator Env oscillator, none physical) with the
    // strings and the soundboard off.
    bool sharedPhysicalTabsHidden() const
    {
        if (readBool ("sym_on") || readBool ("sb_on"))
            return false;
        bool anyOperator = false;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
            {
                if (getMode (osc) == 1)
                    return false;
                anyOperator = anyOperator || OscRole::usesOperatorEg (processorRef, osc);
            }
        return anyOperator;
    }

    int lastRevealVersion = -1;
    int selected = 0, sharedSelected = 0;
    bool shownSharedFolded = false;

    bool sharedFolded() const { return sharedSelected == sharedSubNoise && ! readBool ("subosc_on") && readFloat ("noise_level") <= 0.0005f; }
    juce::String shownRole;
    juce::Rectangle<int> oscCard, sharedCard, controlBay, subtitleArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> rowLabels, sharedLabels;
    std::vector<juce::Rectangle<int>> sharedDividers;

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

    // The tabs list the added oscillators in order.
    int oscForTab (int tab) const
    {
        for (int osc = 0, index = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc) && index++ == tab)
                return osc;

        return selected;
    }

    void updateTabs()
    {
        updateTabItems();
        // A state that changed length moves the tabs after it.
        layoutTabs();
    }

    void layoutTabs()
    {
        auto tabRow = getLocalBounds().reduced (12, pageMargin).removeFromTop (tabRowHeight);
        const auto canAdd = numShown() < OscillatorIds::count;
        addButton.setVisible (canAdd);
        // Named as PLAY's (review 8, I8-31): "+ ADD OSC 4".
        for (int i = 0; i < OscillatorIds::count; ++i)
            if (! processorRef.isOscillatorShown (i))
            {
                addButton.setLabel ("+  ADD OSC " + juce::String (i + 1));
                break;
            }
        oscTabs.setBounds (tabRow.withWidth (juce::jmin (tabRow.getWidth() - (canAdd ? 130 : 0), oscTabs.getIdealWidth())));
        addButton.setBounds (juce::Rectangle<int> (oscTabs.getRight() + gap, tabRow.getCentreY() - DashedAddButton::standardHeight / 2, 128, DashedAddButton::standardHeight));

        if (! sharedCard.isEmpty())
        {
            const auto header = sharedCard.reduced (10, 0).withHeight (sharedHeaderHeight);
            sharedTabs.setBounds (header.withWidth (juce::jmin (header.getWidth() - 60, sharedTabs.getIdealWidth())).reduced (0, 3));
        }
    }

    void updateTabItems()
    {
        static const char* const modeNames[] { "WAVETABLE", "PHYSICAL", "SAMPLE", "GRANULAR", "LIVE" };
        std::vector<int> shown;
        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (processorRef.isOscillatorShown (osc))
                shown.push_back (osc);

        // The one oscillator picker (UI review 9, I9-5), each tab with the
        // oscillator's role after its name.
        oscTabs.setOscillators (shown, [this] (int osc) { return ! isOff (osc); },
                                [this] (int osc)
                                {
                                    const auto role = OscRole::describe (processorRef, osc);
                                    const auto mode = juce::jlimit (0, 4, getMode (osc));
                                    return "OSC " + juce::String (osc + 1) + ": " + juce::String (modeNames[mode]).toLowerCase()
                                           + (role.isNotEmpty() ? ", " + OscRole::describeLong (processorRef, osc) : juce::String())
                                           + (isOff (osc) ? ", switched off" : "") + ".  Right-click to switch it off or remove it.";
                                },
                                [this] (int osc)
                                {
                                    // Off: the dot says it. One rule on every page (I12-2): the
                                    // chosen tab is wide with its role, the others a dot and a
                                    // number; their roles are in their tooltips.
                                    if (osc != selected)
                                        return juce::String();
                                    const auto role = OscRole::describe (processorRef, osc);
                                    return isOff (osc) ? juce::String() : role.isNotEmpty() ? role : juce::String (modeNames[juce::jlimit (0, 4, getMode (osc))]);
                                });
        oscTabs.setSelectedOsc (selected);

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
            if (const auto name = juce::String (OscRole::outputKnobName (processorRef, osc)); controls[(size_t) osc]->egOut.getLabelText() != name)
                controls[(size_t) osc]->egOut.setLabelText (name);

        // SUB + NOISE belong to the oscillators; after the gap, the global
        // drawer: voice settings, the spread and drift every voice shares, the
        // shared strings and the keys' body (review 10, S10-3, S10-4).
        std::vector<StateTabs::Item> sharedItemsList { { "VOICE", voiceModeName(), IlanaTheme::Ui::text2, true, "Poly, mono or legato, how many voices, the pitch-bend range and glide.  The header's VOICES opens this.", false, false },
                               { "SUB + NOISE", {}, IlanaTheme::accent(), readBool ("subosc_on"),
                                 "The sub oscillator and the noise, under every oscillator" },
                               { "SPREAD & DRIFT", {}, IlanaTheme::Ui::text2, true, "How far the voices spread across the stereo field, how their phases start and how far they drift.  The unison itself is on the oscillator's card.", false },
                               { "STRINGS", {}, IlanaTheme::Ui::text2, readBool ("sym_on"),
                                 "Sympathetic strings: shared drone strings that ring with everything you play" },
                               { "SOUNDBOARD", {}, IlanaTheme::Ui::text2, true,
                                 "The acoustic keys' body: soundboard, stretch tuning, sustain pedal (CC64) resonance and the action's noises", false } };
        // An operator voice has no strings or soundboard to tune: the last two
        // tabs wait until an oscillator is physical or they are on (I12-6).
        if (sharedPhysicalTabsHidden())
            sharedItemsList.resize (sharedSympathetic);
        shownShared = (int) sharedItemsList.size();
        sharedTabs.setItems (std::move (sharedItemsList));
        if (sharedSelected >= shownShared)
        {
            sharedSelected = sharedVoice;
            triggerAsyncUpdate();
        }
        sharedTabs.setSelected (sharedSelected);
    }

    // The card's rows for an oscillator, each named for what it shapes; the
    // physical rows come from the list the PHYSICAL page builds from too.
    std::vector<std::pair<juce::String, std::vector<juce::Component*>>> rowsFor (int index) const
    {
        auto& osc = *controls[(size_t) index];
        const auto mode = getMode (index);
        const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
        std::vector<std::pair<juce::String, std::vector<juce::Component*>>> rows;

        // An operator on the Operator Env: its envelope as a picture that
        // opens its one editor (FM's card, UI review 8, S8-4), then its pitch
        // and levels in the FM card's order (RATIO, SEMI, FINE, OUTPUT, LEVEL:
        // V8-5), then the wave it plays; UNISON only once it is on (I8-15).
        if (mode == 0 && OscRole::usesOperatorEg (processorRef, index))
        {
            const auto tuning = OscRole::tuning (processorRef, index);
            rows.push_back ({ "OP ENV", { (juce::Component*) &opEnvGraph } });
            std::vector<juce::Component*> pitch { &osc.tune };
            if (tuning == OscTuning::Ratio)
                pitch.push_back (&osc.ratio);
            if (tuning == OscTuning::Fixed)
                pitch.push_back (&osc.fixedHz);
            pitch.insert (pitch.end(), { &osc.semi, &osc.fine, &osc.egOut, &osc.pan, &osc.ampEnv });
            rows.push_back ({ "PITCH & OUTPUT", pitch });
            std::vector<juce::Component*> wave { &osc.table, &osc.feedback, &osc.feedbackType };
            if (readFloat (prefix + "_unison") > 1.5f)
                wave.push_back (&osc.unison);
            rows.push_back ({ "WAVE", wave });
            // The one level an operator shows is OUTPUT (review 10, I10-1;
            // review 12, I12-1): the oscillator's own level into the voice
            // (VOICE LEVEL) is not drawn here at all; it stays a parameter
            // (the host's list, the matrix).
            return rows;
        }

        std::vector<juce::Component*> pitch { &osc.level, &osc.pan };

        if (mode == 0)
        {
            // A wavetable can be an FM operator: its TUNING, then RATIO or
            // FIXED in SEMI's place (as on the FM page).
            const auto tuning = OscRole::tuning (processorRef, index);
            pitch.push_back (&osc.tune);
            pitch.push_back (tuning == OscTuning::Ratio ? (juce::Component*) &osc.ratio
                                                        : tuning == OscTuning::Fixed ? (juce::Component*) &osc.fixedHz : (juce::Component*) &osc.semi);
        }
        else if (mode != 4)
            pitch.push_back (&osc.semi);

        if (mode != 4)
            pitch.push_back (&osc.fine);

        pitch.push_back (&osc.ampEnv);

        std::vector<juce::Component*> unison { &osc.uniMode, &osc.unison, &osc.detune };

        if (mode != 3)
            unison.insert (unison.end(), { &osc.uniBlend, &osc.spread });

        unison.push_back (&osc.chord);

        // A sample or grain oscillator with one voice has nothing to blend:
        // the row folds to its UNISON and CHORD until UNISON is raised
        // (review 11, I11-11).
        if ((mode == 2 || mode == 3) && readFloat (prefix + "_unison") < 1.5f)
            unison = { &osc.unison, &osc.chord };

        if (mode == 0)
        {
            // An amount whose stage is Off isn't drawn (its column stays
            // empty, so the others don't move: UI review 9, V9-9).
            rows.push_back ({ "SHAPE", { &osc.frame, &osc.warp, readChoice (prefix + "_warp") > 0 ? &osc.warpAmt : nullptr,
                                         &osc.spectral, readChoice (prefix + "_spectral") > 0 ? &osc.spectralAmt : nullptr } });

            if (showsWarpChain (index))
                rows.push_back ({ "WARP CHAIN", { &osc.warp2, readChoice (prefix + "_warp2") > 0 ? &osc.warp2Amt : nullptr,
                                                  &osc.pdEnv, readChoice (prefix + "_pd_env") > 0 ? &osc.pdEnvAmt : nullptr } });
        }
        else if (mode == 1)
        {
            // The string is edited on PHYSICAL only (UI review 9, I9-3): the
            // card keeps its exciter, DECAY and DAMP (PLAY's two) and
            // EDIT STRING › in the header.
            rows.push_back ({ "STRING", { &osc.excite, nullptr, &osc.stringDecay, &osc.stringDamp } });
        }
        else if (mode == 2)
            rows.push_back ({ "SAMPLE", { &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.sampleStart, &osc.sampleEnd,
                                          &osc.sampleFadeIn, &osc.sampleFadeOut } });
        else if (mode == 3)
        {
            std::vector<juce::Component*> grains;
            if (IlanaSynthAudioProcessor::isEffectBuild)
                grains.push_back (&osc.grainLive);
            grains.insert (grains.end(), { &osc.sampleTuned, &osc.sampleReverse, &osc.grainPosition, &osc.grainSize,
                                           &osc.grainDensity, &osc.grainSpray, &osc.grainPitch, &osc.grainSpread });
            rows.push_back ({ "GRAINS", grains });
        }

        rows.push_back ({ mode == 4 ? "LEVEL" : "PITCH & LEVEL", pitch });

        // M7.5 Live: the input itself, so no pitch, shape or unison.
        if (mode != 4)
            rows.push_back ({ mode == 1 ? "STRING COPIES" : "UNISON", unison }); // (copies of the string: I11-9)

        return rows;
    }

    void layoutCard()
    {
        rowLabels.clear();
        controlBay = {};

        if (oscCard.isEmpty())
            return;

        const auto index = selected;
        const auto mode = getMode (index);
        auto& osc = *controls[(size_t) index];

        // Header: name, MODE menu, what it is; the actions and the switch at
        // the right.
        auto header = oscCard.reduced (8, 0).withHeight (headerHeight);
        osc.on.setBounds (IlanaTheme::cardSwitchBounds (oscCard, header.getCentreY()));
        header.setRight (osc.on.getX() - 8);
        header.removeFromLeft (80);
        osc.mode.setBounds (header.removeFromLeft (132).withSizeKeepingCentre (132, 24));
        header.removeFromLeft (10);

        for (auto* button : { opEnvButton.isVisible() ? &opEnvButton : nullptr, stringButton.isVisible() ? &stringButton : nullptr,
                              &loadButton (index), &sampleLoadButton,
                              editButtons[(size_t) index].get(), bounceButtons[(size_t) index].get() })
            if (button != nullptr && button->isVisible())
            {
                const auto width = juce::GlyphArrangement::getStringWidthInt (juce::Font (IlanaTheme::font (IlanaTheme::TextSize::minInteractive)),
                                                                               button->getButtonText()) + 24;
                button->setBounds (header.removeFromRight (width).withSizeKeepingCentre (width, 22));
                header.removeFromRight (6);
            }

        subtitleArea = header;

        auto content = oscCard.reduced (8).withTrimmedTop (headerHeight - 6);
        // An operator on the Operator Env shows its envelope in the rows
        // (the whole width, no sine beside it: review 11, V11-7).
        const auto operatorEnvelope = mode == 0 && OscRole::usesOperatorEg (processorRef, index);
        const auto display = operatorEnvelope ? juce::Rectangle<int>() : content.removeFromLeft (juce::jlimit (220, 330, content.getWidth() * 30 / 100));
        if (! operatorEnvelope)
            content.removeFromLeft (8);

        if (mode == 1)
        {
            // A string moving as it is played (UI review 6, V24); a tine or
            // reed keeps its pickup curve as an inset.
            stringView.setBounds (display);

            if (isElectric (index))
                waveDisplay (index).setBounds (display.withTrimmedLeft (display.getWidth() * 45 / 100).withHeight (display.getHeight() * 36 / 100)
                                                   .reduced (8));
        }
        else
            waveDisplay (index).setBounds (display);

        // The rows packed from the top at about a knob's height, each named
        // level with its controls' labels (UI review 7, S7-25, V7-29); the
        // Operator Env's graph row is twice as tall.
        const auto rows = rowsFor (index);
        auto units = 0;
        for (const auto& row : rows)
            units += rowUnits (row);
        const auto unitHeight = juce::jmin (maxRowHeight, content.getHeight() / juce::jmax (1, units));
        controlBay = content.withHeight (juce::jmin (content.getHeight(), unitHeight * units + 6)).expanded (4, 0);

        // The grid is as many columns as the widest row, six at least.
        size_t gridColumns = 6;
        for (const auto& row : rows)
            if (rowUnits (row) == 1)
                gridColumns = juce::jmax (gridColumns, row.second.size());

        for (size_t r = 0; r < rows.size(); ++r)
        {
            auto row = content.removeFromTop (unitHeight * rowUnits (rows[r]));
            rowLabels.push_back ({ row.removeFromLeft (rowLabelWidth).reduced (8, 0).withTrimmedTop (3).withHeight (18), rows[r].first });
            // On a shared grid of at least seven columns, so knobs line up
            // from row to row (the Operator Env's graph takes the columns
            // after its LEVEL).
            auto items = rows[r].second;
            while (items.size() < gridColumns && rowUnits (rows[r]) == 1)
                items.push_back (nullptr);
            layoutSlots (row, items, rowUnits (rows[r]) == 1);
        }
    }

    // The shared section's tabs in its header, its switch at the right, one
    // row of controls under them.
    void layoutShared()
    {
        auto header = sharedCard.reduced (10, 0).withHeight (sharedHeaderHeight);
        // The switches sit in their tab's content, ahead of its controls
        // (review 11, S11-4, I11-6); folded, the sub's is beside its hint.
        sharedLabels.clear();
        sharedDividers.clear();
        auto row = sharedCard.withTrimmedTop (sharedHeaderHeight).reduced (8, 0).withTrimmedBottom (4);

        if (shownSharedFolded)
        {
            for (auto* item : sharedItems (sharedSubNoise))
                if (item != nullptr)
                    item->setBounds ({});
            subOscOn->setBounds (juce::Rectangle<int> (sharedTabs.getRight() + 14, header.getCentreY() - 12, 44, 24));
            return;
        }

        // SUB + NOISE in two named halves (the sub's controls, then after
        // the gap the noise's), so its row reads as two small modules
        // rather than four knobs and an empty half (UI review 8, S8-29).
        if (sharedSelected == sharedSubNoise)
        {
            const auto items = sharedItems (sharedSubNoise);
            const auto split = std::find (items.begin(), items.end(), nullptr);
            std::vector<juce::Component*> sub (items.begin(), split), noise;
            for (auto it = split; it != items.end(); ++it)
                if (*it != nullptr)
                    noise.push_back (*it);
            // Two halves no wider than their controls need, side by side in
            // the middle of the card, not spread over its whole width with
            // 200 px between knobs (UI review 9, V9-9).
            // (Sized by their controls, three and two cells, no empty slot.)
            constexpr int cellWidth = 150;
            const auto subShare = (int) sub.size() * cellWidth + (rowLabelWidth - 24), noiseShare = (int) noise.size() * cellWidth + (rowLabelWidth - 24);
            row = row.withSizeKeepingCentre (juce::jmin (row.getWidth(), subShare + noiseShare), row.getHeight());
            auto half = row.removeFromLeft (row.getWidth() * subShare / juce::jmax (1, subShare + noiseShare));
            sharedDividers.push_back (juce::Rectangle<int> (row.getX(), row.getY() + 6, 1, row.getHeight() - 12));
            for (const auto& [area, name, group] : { std::tuple<juce::Rectangle<int>*, const char*, std::vector<juce::Component*>*> { &half, "SUB", &sub },
                                               { &row, "NOISE", &noise } })
            {
                sharedLabels.push_back ({ area->removeFromLeft (rowLabelWidth - 24).reduced (8, 0).withTrimmedTop (3).withHeight (18), name });
                layoutSlots (*area, *group);
            }
            return;
        }

        layoutSlots (row, sharedItems (sharedSelected));
    }

    std::vector<juce::Component*> sharedItems (int index)
    {
        switch (index)
        {
            // (The sub's controls, then the noise's after the gap.)
            case sharedSubNoise: return { subOscOn.get(), &subShape, &subOctave, subOscLevel.get(), nullptr, noiseStrip.get(), noiseColourStrip.get() };
            case sharedSpread: return { voiceSpread.get(), unisonRandom.get(), drift.get(), nullptr, nullptr, nullptr, nullptr, nullptr };
            case sharedSympathetic: return { &symOn, &symAmount, &symDecay, &symCount, &symManual, symNotes[0].get(), symNotes[1].get(), symNotes[2].get(),
                             symNotes[3].get(), symNotes[4].get(), symNotes[5].get() };
            case sharedVoice: return { voiceMode.get(), voiceCount.get(), bendRange.get(), glideTime.get(), glideLegato.get(), nullptr, nullptr, nullptr };
            default: return { &sbOn, &sbModel, &sbMix, &sbTone, &sbSize, &stretch, &pedalRes, &mechKey, &mechDamper, &mechPedal };
        }
    }

    // A wavetable oscillator with a warp picked opens a row for the PD
    // chain's second stage and the warp envelope.
    bool showsWarpChain (int index) const
    {
        if (getMode (index) != 0)
            return false;

        const juce::String prefix (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]);
        return readFloat (prefix + "_warp") > 0.5f || readFloat (prefix + "_warp2") > 0.5f;
    }

    // An oscillator tab's right-click menu.
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

    static float slotWeight (juce::Component* item)
    {
        if (dynamic_cast<OperatorEnvDisplay*> (item) != nullptr)
            return 6.6f;

        if (dynamic_cast<ComboControl*> (item) != nullptr)
            return 1.5f;

        if (dynamic_cast<ToggleControl*> (item) != nullptr)
            return 0.75f;

        return 1.1f;
    }

    // `grid`: the card's rows share one column grid (every control one
    // column, at least seven of them), so knobs and menus line up from row to
    // row and the rows fill the card (review 11, S11-2, S11-3); the drawer's
    // rows keep their weights.
    static void layoutSlots (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, bool grid = false)
    {
        if (items.empty())
            return;

        if (grid && items.size() >= 2)
        {
            const auto columns = (int) items.size();
            const auto cellWidth = (float) area.getWidth() / (float) columns;
            const auto left = area.getX();
            for (int i = 0; i < columns; ++i)
                if (items[(size_t) i] != nullptr)
                {
                    // A menu takes the empty column after it (a WARP with no
                    // amount, an exciter's name), so its text has room.
                    const auto span = dynamic_cast<ComboControl*> (items[(size_t) i]) != nullptr && i + 1 < columns && items[(size_t) i + 1] == nullptr ? 2 : 1;
                    items[(size_t) i]->setBounds (juce::Rectangle<int> (left + juce::roundToInt ((float) i * cellWidth), area.getY(),
                                                                          juce::roundToInt ((float) (i + span) * cellWidth) - juce::roundToInt ((float) i * cellWidth),
                                                                          area.getHeight()).reduced (3));
                }
            return;
        }

        const auto totalWidth = area.getWidth();
        auto totalWeight = 0.0f;

        for (auto* item : items)
            totalWeight += slotWeight (item);

        if (totalWeight <= 0.0f)
            return;

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto width = i + 1 == items.size()
                                   ? area.getWidth()
                                   : juce::jmax (1, (int) std::round ((float) totalWidth * slotWeight (items[i]) / totalWeight));

            auto cell = area.removeFromLeft (width).reduced (3);

            if (items[i] != nullptr)
                items[i]->setBounds (cell);
        }
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
            menu.addSeparator();
            menu.addItem (3, "(Any audio: the pitch is detected and one cycle per frame is taken across the file)", false);

            juce::Component::SafePointer<OscPage> safeMenu (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                                [safeMenu, tableId, tableChoiceOffset] (int result)
                                {
                                    if (safeMenu != nullptr && (result == 1 || result == 2))
                                        safeMenu->chooseTable (tableId, tableChoiceOffset,
                                                               result == 2 ? Wavetable::LoadMode::Resynthesize
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

    std::vector<juce::Component*> componentsOf (int i)
    {
        auto& osc = *controls[(size_t) i];
        auto& phys = *physical[(size_t) i];
        return { &osc.on, &osc.sampleTuned, &osc.sampleLoop, &osc.sampleReverse, &osc.mode, &osc.table,
                 &osc.excite, &osc.chord, &osc.ampEnv, &osc.warp, &osc.uniMode, &osc.spectral, &osc.frame,
                 &osc.level, &osc.pan, &osc.semi, &osc.fine, &osc.unison, &osc.detune, &osc.spread,
                 &osc.stringDecay, &osc.stringDamp, &osc.stringSustain, &osc.sampleStart, &osc.sampleEnd,
                 &osc.sampleFadeIn, &osc.sampleFadeOut, &osc.warpAmt, &osc.uniBlend, &osc.spectralAmt,
                 &osc.grainPosition, &osc.grainSize, &osc.grainDensity, &osc.grainSpray, &osc.grainPitch,
                 &osc.grainSpread, &osc.grainLive, &osc.warp2, &osc.pdEnv, &osc.warp2Amt, &osc.pdEnvAmt,
                 &osc.tune, &osc.ratio, &osc.fixedHz, &osc.egOut, &osc.trim, &osc.feedback, &osc.feedbackType,
                 &phys.stiffness, &phys.pickup, &phys.excitePos, &phys.hardness,
                 &phys.pickPos, &phys.bowPressure, &phys.bowSpeed, &phys.bridgeBuzz, &phys.fretRattle,
                 &phys.hammer, &phys.couple, &phys.damper, &phys.registerMap, &phys.slap,
                 &phys.epDistance, &phys.epPosition, &phys.fbGain, &phys.fbDistance, &waveDisplay (i), &loadButton (i), editButtons[(size_t) i].get(),
                 bounceButtons[(size_t) i].get() };
    }

    // The card shows the chosen oscillator only: its rows, its display and
    // its header's actions; the shared card its chosen section.
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

        const auto index = selected;
        const auto mode = getMode (index);
        auto& osc = *controls[(size_t) index];
        const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
        osc.stringSustain.setLabelText (juce::roundToInt (readFloat (prefix + "_excite")) == 10 ? "FEEDBACK" : "SUSTAIN");

        for (const auto& row : rowsFor (index))
            for (auto* item : row.second)
                if (item != nullptr)
                    item->setVisible (true);

        osc.on.setVisible (true);
        osc.mode.setVisible (true);
        // An operator on the Operator Env plays a plain cycle: no table
        // tools, no resampling (UI review 8, I8-15, S8-9, V8-16).
        const auto opEnv = OscRole::usesOperatorEg (processorRef, index);
        // An FM operator reads as one (UI review 9, I9-6): its mode says
        // Operator (as PLAY's strip does) and its table is its WAVE.
        {
            auto& modeBox = osc.mode.getComboBox();
            const juce::String modeText (opEnv ? "Operator" : "Wavetable");
            if (modeBox.getItemText (0) != modeText)
            {
                const auto wasSelected = modeBox.getSelectedId() == 1;
                modeBox.changeItemText (1, modeText);
                if (wasSelected)
                    modeBox.setText (modeText, juce::dontSendNotification);
            }
            osc.table.setLabelText (opEnv ? "WAVE" : "TABLE");
        }
        editButtons[(size_t) index]->setVisible (mode == 0 && ! opEnv);
        // LOAD loads what the mode plays: a wavetable (LOAD...), or a
        // sample or SF2 / SFZ multisample (LOAD, UI review 4, V30; review
        // 7, I7-24).
        loadButton (index).setVisible (mode == 0 && ! opEnv);
        sampleLoadButton.setVisible (mode == 2 || mode == 3);
        bounceButtons[(size_t) index]->setVisible (mode != 4 && ! opEnv);

        stringView.setVisible (mode == 1);
        waveDisplay (index).setVisible ((mode != 1 || isElectric (index)) && ! opEnv);
        waveDisplay (index).setCompact (mode == 1);
        waveDisplay (index).setSingleCycle (opEnv);

        if (mode == 1 && stringPrefix != prefix)
        {
            stringPrefix = prefix;
            stringView.setOscillator (prefix);
        }

        stringView.setColour (oscColour (index));

        shownRole = OscRole::describeLong (processorRef, index);
        getProperties().set ("caption", shownRole); // for the UI test
        shownRows = rowsKey (index);
        // Any mode can play the Operator Env (I7-20).
        opEnvButton.setVisible (opEnv);
        stringButton.setVisible (mode == 1);
        opEnvGraph.setVisible (false);
        opEnvGraph.setSource (prefix, oscColour (index));

        for (const auto& row : rowsFor (index))
            for (auto* item : row.second)
                if (item == &opEnvGraph)
                    opEnvGraph.setVisible (true);

        // The shared card: its chosen section's controls and its switch.
        for (int s = 0; s < numShared; ++s)
            for (auto* item : sharedItems (s))
                if (item != nullptr)
                    item->setVisible (false);

        for (auto* item : sharedItems (sharedSelected))
            if (item != nullptr)
                item->setVisible (true);

        subOscOn->setVisible (sharedSelected == sharedSubNoise);
        symOn.setVisible (sharedSelected == sharedSympathetic);

        if (sharedSelected == sharedSympathetic)
        {
            const auto symOnNow = readBool ("sym_on");
            const auto manual = readBool ("sym_manual");

            for (int n = 0; n < (int) symNotes.size(); ++n)
                symNotes[(size_t) n]->setVisible (manual);

            for (auto* control : { (juce::Component*) &symAmount, (juce::Component*) &symDecay,
                                   (juce::Component*) &symCount, (juce::Component*) &symManual })
                control->setAlpha (symOnNow ? 1.0f : IlanaTheme::dimmedAlpha);
        }

        updateTabs();
        resized();
        repaint();
        if (onModeChanged != nullptr)
            onModeChanged();
    }

    // M7.3: the Tine and Reed excites (7, 8) have their own controls.
    bool isElectric (int index) const
    {
        const auto excite = juce::roundToInt (readFloat (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_excite"));
        return excite == 7 || excite == 8;
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
        if (sharedFolded() != shownSharedFolded)
        {
            resized();
            repaint();
        }

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

        // Manual notes past STRINGS are not sounding.
        const auto stringCount = juce::roundToInt (readFloat ("sym_count"));
        for (int i = 0; i < (int) symNotes.size(); ++i)
            symNotes[(size_t) i]->setAlpha (i < stringCount ? 1.0f : IlanaTheme::dimmedAlpha);

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
                               &osc.uniMode, &osc.uniBlend, &osc.ampEnv,
                               &osc.warp2, &osc.warp2Amt, &osc.pdEnv, &osc.pdEnvAmt,
                               &osc.tune, &osc.ratio, &osc.fixedHz, &osc.egOut, &osc.trim, &osc.feedback,
                               &osc.feedbackType }, enabled);

            // An amount whose stage or envelope is Off does nothing: dim it.
            if (enabled)
            {
                osc.warp2Amt.setAlpha (readChoice (prefix + "_warp2") > 0 ? 1.0f : IlanaTheme::dimmedAlpha);
                osc.pdEnvAmt.setAlpha (readChoice (prefix + "_pd_env") > 0 ? 1.0f : IlanaTheme::dimmedAlpha);
            }

            const auto alpha = enabled ? 1.0f : 0.3f;
            waveDisplay (index).setAlpha (alpha);
            waveDisplay (index).setEnabled (enabled);
            loadButton (index).setEnabled (! chooserOpen && enabled);
            loadButton (index).setAlpha (alpha);
            editButtons[(size_t) index]->setEnabled (enabled);
            editButtons[(size_t) index]->setAlpha (alpha);

            if (index == selected)
                stringView.setAlpha (alpha);
        }

        effectRules.apply();
    }

    // The VOICE tab's quiet state: the mode, so the tab says what lives in it.
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
    // The physical oscillator's string, moving as it is played: one view,
    // following the chosen oscillator.
    PhysicalView stringView { processorRef };
    juce::String stringPrefix { "osc1" };

    std::array<std::unique_ptr<juce::TextButton>, OscillatorIds::count> loadButtons, editButtons, bounceButtons;
    IlanaSynthAudioProcessor::BounceRequest bounceRequest;
    int bouncingOsc = -1, bounceButtonWide = -1;
    DashedAddButton addButton { "+  ADD OSC", "+  ADD OSC" };
    juce::TextButton opEnvButton, stringButton, sampleLoadButton;
    // The Operator Env's graph, for the chosen operator (the FM card's).
    OperatorEnvDisplay opEnvGraph { processorRef };
    OscPicker oscTabs;
    StateTabs sharedTabs;
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
