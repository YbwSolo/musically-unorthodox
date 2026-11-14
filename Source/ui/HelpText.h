#pragma once
#include <juce_core/juce_core.h>
#include <map>

namespace juce { class Component; }

namespace mu
{
// Plain-language explanations shown in the info bar when the mouse is over a control.
// The AI tutor reads the same table, so what it says about a control matches what the plugin says.
struct HelpEntry { juce::String title, body; };
HelpEntry helpFor (const juce::String& key);
const std::map<juce::String, HelpEntry>& helpTable();
void setHelp (juce::Component& c, const juce::String& key);       // defined in Widgets.cpp
}
