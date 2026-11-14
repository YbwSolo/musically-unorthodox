#include "Pitch.h"

namespace mu
{
namespace
{
const float kPi = juce::MathConstants<float>::pi;
}

//==================================================================== PitchDetector
void PitchDetector::prepare (double sampleRate)
{
    fs = sampleRate;
    decim = juce::jmax (1, (int) std::round (fs / 24000.0));
    rate = (float) (fs / (double) decim);
    window = (int) std::round (rate * 0.030f);
    tauMin = juce::jmax (4, (int) std::floor (rate / 1000.0f));
    tauMax = (int) std::ceil (rate / 65.0f);
    hop = juce::jmax (1, (int) std::round (rate * 0.010f));
    total = window + tauMax + 2;
    ring.assign ((size_t) total, 0.0f);
    frame.assign ((size_t) total, 0.0f);
    diff.assign ((size_t) tauMax + 2, 0.0f);

    // 4th-order Butterworth low-pass at 1.8 kHz: keeps the fundamental and its first harmonics, removes what
    // would fold back when every other sample is dropped
    auto design = [this] (Biquad& b, float q)
    {
        const float w = 2.0f * kPi * 1800.0f / (float) fs, c = std::cos (w), alpha = std::sin (w) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b.b0 = (1.0f - c) * 0.5f / a0; b.b1 = (1.0f - c) / a0; b.b2 = b.b0;
        b.a1 = -2.0f * c / a0; b.a2 = (1.0f - alpha) / a0;
    };
    design (lp1, 0.5412f);
    design (lp2, 1.3066f);
    reset();
}

void PitchDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    lp1.z1 = lp1.z2 = lp2.z1 = lp2.z2 = 0.0f;
    ringPos = 0; phase = 0; since = 0; freq = 0.0f; clear = 0.0f; lastGood = 0.0f; pending = 0.0f; heldFrames = 0;
}

bool PitchDetector::push (float x) noexcept
{
    const float y = lp2.run (lp1.run (x));
    if (++phase < decim) return false;
    phase = 0;
    ring[(size_t) ringPos] = y;
    ringPos = ringPos + 1 == total ? 0 : ringPos + 1;
    if (++since < hop) return false;
    since = 0;
    analyse();
    return true;
}

void PitchDetector::analyse() noexcept
{
    // oldest to newest, with the average removed
    double mean = 0.0;
    for (int i = 0; i < total; ++i) { frame[(size_t) i] = ring[(size_t) ((ringPos + i) % total)]; mean += frame[(size_t) i]; }
    mean /= (double) total;
    const float* x = frame.data();
    const int a = total - window;                // the newest stretch of sound
    double energy = 0.0;
    for (int i = 0; i < total; ++i) frame[(size_t) i] -= (float) mean;
    for (int j = 0; j < window; ++j) energy += (double) x[a + j] * x[a + j];
    if (energy / (double) window < 1.0e-8) { freq = 0.0f; clear = 0.0f; lastGood = 0.0f; pending = 0.0f; heldFrames = 0; return; }       // below -80 dB: silence

    // how different the newest stretch is from the same stretch tau samples earlier
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        const float* p = x + a;
        const float* q = x + a - tau;
        float sum = 0.0f;
        for (int j = 0; j < window; ++j) { const float d = p[j] - q[j]; sum += d * d; }
        diff[(size_t) tau] = sum;
    }
    // normalise by the running average, so the first real dip stands out (cumulative mean normalised difference)
    double running = 0.0;
    diff[0] = 1.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * (double) tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (diff[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin; tau < tauMax; ++tau) if (diff[(size_t) tau] < diff[(size_t) best]) best = tau;
        // A rough or breathy stretch in the middle of a note matches itself less well. If the best match is the
        // same note that was just being sung, it is given more benefit of the doubt, so the note is not dropped.
        float limit = 0.30f;
        if (lastGood > 0.0f && std::abs ((float) best - rate / lastGood) < rate / lastGood * 0.06f) limit = 0.45f;
        if (diff[(size_t) best] > limit) { freq = 0.0f; clear = juce::jmax (0.0f, 1.0f - diff[(size_t) best]); lastGood = 0.0f; pending = 0.0f; heldFrames = 0; return; }
    }
    // a parabola through the dip and its neighbours finds the delay between two samples
    float tau = (float) best;
    if (best > 1 && best < tauMax)
    {
        const float y0 = diff[(size_t) best - 1], y1 = diff[(size_t) best], y2 = diff[(size_t) best + 1];
        const float denom = y0 - 2.0f * y1 + y2;
        if (std::abs (denom) > 1.0e-9f) tau += juce::jlimit (-0.5f, 0.5f, 0.5f * (y0 - y2) / denom);
    }
    float f = rate / tau;
    // A leap of more than four semitones is far more often a mistake (a consonant, a breath, the wrong octave)
    // than a real jump. It is only believed once two answers in a row agree on it; until then the last good
    // note is held, and if no agreement comes within 40 ms the sound is treated as having no pitch.
    if (lastGood > 0.0f && (f > lastGood * 1.26f || f < lastGood / 1.26f))
    {
        const bool confirmed = pending > 0.0f && f < pending * 1.06f && f > pending / 1.06f;
        if (! confirmed)
        {
            pending = f;
            if (++heldFrames > 4) { freq = 0.0f; clear = 0.0f; lastGood = 0.0f; pending = 0.0f; heldFrames = 0; return; }
            freq = lastGood;
            clear = juce::jlimit (0.0f, 1.0f, 1.0f - diff[(size_t) best]);
            return;
        }
    }
    pending = 0.0f; heldFrames = 0;
    freq = f; lastGood = f;
    clear = juce::jlimit (0.0f, 1.0f, 1.0f - diff[(size_t) best]);
}

//==================================================================== GrainEngine
void GrainEngine::prepare (int numChannels, double sampleRate)
{
    channels = juce::jlimit (1, 2, numChannels);
    int size = 1 << 14;
    while ((double) size < sampleRate * 0.35) size <<= 1;           // room for the longest delay plus two long periods
    mask = size - 1;
    for (auto& r : ring) r.assign ((size_t) size, 0.0f);
    minPeriod = (float) (sampleRate / 1100.0);
    maxPeriod = (float) (sampleRate / 60.0);
    reset();
}

void GrainEngine::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    now = 0;
}

void GrainEngine::resetVoice (Voice& v, float delaySamples) const noexcept
{
    v.count = 0;
    v.nextHalf = juce::jlimit (minPeriod, maxPeriod, maxPeriod * 0.3f);
    v.nextCentre = (double) now + v.nextHalf;
    v.prevSrc = v.nextCentre - delaySamples;
}

float GrainEngine::read (int channel, double position) const noexcept
{
    // 4-point Hermite interpolation between samples; never reads past what has been written
    const double floorPos = std::floor (position);
    const float f = (float) (position - floorPos);
    const auto i1 = (juce::int64) floorPos;
    const auto& r = ring[(size_t) channel];
    auto at = [&] (juce::int64 i) { return r[(size_t) (juce::jmin (i, now) & mask)]; };
    const float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void GrainEngine::render (Voice& v, float period, bool voiced, float ratio, float delaySamples, float* out, float formant) noexcept
{
    const double t = (double) now;
    period = juce::jlimit (minPeriod, maxPeriod, period);
    ratio = juce::jlimit (0.5f, 2.0f, ratio);
    formant = voiced ? juce::jlimit (0.5f, 2.0f, formant) : 1.0f;

    while (t >= v.nextCentre - (double) v.nextHalf)
    {
        Voice::Grain g;
        g.centre = v.nextCentre; g.half = v.nextHalf; g.rate = formant;
        const double ideal = g.centre - (double) delaySamples;
        double src = ideal;
        if (voiced)
        {
            // step a whole number of periods on from the last slice, so neighbouring slices line up wave for wave
            src = v.prevSrc + std::round ((ideal - v.prevSrc) / (double) period) * (double) period;
            if (std::abs (src - ideal) > (double) period) src = ideal;
            // a slice can only use sound that has already arrived; one that is read faster reaches further ahead
            const double reach = (double) g.half * juce::jmax (0.0, (double) formant - 1.0);
            while (src > g.centre - 2.0 - reach) src -= (double) period;
        }
        g.src = src;
        // closer slices would add up louder, and so would the longer slices of a lowered formant
        g.gain = voiced ? juce::jmin (2.0f, formant / ratio) : 1.0f;
        v.prevSrc = src;
        if (v.count == kMaxGrains) { for (int i = 1; i < kMaxGrains; ++i) v.grains[(size_t) i - 1] = v.grains[(size_t) i]; --v.count; }
        v.grains[(size_t) v.count++] = g;
        v.nextCentre += voiced ? (double) (period / ratio) : (double) period;
        v.nextHalf = juce::jmin (maxPeriod, period / formant);          // two periods of the sound, however fast it is read
    }

    int kept = 0;
    for (int i = 0; i < v.count; ++i)
    {
        const auto g = v.grains[(size_t) i];
        const double offset = t - g.centre;
        const float x = (float) (offset / (double) g.half);
        if (x >= 1.0f) continue;                                           // finished
        v.grains[(size_t) kept++] = g;
        if (x <= -1.0f) continue;
        const float w = (0.5f + 0.5f * std::cos (kPi * x)) * g.gain;
        for (int c = 0; c < channels; ++c) out[c] += w * read (c, g.src + offset * (double) g.rate);
    }
    v.count = kept;
}
}
