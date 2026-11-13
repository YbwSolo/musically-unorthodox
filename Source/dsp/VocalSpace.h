#pragma once
#include "EQEngine.h"

namespace mu
{
// "Vocal Space": sidechain-style dynamic EQ for beat / instrument instances. When the vocal instance in the
// session is active, this carves a little room out of the bands where vocals live. Driven by a 0..1 key
// level that the session bus provides (vocal momentary loudness mapped to a presence value).
class VocalSpace
{
public:
    static constexpr int kBands = 3;
    struct Settings { bool enabled = false; float amount = 0.5f; int speed = 1; };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const Settings& s) noexcept { settings = s; }
    void setKeyLevel (float level01) noexcept { key = juce::jlimit (0.0f, 1.0f, level01); }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getCurrentDepth() const noexcept { return depth; }           // 0..1 for the UI
    float bandFreq (int i) const { return freqs[(size_t) i]; }
    float bandMaxCutDb (int i) const { return maxCut[(size_t) i]; }

private:
    double fs = 48000.0;
    int channels = 2;
    Settings settings;
    float key = 0.0f, depth = 0.0f;
    const std::array<float, kBands> freqs { 260.0f, 2600.0f, 7000.0f };
    const std::array<float, kBands> maxCut { -2.0f, -4.5f, -2.5f };
    const std::array<float, kBands> qs { 0.9f, 0.8f, 1.1f };
    std::array<BiquadCoeffs, kBands> coeffs;
    struct Biquad { float z1 = 0.0f, z2 = 0.0f; };
    std::array<std::array<Biquad, 2>, kBands> states;
};
}
