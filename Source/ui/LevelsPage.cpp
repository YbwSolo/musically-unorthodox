#include "LevelsPage.h"
#include "Theme.h"

namespace mu
{
LevelsPage::LevelsPage (MUProcessor& p) : proc (p)
{
    for (auto* r : { &momentary, &shortTerm, &integrated, &truePeak, &crest }) addAndMakeVisible (r);
    meter.setLabel ("LUFS M");
    addAndMakeVisible (meter);

    output.setSliderStyle (juce::Slider::LinearVertical);
    output.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible (output);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::outputGain, output);
    outputL.setFont (theme::labelFont (11.0f));
    outputL.setColour (juce::Label::textColourId, theme::textDim);
    outputL.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputL);

    checklist.setFont (theme::labelFont (13.0f));
    checklist.setColour (juce::Label::textColourId, theme::text);
    checklist.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (checklist);
    explain.setFont (theme::labelFont (12.0f));
    explain.setColour (juce::Label::textColourId, theme::textDim);
    explain.setJustificationType (juce::Justification::topLeft);
    explain.setText ("Integrated LUFS is the number mastering engineers look at: the loudness of the whole song. True peak is the ceiling. "
                     "The usual hand-off is around -18 LUFS with peaks no higher than -6 dBTP and no limiter on the master. "
                     "Short-term tells you where the loudest sections sit; momentary is what is happening right now.",
                     juce::dontSendNotification);
    addAndMakeVisible (explain);

    resetMeters.onClick = [this] { proc.resetMeters(); };
    addAndMakeVisible (resetMeters);
}

void LevelsPage::refresh()
{
    const auto& l = proc.getLoudness();
    const bool isMaster = proc.isMasterInstance();
    const float tLufs = proc.apvts.getRawParameterValue (ids::targetLufs)->load();
    const float tTp = proc.apvts.getRawParameterValue (ids::targetTruePeak)->load();

    auto fmt = [] (float v, const juce::String& unit) { return v > -90.0f ? juce::String (v, 1) + unit : "--"; };
    auto near = [] (float v, float t, float tol, float tol2) { return std::abs (v - t) <= tol ? theme::good : std::abs (v - t) <= tol2 ? theme::warn : theme::bad; };

    momentary.set (fmt (l.getMomentary(), ""), "momentary LUFS", theme::text);
    shortTerm.set (fmt (l.getShortTerm(), ""), "short-term LUFS", theme::text);
    const float integ = l.getIntegrated();
    integrated.set (fmt (integ, ""), isMaster ? "integrated LUFS (target " + juce::String (tLufs, 1) + ")" : "integrated LUFS",
                    integ > -90.0f && isMaster ? near (integ, tLufs, 1.0f, 3.0f) : theme::text);
    const float tp = l.getTruePeakHold();
    truePeak.set (fmt (tp, " dBTP"), isMaster ? "true peak (ceiling " + juce::String (tTp, 1) + ")" : "true peak",
                  tp > -90.0f ? (tp > 0.0f ? theme::bad : isMaster ? (tp <= tTp ? theme::good : theme::warn) : (tp > -1.0f ? theme::warn : theme::text)) : theme::text);
    crest.set (l.getCrestFactorDb() > 0.0f ? juce::String (l.getCrestFactorDb(), 1) + " dB" : "--", "crest factor (peak over RMS)", theme::text);
    meter.setLevel (l.getMomentary());
    meter.setTargetMarker (isMaster ? tLufs : -200.0f);

    juce::String c;
    if (isMaster)
    {
        if (integ < -90.0f) c << "○  Play the whole song through so the integrated reading settles.\n";
        else c << (std::abs (integ - tLufs) <= 1.0f ? "✓  Loudness is on target.\n" : integ < tLufs ? "○  Mix is " + juce::String (tLufs - integ, 1) + " LU under target - Balance Now on the Mix tab will bring it up.\n"
                                                                                      : "○  Mix is " + juce::String (integ - tLufs, 1) + " LU over target - Balance Now will pull it down.\n");
        if (tp > -90.0f) c << (tp <= tTp ? "✓  Peaks leave enough headroom.\n" : tp > 0.0f ? "✗  Clipping - the master is going over 0 dBTP.\n" : "○  Peaks are " + juce::String (tp - tTp, 1) + " dB over the ceiling.\n");
        if (l.getCrestFactorDb() > 0.0f) c << (l.getCrestFactorDb() >= 8.0f ? "✓  Dynamics are intact (no limiter squashing the mix).\n" : "○  Crest factor is low - the mix looks pre-limited. Bypass any master limiter before hand-off.\n");
    }
    else
    {
        c << "This is a track instance. The master instance judges the hand-off level; here you can watch this track's own level and peaks.\n";
        if (tp > -0.5f) c << "✗  This track is clipping - pull its own fader or trim down.\n";
    }
    checklist.setText (c, juce::dontSendNotification);
}

void LevelsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (160);
    auto side = r.removeFromRight (160);
    theme::drawPanel (g, side.toFloat(), 8.0f);
    theme::drawPanel (g, r.withTrimmedRight (10).toFloat(), 8.0f);
}

void LevelsPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto cards = r.removeFromTop (70);
    const int w = (cards.getWidth() - 40) / 5;
    for (auto* c : { &momentary, &shortTerm, &integrated, &truePeak, &crest })
    {
        c->setBounds (cards.removeFromLeft (w));
        cards.removeFromLeft (10);
    }
    r.removeFromTop (10);
    auto btnRow = r.removeFromTop (28);
    resetMeters.setBounds (btnRow.removeFromRight (130));
    r.removeFromTop (52);

    auto side = r.removeFromRight (160).reduced (12, 12);
    auto mArea = side.removeFromLeft (60);
    meter.setBounds (mArea);
    side.removeFromLeft (10);
    outputL.setBounds (side.removeFromTop (14));
    output.setBounds (side);

    auto main = r.withTrimmedRight (10).reduced (14, 12);
    checklist.setBounds (main.removeFromTop (main.getHeight() / 2));
    explain.setBounds (main);
}
}
