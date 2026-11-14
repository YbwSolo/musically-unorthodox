#include "Echo.h"

namespace mu
{
void Echo::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : lines) l.resize ((int) (fs * 2.05));
    reset();
}

void Echo::reset()
{
    for (auto& l : lines) l.clear();
    hp[0] = hp[1] = lp[0] = lp[1] = 0.0f;
    envelope = 0.0f; duckGain = 1.0f; mixNow = 0.0f;
    timeNow = (float) (fs * 0.001 * currentTimeMs());
}

void Echo::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled)
    {
        if (wasEnabled) { reset(); wasEnabled = false; duckMeter.store (0.0f); }
        return;
    }
    if (! wasEnabled) { reset(); wasEnabled = true; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    float* left = buffer.getWritePointer (0);
    float* right = ch > 1 ? buffer.getWritePointer (1) : nullptr;
    const float target = (float) (fs * 0.001 * currentTimeMs());
    const float glide = 1.0f - std::exp (-1.0f / (float) (fs * 0.12));          // a change of time bends the echoes like tape
    const float fb = juce::jlimit (0.0f, 0.9f, settings.feedback);
    const float hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 160.0f / (float) fs);
    const float lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * (1400.0f * std::pow (9.0f, juce::jlimit (0.0f, 1.0f, settings.tone))) / (float) fs);
    const float attack = 1.0f - std::exp (-1.0f / (float) (fs * 0.010));
    const float release = 1.0f - std::exp (-1.0f / (float) (fs * 0.30));
    const float refFall = std::exp (-1.0f / (float) (fs * 6.0));
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.03));
    const float cross = 1.0f - juce::jlimit (0.0f, 1.0f, settings.width);        // how much of each side's echo also appears on the other
    const float norm = 1.0f / std::sqrt (1.0f + cross * cross);

    for (int i = 0; i < n; ++i)
    {
        const float dryL = left[i], dryR = right != nullptr ? right[i] : left[i];
        const float in = 0.5f * (dryL + dryR);

        const float level = juce::jmax (std::abs (dryL), std::abs (dryR));
        envelope += (level - envelope) * (level > envelope ? attack : release);
        reference = juce::jmax (envelope, reference * refFall, 1.0e-4f);
        const float presence = juce::jlimit (0.0f, 1.0f, envelope / reference);
        const float wanted = 1.0f - settings.duck * 0.85f * presence;
        duckGain += (wanted - duckGain) * (wanted < duckGain ? attack : release);

        timeNow += (target - timeNow) * glide;
        const float yL = lines[0].read (timeNow), yR = lines[1].read (timeNow);

        // The sound goes into the left line; what comes out of the left goes into the right, and what comes out of
        // the right goes round again. So the echoes alternate sides, each one quieter by the feedback amount.
        float x[2] = { in + fb * yR, fb * yL };
        for (int c = 0; c < 2; ++c)
        {
            hp[c] = hpCoeff * hp[c] + (1.0f - hpCoeff) * x[c];
            float v = x[c] - hp[c];
            lp[c] = lpCoeff * lp[c] + (1.0f - lpCoeff) * v;
            lines[c].write (juce::jlimit (-4.0f, 4.0f, lp[c]));
        }

        mixNow += (settings.mix - mixNow) * mixStep;
        const float g = mixNow * duckGain * 1.25f;
        const float wetL = (yL + cross * yR) * norm, wetR = (yR + cross * yL) * norm;
        if (right != nullptr) { left[i] = dryL + g * wetL; right[i] = dryR + g * wetR; }
        else left[i] = dryL + g * 0.7071f * (yL + yR);

        if (++meterCount >= 1024) { meterCount = 0; duckMeter.store (juce::Decibels::gainToDecibels (duckGain, -60.0f), std::memory_order_relaxed); }
    }
}
}
