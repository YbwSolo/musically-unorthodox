#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace mu
{
// ITU-R BS.1770-4 loudness: momentary (400 ms), short-term (3 s), gated integrated,
// plus sample peak and 4x-oversampled true peak.
// process() is real-time safe; integrated loudness is derived from a histogram that the
// message thread reads through readIntegrated().
class LoudnessMeter
{
public:
    static constexpr int kHistBins = 800;           // -80 .. 0 LUFS in 0.1 LU steps
    static constexpr int kSubBlocksShort = 30;      // 30 x 100 ms = 3 s

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void process (const juce::AudioBuffer<float>& buffer);

    float getMomentary()  const noexcept { return momentary.load (std::memory_order_relaxed); }
    float getShortTerm()  const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float getIntegrated() const noexcept { return integrated.load (std::memory_order_relaxed); }
    float getTruePeakHold() const noexcept { return truePeakHold.load (std::memory_order_relaxed); }
    float getTruePeakCurrent() const noexcept { return truePeakCurrent.load (std::memory_order_relaxed); }
    float getSamplePeakHold() const noexcept { return samplePeakHold.load (std::memory_order_relaxed); }
    float getCrestFactorDb() const noexcept { return crestFactor.load (std::memory_order_relaxed); }
    bool  isActive() const noexcept { return getMomentary() > -55.0f; }

    // Called from the message thread (cheap). Recomputes gated integrated loudness from the histogram.
    void readIntegrated();
    void resetPeakHold() { truePeakHold.store (-144.0f); samplePeakHold.store (-144.0f); }
    void resetIntegrated();

    // K-weighting coefficients are public so the test runner can verify them.
    static juce::dsp::IIR::Coefficients<float>::Ptr makeShelf (double fs);
    static juce::dsp::IIR::Coefficients<float>::Ptr makeHighPass (double fs);

private:
    void finishSubBlock();

    double fs = 48000.0;
    int channels = 2;
    int subBlockLen = 4800;      // 100 ms
    int subBlockPos = 0;
    double subBlockSum = 0.0;    // sum of channel-summed k-weighted squares

    std::array<juce::dsp::IIR::Filter<float>, 2> shelf, highPass;
    juce::AudioBuffer<float> kBuffer, osBuffer;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    std::array<float, kSubBlocksShort> ring {};   // mean-square per 100 ms sub-block
    int ringPos = 0;
    int ringFilled = 0;

    std::array<std::atomic<int>, kHistBins> histogram {};
    std::atomic<int> histTotal { 0 };

    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };
    std::atomic<float> truePeakHold { -144.0f }, truePeakCurrent { -144.0f }, samplePeakHold { -144.0f };
    std::atomic<float> crestFactor { 0.0f };
    float blockPeakLin = 0.0f;
    double blockSquares = 0.0;
    int blockCount = 0;
};
}
