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
    ringPos = 0; phase = 0; since = 0; freq = 0.0f; clear = 0.0f; lastGood = 0.0f; pending = 0.0f; heldFrames = 0; rawFreq = 0.0f; rawClear = 0.0f;
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
    rawFreq = 0.0f; rawClear = 0.0f;
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
    // The first dip is the period of a clean voice. In a rough or creaky one every second pulse is weaker, so the
    // wave really repeats every two pulses: the first dip is there but shallow, and the one at twice the delay is
    // much deeper. Taking the first then reads the note an octave too high for a few hundredths of a second, in
    // the middle of a word (on a real raspy rap vocal, 25 times a minute). So when the first dip is not a
    // convincing one, the dips at two and three times its delay are looked at: one that is far deeper is the real
    // period, and one that is somewhat deeper is taken if it is the note that was already being sung.
    if (best > 0 && diff[(size_t) best] > 0.05f)
    {
        const float carried = lastGood > 0.0f ? rate / lastGood : 0.0f;
        const int first = best;
        for (int k = 2; k <= 3; ++k)
        {
            const int from = juce::jmax (tauMin, (int) ((float) (first * k) * 0.94f)), to = juce::jmin (tauMax - 1, (int) ((float) (first * k) * 1.06f) + 1);
            if (from >= to) break;
            int m = from;
            for (int tau = from; tau <= to; ++tau) if (diff[(size_t) tau] < diff[(size_t) m]) m = tau;
            if (m <= from || m >= to) continue;                                  // the bottom of a slope, not a dip
            const bool farDeeper = diff[(size_t) m] < 0.5f * diff[(size_t) best];
            const bool somewhatDeeper = diff[(size_t) m] < 0.8f * diff[(size_t) best];
            const bool theNoteBefore = carried > 0.0f && std::abs ((float) m - carried) < carried * 0.06f;
            if (farDeeper || (somewhatDeeper && theNoteBefore)) best = m;
        }
    }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin; tau < tauMax; ++tau) if (diff[(size_t) tau] < diff[(size_t) best]) best = tau;
        // A rough or breathy stretch in the middle of a note matches itself less well. If the best match is the
        // same note that was just being sung, it is given more benefit of the doubt, so the note is not dropped.
        float limit = 0.30f;
        if (lastGood > 0.0f && std::abs ((float) best - rate / lastGood) < rate / lastGood * 0.06f) limit = 0.45f;
        if (diff[(size_t) best] > limit) { freq = 0.0f; clear = juce::jmax (0.0f, 1.0f - diff[(size_t) best]); rawClear = clear; lastGood = 0.0f; pending = 0.0f; heldFrames = 0; return; }
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
    rawFreq = f; rawClear = juce::jlimit (0.0f, 1.0f, 1.0f - diff[(size_t) best]);
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
    pulse.assign ((size_t) size, 0.0f);
    lpCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 1000.0f / (float) sampleRate);
    hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 50.0f / (float) sampleRate);
    minPeriod = (float) (sampleRate / 1100.0);
    maxPeriod = (float) (sampleRate / 60.0);
    reset();
}

void GrainEngine::reset()
{
    for (auto& r : ring) std::fill (r.begin(), r.end(), 0.0f);
    std::fill (pulse.begin(), pulse.end(), 0.0f);
    lp1 = lp2 = hp = 0.0f; polarity = 1.0f; polarityVotes = 0; sincePolarity = 0;
    now = 0;
}

void GrainEngine::advance() noexcept
{
    // keep a low-passed copy of what just arrived; pulses are found on it
    float m = ring[0][(size_t) (now & mask)];
    if (channels > 1) m = 0.5f * (m + ring[1][(size_t) (now & mask)]);
    lp1 += lpCoeff * (m - lp1);
    lp2 += lpCoeff * (lp1 - lp2);
    hp = hpCoeff * hp + (1.0f - hpCoeff) * lp2;
    pulse[(size_t) (now & mask)] = lp2 - hp;
    ++now;
    ++sincePolarity;
}

// The strongest pulse within "reach" samples of a position. preferNear (0..1) leans towards the position itself,
// so that of two similar peaks the expected one wins and the choice does not hop between them.
double GrainEngine::findPulse (double around, float reach, float preferNear, bool* found) const noexcept
{
    if (found != nullptr) *found = false;
    const juce::int64 newest = now - 1, oldest = now - (mask + 1) + 8;
    const auto lo = juce::jlimit (oldest, newest, (juce::int64) std::floor (around - (double) reach));
    const auto hi = juce::jlimit (oldest, newest, (juce::int64) std::ceil (around + (double) reach));
    juce::int64 best = juce::jlimit (lo, hi, (juce::int64) std::llround (around));
    float bestScore = -1.0e30f;
    for (auto i = lo; i <= hi; ++i)
    {
        const float v = polarity * pulse[(size_t) (i & mask)];
        const float score = v > 0.0f ? v * (1.0f - preferNear * (float) std::abs ((double) i - around) / reach) : v;
        if (score > bestScore) { bestScore = score; best = i; }
    }
    // A best score at the very edge of the search, or one that is not a peak at all, means there is no pulse here
    // (the note has ended, or this is a consonant). Then the expected position is the answer: taking the edge
    // would move the slice a third of a period for no reason, which is a small click at the end of every word.
    if (best <= lo || best >= hi || polarity * pulse[(size_t) (best & mask)] <= 0.0f) return around;
    if (found != nullptr) *found = true;
    {
        const float a = pulse[(size_t) ((best - 1) & mask)], b = pulse[(size_t) (best & mask)], c = pulse[(size_t) ((best + 1) & mask)];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) > 1.0e-12f) return (double) best + (double) juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denom);
    }
    return (double) best;
}

// Pulses point one way or the other depending on the microphone and the wiring. Whichever side has the taller
// peaks is the side to follow; it is only changed after several checks agree, so it cannot flip mid-note.
void GrainEngine::checkPolarity (double around, float period) noexcept
{
    if (sincePolarity < 960) return;
    sincePolarity = 0;
    const juce::int64 newest = now - 1, oldest = now - (mask + 1) + 8;
    const auto lo = juce::jlimit (oldest, newest, (juce::int64) (around - 1.5 * (double) period));
    const auto hi = juce::jlimit (oldest, newest, (juce::int64) (around + 1.5 * (double) period));
    float up = 0.0f, down = 0.0f;
    for (auto i = lo; i <= hi; ++i) { const float v = pulse[(size_t) (i & mask)]; up = juce::jmax (up, v); down = juce::jmax (down, -v); }
    const float other = polarity > 0.0f ? down : up, mine = polarity > 0.0f ? up : down;
    if (other > mine * 1.35f) { if (++polarityVotes >= 4) { polarity = -polarity; polarityVotes = 0; } }
    else polarityVotes = 0;
}

void GrainEngine::renderSync (Voice& v, float period, bool voiced, float ratio, float delaySamples, float* out, float formant) noexcept
{
    const double t = (double) now;
    period = juce::jlimit (minPeriod, maxPeriod, period);
    ratio = juce::jlimit (0.25f, 4.0f, ratio);
    formant = voiced ? juce::jlimit (0.5f, 2.0f, formant) : 1.0f;
    // With nothing to change, and wherever there is no note to change, the sound is simply copied.
    const bool copying = ! voiced || (std::abs (ratio - 1.0f) < 0.0004f && std::abs (formant - 1.0f) < 1.0e-6f);
    // A moved note is laid as slices one period wide, whatever their spacing: that is what keeps the vowel the
    // same, and the gain puts the level right. A copy is cut exactly as wide as the gap to its neighbours, so the
    // slices add up to the sound itself.
    const float ownWidth = juce::jmin (maxPeriod, period / formant);

    for (;;)
    {
        // Each slice rises over exactly the stretch the one before it falls over, so the two always add up to the
        // whole, however suddenly the pitch changes. (With one width for both sides, a pitch that dropped left a
        // gap between slices and one that jumped up piled them on top of each other: a dip or a bump of several
        // dB for a few milliseconds.) A moved slice knows its own width when it is laid. A copied one does not:
        // its falling side is left open, and is set by wherever the next slice turns out to go.
        const bool fallKnown = v.syncFall > 0.0f;
        double centre = v.syncCentre + (copying ? (double) (fallKnown ? v.syncFall : period) : (double) (period / ratio));
        if (t < (fallKnown ? centre - (double) v.syncFall : v.syncCentre)) break;

        // how far from the clock's own position the last slice took its sound: it is carried from slice to slice
        const double carried = v.syncOffset;
        double src = centre - (double) delaySamples + carried;
        bool locked = false;
        if (! voiced)
        {
            // Between notes the offset creeps back to nothing, a sample at a time, so it cannot build up.
            const double less = carried - juce::jlimit (-1.0, 1.0, carried);
            src = centre - (double) delaySamples + juce::jmin (less, (double) delaySamples - 2.0);
        }
        else if (! copying)
        {
            const double p = (double) period;
            checkPolarity (centre - (double) delaySamples, period);
            if (! v.locked && ! fallKnown)
            {
                // The first moved slice after a stretch of copying. It has to sit on one of the voice's pulses, and
                // the pulse nearest to where the copy was reading is up to half a period away. Taking the sound
                // from there would lay the same wave twice, half a period apart, over the end of the copy: for a
                // few milliseconds the two cancel or pile up (a dip or a bump in level, a hollow sound). So the
                // slice is moved instead of the sound: it is laid a little early or late, exactly where that pulse
                // falls in the copy, and the join cannot be heard because nothing has changed yet.
                double pulseAt = findPulse (src, period * 0.5f, 0.15f);
                if (centre + (pulseAt - src) - v.syncCentre < 0.4 * p) pulseAt = findPulse (pulseAt + p, period * 0.3f, 0.5f);
                centre += pulseAt - src;
                src = pulseAt;
            }
            else if (! v.locked) src = findPulse (centre - (double) delaySamples, period * 0.55f, 0.15f);
            else
            {
                const double steps = std::round ((centre - (double) delaySamples - v.prevSrc) / p);
                bool found = true;
                src = steps <= 0.0 ? v.prevSrc                                                      // the same pulse again (pitching up needs repeats)
                                   : findPulse (v.prevSrc + steps * p, period * 0.3f, 0.5f, &found);   // the next pulse, about one period on
                // When the voice goes to another note its pulses are no longer where the old spacing says. Counting
                // on from the last pulse then lands between two pulses of the new note, and stays between them:
                // every slice cuts a pulse in half and the note sounds hoarse until the two happen to line up.
                // So after a change of note, or when there is no pulse where one was expected, the nearest pulse
                // to where the slice should be reading is looked for afresh.
                const bool newNote = std::abs (period - v.syncPeriod) > 0.12f * period;
                if (newNote || ! found)
                {
                    bool again = false;
                    const double fresh = findPulse (centre - (double) delaySamples, period * 0.55f, 0.15f, &again);
                    if (again) src = fresh;
                }
            }
            locked = true;
        }
        // (An open slice is laid the moment the one before reaches its middle, so the rise starts there, give or
        // take a fraction of a sample; only a slice laid badly late is cut short so that it still starts from nothing.)
        double rise = fallKnown ? (double) v.syncFall : juce::jmax (8.0, centre - v.syncCentre);
        if (! fallKnown && centre - t < 0.5 * rise) rise = juce::jmax (8.0, centre - t);
        if (voiced && ! copying)
        {
            // a slice can only use sound that has already arrived; one read faster or slower reaches further
            const double limit = centre - 2.0 - rise * std::abs ((double) formant - 1.0);
            for (int guard = 0; src > limit && guard < 8; ++guard) src = findPulse (src - (double) period, period * 0.3f, 0.5f);
        }
        if (! fallKnown && v.count > 0 && v.syncOpen) v.grains[(size_t) v.count - 1].halfOut = (float) rise;

        Voice::Grain g;
        g.centre = centre; g.src = src; g.half = (float) rise; g.rate = formant;
        g.halfOut = copying ? 0.0f : ownWidth;
        g.gain = copying ? 1.0f : juce::jmin (2.0f, formant / ratio);
        v.prevSrc = src;
        v.syncOffset = src - (centre - (double) delaySamples);
        v.syncCentre = centre;
        v.syncFall = g.halfOut;
        v.syncPeriod = period;
        v.syncOpen = copying;
        v.locked = locked;
        if (v.count == kMaxGrains) { for (int i = 1; i < kMaxGrains; ++i) v.grains[(size_t) i - 1] = v.grains[(size_t) i]; --v.count; }
        v.grains[(size_t) v.count++] = g;
    }

    int kept = 0;
    for (int i = 0; i < v.count; ++i)
    {
        const auto g = v.grains[(size_t) i];
        const double offset = t - g.centre;
        const float x = (float) (offset / (double) (offset > 0.0 && g.halfOut > 0.0f ? g.halfOut : g.half));
        if (x >= 1.0f) continue;
        v.grains[(size_t) kept++] = g;
        if (x <= -1.0f) continue;
        const float w = (0.5f + 0.5f * std::cos (kPi * x)) * g.gain;
        for (int c = 0; c < channels; ++c) out[c] += w * read (c, g.src + offset * (double) g.rate);
    }
    v.count = kept;
}

void GrainEngine::resetVoice (Voice& v, float delaySamples) const noexcept
{
    v.count = 0;
    v.locked = false;
    v.nextHalf = juce::jlimit (minPeriod, maxPeriod, maxPeriod * 0.3f);
    v.nextCentre = (double) now + v.nextHalf;
    v.prevSrc = v.nextCentre - delaySamples;
    v.syncCentre = (double) now; v.syncOffset = 0.0; v.syncFall = 0.0f; v.syncOpen = false;
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
