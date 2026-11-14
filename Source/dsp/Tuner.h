#pragma once
#include "Pitch.h"

namespace mu
{
struct TunerSettings
{
    bool enabled = false;
    int key = 0;               // 0 = C ... 11 = B
    int scale = 0;             // see Tuner::scaleNames()
    float speedMs = 40.0f;     // how quickly the pitch is pulled to the note; 0 = instantly (the hard-tune sound)
    float amount = 1.0f;       // 0..1, how much of the correction is applied
    float keepVibrato = 0.6f;  // 0..1: 1 corrects only the centre of each note and leaves its movement alone
    bool lowLatency = false;
};

// Pitch correction for a single voice. It hears which note is being sung, picks the nearest note of the chosen
// key and scale, and moves the voice there with the grain engine, so the tone of the voice is unchanged.
// Natural settings correct the centre of each note and leave vibrato and slides in place; a speed of zero snaps
// every note flat onto the scale, which is the well-known hard-tune effect.
class Tuner
{
public:
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const TunerSettings& s) noexcept;
    void process (juce::AudioBuffer<float>& buffer, bool passThrough = false) noexcept;   // passThrough: delay only
    int getLatencySamples() const noexcept { return settings.enabled && ! settings.lowLatency ? delay : 0; }

    // for the display (any thread)
    float getDetectedNote() const noexcept { return detectedNote.load (std::memory_order_relaxed); }    // MIDI note with fraction, 0 = no pitch
    float getOutputNote() const noexcept { return outputNote.load (std::memory_order_relaxed); }
    int getTargetNote() const noexcept { return targetNote.load (std::memory_order_relaxed); }

    // Key finding: which notes have been sung so far, and the key that fits them best.
    struct KeyGuess { int key = 0; bool minor = false; float confidence = 0.0f; float secondsHeard = 0.0f; };
    KeyGuess guessKey() const noexcept;
    void forgetNotes() noexcept;

    static juce::StringArray scaleNames() { return { "Chromatic", "Major", "Minor", "Major pentatonic", "Minor pentatonic" }; }
    static juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    static bool noteAllowed (int midiNote, int key, int scale) noexcept;
    static int nearestAllowed (float midiNote, int key, int scale) noexcept;

private:
    void control() noexcept;

    double fs = 48000.0;
    int channels = 2, delay = 960, delayLow = 288;
    TunerSettings settings;
    bool wasEnabled = false, wasLow = false;
    PitchDetector detector;
    GrainEngine engine;
    GrainEngine::Voice voice;

    bool voiced = false;
    float period = 240.0f, ratio = 1.0f, shift = 0.0f, slowNote = 0.0f;
    int target = -1, farFrames = 0;
    std::atomic<float> detectedNote { 0.0f }, outputNote { 0.0f };
    std::atomic<int> targetNote { -1 };
    std::array<std::atomic<float>, 12> sung {};
    std::atomic<float> sungFrames { 0.0f };
};
}
