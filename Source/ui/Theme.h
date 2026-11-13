#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Musically Unorthodox brand: black, white and MU yellow. Headlines and big numbers in Anton, everything else in Inter.
namespace mu::theme
{
inline const juce::Colour black    { 0xff000000 };   // window and graph wells
inline const juce::Colour panel    { 0xff101010 };
inline const juce::Colour raised   { 0xff1a1a1a };
inline const juce::Colour control  { 0xff242424 };
inline const juce::Colour line     { 0xff262626 };
inline const juce::Colour lineHi   { 0xff3d3d3d };
inline const juce::Colour text     { 0xfff2f2f2 };
inline const juce::Colour textDim  { 0xff9c9c9c };
inline const juce::Colour textMute { 0xff5f5f5f };
inline const juce::Colour yellow   { 0xfffbf20d };
inline const juce::Colour onYellow { 0xff000000 };
inline const juce::Colour over     { 0xffff3b30 };   // clipping and "too much" only

juce::Font headline (float px);     // Anton
juce::Font body (float px);         // Inter Regular
juce::Font bodyBold (float px);     // Inter SemiBold

void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill = panel, float radius = 6.0f);
void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.0f);          // black inset for graphs
juce::String dbString (float v, int decimals = 1, const juce::String& unit = " dB");
juce::String hzString (float hz);
juce::String noteName (float hz);

// log-frequency helpers shared by every graph
inline float freqToX (float hz, float x0, float w, float lo = 20.0f, float hi = 20000.0f) { return x0 + w * std::log (hz / lo) / std::log (hi / lo); }
inline float xToFreq (float x, float x0, float w, float lo = 20.0f, float hi = 20000.0f) { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, (x - x0) / w)); }

void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, bool labels = true);
void drawWrapped (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c, float lineSpacing = 1.35f);
float wrappedHeight (const juce::String& s, float width, const juce::Font& f, float lineSpacing = 1.35f);

// One-pole smoothing for animated values, frame-rate independent.
inline float approach (float current, float target, float dtSeconds, float timeConstant)
{
    return target + (current - target) * std::exp (-dtSeconds / juce::jmax (0.001f, timeConstant));
}
}
