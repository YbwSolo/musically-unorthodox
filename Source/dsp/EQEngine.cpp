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

BiquadCoeffs EQEngine::computeBandPass (float hz, float q, double fs) noexcept
{
    const double f = juce::jlimit (10.0, fs * 0.49, (double) hz);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const double alpha = std::sin (w0) / (2.0 * juce::jlimit (0.3, 10.0, (double) q));
    const double cw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    return { (float) (alpha / a0), 0.0f, (float) (-alpha / a0), (float) (-2.0 * cw / a0), (float) ((1.0 - alpha) / a0) };
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
        b.detCoeffs = computeBandPass (b.current.freqHz, juce::jmax (0.5f, b.current.q), fs);
        for (auto& f : b.filters) f.reset();
        b.detector.reset();
        b.envelope = 0.0f; b.dynOffset = 0.0f;
        b.dirty = false;
    }
}

void EQEngine::reset()
{
    for (auto& b : bands)
    {
        for (auto& f : b.filters) f.reset();
        b.detector.reset();
        b.envelope = 0.0f; b.dynOffset = 0.0f;
    }
}

void EQEngine::setBand (int index, const EQBandSettings& s) noexcept
{
    if (index < 0 || index >= kNumEQBands) return;
    auto& b = bands[(size_t) index];
    const auto& t = b.target;
    if (t.enabled != s.enabled || t.type != s.type || t.freqHz != s.freqHz || t.gainDb != s.gainDb || t.q != s.q
        || t.dynamic != s.dynamic || t.thresholdDb != s.thresholdDb || t.rangeDb != s.rangeDb)
    {
        b.target = s;
        b.dirty = true;
    }
}

void EQEngine::updateBand (Band& b, float dynOffset) noexcept
{
    auto& c = b.current;
    const auto& t = b.target;
    const bool typeChanged = c.type != t.type;
    const bool wasEnabled = c.enabled;
    c.type = t.type;
    c.enabled = t.enabled;
    c.dynamic = t.dynamic; c.thresholdDb = t.thresholdDb; c.rangeDb = t.rangeDb;
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

    EQBandSettings eff = c;
    eff.gainDb = juce::jlimit (-24.0f, 24.0f, c.gainDb + dynOffset);
    b.coeffs = computeCoefficients (eff, fs);
    b.detCoeffs = computeBandPass (c.freqHz, juce::jmax (0.5f, c.q), fs);
}

void EQEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! enabled) return;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    const float detAtt = std::exp (-1.0f / (float) (fs * 0.004));
    const float detRel = std::exp (-1.0f / (float) (fs * 0.090));
    const float offSmooth = 0.3f;

    for (int start = 0; start < n; start += kUpdateInterval)
    {
        const int len = juce::jmin (kUpdateInterval, n - start);
        for (auto& b : bands)
        {
            const bool dyn = b.target.enabled && b.target.dynamic
                             && b.target.type != EQType::LowCut && b.target.type != EQType::HighCut && b.target.type != EQType::Notch;
            if (dyn)
            {
                // detector on the band's own region, stereo-linked, measured on this sub-block's input
                float peak = 0.0f;
                for (int i = 0; i < len; ++i)
                {
                    float s = 0.0f;
                    for (int c = 0; c < ch; ++c) s += buffer.getSample (c, start + i);
                    peak = juce::jmax (peak, std::abs (b.detector.process (s / (float) ch, b.detCoeffs)));
                }
                b.envelope = peak > b.envelope ? detAtt * b.envelope + (1.0f - detAtt) * peak : detRel * b.envelope + (1.0f - detRel) * peak;
                const float lvl = juce::Decibels::gainToDecibels (b.envelope, -120.0f);
                const float amount = juce::jlimit (0.0f, 1.0f, (lvl - b.target.thresholdDb) / 12.0f);
                const float wanted = b.target.rangeDb * amount;
                b.dynOffset += (wanted - b.dynOffset) * offSmooth;
                if (std::abs (b.dynOffset) > 0.02f || b.dirty) updateBand (b, b.dynOffset);
            }
            else
            {
                if (b.dynOffset != 0.0f) { b.dynOffset = 0.0f; b.dirty = true; }
                if (b.dirty) updateBand (b, 0.0f);
            }
            b.dynOffsetMeter.store (dyn ? b.dynOffset : 0.0f, std::memory_order_relaxed);

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
