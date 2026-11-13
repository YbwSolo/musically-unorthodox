#pragma once
#include "EQEngine.h"

namespace mu
{
struct StereoSettings
{
    float width = 1.0f;        // 0 = mono, 1 = unchanged, 2 = double side level
    bool monoBass = false;
    float monoBelowHz = 120.0f;
};

// Mid/side width control with an optional "mono bass" that removes side information below a corner
// frequency, so the low end stays solid on club systems and phone speakers. Also reports stereo correlation.
class StereoTool
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const StereoSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getCorrelation() const noexcept { return correlation.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    StereoSettings settings;
    juce::SmoothedValue<float> widthSmooth { 1.0f }, bassSmooth { 0.0f };
    std::array<Biquad, 2> sideHp;
    BiquadCoeffs hpCoeffs { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float hpHz = -1.0f;
    double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
    std::atomic<float> correlation { 1.0f };
};
}
