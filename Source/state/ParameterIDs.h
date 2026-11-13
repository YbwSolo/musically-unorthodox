#pragma once
#include <juce_core/juce_core.h>

namespace mu
{
constexpr int kNumEQBands = 8;

enum class Role : int
{
    Auto = 0,
    Vocal,
    BackingVocal,
    Kick,
    Bass808,
    Drums,
    Melody,
    Beat,
    FX,
    Master,
    NumRoles
};

inline juce::StringArray roleNames()
{
    return { "Auto detect", "Lead vocal", "Backing / adlib", "Kick", "Bass / 808", "Drums", "Melody", "Beat (2-track)", "FX", "Master" };
}

inline juce::String roleSlug (Role r)
{
    static const char* slugs[] = { "auto", "vocal", "backing", "kick", "bass", "drums", "melody", "beat", "fx", "master" };
    return slugs[juce::jlimit (0, (int) Role::NumRoles - 1, (int) r)];
}

enum class EQType : int
{
    LowCut = 0,
    LowShelf,
    Bell,
    HighShelf,
    HighCut,
    Notch
};

inline juce::StringArray eqTypeNames() { return { "Low cut", "Low shelf", "Bell", "High shelf", "High cut", "Notch" }; }

namespace ids
{
    // Instance / routing
    inline const juce::String role        { "role" };
    inline const juce::String bypass      { "bypass" };
    inline const juce::String trim        { "trim" };          // gain written by the balance engine
    inline const juce::String outputGain  { "outputGain" };
    inline const juce::String autoBalance { "autoBalance" };   // this instance accepts balance moves

    // Master targets (meaningful on the Master instance)
    inline const juce::String targetLufs     { "targetLufs" };
    inline const juce::String targetTruePeak { "targetTruePeak" };
    inline const juce::String masterAutoTrim { "masterAutoTrim" };
    inline const juce::String peakGuard      { "peakGuard" };
    inline juce::String roleOffset (Role r)  { return "roleOff_" + roleSlug (r); }

    // Gate
    inline const juce::String gateOn        { "gateOn" };
    inline const juce::String gateThreshold { "gateThreshold" };
    inline const juce::String gateRange     { "gateRange" };
    inline const juce::String gateAttack    { "gateAttack" };
    inline const juce::String gateRelease   { "gateRelease" };
    inline const juce::String gateHold      { "gateHold" };

    // EQ
    inline const juce::String eqEnabled   { "eqEnabled" };
    inline juce::String eqBandEnabled (int b) { return "eqB" + juce::String (b) + "On"; }
    inline juce::String eqBandType    (int b) { return "eqB" + juce::String (b) + "Type"; }
    inline juce::String eqBandFreq    (int b) { return "eqB" + juce::String (b) + "Freq"; }
    inline juce::String eqBandGain    (int b) { return "eqB" + juce::String (b) + "Gain"; }
    inline juce::String eqBandQ       (int b) { return "eqB" + juce::String (b) + "Q"; }
    inline juce::String eqBandDyn     (int b) { return "eqB" + juce::String (b) + "Dyn"; }
    inline juce::String eqBandThr     (int b) { return "eqB" + juce::String (b) + "Thr"; }
    inline juce::String eqBandRange   (int b) { return "eqB" + juce::String (b) + "Range"; }
    inline juce::String eqBandSlope   (int b) { return "eqB" + juce::String (b) + "Slope"; }   // cut filters: 12/24/36/48 dB/oct
    inline juce::String eqBandMode    (int b) { return "eqB" + juce::String (b) + "Mode"; }    // stereo / mid / side
    inline const juce::String eqAutoGain  { "eqAutoGain" };

    // Compressor
    inline const juce::String compEnabled    { "compEnabled" };
    inline const juce::String compThreshold  { "compThreshold" };
    inline const juce::String compRatio      { "compRatio" };
    inline const juce::String compAttack     { "compAttack" };
    inline const juce::String compRelease    { "compRelease" };
    inline const juce::String compKnee       { "compKnee" };
    inline const juce::String compMakeup     { "compMakeup" };
    inline const juce::String compAutoMakeup { "compAutoMakeup" };
    inline const juce::String compMix        { "compMix" };
    inline const juce::String compStyle      { "compStyle" };     // clean / punch / smooth / glue
    inline const juce::String compScHp       { "compScHp" };      // sidechain high-pass

    // Smooth (resonance tamer)
    inline const juce::String smOn    { "smOn" };
    inline const juce::String smDepth { "smDepth" };
    inline const juce::String smFocus { "smFocus" };
    inline const juce::String smSpeed { "smSpeed" };

    // Stereo
    inline const juce::String stWidth    { "stWidth" };
    inline const juce::String stMonoBass { "stMonoBass" };
    inline const juce::String stMonoFreq { "stMonoFreq" };

    // Master
    inline const juce::String maxGain    { "maxGain" };       // drive into the limiter
    inline const juce::String masterMode { "masterMode" };    // hand-off / streaming / loud / custom

    // De-esser
    inline const juce::String dsOn        { "dsOn" };
    inline const juce::String dsFreq      { "dsFreq" };
    inline const juce::String dsThreshold { "dsThreshold" };
    inline const juce::String dsRange     { "dsRange" };
    inline const juce::String dsMode      { "dsMode" };

    // Saturator
    inline const juce::String satOn    { "satOn" };
    inline const juce::String satDrive { "satDrive" };
    inline const juce::String satMix   { "satMix" };
    inline const juce::String satType  { "satType" };

    // Rider
    inline const juce::String riderOn     { "riderOn" };
    inline const juce::String riderTarget { "riderTarget" };
    inline const juce::String riderRange  { "riderRange" };
    inline const juce::String riderSpeed  { "riderSpeed" };

    // Vocal Space (dynamic EQ on beat/instrument instances, keyed from the vocal instance)
    inline const juce::String vsEnabled { "vsEnabled" };
    inline const juce::String vsAmount  { "vsAmount" };
    inline const juce::String vsSpeed   { "vsSpeed" };
}

// Default relative loudness of each role against the lead vocal (LU). Editable on the master instance.
inline float defaultRoleOffsetLu (Role r)
{
    switch (r)
    {
        case Role::Vocal:        return 0.0f;
        case Role::BackingVocal: return -6.0f;
        case Role::Kick:         return -3.5f;
        case Role::Bass808:      return -2.5f;
        case Role::Drums:        return -4.5f;
        case Role::Melody:       return -8.0f;
        case Role::Beat:         return -1.5f;
        case Role::FX:           return -12.0f;
        default:                 return -4.0f;
    }
}
}
