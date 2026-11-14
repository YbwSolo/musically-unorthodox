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

    // Tune (pitch correction)
    inline const juce::String tuneOn      { "tuneOn" };
    inline const juce::String tuneKey     { "tuneKey" };
    inline const juce::String tuneScale   { "tuneScale" };
    inline const juce::String tuneSpeed   { "tuneSpeed" };
    inline const juce::String tuneAmount  { "tuneAmount" };
    inline const juce::String tuneVibrato { "tuneVibrato" };
    inline const juce::String tuneMode    { "tuneMode" };       // simple (automatic) / graph (hand-edited notes)
    inline const juce::String tuneHold    { "tuneHold" };
    inline const juce::String tuneFormant { "tuneFormant" };
    inline const juce::String tuneNotes   { "tuneNotes" };      // the Custom scale: twelve bits, bit 0 = C
    inline const juce::String tuneFlex    { "tuneFlex" };       // cents either side of a note that are left as sung
    inline const juce::String tuneHumanise { "tuneHumanise" };  // held notes are corrected more gently
    inline const juce::String tuneSlides  { "tuneSlides" };     // slides between notes are left alone

    // Peak guard on a track (the master has its own limiter)
    inline const juce::String trackGuard  { "trackGuard" };

    // Doubler
    inline const juce::String dblOn     { "dblOn" };
    inline const juce::String dblMix    { "dblMix" };
    inline const juce::String fxOrder   { "fxOrder" };      // which order the in-line FX tools run in (see FxOrder.h)
    inline const juce::String dblDetune { "dblDetune" };
    inline const juce::String dblTiming { "dblTiming" };
    inline const juce::String dblWidth  { "dblWidth" };
    inline const juce::String dblVoices { "dblVoices" };

    // Reverb
    inline const juce::String rvOn     { "rvOn" };
    inline const juce::String rvMix    { "rvMix" };
    inline const juce::String rvAuto   { "rvAuto" };
    inline const juce::String rvLength { "rvLength" };
    inline const juce::String rvDecay  { "rvDecay" };
    inline const juce::String rvPre    { "rvPre" };
    inline const juce::String rvTone   { "rvTone" };
    inline const juce::String rvDuck   { "rvDuck" };
    inline const juce::String rvType   { "rvType" };
    inline const juce::String rvLowCut { "rvLowCut" };
    inline const juce::String rvWidth  { "rvWidth" };
    inline const juce::String rvSize   { "rvSize" };
    inline const juce::String rvDiffusion { "rvDiffusion" };
    inline const juce::String rvModDepth { "rvModDepth" };
    inline const juce::String rvModRate { "rvModRate" };
    inline const juce::String rvLowDecay { "rvLowDecay" };
    inline const juce::String rvEarly  { "rvEarly" };
    inline const juce::String rvFreeze { "rvFreeze" };
    inline const juce::String rvShimmer { "rvShimmer" };

    // Delay
    inline const juce::String dlOn       { "dlOn" };
    inline const juce::String dlMix      { "dlMix" };
    inline const juce::String dlSync     { "dlSync" };
    inline const juce::String dlNote     { "dlNote" };
    inline const juce::String dlTime     { "dlTime" };
    inline const juce::String dlFeedback { "dlFeedback" };
    inline const juce::String dlTone     { "dlTone" };
    inline const juce::String dlWidth    { "dlWidth" };
    inline const juce::String dlDuck     { "dlDuck" };
    inline const juce::String dlStyle    { "dlStyle" };
    inline const juce::String dlPattern  { "dlPattern" };
    inline const juce::String dlDrive    { "dlDrive" };
    inline const juce::String dlWobble   { "dlWobble" };
    inline const juce::String dlLowCut   { "dlLowCut" };

    // Harmony
    inline const juce::String hmOn      { "hmOn" };
    inline const juce::String hmVoicing { "hmVoicing" };
    inline const juce::String hmMix     { "hmMix" };
    inline const juce::String hmWidth   { "hmWidth" };
    inline const juce::String hmHumanise { "hmHumanise" };

    // Shift
    inline const juce::String psOn        { "psOn" };
    inline const juce::String psSemis     { "psSemis" };
    inline const juce::String psCharacter { "psCharacter" };
    inline const juce::String psMix       { "psMix" };

    // Phone
    inline const juce::String phOn     { "phOn" };
    inline const juce::String phAmount { "phAmount" };
    inline const juce::String phTone   { "phTone" };
    inline const juce::String phDrive  { "phDrive" };

    // Lo-fi
    inline const juce::String lfOn     { "lfOn" };
    inline const juce::String lfCrush  { "lfCrush" };
    inline const juce::String lfWobble { "lfWobble" };
    inline const juce::String lfMix    { "lfMix" };

    // Motion (chorus / flanger / phaser)
    inline const juce::String mdOn    { "mdOn" };
    inline const juce::String mdType  { "mdType" };
    inline const juce::String mdRate  { "mdRate" };
    inline const juce::String mdDepth { "mdDepth" };
    inline const juce::String mdMix   { "mdMix" };

    // Chop
    inline const juce::String chOn    { "chOn" };
    inline const juce::String chRate  { "chRate" };
    inline const juce::String chDepth { "chDepth" };
    inline const juce::String chShape { "chShape" };

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
