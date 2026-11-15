#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "EQEngine.h"

namespace mu
{
struct MultibandSettings
{
    static constexpr int kBands = 3;
    bool enabled = false;
    float crossLowHz = 140.0f, crossHighHz = 3200.0f;        // where the low band ends and where the high band begins
    struct Band { bool on = true; float thresholdDb = -24.0f, ratio = 2.0f, attackMs = 20.0f, releaseMs = 150.0f, makeupDb = 0.0f; };
    std::array<Band, kBands> band;
    float kneeDb = 6.0f;
};

// Three-band compressor for the master: the low end, the middle and the top are each held steady on their own,
// so a bass that jumps about does not make the compressor pull the vocal down with it.
// The bands are split with fourth-order Linkwitz-Riley filters (two Butterworth sections in a row), and the low
// band goes through the second crossover's all-pass, so with no compression the three bands add back up to the
// sound that went in at every frequency (the phase is turned, as with any crossover of this kind, the level is
// not). No latency. Switched off, the sound does not go through the crossovers at all.
class MultibandComp
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const MultibandSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getReductionDb (int band) const noexcept { return grMeter[(size_t) band].load (std::memory_order_relaxed); }    // 0 or negative

    static float gainFor (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;       // dB of gain change for a level

private:
    struct Split
    {
        std::array<Biquad, 2> lp, hp;
        void reset() noexcept { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }
    };
    void design();

    double fs = 48000.0;
    int channels = 2;
    MultibandSettings settings;
    float designedLow = -1.0f, designedHigh = -1.0f;
    BiquadCoeffs lpLow {}, hpLow {}, lpHigh {}, hpHigh {};
    std::array<Split, 2> splitLow, splitHigh, alignLow;            // per channel; alignLow = the low band through the upper crossover's all-pass
    std::array<float, MultibandSettings::kBands> envDb { -120.0f, -120.0f, -120.0f }, gainDb {}, power {};
    std::array<std::atomic<float>, MultibandSettings::kBands> grMeter {};
    float wet = 0.0f;                                             // 0 = not in the path, 1 = fully in: moved over 10 ms so switching does not click
};
}
