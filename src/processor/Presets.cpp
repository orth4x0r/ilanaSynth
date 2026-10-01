#include "ProcessorInternal.h"

namespace
{
// The automatic timbre macro's name (see applyDefaultMacros): FM depth when
// OSC 2 already frequency-modulates OSC 1, the warp when OSC 1 is warped,
// else from OSC 1's mode.
juce::String defaultTimbreMacro (int osc1Mode, float fmAmount, bool osc2On, int osc1Warp)
{
    if (osc2On && fmAmount > 0.01f)
        return "FM";
    if (osc1Mode == 0 && osc1Warp > 0)
        return "WARP";
    return osc1Mode == 0 ? "MORPH" : (osc1Mode == 1 ? "DAMP" : "START");
}
} // namespace

juce::StringArray IlanaSynthAudioProcessor::getFactoryPresetNames() const
{
    juce::StringArray names;

    for (const auto& preset : Presets::getFactoryPresets())
        names.add (preset.name);

    return names;
}

juce::StringArray IlanaSynthAudioProcessor::getFactoryPresetCategories() const
{
    return Presets::getFactoryPresetCategories();
}

juce::Array<juce::File> IlanaSynthAudioProcessor::getUserPresetFiles() const
{
    juce::Array<juce::File> files;

    const auto directory = getUserPresetDirectory();

    if (directory.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.ilanapreset"))
            files.add (entry.getFile());
    }

    files.sort();
    return files;
}

juce::StringArray IlanaSynthAudioProcessor::getFactoryMacroNames (int factoryIndex) const
{
    const auto& presets = Presets::getFactoryPresets();
    juce::StringArray result;
    for (int i = 0; i < 4; ++i)
        result.add ({});

    if (factoryIndex < 0 || factoryIndex >= (int) presets.size())
        return result;

    const auto& preset = presets[(size_t) factoryIndex];
    std::vector<std::pair<juce::String, float>> values;

    for (const auto& value : preset.values)
        values.push_back ({ juce::String (value.id), value.value });

    std::array<juce::String, 4> voiced;
    applyPresetVoicing (preset.name, values, voiced);

    for (size_t macro = 0; macro < 4; ++macro)
    {
        if (macro < preset.macroNames.size() && preset.macroNames[macro] != nullptr)
            result.set ((int) macro, preset.macroNames[macro]);

        if (voiced[macro].isNotEmpty())
            result.set ((int) macro, voiced[macro]);
    }

    // A preset with no macros of its own gets the automatic ones when loaded
    // (applyDefaultMacros): the same choices, read from the recipe's values
    // over the parameters' defaults instead of from a loaded patch. Keep the
    // two in step (the UI test compares them for every preset).
    if (factoryIndex > 0 && preset.macroNames.empty())
    {
        const auto value = [this, &values] (const juce::String& id)
        {
            for (const auto& entry : values)
                if (entry.first == id)
                    return entry.second;

            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                return ranged->convertFrom0to1 (ranged->getDefaultValue());

            return 0.0f;
        };
        const auto changedFrom = [this, &values] (const juce::String& prefix)
        {
            for (const auto& entry : values)
                if (entry.first.startsWith (prefix))
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (entry.first)))
                        if (std::abs (entry.second - ranged->convertFrom0to1 (ranged->getDefaultValue())) > 0.0001f)
                            return true;

            return false;
        };

        auto anySlotAssigned = false, hasReverb = false, hasDelay = false;

        for (int slot = 1; slot <= numFxSlots; ++slot)
        {
            const auto type = value ("fx_slot" + juce::String (slot));
            anySlotAssigned = anySlotAssigned || type > 0.5f;
            hasReverb = hasReverb || (int) type == 13;
            hasDelay = hasDelay || (int) type == 9;
        }

        if (! anySlotAssigned)
        {
            hasReverb = changedFrom ("fx_reverb_");
            hasDelay = changedFrom ("fx_delay_") || changedFrom ("fx_taps_");
        }

        const auto osc1Mode = (int) value ("osc1_mode");
        const auto timbre = defaultTimbreMacro (osc1Mode, value ("fm_amount"), value ("osc2_on") > 0.5f,
                                                (int) value ("osc1_warp"));
        const juce::String defaults[4] {
            value ("f1_cutoff") > 6000.0f ? "DARKEN" : "BRIGHT",
            timbre,
            value ("f1_reso") > 0.5f ? "RESO" : "DRIVE",
            hasReverb || hasDelay ? "SPACE" : (value ("osc1_unison") > 1.5f ? "WIDTH" : "SWELL")
        };

        for (int macro = 0; macro < 4; ++macro)
            if (voiced[(size_t) macro].isEmpty())
                result.set (macro, defaults[macro]);
    }

    return result;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetNames() const
{
    auto names = getFactoryPresetNames();

    for (const auto& file : getUserPresetFiles())
        names.add (file.getFileNameWithoutExtension());

    return names;
}

const IlanaSynthAudioProcessor::UserPresetMeta& IlanaSynthAudioProcessor::getUserPresetMeta (const juce::File& file) const
{
    const auto key = file.getFullPathName();
    const auto modified = file.getLastModificationTime().toMilliseconds();
    auto& entry = userPresetMetaCache[key.toStdString()];

    if (entry.modified != modified)
    {
        entry.modified = modified;
        entry.category = "User";
        entry.tags.clear();

        // Only the root element's attributes are needed.
        if (auto xml = juce::XmlDocument (file).getDocumentElementIfTagMatches (apvts.state.getType().toString()))
        {
            const auto category = xml->getStringAttribute ("presetCategory").trim();

            if (category.isNotEmpty())
                entry.category = category;

            entry.tags = xml->getStringAttribute ("presetTags").trim();
        }
    }

    return entry;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetCategories() const
{
    auto categories = getFactoryPresetCategories();

    for (const auto& file : getUserPresetFiles())
        categories.add (getUserPresetMeta (file).category);

    return categories;
}

juce::StringArray IlanaSynthAudioProcessor::getAllPresetTags() const
{
    juce::StringArray tags;

    for (int i = 0; i < getFactoryPresetCategories().size(); ++i)
        tags.add ({});

    for (const auto& file : getUserPresetFiles())
        tags.add (getUserPresetMeta (file).tags);

    return tags;
}

int IlanaSynthAudioProcessor::getNumAllPresets() const
{
    return (int) Presets::getFactoryPresets().size() + getUserPresetFiles().size();
}

void IlanaSynthAudioProcessor::loadPresetByIndex (int index)
{
    ++dataEpoch; // what the editor draws changes
    const auto factoryCount = (int) Presets::getFactoryPresets().size();

    if (index < 0)
        return;

    if (index < factoryCount)
    {
        loadFactoryPreset (index);
        return;
    }

    const auto files = getUserPresetFiles();
    const auto userIndex = index - factoryCount;

    if (userIndex < files.size())
        loadPresetFromFile (files[userIndex]);
}

void IlanaSynthAudioProcessor::handleAsyncUpdate()
{
    {
        std::shared_ptr<SampleData> audio;
        auto ready = false;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);
            std::swap (ready, bounceReady);
            audio = std::move (bounceResult);
        }
        if (ready)
        {
            juce::String message;
            const auto ok = applyBounce (pendingBounce, std::move (audio), message);
            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                bounceMessage = message;
            }
            bounceState = ok ? BounceState::Done : BounceState::Failed;
        }
    }

    const auto program = pendingProgramChange.exchange (-1);

    if (program >= 0)
    {
        const auto count = Presets::getFactoryPresets().size();

        if (count > 0)
            loadFactoryPreset (program % (int) count);
    }

    if (macrosPending.exchange (false))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (auto* parameter = apvts.getParameter ("macro" + juce::String (i + 1)))
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (pendingMacros[i].load()));
        }
    }

    bool reloadSamples = false;
    bool reloadTables = false;
    std::array<bool, (size_t) numSampleOscs> clearSamples {};
    std::array<bool, (size_t) numUserSlots> clearTables {};

    setOversampling (wantedOversamplingFactor());

    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        reloadSamples = samplesReloadPending;
        samplesReloadPending = false;
        reloadTables = userTablesReloadPending;
        userTablesReloadPending = false;
        clearSamples = pendingSampleClear;
        pendingSampleClear = {};
        clearTables = pendingUserTableClear;
        pendingUserTableClear = {};
    }

    if (reloadSamples || clearSamples != std::array<bool, (size_t) numSampleOscs> {})
    {
        for (int i = 0; i < numSampleOscs; ++i)
        {
            juce::String path;

            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                path = pendingSamplePaths[(size_t) i];
                pendingSamplePaths[(size_t) i].clear();
            }

            if (clearSamples[(size_t) i])
            {
                setUserSample (i, nullptr, {});
                continue;
            }

            if (path.isEmpty())
                continue;

            const juce::File file (path);

            if (file.existsAsFile())
                loadUserSample (i, file);
        }
    }

    if (reloadTables || clearTables != std::array<bool, (size_t) numUserSlots> {})
    {
        for (int i = 0; i < numUserSlots; ++i)
        {
            juce::String path;
            auto mode = 0;

            {
                const juce::SpinLock::ScopedLockType lock (stateLock);
                path = pendingUserTablePaths[(size_t) i];
                mode = pendingUserTableModes[(size_t) i];
                pendingUserTablePaths[(size_t) i].clear();
            }

            if (clearTables[(size_t) i])
            {
                resetUserTableToDefault (i);
                continue;
            }

            if (path.isEmpty())
                continue;

            const juce::File file (path);

            if (file.existsAsFile())
                loadUserWavetable (i, file, (Wavetable::LoadMode) juce::jlimit (0, 2, mode));
        }
    }
}

int IlanaSynthAudioProcessor::assignModSlot (int sourceIndex, int destination, float depth)
{
    for (int i = 0; i < Mod::maxSlots; ++i)
    {
        const auto slot = readModSlot (i);

        if (slot.source != Mod::Source::None || slot.destination != 0)
            continue;

        clearModSlot (i);
        setModSlotValue (i, "src", (float) sourceIndex);
        setModSlotValue (i, "dst", (float) destination);
        setModSlotValue (i, "amt", depth);
        return i;
    }

    return -1;
}

void IlanaSynthAudioProcessor::applyDefaultMacros (const std::array<bool, 4>& keep)
{
    using D = Mod::Destination;
    using Targets = std::vector<std::pair<int, float>>;

    const auto map = [this, &keep] (int macro, const char* name, const Targets& targets)
    {
        if (keep[(size_t) macro])
            return;

        setMacroName (macro, name);

        for (const auto& target : targets)
            assignModSlot ((int) Mod::Source::Macro1 + macro, target.first, target.second);
    };

    const auto on = [this] (const char* id) { return getParam (id) > 0.5f; };

    // 1: tone. Bright patches close down, dark ones open up.
    const auto bright = getParam ("f1_cutoff") > 6000.0f;
    map (0, bright ? "DARKEN" : "BRIGHT", { { (int) D::Filter1Cutoff, bright ? -0.5f : 0.4f } });

    // 2: timbre, from whatever the main oscillators are: FM depth, warp,
    // wavetable frame, string damping or sample start.
    const auto osc1Mode = (int) getParam ("osc1_mode");
    const auto osc2Wave = on ("osc2_on") && (int) getParam ("osc2_mode") == 0;
    const auto timbre = defaultTimbreMacro (osc1Mode, getParam ("fm_amount"), on ("osc2_on"), (int) getParam ("osc1_warp"));

    if (timbre == "FM")
    {
        map (1, "FM", { { (int) D::FmAmount, 0.3f } });
    }
    else if (timbre == "WARP")
    {
        map (1, "WARP", { { (int) D::Osc1Warp, 0.4f } });
    }
    else if (osc1Mode == 0)
    {
        Targets targets { { (int) D::Osc1Frame, 0.4f } };

        if (osc2Wave)
            targets.push_back ({ (int) D::Osc2Frame, 0.4f });

        map (1, "MORPH", targets);
    }
    else if (osc1Mode == 1)
    {
        map (1, "DAMP", { { Mod::destinationForParamId ("osc1_string_damp"), 0.3f } });
    }
    else
    {
        map (1, "START", { { (int) D::Osc1SampleStart, 0.3f } });
    }

    // 3: resonance on resonant patches, else drive into the filter.
    if (getParam ("f1_reso") > 0.5f)
        map (2, "RESO", { { (int) D::Filter1Reso, 0.3f } });
    else
        map (2, "DRIVE", { { (int) D::Filter1Drive, 0.5f } });

    // 4: space if the chain has reverb or delay, else width or swell.
    auto hasReverb = false, hasDelay = false;

    for (int slot = 1; slot <= numFxSlots; ++slot)
    {
        const auto type = (int) getParam (fxSlotIds[(size_t) slot - 1].type);
        hasReverb = hasReverb || type == 13;
        hasDelay = hasDelay || type == 9;
    }

    if (hasReverb || hasDelay)
    {
        Targets targets;

        if (hasReverb)
            targets.push_back ({ (int) D::FxReverbMix, 0.3f });

        if (hasDelay)
            targets.push_back ({ (int) D::FxDelayMix, 0.25f });

        map (3, "SPACE", targets);
    }
    else if (getParam ("osc1_unison") > 1.5f)
    {
        map (3, "WIDTH", { { (int) D::Osc1Detune, 0.3f }, { (int) D::Osc1Spread, 0.4f } });
    }
    else
    {
        map (3, "SWELL", { { (int) D::AmpAttack, 0.3f } });
    }
}

namespace
{
// The voicing table's lines, parsed once (or ILANA_PRESET_VOICING's file,
// re-read on every load so a renderer can iterate without a rebuild).
struct VoicingLine
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> set;
    std::vector<std::pair<juce::String, float>> add; // id+=value: added to the recipe's value
    struct Macro
    {
        int index = 0;
        juce::String name;
        std::vector<std::pair<int, float>> targets;
    };
    std::vector<Macro> macros;
};

int voicingDestination (const juce::String& target)
{
    if (const auto d = Mod::destinationForParamId (target); d > 0)
        return d;
    static const auto names = Mod::getDestinationNames();
    return juce::jmax (0, names.indexOf (target, true));
}

std::vector<VoicingLine> parseVoicing (const juce::String& text)
{
    std::vector<VoicingLine> lines;
    for (auto raw : juce::StringArray::fromLines (text))
    {
        raw = raw.trim();
        if (raw.isEmpty() || raw.startsWith ("#"))
            continue;

        const auto parts = juce::StringArray::fromTokens (raw, "|", "");
        VoicingLine line;
        line.name = parts[0].trim();

        for (const auto& token : juce::StringArray::fromTokens (parts[1], " \t", ""))
        {
            const auto id = token.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = token.fromFirstOccurrenceOf ("=", false, false).trim();
            if (id.isEmpty())
                continue;
            if (id == "fx")
            {
                const auto types = juce::StringArray::fromTokens (value, ",", "");
                for (int slot = 0; slot < IlanaSynthAudioProcessor::numFxSlots; ++slot)
                    line.set.push_back ({ "fx_slot" + juce::String (slot + 1), slot < types.size() ? types[slot].getFloatValue() : 0.0f });
            }
            else if (id.endsWithChar ('+'))
            {
                line.add.push_back ({ id.dropLastCharacters (1), value.getFloatValue() });
            }
            else
            {
                line.set.push_back ({ id, value.getFloatValue() });
            }
        }

        for (const auto& text : juce::StringArray::fromTokens (parts[2], ";", ""))
        {
            const auto head = text.upToFirstOccurrenceOf (":", false, false).trim();
            if (! head.startsWithChar ('m') || ! head.containsChar ('='))
                continue;
            VoicingLine::Macro macro;
            macro.index = head.substring (1).upToFirstOccurrenceOf ("=", false, false).getIntValue();
            macro.name = head.fromFirstOccurrenceOf ("=", false, false).trim();
            for (auto target : juce::StringArray::fromTokens (text.fromFirstOccurrenceOf (":", false, false), ",", ""))
            {
                target = target.trim();
                const auto split = target.lastIndexOfChar (' ');
                if (split <= 0)
                    continue;
                if (const auto d = voicingDestination (target.substring (0, split).trim()); d > 0)
                    macro.targets.push_back ({ d, target.substring (split + 1).getFloatValue() });
                else
                    DBG ("voicing: unknown macro target " << target);
            }
            if (macro.index >= 1 && macro.index <= 4)
                line.macros.push_back (macro);
        }

        lines.push_back (line);
    }
    return lines;
}

const VoicingLine* findVoicing (const char* presetName)
{
    static const auto builtIn = parseVoicing (Presets::getVoicingText());
    static std::vector<VoicingLine> fromFile;

    const auto* table = &builtIn;
    if (const auto path = juce::SystemStats::getEnvironmentVariable ("ILANA_PRESET_VOICING", ""); path.isNotEmpty())
    {
        fromFile = parseVoicing (juce::File (path).loadFileAsString());
        table = &fromFile;
    }

    for (const auto& line : *table)
        if (line.name == presetName)
            return &line;
    return nullptr;
}
} // namespace

void IlanaSynthAudioProcessor::applyPresetVoicing (const char* presetName, std::vector<std::pair<juce::String, float>>& values,
                                                   std::array<juce::String, 4>& macroNames)
{
    if (presetName == nullptr || ! presetVoicingEnabled)
        return;
    const auto* line = findVoicing (presetName);
    if (line == nullptr)
        return;

    const auto find = [&values] (const juce::String& id) -> std::pair<juce::String, float>*
    {
        for (auto& entry : values)
            if (entry.first == id)
                return &entry;
        return nullptr;
    };
    const auto put = [&] (const juce::String& id, float value)
    {
        if (auto* entry = find (id))
            entry->second = value;
        else
            values.push_back ({ id, value });
    };

    for (const auto& [id, value] : line->set)
        put (id, value);

    for (const auto& [id, value] : line->add)
    {
        if (auto* entry = find (id))
            entry->second += value;
        else
            values.push_back ({ id, value }); // the recipe left it at its default: 0 for the ids += is for
    }

    for (const auto& macro : line->macros)
    {
        const auto source = (float) Mod::macroSourceFor (macro.index - 1);
        std::vector<int> free;
        for (int slot = 1; slot <= Mod::maxSlots; ++slot)
        {
            const auto p = "mod" + juce::String (slot);
            auto* src = find (p + "_src");
            if (src != nullptr && juce::roundToInt (src->second) == juce::roundToInt (source))
            {
                src->second = 0.0f;
                put (p + "_dst", 0.0f);
                put (p + "_amt", 0.0f);
                put (p + "_curve", 0.0f);
                put (p + "_aux", 0.0f);
            }
            if (src == nullptr || juce::roundToInt (src->second) == 0)
                free.push_back (slot);
        }

        macroNames[(size_t) macro.index - 1] = macro.name;
        for (size_t i = 0; i < macro.targets.size() && i < free.size(); ++i)
        {
            const auto p = "mod" + juce::String (free[i]);
            put (p + "_src", source);
            put (p + "_dst", (float) macro.targets[i].first);
            put (p + "_amt", macro.targets[i].second);
        }
    }
}

void IlanaSynthAudioProcessor::applyPresetTrims (const char* presetName, const juce::String& category,
                                                  std::vector<std::pair<juce::String, float>>& values)
{
    if (presetName == nullptr || juce::SystemStats::getEnvironmentVariable ("ILANA_NO_TRIMS", "").isNotEmpty())
        return;

    // Leads had far too much glide (the owner's A/B listening, 2026-09-30):
    // a third of what their recipes set.
    if (category == "Lead")
        for (auto& entry : values)
            if (entry.first == "glide")
                entry.second /= 3.0f;

    const Presets::Trim* trim = nullptr;
    for (const auto& candidate : Presets::getTrims())
        if (std::strcmp (candidate.name, presetName) == 0)
            trim = &candidate;
    if (trim == nullptr)
        return;

    const auto find = [&values] (const juce::String& id) -> std::pair<juce::String, float>*
    {
        for (auto& entry : values)
            if (entry.first == id)
                return &entry;
        return nullptr;
    };

    if (trim->levelDb != 0.0f)
    {
        auto* master = find ("master");
        const auto base = master != nullptr ? master->second : -6.0f; // the parameter's default
        const auto level = juce::jlimit (-60.0f, 12.0f, base + trim->levelDb);
        if (master != nullptr)
            master->second = level;
        else
            values.push_back ({ "master", level });
    }

    for (int slot = 1; slot <= Mod::maxSlots; ++slot)
    {
        const auto* source = find ("mod" + juce::String (slot) + "_src");
        if (source == nullptr)
            continue;
        const auto macro = Mod::macroIndexFor ((Mod::Source) juce::roundToInt (source->second));
        if (macro < 0 || macro >= 4 || trim->macroScale[macro] == 1.0f)
            continue;
        if (auto* amount = find ("mod" + juce::String (slot) + "_amt"))
            amount->second = juce::jlimit (-1.0f, 1.0f, amount->second * trim->macroScale[macro]);
    }
}

void IlanaSynthAudioProcessor::loadFactoryPreset (int index)
{
    ++dataEpoch; // what the editor draws changes
    liveRetrigger = true;
    patchCut = true;
    const auto& presets = Presets::getFactoryPresets();

    if (index < 0 || index >= (int) presets.size())
        return;

    setCurrentPresetName (presets[(size_t) index].name);
    setPresetMeta (getFactoryPresetCategories()[index], {});

    for (int macro = 0; macro < Mod::numMacros; ++macro)
    {
        const auto& names = presets[(size_t) index].macroNames;
        setMacroName (macro, macro < (int) names.size() ? juce::String (names[(size_t) macro]) : juce::String());
    }

    for (int lfo = 0; lfo < numLfos; ++lfo)
    {
        const auto& curves = presets[(size_t) index].lfoCurves;
        const auto* text = lfo < (int) curves.size() ? curves[(size_t) lfo] : nullptr;
        setLfoCurve (lfo, text != nullptr ? LfoCurve::fromString (text) : LfoCurve::preset (0));
    }
    resetAllModRemaps();

    // Back to three of each module; anything the preset turns on or routes
    // still shows because it is in use.
    for (auto& mask : revealMasks)
        mask.store (defaultRevealMask);
    ++revealVersion;

    // The rest of the per-patch state a factory preset doesn't carry goes
    // back to its default too, as applyFullState does for a state without it:
    // macro CCs, drawn LFO shapes, loaded samples and user tables.
    for (int macro = 0; macro < Mod::numMacros; ++macro)
        macroCc[macro].store (20 + macro);

    for (int lfo = 0; lfo < numLfos; ++lfo)
        for (int i = 0; i < lfoDrawSteps; ++i)
            setLfoCustomPoint (lfo, i, (float) std::sin (juce::MathConstants<double>::twoPi * (double) i / (double) lfoDrawSteps));

    {
        auto asyncNeeded = false;
        {
            const juce::SpinLock::ScopedLockType lock (stateLock);

            for (int i = 0; i < numSampleOscs; ++i)
                if (samplePaths[(size_t) i].isNotEmpty() || embeddedSamples[(size_t) i] != nullptr)
                {
                    pendingSamplePaths[(size_t) i].clear();
                    pendingSampleClear[(size_t) i] = true;
                    asyncNeeded = true;
                }

            for (int i = 0; i < numUserSlots; ++i)
                if (userTablePaths[(size_t) i].isNotEmpty())
                {
                    pendingUserTablePaths[(size_t) i].clear();
                    pendingUserTableClear[(size_t) i] = true;
                    asyncNeeded = true;
                }
        }

        if (asyncNeeded)
            triggerAsyncUpdate();

        // Edited patch tables go back to their defaults too.
        for (int i = 0; i < numUserSlots; ++i)
            if (isUserSlotEdited (i))
                resetUserTableToDefault (i);
    }

    // A loaded Scala tuning stays through preset browsing (factory presets
    // carry none): tuning_on keeps its value across the reset to defaults.
    const auto tuningWasOn = getParam (tuningOnRef) > 0.5f;

    for (auto* parameter : getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    if (tuningWasOn)
        if (auto* parameter = apvts.getParameter ("tuning_on"))
            parameter->setValueNotifyingHost (1.0f);

    // The original 80 presets predate the separate sub; move them over.
    std::vector<std::pair<juce::String, float>> values;

    for (const auto& value : presets[(size_t) index].values)
        values.push_back ({ juce::String (value.id), value.value });

    if (presets[(size_t) index].category == nullptr)
    {
        const auto find = [&values] (const juce::String& id) -> std::pair<juce::String, float>*
        {
            for (auto& entry : values)
                if (entry.first == id)
                    return &entry;

            return nullptr;
        };

        migrateLegacyOsc3 ([&find] (const juce::String& id, float fallback)
                           {
                               const auto* entry = find (id);
                               return entry != nullptr ? entry->second : fallback;
                           },
                           [&find, &values] (const juce::String& id, float value)
                           {
                               if (auto* entry = find (id))
                                   entry->second = value;
                               else
                                   values.push_back ({ id, value });
                           });
    }

    std::array<juce::String, 4> voicedMacroNames;
    applyPresetVoicing (presets[(size_t) index].name, values, voicedMacroNames);
    for (int macro = 0; macro < 4; ++macro)
        if (voicedMacroNames[(size_t) macro].isNotEmpty())
            setMacroName (macro, voicedMacroNames[(size_t) macro]);

    applyPresetTrims (presets[(size_t) index].name, getFactoryPresetCategories()[index], values);

    const auto applyValues = [this, &values]
    {
        for (const auto& value : values)
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (value.first)))
                ranged->setValueNotifyingHost (ranged->convertTo0to1 (value.second));
    };
    applyValues();

    // M10: resampled presets render their bounces now (synchronously, so a
    // preset sounds the same the moment it is loaded), then their own
    // settings go back on top of what the bounce set.
    if (! presets[(size_t) index].bounces.empty())
    {
        const auto names = getFactoryPresetNames();
        for (const auto& recipe : presets[(size_t) index].bounces)
        {
            const auto source = names.indexOf (juce::String (recipe.source));
            if (source < 0 || source == index)
                continue;
            IlanaSynthAudioProcessor renderer;
            renderer.loadFactoryPreset (source);
            renderer.flushAsyncUpdates();
            BounceRequest request;
            request.targetOsc = juce::jlimit (0, OscillatorIds::count - 1, recipe.osc - 1);
            request.toTable = recipe.toTable;
            request.withFx = recipe.withFx;
            request.muteOthers = false;
            request.note = recipe.note;
            request.holdSeconds = recipe.hold;
            request.tailSeconds = recipe.tail;
            juce::String message;
            applyBounce (request, renderBounce (renderer.buildFullState(), request), message);
        }
        applyValues();
    }

    // Presets written before the rack had slots only enabled modules; if this
    // preset never assigns a slot, restore the classic chain so its effects
    // keep working.
    auto anySlotAssigned = false;

    for (int slot = 1; slot <= numFxSlots; ++slot)
        if (getParam (("fx_slot" + juce::String (slot)).toRawUTF8()) > 0.5f)
            anySlotAssigned = true;

    if (! anySlotAssigned)
    {
        const auto moduleConfigured = [this] (const juce::String& prefix)
        {
            const auto state = apvts.copyState();

            for (int i = 0; i < state.getNumChildren(); ++i)
            {
                const auto id = state.getChild (i).getProperty ("id").toString();

                if (! id.startsWith (prefix))
                    continue;

                if (auto* parameter = apvts.getParameter (id))
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                    {
                        const auto current = apvts.getRawParameterValue (id)->load();
                        const auto defaultValue = ranged->convertFrom0to1 (ranged->getDefaultValue());

                        if (std::abs (current - defaultValue) > 0.0001f)
                            return true;
                    }
                }
            }

            return false;
        };

        const std::pair<const char*, int> legacyModules[] {
            { "fx_drive_", 2 }, { "fx_fold", 2 }, { "fx_crush_", 3 }, { "fx_comb_", 5 },
            { "fx_phaser_", 6 }, { "fx_chorus_", 7 }, { "fx_delay_", 9 }, { "fx_taps_", 9 },
            { "fx_stutter_", 10 }, { "fx_smear_", 11 }, { "fx_freeze_", 12 }, { "fx_reverb_", 13 }
        };

        auto nextSlot = 1;
        bool assignedTypes[64] {};

        for (const auto& module : legacyModules)
        {
            if (nextSlot > numFxSlots)
                break;

            if (module.second < 64 && assignedTypes[module.second])
                continue;

            if (moduleConfigured (module.first))
            {
                if (auto* parameter = apvts.getParameter ("fx_slot" + juce::String (nextSlot)))
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) module.second));

                if (module.second < 64)
                    assignedTypes[module.second] = true;

                ++nextSlot;
            }
        }
    }

    // Auto-mapped presets get the defaults, except on macros the voicing wired
    // (those keep their own name and routing, not the default's on top).
    if (index > 0 && presets[(size_t) index].macroNames.empty())
    {
        std::array<bool, 4> keep {};
        for (size_t macro = 0; macro < 4; ++macro)
            keep[macro] = voicedMacroNames[macro].isNotEmpty();
        applyDefaultMacros (keep);
    }
}

bool IlanaSynthAudioProcessor::savePresetToFile (const juce::File& file)
{
    const auto state = buildFullState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    setCurrentPresetName (file.getFileNameWithoutExtension());
    return true;
}

bool IlanaSynthAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    ++dataEpoch; // what the editor draws changes
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    applyFullState (juce::ValueTree::fromXml (*xml));
    setCurrentPresetName (file.getFileNameWithoutExtension());
    return true;
}
