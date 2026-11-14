#include "SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace mu
{
SpectrumAnalyzer::SpectrumAnalyzer()
{
    fifoBuffer.resize ((size_t) kFFTSize * 8, 0.0f);
    fftData.resize ((size_t) kFFTSize * 2, 0.0f);
    for (int b = 0; b < kNumBands; ++b)
    {
        const float lo = 20.0f * std::pow (1000.0f, (float) b / (float) kNumBands);
        const float hi = 20.0f * std::pow (1000.0f, (float) (b + 1) / (float) kNumBands);
        bandLo[(size_t) b] = lo; bandHi[(size_t) b] = hi;
    }
    reset();
}

void SpectrumAnalyzer::prepare (double sampleRate)
{
    fs = sampleRate;
    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo.reset();
    displayDb.fill (-100.0f);
    longTermDb.fill (-100.0f);
    longTermBinLinear.fill (0.0f);
    longTermFrames = 0;
}

void SpectrumAnalyzer::resetLongTerm()
{
    longTermBinLinear.fill (0.0f);
    longTermFrames = 0;
}

void SpectrumAnalyzer::pushBlock (const juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    if (n == 0 || ch == 0) return;
    int start1, size1, start2, size2;
    fifo.prepareToWrite (n, start1, size1, start2, size2);
    const float scale = 1.0f / (float) ch;
    auto writeRange = [&] (int start, int size, int offset)
    {
        for (int i = 0; i < size; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < ch; ++c) s += buffer.getSample (c, offset + i);
            fifoBuffer[(size_t) (start + i)] = s * scale;
        }
    };
    writeRange (start1, size1, 0);
    writeRange (start2, size2, size1);
    fifo.finishedWrite (size1 + size2);
}

bool SpectrumAnalyzer::update()
{
    if (fifo.getNumReady() < kFFTSize) return false;

    int start1, size1, start2, size2;
    fifo.prepareToRead (kFFTSize, start1, size1, start2, size2);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < size1; ++i) fftData[(size_t) i] = fifoBuffer[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i) fftData[(size_t) (size1 + i)] = fifoBuffer[(size_t) (start2 + i)];
    // 75 % overlap: consume one hop, so frames arrive often enough for a fluid display.
    fifo.finishedRead (kHop);

    window.multiplyWithWindowingTable (fftData.data(), kFFTSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const float norm = 2.0f / (float) kFFTSize;
    bool active = false;
    for (int i = 0; i < kFFTSize / 2; ++i)
    {
        const float mag = fftData[(size_t) i] * norm;
        const float db = juce::Decibels::gainToDecibels (mag, -120.0f);
        binDb[(size_t) i] = db;
        if (db > -80.0f) active = true;
    }

    // Display curve on a log axis. Below ~200 Hz the points are closer together than the FFT bins, so the
    // curve is interpolated between bins; higher up each point takes the strongest bin it covers.
    const float binsPerHz = (float) kFFTSize / (float) fs;
    for (int p = 0; p < kDisplayPoints; ++p)
    {
        const float hz = displayPointHz (p);
        const float hzNext = displayPointHz (juce::jmin (p + 1, kDisplayPoints - 1));
        const float binF = juce::jlimit (1.0f, (float) (kFFTSize / 2 - 2), hz * binsPerHz);
        const float binNext = juce::jlimit (1.0f, (float) (kFFTSize / 2 - 1), hzNext * binsPerHz);
        float v;
        if (binNext - binF < 1.0f)
        {
            const int b0 = (int) binF;
            const float t = binF - (float) b0;
            v = binDb[(size_t) b0] + t * (binDb[(size_t) b0 + 1] - binDb[(size_t) b0]);
        }
        else
        {
            v = -120.0f;
            for (int b = (int) binF; b <= (int) binNext; ++b) v = juce::jmax (v, binDb[(size_t) b]);
        }
        // +4.5 dB/oct tilt so a balanced mix reads roughly level across the display
        rawDb[(size_t) p] = v + 4.5f * std::log2 (hz / 1000.0f);
    }
    // Light smoothing across neighbouring points, then ballistics (fast up, slow down).
    for (int p = 0; p < kDisplayPoints; ++p)
    {
        const int a = juce::jmax (0, p - 2), b = juce::jmin (kDisplayPoints - 1, p + 2);
        float sum = 0.0f, wsum = 0.0f;
        for (int k = a; k <= b; ++k) { const float w = k == p ? 3.0f : (std::abs (k - p) == 1 ? 2.0f : 1.0f); sum += rawDb[(size_t) k] * w; wsum += w; }
        const float v = sum / wsum;
        float& d = displayDb[(size_t) p];
        d = v > d ? v * 0.5f + d * 0.5f : v * 0.05f + d * 0.95f;
    }

    // Long-term average (linear power domain, slow) only when there is signal.
    if (active)
    {
        ++longTermFrames;
        const float alpha = longTermFrames < 250 ? 1.0f / (float) longTermFrames : 0.004f;   // about a 5 s average
        for (int i = 0; i < kFFTSize / 2; ++i)
        {
            const float mag = fftData[(size_t) i] * norm;
            float& lt = longTermBinLinear[(size_t) i];
            lt += alpha * (mag * mag - lt);
        }
        for (int b = 0; b < kNumBands; ++b)
        {
            const int i0 = juce::jlimit (1, kFFTSize / 2 - 1, (int) (bandLo[(size_t) b] * (float) kFFTSize / (float) fs));
            const int i1 = juce::jlimit (i0, kFFTSize / 2 - 1, (int) (bandHi[(size_t) b] * (float) kFFTSize / (float) fs));
            float sum = 0.0f;
            for (int i = i0; i <= i1; ++i) sum += longTermBinLinear[(size_t) i];
            longTermDb[(size_t) b] = 10.0f * std::log10 (juce::jmax (1.0e-12f, sum));
        }
        if (listening && listenFrames < kListenMax)
        {
            std::array<float, kNumBands> power {};
            float total = 0.0f;
            for (int b = 0; b < kNumBands; ++b)
            {
                const int i0 = juce::jlimit (1, kFFTSize / 2 - 1, (int) (bandLo[(size_t) b] * (float) kFFTSize / (float) fs));
                const int i1 = juce::jlimit (i0, kFFTSize / 2 - 1, (int) (bandHi[(size_t) b] * (float) kFFTSize / (float) fs));
                float sum = 0.0f;
                for (int i = i0; i <= i1; ++i) { const float mag = fftData[(size_t) i] * norm; sum += mag * mag; }
                power[(size_t) b] = sum; total += sum;
            }
            // only frames with real sound in them count: the gaps between words say nothing about the tone
            if (total > 1.0e-8f)
            {
                for (int b = 0; b < kNumBands; ++b)
                {
                    listenSum[(size_t) b] += (double) power[(size_t) b];
                    listenShare[(size_t) (listenFrames * kNumBands + b)] = 10.0f * std::log10 (juce::jmax (1.0e-12f, power[(size_t) b]) / total);
                }
                ++listenFrames;
            }
        }
    }
    return true;
}

void SpectrumAnalyzer::startListen()
{
    listenShare.assign ((size_t) kListenMax * kNumBands, -120.0f);
    listenSum.fill (0.0);
    listenFrames = 0;
    listening = true;
}

bool SpectrumAnalyzer::listenBands (std::array<float, kNumBands>& meanDb, std::array<float, kNumBands>& swingDb) const
{
    if (listenFrames < 20 || listenShare.empty()) return false;
    std::vector<float> column ((size_t) listenFrames);
    for (int b = 0; b < kNumBands; ++b)
    {
        meanDb[(size_t) b] = (float) (10.0 * std::log10 (juce::jmax (1.0e-12, listenSum[(size_t) b] / (double) listenFrames)));
        for (int f = 0; f < listenFrames; ++f) column[(size_t) f] = listenShare[(size_t) (f * kNumBands + b)];
        std::sort (column.begin(), column.end());
        swingDb[(size_t) b] = column[(size_t) (listenFrames * 9 / 10)] - column[(size_t) (listenFrames / 2)];
    }
    return true;
}

float SpectrumAnalyzer::bandCentreHz (int band) const
{
    return std::sqrt (bandLo[(size_t) band] * bandHi[(size_t) band]);
}

float SpectrumAnalyzer::displayPointHz (int i) const
{
    return 20.0f * std::pow (1000.0f, (float) i / (float) (kDisplayPoints - 1));
}

float SpectrumAnalyzer::longTermBandDbAt (float hz) const
{
    for (int b = 0; b < kNumBands; ++b)
        if (hz < bandHi[(size_t) b]) return longTermDb[(size_t) b];
    return longTermDb[kNumBands - 1];
}

std::vector<SpectrumAnalyzer::Resonance> SpectrumAnalyzer::findResonances (float minHz, float maxHz, float minProminenceDb) const
{
    std::vector<Resonance> out;
    if (! hasLongTerm()) return out;

    const int half = kFFTSize / 2;
    std::vector<float> fine ((size_t) half, -120.0f), smooth ((size_t) half, -120.0f);
    for (int i = 1; i < half; ++i)
        fine[(size_t) i] = 10.0f * std::log10 (juce::jmax (1.0e-12f, longTermBinLinear[(size_t) i]));

    // 1/3-octave smoothing of the fine spectrum
    for (int i = 1; i < half; ++i)
    {
        const int w = juce::jmax (2, (int) (i * 0.12f));
        const int a = juce::jmax (1, i - w), b = juce::jmin (half - 1, i + w);
        float sum = 0.0f;
        for (int k = a; k <= b; ++k) sum += longTermBinLinear[(size_t) k];
        smooth[(size_t) i] = 10.0f * std::log10 (juce::jmax (1.0e-12f, sum / (float) (b - a + 1)));
    }

    const float binHz = (float) fs / (float) kFFTSize;
    const int iMin = juce::jmax (2, (int) (minHz / binHz)), iMax = juce::jmin (half - 2, (int) (maxHz / binHz));
    for (int i = iMin; i <= iMax; ++i)
    {
        const float prom = fine[(size_t) i] - smooth[(size_t) i];
        if (prom < minProminenceDb) continue;
        if (fine[(size_t) i] < fine[(size_t) i - 1] || fine[(size_t) i] < fine[(size_t) i + 1]) continue;
        if (fine[(size_t) i] < -70.0f) continue;
        // width at -3 dB from the peak (relative to the smooth floor) -> Q estimate
        int lo = i, hi = i;
        while (lo > iMin && fine[(size_t) lo] - smooth[(size_t) lo] > prom * 0.5f) --lo;
        while (hi < iMax && fine[(size_t) hi] - smooth[(size_t) hi] > prom * 0.5f) ++hi;
        const float bw = juce::jmax (1.0f, (float) (hi - lo)) * binHz;
        const float f = (float) i * binHz;
        out.push_back ({ f, prom, juce::jlimit (1.0f, 18.0f, f / bw) });
        i = hi;
    }
    std::sort (out.begin(), out.end(), [] (const Resonance& a, const Resonance& b) { return a.prominenceDb > b.prominenceDb; });
    if (out.size() > 6) out.resize (6);
    return out;
}

float SpectrumAnalyzer::lowEnergyRatio() const
{
    if (! hasLongTerm()) return 0.0f;
    const float binHz = (float) fs / (float) kFFTSize;
    float low = 0.0f, total = 0.0f;
    for (int i = 1; i < kFFTSize / 2; ++i)
    {
        const float v = longTermBinLinear[(size_t) i];
        total += v;
        if ((float) i * binHz < 150.0f) low += v;
    }
    return total > 0.0f ? low / total : 0.0f;
}

float SpectrumAnalyzer::highEnergyRatio() const
{
    if (! hasLongTerm()) return 0.0f;
    const float binHz = (float) fs / (float) kFFTSize;
    float high = 0.0f, total = 0.0f;
    for (int i = 1; i < kFFTSize / 2; ++i)
    {
        const float v = longTermBinLinear[(size_t) i];
        total += v;
        if ((float) i * binHz > 5000.0f) high += v;
    }
    return total > 0.0f ? high / total : 0.0f;
}

float SpectrumAnalyzer::centroidHz() const
{
    if (! hasLongTerm()) return 0.0f;
    const float binHz = (float) fs / (float) kFFTSize;
    float num = 0.0f, den = 0.0f;
    for (int i = 1; i < kFFTSize / 2; ++i)
    {
        const float v = longTermBinLinear[(size_t) i];
        num += v * (float) i * binHz;
        den += v;
    }
    return den > 0.0f ? num / den : 0.0f;
}
}
