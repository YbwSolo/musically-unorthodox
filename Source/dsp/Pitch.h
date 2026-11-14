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
    float freq = 0.0f, clear = 0.0f, lastGood = 0.0f, pending = 0.0f;
    int heldFrames = 0;
};

// Plays a sound back at a different pitch without changing its character. It cuts the incoming sound into
// overlapping two-period slices and lays them down closer together (higher) or further apart (lower). The
// slices themselves are not stretched, so the vowel sound (the formants) stays the same: no chipmunk effect.
// One engine can feed several voices, each with its own delay and pitch.
class GrainEngine
{
public:
    static constexpr int kMaxGrains = 24;
    struct Voice
    {
        struct Grain { double centre = 0, src = 0; float half = 1, gain = 1, rate = 1; };
        std::array<Grain, kMaxGrains> grains;
        int count = 0;
        double nextCentre = 0, prevSrc = 0;
        float nextHalf = 240.0f;
        bool locked = false;          // renderSync: the last slice was centred on one of the voice's own pulses
    };

    void prepare (int numChannels, double sampleRate);
    void reset();
    void resetVoice (Voice& v, float delaySamples) const noexcept;
    void write (int channel, float x) noexcept { ring[(size_t) channel][(size_t) (now & mask)] = x; }
    // Adds this voice's output for the sample just written into out[0..channels).
    // formant: 1 leaves the character of the voice alone; above 1 makes it smaller and brighter, below 1 bigger and
    // darker. It works by reading each slice faster or slower while the slices stay the same distance apart.
    void render (Voice& v, float periodSamples, bool voiced, float ratio, float delaySamples, float* out, float formant = 1.0f) noexcept;
    // The same job done properly for a voice. A sung or spoken note is a train of pulses from the vocal folds, one
    // per period. render() cuts its slices wherever the clock says, so a slice may land on a pulse or between two,
    // and as the pitch estimate drifts the cut point wanders: the tone goes from clear to smeared and back, which
    // is heard as a warble or a phasey, uneasy quality, worst on big shifts such as harmonies. renderSync() finds
    // the pulses and centres every slice on one, so each slice always holds one whole pulse and its ring. That is
    // how pitch-synchronous overlap-add is meant to be done.
    void renderSync (Voice& v, float periodSamples, bool voiced, float ratio, float delaySamples, float* out, float formant = 1.0f) noexcept;
    void advance() noexcept;
    int numChannels() const noexcept { return channels; }

private:
    float read (int channel, double position) const noexcept;
    double findPulse (double around, float reach, float preferNear) const noexcept;
    void checkPolarity (double around, float period) noexcept;
    std::array<std::vector<float>, 2> ring;
    std::vector<float> pulse;              // the sound with everything above about 1 kHz removed: its peaks are the pulses
    float lp1 = 0.0f, lp2 = 0.0f, hp = 0.0f, lpCoeff = 0.1f, hpCoeff = 0.99f, polarity = 1.0f;
    int polarityVotes = 0, sincePolarity = 0;
    juce::int64 now = 0, mask = 0;
    int channels = 1;
    float minPeriod = 40.0f, maxPeriod = 800.0f;
};
}
