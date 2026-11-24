#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mu
{
struct TempoReading
{
    float bpm = 0.0f;            // 0 until enough has been heard
    float other = 0.0f;          // the same pulse counted at half or double speed, when that is between 60 and 180
    float rival = 0.0f;          // a different tempo that fits nearly as well, or 0: then the answer is not safe
    float confidence = 0.0f;     // 0..1: how far the chosen pulse stands above every tempo that is not its half or double
    float seconds = 0.0f;        // how much audio it is based on
    bool heard = false;          // something loud enough to measure is playing
};

// Works out the tempo of whatever is playing through the track.
// 200 times a second it measures the spectrum in 24 bands and adds up how much each band has risen, in decibels:
// that is the onset strength, high wherever a drum or a note starts. To find the tempo it takes the last 20 seconds
// of onset strength and asks, for every tempo from 60 to 180 BPM in tenths, how well the onsets repeat on that
// tempo's whole sixteenth-note grid, counting beats, bars and pairs of bars more than single steps. Scoring the
// grid and not just the beat is what stops a three-three-two hi-hat pattern reading a third too fast.
// The same pulse can always be counted at half or double speed, so that reading is given too.
class TempoDetector
{
public:
    static constexpr int kRate = 200, kFrames = kRate * 20, kOrder = 10, kSize = 1 << kOrder, kBands = 24;

    void prepare (double sampleRate)
    {
        decim = juce::jmax (1, (int) std::round (sampleRate / 48000.0));
        fs = sampleRate / (double) decim;
        hop = juce::jmax (1, (int) std::round (fs / (double) kRate));
        ring.assign ((size_t) kSize, 0.0f); work.assign ((size_t) kSize * 2, 0.0f); window.resize ((size_t) kSize);
        for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kSize - 1));
        // band edges, evenly spaced in pitch from 60 Hz to 16 kHz
        for (int b = 0; b <= kBands; ++b)
            edge[(size_t) b] = juce::jlimit (1, kSize / 2 - 1, (int) std::round (60.0 * std::pow (16000.0 / 60.0, (double) b / (double) kBands) / fs * (double) kSize));
        for (auto& p : prev) p = 0.0f;
        for (auto& f : flux) f.store (0.0f);
        pos = 0; count = 0; phase = 0; acc = 0.0f; floorPower = 0.0f; primed = false;
        write.store (0); filled.store (0); level.store (0.0f);
    }
    // Listening costs a small spectrum 200 times a second, so it runs only while a lesson asks for it.
    void setListening (bool on) { if (on && ! listening.load()) restart.store (true); listening.store (on); }
    bool isListening() const { return listening.load (std::memory_order_relaxed); }
    void startOver() { restart.store (true); }

    void push (const float* left, const float* right, int n) noexcept          // audio thread
    {
        if (! listening.load (std::memory_order_relaxed) || ring.empty()) return;
        if (restart.exchange (false)) { filled.store (0); primed = false; }
        for (int i = 0; i < n; ++i)
        {
            acc += right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            if (++phase < decim) continue;
            ring[(size_t) pos] = acc / (float) decim; acc = 0.0f; phase = 0;
            pos = (pos + 1) & (kSize - 1);
            if (++count >= hop) { count = 0; frame(); }
        }
    }

    TempoReading read() const                                                   // message thread: two or three milliseconds
    {
        TempoReading out;
        const int have = filled.load();
        out.seconds = (float) have / (float) kRate;
        out.heard = level.load() > 1.0e-7f;
        if (have < kRate * 8) return out;                                       // eight seconds first: a bar has to be seen to repeat
        std::vector<float> x ((size_t) have);
        const int w = write.load();
        float mean = 0.0f;
        for (int i = 0; i < have; ++i) { x[(size_t) i] = flux[(size_t) ((w - have + i + kFrames * 2) % kFrames)].load (std::memory_order_relaxed); mean += x[(size_t) i]; }
        mean /= (float) have;
        float power = 0.0f;
        for (auto& v : x) { v = juce::jmax (0.0f, v - mean); power += v * v; }
        if (power < 1.0e-9f) return out;

        const int maxLag = juce::jmin (have / 2, kRate * 4 + 8);                // how well the onsets repeat after every delay up to four seconds
        std::vector<float> acf ((size_t) maxLag + 2, 0.0f);
        for (int lag = 1; lag <= maxLag + 1 && lag < have; ++lag)
        {
            float sum = 0.0f;
            for (int i = lag; i < have; ++i) sum += x[(size_t) i] * x[(size_t) (i - lag)];
            acf[(size_t) lag] = sum / (float) (have - lag) / (power / (float) have);
        }
        constexpr int kTempos = 1201;                                           // 60.0 to 180.0
        std::vector<float> score ((size_t) kTempos, 0.0f);
        for (int t = 0; t < kTempos; ++t)
        {
            const float bpm = 60.0f + 0.1f * (float) t;
            const float step = (float) kRate * 60.0f / bpm / 4.0f;              // one sixteenth note, in frames
            float sum = 0.0f, weights = 0.0f;
            for (int m = 1; (float) m * step + 1.0f < (float) maxLag; ++m)
            {
                const float lag = (float) m * step; const int i = (int) lag; const float f = lag - (float) i;
                const float wgt = 1.0f + (m % 2 == 0 ? 1.0f : 0.0f) + (m % 4 == 0 ? 1.5f : 0.0f) + (m % 8 == 0 ? 1.5f : 0.0f) + (m % 16 == 0 ? 2.0f : 0.0f);
                sum += wgt * (acf[(size_t) i] * (1.0f - f) + acf[(size_t) i + 1] * f); weights += wgt;
            }
            score[(size_t) t] = weights > 0.0f ? sum / weights : 0.0f;
        }
        // between a tempo and its double, a gentle preference for the middle of the range
        auto preferred = [&] (int t) { const float o = std::log2 ((60.0f + 0.1f * (float) t) / 110.0f); return score[(size_t) t] * std::exp (-0.5f * o * o); };
        int best = 0;
        for (int t = 1; t < kTempos; ++t) if (preferred (t) > preferred (best)) best = t;
        if (score[(size_t) best] <= 0.0f) return out;
        out.bpm = 60.0f + 0.1f * (float) best;
        // the best tempo that is not this one, its half or its double
        auto related = [] (float a, float b) { const float r = a > b ? a / b : b / a; return r < 1.04f || std::abs (r - 2.0f) < 0.08f; };
        int rival = -1;
        for (int t = 0; t < kTempos; ++t)
            if (! related (60.0f + 0.1f * (float) t, out.bpm) && (rival < 0 || score[(size_t) t] > score[(size_t) rival])) rival = t;
        const float rivalScore = rival >= 0 ? juce::jmax (0.0f, score[(size_t) rival]) : 0.0f;
        // the half or double may score higher than the preferred one: the family's strength is the stronger of the two
        float family = score[(size_t) best];
        const float twin = out.bpm >= 120.0f ? out.bpm * 0.5f : out.bpm * 2.0f;
        if (twin >= 60.0f && twin <= 180.0f) { out.other = twin; family = juce::jmax (family, score[(size_t) juce::jlimit (0, kTempos - 1, (int) std::round ((twin - 60.0f) * 10.0f))]); }
        out.confidence = juce::jlimit (0.0f, 1.0f, (family - rivalScore) / (family + 1.0e-6f) * 2.5f);
        if (rival >= 0 && rivalScore > 0.8f * family) out.rival = 60.0f + 0.1f * (float) rival;
        return out;
    }

private:
    void frame() noexcept
    {
        for (int i = 0; i < kSize; ++i) work[(size_t) i] = ring[(size_t) ((pos + i) & (kSize - 1))] * window[(size_t) i];
        std::fill (work.begin() + kSize, work.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (work.data(), true);
        float total = 0.0f;
        std::array<float, kBands> band {};
        for (int b = 0; b < kBands; ++b)
        {
            float p = 0.0f;
            for (int k = edge[(size_t) b]; k < juce::jmax (edge[(size_t) b] + 1, edge[(size_t) b + 1]); ++k) p += work[(size_t) k] * work[(size_t) k];
            band[(size_t) b] = p; total += p;
        }
        // rises are measured in decibels above a floor that follows the song's own level, so a quiet song reads like a loud one
        floorPower += (total > floorPower ? 0.02f : 0.002f) * (total - floorPower);
        const float floor = 1.0e-4f * floorPower / (float) kBands + 1.0e-12f;
        float rise = 0.0f;
        for (int b = 0; b < kBands; ++b)
        {
            const float lg = std::log10 (band[(size_t) b] + floor);
            if (primed) rise += juce::jmax (0.0f, lg - prev[(size_t) b]);
            prev[(size_t) b] = lg;
        }
        primed = true;
        const float scaled = total / (float) (kSize * kSize);
        const int w = write.load (std::memory_order_relaxed);
        flux[(size_t) w].store (scaled > 1.0e-9f ? rise : 0.0f, std::memory_order_relaxed);
        write.store ((w + 1) % kFrames, std::memory_order_release);
        filled.store (juce::jmin (kFrames, filled.load (std::memory_order_relaxed) + 1), std::memory_order_relaxed);
        level.store (0.95f * level.load (std::memory_order_relaxed) + 0.05f * scaled, std::memory_order_relaxed);
    }

    double fs = 48000.0;
    int decim = 1, hop = 240, count = 0, pos = 0, phase = 0;
    float acc = 0.0f, floorPower = 0.0f;
    bool primed = false;
    juce::dsp::FFT fft { kOrder };
    std::vector<float> ring, work, window;
    std::array<int, kBands + 1> edge {};
    std::array<float, kBands> prev {};
    std::array<std::atomic<float>, kFrames> flux {};
    std::atomic<int> write { 0 }, filled { 0 };
    std::atomic<float> level { 0.0f };
    std::atomic<bool> listening { false }, restart { false };
};
}
