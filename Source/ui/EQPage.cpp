#include "EQPage.h"
#include "Theme.h"
#include "../license/License.h"

namespace mu
{
static const juce::Colour kBandColours[kNumEQBands] = {
    juce::Colour (0xffff6b6b), juce::Colour (0xffffb347), juce::Colour (0xfff9e45b),
    juce::Colour (0xff4dd9a0), juce::Colour (0xff4da3ff), juce::Colour (0xffb98cff)
};

// ---- EQGraph -------------------------------------------------------------------------------------
EQGraph::EQGraph (MUProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }

juce::Rectangle<float> EQGraph::plotArea() const { return getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (16.0f).withTrimmedLeft (30.0f); }
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
        const float gainForNode = (s.type == EQType::LowCut || s.type == EQType::HighCut || s.type == EQType::Notch) ? 0.0f : s.gainDb;
        const float d = pt.getDistanceFrom ({ xForHz (s.freqHz), yForDb (gainForNode) });
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void EQGraph::setBandParam (int band, const juce::String& id, float value, bool gesture)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        if (gesture) p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        if (gesture) p->endChangeGesture();
    }
    juce::ignoreUnused (band);
}

void EQGraph::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (theme::bg);
    g.fillRoundedRectangle (full, 8.0f);
    auto a = plotArea();

    // grid
    g.setFont (theme::labelFont (10.0f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForHz (hz);
        g.setColour (theme::line.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (theme::textMute);
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String (hz, 0), (int) x - 16, (int) a.getBottom() + 2, 32, 12, juce::Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForDb (db);
        g.setColour (db == 0.0f ? theme::textMute : theme::line.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        g.setColour (theme::textMute);
        g.drawText (theme::dbString (db, 0, ""), 2, (int) y - 6, 26, 12, juce::Justification::centredRight);
    }

    // spectrum
    const auto& spec = proc.getSpectrum();
    const auto& disp = spec.getDisplayDb();
    juce::Path sp;
    bool started = false;
    for (int i = 0; i < SpectrumAnalyzer::kDisplayPoints; ++i)
    {
        const float hz = spec.displayPointHz (i);
        const float x = xForHz (hz);
        const float y = a.getBottom() - juce::jlimit (0.0f, 1.0f, (disp[(size_t) i] + 90.0f) / 96.0f) * a.getHeight();
        if (! started) { sp.startNewSubPath (x, a.getBottom()); sp.lineTo (x, y); started = true; }
        else sp.lineTo (x, y);
    }
    if (started)
    {
        sp.lineTo (a.getRight(), a.getBottom());
        sp.closeSubPath();
        g.setColour (theme::accent.withAlpha (0.16f));
        g.fillPath (sp);
        g.setColour (theme::accent.withAlpha (0.45f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // vocal space ghost bands
    const auto& vs = proc.getVocalSpace();
    if (vs.getCurrentDepth() > 0.01f)
    {
        g.setColour (theme::pro.withAlpha (0.35f * vs.getCurrentDepth()));
        for (int i = 0; i < VocalSpace::kBands; ++i)
        {
            const float x = xForHz (vs.bandFreq (i));
            const float y = yForDb (vs.bandMaxCutDb (i) * vs.getCurrentDepth() * proc.apvts.getRawParameterValue (ids::vsAmount)->load() * 0.01f);
            g.drawLine (x, yForDb (0.0f), x, y, 2.0f);
        }
    }

    // individual band curves + total curve
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
        g.setColour (kBandColours[b].withAlpha (b == selected ? 0.18f : 0.08f));
        g.fillPath (fill);
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

    // nodes
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto s = proc.getBandSettings (b);
        if (! s.enabled) continue;
        const float gainForNode = (s.type == EQType::LowCut || s.type == EQType::HighCut || s.type == EQType::Notch) ? 0.0f : s.gainDb;
        auto c = juce::Point<float> (xForHz (s.freqHz), yForDb (gainForNode));
        const float rad = b == selected ? 8.0f : (b == hovered ? 7.0f : 6.0f);
        g.setColour (kBandColours[b].withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (rad * 2 + 6, rad * 2 + 6).withCentre (c));
        g.setColour (kBandColours[b]);
        g.fillEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c));
        g.setColour (theme::bg);
        g.setFont (theme::titleFont (10.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c).toNearestInt(), juce::Justification::centred);
    }

    g.setColour (theme::textMute);
    g.setFont (theme::labelFont (10.0f));
    g.drawText ("double-click: add band   |   drag: freq/gain   |   wheel: Q   |   alt-click: remove", a.withHeight (14.0f).toNearestInt().translated (0, 2), juce::Justification::centredRight);
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
        if (e.mods.isAltDown()) { setBandParam (b, ids::eqBandEnabled (b), 0.0f, true); repaint(); return; }
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
    const float hz = hzForX (e.position.x);
    setBandParam (dragging, ids::eqBandFreq (dragging), hz, false);
    if (! (s.type == EQType::LowCut || s.type == EQType::HighCut || s.type == EQType::Notch))
        setBandParam (dragging, ids::eqBandGain (dragging), e.mods.isShiftDown() ? s.gainDb : dbForY (e.position.y), false);
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
    // Use the free band whose default frequency is closest to where the user clicked.
    int best = -1; float bestD = 1.0e9f;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        if (proc.getBandSettings (b).enabled) continue;
        const float d = std::abs (std::log (kDefaultBands[b].freq / hz));
        if (d < bestD) { bestD = d; best = b; }
    }
    if (best < 0) return;
    const EQType type = hz < 60.0f ? EQType::LowCut : hz < 150.0f ? EQType::LowShelf : hz > 9000.0f ? EQType::HighShelf : EQType::Bell;
    setBandParam (best, ids::eqBandType (best), (float) type, true);
    setBandParam (best, ids::eqBandFreq (best), hz, true);
    setBandParam (best, ids::eqBandGain (best), type == EQType::LowCut ? 0.0f : dbForY (e.position.y), true);
    setBandParam (best, ids::eqBandQ (best), 0.71f, true);
    setBandParam (best, ids::eqBandEnabled (best), 1.0f, true);
    setBandParam (best, ids::eqEnabled, 1.0f, true);
    selected = best;
    if (onSelectionChanged) onSelectionChanged (best);
    repaint();
}

void EQGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandAt (e.position) >= 0 ? bandAt (e.position) : selected;
    if (b < 0 || ! proc.getBandSettings (b).enabled) return;
    const float q = proc.getBandSettings (b).q;
    setBandParam (b, ids::eqBandQ (b), juce::jlimit (0.1f, 18.0f, q * std::pow (1.15f, -w.deltaY * 5.0f)), true);
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
    typeBox.addItemList (eqTypeNames(), 1);
    addAndMakeVisible (typeBox);

    auto knob = [this] (juce::Slider& s, juce::Label& l)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (s);
        l.setFont (theme::labelFont (11.0f));
        l.setColour (juce::Label::textColourId, theme::textDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    knob (freq, freqL); knob (gain, gainL); knob (q, qL);

    for (int b = 0; b < kNumEQBands; ++b)
    {
        auto& btn = bandButtons[(size_t) b];
        btn.setButtonText (juce::String (b + 1));
        btn.setColour (juce::TextButton::buttonOnColourId, kBandColours[b]);
        btn.onClick = [this, b] { graph.setSelectedBand (b); bindBand (b); };
        addAndMakeVisible (btn);
    }

    bandTitle.setFont (theme::titleFont (12.0f));
    bandTitle.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (bandTitle);

    cleanHarsh.setTooltip ("Pro: finds persistent harsh resonances between 1.2 and 12 kHz in this track and cuts them with narrow bands.");
    cleanHarsh.onClick = [this]
    {
        if (! License::get().isPro()) { cleanResult.setText ("Clean Harsh is a Pro feature - add your key in Settings.", juce::dontSendNotification); return; }
        cleanResult.setText (proc.cleanHarsh(), juce::dontSendNotification);
    };
    addAndMakeVisible (cleanHarsh);
    reset.onClick = [this] { proc.resetEQ(); cleanResult.setText ("", juce::dontSendNotification); };
    addAndMakeVisible (reset);
    cleanResult.setFont (theme::labelFont (11.0f));
    cleanResult.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (cleanResult);

    // Vocal space
    addAndMakeVisible (vsOn);
    vsOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::vsEnabled, vsOn);
    vsAmount.setSliderStyle (juce::Slider::LinearHorizontal);
    vsAmount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    addAndMakeVisible (vsAmount);
    vsAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::vsAmount, vsAmount);
    vsAmountL.setFont (theme::labelFont (11.0f));
    vsAmountL.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (vsAmountL);
    vsSpeed.addItemList ({ "Smooth", "Normal", "Fast" }, 1);
    addAndMakeVisible (vsSpeed);
    vsSpeedAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::vsSpeed, vsSpeed);
    vsInfo.setFont (theme::labelFont (11.0f));
    vsInfo.setColour (juce::Label::textColourId, theme::textDim);
    vsInfo.setText ("Ducks the beat around 260 Hz, 2.6 kHz and 7 kHz only while the Vocal instance is singing, so the vocal sits on top without turning the beat down.", juce::dontSendNotification);
    addAndMakeVisible (vsInfo);

    bindBand (graph.getSelectedBand());
}

void EQPage::bindBand (int band)
{
    if (band == boundBand) return;
    boundBand = band;
    bandOnAtt.reset(); typeAtt.reset(); freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    bandOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::eqBandEnabled (band), bandOn);
    typeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::eqBandType (band), typeBox);
    freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandFreq (band), freq);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandGain (band), gain);
    qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::eqBandQ (band), q);
    bandTitle.setText ("BAND " + juce::String (band + 1), juce::dontSendNotification);
    bandTitle.setColour (juce::Label::textColourId, kBandColours[band]);
    for (int b = 0; b < kNumEQBands; ++b) bandButtons[(size_t) b].setToggleState (b == band, juce::dontSendNotification);
}

void EQPage::refresh()
{
    graph.repaint();
    const auto role = proc.getEffectiveRole();
    const bool show = role != Role::Vocal && role != Role::BackingVocal && role != Role::Master;
    const bool pro = License::get().isPro();
    vsOn.setButtonText (pro ? "Vocal space" : "Vocal space - PRO");
    vsOn.setEnabled (pro);
    vsAmount.setEnabled (pro);
    vsSpeed.setEnabled (pro);
    cleanHarsh.setButtonText (pro ? "CLEAN HARSH" : "CLEAN HARSH (PRO)");
    if (show != vsVisible)
    {
        vsVisible = show;
        for (auto* c : std::initializer_list<juce::Component*> { &vsOn, &vsAmount, &vsAmountL, &vsSpeed, &vsInfo }) c->setVisible (show);
    }
    const auto s = proc.getBandSettings (graph.getSelectedBand());
    const bool hasGain = ! (s.type == EQType::LowCut || s.type == EQType::HighCut || s.type == EQType::Notch);
    gain.setEnabled (hasGain);
}

void EQPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (r.getHeight() - 150);
    theme::drawPanel (g, r.removeFromLeft (r.getWidth() - 300).toFloat(), 8.0f);
    r.removeFromLeft (10);
    theme::drawPanel (g, r.toFloat(), 8.0f);
}

void EQPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto bottom = r.removeFromBottom (150);
    r.removeFromBottom (10);
    auto topRow = r.removeFromTop (28);
    eqOn.setBounds (topRow.removeFromLeft (70));
    int x = topRow.getX() + 80;
    for (auto& b : bandButtons) { b.setBounds (x, topRow.getY(), 28, 26); x += 32; }
    reset.setBounds (topRow.removeFromRight (100));
    topRow.removeFromRight (8);
    cleanHarsh.setBounds (topRow.removeFromRight (150));
    topRow.removeFromRight (8);
    cleanResult.setBounds (topRow.withTrimmedLeft (x - topRow.getX() + 10));
    r.removeFromTop (8);
    graph.setBounds (r);

    auto bandPanel = bottom.removeFromLeft (bottom.getWidth() - 300).reduced (12, 10);
    auto head = bandPanel.removeFromTop (24);
    bandTitle.setBounds (head.removeFromLeft (80));
    bandOn.setBounds (head.removeFromLeft (100));
    typeBox.setBounds (head.removeFromLeft (140).withHeight (24));
    bandPanel.removeFromTop (4);
    const int kw = juce::jmin (110, bandPanel.getWidth() / 3);
    auto knobRow = bandPanel;
    for (auto pair : { std::make_pair (&freq, &freqL), std::make_pair (&gain, &gainL), std::make_pair (&q, &qL) })
    {
        auto cell = knobRow.removeFromLeft (kw);
        pair.second->setBounds (cell.removeFromTop (14));
        pair.first->setBounds (cell);
    }

    bottom.removeFromLeft (10);
    auto vs = bottom.reduced (12, 10);
    auto vrow = vs.removeFromTop (24);
    vsOn.setBounds (vrow.removeFromLeft (160));
    vsSpeed.setBounds (vrow.removeFromRight (100).withHeight (24));
    vs.removeFromTop (6);
    auto arow = vs.removeFromTop (24);
    vsAmountL.setBounds (arow.removeFromLeft (60));
    vsAmount.setBounds (arow);
    vs.removeFromTop (4);
    vsInfo.setBounds (vs);
}
}
