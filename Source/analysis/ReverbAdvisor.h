#pragma once
#include <juce_core/juce_core.h>
#include "../state/ParameterIDs.h"

namespace mu
{
// What the plugin knows about a track when it is asked to choose a reverb for it.
struct ReverbFacts
{
    Role role = Role::Vocal;
    double bpm = 120.0;
    float crestDb = 12.0f;          // peak level above average loudness: high = short, punchy sounds; low = sustained
    float highShare = 0.15f;        // share of the track's energy above 5 kHz
    bool sustainedNotes = false;    // long held notes were heard (singing rather than rapping)
    bool measured = false;          // the track has been heard; false = go by role and tempo alone
};

struct ReverbAdvice
{
    bool use = true;                // false: this kind of track is better left dry
    int type = 1;                   // 0 room, 1 plate, 2 hall
    int length = 1;                 // 0 = one beat, 1 = two beats, 2 = four beats
    float mix = 18.0f, tone = 50.0f, duck = 50.0f, lowCutHz = 180.0f, width = 100.0f;      // in the units of the controls
    juce::String reason;            // one or two sentences for the person at the desk
};

// Chooses a reverb the way an engineer would on a first pass: the kind of space from what the track is, the
// length from the tempo (the tail should be gone before the next phrase), the tone from how bright the track
// already is, and the ducking from how much the words matter.
ReverbAdvice adviseReverb (const ReverbFacts& facts);
}
