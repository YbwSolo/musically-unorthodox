#include "Rider.h"
#include <cmath>

namespace mu
{
void Rider::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    reset();
}

void Rider::reset()
{
    rms = 0.0f; gainDb = 0.0f;
    gainMeter.store (0.0f);
}

void Rider::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    const float rmsCoef = std::exp (-1.0f / (float) (fs * 0.25));                      // 250 ms RMS window
    const float speedSec = settings.speed == 0 ? 0.9f : settings.speed == 1 ? 0.45f : 0.18f;
    const float gCoef = std::exp (-1.0f / (float) (fs * speedSec));
    const float gateDb = settings.targetDb - 28.0f;                                  // below this, hold

    if (! settings.enabled)
    {
        // glide back to unity so switching off never clicks
        const float back = std::exp (-1.0f / (float) (fs * 0.1));
        for (int i = 0; i < n; ++i)
        {
            gainDb *= back;
            if (std::abs (gainDb) > 0.001f)
            {
                const float lin = juce::Decibels::decibelsToGain (gainDb);
                for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= lin;
            }
        }
        gainMeter.store (gainDb, std::memory_order_relaxed);
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        float sq = 0.0f;
        for (int c = 0; c < ch; ++c) { const float v = buffer.getSample (c, i); sq += v * v; }
        rms = rmsCoef * rms + (1.0f - rmsCoef) * (sq / (float) ch);
        const float levelDb = 10.0f * std::log10 (juce::jmax (1.0e-12f, rms));
        if (levelDb > gateDb)
        {
            const float wanted = juce::jlimit (-settings.rangeDb, settings.rangeDb, settings.targetDb - levelDb);
            gainDb = gCoef * gainDb + (1.0f - gCoef) * wanted;
        }
        const float lin = juce::Decibels::decibelsToGain (gainDb);
        for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= lin;
    }
    gainMeter.store (gainDb, std::memory_order_relaxed);
}
}
