#include "PitchFX.h"
#include "Tuner.h"

namespace mu
{
namespace
{
// per voicing: for each voice, how many scale steps and how many octaves away from the lead. 99 = voice not used.
struct Part { int steps, octaves; };
const Part kVoicings[7][PitchFX::kHarmonyVoices] = {
    { { 2, 0 }, { 99, 0 }, { 99, 0 } },         // third up
    { { 4, 0 }, { 99, 0 }, { 99, 0 } },         // fifth up
    { { 2, 0 }, { 4, 0 }, { 99, 0 } },          // third and fifth
    { { 0, 1 }, { 99, 0 }, { 99, 0 } },         // octave up
    { { 0, -1 }, { 99, 0 }, { 99, 0 } },        // octave down
    { { -2, 0 }, { 99, 0 }, { 99, 0 } },        // third below
    { { 2, 0 }, { 4, 0 }, { 0, -1 } } };        // choir
const float kVoiceDelayMs[PitchFX::kHarmonyVoices] = { 11.0f, 17.0f, 23.0f };
const float kVoiceCents[PitchFX::kHarmonyVoices] = { 4.0f, -5.0f, 3.0f };
const float kVoicePan[PitchFX::kHarmonyVoices] = { -1.0f, 1.0f, 0.0f };
}

int PitchFX::numVoices (int voicing) noexcept
{
    int n = 0;
    for (auto& p : kVoicings[juce::jlimit (0, 6, voicing)]) if (p.steps != 99) ++n;
    return n;
}

int PitchFX::harmonyNote (int note, int voice, int voicing, int key, int scale, int mask) noexcept
{
    const auto part = kVoicings[juce::jlimit (0, 6, voicing)][juce::jlimit (0, kHarmonyVoices - 1, voice)];
    if (part.steps == 99) return -1;
    int n = note;
    if (scale == 0)
    {
        // no key chosen: plain intervals (a major third, a perfect fifth, a minor third below)
        n += part.steps == 2 ? 4 : part.steps == 4 ? 7 : part.steps == -2 ? -3 : 0;
    }
    else
    {
        // walk up or down the scale one allowed note at a time
        const int dir = part.steps > 0 ? 1 : -1;
        for (int left = std::abs (part.steps), guard = 0; left > 0 && guard < 24; ++guard)
        {
            n += dir;
            if (Tuner::noteAllowed (n, key, scale, mask)) --left;
        }
    }
    return n + 12 * part.octaves;
}

void PitchFX::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    detector.prepare (fs);
    engine.prepare (1, fs);
    reset();
}

void PitchFX::reset()
{
    detector.reset();
    engine.reset();
    engine.resetVoice (shiftVoice, (float) (fs * 0.012));
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        engine.resetVoice (harmony[(size_t) v], (float) (fs * 0.001 * kVoiceDelayMs[v]));
        ratio[(size_t) v] = ratioGoal[(size_t) v] = 1.0f; hp[(size_t) v] = 0.0f;
        shownHarmony[(size_t) v].store (-1);
    }
    voiced = false; shiftLevel = 0.0f; harmonyLevel = 0.0f; consonantGain = 1.0f;
    period = (float) (fs / 200.0);
    sungNote.store (0.0f);
}

void PitchFX::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const bool on = settings.shiftOn || settings.harmonyOn;
    if (! on) { if (wasOn) { wasOn = false; sungNote.store (0.0f); for (auto& h : shownHarmony) h.store (-1); } return; }
    if (! wasOn) { reset(); wasOn = true; }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    float* left = buffer.getWritePointer (0);
    float* right = ch > 1 ? buffer.getWritePointer (1) : nullptr;
    const float shiftRatio = std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, settings.semitones) / 12.0f);
    // with the character at zero the voice keeps its size; turned either way, the size follows by up to an octave
    const float formant = std::pow (2.0f, juce::jlimit (-1.0f, 1.0f, settings.character));
    const float shiftTarget = settings.shiftOn ? juce::jlimit (0.0f, 1.0f, settings.shiftMix) : 0.0f;
    const int voices = settings.harmonyOn ? numVoices (settings.voicing) : 0;
    const float harmonyTarget = settings.harmonyOn ? settings.harmonyMix * (voices > 1 ? 0.8f : 1.0f) : 0.0f;
    const float glide = 1.0f - std::exp (-1.0f / (float) (fs * 0.012));
    const float hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 150.0f / (float) fs);
    const float shiftDelay = (float) (fs * 0.012);
    float gainL[kHarmonyVoices], gainR[kHarmonyVoices];
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        const float angle = (kVoicePan[v] * settings.harmonyWidth + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        gainL[v] = std::cos (angle); gainR[v] = std::sin (angle);
    }

    for (int i = 0; i < n; ++i)
    {
        const float dryL = left[i], dryR = right != nullptr ? right[i] : left[i];
        const float mono = 0.5f * (dryL + dryR);
        engine.write (0, mono);
        if (detector.push (mono))
        {
            const float f = detector.frequency();
            voiced = f > 0.0f && detector.clarity() > 0.5f;
            if (voiced)
            {
                period = (float) (fs / (double) f);
                const float note = 69.0f + 12.0f * std::log2 (f / 440.0f);
                sungNote.store (note, std::memory_order_relaxed);
                // the lead's nearest note of the scale decides which notes the harmonies take
                const int lead = Tuner::nearestAllowed (note, settings.key, settings.scale, settings.noteMask);
                for (int v = 0; v < kHarmonyVoices; ++v)
                {
                    const int h = v < voices ? harmonyNote (lead, v, settings.voicing, settings.key, settings.scale, settings.noteMask) : -1;
                    shownHarmony[(size_t) v].store (h, std::memory_order_relaxed);
                    // the voice moves by the interval, so it keeps the lead's own vibrato and slides
                    ratioGoal[(size_t) v] = h >= 0 ? std::pow (2.0f, ((float) (h - lead) + kVoiceCents[v] * 0.01f) / 12.0f) : 1.0f;
                }
            }
            else sungNote.store (0.0f, std::memory_order_relaxed);
        }
        shiftLevel += (shiftTarget - shiftLevel) * 0.002f;
        harmonyLevel += (harmonyTarget - harmonyLevel) * 0.002f;
        consonantGain += ((voiced ? 1.0f : 0.4f) - consonantGain) * 0.002f;

        float outL = dryL, outR = dryR;
        if (settings.shiftOn || shiftLevel > 1.0e-4f)
        {
            float shifted = 0.0f;
            engine.render (shiftVoice, period, voiced, shiftRatio, shiftDelay, &shifted, formant);
            outL = dryL * (1.0f - shiftLevel) + shifted * shiftLevel;
            outR = dryR * (1.0f - shiftLevel) + shifted * shiftLevel;
        }
        if (voices > 0 || harmonyLevel > 1.0e-4f)
        {
            float addL = 0.0f, addR = 0.0f;
            for (int v = 0; v < juce::jmax (voices, 1); ++v)
            {
                ratio[(size_t) v] += (ratioGoal[(size_t) v] - ratio[(size_t) v]) * glide;
                float out = 0.0f;
                engine.render (harmony[(size_t) v], period, voiced, ratio[(size_t) v], (float) (fs * 0.001 * kVoiceDelayMs[v]), &out);
                hp[(size_t) v] = hpCoeff * hp[(size_t) v] + (1.0f - hpCoeff) * out;
                out -= hp[(size_t) v];
                addL += out * gainL[v]; addR += out * gainR[v];
            }
            const float g = harmonyLevel * consonantGain;
            if (right != nullptr) { outL += g * addL; outR += g * addR; }
            else outL += g * 0.7071f * (addL + addR);
        }
        engine.advance();
        left[i] = outL;
        if (right != nullptr) right[i] = outR;
    }
}
}
