#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "../state/ParameterIDs.h"

namespace mu
{
enum class ChannelMode : int { Stereo = 0, Mid, Side };

struct EQBandSettings
{
    bool enabled = false;
    EQType type = EQType::Bell;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.71f;
    bool dynamic = false;        // band gain moves with the level in its own frequency region
    float thresholdDb = -30.0f;
    float rangeDb = -6.0f;       // negative = cuts when loud, positive = boosts when loud
    int slope = 0;               // cut filters only: 0..3 = 12/24/36/48 dB per octave
    ChannelMode mode = ChannelMode::Stereo;
};

// Normalised biquad coefficients: b0 b1 b2 a1 a2 (a0 == 1).
using BiquadCoeffs = std::array<float, 5>;

struct Biquad
{
    float z1 = 0.0f, z2 = 0.0f;
    inline float process (float x, const BiquadCoeffs& c) noexcept
    {
        const float y = c[0] * x + z1;
        z1 = c[1] * x - c[3] * y + z2;
        z2 = c[2] * x - c[4] * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

// Eight-band minimum-phase parametric EQ: bells, shelves, notches, cuts up to 48 dB/oct, per-band
// stereo / mid / side, optional dynamic bands, loudness-matched auto gain and band solo.
// Zero latency. Coefficient updates are smoothed and allocation-free.
class EQEngine
{
public:
    static constexpr int kMaxStages = 4;
    static constexpr int kUpdateInterval = 32;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setBand (int index, const EQBandSettings& s) noexcept;   // audio thread
    void setEnabled (bool on) noexcept { enabled = on; }
    void setAutoGain (bool on) noexcept { autoGain = on; }
    void setSoloBand (int band) noexcept { soloBand.store (band, std::memory_order_relaxed); }   // any thread; -1 = off
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    float getBandDynamicOffsetDb (int band) const { return bands[(size_t) band].dynOffsetMeter.load (std::memory_order_relaxed); }
    float getAutoGainDb() const { return autoGainMeter.load (std::memory_order_relaxed); }

    // Static helpers shared with the UI and tests.
    static BiquadCoeffs computeCoefficients (const EQBandSettings& s, double fs) noexcept;          // single stage, uses s.q
    static int computeStages (const EQBandSettings& s, double fs, std::array<BiquadCoeffs, kMaxStages>& out) noexcept;
    static BiquadCoeffs computeBandPass (float hz, float q, double fs) noexcept;
    static float magnitudeDb (const BiquadCoeffs& c, float hz, double fs) noexcept;
    static float bandResponseDb (const EQBandSettings& s, float hz, double fs) noexcept;            // 0 when the band is off

private:
    struct Band
    {
        EQBandSettings target, current;
        std::array<BiquadCoeffs, kMaxStages> coeffs {};
        int numStages = 1;
        BiquadCoeffs detCoeffs { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<std::array<Biquad, 2>, kMaxStages> filters;
        Biquad detector;
        float envelope = 0.0f;
        float dynOffset = 0.0f;
        std::atomic<float> dynOffsetMeter { 0.0f };
        bool dirty = true;
    };

    void updateBand (Band& b, float dynOffset) noexcept;
    void processBand (Band& b, juce::AudioBuffer<float>& buffer, int start, int len, int ch) noexcept;

    double fs = 48000.0;
    int channels = 2;
    bool enabled = true, autoGain = false;
    std::atomic<int> soloBand { -1 };
    std::array<Band, kNumEQBands> bands;
    std::array<std::array<Biquad, 2>, 2> soloFilters;
    double powerIn = 0.0, powerOut = 0.0;
    float autoGainLin = 1.0f;
    std::atomic<float> autoGainMeter { 0.0f };
};
}
