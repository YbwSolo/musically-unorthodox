#include "Theme.h"
#include "BinaryData.h"

namespace mu::theme
{
static juce::Typeface::Ptr loadTypeface (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
static const juce::Typeface::Ptr& antonFace()     { static juce::Typeface::Ptr t = loadTypeface (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize); return t; }
static const juce::Typeface::Ptr& interFace()     { static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize); return t; }
static const juce::Typeface::Ptr& interBoldFace() { static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize); return t; }

juce::Font headline (float px) { return juce::Font (juce::FontOptions (antonFace()).withHeight (px)); }
juce::Font body (float px)     { return juce::Font (juce::FontOptions (interFace()).withHeight (px)); }
juce::Font bodyBold (float px) { return juce::Font (juce::FontOptions (interBoldFace()).withHeight (px)); }

void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float radius)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
}

void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (black);
    g.fillRoundedRectangle (r, radius);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
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

juce::String noteName (float hz)
{
    if (hz < 16.0f) return {};
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> r, bool labels)
{
    static const float minor[] = { 30, 40, 50, 60, 70, 80, 90, 200, 300, 400, 500, 600, 700, 800, 900, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000 };
    static const float major[] = { 100, 1000, 10000 };
    g.setColour (juce::Colour (0xff141414));
    for (float f : minor) { const float x = std::round (freqToX (f, r.getX(), r.getWidth())) + 0.5f; g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); }
    g.setColour (juce::Colour (0xff222222));
    for (float f : major) { const float x = std::round (freqToX (f, r.getX(), r.getWidth())) + 0.5f; g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); }
    if (! labels) return;
    static const float marks[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    static const char* text[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    g.setFont (body (10.0f));
    g.setColour (textMute);
    for (int i = 0; i < 8; ++i)
        g.drawText (text[i], juce::Rectangle<float> (freqToX (marks[i], r.getX(), r.getWidth()) - 20.0f, r.getBottom() - 15.0f, 40.0f, 12.0f), juce::Justification::centred);
}

static juce::AttributedString attributed (const juce::String& s, const juce::Font& f, juce::Colour c, float lineSpacing)
{
    juce::AttributedString a;
    a.setJustification (juce::Justification::topLeft);
    a.setWordWrap (juce::AttributedString::byWord);
    a.setLineSpacing (f.getHeight() * (lineSpacing - 1.0f));
    a.append (s, f, c);
    return a;
}

void drawWrapped (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c, float lineSpacing)
{
    juce::TextLayout layout;
    layout.createLayout (attributed (s, f, c, lineSpacing), r.getWidth());
    layout.draw (g, r);
}

float wrappedHeight (const juce::String& s, float width, const juce::Font& f, float lineSpacing)
{
    juce::TextLayout layout;
    layout.createLayout (attributed (s, f, juce::Colours::white, lineSpacing), width);
    return layout.getHeight();
}
}
