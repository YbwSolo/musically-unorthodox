#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mu
{
// Plain-language explanations shown in the info bar when the mouse is over a control.
struct HelpEntry { juce::String title, body; };
HelpEntry helpFor (const juce::String& key);
inline void setHelp (juce::Component& c, const juce::String& key) { c.getProperties().set ("help", key); }
}
