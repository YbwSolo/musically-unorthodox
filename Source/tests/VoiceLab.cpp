// Runs a recording through one of the plugin's tools, outside a host, so the result can be measured and heard.
//   MUVoiceLab <in.f32> <out.f32> <tool> [name=value ...]
// The files are raw 32-bit floats at 48 kHz: mono in, mono out (or stereo out, interleaved, with stereo=1).
// Tools: tune, harmony, shift, comp, compold, reverb, echo.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "dsp/Tuner.h"
#include "dsp/PitchFX.h"
#include "dsp/Compressor.h"
#include "dsp/Reverb.h"
#include "dsp/Echo.h"
#if MU_LAB_OLD
 #include "OldCompressor.h"
#endif

using namespace mu;

int main (int argc, char** argv)
{
    if (argc < 4) { std::printf ("usage: MUVoiceLab in.f32 out.f32 tool [name=value ...]\n"); return 2; }
    const double fs = 48000.0;
    juce::MemoryBlock data;
    if (! juce::File (argv[1]).loadFileAsData (data)) { std::printf ("cannot read %s\n", argv[1]); return 2; }
    const int n = (int) (data.getSize() / sizeof (float));
    juce::AudioBuffer<float> b (2, n);
    b.copyFrom (0, 0, (const float*) data.getData(), n);
    b.copyFrom (1, 0, (const float*) data.getData(), n);
    const juce::String tool (argv[3]);
    juce::StringPairArray opt;
    for (int i = 4; i < argc; ++i) { const juce::String a (argv[i]); opt.set (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false)); }
    auto num = [&opt] (const char* name, float fallback) { return opt.containsKey (name) ? opt[name].getFloatValue() : fallback; };
    const int block = 256;

    Tuner tuner; tuner.prepare (fs, block, 2);
    PitchFX fx; fx.prepare (fs, block, 2);
    Compressor comp; comp.prepare (fs, block, 2);
    Reverb reverb; reverb.prepare (fs, block, 2);
    Echo echo; echo.prepare (fs, block, 2);

    TunerSettings ts; ts.enabled = true; ts.key = (int) num ("key", 0); ts.scale = (int) num ("scale", 1);
    ts.speedMs = num ("speed", 0.0f); ts.keepVibrato = num ("vibrato", 0.0f); ts.hold = num ("hold", 0.3f); ts.amount = num ("amount", 1.0f);
    ts.flexCents = num ("flex", 0.0f); ts.humanise = num ("humanise", 0.0f); ts.keepSlides = num ("slides", 0.0f); ts.legacySlicing = num ("old", 0.0f) > 0.5f;
    tuner.setSettings (ts);
    PitchFXSettings ps; ps.key = 0; ps.scale = 1;
    if (tool == "harmony") { ps.harmonyOn = true; ps.voicing = (int) num ("voicing", 0); ps.harmonyMix = 1.0f; ps.humanise = num ("humanise", 0.0f); }
    if (tool == "shift")   { ps.shiftOn = true; ps.semitones = num ("semis", -12.0f); ps.shiftMix = 1.0f; }
    fx.setSettings (ps);
    CompressorSettings cs; cs.enabled = true; cs.thresholdDb = num ("thr", -30.0f); cs.ratio = num ("ratio", 4.0f); cs.attackMs = num ("att", 3.0f); cs.releaseMs = num ("rel", 60.0f);
    cs.kneeDb = num ("knee", 3.0f); cs.style = (CompStyle) (int) num ("style", 1); cs.sidechainHpHz = num ("schp", 20.0f); cs.autoMakeup = num ("makeup", 1.0f) > 0.5f;
    cs.catchOverDb = num ("catchover", cs.catchOverDb); cs.catchReleaseMs = num ("catchrel", cs.catchReleaseMs); cs.catchMaxDb = num ("catchmax", cs.catchMaxDb); cs.levelRelease = num ("lrel", cs.levelRelease);
    comp.setSettings (cs);
   #if MU_LAB_OLD
    muold::Compressor oldComp; oldComp.prepare (fs, block, 2);
    muold::CompressorSettings os; os.enabled = true; os.thresholdDb = cs.thresholdDb; os.ratio = cs.ratio; os.attackMs = cs.attackMs; os.releaseMs = cs.releaseMs;
    os.kneeDb = cs.kneeDb; os.style = (muold::CompStyle) (int) num ("style", 1); os.sidechainHpHz = cs.sidechainHpHz; os.autoMakeup = cs.autoMakeup;
    oldComp.setSettings (os);
   #endif
    ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = num ("decay", 2.0f); rs.mix = num ("mix", 1.0f); rs.duck = num ("duck", 0.0f); rs.type = (int) num ("type", 0);
    rs.preDelayMs = num ("predelay", 0.0f); rs.tone = num ("tone", 0.5f); rs.lowCutHz = num ("lowcut", 180.0f); rs.width = num ("width", 1.0f); rs.size = num ("size", 1.0f);
    rs.diffusion = num ("diffusion", 0.8f); rs.modDepth = num ("mod", 0.3f); rs.modRate = num ("rate", 0.6f); rs.lowDecay = num ("lowdecay", 1.0f); rs.early = num ("early", 0.5f);
    rs.freeze = num ("freeze", 0.0f) > 0.5f; rs.shimmer = num ("shimmer", 0.5f);
    reverb.setSettings (rs);
    juce::AudioBuffer<float> dryCopy; dryCopy.makeCopyOf (b);

    int voicedBlocks = 0, slidingBlocks = 0; double slideSum = 0.0, shiftSum = 0.0;
    for (int start = 0; start < n; start += block)
    {
        const int len = juce::jmin (block, n - start);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, start, len);
        if (tool == "tune")
        {
            tuner.process (v);
            if (tuner.getDetectedNote() > 0.0f) { ++voicedBlocks; slideSum += tuner.getSlideSpeed(); if (tuner.getSlideSpeed() > 5.5f) ++slidingBlocks; shiftSum += std::abs (tuner.getShift()); }
        }
        else if (tool == "harmony" || tool == "shift") fx.process (v);
        else if (tool == "comp") comp.process (v);
       #if MU_LAB_OLD
        else if (tool == "compold") oldComp.process (v);
       #endif
        else if (tool == "reverb") reverb.process (v);
        else { std::printf ("unknown tool %s\n", argv[3]); return 2; }
    }
    if (voicedBlocks > 0 && num ("report", 0.0f) > 0.5f)
        std::printf ("   tuner: sliding %.0f%% of the voiced time, mean slide speed %.1f st/s, mean correction %.0f cents\n", 100.0 * slidingBlocks / voicedBlocks, slideSum / voicedBlocks, 100.0 * shiftSum / voicedBlocks);
    if (num ("wetonly", 0.0f) > 0.5f) for (int c = 0; c < 2; ++c) b.addFrom (c, 0, dryCopy, c, 0, n, -1.0f);
    juce::File out (argv[2]);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (num ("stereo", 0.0f) > 0.5f)
        for (int i = 0; i < n; ++i) { stream.writeFloat (b.getSample (0, i)); stream.writeFloat (b.getSample (1, i)); }
    else
        stream.write (b.getReadPointer (0), (size_t) n * sizeof (float));
    return 0;
}
