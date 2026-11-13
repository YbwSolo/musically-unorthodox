#include "CompressorPage.h"
#include "Theme.h"

namespace mu
{
void TransferCurve::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::black);
    g.fillRoundedRectangle (r, 3.0f);
    auto a = r.reduced (12.0f);
    const float lo = -60.0f, hi = 0.0f;
    auto xFor = [&] (float db) { return a.getX() + (db - lo) / (hi - lo) * a.getWidth(); };
    auto yFor = [&] (float db) { return a.getBottom() - (db - lo) / (hi - lo) * a.getHeight(); };

    g.setColour (theme::line.withAlpha (0.7f));
    for (float d = -50.0f; d < 0.0f; d += 10.0f)
    {
        g.drawVerticalLine ((int) xFor (d), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) yFor (d), a.getX(), a.getRight());
    }
    g.setColour (theme::lineHi);
    g.drawLine (xFor (lo), yFor (lo), xFor (hi), yFor (hi), 1.0f);

    const float T = proc.apvts.getRawParameterValue (ids::compThreshold)->load();
    const float R = proc.apvts.getRawParameterValue (ids::compRatio)->load();
    const float W = proc.apvts.getRawParameterValue (ids::compKnee)->load();
    const bool autoMk = proc.apvts.getRawParameterValue (ids::compAutoMakeup)->load() > 0.5f;
    const float mk = autoMk ? Compressor::autoMakeupDb (T, R, W) : proc.apvts.getRawParameterValue (ids::compMakeup)->load();

    juce::Path p;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = lo + (hi - lo) * (float) i / 120.0f;
        const float y = juce::jlimit (lo, hi + 12.0f, x + Compressor::computeGainDb (x, T, R, W) + mk);
        if (i == 0) p.startNewSubPath (xFor (x), yFor (y)); else p.lineTo (xFor (x), yFor (y));
    }
    const bool on = proc.apvts.getRawParameterValue (ids::compEnabled)->load() > 0.5f;
    g.setColour (on ? theme::yellow : theme::textMute);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    const float in = proc.getCompressor().getInputDb();
    if (in > lo && on)
    {
        const float y = in + Compressor::computeGainDb (in, T, R, W) + mk;
        g.setColour (theme::text);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ xFor (in), yFor (juce::jlimit (lo, hi + 12.0f, y)) }));
    }
    g.setColour (theme::textMute);
    g.setFont (theme::body (10.0f));
    g.drawText ("IN", a.removeFromBottom (12.0f).toNearestInt(), juce::Justification::centredRight);
    g.drawText ("OUT", a.removeFromLeft (30.0f).toNearestInt(), juce::Justification::topLeft);
}

CompressorPage::CompressorPage (MUProcessor& p) : proc (p), curve (p)
{
    addAndMakeVisible (compPanel);
    compPanel.bindToggle (proc.apvts, ids::compEnabled);
    addAndMakeVisible (gatePanel);
    gatePanel.bindToggle (proc.apvts, ids::gateOn);
    compPanel.addAndMakeVisible (curve);

    auto mk = [this] (std::unique_ptr<Knob>& k, ModulePanel& panel, const juce::String& id, const juce::String& title, const juce::String& tip)
    {
        k = std::make_unique<Knob> (proc.apvts, id, title, tip);
        panel.addAndMakeVisible (*k);
    };
    mk (threshold, compPanel, ids::compThreshold, "Threshold", "Level where compression starts. Auto Set puts it just above the track's average level.");
    mk (ratio, compPanel, ids::compRatio, "Ratio", "How hard it holds the level once over the threshold. 3-4:1 for vocals, 2:1 for a beat.");
    mk (attack, compPanel, ids::compAttack, "Attack", "How fast it grabs. Fast (1-5 ms) controls peaks, slower (20-30 ms) lets the punch through.");
    mk (release, compPanel, ids::compRelease, "Release", "How fast it lets go. Too fast pumps, too slow dulls.");
    mk (knee, compPanel, ids::compKnee, "Knee", "Soft knee eases in around the threshold. Hard knee grabs.");
    mk (makeup, compPanel, ids::compMakeup, "Makeup", "Gain added after compression. Auto makeup sets it for you.");
    mk (mix, compPanel, ids::compMix, "Mix", "Below 100 % blends the uncompressed signal back in (parallel compression).");
    autoMakeup.setTooltip ("Adds gain to compensate for the reduction so you compare tone, not loudness.");
    compPanel.addAndMakeVisible (autoMakeup);
    autoMakeupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::compAutoMakeup, autoMakeup);

    autoSet.setComponentID ("primary");
    autoSet.setTooltip ("Sets threshold, ratio and timing from this track's role and its measured level.");
    autoSet.onClick = [this] { result.setText (proc.autoCompressor(), juce::dontSendNotification); };
    compPanel.addAndMakeVisible (autoSet);
    result.setFont (theme::body (11.5f));
    result.setColour (juce::Label::textColourId, theme::textDim);
    compPanel.addAndMakeVisible (result);

    grMeter.setLabel ("GAIN REDUCTION");
    compPanel.addAndMakeVisible (grMeter);
    inMeter.setLabel ("IN"); outMeter.setLabel ("OUT");
    compPanel.addAndMakeVisible (inMeter); compPanel.addAndMakeVisible (outMeter);

    mk (gThreshold, gatePanel, ids::gateThreshold, "Threshold", "Opens above this level. Set it just under the quietest words.");
    mk (gRange, gatePanel, ids::gateRange, "Range", "How far it closes. 15-20 dB tidies room noise without sounding cut off.");
    mk (gAttack, gatePanel, ids::gateAttack, "Attack", "How fast it opens.");
    mk (gRelease, gatePanel, ids::gateRelease, "Release", "How fast it closes after the hold.");
    mk (gHold, gatePanel, ids::gateHold, "Hold", "Stays open this long after the level drops, so word endings survive.");
    gateMeter.setLabel ("GATE");
    gatePanel.addAndMakeVisible (gateMeter);
}

void CompressorPage::refresh()
{
    const auto& c = proc.getCompressor();
    grMeter.setReduction (c.getGainReductionDb());
    inMeter.setLevel (c.getInputDb());
    outMeter.setLevel (c.getOutputDb());
    makeup->setEnabled (proc.apvts.getRawParameterValue (ids::compAutoMakeup)->load() < 0.5f);
    gateMeter.setReduction (proc.getGate().getGainReductionDb());
    curve.repaint();
}

void CompressorPage::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void CompressorPage::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    auto gateArea = r.removeFromBottom (150);
    r.removeFromBottom (12);
    compPanel.setBounds (r);
    gatePanel.setBounds (gateArea);

    auto c = compPanel.getContentArea();
    auto top = c.removeFromTop (30);
    autoSet.setBounds (top.removeFromLeft (130));
    top.removeFromLeft (10);
    autoMakeup.setBounds (top.removeFromRight (130));
    result.setBounds (top);
    c.removeFromTop (8);

    auto right = c.removeFromRight (300);
    auto meters = right.removeFromRight (70);
    inMeter.setBounds (meters.removeFromLeft (30));
    meters.removeFromLeft (8);
    outMeter.setBounds (meters.removeFromLeft (30));
    right.removeFromRight (12);
    auto curveArea = right.removeFromTop (right.getHeight() - 34);
    curve.setBounds (curveArea);
    right.removeFromTop (10);
    grMeter.setBounds (right.removeFromTop (24));

    c.removeFromRight (12);
    const int cols = 4;
    const int kw = juce::jmin (124, c.getWidth() / cols);
    const int kh = juce::jmin (150, (c.getHeight() - 12) / 2);
    Knob* ks[7] = { threshold.get(), ratio.get(), attack.get(), release.get(), knee.get(), makeup.get(), mix.get() };
    for (int i = 0; i < 7; ++i)
    {
        const int col = i % cols, row = i / cols;
        ks[i]->setBounds (c.getX() + col * kw, c.getY() + row * (kh + 12), kw, kh);
    }

    auto gc = gatePanel.getContentArea();
    auto gm = gc.removeFromRight (220);
    gm.removeFromTop (8);
    gateMeter.setBounds (gm.removeFromTop (24));
    Knob* gk[5] = { gThreshold.get(), gRange.get(), gAttack.get(), gRelease.get(), gHold.get() };
    const int gw = juce::jmin (110, gc.getWidth() / 5);
    for (int i = 0; i < 5; ++i) gk[i]->setBounds (gc.getX() + i * gw, gc.getY(), gw, gc.getHeight());
}
}
