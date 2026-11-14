#include "SessionBus.h"
#include <cmath>
#include <algorithm>

namespace mu
{
SessionBus::SessionBus() {}
SessionBus::~SessionBus() { stopTimer(); }

int SessionBus::registerClient (SessionClient* c)
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < kMaxInstances; ++i)
    {
        if (! slots[(size_t) i].used.load())
        {
            auto& s = slots[(size_t) i];
            s.client = c;
            s.momentary.store (-100.0f); s.shortTerm.store (-100.0f); s.integrated.store (-100.0f);
            s.truePeak.store (-100.0f); s.active.store (false); s.role.store (0);
            s.used.store (true);
            c->setSessionInstanceNumber (nextInstanceNumber++);
            if (! isTimerRunning()) startTimer (500);
            if (onSessionChanged) onSessionChanged();
            return i;
        }
    }
    return -1;
}

void SessionBus::unregisterClient (int slot)
{
    if (slot < 0 || slot >= kMaxInstances) return;
    const juce::ScopedLock sl (lock);
    auto& s = slots[(size_t) slot];
    s.client = nullptr;
    s.used.store (false);
    s.active.store (false);
    if (getNumInstances() == 0) { stopTimer(); autoBalance = false; }
    if (onSessionChanged) onSessionChanged();
}

void SessionBus::publish (int slot, float momentary, float shortTerm, float integrated, float truePeak, float crest, bool active, Role role) noexcept
{
    if (slot < 0 || slot >= kMaxInstances) return;
    auto& s = slots[(size_t) slot];
    s.momentary.store (momentary, std::memory_order_relaxed);
    s.shortTerm.store (shortTerm, std::memory_order_relaxed);
    s.integrated.store (integrated, std::memory_order_relaxed);
    s.truePeak.store (truePeak, std::memory_order_relaxed);
    s.crest.store (crest, std::memory_order_relaxed);
    s.active.store (active, std::memory_order_relaxed);
    s.role.store ((int) role, std::memory_order_relaxed);
}

float SessionBus::getVocalKeyLevel() const noexcept
{
    float best = 0.0f;
    for (auto& s : slots)
    {
        if (! s.used.load (std::memory_order_relaxed)) continue;
        if (s.role.load (std::memory_order_relaxed) != (int) Role::Vocal) continue;
        const float mom = s.momentary.load (std::memory_order_relaxed);
        const float st = s.shortTerm.load (std::memory_order_relaxed);
        // Presence relative to the vocal's own recent level, so quiet mixes still key correctly.
        const float ref = juce::jmax (-45.0f, st);
        const float key = juce::jlimit (0.0f, 1.0f, (mom - (ref - 12.0f)) / 12.0f);
        best = juce::jmax (best, mom > -60.0f ? key : 0.0f);
    }
    return best;
}

int SessionBus::getNumInstances() const
{
    int n = 0;
    for (auto& s : slots) if (s.used.load()) ++n;
    return n;
}

int SessionBus::getMasterSlot() const
{
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < kMaxInstances; ++i)
    {
        auto& s = slots[(size_t) i];
        if (s.used.load() && s.client != nullptr && s.client->getEffectiveRole() == Role::Master) return i;
    }
    return -1;
}

std::vector<TrackInfo> SessionBus::collectTracks() const
{
    const juce::ScopedLock sl (lock);
    std::vector<TrackInfo> out;
    for (int i = 0; i < kMaxInstances; ++i)
    {
        auto& s = slots[(size_t) i];
        if (! s.used.load() || s.client == nullptr) continue;
        TrackInfo t;
        t.slot = i;
        t.name = s.client->getDisplayName();
        t.role = s.client->getEffectiveRole();
        const float integ = s.integrated.load();
        t.levelLufs = integ > -70.0f ? integ : s.shortTerm.load();
        t.shortLufs = s.shortTerm.load();
        t.playing = s.active.load();
        t.truePeakDb = s.truePeak.load();
        t.trimDb = s.client->getTrimDb();
        t.acceptsBalance = s.client->acceptsBalance();
        t.active = s.active.load() || t.levelLufs > -60.0f;
        t.hasBands = s.client->fillBandsDb (t.bandsDb);
        out.push_back (t);
    }
    return out;
}

BalancePlan SessionBus::computePlan (const BalanceTargets& tg) const
{
    return computeBalance (collectTracks(), tg);
}

void SessionBus::applyPlan (const BalancePlan& plan, float fraction)
{
    const juce::ScopedLock sl (lock);
    for (auto& m : plan.moves)
    {
        if (m.slot < 0 || m.slot >= kMaxInstances) continue;
        auto& s = slots[(size_t) m.slot];
        if (! s.used.load() || s.client == nullptr || ! s.client->acceptsBalance()) continue;
        const float cur = s.client->getTrimDb();
        s.client->setTrimDb (cur + (m.newTrimDb - cur) * juce::jlimit (0.0f, 1.0f, fraction));
    }
    // Everything measured so far describes the old balance. Start again, and give the new levels a few
    // seconds to be heard before any further move is proposed.
    const double now = juce::Time::getMillisecondCounterHiRes();
    resetAtMs = now + 200.0;            // once the trims have finished gliding
    settleUntilMs = now + 4500.0;
    if (! isTimerRunning()) startTimer (500);
}

std::array<float, kAnalysisBands> SessionBus::bandCentres() const
{
    std::array<float, kAnalysisBands> c {};
    for (int b = 0; b < kAnalysisBands; ++b)
        c[(size_t) b] = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) kAnalysisBands);
    return c;
}

std::vector<MaskingClash> SessionBus::computeMasking() const
{
    auto clashes = findMasking (collectTracks(), bandCentres());
    // a clash that already has its cut in place on the covering track has been dealt with
    const juce::ScopedLock sl (lock);
    clashes.erase (std::remove_if (clashes.begin(), clashes.end(), [this] (const MaskingClash& c)
    {
        if (c.slotB < 0 || c.slotB >= kMaxInstances) return false;
        const auto& s = slots[(size_t) c.slotB];
        return s.used.load() && s.client != nullptr && s.client->cutDbNear (c.freqHz) <= -1.0f;
    }), clashes.end());
    return clashes;
}

void SessionBus::applyMaskingClash (const MaskingClash& c)
{
    const juce::ScopedLock sl (lock);
    if (c.slotB < 0 || c.slotB >= kMaxInstances) return;
    auto& s = slots[(size_t) c.slotB];
    if (s.used.load() && s.client != nullptr) s.client->applyMaskingCut (c.freqHz, c.suggestedCutDb);
}

SongKey SessionBus::findSongKey() const
{
    const juce::ScopedLock sl (lock);
    SongKey best;
    // 1. a name that states the key outright: producers label their beats ("beat 128 bpm Gmin")
    for (auto& s : slots)
    {
        if (! s.used.load() || s.client == nullptr) continue;
        int key = 0; bool minor = false;
        if (s.client->getNameKey (key, minor))
        {
            best.found = true; best.key = key; best.minor = minor; best.confidence = 1.0f; best.fromName = true; best.source = s.client->getDisplayName();
            break;
        }
    }
    // 2. what the music sounds like: the master hears everything, then the beat, then instruments; never a lone voice
    SongKey heard; float heardScore = 0.0f;
    for (auto& s : slots)
    {
        if (! s.used.load() || s.client == nullptr) continue;
        const auto role = s.client->getEffectiveRole();
        float weight = 0.0f;
        switch (role)
        {
            case Role::Master: weight = 1.0f; break;
            case Role::Beat:   weight = 0.95f; break;
            case Role::Melody: weight = 0.8f; break;
            case Role::Bass808: weight = 0.5f; break;
            default: break;
        }
        if (weight <= 0.0f) continue;
        const auto e = s.client->getMusicKey();
        if (e.secondsHeard < 6.0f) continue;
        const float score = weight * e.confidence * juce::jmin (1.0f, e.secondsHeard / 20.0f + 0.5f);
        if (score > heardScore)
        {
            heardScore = score;
            heard.found = true; heard.key = e.key; heard.minor = e.minor; heard.confidence = e.confidence; heard.source = s.client->getDisplayName();
        }
    }
    if (! best.found) return heard;
    // Keys that share nearly all their notes are easy to mistake for each other by ear (G minor and B flat major,
    // G minor and D minor), so only a clearly different answer is worth mentioning.
    auto related = [] (const SongKey& a, const SongKey& b)
    {
        const int d = ((a.key - b.key) % 12 + 12) % 12;
        if (a.minor == b.minor) return d == 0 || d == 5 || d == 7;
        const int relative = a.minor ? ((b.key - a.key) % 12 + 12) % 12 : ((a.key - b.key) % 12 + 12) % 12;      // major sits three semitones above its relative minor
        return relative == 3 || d == 0;
    };
    if (heard.found && heard.confidence > 0.75f && ! related (heard, best))
        best.note = "By ear \"" + heard.source + "\" sounds more like " + heard.text() + ", so check the name is right.";
    return best;
}

int SessionBus::sendKeyToAll (int key, bool minor)
{
    const juce::ScopedLock sl (lock);
    int tuned = 0;
    for (auto& s : slots)
    {
        if (! s.used.load() || s.client == nullptr) continue;
        s.client->setSongKey (key, minor);
        if (s.client->isTuneOn()) ++tuned;
    }
    return tuned;
}

std::vector<HotTrack> SessionBus::findHotPeaks() const
{
    const juce::ScopedLock sl (lock);
    std::vector<HotTrack> out;
    for (int i = 0; i < kMaxInstances; ++i)
    {
        auto& s = slots[(size_t) i];
        if (! s.used.load() || s.client == nullptr) continue;
        const auto role = s.client->getEffectiveRole();
        if (role == Role::Master) continue;
        const float over = s.client->getPeakOverDb();
        if (over < 0.5f) continue;
        HotTrack h;
        h.slot = i; h.name = s.client->getDisplayName(); h.role = role; h.overDb = over;
        const float integ = s.integrated.load() > -70.0f ? s.integrated.load() : s.shortTerm.load();
        h.crestDb = integ > -70.0f ? juce::jmax (0.0f, -1.0f + over - integ) : 0.0f;
        h.compressorOn = s.client->isCompressorOn();
        out.push_back (h);
    }
    std::sort (out.begin(), out.end(), [] (const HotTrack& a, const HotTrack& b) { return a.overDb > b.overDb; });
    return out;
}

juce::String SessionBus::tamePeaks (int slot)
{
    juce::String done;
    {
        const juce::ScopedLock sl (lock);
        if (slot < 0 || slot >= kMaxInstances) return {};
        auto& s = slots[(size_t) slot];
        if (! s.used.load() || s.client == nullptr) return {};
        done = s.client->tamePeaks();
    }
    // the levels have changed, so anything measured before is out of date
    const double now = juce::Time::getMillisecondCounterHiRes();
    const juce::ScopedLock sl (lock);
    resetAtMs = now + 300.0;
    settleUntilMs = now + 4500.0;
    if (done.isNotEmpty())
    {
        followUps.erase (std::remove_if (followUps.begin(), followUps.end(), [slot] (const FollowUp& f) { return f.slot == slot; }), followUps.end());
        followUps.push_back ({ slot, now + 6000.0, 0 });
    }
    return done;
}

juce::String SessionBus::takeNotice()
{
    const juce::ScopedLock sl (lock);
    const auto n = notice;
    notice.clear();
    return n;
}

float SessionBus::getMasterCorrelation() const
{
    const juce::ScopedLock sl (lock);
    for (auto& s : slots)
        if (s.used.load() && s.client != nullptr && s.client->getEffectiveRole() == Role::Master) return s.client->getStereoCorrelation();
    return 1.0f;
}

void SessionBus::setTrackLocked (int slot, bool locked)
{
    const juce::ScopedLock sl (lock);
    if (slot < 0 || slot >= kMaxInstances) return;
    auto& s = slots[(size_t) slot];
    if (s.used.load() && s.client != nullptr) s.client->setAcceptsBalance (! locked);
}

void SessionBus::setAutoBalance (bool on, BalanceTargets tg)
{
    const juce::ScopedLock sl (lock);
    autoBalance = on;
    targets = tg;
}

BalancePlan SessionBus::getLastPlan() const
{
    const juce::ScopedLock sl (lock);
    return lastPlan;
}

void SessionBus::resetAllMeasurements()
{
    const juce::ScopedLock sl (lock);
    for (auto& s : slots)
        if (s.used.load() && s.client != nullptr) s.client->resetMeasurement();
}

void SessionBus::startLearn (float seconds, BalanceTargets tg)
{
    resetAllMeasurements();
    const juce::ScopedLock sl (lock);
    targets = tg;
    learning = true;
    learnStartMs = juce::Time::getMillisecondCounterHiRes();
    learnLengthMs = juce::jmax (5.0f, seconds) * 1000.0;
    learnMessage = "Listening... play the loudest part of the song.";
    if (! isTimerRunning()) startTimer (500);
}

void SessionBus::cancelLearn()
{
    const juce::ScopedLock sl (lock);
    learning = false;
    learnMessage = "Cancelled.";
}

LearnState SessionBus::getLearnState() const
{
    const juce::ScopedLock sl (lock);
    LearnState st;
    st.active = learning;
    st.message = learnMessage;
    if (learning)
        st.progress = (float) juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - learnStartMs) / learnLengthMs);
    return st;
}

void SessionBus::setReference (const std::array<float, kAnalysisBands>& bandsDb, const juce::String& sourceName)
{
    const juce::ScopedLock sl (lock);
    referenceBands = bandsDb; referenceName = sourceName; hasReference = true;
}

bool SessionBus::getReference (std::array<float, kAnalysisBands>& out, juce::String& sourceName) const
{
    const juce::ScopedLock sl (lock);
    if (! hasReference) return false;
    out = referenceBands; sourceName = referenceName;
    return true;
}

void SessionBus::clearReference()
{
    const juce::ScopedLock sl (lock);
    hasReference = false;
}

void SessionBus::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    if (resetAtMs > 0.0 && nowMs >= resetAtMs) { resetAtMs = 0.0; resetAllMeasurements(); }
    auto plan = computePlan (targets);
    {
        const juce::ScopedLock sl (lock);
        if (nowMs < settleUntilMs)
        {
            for (auto& m : plan.moves) { m.newTrimDb -= m.deltaDb; m.deltaDb = 0.0f; }
            plan.summary = "Balanced. Measuring the result...";
        }
        lastPlan = plan;
    }

    // Tracks whose peaks were tamed a few seconds ago: if they are still hitting the top, take the next step.
    {
        std::vector<FollowUp> due;
        {
            const juce::ScopedLock sl (lock);
            for (auto it = followUps.begin(); it != followUps.end();)
                if (nowMs >= it->dueMs) { due.push_back (*it); it = followUps.erase (it); } else ++it;
        }
        for (auto& f : due)
        {
            juce::String said; bool again = false;
            {
                const juce::ScopedLock sl (lock);
                auto& s = slots[(size_t) f.slot];
                if (! s.used.load() || s.client == nullptr) continue;
                const float over = s.client->getPeakOverDb();
                if (over <= 0.5f) continue;
                const float level = s.integrated.load() > -70.0f ? s.integrated.load() : s.shortTerm.load();
                const auto name = s.client->getDisplayName();
                if (level > -14.0f)
                {
                    // compressed and still over: the track is simply too loud for any mix
                    const float drop = juce::jmin (12.0f, over + 0.5f);
                    s.client->turnOutputDown (drop);        // after the compressor, so the peaks come down by exactly this much
                    s.client->resetMeasurement();
                    said = "\"" + name + "\" was still " + juce::String (over, 1) + " dB over with its compressor on, because the track itself is too loud (" + juce::String (level, 1) + " LUFS). Its output was turned down " + juce::String (drop, 1) + " dB (Tone tab).";
                }
                else if (f.round < 2)
                {
                    s.client->tamePeaks();
                    again = true;
                    said = "\"" + name + "\" was still " + juce::String (over, 1) + " dB over, so its compressor was set a little harder.";
                }
            }
            const juce::ScopedLock sl (lock);
            if (said.isNotEmpty()) notice = said;
            if (again) followUps.push_back ({ f.slot, nowMs + 6000.0, f.round + 1 });
        }
    }

    bool finishLearn = false;
    {
        const juce::ScopedLock sl (lock);
        if (learning)
        {
            const double elapsed = juce::Time::getMillisecondCounterHiRes() - learnStartMs;
            const int secondsLeft = (int) std::ceil ((learnLengthMs - elapsed) / 1000.0);
            if (elapsed >= learnLengthMs)
            {
                learning = false;
                finishLearn = true;
            }
            else
                learnMessage = "Listening... " + juce::String (juce::jmax (0, secondsLeft)) + " s left. Keep the song playing.";
        }
    }
    if (finishLearn)
    {
        if (plan.usable)
        {
            applyPlan (plan, 1.0f);
            // A voice whose loud words tower over the rest of it will hit the top however it is balanced. That is
            // a job for its compressor, so the learn does it: lead and backing vocals only, and only if their
            // compressor is not already in use.
            juce::StringArray tamed;
            for (auto& h : findHotPeaks())
            {
                float move = 0.0f;                               // the balance has just moved this track; its peaks move with it
                for (auto& m : plan.moves) if (m.slot == h.slot) move = m.deltaDb;
                if ((h.role == Role::Vocal || h.role == Role::BackingVocal) && ! h.compressorOn && h.crestDb > 13.0f && h.overDb + move > 0.5f)
                    if (tamePeaks (h.slot).isNotEmpty()) tamed.add ("\"" + h.name + "\"");
            }
            const juce::ScopedLock sl (lock);
            learnMessage = "Balanced. " + plan.summary.replace ("Balanced. ", "");
            if (! tamed.isEmpty())
            {
                learnMessage << " The loud words on " << tamed.joinIntoString (" and ") << " were far above the rest, so the compressor was switched on there (Dynamics tab). Balancing once more in a few seconds.";
                rebalanceAtMs = nowMs + 6500.0;
            }
        }
        else
        {
            const juce::ScopedLock sl (lock);
            learnMessage = "Nothing was playing - start playback and try again.";
        }
    }
    else if (rebalanceAtMs > 0.0 && nowMs >= rebalanceAtMs)
    {
        rebalanceAtMs = 0.0;
        if (plan.usable)
        {
            applyPlan (plan, 1.0f);
            const juce::ScopedLock sl (lock);
            learnMessage = "Balanced again now the peaks are under control. " + plan.summary;
        }
    }
    else if (autoBalance && plan.usable && nowMs >= settleUntilMs && nowMs - lastAutoApplyMs > 6000.0)
    {
        // Keep balanced: every few seconds, once fresh measurements are in, take most of the remaining step.
        bool worthIt = false;
        for (auto& m : plan.moves) worthIt = worthIt || std::abs (m.deltaDb) > 0.5f;
        if (worthIt) { applyPlan (plan, 0.8f); lastAutoApplyMs = nowMs; }
    }
}
}
