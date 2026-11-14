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
const float kVoiceSize[PitchFX::kHarmonyVoices] = { 0.05f, -0.04f, -0.07f };      // each voice a slightly different size of singer
// the two extra singers of a choir part: a little out of tune with the first, a little late, a little to one side, a slightly different size
const float kChoirCents[PitchFX::kSingers] = { 0.0f, 9.0f, -12.0f };
const float kChoirMs[PitchFX::kSingers] = { 0.0f, 9.0f, 19.0f };
const float kChoirPan[PitchFX::kSingers] = { 0.0f, 0.32f, -0.32f };
const float kChoirSize[PitchFX::kSingers] = { 0.0f, 0.035f, -0.045f };
const int kUpSemis[7] = { 0, 2, 4, 5, 7, 9, 11 }, kDownSemis[7] = { 0, 2, 3, 5, 7, 8, 10 };
}

int PitchFX::numVoices (int voicing) noexcept
{
    int n = 0;
    for (auto& p : kVoicings[juce::jlimit (0, 6, voicing)]) if (p.steps != 99) ++n;
    return n;
}

int PitchFX::stepNote (int note, int steps, int key, int scale, int mask) noexcept
{
    if (steps == 0) return note;
    if (scale == 0)
    {
        // no key chosen: the steps of a plain scale (a major third and a perfect fifth up, a minor third and a fifth down)
        const int count = std::abs (steps);
        const int semis = 12 * (count / 7) + (steps > 0 ? kUpSemis[count % 7] : kDownSemis[count % 7]);
        return note + (steps > 0 ? semis : -semis);
    }
    // walk up or down the scale one allowed note at a time
    int n = note;
    const int dir = steps > 0 ? 1 : -1;
    for (int left = std::abs (steps), guard = 0; left > 0 && guard < 96; ++guard)
    {
        n += dir;
        if (Tuner::noteAllowed (n, key, scale, mask)) --left;
    }
    return n;
}

int PitchFX::harmonyNote (int note, int voice, int voicing, int key, int scale, int mask) noexcept
{
    const auto part = kVoicings[juce::jlimit (0, 6, voicing)][juce::jlimit (0, kHarmonyVoices - 1, voice)];
    if (part.steps == 99) return -1;
    return stepNote (note, part.steps, key, scale, mask) + 12 * part.octaves;
}

int PitchFX::ledNote (int previous, int parallel, int leadNote) noexcept
{
    // Singers do not all move every time the tune does. If the note this voice is on still makes a third, a
    // fifth, a sixth or an octave with the tune, and is no more than a tone and a half from where moving along
    // with it would put it, it stays. Otherwise it moves.
    if (previous < 0) return parallel;
    if (std::abs (previous - parallel) > 3) return parallel;
    const int between = (((previous - leadNote) % 12) + 12) % 12;
    const bool fits = between == 0 || between == 3 || between == 4 || between == 7 || between == 8 || between == 9;
    return fits && previous != leadNote ? previous : parallel;
}

int PitchFX::chordNote (int chordMask, int target, int previous, const int* taken, int numTaken) noexcept
{
    chordMask &= 0xFFF;
    if (chordMask == 0) return target;
    auto inChord = [chordMask] (int n) { return ((chordMask >> (((n % 12) + 12) % 12)) & 1) != 0; };
    auto isTaken = [&] (int n) { for (int i = 0; i < numTaken; ++i) if (taken[i] == n) return true; return false; };
    // stay put if the chord still has this note and it has not drifted far from where the voice belongs
    if (previous >= 0 && inChord (previous) && std::abs (previous - target) <= 5 && ! isTaken (previous)) return previous;
    int best = -1, bestCost = 1000;
    for (int n = target - 9; n <= target + 9; ++n)
    {
        if (! inChord (n)) continue;
        // nearest to where the voice belongs; a small pull towards where it was; a note someone else has is a last resort
        int cost = 2 * std::abs (n - target) + (previous >= 0 ? std::abs (n - previous) : 0) + (isTaken (n) ? 40 : 0);
        if (cost < bestCost) { bestCost = cost; best = n; }
    }
    return best >= 0 ? best : target;
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
    engine.resetVoice (shiftVoiceB, (float) (fs * 0.012));
    for (int v = 0; v < kParts; ++v)
    {
        for (int s = 0; s < kSingers; ++s)
        {
            engine.resetVoice (harmony[(size_t) v][(size_t) s], (float) (fs * 0.001 * (kVoiceDelayMs[v % kHarmonyVoices] + kChoirMs[s])));
            hp[(size_t) v][(size_t) s] = 0.0f;
            driftPhase[(size_t) v][(size_t) s] = 1.7f * (float) v + 2.9f * (float) s; driftCents[(size_t) v][(size_t) s] = 0.0f; driftMs[(size_t) v][(size_t) s] = 0.0f;
        }
        ratio[(size_t) v] = ratioGoal[(size_t) v] = 1.0f; partLevel[(size_t) v] = 0.0f; heldNote[(size_t) v] = -1;
        shownHarmony[(size_t) v].store (-1);
    }
    voiced = false; shiftLevel = 0.0f; harmonyLevel = 0.0f; consonantGain = 1.0f; choirNow = 0.0f; robotRatio = 1.0f; snap = true; unsureFrames = 0;
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

    // ---- Shift
    const int shiftMode = juce::jlimit (0, 2, settings.shiftMode);
    const float amount = juce::jlimit (-24.0f, 24.0f, settings.semitones) + juce::jlimit (-100.0f, 100.0f, settings.fineCents) * 0.01f;
    const float shiftRatio = std::pow (2.0f, amount / 12.0f);
    // Robot: one fixed note, the key note in the octave below middle C, moved by the Pitch knob in whole semitones
    const float robotNote = 48.0f + (float) settings.key + std::round (juce::jlimit (-24.0f, 24.0f, settings.semitones)) + juce::jlimit (-100.0f, 100.0f, settings.fineCents) * 0.01f;
    const float robotPeriod = (float) (fs / (440.0 * std::pow (2.0, ((double) robotNote - 69.0) / 12.0)));
    // with the character at zero the voice keeps its size; turned either way, the size follows by up to an octave
    const float formant = std::pow (2.0f, juce::jlimit (-1.0f, 1.0f, settings.character));
    const float shiftTarget = settings.shiftOn ? juce::jlimit (0.0f, 1.0f, settings.shiftMix) : 0.0f;
    const float drive = juce::jlimit (0.0f, 1.0f, settings.shiftDrive);
    const float drivePre = 1.0f + 6.0f * drive * drive, driveMakeup = 1.0f / std::pow (drivePre, 0.6f);
    const float spread = (1.0f - juce::jlimit (0.0f, 1.0f, settings.shiftWidth)) * juce::MathConstants<float>::pi * 0.25f;
    const float wideNear = std::cos (spread) , wideFar = std::sin (spread);       // copy A: near = left, far = right; copy B the other way round
    const float shiftDelay = (float) (fs * 0.012);

    // ---- Harmony
    const int mode = juce::jlimit (0, 2, settings.harmonyMode);
    if (mode != lastMode) { lastMode = mode; for (auto& held : heldNote) held = -1; }
    const int voicingVoices = numVoices (settings.voicing);
    const int chord = (liveChord >= 0 ? liveChord : settings.chordMask) & 0xFFF;
    bool partOn[kParts]; float partGain[kParts], gainL[kParts][kSingers], gainR[kParts][kSingers], partDelay[kParts], partSize[kParts], partCents[kParts];
    int activeParts = 0;
    for (int v = 0; v < kParts; ++v)
    {
        const auto& own = settings.voice[(size_t) v];
        const int t = v % kHarmonyVoices;
        partOn[v] = settings.harmonyOn && (mode == 0 ? v < voicingVoices : own.on);
        if (partOn[v]) ++activeParts;
        partGain[v] = mode == 0 ? 1.0f : juce::jlimit (0.0f, 1.0f, own.level);
        partDelay[v] = mode == 0 ? kVoiceDelayMs[t] : 11.0f + juce::jlimit (0.0f, 60.0f, own.delayMs);
        partSize[v] = (1.0f + settings.humanise * kVoiceSize[t]) * (mode == 0 ? 1.0f : std::pow (2.0f, 0.5f * juce::jlimit (-1.0f, 1.0f, own.size)));
        partCents[v] = mode == 0 ? kVoiceCents[t] : kVoiceCents[t] * settings.humanise + juce::jlimit (-50.0f, 50.0f, own.detuneCents);
        const float place = (mode == 0 ? kVoicePan[t] : juce::jlimit (-1.0f, 1.0f, own.pan)) * settings.harmonyWidth;
        for (int s = 0; s < kSingers; ++s)
        {
            const float angle = (juce::jlimit (-1.0f, 1.0f, place + kChoirPan[s] * settings.harmonyWidth) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gainL[v][s] = std::cos (angle); gainR[v][s] = std::sin (angle);
        }
    }
    const float choirTarget = settings.harmonyOn ? juce::jlimit (0.0f, 1.0f, settings.choir) : 0.0f;
    const float harmonyTarget = settings.harmonyOn ? settings.harmonyMix * (activeParts > 1 ? 0.8f : 1.0f) : 0.0f;
    const float glide = 1.0f - std::exp (-1.0f / (float) (fs * 0.012));
    const float periodGlide = 1.0f - std::exp (-1.0f / (float) (fs * 0.003));
    const float hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 150.0f / (float) fs);

    for (int i = 0; i < n; ++i)
    {
        const float dryL = left[i], dryR = right != nullptr ? right[i] : left[i];
        const float mono = 0.5f * (dryL + dryR);
        engine.write (0, mono);
        if (detector.push (mono))
        {
            const float f = detector.frequency();
            const bool pitched = f > 0.0f && detector.clarity() > 0.5f;
            // carry on through a 10 or 20 ms gap in the detector's answers, as Tune does
            if (pitched) { if (! voiced) snap = true; voiced = true; unsureFrames = 0; }
            else if (voiced && ++unsureFrames > 2) { voiced = false; unsureFrames = 0; }
            if (pitched)
            {
                period = (float) (fs / (double) f);
                const float note = 69.0f + 12.0f * std::log2 (f / 440.0f);
                sungNote.store (note, std::memory_order_relaxed);
                // the lead's nearest note of the scale decides which notes the harmonies take
                const int lead = Tuner::nearestAllowed (note, settings.key, settings.scale, settings.noteMask);
                int taken[kParts + 1]; int numTaken = 0;
                taken[numTaken++] = lead;
                for (int v = 0; v < kParts; ++v)
                {
                    int h = -1;
                    if (partOn[v])
                    {
                        if (mode == 0) h = harmonyNote (lead, v, settings.voicing, settings.key, settings.scale, settings.noteMask);
                        else
                        {
                            const int parallel = stepNote (lead, settings.voice[(size_t) v].steps, settings.key, settings.scale, settings.noteMask);
                            if (mode == 2 && chord != 0) h = chordNote (chord, parallel, heldNote[(size_t) v], taken, numTaken);
                            else h = settings.leading ? ledNote (heldNote[(size_t) v], parallel, lead) : parallel;
                        }
                        if (mode == 0 && settings.leading) h = ledNote (heldNote[(size_t) v], h, lead);
                        heldNote[(size_t) v] = h;
                        if (numTaken <= kParts) taken[numTaken++] = h;
                    }
                    else heldNote[(size_t) v] = -1;
                    shownHarmony[(size_t) v].store (h, std::memory_order_relaxed);
                    // the voice moves by the interval, so it keeps the lead's own vibrato and slides
                    ratioGoal[(size_t) v] = h >= 0 ? std::pow (2.0f, ((float) (h - lead) + partCents[v] * 0.01f) / 12.0f) : 1.0f;
                }
            }
            else if (! voiced) sungNote.store (0.0f, std::memory_order_relaxed);
        }
        if (snap) { periodNow = period; snap = false; } else periodNow += (period - periodNow) * periodGlide;
        // Humanise: real backing singers are never exactly together. Each voice wanders a few cents and a few
        // milliseconds around the lead, slowly and each at its own speed, and is a slightly different size.
        if (++driftCount >= 64)
        {
            driftCount = 0;
            static const float speeds[kHarmonyVoices] = { 0.23f, 0.31f, 0.17f };
            for (int v = 0; v < kParts; ++v)
                for (int s = 0; s < kSingers; ++s)
                {
                    auto& phase = driftPhase[(size_t) v][(size_t) s];
                    phase += juce::MathConstants<float>::twoPi * speeds[(v + s) % kHarmonyVoices] * (s == 0 ? 1.0f : 1.0f + 0.21f * (float) s) * 64.0f / (float) fs;
                    if (phase > 1000.0f) phase -= 1000.0f * std::floor (phase / 1000.0f);
                    // the extra singers of a choir wander whether or not Humanise is up: that is what makes them a group
                    const float loose = s == 0 ? settings.humanise : juce::jmax (settings.humanise, 0.6f);
                    driftCents[(size_t) v][(size_t) s] = loose * 9.0f * (0.6f * std::sin (phase) + 0.4f * std::sin (phase * 2.37f + 1.0f));
                    driftMs[(size_t) v][(size_t) s] = loose * 7.0f * (0.5f + 0.5f * std::sin (phase * 0.71f + 2.0f * (float) (v + s)));
                }
        }
        shiftLevel += (shiftTarget - shiftLevel) * 0.002f;
        harmonyLevel += (harmonyTarget - harmonyLevel) * 0.002f;
        choirNow += (choirTarget - choirNow) * 0.002f;
        consonantGain += ((voiced ? 1.0f : 0.4f) - consonantGain) * 0.002f;

        float outL = dryL, outR = dryR;
        if (settings.shiftOn || shiftLevel > 1.0e-4f)
        {
            float shiftedL = 0.0f, shiftedR = 0.0f;
            if (shiftMode == 1)
            {
                // two copies moved the same distance in opposite directions, one each side
                float up = 0.0f, down = 0.0f;
                engine.renderSync (shiftVoice, periodNow, voiced, shiftRatio, shiftDelay, &up, formant);
                engine.renderSync (shiftVoiceB, periodNow, voiced, 1.0f / shiftRatio, shiftDelay, &down, formant);
                shiftedL = up * wideNear + down * wideFar; shiftedR = up * wideFar + down * wideNear;
                if (right == nullptr) shiftedL = 0.7071f * (up + down);
            }
            else
            {
                float moved = 0.0f, by = shiftRatio;
                if (shiftMode == 2)
                {
                    // whatever is sung comes out on the one note
                    robotRatio += (juce::jlimit (0.25f, 4.0f, periodNow / robotPeriod) - robotRatio) * 0.02f;
                    by = robotRatio;
                }
                engine.renderSync (shiftVoice, periodNow, voiced, by, shiftDelay, &moved, formant);
                shiftedL = shiftedR = moved;
            }
            if (drive > 0.001f) { shiftedL = std::tanh (shiftedL * drivePre) * driveMakeup; shiftedR = std::tanh (shiftedR * drivePre) * driveMakeup; }
            outL = dryL * (1.0f - shiftLevel) + shiftedL * shiftLevel;
            outR = dryR * (1.0f - shiftLevel) + shiftedR * shiftLevel;
        }
        if (activeParts > 0 || harmonyLevel > 1.0e-4f)
        {
            float addL = 0.0f, addR = 0.0f;
            const int singers = choirNow > 0.004f ? kSingers : 1;
            for (int v = 0; v < kParts; ++v)
            {
                auto& level = partLevel[(size_t) v];
                level += ((partOn[v] ? partGain[v] : 0.0f) - level) * 0.002f;
                if (! partOn[v] && level < 1.0e-4f) continue;
                ratio[(size_t) v] += (ratioGoal[(size_t) v] - ratio[(size_t) v]) * glide;
                for (int s = 0; s < singers; ++s)
                {
                    float out = 0.0f;
                    const float wander = std::pow (2.0f, (driftCents[(size_t) v][(size_t) s] + kChoirCents[s]) / 1200.0f);
                    engine.renderSync (harmony[(size_t) v][(size_t) s], periodNow, voiced, ratio[(size_t) v] * wander,
                                       (float) (fs * 0.001 * (partDelay[v] + kChoirMs[s] + driftMs[(size_t) v][(size_t) s])), &out, partSize[v] * (1.0f + kChoirSize[s]));
                    auto& low = hp[(size_t) v][(size_t) s];
                    low = hpCoeff * low + (1.0f - hpCoeff) * out;
                    out = (out - low) * level * (s == 0 ? 1.0f - 0.3f * choirNow : 0.75f * choirNow);
                    addL += out * gainL[v][s]; addR += out * gainR[v][s];
                }
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
