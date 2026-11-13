#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterIDs.h"

namespace mu
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Default band setup used when the EQ is reset: flat, all bands off.
struct DefaultBand { EQType type; float freq; };
inline const DefaultBand kDefaultBands[kNumEQBands] = {
    { EQType::LowCut,    40.0f },
    { EQType::LowShelf,  120.0f },
    { EQType::Bell,      250.0f },
    { EQType::Bell,      700.0f },
    { EQType::Bell,      2000.0f },
    { EQType::Bell,      5000.0f },
    { EQType::HighShelf, 10000.0f },
    { EQType::HighCut,   18000.0f }
};
}
