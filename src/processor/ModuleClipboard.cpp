// Copy, Paste and Init on one module (ilana 2026-10-28: "go on all of em",
// the RC 1.0 workflow list): an oscillator, a filter, an envelope or an LFO
// is its own parameters (oscN_*, fN_*, the envelope's prefix, lfoN_*) plus,
// for an oscillator, its sample and its patch table, and for an LFO, its
// drawn curve and steps. A snapshot holds them by suffix, so it pastes onto
// any module of the same kind, in this patch or another: a suffix the
// target doesn't have is skipped (the amp envelope's own extras, say).
// Routes into a module stay where they are. Message thread; the page wraps
// each in performEdit, so each is one undo step.

#include "ProcessorInternal.h"

namespace
{
// The oscillator suffixes, read from OSC 1, 2 and 4-6 ("sub_" also starts
// the SUB card's own parameters).
std::vector<juce::String> oscillatorSuffixes (const juce::AudioProcessor& processor)
{
    std::set<juce::String> found;
    for (auto* parameter : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            for (const auto* prefix : { "osc1_", "osc2_", "osc4_", "osc5_", "osc6_" })
                if (withId->paramID.startsWith (prefix))
                    found.insert (withId->paramID.substring (5));
    return { found.begin(), found.end() };
}
} // namespace

juce::String IlanaSynthAudioProcessor::modulePrefix (ModuleKind kind, int index)
{
    switch (kind)
    {
        case ModuleKind::Oscillator: return juce::String (OscillatorIds::prefixes[(size_t) juce::jlimit (0, OscillatorIds::count - 1, index)]);
        case ModuleKind::Filter:     return "f" + juce::String (juce::jlimit (1, 2, index + 1));
        case ModuleKind::Envelope:   return envelopePrefix (juce::jlimit (0, 15, index));
        case ModuleKind::Lfo:        return "lfo" + juce::String (juce::jlimit (1, numLfos, index + 1));
    }
    return {};
}

std::vector<juce::String> IlanaSynthAudioProcessor::moduleParameterIds (ModuleKind kind, int index) const
{
    std::vector<juce::String> ids;
    const auto prefix = modulePrefix (kind, index) + "_";

    if (kind == ModuleKind::Oscillator)
    {
        for (const auto& suffix : oscillatorSuffixes (*this))
            if (apvts.getParameter (prefix + suffix) != nullptr)
                ids.push_back (prefix + suffix);
        return ids;
    }

    for (auto* parameter : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            if (withId->paramID.startsWith (prefix))
                ids.push_back (withId->paramID);
    return ids;
}

IlanaSynthAudioProcessor::ModuleSnapshot IlanaSynthAudioProcessor::copyModule (ModuleKind kind, int index) const
{
    ModuleSnapshot snapshot;
    snapshot.kind = kind;
    snapshot.name = moduleName (kind, index);
    const auto prefixLength = modulePrefix (kind, index).length() + 1;

    for (const auto& id : moduleParameterIds (kind, index))
        snapshot.values.emplace_back (id.substring (prefixLength), readNormalised (id));

    const auto data = capturePatchData();

    if (kind == ModuleKind::Lfo && juce::isPositiveAndBelow (index, numLfos))
    {
        snapshot.curve = data.curves[(size_t) index];
        snapshot.draw = data.draws[(size_t) index];
    }

    if (kind == ModuleKind::Oscillator && juce::isPositiveAndBelow (index, OscillatorIds::count))
    {
        snapshot.embedded = data.embedded[(size_t) index];
        snapshot.liveSample = data.liveSamples[(size_t) index];
        snapshot.samplePath = data.samplePaths[(size_t) index];
        snapshot.sampleOptions = data.sampleOptions[(size_t) index];

        // A patch table goes with it (only when edited: an unedited slot is a
        // factory table every patch has).
        if (const auto* table = apvts.getRawParameterValue (modulePrefix (kind, index) + "_table"))
        {
            const auto slot = juce::roundToInt (table->load()) - TableFactory::getNumFactoryTables();
            if (juce::isPositiveAndBelow (slot, numUserSlots) && isUserSlotEdited (slot))
            {
                snapshot.tableSlot = slot;
                snapshot.table = data.tables[(size_t) slot];
            }
        }
    }

    return snapshot;
}

bool IlanaSynthAudioProcessor::pasteModule (const ModuleSnapshot& snapshot, int index)
{
    const auto prefix = modulePrefix (snapshot.kind, index) + "_";
    auto any = false;

    for (const auto& [suffix, value] : snapshot.values)
        if (apvts.getParameter (prefix + suffix) != nullptr)
        {
            writeNormalised (prefix + suffix, value);
            any = true;
        }

    const auto before = capturePatchData();
    auto after = before;

    if (snapshot.kind == ModuleKind::Lfo && juce::isPositiveAndBelow (index, numLfos))
    {
        after.curves[(size_t) index] = snapshot.curve;
        after.draws[(size_t) index] = snapshot.draw;
    }

    if (snapshot.kind == ModuleKind::Oscillator && juce::isPositiveAndBelow (index, OscillatorIds::count))
    {
        after.embedded[(size_t) index] = snapshot.embedded;
        after.liveSamples[(size_t) index] = snapshot.liveSample;
        after.samplePaths[(size_t) index] = snapshot.samplePath;
        after.sampleOptions[(size_t) index] = snapshot.sampleOptions;

        // The table: kept where it is when this patch holds the same doc
        // there, else put in a free slot (never over a table in use).
        if (snapshot.table != nullptr && juce::isPositiveAndBelow (snapshot.tableSlot, numUserSlots)
            && before.tables[(size_t) snapshot.tableSlot] != snapshot.table)
        {
            if (const auto free = findFreeUserSlot(); free >= 0)
            {
                after.tables[(size_t) free] = snapshot.table;
                writeNormalised (prefix + "table", apvts.getParameter (prefix + "table")->convertTo0to1 ((float) (TableFactory::getNumFactoryTables() + free)));
            }
        }
    }

    if (! samePatchData (before, after))
        restorePatchData (after, before);

    return any;
}

void IlanaSynthAudioProcessor::initModule (ModuleKind kind, int index)
{
    for (const auto& id : moduleParameterIds (kind, index))
        if (auto* parameter = apvts.getParameter (id))
            writeNormalised (id, parameter->getDefaultValue());

    const auto before = capturePatchData();
    auto after = before;

    if (kind == ModuleKind::Lfo && juce::isPositiveAndBelow (index, numLfos))
    {
        after.curves[(size_t) index] = LfoCurve {};
        after.draws[(size_t) index] = {};
    }

    if (kind == ModuleKind::Oscillator && juce::isPositiveAndBelow (index, OscillatorIds::count))
    {
        after.embedded[(size_t) index] = nullptr;
        after.liveSamples[(size_t) index] = nullptr;
        after.samplePaths[(size_t) index] = {};
        after.sampleOptions[(size_t) index] = {};
    }

    if (! samePatchData (before, after))
        restorePatchData (after, before);
}

juce::String IlanaSynthAudioProcessor::moduleName (ModuleKind kind, int index)
{
    switch (kind)
    {
        case ModuleKind::Oscillator: return "OSC " + juce::String (index + 1);
        case ModuleKind::Filter:     return "Filter " + juce::String (index + 1);
        case ModuleKind::Envelope:
        {
            static const char* const named[] { "Amp Env", "Filter Env", "Filter 2 Env", "Mod Env", "Env 5" };
            return index < 5 ? juce::String (named[juce::jlimit (0, 4, index)]) : "Env " + juce::String (index + 1);
        }
        case ModuleKind::Lfo:        return "LFO " + juce::String (index + 1);
    }
    return {};
}
