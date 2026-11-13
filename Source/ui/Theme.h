#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mu::theme
{
inline const juce::Colour bg        { 0xff0e0f12 };
inline const juce::Colour panel     { 0xff16181d };
inline const juce::Colour panelHi   { 0xff1d2027 };
inline const juce::Colour line      { 0xff2a2e37 };
inline const juce::Colour text      { 0xffe9eaee };
inline const juce::Colour textDim   { 0xff8d93a1 };
inline const juce::Colour textMute  { 0xff5a6070 };
inline const juce::Colour accent    { 0xff0078ff };
inline const juce::Colour accentHi  { 0xff4da3ff };
inline const juce::Colour good      { 0xff35d07f };
inline const juce::Colour warn      { 0xffffb020 };
inline const juce::Colour bad       { 0xffff4d5a };
inline const juce::Colour pro       { 0xffb98cff };

juce::Font titleFont (float px);
juce::Font labelFont (float px);
juce::Font monoFont (float px);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f, bool raised = false);
juce::String dbString (float v, int decimals = 1, const juce::String& unit = " dB");
}
