#pragma once
#include "Pitch.h"

namespace mu
{
struct PitchFXSettings
{
    // Shift: the whole voice moved up or down
    bool shiftOn = false;
    float semitones = -12.0f;    // -12..+12
    float character = 0.0f;      // -1..1: the size of the voice. 0 keeps it, negative makes it bigger and darker, positive smaller and brighter
    float shiftMix = 0.4f;       // 0..1: 1 replaces the voice, less than 1 layers the shifted voice under it

    // Harmony: extra voices singing along in the key of the song
    bool harmonyOn = false;
    int voicing = 0;             // see PitchFX::voicingNames()
    float harmonyMix = 0.45f;    // 0..1
    float harmonyWidth = 0.8f;   // 0..1

    int key = 0, scale = 0, noteMask = 0xFFF;       // shared with Tune
};

// Two effects that need to know the note being sung, sharing one pitch detector and one grain engine.
// Shift moves the voice by a fixed number of semitones. Harmony adds up to three voices that sing the same
// words on other notes of the scale (a third above, a fifth above, an octave below), which is how a stacked
// harmony is recorded, without the extra takes. The harmony voices keep the size of the singer's voice, so
// they sound like the same person.
class PitchFX
{
public:
    static constexpr int kHarmonyVoices = 3;
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const PitchFXSettings& s) noexcept { settings = s; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray voicingNames() { return { "Third up", "Fifth up", "Third and fifth", "Octave up", "Octave down", "Third below", "Choir" }; }
    static int numVoices (int voicing) noexcept;
    // The note a harmony voice sings when the lead sings "note": steps counts notes of the scale, octaves whole octaves.
    static int harmonyNote (int note, int voice, int voicing, int key, int scale, int noteMask) noexcept;

    float getSungNote() const noexcept { return sungNote.load (std::memory_order_relaxed); }                 // 0 = no pitch
    int getHarmonyNote (int voice) const noexcept { return shownHarmony[(size_t) juce::jlimit (0, kHarmonyVoices - 1, voice)].load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    PitchFXSettings settings;
    bool wasOn = false;
    PitchDetector detector;
    GrainEngine engine;
    GrainEngine::Voice shiftVoice;
    std::array<GrainEngine::Voice, kHarmonyVoices> harmony;
    std::array<float, kHarmonyVoices> ratio {}, ratioGoal {}, hp {};
    bool voiced = false;
    float period = 240.0f, shiftLevel = 0.0f, harmonyLevel = 0.0f, consonantGain = 1.0f;
    std::atomic<float> sungNote { 0.0f };
    std::array<std::atomic<int>, kHarmonyVoices> shownHarmony {};
};
}
