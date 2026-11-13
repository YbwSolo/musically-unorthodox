#include "EQEngine.h"
#include <cmath>
#include <complex>

namespace mu
{
static bool isCut (EQType t) { return t == EQType::LowCut || t == EQType::HighCut; }
static bool hasGain (EQType t) { return t == EQType::Bell || t == EQType::LowShelf || t == EQType::HighShelf; }

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

int EQEngine::computeStages (const EQBandSettings& s, double fs, std::array<BiquadCoeffs, kMaxStages>& out) noexcept
{
    if (! isCut (s.type))
    {
        out[0] = computeCoefficients (s, fs);
        return 1;
    }
    // Butterworth cascades; the user's Q shapes the corner of the final stage.
    static const float qs[4][4] = {
        { 0.7071f, 0.0f, 0.0f, 0.0f },
        { 0.5412f, 1.3066f, 0.0f, 0.0f },
        { 0.5176f, 0.7071f, 1.9319f, 0.0f },
        { 0.5098f, 0.6013f, 0.9000f, 2.5629f }
    };
    const int n = juce::jlimit (0, 3, s.slope) + 1;
    for (int i = 0; i < n; ++i)
    {
        EQBandSettings st = s;
        st.q = qs[n - 1][i] * (i == n - 1 ? s.q / 0.7071f : 1.0f);
        out[(size_t) i] = computeCoefficients (st, fs);
    }
    return n;
}

BiquadCoeffs EQEngine::computeBandPass (float hz, float q, double fs) noexcept
{
    const double f = juce::jlimit (10.0, fs * 0.49, (double) hz);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const double alpha = std::sin (w0) / (2.0 * juce::jlimit (0.3, 12.0, (double) q));
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
    return mag > 1.0e-6 ? (float) (20.0 * std::log10 (mag)) : -120.0f;
}

float EQEngine::bandResponseDb (const EQBandSettings& s, float hz, double fs) noexcept
{
    if (! s.enabled) return 0.0f;
    std::array<BiquadCoeffs, kMaxStages> c;
    const int n = computeStages (s, fs, c);
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += magnitudeDb (c[(size_t) i], hz, fs);
    return sum;
}

void EQEngine::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (auto& b : bands)
    {
        b.current = b.target;
        b.numStages = computeStages (b.current, fs, b.coeffs);
        b.detCoeffs = computeBandPass (b.current.freqHz, juce::jmax (0.5f, b.current.q), fs);
        b.dirty = false;
    }
    reset();
}

void EQEngine::reset()
{
    for (auto& b : bands)
    {
        for (auto& st : b.filters) for (auto& f : st) f.reset();
        b.detector.reset();
        b.envelope = 0.0f; b.dynOffset = 0.0f;
    }
    for (auto& st : soloFilters) for (auto& f : st) f.reset();
    powerIn = powerOut = 0.0; autoGainLin = 1.0f;
}

void EQEngine::setBand (int index, const EQBandSettings& s) noexcept
{
    if (index < 0 || index >= kNumEQBands) return;
    auto& b = bands[(size_t) index];
    const auto& t = b.target;
    if (t.enabled != s.enabled || t.type != s.type || t.freqHz != s.freqHz || t.gainDb != s.gainDb || t.q != s.q
        || t.dynamic != s.dynamic || t.thresholdDb != s.thresholdDb || t.rangeDb != s.rangeDb || t.slope != s.slope || t.mode != s.mode)
    {
        b.target = s;
        b.dirty = true;
    }
}

void EQEngine::updateBand (Band& b, float dynOffset) noexcept
{
    auto& c = b.current;
    const auto& t = b.target;
    const bool structural = c.type != t.type || c.slope != t.slope || c.mode != t.mode || (t.enabled && ! c.enabled);
    c.type = t.type; c.slope = t.slope; c.mode = t.mode; c.enabled = t.enabled;
    c.dynamic = t.dynamic; c.thresholdDb = t.thresholdDb; c.rangeDb = t.rangeDb;
    if (structural)
        for (auto& st : b.filters) for (auto& f : st) f.reset();

    const float k = structural ? 1.0f : 0.35f;
    c.freqHz = c.freqHz * std::pow (t.freqHz / juce::jmax (1.0f, c.freqHz), k);
    c.gainDb += (t.gainDb - c.gainDb) * k;
    c.q += (t.q - c.q) * k;
    const bool settled = std::abs (c.freqHz - t.freqHz) < 0.002f * t.freqHz
                      && std::abs (c.gainDb - t.gainDb) < 0.01f
                      && std::abs (c.q - t.q) < 0.002f;
    if (settled) { c.freqHz = t.freqHz; c.gainDb = t.gainDb; c.q = t.q; b.dirty = false; }

    EQBandSettings eff = c;
    eff.gainDb = juce::jlimit (-24.0f, 24.0f, c.gainDb + dynOffset);
    b.numStages = computeStages (eff, fs, b.coeffs);
    b.detCoeffs = computeBandPass (c.freqHz, juce::jmax (0.5f, c.q), fs);
}

void EQEngine::processBand (Band& b, juce::AudioBuffer<float>& buffer, int start, int len, int ch) noexcept
{
    const auto mode = b.current.mode;
    if (ch < 2 || mode == ChannelMode::Stereo)
    {
        if (ch < 2 && mode == ChannelMode::Side) return;   // a mono track has no side signal
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c) + start;
            for (int st = 0; st < b.numStages; ++st)
            {
                auto& f = b.filters[(size_t) st][(size_t) c];
                const auto& co = b.coeffs[(size_t) st];
                for (int i = 0; i < len; ++i) d[i] = f.process (d[i], co);
            }
        }
        return;
    }
    auto* l = buffer.getWritePointer (0) + start;
    auto* r = buffer.getWritePointer (1) + start;
    const bool mid = mode == ChannelMode::Mid;
    for (int i = 0; i < len; ++i)
    {
        float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]);
        float& x = mid ? m : s;
        for (int st = 0; st < b.numStages; ++st) x = b.filters[(size_t) st][0].process (x, b.coeffs[(size_t) st]);
        l[i] = m + s; r[i] = m - s;
    }
}

void EQEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    // Band solo: listen to just the region a band is working on.
    const int solo = soloBand.load (std::memory_order_relaxed);
    if (solo >= 0 && solo < kNumEQBands)
    {
        const auto& t = bands[(size_t) solo].target;
        const auto bp = computeBandPass (t.freqHz, juce::jlimit (1.0f, 8.0f, t.q * 1.5f), fs);
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = soloFilters[1][(size_t) c].process (soloFilters[0][(size_t) c].process (d[i], bp), bp) * 2.0f;
        }
        return;
    }
    if (! enabled) { autoGainMeter.store (0.0f, std::memory_order_relaxed); return; }

    const float detAtt = std::exp (-(float) kUpdateInterval / (float) (fs * 0.004));
    const float detRel = std::exp (-(float) kUpdateInterval / (float) (fs * 0.090));
    double pin = 0.0, pout = 0.0;
    if (autoGain)
        for (int c = 0; c < ch; ++c) { auto* d = buffer.getReadPointer (c); for (int i = 0; i < n; ++i) pin += (double) d[i] * d[i]; }

    for (int start = 0; start < n; start += kUpdateInterval)
    {
        const int len = juce::jmin (kUpdateInterval, n - start);
        for (auto& b : bands)
        {
            const bool dyn = b.target.enabled && b.target.dynamic && hasGain (b.target.type);
            if (dyn)
            {
                float peak = 0.0f;
                const bool side = b.target.mode == ChannelMode::Side && ch > 1;
                for (int i = 0; i < len; ++i)
                {
                    float s = 0.0f;
                    if (side) s = 0.5f * (buffer.getSample (0, start + i) - buffer.getSample (1, start + i));
                    else { for (int c = 0; c < ch; ++c) s += buffer.getSample (c, start + i); s /= (float) ch; }
                    peak = juce::jmax (peak, std::abs (b.detector.process (s, b.detCoeffs)));
                }
                b.envelope = peak > b.envelope ? detAtt * b.envelope + (1.0f - detAtt) * peak : detRel * b.envelope + (1.0f - detRel) * peak;
                const float lvl = juce::Decibels::gainToDecibels (b.envelope, -120.0f);
                const float amount = juce::jlimit (0.0f, 1.0f, (lvl - b.target.thresholdDb) / 12.0f);
                b.dynOffset += (b.target.rangeDb * amount - b.dynOffset) * 0.3f;
                if (std::abs (b.dynOffset) > 0.02f || b.dirty) updateBand (b, b.dynOffset);
            }
            else
            {
                if (b.dynOffset != 0.0f) { b.dynOffset = 0.0f; b.dirty = true; }
                if (b.dirty) updateBand (b, 0.0f);
            }
            b.dynOffsetMeter.store (dyn ? b.dynOffset : 0.0f, std::memory_order_relaxed);
            if (b.current.enabled) processBand (b, buffer, start, len, ch);
        }
    }

    for (int c = 0; c < ch; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (d[i])) d[i] = 0.0f;
            pout += (double) d[i] * d[i];
        }
    }

    if (autoGain)
    {
        // Loudness-matched EQ: slow power averages before and after, so a boost does not just sound "louder = better".
        const double a = std::exp (-(double) n / (fs * 1.5));
        if (pin / (double) (n * ch) > 1.0e-9)
        {
            powerIn = a * powerIn + (1.0 - a) * pin;
            powerOut = a * powerOut + (1.0 - a) * pout;
        }
        const float wanted = (powerIn > 1.0e-12 && powerOut > 1.0e-12) ? juce::jlimit (0.25f, 4.0f, (float) std::sqrt (powerIn / powerOut)) : 1.0f;
        const float startGain = autoGainLin;
        autoGainLin += (wanted - autoGainLin) * juce::jmin (1.0f, (float) n / (float) (fs * 0.3));
        for (int c = 0; c < ch; ++c) buffer.applyGainRamp (c, 0, n, startGain, autoGainLin);
        autoGainMeter.store (juce::Decibels::gainToDecibels (autoGainLin), std::memory_order_relaxed);
    }
    else
    {
        if (std::abs (autoGainLin - 1.0f) > 1.0e-4f)
        {
            for (int c = 0; c < ch; ++c) buffer.applyGainRamp (c, 0, n, autoGainLin, 1.0f);
            autoGainLin = 1.0f;
        }
        powerIn = powerOut = 0.0;
        autoGainMeter.store (0.0f, std::memory_order_relaxed);
    }
}
}
