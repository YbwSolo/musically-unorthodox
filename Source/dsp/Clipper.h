#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace mu
{
// Soft clipper for the master, in front of the limiter. The tallest peaks of a mix are a few drum hits a
// millisecond or two long. A limiter turns the whole mix down to get them under the ceiling; a clipper rounds
// off only the tops of those hits, which the ear forgives far more easily, so the limiter has less to do and the
// mix keeps its punch at the same loudness. It works at four times the sample rate, so the harmonics the
// rounding makes do not fold back into the sound. Below its threshold it changes nothing.
class Clipper
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setActive (bool inThePath) noexcept { active = inThePath; }         // master instance only: delays even when off, so the timing never changes
    void setEnabled (bool on) noexcept { enabled = on; }
    void setThresholdDb (float db) noexcept { threshold = juce::Decibels::decibelsToGain (db); }
    void setSoftness (float amount01) noexcept { softness = juce::jlimit (0.0f, 1.0f, amount01); }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    int getLatencySamples() const noexcept { return latency; }
    float getReductionDb() const noexcept { return meter.load (std::memory_order_relaxed); }                 // the deepest it rounded a peak in the last block
    static float shape (float x, float threshold, float softness) noexcept;

private:
    int channels = 2, latency = 0, delayPos = 0;
    bool active = false, enabled = false;
    float threshold = 1.0f, softness = 0.5f, wet = 0.0f;
    double fs = 48000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> work;
    std::array<std::vector<float>, 2> delay;        // the untouched sound, late by the same amount as the clipped one
    std::atomic<float> meter { 0.0f };
};
}
