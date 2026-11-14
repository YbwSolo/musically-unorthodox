#pragma once
#include "Pitch.h"

namespace mu
{
struct DoublerSettings
{
    bool enabled = false;
    float mix = 0.5f;          // 0..1, level of the doubles against the lead
    float detuneCents = 9.0f;  // how far the doubles sit from the lead in pitch
    float timing = 0.5f;       // 0..1, how loose the doubles are in time
    float width = 1.0f;        // 0..1, how far left and right they are placed
    int voices = 0;            // 0 = double (one each side), 1 = stack (two each side)
};

// Makes one vocal take sound like it was recorded two or more times. Each extra voice is a real copy of the
// performance, moved a few cents in pitch and a few milliseconds in time, with both drifting slowly the way a
// singer does between takes. The copies are placed left and right and the lead stays untouched in the middle.
class Doubler
{
public:
    static constexpr int kMaxVoices = 4;
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const DoublerSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getVoiceOffsetCents (int v) const noexcept { return shownCents[(size_t) v].load (std::memory_order_relaxed); }
    float getVoiceDelayMs (int v) const noexcept { return shownDelay[(size_t) v].load (std::memory_order_relaxed); }
    float getVoicePan (int v) const noexcept;
    int getNumVoices() const noexcept { return settings.voices > 0 ? 4 : 2; }

private:
    struct Wander
    {
        float value = 0.0f, goal = 0.0f; int countdown = 0;
        float next (juce::Random& rng, int interval, float smooth) noexcept
        {
            if (--countdown <= 0) { countdown = interval; goal = rng.nextFloat() * 2.0f - 1.0f; }
            value += (goal - value) * smooth;
            return value;
        }
    };
    double fs = 48000.0;
    int channels = 2;
    DoublerSettings settings;
    bool wasEnabled = false;
    PitchDetector detector;
    GrainEngine engine;
    std::array<GrainEngine::Voice, kMaxVoices> voices;
    std::array<Wander, kMaxVoices> pitchDrift, timeDrift;
    std::array<float, kMaxVoices> hp {}, lp {}, ratioNow {}, delayNow {}, gainL {}, gainR {};
    int tick = 0;
    juce::Random rng { 20261114 };
    bool voiced = false;
    float period = 240.0f, level = 0.0f, consonantGain = 1.0f;
    std::array<std::atomic<float>, kMaxVoices> shownCents {}, shownDelay {};
};
}
