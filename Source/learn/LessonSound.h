#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace mu
{
// What a lesson can ask to hear: one bar or two of sixteenth-note steps.
struct LessonPattern
{
    static constexpr int kMaxSteps = 32;
    int steps = 16;                                  // how many steps before the pattern repeats
    int stepsPerBeat = 4;                            // 4 = sixteenth notes, 2 = eighths, 8 = thirty-seconds, 3 or 6 = triplets
    float bpm = 120.0f;
    float swing = 0.0f;                              // 0..1: how late every second step is played
    std::array<signed char, kMaxSteps> kick {}, snare {}, hat {}, openHat {};     // 0 = rest, 1..127 = how hard
    std::array<signed char, kMaxSteps> bass {}, bassLen {};                       // MIDI note (0 = rest) and its length in steps
    std::array<signed char, kMaxSteps> bassSlide {};                              // 0, or the note this 808 note bends to before it ends
    std::array<signed char, kMaxSteps> lead {}, leadLen {};
    std::array<signed char, kMaxSteps> lead2 {}, lead2Len {};                     // a second voice: harmony, counter-melody or a chord note
    std::array<signed char, kMaxSteps> lead3 {}, lead3Len {};
    bool click = false;                              // a metronome tick on every beat
    bool once = false;                               // play through one time and stop, instead of looping
};

// The sounds the Learn section plays so a lesson can be heard, not only read: a plucked key sound for notes and
// melodies, a low sine for an 808, and a drum kit made from sine sweeps and noise. Nothing here is a sample.
// It is mixed in at the very end of the plugin, after every meter and measurement, only while a lesson asks
// for it, and never while the host is bouncing.
class LessonSound
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voices) v = {};
        drum = {};
        playing.store (false); stepNow.store (-1);
        samplesToStep = 0.0; step = -1;
    }

    // ---- from the lesson (message thread) ----
    void playNote (int midiNote, float seconds = 0.6f, int sound = 0)          // sound: 0 keys, 1 808
    {
        const int w = pendingWrite.load (std::memory_order_relaxed);
        pending[(size_t) (w % kPending)] = { midiNote, seconds, sound };
        pendingWrite.store (w + 1, std::memory_order_release);
    }
    void hit (int which, float level = 1.0f) { playNote (-1 - which, level, 2); }   // which: 0 kick, 1 snare, 2 hat, 3 open hat, 4 click
    void setPattern (const LessonPattern& p)
    {
        const juce::SpinLock::ScopedLockType l (patternLock);
        incoming = p; patternChanged.store (true, std::memory_order_release);
    }
    void start() { restart.store (true); playing.store (true); }
    void stop()  { playing.store (false); stepNow.store (-1); }
    bool isPlaying() const { return playing.load (std::memory_order_relaxed); }
    int currentStep() const { return stepNow.load (std::memory_order_relaxed); }      // -1 when stopped
    bool isSounding() const { return sounding.load (std::memory_order_relaxed); }

    // ---- audio thread ----
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        if (n == 0 || ch == 0) return;
        if (patternChanged.load (std::memory_order_acquire))
        {
            const juce::SpinLock::ScopedTryLockType l (patternLock);
            if (l.isLocked()) { pattern = incoming; patternChanged.store (false, std::memory_order_release); }
        }
        for (int r = pendingRead; r != pendingWrite.load (std::memory_order_acquire); ++r, pendingRead = r)
        {
            const auto e = pending[(size_t) (r % kPending)];
            if (e.sound == 2) trigger (-1 - e.note, e.seconds); else noteOn (e.note, e.seconds, e.sound);
        }
        const bool run = playing.load (std::memory_order_relaxed);
        if (run && restart.exchange (false)) { step = -1; samplesToStep = 0.0; }
        if (! run) samplesToStep = 0.0;
        if (! run && ! anythingRinging()) { sounding.store (false, std::memory_order_relaxed); return; }
        sounding.store (true, std::memory_order_relaxed);

        for (int i = 0; i < n; ++i)
        {
            if (run)
            {
                if (samplesToStep <= 0.0) nextStep();
                samplesToStep -= 1.0;
            }
            const float s = render();
            for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] += s;
        }
    }

    static float noteHz (int midiNote) { return 440.0f * std::pow (2.0f, ((float) midiNote - 69.0f) / 12.0f); }

private:
    struct Voice { bool on = false; float phase = 0.0f, hz = 0.0f, slideHz = 0.0f, age = 0.0f, length = 0.0f, level = 0.0f; int sound = 0; };
    struct Drum { float kickAge = -1.0f, kickPhase = 0.0f, kickLevel = 0.0f, snareAge = -1.0f, snarePhase = 0.0f, snareLevel = 0.0f,
                  hatAge = -1.0f, hatLevel = 0.0f, hatLength = 0.04f, clickAge = -1.0f, clickPhase = 0.0f, hp = 0.0f, bp = 0.0f; };
    struct Event { int note; float seconds; int sound; };
    static constexpr int kVoices = 10, kPending = 64;

    bool anythingRinging() const noexcept
    {
        for (auto& v : voices) if (v.on) return true;
        return drum.kickAge >= 0.0f || drum.snareAge >= 0.0f || drum.hatAge >= 0.0f || drum.clickAge >= 0.0f;
    }
    void noteOn (int midiNote, float seconds, int sound, int slideTo = 0) noexcept
    {
        if (midiNote <= 0) return;
        Voice* pick = &voices[0];
        // an 808 plays one note at a time: a new note takes over from the one still sounding
        if (sound == 1) { for (auto& v : voices) if (v.on && v.sound == 1) { pick = &v; break; } }
        if (! (sound == 1 && pick->on && pick->sound == 1))
            for (auto& v : voices) { if (! v.on) { pick = &v; break; } if (v.age > pick->age) pick = &v; }
        const float phase = sound == 1 && pick->on ? pick->phase : 0.0f;          // carried on, so the takeover does not click
        *pick = {};
        pick->on = true; pick->phase = phase; pick->hz = noteHz (midiNote); pick->slideHz = slideTo > 0 ? noteHz (slideTo) : 0.0f;
        pick->length = juce::jlimit (0.05f, 6.0f, seconds); pick->level = sound == 1 ? 0.5f : 0.16f; pick->sound = sound;
    }
    void trigger (int which, float level) noexcept
    {
        level = juce::jlimit (0.0f, 1.0f, level);
        if (which == 0) { drum.kickAge = 0.0f; drum.kickPhase = 0.0f; drum.kickLevel = level; }
        else if (which == 1) { drum.snareAge = 0.0f; drum.snarePhase = 0.0f; drum.snareLevel = level; }
        else if (which == 2) { drum.hatAge = 0.0f; drum.hatLevel = level; drum.hatLength = 0.035f; }
        else if (which == 3) { drum.hatAge = 0.0f; drum.hatLevel = level; drum.hatLength = 0.22f; }
        else { drum.clickAge = 0.0f; drum.clickPhase = 0.0f; }
    }
    void nextStep() noexcept
    {
        const int steps = juce::jlimit (1, LessonPattern::kMaxSteps, pattern.steps);
        if (pattern.once && step + 1 >= steps) { playing.store (false, std::memory_order_relaxed); stepNow.store (-1, std::memory_order_relaxed); samplesToStep = 1.0e9; return; }
        step = (step + 1) % steps;
        const int perBeat = juce::jlimit (1, 8, pattern.stepsPerBeat);
        const double sixteenth = fs * 60.0 / (double) juce::jlimit (40.0f, 220.0f, pattern.bpm) / (double) perBeat;
        const double late = (double) juce::jlimit (0.0f, 1.0f, pattern.swing) * 0.33 * sixteenth;
        samplesToStep += sixteenth + ((step % 2) == 0 ? late : -late);
        const auto at = (size_t) step;
        const float stepSeconds = (float) (sixteenth / fs);
        if (pattern.kick[at] > 0)    trigger (0, (float) pattern.kick[at] / 127.0f);
        if (pattern.snare[at] > 0)   trigger (1, (float) pattern.snare[at] / 127.0f);
        if (pattern.hat[at] > 0)     trigger (2, (float) pattern.hat[at] / 127.0f);
        if (pattern.openHat[at] > 0) trigger (3, (float) pattern.openHat[at] / 127.0f);
        if (pattern.click && (step % perBeat) == 0) trigger (4, 1.0f);
        if (pattern.bass[at] > 0)  noteOn (pattern.bass[at],  stepSeconds * (float) juce::jmax (1, (int) pattern.bassLen[at]),  1, pattern.bassSlide[at]);
        if (pattern.lead[at] > 0)  noteOn (pattern.lead[at],  stepSeconds * (float) juce::jmax (1, (int) pattern.leadLen[at]),  0);
        if (pattern.lead2[at] > 0) noteOn (pattern.lead2[at], stepSeconds * (float) juce::jmax (1, (int) pattern.lead2Len[at]), 0);
        if (pattern.lead3[at] > 0) noteOn (pattern.lead3[at], stepSeconds * (float) juce::jmax (1, (int) pattern.lead3Len[at]), 0);
        stepNow.store (step, std::memory_order_relaxed);
    }
    float render() noexcept
    {
        const float dt = 1.0f / (float) fs, twoPi = juce::MathConstants<float>::twoPi;
        float out = 0.0f;
        for (auto& v : voices)
        {
            if (! v.on) continue;
            float hz = v.hz;
            if (v.slideHz > 0.0f)                       // the bend: over the middle of the note, from its own pitch to the target
                hz *= std::pow (v.slideHz / v.hz, juce::jlimit (0.0f, 1.0f, (v.age - 0.3f * v.length) / (0.4f * v.length)));
            v.phase += hz * dt; if (v.phase >= 1.0f) v.phase -= 1.0f;
            const float attack = juce::jmin (1.0f, v.age / 0.004f);
            float s, env;
            if (v.sound == 1)
            {
                // an 808: a sine with a little second harmonic, held for its length and then let go
                s = std::sin (twoPi * v.phase) + 0.18f * std::sin (2.0f * twoPi * v.phase);
                env = v.age < v.length ? 1.0f : std::exp (-(v.age - v.length) / 0.08f);
                if (v.age > v.length + 0.5f) v.on = false;
            }
            else
            {
                // keys: a plucked tone, brighter at the start, fading over its length
                const float bright = std::exp (-v.age / 0.12f);
                s = std::sin (twoPi * v.phase) + (0.35f + 0.4f * bright) * std::sin (2.0f * twoPi * v.phase) + 0.25f * bright * std::sin (3.0f * twoPi * v.phase);
                env = std::exp (-v.age / juce::jmax (0.15f, v.length * 0.9f)) * (v.age < v.length ? 1.0f : std::exp (-(v.age - v.length) / 0.05f));
                if (v.age > v.length + 0.3f) v.on = false;
            }
            out += s * env * attack * v.level;
            v.age += dt;
        }
        if (drum.kickAge >= 0.0f)
        {
            const float hz = 46.0f + 110.0f * std::exp (-drum.kickAge / 0.03f);
            drum.kickPhase += hz * dt;
            out += 0.7f * drum.kickLevel * std::sin (twoPi * drum.kickPhase) * std::exp (-drum.kickAge / 0.16f);
            drum.kickAge += dt; if (drum.kickAge > 0.9f) drum.kickAge = -1.0f;
        }
        const float noise = (float) ((seed = seed * 1664525u + 1013904223u) >> 8) / 8388608.0f - 1.0f;
        if (drum.snareAge >= 0.0f)
        {
            drum.snarePhase += 190.0f * dt;
            drum.bp += 0.35f * (noise - drum.bp);
            out += drum.snareLevel * (0.30f * (noise - drum.bp * 0.6f) * std::exp (-drum.snareAge / 0.09f) + 0.25f * std::sin (twoPi * drum.snarePhase) * std::exp (-drum.snareAge / 0.05f));
            drum.snareAge += dt; if (drum.snareAge > 0.6f) drum.snareAge = -1.0f;
        }
        if (drum.hatAge >= 0.0f)
        {
            drum.hp += 0.55f * (noise - drum.hp);
            out += 0.16f * drum.hatLevel * (noise - drum.hp) * std::exp (-drum.hatAge / drum.hatLength);
            drum.hatAge += dt; if (drum.hatAge > drum.hatLength * 7.0f) drum.hatAge = -1.0f;
        }
        if (drum.clickAge >= 0.0f)
        {
            drum.clickPhase += 1500.0f * dt;
            out += 0.2f * std::sin (twoPi * drum.clickPhase) * std::exp (-drum.clickAge / 0.012f);
            drum.clickAge += dt; if (drum.clickAge > 0.1f) drum.clickAge = -1.0f;
        }
        return juce::jlimit (-0.95f, 0.95f, out * 0.6f);            // well under a mix's level: it is an example, not a part
    }

    double fs = 48000.0, samplesToStep = 0.0;
    int step = -1, pendingRead = 0;
    unsigned int seed = 12345u;
    std::array<Voice, kVoices> voices {};
    Drum drum;
    LessonPattern pattern, incoming;
    juce::SpinLock patternLock;
    std::array<Event, kPending> pending {};
    std::atomic<int> pendingWrite { 0 }, stepNow { -1 };
    std::atomic<bool> playing { false }, restart { false }, patternChanged { false }, sounding { false };
};
}
