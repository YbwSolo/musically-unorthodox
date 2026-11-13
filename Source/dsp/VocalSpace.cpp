#include "VocalSpace.h"
#include <cmath>

namespace mu
{
void VocalSpace::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    reset();
}

void VocalSpace::reset()
{
    depth = 0.0f;
    for (auto& b : states) for (auto& s : b) s = {};
    for (int i = 0; i < kBands; ++i)
        coeffs[(size_t) i] = EQEngine::computeCoefficients ({ true, EQType::Bell, freqs[(size_t) i], 0.0f, qs[(size_t) i] }, fs);
}

void VocalSpace::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    const float targetDepth = settings.enabled ? key : 0.0f;
    const float attMs = settings.speed == 0 ? 60.0f : settings.speed == 1 ? 25.0f : 8.0f;
    const float relMs = settings.speed == 0 ? 400.0f : settings.speed == 1 ? 180.0f : 80.0f;
    const int step = 32;
    const float attCoef = std::exp (-(float) step / (float) (fs * 0.001 * attMs));
    const float relCoef = std::exp (-(float) step / (float) (fs * 0.001 * relMs));

    for (int start = 0; start < n; start += step)
    {
        const int len = juce::jmin (step, n - start);
        depth = targetDepth > depth ? attCoef * depth + (1.0f - attCoef) * targetDepth
                                    : relCoef * depth + (1.0f - relCoef) * targetDepth;
        if (depth < 0.002f && ! settings.enabled) { depth = 0.0f; return; }

        for (int b = 0; b < kBands; ++b)
        {
            const float gain = maxCut[(size_t) b] * settings.amount * depth;
            coeffs[(size_t) b] = EQEngine::computeCoefficients ({ true, EQType::Bell, freqs[(size_t) b], gain, qs[(size_t) b] }, fs);
            const auto& c = coeffs[(size_t) b];
            for (int chn = 0; chn < ch; ++chn)
            {
                auto* d = buffer.getWritePointer (chn) + start;
                auto& st = states[(size_t) b][(size_t) chn];
                for (int i = 0; i < len; ++i)
                {
                    const float x = d[i];
                    const float y = c[0] * x + st.z1;
                    st.z1 = c[1] * x - c[3] * y + st.z2;
                    st.z2 = c[2] * x - c[4] * y;
                    d[i] = y;
                }
            }
        }
    }
}
}
