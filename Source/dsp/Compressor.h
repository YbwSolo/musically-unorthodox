#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "EQEngine.h"
#include "History.h"

namespace mu
{
enum class CompStyle : int { Clean = 0, Punch, Smooth, Glue, Vocal };

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
    float mix = 1.0f;            // 0..1
    CompStyle style = CompStyle::Clean;
    float sidechainHpHz = 20.0f; // <= 25 Hz means off
};

// Feed-forward stereo-linked compressor with five behaviours:
//   Clean  - hybrid peak/RMS detector, transparent (VCA-like)
//   Punch  - peak detector, fast attack, lets the release snap back (FET-like timing, no added distortion)
//   Smooth - RMS detector with a programme-dependent release (opto-like levelling)
//   Glue   - RMS detector, slow programme-dependent release, made for buses and the master
//   Vocal  - two compressors in one, the way a vocal is usually treated: a slow leveller that evens out the
//            phrases, followed by a fast catcher that only touches the peaks the leveller lets through. Neither
//            has to work hard, so the voice stays open instead of being flattened by one heavy compressor.
// The level is measured and the gain is smoothed in decibels. A short hold on the peak reading keeps the gain
// still for the length of one cycle of a low note; without it the gain rides every cycle and that is heard as
// thickening in the low mids. The detector can be high-passed so kicks, 808s and plosives do not drive it.
class Compressor
{
public:
    static constexpr int kHistory = 512;         // one point every 10 ms -> about 5 s on screen

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const CompressorSettings& s) noexcept { target = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray styleNames() { return { "Clean", "Punch", "Smooth", "Glue", "Vocal" }; }

    float getGainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }
    float getInputDb() const noexcept { return inMeter.load (std::memory_order_relaxed); }
    float getOutputDb() const noexcept { return outMeter.load (std::memory_order_relaxed); }
    const History<kHistory>& levelHistory() const noexcept { return levelHist; }
    const History<kHistory>& reductionHistory() const noexcept { return grHist; }

    static float computeGainDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept;
    static float autoMakeupDb (float thresholdDb, float ratio, float kneeDb) noexcept;

private:
    double fs = 48000.0;
    int channels = 2;
    CompressorSettings target, live;
    juce::AudioBuffer<float> dry;
    float held = 0.0f;            // peak reading with a short hold (linear)
    int holdLeft = 0, holdSamples = 384;
    float catchHeld = 0.0f; int catchHoldLeft = 0;
    float levelGain = 0.0f;       // the main compressor's gain (dB, <= 0)
    float catchGain = 0.0f;       // Vocal style: the peak catcher's gain (dB, <= 0)
    float gainSmooth = 0.0f;      // the two together: what is applied and metered (dB, <= 0)
    float slowGr = 0.0f;          // long average of the reduction, drives programme-dependent release
    float rmsState = 0.0f;
    float adaptiveRelCoef = 0.0f;
    std::array<Biquad, 2> scFilters;
    BiquadCoeffs scCoeffs { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float scHz = -1.0f;
    juce::SmoothedValue<float> makeupSmooth { 1.0f }, mixSmooth { 1.0f };
    std::atomic<float> grMeter { 0.0f }, inMeter { -100.0f }, outMeter { -100.0f };
    History<kHistory> levelHist, grHist;
    int histCounter = 0, histInterval = 480;
    float histPeak = 0.0f, histGr = 0.0f;
};
}
