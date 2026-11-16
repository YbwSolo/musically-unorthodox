#pragma once

namespace mu
{
// What was measured on the stems of the five finished releases the studio chose as its standard (build 23).
// The releases were split into vocals, bass, drums and music with a stem splitter, and each figure is the middle
// one of the five, measured with the plugin's own probe (dsp/VocalProbe.h) on the parts where the lead is
// performing. Stem splitters smear reverb tails and the copies were cut off at 15.8 kHz: the tail figure is
// rough and nothing here says anything about the top octave.
struct VocalReference
{
    int records = 5;
    // the whole instrumental against the vocal, integrated loudness (the way Learn and balance measures tracks)
    float beatOverVocalLu = 2.9f, beatOverVocalLeast = 1.5f, beatOverVocalMost = 5.2f;
    // the stems against the vocal, the same way
    float bassOverVocalLu = 0.0f, drumsOverVocalLu = -1.0f, musicOverVocalLu = -5.9f;
    // the loudest syllables (1 step of 50 ms in 100) above the usual level of the voice
    float peakSpreadDb = 5.0f, peakSpreadLeast = 4.0f, peakSpreadMost = 5.5f;
    // the sides of the vocal against its middle (reverb, delay, doubles and ad-libs out wide)
    float sideDb = -21.8f, sideNarrowest = -25.3f, sideWidest = -13.3f;
    // the loudest esses against the loud vowels
    float essDb = -16.0f, essMost = -13.5f;
    // what is left 0.2 to 0.5 s after a phrase ends, against the phrase (rough)
    float tailDb = -21.0f;
    // the instrumental's sides against its middle between 800 Hz and 5 kHz
    float beatBandSideDb = -4.6f, beatBandSideNarrowest = -8.1f;
};
inline VocalReference vocalReference() { return {}; }
}
