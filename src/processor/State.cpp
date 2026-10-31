#include "ProcessorInternal.h"

juce::ValueTree IlanaSynthAudioProcessor::buildFullState()
{
    auto state = apvts.copyState();
    state.setProperty ("osc3Schema", 2, nullptr);
    state.setProperty ("grainPosSchema", 2, nullptr);
    state.setProperty ("lorenzSchema", 2, nullptr);
    state.setProperty ("stringSchema", 2, nullptr);
    state.setProperty ("redundantKnobsSchema", 2, nullptr);
    state.setProperty ("modRangeSchema", 2, nullptr);
    state.setProperty ("destSchema", 2, nullptr);
    state.setProperty ("tableSchema", 4, nullptr);
    state.setProperty ("exciterLevels", exciterLevels.load(), nullptr);

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        juce::StringArray values;

        {
            const juce::SpinLock::ScopedLockType lock (lfoShapeLock);

            for (int i = 0; i < lfoDrawSteps; ++i)
                values.add (juce::String (lfoCustom[(size_t) lfo][(size_t) i], 5));
        }

        state.setProperty ("lfo" + juce::String (lfo + 1) + "Draw", values.joinIntoString (","), nullptr);
        state.setProperty ("lfo" + juce::String (lfo + 1) + "Curve", getLfoCurve (lfo).toString(), nullptr);
    }

    // The tree holds every property of the state last loaded, so what the
    // patch no longer has is removed rather than saved again.
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto property = "mod" + juce::String (slot + 1) + "Remap";
        if (isModRemapOn (slot))
            state.setProperty (property, getModRemap (slot).toString(), nullptr);
        else
            state.removeProperty (property, nullptr);
    }

    for (int macro = 0; macro < Mod::numMacros; ++macro)
        state.setProperty ("macroCc" + juce::String (macro), macroCc[macro].load(), nullptr);

    // Written even when empty: a missing property means a state from before
    // the map was saved (it keeps the current one), an empty one clears it.
    state.setProperty ("midiCcMap", getParamCcMapText(), nullptr);
    {
        // Colour coding: "key:aarrggbb" for each section given a colour.
        juce::StringArray picked;
        for (int key = 0; key < SectionColours::count; ++key)
            if (const auto argb = getSectionColour (key); argb != 0)
                picked.add (juce::String (key) + ":" + juce::String::toHexString ((juce::int64) argb));
        if (picked.isEmpty())
            state.removeProperty ("sectionColours", nullptr);
        else
            state.setProperty ("sectionColours", picked.joinIntoString (","), nullptr);
    }
    state.setProperty ("oscRevealMask", revealMasks[(size_t) Module::Oscillator].load(), nullptr);
    state.setProperty ("envRevealMask", revealMasks[(size_t) Module::Envelope].load(), nullptr);
    state.setProperty ("lfoRevealMask", revealMasks[(size_t) Module::Lfo].load(), nullptr);

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        for (int i = 0; i < numSampleOscs; ++i)
        {
            const auto property = "osc" + juce::String (i + 1) + "SamplePath";
            // A file that wouldn't load leaves the slot empty but keeps its path.
            const auto& path = samplePaths[(size_t) i].isNotEmpty() ? samplePaths[(size_t) i]
                                                                    : (embeddedSamples[(size_t) i] == nullptr ? missingSamplePaths[(size_t) i] : juce::String());
            if (path.isNotEmpty())
                state.setProperty (property, path, nullptr);
            else
                state.removeProperty (property, nullptr);
        }

        // Step 36: a file's saved options (an edited zone map, an SF2 preset).
        state.removeChild (state.getChildWithName ("SampleOptions"), nullptr);
        juce::ValueTree options ("SampleOptions");
        for (int i = 0; i < numSampleOscs; ++i)
            if (samplePaths[(size_t) i].isNotEmpty() && sampleOptions[(size_t) i].isNotEmpty())
                if (const auto xml = juce::parseXML (sampleOptions[(size_t) i]))
                    if (auto tree = juce::ValueTree::fromXml (*xml); tree.isValid() && tree.getProperty ("path").toString() == samplePaths[(size_t) i])
                    {
                        tree.setProperty ("osc", i, nullptr);
                        options.appendChild (tree, nullptr);
                    }
        if (options.getNumChildren() > 0)
            state.appendChild (options, nullptr);

        for (int i = 0; i < numUserSlots; ++i)
        {
            const auto number = juce::String (i + 1);
            const auto kept = userTablePaths[(size_t) i].isEmpty() && userTableDocs[(size_t) i] == nullptr;
            const auto& path = userTablePaths[(size_t) i].isNotEmpty() ? userTablePaths[(size_t) i]
                                                                       : (kept ? missingTablePaths[(size_t) i] : juce::String());
            if (path.isNotEmpty())
            {
                state.setProperty ("userTablePath" + number, path, nullptr);
                state.setProperty ("userTableMode" + number, userTablePaths[(size_t) i].isNotEmpty() ? userTableModes[(size_t) i]
                                                                                                   : missingTableModes[i], nullptr);
            }
            else
            {
                state.removeProperty ("userTablePath" + number, nullptr);
                state.removeProperty ("userTableMode" + number, nullptr);
            }
        }
    }

    // M7.4: every edited or loaded patch table travels with the patch (see
    // WavetableDoc), stored once per slot however many oscillators play it.
    state.removeChild (state.getChildWithName ("Wavetables"), nullptr);
    juce::ValueTree tables ("Wavetables");
    for (int i = 0; i < numUserSlots; ++i)
    {
        std::shared_ptr<const WavetableDoc> doc;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            doc = userTableDocs[(size_t) i];
        }
        if (doc != nullptr)
        {
            auto table = doc->toValueTree();
            table.setProperty ("slot", i, nullptr);
            tables.appendChild (table, nullptr);
        }
    }
    if (tables.getNumChildren() > 0)
        state.appendChild (tables, nullptr);

    // M8.6: bounced samples live only in the patch.
    state.removeChild (state.getChildWithName ("Samples"), nullptr);
    juce::ValueTree samples ("Samples");
    for (int i = 0; i < numSampleOscs; ++i)
    {
        std::shared_ptr<SampleData> data;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            data = embeddedSamples[(size_t) i];
        }
        if (data != nullptr)
        {
            juce::ValueTree tree;
            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                if (encodedSamples[(size_t) i].first == data)
                    tree = encodedSamples[(size_t) i].second;
            }
            if (! tree.isValid())
            {
                tree = encodeSample (*data);
                const juce::SpinLock::ScopedLockType lock (stateLock);
                encodedSamples[(size_t) i] = { data, tree };
            }
            tree = tree.createCopy();
            tree.setProperty ("osc", i, nullptr);
            samples.appendChild (tree, nullptr);
        }
    }
    if (samples.getNumChildren() > 0)
        state.appendChild (samples, nullptr);

    tuningState.saveTo (state);
    clipState.saveTo (state);

    // The FX patcher's part (roadmap step 17): only when there is one, so a
    // patch that uses none saves as it always did.
    state.removeChild (state.getChildWithName (FxPatchData::patchTag), nullptr);
    if (const auto fx = getFxPatch(); ! fx.isDefault())
        state.appendChild (FxPatchData::toValueTree (fx), nullptr);

    // (The old DX7 mode's "Dx7" child is never written: a DX7 voice is
    // ordinary parameters now.)
    state.removeChild (state.getChildWithName ("Dx7"), nullptr);

    return state;
}

// Before v1.1, OSC 3 doubled as the sub: its table list began with four sub
// shapes (Shape/Sine/PWM/Analog Saw), it always played an octave or two
// down, and the noise followed its route. A patch that used it as a plain
// sub moves to the dedicated SUB (same table, same level, same octave); any
// other use stays on OSC 3 with the octave drop folded into SEMI and its
// table renumbered. Works on plain stored values, before they reach the
// parameters, so old table numbers aren't clamped by the new list.
void IlanaSynthAudioProcessor::migrateLegacyOsc3 (const std::function<float (const juce::String&, float)>& get,
                                                  const std::function<void (const juce::String&, float)>& set)
{
    const auto table = juce::roundToInt (get ("sub_table", 0.0f));
    const auto shape = juce::roundToInt (get ("sub_shape", 1.0f));
    const auto octave = juce::roundToInt (get ("sub_octave", 0.0f));
    const auto semi = juce::roundToInt (get ("sub_semi", 0.0f));
    const auto route = get ("sub_route", 0.0f);

    auto modulated = false;
    const Mod::Destination osc3Targets[] { Mod::Destination::SubLevel, Mod::Destination::SubPitch, Mod::Destination::SubFrame,
                                           Mod::Destination::SubSampleStart, Mod::Destination::SubSampleEnd,
                                           Mod::Destination::SubDetune, Mod::Destination::SubPan, Mod::Destination::SubWarp,
                                           Mod::Destination::SubBlend, Mod::Destination::SubSpread };

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);

        if (get (prefix + "_src", 0.0f) < 0.5f || std::abs (get (prefix + "_amt", 0.0f)) < 1.0e-6f)
            continue;

        const auto destination = juce::roundToInt (get (prefix + "_dst", 0.0f));

        for (const auto target : osc3Targets)
            modulated = modulated || destination == (int) target;
    }

    const auto plainSub = table <= 3
                          && juce::roundToInt (get ("sub_mode", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_unison", 1.0f)) <= 1
                          && juce::roundToInt (get ("sub_warp", 0.0f)) == 0
                          && juce::roundToInt (get ("sub_chord", 0.0f)) == 0
                          && semi == 0 && std::abs (get ("sub_fine", 0.0f)) < 0.01f
                          && std::abs (get ("sub_pan", 0.0f)) < 0.001f
                          && get ("sub_frame", 0.0f) < 0.001f
                          && ! modulated;

    const auto shapeForTable = table == 0 ? shape : (table == 1 ? 0 : (table == 2 ? 1 : 2));
    set ("subosc_route", route);

    if (plainSub)
    {
        set ("subosc_on", get ("sub_on", 1.0f));
        set ("subosc_level", get ("sub_level", 0.3f));
        set ("sub_shape", (float) shapeForTable);
        set ("sub_octave", (float) octave);
        set ("sub_on", 0.0f);
        set ("sub_table", 0.0f);
        return;
    }

    // OSC 3 was a real oscillator: keep it, in the new table numbering, at
    // the same pitch.
    const int shapeTables[] { 8, 6, 10 };
    set ("subosc_on", 0.0f);
    set ("sub_on", get ("sub_on", 1.0f));
    set ("sub_level", get ("sub_level", 0.3f));
    set ("sub_table", (float) (table >= 4 ? table - 4 : shapeTables[juce::jlimit (0, 2, shapeForTable)]));
    set ("sub_semi", (float) juce::jlimit (-24, 24, semi + (octave == 0 ? -12 : -24)));
}

juce::String IlanaSynthAudioProcessor::fmRouteId (int source, int target)
{
    static const char* ids[3][3] {
        { "fm_feedback", "fm_1to2", "fm_1to3" },
        { "fm_amount", "fm_fb2", "fm_2to3" },
        { "fm_3to1", "fm_3to2", "fm_fb3" }
    };

    if (source < 3 && target < 3)
        return ids[source][target];

    return source == target ? "fm_fb" + juce::String (source + 1)
                            : "fm_" + juce::String (source + 1) + "to" + juce::String (target + 1);
}

double IlanaSynthAudioProcessor::getSnappedRatio (int osc) const
{
    const auto& ids = operatorIds[(size_t) juce::jlimit (0, OscillatorIds::count - 1, osc)];
    const auto* ratio = apvts.getRawParameterValue (ids.ratio);
    const auto* snap = apvts.getRawParameterValue (ids.snap);
    return OscTuning::snapRatio (ratio != nullptr ? (double) ratio->load() : 1.0,
                                 snap != nullptr ? (int) snap->load() : 0);
}

void IlanaSynthAudioProcessor::applyFmAlgorithm (int index)
{
    if (! juce::isPositiveAndBelow (index, FmAlgorithms::count()))
        return;

    applyFmRouting (FmAlgorithms::all()[(size_t) index], 0);
}

void IlanaSynthAudioProcessor::applyDx7Algorithm (int number)
{
    if (number < 1 || number > 32)
        return;

    applyFmRouting (FmAlgorithms::dx7 (number), number);
}

void IlanaSynthAudioProcessor::applyFmRouting (const FmAlgorithms::Algorithm& algorithm, int dx7Number)
{
    // One undo step for the whole routing.
    undoManager.beginNewTransaction ("FM algorithm: " + juce::String (algorithm.name));

    const auto set = [this] (const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (value);

            if (std::abs (parameter->getValue() - normalised) > 1.0e-6f)
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (normalised);
                parameter->endChangeGesture();
            }
        }
    };
    const auto read = [this] (const juce::String& id)
    {
        const auto* value = apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : 0.0f;
    };

    // A patch on the Operator Env (a DX7 voice) keeps the DX7's terms: new
    // routes at 100% (each modulator's OUTPUT sets the depth) and new
    // feedback of the type its feedback already uses (DX7 if none); other
    // patches start routes gently and feedback Filtered (UI review 6, I6-11).
    // The feedback moves with the algorithm, as a DX7's FEEDBACK does: the
    // strongest loop the patch had (amount and type) goes to the new
    // algorithm's feedback operator (review 7, I7-9).
    auto operatorEnv = false;
    auto feedbackType = -1;
    auto feedbackAmount = 0.0f;
    for (int op = 0; op < OscillatorIds::count; ++op)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) op]);
        if (! isOscillatorShown (op))
            continue;
        operatorEnv = operatorEnv || juce::roundToInt (read (prefix + "_amp_env")) == OperatorEg::envelopeChoice;
        if (const auto amount = read (fmRouteId (op, op)); amount > 0.001f && amount > feedbackAmount)
        {
            feedbackAmount = amount;
            feedbackType = juce::roundToInt (read (prefix + "_fb_type"));
        }
    }
    if (feedbackType < 0)
        feedbackType = operatorEnv ? FmFeedback::Dx7 : FmFeedback::Filtered;

    for (int op = 0; op < algorithm.numOperators; ++op)
        if (! isOscillatorShown (op) || apvts.getRawParameterValue (oscCoreIds[(size_t) op].on)->load() < 0.5f)
            addOscillator (op);

    for (int source = 0; source < OscillatorIds::count; ++source)
        for (int target = 0; target < OscillatorIds::count; ++target)
        {
            const auto id = fmRouteId (source, target);
            const auto current = apvts.getRawParameterValue (id)->load();

            if (! FmAlgorithms::hasRoute (algorithm, source, target))
                set (id, 0.0f);
            else if (current < 0.001f)
            {
                if (source == target)
                    set (id, feedbackAmount > 0.001f ? feedbackAmount
                             : operatorEnv          ? FmAlgorithms::operatorEnvFeedbackAmount
                                                    : FmAlgorithms::defaultFeedbackAmount);
                else
                    set (id, operatorEnv ? FmAlgorithms::operatorEnvRouteAmount : FmAlgorithms::defaultRouteAmount);
                // Feedback the patch already had keeps its type.
                if (source == target)
                    set (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_fb_type", (float) feedbackType);
            }
        }

    for (int op = 0; op < algorithm.numOperators; ++op)
        set (oscCoreIds[(size_t) op].out, FmAlgorithms::isCarrier (algorithm, op) ? 1.0f : 0.0f);

    set (OperatorEg::dx7AlgorithmId, (float) dx7Number);
}

int IlanaSynthAudioProcessor::findMatchingDx7Algorithm() const
{
    // All six operators, the DX7's routes and carriers. Feedback may be
    // missing (a DX7 voice at feedback 0) but not elsewhere. Algorithms that
    // differ only in their feedback loop then match alike: the number the
    // routing was set from (a .syx voice, a click) breaks the tie.
    for (int op = 0; op < OscillatorIds::count; ++op)
        if (! isOscillatorShown (op) || apvts.getRawParameterValue (oscCoreIds[(size_t) op].on)->load() < 0.5f)
            return 0;

    const auto* stored = apvts.getRawParameterValue (OperatorEg::dx7AlgorithmId);
    const auto preferred = stored != nullptr ? juce::roundToInt (stored->load()) : 0;
    auto first = 0;

    for (int number = 1; number <= 32; ++number)
    {
        const auto& algorithm = FmAlgorithms::dx7 (number);
        auto matches = true;

        for (int source = 0; source < OscillatorIds::count && matches; ++source)
            for (int target = 0; target < OscillatorIds::count && matches; ++target)
            {
                const auto on = apvts.getRawParameterValue (fmRouteId (source, target))->load() > 0.001f;
                matches = source == target ? (! on || algorithm.feedbackOperator == source)
                                           : on == FmAlgorithms::hasRoute (algorithm, source, target);
            }

        for (int op = 0; op < OscillatorIds::count && matches; ++op)
            matches = (apvts.getRawParameterValue (oscCoreIds[(size_t) op].out)->load() > 0.5f)
                      == FmAlgorithms::isCarrier (algorithm, op);

        if (! matches)
            continue;
        if (number == preferred)
            return number;
        if (first == 0)
            first = number;
    }

    return first;
}

int IlanaSynthAudioProcessor::findMatchingFmAlgorithm() const
{
    // A routing can satisfy a smaller algorithm too (Pair + Sine is a 2-Op
    // Stack plus a carrier), so the match using the most operators wins.
    auto best = -1;

    for (int index = 0; index < FmAlgorithms::count(); ++index)
    {
        const auto& algorithm = FmAlgorithms::all()[(size_t) index];
        auto matches = true;

        for (int source = 0; source < OscillatorIds::count && matches; ++source)
            for (int target = 0; target < OscillatorIds::count && matches; ++target)
                matches = (apvts.getRawParameterValue (fmRouteId (source, target))->load() > 0.001f)
                          == FmAlgorithms::hasRoute (algorithm, source, target);

        for (int op = 0; op < OscillatorIds::count && matches; ++op)
        {
            const auto on = apvts.getRawParameterValue (oscCoreIds[(size_t) op].on)->load() > 0.5f;
            const auto out = apvts.getRawParameterValue (oscCoreIds[(size_t) op].out)->load() > 0.5f;

            if (op < algorithm.numOperators)
                matches = on && out == FmAlgorithms::isCarrier (algorithm, op);
        }

        if (matches && (best < 0 || algorithm.numOperators > FmAlgorithms::all()[(size_t) best].numOperators))
            best = index;
    }

    return best;
}

bool IlanaSynthAudioProcessor::isOscillatorShown (int index) const
{

    if (isRevealed (Module::Oscillator, index))
        return true;

    const auto* on = apvts.getRawParameterValue (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on");
    return on != nullptr && on->load() > 0.5f;
}

bool IlanaSynthAudioProcessor::isLfoShown (int index) const
{
    if (isRevealed (Module::Lfo, index))
        return true;

    // Routed by its A or its B output.
    const auto source = Mod::lfoSourceFor (index);
    const auto sourceB = Mod::lfoBSourceFor (index);

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = readModSlot (slot);

        if (routing.destination != 0 && (routing.source == source || routing.aux == source || routing.source == sourceB || routing.aux == sourceB))
            return true;
    }

    return false;
}

juce::Colour IlanaSynthAudioProcessor::lfoColour (int index)
{
    if (index >= 0 && index < SectionColours::numLfos)
        if (const auto picked = SectionColours::pickedFor (SectionColours::lfoKey (index)); picked != 0)
            return juce::Colour (picked);

    switch (index)
    {
        // LFO 1 is rose, not the UI accent's orange (a lit knob read as a
        // selected control).
        case 0: return juce::Colour (0xffff5c9a);
        case 1: return juce::Colour (0xff35c8ff);
        case 2: return juce::Colour (0xff6fe3c1);
        case 3: return juce::Colour (0xffdde35a);
        default: break;
    }

    // LFO 5-16: pastels at fixed hues clear of LFO 1-4, the main envelopes
    // and the macros' yellows (ENV 6-16 use deeper tones of similar hues).
    static constexpr float hues[] { 0.045f, 0.215f, 0.31f, 0.36f, 0.41f, 0.50f, 0.585f, 0.68f, 0.73f, 0.82f, 0.87f, 0.97f };
    return juce::Colour::fromHSV (hues[juce::jlimit (0, 11, index - 4)], 0.5f, 1.0f, 1.0f);
}

void IlanaSynthAudioProcessor::addOscillator (int index)
{
    setRevealed (Module::Oscillator, index, true);

    if (auto* on = apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on"))
        on->setValueNotifyingHost (1.0f);
}

void IlanaSynthAudioProcessor::removeOscillator (int index)
{
    if (auto* on = apvts.getParameter (juce::String (OscillatorIds::prefixes[(size_t) index]) + "_on"))
        on->setValueNotifyingHost (0.0f);

    setRevealed (Module::Oscillator, index, false);
}

int IlanaSynthAudioProcessor::migrateStringKnobs (const std::function<float (const juce::String&, float)>& get,
                                                 const std::function<void (const juce::String&, float)>& set)
{
    auto changed = 0;

    for (const auto* prefixText : OscillatorIds::prefixes)
    {
        const juce::String prefix (prefixText);
        const auto excite = juce::roundToInt (get (prefix + "_excite", 0.0f));
        const auto remap = [&] (const char* suffix, const std::function<float (float)>& convert)
        {
            const auto id = prefix + suffix;
            const auto old = get (id, 0.0f);

            if (old == 0.0f)
                return;

            const auto converted = juce::jlimit (0.0f, 1.0f, convert (old));

            if (converted != old)
            {
                set (id, converted);
                ++changed;
            }
        };

        // A position only mirrors past the middle of the string: the knob
        // now spans bridge to middle (the Piano already did, and its strike
        // keeps its scale; the bow clamped at the middle).
        const auto folded = [] (float v) { return 2.0f * juce::jmin (v, 1.0f - v); };
        const auto clamped = [] (float v) { return juce::jmin (1.0f, 2.0f * v); };

        if (excite != 7 && excite != 8 && excite != 9)
            remap ("_string_excite_pos", excite == 4 ? std::function<float (float)> (clamped) : std::function<float (float)> (folded));

        remap ("_string_pick_pos", folded);
        remap ("_string_pickup", folded);

        // The plucked string's stiffness (an allpass of coefficient -0.7 at
        // full) now reaches -0.95; the Piano's, the hammer's and the Modal
        // Piano's stiffness keep their own scale.
        if (excite != 4 && excite != 5 && excite != 9 && excite != 11)
            remap ("_string_stiffness", [] (float v) { return v * 0.7f / 0.95f; });

        // REGISTER's key scale doubled and its steps doubled again.
        remap ("_register", [] (float v) { return v * 0.25f; });
        // COUPLE grew 30-fold and takes the square, for finer low settings.
        remap ("_couple", [] (float v) { return std::sqrt (v / 30.0f); });
        // RATTLE lost its threshold and BUZZ's contact came down.
        remap ("_fret_rattle", [] (float v) { return v * 0.12f; });
        remap ("_bridge_buzz", [] (float v) { return v * 0.25f; });
    }

    return changed;
}

int IlanaSynthAudioProcessor::migrateOperatorModes (const std::function<float (const juce::String&, float)>& get,
                                                   const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;

    for (const auto* prefixText : OscillatorIds::prefixes)
    {
        const juce::String prefix (prefixText);

        if (juce::roundToInt (get (prefix + "_mode", 0.0f)) != OscMode::wavetable)
            continue;

        const auto tuned = juce::roundToInt (get (prefix + "_tune", (float) OscTuning::Semitones)) != OscTuning::Semitones;
        const auto onOperatorEg = juce::roundToInt (get (prefix + "_amp_env", 0.0f)) == OperatorEg::envelopeChoice;

        if (tuned || onOperatorEg)
        {
            set (prefix + "_mode", (float) OscMode::fmOperator);
            ++moved;
        }
    }

    return moved;
}

int IlanaSynthAudioProcessor::migrateFilterEnvAmounts (const std::function<float (const juce::String&, float)>& get,
                                                       const std::function<void (const juce::String&, float)>& set)
{
    using Mod::Destination;
    using Mod::Source;
    auto moved = 0;

    // The first slot with no source and no destination.
    const auto freeSlot = [&get]
    {
        for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        {
            const auto prefix = "mod" + juce::String (slot);
            if (juce::roundToInt (get (prefix + "_src", 0.0f)) == 0 && juce::roundToInt (get (prefix + "_dst", 0.0f)) == 0)
                return slot;
        }
        return 0;
    };
    const auto route = [&] (int slot, Source source, Destination destination, Source via, float amount)
    {
        const auto prefix = "mod" + juce::String (slot);
        set (prefix + "_src", (float) (int) source);
        set (prefix + "_dst", (float) (int) destination);
        set (prefix + "_aux", (float) (int) via);
        set (prefix + "_amt", amount);
    };

    // VELOCITY (filter_velocity) scaled the amount of both filters' envelopes; it now scales the FILTER ENV source
    // itself, as the other envelopes' VELOCITY knobs do. So when Filter 1's amount becomes one route, the knob stays
    // and carries the velocity share; with other FILTER ENV routes already in the patch (they were never scaled) or
    // no amount to carry, the share is a second route via VELOCITY instead and the knob goes to 0.
    const auto velocity = juce::jlimit (0.0f, 1.0f, get ("filter_velocity", 0.5f));
    auto otherFilterEnvRoutes = 0;
    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        if (juce::roundToInt (get ("mod" + juce::String (slot) + "_src", 0.0f)) == (int) Source::FilterEnv
            && juce::roundToInt (get ("mod" + juce::String (slot) + "_dst", 0.0f)) != 0)
            ++otherFilterEnvRoutes;

    auto keepVelocityKnob = false, leftInPlace = false;

    // (The old default of Filter 1's amount was 1.5 octaves: a patch that
    // never stored one still had it.) The routes are written in the old
    // scale, 6 octaves at 100 %; migrateModDepthRanges, run after this on
    // every path, takes them to the knob-wide one.
    for (const auto filter : { 1, 2 })
    {
        const juce::String prefix (filter == 1 ? "f1" : "f2");
        const auto octaves = get (prefix + "_env", filter == 1 ? 1.5f : 0.0f);

        if (std::abs (octaves) < 1.0e-6f)
            continue;

        const auto first = freeSlot();
        if (first == 0)
        {
            leftInPlace = true;
            continue;
        }

        const auto source = filter == 1 ? Source::FilterEnv : Source::FilterEnv2;
        const auto destination = filter == 1 ? Destination::Filter1Cutoff : Destination::Filter2Cutoff;

        if (filter == 1 && otherFilterEnvRoutes == 0)
        {
            route (first, source, destination, Source::None, octaves / 6.0f);
            keepVelocityKnob = true;
        }
        else
        {
            route (first, source, destination, Source::None, octaves * (1.0f - velocity) / 6.0f);

            if (velocity > 1.0e-6f)
            {
                const auto second = freeSlot();
                if (second == 0)
                {
                    // No room for the velocity share: give the first route the
                    // whole amount rather than lose the sweep.
                    set ("mod" + juce::String (first) + "_amt", octaves / 6.0f);
                }
                else
                    route (second, source, destination, Source::Velocity, octaves * velocity / 6.0f);
            }
        }

        set (prefix + "_env", 0.0f);
        ++moved;
    }

    // The knob keeps its value only where it was folded into the one route; anywhere else the routes carry the share
    // (or a left amount still wants it), so it must not scale the patch's other FILTER ENV routes.
    if (keepVelocityKnob || leftInPlace)
        set ("filter_velocity", velocity);
    else if (velocity > 1.0e-6f)
        set ("filter_velocity", 0.0f);

    return moved;
}

int IlanaSynthAudioProcessor::migrateEnvelopeTension (const std::function<float (const juce::String&, float)>& get,
                                                      const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;
    juce::StringArray prefixes { "amp", "fe", "f2e", "me", "e4" };
    for (int env = 6; env <= 16; ++env)
        prefixes.add ("env" + juce::String (env));

    for (const auto& prefix : prefixes)
    {
        const auto tension = get (prefix + "_curve", 0.0f);

        if (std::abs (tension) < 1.0e-6f)
            continue;

        // The envelope bent each segment by (TENSION + its own), clamped.
        for (const auto* segment : { "_acurve", "_dcurve", "_rcurve" })
            set (prefix + segment, juce::jlimit (-1.0f, 1.0f, get (prefix + segment, 0.0f) + tension));

        set (prefix + "_curve", 0.0f);
        ++moved;
    }

    return moved;
}

int IlanaSynthAudioProcessor::migrateWarpEnvelopes (const std::function<float (const juce::String&, float)>& get,
                                                    const std::function<void (const juce::String&, float)>& set)
{
    using Mod::Source;
    auto moved = 0;
    const auto destinations = Mod::getDestinationNames();

    for (int osc = 0; osc < OscillatorIds::count; ++osc)
    {
        const juce::String prefix (OscillatorIds::prefixes[(size_t) osc]);
        const auto choice = juce::roundToInt (get (prefix + "_pd_env", 0.0f));

        if (choice <= 0)
            continue;

        // ENV 1 to ENV 5 are the amp, filter, filter 2, mod and ENV 5 envelopes,
        // then ENV 6 to 16, then the MSEG.
        const auto index = choice - 1;
        const auto source = index == 0 ? Source::AmpEnv : index == 1 ? Source::FilterEnv : index == 2 ? Source::FilterEnv2
                          : index == 3 ? Source::ModEnv : index == 4 ? Source::Env4
                          : index <= 15 ? (Source) ((int) Source::Env6 + (index - 5)) : Source::Mseg;
        const auto amount = get (prefix + "_pd_env_amt", 1.0f);
        const auto name = "Osc" + juce::String (osc + 1) + " Warp";
        const auto warpDestination = destinations.indexOf (name);
        const auto warp2Destination = destinations.indexOf (name + " 2");
        auto placed = true;

        const auto freeSlot = [&get]
        {
            for (int slot = 1; slot <= Mod::maxSlots; ++slot)
            {
                const auto slotPrefix = "mod" + juce::String (slot);
                if (juce::roundToInt (get (slotPrefix + "_src", 0.0f)) == 0 && juce::roundToInt (get (slotPrefix + "_dst", 0.0f)) == 0)
                    return slot;
            }
            return 0;
        };
        const auto route = [&] (int destination)
        {
            const auto slot = freeSlot();
            if (slot == 0 || destination < 0)
                return false;
            const auto slotPrefix = "mod" + juce::String (slot);
            set (slotPrefix + "_src", (float) (int) source);
            set (slotPrefix + "_dst", (float) destination);
            set (slotPrefix + "_aux", 0.0f);
            set (slotPrefix + "_amt", amount);
            return true;
        };

        // The envelope opened both stages of the chain. A stage that is not
        // set plays nothing, so it gets no route.
        if (juce::roundToInt (get (prefix + "_warp", 0.0f)) != 0)
            placed = route (warpDestination) && placed;
        if (juce::roundToInt (get (prefix + "_warp2", 0.0f)) != 0)
            placed = route (warp2Destination) && placed;

        if (! placed)
            continue; // no free slot: the old settings stay, and still play

        set (prefix + "_pd_env", 0.0f);
        set (prefix + "_pd_env_amt", 1.0f);
        ++moved;
    }

    return moved;
}

void IlanaSynthAudioProcessor::migrateRedundantKnobs (const std::function<float (const juce::String&, float)>& get,
                                                      const std::function<void (const juce::String&, float)>& set)
{
    migrateFilterEnvAmounts (get, set);
    migrateEnvelopeTension (get, set);
    migrateWarpEnvelopes (get, set);
}

int IlanaSynthAudioProcessor::migrateModDepthRanges (const std::function<float (const juce::String&, float)>& get,
                                                     const std::function<void (const juce::String&, float)>& set)
{
    auto moved = 0;

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto prefix = "mod" + juce::String (slot);
        const auto factor = Mod::Range::legacyDepthFactor (juce::roundToInt (get (prefix + "_dst", 0.0f)));
        const auto depth = get (prefix + "_amt", 0.0f);

        if (factor != 1.0f && depth != 0.0f)
        {
            set (prefix + "_amt", depth * factor);
            ++moved;
        }
    }

    return moved;
}

void IlanaSynthAudioProcessor::migrateModDepthRanges()
{
    migrateModDepthRanges ([this] (const juce::String& id, float fallback)
                           {
                               if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                                   return ranged->convertFrom0to1 (ranged->getValue());
                               return fallback;
                           },
                           [this] (const juce::String& id, float value)
                           {
                               if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                                   ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
                           });
}

void IlanaSynthAudioProcessor::migrateOperatorModes()
{
    migrateOperatorModes ([this] (const juce::String& id, float fallback)
                          {
                              const auto* value = apvts.getRawParameterValue (id);
                              return value != nullptr ? value->load() : fallback;
                          },
                          [this] (const juce::String& id, float value)
                          {
                              if (auto* parameter = apvts.getParameter (id))
                                  parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                          });
}

void IlanaSynthAudioProcessor::setOscillatorMode (int index, int mode)
{
    if (index < 0 || index >= OscillatorIds::count || mode < 0 || mode >= OscMode::count)
        return;

    const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
    const auto set = [this, &prefix] (const char* suffix, float value)
    {
        if (auto* parameter = apvts.getParameter (prefix + suffix))
        {
            const auto normalised = parameter->convertTo0to1 (value);
            if (parameter->getValue() != normalised)
                parameter->setValueNotifyingHost (normalised);
        }
    };

    if (mode != OscMode::fmOperator)
    {
        set ("_tune", (float) OscTuning::Semitones);
        const auto* envelope = apvts.getRawParameterValue (prefix + "_amp_env");
        if (mode == OscMode::wavetable && envelope != nullptr && juce::roundToInt (envelope->load()) == OperatorEg::envelopeChoice)
            set ("_amp_env", 0.0f);
    }

    set ("_mode", (float) mode);
}

void IlanaSynthAudioProcessor::addOscillator (int index, int mode)
{
    if (index < 0 || index >= OscillatorIds::count)
        return;

    const juce::String prefix (OscillatorIds::prefixes[(size_t) index]);
    const auto set = [this, &prefix] (const char* suffix, float value)
    {
        if (auto* parameter = apvts.getParameter (prefix + suffix))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    if (mode == OscMode::fmOperator)
    {
        set ("_table", 8.0f); // Sine
        set ("_frame", 0.0f);
        set ("_tune", (float) OscTuning::Ratio);
        set ("_amp_env", (float) OperatorEg::envelopeChoice);
        set ("_mode", (float) OscMode::fmOperator);
    }
    else
        setOscillatorMode (index, mode);

    addOscillator (index);
}

// Builds the built-in wavetables the oscillators use now, so the first note of
// a loaded patch plays them (the audio thread would only ask for them).
void IlanaSynthAudioProcessor::ensureBuiltInTables()
{
    if (! juce::MessageManager::existsAndIsCurrentThread())
        return;

    const auto base = TableFactory::getNumFactoryTables() + numUserSlots;
    for (const auto& ids : oscCoreIds)
        if (const auto choice = (int) getRawParam (ids.table); choice >= base)
            BuiltInTables::build (choice - base);
}

void IlanaSynthAudioProcessor::applyFullState (const juce::ValueTree& stateIn)
{
    liveRetrigger = true;
    patchCut = true;
    ++patchGeneration; // a bounce still rendering belongs to the patch that started it
    auto state = stateIn.createCopy();
    if (state.hasProperty ("presetName")) // a factory preset renamed since the session was saved
        state.setProperty ("presetName", Presets::currentPresetName (state.getProperty ("presetName").toString()), nullptr);
    tuningState.loadFrom (state); // no Tuning child: 12-TET
    // No sectionColours: every section in its default colour.
    {
        SectionColours::Table picked {};
        for (const auto& entry : juce::StringArray::fromTokens (state.getProperty ("sectionColours").toString(), ",", ""))
            if (const auto key = entry.upToFirstOccurrenceOf (":", false, false).getIntValue();
                entry.containsChar (':') && key >= 0 && key < SectionColours::count)
                picked[(size_t) key] = (juce::uint32) entry.fromFirstOccurrenceOf (":", false, false).getHexValue64();
        for (int key = 0; key < SectionColours::count; ++key)
            if (getSectionColour (key) != picked[(size_t) key])
                setSectionColour (key, picked[(size_t) key]);
    }
    clipState.loadFrom (state);   // no Clips child: no clips
    // No FxPatch child: no instances, no patcher graph. Applied after the
    // parameters (below), which it is checked against.
    const auto fxPatchLoaded = FxPatchData::fromValueTree (state.getChildWithName (FxPatchData::patchTag));
    state.removeChild (state.getChildWithName (FxPatchData::patchTag), nullptr);
    {
        // Pre-mask M3b states saved a count of revealed envelopes.
        auto envMask = (int) state.getProperty ("envRevealMask", defaultRevealMask);
        if (! state.hasProperty ("envRevealMask") && state.hasProperty ("envRevealCount"))
            envMask = (1 << juce::jlimit (3, 16, (int) state.getProperty ("envRevealCount"))) - 1;
        revealMasks[(size_t) Module::Oscillator].store ((int) state.getProperty ("oscRevealMask", defaultRevealMask));
        revealMasks[(size_t) Module::Envelope].store (envMask);
        revealMasks[(size_t) Module::Lfo].store ((int) state.getProperty ("lfoRevealMask", defaultRevealMask));
        ++revealVersion;
    }

    if ((int) state.getProperty ("osc3Schema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);

            return juce::ValueTree();
        };

        migrateLegacyOsc3 ([&find] (const juce::String& id, float fallback)
                           {
                               const auto child = find (id);
                               return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                           },
                           [&find, &state] (const juce::String& id, float value)
                           {
                               auto child = find (id);

                               if (! child.isValid())
                               {
                                   child = juce::ValueTree ("PARAM");
                                   child.setProperty ("id", id, nullptr);
                                   state.appendChild (child, nullptr);
                               }

                               child.setProperty ("value", value, nullptr);
                           });

        state.setProperty ("osc3Schema", 2, nullptr);
    }

    // Step 45: the grain position became its own parameter. Before it, the
    // cloud sat at Sample Start, so an older patch takes that value.
    if ((int) state.getProperty ("grainPosSchema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);

            return juce::ValueTree();
        };

        for (int osc = 0; osc < OscillatorIds::count; ++osc)
        {
            const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]);
            const auto start = find (prefix + "_sample_start");

            if (! start.isValid() || ! start.hasProperty ("value"))
                continue;

            auto position = find (prefix + "_grain_pos");

            if (! position.isValid())
            {
                position = juce::ValueTree ("PARAM");
                position.setProperty ("id", prefix + "_grain_pos", nullptr);
                state.appendChild (position, nullptr);
            }

            position.setProperty ("value", start.getProperty ("value"), nullptr);
        }

        state.setProperty ("grainPosSchema", 2, nullptr);
    }

    // 2026-10-23: Lorenz loops once per cycle of RATE (it looped 1.6 to 2.5
    // times). A free-running Lorenz LFO saved before gets the faster RATE
    // that moves it as before; a synced one keeps its division and now
    // loops on it.
    if ((int) state.getProperty ("lorenzSchema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);

            return juce::ValueTree();
        };
        const auto valueOf = [&find] (const juce::String& id, float fallback)
        {
            const auto child = find (id);
            return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
        };

        for (int lfo = 1; lfo <= numLfos; ++lfo)
        {
            const auto prefix = "lfo" + juce::String (lfo);

            if (juce::roundToInt (valueOf (prefix + "_shape", 0.0f)) != LfoSimShapes::Lorenz || valueOf (prefix + "_sync", 0.0f) > 0.5f)
                continue;

            const auto rho = LfoSimInfo::value (LfoSimShapes::Lorenz, 1, valueOf (prefix + "_p2", 0.5f));
            auto rate = find (prefix + "_rate");

            if (! rate.isValid())
            {
                rate = juce::ValueTree ("PARAM");
                rate.setProperty ("id", prefix + "_rate", nullptr);
                state.appendChild (rate, nullptr);
            }

            const auto scaled = (double) valueOf (prefix + "_rate", 4.0f) * LfoSimInfo::lorenzLegacyRateScale (rho);
            rate.setProperty ("value", (float) juce::jlimit (0.01, 40.0, scaled), nullptr);
        }

        state.setProperty ("lorenzSchema", 2, nullptr);
    }

    // v1.1 added factory wavetables ahead of the four user slots, so older
    // saved choices of a user slot move up.
    if ((int) state.getProperty ("tableSchema", 1) < 2)
    {
        const auto added = 40 - 16; // the v1.1 tables (the v1.3 ones are moved below)

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (id == "osc1_table" || id == "osc2_table" || id == "sub_table")
            {
                const auto table = juce::roundToInt ((float) child.getProperty ("value"));

                if (table >= 16)
                    child.setProperty ("value", table + added, nullptr);
            }
        }

        state.setProperty ("tableSchema", 2, nullptr);
    }

    // v1.3 (M10) added 80 factory tables ahead of the User tables, for all
    // six oscillators.
    if ((int) state.getProperty ("tableSchema", 1) < 3)
    {
        const auto added = 120 - 40; // the factory count then (step 19's tables are moved below)

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            for (const auto* prefix : OscillatorIds::prefixes)
                if (id == juce::String (prefix) + "_table")
                {
                    const auto table = juce::roundToInt ((float) child.getProperty ("value"));
                    if (table >= 40)
                        child.setProperty ("value", table + added, nullptr);
                }
        }

        state.setProperty ("tableSchema", 3, nullptr);
    }

    // Step 19 appended 14 factory tables (Arca Tear ... Noise Choir) ahead of
    // the User tables, for all six oscillators.
    if ((int) state.getProperty ("tableSchema", 1) < 4)
    {
        const auto added = 134 - 120; // the factory count then (a fixed number: later tables get their own schema step)

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            for (const auto* prefix : OscillatorIds::prefixes)
                if (id == juce::String (prefix) + "_table")
                {
                    const auto table = juce::roundToInt ((float) child.getProperty ("value"));
                    if (table >= 120)
                        child.setProperty ("value", table + added, nullptr);
                }
        }

        state.setProperty ("tableSchema", 4, nullptr);
    }

    // v1.1 added explicit (FM) destinations ahead of the parameter
    // destinations, so older saved routings to those move up.
    if ((int) state.getProperty ("destSchema", 1) < 2)
    {
        const auto added = Mod::numExplicitDestinations - Mod::explicitDestinationsV10;

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            const auto id = child.getProperty ("id").toString();

            if (id.startsWith ("mod") && id.endsWith ("_dst"))
            {
                const auto destination = juce::roundToInt ((float) child.getProperty ("value"));

                if (destination >= Mod::explicitDestinationsV10)
                    child.setProperty ("value", destination + added, nullptr);
            }
        }

        state.setProperty ("destSchema", 2, nullptr);
    }

    const auto parseDraw = [this] (int lfoIndex, const juce::String& text)
    {
        const auto tokens = juce::StringArray::fromTokens (text, ",", "");

        // A damaged file can hold nan, which would reach the audio thread.
        for (int i = 0; i < lfoDrawSteps && i < tokens.size(); ++i)
            if (const auto value = tokens[i].getFloatValue(); std::isfinite (value))
                setLfoCustomPoint (lfoIndex, i, value);
    };

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto property = "lfo" + juce::String (lfo + 1) + "Draw";
        const auto text = state.getProperty (property).toString();

        // The default sine first, as a factory preset leaves it: a state
        // without a drawing (or a cut short one) keeps nothing of the last patch's.
        for (int i = 0; i < lfoDrawSteps; ++i)
            setLfoCustomPoint (lfo, i, (float) std::sin (juce::MathConstants<double>::twoPi * (double) i / (double) lfoDrawSteps));

        if (text.isNotEmpty())
            parseDraw (lfo, text);

        const auto curveText = state.getProperty ("lfo" + juce::String (lfo + 1) + "Curve").toString();
        setLfoCurve (lfo, curveText.isNotEmpty() ? LfoCurve::fromString (curveText) : LfoCurve::preset (0));
    }

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto remapText = state.getProperty ("mod" + juce::String (slot + 1) + "Remap").toString();
        if (remapText.isNotEmpty())
            setModRemap (slot, LfoCurve::fromString (remapText));
        else if (isModRemapOn (slot))
            resetModRemap (slot);
    }

    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        const auto property = "macroCc" + juce::String (macro);

        if (state.hasProperty (property))
            macroCc[macro].store (juce::jlimit (0, 127, (int) state.getProperty (property)));
    }

    // Learned parameter CCs; older states have none and keep the current map.
    if (state.hasProperty ("midiCcMap"))
        setParamCcMapText (state.getProperty ("midiCcMap").toString());

    auto asyncNeeded = false;

    // M7.4 patch tables: recipe, then embedded frames, then the file (see
    // WavetableDoc). A slot the state does not mention goes back to its
    // default, unless an old-style path below reloads it.
    std::array<bool, (size_t) numUserSlots> restoredTables {};
    {
        const auto tables = state.getChildWithName ("Wavetables");
        juce::StringArray notices;
        for (int child = 0; child < tables.getNumChildren(); ++child)
        {
            const auto tree = tables.getChild (child);
            const auto slot = (int) tree.getProperty ("slot", -1);
            if (slot < 0 || slot >= numUserSlots || restoredTables[(size_t) slot])
                continue;
            WavetableDoc doc;
            juce::String notice;
            if (WavetableDoc::fromValueTree (tree, doc, notice))
                setUserTable (slot, doc);
            else
                resetUserTableToDefault (slot);
            if (notice.isNotEmpty())
                notices.add (notice);
            restoredTables[(size_t) slot] = true;
        }
        state.removeChild (tables, nullptr);

        for (int i = 0; i < numUserSlots; ++i)
            if (! restoredTables[(size_t) i] && isUserSlotEdited (i)
                && state.getProperty ("userTablePath" + juce::String (i + 1)).toString().isEmpty())
                resetUserTableToDefault (i);

        if (! notices.isEmpty())
        {
            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                tableNotice = notices.joinIntoString ("\n");
            }
            ++tableNoticeVersion;
        }
    }

    // M8.6 embedded samples come back at once; they win over a path.
    std::array<bool, (size_t) numSampleOscs> restoredSamples {};
    {
        const auto samples = state.getChildWithName ("Samples");
        for (int child = 0; child < samples.getNumChildren(); ++child)
        {
            const auto tree = samples.getChild (child);
            const auto osc = (int) tree.getProperty ("osc", -1);
            if (osc < 0 || osc >= numSampleOscs || restoredSamples[(size_t) osc])
                continue;
            if (auto data = decodeSample (tree))
            {
                setUserSample (osc, std::move (data), {});
                restoredSamples[(size_t) osc] = true;
            }
        }
        state.removeChild (samples, nullptr);

        // A patch saved in the old DX7 mode (2026-10-01) carries its DX7
        // voice as a "Dx7" child: it becomes the Operator EG's parameters,
        // with every oscillator on the Operator EG and Filtered feedback as
        // the DX7 type (DX7 mode played Filtered as the DX7's average), which
        // is what that mode played.
        {
            const auto dx7 = state.getChildWithName ("Dx7");
            juce::MemoryOutputStream bytes;
            if (dx7.isValid() && juce::Base64::convertFromBase64 (bytes, dx7.getProperty ("voice").toString())
                && bytes.getDataSize() == (size_t) Dx7::voiceBytes)
            {
                // Range-checked: a damaged state can't index past a table.
                Dx7::Voice voice {};
                std::memcpy (voice.data(), bytes.getData(), voice.size());
                Dx7::clampRanges (voice);

                const auto paramNode = [&state] (const juce::String& id)
                {
                    for (int i = 0; i < state.getNumChildren(); ++i)
                        if (state.getChild (i).hasType ("PARAM") && state.getChild (i).getProperty ("id").toString() == id)
                            return state.getChild (i);
                    juce::ValueTree child ("PARAM");
                    child.setProperty ("id", id, nullptr);
                    state.appendChild (child, nullptr);
                    return child;
                };
                for (const auto& value : Presets::Dx7Import::egValues (voice))
                    paramNode (value.id).setProperty ("value", value.value, nullptr);
                for (const auto* prefix : OscillatorIds::prefixes)
                {
                    paramNode (juce::String (prefix) + "_amp_env").setProperty ("value", (float) OperatorEg::envelopeChoice, nullptr);
                    auto feedback = paramNode (juce::String (prefix) + "_fb_type");
                    if ((int) feedback.getProperty ("value", 0) == FmFeedback::Filtered)
                        feedback.setProperty ("value", (float) FmFeedback::Dx7, nullptr);
                }
            }
            state.removeChild (dx7, nullptr);
        }

        for (int i = 0; i < numSampleOscs; ++i)
            if (! restoredSamples[(size_t) i] && isSampleEmbedded (i))
                setUserSample (i, nullptr, {});
    }

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        // Step 36: the files' saved options come back with their paths; a
        // file already loaded reloads when its options changed.
        std::array<juce::String, (size_t) numSampleOscs> savedOptions;
        {
            const auto options = state.getChildWithName ("SampleOptions");
            for (const auto& tree : options)
            {
                const auto osc = (int) tree.getProperty ("osc", -1);
                if (osc >= 0 && osc < numSampleOscs)
                {
                    auto copy = tree.createCopy();
                    copy.removeProperty ("osc", nullptr);
                    savedOptions[(size_t) osc] = copy.toXmlString();
                }
            }
            state.removeChild (options, nullptr);
        }

        // Whatever an earlier load left queued is stale: each slot below either
        // queues its own change or is cancelled (same path as the live one).
        for (int i = 0; i < numSampleOscs; ++i)
        {
            const auto path = state.getProperty ("osc" + juce::String (i + 1) + "SamplePath").toString();
            missingSamplePaths[(size_t) i].clear();
            const auto optionsChanged = savedOptions[(size_t) i] != sampleOptions[(size_t) i];
            sampleOptions[(size_t) i] = savedOptions[(size_t) i];

            if (restoredSamples[(size_t) i])
            {
                pendingSamplePaths[(size_t) i].clear();
                pendingSampleClear[(size_t) i] = false;
            }
            else if (path.isNotEmpty() && (path != samplePaths[(size_t) i] || optionsChanged))
            {
                pendingSamplePaths[(size_t) i] = path;
                pendingSampleClear[(size_t) i] = false;
                samplesReloadPending = true;
                asyncNeeded = true;
            }
            else if (path.isEmpty() && samplePaths[(size_t) i].isNotEmpty())
            {
                pendingSamplePaths[(size_t) i].clear();
                pendingSampleClear[(size_t) i] = true;
                asyncNeeded = true;
            }
            else
            {
                pendingSamplePaths[(size_t) i].clear();
                pendingSampleClear[(size_t) i] = false;
            }
        }

        for (int i = 0; i < numUserSlots; ++i)
        {
            missingTablePaths[(size_t) i].clear();

            if (restoredTables[(size_t) i])
            {
                pendingUserTablePaths[(size_t) i].clear();
                pendingUserTableClear[(size_t) i] = false;
                continue;
            }

            const auto path = state.getProperty ("userTablePath" + juce::String (i + 1)).toString();
            const auto mode = (int) state.getProperty ("userTableMode" + juce::String (i + 1), 0);

            if (path.isNotEmpty() && (path != userTablePaths[(size_t) i] || mode != userTableModes[(size_t) i]))
            {
                pendingUserTablePaths[(size_t) i] = path;
                pendingUserTableModes[(size_t) i] = mode;
                pendingUserTableClear[(size_t) i] = false;
                userTablesReloadPending = true;
                asyncNeeded = true;
            }
            else if (path.isEmpty() && userTablePaths[(size_t) i].isNotEmpty())
            {
                pendingUserTablePaths[(size_t) i].clear();
                pendingUserTableClear[(size_t) i] = true;
                asyncNeeded = true;
            }
            else
            {
                pendingUserTablePaths[(size_t) i].clear();
                pendingUserTableClear[(size_t) i] = false;
            }
        }
    }

    if (asyncNeeded)
        triggerAsyncUpdate();

    // JUCE replaceState retains current values for absent parameters. Fill every
    // omitted parameter from its declared default, including future additions.
    // The saved IDs are collected once, so loading stays linear in the
    // parameter count.
    std::set<juce::String> savedIds;

    for (int child = 0; child < state.getNumChildren(); ++child)
    {
        auto node = state.getChild (child);
        const auto id = node.getProperty ("id").toString();
        savedIds.insert (id);

        // A damaged state can hold "nan" or "inf": NaN survives the
        // parameters' clamping and reached the voices as an index (a crash).
        if (node.hasProperty ("value") && ! std::isfinite ((double) node.getProperty ("value")))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                node.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
    }

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            const auto id = static_cast<juce::AudioProcessorParameterWithID*> (ranged)->paramID;
            const auto found = savedIds.count (id) > 0;

            if (! found)
            {
                juce::ValueTree missingParameter ("PARAM");
                missingParameter.setProperty ("id", id, nullptr);
                missingParameter.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
                state.appendChild (missingParameter, nullptr);
            }
        }

    // A patch saved before the string knobs were rescaled keeps its sound.
    if ((int) state.getProperty ("stringSchema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);
            return juce::ValueTree();
        };
        migrateStringKnobs ([&find] (const juce::String& id, float fallback)
                            {
                                const auto child = find (id);
                                return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                            },
                            [&find] (const juce::String& id, float value)
                            {
                                if (auto child = find (id); child.isValid())
                                    child.setProperty ("value", value, nullptr);
                            });
        state.setProperty ("stringSchema", 2, nullptr);
    }

    // A patch saved with an ENV AMOUNT on Filter 1 or 2, an envelope TENSION or
    // a WARP ENV carries them as matrix routes and curves (the knobs are gone).
    if ((int) state.getProperty ("redundantKnobsSchema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);
            return juce::ValueTree();
        };
        migrateRedundantKnobs ([&find] (const juce::String& id, float fallback)
                                 {
                                     const auto child = find (id);
                                     return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                                 },
                                 [&find] (const juce::String& id, float value)
                                 {
                                     if (auto child = find (id); child.isValid())
                                         child.setProperty ("value", value, nullptr);
                                 });
        state.setProperty ("redundantKnobsSchema", 2, nullptr);
    }

    // A patch saved before 100 % meant the knob's whole range keeps its
    // routes' sound (after the step above, whose cutoff routes are old-scale).
    if ((int) state.getProperty ("modRangeSchema", 1) < 2)
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);
            return juce::ValueTree();
        };
        migrateModDepthRanges ([&find] (const juce::String& id, float fallback)
                               {
                                   const auto child = find (id);
                                   return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                               },
                               [&find] (const juce::String& id, float value)
                               {
                                   if (auto child = find (id); child.isValid())
                                       child.setProperty ("value", value, nullptr);
                               });
        state.setProperty ("modRangeSchema", 2, nullptr);
    }

    // An operator saved before the FM / DX7 type loads as one (it renders
    // the same: the type only picks its card).
    {
        const auto find = [&state] (const juce::String& id)
        {
            for (int i = 0; i < state.getNumChildren(); ++i)
                if (state.getChild (i).getProperty ("id").toString() == id)
                    return state.getChild (i);
            return juce::ValueTree();
        };
        migrateOperatorModes ([&find] (const juce::String& id, float fallback)
                              {
                                  const auto child = find (id);
                                  return child.isValid() && child.hasProperty ("value") ? (float) child.getProperty ("value") : fallback;
                              },
                              [&find] (const juce::String& id, float value)
                              {
                                  if (auto child = find (id); child.isValid())
                                      child.setProperty ("value", value, nullptr);
                              });
    }

    apvts.replaceState (state);
    ensureBuiltInTables();
    // replaceState emptied the undo history: a gesture still open around this
    // load would add a step that restores only the data (a hybrid patch).
    pendingEditData.reset();
    setFxPatch (fxPatchLoaded);
    // An old MSEG module becomes an LFO drawn the same (UI review 9, I9-2).
    moveLegacyMsegToLfo();
    updateExciterLevelMatch (state.hasProperty ("exciterLevels") ? (int) state.getProperty ("exciterLevels") : 0);

    // On the message thread the file loads queued above run now, so the patch
    // is whole when this returns (and what the editor remembers as loaded
    // does not change a moment later).
    if (asyncNeeded && juce::MessageManager::existsAndIsCurrentThread())
        handleUpdateNowIfNeeded();
}

// A patch keeps the exciter trims of the generation it was saved with; it
// moves to the current one only if it plays no exciter whose trim came
// later, so nothing that was already made changes its level. (A session
// saved before the Modal Piano's +20 dB, 2026-10-30, saved 1: its Modal
// Piano stays where it was heard.) Factory presets load as 0.
void IlanaSynthAudioProcessor::updateExciterLevelMatch (int savedLevels)
{
    const auto levels = juce::jlimit (0, Voice::currentExciterLevels, savedLevels);
    auto usesUntrimmed = false;

    for (const auto* prefix : OscillatorIds::prefixes)
    {
        const auto* mode = apvts.getRawParameterValue (juce::String (prefix) + "_mode");
        const auto* excite = apvts.getRawParameterValue (juce::String (prefix) + "_excite");
        if (mode != nullptr && excite != nullptr && juce::roundToInt (mode->load()) == 1
            && Voice::exciterTrim (juce::roundToInt (excite->load())) != 1.0f
            && Voice::exciterTrimLevel (juce::roundToInt (excite->load())) > levels)
            usesUntrimmed = true;
    }

    exciterLevels = usesUntrimmed ? levels : Voice::currentExciterLevels;
}

bool IlanaSynthAudioProcessor::loadTuningScale (const juce::String& sclText, juce::String& error)
{
    ++dataEpoch;
    if (! tuningState.loadScale (sclText, error))
        return false;
    if (auto* parameter = apvts.getParameter ("tuning_on"))
        parameter->setValueNotifyingHost (1.0f);
    return true;
}

bool IlanaSynthAudioProcessor::loadTuningMapping (const juce::String& kbmText, juce::String& error)
{
    ++dataEpoch;
    if (! tuningState.loadMapping (kbmText, error))
        return false;
    if (auto* parameter = apvts.getParameter ("tuning_on"); parameter != nullptr && tuningState.hasScale())
        parameter->setValueNotifyingHost (1.0f);
    return true;
}

void IlanaSynthAudioProcessor::resetTuning()
{
    ++dataEpoch;
    tuningState.reset();
    if (auto* parameter = apvts.getParameter ("tuning_on"))
        parameter->setValueNotifyingHost (0.0f);
}

void IlanaSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = buildFullState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void IlanaSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ++dataEpoch; // what the editor draws changes

    // getXmlFromBinary decodes the text as UTF-8 without checking it, and a
    // damaged state (the fuzz test's bit flips) read past the end and
    // crashed. Same header as copyXmlToBinary: magic, length, text.
    if (data != nullptr && sizeInBytes > 8)
    {
        const auto* bytes = static_cast<const char*> (data);
        if (juce::ByteOrder::littleEndianInt (bytes) == 0x21324356)
        {
            const auto length = (int) juce::ByteOrder::littleEndianInt (bytes + 4);
            if (length < 0 || length > sizeInBytes - 8 || ! juce::CharPointer_UTF8::isValidString (bytes + 8, length))
                return;
        }
    }

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    applyFullState (juce::ValueTree::fromXml (*xml));
    loadedPresetFile = juce::File(); // the host's session, not a preset file
}

// ---- Undo for gestures and for data that isn't a parameter ----------------

IlanaSynthAudioProcessor::PatchData IlanaSynthAudioProcessor::capturePatchData() const
{
    PatchData data;

    {
        const juce::SpinLock::ScopedLockType lock (lfoShapeLock);
        data.draws = lfoCustom;
        data.curves = lfoCurves;
    }

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
        data.remaps[(size_t) slot] = getModRemap (slot);

    data.clips = clipState.getAll();
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        data.fx = fxPatch;
        data.tables = userTableDocs;
        data.embedded = embeddedSamples;
        data.samplePaths = samplePaths;
        data.sampleOptions = sampleOptions;
    }
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        for (size_t i = 0; i < data.liveSamples.size() && i < sampleSlots.size(); ++i)
            data.liveSamples[i] = sampleSlots[i];
    }

    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        data.macroNames[(size_t) macro] = apvts.state.getProperty ("macroName" + juce::String (macro + 1)).toString();
        data.macroCcs[(size_t) macro] = macroCc[macro].load();
    }

    for (size_t kind = 0; kind < data.reveal.size(); ++kind)
        data.reveal[kind] = revealMasks[kind].load();

    data.tuningScale = tuningState.getScaleText();
    data.tuningMapping = tuningState.getMappingText();
    return data;
}

namespace
{
bool sameCurve (const LfoCurve& a, const LfoCurve& b)
{
    if (a.points.size() != b.points.size())
        return false;

    for (size_t i = 0; i < a.points.size(); ++i)
        if (a.points[i].x != b.points[i].x || a.points[i].y != b.points[i].y || a.points[i].tension != b.points[i].tension)
            return false;

    return true;
}

bool sameClips (const std::shared_ptr<const ClipState::Clips>& a, const std::shared_ptr<const ClipState::Clips>& b)
{
    if (a == b)
        return true;

    if (a == nullptr || b == nullptr)
        return false;

    for (size_t i = 0; i < a->size(); ++i)
        if (! ClipState::sameClip ((*a)[i], (*b)[i]))
            return false;

    return true;
}
} // namespace

bool IlanaSynthAudioProcessor::samePatchData (const PatchData& a, const PatchData& b)
{
    if (a.draws != b.draws || ! sameClips (a.clips, b.clips))
        return false;

    if (a.tables != b.tables || a.embedded != b.embedded || a.samplePaths != b.samplePaths || a.sampleOptions != b.sampleOptions
        || a.macroNames != b.macroNames || a.macroCcs != b.macroCcs || a.reveal != b.reveal
        || a.tuningScale != b.tuningScale || a.tuningMapping != b.tuningMapping)
        return false;

    if (a.fx != b.fx && (a.fx == nullptr || b.fx == nullptr || ! FxPatchData::samePatch (*a.fx, *b.fx)))
        return false;

    for (size_t i = 0; i < a.curves.size(); ++i)
        if (! sameCurve (a.curves[i], b.curves[i]))
            return false;

    for (size_t i = 0; i < a.remaps.size(); ++i)
        if (! sameCurve (a.remaps[i], b.remaps[i]))
            return false;

    return true;
}

void IlanaSynthAudioProcessor::restorePatchData (const PatchData& data, const PatchData& reference)
{
    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        if (data.draws[(size_t) lfo] != reference.draws[(size_t) lfo])
            for (int i = 0; i < lfoDrawSteps; ++i)
                setLfoCustomPoint (lfo, i, data.draws[(size_t) lfo][(size_t) i]);

        if (! sameCurve (data.curves[(size_t) lfo], reference.curves[(size_t) lfo]))
            setLfoCurve (lfo, data.curves[(size_t) lfo]);
    }

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
        if (! sameCurve (data.remaps[(size_t) slot], reference.remaps[(size_t) slot]))
            setModRemap (slot, data.remaps[(size_t) slot]);

    if (! sameClips (data.clips, reference.clips))
    {
        clipState.setAll (data.clips);
        clipsEdited();
    }

    if (data.fx != nullptr && (reference.fx == nullptr || ! FxPatchData::samePatch (*data.fx, *reference.fx)))
        setFxPatch (*data.fx);

    for (int slot = 0; slot < numUserSlots; ++slot)
        if (data.tables[(size_t) slot] != reference.tables[(size_t) slot])
        {
            if (data.tables[(size_t) slot] != nullptr)
                setUserTable (slot, *data.tables[(size_t) slot]);
            else
                resetUserTableToDefault (slot);
        }

    for (int osc = 0; osc < numSampleOscs; ++osc)
    {
        const auto i = (size_t) osc;

        if (data.embedded[i] == reference.embedded[i] && data.samplePaths[i] == reference.samplePaths[i]
            && data.sampleOptions[i] == reference.sampleOptions[i])
            continue;

        auto restored = false;
        // Step 36: the same file with other options (a zone map edit): the
        // sample as it played then comes back, without reading the file.
        if (data.embedded[i] == nullptr && data.samplePaths[i].isNotEmpty() && data.liveSamples[i] != nullptr)
        {
            setUserSample (osc, data.liveSamples[i], data.samplePaths[i]);
            setSampleOptions (osc, data.sampleOptions[i]);
            continue;
        }
        setSampleOptions (osc, data.sampleOptions[i]);

        if (data.embedded[i] != nullptr)
        {
            setUserSample (osc, data.embedded[i], {});
            restored = true;
        }
        else if (data.samplePaths[i].isNotEmpty())
        {
            const juce::File file (data.samplePaths[i]);
            restored = file.existsAsFile() && loadUserSample (osc, file);
        }

        if (! restored)
            setUserSample (osc, nullptr, {});
    }

    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        const auto i = (size_t) macro;

        if (data.macroNames[i] != reference.macroNames[i])
            setMacroName (macro, data.macroNames[i]);

        if (data.macroCcs[i] != reference.macroCcs[i])
            macroCc[macro].store (data.macroCcs[i]);
    }

    if (data.reveal != reference.reveal)
    {
        for (size_t kind = 0; kind < data.reveal.size(); ++kind)
            revealMasks[kind].store (data.reveal[kind]);
        ++revealVersion;
    }

    if (data.tuningScale != reference.tuningScale || data.tuningMapping != reference.tuningMapping)
    {
        juce::String error;
        ++dataEpoch;
        tuningState.reset();
        if (data.tuningScale.isNotEmpty())
        {
            tuningState.loadScale (data.tuningScale, error);
            if (data.tuningMapping.isNotEmpty())
                tuningState.loadMapping (data.tuningMapping, error);
        }
        else if (data.tuningMapping.isNotEmpty())
            tuningState.loadMapping (data.tuningMapping, error);
    }
}

// One gesture's change to the data: undo puts back what it was before,
// redo what it was after, each only where the gesture changed it.
struct IlanaSynthAudioProcessor::PatchDataEdit : public juce::UndoableAction
{
    PatchDataEdit (IlanaSynthAudioProcessor& p, PatchData b, PatchData a)
        : processor (p), before (std::move (b)), after (std::move (a)) {}

    bool perform() override
    {
        // The first perform is the gesture itself, already done.
        if (std::exchange (done, true))
            processor.restorePatchData (after, before);
        return true;
    }

    bool undo() override
    {
        processor.restorePatchData (before, after);
        return true;
    }

    int getSizeInUnits() override { return 100; }

    IlanaSynthAudioProcessor& processor;
    PatchData before, after;
    bool done = false;
};

void IlanaSynthAudioProcessor::beginEdit (const juce::String& name)
{
    if (pendingEditData.has_value()) // a gesture that never saw its mouse-up
        endEdit();

    // Parameter changes still waiting for the tree go to the step before
    // (copyState flushes them; the tree otherwise catches up on a timer).
    apvts.copyState();
    undoManager.beginNewTransaction (name);
    pendingEditData = capturePatchData();
}

void IlanaSynthAudioProcessor::endEdit()
{
    if (! pendingEditData.has_value())
        return;

    apvts.copyState(); // this gesture's parameter changes join its step now
    auto before = std::move (*pendingEditData);
    pendingEditData.reset();
    auto after = capturePatchData();

    if (! samePatchData (before, after))
        undoManager.perform (new PatchDataEdit (*this, std::move (before), std::move (after)));
}

juce::int64 IlanaSynthAudioProcessor::getPatchDataHash() const
{
    // The epoch moves with the setters; the macro and learned CCs are also
    // set from the MIDI thread, so their sum is part of the key.
    auto epoch = (juce::uint64) dataEpoch.load();
    for (int macro = 0; macro < Mod::numMacros; ++macro)
        epoch = epoch * 31 + (juce::uint64) macroCc[macro].load();
    for (const auto& mapped : ccParameter)
        epoch = epoch * 31 + (juce::uint64) (mapped.load() + 2);

    if (epoch == hashedDataEpoch)
        return cachedDataHash;

    // FNV-1a over the values (the remaps that are off all hash alike).
    juce::uint64 hash = 14695981039346656037ull;
    const auto add = [&hash] (const void* bytes, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
            hash = (hash ^ static_cast<const juce::uint8*> (bytes)[i]) * 1099511628211ull;
    };
    const auto addCurve = [&add] (const LfoCurve& curve)
    {
        for (const auto& point : curve.points)
            add (&point, sizeof (point));

        const auto count = curve.points.size();
        add (&count, sizeof (count));
    };

    const auto data = capturePatchData();
    add (data.draws.data(), sizeof (data.draws));

    for (const auto& curve : data.curves)
        addCurve (curve);

    for (const auto& curve : data.remaps)
        addCurve (curve);

    if (data.clips != nullptr)
    {
        for (const auto& clip : *data.clips)
        {
            add (&clip.bars, sizeof (clip.bars));

            for (const auto& note : clip.notes)
                add (&note, sizeof (note));

            const auto count = clip.notes.size();
            add (&count, sizeof (count));
        }
    }

    if (data.fx != nullptr && ! data.fx->isDefault())
    {
        const auto text = FxPatchData::toValueTree (*data.fx).toXmlString();
        add (text.toRawUTF8(), text.getNumBytesAsUTF8());
    }

    const auto addText = [&add] (const juce::String& text)
    {
        add (text.toRawUTF8(), text.getNumBytesAsUTF8());
        const auto count = text.length();
        add (&count, sizeof (count));
    };

    // Patch tables by content (an undo that puts a table back is not an
    // edit), kept per doc: a table is up to 1 MB.
    for (size_t slot = 0; slot < data.tables.size(); ++slot)
    {
        const auto& doc = data.tables[slot];
        if (doc == nullptr)
            continue;

        auto& cached = docHashes[slot];
        if (cached.first != doc)
        {
            juce::uint64 docHash = 14695981039346656037ull;
            const auto mix = [&docHash] (const void* bytes, size_t size)
            {
                for (size_t i = 0; i < size; ++i)
                    docHash = (docHash ^ static_cast<const juce::uint8*> (bytes)[i]) * 1099511628211ull;
            };
            mix (doc->name.toRawUTF8(), doc->name.getNumBytesAsUTF8());
            for (const auto& frame : doc->frames)
                mix (frame.data(), frame.size() * sizeof (float));
            const auto frames = doc->frames.size();
            mix (&frames, sizeof (frames));
            cached = { doc, docHash };
        }
        add (&slot, sizeof (slot));
        add (&cached.second, sizeof (cached.second));
    }
    for (auto& cached : docHashes)
        if (cached.first != nullptr && std::find (data.tables.begin(), data.tables.end(), cached.first) == data.tables.end())
            cached = {};

    // Samples by identity (a file's path, a bounce's audio), macros, the
    // learned CCs and the tuning by text.
    for (size_t osc = 0; osc < data.samplePaths.size(); ++osc)
    {
        addText (data.samplePaths[osc]);
        addText (data.sampleOptions[osc]);
        const auto* audio = data.embedded[osc].get();
        add (&audio, sizeof (audio));
    }
    for (size_t macro = 0; macro < data.macroNames.size(); ++macro)
    {
        addText (data.macroNames[macro]);
        add (&data.macroCcs[macro], sizeof (int));
    }
    addText (getParamCcMapText());
    addText (data.tuningScale);
    addText (data.tuningMapping);

    hashedDataEpoch = epoch;
    cachedDataHash = (juce::int64) hash;
    return cachedDataHash;
}
