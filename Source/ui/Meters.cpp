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
    g.setColour (theme::black);
    g.fillRoundedRectangle (bar, 2.0f);

    auto yFor = [&] (float db) { return bar.getBottom() - juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)) * bar.getHeight(); };

    // segmented bar: white body, yellow top region, red over 0
    const float y = yFor (shown);
    const float yWarn = yFor (juce::jmin (hi, -6.0f));
    const float yHot = yFor (juce::jmin (hi, 0.0f));
    auto fill = juce::Rectangle<float> (bar.getX() + 1.0f, y, bar.getWidth() - 2.0f, bar.getBottom() - y);
    g.setColour (theme::text.withAlpha (0.85f));
    g.fillRect (fill.withTop (juce::jmax (fill.getY(), yWarn)));
    if (fill.getY() < yWarn)
    {
        g.setColour (theme::yellow);
        g.fillRect (fill.withBottom (yWarn).withTop (juce::jmax (fill.getY(), yHot)));
    }
    if (fill.getY() < yHot)
    {
        g.setColour (theme::bad);
        g.fillRect (fill.withBottom (yHot));
    }

    if (hold > lo)
    {
        g.setColour (hold > 0.0f ? theme::bad : theme::yellow);
        g.fillRect (bar.getX() + 1.0f, yFor (hold) - 1.0f, bar.getWidth() - 2.0f, 2.0f);
    }
    if (target > lo)
    {
        g.setColour (theme::yellow);
        g.fillRect (bar.getX() - 3.0f, yFor (target) - 1.0f, bar.getWidth() + 6.0f, 2.0f);
    }
    g.setColour (theme::line);
    g.drawRoundedRectangle (bar, 2.0f, 1.0f);
    if (label.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::bodyBold (10.0f));
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
    g.setColour (theme::black);
    g.fillRoundedRectangle (r, 2.0f);
    const float w = juce::jlimit (0.0f, 1.0f, -shown / 24.0f) * r.getWidth();
    g.setColour (theme::yellow);
    g.fillRoundedRectangle (juce::Rectangle<float> (r.getRight() - w, r.getY() + 1.0f, w, r.getHeight() - 2.0f), 2.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::bodyBold (11.0f));
    g.drawText ((label.isNotEmpty() ? label + "   " : juce::String()) + juce::String (shown, 1) + " dB", r.toNearestInt().reduced (8, 0), juce::Justification::centredLeft);
}

void Readout::set (const juce::String& v, const juce::String& c, juce::Colour col)
{
    value = v; caption = c; colour = col; repaint();
}

void Readout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    theme::drawPanel (g, r.toFloat());
    g.setColour (theme::textDim);
    g.setFont (theme::bodyBold (10.0f));
    g.drawText (caption.toUpperCase(), r.removeFromTop (22).reduced (12, 0).translated (0, 4), juce::Justification::centredLeft, true);
    g.setColour (colour);
    g.setFont (theme::headline (juce::jmin (30.0f, (float) r.getHeight() * 0.85f)));
    g.drawText (value, r.reduced (12, 0).translated (0, -2), juce::Justification::centredLeft, true);
}
}
