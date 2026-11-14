#pragma once
#include <array>
#include "Pitch.h"
#include "KeyFinder.h"
#include "TuneGraph.h"

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
    int mode = 0;              // 0 = automatic (corrects whatever is sung, live); 1 = graph (follows the hand-edited notes)
    float hold = 0.3f;         // 0..1, how firmly a note is held before the voice is allowed to move to the next one
    float formantSemis = 0.0f; // the size of the voice: 0 = as recorded, up = smaller and brighter, down = bigger and darker
    int noteMask = 0xFFF;      // with the Custom scale: which of the twelve notes are allowed, bit 0 = C
    float flexCents = 0.0f;    // notes sung within this many cents of the target are left alone
    float humanise = 0.0f;     // 0..1: held notes are corrected more gently than short ones, so long notes keep some life
    float keepSlides = 0.0f;   // 0..1: while the voice is sliding from note to note it is left alone, and only settled notes are tuned
    bool legacySlicing = false; // for comparison tests only: cut the slices the way builds 10 and 11 did
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
    void setGraph (TuneGraph* g) noexcept { graph = g; }
    // where in the song this block starts, so the graph knows which note it is on
    void setTimeline (bool hostPlaying, double seconds) noexcept { playing = hostPlaying; blockTime = seconds; }
    int getLatencySamples() const noexcept { return settings.enabled && ! settings.lowLatency ? delay : 0; }

    // for the display (any thread)
    float getDetectedNote() const noexcept { return detectedNote.load (std::memory_order_relaxed); }    // MIDI note with fraction, 0 = no pitch
    float getSlideSpeed() const noexcept { return sliding; }      // semitones a second; for the lab tool and tests
    float getShift() const noexcept { return shift; }
    float getOutputNote() const noexcept { return outputNote.load (std::memory_order_relaxed); }
    int getTargetNote() const noexcept { return targetNote.load (std::memory_order_relaxed); }

    // Key finding: which notes have been sung so far, and the key that fits them best.
    using KeyGuess = KeyEstimate;
    KeyGuess guessKey() const noexcept;
    void forgetNotes() noexcept;

    static constexpr int kCustomScale = 5;
    static juce::StringArray scaleNames() { return { "Chromatic", "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Custom" }; }
    static juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    static bool noteAllowed (int midiNote, int key, int scale, int customMask = 0xFFF) noexcept;
    static int nearestAllowed (float midiNote, int key, int scale, int customMask = 0xFFF) noexcept;
    static int scaleMask (int key, int scale, int customMask = 0xFFF) noexcept;      // the allowed notes as twelve bits, bit 0 = C

private:
    void control (double seconds) noexcept;

    double fs = 48000.0;
    int channels = 2, delay = 960, delayLow = 288;
    TunerSettings settings;
    bool wasEnabled = false, wasLow = false;
    PitchDetector detector;
    GrainEngine engine;
    GrainEngine::Voice voice;

    bool voiced = false;
    float period = 240.0f, ratio = 1.0f, shift = 0.0f, slowNote = 0.0f;
    float periodNow = 240.0f, outPeriodNow = 240.0f;      // what the engine is given: the same values, moved smoothly between measurements
    int unsureFrames = 0, framesOnNote = 0, centreFrames = 0;
    std::array<float, 16> centreHist {};
    float sliding = 0.0f;                                 // how fast the voice is travelling, in semitones a second
    std::array<float, 18> recent {}; int recentCount = 0, recentAt = 0;      // the last 180 ms of pitch
    bool snap = true;
    int target = -1, farFrames = 0, switchFrames = 0, lastCaptured = -1;
    TuneGraph* graph = nullptr;
    bool playing = false;
    double blockTime = 0.0;
    float formant = 1.0f;
    std::atomic<float> detectedNote { 0.0f }, outputNote { 0.0f };
    std::atomic<int> targetNote { -1 };
    std::array<std::atomic<float>, 12> sung {};
    std::atomic<float> sungFrames { 0.0f };
};
}
