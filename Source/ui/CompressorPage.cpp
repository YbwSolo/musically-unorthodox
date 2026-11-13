#include "CompressorPage.h"
#include "Theme.h"
#include "../license/License.h"

namespace mu
{
void TransferCurve::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::bg);
    g.fillRoundedRectangle (r, 8.0f);
    auto a = r.reduced (10.0f);
    const float lo = -60.0f, hi = 0.0f;
    auto xFor = [&] (float db) { return a.getX() + (db - lo) / (hi - lo) * a.getWidth(); };
    auto yFor = [&] (float db) { return a.getBottom() - (db - lo) / (hi - lo) * a.getHeight(); };

    g.setColour (theme::line.withAlpha (0.6f));
    for (float d = -50.0f; d < 0.0f; d += 10.0f)
    {
        g.drawVerticalLine ((int) xFor (d), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) yFor (d), a.getX(), a.getRight());
    }
    g.setColour (theme::textMute);
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
    g.setColour (on ? theme::accentHi : theme::textMute);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    const float in = proc.getCompressor().getInputDb();
    if (in > lo && on)
    {
        const float y = in + Compressor::computeGainDb (in, T, R, W) + mk;
        g.setColour (theme::warn);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ xFor (in), yFor (juce::jlimit (lo, hi + 12.0f, y)) }));
    }
    g.setColour (theme::textMute);
    g.setFont (theme::labelFont (10.0f));
    g.drawText ("in", a.removeFromBottom (12.0f).toNearestInt(), juce::Justification::centredRight);
    g.drawText ("out", a.removeFromLeft (24.0f).toNearestInt(), juce::Justification::topLeft);
}

CompressorPage::CompressorPage (MUProcessor& p) : proc (p), curve (p)
{
    addAndMakeVisible (curve);
    addAndMakeVisible (on);
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::compEnabled, on);
    addAndMakeVisible (autoMakeup);
    autoMakeupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::compAutoMakeup, autoMakeup);

    auto knob = [this] (juce::Slider& s, juce::Label& l, const juce::String& id)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 18);
        addAndMakeVisible (s);
        l.setFont (theme::labelFont (11.0f));
        l.setColour (juce::Label::textColourId, theme::textDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
        sliderAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
    };
    knob (threshold, thresholdL, ids::compThreshold);
    knob (ratio, ratioL, ids::compRatio);
    knob (attack, attackL, ids::compAttack);
    knob (release, releaseL, ids::compRelease);
    knob (knee, kneeL, ids::compKnee);
    knob (makeup, makeupL, ids::compMakeup);
    knob (mix, mixL, ids::compMix);

    autoSet.setTooltip ("Pro: sets threshold, ratio and timing from this track's role and measured level.");
    autoSet.onClick = [this]
    {
        if (! License::get().isPro()) { result.setText ("Auto set is a Pro feature - add your key in Settings.", juce::dontSendNotification); return; }
        result.setText (proc.autoCompressor(), juce::dontSendNotification);
    };
    addAndMakeVisible (autoSet);
    result.setFont (theme::labelFont (11.0f));
    result.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (result);

    addAndMakeVisible (grMeter);
    inMeter.setLabel ("IN"); outMeter.setLabel ("OUT");
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
}

void CompressorPage::refresh()
{
    const auto& c = proc.getCompressor();
    grMeter.setReduction (c.getGainReductionDb());
    inMeter.setLevel (c.getInputDb());
    outMeter.setLevel (c.getOutputDb());
    makeup.setEnabled (proc.apvts.getRawParameterValue (ids::compAutoMakeup)->load() < 0.5f);
    autoSet.setButtonText (License::get().isPro() ? "AUTO SET" : "AUTO SET (PRO)");
    curve.repaint();
}

void CompressorPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (38);
    auto right = r.removeFromRight (280);
    theme::drawPanel (g, r.toFloat(), 8.0f);
    juce::ignoreUnused (right);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (11.0f));
    g.drawText ("GAIN REDUCTION", grMeter.getBounds().translated (0, -16).withHeight (14), juce::Justification::centredLeft);
}

void CompressorPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto top = r.removeFromTop (28);
    on.setBounds (top.removeFromLeft (130));
    autoSet.setBounds (top.removeFromRight (130));
    top.removeFromRight (8);
    result.setBounds (top);
    r.removeFromTop (10);

    auto right = r.removeFromRight (280);
    right.removeFromLeft (12);
    auto meters = right.removeFromRight (70);
    inMeter.setBounds (meters.removeFromLeft (32));
    meters.removeFromLeft (6);
    outMeter.setBounds (meters.removeFromLeft (32));
    right.removeFromRight (10);
    auto curveArea = right.removeFromTop (right.getHeight() - 56);
    curve.setBounds (curveArea);
    right.removeFromTop (22);
    grMeter.setBounds (right.removeFromTop (22));

    auto panel = r.reduced (12, 12);
    auto row1 = panel.removeFromTop (panel.getHeight() / 2);
    auto row2 = panel;
    auto place = [] (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l)
    {
        const int size = juce::jmin (150, cell.getWidth(), cell.getHeight());
        auto box = cell.withSizeKeepingCentre (size, size);
        l.setBounds (box.removeFromTop (14));
        s.setBounds (box);
    };
    const int w1 = row1.getWidth() / 4;
    place (row1.removeFromLeft (w1), threshold, thresholdL);
    place (row1.removeFromLeft (w1), ratio, ratioL);
    place (row1.removeFromLeft (w1), attack, attackL);
    place (row1, release, releaseL);
    const int w2 = row2.getWidth() / 4;
    place (row2.removeFromLeft (w2), knee, kneeL);
    place (row2.removeFromLeft (w2), makeup, makeupL);
    place (row2.removeFromLeft (w2), mix, mixL);
    autoMakeup.setBounds (row2.withSizeKeepingCentre (row2.getWidth(), 26));
}
}
