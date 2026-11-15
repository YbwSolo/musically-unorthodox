#include "MultibandComp.h"
#include <cmath>

namespace mu
{
void MultibandComp::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    designedLow = designedHigh = -1.0f;
    design();
    reset();
}

void MultibandComp::reset()
{
    for (auto& s : splitLow) s.reset();
    for (auto& s : splitHigh) s.reset();
    for (auto& s : alignLow) s.reset();
    envDb.fill (-120.0f); gainDb.fill (0.0f); power.fill (0.0f);
    for (auto& m : grMeter) m.store (0.0f);
    wet = settings.enabled ? 1.0f : 0.0f;
}

void MultibandComp::design()
{
    const float lo = juce::jlimit (40.0f, 1000.0f, settings.crossLowHz);
    const float hi = juce::jlimit (lo * 2.0f, (float) (fs * 0.4), settings.crossHighHz);
    if (std::abs (lo - designedLow) < 0.01f && std::abs (hi - designedHigh) < 0.01f) return;
    designedLow = lo; designedHigh = hi;
    // a Butterworth low-pass or high-pass section (Q 0.7071); two in a row make the Linkwitz-Riley slope
    auto section = [this] (float hz, bool high)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * (double) hz / fs, c = std::cos (w), alpha = std::sin (w) / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        BiquadCoeffs k;
        if (high) { k[0] = (float) ((1.0 + c) * 0.5 / a0); k[1] = (float) (-(1.0 + c) / a0); k[2] = k[0]; }
        else      { k[0] = (float) ((1.0 - c) * 0.5 / a0); k[1] = (float) ((1.0 - c) / a0);  k[2] = k[0]; }
        k[3] = (float) (-2.0 * c / a0); k[4] = (float) ((1.0 - alpha) / a0);
        return k;
    };
    lpLow = section (lo, false); hpLow = section (lo, true); lpHigh = section (hi, false); hpHigh = section (hi, true);
}

float MultibandComp::gainFor (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float over = levelDb - thresholdDb, slope = 1.0f / juce::jmax (1.0f, ratio) - 1.0f;
    if (kneeDb > 0.01f && std::abs (over) < kneeDb * 0.5f) { const float x = over + kneeDb * 0.5f; return slope * x * x / (2.0f * kneeDb); }
    return over > 0.0f ? slope * over : 0.0f;
}

void MultibandComp::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;
    if (! settings.enabled && wet <= 0.0f) { for (auto& m : grMeter) m.store (0.0f, std::memory_order_relaxed); return; }
    if (settings.enabled && wet <= 0.0f) { for (auto& s : splitLow) s.reset(); for (auto& s : splitHigh) s.reset(); for (auto& s : alignLow) s.reset(); envDb.fill (-120.0f); gainDb.fill (0.0f); power.fill (0.0f); }
    design();

    const float wetStep = 1.0f / (float) (fs * 0.010), wetGoal = settings.enabled ? 1.0f : 0.0f;
    std::array<float, 3> att {}, rel {}, makeup {};
    for (int b = 0; b < 3; ++b)
    {
        att[(size_t) b] = 1.0f - std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (0.1f, settings.band[(size_t) b].attackMs)));
        rel[(size_t) b] = 1.0f - std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (5.0f, settings.band[(size_t) b].releaseMs)));
        makeup[(size_t) b] = settings.band[(size_t) b].on ? settings.band[(size_t) b].makeupDb : 0.0f;
    }
    std::array<float, 3> deepest { 0.0f, 0.0f, 0.0f };
    float* data[2] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float bands[2][3] {};
        std::array<float, 3> peak { 0.0f, 0.0f, 0.0f };
        for (int c = 0; c < ch; ++c)
        {
            const float x = data[c][i];
            auto& sl = splitLow[(size_t) c]; auto& sh = splitHigh[(size_t) c]; auto& al = alignLow[(size_t) c];
            const float low = sl.lp[1].process (sl.lp[0].process (x, lpLow), lpLow);
            const float rest = sl.hp[1].process (sl.hp[0].process (x, hpLow), hpLow);
            const float mid = sh.lp[1].process (sh.lp[0].process (rest, lpHigh), lpHigh);
            const float high = sh.hp[1].process (sh.hp[0].process (rest, hpHigh), hpHigh);
            // the low band through the same phase turn the other two got at the upper crossover
            const float lowAligned = al.lp[1].process (al.lp[0].process (low, lpHigh), lpHigh) + al.hp[1].process (al.hp[0].process (low, hpHigh), hpHigh);
            bands[c][0] = lowAligned; bands[c][1] = mid; bands[c][2] = high;
            for (int b = 0; b < 3; ++b) peak[(size_t) b] = juce::jmax (peak[(size_t) b], bands[c][b] * bands[c][b]);
        }
        float g[3];
        for (int b = 0; b < 3; ++b)
        {
            // The level of the band is its average power (what a level meter shows for a steady note: 3 dB under the
            // peaks of a sine), rising at the attack time and falling at the release time. Both channels move
            // together, on whichever is louder, so the stereo picture does not shift.
            power[(size_t) b] += (peak[(size_t) b] - power[(size_t) b]) * (peak[(size_t) b] > power[(size_t) b] ? att[(size_t) b] : rel[(size_t) b]);
            envDb[(size_t) b] = 10.0f * std::log10 (power[(size_t) b] + 1.0e-12f);
            const auto& bs = settings.band[(size_t) b];
            const float want = bs.on ? gainFor (envDb[(size_t) b], bs.thresholdDb, bs.ratio, settings.kneeDb) : 0.0f;
            gainDb[(size_t) b] += (want - gainDb[(size_t) b]) * 0.02f;                 // takes the steps out of the gain itself
            deepest[(size_t) b] = juce::jmin (deepest[(size_t) b], gainDb[(size_t) b]);
            g[b] = std::pow (10.0f, (gainDb[(size_t) b] + makeup[(size_t) b]) * 0.05f);
        }
        wet = wetGoal > wet ? juce::jmin (wetGoal, wet + wetStep) : juce::jmax (wetGoal, wet - wetStep);
        for (int c = 0; c < ch; ++c)
        {
            const float out = bands[c][0] * g[0] + bands[c][1] * g[1] + bands[c][2] * g[2];
            data[c][i] = wet >= 1.0f ? out : data[c][i] + wet * (out - data[c][i]);
        }
    }
    for (int b = 0; b < 3; ++b) grMeter[(size_t) b].store (deepest[(size_t) b], std::memory_order_relaxed);
}
}
