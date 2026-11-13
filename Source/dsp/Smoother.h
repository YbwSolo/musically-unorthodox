#pragma once
#include "EQEngine.h"

namespace mu
{
struct SmootherSettings
{
    bool enabled = false;
    float depth = 0.4f;      // 0..1
    int focus = 0;           // 0 full range, 1 low mids, 2 harsh highs
    int speed = 1;           // 0 slow, 1 normal, 2 fast
};

// Resonance tamer. Twenty-four quarter-octave bands from 200 Hz to 12.8 kHz are watched continuously;
// any band that pokes out above its neighbours gets a matching narrow cut for as long as it pokes out.
// Flat material passes untouched because nothing stands out; ringing, honky or piercing material is
// evened out without a static EQ dulling the whole take. Zero latency (all IIR).
class Smoother
{
public:
    static constexpr int kBands = 24;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const SmootherSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float bandFreq (int i) const noexcept { return freqs[(size_t) i]; }
    float getBandCutDb (int i) const noexcept { return cutMeter[(size_t) i].load (std::memory_order_relaxed); }
    float getTotalCutDb() const noexcept { return totalMeter.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    SmootherSettings settings;
    std::array<float, kBands> freqs {};
    std::array<BiquadCoeffs, kBands> detCoeffs {}, cutCoeffs {};
    std::array<Biquad, kBands> detectors;
    std::array<std::array<Biquad, 2>, kBands> filters;
    std::array<float, kBands> energy {}, cutDb {}, appliedDb {};
    std::array<std::atomic<float>, kBands> cutMeter {};
    std::atomic<float> totalMeter { 0.0f };
};
}
