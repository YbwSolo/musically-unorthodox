#include "Tuner.h"

namespace mu
{
void Tuner::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    delay = (int) std::round (fs * 0.020);          // lines the voice up with what the detector knows about it
    delayLow = (int) std::round (fs * 0.006);       // for recording: the least that still sounds right
    detector.prepare (fs);
    engine.prepare (channels, fs);
    reset();
}

void Tuner::reset()
{
    detector.reset();
    engine.reset();
    engine.resetVoice (voice, (float) (settings.lowLatency ? delayLow : delay));
    voiced = false; ratio = 1.0f; shift = 0.0f; target = -1;
    period = (float) (fs / 200.0);
    detectedNote.store (0.0f); outputNote.store (0.0f); targetNote.store (-1);
}

void Tuner::setSettings (const TunerSettings& s) noexcept { settings = s; }

bool Tuner::noteAllowed (int note, int key, int scale) noexcept
{
    static const int masks[] = { 0xFFF,                                   // chromatic
                                 (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),     // major
                                 (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),     // natural minor
                                 (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                             // major pentatonic
                                 (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10) };                          // minor pentatonic
    const int degree = (((note - key) % 12) + 12) % 12;
    return (masks[juce::jlimit (0, 4, scale)] >> degree) & 1;
}

int Tuner::nearestAllowed (float note, int key, int scale) noexcept
{
    const int centre = (int) std::lround (note);
    int best = centre; float bestDistance = 100.0f;
    for (int n = centre - 6; n <= centre + 6; ++n)
        if (noteAllowed (n, key, scale))
        {
            const float d = std::abs (note - (float) n);
            if (d < bestDistance) { bestDistance = d; best = n; }
        }
    return best;
}

// Runs each time the detector has a fresh answer (every 10 ms): decide where the voice should be moved to.
void Tuner::control() noexcept
{
    const float f = detector.frequency();
    if (f <= 0.0f || detector.clarity() < 0.5f)
    {
        voiced = false; ratio = 1.0f; shift = 0.0f; target = -1;
        detectedNote.store (0.0f, std::memory_order_relaxed); outputNote.store (0.0f, std::memory_order_relaxed); targetNote.store (-1, std::memory_order_relaxed);
        return;
    }
    const float note = 69.0f + 12.0f * std::log2 (f / 440.0f);
    const bool starting = ! voiced;

    // remember which notes are sung, for key finding
    const int pc = (((int) std::lround (note) % 12) + 12) % 12;
    sung[(size_t) pc].store (sung[(size_t) pc].load (std::memory_order_relaxed) + 1.0f, std::memory_order_relaxed);
    sungFrames.store (sungFrames.load (std::memory_order_relaxed) + 1.0f, std::memory_order_relaxed);

    // The slow version of the pitch is the centre of the note; the difference from it is vibrato and slides.
    // A real move to another note (well away from the current one for 40 ms) starts a new centre straight away.
    if (starting) { slowNote = note; farFrames = 0; }
    if (target >= 0 && std::abs (note - (float) target) > 0.8f) { if (++farFrames >= 4) { slowNote = note; farFrames = 0; } }
    else farFrames = 0;
    slowNote += (note - slowNote) * 0.08f;                                  // about 120 ms
    // what gets corrected: the note as sung (keep vibrato = 0) or only its centre (keep vibrato = 1)
    const float reference = note + settings.keepVibrato * (slowNote - note);

    // nearest note of the scale, with a little stickiness so a voice sitting between two notes does not flicker
    int candidate = nearestAllowed (reference, settings.key, settings.scale);
    if (target >= 0 && candidate != target && noteAllowed (target, settings.key, settings.scale)
        && std::abs (reference - (float) candidate) + 0.12f > std::abs (reference - (float) target)) candidate = target;
    target = candidate;

    const float wanted = juce::jlimit (-7.0f, 7.0f, ((float) target - reference) * settings.amount);
    if (starting || settings.speedMs < 1.0f) shift = wanted;
    else shift += (wanted - shift) * (1.0f - std::exp (-10.0f / settings.speedMs));

    voiced = true;
    ratio = std::pow (2.0f, shift / 12.0f);
    period = (float) (fs / (double) f);
    detectedNote.store (note, std::memory_order_relaxed);
    outputNote.store (note + shift, std::memory_order_relaxed);
    targetNote.store (target, std::memory_order_relaxed);
}

void Tuner::process (juce::AudioBuffer<float>& buffer, bool passThrough) noexcept
{
    if (! settings.enabled)
    {
        if (wasEnabled) { wasEnabled = false; detectedNote.store (0.0f); outputNote.store (0.0f); targetNote.store (-1); }
        return;
    }
    if (! wasEnabled || wasLow != settings.lowLatency) { reset(); wasEnabled = true; wasLow = settings.lowLatency; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    const float d = (float) (settings.lowLatency ? delayLow : delay);
    float* data[2] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int i = 0; i < n; ++i)
    {
        float mono = 0.0f;
        for (int c = 0; c < ch; ++c) { engine.write (c, data[c][i]); mono += data[c][i]; }
        if (detector.push (mono / (float) ch)) control();
        float out[2] = { 0.0f, 0.0f };
        if (passThrough) engine.render (voice, (float) (fs / 200.0), false, 1.0f, d, out);
        else             engine.render (voice, period, voiced, ratio, d, out);
        for (int c = 0; c < ch; ++c) data[c][i] = out[c];
        engine.advance();
    }
}

Tuner::KeyGuess Tuner::guessKey() const noexcept
{
    // Krumhansl-Kessler key profiles: how strongly each note of the scale belongs to a major or a minor key
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    float h[12]; float total = 0.0f;
    for (int i = 0; i < 12; ++i) { h[i] = sung[(size_t) i].load (std::memory_order_relaxed); total += h[i]; }
    KeyGuess guess;
    guess.secondsHeard = sungFrames.load (std::memory_order_relaxed) * 0.01f;
    if (total < 1.0f) return guess;
    auto correlate = [&h] (const float* profile, int key)
    {
        float mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += h[i]; mp += profile[i]; }
        mh /= 12.0f; mp /= 12.0f;
        float num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float a = h[(i + key) % 12] - mh, b = profile[i] - mp;
            num += a * b; dh += a * a; dp += b * b;
        }
        return dh > 0.0f ? num / std::sqrt (dh * dp) : 0.0f;
    };
    float best = -2.0f, second = -2.0f;
    for (int key = 0; key < 12; ++key)
        for (int m = 0; m < 2; ++m)
        {
            const float c = correlate (m == 0 ? major : minor, key);
            if (c > best) { second = best; best = c; guess.key = key; guess.minor = m == 1; }
            else if (c > second) second = c;
        }
    guess.confidence = juce::jlimit (0.0f, 1.0f, best);
    return guess;
}

void Tuner::forgetNotes() noexcept
{
    for (auto& s : sung) s.store (0.0f);
    sungFrames.store (0.0f);
}
}
