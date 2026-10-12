#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <string>
#include <array>
#include <utility>
#include <vector>

#include "Dx7Engine.h"

// M5: one-click FM routings for the operator diagram. Each sets the whole
// matrix (and which operators are heard); the amounts stay editable after.
// Operators are the oscillators, 0-based: 0 = OSC 1.
namespace FmAlgorithms
{
struct Algorithm
{
    const char* name;
    int numOperators;
    std::vector<std::pair<int, int>> routes; // source -> target
    std::vector<int> carriers;               // operators that are heard
    int feedbackOperator;                    // -1: none
};

inline const std::vector<Algorithm>& all()
{
    // The last seven follow the Yamaha DX7 algorithms they are named after
    // (DX operator n = OSC n, so OSC 1 is always a carrier). Review 6: the
    // grid shows only the first nine (BASIC); the DX7 pages show all 32
    // under their own numbers, these seven among them.
    static const std::vector<Algorithm> list {
        { "2-Op Stack", 2, { { 1, 0 } }, { 0 }, 1 },
        { "3-Op Stack", 3, { { 2, 1 }, { 1, 0 } }, { 0 }, 2 },
        { "Two Into One", 3, { { 1, 0 }, { 2, 0 } }, { 0 }, 2 },
        { "Pair + Sine", 3, { { 1, 0 } }, { 0, 2 }, 1 },
        { "Three Carriers", 3, {}, { 0, 1, 2 }, -1 },
        { "Two Pairs", 4, { { 1, 0 }, { 3, 2 } }, { 0, 2 }, 3 },
        { "4-Op Stack", 4, { { 3, 2 }, { 2, 1 }, { 1, 0 } }, { 0 }, 3 },
        { "Diamond", 4, { { 3, 1 }, { 3, 2 }, { 1, 0 }, { 2, 0 } }, { 0 }, 3 },
        { "One Drives Three", 4, { { 3, 0 }, { 3, 1 }, { 3, 2 } }, { 0, 1, 2 }, 3 },
        { "DX7 1", 6, { { 1, 0 }, { 5, 4 }, { 4, 3 }, { 3, 2 } }, { 0, 2 }, 5 },
        { "DX7 5", 6, { { 1, 0 }, { 3, 2 }, { 5, 4 } }, { 0, 2, 4 }, 5 },
        { "DX7 7", 6, { { 1, 0 }, { 3, 2 }, { 4, 2 }, { 5, 4 } }, { 0, 2 }, 5 },
        { "DX7 16", 6, { { 1, 0 }, { 3, 2 }, { 2, 0 }, { 5, 4 }, { 4, 0 } }, { 0 }, 5 },
        { "DX7 19", 6, { { 2, 1 }, { 1, 0 }, { 5, 3 }, { 5, 4 } }, { 0, 3, 4 }, 5 },
        { "DX7 22", 6, { { 1, 0 }, { 5, 2 }, { 5, 3 }, { 5, 4 } }, { 0, 2, 3, 4 }, 5 },
        { "DX7 32", 6, {}, { 0, 1, 2, 3, 4, 5 }, 5 },
    };

    return list;
}

inline int count() { return (int) all().size(); }

// The algorithms on the grid's BASIC page (the rest are DX7 ones).
constexpr int numBasic = 9;

// The DX7's 32 algorithms (number 1-32) in this matrix's terms: operator n
// is OSC n, with the DX7's carriers and its feedback operator.
inline const Algorithm& dx7 (int number)
{
    static const std::vector<Algorithm> list = []
    {
        static std::vector<std::string> names;
        names.reserve (32);
        std::vector<Algorithm> result;
        for (int a = 0; a < 32; ++a)
        {
            const auto routing = Dx7::routing (a);
            names.push_back ("DX7 " + std::to_string (a + 1));
            Algorithm algorithm { names.back().c_str(), 6, {}, {}, routing.feedbackOp };
            for (int source = 0; source < 6; ++source)
                for (int target = 0; target < 6; ++target)
                    if (routing.modulates[(size_t) source][(size_t) target] && source != target)
                        algorithm.routes.push_back ({ source, target });
            for (int op = 0; op < 6; ++op)
                if (routing.carrier[(size_t) op])
                    algorithm.carriers.push_back (op);
            result.push_back (std::move (algorithm));
        }
        return result;
    }();
    return list[(size_t) juce::jlimit (1, 32, number) - 1];
}

// How the DX7 manual describes each algorithm's layout, for tooltips.
inline juce::String dx7Description (int number)
{
    const auto& algorithm = dx7 (number);
    juce::String text = juce::String (algorithm.carriers.size()) + (algorithm.carriers.size() == 1 ? " carrier" : " carriers");
    // The tallest stack: the longest chain of modulators down to a carrier.
    std::array<int, 6> depth {};
    for (int pass = 0; pass < 6; ++pass)
        for (const auto& route : algorithm.routes)
            depth[(size_t) route.first] = std::max (depth[(size_t) route.first], depth[(size_t) route.second] + 1);
    const auto tallest = *std::max_element (depth.begin(), depth.end()) + 1;
    text << ", stacks up to " << tallest << (tallest == 1 ? " operator" : " operators") << " tall";
    if (algorithm.feedbackOperator >= 0)
        text << ", feedback on OSC " << (algorithm.feedbackOperator + 1);
    return text;
}

inline bool isCarrier (const Algorithm& algorithm, int op)
{
    return std::find (algorithm.carriers.begin(), algorithm.carriers.end(), op) != algorithm.carriers.end();
}

inline bool hasRoute (const Algorithm& algorithm, int source, int target)
{
    if (source == target)
        return algorithm.feedbackOperator == source;

    for (const auto& route : algorithm.routes)
        if (route.first == source && route.second == target)
            return true;

    return false;
}

// Amounts a new route starts at (existing routes keep theirs). On a patch
// whose operators play the Operator Env, the modulator's OUTPUT sets the
// depth, so routes start at 100% as a DX7 voice's do, and feedback at the
// DX7's 6 (25%).
constexpr float defaultRouteAmount = 0.35f;
constexpr float defaultFeedbackAmount = 0.15f;
constexpr float operatorEnvRouteAmount = 1.0f;
constexpr float operatorEnvFeedbackAmount = 0.25f;
} // namespace FmAlgorithms
