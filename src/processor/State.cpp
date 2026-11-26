#include "ProcessorInternal.h"
#include "../ThreadChecks.h"
#include "../BuildInfo.h"
#include "../SafeXml.h"
#include "Migrations.h"

namespace
{
    // The migration stamps a state saved now carries. One above these means a
    // newer build wrote it.
    struct SavedSchema { const char* name; int version; };
    constexpr SavedSchema savedSchemas[] = {
        { "osc3Schema", 2 }, { "grainPosSchema", 2 }, { "lorenzSchema", 2 }, { "stringSchema", 2 },
        { "redundantKnobsSchema", 2 }, { "modRangeSchema", 2 }, { "destSchema", 2 }, { "tableSchema", 4 },
        { "boardMixSchema", 5 }, { "glideSchema", 2 }, { "bodyLevelSchema", 2 },
    };

    bool savedByNewerBuild (const juce::ValueTree& state)
    {
        for (const auto& schema : savedSchemas)
            if ((int) state.getProperty (schema.name, 0) > schema.version)
                return true;
        return false;
    }
}

juce::ValueTree IlanaSynthAudioProcessor::buildFullState()
{
    auto state = apvts.copyState();
    for (const auto& schema : savedSchemas)
        state.setProperty (schema.name, schema.version, nullptr);
    state.setProperty ("build", ilana::buildLabel(), nullptr);
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
    // The IR reverb's file, by path (as a sample file is).
    if (reverbIrFile.isEmpty())
        state.removeProperty ("reverbIr", nullptr);
    else
        state.setProperty ("reverbIr", reverbIrFile, nullptr);
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
        {
            // A file that is missing right now (drive offline) keeps its options
            // with its path, as the path itself is kept above.
            const auto& path = samplePaths[(size_t) i].isNotEmpty() ? samplePaths[(size_t) i]
                                                                    : (embeddedSamples[(size_t) i] == nullptr ? missingSamplePaths[(size_t) i] : juce::String());
            if (path.isNotEmpty() && sampleOptions[(size_t) i].isNotEmpty())
                if (const auto xml = SafeXml::parse (sampleOptions[(size_t) i]))
                    if (auto tree = juce::ValueTree::fromXml (*xml); tree.isValid() && tree.getProperty ("path").toString() == path)
                    {
                        tree.setProperty ("osc", i, nullptr);
                        options.appendChild (tree, nullptr);
                    }
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

    // FX CHAIN 1 / CHAIN 2: the chain not in the rack now is saved with the
    // patch (the one in the rack is its parameters and FxPatch above).
    if (chainBValid)
    {
        state.setProperty ("fxOtherChain", showingChainA ? chainB : chainA, nullptr);
        state.setProperty ("fxShowingChain", showingChainA ? 1 : 2, nullptr);
    }
    else
    {
        state.removeProperty ("fxOtherChain", nullptr);
        state.removeProperty ("fxShowingChain", nullptr);
    }

    // (The old DX7 mode's "Dx7" child is never written: a DX7 voice is
    // ordinary parameters now.)
    state.removeChild (state.getChildWithName ("Dx7"), nullptr);

    return state;
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
    const PatchLoadGate loadGate (*this);
    liveRetrigger = true;
    patchCut = true;
    ++patchGeneration; // a bounce still rendering belongs to the patch that started it
    auto state = stateIn.createCopy();
    if (state.hasProperty ("presetName")) // a factory preset renamed since the session was saved
        state.setProperty ("presetName", Presets::currentPresetName (state.getProperty ("presetName").toString()), nullptr);
    tuningState.loadFrom (state); // no Tuning child: 12-TET
    setReverbIrPath (state.getProperty ("reverbIr").toString()); // none: no IR

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
    // FX CHAIN 1 / CHAIN 2 (none saved: one chain, CHAIN 1 showing).
    {
        const auto other = state.getProperty ("fxOtherChain").toString();
        chainBValid = other.isNotEmpty();
        showingChainA = ! chainBValid || (int) state.getProperty ("fxShowingChain", 1) != 2;
        chainA = showingChainA ? juce::String() : other;
        chainB = showingChainA ? other : juce::String();
    }
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

    // The migrations for the state as saved (Migrations::table, in order).
    Migrations::runSaved (state, Migrations::SavedStage::beforeDefaults);

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

    // The migrations for the whole parameter set (Migrations::table, in order).
    Migrations::runSaved (state, Migrations::SavedStage::afterDefaults);

    apvts.replaceState (state);
    undoManager.clearUndoHistory(); // (the tree's own undo manager did this; it has none now)
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
    preloadAirwindowsForPatch();
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

    {
        const juce::ScopedLock lock (stateLoadLock);
        if (! unreadableState.isEmpty())
        {
            // Nothing changed since the state that couldn't be read: save it back as it came.
            if (xml->toString() == unreadableStateBase)
            {
                destData = unreadableState;
                return;
            }
            unreadableState.reset();
            unreadableStateBase.clear();
            stateLoadNotice.clear();
            ++stateLoadNoticeVersion;
        }
    }

    copyXmlToBinary (*xml, destData);
}

juce::String IlanaSynthAudioProcessor::getStateLoadNotice() const
{
    const juce::ScopedLock lock (stateLoadLock);
    return stateLoadNotice;
}

void IlanaSynthAudioProcessor::setStateLoadNotice (const juce::String& text)
{
    const juce::ScopedLock lock (stateLoadLock);
    unreadableState.reset();
    unreadableStateBase.clear();
    if (text != stateLoadNotice)
    {
        stateLoadNotice = text;
        ++stateLoadNoticeVersion;
    }
}

void IlanaSynthAudioProcessor::keepUnreadableState (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml (buildFullState().createXml());
    const auto base = xml->toString();

    const juce::ScopedLock lock (stateLoadLock);
    unreadableState.replaceAll (data, (size_t) sizeInBytes);
    unreadableStateBase = base;
    stateLoadNotice = "This track's saved patch could not be read and was not loaded. "
                      "Its saved data is kept unchanged until you edit the patch.";
    ++stateLoadNoticeVersion;
}

void IlanaSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ++dataEpoch; // what the editor draws changes

    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > SafeXml::maxBytes)
    {
        if (data != nullptr && sizeInBytes > 0)
            keepUnreadableState (data, sizeInBytes);
        return;
    }

    // getXmlFromBinary decodes the text as UTF-8 without checking it, and a
    // damaged state (the fuzz test's bit flips) read past the end and
    // crashed. Same header as copyXmlToBinary: magic, length, text.
    const auto* bytes = static_cast<const char*> (data);
    if (sizeInBytes > 8 && juce::ByteOrder::littleEndianInt (bytes) == 0x21324356)
    {
        const auto length = (int) juce::ByteOrder::littleEndianInt (bytes + 4);
        if (length < 0 || length > sizeInBytes - 8 || ! juce::CharPointer_UTF8::isValidString (bytes + 8, length)
            || ! SafeXml::depthIsSafe (juce::String::fromUTF8 (bytes + 8, length)))
        {
            keepUnreadableState (data, sizeInBytes);
            return;
        }
    }
    else
    {
        if (! juce::CharPointer_UTF8::isValidString (bytes, sizeInBytes)
            || ! SafeXml::depthIsSafe (juce::String::fromUTF8 (bytes, sizeInBytes)))
        {
            keepUnreadableState (data, sizeInBytes);
            return;
        }
    }

    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
    {
        keepUnreadableState (data, sizeInBytes);
        return;
    }

    const auto state = juce::ValueTree::fromXml (*xml);
    const auto newer = savedByNewerBuild (state);
    const auto savedBy = state.getProperty ("build").toString();
    applyFullState (state);
    loadedPresetFile = juce::File(); // the host's session, not a preset file

    setStateLoadNotice (newer ? "This track was saved by a newer ilanaSynth build"
                                    + (savedBy.isNotEmpty() ? " (" + savedBy + ")" : juce::String())
                                    + ". What this build (" + ilana::buildLabel() + ") doesn't know is left out, "
                                    "and saving the set here drops it."
                              : juce::String());
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
    data.colours = getSectionColours();
    data.reverbIr = reverbIrFile;
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
        || a.tuningScale != b.tuningScale || a.tuningMapping != b.tuningMapping || a.colours != b.colours
        || a.reverbIr != b.reverbIr)
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
    if (data.reverbIr != reference.reverbIr)
        setReverbIrPath (data.reverbIr);

    for (int key = 0; key < SectionColours::count; ++key)
        if (data.colours[(size_t) key] != reference.colours[(size_t) key])
            setSectionColour (key, data.colours[(size_t) key]);

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
    ILANA_ASSERT_MESSAGE_THREAD();
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
    ILANA_ASSERT_MESSAGE_THREAD();
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
    add (data.colours.data(), sizeof (data.colours));
    addText (data.reverbIr);
    addText (data.tuningScale);
    addText (data.tuningMapping);

    hashedDataEpoch = epoch;
    cachedDataHash = (juce::int64) hash;
    return cachedDataHash;
}
