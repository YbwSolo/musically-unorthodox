#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mu::icons
{
// Icons are the SVGs in Assets/icons, loaded once as outlines so they can be drawn in any colour at any size.
const juce::Path& get (const juce::String& name);
void draw (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour);
}
