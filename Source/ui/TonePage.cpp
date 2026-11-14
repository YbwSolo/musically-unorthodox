#include "TonePage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

TonePage::TonePage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      smDepth (p, ids::smDepth, "Depth"), smFocus (p, ids::smFocus, { "Full range", "Low mids", "Harsh highs" }), smSpeed (p, ids::smSpeed, { "Slow", "Normal", "Fast" }),
      satType (p, ids::satType, { "Tape", "Tube", "Warm" }), satDrive (p, ids::satDrive, "Drive"), satMix (p, ids::satMix, "Mix"),
      riderTarget (p, ids::riderTarget, "Target"), riderRange (p, ids::riderRange, "Range"), riderSpeed (p, ids::riderSpeed, { "Slow", "Normal", "Fast" }),
      vsAmount (p, ids::vsAmount, "Amount"), vsSpeed (p, ids::vsSpeed, { "Smooth", "Normal", "Fast" }),
      stWidth (p, ids::stWidth, "Width"), stMonoFreq (p, ids::stMonoFreq, "Mono below"), stMonoBass (p, ids::stMonoBass, "Mono bass"),
      outGain (p, ids::outputGain, "Output"), trim (p, ids::trim, "Balance trim")
{
    useTabPresets (presets, proc, "tone", setStatus);
    addAndMakeVisible (presets);
    smooth.setPower (p, ids::smOn); sat.setPower (p, ids::satOn); rider.setPower (p, ids::riderOn); space.setPower (p, ids::vsEnabled);
    for (auto* c : std::initializer_list<juce::Component*> { &smooth, &sat, &rider, &space, &stereo, &output, &smDepth, &smFocus, &smSpeed, &smDelta, &satType, &satDrive, &satMix,
                                                              &riderTarget, &riderRange, &riderSpeed, &vsAmount, &vsSpeed, &stWidth, &stMonoFreq, &stMonoBass, &outGain, &trim })
        addAndMakeVisible (c);
    smDelta.setClickingTogglesState (true);
    smDelta.onClick = [this] { proc.setListenMode (smDelta.getToggleState() ? Listen::SmoothDelta : Listen::Normal); };
    setHelp (stereo, "st.corr");
}

void TonePage::visibilityChanged()
{
    if (! isVisible())
    {
        if (proc.getListenMode() == Listen::SmoothDelta) proc.setListenMode (Listen::Normal);
        smDelta.setToggleState (false, juce::dontSendNotification);
    }
}

void TonePage::tick (float dt)
{
    presets.refresh();
    const auto& sm = proc.getSmoother();
    for (int i = 0; i < Smoother::kBands; ++i)
    {
        const float c = sm.getBandCutDb (i);
        cuts[(size_t) i] = c < cuts[(size_t) i] ? approach (cuts[(size_t) i], c, dt, 0.03f) : approach (cuts[(size_t) i], c, dt, 0.12f);
    }
    proc.pumpAnalysis();
    const auto& live = proc.getSpectrum().getDisplayDb();
    const bool playing = proc.isPlaying();
    for (int i = 0; i < SpectrumAnalyzer::kDisplayPoints; ++i)
        spectrum[(size_t) i] = approach (spectrum[(size_t) i], playing ? live[(size_t) i] : -96.0f, dt, playing ? 0.06f : 0.35f);
    rideGain = approach (rideGain, proc.getRider().getCurrentGainDb(), dt, 0.08f);
    vsDepth = approach (vsDepth, proc.getVocalSpace().getCurrentDepth(), dt, 0.06f);
    corr = approach (corr, proc.getStereo().getCorrelation(), dt, 0.15f);
    const float pk = juce::Decibels::decibelsToGain (juce::jmax (proc.getLoudness().getChannelPeakDb (0), proc.getLoudness().getChannelPeakDb (1)), -100.0f);
    peakLin = pk > peakLin ? pk : approach (peakLin, pk, dt, 0.2f);
    for (auto* r : { &smoothWell, &satWell, &riderWell, &spaceWell, &corrWell }) repaint (*r);

    slow -= dt;
    if (slow <= 0.0f)
    {
        slow = 0.2f;
        const float total = sm.getTotalCutDb();
        smooth.setReadout (total < -0.1f ? juce::String (total, 1) + " dB" : juce::String());
        rider.setReadout (proc.getParam (ids::riderOn) > 0.5f ? dbString (rideGain) : juce::String());
        const auto role = proc.getEffectiveRole();
        const bool vsAllowed = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master;
        space.setTitle (vsAllowed ? "Vocal space" : "Vocal space (for beats and instruments)");
        vsAmount.setDimmed (! vsAllowed);
        stMonoFreq.setDimmed (proc.getParam (ids::stMonoBass) < 0.5f);
    }
}

void TonePage::paint (juce::Graphics& g) { g.fillAll (black); }

void TonePage::paintOverChildren (juce::Graphics& g)
{
    // ---- Smooth: the track's spectrum, with what is being pulled down hanging from the top
    {
        const auto r = smoothWell.toFloat();
        drawWell (g, r);
        const auto plot = r.reduced (1.0f);
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        drawFreqGrid (g, plot);

        juce::Path shape;
        const int n = SpectrumAnalyzer::kDisplayPoints;
        shape.startNewSubPath (plot.getX(), plot.getBottom() + 2.0f);
        for (int i = 0; i < n; ++i)
            shape.lineTo (plot.getX() + plot.getWidth() * (float) i / (float) (n - 1),
                          plot.getBottom() - juce::jlimit (0.0f, 1.0f, (spectrum[(size_t) i] + 84.0f) / 78.0f) * plot.getHeight());
        shape.lineTo (plot.getRight(), plot.getBottom() + 2.0f);
        shape.closeSubPath();
        g.setGradientFill (juce::ColourGradient (text.withAlpha (0.16f), 0.0f, plot.getY(), text.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (shape);
        g.setColour (text.withAlpha (0.3f));
        g.strokePath (shape, juce::PathStrokeType (1.0f));

        const float maxCut = 9.0f, reach = plot.getHeight() * 0.6f;
        g.setFont (body (10.0f));
        for (int db = 3; db <= 6; db += 3)
        {
            const float y = plot.getY() + reach * (float) db / maxCut;
            g.setColour (juce::Colour (0xff1c1c1c));
            g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
            g.setColour (textMute);
            g.drawText ("-" + juce::String (db), juce::Rectangle<float> (plot.getRight() - 28.0f, y - 13.0f, 24.0f, 12.0f), juce::Justification::centredRight);
        }
        const auto& sm = proc.getSmoother();
        const bool on = proc.getParam (ids::smOn) > 0.5f;
        for (int i = 0; i < Smoother::kBands; ++i)
        {
            const float x = freqToX (sm.bandFreq (i), plot.getX(), plot.getWidth());
            const float w = juce::jmax (6.0f, (freqToX (sm.bandFreq (i) * 1.189f, plot.getX(), plot.getWidth()) - x) - 3.0f);
            const float h = juce::jlimit (0.0f, 1.0f, -cuts[(size_t) i] / maxCut) * reach;
            g.setColour (on ? yellow.withAlpha (0.22f) : control);
            g.fillRect (x - w * 0.5f, plot.getY(), w, 2.0f);
            if (h > 0.5f) { g.setColour (yellow); g.fillRect (x - w * 0.5f, plot.getY(), w, h + 2.0f); }
        }
        g.restoreState();
        g.setFont (body (11.0f));
        g.setColour (textMute);
        g.drawText (on ? "Yellow bars: how far each resonance is being turned down right now" : "Switch Smooth on to tame resonances",
                    juce::Rectangle<float> (plot.getX() + 12.0f, plot.getY() + 10.0f, plot.getWidth() - 60.0f, 14.0f), juce::Justification::centredLeft);
    }

    // ---- Saturation: the shape the wave is pushed through
    {
        const auto r = satWell.toFloat();
        drawWell (g, r);
        const auto plot = r.reduced (12.0f);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRect (plot.getCentreX(), plot.getY(), 1.0f, plot.getHeight());
        g.fillRect (plot.getX(), plot.getCentreY(), plot.getWidth(), 1.0f);
        g.setColour (lineHi);
        g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);
        const int type = (int) proc.getParam (ids::satType);
        const float drive = juce::Decibels::decibelsToGain (proc.getParam (ids::satDrive));
        const float mix = proc.getParam (ids::satMix) * 0.01f;
        const bool on = proc.getParam (ids::satOn) > 0.5f;
        const float norm = juce::jmax (0.001f, std::abs (Saturator::shape (1.0f, type, drive)));
        auto shaped = [&] (float x) { const float wet = Saturator::shape (x, type, drive) / norm; return on ? x + (wet - x) * mix : x; };
        juce::Path curve, lit;
        bool litStarted = false;
        const float reach = juce::jlimit (0.02f, 1.0f, peakLin);
        for (int i = 0; i <= 160; ++i)
        {
            const float x = -1.0f + (float) i / 80.0f;
            const float px = plot.getCentreX() + x * plot.getWidth() * 0.5f, py = plot.getCentreY() - shaped (x) * plot.getHeight() * 0.5f;
            if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
            if (std::abs (x) <= reach) { if (! litStarted) { lit.startNewSubPath (px, py); litStarted = true; } else lit.lineTo (px, py); }
        }
        g.setColour (on ? text.withAlpha (0.35f) : textMute);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
        if (on && proc.isPlaying()) { g.setColour (yellow); g.strokePath (lit, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    }

    // ---- Rider: the fader move it is making
    {
        const auto r = riderWell.toFloat();
        drawWell (g, r);
        const float range = juce::jmax (1.0f, proc.getParam (ids::riderRange));
        const auto track = r.reduced (14.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 28.0f, 4.0f);
        g.setColour (control);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (lineHi);
        g.fillRect (track.getCentreX() - 0.5f, r.getY() + 8.0f, 1.0f, r.getHeight() - 16.0f);
        const float x = track.getCentreX() + juce::jlimit (-1.0f, 1.0f, rideGain / range) * track.getWidth() * 0.5f;
        g.setColour (yellow);
        g.fillRoundedRectangle (juce::jmin (x, track.getCentreX()), track.getY(), std::abs (x - track.getCentreX()), 4.0f, 2.0f);
        g.setColour (text);
        g.fillRoundedRectangle (x - 4.0f, r.getCentreY() - 10.0f, 8.0f, 20.0f, 3.0f);
        g.setFont (body (10.0f));
        g.setColour (textMute);
        g.drawText ("-" + juce::String (juce::roundToInt (range)), juce::Rectangle<float> (r.getX() + 6.0f, r.getBottom() - 14.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
        g.drawText ("+" + juce::String (juce::roundToInt (range)), juce::Rectangle<float> (r.getRight() - 36.0f, r.getBottom() - 14.0f, 30.0f, 11.0f), juce::Justification::centredRight);
    }

    // ---- Vocal space: the pocket being opened for the voice
    {
        const auto r = spaceWell.toFloat();
        drawWell (g, r);
        const auto plot = r.reduced (1.0f);
        const auto& vs = proc.getVocalSpace();
        const float amount = proc.getParam (ids::vsAmount) * 0.01f;
        const double fs = 48000.0;
        juce::Path full, now;
        for (int i = 0; i <= 100; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 100.0f;
            const float hz = xToFreq (x, plot.getX(), plot.getWidth());
            float cutFull = 0.0f;
            for (int b = 0; b < VocalSpace::kBands; ++b)
            {
                EQBandSettings s { true, EQType::Bell, vs.bandFreq (b), vs.bandMaxCutDb (b) * amount, 0.9f };
                cutFull += EQEngine::bandResponseDb (s, hz, fs);
            }
            const float yFull = plot.getY() + 10.0f + (-cutFull / 6.0f) * (plot.getHeight() - 20.0f), yNow = plot.getY() + 10.0f + (-cutFull * vsDepth / 6.0f) * (plot.getHeight() - 20.0f);
            if (i == 0) { full.startNewSubPath (x, yFull); now.startNewSubPath (x, yNow); } else { full.lineTo (x, yFull); now.lineTo (x, yNow); }
        }
        g.setColour (lineHi);
        g.strokePath (full, juce::PathStrokeType (1.0f));
        g.setColour (yellow);
        g.strokePath (now, juce::PathStrokeType (2.0f));
    }

    // ---- Stereo: correlation
    {
        const auto r = corrWell.toFloat();
        g.setFont (body (10.5f));
        g.setColour (textMute);
        g.drawText ("Correlation", r.withHeight (12.0f), juce::Justification::centredLeft);
        const auto track = juce::Rectangle<float> (r.getX(), r.getY() + 18.0f, r.getWidth(), 6.0f);
        g.setColour (control);
        g.fillRoundedRectangle (track, 3.0f);
        const float x = track.getCentreX() + juce::jlimit (-1.0f, 1.0f, corr) * track.getWidth() * 0.5f;
        g.setColour (corr < 0.0f ? over : yellow);
        g.fillRoundedRectangle (juce::jmin (x, track.getCentreX()), track.getY(), juce::jmax (2.0f, std::abs (x - track.getCentreX())), 6.0f, 3.0f);
        g.setColour (textMute);
        g.drawText ("-1", juce::Rectangle<float> (r.getX(), r.getY() + 28.0f, 20.0f, 11.0f), juce::Justification::centredLeft);
        g.drawText ("0", juce::Rectangle<float> (r.getCentreX() - 10.0f, r.getY() + 28.0f, 20.0f, 11.0f), juce::Justification::centred);
        g.drawText ("+1", juce::Rectangle<float> (r.getRight() - 20.0f, r.getY() + 28.0f, 20.0f, 11.0f), juce::Justification::centredRight);
    }

    g.setFont (body (10.5f));
    g.setColour (textMute);
    g.drawText ("Focus", smFocusLabel, juce::Justification::centredLeft);
    g.drawText ("Speed", smSpeedLabel, juce::Justification::centredLeft);
}

void TonePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    presets.setBounds (r.removeFromTop (26).removeFromRight (270));
    r.removeFromTop (8);
    auto top = r.removeFromTop ((r.getHeight() - 10) * 55 / 100);
    r.removeFromTop (10);

    // ---- top row: Smooth | Saturation
    auto satArea = top.removeFromRight (360);
    top.removeFromRight (10);
    smooth.setBounds (top);
    sat.setBounds (satArea);

    auto s = top.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto sc = s.removeFromBottom (92);
    s.removeFromBottom (10);
    smoothWell = s;
    smDepth.setBounds (sc.removeFromLeft (84));
    sc.removeFromLeft (16);
    smDelta.setBounds (sc.removeFromRight (34).withSizeKeepingCentre (34, 34));
    sc.removeFromRight (16);
    auto focusCol = sc.removeFromLeft (juce::jmin (300, sc.getWidth() * 58 / 100));
    sc.removeFromLeft (16);
    focusCol = focusCol.withSizeKeepingCentre (focusCol.getWidth(), 46);
    smFocusLabel = focusCol.removeFromTop (16);
    smFocus.setBounds (focusCol.removeFromTop (28));
    auto speedCol = sc.withSizeKeepingCentre (sc.getWidth(), 46);
    smSpeedLabel = speedCol.removeFromTop (16);
    smSpeed.setBounds (speedCol.removeFromTop (28));

    auto a = satArea.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto ac = a.removeFromRight (150);
    a.removeFromRight (12);
    satWell = a.withSizeKeepingCentre (juce::jmin (a.getWidth(), a.getHeight()), juce::jmin (a.getWidth(), a.getHeight()));
    satType.setBounds (ac.removeFromTop (28));
    ac.removeFromTop (10);
    layoutRow (ac.removeFromTop (juce::jmin (96, ac.getHeight())), { &satDrive, &satMix }, 6);

    // ---- bottom row: Rider | Vocal space | Stereo | Output
    const int gap = 10;
    const int outW = 190;
    const int cellW = (r.getWidth() - outW - gap * 3) / 3;
    auto riderArea = r.removeFromLeft (cellW); r.removeFromLeft (gap);
    auto spaceArea = r.removeFromLeft (cellW); r.removeFromLeft (gap);
    auto stereoArea = r.removeFromLeft (cellW); r.removeFromLeft (gap);
    rider.setBounds (riderArea); space.setBounds (spaceArea); stereo.setBounds (stereoArea); output.setBounds (r);

    auto inner = [] (juce::Rectangle<int> area) { return area.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12); };
    auto ri = inner (riderArea);
    riderWell = ri.removeFromTop (40);
    ri.removeFromTop (8);
    riderSpeed.setBounds (ri.removeFromBottom (26));
    ri.removeFromBottom (6);
    layoutRow (ri, { &riderTarget, &riderRange }, 6);

    auto si = inner (spaceArea);
    spaceWell = si.removeFromTop (40);
    si.removeFromTop (8);
    vsSpeed.setBounds (si.removeFromBottom (26));
    si.removeFromBottom (6);
    vsAmount.setBounds (si.withSizeKeepingCentre (90, si.getHeight()));

    auto ti = inner (stereoArea);
    corrWell = ti.removeFromTop (40);
    ti.removeFromTop (8);
    stMonoBass.setBounds (ti.removeFromBottom (26));
    ti.removeFromBottom (6);
    layoutRow (ti, { &stWidth, &stMonoFreq }, 6);

    auto oi = inner (r);
    layoutRow (oi.withSizeKeepingCentre (oi.getWidth(), juce::jmin (oi.getHeight(), 110)), { &outGain, &trim }, 6);
}
}
