#include "ReverbAdvisor.h"
#include <cmath>

namespace mu
{
ReverbAdvice adviseReverb (const ReverbFacts& f)
{
    ReverbAdvice a;
    const double bpm = juce::jlimit (40.0, 300.0, f.bpm);
    const float beat = (float) (60.0 / bpm);                 // seconds
    const juce::String tempo = juce::String (juce::roundToInt (bpm)) + " BPM";

    // how long the tail should last for this kind of track, in seconds, before it is fitted to the beat
    float wanted = 1.1f;
    juce::String what;
    switch (f.role)
    {
        case Role::Vocal:
        {
            const bool rap = f.measured && ! f.sustainedNotes && f.crestDb > 13.0f;
            if (rap)                    { wanted = 0.55f; a.type = 1; a.mix = 11.0f; a.duck = 70.0f; what = "Rapped lead vocal: a short plate, kept low and ducked hard, so the words stay dry and upfront"; }
            else if (f.sustainedNotes)  { wanted = 1.5f;  a.type = 1; a.mix = 19.0f; a.duck = 50.0f; what = "Sung lead vocal with held notes: a plate long enough to carry them"; }
            else                        { wanted = 1.0f;  a.type = 1; a.mix = 16.0f; a.duck = 58.0f; what = "Lead vocal: a plate, the usual vocal reverb, ducked so the words stay clear"; }
            a.lowCutHz = 200.0f;
            break;
        }
        case Role::BackingVocal: wanted = 1.7f;  a.type = 2; a.mix = 26.0f; a.duck = 25.0f; a.lowCutHz = 230.0f; what = "Backing vocals and adlibs: a hall, wetter than the lead, so they sit behind it"; break;
        case Role::Drums:        wanted = 0.45f; a.type = 0; a.mix = 12.0f; a.duck = 10.0f; a.lowCutHz = 260.0f; what = "Drums: a short room that adds size without smearing the hits"; break;
        case Role::Melody:       wanted = 1.9f;  a.type = 2; a.mix = 20.0f; a.duck = 15.0f; a.lowCutHz = 180.0f; what = "Melody: a hall, to put the instrument in a space behind the vocal"; break;
        case Role::FX:           wanted = 2.8f;  a.type = 2; a.mix = 32.0f; a.duck = 0.0f;  a.lowCutHz = 200.0f; what = "Effects: a long hall, because these are meant to wash"; break;
        case Role::Kick:
        case Role::Bass808:
            a.use = false;
            a.reason = "Kick and 808 are left dry: reverb on the low end blurs the punch and muddies the whole mix. If you want size on a kick, use a very short room on the drums instead.";
            return a;
        case Role::Beat:
            a.use = false;
            a.reason = "A finished two-track beat already has its own reverb mixed in. Adding more over the whole beat washes out the drums, so it is left off.";
            return a;
        case Role::Master:
            a.use = false;
            a.reason = "Reverb does not belong on the master: it would be added to every track at once, bass included. Put it on the tracks that need it.";
            return a;
        default: wanted = 1.1f; what = "A plate that suits most sounds"; break;
    }

    // the nearest of one, two or four beats, so the tail ends in time with the song
    const float options[] = { beat, beat * 2.0f, beat * 4.0f };
    float best = 1.0e9f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs (std::log (options[i] / wanted));
        if (d < best) { best = d; a.length = i; }
    }
    const int beats = a.length == 0 ? 1 : (a.length == 1 ? 2 : 4);

    // a bright source gets a darker reverb, or the S sounds ring; a dull one can take a brighter tail
    juce::String toneNote;
    if (! f.measured)              { a.tone = 50.0f; }
    else if (f.highShare > 0.22f)  { a.tone = 32.0f; toneNote = " The track is bright, so the reverb is kept dark to stop the S sounds ringing."; }
    else if (f.highShare < 0.06f)  { a.tone = 66.0f; toneNote = " The track is on the dull side, so the reverb is a little brighter to add air."; }
    else                           { a.tone = 48.0f; }

    static const char* const kinds[] = { "room", "plate", "hall" };
    a.reason = what + ". At " + tempo + " that is " + juce::String (beats) + (beats == 1 ? " beat" : " beats") + " (" + juce::String (options[a.length], 2) + " s) of " + kinds[a.type]
             + ", with a 64th-note gap before it starts." + toneNote + (f.measured ? juce::String() : juce::String (" Play the track and press this again for a closer fit."));
    return a;
}
}
