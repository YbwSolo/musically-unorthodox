// Offline multi-instance harness: vocal, beat and 808 instances plus a master, fed with synthetic stems and
// driven through the real SessionBus: balancing, presets, Mix Brain, reference match, A/B and mastering.
// Build target: MUSession.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "PluginProcessor.h"
#include "ai/TutorKnowledge.h"
#include "SyntheticStems.h"
#include <unistd.h>

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

    // ---- in-plugin updates, against a local feed
    {
        UpdateInfo parsed;
        const juce::String goodSha = juce::String::repeatedString ("ab", 32);
        check (Updater::parseFeed ("{\"build\": 7, \"version\": \"0.4.7\", \"file\": \"MusicallyUnorthodox-bundles.zip\", \"sha256\": \"" + goodSha + "\", \"notes\": \"n\"}", parsed)
               && parsed.build == 7 && parsed.version == "0.4.7", "Update feed is read");
        check (! Updater::parseFeed ("{\"build\": 7, \"file\": \"../evil.zip\", \"sha256\": \"" + goodSha + "\"}", parsed)
               && ! Updater::parseFeed ("{\"build\": 7, \"file\": \"https://elsewhere.example/x.zip\", \"sha256\": \"" + goodSha + "\"}", parsed)
               && ! Updater::parseFeed ("not json", parsed) && ! Updater::parseFeed ("{\"build\": 7, \"file\": \"a.zip\", \"sha256\": \"short\"}", parsed),
               "Update feed rejects paths, other sites, bad checksums and junk");

        auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-updater-test");
        root.deleteRecursively();
        const auto feed = root.getChildFile ("feed"), stage = root.getChildFile ("stage"), installed = root.getChildFile ("installed");
        feed.createDirectory();
        auto makeBundle = [] (const juce::File& bundle, const juce::String& binaryText, const juce::String& extraFile)
        {
            bundle.getChildFile ("Contents/MacOS").createDirectory();
            bundle.getChildFile ("Contents/Resources").createDirectory();
            const auto bin = bundle.getChildFile ("Contents/MacOS/Musically Unorthodox");
            bin.replaceWithText (binaryText); bin.setExecutePermission (true);
            bundle.getChildFile ("Contents/Info.plist").replaceWithText ("plist " + binaryText);
            bundle.getChildFile ("Contents/Resources").getChildFile (extraFile).replaceWithText (extraFile);
        };
        makeBundle (stage.getChildFile ("Components/Musically Unorthodox.component"), "new au binary", "new.txt");
        makeBundle (stage.getChildFile ("VST3/Musically Unorthodox.vst3"), "new vst3 binary", "new.txt");
        const auto au = installed.getChildFile ("Musically Unorthodox.component"), vst3 = installed.getChildFile ("Musically Unorthodox.vst3");
        makeBundle (au, "old au binary", "stale.txt");
        makeBundle (vst3, "old vst3 binary", "stale.txt");
        juce::ChildProcess zipper;
        const auto zip = feed.getChildFile ("MusicallyUnorthodox-bundles.zip");
        const bool zipped = zipper.start (juce::StringArray { "sh", "-c", "cd '" + stage.getFullPathName() + "' && zip -qry '" + zip.getFullPathName() + "' Components VST3" })
                            && zipper.waitForProcessToFinish (20000) && zip.existsAsFile();
        check (zipped, "Test update package built");
        auto writeFeed = [&] (int build, const juce::String& sha)
        {
            feed.getChildFile ("latest.json").replaceWithText ("{ \"build\": " + juce::String (build) + ", \"version\": \"0.4." + juce::String (build)
                                                               + "\", \"file\": \"MusicallyUnorthodox-bundles.zip\", \"sha256\": \"" + sha + "\", \"notes\": \"Test notes\" }");
        };
        auto wait = [] (Updater& u, std::initializer_list<UpdateState::Phase> busy)
        {
            for (int i = 0; i < 600; ++i)
            {
                const auto ph = u.getState().phase;
                bool still = false;
                for (auto b : busy) still = still || ph == b;
                if (! still) break;
                juce::Thread::sleep (20);
            }
            return u.getState();
        };
        using Phase = UpdateState::Phase;
        const auto feedUrl = "file://" + feed.getFullPathName();

        writeFeed (7, Updater::sha256Of (zip));
        {
            Updater u;
            u.configureForTesting (feedUrl, au, vst3, 5);
            u.checkNow();
            auto st = wait (u, { Phase::Checking });
            check (st.phase == Phase::Available && st.latest.build == 7 && st.latest.notes == "Test notes", "A newer build is found", st.message);
            u.installNow();
            st = wait (u, { Phase::Downloading, Phase::Installing, Phase::Available });
            check (st.phase == Phase::Installed, "The update downloads, verifies and installs", st.message);
            const auto bin = au.getChildFile ("Contents/MacOS/Musically Unorthodox");
            check (bin.loadFileAsString() == "new au binary" && vst3.getChildFile ("Contents/MacOS/Musically Unorthodox").loadFileAsString() == "new vst3 binary", "Both plugin bundles are replaced in place");
            check (::access (bin.getFullPathName().toRawUTF8(), X_OK) == 0, "The new binary keeps its executable permission");
            check (! au.getChildFile ("Contents/Resources/stale.txt").exists() && au.getChildFile ("Contents/Resources/new.txt").existsAsFile(), "Files the new version dropped are removed");
            bool leftovers = false;
            for (const auto& e : juce::RangedDirectoryIterator (installed, true)) leftovers = leftovers || e.getFile().getFileName().endsWith (".mu-new");
            check (! leftovers, "No temporary files are left behind");
        }
        {
            Updater u;
            u.configureForTesting (feedUrl, au, vst3, 7);
            u.checkNow();
            check (wait (u, { Phase::Checking }).phase == Phase::UpToDate, "The same build reports that it is up to date");
        }
        {
            makeBundle (au, "old au binary", "stale.txt");
            writeFeed (8, juce::String::repeatedString ("0", 64));
            Updater u;
            u.configureForTesting (feedUrl, au, vst3, 5);
            u.checkNow();
            wait (u, { Phase::Checking });
            u.installNow();
            const auto st = wait (u, { Phase::Downloading, Phase::Installing, Phase::Available });
            check (st.phase == Phase::Failed && au.getChildFile ("Contents/MacOS/Musically Unorthodox").loadFileAsString() == "old au binary",
                   "A download that does not match its checksum is refused and nothing is touched", st.message);
        }
        {
            Updater u;
            u.configureForTesting ("file://" + root.getChildFile ("nowhere").getFullPathName(), au, vst3, 5);
            u.checkNow();
            const auto st = wait (u, { Phase::Checking });
            check (st.phase == Phase::Failed, "An unreachable feed fails politely", st.message);
        }
        // the real, published feed (only when asked for: it needs the internet)
        if (juce::SystemStats::getEnvironmentVariable ("MU_TEST_REAL_FEED", {}).isNotEmpty())
        {
            makeBundle (au, "old au binary", "stale.txt");
            makeBundle (vst3, "old vst3 binary", "stale.txt");
            Updater u;
            u.configureForTesting (MU_UPDATE_BASE_URL, au, vst3, 1);
            u.checkNow();
            auto st = wait (u, { Phase::Checking });
            check (st.phase == Phase::Available && st.latest.build >= 6, "The published update feed is reachable", "build " + juce::String (st.latest.build) + " " + st.message);
            u.installNow();
            for (int i = 0; i < 3000 && (u.getState().phase == Phase::Downloading || u.getState().phase == Phase::Installing || u.getState().phase == Phase::Available); ++i) juce::Thread::sleep (20);
            st = u.getState();
            const auto bin = au.getChildFile ("Contents/MacOS/Musically Unorthodox");
            check (st.phase == Phase::Installed && bin.getSize() > 1000000 && au.getChildFile ("Contents/Info.plist").existsAsFile()
                   && vst3.getChildFile ("Contents/MacOS/Musically Unorthodox").getSize() > 1000000 && ! au.getChildFile ("Contents/Resources/stale.txt").exists(),
                   "The published update downloads, passes its checksum and installs", st.message + " (" + juce::String (bin.getSize() / 1000000) + " MB binary)");
        }
        root.deleteRecursively();
    }

    // ---- AI tutor plumbing (against tests/mock_ollama.py when MU_TEST_AI_URL is set)
    {
        check (LocalAI::chooseModel ({ "nomic-embed-text:latest", "mistral:7b", "llama3.2:latest" }, {}) == "llama3.2:latest", "Tutor prefers a small chat model and skips embedding models");
        check (LocalAI::chooseModel ({ "mistral:7b", "llama3.2:latest" }, "mistral:7b") == "mistral:7b" && LocalAI::chooseModel ({}, {}).isEmpty(), "Tutor honours a chosen model and copes with none");
        check (LocalAI::parseAnswer ("{\"message\":{\"role\":\"assistant\",\"content\":\"<think>hmm</think>**Cut** the mud.\"}}") == "Cut the mud.", "Tutor answers are cleaned of reasoning tags and markdown");
        const auto request = LocalAI::buildRequest ("llama3.2", "sys", "why \"muddy\"?");
        const auto parsedRequest = juce::JSON::parse (request);
        check (parsedRequest.isObject() && ! (bool) parsedRequest.getProperty ("stream", true) && parsedRequest["messages"].size() == 2, "Tutor request is valid JSON with streaming off");

        const auto prompt = vocal->tutorPrompt ("Why does my vocal sound muddy?");
        const auto trackNotes = vocal->tutorTrackNotes();
        const auto dumpTo = juce::SystemStats::getEnvironmentVariable ("MU_DUMP_TUTOR_PROMPT", {});
        if (dumpTo.isNotEmpty()) juce::File (dumpTo).replaceWithText (trackNotes);
        check (trackNotes.contains ("Lead vocal") && trackNotes.contains ("LUFS") && trackNotes.contains ("THE WHOLE SESSION") && trackNotes.contains ("tracks have the plugin"),
               "Tutor is told about this track and the session around it", juce::String (trackNotes.length()) + " characters");
        check (prompt.contains ("NOTES FOR THIS QUESTION") && prompt.contains ("Mud is a build-up") && prompt.lastIndexOf ("NOTES FOR THIS QUESTION") < prompt.lastIndexOf ("THIS TRACK\n"),
               "Tutor is handed the guide notes that match the question");
        {
            // the right note must come first for the questions students actually ask
            const std::pair<const char*, const char*> expect[] = {
                { "Why does my vocal sound muddy?", "Muddy sound" },                 { "My S sounds are really sharp", "Sharp S sounds" },
                { "The beat is drowning out my vocal", "Vocal gets lost in the beat" }, { "my 808 disappears on phone speakers", "Bass or 808 missing on small speakers" },
                { "How loud should my song be for Spotify?", "How loud a finished song should be" }, { "the mix is pumping", "Pumping and over-compression" },
                { "how do I add reverb", "Reverb and delay" },                       { "my kick and 808 are fighting", "Kick and 808 fighting" },
                { "how do I make my drums punchier", "Punchier drums" },             { "can't hear my vocal over the beat", "Vocal gets lost in the beat" },
                { "what should I do first, I'm a beginner", "Where to start" },      { "my adlibs are too loud", "Backing vocals and adlibs" } };
            juce::String wrong;
            for (auto& e : expect)
            {
                const auto titles = tutor::noteTitlesFor (e.first);
                if (titles.isEmpty() || titles[0] != e.second) wrong << e.first << " -> " << (titles.isEmpty() ? juce::String ("nothing") : titles[0]) << "; ";
            }
            check (wrong.isEmpty(), "The guide finds the right note for common questions", wrong);
            const auto knee = tutor::noteTitlesFor ("what does the knee do?");
            check (knee.contains ("Knee (Dynamics tab)"), "The guide finds the plugin's own help for a named control", knee.joinIntoString (", "));
            check (tutor::notesFor ("hello there").isEmpty() && tutor::numMixingNotes() >= 30, "The guide stays out of the way when nothing matches");
            const auto request = LocalAI::buildChatRequest ("sys", "why?", { { "what is mud?", "Too much 200 to 400 Hz." } });
            check (juce::JSON::parse (request)["messages"].size() == 4, "A follow-up question is sent with the exchange before it");
        }

        const auto address = juce::SystemStats::getEnvironmentVariable ("MU_TEST_AI_URL", {});
        if (address.isNotEmpty())
        {
            LocalAI ai;
            ai.setAddressForTesting (address);
            ai.refresh();
            for (int i = 0; i < 200 && ! ai.getStatus().available; ++i) juce::Thread::sleep (20);
            check (ai.getStatus().available && ai.getStatus().model == "llama3.2:latest", "Tutor finds the local model", ai.getStatus().model);
            juce::Thread::sleep (50);
            ai.ask (prompt, "Why does my vocal sound muddy?");
            for (int i = 0; i < 400 && ai.getStatus().answerId == 0; ++i) juce::Thread::sleep (20);
            const auto st = ai.getStatus();
            check (st.answer.startsWith ("Mock answer to: Why does my vocal sound muddy?") && ! st.busy, "Tutor question goes out and the answer comes back", st.answer + st.error);
        }
        LocalAI absent;
        absent.setAddressForTesting ("http://127.0.0.1:9");
        absent.refresh();
        juce::Thread::sleep (400);
        for (int i = 0; i < 150 && absent.getStatus().available; ++i) juce::Thread::sleep (20);
        check (! absent.getStatus().available, "Tutor stays off when no local AI is running");

        // ---- built-in tutor: read the feed, install from a local feed, start the helper, ask, remove
        {
            TutorPack tp;
            const juce::String sha = juce::String::repeatedString ("cd", 32);
            auto feedText = [&] (const juce::String& file, const juce::String& url)
            {
                return "{ \"build\": 7, \"tutor\": { \"version\": \"t1\", \"file\": \"" + file + "\", \"sha256\": \"" + sha + "\", \"model\": { \"name\": \"Test model\", \"url\": \""
                       + url + "\", \"sha256\": \"" + sha + "\", \"bytes\": 1000 } } }";
            };
            check (LocalAI::parseTutorFeed (feedText ("tutor.zip", "https://huggingface.co/a/b/resolve/main/m.gguf"), tp) && tp.modelBytes == 1000 && tp.modelName == "Test model", "Tutor feed is read");
            check (! LocalAI::parseTutorFeed (feedText ("../tutor.zip", "https://huggingface.co/a/m.gguf"), tp)
                   && ! LocalAI::parseTutorFeed (feedText ("tutor.zip", "https://elsewhere.example/m.gguf"), tp)
                   && ! LocalAI::parseTutorFeed ("{ \"build\": 7 }", tp), "Tutor feed rejects paths, other model hosts and feeds without a tutor");
            check (LocalAI::parseAnswer ("{\"choices\":[{\"message\":{\"content\":\"### Tip\\n* Cut 300 Hz\"}}]}") == "Tip\n- Cut 300 Hz", "Built-in tutor answers are read and cleaned");
        }
        const juce::File helperScript (juce::SystemStats::getEnvironmentVariable ("MU_TEST_TUTOR_HELPER", {}));
        if (helperScript.existsAsFile())
        {
            auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-tutor-test");
            root.deleteRecursively();
            const auto feed = root.getChildFile ("feed"), stage = root.getChildFile ("stage");
            feed.createDirectory();
            const auto program = stage.getChildFile ("tutor/x64/llama-server");
            program.getParentDirectory().createDirectory();
            helperScript.copyFileTo (program);
            program.setExecutePermission (true);
            stage.getChildFile ("tutor/VERSION").replaceWithText ("t1");
            const auto zip = feed.getChildFile ("tutor.zip");
            juce::ChildProcess zipper;
            const bool zipped = zipper.start (juce::StringArray { "sh", "-c", "cd '" + stage.getFullPathName() + "' && zip -qry '" + zip.getFullPathName() + "' tutor" })
                                && zipper.waitForProcessToFinish (20000) && zip.existsAsFile();
            const auto model = feed.getChildFile ("model.gguf");
            {
                juce::FileOutputStream out (model);
                juce::Random rng (7);
                for (int i = 0; i < 100000; ++i) out.writeInt (rng.nextInt());
            }
            auto writeFeed = [&] (const juce::String& modelSha)
            {
                feed.getChildFile ("latest.json").replaceWithText ("{ \"build\": 7, \"tutor\": { \"version\": \"t1\", \"file\": \"tutor.zip\", \"sha256\": \"" + LocalAI::sha256Of (zip)
                    + "\", \"model\": { \"name\": \"Test model\", \"url\": \"file://" + model.getFullPathName() + "\", \"sha256\": \"" + modelSha + "\", \"bytes\": " + juce::String (model.getSize()) + " } } }");
            };
            auto settle = [] (LocalAI& a) { for (int i = 0; i < 1500 && a.getStatus().setup == LocalAI::Setup::Downloading; ++i) juce::Thread::sleep (20); juce::Thread::sleep (150); return a.getStatus(); };
            const auto feedUrl = "file://" + feed.getFullPathName();
            check (zipped, "Test tutor package built");

            {
                writeFeed (juce::String::repeatedString ("0", 64));
                LocalAI bad;
                bad.setAddressForTesting ("http://127.0.0.1:9");
                bad.configureForTesting (feedUrl, root.getChildFile ("home-bad"));
                bad.installBuiltIn();
                const auto st = settle (bad);
                check (st.setup == LocalAI::Setup::Failed && ! st.available, "A tutor download that does not match its checksum is refused", st.setupMessage);
            }
            writeFeed (LocalAI::sha256Of (model));
            {
                const auto home = root.getChildFile ("home");
                home.createDirectory();
                // a download that was cut off earlier
                home.getChildFile ("model-" + LocalAI::sha256Of (model).substring (0, 12) + ".gguf.part").replaceWithData ("half", 4);
                LocalAI ai;
                ai.setAddressForTesting ("http://127.0.0.1:9");
                ai.configureForTesting (feedUrl, home);
                ai.refresh();
                juce::Thread::sleep (500);
                check (! ai.getStatus().available && ai.getStatus().setup == LocalAI::Setup::NotInstalled, "Built-in tutor starts switched off");
                ai.installBuiltIn();
                auto st = settle (ai);
                check (st.setup == LocalAI::Setup::Ready && st.available && st.builtIn && st.model == "Test model", "One press downloads and installs the built-in tutor", st.setupMessage);
                ai.ask (prompt, "Why is my vocal muddy?");
                for (int i = 0; i < 1500 && ai.getStatus().answerId == 0; ++i) juce::Thread::sleep (20);
                st = ai.getStatus();
                check (st.answer.startsWith ("Built-in answer\n- to: Why is my vocal muddy? (model model-") && st.answer.contains ("ctx 4096") && ! st.busy,
                       "The plugin starts the helper and gets an answer back", st.answer + st.error);
                ai.ask (prompt, "And the bass?");
                for (int i = 0; i < 1500 && ai.getStatus().answerId < 2; ++i) juce::Thread::sleep (20);
                check (ai.getStatus().answer.contains ("And the bass?"), "A second question reuses the running helper", ai.getStatus().answer + ai.getStatus().error);
                {
                    LocalAI second;                                   // another plugin process on the same computer
                    second.setAddressForTesting ("http://127.0.0.1:9");
                    second.configureForTesting (feedUrl, home);
                    second.refresh();
                    juce::Thread::sleep (400);
                    check (second.getStatus().available && second.getStatus().builtIn, "Other instances find the installed tutor");
                }
                ai.removeBuiltIn();
                juce::Thread::sleep (600);
                bool left = false;
                for (const auto& e : juce::RangedDirectoryIterator (home, true, "*", juce::File::findFiles)) { left = true; juce::ignoreUnused (e); }
                check (! ai.getStatus().available && ai.getStatus().setup == LocalAI::Setup::NotInstalled && ! left, "Removing the tutor deletes it from the disk");
            }
            root.deleteRecursively();
        }
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
