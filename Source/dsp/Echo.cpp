#include "Echo.h"

namespace mu
{
void Echo::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : lines) l.resize ((int) (fs * 4.1));
    for (int c = 0; c < 2; ++c)
    {
        smear[c][0].length = (int) (fs * 0.001 * (c == 0 ? 11.3 : 12.1)); smear[c][0].line.resize (smear[c][0].length + 4);
        smear[c][1].length = (int) (fs * 0.001 * (c == 0 ? 17.9 : 19.3)); smear[c][1].line.resize (smear[c][1].length + 4);
    }
    reset();
}

void Echo::reset()
{
    for (auto& l : lines) l.clear();
    for (auto& side : smear) for (auto& a : side) a.line.clear();
    hp[0] = hp[1] = lp[0] = lp[1] = lp2[0] = lp2[1] = dc[0] = dc[1] = 0.0f;
    envelope = 0.0f; duckGain = 1.0f; mixNow = 0.0f;
    timeNow = (float) (fs * 0.001 * currentTimeMs());
}

void Echo::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled)
    {
        if (wasEnabled) { reset(); wasEnabled = false; duckMeter.store (0.0f); }
        return;
    }
    if (! wasEnabled) { reset(); wasEnabled = true; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    float* left = buffer.getWritePointer (0);
    float* right = ch > 1 ? buffer.getWritePointer (1) : nullptr;
    const float target = (float) (fs * 0.001 * currentTimeMs());
    const float glide = 1.0f - std::exp (-1.0f / (float) (fs * 0.12));          // a change of time bends the echoes like tape
    const float fb = juce::jlimit (0.0f, 0.9f, settings.feedback);
    const int style = juce::jlimit (0, 3, settings.style), pattern = juce::jlimit (0, 2, settings.pattern);
    const bool tape = style == 1, analog = style == 2, diffused = style == 3;
    const float hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (20.0f, 1000.0f, settings.lowCutHz) / (float) fs);
    // how bright a repeat can be: digital to 16 kHz, tape to 9 kHz, analog to 4.5 kHz at the brightest
    const float top = analog ? 4500.0f : tape ? 9000.0f : 16000.0f;
    const float cutoff = juce::jmin ((float) fs * 0.45f, (top / 9.0f) * std::pow (9.0f, juce::jlimit (0.0f, 1.0f, settings.tone)));
    const float lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) fs);
    const float push = 1.0f + 5.0f * juce::jlimit (0.0f, 1.0f, settings.drive);
    const float dcCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 20.0f / (float) fs);
    // Wobble, set by how far the pitch of an echo moves, not by a share of the delay time, so it sounds the same
    // at every time: up to 12 cents of slow wow and 4 cents of fast flutter.
    const float wob = juce::jlimit (0.0f, 1.0f, settings.wobble);
    const float wowSamples = wob * 0.00696f / (juce::MathConstants<float>::twoPi * 0.37f) * (float) fs;
    const float flutterSamples = wob * 0.00231f / (juce::MathConstants<float>::twoPi * 5.7f) * (float) fs;
    const float wowStep = juce::MathConstants<float>::twoPi * 0.37f / (float) fs, flutterStep = juce::MathConstants<float>::twoPi * 5.7f / (float) fs;
    const float attack = 1.0f - std::exp (-1.0f / (float) (fs * 0.010));
    const float release = 1.0f - std::exp (-1.0f / (float) (fs * 0.30));
    const float refFall = std::exp (-1.0f / (float) (fs * 6.0));
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.03));
    const float cross = 1.0f - juce::jlimit (0.0f, 1.0f, settings.width);        // how much of each side's echo also appears on the other
    const float norm = 1.0f / std::sqrt (1.0f + cross * cross);

    for (int i = 0; i < n; ++i)
    {
        const float dryL = left[i], dryR = right != nullptr ? right[i] : left[i];
        const float in = 0.5f * (dryL + dryR);

        const float level = juce::jmax (std::abs (dryL), std::abs (dryR));
        envelope += (level - envelope) * (level > envelope ? attack : release);
        reference = juce::jmax (envelope, reference * refFall, 1.0e-4f);
        const float presence = juce::jlimit (0.0f, 1.0f, envelope / reference);
        const float wanted = 1.0f - settings.duck * 0.85f * presence;
        duckGain += (wanted - duckGain) * (wanted < duckGain ? attack : release);

        timeNow += (target - timeNow) * glide;
        float drift = 0.0f;
        if (wob > 0.0f)
        {
            wowPhase += wowStep; if (wowPhase > juce::MathConstants<float>::twoPi) wowPhase -= juce::MathConstants<float>::twoPi;
            flutterPhase += flutterStep; if (flutterPhase > juce::MathConstants<float>::twoPi) flutterPhase -= juce::MathConstants<float>::twoPi;
            drift = wowSamples * (1.0f + std::sin (wowPhase)) + flutterSamples * (1.0f + std::sin (flutterPhase));
        }
        const float timeL = timeNow + drift, timeR = (pattern == 2 ? timeNow * 0.75f : timeNow) + drift;
        const float yL = lines[0].read (timeL), yR = lines[1].read (timeR);

        // Ping-pong: the sound goes into the left line; what comes out of the left goes into the right, and what
        // comes out of the right goes round again, so the echoes alternate sides. Straight and Dual: both sides
        // hear the sound and each feeds itself.
        float x[2];
        if (pattern == 0) { x[0] = in + fb * yR; x[1] = fb * yL; }
        else              { x[0] = in + fb * yL; x[1] = in + fb * yR; }
        for (int c = 0; c < 2; ++c)
        {
            hp[c] = hpCoeff * hp[c] + (1.0f - hpCoeff) * x[c];
            float v = x[c] - hp[c];
            lp[c] = lpCoeff * lp[c] + (1.0f - lpCoeff) * v;
            v = lp[c];
            if (analog) { lp2[c] = lpCoeff * lp2[c] + (1.0f - lpCoeff) * v; v = lp2[c]; }
            // the filters come first, so what the saturation adds stays well under the top of the range
            if (tape)        v = std::tanh (v * push) / push;
            else if (analog)
            {
                // uneven: the two halves of the wave are treated differently, which adds the octave. What that
                // leaves at 0 Hz is taken straight back out.
                v = std::tanh ((v + 0.35f * v * v) * push) / push;
                dc[c] += (v - dc[c]) * dcCoeff;
                v -= dc[c];
            }
            else if (diffused) v = smear[c][1].run (smear[c][0].run (v, 0.55f), 0.5f);
            lines[c].write (juce::jlimit (-4.0f, 4.0f, v));
        }

        mixNow += (settings.mix - mixNow) * mixStep;
        const float g = mixNow * duckGain * 1.25f;
        const float wetL = pattern == 0 ? (yL + cross * yR) * norm : yL, wetR = pattern == 0 ? (yR + cross * yL) * norm : yR;
        if (right != nullptr) { left[i] = dryL + g * wetL; right[i] = dryR + g * wetR; }
        else left[i] = dryL + g * 0.7071f * (yL + yR);

        if (++meterCount >= 1024) { meterCount = 0; duckMeter.store (juce::Decibels::gainToDecibels (duckGain, -60.0f), std::memory_order_relaxed); }
    }
}
}
