#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace mu
{
struct EchoSettings
{
    bool enabled = false;
    float mix = 0.22f;          // 0..1, level of the echoes under the dry sound
    bool sync = true;           // take the time from the song's tempo
    int note = 3;               // with sync: see Echo::noteNames()
    float timeMs = 250.0f;      // without sync
    float feedback = 0.35f;     // 0..0.9, how much of each echo goes round again
    float tone = 0.45f;         // 0 = dark, each repeat duller; 1 = bright
    float width = 0.7f;         // 0 = every echo in the middle; 1 = echoes bounce left, right, left
    float duck = 0.5f;          // 0..1, how far the echoes step back while the dry sound is playing
    double bpm = 120.0;
};

// A delay timed to the song. 60,000 divided by the tempo is one beat in milliseconds; the echo time is that
// multiplied by the note length (a quarter note is one beat, an eighth is half, a dotted eighth is three
// quarters). Each repeat loses some top and bottom so the echoes fall behind the voice, and they step back while
// the voice is sounding, so the words stay clear and the echoes fill the gaps.
class Echo
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const EchoSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray noteNames() { return { "1/2", "1/4", "1/8 dotted", "1/8", "1/8 triplet", "1/16" }; }
    static float noteBeats (int index) noexcept
    {
        static const float beats[] = { 2.0f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
        return beats[juce::jlimit (0, 5, index)];
    }
    static float syncedMs (double bpm, int note) noexcept { return (float) (60000.0 / juce::jlimit (40.0, 300.0, bpm)) * noteBeats (note); }
    float currentTimeMs() const noexcept { return juce::jlimit (10.0f, 2000.0f, settings.sync ? syncedMs (settings.bpm, settings.note) : settings.timeMs); }
    // how long the echoes take to fall 60 dB
    float tailSeconds() const noexcept
    {
        const float fb = juce::jlimit (0.0f, 0.9f, settings.feedback);
        const float repeats = fb > 0.01f ? -3.0f / std::log10 (fb) : 1.0f;
        return juce::jmin (20.0f, currentTimeMs() * 0.001f * (1.0f + repeats));
    }
    float getDuckDb() const noexcept { return duckMeter.load (std::memory_order_relaxed); }

private:
    struct Line
    {
        std::vector<float> data; int pos = 0;
        void resize (int samples) { data.assign ((size_t) juce::jmax (8, samples + 8), 0.0f); pos = 0; }
        void clear() { std::fill (data.begin(), data.end(), 0.0f); pos = 0; }
        float read (float back) const noexcept
        {
            const int size = (int) data.size();
            const int whole = (int) back; const float f = back - (float) whole;
            int i = pos - whole; if (i < 0) i += size;
            int j = i - 1; if (j < 0) j += size;
            return data[(size_t) i] * (1.0f - f) + data[(size_t) j] * f;
        }
        void write (float x) noexcept { data[(size_t) pos] = x; pos = pos + 1 == (int) data.size() ? 0 : pos + 1; }
    };
    double fs = 48000.0;
    int channels = 2;
    EchoSettings settings;
    bool wasEnabled = false;
    Line lines[2];
    float timeNow = 12000.0f, hp[2] = { 0.0f, 0.0f }, lp[2] = { 0.0f, 0.0f };
    float envelope = 0.0f, reference = 1.0e-4f, duckGain = 1.0f, mixNow = 0.0f;
    std::atomic<float> duckMeter { 0.0f };
    int meterCount = 0;
};
}
