#include "ProcessorInternal.h"

// Roadmap step 17: effect instances and the FX patcher (FxPatch.h for the
// data, FxEngine.h for an instance's engine, docs/ROUTING-CPU.md for the
// rules followed here).
//
// Every change to the patch is made on the message thread (or a host's state
// load) under fxEditLock and published whole: the audio thread only ever sees
// atomic pointers (slotEngines, slotValues) and the runners' programs, and
// anything it may still be reading is freed a second later (retireFx).

// How long an effect keeps sounding after its input stops, for the patcher's
// silence skip (ROUTING-CPU A5). Negative: never skipped, because it can
// sound on its own or for longer than anyone would wait (delays, reverbs,
// loops, frozen or stuttered audio, the vocoder's carrier, Airwindows'
// algorithms, some of which ring). The rest settle well inside a second.
int fxTailSamples (const PatchGraph::CompiledNode& node, double sampleRate)
{
    switch (node.kind)
    {
        case 5:  // Comb
        case 9:  // Delay
        case 10: // Stutter
        case 11: // Smear
        case 12: // Freeze
        case 13: // Reverb
        case 17: // Tape Stop
        case 24: // Shifter (its feedback)
        case 28: // Feedback
        case 30: // Airwindows
        case 31: // Vocoder
            return -1;
        default:
            break;
    }
    if (airwindows::categoryForFxType (node.kind) >= 0)
        return -1;
    return (int) sampleRate;
}

namespace
{
constexpr auto numSlots = IlanaSynthAudioProcessor::numFxSlots;

bool fxNodeInPlace (int) { return true; }

constexpr float maxLoopGain = 0.7f;  // a loop through effects that add gain still dies away
constexpr float newLoopGain = 0.5f;

// A cable that closes a loop is held to maxLoopGain; one just drawn starts at
// newLoopGain (`before`: the graph as it was, to tell new cables from old).
void limitLoopGains (PatchGraph::Graph& graph, const PatchGraph::Graph* before)
{
    const auto loops = graph.feedbackCables();
    for (size_t c = 0; c < loops.size(); ++c)
    {
        if (! loops[c])
            continue;
        const auto& cable = graph.cables()[c];
        auto gain = cable.gain;
        if (before != nullptr && before->findCable (cable.from, cable.to) < 0 && gain == 1.0f)
            gain = newLoopGain;
        gain = juce::jlimit (-maxLoopGain, maxLoopGain, gain);
        if (gain != cable.gain)
            graph.setCableGain ((int) c, gain);
    }
}

// Takes a node out and joins what fed it to what it fed (a chain stays a chain).
void removeNodeBridged (PatchGraph::Graph& graph, PatchGraph::NodeId id)
{
    std::vector<PatchGraph::Cable> into, outOf;
    for (const auto& cable : graph.cables())
    {
        if (cable.to.node == id && cable.from.node != id)
            into.push_back (cable);
        if (cable.from.node == id && cable.to.node != id)
            outOf.push_back (cable);
    }
    graph.removeNode (id);
    for (const auto& in : into)
        for (const auto& out : outOf)
            if (graph.findCable (in.from, out.to) < 0)
                graph.connect (in.from, out.to, in.gain * out.gain);
}
} // namespace

//==============================================================================
FxPatchData::Patch IlanaSynthAudioProcessor::getFxPatch() const
{
    const juce::SpinLock::ScopedLockType lock (stateLock);
    return *fxPatch;
}

void IlanaSynthAudioProcessor::setFxPatch (const FxPatchData::Patch& patch)
{
    const juce::ScopedLock edit (fxEditLock);
    auto copy = patch;
    for (auto& slot : copy.slots)
        if (slot.isDefault())
            slot = {};
    if (FxPatchData::samePatch (copy, getFxPatch()))
        return;
    publishFxPatch (std::move (copy));
    // The slots' types may still be on their way (a state load sets the
    // parameters after): the patch is checked against them afterwards.
    fxReconcileWanted = true;
    triggerAsyncUpdate();
}

void IlanaSynthAudioProcessor::publishFxPatch (FxPatchData::Patch patch)
{
    const juce::ScopedLock edit (fxEditLock);
    auto shared = std::make_shared<const FxPatchData::Patch> (std::move (patch));
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        fxPatch = std::move (shared);
    }
    applyFxPatchRuntime();
    compileFxPatchPlan();
    ++dataEpoch;
}

// The engines and value tables the published patch asks for.
void IlanaSynthAudioProcessor::applyFxPatchRuntime()
{
    const juce::ScopedLock edit (fxEditLock);
    const auto patch = getFxPatch();

    for (int s = 0; s < numSlots; ++s)
    {
        const auto& slot = patch.slots[(size_t) s];

        if (slot.own && ownedEngines[(size_t) s] == nullptr)
        {
            auto engine = std::make_unique<FxEngine> (*this);
            engine->prepare (currentSampleRate, fxEngineBlockSize, expectedBlockSize);

            // Its Airwindows algorithm made now, not on the audio thread.
            const auto valueOf = [&] (const juce::String& id)
            {
                if (slot.hasValues)
                    for (const auto& [key, value] : slot.values)
                        if (key == id)
                            return value;
                const auto* raw = apvts.getRawParameterValue (id);
                return raw != nullptr ? raw->load() : 0.0f;
            };
            if (slot.type == 30)
                engine->preloadAirwindows ((int) std::round (valueOf ("fx_aw_algo")));
            if (const auto category = airwindows::categoryForFxType (slot.type); category >= 0)
                engine->preloadAirwindowsCategory (category, (int) std::round (valueOf (makeAwCategoryRefs()[(size_t) category][0].id)));

            slotEngines[(size_t) s].store (engine.get(), std::memory_order_release);
            ownedEngines[(size_t) s] = std::move (engine);
        }
        else if (! slot.own && ownedEngines[(size_t) s] != nullptr)
        {
            slotEngines[(size_t) s].store (nullptr, std::memory_order_release);
            retireFx (std::move (ownedEngines[(size_t) s]), nullptr);
        }

        std::unique_ptr<std::vector<float>> table;
        if (slot.hasValues)
        {
            table = std::make_unique<std::vector<float>> (paramCacheIds.size(), std::numeric_limits<float>::quiet_NaN());
            for (const auto& [id, value] : slot.values)
                if (const auto* entry = findParam (id.toRawUTF8()); entry != nullptr && entry->index >= 0)
                    (*table)[(size_t) entry->index] = value;
        }

        const auto* same = ownedValues[(size_t) s].get();
        if (table == nullptr && same == nullptr)
            continue;
        if (table != nullptr && same != nullptr && *table == *same)
            continue;
        slotValues[(size_t) s].store (table != nullptr ? table->data() : nullptr, std::memory_order_release);
        retireFx (nullptr, std::move (ownedValues[(size_t) s]));
        ownedValues[(size_t) s] = std::move (table);
    }

    freeRetiredFx (false);
}

// The audio thread may be inside a block with these: freed a second later.
void IlanaSynthAudioProcessor::retireFx (std::unique_ptr<FxEngine> engine, std::unique_ptr<std::vector<float>> values)
{
    if (engine == nullptr && values == nullptr)
        return;
    retiredFx.push_back ({ juce::Time::getMillisecondCounter(), std::move (engine), std::move (values) });
}

void IlanaSynthAudioProcessor::freeRetiredFx (bool all)
{
    const juce::ScopedLock edit (fxEditLock);
    const auto now = juce::Time::getMillisecondCounter();
    retiredFx.erase (std::remove_if (retiredFx.begin(), retiredFx.end(),
                                     [all, now] (const RetiredFx& r) { return all || now - r.when >= 1000; }),
                     retiredFx.end());
}

//==============================================================================
const std::vector<juce::String>& IlanaSynthAudioProcessor::fxParamIds (int type) const
{
    static const std::vector<juce::String> none;
    if (! juce::isPositiveAndBelow (type, (int) fxParamIdCache.size()))
        return none;
    auto& ids = fxParamIdCache[(size_t) type];
    if (ids.empty())
        for (const auto& id : paramCacheIds)
            if (FxPatchData::isParamOf (type, id))
                ids.push_back (id);
    return ids;
}

std::vector<std::pair<juce::String, float>> IlanaSynthAudioProcessor::snapshotFxParams (int type) const
{
    std::vector<std::pair<juce::String, float>> values;
    for (const auto& id : fxParamIds (type))
        if (const auto* raw = apvts.getRawParameterValue (id))
            values.emplace_back (id, raw->load());
    return values;
}

void IlanaSynthAudioProcessor::writeFxParams (const std::vector<std::pair<juce::String, float>>& values)
{
    for (const auto& [id, value] : values)
        if (auto* parameter = apvts.getParameter (id))
        {
            const auto normalised = parameter->convertTo0to1 (value);
            if (parameter->getValue() != normalised)
                parameter->setValueNotifyingHost (normalised);
        }
}

juce::String IlanaSynthAudioProcessor::fxSlotLabel (int slotIndex) const
{
    const auto type = (int) getRawParam (fxSlotIds[(size_t) slotIndex].type);
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("fx_slot" + juce::String (slotIndex + 1)));
    auto label = choice != nullptr && juce::isPositiveAndBelow (type, choice->choices.size()) ? choice->choices[type].toUpperCase()
                                                                                              : juce::String();
    // Two of the same effect: numbered in slot order.
    auto count = 0, number = 0;
    for (int s = 0; s < numSlots; ++s)
        if ((int) getRawParam (fxSlotIds[(size_t) s].type) == type)
        {
            ++count;
            if (s == slotIndex)
                number = count;
        }
    return count > 1 ? label + " " + juce::String (number) : label;
}

//==============================================================================
// After the slots' types changed (a library pick, automation, a state load):
// an instance whose slot now holds another effect is dropped; every effect
// has one card in front (its settings on the parameters); the patcher's
// nodes follow the slots.
void IlanaSynthAudioProcessor::reconcileFxPatch()
{
    const juce::ScopedLock edit (fxEditLock);
    auto patch = getFxPatch();
    std::array<int, (size_t) numSlots> types {};
    for (int s = 0; s < numSlots; ++s)
        types[(size_t) s] = (int) getRawParam (fxSlotIds[(size_t) s].type);

    for (int s = 0; s < numSlots; ++s)
    {
        auto& slot = patch.slots[(size_t) s];
        if (! slot.isDefault() && slot.type != types[(size_t) s])
            slot = {};
    }

    // A type whose every slot plays its own copy: the first one comes to the front.
    std::vector<std::pair<juce::String, float>> toWrite;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto type = types[(size_t) s];
        if (type == 0)
            continue;
        auto front = false;
        auto first = -1;
        for (int o = 0; o < numSlots; ++o)
            if (types[(size_t) o] == type)
            {
                if (first < 0)
                    first = o;
                front = front || ! patch.slots[(size_t) o].hasValues;
            }
        if (! front && first == s)
        {
            auto& slot = patch.slots[(size_t) s];
            toWrite.insert (toWrite.end(), slot.values.begin(), slot.values.end());
            slot.hasValues = false;
            slot.values.clear();
        }
    }

    if (patch.hasGraph)
    {
        auto& graph = patch.graph;
        std::array<bool, (size_t) numSlots> placed {};
        std::vector<PatchGraph::NodeId> gone;
        for (const auto& node : graph.nodes())
        {
            if (node.isFixed())
                continue;
            const auto s = node.ref - 1;
            if (! juce::isPositiveAndBelow (s, numSlots) || types[(size_t) s] == 0 || placed[(size_t) s])
                gone.push_back (node.id);
            else
                placed[(size_t) s] = true;
        }
        for (const auto id : gone)
            removeNodeBridged (graph, id);

        for (int s = 0; s < numSlots; ++s)
        {
            if (types[(size_t) s] == 0 || placed[(size_t) s])
                continue;
            // In front of OUTPUT: what fed OUTPUT feeds the new effect.
            const auto* output = graph.find (PatchGraph::outputNodeId);
            const auto x = output != nullptr ? output->x : 0.0f, y = output != nullptr ? output->y : 0.0f;
            const auto id = graph.addNode (types[(size_t) s], 1, 1, x, y, {}, s + 1);
            if (id == 0)
                break;
            std::vector<PatchGraph::Cable> feeders;
            for (const auto& cable : graph.cables())
                if (cable.to.node == PatchGraph::outputNodeId)
                    feeders.push_back (cable);
            for (const auto& cable : feeders)
            {
                graph.disconnect (cable.from, cable.to);
                graph.connect (cable.from, { id, 0 }, cable.gain);
            }
            if (feeders.empty())
                graph.connect ({ PatchGraph::inputNodeId, 0 }, { id, 0 });
            graph.connect ({ id, 0 }, { PatchGraph::outputNodeId, 0 });
            graph.moveNode (PatchGraph::outputNodeId, x + 180.0f, y);
        }

        for (const auto& node : std::vector<PatchGraph::Node> (graph.nodes()))
        {
            if (node.isFixed())
                continue;
            const auto s = node.ref - 1;
            const auto label = fxSlotLabel (s).toStdString();
            if (node.kind != types[(size_t) s] || node.label != label)
                if (auto* editable = graph.find (node.id))
                {
                    editable->kind = types[(size_t) s];
                    editable->label = label;
                }
        }
        limitLoopGains (graph, nullptr);
    }

    if (! FxPatchData::samePatch (patch, getFxPatch()))
        publishFxPatch (std::move (patch));
    writeFxParams (toWrite);
}

//==============================================================================
bool IlanaSynthAudioProcessor::isFxPatcherOn() const
{
    return getRawParam (fxPatcherRef) > 0.5f;
}

void IlanaSynthAudioProcessor::setFxPatcherOn (bool on)
{
    const juce::ScopedLock edit (fxEditLock);
    if (on && ! getFxPatch().hasGraph)
    {
        // The rack as it is, so turning the patcher on changes nothing heard.
        auto patch = getFxPatch();
        patch.graph = getFxRackGraph();
        for (const auto& node : std::vector<PatchGraph::Node> (patch.graph.nodes()))
            patch.graph.setBypassed (node.id, false); // a slot's bypass is its parameter
        patch.hasGraph = true;
        publishFxPatch (std::move (patch));
        reconcileFxPatch(); // the labels
    }
    if (auto* parameter = apvts.getParameter (fxPatcherRef.id))
        parameter->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

PatchGraph::Graph IlanaSynthAudioProcessor::getFxPatchGraph() const
{
    const auto patch = getFxPatch();
    if (! patch.hasGraph)
        return const_cast<IlanaSynthAudioProcessor*> (this)->getFxRackGraph();
    auto graph = patch.graph;
    for (const auto& node : std::vector<PatchGraph::Node> (graph.nodes()))
        if (! node.isFixed() && juce::isPositiveAndBelow (node.ref - 1, numSlots))
            graph.setBypassed (node.id, getRawParam (fxSlotIds[(size_t) (node.ref - 1)].bypass) > 0.5f);
    return graph;
}

void IlanaSynthAudioProcessor::setFxPatchGraph (const PatchGraph::Graph& graph)
{
    const juce::ScopedLock edit (fxEditLock);
    auto patch = getFxPatch();
    auto next = graph;
    for (const auto& node : std::vector<PatchGraph::Node> (next.nodes()))
        next.setBypassed (node.id, false);
    limitLoopGains (next, patch.hasGraph ? &patch.graph : nullptr);
    patch.graph = std::move (next);
    patch.hasGraph = true;
    if (! FxPatchData::samePatch (patch, getFxPatch()))
        publishFxPatch (std::move (patch));
}

float IlanaSynthAudioProcessor::getFxLoopLatencyMs() const
{
    const auto patch = getFxPatch();
    if (! patch.hasGraph)
        return 0.0f;
    const auto loops = patch.graph.feedbackCables();
    if (std::find (loops.begin(), loops.end(), true) == loops.end())
        return 0.0f;
    return (float) (1000.0 * (double) PatchGraph::CompileOptions().feedbackChunk / juce::jmax (1.0, currentSampleRate));
}

//==============================================================================
// Instances.

bool IlanaSynthAudioProcessor::isFxInstance (int slotIndex) const
{
    return juce::isPositiveAndBelow (slotIndex, numSlots) && getFxPatch().slots[(size_t) slotIndex].own;
}

bool IlanaSynthAudioProcessor::isFxSlotInFront (int slotIndex) const
{
    return ! juce::isPositiveAndBelow (slotIndex, numSlots) || ! getFxPatch().slots[(size_t) slotIndex].hasValues;
}

int IlanaSynthAudioProcessor::addFxEffect (int type)
{
    const juce::ScopedLock edit (fxEditLock);
    if (type <= 0 || type > numFxTypes)
        return -1;

    auto slot = -1;
    auto existing = false;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto held = (int) getRawParam (fxSlotIds[(size_t) s].type);
        if (held == 0 && slot < 0)
            slot = s;
        existing = existing || held == type;
    }
    if (slot < 0)
        return -1;

    if (existing)
    {
        // A new instance, in front with the settings the type's card shows
        // now; the instances that were in front keep a copy of them.
        auto patch = getFxPatch();
        const auto snapshot = snapshotFxParams (type);
        for (int s = 0; s < numSlots; ++s)
            if ((int) getRawParam (fxSlotIds[(size_t) s].type) == type && ! patch.slots[(size_t) s].hasValues)
            {
                auto& other = patch.slots[(size_t) s];
                other.type = type;
                other.hasValues = true;
                other.values = snapshot;
            }
        auto& added = patch.slots[(size_t) slot];
        added = {};
        added.type = type;
        added.own = true;
        publishFxPatch (std::move (patch));
    }

    assignFxSlot (slot + 1, type);
    reconcileFxPatch();
    return slot;
}

void IlanaSynthAudioProcessor::removeFxEffect (int slotIndex)
{
    const juce::ScopedLock edit (fxEditLock);
    if (! juce::isPositiveAndBelow (slotIndex, numSlots))
        return;
    const auto type = (int) getRawParam (fxSlotIds[(size_t) slotIndex].type);

    // The card in front going: another instance of it comes forward first.
    if (type != 0 && ! getFxPatch().slots[(size_t) slotIndex].hasValues)
        for (int s = 0; s < numSlots; ++s)
            if (s != slotIndex && (int) getRawParam (fxSlotIds[(size_t) s].type) == type)
            {
                if (getFxPatch().slots[(size_t) s].hasValues)
                    focusFxInstance (s);
                break;
            }

    auto patch = getFxPatch();
    patch.slots[(size_t) slotIndex] = {};
    publishFxPatch (std::move (patch));
    if (auto* parameter = apvts.getParameter ("fx_slot" + juce::String (slotIndex + 1)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (0.0f));
    reconcileFxPatch();
}

// The swap in three steps, so nothing is heard: (1) the instances in front
// get a copy of the parameters' settings while the new one still reads its
// own; (2) the parameters take the new one's settings; (3) the new one reads
// the parameters.
void IlanaSynthAudioProcessor::focusFxInstance (int slotIndex)
{
    const juce::ScopedLock edit (fxEditLock);
    if (! juce::isPositiveAndBelow (slotIndex, numSlots))
        return;
    auto patch = getFxPatch();
    const auto chosen = patch.slots[(size_t) slotIndex];
    const auto type = (int) getRawParam (fxSlotIds[(size_t) slotIndex].type);
    if (! chosen.hasValues || type == 0 || chosen.type != type)
        return;

    const auto snapshot = snapshotFxParams (type);
    for (int s = 0; s < numSlots; ++s)
        if (s != slotIndex && (int) getRawParam (fxSlotIds[(size_t) s].type) == type && ! patch.slots[(size_t) s].hasValues)
        {
            auto& other = patch.slots[(size_t) s];
            other.type = type;
            other.hasValues = true;
            other.values = snapshot;
        }
    publishFxPatch (patch);

    writeFxParams (chosen.values);

    auto& front = patch.slots[(size_t) slotIndex];
    front.hasValues = false;
    front.values.clear();
    publishFxPatch (std::move (patch));
}

void IlanaSynthAudioProcessor::makeFxInstance (int slotIndex)
{
    const juce::ScopedLock edit (fxEditLock);
    if (! juce::isPositiveAndBelow (slotIndex, numSlots))
        return;
    const auto type = (int) getRawParam (fxSlotIds[(size_t) slotIndex].type);
    auto patch = getFxPatch();
    auto& slot = patch.slots[(size_t) slotIndex];
    if (type == 0 || slot.own)
        return;
    slot.type = type;
    slot.own = true;
    if (! slot.hasValues)
    {
        slot.hasValues = true;
        slot.values = snapshotFxParams (type);
    }
    publishFxPatch (std::move (patch));
    focusFxInstance (slotIndex);
}

// A slot is its effect plus its bypass, solo, MIX and band, its instance and
// its node: they move together.
void IlanaSynthAudioProcessor::swapFxSlots (int a, int b)
{
    const juce::ScopedLock edit (fxEditLock);
    if (a == b || ! juce::isPositiveAndBelow (a, numSlots) || ! juce::isPositiveAndBelow (b, numSlots))
        return;

    std::swap (ownedEngines[(size_t) a], ownedEngines[(size_t) b]);
    std::swap (ownedValues[(size_t) a], ownedValues[(size_t) b]);
    for (const auto s : { a, b })
    {
        slotEngines[(size_t) s].store (ownedEngines[(size_t) s].get(), std::memory_order_release);
        slotValues[(size_t) s].store (ownedValues[(size_t) s] != nullptr ? ownedValues[(size_t) s]->data() : nullptr, std::memory_order_release);
    }

    auto patch = getFxPatch();
    std::swap (patch.slots[(size_t) a], patch.slots[(size_t) b]);
    if (patch.hasGraph)
        for (const auto& node : std::vector<PatchGraph::Node> (patch.graph.nodes()))
            if (auto* editable = patch.graph.find (node.id); editable != nullptr && ! node.isFixed())
                editable->ref = node.ref == a + 1 ? b + 1 : node.ref == b + 1 ? a + 1 : node.ref;
    publishFxPatch (std::move (patch));

    for (const auto* suffix : { "", "_bypass", "_solo", "_mix", "_band" })
    {
        auto* first = apvts.getParameter ("fx_slot" + juce::String (a + 1) + suffix);
        auto* second = apvts.getParameter ("fx_slot" + juce::String (b + 1) + suffix);
        if (first != nullptr && second != nullptr)
        {
            const auto firstValue = first->getValue();
            const auto secondValue = second->getValue();
            first->setValueNotifyingHost (secondValue);
            second->setValueNotifyingHost (firstValue);
        }
    }
    reconcileFxPatch(); // the numbered labels
}

void IlanaSynthAudioProcessor::FxSlotTypeListener::parameterValueChanged (int, float)
{
    // Any thread (host automation runs on the audio thread): flag and post.
    if (! processor.fxReconcileWanted.exchange (true))
        processor.triggerAsyncUpdate();
}

//==============================================================================
// The compiled plans.

// What the rack's plan depends on: the routing and, per slot, whether it
// holds an effect, is bypassed, is soloed. (What the slot's effect is, its
// MIX and band are read by runFxSlot as it runs.)
std::uint64_t IlanaSynthAudioProcessor::fxRackSignature()
{
    std::uint64_t signature = (std::uint64_t) 1 << 63;
    if (getParam (fxRoutingRef) > 0.5f)
        signature |= (std::uint64_t) 1 << 62;
    for (int s = 0; s < numSlots; ++s)
    {
        const auto& ids = fxSlotIds[(size_t) s];
        const auto bits = ((int) getParam (ids.type) != 0 ? 1u : 0u) | (getParam (ids.bypass) > 0.5f ? 2u : 0u)
                          | (getParam (ids.solo) > 0.5f ? 4u : 0u);
        signature |= (std::uint64_t) bits << (3 * s);
    }
    return signature;
}

void IlanaSynthAudioProcessor::compileFxRackPlan (std::uint64_t signature)
{
    const juce::SpinLock::ScopedLockType lock (fxCompileLock);
    // The slots read twice around the graph: a change in between means the
    // graph may not be the signature's, so read it again.
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const auto graph = getFxRackGraph();
        const auto after = fxRackSignature();
        if (after != signature)
        {
            signature = after;
            continue;
        }
        PatchGraph::CompileOptions options;
        options.inPlace = fxNodeInPlace;
        auto plan = PatchGraph::compile (graph, options);
        plan.tag = signature;
        fxRackRunner->publish (plan);
        return;
    }
}

void IlanaSynthAudioProcessor::compileFxPatchPlan()
{
    const auto patch = getFxPatch();
    PatchGraph::Plan plan;
    if (patch.hasGraph)
    {
        PatchGraph::CompileOptions options;
        options.inPlace = fxNodeInPlace;
        plan = PatchGraph::compile (patch.graph, options);
    }
    else
    {
        plan.ok = false; // no graph yet: the rack runs as SERIES / PARALLEL
    }
    const juce::SpinLock::ScopedLockType lock (fxCompileLock);
    fxPatchRunner->publish (plan);
}

void IlanaSynthAudioProcessor::compileFxPlansNow()
{
    if (fxReconcileWanted.exchange (false))
        reconcileFxPatch();
    compileFxRackPlan (fxRackSignature());
    compileFxPatchPlan();
}

bool IlanaSynthAudioProcessor::isFxRackPlanCurrent()
{
    const auto* plan = fxRackRunner->publishedPlan();
    return plan != nullptr && plan->tag == fxRackSignature();
}

// PATCHER: each node runs its slot as the rack does (MIX, band, solo,
// meters, CPU share); a bypassed or empty slot's node passes its input on.
// Nodes whose input has been silent past their tail sleep (ROUTING-CPU A5).
void IlanaSynthAudioProcessor::processEffectsPatched (juce::AudioBuffer<float>& buffer)
{
    const auto* plan = fxPatchRunner->activePlan();
    if (plan == nullptr || ! plan->ok)
        return processEffectsFixed (buffer);

    for (int index = 0; index < numSlots; ++index)
        if (! isFxSlotActive (index))
            clearFxSlotMeters (index);

    fxPatchRunner->process (buffer, [this] (const PatchGraph::CompiledNode& node, PatchGraph::NodeBuffers& io, int length)
    {
        if (io.numOutputs <= 0 || io.numInputs <= 0)
            return;
        auto& out = *io.out[0];
        if (! node.inPlace && io.in[0] != io.out[0])
            for (int c = 0; c < out.getNumChannels(); ++c)
                out.copyFrom (c, 0, *io.in[0], c, 0, length);
        if (node.ref >= 1 && node.ref <= numSlots && isFxSlotActive (node.ref - 1))
            runFxSlot (node.ref, out);
    });

    // (The plan again: process() may have taken up a new one.)
    if (const auto* running = fxPatchRunner->activePlan())
        for (const auto& node : running->nodes)
            if (node.ref >= 1 && node.ref <= numSlots)
                if (const auto* stats = fxPatchRunner->statsFor (node.id); stats != nullptr && stats->sleeping.load (std::memory_order_relaxed))
                    clearFxSlotMeters (node.ref - 1);

    sanitiseBuffer (buffer);
}

//==============================================================================
// The card displays read the engine of the card in front of each effect.
const IlanaSynthAudioProcessor::FxEngine& IlanaSynthAudioProcessor::displayEngine (int type) const
{
    const auto patch = getFxPatch();
    for (int s = 0; s < numSlots; ++s)
        if ((int) getRawParam (fxSlotIds[(size_t) s].type) == type && ! patch.slots[(size_t) s].hasValues)
            return engineForSlot (s);
    return *fxMain;
}
