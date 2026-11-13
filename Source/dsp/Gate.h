#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace mu
{
struct GateSettings
{
    bool enabled = false;
    float thresholdDb = -50.0f;
    float rangeDb = 20.0f;      // how far it closes
    float attackMs = 1.5f;
    float releaseMs = 120.0f;
    float holdMs = 40.0f;
};

// Downward expander / gate with hysteresis and hold. Soft 4:1 slope below threshold so breaths fade
// instead of chopping. Stereo-linked.
class Gate
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const GateSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    GateSettings settings;
    float envelope = 0.0f;
    float gainDb = 0.0f;
    int holdCounter = 0;
    bool open = false;
    std::atomic<float> grMeter { 0.0f };
};
}
