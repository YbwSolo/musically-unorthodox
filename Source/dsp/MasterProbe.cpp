#include "MasterProbe.h"
#include <algorithm>
#include <cmath>

namespace mu
{
void MasterProbe::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    auto lp = [this] (float hz) { return EQEngine::computeCoefficients ({ true, EQType::HighCut, hz, 0.0f, 0.71f }, fs); };
    auto hp = [this] (float hz) { return EQEngine::computeCoefficients ({ true, EQType::LowCut, hz, 0.0f, 0.71f }, fs); };
    lp150 = lp (150.0f); hp150 = hp (150.0f); lp800 = lp (800.0f); hp800 = hp (800.0f); lp5k = lp (5000.0f); hp5k = hp (5000.0f); hp40 = hp (40.0f); lp120 = lp (120.0f);
    winLen = (int) (fs * 0.4); subLen = (int) (fs * 0.05);
    lowDb.reserve (kMaxWindows); levelDb.reserve (kMaxWindows); shortTerm.reserve (kMaxLoudness);
    start(); running.store (false);
}

void MasterProbe::start()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    total = {}; lowDb.clear(); levelDb.clear(); shortTerm.clear();
    restart.fetch_add (1);
    running.store (true);
}

void MasterProbe::merge() noexcept
{
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked()) return;                       // the reader has it: keep the block and hand it over next time
    total.ll += pending.ll; total.rr += pending.rr; total.lr += pending.lr; total.mono += pending.mono;
    for (int b = 0; b < 4; ++b) { total.mm[(size_t) b] += pending.mm[(size_t) b]; total.ss[(size_t) b] += pending.ss[(size_t) b]; }
    total.samples += pending.samples; total.activeWindows += pending.activeWindows; total.punchSum += pending.punchSum;
    total.peak = juce::jmax (total.peak, pending.peak); total.flatTops += pending.flatTops;
    total.lowestCorr = juce::jmin (total.lowestCorr, pending.lowestCorr);
    total.maxShort = juce::jmax (total.maxShort, pending.maxShort); total.maxMomentary = juce::jmax (total.maxMomentary, pending.maxMomentary);
    for (int i = 0; i < pendWindows; ++i)
        if ((int) lowDb.size() < kMaxWindows) { lowDb.push_back (pendLow[(size_t) i]); levelDb.push_back (pendLevel[(size_t) i]); }     // room was reserved: no allocation
    for (int i = 0; i < pendLoudCount; ++i) if ((int) shortTerm.size() < kMaxLoudness) shortTerm.push_back (pendLoud[(size_t) i]);
    pending = {}; pendWindows = 0; pendLoudCount = 0;
}

void MasterProbe::process (const juce::AudioBuffer<float>& buffer, float shortLufs, float momentaryLufs) noexcept
{
    if (! running.load (std::memory_order_relaxed)) return;
    if (const int r = restart.load (std::memory_order_relaxed); r != seenRestart)
    {
        seenRestart = r; pending = {}; pendWindows = 0; pendLoudCount = 0;
        winPos = subPos = winSubs = runLength = loudTick = 0; winSq = winLowSq = winLL = winRR = winLR = 0.0; subPeak = winPeakSum = 0.0f;
        midF = {}; sideF = {}; lowA.reset(); lowB.reset();
    }
    const int n = buffer.getNumSamples();
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getNumChannels() > 1 && channels > 1 ? buffer.getReadPointer (1) : l;
    for (int i = 0; i < n; ++i)
    {
        const float a = l[i], b = r[i], mid = 0.5f * (a + b), side = 0.5f * (a - b);
        // the four ranges of the middle and of the sides
        auto ranges = [this] (Chain& f, float x, std::array<double, 4>& sums)
        {
            const float b0 = f.a.process (x, lp150);
            const float b1 = f.c.process (f.b.process (x, hp150), lp800);
            const float b2 = f.e.process (f.d.process (x, hp800), lp5k);
            const float b3 = f.f.process (x, hp5k);
            sums[0] += (double) b0 * b0; sums[1] += (double) b1 * b1; sums[2] += (double) b2 * b2; sums[3] += (double) b3 * b3;
        };
        ranges (midF, mid, pending.mm); ranges (sideF, side, pending.ss);
        const float low = lowB.process (lowA.process (mid, hp40), lp120);
        pending.ll += (double) a * a; pending.rr += (double) b * b; pending.lr += (double) a * b; pending.mono += (double) mid * mid;
        winLL += (double) a * a; winRR += (double) b * b; winLR += (double) a * b;
        winSq += 0.5 * ((double) a * a + (double) b * b); winLowSq += (double) low * low;
        const float top = juce::jmax (std::abs (a), std::abs (b));
        pending.peak = juce::jmax (pending.peak, top);
        subPeak = juce::jmax (subPeak, top);
        // a run of samples stuck at the same top value is a clipped peak
        if (top >= 0.985f && std::abs (top - lastSample) < 1.0e-4f) { if (++runLength == 2) ++pending.flatTops; } else runLength = 0;
        lastSample = top;
        if (++subPos >= subLen) { subPos = 0; winPeakSum += subPeak; subPeak = 0.0f; ++winSubs; }
        if (++winPos >= winLen)
        {
            const double rms = std::sqrt (winSq / (double) winLen);
            if (rms > 1.0e-3)                                                  // -60 dBFS: quieter than that is a gap, not music
            {
                pending.samples += (double) winLen; pending.activeWindows += 1.0;
                if (winSubs > 0) pending.punchSum += 20.0 * std::log10 (juce::jmax (1.0e-9, (double) winPeakSum / (double) winSubs) / rms);
                const double denom = std::sqrt (winLL * winRR);
                if (denom > 1.0e-12) pending.lowestCorr = juce::jmin (pending.lowestCorr, (float) (winLR / denom));
                if (pendWindows < (int) pendLow.size())
                {
                    pendLow[(size_t) pendWindows] = (float) (10.0 * std::log10 (winLowSq / (double) winLen + 1.0e-12));
                    pendLevel[(size_t) pendWindows] = (float) (20.0 * std::log10 (rms));
                    ++pendWindows;
                }
            }
            winPos = 0; winSq = winLowSq = winLL = winRR = winLR = 0.0; winPeakSum = 0.0f; winSubs = 0;
        }
    }
    pending.maxShort = juce::jmax (pending.maxShort, shortLufs); pending.maxMomentary = juce::jmax (pending.maxMomentary, momentaryLufs);
    loudTick += n;
    if (loudTick >= (int) fs) { loudTick = 0; if (shortLufs > -70.0f && pendLoudCount < (int) pendLoud.size()) pendLoud[(size_t) pendLoudCount++] = shortLufs; }
    merge();
}

float MasterProbe::secondsHeard() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return (float) (total.samples / fs);
}

ProbeFacts MasterProbe::read() const
{
    Totals t; std::vector<float> low, level, loud;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        t = total; low = lowDb; level = levelDb; loud = shortTerm;
    }
    ProbeFacts f;
    f.seconds = (float) (t.samples / fs);
    if (t.samples < 1.0) return f;
    auto db10 = [] (double x) { return (float) (10.0 * std::log10 (juce::jmax (1.0e-12, x))); };
    f.samplePeakDb = 20.0f * std::log10 (juce::jmax (1.0e-7f, t.peak));
    const double all = t.ll + t.rr;                    // includes the gaps, so normalise by everything that was added
    double midAll = 0.0, sideAll = 0.0;
    for (int b = 0; b < 4; ++b) { midAll += t.mm[(size_t) b]; sideAll += t.ss[(size_t) b]; f.sideToMidDb[(size_t) b] = db10 (t.ss[(size_t) b] / juce::jmax (1.0e-12, t.mm[(size_t) b])); }
    f.widthDb = db10 (sideAll / juce::jmax (1.0e-12, midAll));
    f.correlation = (float) (t.lr / juce::jmax (1.0e-12, std::sqrt (t.ll * t.rr)));
    f.lowestCorrelation = t.lowestCorr;
    f.monoLossDb = db10 (t.mono / juce::jmax (1.0e-12, 0.5 * all));
    f.flatTops = t.flatTops;
    f.punchDb = t.activeWindows > 0.0 ? (float) (t.punchSum / t.activeWindows) : 0.0f;
    // spread of a level over the song: from its 10th to its 95th percentile, the way loudness range is read
    auto spread = [] (std::vector<float> v)
    {
        if (v.size() < 8) return 0.0f;
        std::sort (v.begin(), v.end());
        return v[v.size() * 95 / 100] - v[v.size() / 10];
    };
    if (! level.empty())
    {
        double sum = 0.0; for (auto x : level) sum += std::pow (10.0, x / 10.0);
        f.rmsDb = db10 (sum / (double) level.size());
        f.crestDb = f.samplePeakDb - f.rmsDb;
        // only the windows within 20 dB of the loud ones count, so an intro or a breakdown is not called "swing"
        std::vector<float> sortedLevel = level; std::sort (sortedLevel.begin(), sortedLevel.end());
        const float gate = sortedLevel[sortedLevel.size() * 9 / 10] - 20.0f;
        std::vector<float> lowKept, levelKept;
        for (size_t i = 0; i < level.size(); ++i) if (level[i] > gate) { lowKept.push_back (low[i]); levelKept.push_back (level[i]); }
        f.lowSwingDb = spread (lowKept); f.levelSwingDb = spread (levelKept);
        if (lowKept.size() >= 8) { std::sort (lowKept.begin(), lowKept.end()); f.lowTypicalDb = lowKept[lowKept.size() / 2]; f.lowLoudDb = lowKept[lowKept.size() * 9 / 10]; }
    }
    // loudness range (EBU Tech 3342): the spread of the short-term loudness, with silence and anything more than
    // 20 LU under the average left out
    if (loud.size() >= 6)
    {
        double sum = 0.0; int count = 0;
        for (auto x : loud) if (x > -70.0f) { sum += std::pow (10.0, x / 10.0); ++count; }
        if (count > 0)
        {
            const float gate = db10 (sum / count) - 20.0f;
            std::vector<float> kept; for (auto x : loud) if (x > gate) kept.push_back (x);
            f.loudnessRangeLu = spread (kept);
        }
    }
    f.maxShortTermLufs = t.maxShort; f.maxMomentaryLufs = t.maxMomentary;
    return f;
}
}
