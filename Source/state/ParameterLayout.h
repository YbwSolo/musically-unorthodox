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
    { EQType::Bell,      350.0f },
    { EQType::Bell,      1500.0f },
    { EQType::Bell,      4000.0f },
    { EQType::HighShelf, 10000.0f }
};
}
