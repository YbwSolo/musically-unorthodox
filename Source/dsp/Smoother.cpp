#include "Smoother.h"
#include <cmath>

namespace mu
{
static constexpr float kBandQ = 5.0f;
static constexpr int kStep = 32;

void Smoother::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (int i = 0; i < kBands; ++i)
    {
        freqs[(size_t) i] = juce::jmin ((float) (fs * 0.45), 200.0f * std::pow (2.0f, (float) i / 4.0f));
        detCoeffs[(size_t) i] = EQEngine::computeBandPass (freqs[(size_t) i], kBandQ, fs);
    }
    reset();
}

void Smoother::reset()
{
    for (auto& d : detectors) d.reset();
    for (auto& b : filters) for (auto& f : b) f.reset();
    energy.fill (0.0f); cutDb.fill (0.0f); appliedDb.fill (0.0f);
    for (int i = 0; i < kBands; ++i)
    {
        cutCoeffs[(size_t) i] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        cutMeter[(size_t) i].store (0.0f);
    }
    totalMeter.store (0.0f);
}

void Smoother::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    bool anyActive = false;
    for (float c : appliedDb) anyActive = anyActive || c < -0.05f;
    if (! settings.enabled && ! anyActive)
    {
        totalMeter.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const float depth = settings.enabled ? juce::jlimit (0.0f, 1.0f, settings.depth) : 0.0f;
    const float threshold = 7.0f - 5.0f * depth;            // dB a band must stand above its neighbours
    const float maxCut = 3.0f + 9.0f * depth;
    const int lo = settings.focus == 2 ? 13 : 0;             // harsh highs: from about 1.9 kHz
    const int hi = settings.focus == 1 ? 15 : kBands - 1;    // low mids: up to about 2.7 kHz
    const float envMs = settings.speed == 0 ? 60.0f : settings.speed == 1 ? 25.0f : 10.0f;
    const float relMs = settings.speed == 0 ? 300.0f : settings.speed == 1 ? 140.0f : 60.0f;
    const float envCoef = std::exp (-1.0f / (float) (fs * 0.001 * envMs));
    const float attCoef = std::exp (-(float) kStep / (float) (fs * 0.001 * 8.0f));
    const float relCoef = std::exp (-(float) kStep / (float) (fs * 0.001 * relMs));

    float total = 0.0f;
    for (int start = 0; start < n; start += kStep)
    {
        const int len = juce::jmin (kStep, n - start);

        // 1. band energies of the incoming audio
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < ch; ++c) s += buffer.getSample (c, start + i);
            s /= (float) ch;
            for (int b = 0; b < kBands; ++b)
            {
                const float v = detectors[(size_t) b].process (s, detCoeffs[(size_t) b]);
                energy[(size_t) b] = envCoef * energy[(size_t) b] + (1.0f - envCoef) * v * v;
            }
        }

        // 2. how far each band stands out from the bands around it
        std::array<float, kBands> db;
        for (int b = 0; b < kBands; ++b) db[(size_t) b] = 10.0f * std::log10 (energy[(size_t) b] + 1.0e-12f);
        for (int b = 0; b < kBands; ++b)
        {
            float wanted = 0.0f;
            if (b >= lo && b <= hi && db[(size_t) b] > -70.0f && depth > 0.0f)
            {
                float sum = 0.0f; int count = 0;
                for (int k = -3; k <= 3; ++k)
                {
                    if (k == 0 || std::abs (k) == 1) continue;        // skip self and the bands the peak leaks into
                    const int j = b + k;
                    if (j < 0 || j >= kBands) continue;
                    sum += db[(size_t) j]; ++count;
                }
                if (count > 0)
                {
                    const float excess = db[(size_t) b] - sum / (float) count - threshold;
                    if (excess > 0.0f) wanted = -juce::jmin (maxCut, excess * 0.75f);
                }
            }
            float& c = cutDb[(size_t) b];
            c = wanted < c ? attCoef * c + (1.0f - attCoef) * wanted : relCoef * c + (1.0f - relCoef) * wanted;
            if (c > -0.02f && wanted == 0.0f) c = 0.0f;

            if (std::abs (c - appliedDb[(size_t) b]) > 0.1f || (c == 0.0f && appliedDb[(size_t) b] != 0.0f))
            {
                appliedDb[(size_t) b] = c;
                if (c == 0.0f) { cutCoeffs[(size_t) b] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f }; for (auto& f : filters[(size_t) b]) f.reset(); }
                else cutCoeffs[(size_t) b] = EQEngine::computeCoefficients ({ true, EQType::Bell, freqs[(size_t) b], c, kBandQ }, fs);
            }
            cutMeter[(size_t) b].store (appliedDb[(size_t) b], std::memory_order_relaxed);
            total = juce::jmin (total, appliedDb[(size_t) b]);
        }

        // 3. apply the cuts
        for (int b = 0; b < kBands; ++b)
        {
            if (appliedDb[(size_t) b] == 0.0f) continue;
            const auto& co = cutCoeffs[(size_t) b];
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c) + start;
                auto& f = filters[(size_t) b][(size_t) c];
                for (int i = 0; i < len; ++i) d[i] = f.process (d[i], co);
            }
        }
    }
    totalMeter.store (total, std::memory_order_relaxed);
}
}
