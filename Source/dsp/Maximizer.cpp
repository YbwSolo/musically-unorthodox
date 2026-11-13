#include "Maximizer.h"
#include <cmath>

namespace mu
{
void Maximizer::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    lookahead = juce::jmax (16, (int) std::round (fs * kLookaheadSeconds));
    delaySamples = lookahead - 1 + TruePeakDetector::kDelay;
    holdLen = lookahead + TruePeakDetector::kDelay + 2;
    delayL.assign ((size_t) delaySamples, 0.0f);
    delayR.assign ((size_t) delaySamples, 0.0f);
    minValues.assign ((size_t) holdLen + 2, 1.0f);
    minIndex.assign ((size_t) holdLen + 2, 0);
    avgRing.assign ((size_t) lookahead, 1.0f);
    driveSmooth.reset (fs, 0.05);
    histInterval = juce::jmax (1, (int) (fs * 0.01));
    grHist.fill (0.0f);
    reset();
}

void Maximizer::reset()
{
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    std::fill (avgRing.begin(), avgRing.end(), 1.0f);
    for (auto& d : detectors) d.reset();
    delayPos = 0; envelope = 1.0f; slowEnv = 1.0f;
    dqHead = 0; dqCount = 0; sampleIndex = 0;
    avgPos = 0; avgSum = (double) lookahead;
    grMeter.store (0.0f);
    histCounter = 0; histMin = 1.0f;
}

void Maximizer::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    if (! active)
    {
        if (wasActive) { reset(); wasActive = false; }
        grMeter.store (0.0f, std::memory_order_relaxed);
        return;
    }
    wasActive = true;

    driveSmooth.setTargetValue (enabled ? juce::Decibels::decibelsToGain (gainDb) : 1.0f);
    const float fastRel = std::exp (-1.0f / (float) (fs * 0.06));
    const float slowRel = std::exp (-1.0f / (float) (fs * 0.6));
    const int cap = (int) minValues.size();
    float blockMin = 1.0f;

    float* l = buffer.getWritePointer (0);
    float* r = ch > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float drive = driveSmooth.getNextValue();
        const float inL = l[i] * drive, inR = r ? r[i] * drive : inL;

        // inter-sample peak of the driven signal
        float peak = detectors[0].process (inL);
        if (r) peak = juce::jmax (peak, detectors[1].process (inR));
        peak = juce::jmax (peak, std::abs (inL), std::abs (inR));
        const float required = (enabled && peak > ceilingLin) ? ceilingLin / peak : 1.0f;

        // release: a fast recovery for single peaks, held back by a slow envelope when it has been limiting for a while
        slowEnv = juce::jmin (required, slowRel * slowEnv + (1.0f - slowRel));
        const float recovered = fastRel * envelope + (1.0f - fastRel) * juce::jmin (1.0f, slowEnv * 1.26f);
        envelope = juce::jmin (required, recovered);

        // sliding minimum over the hold window (monotonic deque)
        while (dqCount > 0 && minValues[(size_t) ((dqHead + dqCount - 1) % cap)] >= envelope) --dqCount;
        const int tail = (dqHead + dqCount) % cap;
        minValues[(size_t) tail] = envelope; minIndex[(size_t) tail] = sampleIndex; ++dqCount;
        while (sampleIndex - minIndex[(size_t) dqHead] >= holdLen) { dqHead = (dqHead + 1) % cap; --dqCount; }
        const float held = minValues[(size_t) dqHead];
        ++sampleIndex;

        // moving average = linear attack ramp that lands exactly when the peak leaves the delay line
        avgSum += (double) held - (double) avgRing[(size_t) avgPos];
        avgRing[(size_t) avgPos] = held;
        avgPos = avgPos + 1 == lookahead ? 0 : avgPos + 1;
        const float gain = juce::jmin (1.0f, (float) (avgSum / (double) lookahead));
        blockMin = juce::jmin (blockMin, gain);

        const float outL = delayL[(size_t) delayPos] * gain;
        const float outR = delayR[(size_t) delayPos] * gain;
        delayL[(size_t) delayPos] = inL; delayR[(size_t) delayPos] = inR;
        delayPos = delayPos + 1 == delaySamples ? 0 : delayPos + 1;
        l[i] = outL;
        if (r) r[i] = outR;

        histMin = juce::jmin (histMin, gain);
        if (++histCounter >= histInterval)
        {
            grHist.push (juce::Decibels::gainToDecibels (histMin, -60.0f));
            histCounter = 0; histMin = 1.0f;
        }
    }
    avgSum = 0.0;   // re-derive the running sum each block so rounding never accumulates
    for (float v : avgRing) avgSum += v;
    if (sampleIndex > (1 << 30))
    {
        const int shift = sampleIndex - holdLen - 4;
        for (int k = 0; k < dqCount; ++k) minIndex[(size_t) ((dqHead + k) % cap)] -= shift;
        sampleIndex -= shift;
    }
    grMeter.store (juce::Decibels::gainToDecibels (blockMin, -60.0f), std::memory_order_relaxed);
}
}
