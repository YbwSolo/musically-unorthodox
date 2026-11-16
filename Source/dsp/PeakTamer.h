#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace mu
{
// Holds the loudest syllables. A compressor set for a whole phrase evens the phrases out and still lets single
// words jump; on the reference records the loudest syllables stand about 5 dB above the usual level of the
// voice, on a vocal straight out of this plugin's compressor about 7. This follows the usual level of the
// voice by itself (so there is no threshold to set, and a quiet verse is treated like a loud one) and turns
// down, quickly and briefly, only what stands 2 to 3.5 dB or more above it, by 6 dB at the very most. It adds no delay.
class PeakTamer
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aAtt = coeff (0.002f); aRel = coeff (0.030f); aUsual = coeff (1.2f); aUsualUp = coeff (0.6f); aUsualFast = coeff (0.15f);
        warmLen = (int) (fs * 0.5);
        gAtt = coeff (0.001f); gRel = coeff (0.060f);
        reset();
    }
    void reset() noexcept { env = 0.0f; usualDb = -100.0f; warm = 0; gainDb = 0.0f; reduction.store (0.0f); }
    void setEnabled (bool on) noexcept { enabled = on; }
    void setAmount (float amount01) noexcept { amount = juce::jlimit (0.0f, 1.0f, amount01); }
    float getReductionDb() const noexcept { return reduction.load (std::memory_order_relaxed); }       // 0 or negative

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        if (n == 0 || ch == 0) return;
        if (! enabled || amount <= 0.001f) { if (gainDb < -0.01f) fadeOut (buffer); else { gainDb = 0.0f; reduction.store (0.0f, std::memory_order_relaxed); } trackOnly (buffer); return; }
        // more amount: a higher ratio, and it starts a little nearer the usual level (3.5 dB above it down to 2)
        const float ratio = 1.0f + 3.0f * amount, slope = 1.0f - 1.0f / ratio, above = 3.5f - 1.5f * amount;
        float worst = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float over = follow (buffer, i, ch) - (usualDb + above);
            // soft knee, 3 dB wide
            float past = 0.0f;
            if (over > 1.5f) past = over; else if (over > -1.5f) { const float t = over + 1.5f; past = t * t / 6.0f; }
            // nothing until the usual level is known (the first half second of sound), and never more than 6 dB
            const float target = warm >= warmLen ? -juce::jmin (kMost, past * slope) : 0.0f;
            gainDb = target < gainDb ? gAtt * gainDb + (1.0f - gAtt) * target : gRel * gainDb + (1.0f - gRel) * target;
            worst = juce::jmin (worst, gainDb);
            const float g = std::pow (10.0f, gainDb * 0.05f);
            for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= g;
        }
        reduction.store (worst, std::memory_order_relaxed);
    }

private:
    static constexpr float kMost = 6.0f;
    float coeff (float seconds) const { return std::exp (-1.0f / (float) (fs * (double) seconds)); }
    // the level right now (a few ms), and the usual level of the voice while it is going
    float follow (const juce::AudioBuffer<float>& buffer, int i, int ch) noexcept
    {
        float p = 0.0f;
        for (int c = 0; c < ch; ++c) { const float x = buffer.getReadPointer (c)[i]; p += x * x; }
        p /= (float) ch;
        env = p > env ? aAtt * env + (1.0f - aAtt) * p : aRel * env + (1.0f - aRel) * p;
        const float db = 10.0f * std::log10 (env + 1.0e-12f);
        if (db > -65.0f)
        {
            if (usualDb < -99.0f) usualDb = db;
            else if (db > usualDb - 25.0f)
            {
                // finds the level within half a second; after that it follows a louder passage in about half a
                // second (so the start of a chorus is not squashed) and a quieter one slowly
                const float a = warm < warmLen ? aUsualFast : db > usualDb ? aUsualUp : aUsual;
                usualDb = a * usualDb + (1.0f - a) * db;
                if (warm < warmLen) ++warm;
            }
        }
        return db;
    }
    void trackOnly (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        for (int i = 0; i < n; ++i) follow (buffer, i, ch);
    }
    void fadeOut (juce::AudioBuffer<float>& buffer) noexcept      // switched off mid-word: let go over the release, no step
    {
        const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            gainDb *= gRel;
            const float g = std::pow (10.0f, gainDb * 0.05f);
            for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= g;
        }
        reduction.store (gainDb, std::memory_order_relaxed);
    }

    double fs = 48000.0;
    bool enabled = false;
    float amount = 0.5f, aAtt = 0.0f, aRel = 0.0f, aUsual = 0.0f, aUsualUp = 0.0f, aUsualFast = 0.0f, gAtt = 0.0f, gRel = 0.0f;
    float env = 0.0f, usualDb = -100.0f, gainDb = 0.0f;
    int warm = 0, warmLen = 24000;
    std::atomic<float> reduction { 0.0f };
};
}
