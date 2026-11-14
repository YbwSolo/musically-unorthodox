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
    static constexpr int kFFTOrder = 12;               // 4096: 11.7 Hz bins at 48 k
    static constexpr int kHop = 1024;                  // 75 % overlap -> a new frame every 21 ms at 48 k
    static constexpr int kFFTSize = 1 << kFFTOrder;
    static constexpr int kNumBands = 32;               // 1/3-octave-ish bands 20 Hz .. 20 kHz
    static constexpr int kDisplayPoints = 320;

    struct Resonance { float freqHz; float prominenceDb; float q; };

    SpectrumAnalyzer();
    void prepare (double sampleRate);
    void reset();
    void resetLongTerm();                                              // message thread: restart the slow average
    void pushBlock (const juce::AudioBuffer<float>& buffer) noexcept;   // audio thread

    // Message thread. Returns true if a new frame was produced.
    bool update();
    int pump (int maxFrames = 16) { int n = 0; while (n < maxFrames && update()) ++n; return n; }

    const std::array<float, kDisplayPoints>& getDisplayDb() const { return displayDb; }
    const std::array<float, kNumBands>& getLongTermBandsDb() const { return longTermDb; }
    float bandCentreHz (int band) const;
    float displayPointHz (int i) const;
    float longTermBandDbAt (float hz) const;
    bool hasLongTerm() const { return longTermFrames > 40; }
    int getLongTermFrames() const { return longTermFrames; }

    // A listening pass: from startListen() on, every frame that has sound in it is kept, so the analysis can look
    // at a known stretch of the track (and at how each band behaves over that stretch) instead of whatever
    // happened to be playing in the last few seconds. Message thread.
    static constexpr int kListenMax = 1500;            // about 32 s of sound
    void startListen();
    void stopListen() { listening = false; }
    bool isListening() const { return listening; }
    float getListenSeconds() const { return (float) ((double) listenFrames * kHop / fs); }
    // meanDb: the average level of each band over the pass. swingDb: how much each band's share of the sound
    // moves from its usual value to its loud moments (a build-up that is always there scores low, one that comes
    // and goes with certain notes or words scores high). False if there is no pass to report.
    bool listenBands (std::array<float, kNumBands>& meanDb, std::array<float, kNumBands>& swingDb) const;

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
    juce::AbstractFifo fifo { kFFTSize * 8 };
    std::vector<float> fifoBuffer;
    std::vector<float> fftData;                        // 2 * kFFTSize
    std::array<float, kFFTSize / 2> binDb {};
    std::array<float, kDisplayPoints> displayDb {}, rawDb {};
    std::array<float, kNumBands> longTermDb {};
    std::array<float, kNumBands> bandLo {}, bandHi {};
    int longTermFrames = 0;
    std::array<float, kFFTSize / 2> longTermBinLinear {};
    bool listening = false;
    int listenFrames = 0;
    std::array<double, kNumBands> listenSum {};
    std::vector<float> listenShare;                    // kListenMax x kNumBands: each band's level against the frame's total
};
}
