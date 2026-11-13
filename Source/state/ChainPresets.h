#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "ParameterIDs.h"

namespace mu
{
struct ParamValue { juce::String id; float value; };

struct ChainPreset
{
    juce::String name;
    juce::String blurb;
    Role role;
    std::vector<ParamValue> values;
};

// Whole-chain starting points modelled on the MU studio Logic template (gate -> EQ -> compression ->
// de-ess -> saturation -> rider). Thresholds get re-fitted to the measured level when the preset is applied.
const std::vector<ChainPreset>& chainPresets();
juce::StringArray chainPresetNames();
}
