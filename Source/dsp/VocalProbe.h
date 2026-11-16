#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace mu
{
// What a finished vocal is measured by, in the same terms the reference records' vocals were measured in.
struct VocalFacts
{
    float seconds = 0.0f;           // of singing or rapping heard (silence between phrases is not counted)
    float typicalDb = -100.0f;      // the usual level of a 50 ms step while the voice is going
    float peakSpreadDb = 0.0f;      // how far the loudest syllables (1 step in 100) stand above that
    float sideDb = -100.0f;         // the sides against the middle while the voice is going (-100 = mono)
    float bandSideDb = -100.0f;     // the same between 800 Hz and 5 kHz, where the width of a beat is heard
    float essDb = -100.0f;          // the loudest esses (5 to 10 kHz, 1 step in 20) against the loud vowels (300 Hz to 3 kHz)
    bool enough() const { return seconds >= 6.0f; }
};

// Collects those figures from a track's output while it plays. The audio thread only adds to counters; the
// message thread reads them whenever it likes.
class VocalProbe
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        stepLen = juce::jmax (1, (int) (fs * 0.025));
        auto hp = [this] (float hz) { return std::exp (-juce::MathConstants<float>::twoPi * hz / (float) fs); };
        aHiHp = hp (5000.0f); aHiLp = hp (10000.0f); aBdHp = hp (300.0f); aBdLp = hp (3000.0f); aWdHp = hp (800.0f); aWdLp = hp (5000.0f);
        clear();
    }
    void start() { clear(); running.store (true); }
    void stop()  { running.store (false); }
    bool isRunning() const { return running.load (std::memory_order_relaxed); }

    void process (const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (! running.load (std::memory_order_relaxed)) return;
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        if (ch == 0) return;
        const float* l = buffer.getReadPointer (0);
        const float* r = ch > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
            midSum += (double) m * m; sideSum += (double) s * s;
            // two plain band splits (one pole each side): the esses and the body of the voice
            // (the side that has to keep the other band out is two poles, the outer side one)
            hiLp1 += (1.0f - aHiLp) * (m - hiLp1); hiHp1 += (1.0f - aHiHp) * (hiLp1 - hiHp1); const float h1 = hiLp1 - hiHp1;
            hiHp2 += (1.0f - aHiHp) * (h1 - hiHp2); const float hi = h1 - hiHp2;
            bdLp1 += (1.0f - aBdLp) * (m - bdLp1); bdLp2 += (1.0f - aBdLp) * (bdLp1 - bdLp2); bdHp1 += (1.0f - aBdHp) * (bdLp2 - bdHp1); const float bd = bdLp2 - bdHp1;
            hiSum += (double) hi * hi; bdSum += (double) bd * bd;
            wmLp += (1.0f - aWdLp) * (m - wmLp); wmHp += (1.0f - aWdHp) * (wmLp - wmHp); const float wm = wmLp - wmHp;
            wsLp += (1.0f - aWdLp) * (s - wsLp); wsHp += (1.0f - aWdHp) * (wsLp - wsHp); const float ws = wsLp - wsHp;
            wMidSum += (double) wm * wm; wSideSum += (double) ws * ws;
            if (++count >= stepLen) endStep();
        }
    }

    VocalFacts read() const
    {
        VocalFacts f;
        std::array<int, kBins> lev {};
        int total = 0;
        for (int b = 0; b < kBins; ++b) { lev[(size_t) b] = level[(size_t) b].load (std::memory_order_relaxed); total += lev[(size_t) b]; }
        if (total < 8) return f;
        // the voice is going wherever the level is within 25 dB of its loud steps
        const int loud = percentileBin (lev, total, 0.90f), from = juce::jmax (0, loud - 50);
        int active = 0; double mid = 0.0, side = 0.0, wMid = 0.0, wSide = 0.0;
        std::array<int, kBins> act {};
        for (int b = from; b < kBins; ++b)
        {
            act[(size_t) b] = lev[(size_t) b]; active += lev[(size_t) b];
            mid += midPow[(size_t) b].load (std::memory_order_relaxed); side += sidePow[(size_t) b].load (std::memory_order_relaxed);
            wMid += wMidPow[(size_t) b].load (std::memory_order_relaxed); wSide += wSidePow[(size_t) b].load (std::memory_order_relaxed);
        }
        if (active < 8) return f;
        f.seconds = (float) active * (float) stepLen * 2.0f / (float) fs;
        f.typicalDb = binDb (percentileBin (act, active, 0.50f));
        f.peakSpreadDb = binDb (percentileBin (act, active, 0.99f)) - f.typicalDb;
        f.sideDb = (side > 0.0 && mid > 0.0) ? juce::jmax (-100.0f, (float) (10.0 * std::log10 (side / mid))) : -100.0f;
        f.bandSideDb = (wSide > 0.0 && wMid > 0.0) ? juce::jmax (-100.0f, (float) (10.0 * std::log10 (wSide / wMid))) : -100.0f;
        std::array<int, kBins> hi {}, bd {};
        int nh = 0, nb = 0;
        for (int b = 0; b < kBins; ++b) { hi[(size_t) b] = ess[(size_t) b].load (std::memory_order_relaxed); nh += hi[(size_t) b]; bd[(size_t) b] = body[(size_t) b].load (std::memory_order_relaxed); nb += bd[(size_t) b]; }
        if (nh >= 40 && nb >= 40) f.essDb = binDb (percentileBin (hi, nh, 0.95f)) - binDb (percentileBin (bd, nb, 0.75f));
        return f;
    }

private:
    static constexpr int kBins = 240;                                   // -120 to 0 dB in half-dB steps
    static int toBin (double power) { return juce::jlimit (0, kBins - 1, (int) std::lround ((10.0 * std::log10 (power + 1.0e-14) + 120.0) * 2.0)); }
    static float binDb (int bin) { return (float) bin * 0.5f - 120.0f; }
    static int percentileBin (const std::array<int, kBins>& h, int total, float share)
    {
        const int want = juce::jmax (1, (int) std::ceil (share * (float) total));
        int seen = 0;
        for (int b = 0; b < kBins; ++b) { seen += h[(size_t) b]; if (seen >= want) return b; }
        return kBins - 1;
    }
    void clear()
    {
        for (int b = 0; b < kBins; ++b) { level[(size_t) b].store (0); ess[(size_t) b].store (0); body[(size_t) b].store (0); midPow[(size_t) b].store (0.0); sidePow[(size_t) b].store (0.0); wMidPow[(size_t) b].store (0.0); wSidePow[(size_t) b].store (0.0); }
        midSum = sideSum = hiSum = bdSum = wMidSum = wSideSum = 0.0; pairMid = pairSide = pairWMid = pairWSide = 0.0; count = 0; half = false; usual = -100.0f;
        wmLp = wmHp = wsLp = wsHp = 0.0f;
        hiLp1 = hiHp1 = hiHp2 = bdLp1 = bdLp2 = bdHp1 = 0.0f;
    }
    // every 25 ms: the esses and the body; every second one of those (50 ms): the level and the width
    void endStep() noexcept
    {
        const double inv = 1.0 / (double) count;
        const double m = midSum * inv, s = sideSum * inv;
        const float nowDb = (float) (10.0 * std::log10 (m + 1.0e-14));
        if (usual > -99.0f && nowDb > usual - 12.0f)
        {
            const int h = toBin (hiSum * inv), b = toBin (bdSum * inv);
            ess[(size_t) h].fetch_add (1, std::memory_order_relaxed); body[(size_t) b].fetch_add (1, std::memory_order_relaxed);
        }
        pairMid += m; pairSide += s; pairWMid += wMidSum * inv; pairWSide += wSideSum * inv;
        if (half)
        {
            const double pm = 0.5 * pairMid, ps = 0.5 * pairSide;
            const float db = (float) (10.0 * std::log10 (pm + 1.0e-14));
            if (db > -70.0f)
            {
                const int b = toBin (pm);
                level[(size_t) b].fetch_add (1, std::memory_order_relaxed);
                midPow[(size_t) b].store (midPow[(size_t) b].load (std::memory_order_relaxed) + pm, std::memory_order_relaxed);
                sidePow[(size_t) b].store (sidePow[(size_t) b].load (std::memory_order_relaxed) + ps, std::memory_order_relaxed);
                wMidPow[(size_t) b].store (wMidPow[(size_t) b].load (std::memory_order_relaxed) + 0.5 * pairWMid, std::memory_order_relaxed);
                wSidePow[(size_t) b].store (wSidePow[(size_t) b].load (std::memory_order_relaxed) + 0.5 * pairWSide, std::memory_order_relaxed);
                if (usual < -99.0f) usual = db; else if (db > usual - 25.0f) usual += 0.1f * (db - usual);
            }
            pairMid = pairSide = pairWMid = pairWSide = 0.0;
        }
        half = ! half;
        midSum = sideSum = hiSum = bdSum = wMidSum = wSideSum = 0.0; count = 0;
    }

    double fs = 48000.0;
    int stepLen = 1200, count = 0;
    bool half = false;
    float aHiHp = 0.0f, aHiLp = 0.0f, aBdHp = 0.0f, aBdLp = 0.0f, hiLp1 = 0.0f, hiHp1 = 0.0f, hiHp2 = 0.0f, bdLp1 = 0.0f, bdLp2 = 0.0f, bdHp1 = 0.0f, usual = -100.0f;
    double midSum = 0.0, sideSum = 0.0, hiSum = 0.0, bdSum = 0.0, wMidSum = 0.0, wSideSum = 0.0, pairMid = 0.0, pairSide = 0.0, pairWMid = 0.0, pairWSide = 0.0;
    float aWdHp = 0.0f, aWdLp = 0.0f, wmLp = 0.0f, wmHp = 0.0f, wsLp = 0.0f, wsHp = 0.0f;
    std::atomic<bool> running { false };
    std::array<std::atomic<int>, kBins> level {}, ess {}, body {};
    std::array<std::atomic<double>, kBins> midPow {}, sidePow {}, wMidPow {}, wSidePow {};
};
}
