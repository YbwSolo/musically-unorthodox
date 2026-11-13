// Offline multi-instance harness: vocal, beat and 808 instances plus a master, fed with synthetic stems and
// driven through the real SessionBus: balancing, presets, Mix Brain, reference match, A/B and mastering.
// Build target: MUSession.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "PluginProcessor.h"
#include "SyntheticStems.h"

using namespace mu;

static int failures = 0;
static void check (bool ok, const char* name, const juce::String& detail = {})
{
    std::printf ("%s  %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.isEmpty() ? "" : "  -- ", detail.toRawUTF8());
    if (! ok) ++failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 512;

    auto make = [&] (Role r)
    {
        auto p = std::make_unique<MUProcessor>();
        p->setPlayConfigDetails (2, 2, fs, block);
        p->prepareToPlay (fs, block);
        p->setParam (ids::role, (float) r);
        return p;
    };
    auto vocal = make (Role::Vocal), beat = make (Role::Beat), bass = make (Role::Bass808), master = make (Role::Master);
    synth::Vocal vocalSrc; synth::Beat beatSrc; synth::Bass808 bassSrc;
    vocalSrc.prepare ((float) fs); beatSrc.prepare ((float) fs); bassSrc.prepare ((float) fs);
    float vocalGain = 0.3f, beatGain = 0.5f, bassGain = 0.12f;   // deliberately unbalanced: quiet vocal, loud beat, quiet 808

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> v (2, block), b (2, block), k (2, block), mix (2, block);
    auto runSeconds = [&] (double seconds)
    {
        const int blocks = (int) (seconds * fs / block);
        for (int n = 0; n < blocks; ++n)
        {
            for (int i = 0; i < block; ++i)
            {
                const float vs = vocalSrc.next() * vocalGain; v.setSample (0, i, vs); v.setSample (1, i, vs);
                float l, r; beatSrc.next (l, r); b.setSample (0, i, l * beatGain); b.setSample (1, i, r * beatGain);
                const float ks = bassSrc.next() * bassGain; k.setSample (0, i, ks); k.setSample (1, i, ks);
            }
            vocal->processBlock (v, midi); beat->processBlock (b, midi); bass->processBlock (k, midi);
            mix.clear();
            for (auto* s : { &v, &b, &k }) for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, *s, c, 0, block);
            master->processBlock (mix, midi);
            for (auto* p : { vocal.get(), beat.get(), bass.get(), master.get() }) p->pumpAnalysis();   // audio runs faster than real time here
            if (n % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);   // let timers tick
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    };
    auto remeasure = [&] { for (auto* p : { vocal.get(), beat.get(), bass.get(), master.get() }) p->resetMeters(); };

    // ---- session plumbing
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    check (master->getBus().getNumInstances() == 4, "Four instances on the bus");
    check (master->getBus().getMasterSlot() == master->getSessionSlot(), "Master slot found");
    check (vocal->getLatencySamples() > 0 && vocal->getLatencySamples() < 96, "Track instances stay low-latency for recording", juce::String (vocal->getLatencySamples()) + " samples (" + juce::String (1000.0 * vocal->getLatencySamples() / fs, 1) + " ms)");
    check (master->getLatencySamples() > vocal->getLatencySamples(), "Only the master carries the limiter look-ahead", juce::String (master->getLatencySamples()) + " samples (" + juce::String (1000.0 * master->getLatencySamples() / fs, 1) + " ms)");

    // ---- Mix Brain refuses to guess before it has heard anything
    {
        juce::String status;
        check (vocal->analyse (status).empty() && status.containsIgnoreCase ("play"), "Analyse asks for playback before it has heard the track", status);
    }

    runSeconds (8.0);
    auto tracks = master->getBus().collectTracks();
    check (tracks.size() == 4, "collectTracks sees everyone", juce::String ((int) tracks.size()));
    for (auto& t : tracks)
        std::printf ("   %-14s role=%-14s level=%7.2f LUFS  tp=%6.2f  active=%d bands=%d\n", t.name.toRawUTF8(), roleNames()[(int) t.role].toRawUTF8(), t.levelLufs, t.truePeakDb, (int) t.active, (int) t.hasBands);

    // ---- balance
    auto plan = master->getBus().computePlan (master->currentTargets());
    check (plan.usable, "Plan usable", plan.summary);
    master->balanceNow();
    std::printf ("   trims: vocal %.2f  beat %.2f  808 %.2f\n", vocal->getTrimDb(), beat->getTrimDb(), bass->getTrimDb());
    check (beat->getTrimDb() < vocal->getTrimDb(), "Beat trimmed relative to the vocal");
    check (bass->getTrimDb() > beat->getTrimDb(), "808 lifted relative to the beat");
    check (std::abs (master->getTrimDb()) < 0.01f, "Master trim untouched");
    runSeconds (0.5);      // the trims glide, then balancing restarts every measurement by itself
    runSeconds (8.0);
    const float vL = vocal->getLoudness().getIntegrated(), bL = beat->getLoudness().getIntegrated(), mixL = master->getMixMeter().getIntegrated();
    std::printf ("   after: vocal %.2f  beat %.2f  808 %.2f  mix %.2f LUFS, mix peak %.2f dBTP\n", vL, bL, bass->getLoudness().getIntegrated(), mixL, master->getMixMeter().getTruePeakHold());
    check (std::abs ((bL - vL) - (-1.5f)) < 1.0f, "Beat sits about 1.5 LU under the vocal", juce::String (bL - vL, 2));
    check (mixL <= -18.0f + 1.0f && mixL > -30.0f, "Mix lands at or just under -18 LUFS", juce::String (mixL, 2));
    check (master->getMixMeter().getTruePeakHold() <= -6.0f + 1.0f, "Mix keeps its headroom", juce::String (master->getMixMeter().getTruePeakHold(), 2) + " dBTP");

    // ---- lock
    master->getBus().setTrackLocked (beat->getSessionSlot(), true);
    check (! beat->acceptsBalance(), "A track can be locked from the session view");
    master->getBus().setTrackLocked (beat->getSessionSlot(), false);

    // ---- presets
    {
        const auto& presets = chainPresets();
        bool idsOk = true; juce::String bad;
        for (auto& p : presets)
            for (auto& pv : p.values)
                if (vocal->apvts.getParameter (pv.id) == nullptr) { idsOk = false; bad = p.name + " / " + pv.id; }
        check (idsOk, "Every preset only refers to real parameters", bad);
        check (presets.size() >= 28, "Preset library size", juce::String ((int) presets.size()) + " presets in " + juce::String (chainPresetCategories().size()) + " categories");
        juce::StringArray names;
        for (auto& p : presets) names.addIfNotAlreadyThere (p.name);
        check (names.size() == (int) presets.size(), "Preset names are unique");

        const auto msg = vocal->applyChainPreset (findChainPreset ("Rap vocal - punchy"));
        check (msg.contains ("fitted"), "Vocal chain fits its thresholds to the measured level", msg);
        check (vocal->getParam (ids::compEnabled) > 0.5f && vocal->getParam (ids::dsOn) > 0.5f && vocal->getParam (ids::riderOn) > 0.5f && (int) vocal->getParam (ids::compStyle) == 1, "Vocal chain switches its modules on");
        check (vocal->getCurrentPresetName() == "Rap vocal - punchy", "Current preset is remembered");
        beat->applyChainPreset (findChainPreset ("Beat - make room for the vocal"));
        check (beat->getParam (ids::vsEnabled) > 0.5f, "Beat chain turns Vocal space on");
        bass->applyChainPreset (findChainPreset ("808 - clean sub"));
        check (bass->getParam (ids::stMonoBass) > 0.5f && bass->getParam (ids::satOn) > 0.5f, "808 chain makes the sub mono and adds harmonics");

        remeasure();
        runSeconds (6.0);
        const float gr = vocal->getCompressor().reductionHistory().get (0);
        float worst = 0.0f;
        for (int i = 0; i < 400; ++i) worst = juce::jmin (worst, vocal->getCompressor().reductionHistory().get (i));
        check (worst < -2.0f && worst > -14.0f, "Fitted vocal compressor works in a sensible range on the loud words", juce::String (worst, 1) + " dB at most (now " + juce::String (gr, 1) + ")");
        float dynMove = 0.0f;
        for (int band = 0; band < kNumEQBands; ++band) dynMove = juce::jmin (dynMove, vocal->getEQ().getBandDynamicOffsetDb (band));
        std::printf ("   vocal chain: comp worst %.1f dB, de-esser %.1f dB, rider %+.1f dB, dynamic EQ %.1f dB\n", worst, vocal->getDeEsser().getGainReductionDb(), vocal->getRider().getCurrentGainDb(), dynMove);
        check (vocal->getVocalSpace().getCurrentDepth() == 0.0f, "Vocal space never runs on the vocal itself");
    }

    // ---- Mix Brain
    {
        vocal->applyChainPreset (findChainPreset ("Clean slate"));
        vocal->setParam (ids::role, (float) Role::Vocal);
        beat->applyChainPreset (findChainPreset ("Clean slate"));
        beat->setParam (ids::role, (float) Role::Beat);
        runSeconds (8.0);
        juce::String status;
        auto found = vocal->analyse (status);
        std::printf ("   vocal analysis: %s\n", status.toRawUTF8());
        for (auto& s : found) std::printf ("      - %s  =>  %s\n", s.title.toRawUTF8(), s.action.toRawUTF8());
        check (! found.empty(), "Mix Brain finds something to say about the synthetic vocal");
        bool allExplain = true;
        for (auto& s : found) allExplain = allExplain && s.why.length() > 30 && s.action.isNotEmpty();
        check (allExplain, "Every suggestion carries a reason and an action");

        juce::String beatStatus;
        auto beatFound = beat->analyse (beatStatus);
        for (auto& s : beatFound) std::printf ("      [beat] %s  =>  %s\n", s.title.toRawUTF8(), s.action.toRawUTF8());
        bool clash = false;
        for (auto& s : beatFound) if (s.kind == Suggestion::Kind::Masking) clash = true;
        check (clash, "The beat is told it is covering the vocal");

        const Suggestion* own = nullptr;
        for (auto& s : found) if (s.kind != Suggestion::Kind::Masking) { own = &s; break; }
        if (own != nullptr)
        {
            int before = 0, after = 0;
            for (int band = 0; band < kNumEQBands; ++band) before += vocal->getParam (ids::eqBandEnabled (band)) > 0.5f ? 1 : 0;
            const auto result = vocal->applySuggestion (*own);
            for (int band = 0; band < kNumEQBands; ++band) after += vocal->getParam (ids::eqBandEnabled (band)) > 0.5f ? 1 : 0;
            const bool changed = after > before || vocal->getParam (ids::dsOn) > 0.5f || vocal->getParam (ids::satOn) > 0.5f;
            check (changed && result.isNotEmpty(), "Applying a suggestion changes the chain", result);
        }
        for (auto& s : beatFound)
            if (s.kind == Suggestion::Kind::Masking)
            {
                beat->applySuggestion (s);
                bool dyn = false;
                for (int band = 0; band < kNumEQBands; ++band) dyn = dyn || (beat->getParam (ids::eqBandEnabled (band)) > 0.5f && beat->getParam (ids::eqBandDyn (band)) > 0.5f);
                check (dyn, "Fixing a collision adds a dynamic cut to the track that gives way");
                break;
            }
    }

    // ---- reference match
    {
        check (master->matchReference (0.8f).containsIgnoreCase ("No reference"), "Match explains itself when there is no reference");
        const auto cap = beat->captureReference();
        juce::String name;
        check (master->hasReference (name) && name.isNotEmpty(), "A reference captured on one instance is available to the others", cap);
        runSeconds (6.0);     // applying suggestions restarted the vocal's tone measurement
        const auto res = vocal->matchReference (0.8f);
        check (res.startsWith ("Matched") || res.startsWith ("Your tone"), "Match builds ordinary EQ bands", res);
    }

    // ---- A/B
    {
        vocal->setParam (ids::compRatio, 2.0f);
        vocal->switchAB();                       // now on B, which starts as a copy of A
        vocal->setParam (ids::compRatio, 8.0f);
        vocal->switchAB();                       // back to A
        const float a = vocal->getParam (ids::compRatio);
        vocal->switchAB();
        const float bb = vocal->getParam (ids::compRatio);
        check (std::abs (a - 2.0f) < 0.05f && std::abs (bb - 8.0f) < 0.05f, "A/B keeps two separate sets of settings", "A " + juce::String (a, 1) + ":1, B " + juce::String (bb, 1) + ":1");
        check ((int) vocal->getParam (ids::role) == (int) Role::Vocal, "A/B leaves the track role alone");
        vocal->switchAB();
    }

    // ---- delta listening: a flat EQ changes nothing, so its delta is silence
    {
        vocal->resetEQ();
        vocal->setListenMode (Listen::EqDelta);
        runSeconds (1.0);
        check (vocal->getLoudness().getMomentary() < -70.0f, "EQ delta of a flat EQ is silence", juce::String (vocal->getLoudness().getMomentary(), 1));
        vocal->setListenMode (Listen::Normal);
        runSeconds (1.0);
        check (vocal->getLoudness().getMomentary() > -60.0f, "Normal listening returns after delta");
    }

    // ---- mastering
    {
        master->applyChainPreset (findChainPreset ("Streaming master"));
        check (master->getParam (ids::peakGuard) > 0.5f && master->getParam (ids::masterAutoTrim) > 0.5f && std::abs (master->getParam (ids::targetLufs) + 14.0f) < 0.01f, "Streaming master arms the limiter and auto level at -14 LUFS");
        const float vTrim = vocal->getTrimDb();
        master->setAutoBalanceMode (true);
        remeasure();
        runSeconds (20.0);
        master->setAutoBalanceMode (false);
        const float integ = master->getLoudness().getIntegrated(), tp = master->getLoudness().getTruePeakHold();
        check (std::abs (integ + 14.0f) < 1.0f, "Streaming master lands on -14 LUFS", juce::String (integ, 2) + " LUFS (auto level " + juce::String (master->getMasterAutoTrimDb(), 1) + " dB)");
        check (tp <= -1.0f + 0.1f, "Streaming master stays under -1 dBTP", juce::String (tp, 2) + " dBTP");
        check (std::abs (vocal->getTrimDb() - vTrim) < 2.5f, "Mastering gain does not drag the track balance around", "vocal trim moved " + juce::String (vocal->getTrimDb() - vTrim, 2) + " dB with Keep balanced on");

        master->applyMasterMode (2);
        remeasure();
        runSeconds (20.0);
        const float loud = master->getLoudness().getIntegrated(), loudTp = master->getLoudness().getTruePeakHold();
        float worst = 0.0f;
        for (int i = 0; i < 500; ++i) worst = juce::jmin (worst, master->getMaximizer().reductionHistory().get (i));
        check (std::abs (loud + 9.0f) < 1.2f || master->isAutoLevelHeld(), "Loud master reaches -9 LUFS, or holds rather than crushing the mix", juce::String (loud, 2) + " LUFS, limiter up to " + juce::String (-worst, 1) + " dB" + (master->isAutoLevelHeld() ? ", held" : ""));
        check (worst > -9.5f, "Auto level never drives the limiter past a sensible amount", juce::String (-worst, 1) + " dB");
        check (loudTp <= -2.0f + 0.1f, "Loud master stays under -2 dBTP", juce::String (loudTp, 2) + " dBTP");

        master->applyMasterMode (0);
        remeasure();
        runSeconds (16.0);
        check (std::abs (master->getLoudness().getIntegrated() + 18.0f) < 1.0f, "Mix hand-off returns to -18 LUFS", juce::String (master->getLoudness().getIntegrated(), 2));
    }

    check (master->getBus().getVocalKeyLevel() >= 0.0f, "Vocal key level available");

    // ---- state round trip
    {
        juce::MemoryBlock state;
        master->setAutoBalanceMode (true);
        master->getStateInformation (state);
        auto copy = std::make_unique<MUProcessor>();
        copy->setStateInformation (state.getData(), (int) state.getSize());
        check ((int) copy->getParam (ids::role) == (int) Role::Master, "State restores role");
        check (copy->getAutoBalanceMode(), "State restores keep-balanced mode");
        check (copy->getCurrentPresetName() == master->getCurrentPresetName(), "State restores the preset name", copy->getCurrentPresetName());
        check (std::abs (copy->getParam (ids::targetLufs) - master->getParam (ids::targetLufs)) < 0.01f, "State restores the loudness target");
        master->setAutoBalanceMode (false);
    }

    // ---- the editor builds, runs and closes cleanly on every tab (needs a display)
    if (juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isNotEmpty() || juce::SystemStats::getOperatingSystemType() != juce::SystemStats::Linux)
    {
        for (int tab = 0; tab < 6; ++tab)
        {
            vocal->setCurrentTab (tab);
            std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
            ed->setVisible (true);
            runSeconds (0.4);
            ed.reset();
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (master->createEditor());
        runSeconds (0.4);
        ed.reset();
        check (true, "Editor opens and closes on every tab");
    }

    // ---- tear down in a mixed order to exercise slot reuse
    beat.reset();
    check (master->getBus().getNumInstances() == 3, "Instance unregistered");
    vocal.reset(); bass.reset(); master.reset();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "SESSION HARNESS PASSED" : "SESSION HARNESS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
