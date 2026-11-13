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
    reset();
}

void Compressor::reset()
{
    envelope = 0.0f; gainSmooth = 0.0f; rmsState = 0.0f;
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

    if (! live.enabled)
    {
        gainSmooth = 0.0f;
        grMeter.store (0.0f, std::memory_order_relaxed);
        outMeter.store (inMeter.load (std::memory_order_relaxed), std::memory_order_relaxed);
        return;
    }

    for (int c = 0; c < ch; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);

    const float attCoef = std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (0.05f, live.attackMs)));
    const float relCoef = std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (1.0f, live.releaseMs)));
    const float rmsCoef = std::exp (-1.0f / (float) (fs * 0.005));
    const float makeup = live.autoMakeup ? autoMakeupDb (live.thresholdDb, live.ratio, live.kneeDb) : live.makeupDb;
    makeupSmooth.setTargetValue (juce::Decibels::decibelsToGain (makeup));
    mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, live.mix));

    float minGain = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f, sq = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            const float v = buffer.getSample (c, i);
            peak = juce::jmax (peak, std::abs (v));
            sq += v * v;
        }
        rmsState = rmsCoef * rmsState + (1.0f - rmsCoef) * (sq / (float) ch);
        const float det = juce::jmax (peak, std::sqrt (rmsState));   // hybrid: whichever is louder
        envelope = det > envelope ? attCoef * envelope + (1.0f - attCoef) * det
                                  : relCoef * envelope + (1.0f - relCoef) * det;
        const float levelDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
        const float g = computeGainDb (levelDb, live.thresholdDb, live.ratio, live.kneeDb);
        gainSmooth += (g - gainSmooth) * 0.2f;
        minGain = juce::jmin (minGain, gainSmooth);
        const float lin = juce::Decibels::decibelsToGain (gainSmooth) * makeupSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();
        for (int c = 0; c < ch; ++c)
        {
            const float wet = buffer.getSample (c, i) * lin;
            buffer.setSample (c, i, wet * mix + dry.getSample (c, i) * (1.0f - mix));
        }
    }
    grMeter.store (minGain, std::memory_order_relaxed);
    float outPeak = 0.0f;
    for (int c = 0; c < ch; ++c) outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, 0, n));
    outMeter.store (juce::Decibels::gainToDecibels (outPeak, -100.0f), std::memory_order_relaxed);
}
}
