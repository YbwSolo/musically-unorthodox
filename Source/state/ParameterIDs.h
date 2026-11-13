#pragma once
#include <juce_core/juce_core.h>

namespace mu
{
constexpr int kNumEQBands = 6;

namespace ids
{
    // Instance / routing
    inline const juce::String role        { "role" };
    inline const juce::String bypass      { "bypass" };
    inline const juce::String trim        { "trim" };          // gain written by the balance engine
    inline const juce::String outputGain  { "outputGain" };
    inline const juce::String autoBalance { "autoBalance" };   // this instance accepts balance moves

    // Master targets (only meaningful on the Master instance)
    inline const juce::String targetLufs  { "targetLufs" };
    inline const juce::String targetTruePeak { "targetTruePeak" };

    // EQ
    inline const juce::String eqEnabled   { "eqEnabled" };
    inline juce::String eqBandEnabled (int b) { return "eqB" + juce::String (b) + "On"; }
    inline juce::String eqBandType    (int b) { return "eqB" + juce::String (b) + "Type"; }
    inline juce::String eqBandFreq    (int b) { return "eqB" + juce::String (b) + "Freq"; }
    inline juce::String eqBandGain    (int b) { return "eqB" + juce::String (b) + "Gain"; }
    inline juce::String eqBandQ       (int b) { return "eqB" + juce::String (b) + "Q"; }

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

    // Vocal Space (dynamic EQ on beat/instrument instances, keyed from the vocal instance)
    inline const juce::String vsEnabled { "vsEnabled" };
    inline const juce::String vsAmount  { "vsAmount" };
    inline const juce::String vsSpeed   { "vsSpeed" };
}

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
    return { "Auto detect", "Vocal", "Backing vocal", "Kick", "Bass / 808", "Drums", "Melody", "Beat (full)", "FX", "Master" };
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
}
