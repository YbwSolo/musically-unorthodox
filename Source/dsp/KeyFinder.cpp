#include "KeyFinder.h"

namespace mu
{
KeyEstimate keyFromNotes (const float* h) noexcept
{
    // how strongly each note of the scale belongs to a major or a minor key
    static const float major[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    KeyEstimate guess;
    float total = 0.0f;
    for (int i = 0; i < 12; ++i) total += h[i];
    if (total <= 0.0f) return guess;
    auto correlate = [h] (const float* profile, int key)
    {
        float mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += h[i]; mp += profile[i]; }
        mh /= 12.0f; mp /= 12.0f;
        float num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float a = h[(i + key) % 12] - mh, b = profile[i] - mp;
            num += a * b; dh += a * a; dp += b * b;
        }
        return dh > 0.0f ? num / std::sqrt (dh * dp) : 0.0f;
    };
    float best = -2.0f;
    for (int key = 0; key < 12; ++key)
        for (int m = 0; m < 2; ++m)
        {
            const float c = correlate (m == 0 ? major : minor, key);
            if (c > best) { best = c; guess.key = key; guess.minor = m == 1; }
        }
    guess.confidence = juce::jlimit (0.0f, 1.0f, best);
    return guess;
}

bool keyFromName (const juce::String& name, int& key, bool& minor)
{
    // split on anything that is not a letter, a digit or a sharp sign
    juce::StringArray tokens;
    juce::String current;
    for (auto c : name)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '#') current << juce::String::charToString (c);
        else { if (current.isNotEmpty()) tokens.add (current); current.clear(); }
    }
    if (current.isNotEmpty()) tokens.add (current);

    auto noteOf = [] (const juce::String& t, int& semitone, int& used)
    {
        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };                // A B C D E F G
        if (t.isEmpty()) return false;
        const auto c = t[0];
        if (c < 'A' || c > 'G') return false;                               // note names are written with a capital
        semitone = base[c - 'A']; used = 1;
        if (t.length() > 1 && t[1] == '#') { semitone = (semitone + 1) % 12; used = 2; }
        else if (t.length() > 1 && t[1] == 'b' && (t.length() == 2 || ! juce::CharacterFunctions::isLetter (t[2]) || t.substring (2).equalsIgnoreCase ("m")
                                                    || t.substring (2).startsWithIgnoreCase ("mi") || t.substring (2).startsWithIgnoreCase ("ma")))
        { semitone = (semitone + 11) % 12; used = 2; }
        return true;
    };
    auto quality = [] (const juce::String& q, bool& isMinor)
    {
        const auto l = q.toLowerCase();
        if (q == "m" || l == "min" || l == "minor") { isMinor = true; return true; }      // "Gm" counts, "FM" (a synth, a radio) does not
        if (l == "maj" || l == "major") { isMinor = false; return true; }
        return false;
    };
    for (int i = 0; i < tokens.size(); ++i)
    {
        int semitone = 0, used = 0;
        if (! noteOf (tokens[i], semitone, used)) continue;
        const auto rest = tokens[i].substring (used);
        bool isMinor = false;
        if (rest.isNotEmpty()) { if (quality (rest, isMinor)) { key = semitone; minor = isMinor; return true; } }
        else if (i + 1 < tokens.size() && tokens[i + 1].length() >= 3 && quality (tokens[i + 1], isMinor)) { key = semitone; minor = isMinor; return true; }
    }
    return false;
}

void KeyFinder::prepare (double sampleRate)
{
    fs = sampleRate;
    decim = juce::jmax (1, (int) std::round (fs / 11025.0));
    rate = (float) (fs / (double) decim);
    lpA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2400.0f / (float) fs);
    fft = std::make_unique<juce::dsp::FFT> (kOrder);
    ring.assign ((size_t) kSize, 0.0f);
    work.assign ((size_t) kSize * 2, 0.0f);
    window.resize ((size_t) kSize);
    for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1));
    reset();
}

void KeyFinder::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    phase = 0; fill = 0; lp1 = lp2 = 0.0f; loudest = 0.0f;
    for (auto& c : chroma) c.store (0.0f);
    frames.store (0.0f);
}

void KeyFinder::push (const float* left, const float* right, int n) noexcept
{
    if (fft == nullptr) return;
    for (int i = 0; i < n; ++i)
    {
        const float x = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
        lp1 += lpA * (x - lp1);
        lp2 += lpA * (lp1 - lp2);
        if (++phase < decim) continue;
        phase = 0;
        ring[(size_t) fill] = lp2;
        if (++fill == kSize)
        {
            analyse();
            // keep the newer half, so each stretch of sound is looked at twice
            std::copy (ring.begin() + kSize / 2, ring.end(), ring.begin());
            fill = kSize / 2;
        }
    }
}

void KeyFinder::analyse() noexcept
{
    double energy = 0.0;
    for (int i = 0; i < kSize; ++i) { work[(size_t) i] = ring[(size_t) i] * window[(size_t) i]; energy += (double) ring[(size_t) i] * ring[(size_t) i]; }
    std::fill (work.begin() + kSize, work.end(), 0.0f);
    const float level = (float) std::sqrt (energy / (double) kSize);
    loudest = juce::jmax (level, loudest * 0.995f);
    if (level < 1.0e-4f || level < loudest * 0.05f) return;                 // silence and the quiet gaps between sounds say nothing about the key

    fft->performFrequencyOnlyForwardTransform (work.data(), true);
    const float binHz = rate / (float) kSize;
    const int lo = juce::jmax (2, (int) (100.0f / binHz)), hi = juce::jmin (kSize / 2 - 2, (int) (2000.0f / binHz));
    float frame[12] = {};
    for (int b = lo; b <= hi; ++b)
    {
        const float m = work[(size_t) b];
        if (m <= work[(size_t) b - 1] || m < work[(size_t) b + 1]) continue;      // only the peaks: each one is a note or one of its overtones
        // the true frequency lies between bins; a parabola through the peak finds it
        const float a = work[(size_t) b - 1], c = work[(size_t) b + 1];
        const float denom = a - 2.0f * m + c;
        const float shift = std::abs (denom) > 1.0e-12f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denom) : 0.0f;
        const float hz = ((float) b + shift) * binHz;
        const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
        const float nearest = std::round (note);
        const float off = std::abs (note - nearest);
        if (off > 0.35f) continue;                                              // between two notes: noise, or an instrument that is not in tune
        const int pc = (((int) nearest % 12) + 12) % 12;
        frame[pc] += std::sqrt (m) * (1.0f - off);                              // square root so one loud note does not drown the rest
    }
    float peak = 0.0f;
    for (float v : frame) peak = juce::jmax (peak, v);
    if (peak <= 0.0f) return;
    for (int i = 0; i < 12; ++i) chroma[(size_t) i].store (chroma[(size_t) i].load (std::memory_order_relaxed) + frame[i] / peak, std::memory_order_relaxed);
    frames.store (frames.load (std::memory_order_relaxed) + 1.0f, std::memory_order_relaxed);
}

KeyEstimate KeyFinder::estimate() const noexcept
{
    float h[12];
    for (int i = 0; i < 12; ++i) h[i] = chroma[(size_t) i].load (std::memory_order_relaxed);
    auto e = keyFromNotes (h);
    e.secondsHeard = frames.load (std::memory_order_relaxed) * (float) (kSize / 2) / rate;
    return e;
}
}
