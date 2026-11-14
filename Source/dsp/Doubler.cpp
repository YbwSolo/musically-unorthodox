#include "Doubler.h"

namespace mu
{
namespace
{
// Each voice has its own place in time, pitch and space, so no two are alike.
const float kBaseDelayMs[Doubler::kMaxVoices] = { 13.0f, 19.0f, 27.0f, 34.0f };
const float kDetune[Doubler::kMaxVoices] = { -1.0f, 1.0f, 1.7f, -1.6f };
const float kPan[Doubler::kMaxVoices] = { -1.0f, 1.0f, -0.55f, 0.55f };
}

void Doubler::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    detector.prepare (fs);
    engine.prepare (1, fs);
    reset();
}

void Doubler::reset()
{
    detector.reset();
    engine.reset();
    for (int v = 0; v < kMaxVoices; ++v)
    {
        engine.resetVoice (voices[(size_t) v], (float) (fs * 0.001 * kBaseDelayMs[v]));
        hp[(size_t) v] = lp[(size_t) v] = 0.0f;
    }
    voiced = false; level = 0.0f; consonantGain = 1.0f; tick = 0;
    period = (float) (fs / 200.0);
}

float Doubler::getVoicePan (int v) const noexcept { return kPan[juce::jlimit (0, kMaxVoices - 1, v)] * settings.width; }

void Doubler::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { wasEnabled = false; return; }
    if (! wasEnabled) { reset(); wasEnabled = true; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    const int count = settings.voices > 0 ? 4 : 2;
    const float target = settings.mix * (count == 4 ? 0.62f : 0.8f);
    const int interval = (int) (fs * 0.45 / 32.0);                           // a new drift goal about twice a second
    const float smooth = 1.0f - std::exp (-32.0f / (float) (fs * 0.35));
    const float hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 140.0f / (float) fs);
    const float lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 9000.0f / (float) fs);
    float* left = buffer.getWritePointer (0);
    float* right = ch > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float mono = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
        engine.write (0, mono);
        if (detector.push (mono))
        {
            const float f = detector.frequency();
            voiced = f > 0.0f && detector.clarity() > 0.5f;
            if (voiced) period = (float) (fs / (double) f);
        }
        level += (target - level) * 0.001f;
        // a doubled consonant is a flam, so the copies drop back when there is no note to follow
        consonantGain += ((voiced ? 1.0f : 0.45f) - consonantGain) * 0.002f;

        // each voice's pitch, timing and place drift slowly; worked out 32 samples at a time
        if (--tick <= 0)
        {
            tick = 32;
            for (int v = 0; v < count; ++v)
            {
                const float pd = pitchDrift[(size_t) v].next (rng, interval + v * 29, smooth);
                const float td = timeDrift[(size_t) v].next (rng, interval + v * 43, smooth);
                const float cents = kDetune[v] * settings.detuneCents + pd * (2.0f + 5.0f * settings.timing);
                const float delayMs = juce::jmax (3.0f, kBaseDelayMs[v] * (0.55f + 0.9f * settings.timing) + td * (1.0f + 7.0f * settings.timing));
                const float angle = (kPan[v] * settings.width + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                ratioNow[(size_t) v] = std::pow (2.0f, cents / 1200.0f);
                delayNow[(size_t) v] = (float) (fs * 0.001 * delayMs);
                gainL[(size_t) v] = std::cos (angle); gainR[(size_t) v] = std::sin (angle);
                shownCents[(size_t) v].store (cents, std::memory_order_relaxed);
                shownDelay[(size_t) v].store (delayMs, std::memory_order_relaxed);
            }
        }

        float addL = 0.0f, addR = 0.0f;
        for (int v = 0; v < count; ++v)
        {
            float out = 0.0f;
            engine.render (voices[(size_t) v], period, voiced, ratioNow[(size_t) v], delayNow[(size_t) v], &out);
            // thin the copies a little so they sit behind the lead instead of on top of it
            hp[(size_t) v] = hpCoeff * hp[(size_t) v] + (1.0f - hpCoeff) * out;
            out -= hp[(size_t) v];
            lp[(size_t) v] = lpCoeff * lp[(size_t) v] + (1.0f - lpCoeff) * out;
            out = lp[(size_t) v];
            addL += out * gainL[(size_t) v];
            addR += out * gainR[(size_t) v];
        }
        engine.advance();
        const float g = level * consonantGain;
        if (right != nullptr) { left[i] += g * addL; right[i] += g * addR; }
        else left[i] += g * 0.7071f * (addL + addR);
    }
}
}
