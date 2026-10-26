// Duplicating and reordering oscillators (ilana 2026-10-26: "add option to
// duplicate osc", "an easier way to reorder and delete oscs", "drag to
// reorder"). An oscillator is its oscN_* parameters plus what points at it
// by number: the FM matrix, FM noise, its PATCH cable, the vector pad's
// corners, the mod routes into its knobs, its sample and its shown bit. A
// swap trades all of these between two slots, so the card order changes and
// the sound doesn't. Message thread; the page wraps each in performEdit.

#include "ProcessorInternal.h"

namespace
{
using D = Mod::Destination;
constexpr int numOscs = OscillatorIds::count;

// The explicit per-oscillator targets, OSC 3 under its legacy "Sub".
constexpr int numOscFields = 10;
constexpr D oscTargets[numOscs][numOscFields] {
    { D::Osc1Pitch, D::Osc1Frame, D::Osc1Level, D::Osc1SampleStart, D::Osc1SampleEnd, D::Osc1Detune, D::Osc1Pan, D::Osc1Warp, D::Osc1Blend, D::Osc1Spread },
    { D::Osc2Pitch, D::Osc2Frame, D::Osc2Level, D::Osc2SampleStart, D::Osc2SampleEnd, D::Osc2Detune, D::Osc2Pan, D::Osc2Warp, D::Osc2Blend, D::Osc2Spread },
    { D::SubPitch, D::SubFrame, D::SubLevel, D::SubSampleStart, D::SubSampleEnd, D::SubDetune, D::SubPan, D::SubWarp, D::SubBlend, D::SubSpread },
    { D::Osc4Pitch, D::Osc4Frame, D::Osc4Level, D::Osc4SampleStart, D::Osc4SampleEnd, D::Osc4Detune, D::Osc4Pan, D::Osc4Warp, D::Osc4Blend, D::Osc4Spread },
    { D::Osc5Pitch, D::Osc5Frame, D::Osc5Level, D::Osc5SampleStart, D::Osc5SampleEnd, D::Osc5Detune, D::Osc5Pan, D::Osc5Warp, D::Osc5Blend, D::Osc5Spread },
    { D::Osc6Pitch, D::Osc6Frame, D::Osc6Level, D::Osc6SampleStart, D::Osc6SampleEnd, D::Osc6Detune, D::Osc6Pan, D::Osc6Warp, D::Osc6Blend, D::Osc6Spread },
};

// The nine original FM cells are explicit destinations; the rest are
// parameter destinations named by their fm_* IDs.
int fmCellDestination (int source, int target)
{
    static const auto table = []
    {
        constexpr D cells[3][3] { { D::FmFeedback, D::Fm1to2, D::Fm1to3 },
                                  { D::FmAmount, D::Fm2Feedback, D::Fm2to3 },
                                  { D::Fm3to1, D::Fm3to2, D::Fm3Feedback } };
        std::array<std::array<int, numOscs>, numOscs> result {};
        for (int s = 0; s < numOscs; ++s)
            for (int t = 0; t < numOscs; ++t)
                result[(size_t) s][(size_t) t] = s < 3 && t < 3 ? (int) cells[s][t]
                                                                : Mod::destinationForParamId (IlanaSynthAudioProcessor::fmRouteId (s, t));
        return result;
    }();
    return table[(size_t) source][(size_t) target];
}

// The suffixes every oscillator has its own parameter for. (Read from OSC
// 1, 2 and 4-6: "sub_" also starts the SUB card's own sub_colour.)
const std::vector<juce::String>& oscSuffixes (const juce::AudioProcessor& processor)
{
    static const auto suffixes = [&processor]
    {
        std::set<juce::String> found;
        for (auto* parameter : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                for (const auto* prefix : { "osc1_", "osc2_", "osc4_", "osc5_", "osc6_" })
                    if (withId->paramID.startsWith (prefix))
                        found.insert (withId->paramID.substring (5));
        return std::vector<juce::String> (found.begin(), found.end());
    }();
    return suffixes;
}

juce::String oscParamId (int osc, const juce::String& suffix)
{
    return juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_" + suffix;
}

// What a mod destination points at, in oscillator terms.
struct OscTarget
{
    enum Kind { none, field, fmCell, param, fmNoise } kind = none;
    int a = -1, b = -1;   // the oscillator (field, param, fmNoise), or an FM cell's source and target
    int fieldIndex = -1;  // field: the column of oscTargets
    juce::String suffix;  // param: the oscN_ suffix

    bool involves (int osc) const { return kind != none && (a == osc || b == osc); }
};

OscTarget decodeTarget (const juce::AudioProcessor& processor, int destination)
{
    OscTarget target;
    if (destination <= 0)
        return target;

    for (int osc = 0; osc < numOscs; ++osc)
        for (int f = 0; f < numOscFields; ++f)
            if ((int) oscTargets[osc][f] == destination)
            {
                target.kind = OscTarget::field;
                target.a = osc;
                target.fieldIndex = f;
                return target;
            }

    for (int source = 0; source < numOscs; ++source)
        for (int to = 0; to < numOscs; ++to)
            if (fmCellDestination (source, to) == destination)
            {
                target.kind = OscTarget::fmCell;
                target.a = source;
                target.b = to;
                return target;
            }

    const auto index = Mod::paramDestinationIndex (destination);
    if (index < 0)
        return target;

    const juce::String id (Mod::getParamDestinations()[(size_t) index].id);
    for (int osc = 0; osc < numOscs; ++osc)
    {
        if (id == "fm_noise" + juce::String (osc + 1))
        {
            target.kind = OscTarget::fmNoise;
            target.a = osc;
            return target;
        }

        const auto prefix = juce::String (OscillatorIds::prefixes[(size_t) osc]) + "_";
        if (id.startsWith (prefix))
        {
            const auto suffix = id.substring (prefix.length());
            const auto& suffixes = oscSuffixes (processor);
            if (std::find (suffixes.begin(), suffixes.end(), suffix) != suffixes.end())
            {
                target.kind = OscTarget::param;
                target.a = osc;
                target.suffix = suffix;
                return target;
            }
        }
    }
    return target;
}

// The destination with each oscillator moved by map; 0 if it has none.
int encodeTarget (const OscTarget& target, const std::array<int, numOscs>& map)
{
    switch (target.kind)
    {
        case OscTarget::field: return (int) oscTargets[map[(size_t) target.a]][target.fieldIndex];
        case OscTarget::fmCell: return fmCellDestination (map[(size_t) target.a], map[(size_t) target.b]);
        case OscTarget::fmNoise: return Mod::destinationForParamId ("fm_noise" + juce::String (map[(size_t) target.a] + 1));
        case OscTarget::param: return Mod::destinationForParamId (oscParamId (map[(size_t) target.a], target.suffix));
        case OscTarget::none: break;
    }
    return 0;
}

std::array<int, numOscs> swapMap (int a, int b)
{
    std::array<int, numOscs> map {};
    for (int i = 0; i < numOscs; ++i)
        map[(size_t) i] = i == a ? b : i == b ? a : i;
    return map;
}

const juce::StringArray modSlotFields { "src", "dst", "amt", "curve", "pol", "aux", "byp", "stereo" };
} // namespace

float IlanaSynthAudioProcessor::readNormalised (const juce::String& id) const
{
    if (auto* parameter = apvts.getParameter (id))
        return parameter->getValue();
    return 0.0f;
}

void IlanaSynthAudioProcessor::writeNormalised (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
        if (parameter->getValue() != value)
            parameter->setValueNotifyingHost (value);
}

void IlanaSynthAudioProcessor::swapOscillators (int a, int b)
{
    if (a == b || ! juce::isPositiveAndBelow (a, numOscs) || ! juce::isPositiveAndBelow (b, numOscs))
        return;

    const auto map = swapMap (a, b);

    // Each value is read before anything is written.
    std::vector<std::pair<juce::String, float>> writes;
    const auto trade = [this, &writes] (const juce::String& first, const juce::String& second)
    {
        if (apvts.getParameter (first) == nullptr || apvts.getParameter (second) == nullptr)
            return;
        writes.emplace_back (first, readNormalised (second));
        writes.emplace_back (second, readNormalised (first));
    };

    for (const auto& suffix : oscSuffixes (*this))
        trade (oscParamId (a, suffix), oscParamId (b, suffix));

    for (int source = 0; source < numOscs; ++source)
        for (int target = 0; target < numOscs; ++target)
            writes.emplace_back (fmRouteId (source, target), readNormalised (fmRouteId (map[(size_t) source], map[(size_t) target])));

    trade ("fm_noise" + juce::String (a + 1), "fm_noise" + juce::String (b + 1));
    trade (VoiceRouting::cableParamId (a), VoiceRouting::cableParamId (b));

    for (const auto* corner : { "vec_a", "vec_b", "vec_c", "vec_d" })
        if (auto* parameter = apvts.getParameter (corner))
        {
            const auto osc = juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue()));
            if (juce::isPositiveAndBelow (osc, numOscs))
                writes.emplace_back (corner, parameter->convertTo0to1 ((float) map[(size_t) osc]));
        }

    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto destination = readModSlot (slot).destination;
        const auto target = decodeTarget (*this, destination);
        if (target.involves (a) || target.involves (b))
            if (const auto moved = encodeTarget (target, map); moved > 0)
                if (auto* parameter = apvts.getParameter (getModSlotParamId (slot, "dst")))
                    writes.emplace_back (getModSlotParamId (slot, "dst"), parameter->convertTo0to1 ((float) moved));
    }

    // The sample and the shown bit.
    const auto before = capturePatchData();
    auto after = before;
    for (auto* arrays : { &after.embedded, &after.liveSamples })
        std::swap ((*arrays)[(size_t) a], (*arrays)[(size_t) b]);
    std::swap (after.samplePaths[(size_t) a], after.samplePaths[(size_t) b]);
    std::swap (after.sampleOptions[(size_t) a], after.sampleOptions[(size_t) b]);
    auto& shown = after.reveal[(size_t) Module::Oscillator];
    const auto bitA = (shown >> a) & 1, bitB = (shown >> b) & 1;
    shown = (shown & ~((1 << a) | (1 << b))) | (bitA << b) | (bitB << a);

    for (const auto& [id, value] : writes)
        writeNormalised (id, value);
    restorePatchData (after, before);
}

int IlanaSynthAudioProcessor::moveOscillator (int osc, int position)
{
    std::vector<int> shown;
    for (int i = 0; i < numOscs; ++i)
        if (isOscillatorShown (i))
            shown.push_back (i);

    auto at = (int) (std::find (shown.begin(), shown.end(), osc) - shown.begin());
    if (at >= (int) shown.size())
        return osc;

    position = juce::jlimit (0, (int) shown.size() - 1, position);
    for (; at < position; ++at)
        swapOscillators (shown[(size_t) at], shown[(size_t) at + 1]);
    for (; at > position; --at)
        swapOscillators (shown[(size_t) at], shown[(size_t) at - 1]);
    return shown[(size_t) position];
}

int IlanaSynthAudioProcessor::duplicateOscillator (int from)
{
    if (! juce::isPositiveAndBelow (from, numOscs) || ! isOscillatorShown (from))
        return -1;

    auto copy = -1;
    for (int i = 0; i < numOscs && copy < 0; ++i)
        if (! isOscillatorShown (i))
            copy = i;
    if (copy < 0)
        return -1;

    // A removed oscillator keeps its settings: its old routes go first.
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
        if (decodeTarget (*this, readModSlot (slot).destination).involves (copy))
            clearModSlot (slot);

    for (const auto& suffix : oscSuffixes (*this))
        if (apvts.getParameter (oscParamId (copy, suffix)) != nullptr)
            writeNormalised (oscParamId (copy, suffix), readNormalised (oscParamId (from, suffix)));

    // The copy plays what the original plays: the same FM in and out (not
    // between the two) and the same feedback.
    for (int other = 0; other < numOscs; ++other)
    {
        const auto own = other == from || other == copy;
        writeNormalised (fmRouteId (copy, other), own ? 0.0f : readNormalised (fmRouteId (from, other)));
        writeNormalised (fmRouteId (other, copy), own ? 0.0f : readNormalised (fmRouteId (other, from)));
    }
    writeNormalised (fmRouteId (copy, copy), readNormalised (fmRouteId (from, from)));
    writeNormalised ("fm_noise" + juce::String (copy + 1), readNormalised ("fm_noise" + juce::String (from + 1)));
    writeNormalised (VoiceRouting::cableParamId (copy), readNormalised (VoiceRouting::cableParamId (from)));

    // Its mod routes, each into a free slot while there is one.
    const auto map = swapMap (from, copy);
    for (int slot = 0; slot < Mod::maxSlots; ++slot)
    {
        const auto target = decodeTarget (*this, readModSlot (slot).destination);
        if (! target.involves (from) || target.involves (copy))
            continue;

        const auto destination = encodeTarget (target, map);
        if (destination <= 0)
            continue;

        auto free = -1;
        for (int i = 0; i < Mod::maxSlots && free < 0; ++i)
        {
            const auto candidate = readModSlot (i);
            if (candidate.source == Mod::Source::None && candidate.destination == 0)
                free = i;
        }
        if (free < 0)
            break;

        for (const auto& field : modSlotFields)
            writeNormalised (getModSlotParamId (free, field), readNormalised (getModSlotParamId (slot, field)));
        setModSlotValue (free, "dst", (float) destination);
        setModRemap (free, getModRemap (slot));
    }

    const auto before = capturePatchData();
    auto after = before;
    after.embedded[(size_t) copy] = before.embedded[(size_t) from];
    after.liveSamples[(size_t) copy] = before.liveSamples[(size_t) from];
    after.samplePaths[(size_t) copy] = before.samplePaths[(size_t) from];
    after.sampleOptions[(size_t) copy] = before.sampleOptions[(size_t) from];
    after.reveal[(size_t) Module::Oscillator] |= 1 << copy;
    restorePatchData (after, before);

    // The copy's card comes right after the original's.
    auto position = 0;
    for (int i = 0; i < from; ++i)
        if (isOscillatorShown (i))
            ++position;
    return moveOscillator (copy, copy < from ? position : position + 1);
}
