#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace mu
{
// 4x true-peak estimator in the style of ITU-R BS.1770 annex 2: a 48-tap windowed-sinc interpolator
// evaluated as four 12-tap polyphase branches. Returns the largest inter-sample magnitude for each
// input sample. The estimate lags the input by kDelay samples.
class TruePeakDetector
{
public:
    static constexpr int kPhases = 4;
    static constexpr int kTaps = 12;
    static constexpr int kDelay = 6;

    TruePeakDetector()
    {
        const double centre = (kPhases * kTaps - 1) * 0.5;
        const double pi = 3.14159265358979323846;
        for (int p = 0; p < kPhases; ++p)
        {
            double sum = 0.0;
            for (int k = 0; k < kTaps; ++k)
            {
                const int n = k * kPhases + p;
                const double x = ((double) n - centre) / (double) kPhases;
                const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double window = 0.5 - 0.5 * std::cos (2.0 * pi * ((double) n + 0.5) / (double) (kPhases * kTaps));
                coeffs[(size_t) p][(size_t) k] = (float) (sinc * window);
                sum += sinc * window;
            }
            for (auto& c : coeffs[(size_t) p]) c = (float) (c / sum);
        }
        reset();
    }

    void reset() noexcept { history.fill (0.0f); pos = 0; }

    inline float process (float x) noexcept
    {
        history[(size_t) pos] = x;
        float peak = 0.0f;
        for (int p = 0; p < kPhases; ++p)
        {
            float acc = 0.0f;
            int idx = pos;
            for (int k = 0; k < kTaps; ++k)
            {
                acc += history[(size_t) idx] * coeffs[(size_t) p][(size_t) k];
                idx = idx == 0 ? kTaps - 1 : idx - 1;
            }
            peak = std::max (peak, std::abs (acc));
        }
        pos = pos + 1 == kTaps ? 0 : pos + 1;
        return peak;
    }

private:
    std::array<std::array<float, kTaps>, kPhases> coeffs {};
    std::array<float, kTaps> history {};
    int pos = 0;
};
}
