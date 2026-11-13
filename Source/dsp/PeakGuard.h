#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>

namespace mu
{
// Transparent look-ahead peak limiter used on the master instance so nothing goes over the true-peak
// ceiling while the session is being balanced. 2 ms look-ahead, fast release, stereo linked.
// It is a safety net for the hand-off, not a loudness tool.
class PeakGuard
{
public:
    static constexpr double kLookaheadSeconds = 0.002;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setCeilingDb (float db) noexcept { ceilingLin = juce::Decibels::decibelsToGain (db - 0.3f); }   // small margin for inter-sample peaks
    void setEnabled (bool on) noexcept { enabled = on; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    int getLatencySamples() const noexcept { return lookahead; }
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    int lookahead = 96;
    bool enabled = false;
    float ceilingLin = 0.5f;
    std::vector<float> delayL, delayR, gainWindow;
    int writePos = 0;
    float gain = 1.0f;
    std::atomic<float> grMeter { 0.0f };
};
}
