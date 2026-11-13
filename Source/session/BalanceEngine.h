#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "../state/ParameterIDs.h"

namespace mu
{
constexpr int kAnalysisBands = 32;

struct TrackInfo
{
    int slot = -1;
    juce::String name;
    Role role = Role::Beat;
    float levelLufs = -100.0f;      // measured after this instance's processing (includes current trim)
    float truePeakDb = -100.0f;
    float trimDb = 0.0f;            // current balance trim applied inside the instance
    bool acceptsBalance = true;
    bool active = false;
    std::array<float, kAnalysisBands> bandsDb {};
    bool hasBands = false;
};

struct BalanceMove { int slot = -1; juce::String name; float deltaDb = 0.0f; float newTrimDb = 0.0f; juce::String note; };

struct BalancePlan
{
    std::vector<BalanceMove> moves;
    float projectedMasterLufs = -100.0f;
    float projectedMasterTruePeak = -100.0f;
    float measuredMasterLufs = -100.0f;
    float measuredMasterTruePeak = -100.0f;
    bool hasMaster = false;
    bool usable = false;
    juce::String summary;
};

struct MaskingClash
{
    int slotA = -1, slotB = -1;   // B is the track that should give way
    juce::String nameA, nameB;
    float freqHz = 0.0f;
    float severityDb = 0.0f;      // how far above the clash threshold
    float suggestedCutDb = 0.0f;
};

struct BalanceTargets
{
    float masterLufs = -18.0f;
    float masterTruePeak = -6.0f;
    float maxMoveDb = 12.0f;
    std::array<float, (size_t) Role::NumRoles> offsets = defaultOffsets();

    static std::array<float, (size_t) Role::NumRoles> defaultOffsets()
    {
        std::array<float, (size_t) Role::NumRoles> o {};
        for (int r = 0; r < (int) Role::NumRoles; ++r) o[(size_t) r] = defaultRoleOffsetLu ((Role) r);
        return o;
    }
    float offsetFor (Role r) const { return offsets[(size_t) juce::jlimit (0, (int) Role::NumRoles - 1, (int) r)]; }
};

int rolePriority (Role r) noexcept;   // higher = keeps its frequency space

BalancePlan computeBalance (const std::vector<TrackInfo>& tracks, const BalanceTargets& targets);
std::vector<MaskingClash> findMasking (const std::vector<TrackInfo>& tracks, const std::array<float, kAnalysisBands>& bandCentresHz);
}
