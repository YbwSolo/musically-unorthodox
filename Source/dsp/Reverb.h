#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mu
{
struct ReverbSettings
{
    bool enabled = false;
    float mix = 0.2f;            // 0..1, how much reverb is added under the dry sound
    bool autoTime = true;        // take pre-delay and decay from the song's tempo
    int length = 1;              // with autoTime: 0 = one beat, 1 = two beats, 2 = four beats
    float decaySeconds = 1.2f;   // without autoTime
    float preDelayMs = 25.0f;    // without autoTime
    float tone = 0.5f;           // 0 = dark, 1 = bright
    float duck = 0.5f;           // 0..1, how far the reverb steps back while the dry sound is playing
    int type = 1;                // see Reverb::typeNames()
    float lowCutHz = 180.0f;     // the reverb carries nothing below this
    float width = 1.0f;          // 0 = the reverb sits in the middle, 1 = full stereo
    double bpm = 120.0;
    // advanced
    float size = 1.0f;           // 0.3..2: how big the space is (the spacing of its reflections), separate from how long it rings
    float diffusion = 0.8f;      // 0..1: how smeared the sound is on the way in. Low = grainy, separate echoes; high = smooth
    float modDepth = 0.3f;       // 0..1: slow movement inside the tail, which keeps long tails from ringing on one note
    float modRate = 0.6f;        // Hz
    float lowDecay = 1.0f;       // 0.3..2: the lows ring this many times as long as the mids
    float early = 0.5f;          // 0..1: balance between the first reflections and the tail (0.5 = as the space is designed)
    bool freeze = false;         // hold the tail as it is, for as long as this is on
    float shimmer = 0.5f;        // Shimmer only: how much of the tail is fed back an octave up
};

// Five different reverb engines, not one engine with nine names:
//   Room, Chamber, Hall, Cathedral, Ambient - a sixteen-line feedback network. The sixteen delay lines feed each
//     other through a mixing matrix that loses no energy, so the tail is dense and its length is exact; every line
//     moves slightly on its own slow cycle so no single ring can build up. The spaces differ in the spacing of the
//     lines, in their own pattern of first reflections, and in how the sound is smeared on the way in.
//   Plate - a figure-of-eight tank (after Dattorro's published plate design): two loops of all-pass filters and
//     delays feeding each other, tapped at many points. No separate reflections; dense from the first instant.
//   Vintage - an eight-line network with a band-limited input and deep, audible movement, the way the first
//     digital reverbs sounded.
//   Shimmer - the hall network with part of its tail fed back in an octave higher.
//   Gated - not a ringing tank at all: a dense burst of reflections that holds its level and then stops dead.
// In all of them highs fade faster than mids (Tone), lows can fade slower or faster (Low decay), and with auto timing
// the gap before the reverb (pre-delay) is a 64th note and the decay is one, two or four beats of the song.
class Reverb
{
public:
    enum Type { Room = 0, Plate, Hall, Chamber, Cathedral, Ambient, Vintage, Shimmer, Gated, NumTypes };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const ReverbSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray typeNames() { return { "Room", "Plate", "Hall", "Chamber", "Cathedral", "Ambient", "Vintage", "Shimmer", "Gated" }; }
    static const char* typeBlurb (int type) noexcept;
    static float beatMs (double bpm) noexcept { return (float) (60000.0 / juce::jlimit (40.0, 300.0, bpm)); }
    static float autoPreDelayMs (double bpm) noexcept { return juce::jlimit (5.0f, 80.0f, beatMs (bpm) / 16.0f); }             // a 64th note
    static float autoDecaySeconds (double bpm, int length) noexcept
    {
        const float beats = length <= 0 ? 1.0f : (length == 1 ? 2.0f : 4.0f);
        return juce::jlimit (0.25f, 6.0f, beats * beatMs (bpm) * 0.001f);
    }
    // Gated: how long the burst lasts for a given decay setting
    static float gateSeconds (float decaySeconds) noexcept { return juce::jlimit (0.08f, 1.0f, decaySeconds * 0.3f); }
    float currentDecaySeconds() const noexcept { return settings.autoTime ? autoDecaySeconds (settings.bpm, settings.length) : settings.decaySeconds; }
    float currentPreDelayMs() const noexcept { return settings.autoTime ? autoPreDelayMs (settings.bpm) : settings.preDelayMs; }
    float getDuckDb() const noexcept { return duckMeter.load (std::memory_order_relaxed); }
    float getWetLevelDb() const noexcept { return wetMeter.load (std::memory_order_relaxed); }

private:
    static constexpr int kLines = 16;
    static constexpr int kEarly = 8;
    static constexpr int kGateTaps = 80;
    struct Delay
    {
        std::vector<float> data; int pos = 0;
        void resize (int samples) { data.assign ((size_t) juce::jmax (8, samples + 8), 0.0f); pos = 0; }
        void clear() { std::fill (data.begin(), data.end(), 0.0f); pos = 0; }
        float read (int back) const noexcept { int i = pos - back; if (i < 0) i += (int) data.size(); return data[(size_t) i]; }
        float readFrac (float back) const noexcept
        {
            const int whole = (int) back; const float f = back - (float) whole;
            return read (whole) * (1.0f - f) + read (whole + 1) * f;
        }
        void write (float x) noexcept { data[(size_t) pos] = x; pos = pos + 1 == (int) data.size() ? 0 : pos + 1; }
    };
    struct Allpass
    {
        Delay line; float length = 1.0f, g = 0.6f;
        float run (float x) noexcept { const float d = line.read ((int) length); const float v = x + g * d; line.write (v); return d - g * v; }
        float runFrac (float x, float len) noexcept { const float d = line.readFrac (len); const float v = x + g * d; line.write (v); return d - g * v; }
    };
    // one feedback path: what a trip round it takes off the mids, the highs and the lows
    struct Fade { float gain = 0.0f, pole = 0.0f, state = 0.0f, lowExtra = 0.0f, lowState = 0.0f; };
    static void setFade (Fade& f, float tripSeconds, float decay, float highDecay, float lowDecay, bool frozen) noexcept;
    float fade (Fade& f, float x) const noexcept
    {
        f.lowState += (x - f.lowState) * lowCoeff;
        x += f.lowExtra * f.lowState;
        f.state = f.gain * x + f.pole * f.state;
        return f.state;
    }

    void update() noexcept;
    void applyType() noexcept;
    void layout() noexcept;
    void network (const float* feed, float& wetL, float& wetR) noexcept;
    void plate (float in, float& wetL, float& wetR) noexcept;
    void gated (const float* feed, float& wetL, float& wetR) noexcept;
    float shimmerVoice (float x) noexcept;

    double fs = 48000.0;
    int channels = 2;
    ReverbSettings settings;
    bool wasEnabled = false;
    int type = -1;

    // on the way in
    Delay pre[2];
    std::array<Allpass, 6> diffuse[2];
    int diffuseCount = 4;
    float inHp[2] = { 0.0f, 0.0f }, inLp[2] = { 0.0f, 0.0f }, inLpCoeff = 0.0f;
    float earlyTime[2][kEarly] {}, earlyGain[kEarly] {};
    int earlyCount = 0;

    // the feedback network
    std::array<Delay, kLines> lines;
    std::array<float, kLines> lengths {}, lfoSin {}, lfoCos {}, lfoStep {};
    std::array<Fade, kLines> fades;
    int lineCount = kLines;
    float modSamples = 0.0f;

    // the plate tank
    Allpass plateIn[4], tankAp1[2], tankAp2[2];
    Delay tankDelay1[2], tankDelay2[2];
    Fade tankFade[2];
    float tankLen1[2] {}, tankLen2[2] {}, tankApLen1[2] {}, tankApLen2[2] {}, tankGain[2] = { 0.5f, 0.5f }, tankOut[2] = { 0.0f, 0.0f };
    float tapScale = 1.0f;

    // shimmer
    std::vector<float> shimmerBuffer; int shimmerPos = 0; float shimmerPhase = 0.0f, shimmerLp = 0.0f, shimmerLp2 = 0.0f, shimmerHp = 0.0f, shimmerFeed = 0.0f, shimmerCoeff = 0.4f;

    // gated
    Delay gateLine[2];
    float gateTime[2][kGateTaps] {}, gateGain[2][kGateTaps] {};
    float shownGate = -1.0f;

    // wet path: low cut, tone, duck
    float hpState[2][2] {}, hpCoeff = 0.0f, lpState[2] = { 0.0f, 0.0f }, lpCoeff = 0.0f, lowCoeff = 0.0f;
    float envelope = 0.0f, reference = 1.0e-4f, duckGain = 1.0f, mixNow = 0.0f, preSamples = 1.0f, inputGain = 1.0f;
    float shownDecay = -1.0f, shownTone = -1.0f, shownLowCut = -1.0f, shownSize = -1.0f, shownLowDecay = -1.0f, shownDiffusion = -1.0f, shownEarly = -1.0f;
    bool shownFreeze = false;
    float earlyLevel = 0.35f, lateLevel = 0.30f, highShare = 1.0f;
    std::atomic<float> duckMeter { 0.0f }, wetMeter { -100.0f };
    float wetPeak = 0.0f; int meterCount = 0;
};
}
