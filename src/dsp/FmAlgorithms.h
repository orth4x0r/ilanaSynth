#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

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
    // (DX operator n = OSC n, so OSC 1 is always a carrier).
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
        { "DX 1", 6, { { 1, 0 }, { 5, 4 }, { 4, 3 }, { 3, 2 } }, { 0, 2 }, 5 },
        { "DX 5 Keys", 6, { { 1, 0 }, { 3, 2 }, { 5, 4 } }, { 0, 2, 4 }, 5 },
        { "DX 7", 6, { { 1, 0 }, { 3, 2 }, { 4, 2 }, { 5, 4 } }, { 0, 2 }, 5 },
        { "DX 16", 6, { { 1, 0 }, { 3, 2 }, { 2, 0 }, { 5, 4 }, { 4, 0 } }, { 0 }, 5 },
        { "DX 19", 6, { { 2, 1 }, { 1, 0 }, { 5, 3 }, { 5, 4 } }, { 0, 3, 4 }, 5 },
        { "DX 22", 6, { { 1, 0 }, { 5, 2 }, { 5, 3 }, { 5, 4 } }, { 0, 2, 3, 4 }, 5 },
        { "DX 32 Organ", 6, {}, { 0, 1, 2, 3, 4, 5 }, 5 },
    };

    return list;
}

inline int count() { return (int) all().size(); }

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

// Amounts a new route starts at (existing routes keep theirs).
constexpr float defaultRouteAmount = 0.35f;
constexpr float defaultFeedbackAmount = 0.15f;
} // namespace FmAlgorithms
