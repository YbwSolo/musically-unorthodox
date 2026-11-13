#include "EQPage.h"
#include "Theme.h"

namespace mu
{
static bool hasGain (EQType t) { return ! (t == EQType::LowCut || t == EQType::HighCut || t == EQType::Notch); }

// ---- EQGraph -------------------------------------------------------------------------------------
EQGraph::EQGraph (MUProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }

juce::Rectangle<float> EQGraph::plotArea() const { return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (18.0f).withTrimmedLeft (34.0f).withTrimmedTop (22.0f); }
float EQGraph::xForHz (float hz) const { auto a = plotArea(); return a.getX() + std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f) * a.getWidth(); }
float EQGraph::hzForX (float x) const { auto a = plotArea(); return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth())); }
float EQGraph::yForDb (float db) const { auto a = plotArea(); return a.getBottom() - (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb) * a.getHeight(); }
float EQGraph::dbForY (float y) const { auto a = plotArea(); return maxDb - juce::jlimit (0.0f, 1.0f, (y - a.getY()) / a.getHeight()) * (maxDb - minDb); }

int EQGraph::bandAt (juce::Point<float> pt) const
{
    int best = -1; float bestD = 14.0f;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto s = proc.getBandSettings (b);
        if (! s.enabled) continue;
        const float d = pt.getDistanceFrom ({ xForHz (s.freqHz), yForDb (hasGain (s.type) ? s.gainDb : 0.0f) });
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EQGraph::setBandParam (const juce::String& id, float value, bool gesture)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        if (gesture) p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        if (gesture) p->endChangeGesture();
    }
}

void EQGraph::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (theme::black);
    g.fillRoundedRectangle (full, 3.0f);
    auto a = plotArea();

    g.setFont (theme::body (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForHz (hz);
        g.setColour (theme::line.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (theme::textMute);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String (hz, 0), (int) x - 16, (int) a.getBottom() + 3, 32, 12, juce::Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForDb (db);
        g.setColour (db == 0.0f ? theme::lineHi : theme::line.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (theme::textMute);
        g.drawText (theme::dbString (db, 0, ""), 2, (int) y - 6, 28, 12, juce::Justification::centredRight);
    }

    // spectrum
    const auto& spec = proc.getSpectrum();
    const auto& disp = spec.getDisplayDb();
    juce::Path sp;
    bool started = false;
    for (int i = 0; i < SpectrumAnalyzer::kDisplayPoints; ++i)
    {
        const float x = xForHz (spec.displayPointHz (i));
        const float y = a.getBottom() - juce::jlimit (0.0f, 1.0f, (disp[(size_t) i] + 90.0f) / 96.0f) * a.getHeight();
        if (! started) { sp.startNewSubPath (x, a.getBottom()); sp.lineTo (x, y); started = true; }
        else sp.lineTo (x, y);
    }
    if (started)
    {
        sp.lineTo (a.getRight(), a.getBottom());
        sp.closeSubPath();
        g.setColour (theme::yellow.withAlpha (0.10f));
        g.fillPath (sp);
        g.setColour (theme::yellow.withAlpha (0.35f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // vocal space ghost
    const auto& vs = proc.getVocalSpace();
    if (vs.getCurrentDepth() > 0.01f)
    {
        g.setColour (theme::text.withAlpha (0.35f * vs.getCurrentDepth()));
        for (int i = 0; i < VocalSpace::kBands; ++i)
        {
            const float x = xForHz (vs.bandFreq (i));
            const float y = yForDb (vs.bandMaxCutDb (i) * vs.getCurrentDepth() * proc.apvts.getRawParameterValue (ids::vsAmount)->load() * 0.01f);
            g.drawLine (x, yForDb (0.0f), x, y, 2.0f);
        }
    }

    const bool eqOn = proc.apvts.getRawParameterValue (ids::eqEnabled)->load() > 0.5f;
    const int steps = 160;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto s = proc.getBandSettings (b);
        if (! s.enabled) continue;
        juce::Path bp;
        for (int i = 0; i <= steps; ++i)
        {
            const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) steps);
            const float y = yForDb (proc.eqBandMagnitudeDbAt (b, hz));
            if (i == 0) bp.startNewSubPath (xForHz (hz), y); else bp.lineTo (xForHz (hz), y);
        }
        auto fill = bp;
        fill.lineTo (a.getRight(), yForDb (0.0f));
        fill.lineTo (a.getX(), yForDb (0.0f));
        fill.closeSubPath();
        g.setColour (theme::bandColour (b).withAlpha (b == selected ? 0.16f : 0.07f));
        g.fillPath (fill);
        // dynamic movement indicator: a line from the static node down/up to where the band currently sits
        if (s.dynamic && hasGain (s.type))
        {
            const float off = proc.getEQ().getBandDynamicOffsetDb (b);
            if (std::abs (off) > 0.05f)
            {
                g.setColour (theme::bandColour (b).withAlpha (0.9f));
                g.drawLine (xForHz (s.freqHz), yForDb (s.gainDb), xForHz (s.freqHz), yForDb (s.gainDb + off), 3.0f);
            }
        }
    }
    juce::Path total;
    for (int i = 0; i <= steps; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) steps);
        const float y = yForDb (proc.eqMagnitudeDbAt (hz));
        if (i == 0) total.startNewSubPath (xForHz (hz), y); else total.lineTo (xForHz (hz), y);
    }
    g.setColour (eqOn ? theme::text : theme::textMute);
    g.strokePath (total, juce::PathStrokeType (2.0f));

    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto s = proc.getBandSettings (b);
        if (! s.enabled) continue;
        auto c = juce::Point<float> (xForHz (s.freqHz), yForDb (hasGain (s.type) ? s.gainDb : 0.0f));
        const float rad = b == selected ? 8.0f : (b == hovered ? 7.0f : 6.0f);
        g.setColour (theme::bandColour (b).withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (rad * 2 + 6, rad * 2 + 6).withCentre (c));
        g.setColour (theme::bandColour (b));
        g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
        if (s.dynamic)
        {
            g.setColour (theme::black);
            g.drawEllipse (juce::Rectangle<float> (rad * 2 - 4, rad * 2 - 4).withCentre (c), 1.5f);
        }
        g.setColour (theme::black);
        g.setFont (theme::bodyBold (9.5f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c).toNearestInt(), juce::Justification::centred);
    }

    g.setColour (theme::textMute);
    g.setFont (theme::body (10.5f));
    g.drawText ("double-click: add band    drag: freq / gain    wheel: Q    alt-click: remove", getLocalBounds().removeFromTop (20).reduced (10, 0), juce::Justification::centredRight);
}

void EQGraph::mouseMove (const juce::MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b != hovered) { hovered = b; repaint(); }
}

void EQGraph::mouseDown (const juce::MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b >= 0)
    {
        if (e.mods.isAltDown()) { setBandParam (ids::eqBandEnabled (b), 0.0f, true); repaint(); return; }
        selected = b; dragging = b;
        if (auto* pf = proc.apvts.getParameter (ids::eqBandFreq (b))) pf->beginChangeGesture();
        if (auto* pg = proc.apvts.getParameter (ids::eqBandGain (b))) pg->beginChangeGesture();
        if (onSelectionChanged) onSelectionChanged (b);
        repaint();
    }
}

void EQGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto s = proc.getBandSettings (dragging);
    setBandParam (ids::eqBandFreq (dragging), hzForX (e.position.x), false);
    if (hasGain (s.type))
        setBandParam (ids::eqBandGain (dragging), e.mods.isShiftDown() ? s.gainDb : dbForY (e.position.y), false);
    repaint();
}

void EQGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        if (auto* pf = proc.apvts.getParameter (ids::eqBandFreq (dragging))) pf->endChangeGesture();
        if (auto* pg = proc.apvts.getParameter (ids::eqBandGain (dragging))) pg->endChangeGesture();
    }
    dragging = -1;
}

void EQGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (bandAt (e.position) >= 0) return;
    const float hz = hzForX (e.position.x);
    int best = -1; float bestD = 1.0e9f;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        if (proc.getBandSettings (b).enabled) continue;
        const float d = std::abs (std::log (kDefaultBands[b].freq / hz));
        if (d < bestD) { bestD = d; best = b; }
    }
    if (best < 0) return;
    const EQType type = hz < 60.0f ? EQType::LowCut : hz < 150.0f ? EQType::LowShelf : hz > 9000.0f ? EQType::HighShelf : EQType::Bell;
    setBandParam (ids::eqBandType (best), (float) type, true);
    setBandParam (ids::eqBandFreq (best), hz, true);
    setBandParam (ids::eqBandGain (best), type == EQType::LowCut ? 0.0f : dbForY (e.position.y), true);
    setBandParam (ids::eqBandQ (best), 0.71f, true);
    setBandParam (ids::eqBandDyn (best), 0.0f, true);
    setBandParam (ids::eqBandEnabled (best), 1.0f, true);
    setBandParam (ids::eqEnabled, 1.0f, true);
    selected = best;
    if (onSelectionChanged) onSelectionChanged (best);
    repaint();
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandAt (e.position) >= 0 ? bandAt (e.position) : selected;
    if (b < 0 || ! proc.getBandSettings (b).enabled) return;
    const float q = proc.getBandSettings (b).q;
    setBandParam (ids::eqBandQ (b), juce::jlimit (0.1f, 18.0f, q * std::pow (1.15f, -w.deltaY * 5.0f)), true);
    repaint();
}

// ---- EQPage --------------------------------------------------------------------------------------
EQPage::EQPage (MUProcessor& p) : proc (p), graph (p)
{
    addAndMakeVisible (graph);
    graph.onSelectionChanged = [this] (int b) { bindBand (b); };

    addAndMakeVisible (eqOn);
    eqOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::eqEnabled, eqOn);
    addAndMakeVisible (bandOn);
    addAndMakeVisible (dynOn);
    dynOn.setTooltip ("Dynamic band: the gain only moves when the level in this band goes over the threshold. Use it for mud, harshness and masking so the cut is only there when it is needed.");
    typeBox.addItemList (eqTypeNames(), 1);
    addAndMakeVisible (typeBox);

    auto knob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 17);
        addAndMakeVisible (s);
    };
    knob (freq); knob (gain); knob (q); knob (thr); knob (range);

    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto& btn = bandButtons[(size_t) b];
        btn.setButtonText (juce::String (b + 1));
        btn.setColour (juce::TextButton::buttonOnColourId, theme::bandColour (b));
        btn.onClick = [this, b] { graph.setSelectedBand (b); bindBand (b); };
        addAndMakeVisible (btn);
    }

    bandTitle.setFont (theme::headline (15.0f));
    addAndMakeVisible (bandTitle);
    dynHint.setFont (theme::body (11.0f));
    dynHint.setColour (juce::Label::textColourId, theme::textMute);
    dynHint.setText ("THRESHOLD = level in this band where it starts moving.  RANGE = how far it moves (minus = cut).", juce::dontSendNotification);
    addAndMakeVisible (dynHint);

    cleanHarsh.setComponentID ("primary");
    cleanHarsh.setTooltip ("Finds persistent harsh resonances between 1.2 and 12 kHz in this track and adds narrow dynamic cuts on bands 4-6.");
    cleanHarsh.onClick = [this] { cleanResult.setText (proc.cleanHarsh(), juce::dontSendNotification); };
    addAndMakeVisible (cleanHarsh);
    reset.setComponentID ("ghost");
    reset.onClick = [this] { proc.resetEQ(); cleanResult.setText ("", juce::dontSendNotification); };
    addAndMakeVisible (reset);
    cleanResult.setFont (theme::body (11.5f));
    cleanResult.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (cleanResult);

    bindBand (graph.getSelectedBand());
}

void EQPage::bindBand (int band)
{
    if (band == boundBand) return;
    boundBand = band;
    bandOnAtt.reset(); typeAtt.reset(); freqAtt.reset(); gainAtt.reset(); qAtt.reset(); dynOnAtt.reset(); thrAtt.reset(); rangeAtt.reset();
    bandOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::eqBandEnabled (band), bandOn);
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::eqBandType (band), typeBox);
    freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandFreq (band), freq);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandGain (band), gain);
    qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandQ (band), q);
    dynOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::eqBandDyn (band), dynOn);
    thrAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandThr (band), thr);
    rangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandRange (band), range);
    bandTitle.setText ("BAND " + juce::String (band + 1), juce::dontSendNotification);
    bandTitle.setColour (juce::Label::textColourId, theme::bandColour (band));
    for (int b = 0; b < kNumEQBands; ++b) bandButtons[(size_t) b].setToggleState (b == band, juce::dontSendNotification);
}

void EQPage::refresh()
{
    graph.repaint();
    const auto s = proc.getBandSettings (graph.getSelectedBand());
    const bool hg = hasGain (s.type);
    gain.setEnabled (hg);
    dynOn.setEnabled (hg);
    thr.setEnabled (hg && s.dynamic);
    range.setEnabled (hg && s.dynamic);
}

void EQPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (18, 16);
    auto bottom = r.removeFromBottom (150);
    theme::drawPanel (g, bottom.toFloat());
    auto labels = bottom.reduced (14, 10);
    labels.removeFromTop (26);
    g.setColour (theme::textDim);
    g.setFont (theme::bodyBold (10.0f));
    const char* names[5] = { "FREQ", "GAIN", "Q", "THRESHOLD", "RANGE" };
    juce::Slider* sl[5] = { &freq, &gain, &q, &thr, &range };
    for (int i = 0; i < 5; ++i)
        g.drawText (names[i], sl[i]->getBounds().withY (sl[i]->getY() - 14).withHeight (14), juce::Justification::centred);
    // divider between static and dynamic controls
    g.setColour (theme::line);
    g.fillRect (thr.getX() - 14, bottom.getY() + 44, 1, bottom.getHeight() - 56);
}

void EQPage::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    auto bottom = r.removeFromBottom (150);
    r.removeFromBottom (12);
    auto topRow = r.removeFromTop (28);
    eqOn.setBounds (topRow.removeFromLeft (70));
    int x = topRow.getX() + 84;
    for (auto& b : bandButtons) { b.setBounds (x, topRow.getY(), 28, 26); x += 32; }
    reset.setBounds (topRow.removeFromRight (110));
    topRow.removeFromRight (8);
    cleanHarsh.setBounds (topRow.removeFromRight (140));
    topRow.removeFromRight (10);
    cleanResult.setBounds (topRow.withTrimmedLeft (x - topRow.getX() + 12));
    r.removeFromTop (10);
    graph.setBounds (r);

    auto panel = bottom.reduced (14, 10);
    auto head = panel.removeFromTop (26);
    bandTitle.setBounds (head.removeFromLeft (80));
    bandOn.setBounds (head.removeFromLeft (96));
    typeBox.setBounds (head.removeFromLeft (140).withHeight (24));
    head.removeFromLeft (24);
    dynOn.setBounds (head.removeFromLeft (100));
    dynHint.setBounds (head);
    panel.removeFromTop (16);
    const int kw = 96;
    juce::Slider* sl[5] = { &freq, &gain, &q, &thr, &range };
    int kx = panel.getX();
    for (int i = 0; i < 5; ++i)
    {
        if (i == 3) kx += 28;
        sl[i]->setBounds (kx, panel.getY(), kw, panel.getHeight());
        kx += kw + 6;
    }
}
}
