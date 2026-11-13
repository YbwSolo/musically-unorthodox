#include "Compressor.h"
#include <cmath>

namespace mu
{
float Compressor::computeGainDb (float x, float T, float R, float W) noexcept
{
    // Soft-knee static curve (returns gain change in dB, <= 0).
    const float over = x - T;
    if (W <= 0.0f || over <= -W * 0.5f) return over <= 0.0f ? 0.0f : over * (1.0f / R - 1.0f);
    if (over >= W * 0.5f) return over * (1.0f / R - 1.0f);
    const float t = over + W * 0.5f;
    return (1.0f / R - 1.0f) * t * t / (2.0f * W);
}

float Compressor::autoMakeupDb (float T, float R, float W) noexcept
{
    // Gain needed to bring a signal sitting ~12 dB above threshold back to unity, halved so it never overshoots.
    const float probe = juce::jmin (0.0f, T + 12.0f);
    return -computeGainDb (probe, T, R, W) * 0.5f;
}

void Compressor::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    dry.setSize (2, maxBlockSize, false, false, true);
    makeupSmooth.reset (fs, 0.05);
    mixSmooth.reset (fs, 0.05);
    histInterval = juce::jmax (1, (int) (fs * 0.01));
    levelHist.fill (-100.0f); grHist.fill (0.0f);
    scHz = -1.0f;
    reset();
}

void Compressor::reset()
{
    envelope = 0.0f; gainSmooth = 0.0f; slowGr = 0.0f; rmsState = 0.0f;
    for (auto& f : scFilters) f.reset();
    grMeter.store (0.0f);
}

void Compressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    live = target;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    float inPeak = 0.0f;
    for (int c = 0; c < ch; ++c) inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, 0, n));
    inMeter.store (juce::Decibels::gainToDecibels (inPeak, -100.0f), std::memory_order_relaxed);

    auto pushHistory = [this] (float peak, float gr, int count)
    {
        histPeak = juce::jmax (histPeak, peak);
        histGr = juce::jmin (histGr, gr);
        histCounter += count;
        if (histCounter >= histInterval)
        {
            levelHist.push (juce::Decibels::gainToDecibels (histPeak, -100.0f));
            grHist.push (histGr);
            histCounter = 0; histPeak = 0.0f; histGr = 0.0f;
        }
    };

    if (! live.enabled)
    {
        gainSmooth = 0.0f; slowGr = 0.0f;
        grMeter.store (0.0f, std::memory_order_relaxed);
        outMeter.store (inMeter.load (std::memory_order_relaxed), std::memory_order_relaxed);
        pushHistory (inPeak, 0.0f, n);
        return;
    }

    for (int c = 0; c < ch; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);

    const bool useSc = live.sidechainHpHz > 25.0f;
    if (useSc && std::abs (live.sidechainHpHz - scHz) > 0.5f)
    {
        scHz = live.sidechainHpHz;
        scCoeffs = EQEngine::computeCoefficients ({ true, EQType::LowCut, scHz, 0.0f, 0.7071f }, fs);
    }

    const auto style = live.style;
    const bool peakDetect = style == CompStyle::Punch;
    const bool rmsDetect = style == CompStyle::Smooth || style == CompStyle::Glue;
    const float attMs = juce::jmax (0.05f, live.attackMs) * (style == CompStyle::Punch ? 0.6f : 1.0f);
    const float attCoef = std::exp (-1.0f / (float) (fs * 0.001 * attMs));
    const float baseRel = juce::jmax (1.0f, live.releaseMs);
    const float rmsMs = style == CompStyle::Glue ? 30.0f : style == CompStyle::Smooth ? 15.0f : 5.0f;
    const float rmsCoef = std::exp (-1.0f / (float) (fs * 0.001 * rmsMs));
    const float slowCoef = std::exp (-1.0f / (float) (fs * 0.6));
    const float makeup = live.autoMakeup ? autoMakeupDb (live.thresholdDb, live.ratio, live.kneeDb) : live.makeupDb;
    const float knee = style == CompStyle::Smooth ? juce::jmax (live.kneeDb, 10.0f) : live.kneeDb;
    makeupSmooth.setTargetValue (juce::Decibels::decibelsToGain (makeup));
    mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, live.mix));

    const bool adaptiveRelease = style == CompStyle::Smooth || style == CompStyle::Glue;
    const float fixedRelCoef = std::exp (-1.0f / (float) (fs * 0.001 * baseRel * (style == CompStyle::Punch ? 0.7f : 1.0f)));
    if (adaptiveRelCoef <= 0.0f) adaptiveRelCoef = fixedRelCoef;

    float minGain = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f, sq = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            float v = buffer.getSample (c, i);
            if (useSc) v = scFilters[(size_t) c].process (v, scCoeffs);
            peak = juce::jmax (peak, std::abs (v));
            sq += v * v;
        }
        rmsState = rmsCoef * rmsState + (1.0f - rmsCoef) * (sq / (float) ch);
        const float rms = std::sqrt (rmsState);
        const float det = peakDetect ? peak : rmsDetect ? rms * 1.4142f : juce::jmax (peak, rms);

        // Programme-dependent release: the longer it has been compressing, the slower it lets go,
        // so single peaks recover fast while sustained loud passages stay smooth.
        float relCoef = fixedRelCoef;
        if (adaptiveRelease && (i & 15) == 0)
        {
            const float relMs = style == CompStyle::Smooth ? baseRel * (0.5f + 3.5f * juce::jlimit (0.0f, 1.0f, -slowGr / 6.0f))
                                                           : baseRel * (1.0f + 3.0f * juce::jlimit (0.0f, 1.0f, -slowGr / 4.0f));
            adaptiveRelCoef = std::exp (-1.0f / (float) (fs * 0.001 * relMs));
        }
        if (adaptiveRelease) relCoef = adaptiveRelCoef;

        envelope = det > envelope ? attCoef * envelope + (1.0f - attCoef) * det
                                  : relCoef * envelope + (1.0f - relCoef) * det;
        const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
        const float g = computeGainDb (levelDb, live.thresholdDb, live.ratio, knee);
        gainSmooth += (g - gainSmooth) * 0.2f;
        slowGr = slowCoef * slowGr + (1.0f - slowCoef) * gainSmooth;
        minGain = juce::jmin (minGain, gainSmooth);
        const float lin = juce::Decibels::decibelsToGain (gainSmooth) * makeupSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();
        for (int c = 0; c < ch; ++c)
        {
            float wet = buffer.getSample (c, i) * lin;
            if (style == CompStyle::Punch)
            {
                // A touch of odd-harmonic bite that grows with how hard it is working.
                const float amt = juce::jlimit (0.0f, 0.25f, -gainSmooth * 0.02f);
                wet = wet * (1.0f - amt) + amt * std::tanh (wet * 1.6f) / 1.6f * 1.25f;
            }
            buffer.setSample (c, i, wet * mix + dry.getSample (c, i) * (1.0f - mix));
        }
    }
    grMeter.store (minGain, std::memory_order_relaxed);
    float outPeak = 0.0f;
    for (int c = 0; c < ch; ++c) outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, 0, n));
    outMeter.store (juce::Decibels::gainToDecibels (outPeak, -100.0f), std::memory_order_relaxed);
    pushHistory (inPeak, minGain, n);
}
}
