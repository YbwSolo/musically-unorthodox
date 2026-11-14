#include "BalanceEngine.h"
#include <cmath>
#include <algorithm>

namespace mu
{
int rolePriority (Role r) noexcept
{
    switch (r)
    {
        case Role::Vocal:        return 100;
        case Role::Kick:         return 80;
        case Role::Bass808:      return 75;
        case Role::BackingVocal: return 60;
        case Role::Drums:        return 55;
        case Role::Beat:         return 50;
        case Role::Melody:       return 40;
        case Role::FX:           return 10;
        default:                 return 30;
    }
}

static double lufsToPower (float l) { return std::pow (10.0, (double) l / 10.0); }
static float powerToLufs (double p) { return p > 1.0e-12 ? (float) (10.0 * std::log10 (p)) : -100.0f; }

BalancePlan computeBalance (const std::vector<TrackInfo>& tracks, const BalanceTargets& t)
{
    BalancePlan plan;

    std::vector<const TrackInfo*> parts;
    const TrackInfo* master = nullptr;
    for (auto& tr : tracks)
    {
        if (tr.role == Role::Master) { master = &tr; continue; }
        if (tr.active && tr.levelLufs > -60.0f) parts.push_back (&tr);
    }
    if (master != nullptr)
    {
        plan.hasMaster = true;
        plan.measuredMasterLufs = master->levelLufs;
        plan.measuredMasterTruePeak = master->truePeakDb;
    }
    if (parts.empty())
    {
        plan.summary = "No active tracks yet - play the song so every instance can listen.";
        return plan;
    }

    // 1. Anchor: the lead vocal if present, otherwise the loudest track becomes the 0 LU reference
    //    and its role offset is subtracted so the table still holds.
    const TrackInfo* anchor = nullptr;
    for (auto* p : parts) if (p->role == Role::Vocal) { anchor = p; break; }
    if (anchor == nullptr)
        anchor = *std::max_element (parts.begin(), parts.end(), [] (auto* a, auto* b) { return a->levelLufs < b->levelLufs; });
    const float anchorOffset = t.offsetFor (anchor->role);

    // 2. Desired level of each track relative to the anchor's current level.
    std::vector<float> desired (parts.size());
    for (size_t i = 0; i < parts.size(); ++i)
        desired[i] = anchor->levelLufs + (t.offsetFor (parts[i]->role) - anchorOffset);

    // 3. Global shift so the projected master lands on the target. Projected master is derived from the
    //    measured master when there is one (keeps bus processing / correlation in the estimate),
    //    otherwise from the power sum of the parts.
    double powBefore = 0.0, powAfter = 0.0;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        powBefore += lufsToPower (parts[i]->levelLufs);
        powAfter  += lufsToPower (desired[i]);
    }
    const float relChange = powerToLufs (powAfter) - powerToLufs (powBefore);
    const float masterNow = (master != nullptr && master->levelLufs > -60.0f) ? master->levelLufs : powerToLufs (powBefore);
    float shift = t.masterLufs - (masterNow + relChange);

    // True-peak guard: if the master would exceed the ceiling, give up some loudness instead.
    if (master != nullptr && master->truePeakDb > -60.0f)
    {
        const float projectedTp = master->truePeakDb + relChange + shift;
        if (projectedTp > t.masterTruePeak)
            shift -= (projectedTp - t.masterTruePeak);
    }

    // Channel headroom: a track whose own peaks would end up over the top of its channel is not a job for a
    // harder compressor, it means the song as a whole is being asked to sit too high. Everything comes down
    // together by what that track needs, so the balance is kept and nothing is squashed to fit.
    for (size_t i = 0; i < parts.size(); ++i)
    {
        auto* p = parts[i];
        if (! p->acceptsBalance) continue;
        const float peakNow = juce::jmax (p->truePeakDb, p->peakOverDb > 0.0f ? -1.0f + p->peakOverDb : -100.0f);
        if (peakNow < -60.0f) continue;
        const float after = peakNow + (desired[i] + shift - p->levelLufs);
        const float need = juce::jmin (9.0f, after - t.trackTruePeak);
        if (need > plan.headroomDb + 0.05f) { plan.headroomDb = need; plan.headroomTrack = p->name; }
    }
    shift -= plan.headroomDb;

    // 4. Moves, clamped. The clamp is per track so one wild track cannot drag the session.
    float maxAbs = 0.0f;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        auto* p = parts[i];
        float delta = desired[i] + shift - p->levelLufs;
        delta = juce::jlimit (-t.maxMoveDb, t.maxMoveDb, delta);
        if (! p->acceptsBalance) delta = 0.0f;
        float newTrim = juce::jlimit (-24.0f, 24.0f, p->trimDb + delta);
        delta = newTrim - p->trimDb;
        if (std::abs (delta) < 0.1f) delta = 0.0f, newTrim = p->trimDb;
        maxAbs = juce::jmax (maxAbs, std::abs (delta));

        BalanceMove m;
        m.slot = p->slot; m.name = p->name; m.deltaDb = delta; m.newTrimDb = newTrim;
        if (! p->acceptsBalance) m.note = "locked";
        else if (delta > 0.05f) m.note = "bring up";
        else if (delta < -0.05f) m.note = "pull down";
        else m.note = "sits right";
        plan.moves.push_back (m);
    }

    double powFinal = 0.0;
    for (size_t i = 0; i < parts.size(); ++i)
        powFinal += lufsToPower (parts[i]->levelLufs + plan.moves[i].deltaDb);
    const float finalChange = powerToLufs (powFinal) - powerToLufs (powBefore);
    plan.projectedMasterLufs = masterNow + finalChange;
    plan.projectedMasterTruePeak = (master != nullptr && master->truePeakDb > -60.0f) ? master->truePeakDb + finalChange : -100.0f;
    plan.usable = true;

    juce::String s;
    s << parts.size() << " track" << (parts.size() == 1 ? "" : "s") << " analysed, anchored on " << anchor->name << ". ";
    s << "Projected master " << juce::String (plan.projectedMasterLufs, 1) << " LUFS";
    if (plan.projectedMasterTruePeak > -60.0f) s << " / " << juce::String (plan.projectedMasterTruePeak, 1) << " dBTP";
    s << " (target " << juce::String (t.masterLufs, 1) << " LUFS, " << juce::String (t.masterTruePeak, 1) << " dBTP).";
    if (plan.headroomDb > 0.3f)
        s << " The whole song sits " << juce::String (plan.headroomDb, 1) << " dB lower than the target so the peaks of \"" << plan.headroomTrack << "\" have room; the master makes the loudness back up.";
    if (maxAbs >= t.maxMoveDb - 0.01f) s << " Some moves hit the " << juce::String (juce::roundToInt (t.maxMoveDb)) << " dB limit - check those tracks' own levels.";
    plan.summary = s;
    return plan;
}

std::vector<MaskingClash> findMasking (const std::vector<TrackInfo>& tracks, const std::array<float, kAnalysisBands>& centres)
{
    std::vector<MaskingClash> out;
    std::vector<const TrackInfo*> parts;
    for (auto& t : tracks)
        if (t.role != Role::Master && t.active && t.hasBands) parts.push_back (&t);

    // Normalise each track's band curve to its own peak so we compare shape, then weight by level.
    for (size_t a = 0; a < parts.size(); ++a)
    {
        for (size_t b = a + 1; b < parts.size(); ++b)
        {
            const TrackInfo* hi = parts[a];
            const TrackInfo* lo = parts[b];
            if (rolePriority (lo->role) > rolePriority (hi->role)) std::swap (hi, lo);
            if (rolePriority (hi->role) == rolePriority (lo->role)) continue;

            float hiMax = -200.0f, loMax = -200.0f;
            for (int i = 0; i < kAnalysisBands; ++i)
            {
                hiMax = juce::jmax (hiMax, hi->bandsDb[(size_t) i]);
                loMax = juce::jmax (loMax, lo->bandsDb[(size_t) i]);
            }
            if (hiMax < -90.0f || loMax < -90.0f) continue;

            // Look for bands where the lower-priority track is near its own peak AND louder than the
            // priority track in absolute terms inside the priority track's important region.
            // Where the priority track needs to be heard. A voice is understood between about 250 Hz and 6 kHz;
            // whatever a raw vocal has under that is rumble and plosives, and nothing should be cut out of a beat
            // to make room for those.
            const bool voice = hi->role == Role::Vocal || hi->role == Role::BackingVocal;
            const float needLo = voice ? 250.0f : 60.0f, needHi = voice ? 6000.0f : 12000.0f;
            int bestBand = -1; float bestScore = 0.0f;
            for (int i = 2; i < kAnalysisBands - 2; ++i)
            {
                if (centres[(size_t) i] < needLo || centres[(size_t) i] > needHi) continue;
                const float hiRel = hi->bandsDb[(size_t) i] - hiMax;     // <= 0
                const float loRel = lo->bandsDb[(size_t) i] - loMax;
                if (hiRel < -18.0f) continue;                              // not important to the priority track
                // (the band levels are measured after each track's fader, so they are compared as they are)
                const float absDiff = lo->bandsDb[(size_t) i] - hi->bandsDb[(size_t) i];
                const float score = absDiff + loRel * 0.5f + 6.0f;       // > 0 means the lower track is sitting on top
                if (score > bestScore) { bestScore = score; bestBand = i; }
            }
            if (bestBand >= 0 && bestScore > 3.0f)
            {
                MaskingClash c;
                c.slotA = hi->slot; c.slotB = lo->slot; c.nameA = hi->name; c.nameB = lo->name;
                c.freqHz = centres[(size_t) bestBand];
                c.severityDb = bestScore;
                c.suggestedCutDb = -juce::jlimit (1.5f, 4.0f, bestScore * 0.4f);
                out.push_back (c);
            }
        }
    }
    std::sort (out.begin(), out.end(), [] (const MaskingClash& x, const MaskingClash& y) { return x.severityDb > y.severityDb; });
    if (out.size() > 8) out.resize (8);
    return out;
}
}
