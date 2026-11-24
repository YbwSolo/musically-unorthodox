// A lab tool: MUTempoLab runs the tempo detector on patterns of known tempo; MUTempoLab <stereo.f32> [skip seconds] runs it on a recording.
#include "../dsp/TempoDetector.h"
#include "../learn/LessonSound.h"
#include <cstdio>
#include <fstream>
int main (int argc, char** argv)
{
    mu::TempoDetector det; det.prepare (48000.0); det.setListening (true);
    if (argc > 1)
    {
        std::ifstream f (argv[1], std::ios::binary); std::vector<float> d; float v[2];
        const double skip = argc > 2 ? atof (argv[2]) : 0.0; const float gain = argc > 3 ? (float) atof (argv[3]) : 1.0f; f.seekg ((std::streamoff) (skip * 48000.0) * 8);
        std::vector<float> l, r; while (f.read ((char*) v, 8) && l.size() < 48000 * 30) { l.push_back (v[0] * gain); r.push_back (v[1] * gain); }
        for (size_t i = 0; i + 480 <= l.size(); i += 480) { det.push (&l[i], &r[i], 480); if (i % (48000 * 5) == 0 && i > 0) { auto t = det.read(); std::printf ("  %2ds %.1f", (int) (i / 48000), t.bpm); } }
        auto t = det.read(); std::printf ("  -> %.1f (or %.1f) conf %.2f rival %.1f\n", t.bpm, t.other, t.confidence, t.rival); return 0;
    }
    for (float bpm : { 70.0f, 76.0f, 82.0f, 90.0f, 96.0f, 104.0f, 120.0f, 128.0f, 140.0f, 142.0f, 160.0f })
        for (int style = 0; style < 3; ++style)
        {
            mu::LessonSound s; s.prepare (48000.0); det.prepare (48000.0); det.setListening (true);
            mu::LessonPattern p; p.bpm = bpm;
            if (style == 0) { p.kick[0] = p.kick[8] = 100; p.snare[4] = p.snare[12] = 100; for (int i = 0; i < 16; i += 2) p.hat[i] = 80; }
            else if (style == 1) { p.steps = 32; for (int i : { 0, 6, 16, 19, 26 }) p.kick[i] = 100; p.snare[8] = p.snare[24] = 100; for (int i = 0; i < 32; i += 2) p.hat[i] = 80; p.hat[13] = p.hat[29] = p.hat[31] = 60; for (int i : {0, 6, 16, 26}) { p.bass[i] = 36; p.bassLen[i] = 5; } }
            else { p.steps = 32; for (int i : { 0, 10, 16, 23, 26 }) p.kick[i] = 100; p.snare[8] = p.snare[28] = 100; for (int i : { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 }) p.hat[i] = 85; }
            s.setPattern (p); s.start();
            juce::AudioBuffer<float> b (2, 480);
            for (int n = 0; n < 100 * 16; ++n) { b.clear(); s.process (b); det.push (b.getReadPointer (0), b.getReadPointer (1), 480); }
            auto t = det.read();
            std::printf ("%s %5.1f -> %6.2f conf %.2f %s\n", style == 0 ? "plain" : style == 1 ? "trap " : "drill", bpm, t.bpm, t.confidence,
                         std::abs (t.bpm - bpm) < 0.6f ? "ok" : (std::abs (t.bpm * 2 - bpm) < 1.2f || std::abs (t.bpm - bpm * 2) < 1.2f) ? "octave" : "WRONG");
        }
}
