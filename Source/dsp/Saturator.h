#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace mu
{
struct SaturatorSettings
{
    bool enabled = false;
    float driveDb = 6.0f;
    float mix = 0.4f;    // 0..1
    int type = 0;        // 0 tape, 1 tube, 2 warm
};

// Harmonic saturation, 2x oversampled, level-compensated so turning it on does not just get louder.
class Saturator
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const SaturatorSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static float shape (float x, int type, float drive) noexcept;
    int getLatencySamples() const noexcept { return os ? (int) std::round (os->getLatencyInSamples()) : 0; }

private:
    double fs = 48000.0;
    int channels = 2;
    SaturatorSettings settings;
    std::unique_ptr<juce::dsp::Oversampling<float>> os, osDry;   // dry runs through an identical oversampler so both paths share one latency
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> driveSmooth { 1.0f }, mixSmooth { 0.0f };
    std::array<float, 2> dcState {}, dcPrev {};
};
}
