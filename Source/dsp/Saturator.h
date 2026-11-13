#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>

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
    int getLatencySamples() const noexcept { return latency; }

private:
    double fs = 48000.0;
    int channels = 2;
    SaturatorSettings settings;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> dryDelay;   // dry path = pure delay matching the oversampler's latency
    int dryPos = 0, latency = 0;
    bool wetWasActive = false;
    juce::SmoothedValue<float> driveSmooth { 1.0f }, mixSmooth { 0.0f };
    std::array<float, 2> dcState {}, dcPrev {};
};
}
