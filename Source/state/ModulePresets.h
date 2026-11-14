#pragma once
#include "ChainPresets.h"

namespace mu
{
// One tool on the Tune or FX tab: its name, the switch that turns it on, and a line saying what it is for.
struct FxModule
{
    juce::String id, name, onParam, blurb;
};

// A named setting for one tool. Loading it switches the tool on and sets its controls; nothing else is touched.
struct ModulePreset
{
    juce::String module, name, blurb;
    std::vector<ParamValue> values;
};

// A complete sound: several tools set together. Loading one switches every FX tool off first, then sets the ones
// it uses, so nothing is left over from the sound before.
struct SoundPreset
{
    juce::String name, blurb;
    std::vector<ParamValue> values;
};

const std::vector<FxModule>& fxModules();                       // the FX rack, as listed on the FX tab
const FxModule& tuneModule();
const std::vector<ModulePreset>& modulePresets();
std::vector<int> modulePresetsFor (const juce::String& module); // indices into modulePresets()
const std::vector<SoundPreset>& soundPresets();

// Not a real control: tells Tune to allow only the key note, which is what makes a voice sound like a robot.
inline const juce::String kRootOnly { "@rootOnly" };
}
