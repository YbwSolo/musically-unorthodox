#include "MeterRail.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;
static constexpr float kMeterFloor = -60.0f;
static float meterPos (float db) { return juce::jlimit (0.0f, 1.0f, std::pow ((db - kMeterFloor) / -kMeterFloor, 1.6f)); }   // more room near the top

MeterRail::MeterRail (MUProcessor& p) : proc (p) { setHelp (*this, "rail.peak"); }

void MeterRail::mouseDown (const juce::MouseEvent&) { proc.resetMeters(); hold = { -100.0f, -100.0f }; clipFlash = 0.0f; }

void MeterRail::tick (float dt)
{
    const auto& l = proc.getLoudness();
    for (int c = 0; c < 2; ++c)
    {
        const float in = l.getChannelPeakDb (c);
        auto& lv = level[(size_t) c];
        lv = in > lv ? in : juce::jmax (in, lv - 26.0f * dt);
        if (lv >= hold[(size_t) c]) { hold[(size_t) c] = lv; holdAge[(size_t) c] = 0.0f; }
        else { holdAge[(size_t) c] += dt; if (holdAge[(size_t) c] > 1.5f) hold[(size_t) c] -= 14.0f * dt; }
        if (in > -0.1f) clipFlash = 1.0f;
    }
    clipFlash = juce::jmax (0.0f, clipFlash - dt * 0.4f);
    const float reduction = juce::jmin (proc.getCompressor().getGainReductionDb(), proc.isMasterInstance() ? proc.getMaximizer().getGainReductionDb() : 0.0f);
    gr = reduction < gr ? reduction : approach (gr, reduction, dt, 0.12f);
    lufs = l.getShortTerm();
    tp = l.getTruePeakHold();
    repaint();
}

void MeterRail::paint (juce::Graphics& g)
{
    g.fillAll (black);
    g.setColour (line);
    g.fillRect (0, 0, 1, getHeight());

    auto r = getLocalBounds().reduced (10, 12);
    auto readouts = r.removeFromBottom (92);
    const auto bars = r.toFloat();

    // scale
    g.setFont (body (9.5f));
    static const int marks[] = { 0, -6, -12, -18, -24, -36, -48 };
    const float scaleX = bars.getX() + 30.0f;
    for (int m : marks)
    {
        const float y = bars.getBottom() - meterPos ((float) m) * bars.getHeight();
        g.setColour (line);
        g.fillRect (bars.getX(), y, 26.0f, 1.0f);
        g.setColour (textMute);
        g.drawText (juce::String (m), juce::Rectangle<float> (scaleX, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredLeft);
    }

    for (int c = 0; c < 2; ++c)
    {
        const auto bar = juce::Rectangle<float> (bars.getX() + (float) c * 10.0f, bars.getY(), 8.0f, bars.getHeight());
        g.setColour (juce::Colour (0xff141414));
        g.fillRect (bar);
        const float h = meterPos (level[(size_t) c]) * bar.getHeight();
        const float hot = bar.getBottom() - meterPos (-6.0f) * bar.getHeight();
        g.setColour (text.withAlpha (0.85f));
        g.fillRect (bar.withTop (juce::jmax (bar.getBottom() - h, hot)));
        if (bar.getBottom() - h < hot) { g.setColour (yellow); g.fillRect (bar.withTop (bar.getBottom() - h).withBottom (hot)); }
        const float hy = bar.getBottom() - meterPos (hold[(size_t) c]) * bar.getHeight();
        g.setColour (hold[(size_t) c] > -0.1f ? over : text);
        if (hold[(size_t) c] > kMeterFloor + 1.0f) g.fillRect (bar.getX(), hy - 1.0f, bar.getWidth(), 2.0f);
    }
    if (clipFlash > 0.0f) { g.setColour (over.withAlpha (clipFlash)); g.fillRect (bars.getX(), bars.getY() - 5.0f, 18.0f, 3.0f); }

    // gain reduction, hanging from the top
    const auto grBar = juce::Rectangle<float> (bars.getX() + 21.0f, bars.getY(), 5.0f, bars.getHeight());
    g.setColour (juce::Colour (0xff141414));
    g.fillRect (grBar);
    const float amount = juce::jlimit (0.0f, 1.0f, -gr / 20.0f);
    if (amount > 0.004f) { g.setColour (yellow); g.fillRect (grBar.withHeight (grBar.getHeight() * amount)); }

    // readouts
    readouts.removeFromTop (10);
    auto row = [&] (const juce::String& value, const juce::String& unit, bool alarm)
    {
        auto line1 = readouts.removeFromTop (24);
        g.setFont (headline (22.0f));
        g.setColour (alarm ? over : text);
        g.drawText (value, line1, juce::Justification::centredLeft);
        g.setFont (body (9.5f));
        g.setColour (textMute);
        g.drawText (unit, readouts.removeFromTop (12), juce::Justification::centredLeft);
        readouts.removeFromTop (4);
    };
    row (lufs > -70.0f ? juce::String (lufs, 1) : juce::String ("--"), "LUFS short", false);
    row (tp > -70.0f ? juce::String (tp, 1) : juce::String ("--"), "dB true peak", tp > -0.05f);
}
}
