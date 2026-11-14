#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "LoudnessMeter.h"

namespace mu
{
// How loud the track is where it enters the plugin, over the last three seconds, on the same scale as the
// loudness meter (K-weighted, LUFS). The tools that work to a threshold (compressor, de-esser, gate, rider) are
// fitted to this, not to the level at the output, which already has their own work in it.
class LevelProbe
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
        subLength = juce::jmax (1, (int) (fs * 0.1));
        for (int c = 0; c < 2; ++c) { shelf[(size_t) c].coefficients = LoudnessMeter::makeShelf (fs); highPass[(size_t) c].coefficients = LoudnessMeter::makeHighPass (fs); }
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { shelf[(size_t) c].reset(); highPass[(size_t) c].reset(); }
        ring.fill (0.0f); at = 0; filled = 0; count = 0; sum = 0.0;
        level.store (-100.0f);
    }
    void process (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < ch; ++c)
            {
                const float v = highPass[(size_t) c].processSample (shelf[(size_t) c].processSample (buffer.getSample (c, i)));
                sum += (double) v * v;
            }
            if (++count >= subLength)
            {
                const float power = (float) (sum / (double) count);
                ring[(size_t) at] = power; at = (at + 1) % (int) ring.size(); filled = juce::jmin (filled + 1, (int) ring.size());
                count = 0; sum = 0.0;
                // only the stretches with sound in them: the gaps between phrases are not part of how loud a voice is
                double total = 0.0; int used = 0;
                for (int k = 0; k < filled; ++k) if (ring[(size_t) k] > 1.0e-7f) { total += ring[(size_t) k]; ++used; }
                level.store (used >= 3 ? (float) (-0.691 + 10.0 * std::log10 (total / used)) : -100.0f, std::memory_order_relaxed);
            }
        }
    }
    float getLevel() const noexcept { return level.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0, sum = 0.0;
    int channels = 2, subLength = 4800, count = 0, at = 0, filled = 0;
    std::array<juce::dsp::IIR::Filter<float>, 2> shelf, highPass;
    std::array<float, 30> ring {};
    std::atomic<float> level { -100.0f };
};
}
