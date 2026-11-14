#pragma once
#include "Pitch.h"

namespace mu
{
// One harmony voice, when the voices are set one by one.
struct HarmonyVoiceSettings
{
    bool on = false;
    int steps = 2;               // notes of the scale away from the lead: 2 is a third up, -3 a fourth down, 7 an octave up
    float level = 1.0f;          // 0..1
    float pan = 0.0f;            // -1 left .. 1 right
    float delayMs = 0.0f;        // 0..60: how far behind the lead this singer comes in
    float size = 0.0f;           // -1..1: a bigger, darker voice or a smaller, brighter one
    float detuneCents = 0.0f;    // -50..50
};

struct PitchFXSettings
{
    static constexpr int kParts = 4;

    // Shift: the whole voice moved up or down
    bool shiftOn = false;
    float semitones = -12.0f;    // -24..+24
    float character = 0.0f;      // -1..1: the size of the voice. 0 keeps it, negative makes it bigger and darker, positive smaller and brighter
    float shiftMix = 0.4f;       // 0..1: 1 replaces the voice, less than 1 layers the shifted voice under it
    int shiftMode = 0;           // see PitchFX::shiftModeNames(). 0 is what every build up to 16 had.
    float fineCents = 0.0f;      // -100..100, added to the semitones
    float shiftDrive = 0.0f;     // 0..1: saturation on the shifted voice
    float shiftWidth = 1.0f;     // 0..1: Wide only, how far apart the two copies sit

    // Harmony: extra voices singing along in the key of the song
    bool harmonyOn = false;
    int voicing = 0;             // see PitchFX::voicingNames()
    float harmonyMix = 0.45f;    // 0..1
    float harmonyWidth = 0.8f;   // 0..1
    float humanise = 0.4f;       // 0..1: how much the harmony voices differ from the lead in timing, tuning and tone
    int harmonyMode = 0;         // see PitchFX::harmonyModeNames(). 0 is what every build up to 16 had.
    std::array<HarmonyVoiceSettings, kParts> voice;
    bool leading = false;        // voice leading: a voice stays on its note while that still fits, instead of always moving with the lead
    float choir = 0.0f;          // 0..1: every voice becomes a small group of singers
    int chordMask = 0;           // Chords: the notes of the chord right now, twelve bits, bit 0 = C. 0 = no chord given.

    int key = 0, scale = 0, noteMask = 0xFFF;       // shared with Tune
};

// Two effects that need to know the note being sung, sharing one pitch detector and one grain engine.
//
// Shift moves the voice by a fixed amount (Shift), makes two copies moved opposite ways and spread left and
// right (Wide: a few cents each way is the classic thickener), or holds it on one note (Robot).
//
// Harmony adds voices that sing the same words on other notes. They can be picked as a ready-made voicing, set
// one by one (up to four, each with its own interval, level, place, delay, size and tuning), or made to sing the
// notes of a chord that changes through the song. With voice leading a voice stays where it is while its note
// still fits, the way singers do. Choir turns every voice into three singers.
class PitchFX
{
public:
    static constexpr int kHarmonyVoices = 3;                                 // the ready-made voicings use up to three
    static constexpr int kParts = PitchFXSettings::kParts, kSingers = 3;     // four parts, each one singer or (choir) three
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const PitchFXSettings& s) noexcept { settings = s; }
    // Chords: the chord at this moment of the song (twelve bits, bit 0 = C; 0 = none), given block by block by
    // whoever knows where the song is. -1 leaves the chord in the settings in charge.
    void setChord (int mask) noexcept { liveChord = mask; }
    void process (juce::AudioBuffer<float>& buffer) noexcept;

    static juce::StringArray voicingNames() { return { "Third up", "Fifth up", "Third and fifth", "Octave up", "Octave down", "Third below", "Choir" }; }
    static juce::StringArray harmonyModeNames() { return { "Voicing", "Voices", "Chords" }; }
    static juce::StringArray shiftModeNames() { return { "Shift", "Wide", "Robot" }; }
    static int numVoices (int voicing) noexcept;
    // The note a harmony voice sings when the lead sings "note": steps counts notes of the scale, octaves whole octaves.
    static int harmonyNote (int note, int voice, int voicing, int key, int scale, int noteMask) noexcept;
    // The note a number of scale steps above (or below) another. With no key chosen the steps are those of a plain scale.
    static int stepNote (int note, int steps, int key, int scale, int noteMask) noexcept;
    // Voice leading: the note a voice should take now, given where it was and where plain parallel motion would put it.
    static int ledNote (int previous, int parallel, int leadNote) noexcept;
    // Chords: the chord note nearest to "target", staying on "previous" when that is still in the chord and close
    // enough, and keeping off the notes in "taken" when there is a choice.
    static int chordNote (int chordMask, int target, int previous, const int* taken, int numTaken) noexcept;

    float getSungNote() const noexcept { return sungNote.load (std::memory_order_relaxed); }                 // 0 = no pitch
    int getHarmonyNote (int voice) const noexcept { return shownHarmony[(size_t) juce::jlimit (0, kParts - 1, voice)].load (std::memory_order_relaxed); }

private:
    double fs = 48000.0;
    int channels = 2;
    PitchFXSettings settings;
    bool wasOn = false;
    PitchDetector detector;
    GrainEngine engine;
    GrainEngine::Voice shiftVoice, shiftVoiceB;
    std::array<std::array<GrainEngine::Voice, kSingers>, kParts> harmony;
    std::array<float, kParts> ratio {}, ratioGoal {}, partLevel {};
    std::array<std::array<float, kSingers>, kParts> hp {}, driftPhase {}, driftCents {}, driftMs {};
    std::array<int, kParts> heldNote {};
    bool voiced = false;
    float period = 240.0f, periodNow = 240.0f, shiftLevel = 0.0f, harmonyLevel = 0.0f, consonantGain = 1.0f, choirNow = 0.0f, robotRatio = 1.0f;
    int driftCount = 0, unsureFrames = 0, lastMode = -1, liveChord = -1;
    bool snap = true;
    std::atomic<float> sungNote { 0.0f };
    std::array<std::atomic<int>, kParts> shownHarmony {};
};
}
