#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

namespace mu
{
// Audio thread pushes samples into a lock-free FIFO; the message thread (via update()) runs the FFT
// and keeps a smoothed display curve plus a slow long-term band average used by the analysis.
class SpectrumAnalyzer
{
public:
    static constexpr int kFFTOrder = 11;               // 2048
    static constexpr int kFFTSize = 1 << kFFTOrder;
    static constexpr int kNumBands = 32;               // 1/3-octave-ish bands 20 Hz .. 20 kHz
    static constexpr int kDisplayPoints = 256;

    struct Resonance { float freqHz; float prominenceDb; float q; };

    SpectrumAnalyzer();
    void prepare (double sampleRate);
    void reset();
    void pushBlock (const juce::AudioBuffer<float>& buffer) noexcept;   // audio thread

    // Message thread. Returns true if a new frame was produced.
    bool update();

    const std::array<float, kDisplayPoints>& getDisplayDb() const { return displayDb; }
    const std::array<float, kNumBands>& getLongTermBandsDb() const { return longTermDb; }
    float bandCentreHz (int band) const;
    float displayPointHz (int i) const;
    float longTermBandDbAt (float hz) const;
    bool hasLongTerm() const { return longTermFrames > 20; }

    // Finds narrow peaks sticking out of the smoothed long-term spectrum (for "Clean harsh").
    std::vector<Resonance> findResonances (float minHz, float maxHz, float minProminenceDb) const;

    // Spectral features used by the role detector (dB relative, 0 = no data).
    float lowEnergyRatio() const;    // energy below 150 Hz / total
    float centroidHz() const;
    float highEnergyRatio() const;   // energy above 5 kHz / total

    double getSampleRate() const { return fs; }

private:
    double fs = 48000.0;
    juce::dsp::FFT fft { kFFTOrder };
    juce::dsp::WindowingFunction<float> window { kFFTSize, juce::dsp::WindowingFunction<float>::hann };
    juce::AbstractFifo fifo { kFFTSize * 4 };
    std::vector<float> fifoBuffer;
    std::vector<float> fftData;                        // 2 * kFFTSize
    std::array<float, kFFTSize / 2> binDb {};
    std::array<float, kDisplayPoints> displayDb {};
    std::array<float, kNumBands> longTermDb {};
    std::array<float, kNumBands> bandLo {}, bandHi {};
    int longTermFrames = 0;
    std::array<float, kFFTSize / 2> longTermBinLinear {};
};
}
