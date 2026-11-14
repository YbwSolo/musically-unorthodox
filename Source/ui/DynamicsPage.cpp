#include "DynamicsPage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

// Scrolling picture of the last five seconds: level in grey, gain reduction in yellow hanging from the top,
// both on the same decibel scale, with a threshold line you can drag.
class CompScope : public juce::Component
{
public:
    static constexpr float kTop = 0.0f, kBottom = -54.0f;
    explicit CompScope (MUProcessor& p) : proc (p) { setHelp (*this, "comp.scope"); }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        const auto r = full.reduced (1.0f);
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());

        g.setFont (body (10.0f));
        for (int db = -48; db <= -6; db += 6)
        {
            const float y = dbToY (r, (float) db);
            g.setColour (db % 12 == 0 ? juce::Colour (0xff1e1e1e) : juce::Colour (0xff131313));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
            if (db % 12 == 0) { g.setColour (textMute); g.drawText (juce::String (db), juce::Rectangle<float> (r.getRight() - 30.0f, y - 13.0f, 26.0f, 12.0f), juce::Justification::centredRight); }
        }

        const auto& comp = proc.getCompressor();
        const int n = Compressor::kHistory;
        const float step = r.getWidth() / (float) (n - 1);
        juce::Path level, reduction;
        level.startNewSubPath (r.getX(), r.getBottom() + 2.0f);
        reduction.startNewSubPath (r.getX(), r.getY());
        for (int i = 0; i < n; ++i)
        {
            const int age = n - 1 - i;
            const float x = r.getX() + step * (float) i;
            level.lineTo (x, dbToY (r, juce::jmax (kBottom - 2.0f, comp.levelHistory().get (age))));
            reduction.lineTo (x, r.getY() + (-comp.reductionHistory().get (age)) / (kTop - kBottom) * r.getHeight());
        }
        level.lineTo (r.getRight(), r.getBottom() + 2.0f); level.closeSubPath();
        reduction.lineTo (r.getRight(), r.getY()); reduction.closeSubPath();
        g.setGradientFill (juce::ColourGradient (text.withAlpha (0.20f), 0.0f, r.getY(), text.withAlpha (0.05f), 0.0f, r.getBottom(), false));
        g.fillPath (level);
        g.setColour (text.withAlpha (0.35f));
        g.strokePath (level, juce::PathStrokeType (1.0f));
        g.setColour (yellow.withAlpha (0.28f));
        g.fillPath (reduction);
        g.setColour (yellow);
        g.strokePath (reduction, juce::PathStrokeType (1.4f));

        // threshold
        const bool on = proc.getParam (ids::compEnabled) > 0.5f;
        const float thr = proc.getParam (ids::compThreshold);
        const float ty = dbToY (r, thr);
        g.setColour (on ? yellow : textMute);
        for (float x = r.getX(); x < r.getRight(); x += 9.0f) g.fillRect (x, ty - 0.5f, 5.0f, 1.0f);
        const auto tag = juce::Rectangle<float> (r.getX() + 10.0f, ty - 20.0f < r.getY() + 4.0f ? ty + 4.0f : ty - 20.0f, 118.0f, 16.0f);
        g.setFont (bodyBold (11.0f));
        g.drawText ("Threshold " + juce::String (thr, 1) + " dB", tag, juce::Justification::centredLeft);
        g.restoreState();

        g.setFont (body (10.5f));
        g.setColour (textMute);
        g.drawText ("5 seconds", juce::Rectangle<float> (r.getX() + 10.0f, r.getBottom() - 18.0f, 80.0f, 12.0f), juce::Justification::centredLeft);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool near = std::abs ((float) e.y - dbToY (getLocalBounds().toFloat().reduced (1.0f), proc.getParam (ids::compThreshold))) < 8.0f;
        setMouseCursor (near ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (auto* p = proc.apvts.getParameter (ids::compThreshold)) p->beginChangeGesture();
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float db = juce::jlimit (-60.0f, 0.0f, kTop + ((float) e.y - r.getY()) / r.getHeight() * (kBottom - kTop));
        if (auto* p = proc.apvts.getParameter (ids::compThreshold)) p->setValueNotifyingHost (p->convertTo0to1 (db));
    }
    void mouseUp (const juce::MouseEvent&) override { if (auto* p = proc.apvts.getParameter (ids::compThreshold)) p->endChangeGesture(); }

private:
    static float dbToY (juce::Rectangle<float> r, float db) { return r.getY() + (kTop - db) / (kTop - kBottom) * r.getHeight(); }
    MUProcessor& proc;
};

// Input level against output level, with a dot showing where the signal is right now.
class TransferCurve : public juce::Component
{
public:
    explicit TransferCurve (MUProcessor& p) : proc (p) { setHelp (*this, "comp.curve"); }
    void tick (float dt)
    {
        const float in = proc.getCompressor().getInputDb();
        inDb = in > inDb ? in : approach (inDb, in, dt, 0.15f);
    }
    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        const auto r = full.reduced (10.0f);
        const float lo = -60.0f;
        auto toX = [&] (float db) { return r.getX() + (db - lo) / -lo * r.getWidth(); };
        auto toY = [&] (float db) { return r.getBottom() - (db - lo) / -lo * r.getHeight(); };
        g.setColour (juce::Colour (0xff1a1a1a));
        for (int db = -48; db < 0; db += 12) { g.fillRect (toX ((float) db), r.getY(), 1.0f, r.getHeight()); g.fillRect (r.getX(), toY ((float) db), r.getWidth(), 1.0f); }
        g.setColour (lineHi);
        g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);

        const float thr = proc.getParam (ids::compThreshold), ratio = proc.getParam (ids::compRatio);
        const float knee = ((int) proc.getParam (ids::compStyle) == (int) CompStyle::Smooth || (int) proc.getParam (ids::compStyle) == (int) CompStyle::Vocal) ? juce::jmax (8.0f, proc.getParam (ids::compKnee)) : proc.getParam (ids::compKnee);
        const bool on = proc.getParam (ids::compEnabled) > 0.5f;
        juce::Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float in = lo + (float) i * 0.5f;
            const float out = in + (on ? Compressor::computeGainDb (in, thr, ratio, knee) : 0.0f);
            if (i == 0) p.startNewSubPath (toX (in), toY (out)); else p.lineTo (toX (in), toY (out));
        }
        g.setColour (on ? yellow : textMute);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (yellow.withAlpha (0.35f));
        g.fillRect (toX (thr), r.getY(), 1.0f, r.getHeight());

        if (inDb > lo)
        {
            const float out = inDb + (on ? Compressor::computeGainDb (inDb, thr, ratio, knee) : 0.0f);
            g.setColour (juce::Colours::black);
            g.fillEllipse (toX (inDb) - 6.0f, toY (out) - 6.0f, 12.0f, 12.0f);
            g.setColour (text);
            g.fillEllipse (toX (inDb) - 4.5f, toY (out) - 4.5f, 9.0f, 9.0f);
        }
        g.setFont (body (10.0f));
        g.setColour (textMute);
        g.drawText ("In", juce::Rectangle<float> (r.getRight() - 30.0f, r.getBottom() - 14.0f, 28.0f, 12.0f), juce::Justification::centredRight);
        g.drawText ("Out", juce::Rectangle<float> (r.getX() + 2.0f, r.getY(), 30.0f, 12.0f), juce::Justification::centredLeft);
    }
private:
    MUProcessor& proc;
    float inDb = -100.0f;
};

static const char* styleBlurb (int s)
{
    switch (s)
    {
        case 1:  return "Punch: fast peak detector that snaps back quickly. Kicks, snares and anything that should hit. It adds no distortion of its own.";
        case 4:  return "Vocal: two compressors in one. A slow leveller evens out the phrases and a fast catcher behind it takes only the peaks, so neither works hard and the voice stays open. Rap and sung leads.";
        case 2:  return "Smooth: listens to the average level and releases slowly when it has been working hard. Sung vocals, bass, keys, anything that should feel even.";
        case 3:  return "Glue: gentle and slow, with a release that follows the music. Drum groups, the beat and the master, where everything should move together.";
        default: return "Clean: follows peaks and average level together and adds no colour. Use it when you want control and nobody should hear a compressor.";
    }
}

DynamicsPage::DynamicsPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      style (p, ids::compStyle, { "Clean", "Punch", "Smooth", "Glue", "Vocal" }),
      thr (p, ids::compThreshold, "Threshold"), ratio (p, ids::compRatio, "Ratio"), attack (p, ids::compAttack, "Attack"), release (p, ids::compRelease, "Release"),
      knee (p, ids::compKnee, "Knee"), makeup (p, ids::compMakeup, "Makeup"), mix (p, ids::compMix, "Mix"), scHp (p, ids::compScHp, "Sidechain HP"),
      autoMakeup (p, ids::compAutoMakeup, "Auto makeup"),
      gThr (p, ids::gateThreshold, "Threshold"), gRange (p, ids::gateRange, "Range"), gAttack (p, ids::gateAttack, "Attack"), gRelease (p, ids::gateRelease, "Release"), gHold (p, ids::gateHold, "Hold"),
      dFreq (p, ids::dsFreq, "Frequency"), dThr (p, ids::dsThreshold, "Threshold"), dRange (p, ids::dsRange, "Range"),
      dMode (p, ids::dsMode, { "Split band", "Wide band" })
{
    useTabPresets (presets, proc, "dynamics", setStatus);
    addAndMakeVisible (presets);
    scope = std::make_unique<CompScope> (proc);
    curve = std::make_unique<TransferCurve> (proc);
    compSection.setPower (proc, ids::compEnabled);
    gateSection.setPower (proc, ids::gateOn);
    deEssSection.setPower (proc, ids::dsOn);
    for (auto* c : std::initializer_list<juce::Component*> { &compSection, &gateSection, &deEssSection, scope.get(), curve.get(), &style, &thr, &ratio, &attack, &release, &knee, &makeup, &mix, &scHp,
                                                              &autoMakeup, &autoSet, &gThr, &gRange, &gAttack, &gRelease, &gHold, &dFreq, &dThr, &dRange, &dMode })
        addAndMakeVisible (c);
    styleButton (autoSet, "Auto set", true, "comp.auto");
    autoSet.onClick = [this] { setStatus (proc.autoCompressor()); };
}

DynamicsPage::~DynamicsPage() = default;

void DynamicsPage::tick (float dt)
{
    presets.refresh();
    curve->tick (dt);
    scope->repaint();
    curve->repaint();
    const float cg = proc.getCompressor().getGainReductionDb();
    compSection.setReadout (cg < -0.05f ? juce::String (cg, 1) + " dB" : juce::String());
    const float gg = proc.getGate().getGainReductionDb(), dg = proc.getDeEsser().getGainReductionDb();
    gateGr = gg < gateGr ? gg : approach (gateGr, gg, dt, 0.1f);
    deEssGr = dg < deEssGr ? dg : approach (deEssGr, dg, dt, 0.1f);
    repaint (gateMeter.expanded (2)); repaint (deEssMeter.expanded (2));

    slow -= dt;
    if (slow <= 0.0f)
    {
        slow = 0.2f;
        makeup.setDimmed (proc.getParam (ids::compAutoMakeup) > 0.5f);
        const int s = (int) proc.getParam (ids::compStyle);
        if (s != shownStyle) { shownStyle = s; repaint (styleTextArea); }
        gateSection.setReadout (gateGr < -0.1f ? juce::String (gateGr, 1) + " dB" : juce::String());
        deEssSection.setReadout (deEssGr < -0.1f ? juce::String (deEssGr, 1) + " dB" : juce::String());
    }
}

void DynamicsPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
}

void DynamicsPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    presets.setBounds (r.removeFromTop (26).removeFromRight (270));
    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (168);
    r.removeFromBottom (10);
    compSection.setBounds (r);

    auto c = compSection.getBounds().withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto controls = c.removeFromBottom (92);
    c.removeFromBottom (10);
    auto styleRow = c.removeFromBottom (30);
    c.removeFromBottom (10);
    curve->setBounds (c.removeFromRight (c.getHeight()));
    c.removeFromRight (10);
    scope->setBounds (c);

    style.setBounds (styleRow.removeFromLeft (300));
    styleRow.removeFromLeft (14);
    autoSet.setBounds (styleRow.removeFromRight (96));
    styleRow.removeFromRight (12);
    autoMakeup.setBounds (styleRow.removeFromRight (124));
    styleTextArea = styleRow;
    layoutRow (controls, { &thr, &ratio, &attack, &release, &knee, &makeup, &mix, &scHp }, 6);

    auto gate = bottom.removeFromLeft ((bottom.getWidth() - 10) * 56 / 100);
    bottom.removeFromLeft (10);
    gateSection.setBounds (gate);
    deEssSection.setBounds (bottom);

    auto gc = gate.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    gateMeter = gc.removeFromRight (8).reduced (0, 6);
    gc.removeFromRight (12);
    layoutRow (gc.withSizeKeepingCentre (gc.getWidth(), juce::jmin (gc.getHeight(), 96)), { &gThr, &gRange, &gAttack, &gRelease, &gHold }, 6);

    auto dc = bottom.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    deEssMeter = dc.removeFromRight (8).reduced (0, 6);
    dc.removeFromRight (12);
    auto modeArea = dc.removeFromRight (176);
    dMode.setBounds (modeArea.withSizeKeepingCentre (176, 28));
    dc.removeFromRight (10);
    layoutRow (dc.withSizeKeepingCentre (dc.getWidth(), juce::jmin (dc.getHeight(), 96)), { &dFreq, &dThr, &dRange }, 6);
}

// drawn on top of the section panels: the reduction meters and the description of the chosen style
void DynamicsPage::paintOverChildren (juce::Graphics& g)
{
    drawReductionBar (g, gateMeter.toFloat(), gateGr, 30.0f);
    drawReductionBar (g, deEssMeter.toFloat(), deEssGr, 12.0f);
    drawWrapped (g, styleBlurb ((int) proc.getParam (ids::compStyle)), styleTextArea.toFloat().withTrimmedTop (1.0f), body (11.0f), textDim, 1.25f);
}
}
