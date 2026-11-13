#include "DeEsser.h"
#include <cmath>

namespace mu
{
void DeEsser::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    live.freqHz = -1.0f;   // force coefficient build
    setSettings (settings);
    reset();
}

void DeEsser::reset()
{
    for (auto& f : detFilters) f = {};
    for (auto& ch : lpFilters) for (auto& f : ch) f = {};
    for (auto& ch : hpFilters) for (auto& f : ch) f = {};
    envelope = 0.0f; gainDb = 0.0f;
    grMeter.store (0.0f);
}

void DeEsser::setSettings (const DeEsserSettings& s) noexcept
{
    settings = s;
    if (std::abs (live.freqHz - s.freqHz) > 1.0f)
    {
        // Detector: bell-shaped band-pass built from a peak filter with +24 dB then normalised,
        // simpler: use a 2nd-order band-pass (RBJ constant 0 dB peak gain).
        const double f = juce::jlimit (1000.0, fs * 0.45, (double) s.freqHz);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double q = 1.4;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        detCoeffs = { (float) (alpha / a0), 0.0f, (float) (-alpha / a0), (float) (-2.0 * cw / a0), (float) ((1.0 - alpha) / a0) };
        // LR4 crossover a little below the ess frequency so the whole sibilant region is in the high band.
        lpCoeffs = EQEngine::computeCoefficients ({ true, EQType::HighCut, (float) (f * 0.6), 0.0f, 0.7071f }, fs);
        hpCoeffs = EQEngine::computeCoefficients ({ true, EQType::LowCut, (float) (f * 0.6), 0.0f, 0.7071f }, fs);
        live.freqHz = s.freqHz;
    }
}

void DeEsser::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;
    if (! settings.enabled) { gainDb = 0.0f; grMeter.store (0.0f, std::memory_order_relaxed); return; }

    const float att = std::exp (-1.0f / (float) (fs * 0.0005));
    const float rel = std::exp (-1.0f / (float) (fs * 0.040));
    const float gAtt = std::exp (-1.0f / (float) (fs * 0.0008));
    const float gRel = std::exp (-1.0f / (float) (fs * 0.060));
    float minGain = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float det = 0.0f;
        for (int c = 0; c < ch; ++c)
            det = juce::jmax (det, std::abs (detFilters[(size_t) c].run (buffer.getSample (c, i), detCoeffs)));
        envelope = det > envelope ? att * envelope + (1.0f - att) * det : rel * envelope + (1.0f - rel) * det;
        const float lvl = juce::Decibels::gainToDecibels (envelope, -120.0f);
        const float over = lvl - settings.thresholdDb;
        const float target = over > 0.0f ? -juce::jmin (settings.rangeDb, over * 1.5f) : 0.0f;
        gainDb = target < gainDb ? gAtt * gainDb + (1.0f - gAtt) * target : gRel * gainDb + (1.0f - gRel) * target;
        minGain = juce::jmin (minGain, gainDb);
        const float g = juce::Decibels::decibelsToGain (gainDb);

        for (int c = 0; c < ch; ++c)
        {
            float* d = buffer.getWritePointer (c) + i;
            if (settings.splitBand)
            {
                auto& lp = lpFilters[(size_t) c];
                auto& hp = hpFilters[(size_t) c];
                const float low = lp[1].run (lp[0].run (*d, lpCoeffs), lpCoeffs);
                const float high = hp[1].run (hp[0].run (*d, hpCoeffs), hpCoeffs);
                *d = low + high * g;
            }
            else *d *= g;
        }
    }
    grMeter.store (minGain, std::memory_order_relaxed);
}
}
