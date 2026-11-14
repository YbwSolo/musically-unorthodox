#include "ColourFX.h"

namespace mu
{
namespace
{
const float kTwoPi = juce::MathConstants<float>::twoPi;
}

//==================================================================== filter
void ColourBiquad::highPass (float hz, float q, double fs) noexcept
{
    const float w = kTwoPi * hz / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha;
    b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
}

void ColourBiquad::lowPass (float hz, float q, double fs) noexcept
{
    const float w = kTwoPi * juce::jmin (hz, (float) fs * 0.45f) / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha;
    b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
}

void ColourBiquad::peak (float hz, float q, float gainDb, double fs) noexcept
{
    const float A = std::pow (10.0f, gainDb / 40.0f);
    const float w = kTwoPi * hz / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q), a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0; b1 = -2.0f * c / a0; b2 = (1.0f - alpha * A) / a0; a1 = -2.0f * c / a0; a2 = (1.0f - alpha / A) / a0;
}

float ColourBiquad::magnitudeDb (float hz, double fs) const noexcept
{
    const double w = juce::MathConstants<double>::twoPi * (double) hz / fs;
    const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
    const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    return (float) (10.0 * std::log10 (juce::jmax (1.0e-20, (nr * nr + ni * ni) / juce::jmax (1.0e-20, dr * dr + di * di))));
}

//==================================================================== Phone
void Phone::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    shownTone = -1.0f;
    update();
    reset();
}

void Phone::reset() noexcept
{
    for (auto* f : { &hp1, &hp2, &lp1, &lp2, &nose }) f->clear();
    mixNow = 0.0f;
}

void Phone::update() noexcept
{
    if (settings.tone == shownTone) return;
    shownTone = settings.tone;
    const float lo = lowEdgeHz (settings.tone), hi = highEdgeHz (settings.tone);
    hp1.highPass (lo, 0.5412f, fs); hp2.highPass (lo, 1.3066f, fs);
    lp1.lowPass (hi, 0.5412f, fs);  lp2.lowPass (hi, 1.3066f, fs);
    nose.peak (std::sqrt (lo * hi) * 1.25f, 1.1f, 5.0f, fs);
}

float Phone::responseDb (float hz) const noexcept
{
    return hp1.magnitudeDb (hz, fs) + hp2.magnitudeDb (hz, fs) + lp1.magnitudeDb (hz, fs) + lp2.magnitudeDb (hz, fs) + nose.magnitudeDb (hz, fs);
}

void Phone::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    update();
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const float pre = 1.0f + 9.0f * juce::jlimit (0.0f, 1.0f, settings.drive);
    const float post = 1.6f / std::sqrt (pre);                  // keeps the loudness roughly where it was as the drive goes up
    const float step = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    for (int i = 0; i < n; ++i)
    {
        mixNow += (settings.amount - mixNow) * step;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            float x = hp2.run (c, hp1.run (c, d[i]));
            x = nose.run (c, x);
            x = std::tanh (x * pre) * post;
            x = lp2.run (c, lp1.run (c, x));
            d[i] = d[i] * (1.0f - mixNow) + x * mixNow;
        }
    }
}

//==================================================================== Lo-fi
void LoFi::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : line) l.assign ((size_t) (fs * 0.05) + 8, 0.0f);
    reset();
}

void LoFi::reset() noexcept
{
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    pos = 0; mixNow = 0.0f;
    for (int c = 0; c < 2; ++c) { phase[c] = 1.0f; held[c] = 0.0f; lp[c] = 0.0f; }
}

void LoFi::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const int size = (int) line[0].size();
    const float crush = juce::jlimit (0.0f, 1.0f, settings.crush);
    const float step = rateHz (crush, fs) / (float) fs;                       // 1 = every sample is kept
    const float levels = std::pow (2.0f, bits (crush) - 1.0f);
    const float lpCoeff = std::exp (-kTwoPi * (14000.0f * std::pow (0.25f, crush)) / (float) fs);
    const float base = (float) (fs * 0.012), swing = (float) (fs * 0.005) * juce::jlimit (0.0f, 1.0f, settings.wobble);
    const float wowStep = kTwoPi * 0.55f / (float) fs, flutterStep = kTwoPi * 6.3f / (float) fs;
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    for (int i = 0; i < n; ++i)
    {
        wow += wowStep; if (wow > kTwoPi) wow -= kTwoPi;
        flutter += flutterStep; if (flutter > kTwoPi) flutter -= kTwoPi;
        const float back = base + swing * (std::sin (wow) + 0.025f * std::sin (flutter));      // a slow drift with a faint fast shiver on top
        const int whole = (int) back; const float frac = back - (float) whole;
        mixNow += (settings.mix - mixNow) * mixStep;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            line[(size_t) c][(size_t) pos] = d[i];
            int a = pos - whole; if (a < 0) a += size;
            int b = a - 1; if (b < 0) b += size;
            const float x = swing > 0.0f ? line[(size_t) c][(size_t) a] * (1.0f - frac) + line[(size_t) c][(size_t) b] * frac : d[i];
            // keep one sample in every few and hold it, then round it to a coarse level
            phase[c] += step;
            if (phase[c] >= 1.0f) { phase[c] -= 1.0f; held[c] = std::round (x * levels) / levels; }
            lp[c] = lpCoeff * lp[c] + (1.0f - lpCoeff) * held[c];
            d[i] = d[i] * (1.0f - mixNow) + lp[c] * mixNow;
        }
        pos = pos + 1 == size ? 0 : pos + 1;
    }
}

//==================================================================== Motion
void Motion::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate; channels = juce::jlimit (1, 2, numChannels);
    for (auto& l : line) l.assign ((size_t) (fs * 0.06) + 8, 0.0f);
    reset();
}

void Motion::reset() noexcept
{
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    pos = 0; feedback[0] = feedback[1] = 0.0f; mixNow = 0.0f;
    for (auto& channel : ap) for (auto& s : channel) s = 0.0f;
}

void Motion::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { if (mixNow > 0.0f) reset(); return; }
    if (settings.type != lastType) { lastType = settings.type; const float keep = mixNow; reset(); mixNow = keep; }
    const int n = buffer.getNumSamples(), ch = juce::jmin (channels, buffer.getNumChannels());
    const int size = (int) line[0].size();
    const float lfoStep = kTwoPi * juce::jlimit (0.02f, 12.0f, settings.rateHz) / (float) fs;
    const float depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    const float mixStep = 1.0f - std::exp (-1.0f / (float) (fs * 0.02));
    auto readLine = [&] (int c, float back)
    {
        const int whole = (int) back; const float frac = back - (float) whole;
        int a = pos - whole; if (a < 0) a += size;
        int b = a - 1; if (b < 0) b += size;
        return line[(size_t) c][(size_t) a] * (1.0f - frac) + line[(size_t) c][(size_t) b] * frac;
    };
    for (int i = 0; i < n; ++i)
    {
        lfo += lfoStep; if (lfo > kTwoPi) lfo -= kTwoPi;
        mixNow += (settings.mix - mixNow) * mixStep;
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float dry = d[i];
            const float angle = lfo + (c == 1 ? juce::MathConstants<float>::halfPi : 0.0f);       // the right side runs a quarter turn behind, for width
            const float sweep = 0.5f + 0.5f * std::sin (angle);
            float wet = 0.0f;
            if (settings.type == 0)
            {
                line[(size_t) c][(size_t) pos] = dry;
                const float a = (float) fs * 0.001f * (16.0f + 6.0f * depth * std::sin (angle));
                const float b = (float) fs * 0.001f * (24.0f + 5.0f * depth * std::sin (angle * 0.5f + 2.1f));
                wet = 0.6f * (readLine (c, a) + readLine (c, b));
                d[i] = dry + mixNow * (wet - dry * 0.25f);
            }
            else if (settings.type == 1)
            {
                line[(size_t) c][(size_t) pos] = dry + 0.55f * feedback[c];
                wet = readLine (c, (float) fs * 0.001f * (0.35f + (0.3f + 5.0f * depth) * sweep));
                feedback[c] = wet;
                d[i] = dry + mixNow * wet;
            }
            else
            {
                // six stages that each turn the wave round at one frequency; mixed back with the dry sound they cut notches
                const float hz = 300.0f * std::pow (10.0f, sweep * (0.3f + 0.7f * depth));
                const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, (float) fs * 0.4f) / (float) fs);
                const float k = (t - 1.0f) / (t + 1.0f);
                float x = dry + 0.5f * feedback[c];
                for (int s = 0; s < 6; ++s)
                {
                    const float y = k * x + ap[c][s];
                    ap[c][s] = x - k * y;
                    x = y;
                }
                feedback[c] = x;
                d[i] = dry + mixNow * x;
            }
        }
        pos = pos + 1 == size ? 0 : pos + 1;
    }
    shownPhase.store (lfo / kTwoPi, std::memory_order_relaxed);
}

//==================================================================== Chop
void Chop::prepare (double sampleRate)
{
    fs = sampleRate;
    smooth = 1.0f - std::exp (-1.0f / (float) (fs * 0.0025));       // edges take a few milliseconds, so the gate does not click
    reset();
}

float Chop::gateAt (float p, float shape) noexcept
{
    // Open around the middle of the first half of the step, shut around the middle of the second. Hardness sets
    // how quickly it moves between the two: at zero the whole step is one smooth rise and fall, at full it switches.
    float x = std::abs (p - std::floor (p) - 0.25f);
    if (x > 0.5f) x = 1.0f - x;
    const float w = juce::jmax (0.004f, 0.5f * (1.0f - juce::jlimit (0.0f, 1.0f, shape)));
    const float t = juce::jlimit (0.0f, 1.0f, (0.25f + w * 0.5f - x) / w);
    return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
}

void Chop::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! settings.enabled) { gain = 1.0f; return; }
    const int n = buffer.getNumSamples(), ch = buffer.getNumChannels();
    const double beats = (double) rateBeats (settings.rate);
    const double perSample = juce::jlimit (40.0, 300.0, settings.bpm) / 60.0 / fs / beats;
    if (playing) { phase = ppq / beats; phase -= std::floor (phase); }
    const float depth = juce::jlimit (0.0f, 1.0f, settings.depth);
    for (int i = 0; i < n; ++i)
    {
        const float wanted = 1.0f - depth * (1.0f - gateAt ((float) phase, settings.shape));
        gain += (wanted - gain) * smooth;
        for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= gain;
        phase += perSample; if (phase >= 1.0) phase -= 1.0;
    }
    shownPhase.store ((float) phase, std::memory_order_relaxed);
}
}
