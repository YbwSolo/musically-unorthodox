#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace mu
{
struct KeyEstimate
{
    int key = 0;                 // 0 = C ... 11 = B
    bool minor = false;
    float confidence = 0.0f;     // 0..1, how well the notes fit the key
    float secondsHeard = 0.0f;
};

// The key that best fits how much each of the twelve notes was heard (Krumhansl-Kessler key profiles).
KeyEstimate keyFromNotes (const float* twelve) noexcept;

// A key written in a track or file name: "beat 128 bpm Gmin", "Dark keys F# minor", "loop_Am_90".
// Returns false when the name does not state one.
bool keyFromName (const juce::String& name, int& key, bool& minor);

// Works out the key of music with many notes at once (a beat, a full mix, chords), which a single-voice pitch
// detector cannot do. It measures how much energy sits on each of the twelve notes, a few times a second, and
// adds it up. Cheap enough to leave running.
class KeyFinder
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void push (const float* left, const float* right, int numSamples) noexcept;     // audio thread; right may be null
    KeyEstimate estimate() const noexcept;                                           // any thread

private:
    void analyse() noexcept;

    static constexpr int kOrder = 12, kSize = 1 << kOrder;
    double fs = 48000.0;
    float rate = 12000.0f;
    int decim = 4, phase = 0, fill = 0;
    float lpA = 0.0f, lp1 = 0.0f, lp2 = 0.0f, loudest = 0.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring, work, window;
    std::array<std::atomic<float>, 12> chroma {};
    std::atomic<float> frames { 0.0f };
};
}
