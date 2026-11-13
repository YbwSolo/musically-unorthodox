#include "SessionBus.h"
#include <cmath>

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
    return findMasking (collectTracks(), bandCentres());
}

void SessionBus::applyMaskingClash (const MaskingClash& c)
{
    const juce::ScopedLock sl (lock);
    if (c.slotB < 0 || c.slotB >= kMaxInstances) return;
    auto& s = slots[(size_t) c.slotB];
    if (s.used.load() && s.client != nullptr) s.client->applyMaskingCut (c.freqHz, c.suggestedCutDb);
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

void SessionBus::timerCallback()
{
    auto plan = computePlan (targets);
    {
        const juce::ScopedLock sl (lock);
        lastPlan = plan;
    }
    if (autoBalance && plan.usable)
        applyPlan (plan, 0.25f);   // glide towards the plan rather than jumping
}
}
