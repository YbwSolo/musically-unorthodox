#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "ParameterIDs.h"

namespace mu
{
struct ParamValue { juce::String id; float value; };

struct ChainPreset
{
    juce::String category;     // Vocals, Drums and bass, Instruments, Beat, Master
    juce::String name;
    juce::String blurb;        // what it does and when to reach for it
    Role role;
    std::vector<ParamValue> values;
};

// Whole-chain starting points. Each one sets every module, so loading a preset never leaves
// something switched on from before. Thresholds are re-fitted to the measured level when applied.
const std::vector<ChainPreset>& chainPresets();
juce::StringArray chainPresetCategories();
int findChainPreset (const juce::String& name);
}
