#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "../dsp/EQEngine.h"
#include "../session/BalanceEngine.h"

namespace mu
{
// What the track is, for the EQ analysis. The track kind (the session's role) is coarse: "Melody" covers a piano
// and a distorted guitar, which go wrong in different places. Naming the source tells the analysis which shape
// to compare with and which problems to look for.
enum class EqSource : int
{
    TrackKind = 0,
    LeadVocal, RapVocal, HighVocal, LowVocal, BackingVocals, Adlibs,                                   // voices
    Kick, Snare, Hats, DrumBus, Sub808, Bass, Piano, AcousticGuitar, ElectricGuitar, SynthLead, Pad, Brass, FullBeat, SoundFx,   // instruments
    Count
};
juce::StringArray eqSourceNames();
inline bool eqSourceIsVocal (EqSource s) { return s >= EqSource::LeadVocal && s <= EqSource::Adlibs; }
inline bool eqSourceIsInstrument (EqSource s) { return s >= EqSource::Kick && s < EqSource::Count; }
Role eqSourceRole (EqSource s, Role trackRole);          // which family of rules a source belongs to

// The three jobs an EQ does, in the order they are done.
//   Surgical: take out what should not be there (rumble, mud, boxiness, a ringing resonance). Cuts.
//   Tonal:    bring out what is good (warmth, body, presence, clarity, air). Broad boosts, worked out on the
//             sound as it will be once the surgical cuts are in.
//   Dynamic:  for a problem that is only there some of the time (harshness on loud words, one booming note):
//             a cut that works only while the problem is there.
enum class EqStage : int { Surgical = 0, Tonal, Dynamic };
constexpr int kAllEqPasses = 7;                           // bit 0 surgical, bit 1 tonal, bit 2 dynamic

// Where a beat or an instrument covers the lead vocal, worked out from the two measured spectra: up to three
// places, how deep to dip each while the vocal is singing, and how wide.
struct VocalRoomPlan
{
    bool found = false;
    int count = 0;
    std::array<float, 3> freqHz { 260.0f, 2600.0f, 7000.0f }, cutDb { 0.0f, 0.0f, 0.0f }, q { 0.9f, 0.8f, 1.1f };
    juce::String text;
};
VocalRoomPlan planVocalRoom (const std::array<float, kAnalysisBands>& vocalDb, const std::array<float, kAnalysisBands>& trackDb);

// One piece of advice from the Mix Brain: what was heard, why it matters, and the move that fixes it.
struct Suggestion
{
    enum class Kind { EqBand, DeEsser, Saturation, Masking, VocalRoom };
    Kind kind = Kind::EqBand;
    EqStage stage = EqStage::Surgical;
    VocalRoomPlan room;        // for VocalRoom
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
// source says what the track is when the role alone is too coarse; passes chooses which of the three jobs to do.
std::vector<Suggestion> analyseTone (const BandArray& measuredDb, Role role, const BandArray* swingDb = nullptr,
                                     EqSource source = EqSource::TrackKind, int passes = kAllEqPasses, float depth = 1.0f);
// depth: 1 lists what clearly stands out; 0.6 ("Detailed") also lists smaller differences, for the ear to judge.
// The shape a source is compared with (the role's curve when the source has none of its own).
BandArray sourceTargetCurve (EqSource source, Role trackRole);

// Describes a masking clash in words for the track that is asking.
Suggestion describeClash (const MaskingClash& clash, Role winnerRole, Role loserRole, bool askerIsLoser);

// EQ match: up to maxBands broad bells that move `measured` towards `reference`. amount 0..1.
std::vector<EQBandSettings> matchReference (const BandArray& measuredDb, const BandArray& referenceDb, float amount, int maxBands = 5);
}
