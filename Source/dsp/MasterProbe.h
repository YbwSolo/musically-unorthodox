#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include "EQEngine.h"

namespace mu
{
// What a listening pass over a mix measured, apart from loudness and tone (which the loudness meter and the
// analyser measure). Every figure here is a measurement of the samples, not an opinion about them.
struct ProbeFacts
{
    float seconds = 0.0f;                      // of sound, silence left out
    float samplePeakDb = -144.0f, rmsDb = -144.0f, crestDb = 0.0f;       // crest: the tallest sample against the average level
    float punchDb = 0.0f;                      // how far the hits stand above the level around them: 50 ms peaks against 400 ms levels, averaged
    float correlation = 1.0f, lowestCorrelation = 1.0f;                   // +1 = the same both sides, 0 = unrelated, -1 = cancels in mono
    std::array<float, 4> sideToMidDb { -60.0f, -60.0f, -60.0f, -60.0f };  // width in four ranges: under 150 Hz, to 800 Hz, to 5 kHz, above
    float widthDb = -60.0f;                    // side against mid over the whole range
    float monoLossDb = 0.0f;                   // how much quieter the mix is when summed to mono
    float lowSwingDb = 0.0f;                   // how much the 40 to 120 Hz range moves from moment to moment (spread of its level, in dB)
    float levelSwingDb = 0.0f;                 // the same for the whole mix, to compare it with
    float lowTypicalDb = -120.0f, lowLoudDb = -120.0f;   // the 40 to 120 Hz range: its usual level and its loud notes (dBFS, average over 400 ms)
    int flatTops = 0;                          // runs of three or more samples stuck at the top: the mix was clipped before it got here
    float loudnessRangeLu = 0.0f, maxShortTermLufs = -100.0f, maxMomentaryLufs = -100.0f;
};

// Collects those figures on the audio thread while a pass is running. Nothing is allocated or locked there:
// each block is added to a pending total, and handed over whenever the reader is not holding the lock.
class MasterProbe
{
public:
    void prepare (double sampleRate, int numChannels);
    void start();                              // message thread: begin a pass (clears everything)
    void stop() { running.store (false); }
    bool isRunning() const { return running.load(); }
    // audio thread. shortTermLufs and momentaryLufs come from the loudness meter that is fed the same sound.
    void process (const juce::AudioBuffer<float>& buffer, float shortTermLufs, float momentaryLufs) noexcept;
    ProbeFacts read() const;                   // message thread
    float secondsHeard() const;

private:
    struct Totals
    {
        double ll = 0, rr = 0, lr = 0, mono = 0; std::array<double, 4> mm {}, ss {};
        double samples = 0, activeWindows = 0, punchSum = 0; float peak = 0; int flatTops = 0;
        float lowestCorr = 1.0f, maxShort = -100.0f, maxMomentary = -100.0f;
    };
    static constexpr int kMaxWindows = 6000;   // 400 ms each: 40 minutes
    static constexpr int kMaxLoudness = 3600;  // one a second: an hour
    void merge() noexcept;

    double fs = 48000.0;
    int channels = 2;
    std::atomic<bool> running { false };
    std::atomic<int> restart { 0 };
    int seenRestart = 0;
    // filters for the four ranges, on the middle and on the sides, and for the low end
    BiquadCoeffs lp150 {}, hp150 {}, lp800 {}, hp800 {}, lp5k {}, hp5k {}, hp40 {}, lp120 {};
    struct Chain { Biquad a, b, c, d, e, f; } midF, sideF;
    Biquad lowA, lowB;
    // the window being filled
    int winPos = 0, winLen = 19200, subPos = 0, subLen = 2400, runLength = 0, loudTick = 0;
    double winSq = 0, winLowSq = 0, winLL = 0, winRR = 0, winLR = 0; float subPeak = 0, winPeakSum = 0; int winSubs = 0; float lastSample = 0;
    Totals pending;
    std::array<float, 16> pendLow {}, pendLevel {}; int pendWindows = 0;
    std::array<float, 8> pendLoud {}; int pendLoudCount = 0;
    // shared with the reader
    mutable juce::SpinLock lock;
    Totals total;
    std::vector<float> lowDb, levelDb, shortTerm;
};
}
