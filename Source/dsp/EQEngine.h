#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../state/ParameterIDs.h"

namespace mu
{
struct EQBandSettings
{
    bool enabled = false;
    EQType type = EQType::Bell;
    float freqHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.71f;
};

// Normalised biquad coefficients: b0 b1 b2 a1 a2 (a0 == 1).
using BiquadCoeffs = std::array<float, 5>;

// Six-band minimum-phase parametric EQ with smoothed, allocation-free coefficient updates.
class EQEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setBand (int index, const EQBandSettings& s) noexcept;   // call from the audio thread (processBlock)
    void setEnabled (bool on) noexcept { enabled = on; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    // UI helpers (use the target settings).
    float magnitudeDbAt (float hz) const;
    float bandMagnitudeDbAt (int band, float hz) const;
    EQBandSettings getBandTarget (int band) const { return bands[(size_t) band].target; }

    static BiquadCoeffs computeCoefficients (const EQBandSettings& s, double fs) noexcept;
    static float magnitudeDb (const BiquadCoeffs& c, float hz, double fs) noexcept;
    static constexpr int kUpdateInterval = 32;

private:
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
        void reset() { z1 = z2 = 0.0f; }
    };

    struct Band
    {
        EQBandSettings target, current;
        BiquadCoeffs coeffs { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        std::array<Biquad, 2> filters;
        bool dirty = true;
    };

    void updateBand (Band& b) noexcept;

    double fs = 48000.0;
    int channels = 2;
    bool enabled = true;
    std::array<Band, kNumEQBands> bands;
};
}
