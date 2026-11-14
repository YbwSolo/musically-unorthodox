#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mu
{
// Finds the note being sung. Works on one voice at a time (a vocal, a bass, a lead line), between about
// 65 Hz and 1 kHz, using the YIN method: it looks for the delay at which the wave best matches itself.
// A fresh answer arrives every 10 ms and describes the sound roughly 15 to 20 ms ago.
class PitchDetector
{
public:
    void prepare (double sampleRate);
    void reset();
    bool push (float monoSample) noexcept;                    // true when a new estimate has just been made
    float frequency() const noexcept { return freq; }         // Hz; 0 when there is no clear pitch
    float clarity() const noexcept { return clear; }          // 0..1, how periodic the sound is

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float run (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    void analyse() noexcept;

    double fs = 48000.0;
    float rate = 24000.0f;                 // after decimation
    int decim = 2, phase = 0;
    Biquad lp1, lp2;
    std::vector<float> ring, frame, diff;
    int ringPos = 0, total = 0, window = 720, tauMin = 24, tauMax = 369, hop = 240, since = 0;
    float freq = 0.0f, clear = 0.0f, lastGood = 0.0f;
    int jumpFrames = 0;
};

// Plays a sound back at a different pitch without changing its character. It cuts the incoming sound into
// overlapping two-period slices and lays them down closer together (higher) or further apart (lower). The
// slices themselves are not stretched, so the vowel sound (the formants) stays the same: no chipmunk effect.
// One engine can feed several voices, each with its own delay and pitch.
class GrainEngine
{
public:
    static constexpr int kMaxGrains = 8;
    struct Voice
    {
        struct Grain { double centre = 0, src = 0; float half = 1, gain = 1; };
        std::array<Grain, kMaxGrains> grains;
        int count = 0;
        double nextCentre = 0, prevSrc = 0;
        float nextHalf = 240.0f;
    };

    void prepare (int numChannels, double sampleRate);
    void reset();
    void resetVoice (Voice& v, float delaySamples) const noexcept;
    void write (int channel, float x) noexcept { ring[(size_t) channel][(size_t) (now & mask)] = x; }
    // Adds this voice's output for the sample just written into out[0..channels).
    void render (Voice& v, float periodSamples, bool voiced, float ratio, float delaySamples, float* out) noexcept;
    void advance() noexcept { ++now; }
    int numChannels() const noexcept { return channels; }

private:
    float read (int channel, double position) const noexcept;
    std::array<std::vector<float>, 2> ring;
    juce::int64 now = 0, mask = 0;
    int channels = 1;
    float minPeriod = 40.0f, maxPeriod = 800.0f;
};
}
