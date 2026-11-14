#include "Compressor.h"
#include <cmath>

namespace mu
{
float Compressor::computeGainDb (float x, float T, float R, float W) noexcept
{
    // Soft-knee static curve (returns gain change in dB, <= 0).
    const float over = x - T;
    if (W <= 0.0f || over <= -W * 0.5f) return over <= 0.0f ? 0.0f : over * (1.0f / R - 1.0f);
    if (over >= W * 0.5f) return over * (1.0f / R - 1.0f);
    const float t = over + W * 0.5f;
    return (1.0f / R - 1.0f) * t * t / (2.0f * W);
}

float Compressor::autoMakeupDb (float T, float R, float W) noexcept
{
    // Gain needed to bring a signal sitting ~12 dB above threshold back to unity, halved so it never overshoots.
    const float probe = juce::jmin (0.0f, T + 12.0f);
    return -computeGainDb (probe, T, R, W) * 0.5f;
}

void Compressor::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);
    dry.setSize (2, maxBlockSize, false, false, true);
    makeupSmooth.reset (fs, 0.05);
    mixSmooth.reset (fs, 0.05);
    histInterval = juce::jmax (1, (int) (fs * 0.01));
    holdSamples = juce::jmax (1, (int) (fs * 0.008));        // one half-cycle of anything down to about 60 Hz
    levelHist.fill (-100.0f); grHist.fill (0.0f);
    scHz = -1.0f;
    reset();
}

void Compressor::reset()
{
    held = 0.0f; holdLeft = 0; catchHeld = 0.0f; catchHoldLeft = 0;
    levelGain = 0.0f; catchGain = 0.0f; gainSmooth = 0.0f; slowGr = 0.0f; rmsState = 0.0f;
    for (auto& f : scFilters) f.reset();
    grMeter.store (0.0f);
}

void Compressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    live = target;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (channels, buffer.getNumChannels());
    if (n == 0 || ch == 0) return;

    float inPeak = 0.0f;
    for (int c = 0; c < ch; ++c) inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, 0, n));
    inMeter.store (juce::Decibels::gainToDecibels (inPeak, -100.0f), std::memory_order_relaxed);

    auto pushHistory = [this] (float peak, float gr, int count)
    {
        histPeak = juce::jmax (histPeak, peak);
        histGr = juce::jmin (histGr, gr);
        histCounter += count;
        if (histCounter >= histInterval)
        {
            levelHist.push (juce::Decibels::gainToDecibels (histPeak, -100.0f));
            grHist.push (histGr);
            histCounter = 0; histPeak = 0.0f; histGr = 0.0f;
        }
    };

    if (! live.enabled)
    {
        gainSmooth = 0.0f; levelGain = 0.0f; catchGain = 0.0f; slowGr = 0.0f;
        grMeter.store (0.0f, std::memory_order_relaxed);
        outMeter.store (inMeter.load (std::memory_order_relaxed), std::memory_order_relaxed);
        pushHistory (inPeak, 0.0f, n);
        return;
    }

    for (int c = 0; c < ch; ++c) dry.copyFrom (c, 0, buffer, c, 0, n);

    // The Vocal style never lets the lows drive it: on real recordings a plosive or a thump is often the loudest
    // thing in the take, and a leveller that hears it holds the next few words down.
    const float scWanted = (live.style == CompStyle::Vocal && live.sidechainHpHz <= 25.0f) ? 110.0f : live.sidechainHpHz;
    const bool useSc = scWanted > 25.0f;
    if (useSc && std::abs (scWanted - scHz) > 0.5f)
    {
        scHz = scWanted;
        scCoeffs = EQEngine::computeCoefficients ({ true, EQType::LowCut, scHz, 0.0f, 0.7071f }, fs);
    }

    const auto style = live.style;
    const bool vocal = style == CompStyle::Vocal;
    const bool peakDetect = style == CompStyle::Punch;
    const bool rmsDetect = style == CompStyle::Smooth || style == CompStyle::Glue || vocal;
    const float attMs = juce::jmax (0.05f, live.attackMs) * (style == CompStyle::Punch ? 0.6f : 1.0f);
    const float attCoef = std::exp (-1.0f / (float) (fs * 0.001 * attMs));
    const float baseRel = juce::jmax (1.0f, live.releaseMs);
    const float rmsMs = style == CompStyle::Glue ? 30.0f : (style == CompStyle::Smooth || vocal) ? 15.0f : 5.0f;
    const float rmsCoef = std::exp (-1.0f / (float) (fs * 0.001 * rmsMs));
    const float slowCoef = std::exp (-1.0f / (float) (fs * 0.6));
    const float dropCoef = 1.0f - std::exp (-1.0f / (float) (fs * 0.0015));
    const float makeup = live.autoMakeup ? autoMakeupDb (live.thresholdDb, live.ratio, live.kneeDb) : live.makeupDb;
    const float knee = (style == CompStyle::Smooth || vocal) ? juce::jmax (live.kneeDb, 8.0f) : live.kneeDb;
    makeupSmooth.setTargetValue (juce::Decibels::decibelsToGain (makeup));
    mixSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, live.mix));

    // Programme-dependent release: the longer it has been compressing, the slower it lets go, so single peaks
    // recover fast while sustained loud passages stay smooth. Punch keeps its fixed, snappy release.
    const bool adaptiveRelease = style != CompStyle::Punch;
    const float fixedRelCoef = std::exp (-1.0f / (float) (fs * 0.001 * baseRel * (style == CompStyle::Punch ? 0.7f : 1.0f)));
    if (adaptiveRelCoef <= 0.0f) adaptiveRelCoef = fixedRelCoef;

    // Vocal style, second stage: catches what is still more than 10 dB above the threshold after levelling
    const float catchThr = live.thresholdDb + live.catchOverDb;
    const float catchAtt = std::exp (-1.0f / (float) (fs * 0.0003));
    const float catchRel = std::exp (-1.0f / (float) (fs * 0.001 * juce::jmax (1.0f, live.catchReleaseMs)));

    float minGain = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f, sq = 0.0f, rawPeak = 0.0f;
        for (int c = 0; c < ch; ++c)
        {
            float v = buffer.getSample (c, i);
            rawPeak = juce::jmax (rawPeak, std::abs (v));
            if (useSc) v = scFilters[(size_t) c].process (v, scCoeffs);
            peak = juce::jmax (peak, std::abs (v));
            sq += v * v;
        }
        // peak reading that stays put for one cycle of the lowest notes, then falls quickly to the sound
        if (peak >= held) { held = peak; holdLeft = holdSamples; }
        else if (holdLeft > 0) --holdLeft;
        else held += (peak - held) * dropCoef;
        rmsState = rmsCoef * rmsState + (1.0f - rmsCoef) * (sq / (float) ch);
        const float rms = std::sqrt (rmsState) * 1.4142f;
        const float det = peakDetect ? held : rmsDetect ? rms : juce::jmax (held, rms);

        if (adaptiveRelease && (i & 15) == 0)
        {
            const float deep = juce::jlimit (0.0f, 1.0f, -slowGr / (style == CompStyle::Glue ? 4.0f : 6.0f));
            const float relMs = style == CompStyle::Glue  ? baseRel * (1.0f + 3.0f * deep)
                              : style == CompStyle::Clean ? baseRel * (0.8f + 1.7f * deep)
                                                          : baseRel * (0.5f + (vocal ? live.levelRelease : 3.5f) * deep);
            adaptiveRelCoef = std::exp (-1.0f / (float) (fs * 0.001 * relMs));
        }
        const float relCoef = adaptiveRelease ? adaptiveRelCoef : fixedRelCoef;

        // static curve on the level in dB, then attack and release on the gain itself
        const float levelDb = juce::Decibels::gainToDecibels (det, -100.0f);
        const float g = computeGainDb (levelDb, live.thresholdDb, live.ratio, knee);
        levelGain = g < levelGain ? attCoef * levelGain + (1.0f - attCoef) * g
                                  : relCoef * levelGain + (1.0f - relCoef) * g;
        if (vocal)
        {
            // The catcher hears the voice as the leveller leaves it, and hears all of it: the leveller ignores
            // the lows so a plosive does not duck the whole word, but a peak is a peak whatever it is made of.
            const float after = rawPeak * juce::Decibels::decibelsToGain (levelGain);
            if (after >= catchHeld) { catchHeld = after; catchHoldLeft = holdSamples; }
            else if (catchHoldLeft > 0) --catchHoldLeft;
            else catchHeld += (after - catchHeld) * dropCoef;
            const float gc = juce::jmax (-live.catchMaxDb, computeGainDb (juce::Decibels::gainToDecibels (catchHeld, -100.0f), catchThr, 6.0f, 3.0f));
            catchGain = gc < catchGain ? catchAtt * catchGain + (1.0f - catchAtt) * gc
                                       : catchRel * catchGain + (1.0f - catchRel) * gc;
        }
        else catchGain = 0.0f;
        gainSmooth = levelGain + catchGain;
        slowGr = slowCoef * slowGr + (1.0f - slowCoef) * levelGain;
        minGain = juce::jmin (minGain, gainSmooth);
        const float lin = juce::Decibels::decibelsToGain (gainSmooth) * makeupSmooth.getNextValue();
        const float mix = mixSmooth.getNextValue();
        for (int c = 0; c < ch; ++c)
            buffer.setSample (c, i, buffer.getSample (c, i) * lin * mix + dry.getSample (c, i) * (1.0f - mix));
    }
    grMeter.store (minGain, std::memory_order_relaxed);
    float outPeak = 0.0f;
    for (int c = 0; c < ch; ++c) outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, 0, n));
    outMeter.store (juce::Decibels::gainToDecibels (outPeak, -100.0f), std::memory_order_relaxed);
    pushHistory (inPeak, minGain, n);
}
}
