#include "LevelsPage.h"
#include "Theme.h"

static juce::String u8 (const char* bytes) { return juce::String (juce::CharPointer_UTF8 (bytes)); }

namespace mu
{
LevelsPage::LevelsPage (MUProcessor& p) : proc (p)
{
    for (auto* r : { &momentary, &shortTerm, &integrated, &truePeak, &crest }) addAndMakeVisible (r);
    meter.setLabel ("LUFS M");
    addAndMakeVisible (meter);

    output.setSliderStyle (juce::Slider::LinearVertical);
    output.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    output.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (output);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::outputGain, output);
    outputL.setFont (theme::bodyBold (10.0f));
    outputL.setColour (juce::Label::textColourId, theme::textDim);
    outputL.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputL);

    checklist.setFont (theme::body (13.5f));
    checklist.setColour (juce::Label::textColourId, theme::text);
    checklist.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (checklist);
    explain.setFont (theme::body (12.0f));
    explain.setColour (juce::Label::textColourId, theme::textDim);
    explain.setJustificationType (juce::Justification::topLeft);
    explain.setText ("INTEGRATED is the loudness of the whole song and the number a mastering engineer asks for. "
                     "TRUE PEAK is the ceiling, measured between samples. SHORT-TERM shows how loud the section you are in is; "
                     "MOMENTARY is right now. CREST FACTOR is peak over average: under 8 dB means the mix is already squashed.",
                     juce::dontSendNotification);
    addAndMakeVisible (explain);

    resetMeters.setComponentID ("ghost");
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
    auto near = [] (float v, float t, float tol, float tol2) { return std::abs (v - t) <= tol ? theme::good : std::abs (v - t) <= tol2 ? theme::yellow : theme::bad; };

    momentary.set (fmt (l.getMomentary(), ""), "momentary LUFS", theme::text);
    shortTerm.set (fmt (l.getShortTerm(), ""), "short-term LUFS", theme::text);
    const float integ = l.getIntegrated();
    integrated.set (fmt (integ, ""), isMaster ? "integrated (target " + juce::String (tLufs, 1) + ")" : "integrated LUFS",
                    integ > -90.0f && isMaster ? near (integ, tLufs, 1.0f, 3.0f) : theme::text);
    const float tp = l.getTruePeakHold();
    truePeak.set (fmt (tp, ""), isMaster ? "true peak dBTP (ceiling " + juce::String (tTp, 1) + ")" : "true peak dBTP",
                  tp > -90.0f ? (tp > 0.0f ? theme::bad : isMaster ? (tp <= tTp ? theme::good : theme::yellow) : (tp > -1.0f ? theme::yellow : theme::text)) : theme::text);
    crest.set (l.getCrestFactorDb() > 0.0f ? juce::String (l.getCrestFactorDb(), 1) + " dB" : "--", "crest factor", theme::text);
    meter.setLevel (l.getMomentary());
    meter.setTargetMarker (isMaster ? tLufs : -200.0f);

    juce::String c;
    if (isMaster)
    {
        if (integ < -90.0f) c << u8 ("\xE2\x97\x8B") + "  Play the whole song through so the integrated reading settles.\n\n";
        else c << (std::abs (integ - tLufs) <= 1.0f ? u8 ("\xE2\x9C\x93") + "  Loudness is on target.\n\n"
                 : integ < tLufs ? u8 ("\xE2\x97\x8B") + "  Song is " + juce::String (tLufs - integ, 1) + " LU under target. Learn & Balance on the Mix tab, or switch on Auto trim.\n\n"
                                 : u8 ("\xE2\x97\x8B") + "  Song is " + juce::String (integ - tLufs, 1) + " LU over target. Learn & Balance will pull it down.\n\n");
        if (tp > -90.0f) c << (tp <= tTp ? u8 ("\xE2\x9C\x93") + "  Peaks leave enough headroom.\n\n" : tp > 0.0f ? u8 ("\xE2\x9C\x97") + "  Clipping: the master is going over 0 dBTP. Switch on Peak guard or bring the mix down.\n\n" : u8 ("\xE2\x97\x8B") + "  Peaks are " + juce::String (tp - tTp, 1) + " dB over the ceiling. Peak guard on the Mix tab will catch them.\n\n");
        if (l.getCrestFactorDb() > 0.0f) c << (l.getCrestFactorDb() >= 8.0f ? u8 ("\xE2\x9C\x93") + "  Dynamics are intact. No limiter is squashing the mix.\n" : u8 ("\xE2\x97\x8B") + "  Crest factor is low: the mix looks pre-limited. Bypass any limiter on the stereo out before hand-off.\n");
        const float at = proc.getMasterAutoTrimDb();
        if (std::abs (at) > 0.05f) c << "\nAuto trim is currently adding " << theme::dbString (at) << " on the master.";
    }
    else
    {
        c << "This is a track instance. The Master instance judges the hand-off level; here you can watch this track's own level and peaks.\n\n";
        if (tp > -0.5f) c << u8 ("\xE2\x9C\x97") + "  This track is clipping. Pull its own fader or trim down.\n";
        else c << u8 ("\xE2\x9C\x93") + "  No clipping on this track.\n";
        const float trim = proc.getTrimDb();
        if (std::abs (trim) > 0.05f) c << "\nBalance trim on this track: " << theme::dbString (trim) << ".";
    }
    checklist.setText (c, juce::dontSendNotification);
}

void LevelsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (18, 16);
    r.removeFromTop (72 + 12 + 30 + 12);
    auto side = r.removeFromRight (170);
    theme::drawPanel (g, side.toFloat());
    r.removeFromRight (12);
    theme::drawPanel (g, r.toFloat());
    theme::drawSectionTitle (g, r.removeFromTop (34).reduced (12, 0), "Ready for mastering?");
}

void LevelsPage::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    auto cards = r.removeFromTop (72);
    const int w = (cards.getWidth() - 48) / 5;
    for (auto* c : { &momentary, &shortTerm, &integrated, &truePeak, &crest })
    {
        c->setBounds (cards.removeFromLeft (w));
        cards.removeFromLeft (12);
    }
    r.removeFromTop (12);
    auto btnRow = r.removeFromTop (30);
    resetMeters.setBounds (btnRow.removeFromRight (140));
    r.removeFromTop (12);

    auto side = r.removeFromRight (170).reduced (14, 14);
    auto mArea = side.removeFromLeft (56);
    meter.setBounds (mArea);
    side.removeFromLeft (14);
    outputL.setBounds (side.removeFromTop (14));
    output.setBounds (side);

    r.removeFromRight (12);
    auto main = r.reduced (14, 12);
    main.removeFromTop (30);
    checklist.setBounds (main.removeFromTop (juce::jmax (120, main.getHeight() - 90)));
    explain.setBounds (main);
}
}
