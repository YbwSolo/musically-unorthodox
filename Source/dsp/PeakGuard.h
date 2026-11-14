#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace mu
{
// A safety net at the end of a track's chain. Anything that would leave the plugin above about -1 dB is turned down the
// instant it arrives and let back up over about a tenth of a second, so one loud word cannot clip the track or
// the mix it feeds. It adds no delay. It is there for the odd peak; a track that leans on it all the time needs
// its compressor, not this.
class PeakGuard
{
public:
    void prepare (double sampleRate) noexcept
    {
        release = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.12));
        holdSamples = (int) (sampleRate * 0.008);
        reset();
    }
    void reset() noexcept { gain = 1.0f; hold = 0; worst = 1.0f; count = 0; reduction.store (0.0f); }
    void setEnabled (bool on) noexcept { enabled = on; }
    void setCeilingDb (float db) noexcept { ceiling = juce::Decibels::decibelsToGain (db); }
    float getReductionDb() const noexcept { return reduction.load (std::memory_order_relaxed); }      // 0 or negative

    // the loudest moment that has arrived since the last reset, before the guard touched it
    float getInputPeakDb() const noexcept { return juce::Decibels::gainToDecibels (inputPeak.load (std::memory_order_relaxed), -100.0f); }
    void resetInputPeak() noexcept { inputPeak.store (0.0f); }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        float loudest = 0.0f;
        for (int c = 0; c < ch; ++c) loudest = juce::jmax (loudest, buffer.getMagnitude (c, 0, n));
        if (loudest > inputPeak.load (std::memory_order_relaxed)) inputPeak.store (loudest, std::memory_order_relaxed);
        if (! enabled) { if (gain != 1.0f) { gain = 1.0f; hold = 0; reduction.store (0.0f); } return; }
        if (loudest * gain <= ceiling && gain > 0.9999f) { if (++count >= 8) { reduction.store (0.0f, std::memory_order_relaxed); count = 0; worst = 1.0f; } return; }      // nothing to do: leave the sound untouched
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < ch; ++c) peak = juce::jmax (peak, std::abs (buffer.getReadPointer (c)[i]));
            const float needed = peak * gain > ceiling ? ceiling / peak : 1.0f;
            if (needed < gain) { gain = needed; hold = holdSamples; }
            else if (hold > 0) --hold;
            else gain += (1.0f - gain) * release;
            if (gain < 0.9999f) for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= gain;
            worst = juce::jmin (worst, gain);
            if (++count >= 1024)
            {
                reduction.store (juce::Decibels::gainToDecibels (worst, -60.0f), std::memory_order_relaxed);
                worst = 1.0f; count = 0;
            }
        }
    }

private:
    bool enabled = false;
    float ceiling = 0.8414f, gain = 1.0f, release = 0.001f, worst = 1.0f;       // -1.5 dB on the samples keeps the true peak (between samples) at about -1
    int hold = 0, holdSamples = 384, count = 0;
    std::atomic<float> reduction { 0.0f }, inputPeak { 0.0f };
};
}
