#include "PeakGuard.h"
#include <cmath>

namespace mu
{
void PeakGuard::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    lookahead = juce::jmax (8, (int) std::round (fs * kLookaheadSeconds));
    delayL.assign ((size_t) lookahead, 0.0f);
    delayR.assign ((size_t) lookahead, 0.0f);
    gainWindow.assign ((size_t) lookahead, 1.0f);
    reset();
}

void PeakGuard::reset()
{
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    std::fill (gainWindow.begin(), gainWindow.end(), 1.0f);
    writePos = 0; gain = 1.0f;
    grMeter.store (0.0f);
}

void PeakGuard::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    // The delay always runs so latency is constant whether or not the guard is active.
    const float rel = std::exp (-1.0f / (float) (fs * 0.08));
    const float att = std::exp (-1.0f / (float) (fs * kLookaheadSeconds * 0.25));   // settles well inside the look-ahead window
    float minGain = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        float* l = buffer.getWritePointer (0) + i;
        float* r = ch > 1 ? buffer.getWritePointer (1) + i : nullptr;
        const float inL = *l, inR = r ? *r : inL;

        // required gain for the incoming sample
        const float peak = juce::jmax (std::abs (inL), std::abs (inR));
        const float required = (enabled && peak > ceilingLin) ? ceilingLin / peak : 1.0f;
        gainWindow[(size_t) writePos] = required;

        // minimum over the look-ahead window = what the sample leaving the delay will need at the latest
        float need = 1.0f;
        for (float g : gainWindow) need = juce::jmin (need, g);

        gain = need < gain ? att * gain + (1.0f - att) * need : rel * gain + (1.0f - rel) * need;
        gain = juce::jmin (gain, need);   // never let an over-ceiling sample through
        minGain = juce::jmin (minGain, gain);

        const float outL = delayL[(size_t) writePos] * gain;
        const float outR = delayR[(size_t) writePos] * gain;
        delayL[(size_t) writePos] = inL;
        delayR[(size_t) writePos] = inR;
        writePos = (writePos + 1) % lookahead;

        *l = outL;
        if (r) *r = outR;
    }
    grMeter.store (juce::Decibels::gainToDecibels (minGain, -60.0f), std::memory_order_relaxed);
}
}
