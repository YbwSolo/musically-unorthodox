#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "../dsp/EQEngine.h"
#include "../session/BalanceEngine.h"

namespace mu
{
// One piece of advice from the Mix Brain: what was heard, why it matters, and the move that fixes it.
struct Suggestion
{
    enum class Kind { EqBand, DeEsser, Saturation, Masking };
    Kind kind = Kind::EqBand;
    juce::String title;        // "Mud around 250 Hz"
    juce::String why;          // plain-language explanation shown to the student
    juce::String action;       // "Dynamic cut, -3.0 dB at 250 Hz"
    EQBandSettings band;       // for EqBand
    float freqHz = 0.0f;
    float amountDb = 0.0f;     // how far off the measurement was
    MaskingClash clash;        // for Masking
};

using BandArray = std::array<float, kAnalysisBands>;

BandArray analysisBandCentres();

// Typical long-term tonal balance for a role, in dB per analysis band (relative; only the shape matters).
BandArray roleTargetCurve (Role role);

// Compares a track's long-term spectrum with the usual shape for its role and returns the clear deviations.
// swingDb (optional, from a listening pass): how much each band's share of the sound comes and goes. It decides
// whether a cut should be fixed (the build-up is always there) or dynamic (it only appears on some notes or words).
std::vector<Suggestion> analyseTone (const BandArray& measuredDb, Role role, const BandArray* swingDb = nullptr);

// Describes a masking clash in words for the track that is asking.
Suggestion describeClash (const MaskingClash& clash, Role winnerRole, Role loserRole, bool askerIsLoser);

// EQ match: up to maxBands broad bells that move `measured` towards `reference`. amount 0..1.
std::vector<EQBandSettings> matchReference (const BandArray& measuredDb, const BandArray& referenceDb, float amount, int maxBands = 5);
}
