#pragma once
// Synthetic stems for the offline harness and the UI preview: a "vocal" with words of uneven loudness, low-mid
// build-up and sibilance, a "beat" with kick, snare, hats and a pad that crowds the vocal range, and an "808".
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace mu::synth
{
struct Biq
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bandPass (float hz, float q, float fs)
    {
        const float w = 2.0f * juce::MathConstants<float>::pi * hz / fs, al = std::sin (w) / (2.0f * q), a0 = 1.0f + al;
        b0 = al / a0; b1 = 0.0f; b2 = -al / a0; a1 = -2.0f * std::cos (w) / a0; a2 = (1.0f - al) / a0;
    }
    float run (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

class Vocal
{
public:
    void prepare (float sampleRate) { fs = sampleRate; f1.bandPass (260.0f, 1.2f, fs); f2.bandPass (900.0f, 2.0f, fs); f3.bandPass (2800.0f, 2.5f, fs); ess.bandPass (6800.0f, 2.0f, fs); }
    float next()
    {
        const float wordLen = 0.32f * fs;
        const int word = (int) ((float) n / wordLen);
        const float pos = std::fmod ((float) n, wordLen) / wordLen;
        static const float loud[] = { 0.9f, 0.35f, 0.6f, 1.0f, 0.25f, 0.7f, 0.45f, 0.95f, 0.0f, 0.55f, 0.3f, 0.85f };
        const float level = loud[word % 12];
        const float env = level * (pos < 0.08f ? pos / 0.08f : pos > 0.75f ? juce::jmax (0.0f, (1.0f - pos) / 0.25f) : 1.0f);
        const float f0 = 165.0f * (1.0f + 0.012f * std::sin (2.0f * juce::MathConstants<float>::pi * 5.5f * (float) n / fs)) * (1.0f + 0.12f * (float) ((word * 7) % 5) / 5.0f);
        phase += f0 / fs; if (phase > 1.0f) phase -= 1.0f;
        const float saw = 2.0f * phase - 1.0f;
        const float voiced = 1.6f * f1.run (saw) + 0.9f * f2.run (saw) + 0.55f * f3.run (saw);
        const float noise = rng.nextFloat() * 2.0f - 1.0f;
        const bool sibilant = (word % 3 == 1) && pos < 0.18f;
        const float s = ess.run (noise) * (sibilant ? 0.9f * level : 0.02f);
        ++n;
        return 0.5f * (voiced * env + s);
    }
private:
    float fs = 48000.0f, phase = 0.0f; long n = 0; Biq f1, f2, f3, ess; juce::Random rng { 7 };
};

class Beat
{
public:
    void prepare (float sampleRate) { fs = sampleRate; hat.bandPass (9000.0f, 1.0f, fs); snare.bandPass (1800.0f, 0.8f, fs); pad.bandPass (2600.0f, 1.4f, fs); }
    void next (float& l, float& r)
    {
        const float beatLen = 0.5f * fs;
        const float t = std::fmod ((float) n, beatLen) / fs;
        const int beat = (int) ((float) n / beatLen);
        // kick on every beat
        const float kf = 48.0f + 90.0f * std::exp (-t * 38.0f);
        kickPhase += kf / fs;
        const float kick = std::sin (2.0f * juce::MathConstants<float>::pi * kickPhase) * std::exp (-t * 9.0f) * 0.6f;
        // snare on 2 and 4
        const float noise = rng.nextFloat() * 2.0f - 1.0f;
        const float sn = (beat % 2 == 1) ? snare.run (noise) * std::exp (-t * 22.0f) * 0.9f : snare.run (0.0f);
        // hats in eighths
        const float ht = std::fmod ((float) n, beatLen * 0.5f) / fs;
        const float h = hat.run (noise) * std::exp (-ht * 60.0f) * 0.35f;
        // pad: a bright chord that sits right where the vocal needs to be heard
        float chord = 0.0f;
        static const float notes[] = { 220.0f, 277.2f, 329.6f, 440.0f };
        for (int i = 0; i < 4; ++i) { padPhase[i] += notes[i] / fs; if (padPhase[i] > 1.0f) padPhase[i] -= 1.0f; chord += 2.0f * padPhase[i] - 1.0f; }
        const float bright = pad.run (chord) * 0.6f;
        const float body = chord * 0.06f;
        l = kick + sn + h + bright + body;
        r = kick + sn + h * 0.6f + bright * 0.8f + body + 0.05f * hat.run (rng.nextFloat() - 0.5f);
        ++n;
    }
private:
    float fs = 48000.0f, kickPhase = 0.0f, padPhase[4] { 0, 0.3f, 0.6f, 0.1f }; long n = 0; Biq hat, snare, pad; juce::Random rng { 11 };
};

class Bass808
{
public:
    void prepare (float sampleRate) { fs = sampleRate; }
    float next()
    {
        const float len = 1.0f * fs;
        const float t = std::fmod ((float) n, len) / fs;
        const int bar = (int) ((float) n / len);
        const float f = (bar % 2 == 0) ? 55.0f : 49.0f;
        phase += f / fs;
        const float env = std::exp (-t * 1.6f);
        ++n;
        return 0.5f * env * (std::sin (2.0f * juce::MathConstants<float>::pi * phase) + 0.06f * std::sin (4.0f * juce::MathConstants<float>::pi * phase));
    }
private:
    float fs = 48000.0f, phase = 0.0f; long n = 0;
};
}
