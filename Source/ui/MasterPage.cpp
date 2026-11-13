#include "MasterPage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

namespace
{
    struct Mode { const char* name; const char* level; const char* peak; const char* use; };
    const Mode kModes[4] = {
        { "Mix hand-off",     "-18 LUFS", "-6 dBTP peak", "Leaves headroom for a mastering engineer" },
        { "Streaming master", "-14 LUFS", "-1 dBTP peak", "Ready for Spotify, Apple Music and YouTube" },
        { "Loud master",      "-9 LUFS",  "-2 dBTP peak", "Club, drill and trap releases that must compete" },
        { "Custom",           "Your own", "target and ceiling", "Set the knobs on the right yourself" } };
}

MasterPage::MasterPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      drive (p, ids::maxGain, "Drive"), ceiling (p, ids::targetTruePeak, "Ceiling"), target (p, ids::targetLufs, "Target"),
      autoLevel (p, ids::masterAutoTrim, "Auto level")
{
    limiter.setPower (p, ids::peakGuard);
    for (auto* c : std::initializer_list<juce::Component*> { &loudness, &limiter, &checks, &drive, &ceiling, &target, &autoLevel, &resetMeasure, &makeMaster }) addAndMakeVisible (c);
    styleButton (resetMeasure, "Measure again", false, "master.history");
    resetMeasure.onClick = [this] { proc.resetMeters(); setStatus ("Measurement restarted. Play the loudest section of the song."); };
    styleButton (makeMaster, "Make this the master", true, "role");
    makeMaster.onClick = [this] { proc.setParam (ids::role, (float) Role::Master); proc.applyMasterMode ((int) proc.getParam (ids::masterMode)); };
    setHelp (*this, "masterMode");
    setHelp (checks, "master.check");
    setHelp (loudness, "master.history");
    // moving a target by hand means the mode is no longer one of the presets
    for (auto* k : { &ceiling, &target })
        k->slider.onDragStart = [this] { proc.setParam (ids::masterMode, 3.0f); };
}

int MasterPage::cardAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i) if (cards[(size_t) i].contains (p)) return i;
    return -1;
}

void MasterPage::mouseDown (const juce::MouseEvent& e)
{
    const int c = cardAt (e.getPosition());
    if (c < 0 || ! isMaster) return;
    proc.applyMasterMode (c);
    setStatus (c == 3 ? juce::String ("Custom: set the target and ceiling yourself.")
                      : juce::String (kModes[c].name) + ": aiming for " + kModes[c].level + " under a " + juce::String (kModes[c].peak).upToFirstOccurrenceOf (" peak", false, false) + " ceiling. Play the loudest section and let the level settle.");
}

void MasterPage::mouseMove (const juce::MouseEvent& e)
{
    hoverCard = isMaster ? cardAt (e.getPosition()) : -1;
    setMouseCursor (hoverCard >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void MasterPage::mouseExit (const juce::MouseEvent&) { hoverCard = -1; }

void MasterPage::updateChecks()
{
    const auto& l = proc.getLoudness();
    const float tgt = proc.getParam (ids::targetLufs), ceil = proc.getParam (ids::targetTruePeak);
    const float integ = l.getIntegrated(), tp = l.getTruePeakHold();
    const bool measured = isMaster && integ > -60.0f;

    auto& a = items[0];
    a.title = "Loudness";
    if (! measured) { a.state = 0; a.detail = "Play the loudest section to measure"; }
    else
    {
        const float d = integ - tgt;
        a.state = std::abs (d) <= 1.0f ? 1 : 2;
        a.detail = juce::String (integ, 1) + " LUFS, " + (std::abs (d) <= 1.0f ? juce::String ("on target") : juce::String (std::abs (d), 1) + (d < 0.0f ? " LU under target" : " LU over target"));
    }
    auto& b = items[1];
    b.title = "True peak";
    if (! measured) { b.state = 0; b.detail = "Not measured yet"; }
    else { b.state = tp <= ceil + 0.1f ? 1 : 2; b.detail = juce::String (tp, 1) + " dBTP, " + (tp <= ceil + 0.1f ? "under the ceiling" : "over the ceiling: switch the limiter on"); }

    auto& c = items[2];
    c.title = "Limiter";
    float worst = 0.0f;
    for (int i = 0; i < 500; ++i) worst = juce::jmin (worst, proc.getMaximizer().reductionHistory().get (i));
    if (! measured) { c.state = 0; c.detail = "Not measured yet"; }
    else if (proc.getParam (ids::peakGuard) < 0.5f) { c.state = tp <= ceil + 0.1f ? 1 : 2; c.detail = "Switched off"; }
    else if (proc.isAutoLevelHeld()) { c.state = 2; c.detail = "Holding at " + juce::String (-worst, 1) + " dB. The mix is too peaky to go louder cleanly: tame the loudest hits or lower the target"; }
    else { c.state = worst > -4.0f ? 1 : 2; c.detail = worst > -0.1f ? juce::String ("Not needed right now") : juce::String (-worst, 1) + " dB at most, " + (worst > -4.0f ? "working lightly" : "working hard: the punch will suffer"); }

    auto& d = items[3];
    d.title = "Punch";
    const float plr = tp - integ;
    if (! measured) { d.state = 0; d.detail = "Not measured yet"; }
    else { d.state = plr >= 7.0f ? 1 : 2; d.detail = juce::String (plr, 1) + " dB between loudness and peaks, " + (plr >= 9.0f ? "plenty of punch" : plr >= 7.0f ? "tight but alive" : "squashed: ease off the loudness"); }

    auto& e = items[4];
    e.title = "Mono";
    const float corr = proc.getStereo().getCorrelation();
    if (! measured) { e.state = 0; e.detail = "Not measured yet"; }
    else { e.state = corr > 0.1f ? 1 : 2; e.detail = corr > 0.5f ? "Solid in mono" : corr > 0.1f ? "Wide, still fine in mono" : "Parts will vanish in mono: narrow the width"; }
}

void MasterPage::tick (float dt)
{
    const int mode = (int) proc.getParam (ids::masterMode);
    for (int i = 0; i < 4; ++i)
    {
        const float goal = (isMaster && i == mode) ? 1.0f : (i == hoverCard ? 0.25f : 0.0f);
        if (std::abs (cardGlow[(size_t) i] - goal) > 0.004f) { cardGlow[(size_t) i] = approach (cardGlow[(size_t) i], goal, dt, 0.06f); repaint (cards[(size_t) i]); }
    }
    repaint (historyWell); repaint (grWell); repaint (readoutArea);

    slow -= dt;
    if (slow <= 0.0f)
    {
        slow = 0.25f;
        const bool m = proc.isMasterInstance();
        if (m != isMaster) { isMaster = m; resized(); repaint(); }
        integrated = proc.getLoudness().getIntegrated();
        updateChecks();
        repaint (checkArea);
        const float gr = proc.getMaximizer().getGainReductionDb();
        limiter.setReadout (gr < -0.05f ? juce::String (gr, 1) + " dB" : juce::String());
        drive.setDimmed (! isMaster); ceiling.setDimmed (! isMaster); target.setDimmed (! isMaster);
    }
}

void MasterPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    for (int i = 0; i < 4; ++i)
    {
        const auto r = cards[(size_t) i].toFloat();
        const float glow = cardGlow[(size_t) i];
        g.setColour (panel.interpolatedWith (yellow, glow));
        g.fillRoundedRectangle (r, 6.0f);
        const auto main = text.interpolatedWith (onYellow, glow), sub = textDim.interpolatedWith (onYellow, glow);
        auto in = r.reduced (16.0f, 12.0f);
        g.setFont (bodyBold (12.5f));
        g.setColour (isMaster ? main : textMute);
        g.drawText (kModes[i].name, in.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setFont (headline (30.0f));
        auto big = in.removeFromTop (36.0f);
        g.drawText (kModes[i].level, big, juce::Justification::centredLeft);
        g.setFont (body (11.5f));
        g.setColour (isMaster ? sub : textMute);
        const float lw = headline (30.0f).getStringWidthFloat (kModes[i].level) + 10.0f;
        g.drawText (kModes[i].peak, big.withTrimmedLeft (lw).withTrimmedTop (12.0f), juce::Justification::centredLeft);
        g.drawFittedText (kModes[i].use, in.toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
    }
}

void MasterPage::paintOverChildren (juce::Graphics& g)
{
    const float tgt = proc.getParam (ids::targetLufs);

    // ---- loudness history: the last minute against the target
    {
        const auto full = historyWell.toFloat();
        drawWell (g, full);
        const auto r = full.reduced (1.0f);
        const float top = juce::jmin (-4.0f, tgt + 8.0f), bottom = top - 30.0f;
        auto toY = [&] (float lufs) { return r.getY() + (top - lufs) / (top - bottom) * r.getHeight(); };
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        g.setFont (body (10.0f));
        for (int v = (int) std::ceil (bottom / 6.0f) * 6; v < (int) top; v += 6)
        {
            g.setColour (juce::Colour (0xff181818)); g.fillRect (r.getX(), toY ((float) v), r.getWidth(), 1.0f);
            g.setColour (textMute); g.drawText (juce::String (v), juce::Rectangle<float> (r.getRight() - 30.0f, toY ((float) v) - 13.0f, 26.0f, 12.0f), juce::Justification::centredRight);
        }
        g.setColour (yellow.withAlpha (0.08f));
        g.fillRect (r.getX(), toY (tgt + 1.0f), r.getWidth(), toY (tgt - 1.0f) - toY (tgt + 1.0f));
        const auto& h = proc.getLoudnessHistory();
        const int n = MUProcessor::kLoudnessHistory;
        juce::Path line, fill;
        bool started = false;
        for (int i = 0; i < n; ++i)
        {
            const float v = h.get (n - 1 - i);
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = toY (juce::jmax (bottom - 2.0f, v));
            if (! started) { line.startNewSubPath (x, y); started = true; } else line.lineTo (x, y);
        }
        fill = line; fill.lineTo (r.getRight(), r.getBottom() + 2.0f); fill.lineTo (r.getX(), r.getBottom() + 2.0f); fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (text.withAlpha (0.16f), 0.0f, r.getY(), text.withAlpha (0.03f), 0.0f, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (text.withAlpha (0.8f));
        g.strokePath (line, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
        g.setColour (yellow);
        for (float x = r.getX(); x < r.getRight(); x += 9.0f) g.fillRect (x, toY (tgt) - 0.5f, 5.0f, 1.0f);
        g.setFont (bodyBold (11.0f));
        g.drawText ("Target " + juce::String (tgt, 1) + " LUFS", juce::Rectangle<float> (r.getX() + 10.0f, toY (tgt) - 18.0f, 150.0f, 14.0f), juce::Justification::centredLeft);
        g.restoreState();
        g.setFont (body (10.5f)); g.setColour (textMute);
        g.drawText ("Last 60 seconds", juce::Rectangle<float> (r.getX() + 10.0f, r.getBottom() - 18.0f, 120.0f, 12.0f), juce::Justification::centredLeft);
    }

    // ---- big readouts
    {
        auto r = readoutArea;
        auto cell = [&] (const juce::String& value, const juce::String& label, bool highlight, bool alarm)
        {
            auto c = r.removeFromTop (64);
            g.setFont (headline (40.0f));
            g.setColour (alarm ? over : highlight ? yellow : text);
            g.drawText (value, c.removeFromTop (44), juce::Justification::centredLeft);
            g.setFont (body (11.0f));
            g.setColour (textMute);
            g.drawText (label, c, juce::Justification::centredLeft);
            r.removeFromTop (6);
        };
        const auto& l = proc.getLoudness();
        const float tp = l.getTruePeakHold();
        cell (integrated > -70.0f ? juce::String (integrated, 1) : juce::String ("--"), "LUFS integrated", integrated > -70.0f && std::abs (integrated - tgt) <= 1.0f, false);
        cell (l.getShortTerm() > -70.0f ? juce::String (l.getShortTerm(), 1) : juce::String ("--"), "LUFS short-term", false, false);
        cell (tp > -70.0f ? juce::String (tp, 1) : juce::String ("--"), "dB true peak", false, tp > proc.getParam (ids::targetTruePeak) + 0.1f);
        if (isMaster && proc.getParam (ids::masterAutoTrim) > 0.5f)
        {
            g.setFont (body (11.5f)); g.setColour (textDim);
            g.drawText ("Auto level " + dbString (proc.getMasterAutoTrimDb()), r.removeFromTop (16), juce::Justification::centredLeft);
        }
    }

    // ---- limiter reduction over the last five seconds
    {
        const auto full = grWell.toFloat();
        drawWell (g, full);
        const auto r = full.reduced (1.0f);
        const float maxDb = 12.0f;
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        g.setFont (body (10.0f));
        for (int v = 3; v < 12; v += 3)
        {
            const float y = r.getY() + (float) v / maxDb * r.getHeight();
            g.setColour (juce::Colour (0xff181818)); g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
            g.setColour (textMute); g.drawText ("-" + juce::String (v), juce::Rectangle<float> (r.getRight() - 26.0f, y - 13.0f, 22.0f, 12.0f), juce::Justification::centredRight);
        }
        const auto& h = proc.getMaximizer().reductionHistory();
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY());
        for (int i = 0; i < 512; ++i)
            p.lineTo (r.getX() + r.getWidth() * (float) i / 511.0f, r.getY() + juce::jlimit (0.0f, 1.0f, -h.get (511 - i) / maxDb) * r.getHeight());
        p.lineTo (r.getRight(), r.getY()); p.closeSubPath();
        g.setColour (yellow.withAlpha (0.28f)); g.fillPath (p);
        g.setColour (yellow); g.strokePath (p, juce::PathStrokeType (1.3f));
        g.restoreState();
    }

    // ---- release check
    {
        auto r = checkArea;
        const int w = r.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
        {
            auto c = r.removeFromLeft (w).reduced (4, 0);
            const auto& it = items[(size_t) i];
            const auto dot = juce::Rectangle<float> ((float) c.getX(), (float) c.getY() + 3.0f, 14.0f, 14.0f);
            if (it.state == 1)
            {
                g.setColour (yellow); g.fillEllipse (dot);
                juce::Path tick; tick.startNewSubPath (dot.getX() + 3.5f, dot.getCentreY()); tick.lineTo (dot.getX() + 6.0f, dot.getBottom() - 4.0f); tick.lineTo (dot.getRight() - 3.5f, dot.getY() + 4.5f);
                g.setColour (onYellow); g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else if (it.state == 2) { g.setColour (over); g.fillEllipse (dot); g.setColour (text); g.fillRect (dot.getCentreX() - 0.8f, dot.getY() + 3.0f, 1.6f, 5.0f); g.fillRect (dot.getCentreX() - 0.8f, dot.getBottom() - 4.5f, 1.6f, 1.6f); }
            else { g.setColour (lineHi); g.drawEllipse (dot.reduced (1.0f), 1.5f); }
            auto textArea = c.withTrimmedLeft (22);
            g.setFont (bodyBold (12.5f));
            g.setColour (it.state == 0 ? textDim : text);
            g.drawText (it.title, textArea.removeFromTop (20), juce::Justification::centredLeft);
            drawWrapped (g, it.detail, textArea.toFloat(), body (11.5f), textDim, 1.3f);
        }
    }

    if (! isMaster)
    {
        fillPanel (g, bannerArea.toFloat(), raised);
        g.setColour (yellow);
        g.fillRect (bannerArea.withWidth (3));
        auto b = bannerArea.reduced (16, 0);
        b.removeFromRight (190);
        g.setFont (bodyBold (12.5f)); g.setColour (text);
        g.drawText ("This instance is on a track, not the master", b.removeFromTop (bannerArea.getHeight() / 2).withTrimmedTop (8), juce::Justification::centredLeft);
        g.setFont (body (12.0f)); g.setColour (textDim);
        g.drawText ("Mastering runs on the instance whose role is Master. Put the plugin last on the Stereo Out and open it there, or switch this one over.", b.withTrimmedBottom (8), juce::Justification::centredLeft, true);
    }
}

void MasterPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    if (! isMaster) { bannerArea = r.removeFromTop (52); r.removeFromTop (10); makeMaster.setBounds (bannerArea.withTrimmedLeft (bannerArea.getWidth() - 186).reduced (0, 12).withTrimmedRight (14)); }
    makeMaster.setVisible (! isMaster);

    auto cardRow = r.removeFromTop (isMaster ? 112 : 104);
    r.removeFromTop (10);
    const int cw = (cardRow.getWidth() - 30) / 4;
    for (int i = 0; i < 4; ++i) { cards[(size_t) i] = cardRow.removeFromLeft (cw); cardRow.removeFromLeft (10); }

    auto checkRow = r.removeFromBottom (112);
    r.removeFromBottom (10);
    checks.setBounds (checkRow);
    checkArea = checkRow.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (10);

    auto limArea = r.removeFromRight (340);
    r.removeFromRight (10);
    loudness.setBounds (r);
    limiter.setBounds (limArea);

    auto l = r.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    readoutArea = l.removeFromLeft (150);
    l.removeFromLeft (8);
    historyWell = l;
    resetMeasure.setBounds (loudness.getBounds().getRight() - 122, loudness.getBounds().getY() + 6, 110, 22);

    auto m = limArea.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto knobs = m.removeFromBottom (92);
    m.removeFromBottom (8);
    autoLevel.setBounds (m.removeFromBottom (22));
    m.removeFromBottom (8);
    grWell = m;
    layoutRow (knobs, { &drive, &ceiling, &target }, 6);
}
}
