#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Musically Unorthodox brand: black, white, MU yellow (#FBF20D). Headlines in Anton, everything else in Inter.
namespace mu::theme
{
inline const juce::Colour black     { 0xff0a0a0a };
inline const juce::Colour bg        { 0xff0e0e0e };
inline const juce::Colour panel     { 0xff161616 };
inline const juce::Colour panelHi   { 0xff1e1e1e };
inline const juce::Colour line      { 0xff2a2a2a };
inline const juce::Colour lineHi    { 0xff3a3a3a };
inline const juce::Colour text      { 0xfff4f4f4 };
inline const juce::Colour textDim   { 0xffa3a3a3 };
inline const juce::Colour textMute  { 0xff666666 };
inline const juce::Colour yellow    { 0xfffbf20d };
inline const juce::Colour yellowDim { 0xffb8b10a };
inline const juce::Colour onYellow  { 0xff0a0a0a };
inline const juce::Colour good      { 0xff58d68d };
inline const juce::Colour warn      { 0xfffbf20d };
inline const juce::Colour bad       { 0xffff4b4b };

juce::Font headline (float px);     // Anton
juce::Font body (float px);         // Inter Regular
juce::Font bodyBold (float px);     // Inter SemiBold
juce::Font mono (float px);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool raised = false);
void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& hint = {});
juce::String dbString (float v, int decimals = 1, const juce::String& unit = " dB");
juce::String hzString (float hz);

// Band colours for the EQ: yellow family plus greys so it stays on-brand but readable.
juce::Colour bandColour (int band);
}
