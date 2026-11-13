#include "Theme.h"
#include "BinaryData.h"

namespace mu::theme
{
static juce::Typeface::Ptr loadTypeface (const void* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

static const juce::Typeface::Ptr& antonFace()
{
    static juce::Typeface::Ptr t = loadTypeface (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize);
    return t;
}
static const juce::Typeface::Ptr& interFace()
{
    static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    return t;
}
static const juce::Typeface::Ptr& interBoldFace()
{
    static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return t;
}

juce::Font headline (float px) { return juce::Font (juce::FontOptions (antonFace()).withHeight (px)); }
juce::Font body (float px)     { return juce::Font (juce::FontOptions (interFace()).withHeight (px)); }
juce::Font bodyBold (float px) { return juce::Font (juce::FontOptions (interBoldFace()).withHeight (px)); }
juce::Font mono (float px)     { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), px, juce::Font::plain)); }

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool raised)
{
    g.setColour (raised ? panelHi : panel);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::String& hint)
{
    g.setColour (yellow);
    g.fillRect (r.removeFromLeft (3).reduced (0, 3));
    r.removeFromLeft (9);
    g.setColour (text);
    g.setFont (headline (15.0f));
    const int w = juce::roundToInt (headline (15.0f).getStringWidthFloat (title.toUpperCase())) + 12;
    g.drawText (title.toUpperCase(), r.removeFromLeft (w), juce::Justification::centredLeft);
    if (hint.isNotEmpty())
    {
        g.setColour (textMute);
        g.setFont (body (11.0f));
        g.drawText (hint, r, juce::Justification::centredLeft, true);
    }
}

juce::String dbString (float v, int decimals, const juce::String& unit)
{
    if (v <= -99.0f) return "-inf" + unit;
    return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + unit;
}

juce::String hzString (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::Colour bandColour (int band)
{
    static const juce::Colour c[] = {
        juce::Colour (0xfffbf20d), juce::Colour (0xffffffff), juce::Colour (0xffffb020),
        juce::Colour (0xffbdbdbd), juce::Colour (0xfffff08a), juce::Colour (0xffff7a3d)
    };
    return c[juce::jlimit (0, 5, band)];
}
}
