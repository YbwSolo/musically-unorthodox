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
    double bpm = 120.0;
};

// A clean reverb built for vocals. Eight delay lines feed each other through a mixing matrix that loses no
// energy, so the tail is dense and smooth and its length is set exactly: each line is turned down by just
// enough per trip to fall 60 dB in the chosen decay time. Highs fade faster than lows, as they do in a room.
// With auto timing the gap before the reverb (pre-delay) is a 64th note and the decay is one, two or four beats
// of the song: 60,000 divided by the tempo is the length of one beat in milliseconds.
class Reverb
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const ReverbSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static float beatMs (double bpm) noexcept { return (float) (60000.0 / juce::jlimit (40.0, 300.0, bpm)); }
    static float autoPreDelayMs (double bpm) noexcept { return juce::jlimit (5.0f, 80.0f, beatMs (bpm) / 16.0f); }             // a 64th note
    static float autoDecaySeconds (double bpm, int length) noexcept
    {
        const float beats = length <= 0 ? 1.0f : (length == 1 ? 2.0f : 4.0f);
        return juce::jlimit (0.25f, 6.0f, beats * beatMs (bpm) * 0.001f);
    }
    float currentDecaySeconds() const noexcept { return settings.autoTime ? autoDecaySeconds (settings.bpm, settings.length) : settings.decaySeconds; }
    float currentPreDelayMs() const noexcept { return settings.autoTime ? autoPreDelayMs (settings.bpm) : settings.preDelayMs; }
    float getDuckDb() const noexcept { return duckMeter.load (std::memory_order_relaxed); }
    float getWetLevelDb() const noexcept { return wetMeter.load (std::memory_order_relaxed); }

private:
    static constexpr int kLines = 8;
    struct Delay
    {
        std::vector<float> data; int pos = 0, length = 1;
        void resize (int samples) { data.assign ((size_t) juce::jmax (4, samples + 4), 0.0f); pos = 0; }
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
        Delay line; int length = 1; float g = 0.6f;
        float run (float x) noexcept { const float d = line.read (length); const float v = x + g * d; line.write (v); return d - g * v; }
    };
    void update() noexcept;

    double fs = 48000.0;
    int channels = 2;
    ReverbSettings settings;
    bool wasEnabled = false;
    Delay pre[2];
    std::array<Allpass, 4> diffuse[2];
    std::array<Delay, kLines> lines;
    std::array<int, kLines> lengths {};
    std::array<float, kLines> gainLow {}, pole {}, state {};
    float lfoPhase = 0.0f, lfoStep = 0.0f;
    // wet path: low cut, then duck
    float hpState[2] = { 0.0f, 0.0f }, hpCoeff = 0.0f, lpState[2] = { 0.0f, 0.0f }, lpCoeff = 0.0f;
    float inHp[2] = { 0.0f, 0.0f };
    float envelope = 0.0f, reference = 1.0e-4f, duckGain = 1.0f, mixNow = 0.0f, preSamples = 1.0f;
    float shownDecay = -1.0f, shownTone = -1.0f;
    std::atomic<float> duckMeter { 0.0f }, wetMeter { -100.0f };
    float wetPeak = 0.0f; int meterCount = 0;
};
}
