#include "Reverb.h"

namespace mu
{
namespace
{
// What makes each space what it is. Line lengths are in milliseconds at size 1: the shortest and longest of the
// network's delay lines, with the rest spread between them. "early" and "late" are the levels of the first
// reflections and of the tail, "mod" scales how much the tail moves, "high" how long the highs last compared with a
// plate, "earlySet" picks the pattern of first reflections (-1: none) and "smear" stretches the input diffusers.
struct Space { float minMs, maxMs, early, late, diffusion, mod, high; int earlySet, lines, diffusers; float smear; };
const Space kSpace[Reverb::NumTypes] =
{
    /* Room      */ {  9.0f,  37.0f, 0.480f, 0.200f, 0.62f, 0.5f, 0.80f,  0, 16, 4, 1.0f },
    /* Plate     */ {  0.0f,   0.0f, 0.000f, 0.322f, 0.75f, 2.0f, 1.00f, -1,  0, 4, 1.0f },
    /* Hall      */ { 31.0f,  97.0f, 0.310f, 0.322f, 0.74f, 1.0f, 0.85f,  2, 16, 4, 1.6f },
    /* Chamber   */ { 17.0f,  59.0f, 0.392f, 0.233f, 0.72f, 0.7f, 0.95f,  1, 16, 4, 1.2f },
    /* Cathedral */ { 29.0f, 151.0f, 0.282f, 0.440f, 0.76f, 1.2f, 0.70f,  3, 16, 5, 2.0f },
    /* Ambient   */ { 23.0f,  89.0f, 0.000f, 0.333f, 0.80f, 1.6f, 0.90f, -1, 16, 6, 3.0f },
    /* Vintage   */ { 21.0f,  71.0f, 0.369f, 0.271f, 0.60f, 2.8f, 0.60f,  1,  8, 3, 1.0f },
    /* Shimmer   */ { 37.0f, 113.0f, 0.157f, 0.392f, 0.76f, 1.2f, 0.90f,  2, 16, 5, 2.0f },
    /* Gated     */ {  0.0f,   0.0f, 0.000f, 0.225f, 0.72f, 0.0f, 1.00f, -1,  0, 5, 1.6f },
};
const float kDiffuseMs[6] = { 1.9f, 3.1f, 4.7f, 7.3f, 11.3f, 17.9f };

// First reflections: when they arrive (ms, left and right) and how strong they are. A room's come at once and
// close together; a cathedral's take a long time to come back and are far apart.
const float kEarlyMs[4][2][8] =
{
    { {  3.1f,  5.9f,  9.7f, 13.3f, 17.9f, 22.7f,  28.1f,  34.3f }, {  3.7f,  6.7f, 10.9f, 14.9f, 19.3f, 24.1f,  29.9f,  36.1f } },      // room
    { {  6.1f, 10.3f, 15.7f, 21.1f, 27.7f, 35.3f,  43.9f,  53.3f }, {  7.3f, 11.9f, 17.3f, 23.3f, 30.1f, 37.9f,  46.7f,  56.3f } },      // chamber
    { { 12.7f, 19.9f, 28.3f, 37.1f, 47.9f, 59.3f,  71.9f,  86.3f }, { 14.3f, 22.1f, 30.7f, 40.3f, 51.1f, 62.9f,  75.7f,  90.1f } },      // hall
    { { 22.3f, 34.1f, 47.3f, 61.7f, 77.9f, 95.3f, 114.7f, 135.1f }, { 25.1f, 37.9f, 51.7f, 66.1f, 82.3f, 99.7f, 119.3f, 140.3f } },      // cathedral
};
const float kEarlyGains[4][8] =
{
    { 0.80f, 0.71f, 0.62f, 0.54f, 0.46f, 0.38f, 0.31f, 0.24f },
    { 0.70f, 0.62f, 0.55f, 0.48f, 0.41f, 0.34f, 0.28f, 0.22f },
    { 0.55f, 0.50f, 0.45f, 0.40f, 0.35f, 0.30f, 0.25f, 0.20f },
    { 0.45f, 0.42f, 0.38f, 0.34f, 0.30f, 0.26f, 0.22f, 0.18f },
};

// The plate tank's lengths, in samples at the 29,761 Hz its published design uses; scaled to the real sample rate.
const double kPlateRate = 29761.0;
const float kPlateIn[4] = { 142.0f, 107.0f, 379.0f, 277.0f };
const float kTankAp1[2] = { 672.0f, 908.0f }, kTankDelay1[2] = { 4453.0f, 4217.0f }, kTankAp2[2] = { 1800.0f, 2656.0f }, kTankDelay2[2] = { 3720.0f, 3163.0f };

int nextPrime (int n)
{
    auto prime = [] (int v) { if (v < 4) return v > 1; if (v % 2 == 0) return false; for (int d = 3; d * d <= v; d += 2) if (v % d == 0) return false; return true; };
    while (! prime (n)) ++n;
    return n;
}
}

const char* Reverb::typeBlurb (int t) noexcept
{
    switch (t)
    {
        case Room:      return "Room: small and close. Reflections arrive at once, the tail thins quickly. Puts a dry vocal in a real space without washing it.";
        case Plate:     return "Plate: a sheet of metal, not a room. Dense and bright from the first instant with no separate echoes. The classic vocal reverb.";
        case Hall:      return "Hall: large. The first reflections come back late and quiet and the tail moves slowly. For sung hooks and anything that should feel big.";
        case Chamber:   return "Chamber: a hard-walled echo room. Thicker than a room, tighter than a hall, and it keeps its highs. Good on rap vocals that need body.";
        case Cathedral: return "Cathedral: huge, dark and slow. Long gaps before the reflections and a tail that hangs. Use a little, on intros and endings.";
        case Ambient:   return "Ambient: no reflections at all, just a soft cloud that swells in behind the sound. For pads, adlibs and washes.";
        case Vintage:   return "Vintage: an early digital reverb. Darker, a little grainy, and the tail moves audibly. The 80s vocal sound.";
        case Shimmer:   return "Shimmer: a hall whose tail is fed back in an octave higher, so it rises into a glassy halo. Shimmer sets how much.";
        case Gated:     return "Gated: a burst of reflections that holds its level and then stops dead. Decay sets how long the burst lasts. Big on snares and adlibs.";
        default:        return "";
    }
}

void Reverb::setFade (Fade& f, float trip, float decay, float highDecay, float lowDecay, bool frozen) noexcept
{
    if (frozen) { f.gain = 1.0f; f.pole = 0.0f; f.lowExtra = 0.0f; return; }
    const float mid = std::pow (10.0f, -3.0f * trip / decay);                 // falls 60 dB in "decay" seconds
    const float high = std::pow (10.0f, -3.0f * trip / highDecay);
    const float low = std::pow (10.0f, -3.0f * trip / lowDecay);
    f.pole = (mid - high) / (mid + high);
    f.gain = mid * (1.0f - f.pole);
    f.lowExtra = low / mid - 1.0f;
}

void Reverb::prepare (double sampleRate, int, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    const double k = fs / kPlateRate;
    for (int c = 0; c < 2; ++c)
    {
        pre[c].resize ((int) (fs * 0.85));                                       // 500 ms of pre-delay and the latest reflection of the largest space
        for (int i = 0; i < 6; ++i) diffuse[c][(size_t) i].line.resize ((int) (fs * 0.001 * kDiffuseMs[i] * 1.07 * 3.0) + 16);
        gateLine[c].resize ((int) (fs * 1.1));
        tankAp1[c].line.resize ((int) (kTankAp1[c] * k * 1.6) + 32 + (int) (64.0 * fs / 48000.0));
        tankAp2[c].line.resize ((int) (kTankAp2[c] * k * 1.6) + 16);
        tankDelay1[c].resize ((int) (kTankDelay1[c] * k * 1.6) + 16);
        tankDelay2[c].resize ((int) (kTankDelay2[c] * k * 1.6) + 16);
    }
    for (int i = 0; i < 4; ++i) plateIn[i].line.resize ((int) (kPlateIn[i] * k) + 16);
    for (auto& l : lines) l.resize ((int) (fs * 0.001 * 151.0 * 2.0) + 64 + (int) (64.0 * fs / 48000.0));      // room for the largest space at its largest size, and for its movement
    shimmerBuffer.assign ((size_t) (fs * 0.2) + 8, 0.0f);
    for (int i = 0; i < kLines; ++i) { lfoSin[(size_t) i] = std::sin (2.399f * (float) i); lfoCos[(size_t) i] = std::cos (2.399f * (float) i); }
    lowCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 220.0f / (float) fs);
    inLpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 7000.0f / (float) fs);
    shimmerCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4500.0f / (float) fs);
    type = -1;
    reset();
}

void Reverb::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        pre[c].clear(); gateLine[c].clear();
        for (auto& ap : diffuse[c]) ap.line.clear();
        tankAp1[c].line.clear(); tankAp2[c].line.clear(); tankDelay1[c].clear(); tankDelay2[c].clear();
        tankFade[c].state = tankFade[c].lowState = 0.0f; tankOut[c] = 0.0f;
        hpState[c][0] = hpState[c][1] = lpState[c] = inHp[c] = inLp[c] = 0.0f;
    }
    for (auto& ap : plateIn) ap.line.clear();
    for (auto& l : lines) l.clear();
    for (auto& f : fades) f.state = f.lowState = 0.0f;
    std::fill (shimmerBuffer.begin(), shimmerBuffer.end(), 0.0f);
    shimmerPos = 0; shimmerPhase = 0.0f; shimmerLp = shimmerLp2 = shimmerHp = shimmerFeed = 0.0f;
    envelope = 0.0f; duckGain = 1.0f; mixNow = 0.0f; wetPeak = 0.0f; inputGain = 1.0f;
}

// A different space: nothing that is still ringing in the old one belongs in it.
void Reverb::applyType() noexcept
{
    type = juce::jlimit (0, (int) NumTypes - 1, settings.type);
    const auto& sp = kSpace[type];
    lineCount = sp.lines; diffuseCount = sp.diffusers; highShare = sp.high;
    earlyCount = sp.earlySet >= 0 ? kEarly : 0;
    for (auto& l : lines) l.clear();
    for (auto& f : fades) f.state = f.lowState = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        for (auto& ap : diffuse[c]) ap.line.clear();
        gateLine[c].clear();
        tankAp1[c].line.clear(); tankAp2[c].line.clear(); tankDelay1[c].clear(); tankDelay2[c].clear();
        tankFade[c].state = tankFade[c].lowState = 0.0f; tankOut[c] = 0.0f;
        inLp[c] = 0.0f;
    }
    for (auto& ap : plateIn) ap.line.clear();
    std::fill (shimmerBuffer.begin(), shimmerBuffer.end(), 0.0f);
    shimmerFeed = 0.0f;
    shownSize = -1.0f; shownDecay = -1.0f; shownDiffusion = -1.0f; shownEarly = -1.0f; shownGate = -1.0f;
}

// Where everything sits for this space at this size.
void Reverb::layout() noexcept
{
    const auto& sp = kSpace[type];
    const float size = juce::jlimit (0.3f, 2.0f, settings.size);
    shownSize = settings.size;
    for (int i = 0; i < lineCount; ++i)
    {
        const float ms = sp.minMs * std::pow (sp.maxMs / sp.minMs, (float) i / (float) (lineCount - 1)) * size;
        lengths[(size_t) i] = (float) juce::jmin (nextPrime ((int) (fs * 0.001 * ms)), (int) lines[(size_t) i].data.size() - 16 - (int) (32.0 * fs / 48000.0));       // the movement never reads past the end
    }
    if (sp.earlySet >= 0)
        for (int c = 0; c < 2; ++c)
            for (int e = 0; e < kEarly; ++e)
                earlyTime[c][e] = juce::jmin ((float) (fs * 0.001 * kEarlyMs[sp.earlySet][c][e] * size), (float) (fs * 0.3));
    for (int e = 0; e < kEarly; ++e) earlyGain[e] = sp.earlySet >= 0 ? kEarlyGains[sp.earlySet][e] : 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 6; ++i)
            diffuse[c][(size_t) i].length = (float) juce::jmin (nextPrime ((int) (fs * 0.001 * kDiffuseMs[i] * (c == 0 ? 1.0 : 1.07) * sp.smear)), (int) diffuse[c][(size_t) i].line.data.size() - 8);
    // The plate: the same tank, larger or smaller. One trip round the full-size tank takes about 0.7 s, so a short
    // decay would fall away in a few big steps instead of smoothly; for short decays the tank is made smaller.
    const float plateDecay = juce::jlimit (0.1f, 30.0f, currentDecaySeconds());
    const float plateSize = juce::jlimit (0.3f, 1.5f, size * juce::jmin (1.0f, 0.3f + 0.45f * plateDecay));
    tapScale = (float) (fs / kPlateRate) * plateSize;
    for (int i = 0; i < 4; ++i) plateIn[i].length = std::floor (kPlateIn[i] * (float) (fs / kPlateRate));
    for (int h = 0; h < 2; ++h)
    {
        tankApLen1[h] = std::floor (kTankAp1[h] * tapScale); tankApLen2[h] = std::floor (kTankAp2[h] * tapScale);
        tankLen1[h] = std::floor (kTankDelay1[h] * tapScale); tankLen2[h] = std::floor (kTankDelay2[h] * tapScale);
        tankAp1[h].g = 0.7f; tankAp2[h].g = 0.5f; tankAp2[h].length = tankApLen2[h];
    }
    shownDecay = -1.0f;
}

void Reverb::update() noexcept
{
    if (juce::jlimit (0, (int) NumTypes - 1, settings.type) != type) applyType();
    const float decay = juce::jlimit (0.1f, 30.0f, currentDecaySeconds());
    if (settings.size != shownSize || (type == Plate && decay != shownDecay)) layout();
    const auto& sp = kSpace[type];
    if (settings.lowCutHz != shownLowCut)
    {
        shownLowCut = settings.lowCutHz;
        hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * juce::jlimit (40.0f, 1000.0f, settings.lowCutHz) / (float) fs);      // keeps the reverb out of the low end
    }
    if (decay != shownDecay || settings.tone != shownTone || settings.lowDecay != shownLowDecay || settings.freeze != shownFreeze)
    {
        shownDecay = decay; shownTone = settings.tone; shownLowDecay = settings.lowDecay; shownFreeze = settings.freeze;
        // highs last between 25 % (dark) and 80 % (bright) as long as the mids; lows as long as Low decay says
        const float highDecay = juce::jmax (0.05f, decay * (0.25f + 0.55f * settings.tone) * highShare);
        const float lowDecay = juce::jmax (0.05f, decay * juce::jlimit (0.3f, 2.0f, settings.lowDecay));
        for (int i = 0; i < lineCount; ++i)
            setFade (fades[(size_t) i], lengths[(size_t) i] / (float) fs, decay, highDecay, lowDecay, settings.freeze);
        for (int h = 0; h < 2; ++h)
            // (the tank measures about 8 % longer than its lengths suggest, so it is asked for that much less)
            setFade (tankFade[h], (tankApLen1[h] + tankLen1[h] + tankApLen2[h] + tankLen2[h]) / (float) fs, decay * 0.92f, highDecay * 0.92f, lowDecay * 0.92f, settings.freeze);
        lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * (4500.0f + 9500.0f * settings.tone) / (float) fs);
    }
    if (settings.diffusion != shownDiffusion)
    {
        shownDiffusion = settings.diffusion;
        const float g = sp.diffusion * juce::jmin (1.0f, 0.35f + 0.8f * juce::jlimit (0.0f, 1.0f, settings.diffusion));
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 6; ++i) diffuse[c][(size_t) i].g = i < 2 ? g : g * 0.9f;
        plateIn[0].g = plateIn[1].g = g; plateIn[2].g = plateIn[3].g = g * 0.83f;
    }
    if (settings.early != shownEarly)
    {
        shownEarly = settings.early;
        const float e = juce::jlimit (0.0f, 1.0f, settings.early);
        earlyLevel = sp.early * juce::jmin (2.0f, e * 2.0f);
        lateLevel = sp.late * juce::jmin (1.0f, (1.0f - e) * 2.0f);
    }
    preSamples = juce::jlimit (1.0f, (float) (fs * 0.5), (float) (fs * 0.001 * currentPreDelayMs()));
    modSamples = juce::jlimit (0.0f, 1.0f, settings.modDepth) * sp.mod * 10.0f * (float) (fs / 48000.0);
    const float rate = juce::jlimit (0.05f, 5.0f, settings.modRate);
    for (int i = 0; i < kLines; ++i)
    {
        const float spread = 0.55f + 0.9f * ((float) i * 0.6180339f - std::floor ((float) i * 0.6180339f));      // every line on its own cycle
        lfoStep[(size_t) i] = 2.0f * std::sin (juce::MathConstants<float>::pi * rate * spread / (float) fs);
    }
    if (type == Gated)
    {
        const float gate = gateSeconds (decay);
        if (gate != shownGate)
        {
            shownGate = gate;
            // eighty reflections a side spread over the burst, a little stronger towards the end so it holds its level
            for (int c = 0; c < 2; ++c)
            {
                juce::Random random (4711 + 97 * c);
                for (int t = 0; t < kGateTaps; ++t)
                {
                    const float at = ((float) t + random.nextFloat()) / (float) kGateTaps;
                    gateTime[c][t] = juce::jlimit (1.0f, (float) gateLine[c].data.size() - 8.0f, at * gate * (float) fs);
                    gateGain[c][t] = (random.nextBool() ? 1.0f : -1.0f) * (0.75f + 0.25f * at) * 0.21f;
                }
            }
        }
    }
}

// Sixteen (or eight) delay lines feeding each other. Each line is read a little earlier or later as its own slow
// cycle turns, fades by one trip's worth, and then all of them are mixed into each other without gaining or
// losing energy before going round again.
void Reverb::network (const float* feed, float& wetL, float& wetR) noexcept
{
    float v[kLines];
    const int n = lineCount;
    for (int k = 0; k < n; ++k)
    {
        const auto i = (size_t) k;
        lfoSin[i] += lfoStep[i] * lfoCos[i]; lfoCos[i] -= lfoStep[i] * lfoSin[i];
        v[k] = fade (fades[i], lines[i].readFrac (lengths[i] + modSamples * lfoSin[i]));
    }
    wetL = 0.0f; wetR = 0.0f;
    for (int k = 0; k < n; ++k)
    {
        wetL += (((k & 1) != 0) != ((k & 4) != 0)) ? -v[k] : v[k];
        wetR += (((k & 2) != 0) != ((k & 8) != 0)) ? -v[k] : v[k];
    }
    const float out = n == 8 ? 1.0f : 0.70710678f;
    wetL *= out; wetR *= out;
    // a Hadamard matrix, done as passes of sums and differences
    for (int span = 1; span < n; span <<= 1)
        for (int a = 0; a < n; a += span << 1)
            for (int b = a; b < a + span; ++b) { const float p = v[b], q = v[b + span]; v[b] = p + q; v[b + span] = p - q; }
    const float norm = n == 8 ? 0.35355339f : 0.25f, in = n == 8 ? 0.5f : 0.35355339f;
    for (int k = 0; k < n; ++k)
        lines[(size_t) k].write (v[k] * norm + in * ((k & 1) ? feed[1] : feed[0]) * ((k & 2) ? -1.0f : 1.0f));
}

// The plate: two loops, each an all-pass that moves, a delay, the fading, a second all-pass and a second delay,
// feeding each other in a figure of eight. The output is taken from seven points round the tank for each side.
void Reverb::plate (float x, float& wetL, float& wetR) noexcept
{
    for (auto& ap : plateIn) x = ap.run (x);
    float next[2];
    for (int h = 0; h < 2; ++h)
    {
        const auto i = (size_t) h;
        lfoSin[i] += lfoStep[i] * lfoCos[i]; lfoCos[i] -= lfoStep[i] * lfoSin[i];
        const float a = tankAp1[h].runFrac (x + tankOut[1 - h], tankApLen1[h] + 1.5f * modSamples * lfoSin[i]);
        tankDelay1[h].write (a);
        const float b = tankAp2[h].run (fade (tankFade[h], tankDelay1[h].read ((int) tankLen1[h])));
        tankDelay2[h].write (b);
        next[h] = tankDelay2[h].read ((int) tankLen2[h]);
    }
    tankOut[0] = next[0]; tankOut[1] = next[1];
    auto at = [this] (float samples) { return (int) (samples * tapScale); };
    wetL = tankDelay1[1].read (at (266.0f)) + tankDelay1[1].read (at (2974.0f)) - tankAp2[1].line.read (at (1913.0f)) + tankDelay2[1].read (at (1996.0f))
         - tankDelay1[0].read (at (1990.0f)) - tankAp2[0].line.read (at (187.0f)) - tankDelay2[0].read (at (1066.0f));
    wetR = tankDelay1[0].read (at (353.0f)) + tankDelay1[0].read (at (3627.0f)) - tankAp2[0].line.read (at (1228.0f)) + tankDelay2[0].read (at (2673.0f))
         - tankDelay1[1].read (at (2111.0f)) - tankAp2[1].line.read (at (335.0f)) - tankDelay2[1].read (at (121.0f));
}

// Gated: nothing goes round. The smeared sound is simply heard again at eighty moments spread over the burst.
void Reverb::gated (const float* feed, float& wetL, float& wetR) noexcept
{
    gateLine[0].write (feed[0]); gateLine[1].write (feed[1]);
    wetL = 0.0f; wetR = 0.0f;
    for (int t = 0; t < kGateTaps; ++t)
    {
        wetL += gateGain[0][t] * gateLine[0].read ((int) gateTime[0][t]);
        wetR += gateGain[1][t] * gateLine[1].read ((int) gateTime[1][t]);
    }
}

// An octave up: two readers sweep through the last 85 ms twice as fast as it was written, taking turns so the
// jump one of them has to make is always made while it is silent.
float Reverb::shimmerVoice (float x) noexcept
{
    const int size = (int) shimmerBuffer.size();
    const float window = (float) (fs * 0.085);
    // Only what is under about 4.5 kHz goes in: read at double speed, anything higher would fold back down as
    // tones that were never played, and each trip round would add more of them.
    shimmerLp += (x - shimmerLp) * shimmerCoeff;
    shimmerLp2 += (shimmerLp - shimmerLp2) * shimmerCoeff;
    shimmerBuffer[(size_t) shimmerPos] = shimmerLp2;
    shimmerPhase += 1.0f / window; if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
    const float other = shimmerPhase + 0.5f >= 1.0f ? shimmerPhase - 0.5f : shimmerPhase + 0.5f;
    auto read = [&] (float phase) { int i = shimmerPos - (int) (window * (1.0f - phase)) - 1; if (i < 0) i += size; return shimmerBuffer[(size_t) i]; };
    const float s = std::sin (juce::MathConstants<float>::pi * shimmerPhase);
    float y = s * s * read (shimmerPhase) + (1.0f - s * s) * read (other);
    shimmerPos = shimmerPos + 1 == size ? 0 : shimmerPos + 1;
    shimmerHp += (y - shimmerHp) * 0.03f;           // and no rumble
    y -= shimmerHp;
    return std::tanh (y * 1.5f) / 1.5f;
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
    const float wantedIn = settings.freeze ? 0.0f : 1.0f;
    // a long tail holds more sound, so less of it is fed back: the halo grows with the tail without taking over
    const float tail = juce::jlimit (0.1f, 30.0f, currentDecaySeconds());
    const float shimmerAmount = type == Shimmer ? 0.55f * juce::jlimit (0.0f, 1.0f, settings.shimmer) * juce::jmin (1.0f, std::sqrt (2.0f / tail)) : 0.0f;

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

        // into the pre-delay, with the lows taken off first (and, for the vintage unit, the top as well)
        inputGain += (wantedIn - inputGain) * mixStep;
        float in[2] = { dryL, dryR };
        for (int c = 0; c < 2; ++c)
        {
            inHp[c] = inHpCoeff * inHp[c] + (1.0f - inHpCoeff) * in[c];
            float x = (in[c] - inHp[c]) * inputGain;
            if (type == Vintage) { inLp[c] = inLpCoeff * inLp[c] + (1.0f - inLpCoeff) * x; x = inLp[c]; }
            pre[c].write (x);
        }
        float feed[2], early[2] = { 0.0f, 0.0f };
        for (int c = 0; c < 2; ++c)
        {
            float x = pre[c].readFrac (preSamples);
            for (int e = 0; e < earlyCount; ++e) early[c] += earlyGain[e] * pre[c].readFrac (preSamples + earlyTime[c][e]);
            for (int d = 0; d < diffuseCount; ++d) x = diffuse[c][(size_t) d].run (x);
            feed[c] = x + shimmerFeed;
        }

        float wetL = 0.0f, wetR = 0.0f;
        if (type == Plate)      plate (0.5f * (feed[0] + feed[1]), wetL, wetR);
        else if (type == Gated) gated (feed, wetL, wetR);
        else                    network (feed, wetL, wetR);
        if (shimmerAmount > 0.0f) shimmerFeed = shimmerAmount * shimmerVoice (0.5f * (wetL + wetR));
        else shimmerFeed = 0.0f;

        wetL = wetL * lateLevel + early[0] * earlyLevel;
        wetR = wetR * lateLevel + early[1] * earlyLevel;
        // width: what the two sides share stays, what differs between them is turned down
        const float middle = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * juce::jlimit (0.0f, 1.0f, settings.width);
        float wet[2] = { middle + side, middle - side };
        for (int c = 0; c < 2; ++c)
        {
            // the low cut, twice over, so what is under it is properly gone and the reverb adds no weight to the low mids
            for (int s = 0; s < 2; ++s) { hpState[c][s] = hpCoeff * hpState[c][s] + (1.0f - hpCoeff) * wet[c]; wet[c] -= hpState[c][s]; }
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
