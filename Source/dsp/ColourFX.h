#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mu
{
// Small second-order filter shared by the colour effects.
struct ColourBiquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = { 0, 0 }, z2[2] = { 0, 0 };
    void clear() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    float run (int c, float x) noexcept { const float y = b0 * x + z1[c]; z1[c] = b1 * x - a1 * y + z2[c]; z2[c] = b2 * x - a2 * y; return y; }
    void highPass (float hz, float q, double fs) noexcept;
    void lowPass (float hz, float q, double fs) noexcept;
    void peak (float hz, float q, float gainDb, double fs) noexcept;
    float magnitudeDb (float hz, double fs) const noexcept;
};

//==================================================================== Phone
struct PhoneSettings
{
    bool enabled = false;
    float amount = 1.0f;       // 0..1: how much of the effect replaces the clean sound
    float tone = 0.4f;         // 0 = narrow and boxy (an old phone), 1 = wider (a megaphone, a small radio)
    float drive = 0.35f;       // 0..1: how hard the little speaker is pushed
};

// The voice through a small speaker: a telephone, a megaphone, a radio. Everything below and above the middle
// of the voice is filtered off, the nasal middle is pushed, and the sound is overdriven a little, as a tiny
// speaker would do.
class Phone
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const PhoneSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    static float lowEdgeHz (float tone) noexcept { return 600.0f - 350.0f * juce::jlimit (0.0f, 1.0f, tone); }
    static float highEdgeHz (float tone) noexcept { return 2200.0f + 2800.0f * juce::jlimit (0.0f, 1.0f, tone); }
    float responseDb (float hz) const noexcept;        // for the display

private:
    void update() noexcept;
    double fs = 48000.0;
    int channels = 2;
    PhoneSettings settings;
    float shownTone = -1.0f, mixNow = 0.0f;
    ColourBiquad hp1, hp2, lp1, lp2, nose;
};

//==================================================================== Lo-fi
struct LoFiSettings
{
    bool enabled = false;
    float crush = 0.4f;        // 0..1: fewer samples a second and fewer steps of level, like an old sampler
    float wobble = 0.3f;       // 0..1: pitch drifting slowly up and down, like a worn tape
    float mix = 1.0f;          // 0..1
};

// Old-machine character: the sound is stored at a lower rate and with coarser steps, which adds grit and a dull,
// boxed-in top, and the pitch wavers slightly the way a tired tape does.
class LoFi
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const LoFiSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    static float rateHz (float crush, double fs) noexcept { return (float) fs * std::pow (4000.0f / (float) fs, juce::jlimit (0.0f, 1.0f, crush)); }
    static float bits (float crush) noexcept { return 16.0f - 10.0f * juce::jlimit (0.0f, 1.0f, crush); }

private:
    double fs = 48000.0;
    int channels = 2;
    LoFiSettings settings;
    std::array<std::vector<float>, 2> line;
    int pos = 0;
    float phase[2] = { 0.0f, 0.0f }, held[2] = { 0.0f, 0.0f }, lp[2] = { 0.0f, 0.0f };
    float wow = 0.0f, flutter = 0.0f, mixNow = 0.0f;
};

//==================================================================== Motion
struct MotionSettings
{
    bool enabled = false;
    int type = 0;              // 0 = chorus, 1 = flanger, 2 = phaser
    float rateHz = 0.6f;
    float depth = 0.5f;        // 0..1
    float mix = 0.5f;          // 0..1
};

// Movement effects. Chorus adds copies whose timing drifts, for width and shimmer. Flanger sweeps a very short
// echo up and down, the jet-plane whoosh. Phaser sweeps notches through the tone, a swirl.
class Motion
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const MotionSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getPhase() const noexcept { return shownPhase.load (std::memory_order_relaxed); }       // 0..1, for the display

private:
    double fs = 48000.0;
    int channels = 2;
    MotionSettings settings;
    std::array<std::vector<float>, 2> line;
    int pos = 0, lastType = -1;
    float lfo = 0.0f, feedback[2] = { 0.0f, 0.0f }, mixNow = 0.0f;
    float ap[2][6] = {};
    std::atomic<float> shownPhase { 0.0f };
};

//==================================================================== Chop
struct ChopSettings
{
    bool enabled = false;
    int rate = 3;              // see Chop::rateNames()
    float depth = 1.0f;        // 0..1: how far the sound drops in the gaps
    float shape = 0.7f;        // 0 = smooth pulse (tremolo), 1 = hard on and off (a gate)
    double bpm = 120.0;
};

// Chops the sound in time with the song: on for half of each step, off (or just quieter) for the other half.
// While the song plays the steps are locked to the beat grid, so the chops always land on the beat.
class Chop
{
public:
    void prepare (double sampleRate);
    void reset() noexcept { phase = 0.0; gain = 1.0f; }
    void setSettings (const ChopSettings& s) noexcept { settings = s; }
    // where the block starts in the song, in quarter notes; used only while the song is playing
    void setTimeline (bool hostPlaying, double quarterNotes) noexcept { playing = hostPlaying; ppq = quarterNotes; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray rateNames() { return { "1/4", "1/8", "1/8 triplet", "1/16", "1/32" }; }
    static float rateBeats (int index) noexcept
    {
        static const float beats[] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 0.125f };
        return beats[juce::jlimit (0, 4, index)];
    }
    static float gateAt (float phase01, float shape) noexcept;       // 0..1: how open the gate is at this point of a step
    float getPhase() const noexcept { return shownPhase.load (std::memory_order_relaxed); }

private:
    double fs = 48000.0, phase = 0.0, ppq = 0.0;
    bool playing = false;
    ChopSettings settings;
    float gain = 1.0f, smooth = 0.01f;
    std::atomic<float> shownPhase { 0.0f };
};
}
