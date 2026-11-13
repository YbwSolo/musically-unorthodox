#include "Saturator.h"
#include <cmath>

namespace mu
{
float Saturator::shape (float x, int type, float drive) noexcept
{
    const float d = juce::jmax (1.0f, drive);
    const float in = x * d;
    float y;
    switch (type)
    {
        case 1: // tube: asymmetric, gentle even harmonics
        {
            const float pos = in >= 0.0f ? in : in * 0.85f;
            y = pos / (1.0f + std::abs (pos)) * 1.6f;
            y = std::tanh (y);
            break;
        }
        case 2: // warm: cubic soft knee, mostly odd harmonics, very polite
        {
            const float c = juce::jlimit (-1.5f, 1.5f, in);
            y = c - (c * c * c) / 6.75f;
            y = std::tanh (y * 0.9f);
            break;
        }
        case 0: // tape
        default:
            y = std::tanh (in) + 0.03f * std::tanh (in * 0.5f) * std::tanh (in * 0.5f);
            break;
    }
    // Level compensation: normalise so a -12 dBFS sine keeps roughly its level.
    const float ref = 0.25f * d;
    const float refOut = type == 0 ? std::tanh (ref) : type == 1 ? std::tanh (ref / (1.0f + ref) * 1.6f) : std::tanh ((ref - ref * ref * ref / 6.75f) * 0.9f);
    return y * (0.25f / juce::jmax (1.0e-4f, refOut));
}

void Saturator::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    os = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    os->initProcessing ((size_t) maxBlockSize);
    latency = juce::jmax (1, (int) std::round (os->getLatencyInSamples()));
    for (auto& d : dryDelay) d.assign ((size_t) latency, 0.0f);
    dry.setSize (2, maxBlockSize, false, false, true);
    driveSmooth.reset (fs, 0.05);
    mixSmooth.reset (fs, 0.05);
    reset();
}

void Saturator::reset()
{
    if (os) os->reset();
    for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
    dryPos = 0;
    dcState.fill (0.0f); dcPrev.fill (0.0f);
    wetWasActive = false;
}

void Saturator::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0 || os == nullptr) return;

    mixSmooth.setTargetValue (settings.enabled ? juce::jlimit (0.0f, 1.0f, settings.mix) : 0.0f);
    driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (settings.driveDb));
    const bool wetActive = settings.enabled || mixSmooth.getCurrentValue() > 0.001f;

    // Dry path: a pure delay of the oversampler's latency, so the plugin's latency never changes
    // and the oversampler only costs CPU while saturation is actually in use.
    for (int c = 0; c < ch; ++c)
    {
        auto& line = dryDelay[(size_t) c];
        const float* in = buffer.getReadPointer (c);
        float* out = dry.getWritePointer (c);
        int pos = dryPos;
        for (int i = 0; i < n; ++i)
        {
            out[i] = line[(size_t) pos];
            line[(size_t) pos] = in[i];
            pos = pos + 1 == latency ? 0 : pos + 1;
        }
    }
    dryPos = (dryPos + n) % latency;

    if (! wetActive)
    {
        for (int c = 0; c < ch; ++c) buffer.copyFrom (c, 0, dry, c, 0, n);
        mixSmooth.skip (n); driveSmooth.skip (n);
        wetWasActive = false;
        return;
    }
    if (! wetWasActive) { os->reset(); wetWasActive = true; }

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) ch, 0, (size_t) n);
    auto up = os->processSamplesUp (block);
    const float drive = driveSmooth.getTargetValue();
    for (size_t c = 0; c < up.getNumChannels(); ++c)
    {
        auto* d = up.getChannelPointer (c);
        for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shape (d[i], settings.type, drive);
    }
    os->processSamplesDown (block);
    driveSmooth.skip (n);

    // DC blocker (tube mode is asymmetric) + dry/wet
    const float R = 1.0f - (float) (2.0 * juce::MathConstants<double>::pi * 8.0 / fs);
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSmooth.getNextValue();
        for (int c = 0; c < ch; ++c)
        {
            float* d = buffer.getWritePointer (c) + i;
            const float x = *d;
            const float y = x - dcPrev[(size_t) c] + R * dcState[(size_t) c];
            dcPrev[(size_t) c] = x; dcState[(size_t) c] = y;
            *d = y * mix + dry.getSample (c, i) * (1.0f - mix);
        }
    }
}
}
