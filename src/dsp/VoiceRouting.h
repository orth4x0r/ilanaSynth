#pragma once

#include <array>
#include <cstdint>
#include <string>

// Reroutable oscillators (roadmap step 16): where each oscillator goes inside
// the voice, set by cables on the PATCH tab.
//
//   Patch    what the cables say: for each source (OSC 1-6, the SUB + NOISE
//            pair) and each stage (Filter 1, Filter 2, WEST, BODY) the set of
//            places its output goes (F1, F2, WEST, BODY, OUT), as a 5-bit
//            mask. Saved as the appended choice parameters cable_* (one per
//            source and stage) and switched on by route_cables. Off, as in
//            every older patch, the voice runs its fixed chain (the _route
//            menus, SERIAL / PARALLEL and WEST's position). BODY is the
//            physical model's body (the Classic resonator or a material
//            body): with nothing cabled into it, it is the body of every
//            older patch, struck by the strings and ringing beside the
//            chain; cabled in, audio runs through it and it goes where its
//            cable says.
//
//            In the saved parameters the bits are F1 1, F2 2, WEST 4, OUT 8
//            (as before BODY existed) and BODY 16; in the code BODY sits
//            before OUT (so a stage's number is its bit). toParamMask /
//            fromParamMask swap the two at the parameters.
//   compile  the patch to a Table the voice indexes (docs/ROUTING-CPU.md,
//            rules B1-B3): the stages in the order they run, which of them
//            are used (a stage with no input or not heard is skipped), and
//            one bus per distinct source mask, so the voice's source loops
//            keep adding each oscillator into one bus as they do now.
//
// Plain C++ (no JUCE) so the processor, the voice, the page and the tests
// share it. Loops between stages are refused by the page; a loop that comes
// in anyway (an edited file) keeps only the edges running F1 -> F2 -> WEST.
namespace VoiceRouting
{
enum Target : std::uint8_t
{
    toF1 = 1,
    toF2 = 2,
    toWest = 4,
    toBody = 8,
    toOut = 16,
    allTargets = 31
};

// The saved masks keep OUT at 8 and put BODY at 16 (the bits swap).
inline constexpr std::uint8_t swapBodyAndOut (std::uint8_t mask)
{
    return (std::uint8_t) ((mask & 7) | ((mask & 8) != 0 ? 16 : 0) | ((mask & 16) != 0 ? 8 : 0));
}
inline constexpr std::uint8_t toParamMask (std::uint8_t mask) { return swapBodyAndOut (mask); }
inline constexpr std::uint8_t fromParamMask (std::uint8_t mask) { return swapBodyAndOut (mask); }

inline constexpr int numOscSources = 6;
inline constexpr int subNoiseSource = 6; // the SUB + NOISE pair shares one route
inline constexpr int numSources = 7;

enum Stage
{
    f1 = 0,
    f2 = 1,
    west = 2,
    body = 3,
    numStages = 4,
    out = 4 // (a destination, not a stage)
};

inline constexpr std::uint8_t bitOf (int destination) { return (std::uint8_t) (1u << destination); }

// The voice's buses: the fixed chain uses FilterRoute::Count (5); the cables
// one per distinct source mask (at most 7), and a muted source writes the
// last bus, which nothing reads.
inline constexpr int maxBuses = 8;
inline constexpr int mutedBus = maxBuses - 1;

struct Patch
{
    std::array<std::uint8_t, numSources> source {};
    std::array<std::uint8_t, numStages> stage {};

    bool operator== (const Patch& other) const { return source == other.source && stage == other.stage; }
    bool operator!= (const Patch& other) const { return ! (*this == other); }
};

struct Table
{
    bool on = false;
    std::array<int, numSources> busOf {};         // the bus each source adds into
    std::array<std::uint8_t, maxBuses> busMask {}; // where each bus goes (only used stages and OUT)
    int numBuses = 0;
    std::array<int, numStages> order {};          // the used stages, in the order they run
    int numOrdered = 0;
    std::array<bool, numStages> used {};
    std::array<std::uint8_t, numStages> stageOut {}; // each used stage's destinations (only used stages and OUT)
    // Destinations that both filters feed: there the two are mixed as the
    // fixed chain's PARALLEL mixes them ((F1 * g1 + F2 * g2) * 0.7071,
    // BALANCE setting g1 and g2), so two filters side by side don't jump 3 dB.
    std::uint8_t filterPairInto = 0;
};

// The stage-to-stage edges with a stage's own bit removed.
inline Patch withoutSelfEdges (Patch patch)
{
    for (int s = 0; s < numStages; ++s)
        patch.stage[(size_t) s] = (std::uint8_t) (patch.stage[(size_t) s] & ~bitOf (s) & allTargets);
    for (auto& mask : patch.source)
        mask = (std::uint8_t) (mask & allTargets);
    return patch;
}

// The stages in an order where every edge between them runs forward; false
// (and order untouched) when they form a loop.
inline bool topologicalOrder (const Patch& patch, std::array<int, numStages>& order)
{
    std::array<int, numStages> indegree {};
    for (int from = 0; from < numStages; ++from)
        for (int to = 0; to < numStages; ++to)
            if (from != to && (patch.stage[(size_t) from] & bitOf (to)) != 0)
                ++indegree[(size_t) to];

    std::array<int, numStages> result {};
    std::array<bool, numStages> placed {};
    for (int count = 0; count < numStages; ++count)
    {
        auto next = -1;
        for (int s = 0; s < numStages && next < 0; ++s)
            if (! placed[(size_t) s] && indegree[(size_t) s] == 0)
                next = s;
        if (next < 0)
            return false;
        placed[(size_t) next] = true;
        result[(size_t) count] = next;
        for (int to = 0; to < numStages; ++to)
            if (to != next && (patch.stage[(size_t) next] & bitOf (to)) != 0)
                --indegree[(size_t) to];
    }
    order = result;
    return true;
}

inline bool hasLoop (const Patch& patch)
{
    std::array<int, numStages> order {};
    return ! topologicalOrder (withoutSelfEdges (patch), order);
}

// A patch with a loop between stages keeps only its forward edges (F1 before
// F2 before WEST before BODY).
inline Patch withoutLoops (Patch patch)
{
    patch = withoutSelfEdges (patch);
    if (! hasLoop (patch))
        return patch;
    for (int from = 0; from < numStages; ++from)
        for (int to = 0; to < from; ++to)
            patch.stage[(size_t) from] = (std::uint8_t) (patch.stage[(size_t) from] & ~bitOf (to));
    return patch;
}

inline Table compile (const Patch& input)
{
    const auto patch = withoutLoops (input);
    Table table;
    table.on = true;

    std::array<int, numStages> order {};
    topologicalOrder (patch, order);

    // A stage hears something when a source or a stage that hears something
    // feeds it (in order, so upstream is settled first) ...
    std::array<bool, numStages> hasInput {};
    for (const auto s : order)
    {
        for (const auto mask : patch.source)
            hasInput[(size_t) s] = hasInput[(size_t) s] || (mask & bitOf (s)) != 0;
        for (int from = 0; from < numStages; ++from)
            hasInput[(size_t) s] = hasInput[(size_t) s] || (hasInput[(size_t) from] && from != s && (patch.stage[(size_t) from] & bitOf (s)) != 0);
    }
    // ... and is heard when it reaches OUT (in reverse order).
    std::array<bool, numStages> heard {};
    for (int k = numStages - 1; k >= 0; --k)
    {
        const auto s = order[(size_t) k];
        heard[(size_t) s] = (patch.stage[(size_t) s] & toOut) != 0;
        for (int to = 0; to < numStages; ++to)
            heard[(size_t) s] = heard[(size_t) s] || (heard[(size_t) to] && to != s && (patch.stage[(size_t) s] & bitOf (to)) != 0);
    }

    // What a used stage or a bus may feed: OUT and the used stages.
    std::uint8_t live = toOut;
    for (int s = 0; s < numStages; ++s)
    {
        table.used[(size_t) s] = hasInput[(size_t) s] && heard[(size_t) s];
        if (table.used[(size_t) s])
            live = (std::uint8_t) (live | bitOf (s));
    }

    for (const auto s : order)
        if (table.used[(size_t) s])
        {
            table.order[(size_t) table.numOrdered++] = s;
            table.stageOut[(size_t) s] = (std::uint8_t) (patch.stage[(size_t) s] & live);
        }

    if (table.used[f1] && table.used[f2])
        table.filterPairInto = (std::uint8_t) (table.stageOut[f1] & table.stageOut[f2]);

    // One bus per distinct mask; a source that reaches nothing heard is muted.
    for (int source = 0; source < numSources; ++source)
    {
        const auto mask = (std::uint8_t) (patch.source[(size_t) source] & live);
        auto bus = mutedBus;
        if (mask != 0)
        {
            for (int b = 0; b < table.numBuses && bus == mutedBus; ++b)
                if (table.busMask[(size_t) b] == mask)
                    bus = b;
            if (bus == mutedBus)
            {
                bus = table.numBuses++;
                table.busMask[(size_t) bus] = mask;
            }
        }
        table.busOf[(size_t) source] = bus;
    }
    return table;
}

// The cables that play what the fixed chain plays: the page shows these
// while the cables are off, and the first cable edit starts from them.
// routes: each source's _route choice (Default, Filter 1, Filter 2, No
// filter, Both); westPosition: After Filters 0, Replace Filter 2 1.
// The same sound, except for three corners the cables don't model (each a
// drive or a filter copy): in PARALLEL, Filter 2 no longer hears Filter 1's
// DRIVE on the shared input; Both becomes a cable to each filter (the fixed
// chain runs it through its own copy of the pair); and in WEST's Replace
// Filter 2 mode, Filter 2's DRIVE no longer comes before WEST.
inline Patch fromChain (const std::array<int, numSources>& routes, bool parallel, bool westOn, int westPosition)
{
    const auto westReplaces = westOn && westPosition == 1;
    const auto westAfter = westOn && westPosition == 0;
    const std::uint8_t secondFilter = westReplaces ? toWest : toF2;
    const std::uint8_t afterFilters = westAfter ? toWest : toOut;

    Patch patch;
    for (int source = 0; source < numSources; ++source)
    {
        switch (routes[(size_t) source])
        {
            case 1: patch.source[(size_t) source] = toF1; break;
            case 2: patch.source[(size_t) source] = secondFilter; break;
            case 3: patch.source[(size_t) source] = afterFilters; break;
            case 4: patch.source[(size_t) source] = (std::uint8_t) (toF1 | secondFilter); break;
            default: patch.source[(size_t) source] = parallel ? (std::uint8_t) (toF1 | secondFilter) : toF1; break;
        }
    }

    patch.stage[f1] = parallel ? afterFilters : secondFilter;
    // (While WEST takes its place Filter 2 hears nothing; its cable to what
    // follows is kept, so a cable into it plays at once.)
    patch.stage[f2] = afterFilters;
    patch.stage[west] = toOut;
    patch.stage[body] = toOut; // (nothing cabled into it: the body of the fixed chain)
    return patch;
}

// The default patch: every source into Filter 1, Filter 1 into Filter 2,
// Filter 2 out (today's default chain).
inline Patch defaultPatch() { return fromChain ({}, false, false, 0); }

// The parameters holding a patch: the sources (OSC 1-6, then SUB + NOISE),
// then the stages (F1, F2, WEST, BODY).
inline constexpr int numCableParams = numSources + numStages;

inline const char* cableParamId (int index)
{
    static const char* const ids[numCableParams] { "cable_osc1", "cable_osc2", "cable_osc3", "cable_osc4", "cable_osc5",
                                                   "cable_osc6", "cable_subnoise", "cable_f1", "cable_f2", "cable_west", "cable_body" };
    return ids[index];
}

inline const char* cableParamName (int index)
{
    static const char* const names[numCableParams] { "OSC 1 Cables", "OSC 2 Cables", "OSC 3 Cables", "OSC 4 Cables",
                                                     "OSC 5 Cables", "OSC 6 Cables", "Sub + Noise Cables",
                                                     "Filter 1 Cables", "Filter 2 Cables", "West Cables", "Body Cables" };
    return names[index];
}

// Choice names for a saved mask (the cable_* parameters' text, 32 of them:
// the first sixteen as before BODY existed): "F1 + OUT", "F2 + BODY + OUT".
inline const char* const* maskNames()
{
    static const auto names = []
    {
        static std::array<std::string, 32> text;
        static std::array<const char*, 32> pointers {};
        for (int mask = 0; mask < 32; ++mask)
        {
            std::string name;
            const auto add = [&name] (const char* part) { name += name.empty() ? "" : " + "; name += part; };
            if ((mask & 1) != 0) add ("F1");
            if ((mask & 2) != 0) add ("F2");
            if ((mask & 4) != 0) add ("WEST");
            if ((mask & 16) != 0) add ("BODY");
            if ((mask & 8) != 0) add ("OUT");
            if (mask == 0) name = "None";
            if (mask == 15) name = "All";
            if (mask == 31) name = "All + BODY";
            text[(size_t) mask] = name;
            pointers[(size_t) mask] = text[(size_t) mask].c_str();
        }
        return pointers;
    }();
    return names.data();
}
} // namespace VoiceRouting
