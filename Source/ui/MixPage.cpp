#include "MixPage.h"
#include "Theme.h"
#include "../license/License.h"

namespace mu
{
// ---- SessionTable --------------------------------------------------------------------------------
void SessionTable::update (std::vector<TrackInfo> tracks, const BalancePlan& plan, int ownSlot)
{
    rows.clear();
    std::sort (tracks.begin(), tracks.end(), [] (const TrackInfo& a, const TrackInfo& b)
    {
        if ((a.role == Role::Master) != (b.role == Role::Master)) return a.role == Role::Master;
        return rolePriority (a.role) > rolePriority (b.role);
    });
    for (auto& t : tracks)
    {
        Row r; r.t = t; r.own = t.slot == ownSlot;
        for (auto& m : plan.moves) if (m.slot == t.slot) { r.delta = m.deltaDb; r.note = m.note; }
        if (t.role == Role::Master) r.note = "master";
        rows.push_back (r);
    }
    repaint();
}

void SessionTable::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    theme::drawPanel (g, r.toFloat(), 8.0f);
    r.reduce (10, 6);
    auto header = r.removeFromTop (20);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (11.0f));
    const int wName = juce::jmax (120, r.getWidth() - 400);
    auto cols = [&] (juce::Rectangle<int> row)
    {
        struct C { juce::Rectangle<int> dot, name, role, level, peak, trim, delta; } c;
        c.dot = row.removeFromLeft (16);
        c.name = row.removeFromLeft (wName);
        c.role = row.removeFromLeft (90);
        c.level = row.removeFromLeft (70);
        c.peak = row.removeFromLeft (70);
        c.trim = row.removeFromLeft (70);
        c.delta = row;
        return c;
    };
    auto h = cols (header);
    g.drawText ("TRACK", h.name, juce::Justification::centredLeft);
    g.drawText ("ROLE", h.role, juce::Justification::centredLeft);
    g.drawText ("LUFS", h.level, juce::Justification::centredRight);
    g.drawText ("PEAK", h.peak, juce::Justification::centredRight);
    g.drawText ("TRIM", h.trim, juce::Justification::centredRight);
    g.drawText ("NEXT MOVE", h.delta, juce::Justification::centredRight);
    g.setColour (theme::line);
    g.fillRect (r.removeFromTop (1));

    if (rows.empty())
    {
        g.setColour (theme::textMute);
        g.setFont (theme::labelFont (13.0f));
        g.drawText ("No other instances found. Add Musically Unorthodox to each track you want balanced.", r.removeFromTop (40), juce::Justification::centred);
        return;
    }

    for (auto& row : rows)
    {
        auto line = r.removeFromTop (24);
        if (row.own)
        {
            g.setColour (theme::accent.withAlpha (0.12f));
            g.fillRoundedRectangle (line.toFloat(), 4.0f);
        }
        auto c = cols (line);
        g.setColour (MUProcessor::roleColour (row.t.role));
        g.fillEllipse (c.dot.withSizeKeepingCentre (8, 8).toFloat());
        g.setFont (theme::labelFont (13.0f));
        g.setColour (row.t.active ? theme::text : theme::textMute);
        g.drawText (row.t.name + (row.own ? "  (this)" : ""), c.name, juce::Justification::centredLeft, true);
        g.setColour (theme::textDim);
        g.drawText (roleNames()[(int) row.t.role], c.role, juce::Justification::centredLeft, true);
        g.setFont (theme::monoFont (12.0f));
        g.setColour (row.t.active ? theme::text : theme::textMute);
        g.drawText (row.t.levelLufs > -90.0f ? juce::String (row.t.levelLufs, 1) : "-", c.level, juce::Justification::centredRight);
        g.setColour (row.t.truePeakDb > 0.0f ? theme::bad : row.t.truePeakDb > -3.0f ? theme::warn : theme::textDim);
        g.drawText (row.t.truePeakDb > -90.0f ? juce::String (row.t.truePeakDb, 1) : "-", c.peak, juce::Justification::centredRight);
        g.setColour (theme::textDim);
        g.drawText (row.t.role == Role::Master ? "" : theme::dbString (row.t.trimDb, 1, ""), c.trim, juce::Justification::centredRight);
        if (row.t.role == Role::Master)
            g.setColour (theme::textMute);
        else
            g.setColour (std::abs (row.delta) < 0.1f ? theme::good : std::abs (row.delta) > 6.0f ? theme::warn : theme::accentHi);
        g.drawText (row.t.role == Role::Master ? "master" : (std::abs (row.delta) < 0.1f ? "ok" : theme::dbString (row.delta, 1)) , c.delta, juce::Justification::centredRight);
    }
}

// ---- MixPage -------------------------------------------------------------------------------------
MixPage::MixPage (MUProcessor& p) : proc (p)
{
    addAndMakeVisible (table);
    for (auto* r : { &masterNow, &masterPeak, &projected, &ownLevel }) addAndMakeVisible (r);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.setFont (theme::labelFont (12.0f));
        l.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (l);
    };
    setupSlider (targetLufs, targetLufsLabel, "Target loudness before mastering");
    setupSlider (targetTp, targetTpLabel, "True peak ceiling");
    targetLufsAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::targetLufs, targetLufs);
    targetTpAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::targetTruePeak, targetTp);
    targetLufs.setTooltip ("-18 LUFS integrated with -6 dBTP peaks is the usual hand-off level. Trap and drill mixes often run hotter (-14 to -12).");

    balanceNow.setTooltip ("Analyse every instance and move each track's trim so the whole session sits at the target.");
    balanceNow.onClick = [this] { proc.balanceNow(); refresh(); };
    addAndMakeVisible (balanceNow);

    autoBalance.setTooltip ("Pro: keep re-balancing while the song plays, gliding gently towards the plan.");
    autoBalance.onClick = [this]
    {
        if (! License::get().isPro()) { autoBalance.setToggleState (false, juce::dontSendNotification); return; }
        proc.setAutoBalanceMode (autoBalance.getToggleState());
    };
    addAndMakeVisible (autoBalance);

    makeMaster.onClick = [this]
    {
        if (auto* param = proc.apvts.getParameter (ids::role))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) Role::Master));
            param->endChangeGesture();
        }
        refresh();
    };
    addAndMakeVisible (makeMaster);

    resetTrims.onClick = [this] { proc.resetTrim(); };
    addAndMakeVisible (resetTrims);

    addAndMakeVisible (acceptMoves);
    acceptAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::autoBalance, acceptMoves);

    for (auto* l : { &summary, &maskingTitle, &hint })
    {
        l->setFont (theme::labelFont (12.0f));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (l);
    }
    maskingTitle.setFont (theme::titleFont (12.0f));
    maskingTitle.setColour (juce::Label::textColourId, theme::text);
    summary.setMinimumHorizontalScale (1.0f);
    hint.setText ("Put one instance on your master bus and set its role to Master. That instance runs the session balance; every other instance just needs the right role.", juce::dontSendNotification);
    refresh();
}

void MixPage::rebuildMaskingButtons()
{
    maskingButtons.clear();
    const bool pro = License::get().isPro();
    for (size_t i = 0; i < clashes.size(); ++i)
    {
        auto& c = clashes[i];
        auto* b = maskingButtons.add (new juce::TextButton());
        b->setButtonText ((pro ? "FIX  " : "PRO  ") + c.nameB + " sits on " + c.nameA + " around "
                          + (c.freqHz >= 1000.0f ? juce::String (c.freqHz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (c.freqHz)) + " Hz")
                          + "  (" + juce::String (c.suggestedCutDb, 1) + " dB)");
        b->setEnabled (pro);
        b->onClick = [this, i]
        {
            if (i < clashes.size()) proc.getBus().applyMaskingClash (clashes[i]);
        };
        addAndMakeVisible (b);
    }
    resized();
}

void MixPage::refresh()
{
    const bool isMaster = proc.isMasterInstance();
    auto& bus = proc.getBus();
    const auto plan = bus.getLastPlan();
    table.update (bus.collectTracks(), plan, proc.getSessionSlot());

    const auto& l = proc.getLoudness();
    auto colourFor = [] (float v, float target, float tol) { return std::abs (v - target) <= tol ? theme::good : std::abs (v - target) <= tol * 2.5f ? theme::warn : theme::bad; };
    const float tLufs = proc.apvts.getRawParameterValue (ids::targetLufs)->load();
    const float tTp = proc.apvts.getRawParameterValue (ids::targetTruePeak)->load();

    if (isMaster)
    {
        const float integ = l.getIntegrated();
        masterNow.set (integ > -90.0f ? juce::String (integ, 1) + " LUFS" : "--", "master integrated", integ > -90.0f ? colourFor (integ, tLufs, 1.0f) : theme::textDim);
        const float tp = l.getTruePeakHold();
        masterPeak.set (tp > -90.0f ? juce::String (tp, 1) + " dBTP" : "--", "master true peak", tp > -90.0f ? (tp <= tTp ? theme::good : tp <= tTp + 2.0f ? theme::warn : theme::bad) : theme::textDim);
        projected.set (plan.usable ? juce::String (plan.projectedMasterLufs, 1) + " LUFS" : "--", "after balance", plan.usable ? colourFor (plan.projectedMasterLufs, tLufs, 1.0f) : theme::textDim);
        summary.setText (plan.summary, juce::dontSendNotification);
        autoBalance.setToggleState (proc.getAutoBalanceMode() && License::get().isPro(), juce::dontSendNotification);
        autoBalance.setButtonText (License::get().isPro() ? "Auto balance (keeps following the mix)" : "Auto balance - PRO");
        if (lastClashCount != (int) clashes.size() || ! lastWasMaster)
        {
            clashes = bus.computeMasking();
            rebuildMaskingButtons();
            lastClashCount = (int) clashes.size();
        }
        else
        {
            auto fresh = bus.computeMasking();
            if (fresh.size() != clashes.size()) { clashes = fresh; rebuildMaskingButtons(); lastClashCount = (int) clashes.size(); }
            else clashes = fresh;
        }
        maskingTitle.setText (clashes.empty() ? "Masking: nothing fighting for the same space right now" : "Masking: tracks sitting on top of each other", juce::dontSendNotification);
    }
    else
    {
        const float lv = l.getIntegrated() > -90.0f ? l.getIntegrated() : l.getShortTerm();
        ownLevel.set (lv > -90.0f ? juce::String (lv, 1) + " LUFS" : "--", "this track", theme::text);
        const float trim = proc.getTrimDb();
        masterNow.set (theme::dbString (trim), "balance trim applied", std::abs (trim) < 0.05f ? theme::textDim : theme::accentHi);
        float mine = 0.0f; juce::String note = "waiting for the master";
        for (auto& m : plan.moves) if (m.slot == proc.getSessionSlot()) { mine = m.deltaDb; note = m.note; }
        projected.set (plan.usable ? theme::dbString (mine) : "--", "next move (" + note + ")", std::abs (mine) < 0.1f ? theme::good : theme::accentHi);
        const int masterSlot = bus.getMasterSlot();
        summary.setText (masterSlot < 0 ? "No master instance in the session yet." : plan.summary, juce::dontSendNotification);
        maskingTitle.setText ("", juce::dontSendNotification);
        if (! maskingButtons.isEmpty()) { maskingButtons.clear(); lastClashCount = -1; }
    }

    if (lastWasMaster != isMaster) { lastWasMaster = isMaster; resized(); }
    makeMaster.setVisible (! isMaster && bus.getMasterSlot() < 0);
}

void MixPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MixPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    const bool isMaster = proc.isMasterInstance();

    auto top = r.removeFromTop (70);
    const int cardW = (top.getWidth() - 20) / 3;
    if (isMaster)
    {
        ownLevel.setVisible (false);
        masterNow.setBounds (top.removeFromLeft (cardW)); top.removeFromLeft (10);
        masterPeak.setBounds (top.removeFromLeft (cardW)); top.removeFromLeft (10);
        projected.setBounds (top);
    }
    else
    {
        ownLevel.setVisible (true);
        masterPeak.setVisible (false);
        ownLevel.setBounds (top.removeFromLeft (cardW)); top.removeFromLeft (10);
        masterNow.setBounds (top.removeFromLeft (cardW)); top.removeFromLeft (10);
        projected.setBounds (top);
    }
    masterPeak.setVisible (isMaster);
    r.removeFromTop (12);

    auto controls = r.removeFromTop (isMaster ? 92 : 70);
    for (auto* c : std::initializer_list<juce::Component*> { &targetLufs, &targetTp, &targetLufsLabel, &targetTpLabel, &balanceNow, &autoBalance }) c->setVisible (isMaster);
    for (auto* c : std::initializer_list<juce::Component*> { &acceptMoves, &resetTrims, &hint }) c->setVisible (! isMaster);
    if (isMaster)
    {
        auto left = controls.removeFromLeft (controls.getWidth() - 300);
        auto row1 = left.removeFromTop (44);
        targetLufsLabel.setBounds (row1.removeFromLeft (210));
        targetLufs.setBounds (row1);
        auto row2 = left.removeFromTop (44);
        targetTpLabel.setBounds (row2.removeFromLeft (210));
        targetTp.setBounds (row2);
        controls.removeFromLeft (16);
        balanceNow.setBounds (controls.removeFromTop (40));
        controls.removeFromTop (8);
        autoBalance.setBounds (controls.removeFromTop (26));
    }
    else
    {
        auto row = controls.removeFromTop (28);
        acceptMoves.setBounds (row.removeFromLeft (280));
        row.removeFromLeft (10);
        resetTrims.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (10);
        makeMaster.setBounds (row.removeFromLeft (260));
        controls.removeFromTop (6);
        hint.setBounds (controls);
    }
    r.removeFromTop (8);
    summary.setBounds (r.removeFromTop (34));
    r.removeFromTop (6);

    int maskH = 0;
    if (isMaster)
    {
        maskH = 22 + (int) maskingButtons.size() * 30;
        maskH = juce::jmin (maskH, r.getHeight() / 2);
    }
    const int tableH = juce::jmin (juce::jmax (80, r.getHeight() - maskH - 8), table.preferredHeight());
    table.setBounds (r.removeFromTop (tableH));
    r.removeFromTop (8);
    maskingTitle.setBounds (r.removeFromTop (22));
    for (auto* b : maskingButtons)
    {
        if (r.getHeight() < 28) { b->setVisible (false); continue; }
        b->setVisible (true);
        b->setBounds (r.removeFromTop (26));
        r.removeFromTop (4);
    }
}
}
