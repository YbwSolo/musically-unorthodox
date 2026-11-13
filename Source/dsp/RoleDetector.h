#pragma once
#include "../state/ParameterIDs.h"

namespace mu
{
struct RoleFeatures
{
    float lowEnergyRatio = 0.0f;   // < 150 Hz share of energy
    float highEnergyRatio = 0.0f;  // > 5 kHz share
    float centroidHz = 0.0f;
    float crestFactorDb = 0.0f;
    bool valid = false;
};

struct RoleGuess { Role role = Role::Beat; float confidence = 0.0f; const char* reason = ""; };

// Heuristic instrument-role classifier from long-term spectral shape and dynamics.
// Deliberately conservative: when unsure it returns Beat (full mix) with low confidence.
RoleGuess detectRole (const RoleFeatures& f) noexcept;
}
