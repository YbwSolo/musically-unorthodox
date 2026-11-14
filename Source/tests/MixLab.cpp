// Puts a real voice and a real backing track through real plugin instances (vocal, beat, master), does what a
// student would do on the Mix tab, and writes the results so the mix can be measured outside a host.
//   MUMixLab <voice.f32 mono> <beat.f32 stereo> <outFolder> [preset=<chain preset name>] [suggest=1]
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
    if (argc < 4) { std::printf ("usage: MUMixLab voice.f32 beat.f32 outFolder [preset=name] [suggest=1]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 512;
    juce::MemoryBlock voiceData, beatData;
    if (! juce::File (argv[1]).loadFileAsData (voiceData) || ! juce::File (argv[2]).loadFileAsData (beatData)) { std::printf ("cannot read the recordings\n"); return 2; }
    const auto* voice = (const float*) voiceData.getData(); const int voiceLen = (int) (voiceData.getSize() / sizeof (float));
    const auto* music = (const float*) beatData.getData();  const int musicLen = (int) (beatData.getSize() / (2 * sizeof (float)));
    juce::StringPairArray opt;
    for (int i = 4; i < argc; ++i) { const juce::String a (argv[i]); opt.set (a.upToFirstOccurrenceOf ("=", false, false), a.fromFirstOccurrenceOf ("=", false, false)); }
    const juce::File folder (argv[3]); folder.createDirectory();

    LabHead head;
    auto make = [&] (Role r)
    {
        auto p = std::make_unique<MUProcessor>();
        p->setPlayConfigDetails (2, 2, fs, block);
        p->prepareToPlay (fs, block);
        p->setPlayHead (&head);
        p->setParam (ids::role, (float) r);
        return p;
    };
    auto vocal = make (Role::Vocal), beat = make (Role::Beat), master = make (Role::Master);
    const float voiceGain = opt.containsKey ("voicegain") ? opt["voicegain"].getFloatValue() : 1.0f;

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> v (2, block), b (2, block), mix (2, block);
    juce::int64 pos = 0;
    std::unique_ptr<juce::FileOutputStream> recVoiceIn, recVoiceOut, recSum, recMix;
    auto runSeconds = [&] (double seconds)
    {
        const int blocks = (int) (seconds * fs / block);
        for (int n = 0; n < blocks; ++n)
        {
            for (int i = 0; i < block; ++i, ++pos)
            {
                const float vs = voice[(int) (pos % voiceLen)] * voiceGain; v.setSample (0, i, vs); v.setSample (1, i, vs);
                const int m = (int) (pos % musicLen); b.setSample (0, i, music[2 * m]); b.setSample (1, i, music[2 * m + 1]);
                if (recVoiceIn != nullptr) { recVoiceIn->writeFloat (vs); recSum->writeFloat (0.5f * (vs + vs + music[2 * m] + music[2 * m + 1])); }
            }
            vocal->processBlock (v, midi); beat->processBlock (b, midi);
            mix.clear();
            for (auto* s : { &v, &b }) for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, *s, c, 0, block);
            master->processBlock (mix, midi);
            if (recVoiceOut != nullptr)
                for (int i = 0; i < block; ++i) { recVoiceOut->writeFloat (0.5f * (v.getSample (0, i) + v.getSample (1, i))); recMix->writeFloat (0.5f * (mix.getSample (0, i) + mix.getSample (1, i))); }
            head.seconds += (double) block / fs;
            for (auto* p : { vocal.get(), beat.get(), master.get() }) p->pumpAnalysis();
            if (n % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    };
    auto waitFor = [&] (double wallSeconds) { const double until = juce::Time::getMillisecondCounterHiRes() + wallSeconds * 1000.0; while (juce::Time::getMillisecondCounterHiRes() < until) runSeconds (1.0); };

    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    runSeconds (10.0);
    if (opt.containsKey ("preset"))
    {
        const int index = findChainPreset (opt["preset"]);
        std::printf ("preset: %s\n", index >= 0 ? vocal->applyChainPreset (index).toRawUTF8() : "not found");
        runSeconds (4.0);
    }
    master->learnAndBalance (6.0f);
    waitFor (9.0);
    waitFor (7.0);
    for (int round = 0; round < 3; ++round)
    {
        auto hot = master->getBus().findHotPeaks();
        if (hot.empty()) break;
        std::printf ("to fix: %s\n", master->getBus().tamePeaks (hot.front().slot).toRawUTF8());
        waitFor (7.5);
    }
    if (opt["suggest"] == "1")
    {
        juce::String status;
        for (auto& s : vocal->analyse (status)) std::printf ("suggestion applied: %s -> %s\n", s.title.toRawUTF8(), vocal->applySuggestion (s).toRawUTF8());
        runSeconds (3.0);
    }
    std::printf ("vocal: trim %.1f dB, comp %s style %d thr %.1f ratio %.1f att %.0f rel %.0f, eq %s, sat %s, rider %s, reverb %s, guard %s\n", vocal->getTrimDb(),
                 vocal->getParam (ids::compEnabled) > 0.5f ? "on" : "off", (int) vocal->getParam (ids::compStyle), vocal->getParam (ids::compThreshold), vocal->getParam (ids::compRatio),
                 vocal->getParam (ids::compAttack), vocal->getParam (ids::compRelease), vocal->getParam (ids::eqEnabled) > 0.5f ? "on" : "off", vocal->getParam (ids::satOn) > 0.5f ? "on" : "off",
                 vocal->getParam (ids::riderOn) > 0.5f ? "on" : "off", vocal->getParam (ids::rvOn) > 0.5f ? "on" : "off", vocal->getParam (ids::trackGuard) > 0.5f ? "on" : "off");
    std::printf ("beat: trim %.1f dB; master: trim %.1f dB\n", beat->getTrimDb(), master->getTrimDb());

    pos = 0;
    runSeconds (3.0);
    pos = 0;
    recVoiceIn = std::make_unique<juce::FileOutputStream> (folder.getChildFile ("voice-in.f32")); recVoiceOut = std::make_unique<juce::FileOutputStream> (folder.getChildFile ("voice-out.f32"));
    recSum = std::make_unique<juce::FileOutputStream> (folder.getChildFile ("sum-in.f32")); recMix = std::make_unique<juce::FileOutputStream> (folder.getChildFile ("mix-out.f32"));
    for (auto* s : { recVoiceIn.get(), recVoiceOut.get(), recSum.get(), recMix.get() }) { s->setPosition (0); s->truncate(); }
    runSeconds (juce::jmin (30.0, (double) voiceLen / fs));
    std::printf ("vocal compressor reduction now %.1f dB\n", vocal->getCompressor().getGainReductionDb());
    return 0;
}
