#include "Theme.h"

namespace mu::theme
{
juce::Font titleFont (float px) { return juce::Font (juce::FontOptions (px, juce::Font::bold)); }
juce::Font labelFont (float px) { return juce::Font (juce::FontOptions (px)); }
juce::Font monoFont (float px)  { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), px, juce::Font::plain)); }

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, bool raised)
{
    g.setColour (raised ? panelHi : panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

juce::String dbString (float v, int decimals, const juce::String& unit)
{
    if (v <= -99.0f) return "-inf" + unit;
    return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + unit;
}
}
