#pragma once
#include "EQEngine.h"
#include <atomic>

namespace mu
{
struct DeEsserSettings
{
    bool enabled = false;
    float freqHz = 6000.0f;
    float thresholdDb = -30.0f;
    float rangeDb = 6.0f;
    bool splitBand = true;   // true: only the highs are pulled; false: whole signal ducks
};

// Sibilance controller. Detector is a band-pass around the ess frequency; in split mode the signal goes
// through a Linkwitz-Riley crossover (sums flat) so only the top end is attenuated.
class DeEsser
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const DeEsserSettings& s) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

private:
    struct Biquad { float z1 = 0.0f, z2 = 0.0f; inline float run (float x, const BiquadCoeffs& c) noexcept { const float y = c[0]*x + z1; z1 = c[1]*x - c[3]*y + z2; z2 = c[2]*x - c[4]*y; return y; } };

    double fs = 48000.0;
    int channels = 2;
    DeEsserSettings settings, live;
    BiquadCoeffs detCoeffs {}, lpCoeffs {}, hpCoeffs {};
    std::array<Biquad, 2> detFilters;
    std::array<std::array<Biquad, 2>, 2> lpFilters, hpFilters;   // Linkwitz-Riley 4th order: two cascaded Butterworth stages per side
    float envelope = 0.0f, gainDb = 0.0f;
    std::atomic<float> grMeter { 0.0f };
};
}
