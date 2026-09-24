#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace TableFactory
{
    int getNumFactoryTables();
    juce::StringArray getFactoryTableNames();
    std::vector<std::vector<float>> generate (int tableIndex);
}
