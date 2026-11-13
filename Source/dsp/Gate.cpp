#include "Gate.h"
#include <cmath>

namespace mu
{
void Gate::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    reset();
}

void Gate::reset()
{
    envelope = 0.0f; gainDb = 0.0f; holdCounter = 0; open = false;
    grMeter.store (0.0f);
}

void Gate::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;
    if (! settings.enabled)
    {
        gainDb = 0.0f; open = true;
        grMeter.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const float detAtt = std::exp (-1.0f / (float) (fs * 0.0003));         // 0.3 ms peak detector
    const float detRel = std::exp (-1.0f / (float) (fs * 0.020));          // 20 ms
    const float gAtt = std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (0.05f, settings.attackMs)));
    const float gRel = std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (5.0f, settings.releaseMs)));
    const int holdSamples = (int) (fs * 0.001 * settings.holdMs);
    const float openThr = settings.thresholdDb;
    const float closeThr = settings.thresholdDb - 4.0f;                      // hysteresis
    const float range = settings.rangeDb;
    float minGain = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < ch; ++c) peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));
        envelope = peak > envelope ? detAtt * envelope + (1.0f - detAtt) * peak : detRel * envelope + (1.0f - detRel) * peak;
        const float levelDb = juce::Decibels::gainToDecibels (envelope, -120.0f);

        if (levelDb > openThr) { open = true; holdCounter = holdSamples; }
        else if (levelDb < closeThr)
        {
            if (holdCounter > 0) --holdCounter; else open = false;
        }

        float target = 0.0f;
        if (! open)
        {
            // soft slope: 4:1 expansion below the close threshold, capped by range
            target = juce::jmax (-range, (levelDb - closeThr) * 3.0f);
        }
        gainDb = target > gainDb ? gAtt * gainDb + (1.0f - gAtt) * target : gRel * gainDb + (1.0f - gRel) * target;
        minGain = juce::jmin (minGain, gainDb);
        const float lin = juce::Decibels::decibelsToGain (gainDb);
        for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= lin;
    }
    grMeter.store (minGain, std::memory_order_relaxed);
}
}
