#include "Meters.h"
#include "Theme.h"

namespace mu
{
void LevelMeter::setLevel (float db, float peakHoldDb)
{
    value = db;
    shown = db > shown ? db : shown - 1.2f;
    hold = peakHoldDb;
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto bar = r.withTrimmedBottom (label.isEmpty() ? 0.0f : 16.0f).reduced (0.0f, 2.0f);
    g.setColour (theme::bg);
    g.fillRoundedRectangle (bar, 3.0f);

    auto yFor = [&] (float db) { return bar.getBottom() - juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)) * bar.getHeight(); };

    const float y = yFor (shown);
    juce::ColourGradient grad (theme::good, bar.getX(), bar.getBottom(), theme::bad, bar.getX(), bar.getY(), false);
    grad.addColour (0.75, theme::warn);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (juce::Rectangle<float> (bar.getX() + 1.0f, y, bar.getWidth() - 2.0f, bar.getBottom() - y), 2.0f);

    if (hold > lo)
    {
        g.setColour (hold > 0.0f ? theme::bad : theme::text);
        g.fillRect (bar.getX() + 1.0f, yFor (hold) - 1.0f, bar.getWidth() - 2.0f, 2.0f);
    }
    if (target > lo)
    {
        g.setColour (theme::accentHi);
        g.fillRect (bar.getX() - 2.0f, yFor (target) - 1.0f, bar.getWidth() + 4.0f, 2.0f);
    }
    g.setColour (theme::line);
    g.drawRoundedRectangle (bar, 3.0f, 1.0f);
    if (label.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (11.0f));
        g.drawText (label, r.removeFromBottom (14.0f).toNearestInt(), juce::Justification::centred);
    }
}

void GainReductionMeter::setReduction (float db)
{
    shown = db < shown ? db : shown + 0.6f;
    if (shown > 0.0f) shown = 0.0f;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::bg);
    g.fillRoundedRectangle (r, 3.0f);
    const float w = juce::jlimit (0.0f, 1.0f, -shown / 24.0f) * r.getWidth();
    g.setColour (theme::warn);
    g.fillRoundedRectangle (juce::Rectangle<float> (r.getRight() - w, r.getY() + 1.0f, w, r.getHeight() - 2.0f), 2.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::monoFont (11.0f));
    g.drawText (juce::String (shown, 1) + " dB", r.toNearestInt().reduced (6, 0), juce::Justification::centredLeft);
}

void Readout::set (const juce::String& v, const juce::String& c, juce::Colour col)
{
    value = v; caption = c; colour = col; repaint();
}

void Readout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    theme::drawPanel (g, r.toFloat(), 8.0f);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (11.0f));
    g.drawText (caption.toUpperCase(), r.removeFromTop (20).reduced (10, 0), juce::Justification::centredLeft);
    g.setColour (colour);
    g.setFont (theme::titleFont (juce::jmin (26.0f, (float) r.getHeight() * 0.8f)));
    g.drawText (value, r.reduced (10, 0), juce::Justification::centredLeft);
}
}
