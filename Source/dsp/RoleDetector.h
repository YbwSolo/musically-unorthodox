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
    bool mono = false;             // the track itself is mono, so it cannot be a stereo beat
    bool voiceTools = false;       // Tune or Harmony is switched on, which only makes sense on a voice
};

struct RoleGuess { Role role = Role::Beat; float confidence = 0.0f; const char* reason = ""; };

// Heuristic instrument-role classifier from long-term spectral shape and dynamics.
// Deliberately conservative: when unsure it returns Beat (full mix) with low confidence.
RoleGuess detectRole (const RoleFeatures& f) noexcept;

// What a track's name says it is: "rap", "Lead Vox", "808", "Hook adlibs", "beat 128 bpm Gmin", "Stereo Out".
// Returns false for names that say nothing, such as "Audio 3".
bool roleFromName (const juce::String& name, Role& role);
}
