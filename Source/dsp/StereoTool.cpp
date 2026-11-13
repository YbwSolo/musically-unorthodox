#include "StereoTool.h"
#include <cmath>

namespace mu
{
void StereoTool::prepare (double sampleRate, int, int)
{
    fs = sampleRate;
    widthSmooth.reset (fs, 0.05);
    bassSmooth.reset (fs, 0.05);
    widthSmooth.setCurrentAndTargetValue (settings.width);
    bassSmooth.setCurrentAndTargetValue (settings.monoBass ? 1.0f : 0.0f);
    hpHz = -1.0f;
    reset();
}

void StereoTool::reset()
{
    for (auto& f : sideHp) f.reset();
    sumLR = sumLL = sumRR = 0.0;
}

void StereoTool::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) { correlation.store (1.0f, std::memory_order_relaxed); return; }

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    widthSmooth.setTargetValue (juce::jlimit (0.0f, 2.0f, settings.width));
    bassSmooth.setTargetValue (settings.monoBass ? 1.0f : 0.0f);
    if (std::abs (settings.monoBelowHz - hpHz) > 0.5f)
    {
        hpHz = settings.monoBelowHz;
        hpCoeffs = EQEngine::computeCoefficients ({ true, EQType::LowCut, hpHz, 0.0f, 0.7071f }, fs);
    }

    const bool active = widthSmooth.isSmoothing() || bassSmooth.isSmoothing()
                        || std::abs (widthSmooth.getTargetValue() - 1.0f) > 1.0e-4f || bassSmooth.getTargetValue() > 0.0f;
    const double decay = std::exp (-(double) n / (fs * 0.3));
    double lr = 0.0, ll = 0.0, rr = 0.0;

    if (active)
    {
        for (int i = 0; i < n; ++i)
        {
            const float w = widthSmooth.getNextValue();
            const float b = bassSmooth.getNextValue();
            const float m = 0.5f * (l[i] + r[i]);
            float s = 0.5f * (l[i] - r[i]);
            // two cascaded 12 dB stages on the side signal, blended in so switching never clicks
            const float sHp = sideHp[1].process (sideHp[0].process (s, hpCoeffs), hpCoeffs);
            s = (s + (sHp - s) * b) * w;
            l[i] = m + s; r[i] = m - s;
        }
    }
    for (int i = 0; i < n; ++i) { lr += (double) l[i] * r[i]; ll += (double) l[i] * l[i]; rr += (double) r[i] * r[i]; }
    sumLR = decay * sumLR + (1.0 - decay) * lr;
    sumLL = decay * sumLL + (1.0 - decay) * ll;
    sumRR = decay * sumRR + (1.0 - decay) * rr;
    const double den = std::sqrt (sumLL * sumRR);
    correlation.store (den > 1.0e-12 ? (float) juce::jlimit (-1.0, 1.0, sumLR / den) : 1.0f, std::memory_order_relaxed);
}
}
