// Runs a finished mix through a real master instance of the plugin and lets the mastering assistant do its job,
// so the decisions and the result can be read and measured outside a host.
//   MUMasterLab <mix.f32 stereo, 48 kHz> <outFolder> [style=0..5] [intensity=0..100] [ref=<reference.f32 stereo>]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "PluginProcessor.h"

using namespace mu;

struct LabHead : juce::AudioPlayHead
{
    double seconds = 0.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (120.0); p.setIsPlaying (true); p.setTimeInSeconds (seconds); p.setTimeInSamples ((juce::int64) (seconds * 48000.0)); p.setPpqPosition (seconds * 2.0);
        return p;
    }
};

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: MUMasterLab mix.f32 outFolder [style=0..5] [intensity=0..100] [ref=reference.f32]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 512;
    juce::MemoryBlock data;
    if (! juce::File (argv[1]).loadFileAsData (data)) { std::printf ("cannot read the mix\n"); return 2; }
    const float* mixData = (const float*) data.getData();
    const int frames = (int) (data.getSize() / sizeof (float) / 2);
    juce::StringPairArray opt;
    for (int i = 3; i < argc; ++i) { const juce::String a (argv[i]); opt.set (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false)); }
    const juce::File folder (argv[2]); folder.createDirectory();

    LabHead head;
    auto master = std::make_unique<MUProcessor>();
    master->setPlayConfigDetails (2, 2, fs, block);
    master->prepareToPlay (fs, block);
    master->setPlayHead (&head);
    master->setParam (ids::role, (float) Role::Master);
    master->setParam (ids::mstStyle, opt.containsKey ("style") ? opt["style"].getFloatValue() : 0.0f);
    master->setParam (ids::mstIntensity, opt.containsKey ("intensity") ? opt["intensity"].getFloatValue() : 50.0f);

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    juce::int64 pos = 0;
    std::unique_ptr<juce::FileOutputStream> rec;
    auto runSeconds = [&] (double seconds)
    {
        const int blocks = (int) (seconds * fs / block);
        for (int n = 0; n < blocks; ++n)
        {
            for (int i = 0; i < block; ++i, ++pos) { const int m = (int) (pos % frames); buf.setSample (0, i, mixData[2 * m]); buf.setSample (1, i, mixData[2 * m + 1]); }
            master->processBlock (buf, midi);
            if (rec != nullptr) for (int i = 0; i < block; ++i) { rec->writeFloat (buf.getSample (0, i)); rec->writeFloat (buf.getSample (1, i)); }
            head.seconds += (double) block / fs;
            master->pumpAnalysis();
            if (n % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    };
    auto printPart = [&] (const char* title, const char* part) { const auto lines = master->masterText (part); if (lines.isEmpty()) return; std::printf ("%s\n", title); for (auto& l : lines) std::printf ("  - %s\n", l.toRawUTF8()); };

    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    runSeconds (2.0);
    if (opt.containsKey ("ref"))
    {
        juce::MemoryBlock refData;
        if (juce::File (opt["ref"]).loadFileAsData (refData))
        {
            const int rf = (int) (refData.getSize() / sizeof (float) / 2);
            juce::AudioBuffer<float> ref (2, rf);
            const float* r = (const float*) refData.getData();
            for (int i = 0; i < rf; ++i) { ref.setSample (0, i, r[2 * i]); ref.setSample (1, i, r[2 * i + 1]); }
            juce::File tmp (folder.getChildFile ("reference.wav"));
            (void) tmp;
            const auto facts = measureMix (ref, fs);
            std::printf ("reference: %.1f LUFS, %.1f dBTP, range %.1f LU, width %.1f dB\n", facts.integratedLufs, facts.truePeakDb, facts.probe.loudnessRangeLu, facts.probe.widthDb);
        }
    }
    std::printf ("%s\n", master->startMastering().toRawUTF8());
    runSeconds ((double) frames / fs);                    // the whole song once
    const auto f0 = master->getMixFacts();
    std::printf ("%s\n", master->finishMastering().toRawUTF8());
    const auto& f = master->getMixFacts();
    std::printf ("mix: %.0f s, %.1f LUFS, %.1f dBTP, range %.1f LU, crest %.1f dB, punch %.1f dB, correlation %.2f (lowest %.2f), width %.1f dB (low %.1f), mono loss %.1f dB, low swing %.1f / level swing %.1f dB, flat tops %d\n",
                 f.probe.seconds, f.integratedLufs, f.truePeakDb, f.probe.loudnessRangeLu, f.probe.crestDb, f.probe.punchDb, f.probe.correlation, f.probe.lowestCorrelation, f.probe.widthDb, f.probe.sideToMidDb[0],
                 f.probe.monoLossDb, f.probe.lowSwingDb, f.probe.levelSwingDb, f.probe.flatTops);
    (void) f0;
    printPart ("DETECTED", "detected"); printPart ("PROCESSING", "processing"); printPart ("LEFT ALONE", "left"); printPart ("NOTES", "notes");
    // keep playing until the level has settled and the master has been measured
    for (int guard = 0; guard < 40 && master->getMasterStage() != MUProcessor::MasterStage::Done; ++guard) runSeconds (5.0);
    std::printf ("stage %d, auto level %.1f dB, limiter %.1f dB, multiband low %.1f dB, clipper %.1f dB\n", (int) master->getMasterStage(), master->getMasterAutoTrimDb(), master->getMaximizer().getGainReductionDb(),
                 master->getMultiband().getReductionDb (0), master->getClipper().getReductionDb());
    printPart ("REPORT", "report");
    // one more time through the song, recorded
    pos = 0;
    rec = folder.getChildFile ("master-st.f32").createOutputStream();
    if (rec != nullptr) { rec->setPosition (0); rec->truncate(); }
    runSeconds ((double) frames / fs);
    rec.reset();
    return 0;
}
