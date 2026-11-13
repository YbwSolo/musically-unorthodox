#include "EQEngine.h"
#include <cmath>
#include <complex>

namespace mu
{
// RBJ audio EQ cookbook.
BiquadCoeffs EQEngine::computeCoefficients (const EQBandSettings& s, double fs) noexcept
{
    const double f = juce::jlimit (10.0, fs * 0.49, (double) s.freqHz);
    const double q = juce::jlimit (0.05, 30.0, (double) s.q);
    const double A = std::pow (10.0, (double) s.gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * q);
    double b0, b1, b2, a0, a1, a2;

    switch (s.type)
    {
        case EQType::LowCut:
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
            break;
        case EQType::HighCut:
            b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
            break;
        case EQType::Notch:
            b0 = 1; b1 = -2 * cw; b2 = 1;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
            break;
        case EQType::LowShelf:
        {
            const double sa = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * cw + sa);
            b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            b2 = A * ((A + 1) - (A - 1) * cw - sa);
            a0 = (A + 1) + (A - 1) * cw + sa;
            a1 = -2 * ((A - 1) + (A + 1) * cw);
            a2 = (A + 1) + (A - 1) * cw - sa;
            break;
        }
        case EQType::HighShelf:
        {
            const double sa = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * cw + sa);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 = A * ((A + 1) + (A - 1) * cw - sa);
            a0 = (A + 1) - (A - 1) * cw + sa;
            a1 = 2 * ((A - 1) - (A + 1) * cw);
            a2 = (A + 1) - (A - 1) * cw - sa;
            break;
        }
        case EQType::Bell:
        default:
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
            break;
    }
    return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
}

float EQEngine::magnitudeDb (const BiquadCoeffs& c, float hz, double fs) noexcept
{
    const double w = 2.0 * juce::MathConstants<double>::pi * (double) hz / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const std::complex<double> num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
    const std::complex<double> den = 1.0 + (double) c[3] * z1 + (double) c[4] * z2;
    const double mag = std::abs (num / den);
    return mag > 1.0e-5 ? (float) (20.0 * std::log10 (mag)) : -100.0f;
}

void EQEngine::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (auto& b : bands)
    {
        b.current = b.target;
        b.coeffs = computeCoefficients (b.current, fs);
        for (auto& f : b.filters) f.reset();
        b.dirty = false;
    }
}

void EQEngine::reset()
{
    for (auto& b : bands)
        for (auto& f : b.filters) f.reset();
}

void EQEngine::setBand (int index, const EQBandSettings& s) noexcept
{
    if (index < 0 || index >= kNumEQBands) return;
    auto& b = bands[(size_t) index];
    if (b.target.enabled != s.enabled || b.target.type != s.type || b.target.freqHz != s.freqHz
        || b.target.gainDb != s.gainDb || b.target.q != s.q)
    {
        b.target = s;
        b.dirty = true;
    }
}

void EQEngine::updateBand (Band& b) noexcept
{
    auto& c = b.current;
    const auto& t = b.target;
    const bool typeChanged = c.type != t.type;
    const bool wasEnabled = c.enabled;
    c.type = t.type;
    c.enabled = t.enabled;
    if (c.enabled && ! wasEnabled)
        for (auto& f : b.filters) f.reset();

    const float k = typeChanged ? 1.0f : 0.35f;
    c.freqHz = c.freqHz * std::pow (t.freqHz / juce::jmax (1.0f, c.freqHz), k);
    c.gainDb += (t.gainDb - c.gainDb) * k;
    c.q += (t.q - c.q) * k;
    const bool settled = std::abs (c.freqHz - t.freqHz) < 0.002f * t.freqHz
                      && std::abs (c.gainDb - t.gainDb) < 0.01f
                      && std::abs (c.q - t.q) < 0.002f;
    if (settled) { c.freqHz = t.freqHz; c.gainDb = t.gainDb; c.q = t.q; b.dirty = false; }
    b.coeffs = computeCoefficients (c, fs);
}

void EQEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! enabled) return;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());

    for (int start = 0; start < n; start += kUpdateInterval)
    {
        const int len = juce::jmin (kUpdateInterval, n - start);
        for (auto& b : bands)
        {
            if (b.dirty) updateBand (b);
            if (! b.current.enabled) continue;
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c) + start;
                auto& f = b.filters[(size_t) c];
                for (int i = 0; i < len; ++i) d[i] = f.process (d[i], b.coeffs);
            }
        }
    }
    for (int c = 0; c < ch; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (d[i])) d[i] = 0.0f;
    }
}

float EQEngine::bandMagnitudeDbAt (int band, float hz) const
{
    const auto& b = bands[(size_t) band];
    if (! b.target.enabled) return 0.0f;
    return magnitudeDb (computeCoefficients (b.target, fs), hz, fs);
}

float EQEngine::magnitudeDbAt (float hz) const
{
    float sum = 0.0f;
    for (int i = 0; i < kNumEQBands; ++i) sum += bandMagnitudeDbAt (i, hz);
    return sum;
}
}
