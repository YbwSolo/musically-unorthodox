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
    osDry = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    os->initProcessing ((size_t) maxBlockSize);
    osDry->initProcessing ((size_t) maxBlockSize);
    dry.setSize (2, maxBlockSize, false, false, true);
    driveSmooth.reset (fs, 0.05);
    mixSmooth.reset (fs, 0.05);
    reset();
}

void Saturator::reset()
{
    if (os) os->reset();
    if (osDry) osDry->reset();
    dcState.fill (0.0f); dcPrev.fill (0.0f);
}

void Saturator::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0 || os == nullptr) return;

    mixSmooth.setTargetValue (settings.enabled ? juce::jlimit (0.0f, 1.0f, settings.mix) : 0.0f);
    driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (settings.driveDb));
    const bool wetActive = settings.enabled || mixSmooth.getCurrentValue() > 0.001f;

    // Dry path always goes up and down so the plugin's latency never changes.
    for (int c = 0; c < ch; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);
    juce::dsp::AudioBlock<float> dryBlock (dry.getArrayOfWritePointers(), (size_t) ch, 0, (size_t) n);
    osDry->processSamplesUp (dryBlock);
    osDry->processSamplesDown (dryBlock);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) ch, 0, (size_t) n);
    auto up = os->processSamplesUp (block);
    if (wetActive)
    {
        const float drive = driveSmooth.getTargetValue();
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shape (d[i], settings.type, drive);
        }
    }
    os->processSamplesDown (block);
    driveSmooth.skip (n);
    if (! wetActive)
    {
        for (int c = 0; c < ch; ++c) buffer.copyFrom (c, 0, dry, c, 0, n);
        mixSmooth.skip (n);
        return;
    }

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
