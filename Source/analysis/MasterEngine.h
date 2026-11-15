#pragma once
#include "TonalAnalysis.h"
#include "../dsp/MasterProbe.h"
#include "../dsp/MultibandComp.h"

namespace mu
{
// Everything one listening pass measured about a mix (or about a reference track, or about the finished master).
struct MasterFacts
{
    ProbeFacts probe;
    BandArray bandsDb {}, swingDb {};
    bool hasBands = false;
    float integratedLufs = -100.0f, truePeakDb = -144.0f;
    bool enough() const { return probe.seconds >= 15.0f && hasBands && integratedLufs > -60.0f; }
    float peakToLoudness() const { return truePeakDb - integratedLufs; }       // how far the peaks stand above the loudness
};

enum class MasterStyle : int { Transparent = 0, Warm, Loud, Dynamic, Bright, Aggressive, Count };
juce::StringArray masterStyleNames();
juce::String masterStyleBlurb (MasterStyle);
BandArray masterTargetCurve (MasterStyle);
// How far a mix's tone is from a target shape, in dB (the typical size of the difference between 60 Hz and 12 kHz).
float toneDistanceDb (const BandArray& measuredDb, const BandArray& targetDb);

// What the engine decided for one song: which processors to use, how each is set, and why.
struct MasterPlan
{
    juce::StringArray detected;        // what was found, each with its measurement
    juce::StringArray processing;      // what will be done about it
    juce::StringArray leftAlone;       // what was considered and not used, and why
    juce::StringArray notes;           // what a master cannot fix: it belongs in the mix
    std::vector<EQBandSettings> eq;    // corrective, tonal and dynamic bands, in that order
    bool glue = false; float glueRatio = 1.5f, glueThresholdDb = -20.0f;
    MultibandSettings multiband;
    bool saturate = false; int satType = 0; float satDriveDb = 3.0f, satMix = 0.25f;
    bool monoBass = false; float monoBelowHz = 120.0f, width = 1.0f;
    bool clip = false; float clipAmount = 0.5f, clipSoftness = 0.5f;
    float targetLufs = -14.0f, ceilingDb = -1.0f;
    BandArray targetCurve {};
};

// The decision. A set of rules worked from the measurements: no model, nothing random. style leans the tone and
// the loudness; intensity (0..1) scales how far every move goes; a reference, if there is one, replaces the
// style's tone and loudness with those of the reference, inside the same limits.
MasterPlan planMaster (const MasterFacts& mix, MasterStyle style, float intensity, const MasterFacts* reference = nullptr);

// After the master has been measured too: what changed, with the figures. A line says "improved" only when the
// measurement moved the right way by a margin; otherwise it says what was measured.
juce::StringArray masterReport (const MasterFacts& mix, const MasterFacts& master, const MasterPlan& plan);

// Measures a whole stereo recording in one go (a reference file, or a mix in a test), the same way a listening pass does.
MasterFacts measureMix (const juce::AudioBuffer<float>& stereo, double sampleRate);
}
