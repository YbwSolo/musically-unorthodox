#include "MixPage.h"
#include "../PluginProcessor.h"
#include "../analysis/TonalAnalysis.h"

namespace mu
{
using namespace theme;
static constexpr float kLaneLo = -48.0f, kLaneHi = -6.0f;

MixPage::MixPage (MUProcessor& p, std::function<void (const juce::String&)> status) : proc (p), setStatus (std::move (status))
{
    styleButton (learnButton, "Learn and balance", true, "mix.learn");
    learnButton.onClick = [this]
    {
        if (proc.getBus().getLearnState().active) { proc.getBus().cancelLearn(); return; }
        proc.learnAndBalance (20.0f);
    };
    styleButton (balanceButton, "Balance now", false, "mix.balance");
    balanceButton.onClick = [this]
    {
        proc.balanceNow();
        setStatus (plan.usable ? "Balanced. " + plan.summary : juce::String ("Nothing to balance yet. Play the song so every track can be measured."));
    };
    keepBalanced.setButtonText ("Keep balanced");
    keepBalanced.setWantsKeyboardFocus (false);
    keepBalanced.setToggleState (proc.getBus().isAutoBalance(), juce::dontSendNotification);
    keepBalanced.onClick = [this] { proc.setAutoBalanceMode (keepBalanced.getToggleState()); proc.getBus().setAutoBalance (keepBalanced.getToggleState(), proc.currentTargets()); };
    setHelp (keepBalanced, "mix.auto");
    for (auto* c : std::initializer_list<juce::Component*> { &learnButton, &balanceButton, &keepBalanced }) addAndMakeVisible (c);
    for (size_t i = 0; i < fixButtons.size(); ++i)
    {
        styleButton (fixButtons[i], "Fix", true, "mix.collisions");
        fixButtons[i].onClick = [this, i]
        {
            if (i >= clashes.size()) return;
            const auto c = clashes[i];
            proc.getBus().applyMaskingClash (c);
            fixedNote = "Fixed: \"" + c.nameB + "\" now dips up to " + juce::String (std::abs (juce::jlimit (-6.0f, -1.5f, c.suggestedCutDb)), 1) + " dB around " + hzString (c.freqHz)
                      + " while it is loud there. The cut is on that track's EQ tab.";
            fixedShow = 8.0f;
            setStatus (fixedNote);
            refreshData();
        };
        addChildComponent (fixButtons[i]);
    }
    refreshData();
}

void MixPage::refreshData()
{
    auto& bus = proc.getBus();
    auto tracks = bus.collectTracks();
    plan = bus.getLastPlan();
    clashes = bus.computeMasking();
    if (clashes.size() > 4) clashes.resize (4);
    learn = bus.getLearnState();

    hasMaster = false;
    const auto targets = proc.isMasterInstance() ? proc.currentTargets() : bus.getTargets();
    targetLufs = targets.masterLufs; targetPeak = targets.masterTruePeak;
    mixLufs = -100.0f; mixPeak = -100.0f; liveLufs = -100.0f;
    double livePower = 0.0;

    std::stable_sort (tracks.begin(), tracks.end(), [] (const TrackInfo& a, const TrackInfo& b)
    {
        if ((a.role == Role::Master) != (b.role == Role::Master)) return b.role == Role::Master;
        return rolePriority (a.role) > rolePriority (b.role);
    });

    lanes.clear();
    for (auto& t : tracks)
    {
        if (t.role == Role::Master) { hasMaster = true; mixLufs = t.levelLufs; mixPeak = t.truePeakDb; if (t.playing) liveLufs = t.shortLufs; continue; }
        if (t.playing && t.shortLufs > -70.0f) livePower += std::pow (10.0, (double) t.shortLufs / 10.0);
        auto& st = laneState[t.slot];
        st.info = t;
        float move = 0.0f;
        for (auto& m : plan.moves) if (m.slot == t.slot) move = m.deltaDb;
        st.move = move;
        lanes.push_back (st);
    }
    // Without a master instance the mix level is worked out from the tracks that are playing (their loudness
    // added together), so the dial moves with the music instead of showing a fixed forecast.
    if (! hasMaster) { liveLufs = livePower > 0.0 ? (float) (10.0 * std::log10 (livePower)) : -100.0f; mixLufs = liveLufs; }

    for (size_t i = 0; i < fixButtons.size(); ++i) fixButtons[i].setVisible (i < clashes.size());
    learnButton.setButtonText (learn.active ? "Stop listening" : "Learn and balance");
    resized();
}

void MixPage::tick (float dt)
{
    refreshTimer -= dt;
    if (refreshTimer <= 0.0f) { refreshTimer = 0.25f; refreshData(); }

    const bool playing = proc.isPlaying();
    for (auto& lane : lanes)
    {
        auto& st = laneState[lane.info.slot];
        const float target = lane.info.active ? lane.info.levelLufs : -70.0f;
        st.level = approach (st.level, target, dt, 0.18f);
        st.appear = juce::jmin (1.0f, st.appear + dt * 4.0f);
        float peak = -200.0f;
        if (lane.info.hasBands) for (float v : lane.info.bandsDb) peak = juce::jmax (peak, v);
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float v = lane.info.hasBands && lane.info.active ? juce::jlimit (0.0f, 1.0f, (lane.info.bandsDb[(size_t) b] - (peak - 30.0f)) / 30.0f) : 0.0f;
            st.heat[(size_t) b] = approach (st.heat[(size_t) b], v, dt, 0.25f);
        }
        lane.level = st.level; lane.appear = st.appear; lane.heat = st.heat;
    }
    dialValue = approach (dialValue, liveLufs > -70.0f ? liveLufs : (hasMaster && mixLufs > -70.0f ? mixLufs : targetLufs - 20.0f), dt, 0.2f);
    fixedShow = juce::jmax (0.0f, fixedShow - dt);
    dialPeak = approach (dialPeak, mixPeak, dt, 0.2f);
    learnProgress = learn.active ? juce::jlimit (0.0f, 1.0f, learnProgress + dt / 20.0f) : approach (learnProgress, 0.0f, dt, 0.15f);
    if (learn.active && learnProgress < learn.progress - 0.05f) learnProgress = learn.progress;
    pulse = playing ? std::fmod (pulse + dt, 2.0f) : 0.0f;
    march = std::fmod (march + dt * 26.0f, 10.0f);
    repaint();
}

// ---- dial ------------------------------------------------------------------------------------------
void MixPage::drawDial (juce::Graphics& g, juce::Rectangle<float> area)
{
    fillPanel (g, area);
    auto r = area.reduced (16.0f, 12.0f);
    g.setFont (bodyBold (13.0f));
    g.setColour (text);
    g.drawText ("Mix level", r.removeFromTop (20.0f), juce::Justification::centredLeft);

    const float side = juce::jmin (r.getWidth(), r.getHeight() - 46.0f);
    const auto box = juce::Rectangle<float> (side, side).withCentre ({ r.getCentreX(), r.getY() + side * 0.5f + 4.0f });
    const float radius = side * 0.5f - 8.0f;
    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    const float lo = targetLufs - 18.0f, hi = targetLufs + 6.0f;
    auto angleFor = [&] (float lufs) { return a0 + juce::jlimit (0.0f, 1.0f, (lufs - lo) / (hi - lo)) * (a1 - a0); };
    const auto c = box.getCentre();

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
    g.setColour (control);
    g.strokePath (track, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    // the window to aim for: one LU either side of the target
    juce::Path window;
    window.addCentredArc (c.x, c.y, radius, radius, 0.0f, angleFor (targetLufs - 1.0f), angleFor (targetLufs + 1.0f), true);
    g.setColour (yellow.withAlpha (0.28f));
    g.strokePath (window, juce::PathStrokeType (10.0f));

    const bool measured = mixLufs > -70.0f;
    const bool onTarget = measured && std::abs (mixLufs - targetLufs) <= 1.0f;
    if (measured)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, angleFor (dialValue), true);
        g.setColour (mixLufs > targetLufs + 1.0f ? over : (onTarget ? yellow : text));
        g.strokePath (value, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
    // target tick
    const float ta = angleFor (targetLufs);
    juce::Path tick;
    tick.addRectangle (-1.5f, -radius - 11.0f, 3.0f, 22.0f);
    g.setColour (yellow);
    g.fillPath (tick, juce::AffineTransform::rotation (ta).translated (c.x, c.y));

    g.setFont (headline (side * 0.30f));
    g.setColour (onTarget ? yellow : text);
    g.drawText (measured ? juce::String (mixLufs, 1) : juce::String ("--"), box.withTrimmedTop (side * 0.22f).withHeight (side * 0.36f), juce::Justification::centred);
    g.setFont (body (11.0f));
    g.setColour (textMute);
    g.drawText (hasMaster ? "LUFS integrated" : (measured ? "LUFS now, estimated from the tracks" : "Press play to measure"), box.withTrimmedTop (side * 0.58f).withHeight (14.0f), juce::Justification::centred);

    auto foot = juce::Rectangle<float> (r.getX(), r.getBottom() - 38.0f, r.getWidth(), 38.0f);
    auto row = [&] (const juce::String& label, const juce::String& value, bool alarm)
    {
        auto line1 = foot.removeFromTop (19.0f);
        g.setFont (body (12.0f)); g.setColour (textDim);
        g.drawText (label, line1, juce::Justification::centredLeft);
        g.setFont (bodyBold (12.0f)); g.setColour (alarm ? over : text);
        g.drawText (value, line1, juce::Justification::centredRight);
    };
    row ("Target for the mix", juce::String (targetLufs, 1) + " LUFS", false);
    row ("Highest peak", mixPeak > -70.0f ? juce::String (mixPeak, 1) + " dBTP  (keep under " + juce::String (juce::roundToInt (targetPeak)) + ")" : juce::String ("--"), mixPeak > targetPeak + 0.1f);
}

// ---- lanes -----------------------------------------------------------------------------------------
void MixPage::drawLanes (juce::Graphics& g)
{
    fillPanel (g, lanesArea.toFloat());
    auto r = lanesArea.reduced (14, 0);
    auto head = r.removeFromTop (38);
    g.setFont (bodyBold (13.0f)); g.setColour (text);
    g.drawText ("Session", head.removeFromLeft (70), juce::Justification::centredLeft);
    g.setFont (body (12.0f)); g.setColour (textMute);
    g.drawText (juce::String ((int) lanes.size()) + (lanes.size() == 1 ? " track" : " tracks") + (hasMaster ? " and the master" : ", no master yet"), head, juce::Justification::centredLeft);

    if (lanes.empty())
    {
        drawWrapped (g, "Add Musically Unorthodox to each track you want to balance (vocals, beat, 808, drums) and to the Stereo Out with its role set to Master. Each one appears here as a lane.",
                     r.toFloat().withTrimmedTop (8.0f).withWidth (juce::jmin (520.0f, (float) r.getWidth())), body (12.5f), textDim);
        return;
    }

    // column guide
    const int nameW = 150, heatW = juce::jlimit (150, 260, r.getWidth() / 4), moveW = 92;
    auto cols = r.removeFromTop (18);
    g.setFont (body (10.5f)); g.setColour (textMute);
    g.drawText ("Track", cols.removeFromLeft (nameW), juce::Justification::centredLeft);
    cols.removeFromLeft (10);
    g.drawText ("Where its energy is", cols.removeFromLeft (heatW), juce::Justification::centredLeft);
    cols.removeFromLeft (14);
    cols.removeFromRight (moveW);
    g.drawText ("Level and where it should sit", cols, juce::Justification::centredLeft);
    for (int lufs = -40; lufs <= -10; lufs += 10)
    {
        const float x = (float) cols.getX() + (float) cols.getWidth() * ((float) lufs - kLaneLo) / (kLaneHi - kLaneLo);
        if (x > (float) cols.getX() + 170.0f)
            g.drawText (juce::String (lufs) + (lufs == -10 ? " LUFS" : ""), juce::Rectangle<float> (x - 30.0f, (float) cols.getY(), 60.0f, (float) cols.getHeight()), juce::Justification::centred);
    }
    r.removeFromTop (4);

    const int rowH = juce::jlimit (26, 54, r.getHeight() / (int) lanes.size());
    const int visible = juce::jmin ((int) lanes.size(), r.getHeight() / rowH);
    for (int i = 0; i < visible; ++i)
    {
        auto& lane = lanes[(size_t) i];
        auto row = r.removeFromTop (rowH);
        lane.bounds = row;
        const bool mine = lane.info.slot == proc.getSessionSlot();
        const float alpha = lane.appear;
        if (mine) { g.setColour (raised); g.fillRoundedRectangle (row.toFloat().expanded (6.0f, 0.0f).reduced (0.0f, 1.0f), 4.0f); }
        g.setColour (line);
        if (i > 0 && ! mine) g.fillRect (row.getX(), row.getY(), row.getWidth(), 1);

        auto cell = row.reduced (0, juce::jmax (3, (rowH - 30) / 2));
        // name and role
        auto name = cell.removeFromLeft (nameW);
        g.setColour (MUProcessor::roleColour (lane.info.role).withAlpha (alpha));
        g.fillRect (name.removeFromLeft (3).reduced (0, 2));
        name.removeFromLeft (9);
        g.setFont (bodyBold (12.5f)); g.setColour ((lane.info.active ? text : textDim).withAlpha (alpha));
        g.drawText (lane.info.name, name.removeFromTop (name.getHeight() / 2 + 2), juce::Justification::bottomLeft);
        g.setFont (body (10.5f)); g.setColour (textMute.withAlpha (alpha));
        const auto roleName = roleNames()[(int) lane.info.role];
        g.drawText (lane.info.name.startsWith (roleName) ? juce::String (mine ? "This track" : "") : roleName + (mine ? ", this track" : ""), name, juce::Justification::topLeft);
        cell.removeFromLeft (10);

        // heat strip
        auto heat = cell.removeFromLeft (heatW).toFloat().reduced (0.0f, 4.0f);
        const float cw = heat.getWidth() / (float) kAnalysisBands;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float v = lane.heat[(size_t) b];
            g.setColour (juce::Colour (0xff1c1c1c).interpolatedWith (yellow, v * v).withAlpha (alpha));
            g.fillRect (heat.getX() + (float) b * cw, heat.getY(), cw - 1.0f, heat.getHeight());
        }
        for (auto& c : clashes)
            if (c.slotA == lane.info.slot || c.slotB == lane.info.slot)
            {
                // bracket the range where this track collides with another
                const float x = heat.getX() + heat.getWidth() * std::log (c.freqHz / 20.0f) / std::log (1000.0f);
                g.setColour (c.slotB == lane.info.slot ? over : text);
                g.drawRect (juce::Rectangle<float> (x - cw * 1.5f, heat.getY() - 2.0f, cw * 3.0f, heat.getHeight() + 4.0f), 1.5f);
            }
        cell.removeFromLeft (14);

        // move
        auto moveCell = cell.removeFromRight (moveW);
        lane.lockArea = moveCell.removeFromRight (24);
        const bool locked = ! lane.info.acceptsBalance;
        icons::draw (g, "link", lane.lockArea.toFloat().reduced (4.0f), locked ? textMute : textDim.withAlpha (0.0f));
        if (locked)
        {
            g.setFont (body (11.5f)); g.setColour (textMute);
            g.drawText ("Locked", moveCell, juce::Justification::centredRight);
        }
        else if (lane.info.active && plan.usable)
        {
            const bool fine = std::abs (lane.move) < 0.5f;
            g.setFont (bodyBold (12.0f)); g.setColour ((fine ? textDim : yellow).withAlpha (alpha));
            g.drawText (fine ? juce::String ("In place") : dbString (lane.move), moveCell, juce::Justification::centredRight);
        }

        // level bar with target
        auto bar = cell.toFloat().reduced (0.0f, 9.0f);
        auto xFor = [&] (float lufs) { return bar.getX() + juce::jlimit (0.0f, 1.0f, (lufs - kLaneLo) / (kLaneHi - kLaneLo)) * bar.getWidth(); };
        g.setColour (control);
        g.fillRoundedRectangle (bar, 3.0f);
        if (lane.level > kLaneLo)
        {
            g.setColour ((lane.info.active ? text : textMute).withAlpha (0.9f * alpha));
            g.fillRoundedRectangle (bar.withRight (xFor (lane.level)), 3.0f);
        }
        if (lane.info.active && plan.usable && ! locked)
        {
            const float targetX = xFor (lane.info.levelLufs + lane.move), nowX = xFor (lane.level);
            g.setColour (yellow);
            g.fillRect (targetX - 1.5f, bar.getY() - 5.0f, 3.0f, bar.getHeight() + 10.0f);
            if (std::abs (targetX - nowX) > 6.0f)
            {
                const float y = bar.getCentreY(), dir = targetX > nowX ? 1.0f : -1.0f;
                // dashes that travel towards where the track should be
                const float a = juce::jmin (nowX, targetX), b = juce::jmax (nowX, targetX) - 8.0f;
                for (float x = a - 10.0f + (dir > 0.0f ? march : 10.0f - march); x < b; x += 10.0f)
                    if (x + 6.0f > a) g.fillRect (juce::jmax (a, x), y - 1.0f, juce::jmin (6.0f, b - x), 2.0f);
                juce::Path head2;
                head2.addTriangle (targetX - dir * 2.0f, y, targetX - dir * 9.0f, y - 5.0f, targetX - dir * 9.0f, y + 5.0f);
                g.fillPath (head2);
            }
        }
    }
    if (visible < (int) lanes.size())
    {
        g.setFont (body (11.0f)); g.setColour (textMute);
        g.drawText ("+" + juce::String ((int) lanes.size() - visible) + " more tracks in the session", r.removeFromBottom (14), juce::Justification::centredLeft);
    }
}

// ---- collisions ------------------------------------------------------------------------------------
void MixPage::drawCollisions (juce::Graphics& g)
{
    fillPanel (g, collisionArea.toFloat());
    auto r = collisionArea.reduced (14, 0);
    auto head = r.removeFromTop (38);
    icons::draw (g, "collision", head.removeFromLeft (20).toFloat().reduced (1.0f), clashes.empty() ? textDim : yellow);
    head.removeFromLeft (8);
    g.setFont (bodyBold (13.0f)); g.setColour (text);
    g.drawText ("Collisions", head.removeFromLeft (80), juce::Justification::centredLeft);
    g.setFont (body (12.0f)); g.setColour (textMute);
    g.drawText (clashes.empty() ? juce::String() : juce::String ((int) clashes.size()) + " found, most serious first", head.removeFromLeft (190), juce::Justification::centredLeft);
    if (fixedShow > 0.0f && ! clashes.empty()) { g.setColour (yellow); g.drawText (fixedNote, head, juce::Justification::centredRight, true); }

    if (clashes.empty())
    {
        g.setFont (body (12.5f)); g.setColour (fixedShow > 0.0f ? yellow : textDim);
        if (fixedShow > 0.0f) { g.drawText (fixedNote, r.removeFromTop (22), juce::Justification::centredLeft, true); return; }
        g.drawText (lanes.size() < 2 ? "Collisions show up once two or more tracks are playing." : "No collisions. Every track has room where it matters.", r.removeFromTop (22), juce::Justification::centredLeft);
        return;
    }
    const int rowH = juce::jmin (34, r.getHeight() / (int) clashes.size());
    for (size_t i = 0; i < clashes.size(); ++i)
    {
        const auto& c = clashes[i];
        auto row = r.removeFromTop (rowH);
        if (i > 0) { g.setColour (line); g.fillRect (row.getX(), row.getY(), row.getWidth(), 1); }
        row.removeFromRight (70);
        g.setFont (bodyBold (12.5f)); g.setColour (text);
        const juce::String what = "\"" + c.nameB + "\" is covering \"" + c.nameA + "\" around " + hzString (c.freqHz);
        const int w = juce::jmin (row.getWidth() / 2 + 60, juce::roundToInt (bodyBold (12.5f).getStringWidthFloat (what)) + 16);
        g.drawText (what, row.removeFromLeft (w), juce::Justification::centredLeft, true);
        g.setFont (body (12.0f)); g.setColour (textDim);
        g.drawText ("Fix: dynamic cut of " + juce::String (c.suggestedCutDb, 1) + " dB on \"" + c.nameB + "\"", row, juce::Justification::centredLeft, true);
    }
}

void MixPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    drawDial (g, dialArea.toFloat());

    fillPanel (g, actionArea.toFloat());
    auto a = actionArea.reduced (16, 0);
    a.removeFromTop (14 + 36 + 8);
    // listening progress
    auto strip = a.removeFromTop (4).toFloat();
    g.setColour (control); g.fillRoundedRectangle (strip, 2.0f);
    if (learnProgress > 0.004f) { g.setColour (yellow); g.fillRoundedRectangle (strip.withWidth (strip.getWidth() * learnProgress), 2.0f); }
    a.removeFromTop (8 + 28 + 10 + 22 + 10);
    juce::String msg = learn.message;
    if (msg.isEmpty()) msg = plan.usable ? plan.summary : juce::String ("Play the loudest part of the song, then press Learn and balance. Every track is measured for 20 seconds and set to the level its role needs.");
    drawWrapped (g, msg, a.toFloat(), body (12.0f), textDim);

    drawLanes (g);
    drawCollisions (g);
}

void MixPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto left = r.removeFromLeft (290);
    r.removeFromLeft (10);
    dialArea = left.removeFromTop (juce::jmin (330, left.getHeight() * 55 / 100));
    left.removeFromTop (10);
    actionArea = left;
    auto a = actionArea.reduced (16, 0);
    a.removeFromTop (14);
    learnButton.setBounds (a.removeFromTop (36));
    a.removeFromTop (8 + 4 + 8);
    balanceButton.setBounds (a.removeFromTop (28));
    a.removeFromTop (10);
    keepBalanced.setBounds (a.removeFromTop (22));

    const int collisionH = 38 + juce::jmax (1, (int) clashes.size()) * 34 + 10;
    collisionArea = r.removeFromBottom (collisionH);
    r.removeFromBottom (10);
    lanesArea = r;

    auto c = collisionArea.reduced (14, 0);
    c.removeFromTop (38);
    for (size_t i = 0; i < clashes.size() && i < fixButtons.size(); ++i)
    {
        auto row = c.removeFromTop (34);
        fixButtons[i].setBounds (row.removeFromRight (60).withSizeKeepingCentre (60, 24));
    }
}

void MixPage::mouseDown (const juce::MouseEvent& e)
{
    for (auto& lane : lanes)
        if (lane.bounds.contains (e.getPosition()) && lane.lockArea.expanded (40, 0).contains (e.getPosition()))
        {
            proc.getBus().setTrackLocked (lane.info.slot, lane.info.acceptsBalance);
            setStatus (lane.info.acceptsBalance ? "\"" + lane.info.name + "\" is locked: balancing will leave its level alone." : "\"" + lane.info.name + "\" is unlocked and will be balanced with the rest.");
            refreshTimer = 0.0f;
            return;
        }
}

void MixPage::mouseMove (const juce::MouseEvent& e)
{
    juce::String key = "mix.lanes";
    if (dialArea.contains (e.getPosition())) key = "mix.dial";
    else if (collisionArea.contains (e.getPosition())) key = "mix.collisions";
    else for (auto& lane : lanes) if (lane.bounds.contains (e.getPosition()) && lane.lockArea.expanded (40, 0).contains (e.getPosition())) key = "mix.lock";
    getProperties().set ("help", key);
}
}
