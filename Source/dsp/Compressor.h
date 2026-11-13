#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace mu
{
struct CompressorSettings
{
    bool enabled = false;
    float thresholdDb = -18.0f;
    float ratio = 3.0f;
    float attackMs = 10.0f;
    float releaseMs = 120.0f;
    float kneeDb = 6.0f;
    float makeupDb = 0.0f;
    bool autoMakeup = true;
    float mix = 1.0f;   // 0..1
};

// Feed-forward stereo-linked compressor: hybrid peak/RMS detector, soft knee, log-domain smoothing.
class Compressor
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const CompressorSettings& s) noexcept { target = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }
    float getInputDb() const noexcept { return inMeter.load (std::memory_order_relaxed); }
    float getOutputDb() const noexcept { return outMeter.load (std::memory_order_relaxed); }

    static float computeGainDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept;
    static float autoMakeupDb (float thresholdDb, float ratio, float kneeDb) noexcept;

private:
    double fs = 48000.0;
    int channels = 2;
    CompressorSettings target, live;
    juce::AudioBuffer<float> dry;
    float envelope = 0.0f;        // detector level (linear)
    float gainSmooth = 0.0f;      // smoothed gain reduction (dB, <= 0)
    float rmsState = 0.0f;
    juce::SmoothedValue<float> makeupSmooth { 1.0f }, mixSmooth { 1.0f };
    std::atomic<float> grMeter { 0.0f }, inMeter { -100.0f }, outMeter { -100.0f };
};
}
