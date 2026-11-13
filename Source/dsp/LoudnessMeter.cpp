#include "LoudnessMeter.h"
#include <cmath>

namespace mu
{
juce::dsp::IIR::Coefficients<float>::Ptr LoudnessMeter::makeShelf (double fs)
{
    const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
    const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
    const double Vh = std::pow (10.0, G / 20.0);
    const double Vb = std::pow (Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;
    const double b0 = (Vh + Vb * K / Q + K * K) / a0;
    const double b1 = 2.0 * (K * K - Vh) / a0;
    const double b2 = (Vh - Vb * K / Q + K * K) / a0;
    const double a1 = 2.0 * (K * K - 1.0) / a0;
    const double a2 = (1.0 - K / Q + K * K) / a0;
    return new juce::dsp::IIR::Coefficients<float> ((float) b0, (float) b1, (float) b2, 1.0f, (float) a1, (float) a2);
}

juce::dsp::IIR::Coefficients<float>::Ptr LoudnessMeter::makeHighPass (double fs)
{
    const double f0 = 38.13547087602444, Q = 0.5003270373238773;
    const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
    const double a0 = 1.0 + K / Q + K * K;
    const double a1 = 2.0 * (K * K - 1.0) / a0;
    const double a2 = (1.0 - K / Q + K * K) / a0;
    return new juce::dsp::IIR::Coefficients<float> (1.0f, -2.0f, 1.0f, 1.0f, (float) a1, (float) a2);
}

void LoudnessMeter::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    subBlockLen = juce::jmax (1, (int) std::round (fs * 0.1));
    kBuffer.setSize (2, maxBlockSize, false, false, true);
    for (auto& f : shelf)    f.coefficients = makeShelf (fs);
    for (auto& f : highPass) f.coefficients = makeHighPass (fs);
    reset();
}

void LoudnessMeter::reset()
{
    for (auto& f : shelf) f.reset();
    for (auto& f : highPass) f.reset();
    for (auto& d : tpDetectors) d.reset();
    subBlockPos = 0; subBlockSum = 0.0;
    ring.fill (0.0f); ringPos = 0; ringFilled = 0;
    momentary.store (-100.0f); shortTerm.store (-100.0f);
    truePeakCurrent.store (-144.0f);
    blockPeakLin = 0.0f; blockSquares = 0.0; blockCount = 0;
    resetIntegrated();
}

void LoudnessMeter::resetIntegrated()
{
    for (auto& h : histogram) h.store (0, std::memory_order_relaxed);
    histTotal.store (0);
    integrated.store (-100.0f);
}

void LoudnessMeter::process (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    // --- sample peak + true peak
    float peak = 0.0f;
    for (int c = 0; c < ch; ++c)
        peak = juce::jmax (peak, buffer.getMagnitude (c, 0, n));
    samplePeakHold.store (juce::jmax (samplePeakHold.load (std::memory_order_relaxed), juce::Decibels::gainToDecibels (peak, -144.0f)), std::memory_order_relaxed);

    float tp = peak;
    for (int c = 0; c < ch; ++c)
    {
        const float* d = buffer.getReadPointer (c);
        auto& det = tpDetectors[(size_t) c];
        for (int i = 0; i < n; ++i) tp = juce::jmax (tp, det.process (d[i]));
        channelPeak[(size_t) c].store (juce::Decibels::gainToDecibels (buffer.getMagnitude (c, 0, n), -144.0f), std::memory_order_relaxed);
    }
    if (ch == 1) channelPeak[1].store (channelPeak[0].load (std::memory_order_relaxed), std::memory_order_relaxed);
    const float tpDb = juce::Decibels::gainToDecibels (tp, -144.0f);
    truePeakCurrent.store (tpDb, std::memory_order_relaxed);
    truePeakHold.store (juce::jmax (truePeakHold.load (std::memory_order_relaxed), tpDb), std::memory_order_relaxed);

    // --- K-weighting into kBuffer
    for (int c = 0; c < ch; ++c)
    {
        kBuffer.copyFrom (c, 0, buffer, c, 0, n);
        auto* d = kBuffer.getWritePointer (c);
        for (int i = 0; i < n; ++i)
            d[i] = highPass[(size_t) c].processSample (shelf[(size_t) c].processSample (d[i]));
    }

    // --- accumulate 100 ms sub-blocks
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int c = 0; c < ch; ++c)
        {
            const float v = kBuffer.getSample (c, i);
            s += (double) v * v;
        }
        subBlockSum += s;
        if (++subBlockPos >= subBlockLen)
            finishSubBlock();
    }

    // crest factor over roughly the last second (unweighted)
    double sq = 0.0;
    for (int c = 0; c < ch; ++c)
    {
        auto* d = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) sq += (double) d[i] * d[i];
    }
    blockSquares += sq / (double) juce::jmax (1, ch);
    blockPeakLin = juce::jmax (blockPeakLin, peak);
    blockCount += n;
    if (blockCount >= (int) fs)
    {
        const double rms = std::sqrt (blockSquares / (double) blockCount);
        if (rms > 1.0e-6)
            crestFactor.store ((float) (20.0 * std::log10 (blockPeakLin / rms)), std::memory_order_relaxed);
        blockSquares = 0.0; blockPeakLin = 0.0f; blockCount = 0;
    }
}

void LoudnessMeter::finishSubBlock()
{
    const float ms = (float) (subBlockSum / (double) subBlockLen);
    subBlockSum = 0.0; subBlockPos = 0;

    ring[(size_t) ringPos] = ms;
    ringPos = (ringPos + 1) % kSubBlocksShort;
    ringFilled = juce::jmin (ringFilled + 1, kSubBlocksShort);

    auto meanOfLast = [this] (int count)
    {
        count = juce::jmin (count, ringFilled);
        if (count <= 0) return 0.0f;
        float sum = 0.0f;
        for (int k = 1; k <= count; ++k)
            sum += ring[(size_t) ((ringPos - k + kSubBlocksShort) % kSubBlocksShort)];
        return sum / (float) count;
    };

    auto toLufs = [] (float meanSquare) { return meanSquare > 1.0e-12f ? -0.691f + 10.0f * std::log10 (meanSquare) : -100.0f; };

    const float mom = toLufs (meanOfLast (4));
    momentary.store (mom, std::memory_order_relaxed);
    shortTerm.store (toLufs (meanOfLast (kSubBlocksShort)), std::memory_order_relaxed);

    if (ringFilled >= 4 && mom > -70.0f)
    {
        const int bin = juce::jlimit (0, kHistBins - 1, (int) std::round ((mom + 80.0f) * 10.0f));
        histogram[(size_t) bin].fetch_add (1, std::memory_order_relaxed);
        histTotal.fetch_add (1, std::memory_order_relaxed);
    }
}

void LoudnessMeter::readIntegrated()
{
    if (histTotal.load() == 0) { integrated.store (-100.0f); return; }

    auto binToLufs = [] (int b) { return (float) b * 0.1f - 80.0f; };
    auto lufsToMs  = [] (float l) { return std::pow (10.0f, (l + 0.691f) / 10.0f); };

    double sum = 0.0; long count = 0;
    for (int b = 0; b < kHistBins; ++b)
    {
        const int c = histogram[(size_t) b].load (std::memory_order_relaxed);
        if (c == 0) continue;
        sum += (double) c * lufsToMs (binToLufs (b));
        count += c;
    }
    if (count == 0) { integrated.store (-100.0f); return; }
    const float relGate = (-0.691f + 10.0f * std::log10 ((float) (sum / (double) count))) - 10.0f;

    sum = 0.0; count = 0;
    for (int b = 0; b < kHistBins; ++b)
    {
        const float l = binToLufs (b);
        if (l <= relGate) continue;
        const int c = histogram[(size_t) b].load (std::memory_order_relaxed);
        if (c == 0) continue;
        sum += (double) c * lufsToMs (l);
        count += c;
    }
    integrated.store (count > 0 ? -0.691f + 10.0f * std::log10 ((float) (sum / (double) count)) : -100.0f);
}
}
