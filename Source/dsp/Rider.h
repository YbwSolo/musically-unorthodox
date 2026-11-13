#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace mu
{
struct RiderSettings
{
    bool enabled = false;
    float targetDb = -20.0f;   // RMS target
    float rangeDb = 6.0f;      // max boost / cut
    int speed = 1;             // 0 slow, 1 normal, 2 fast
};

// Level rider: slowly moves the gain so the track's RMS sits at the target, within +/- range.
// Holds its position when the track goes quiet so breaths and gaps are not pumped up.
class Rider
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const RiderSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getCurrentGainDb() const noexcept { return gainMeter.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    RiderSettings settings;
    float rms = 0.0f;
    float gainDb = 0.0f;
    std::atomic<float> gainMeter { 0.0f };
};
}
