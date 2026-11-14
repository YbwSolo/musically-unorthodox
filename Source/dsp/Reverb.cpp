#include "Reverb.h"

namespace mu
{
namespace
{
// Line lengths in milliseconds: spread out and sharing no common factors, so their echoes never pile up.
const float kLineMs[8] = { 31.3f, 36.7f, 41.9f, 47.3f, 53.9f, 60.1f, 67.3f, 74.9f };
const float kDiffuseMs[4] = { 4.7f, 3.5f, 12.6f, 9.4f };
const float kEarlyMs[2][5] = { { 7.3f, 13.1f, 21.7f, 33.4f, 47.9f }, { 8.9f, 15.4f, 24.1f, 36.8f, 51.2f } };
const float kEarlyGain[5] = { 0.50f, 0.42f, 0.34f, 0.26f, 0.19f };

// Three kinds of space, made from the same eight lines. A room is small: short lines, strong first reflections,
// a tail that thins out quickly. A plate is a sheet of metal: no separate reflections, dense from the first
// instant. A hall is large: long lines, the reflections arrive late and quiet, and the tail moves slowly.
const float kTypeScale[3] = { 0.55f, 1.0f, 1.4f };
const float kTypeEarly[3] = { 0.62f, 0.35f, 0.20f };
const float kTypeLate[3] = { 0.27f, 0.30f, 0.32f };
const float kTypeWobble[3] = { 3.0f, 6.0f, 10.0f };
const float kTypeHigh[3] = { 0.8f, 1.0f, 0.85f };            // how long the highs last, against the plate
const float kTypeDiffuse[3][2] = { { 0.55f, 0.48f }, { 0.70f, 0.62f }, { 0.74f, 0.68f } };

int nextPrime (int n)
{
    auto prime = [] (int v) { if (v < 4) return v > 1; if (v % 2 == 0) return false; for (int d = 3; d * d <= v; d += 2) if (v % d == 0) return false; return true; };
    while (! prime (n)) ++n;
    return n;
}
}

void Reverb::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (int c = 0; c < 2; ++c)
    {
        pre[c].resize ((int) (fs * 0.32));
        for (int i = 0; i < 4; ++i)
        {
            auto& ap = diffuse[c][(size_t) i];
            ap.length = nextPrime ((int) (fs * 0.001 * kDiffuseMs[i] * (c == 0 ? 1.0 : 1.07)));
            ap.line.resize (ap.length + 2);
            ap.g = i < 2 ? 0.70f : 0.62f;
        }
    }
    for (int i = 0; i < kLines; ++i)
    {
        lengths[(size_t) i] = nextPrime ((int) (fs * 0.001 * kLineMs[i]));
        lines[(size_t) i].resize (nextPrime ((int) (fs * 0.001 * kLineMs[i] * kTypeScale[2])) + 64);      // room for the largest space
    }
    lfoStep = (float) (juce::MathConstants<double>::twoPi * 0.7 / fs);
    shownDecay = -1.0f; shownTone = -1.0f; shownLowCut = -1.0f; shownType = -1;
    reset();
}

void Reverb::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        pre[c].clear();
        for (auto& ap : diffuse[c]) ap.line.clear();
        hpState[c] = lpState[c] = inHp[c] = 0.0f;
    }
    for (auto& l : lines) l.clear();
    state.fill (0.0f);
    envelope = 0.0f; duckGain = 1.0f; mixNow = 0.0f; wetPeak = 0.0f;
}

void Reverb::applyType() noexcept
{
    const int t = juce::jlimit (0, 2, settings.type);
    shownType = t;
    for (int i = 0; i < kLines; ++i) lengths[(size_t) i] = nextPrime ((int) (fs * 0.001 * kLineMs[i] * kTypeScale[t]));
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 4; ++i) diffuse[c][(size_t) i].g = kTypeDiffuse[t][i < 2 ? 0 : 1];
    earlyLevel = kTypeEarly[t]; lateLevel = kTypeLate[t]; wobbleDepth = kTypeWobble[t]; highShare = kTypeHigh[t];
    // what is ringing in the lines belongs to the old space
    for (auto& l : lines) l.clear();
    state.fill (0.0f);
    shownDecay = -1.0f;
}

void Reverb::update() noexcept
{
    if (juce::jlimit (0, 2, settings.type) != shownType) applyType();
    if (settings.lowCutHz != shownLowCut)
    {
        shownLowCut = settings.lowCutHz;
        hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (40.0f, 1000.0f, settings.lowCutHz) / (float) fs);      // keeps the reverb out of the low end
    }
    const float decay = juce::jlimit (0.2f, 10.0f, currentDecaySeconds());
    if (decay != shownDecay || settings.tone != shownTone)
    {
        shownDecay = decay; shownTone = settings.tone;
        // highs last between 25 % (dark) and 80 % (bright) as long as the rest
        const float highDecay = decay * (0.25f + 0.55f * settings.tone) * highShare;
        for (int i = 0; i < kLines; ++i)
        {
            const float trips = (float) lengths[(size_t) i] / (float) fs;
            const float low = std::pow (10.0f, -3.0f * trips / decay);              // falls 60 dB in "decay" seconds
            const float high = std::pow (10.0f, -3.0f * trips / highDecay);
            pole[(size_t) i] = (low - high) / (low + high);
            gainLow[(size_t) i] = low * (1.0f - pole[(size_t) i]);
        }
        lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * (4500.0f + 9500.0f * settings.tone) / (float) fs);
    }
    preSamples = juce::jlimit (1.0f, (float) (fs * 0.3), (float) (fs * 0.001 * currentPreDelayMs()));
}

void Reverb::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled)
    {
        if (wasEnabled) { reset(); wasEnabled = false; duckMeter.store (0.0f); wetMeter.store (-100.0f); }
        return;
    }
    wasEnabled = true;
    update();

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    float* left = buffer.getWritePointer (0);
    float* right = ch > 1 ? buffer.getWritePointer (1) : nullptr;
    const float attack = 1.0f - std::exp (-1.0f / (float) (fs * 0.010));
    const float release = 1.0f - std::exp (-1.0f / (float) (fs * 0.25));
    const float refFall = std::exp (-1.0f / (float) (fs * 6.0));
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.03));
    const float inHpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 120.0f / (float) fs);
    const float norm = 0.35355339f;                                                // 1 / sqrt(8)

    for (int i = 0; i < n; ++i)
    {
        const float dryL = left[i], dryR = right != nullptr ? right[i] : left[i];

        // how loud the dry sound is, against its own recent loudest: the reverb steps back while it plays
        const float level = juce::jmax (std::abs (dryL), std::abs (dryR));
        envelope += (level - envelope) * (level > envelope ? attack : release);
        reference = juce::jmax (envelope, reference * refFall, 1.0e-4f);
        const float presence = juce::jlimit (0.0f, 1.0f, envelope / reference);
        const float wanted = 1.0f - settings.duck * 0.8f * presence;
        duckGain += (wanted - duckGain) * (wanted < duckGain ? attack : release);

        // into the pre-delay, with the lows taken off first
        float in[2] = { dryL, dryR };
        for (int c = 0; c < 2; ++c)
        {
            inHp[c] = inHpCoeff * inHp[c] + (1.0f - inHpCoeff) * in[c];
            pre[c].write (in[c] - inHp[c]);
        }
        float feed[2], early[2] = { 0.0f, 0.0f };
        for (int c = 0; c < 2; ++c)
        {
            float x = pre[c].readFrac (preSamples);
            for (int e = 0; e < 5; ++e) early[c] += kEarlyGain[e] * pre[c].readFrac (preSamples + (float) (fs * 0.001 * kEarlyMs[c][e]));
            for (auto& ap : diffuse[c]) x = ap.run (x);
            feed[c] = x;
        }

        // read the eight lines (two of them wobble very slightly so no single ring can build up)
        lfoPhase += lfoStep; if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
        const float wobble = wobbleDepth * std::sin (lfoPhase);
        float v[kLines];
        for (int k = 0; k < kLines; ++k)
        {
            const float out = (k == 2) ? lines[(size_t) k].readFrac ((float) lengths[(size_t) k] + 12.0f + wobble)
                            : (k == 5) ? lines[(size_t) k].readFrac ((float) lengths[(size_t) k] + 12.0f - wobble)
                                       : lines[(size_t) k].read (lengths[(size_t) k]);
            // one trip's worth of fading: a little for the lows, more for the highs
            state[(size_t) k] = gainLow[(size_t) k] * out + pole[(size_t) k] * state[(size_t) k];
            v[k] = state[(size_t) k];
        }
        float wetL = 0.0f, wetR = 0.0f;
        for (int k = 0; k < kLines; ++k)
        {
            wetL += ((k & 1) ? -v[k] : v[k]) * ((k & 4) ? 0.8f : 1.0f);
            wetR += ((k & 2) ? -v[k] : v[k]) * ((k & 4) ? 1.0f : 0.8f);
        }
        // mix the lines into each other without gaining or losing energy (a Hadamard matrix, done as three passes)
        for (int span = 1; span < kLines; span <<= 1)
            for (int a = 0; a < kLines; a += span << 1)
                for (int b = a; b < a + span; ++b) { const float p = v[b], q = v[b + span]; v[b] = p + q; v[b + span] = p - q; }
        for (int k = 0; k < kLines; ++k)
            lines[(size_t) k].write (v[k] * norm + 0.5f * ((k & 1) ? feed[1] : feed[0]));

        wetL = wetL * lateLevel + early[0] * earlyLevel;
        wetR = wetR * lateLevel + early[1] * earlyLevel;
        // width: what the two sides share stays, what differs between them is turned down
        const float middle = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * juce::jlimit (0.0f, 1.0f, settings.width);
        float wet[2] = { middle + side, middle - side };
        for (int c = 0; c < 2; ++c)
        {
            hpState[c] = hpCoeff * hpState[c] + (1.0f - hpCoeff) * wet[c];
            wet[c] -= hpState[c];
            lpState[c] = lpCoeff * lpState[c] + (1.0f - lpCoeff) * wet[c];
            wet[c] = lpState[c];
        }
        mixNow += (settings.mix - mixNow) * mixStep;
        const float g = mixNow * duckGain * 1.6f;
        if (right != nullptr) { left[i] = dryL + g * wet[0]; right[i] = dryR + g * wet[1]; }
        else left[i] = dryL + g * 0.5f * (wet[0] + wet[1]);

        wetPeak = juce::jmax (wetPeak, std::abs (g * wet[0]), std::abs (g * wet[1]));
        if (++meterCount >= 1024)
        {
            meterCount = 0;
            duckMeter.store (juce::Decibels::gainToDecibels (duckGain, -60.0f), std::memory_order_relaxed);
            wetMeter.store (juce::Decibels::gainToDecibels (wetPeak, -100.0f), std::memory_order_relaxed);
            wetPeak = 0.0f;
        }
    }
}
}
