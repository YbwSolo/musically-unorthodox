#include "Clipper.h"
#include <cmath>

namespace mu
{
void Clipper::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) channels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) maxBlockSize);
    latency = (int) std::lround (os->getLatencyInSamples());
    work.setSize (channels, maxBlockSize);
    for (auto& d : delay) d.assign ((size_t) juce::jmax (1, latency), 0.0f);
    reset();
}

void Clipper::reset()
{
    if (os != nullptr) os->reset();
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    delayPos = 0; wet = enabled ? 1.0f : 0.0f; meter.store (0.0f);
}

// Straight up to where the knee starts, then bending over so that it never quite reaches the threshold.
float Clipper::shape (float x, float t, float soft) noexcept
{
    const float a = std::abs (x), start = t * (1.0f - 0.5f * soft);
    if (a <= start) return x;
    const float room = t - start;
    const float y = room > 1.0e-6f ? start + room * std::tanh ((a - start) / room) : t;
    return x < 0.0f ? -y : y;
}

void Clipper::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (! active || n == 0 || ch == 0 || os == nullptr || n > work.getNumSamples()) { meter.store (0.0f, std::memory_order_relaxed); return; }

    const bool running = enabled || wet > 0.0f;
    float deepest = 0.0f;
    if (running)
    {
        for (int c = 0; c < ch; ++c) work.copyFrom (c, 0, buffer, c, 0, n);
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), (size_t) ch, (size_t) n);
        auto up = os->processSamplesUp (block);
        const float t = threshold, soft = softness;
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float x = d[i], y = shape (x, t, soft);
                if (y != x) deepest = juce::jmin (deepest, 20.0f * std::log10 (std::abs (y) / std::abs (x)));
                d[i] = y;
            }
        }
        os->processSamplesDown (block);
    }
    else os->reset();

    // the untouched sound, delayed to match; while switching, the two are crossed over 10 ms
    const float step = 1.0f / (float) (fs * 0.010), goal = enabled ? 1.0f : 0.0f;
    const int len = (int) delay[0].size();
    float w = wet; int pos = delayPos;
    for (int c = 0; c < ch; ++c)
    {
        w = wet; pos = delayPos;
        auto* d = buffer.getWritePointer (c);
        auto& line = delay[(size_t) c];
        const float* clipped = work.getReadPointer (c);
        for (int i = 0; i < n; ++i)
        {
            float dry = d[i];
            if (latency > 0) { const float old = line[(size_t) pos]; line[(size_t) pos] = dry; dry = old; pos = pos + 1 == len ? 0 : pos + 1; }
            w = goal > w ? juce::jmin (goal, w + step) : juce::jmax (goal, w - step);
            d[i] = running ? dry + w * (clipped[i] - dry) : dry;
        }
    }
    wet = w; delayPos = pos;
    meter.store (deepest, std::memory_order_relaxed);
}
}
