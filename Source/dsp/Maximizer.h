#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include "TruePeak.h"
#include "History.h"

namespace mu
{
// Master limiter / maximizer. True-peak aware (4x inter-sample detection), 4 ms look-ahead with a
// linear-ramp attack so the gain is already down when the peak arrives, and a programme-dependent release.
// "Gain" drives the mix into the ceiling to raise loudness; with gain at 0 it is a transparent safety net.
// Only the master instance runs it; on every other instance it is a zero-latency pass-through.
class Maximizer
{
public:
    static constexpr double kLookaheadSeconds = 0.004;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setActive (bool isMasterInstance) noexcept { active = isMasterInstance; }
    void setEnabled (bool on) noexcept { enabled = on; }
    void setGainDb (float db) noexcept { gainDb = db; }
    void setCeilingDb (float db) noexcept { ceilingLin = juce::Decibels::decibelsToGain (db - 0.05f); }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept { return delaySamples; }       // latency while active
    bool isActive() const noexcept { return active; }
    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }
    const History<512>& reductionHistory() const noexcept { return grHist; }

private:
    double fs = 48000.0;
    int channels = 2;
    bool active = false, enabled = false, wasActive = false;
    float gainDb = 0.0f, ceilingLin = 0.5f;
    juce::SmoothedValue<float> driveSmooth { 1.0f };

    int lookahead = 192, delaySamples = 197, holdLen = 200;
    std::vector<float> delayL, delayR;
    int delayPos = 0;
    std::array<TruePeakDetector, 2> detectors;

    // release envelope -> sliding minimum (hold) -> moving average (attack ramp)
    float envelope = 1.0f, slowEnv = 1.0f;
    std::vector<float> minValues; std::vector<int> minIndex;   // monotonic deque as a ring
    int dqHead = 0, dqCount = 0, sampleIndex = 0;
    std::vector<float> avgRing; int avgPos = 0; double avgSum = 0.0;

    std::atomic<float> grMeter { 0.0f };
    History<512> grHist;
    int histCounter = 0, histInterval = 480; float histMin = 1.0f;
};
}
