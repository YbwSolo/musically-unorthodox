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
    float tone = 0.4f;         // 0 = narrow and boxy, 1 = wider
    float drive = 0.35f;       // 0..1: how hard the little speaker is pushed
    int mode = 0;              // see Phone::modeNames(). 0 is the sound every build up to 16 had.
    float noise = 0.0f;        // 0..1: the noise of the line or the airwaves, only there while the track is sounding
};

// The voice through something small and cheap. Each mode is a different device: where its band sits, where it
// rings, how it distorts when pushed, and what noise it makes.
//   Small speaker  - the original: a band-pass, a nasal push and soft overdrive
//   Telephone      - 300 to 3400 Hz as a phone line carries, a slightly lopsided overdrive, line hiss and the odd tick
//   Megaphone      - two horn resonances and hard clipping
//   Radio          - a wider band levelled by an automatic gain that breathes, static crackle, reception that fades
//   Walkie-talkie  - a narrow band clipped hard, with a burst of noise as each phrase ends
class Phone
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const PhoneSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    static juce::StringArray modeNames() { return { "Small speaker", "Telephone", "Megaphone", "Radio", "Walkie-talkie" }; }
    static float lowEdgeHz (float tone, int mode = 0) noexcept;
    static float highEdgeHz (float tone, int mode = 0) noexcept;
    float responseDb (float hz) const noexcept;        // for the display

private:
    void update() noexcept;
    float nextNoise() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    double fs = 48000.0;
    int channels = 2;
    PhoneSettings settings;
    float shownTone = -1.0f, mixNow = 0.0f;
    int shownMode = -1;
    ColourBiquad hp1, hp2, lp1, lp2, nose, horn;
    juce::uint32 seed = 0x2545F491u;
    float activity = 0.0f, quick = 0.0f, agc = 0.1f, noiseHp = 0.0f, crackle = 0.0f, burst = 0.0f, fade = 0.0f, noiseNow = 0.0f;
    bool wasActive = false;
};

//==================================================================== Lo-fi
struct LoFiSettings
{
    bool enabled = false;
    float crush = 0.4f;        // 0..1: fewer samples a second and fewer steps of level, like an old sampler
    float wobble = 0.3f;       // 0..1: pitch drifting slowly up and down, like a worn tape
    float mix = 1.0f;          // 0..1
    // the rest are off by default, which leaves the sound every build up to 16 had
    float drive = 0.0f;        // 0..1: tape and preamp saturation
    float noise = 0.0f;        // 0..1
    int noiseType = 0;         // see LoFi::noiseNames()
    float wear = 0.0f;         // 0..1: the level sagging and dropping out for a moment, as worn tape does
    float tone = 1.0f;         // 1 = open, 0 = everything above 1.5 kHz rolled away
};

// Old-machine character, as a chain of five small modules that each do one thing:
//   Drive  - saturation      Wobble - wow and flutter      Crush - lower rate and coarser steps
//   Wear   - sag and dropouts      Tone - the top rolled off      plus Noise (vinyl or tape) mixed in at the end
class LoFi
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const LoFiSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    static juce::StringArray noiseNames() { return { "Vinyl", "Tape" }; }
    static float rateHz (float crush, double fs) noexcept { return (float) fs * std::pow (4000.0f / (float) fs, juce::jlimit (0.0f, 1.0f, crush)); }
    static float bits (float crush) noexcept { return 16.0f - 10.0f * juce::jlimit (0.0f, 1.0f, crush); }
    static float toneHz (float tone) noexcept { return 20000.0f * std::pow (1500.0f / 20000.0f, 1.0f - juce::jlimit (0.0f, 1.0f, tone)); }

private:
    float nextNoise() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    double fs = 48000.0;
    int channels = 2;
    LoFiSettings settings;
    std::array<std::vector<float>, 2> line;
    int pos = 0;
    float phase[2] = { 0.0f, 0.0f }, held[2] = { 0.0f, 0.0f }, lp[2] = { 0.0f, 0.0f };
    float wow = 0.0f, flutter = 0.0f, mixNow = 0.0f;
    juce::uint32 seed = 0x9E3779B9u;
    ColourBiquad toneFilter, crackleBand;
    float shownTone = -1.0f, activity = 0.0f, noiseNow = 0.0f, hissLp[2] = { 0.0f, 0.0f }, rumble = 0.0f, crackle = 0.0f;
    float sag = 0.0f, dropDepth = 0.0f, wearGain = 1.0f;
    int dropLeft = 0, dropLength = 1;
};

//==================================================================== Motion
struct MotionSettings
{
    static constexpr int kPoints = 16;
    bool enabled = false;
    int type = 0;              // see Motion::typeNames()
    float rateHz = 0.6f;
    float depth = 0.5f;        // 0..1
    float mix = 0.5f;          // 0..1
    // the movement itself. Sine, free-running, is what every build up to 16 had.
    int shape = 0;             // see Motion::shapeNames(); the last one is the curve drawn on the FX tab
    std::array<float, kPoints> curve {};        // 0..1 at sixteen evenly spaced points of one cycle
    float smooth = 1.0f;       // drawn curve: 0 = sixteen flat steps, 1 = a line through the points
    bool sync = false;         // one cycle lasts a musical length instead of 1 / rate seconds
    int note = 2;              // see Motion::noteNames()
    double bpm = 120.0;
};

// Movement effects: something is swept up and down, and what moves it can be a plain wave or a curve you draw.
//   Chorus  - copies whose timing drifts: width and shimmer        Flanger - a very short echo swept: the jet whoosh
//   Phaser  - notches swept through the tone: a swirl               Filter  - the top end opened and closed
//   Pan     - the sound moved between left and right                Volume  - the level pumped down and back
class Motion
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setSettings (const MotionSettings& s) noexcept { settings = s; }
    void setTimeline (bool hostPlaying, double quarterNotes) noexcept { playing = hostPlaying; ppq = quarterNotes; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getPhase() const noexcept { return shownPhase.load (std::memory_order_relaxed); }       // 0..1, for the display

    static juce::StringArray typeNames() { return { "Chorus", "Flanger", "Phaser", "Filter", "Pan", "Volume" }; }
    static juce::StringArray shapeNames() { return { "Sine", "Triangle", "Ramp up", "Ramp down", "Square", "Drawn" }; }
    static juce::StringArray noteNames() { return { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" }; }
    static float noteBeats (int index) noexcept
    {
        static const float beats[] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
        return beats[juce::jlimit (0, 6, index)];
    }
    // where the movement is, 0..1, at a point 0..1 of its cycle
    static float valueAt (const MotionSettings& s, float phase01) noexcept;

private:
    double fs = 48000.0;
    int channels = 2;
    MotionSettings settings;
    std::array<std::vector<float>, 2> line;
    int pos = 0, lastType = -1;
    double phase = 0.0, slowPhase = 0.0, ppq = 0.0;
    bool playing = false;
    float feedback[2] = { 0.0f, 0.0f }, mixNow = 0.0f;
    float ap[2][6] = {};
    float glide[2] = { 0.5f, 0.5f }, glideSlow[2] = { 0.5f, 0.5f };
    float svLow[2] = { 0.0f, 0.0f }, svBand[2] = { 0.0f, 0.0f };
    std::atomic<float> shownPhase { 0.0f };
};

//==================================================================== Chop
struct ChopSettings
{
    static constexpr int kSteps = 16;
    bool enabled = false;
    int rate = 3;              // see Chop::rateNames(): the length of one step
    float depth = 1.0f;        // 0..1: how far the sound drops in the gaps
    float shape = 0.7f;        // 0 = smooth edges (tremolo), 1 = hard on and off (a gate)
    double bpm = 120.0;
    // Pulse (0) is what every build up to 16 had: on for half of each step, off for the other half.
    // Pattern (1) plays the sixteen steps below.
    int mode = 0;
    std::array<float, kSteps> steps {};         // 0..1: how loud each step is; 0 is a gap
    int length = 16;           // how many of the steps are played before the pattern starts again
    float gate = 1.0f;         // 0.1..1: how much of each step stays open
    float swing = 0.0f;        // 0..1: every second step played late
};

// Chops the sound in time with the song. Pulse switches it on and off evenly. Pattern is a sixteen-step
// sequencer: each step has its own level, so rhythms, stutters and trance gates can be built. While the song
// plays the steps are locked to the beat grid, so the chops always land on the beat.
class Chop
{
public:
    void prepare (double sampleRate);
    void reset() noexcept { phase = 0.0; gain = 1.0f; }
    void setSettings (const ChopSettings& s) noexcept { settings = s; }
    // where the block starts in the song, in quarter notes; used only while the song is playing
    void setTimeline (bool hostPlaying, double quarterNotes) noexcept { playing = hostPlaying; ppq = quarterNotes; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray modeNames() { return { "Pulse", "Pattern" }; }
    static juce::StringArray rateNames() { return { "1/4", "1/8", "1/8 triplet", "1/16", "1/32" }; }
    static float rateBeats (int index) noexcept
    {
        static const float beats[] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 0.125f };
        return beats[juce::jlimit (0, 4, index)];
    }
    static float gateAt (float phase01, float shape) noexcept;       // Pulse: 0..1, how open the gate is at this point of a step
    // Pattern: how open the gate is at this position, counted in steps from the start of the pattern
    static float patternAt (const ChopSettings& s, double positionInSteps) noexcept;
    float getPhase() const noexcept { return shownPhase.load (std::memory_order_relaxed); }       // Pulse: 0..1 of a step. Pattern: 0..1 of the pattern.

private:
    double fs = 48000.0, phase = 0.0, ppq = 0.0;
    bool playing = false;
    ChopSettings settings;
    float gain = 1.0f, smooth = 0.01f;
    std::atomic<float> shownPhase { 0.0f };
};
}
