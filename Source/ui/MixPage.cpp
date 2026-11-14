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
        setStatus (plan.usable ? "Balanced. " + plan.summary.replace ("Balanced. ", "") : juce::String ("Nothing to balance yet. Play the song so every track can be measured."));
    };
    styleButton (bounceButton, "Bounce check", false, "mix.check");
    bounceButton.onClick = [this]
    {
        showCheck = ! showCheck;
        checkTimer = 0.0f;
        bounceButton.setButtonText (showCheck ? "Back to tracks" : "Bounce check");
        if (showCheck) setStatus ("Bounce check: what to put right before you export the song. It reads the whole session, so play the song through first.");
        refreshData();
    };
    styleButton (keyButton, "Set on all tracks", false, "mix.key");
    keyButton.onClick = [this]
    {
        if (! songKey.found) { setStatus ("The key is not known yet. Play the song with the plugin on the beat or the Stereo Out, or name the beat's track with its key (for example \"beat Gmin\")."); return; }
        const int total = proc.getBus().getNumInstances();
        const int tuned = proc.getBus().sendKeyToAll (songKey.key, songKey.minor);
        setStatus (songKey.text() + " set on all " + juce::String (total) + (total == 1 ? " instance. " : " instances. ")
                   + (tuned == 0 ? juce::String ("None has Tune on yet; the key is ready for when one does.") : juce::String (tuned) + (tuned == 1 ? " has Tune on and now follows it." : " have Tune on and now follow it.")));
    };
    keepBalanced.setButtonText ("Keep balanced");
    keepBalanced.setWantsKeyboardFocus (false);
    keepBalanced.setToggleState (proc.getBus().isAutoBalance(), juce::dontSendNotification);
    keepBalanced.onClick = [this] { proc.setAutoBalanceMode (keepBalanced.getToggleState()); proc.getBus().setAutoBalance (keepBalanced.getToggleState(), proc.currentTargets()); };
    setHelp (keepBalanced, "mix.auto");
    for (auto* c : std::initializer_list<juce::Component*> { &learnButton, &balanceButton, &keepBalanced, &bounceButton, &keyButton }) addAndMakeVisible (c);
    for (size_t i = 0; i < fixButtons.size(); ++i)
    {
        styleButton (fixButtons[i], "Fix", true, "mix.collisions");
        fixButtons[i].onClick = [this, i]
        {
            if (i >= fixes.size()) return;
            if (fixes[i].peaks)
            {
                const auto h = fixes[i].hot;
                const auto done = proc.getBus().tamePeaks (h.slot);
                fixedNote = done.isEmpty() ? "Play \"" + h.name + "\" first so it can be measured." : "\"" + h.name + "\": " + done + " Its settings are on that track's Dynamics tab.";
                fixedShow = 9.0f;
                setStatus (fixedNote);
                refreshData();
                return;
            }
            const auto c = fixes[i].clash;
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
    // what there is to fix: tracks whose peaks are too hot first, then the collisions; four at most
    fixes.clear();
    for (auto& h : bus.findHotPeaks()) { if (fixes.size() >= 2) break; Fix f; f.peaks = true; f.hot = h; fixes.push_back (f); }
    for (auto& c : clashes) { if (fixes.size() >= 4) break; Fix f; f.clash = c; fixes.push_back (f); }
    songKey = bus.findSongKey();
    if (showCheck) checkItems = proc.bounceCheck();
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

    for (size_t i = 0; i < fixButtons.size(); ++i) fixButtons[i].setVisible (i < fixes.size());
    learnButton.setButtonText (learn.active ? "Stop listening" : "Learn and balance");
    resized();
}

void MixPage::tick (float dt)
{
    refreshTimer -= dt;
    if (refreshTimer <= 0.0f)
    {
        refreshTimer = 0.25f;
        // anything the session did on its own (a follow-up to a Fix) is said out loud
        const auto notice = proc.getBus().takeNotice();
        if (notice.isNotEmpty()) { fixedNote = notice; fixedShow = 10.0f; setStatus (notice); }
        refreshData();
    }

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
    window.addCentredArc (c.x, c.y, radius, radius, 0.0f, angleFor (targetLufs - (hasMaster ? 1.0f : 2.5f)), angleFor (targetLufs + (hasMaster ? 1.0f : 2.5f)), true);
    g.setColour (yellow.withAlpha (0.28f));
    g.strokePath (window, juce::PathStrokeType (10.0f));

    const bool measured = mixLufs > -70.0f;
    // Without a master the reading is the last three seconds of the tracks added together, which rises and falls
    // with the song, so it is judged more loosely than a true whole-song measurement.
    const float tolerance = hasMaster ? 1.0f : 2.5f;
    const bool onTarget = measured && std::abs (mixLufs - targetLufs) <= tolerance;
    if (measured)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, angleFor (dialValue), true);
        g.setColour (mixLufs > targetLufs + (hasMaster ? 1.0f : 4.0f) ? over : (onTarget ? yellow : text));
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
    g.drawText (juce::String ((int) lanes.size()) + (lanes.size() == 1 ? " track" : " tracks") + (hasMaster ? " and the master" : ", no master yet"), head.removeFromLeft (190), juce::Justification::centredLeft);
    // the key of the song, and where that came from
    head.removeFromRight (keyButton.getWidth() + 10);
    g.setFont (bodyBold (12.5f)); g.setColour (songKey.found ? yellow : textMute);
    const juce::String keyText = songKey.found ? "Song key: " + songKey.text() : juce::String ("Song key: not known yet");
    const int keyW = juce::roundToInt (bodyBold (12.5f).getStringWidthFloat (keyText)) + 10;
    g.drawText (keyText, head.removeFromRight (keyW), juce::Justification::centredRight);
    if (songKey.found)
    {
        g.setFont (body (11.5f)); g.setColour (textMute);
        g.drawText ((songKey.fromName ? "from the name of \"" : "heard on \"") + songKey.source + "\"", head, juce::Justification::centredRight, true);
    }
    if (showCheck) { drawBounceCheck (g); return; }

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
        for (auto& f : fixes)
            if (f.peaks && f.hot.slot == lane.info.slot)
            {
                // this track's loud moments are hitting the top
                g.setColour (over);
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ bar.getRight() - 5.0f, bar.getCentreY() }));
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
    icons::draw (g, "collision", head.removeFromLeft (20).toFloat().reduced (1.0f), fixes.empty() ? textDim : yellow);
    head.removeFromLeft (8);
    g.setFont (bodyBold (13.0f)); g.setColour (text);
    g.drawText ("To fix", head.removeFromLeft (54), juce::Justification::centredLeft);
    g.setFont (body (12.0f)); g.setColour (textMute);
    g.drawText (fixes.empty() ? juce::String() : juce::String ((int) fixes.size()) + " found: hot peaks first, then collisions", head.removeFromLeft (260), juce::Justification::centredLeft);
    if (fixedShow > 0.0f && ! fixes.empty()) { g.setColour (yellow); g.drawText (fixedNote, head, juce::Justification::centredRight, true); }

    if (fixes.empty())
    {
        g.setFont (body (12.5f)); g.setColour (fixedShow > 0.0f ? yellow : textDim);
        if (fixedShow > 0.0f) { g.drawText (fixedNote, r.removeFromTop (22), juce::Justification::centredLeft, true); return; }
        g.drawText (lanes.size() < 2 ? "Nothing to fix. Collisions show up once two or more tracks are playing." : "Nothing to fix. No track is hitting the top and every track has room where it matters.", r.removeFromTop (22), juce::Justification::centredLeft);
        return;
    }
    const int rowH = juce::jmin (34, r.getHeight() / (int) fixes.size());
    for (size_t i = 0; i < fixes.size(); ++i)
    {
        const auto& f = fixes[i];
        auto row = r.removeFromTop (rowH);
        if (i > 0) { g.setColour (line); g.fillRect (row.getX(), row.getY(), row.getWidth(), 1); }
        row.removeFromRight (70);
        juce::String what, how;
        if (f.peaks)
        {
            what = "\"" + f.hot.name + "\" peaks " + juce::String (f.hot.overDb, 1) + " dB over the top";
            how = (f.hot.crestDb > 0.5f ? "Its loud moments are " + juce::String (juce::roundToInt (f.hot.crestDb)) + " dB above its average. " : juce::String())
                + (f.hot.compressorOn ? "Fix: lean on its compressor a little harder" : "Fix: switch on its compressor, fitted to its level");
        }
        else
        {
            what = "\"" + f.clash.nameB + "\" is covering \"" + f.clash.nameA + "\" around " + hzString (f.clash.freqHz);
            how = "Fix: dynamic cut of " + juce::String (f.clash.suggestedCutDb, 1) + " dB on \"" + f.clash.nameB + "\"";
        }
        g.setFont (bodyBold (12.5f)); g.setColour (f.peaks ? over.brighter (0.25f) : text);
        const int w = juce::jmin (row.getWidth() / 2 + 60, juce::roundToInt (bodyBold (12.5f).getStringWidthFloat (what)) + 16);
        g.drawText (what, row.removeFromLeft (w), juce::Justification::centredLeft, true);
        g.setFont (body (12.0f)); g.setColour (textDim);
        g.drawText (how, row, juce::Justification::centredLeft, true);
    }
}

// ---- bounce check ----------------------------------------------------------------------------------
void MixPage::drawBounceCheck (juce::Graphics& g)
{
    auto r = lanesArea.reduced (14, 0);
    r.removeFromTop (38);
    g.setFont (headline (22.0f)); g.setColour (text);
    g.drawText ("READY TO BOUNCE?", r.removeFromTop (30), juce::Justification::centredLeft);
    int problems = 0, warnings = 0;
    for (auto& c : checkItems) { if (c.state == CheckItem::Problem) ++problems; if (c.state == CheckItem::Warn) ++warnings; }
    g.setFont (body (12.5f)); g.setColour (problems > 0 ? over.brighter (0.25f) : warnings > 0 ? yellow : textDim);
    g.drawText (problems > 0 ? juce::String (problems) + (problems == 1 ? " thing has" : " things have") + " to be put right first."
                : warnings > 0 ? juce::String (warnings) + (warnings == 1 ? " thing is" : " things are") + " worth a look, then it is ready."
                               : juce::String ("Nothing stands in the way. Bounce it, then listen on a phone and in a car."), r.removeFromTop (20), juce::Justification::centredLeft);
    r.removeFromTop (8);
    for (auto& c : checkItems)
    {
        const float textW = (float) r.getWidth() - 150.0f;
        const int h = juce::jmax (30, (int) wrappedHeight (c.detail, textW, body (12.0f)) + 14);
        if (r.getHeight() < h) break;
        auto row = r.removeFromTop (h);
        g.setColour (line); g.fillRect (row.getX(), row.getY(), row.getWidth(), 1);
        const auto colour = c.state == CheckItem::Problem ? over : c.state == CheckItem::Warn ? yellow : c.state == CheckItem::Good ? text : textMute;
        auto mark = row.removeFromLeft (22).toFloat();
        if (c.state == CheckItem::Good) { g.setColour (text); g.drawEllipse (mark.withSizeKeepingCentre (10.0f, 10.0f).withY ((float) row.getY() + 10.0f), 1.5f); }
        else { g.setColour (colour); g.fillEllipse (mark.withSizeKeepingCentre (10.0f, 10.0f).withY ((float) row.getY() + 10.0f)); }
        g.setFont (bodyBold (12.5f)); g.setColour (c.state == CheckItem::Good ? textDim : colour.brighter (c.state == CheckItem::Problem ? 0.25f : 0.0f));
        g.drawText (c.title, row.removeFromLeft (128).withHeight (30), juce::Justification::centredLeft, true);
        drawWrapped (g, c.detail, row.toFloat().withTrimmedTop (7.0f), body (12.0f), c.state == CheckItem::Good ? textMute : textDim);
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
    auto pair = a.removeFromTop (28);
    balanceButton.setBounds (pair.removeFromLeft ((pair.getWidth() - 8) / 2));
    pair.removeFromLeft (8);
    bounceButton.setBounds (pair);
    a.removeFromTop (10);
    keepBalanced.setBounds (a.removeFromTop (22));

    const int collisionH = 38 + juce::jmax (1, (int) fixes.size()) * 34 + 10;
    collisionArea = r.removeFromBottom (collisionH);
    r.removeFromBottom (10);
    lanesArea = r;
    keyButton.setBounds (lanesArea.getRight() - 14 - 128, lanesArea.getY() + 8, 128, 22);

    auto c = collisionArea.reduced (14, 0);
    c.removeFromTop (38);
    for (size_t i = 0; i < fixes.size() && i < fixButtons.size(); ++i)
    {
        auto row = c.removeFromTop (34);
        fixButtons[i].setBounds (row.removeFromRight (60).withSizeKeepingCentre (60, 24));
    }
}

void MixPage::mouseDown (const juce::MouseEvent& e)
{
    if (showCheck) return;
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
    else if (showCheck && lanesArea.contains (e.getPosition())) key = "mix.check";
    else for (auto& lane : lanes) if (lane.bounds.contains (e.getPosition()) && lane.lockArea.expanded (40, 0).contains (e.getPosition())) key = "mix.lock";
    getProperties().set ("help", key);
}
}
