#include "MixPage.h"
#include "Theme.h"

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
        rows.push_back (r);
    }
    repaint();
}

void SessionTable::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (24);
    g.setColour (theme::textMute);
    g.setFont (theme::bodyBold (10.0f));
    const int wName = juce::jmax (120, r.getWidth() - 430);
    struct Cols { juce::Rectangle<int> dot, name, role, level, peak, trim, delta; };
    auto cols = [&] (juce::Rectangle<int> row)
    {
        Cols c;
        c.dot = row.removeFromLeft (18);
        c.name = row.removeFromLeft (wName);
        c.role = row.removeFromLeft (120);
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
        g.setFont (theme::body (12.5f));
        g.drawText ("No instances found yet. Add Musically Unorthodox to each track you want balanced.", r.removeFromTop (40), juce::Justification::centredLeft);
        return;
    }

    for (auto& row : rows)
    {
        auto line = r.removeFromTop (26);
        if (row.own)
        {
            g.setColour (theme::yellow.withAlpha (0.08f));
            g.fillRect (line);
            g.setColour (theme::yellow);
            g.fillRect (line.removeFromLeft (2));
        }
        auto c = cols (line);
        g.setColour (MUProcessor::roleColour (row.t.role));
        g.fillEllipse (c.dot.withSizeKeepingCentre (7, 7).toFloat());
        g.setFont (theme::body (12.5f));
        g.setColour (row.t.active ? theme::text : theme::textMute);
        g.drawText (row.t.name + (row.own ? "  (this)" : ""), c.name, juce::Justification::centredLeft, true);
        g.setColour (theme::textDim);
        g.drawText (roleNames()[(int) row.t.role], c.role, juce::Justification::centredLeft, true);
        g.setFont (theme::bodyBold (12.0f));
        g.setColour (row.t.active ? theme::text : theme::textMute);
        g.drawText (row.t.levelLufs > -90.0f ? juce::String (row.t.levelLufs, 1) : "-", c.level, juce::Justification::centredRight);
        g.setColour (row.t.truePeakDb > 0.0f ? theme::bad : row.t.truePeakDb > -3.0f ? theme::yellow : theme::textDim);
        g.drawText (row.t.truePeakDb > -90.0f ? juce::String (row.t.truePeakDb, 1) : "-", c.peak, juce::Justification::centredRight);
        g.setColour (theme::textDim);
        g.drawText (row.t.role == Role::Master ? "" : theme::dbString (row.t.trimDb, 1, ""), c.trim, juce::Justification::centredRight);
        if (row.t.role == Role::Master) g.setColour (theme::textMute);
        else g.setColour (std::abs (row.delta) < 0.1f ? theme::good : theme::yellow);
        g.drawText (row.t.role == Role::Master ? "master" : (! row.t.active ? "silent" : std::abs (row.delta) < 0.1f ? "sits right" : theme::dbString (row.delta, 1)), c.delta, juce::Justification::centredRight);
    }
}

// ---- MixPage -------------------------------------------------------------------------------------
MixPage::MixPage (MUProcessor& p) : proc (p)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);

    for (auto* c : { &card1, &card2, &card3, &card4 }) content.addAndMakeVisible (c);

    learnBtn.setComponentID ("primary");
    learnBtn.setTooltip ("Clears every instance's reading, listens for 20 seconds while you play the loudest section, then moves every track to its target in one go.");
    learnBtn.onClick = [this] { proc.learnAndBalance (20.0f); refresh(); };
    balanceBtn.setComponentID ("ghost");
    balanceBtn.setTooltip ("Apply the current plan right now using what has been measured so far.");
    balanceBtn.onClick = [this] { proc.balanceNow(); refresh(); };
    cancelBtn.setComponentID ("ghost");
    cancelBtn.onClick = [this] { proc.getBus().cancelLearn(); refresh(); };
    for (auto* b : { &learnBtn, &balanceBtn, &cancelBtn }) content.addAndMakeVisible (b);

    autoBalance.setTooltip ("Keeps re-balancing gently while the song plays. Switch it off once you are happy so you can ride faders by hand.");
    autoBalance.onClick = [this] { proc.setAutoBalanceMode (autoBalance.getToggleState()); };
    content.addAndMakeVisible (autoBalance);
    autoTrim.setTooltip ("The master instance adjusts its own gain so the integrated loudness lands exactly on the target.");
    content.addAndMakeVisible (autoTrim);
    autoTrimAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::masterAutoTrim, autoTrim);
    peakGuard.setTooltip ("A transparent 2 ms look-ahead limiter at the ceiling so nothing you hand over has peaks above it. Safety net, not a loudness tool.");
    content.addAndMakeVisible (peakGuard);
    peakGuardAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::peakGuard, peakGuard);

    content.addAndMakeVisible (progress);
    status.setFont (theme::body (12.5f));
    status.setColour (juce::Label::textColourId, theme::textDim);
    status.setJustificationType (juce::Justification::centredLeft);
    content.addAndMakeVisible (status);

    makeMaster.setComponentID ("primary");
    makeMaster.onClick = [this] { proc.setParam (ids::role, (float) Role::Master); refresh(); };
    content.addAndMakeVisible (makeMaster);
    resetTrims.setComponentID ("ghost");
    resetTrims.onClick = [this] { proc.resetTrim(); };
    content.addAndMakeVisible (resetTrims);
    content.addAndMakeVisible (acceptMoves);
    acceptAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::autoBalance, acceptMoves);

    // targets
    content.addAndMakeVisible (targetsPanel);
    targetLufs = std::make_unique<HSlider> (proc.apvts, ids::targetLufs, "Song loudness (integrated)", 190,
        "Where the whole song should sit before mastering. -18 LUFS is the standard hand-off; trap and drill often go hotter (-14 to -12).");
    targetTp = std::make_unique<HSlider> (proc.apvts, ids::targetTruePeak, "True-peak ceiling", 190,
        "Headroom left for the mastering engineer. -6 dBTP is safe; never above -3.");
    targetsPanel.addAndMakeVisible (*targetLufs);
    targetsPanel.addAndMakeVisible (*targetTp);
    for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r)
    {
        auto s = std::make_unique<HSlider> (proc.apvts, ids::roleOffset ((Role) r), roleNames()[r], 120,
            "How loud this role should be against the lead vocal (0 LU = same loudness).");
        if ((Role) r == Role::Vocal) s->getSlider().setEnabled (false);
        targetsPanel.addAndMakeVisible (*s);
        roleSliders.push_back (std::move (s));
    }
    resetTargets.setComponentID ("ghost");
    resetTargets.onClick = [this]
    {
        proc.setParam (ids::targetLufs, -18.0f); proc.setParam (ids::targetTruePeak, -6.0f);
        for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r) proc.setParam (ids::roleOffset ((Role) r), defaultRoleOffsetLu ((Role) r));
    };
    targetsPanel.addAndMakeVisible (resetTargets);

    content.addAndMakeVisible (sessionPanel);
    sessionPanel.addAndMakeVisible (table);
    content.addAndMakeVisible (maskingPanel);
    maskingEmpty.setFont (theme::body (12.5f));
    maskingEmpty.setColour (juce::Label::textColourId, theme::textMute);
    maskingEmpty.setText ("Nothing is fighting for the same space right now.", juce::dontSendNotification);
    maskingPanel.addAndMakeVisible (maskingEmpty);
    refresh();
}

void MixPage::setStatus (const juce::String& s)
{
    statusText = s;
    statusUntil = juce::Time::getMillisecondCounterHiRes() + 8000.0;
    status.setText (s, juce::dontSendNotification);
}

void MixPage::rebuildMaskingButtons()
{
    maskingButtons.clear();
    for (size_t i = 0; i < clashes.size(); ++i)
    {
        auto& c = clashes[i];
        auto* b = maskingButtons.add (new juce::TextButton());
        b->setComponentID ("ghost");
        b->setButtonText ("FIX   " + c.nameB + "  sits on  " + c.nameA + "  around " + theme::hzString (c.freqHz) + "   (" + juce::String (c.suggestedCutDb, 1) + " dB dynamic cut)");
        b->onClick = [this, i] { if (i < clashes.size()) proc.getBus().applyMaskingClash (clashes[i]); };
        maskingPanel.addAndMakeVisible (b);
    }
    maskingEmpty.setVisible (clashes.empty());
    resized();
}

void MixPage::refresh()
{
    const bool isMaster = proc.isMasterInstance();
    auto& bus = proc.getBus();
    const auto plan = bus.getLastPlan();
    const auto tracks = bus.collectTracks();
    const int oldH = table.preferredHeight();
    table.update (tracks, plan, proc.getSessionSlot());
    const bool tableGrew = table.preferredHeight() != oldH;

    const auto& l = proc.getLoudness();
    const float tLufs = proc.apvts.getRawParameterValue (ids::targetLufs)->load();
    const float tTp = proc.apvts.getRawParameterValue (ids::targetTruePeak)->load();
    auto near = [] (float v, float t, float tol) { return std::abs (v - t) <= tol ? theme::good : std::abs (v - t) <= tol * 3.0f ? theme::yellow : theme::bad; };

    const auto learn = bus.getLearnState();
    progress.setProgress (learn.progress, learn.active);
    learnBtn.setEnabled (! learn.active);
    cancelBtn.setVisible (learn.active && isMaster);
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (learn.active || (learn.message.isNotEmpty() && now > statusUntil))
        status.setText (learn.message.isNotEmpty() ? learn.message : (plan.usable ? plan.summary : ""), juce::dontSendNotification);
    else if (now > statusUntil)
        status.setText (plan.usable ? plan.summary : "Play the song so every instance can listen, then press Learn & Balance.", juce::dontSendNotification);

    if (isMaster)
    {
        const float integ = l.getIntegrated();
        card1.set (integ > -90.0f ? juce::String (integ, 1) : "--", "master LUFS now", integ > -90.0f ? near (integ, tLufs, 1.0f) : theme::textDim);
        const float tp = l.getTruePeakHold();
        card2.set (tp > -90.0f ? juce::String (tp, 1) : "--", "master true peak", tp > -90.0f ? (tp <= tTp ? theme::good : tp > 0.0f ? theme::bad : theme::yellow) : theme::textDim);
        card3.set (plan.usable ? juce::String (plan.projectedMasterLufs, 1) : "--", "after balance", plan.usable ? near (plan.projectedMasterLufs, tLufs, 1.0f) : theme::textDim);
        int heard = 0, total = 0;
        for (auto& t : tracks) if (t.role != Role::Master) { ++total; if (t.active) ++heard; }
        const float at = proc.getMasterAutoTrimDb();
        card4.set (std::abs (at) > 0.05f ? theme::dbString (at) : juce::String (heard) + " / " + juce::String (total),
                   std::abs (at) > 0.05f ? "master auto trim" : "tracks heard", theme::text);
        autoBalance.setToggleState (proc.getAutoBalanceMode(), juce::dontSendNotification);
        auto fresh = bus.computeMasking();
        bool changed = fresh.size() != clashes.size();
        for (size_t i = 0; ! changed && i < fresh.size(); ++i) changed = fresh[i].slotB != clashes[i].slotB || std::abs (fresh[i].freqHz - clashes[i].freqHz) > 1.0f;
        clashes = fresh;
        if (changed) rebuildMaskingButtons();
    }
    else
    {
        const float lv = l.getIntegrated() > -90.0f ? l.getIntegrated() : l.getShortTerm();
        card1.set (lv > -90.0f ? juce::String (lv, 1) : "--", "this track LUFS", theme::text);
        const float trim = proc.getTrimDb();
        card2.set (theme::dbString (trim), "balance trim applied", std::abs (trim) < 0.05f ? theme::textDim : theme::yellow);
        float mine = 0.0f; juce::String note = "waiting for master";
        for (auto& m : plan.moves) if (m.slot == proc.getSessionSlot()) { mine = m.deltaDb; note = m.note; }
        card3.set (plan.usable ? theme::dbString (mine) : "--", "next move (" + note + ")", std::abs (mine) < 0.1f ? theme::good : theme::yellow);
        const float tp = l.getTruePeakHold();
        card4.set (tp > -90.0f ? juce::String (tp, 1) : "--", "this track true peak", tp > -0.3f ? theme::bad : theme::text);
        const int masterSlot = bus.getMasterSlot();
        if (masterSlot < 0 && now > statusUntil) status.setText ("No master instance in this session yet. Put one on the stereo out and set its role to Master, or use the button above.", juce::dontSendNotification);
        if (! maskingButtons.isEmpty()) { maskingButtons.clear(); clashes.clear(); }
    }

    makeMaster.setVisible (! isMaster && bus.getMasterSlot() < 0);
    if (lastWasMaster != isMaster || tableGrew) { lastWasMaster = isMaster; resized(); }
}

void MixPage::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void MixPage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int w = getWidth() - (viewport.isVerticalScrollBarShown() ? 10 : 0);
    auto r = juce::Rectangle<int> (0, 0, w, 10000).reduced (18, 16);
    const int startY = r.getY();
    if (proc.isMasterInstance()) layoutMaster (r); else layoutTrack (r);
    juce::ignoreUnused (startY);
}

void MixPage::layoutMaster (juce::Rectangle<int> r)
{
    for (auto* c : std::initializer_list<juce::Component*> { &makeMaster, &resetTrims, &acceptMoves }) c->setVisible (false);
    for (auto* c : std::initializer_list<juce::Component*> { &learnBtn, &balanceBtn, &autoBalance, &autoTrim, &peakGuard, &progress, &targetsPanel, &maskingPanel }) c->setVisible (true);

    auto cards = r.removeFromTop (72);
    const int cw = (cards.getWidth() - 36) / 4;
    for (auto* c : { &card1, &card2, &card3, &card4 }) { c->setBounds (cards.removeFromLeft (cw)); cards.removeFromLeft (12); }
    r.removeFromTop (14);

    auto actions = r.removeFromTop (40);
    learnBtn.setBounds (actions.removeFromLeft (230));
    actions.removeFromLeft (10);
    balanceBtn.setBounds (actions.removeFromLeft (150));
    actions.removeFromLeft (10);
    cancelBtn.setBounds (actions.removeFromLeft (100));
    actions.removeFromLeft (16);
    auto toggles = actions;
    autoBalance.setBounds (toggles.removeFromLeft (270).withSizeKeepingCentre (270, 24));
    r.removeFromTop (6);
    auto toggles2 = r.removeFromTop (24);
    toggles2.removeFromLeft (0);
    autoTrim.setBounds (toggles2.removeFromLeft (230));
    peakGuard.setBounds (toggles2.removeFromLeft (260));
    r.removeFromTop (8);
    progress.setBounds (r.removeFromTop (4));
    r.removeFromTop (6);
    status.setBounds (r.removeFromTop (22));
    r.removeFromTop (12);

    // targets panel: 2 target rows + role grid (2 columns)
    const int roleRows = (int) (roleSliders.size() + 1) / 2;
    const int targetsH = 35 + 10 + 2 * 28 + 10 + roleRows * 26 + 10 + 30 + 10;
    targetsPanel.setBounds (r.removeFromTop (targetsH));
    {
        auto c = targetsPanel.getContentArea();
        targetLufs->setBounds (c.removeFromTop (28));
        targetTp->setBounds (c.removeFromTop (28));
        c.removeFromTop (10);
        auto grid = c.removeFromTop (roleRows * 26);
        const int colW = grid.getWidth() / 2;
        for (size_t i = 0; i < roleSliders.size(); ++i)
        {
            const int col = (int) i % 2, row = (int) i / 2;
            roleSliders[i]->setBounds (grid.getX() + col * colW, grid.getY() + row * 26, colW - 12, 26);
        }
        c.removeFromTop (10);
        resetTargets.setBounds (c.removeFromTop (28).removeFromLeft (150));
    }
    r.removeFromTop (12);

    sessionPanel.setBounds (r.removeFromTop (35 + table.preferredHeight() + 10));
    table.setBounds (sessionPanel.getContentArea());
    r.removeFromTop (12);

    const int maskH = 35 + 10 + (clashes.empty() ? 24 : (int) clashes.size() * 32) + 10;
    maskingPanel.setBounds (r.removeFromTop (maskH));
    {
        auto c = maskingPanel.getContentArea();
        maskingEmpty.setBounds (c.removeFromTop (24));
        for (auto* b : maskingButtons) { b->setBounds (c.removeFromTop (28)); c.removeFromTop (4); }
    }
    content.setSize (viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? 10 : 0), maskingPanel.getBottom() + 16);
}

void MixPage::layoutTrack (juce::Rectangle<int> r)
{
    for (auto* c : std::initializer_list<juce::Component*> { &learnBtn, &balanceBtn, &cancelBtn, &autoBalance, &autoTrim, &peakGuard, &progress, &targetsPanel, &maskingPanel }) c->setVisible (false);
    for (auto* c : std::initializer_list<juce::Component*> { &resetTrims, &acceptMoves }) c->setVisible (true);

    auto cards = r.removeFromTop (72);
    const int cw = (cards.getWidth() - 36) / 4;
    for (auto* c : { &card1, &card2, &card3, &card4 }) { c->setBounds (cards.removeFromLeft (cw)); cards.removeFromLeft (12); }
    r.removeFromTop (14);

    auto actions = r.removeFromTop (36);
    acceptMoves.setBounds (actions.removeFromLeft (260).withSizeKeepingCentre (260, 24));
    actions.removeFromLeft (10);
    resetTrims.setBounds (actions.removeFromLeft (140));
    actions.removeFromLeft (10);
    makeMaster.setBounds (actions.removeFromLeft (280));
    r.removeFromTop (8);
    status.setBounds (r.removeFromTop (22));
    r.removeFromTop (12);

    sessionPanel.setBounds (r.removeFromTop (35 + table.preferredHeight() + 10));
    table.setBounds (sessionPanel.getContentArea());
    content.setSize (viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? 10 : 0), sessionPanel.getBottom() + 16);
}
}
