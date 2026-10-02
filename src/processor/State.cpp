#include "ProcessorInternal.h"

juce::ValueTree IlanaSynthAudioProcessor::buildFullState()
{
    auto state = apvts.copyState();
    state.setProperty ("osc3Schema", 2, nullptr);
    state.setProperty ("destSchema", 2, nullptr);
    state.setProperty ("tableSchema", 3, nullptr);
    state.setProperty ("exciterLevels", exciterLevelMatch.load() ? 1 : 0, nullptr);

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

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
        if (isModRemapOn (slot))
            state.setProperty ("mod" + juce::String (slot + 1) + "Remap", getModRemap (slot).toString(), nullptr);

    for (int macro = 0; macro < Mod::numMacros; ++macro)
        state.setProperty ("macroCc" + juce::String (macro), macroCc[macro].load(), nullptr);

    if (const auto ccMap = getParamCcMapText(); ccMap.isNotEmpty())
        state.setProperty ("midiCcMap", ccMap, nullptr);
    else
        state.removeProperty ("midiCcMap", nullptr);
    state.setProperty ("oscRevealMask", revealMasks[(size_t) Module::Oscillator].load(), nullptr);
    state.setProperty ("envRevealMask", revealMasks[(size_t) Module::Envelope].load(), nullptr);
    state.setProperty ("lfoRevealMask", revealMasks[(size_t) Module::Lfo].load(), nullptr);

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);

        for (int i = 0; i < numSampleOscs; ++i)
            if (samplePaths[(size_t) i].isNotEmpty())
                state.setProperty ("osc" + juce::String (i + 1) + "SamplePath", samplePaths[(size_t) i], nullptr);

        for (int i = 0; i < numUserSlots; ++i)
            if (userTablePaths[(size_t) i].isNotEmpty())
            {
                state.setProperty ("userTablePath" + juce::String (i + 1), userTablePaths[(size_t) i], nullptr);
                state.setProperty ("userTableMode" + juce::String (i + 1), userTableModes[(size_t) i], nullptr);
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
            auto tree = encodeSample (*data);
            tree.setProperty ("osc", i, nullptr);
            samples.appendChild (tree, nullptr);
        }
    }
    if (samples.getNumChildren() > 0)
        state.appendChild (samples, nullptr);

    tuningState.saveTo (state);
    clipState.saveTo (state);

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

    const auto& algorithm = FmAlgorithms::all()[(size_t) index];

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
                set (id, source == target ? FmAlgorithms::defaultFeedbackAmount : FmAlgorithms::defaultRouteAmount);
                // New feedback starts Filtered (calm at high amounts); feedback
                // the patch already had keeps its type.
                if (source == target)
                    set (juce::String (OscillatorIds::prefixes[(size_t) source]) + "_fb_type", (float) FmFeedback::Filtered);
            }
        }

    for (int op = 0; op < algorithm.numOperators; ++op)
        set (oscCoreIds[(size_t) op].out, FmAlgorithms::isCarrier (algorithm, op) ? 1.0f : 0.0f);
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

    const auto source = Mod::lfoSourceFor (index);

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto routing = readModSlot (slot);

        if (routing.destination != 0 && (routing.source == source || routing.aux == source))
            return true;
    }

    return false;
}

juce::Colour IlanaSynthAudioProcessor::lfoColour (int index)
{
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

void IlanaSynthAudioProcessor::applyFullState (const juce::ValueTree& stateIn)
{
    liveRetrigger = true;
    patchCut = true;
    auto state = stateIn.createCopy();
    tuningState.loadFrom (state); // no Tuning child: 12-TET
    clipState.loadFrom (state);   // no Clips child: no clips
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
        const auto added = TableFactory::getNumFactoryTables() - 40;

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

        for (int i = 0; i < lfoDrawSteps && i < tokens.size(); ++i)
            setLfoCustomPoint (lfoIndex, i, tokens[i].getFloatValue());
    };

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto property = "lfo" + juce::String (lfo + 1) + "Draw";
        const auto text = state.getProperty (property).toString();

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

        for (int i = 0; i < numSampleOscs; ++i)
        {
            const auto path = state.getProperty ("osc" + juce::String (i + 1) + "SamplePath").toString();

            if (restoredSamples[(size_t) i])
            {
                pendingSamplePaths[(size_t) i].clear();
                pendingSampleClear[(size_t) i] = false;
            }
            else if (path.isNotEmpty() && path != samplePaths[(size_t) i])
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
        }

        for (int i = 0; i < numUserSlots; ++i)
        {
            if (restoredTables[(size_t) i])
            {
                pendingUserTablePaths[(size_t) i].clear();
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

    apvts.replaceState (state);
    updateExciterLevelMatch (state.hasProperty ("exciterLevels") ? (int) state.getProperty ("exciterLevels") == 1 : false);
}

// A patch saved with the matched exciter levels keeps them; one saved before
// (or a factory preset) gets them only if it plays none of the trimmed
// exciters, so nothing that was already made changes its level.
void IlanaSynthAudioProcessor::updateExciterLevelMatch (bool savedWithMatch)
{
    auto usesTrimmed = false;

    for (const auto* prefix : OscillatorIds::prefixes)
    {
        const auto* mode = apvts.getRawParameterValue (juce::String (prefix) + "_mode");
        const auto* excite = apvts.getRawParameterValue (juce::String (prefix) + "_excite");
        if (mode != nullptr && excite != nullptr && juce::roundToInt (mode->load()) == 1
            && Voice::exciterTrim (juce::roundToInt (excite->load())) != 1.0f)
            usesTrimmed = true;
    }

    exciterLevelMatch = savedWithMatch || ! usesTrimmed;
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
    const auto epoch = dataEpoch.load();

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

    hashedDataEpoch = epoch;
    cachedDataHash = (juce::int64) hash;
    return cachedDataHash;
}
