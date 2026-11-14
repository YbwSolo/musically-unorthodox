// Offline multi-instance harness: vocal, beat and 808 instances plus a master, fed with synthetic stems and
// driven through the real SessionBus: balancing, presets, Mix Brain, reference match, A/B and mastering.
// Build target: MUSession.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "PluginProcessor.h"
#include "state/UserPresets.h"
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

// The song position a host reports: 120 BPM, moving forward while "playing".
struct TestHead : juce::AudioPlayHead
{
    double seconds = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (120.0); p.setIsPlaying (playing); p.setTimeInSeconds (seconds); p.setTimeInSamples ((juce::int64) (seconds * 48000.0)); p.setPpqPosition (seconds * 2.0);
        return p;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 512;

    TestHead head;
    auto make = [&] (Role r)
    {
        auto p = std::make_unique<MUProcessor>();
        p->setPlayConfigDetails (2, 2, fs, block);
        p->prepareToPlay (fs, block);
        p->setPlayHead (&head);
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
            if (head.playing) head.seconds += (double) block / fs;
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
        check (vocal->getParam (ids::compEnabled) > 0.5f && vocal->getParam (ids::dsOn) > 0.5f && vocal->getParam (ids::riderOn) > 0.5f && (int) vocal->getParam (ids::compStyle) == 4, "Vocal chain switches its modules on");
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
        const auto facts = vocal->tutorFacts();
        if (dumpTo.isNotEmpty())
        {
            juce::File (dumpTo).replaceWithText (trackNotes);
            juce::String lines;
            for (auto& k : facts.getAllKeys()) lines << k << "=" << facts[k] << "\n";
            juce::File (dumpTo).getSiblingFile ("sample_tutor_facts.txt").replaceWithText (lines);
        }
        const auto balanceNotes = tutor::notesFor ("Is my mix balanced right now?", facts);
        check (facts["balance"].contains ("should come") && balanceNotes.contains (facts["balance"]) && ! balanceNotes.containsChar ('{')
               && facts["mixloudness"].contains ("LUFS") && facts["width"].contains ("correlation"),
               "Measurements are turned into finished sentences for the tutor", facts["balance"]);
        check (trackNotes.contains ("Lead vocal") && trackNotes.contains ("LUFS") && trackNotes.contains ("THE WHOLE SESSION") && trackNotes.contains ("tracks have the plugin"),
               "Tutor is told about this track and the session around it", juce::String (trackNotes.length()) + " characters");
        check (prompt.contains ("GUIDE NOTES FOR THIS QUESTION\n") && prompt.contains ("Mud is a build-up") && prompt.lastIndexOf ("GUIDE NOTES FOR THIS QUESTION\n") < prompt.lastIndexOf ("THIS TRACK\n"),
               "Tutor is handed the guide notes that match the question");
        {
            // the right note must come first for the questions students actually ask
            const std::pair<const char*, const char*> expect[] = {
                { "Why does my vocal sound muddy?", "Muddy sound" },                 { "My S sounds are really sharp", "Sharp S sounds" },
                { "The beat is drowning out my vocal", "Vocal gets lost in the beat" }, { "my 808 disappears on phone speakers", "Bass or 808 missing on small speakers" },
                { "How loud should my song be for Spotify?", "How loud a finished song should be" }, { "the mix is pumping", "Pumping and over-compression" },
                { "how do I add reverb", "Reverb" }, { "how do I get the autotune sound", "Tuning a vocal" }, { "make my hook sound stacked", "Doubles and stacks" },                       { "my kick and 808 are fighting", "Kick and 808 fighting" },
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
            const juce::String two = "{ \"tutor\": { \"version\": \"t1\", \"file\": \"tutor.zip\", \"sha256\": \"" + sha + "\", \"models\": ["
                "{ \"id\": \"light\", \"name\": \"Light\", \"url\": \"https://huggingface.co/a/l.gguf\", \"sha256\": \"" + sha + "\", \"bytes\": 2000 },"
                "{ \"id\": \"smart\", \"name\": \"Smart\", \"url\": \"https://huggingface.co/a/s.gguf\", \"sha256\": \"" + juce::String::repeatedString ("ef", 32) + "\", \"bytes\": 3000, \"minMemoryGb\": 16, \"appleSilicon\": true } ] } }";
            TutorPack both;
            check (LocalAI::parseTutorFeed (two, both) && both.models.size() == 2 && both.modelName == "Light", "Tutor feed can offer a lighter and a smarter model");
            check (LocalAI::pickModel (both.models, {}, true, 16) == 1 && LocalAI::pickModel (both.models, {}, true, 8) == 0 && LocalAI::pickModel (both.models, {}, false, 32) == 0
                   && LocalAI::pickModel (both.models, "smart", false, 8) == 1 && LocalAI::pickModel (both.models, "light", true, 64) == 0,
                   "The smarter model goes to Macs that can run it, and a chosen model is respected");
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
                {
                    // the feed now names a different model: the installed tutor offers the upgrade and takes it
                    juce::FileOutputStream out (model);
                    out.writeText ("a newer model", false, false, nullptr);
                }
                writeFeed (LocalAI::sha256Of (model));
                {
                    LocalAI later;
                    later.setAddressForTesting ("http://127.0.0.1:9");
                    later.configureForTesting (feedUrl, home);
                    later.refresh();
                    juce::Thread::sleep (500);
                    auto up = later.getStatus();
                    check (up.available && up.upgradeAvailable && up.upgradeBytes == model.getSize(), "An installed tutor notices a newer model in the feed");
                    later.installBuiltIn();
                    up = settle (later);
                    int models = 0;
                    for (const auto& e : juce::RangedDirectoryIterator (home, false, "model-*", juce::File::findFiles)) { ++models; juce::ignoreUnused (e); }
                    check (up.setup == LocalAI::Setup::Ready && ! up.upgradeAvailable && models == 1, "Upgrading swaps the model and removes the old one", up.setupMessage);
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

    // ---- Fix, FX, low latency, mono to stereo
    {
        for (auto* pr : { vocal.get(), beat.get(), bass.get() }) pr->applyChainPreset (findChainPreset ("Clean slate"));
        remeasure();
        runSeconds (5.0);
        const auto before = vocal->getBus().computeMasking();
        check (! before.empty(), "A collision is found between the tracks", juce::String ((int) before.size()));
        if (! before.empty())
        {
            const auto c = before.front();
            MUProcessor* covering = nullptr;
            for (auto* pr : { vocal.get(), beat.get(), bass.get() }) if (pr->getSessionSlot() == c.slotB) covering = pr;
            vocal->getBus().applyMaskingClash (c);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
            const auto after = vocal->getBus().computeMasking();
            bool still = false;
            for (auto& a : after) still = still || (a.slotB == c.slotB && std::abs (std::log2 (a.freqHz / c.freqHz)) < 0.4f);
            check (! still && covering != nullptr && covering->cutDbNear (c.freqHz) <= -1.5f, "Fix puts a cut on the covering track and the collision leaves the list",
                   covering != nullptr ? juce::String (covering->cutDbNear (c.freqHz), 1) + " dB at " + juce::String (juce::roundToInt (c.freqHz)) + " Hz" : juce::String ("no track"));
            if (covering != nullptr)
            {
                float threshold = 0.0f;
                for (int bnd = 0; bnd < kNumEQBands; ++bnd)
                {
                    const auto e = covering->getBandSettings (bnd);
                    if (e.enabled && e.dynamic && std::abs (std::log2 (e.freqHz / c.freqHz)) < 0.4f) threshold = e.thresholdDb;
                }
                const float level = covering->getLoudness().getShortTerm();
                check (threshold < -12.0f && std::abs (threshold - (level - 16.0f)) < 3.0f, "The cut's threshold is fitted to how loud that track is",
                       juce::String (threshold, 1) + " dB threshold on a track at " + juce::String (level, 1) + " LUFS");
                runSeconds (2.0);
                bool biting = false; float depth = 0.0f;
                for (int bnd = 0; bnd < kNumEQBands; ++bnd)
                {
                    const auto e = covering->getBandSettings (bnd);
                    if (e.enabled && e.dynamic && std::abs (std::log2 (e.freqHz / c.freqHz)) < 0.4f) { biting = covering->getEQ().getBandDynamicOffsetDb (bnd) < -0.5f; depth = covering->getEQ().getBandDynamicOffsetDb (bnd); }
                }
                check (biting, "The cut is working while that track plays", juce::String (depth, 1) + " dB right now");
            }
        }

        // presets with FX switch them on and leave the song's key alone
        vocal->setParam (ids::tuneKey, 7.0f); vocal->setParam (ids::tuneScale, 2.0f);
        vocal->applyChainPreset (findChainPreset ("Hook stack"));
        check (vocal->getParam (ids::tuneOn) > 0.5f && vocal->getParam (ids::dblOn) > 0.5f && vocal->getParam (ids::dblVoices) > 0.5f && vocal->getParam (ids::rvOn) > 0.5f
               && (int) vocal->getParam (ids::tuneKey) == 7 && (int) vocal->getParam (ids::tuneScale) == 2, "FX presets switch on tune, doubles and reverb and keep the song's key");
        runSeconds (3.0);
        double side = 0.0, midE = 0.0; bool finite = true;
        for (int i = 0; i < block; ++i)
        {
            const float l = v.getSample (0, i), r = v.getSample (1, i);
            side += (double) (l - r) * (l - r); midE += (double) (l + r) * (l + r);
            finite = finite && std::isfinite (l) && std::isfinite (r);
        }
        check (finite && side > midE * 1.0e-4 && vocal->getTailLengthSeconds() > 0.2 && vocal->getTuner().getOutputNote() > 0.0f,
               "Tune, doubles and reverb run in the chain and widen a centred vocal", "side/mid energy " + juce::String (side / juce::jmax (1.0e-12, midE), 4));

        // low latency mode: the delaying stages leave the path on every instance, and come back afterwards
        const int tuned = vocal->getLatencySamples(), mastered = master->getLatencySamples();
        vocal->getBus().setLowLatency (true);
        runSeconds (0.5);
        const int tunedLow = vocal->getLatencySamples(), masteredLow = master->getLatencySamples();
        check (tuned >= (int) (fs * 0.020) && tunedLow <= tuned - (int) (fs * 0.019) && masteredLow < mastered, "Low latency mode takes the delaying stages out of every instance",
               "vocal " + juce::String (tuned) + " to " + juce::String (tunedLow) + " samples, master " + juce::String (mastered) + " to " + juce::String (masteredLow));
        vocal->getBus().setLowLatency (false);
        runSeconds (0.5);
        check (vocal->getLatencySamples() == tuned && master->getLatencySamples() == mastered, "Turning low latency off restores full quality");
        vocal->applyChainPreset (findChainPreset ("Clean slate"));

        // a mono track can load the plugin as mono in, stereo out, so its doubles and reverb can be wide
        MUProcessor monoToStereo;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono()); layout.outputBuses.add (juce::AudioChannelSet::stereo());
        const bool accepted = monoToStereo.checkBusesLayoutSupported (layout) && monoToStereo.setBusesLayout (layout);
        monoToStereo.setRateAndBufferSizeDetails (fs, block);
        monoToStereo.prepareToPlay (fs, block);
        monoToStereo.setParam (ids::dblOn, 1.0f);
        juce::AudioBuffer<float> mb (2, block);
        double diff = 0.0, right = 0.0;
        for (int n = 0; n < 150; ++n)
        {
            for (int i = 0; i < block; ++i) { mb.setSample (0, i, vocalSrc.next() * 0.4f); mb.setSample (1, i, 0.0f); }
            monoToStereo.processBlock (mb, midi);
            if (n > 100) for (int i = 0; i < block; ++i) { diff += std::abs (mb.getSample (0, i) - mb.getSample (1, i)); right += std::abs (mb.getSample (1, i)); }
        }
        check (accepted && right > 1.0 && diff > right * 0.02, "Mono in, stereo out is supported and gives a mono vocal wide doubles", "accepted " + juce::String ((int) accepted));
    }

    // ---- the order of the in-line FX tools
    {
        namespace fo = mu::fxorder;
        auto fresh = [&] (std::unique_ptr<MUProcessor>& p, int order)
        {
            p = std::make_unique<MUProcessor>();
            p->setRateAndBufferSizeDetails (fs, block);
            p->prepareToPlay (fs, block);
            p->setParam (ids::lfOn, 1.0f); p->setParam (ids::lfCrush, 80.0f); p->setParam (ids::lfMix, 100.0f); p->setParam (ids::rvOn, 1.0f); p->setParam (ids::rvMix, 40.0f);
            p->setParam (ids::fxOrder, (float) order);
        };
        const int reverbFirst = fo::encode (fo::moved (fo::decode (0), fo::Reverb, 0));       // Reverb, then Phone, Lo-fi, Motion, Chop, Delay
        std::unique_ptr<MUProcessor> standard, swapped, swappedAgain;
        fresh (standard, 0); fresh (swapped, reverbFirst); fresh (swappedAgain, reverbFirst);
        juce::AudioBuffer<float> a (2, block), b (2, block), c (2, block);
        double sum = 0.0, diffOrder = 0.0, diffSame = 0.0;
        for (int n = 0; n < 200; ++n)
        {
            for (int i = 0; i < block; ++i)
            {
                // a short note twice a second, so there are tails to hear
                const int at = (n * block + i) % (int) (fs * 0.5);
                const float v = at < (int) (fs * 0.12) ? 0.3f * (float) (std::sin (juce::MathConstants<double>::twoPi * 330.0 * at / fs) + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 2640.0 * at / fs)) : 0.0f;
                for (auto* buf : { &a, &b, &c }) { buf->setSample (0, i, v); buf->setSample (1, i, v); }
            }
            standard->processBlock (a, midi); swapped->processBlock (b, midi); swappedAgain->processBlock (c, midi);
            if (n > 60) for (int i = 0; i < block; ++i)
            {
                sum += (double) a.getSample (0, i) * a.getSample (0, i);
                diffOrder += std::pow ((double) a.getSample (0, i) - b.getSample (0, i), 2.0);
                diffSame += std::pow ((double) b.getSample (0, i) - c.getSample (0, i), 2.0);
            }
        }
        const double byOrder = 10.0 * std::log10 (diffOrder / juce::jmax (1.0e-12, sum) + 1.0e-12), same = 10.0 * std::log10 (diffSame / juce::jmax (1.0e-12, sum) + 1.0e-12);
        check (byOrder > -30.0 && same < -60.0, "Reverb before Lo-fi sounds different from Lo-fi before Reverb, and the same order gives the same sound",
               "difference " + juce::String (byOrder, 1) + " dB against the signal between the two orders, " + juce::String (same, 1) + " dB between two runs of one order");

        // changing the order while the song plays: the track dips for a moment and comes back, with no step in it
        std::unique_ptr<MUProcessor> live;
        fresh (live, 0);
        live->setParam (ids::lfOn, 0.0f); live->setParam (ids::rvOn, 0.0f);
        float worstStep = 0.0f, lowest = 1.0f, after = 0.0f; double phase = 0.0;
        for (int n = 0; n < 120; ++n)
        {
            if (n == 60) live->setParam (ids::fxOrder, (float) reverbFirst);
            for (int i = 0; i < block; ++i) { phase += juce::MathConstants<double>::twoPi * 220.0 / fs; const float v = 0.3f * (float) std::sin (phase); a.setSample (0, i, v); a.setSample (1, i, v); }
            live->processBlock (a, midi);
            if (n >= 58)
            {
                for (int i = 2; i < block; ++i) worstStep = juce::jmax (worstStep, std::abs (a.getSample (0, i) - 2.0f * a.getSample (0, i - 1) + a.getSample (0, i - 2)));
                if (n < 66) lowest = juce::jmin (lowest, a.getMagnitude (0, block - 48, 48));        // the end of each block: the swap happens between blocks
                if (n > 100) after = juce::jmax (after, a.getMagnitude (0, 0, block));
            }
        }
        const float steadyCurve = 0.3f * (float) std::pow (juce::MathConstants<double>::twoPi * 220.0 / fs, 2.0);       // the second difference of the tone itself
        check (worstStep < steadyCurve * 6.0f && lowest < 0.05f && after > 0.2f && juce::roundToInt (live->getParam (ids::fxOrder)) == reverbFirst,
               "Changing the FX order during playback fades the track out and back in with no click",
               "largest step " + juce::String (worstStep / steadyCurve, 1) + " times the tone's own curve; dips to " + juce::String (lowest, 3) + " and returns to " + juce::String (after, 2));
        // the order travels with a chain preset of your own, and the built-in chains and sounds go back to the standard order
        live->setParam (ids::fxOrder, (float) reverbFirst);        // A has Reverb first
        live->switchAB();                                          // to B
        live->setParam (ids::fxOrder, 0.0f);                       // B has the standard order
        live->switchAB();                                          // back to A
        const bool abKeeps = juce::roundToInt (live->getParam (ids::fxOrder)) == reverbFirst;
        live->applyChainPreset (findChainPreset ("Rap vocal - punchy"));
        const bool chainResets = juce::roundToInt (live->getParam (ids::fxOrder)) == 0;
        live->setParam (ids::fxOrder, (float) reverbFirst);
        live->applySound (0);
        check (abKeeps && chainResets && juce::roundToInt (live->getParam (ids::fxOrder)) == 0, "The order is part of A/B, and a built-in chain or sound puts it back to the standard order");
    }

    // ---- build 11: names, song key, hot peaks, presets and sounds, graph, reverb fit, checks
    {
        auto named = [] (MUProcessor& p, const juce::String& name) { juce::AudioProcessor::TrackProperties props; props.name = name; p.updateTrackProperties (props); };
        for (auto* p : { vocal.get(), beat.get(), bass.get() }) p->applyChainPreset (findChainPreset ("Clean slate"));
        for (auto* p : { vocal.get(), beat.get(), bass.get(), master.get() }) p->setParam (ids::role, (float) Role::Auto);
        named (*vocal, "rap"); named (*beat, "beat 128 bpm Gmin"); named (*bass, "808"); named (*master, "Stereo Out");
        runSeconds (1.0);
        check (vocal->getEffectiveRole() == Role::Vocal && beat->getEffectiveRole() == Role::Beat && bass->getEffectiveRole() == Role::Bass808 && master->getEffectiveRole() == Role::Master,
               "With roles on Auto, the track names decide: rap, beat, 808, Stereo Out", roleNames()[(int) vocal->getEffectiveRole()] + " / " + roleNames()[(int) beat->getEffectiveRole()] + " / "
               + roleNames()[(int) bass->getEffectiveRole()] + " / " + roleNames()[(int) master->getEffectiveRole()]);

        // the key written in the beat's name is found from any instance and can be sent to all of them
        vocal->setParam (ids::tuneKey, 0.0f); vocal->setParam (ids::tuneScale, 0.0f);
        const auto key = master->getBus().findSongKey();
        const auto said = vocal->findKey();
        check (key.found && key.fromName && key.key == 7 && key.minor && (int) vocal->getParam (ids::tuneKey) == 7 && (int) vocal->getParam (ids::tuneScale) == 2 && said.contains ("G minor"),
               "Find key reads G minor from the name of the beat", said);
        beat->setParam (ids::tuneKey, 3.0f);
        const auto sent = master->sendKeyToSession();
        check (sent.contains ("Choose Major or Minor"), "The master will not send a key until one is chosen", sent);
        master->findKey();
        const auto sent2 = master->sendKeyToSession();
        check ((int) beat->getParam (ids::tuneKey) == 7 && (int) beat->getParam (ids::tuneScale) == 2 && (int) bass->getParam (ids::tuneKey) == 7 && sent2.contains ("G minor"), "Send key to all tracks sets every instance", sent2);
        // without a name to read, the master works the key out by ear and says where it heard it
        named (*beat, "Audio 2");
        runSeconds (12.0);
        const auto heard = master->getBus().findSongKey();
        check (heard.found && ! heard.fromName && heard.source == "Stereo Out", "Without a key in any name, the key is heard on the master",
               heard.found ? heard.text() + " from " + heard.source + ", confidence " + juce::String (heard.confidence, 2) : juce::String ("not found"));
        named (*beat, "beat 128 bpm Gmin");

        // a vocal recorded far too hot: the guard holds it at -1 dB, the session lists it, and Fix compresses it
        const float wasGain = vocalGain;
        vocalGain = 2.6f;
        remeasure();
        runSeconds (6.0);
        auto hot = master->getBus().findHotPeaks();
        const bool listed = ! hot.empty() && hot.front().name == "rap";
        const float out = vocal->getLoudness().getTruePeakHold();
        check (listed && out < -0.5f && hot.front().overDb > 2.0f, "A track that is too hot is held under the top by its peak guard and listed for fixing",
               listed ? juce::String (hot.front().overDb, 1) + " dB over, " + juce::String (hot.front().crestDb, 1) + " dB above its average, leaving at " + juce::String (out, 1) + " dBTP" : juce::String ("not listed"));
        const auto done = listed ? master->getBus().tamePeaks (hot.front().slot) : juce::String();
        runSeconds (2.0);
        check (vocal->getParam (ids::compEnabled) > 0.5f && done.contains ("Compressor"), "Fix switches on the compressor, fitted to the track", done);
        // the session works to the clock on the wall, so wait in real time for it to look again
        auto waitFor = [&] (double wallSeconds) { const double until = juce::Time::getMillisecondCounterHiRes() + wallSeconds * 1000.0; while (juce::Time::getMillisecondCounterHiRes() < until) runSeconds (1.0); };
        waitFor (7.5);
        const auto notice = master->getBus().takeNotice();
        runSeconds (4.0);
        hot = master->getBus().findHotPeaks();
        check (notice.contains ("too loud") && hot.empty(), "If it is still over a few seconds later because the track is simply too loud, it is turned down, and says so",
               notice + (hot.empty() ? juce::String() : " Still " + juce::String (hot.front().overDb, 1) + " dB over."));

        // Learn and balance does the same on its own for a vocal, then balances again
        vocal->applyChainPreset (findChainPreset ("Clean slate"));        // also puts the output back to 0 dB
        vocal->setParam (ids::role, (float) Role::Auto);
        vocal->setTrimDb (0.0f);
        master->getBus().setTrackLocked (vocal->getSessionSlot(), true);      // keeps its level, so its peaks stay hot through the balance
        remeasure();
        runSeconds (3.0);
        master->learnAndBalance (5.0f);
        for (int i = 0; i < 20 && master->getBus().getLearnState().active; ++i) runSeconds (1.0);
        const auto message = master->getBus().getLearnState().message;
        check (vocal->getParam (ids::compEnabled) > 0.5f && message.contains ("compressor was switched on"), "Learn and balance compresses a vocal whose loud words tower over the rest", message);
        // Learn also fits the EQ of a vocal nobody has touched to the voice that is there, and says so
        check (message.contains ("was fitted to the voice") == ! vocal->isEqUntouched(), "Learn fits the EQ of an untouched vocal to the voice and says so, or leaves a voice that is already right alone",
               message.contains ("was fitted to the voice") ? juce::String ("fitted") : juce::String ("left alone"));
        waitFor (8.0);
        check (master->getBus().getLearnState().message.contains ("Balanced again"), "...and balances once more when the new levels have been heard", master->getBus().getLearnState().message);
        master->getBus().setTrackLocked (vocal->getSessionSlot(), false);
        waitFor (7.0);                               // let any follow-up finish before moving on
        master->getBus().takeNotice();
        vocalGain = wasGain;
        vocal->setTrimDb (0.0f);
        vocal->setParam (ids::outputGain, 0.0f);
        vocal->applyChainPreset (findChainPreset ("Clean slate"));
        vocal->setParam (ids::role, (float) Role::Auto);

        // A suggestion is what is still missing with the EQ as it stands: applying it again adds to the band that is
        // already doing that job, up to a ceiling, instead of replacing it or stacking a second band on top
        {
            Suggestion lift; lift.kind = Suggestion::Kind::EqBand; lift.action = "Boost +5.0 dB at 3.0 kHz";
            lift.band.enabled = true; lift.band.type = EQType::Bell; lift.band.freqHz = 3000.0f; lift.band.gainDb = 5.0f; lift.band.q = 0.7f;
            vocal->applySuggestion (lift);
            auto nearby = lift; nearby.band.freqHz = 3400.0f;
            const auto said3 = vocal->applySuggestion (nearby);
            int bands = 0; float gain = 0.0f;
            for (int b = 0; b < kNumEQBands; ++b) if (vocal->getParam (ids::eqBandEnabled (b)) > 0.5f) { ++bands; gain = vocal->getBandSettings (b).gainDb; }
            check (bands == 1 && std::abs (gain - 8.0f) < 0.01f && said3.contains ("added to what was there"), "A second boost in the same place is added to the first, and the total stops at +8 dB",
                   juce::String (bands) + " band, " + juce::String (gain, 1) + " dB. " + said3);
            vocal->applyChainPreset (findChainPreset ("Clean slate"));
            vocal->setParam (ids::role, (float) Role::Auto);
        }

        // every preset of every tool loads and switches its tool on; the robot borrows the scale and gives it back
        vocal->setParam (ids::tuneKey, 7.0f); vocal->setParam (ids::tuneScale, 2.0f);
        bool allOn = true; int count = 0; juce::String failed;
        for (int i = 0; i < (int) modulePresets().size(); ++i)
        {
            const auto& mp = modulePresets()[(size_t) i];
            const auto said2 = vocal->applyModulePreset (i);
            juce::String onParam = mp.module == "tune" ? ids::tuneOn : juce::String();
            for (auto& m : fxModules()) if (m.id == mp.module) onParam = m.onParam;
            const bool ok = said2.isNotEmpty() && vocal->getParam (onParam) > 0.5f && vocal->getModulePresetName (mp.module) == mp.name;
            if (! ok) { allOn = false; failed = mp.module + "/" + mp.name; }
            ++count;
        }
        check (allOn && count >= 70 && modulePresetsFor ("delay").size() >= 8 && modulePresetsFor ("reverb").size() >= 8 && modulePresetsFor ("tune").size() >= 10,
               "Every tool preset loads and switches its tool on", juce::String (count) + " presets" + (failed.isEmpty() ? juce::String() : ", failed: " + failed));
        const auto tunePresets = modulePresetsFor ("tune");
        int robot = -1, natural = -1;
        for (int i : tunePresets) { if (modulePresets()[(size_t) i].name == "Robotic vocal") robot = i; if (modulePresets()[(size_t) i].name == "Natural vocal") natural = i; }
        vocal->setParam (ids::tuneScale, 2.0f);
        vocal->applyModulePreset (robot);
        const bool oneNote = (int) vocal->getParam (ids::tuneScale) == Tuner::kCustomScale && (int) vocal->getParam (ids::tuneNotes) == (1 << 7);
        vocal->applyModulePreset (natural);
        check (oneNote && (int) vocal->getParam (ids::tuneScale) == 2 && (int) vocal->getParam (ids::tuneKey) == 7, "The Robot preset allows only the key note, and the next preset gives the scale back");

        // every sound loads, runs clean, and leaves nothing on from the sound before it
        bool finite = true; juce::String worstSound; float worstPeak = 0.0f;
        for (int i = 0; i < (int) soundPresets().size(); ++i)
        {
            vocal->applySound (i);
            runSeconds (1.2);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < block; ++n) finite = finite && std::isfinite (v.getSample (c, n));
            const float peak = v.getMagnitude (0, block);
            if (peak > worstPeak) { worstPeak = peak; worstSound = soundPresets()[(size_t) i].name; }
        }
        int stacked = -1, phoneVerse = -1;
        for (int i = 0; i < (int) soundPresets().size(); ++i) { if (soundPresets()[(size_t) i].name == "Stacked hook") stacked = i; if (soundPresets()[(size_t) i].name == "Phone verse") phoneVerse = i; }
        vocal->applySound (stacked);
        const bool stackOn = vocal->getParam (ids::dblOn) > 0.5f && vocal->getParam (ids::hmOn) > 0.5f && vocal->getParam (ids::rvOn) > 0.5f;
        vocal->applySound (phoneVerse);
        check (finite && worstPeak <= 0.9f && stackOn && vocal->getParam (ids::phOn) > 0.5f && vocal->getParam (ids::dblOn) < 0.5f && vocal->getParam (ids::hmOn) < 0.5f && vocal->getSoundName() == "Phone verse",
               "Every sound loads and runs clean, and a new sound clears the one before", juce::String ((int) soundPresets().size()) + " sounds, loudest output " + juce::String (juce::Decibels::gainToDecibels (worstPeak), 1) + " dB (" + worstSound + ")");
        vocal->applyChainPreset (findChainPreset ("Clean slate"));
        vocal->setParam (ids::role, (float) Role::Auto);

        // the delay follows the host's tempo
        vocal->setParam (ids::dlOn, 1.0f); vocal->setParam (ids::dlSync, 1.0f); vocal->setParam (ids::dlNote, 2.0f);
        runSeconds (0.5);
        check (std::abs (vocal->getEcho().currentTimeMs() - 375.0f) < 0.5f && vocal->getTailLengthSeconds() > 0.5, "Delay takes its time from the song: a dotted eighth at 120 BPM is 375 ms", juce::String (vocal->getEcho().currentTimeMs(), 1) + " ms");
        vocal->setParam (ids::dlOn, 0.0f);

        // reverb fitted to the track
        const auto fitVocal = vocal->fitReverb();
        const auto fitBass = bass->fitReverb();
        check (vocal->getParam (ids::rvOn) > 0.5f && (int) vocal->getParam (ids::rvType) == 1 && bass->getParam (ids::rvOn) < 0.5f && fitBass.contains ("dry") && fitVocal.contains ("120 BPM"),
               "Fit to this track chooses a plate for the vocal and leaves the 808 dry", fitVocal);
        vocal->setParam (ids::rvOn, 0.0f);

        // graph view: capture, correct, save with the song, reopen
        vocal->setParam (ids::tuneKey, 7.0f); vocal->setParam (ids::tuneScale, 2.0f);
        vocal->startCapture();
        runSeconds (5.0);
        const bool wasCapturing = vocal->isCapturing();
        head.playing = false;                       // stopping the song ends the capture
        runSeconds (0.6);
        head.playing = true;
        const int found = (int) vocal->getGraph().getNotes().size();
        const auto corrected = vocal->correctAllNotes (0.35f);
        int onScale = 0;
        for (auto& n : vocal->getGraph().getNotes()) if (std::abs (n.pitch - std::round (n.pitch)) < 0.001f && Tuner::noteAllowed ((int) std::lround (n.pitch), 7, 2)) ++onScale;
        check (wasCapturing && ! vocal->isCapturing() && found >= 6 && onScale == found && (int) vocal->getParam (ids::tuneMode) == 1 && vocal->getParam (ids::tuneOn) > 0.5f,
               "Graph: capture writes the notes down, stopping the song ends it, Correct all puts them on the scale", juce::String (found) + " notes. " + corrected);
        juce::MemoryBlock saved;
        vocal->getStateInformation (saved);
        {
            MUProcessor reopened;
            reopened.setPlayConfigDetails (2, 2, fs, block);
            reopened.prepareToPlay (fs, block);
            reopened.setStateInformation (saved.getData(), (int) saved.getSize());
            const auto& a = vocal->getGraph().getNotes(); const auto& b2 = reopened.getGraph().getNotes();
            const bool same = a.size() == b2.size() && ! a.empty() && std::abs (a[0].pitch - b2[0].pitch) < 0.01f && a[0].start == b2[0].start
                           && std::abs (reopened.getGraph().targetAt (a[0].start + 5) - vocal->getGraph().targetAt (a[0].start + 5)) < 0.03f && (int) reopened.getParam (ids::tuneMode) == 1;
            check (same, "The edited notes are saved with the song and come back when it is reopened", juce::String ((int) saved.getSize() / 1024) + " kB of state");

            // a song saved before Tune had its own tab opens on the tab it was left on
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()))
            {
                xml->removeAttribute ("uiTabs"); xml->setAttribute ("uiTab", 5);          // 5 used to be Master
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                reopened.setStateInformation (old.getData(), (int) old.getSize());
                check (reopened.getCurrentTab() == 6, "A song saved by an earlier build reopens on the same tab", "tab " + juce::String (reopened.getCurrentTab()));
            }
        }
        vocal->clearGraph();
        vocal->setParam (ids::tuneMode, 0.0f); vocal->setParam (ids::tuneOn, 0.0f);

        // bounce check and listening checks
        remeasure();
        runSeconds (6.0);
        const auto items = master->bounceCheck();
        bool hasLoudness = false, hasPeaks = false, hasMono = false;
        for (auto& c : items) { hasLoudness = hasLoudness || c.title == "Loudness"; hasPeaks = hasPeaks || c.title == "Peaks"; hasMono = hasMono || c.title == "Mono"; }
        check (hasLoudness && hasPeaks && hasMono && items.size() >= 5, "Bounce check reports loudness, peaks, mono, track peaks, balance and collisions", juce::String ((int) items.size()) + " lines; first: " + items.front().title + ": " + items.front().detail);
        master->setListenCheck (ListenCheck::Phone);
        const float lufsBefore = master->getLoudness().getShortTerm();
        runSeconds (3.0);
        double side = 0.0, level = 0.0;
        for (int n = 0; n < block; ++n) { side += std::abs (mix.getSample (0, n) - mix.getSample (1, n)); level += std::abs (mix.getSample (0, n)); }
        check (side < 1.0e-6 && level > 0.01 && std::abs (master->getLoudness().getShortTerm() - lufsBefore) < 1.5f, "The phone-speaker check changes what is heard and not what is measured",
               "meter " + juce::String (lufsBefore, 1) + " then " + juce::String (master->getLoudness().getShortTerm(), 1) + " LUFS");
        master->setNonRealtime (true);
        runSeconds (0.5);
        side = 0.0;
        for (int n = 0; n < block; ++n) side += std::abs (mix.getSample (0, n) - mix.getSample (1, n));
        master->setNonRealtime (false);
        master->setListenCheck (ListenCheck::Off);
        check (side > 1.0e-3, "A listening check is left out of a bounce");
        for (auto* p : { vocal.get(), beat.get(), bass.get(), master.get() }) named (*p, {});
        vocal->setParam (ids::role, (float) Role::Vocal); beat->setParam (ids::role, (float) Role::Beat); bass->setParam (ids::role, (float) Role::Bass808); master->setParam (ids::role, (float) Role::Master);
    }

    // ---- the editor builds, runs and closes cleanly on every tab (needs a display)
    if (juce::SystemStats::getEnvironmentVariable ("DISPLAY", {}).isNotEmpty() || juce::SystemStats::getOperatingSystemType() != juce::SystemStats::Linux)
    {
        for (int tab = 0; tab < 8; ++tab)
        {
            vocal->setCurrentTab (tab);
            std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
            ed->setVisible (true);
            runSeconds (0.4);
            ed.reset();
        }
        // every tool on the FX rack, each switched on, and the Tune graph with notes in it
        for (int tool = 0; tool < (int) fxModules().size(); ++tool)
        {
            vocal->setCurrentTab (5);
            vocal->apvts.state.setProperty ("fxModule", tool, nullptr);
            vocal->setParam (fxModules()[(size_t) tool].onParam, 1.0f);
            std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
            ed->setVisible (true);
            runSeconds (0.3);
            ed.reset();
            vocal->setParam (fxModules()[(size_t) tool].onParam, 0.0f);
        }
        vocal->setCurrentTab (4);
        vocal->startCapture();
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (vocal->createEditor());
            ed->setVisible (true);
            runSeconds (2.0);
            vocal->stopCapture();
            vocal->correctAllNotes (0.5f);
            runSeconds (0.5);
            ed.reset();
        }
        vocal->clearGraph();
        std::unique_ptr<juce::AudioProcessorEditor> ed (master->createEditor());
        runSeconds (0.4);
        ed.reset();
        check (true, "Editor opens and closes on every tab");
    }

    // ---- build 12: the balance trim sits after the processing, and presets of your own
    {
        auto near = [] (float a, float b, float tolerance) { return std::abs (a - b) <= tolerance; };
        vocal->setParam (ids::role, (float) Role::Vocal);
        vocal->applyChainPreset (findChainPreset ("Clean slate"));
        vocal->setTrimDb (0.0f); vocal->setParam (ids::outputGain, 0.0f);
        remeasure(); runSeconds (5.0);
        vocal->applyChainPreset (findChainPreset ("Rap vocal - punchy"));
        runSeconds (1.0); remeasure(); runSeconds (5.0);
        const float before = vocal->getLoudness().getShortTerm();
        const float thresholdWas = vocal->getParam (ids::compThreshold);
        vocal->setTrimDb (8.0f);
        runSeconds (1.0); remeasure(); runSeconds (5.0);
        const float after = vocal->getLoudness().getShortTerm();
        check (std::abs ((after - before) - 8.0f) < 1.0f && std::abs (vocal->getParam (ids::compThreshold) - thresholdWas) < 0.01f && ! vocal->isTrimBeforeChain(),
               "Balancing a track up by 8 dB makes it 8 dB louder and leaves its compressor working exactly as it was", juce::String (after - before, 2) + " dB louder");

        // a song saved by an earlier build had the trim in front: it opens sounding the same, and moves over when next balanced
        // (a saved state is an 8-byte header, then the settings as XML text; take the new marker out of it)
        juce::MemoryBlock saved; vocal->getStateInformation (saved);
        auto text = juce::String::fromUTF8 ((const char*) saved.getData() + 8, (int) saved.getSize() - 8);
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (text));
        xml->removeAttribute ("trimAfter");
        const auto stripped = xml->toString (juce::XmlElement::TextFormat().singleLine());
        juce::MemoryOutputStream rebuilt;
        rebuilt.writeInt (0x21324356); rebuilt.writeInt ((int) stripped.getNumBytesAsUTF8()); rebuilt.write (stripped.toRawUTF8(), stripped.getNumBytesAsUTF8()); rebuilt.writeByte (0);
        vocal->setStateInformation (rebuilt.getData(), (int) rebuilt.getDataSize());
        runSeconds (1.0); remeasure(); runSeconds (5.0);
        const bool wasInFront = vocal->isTrimBeforeChain();
        const float oldWay = vocal->getLoudness().getShortTerm();
        const float oldThreshold = vocal->getParam (ids::compThreshold);
        vocal->setTrimDb (8.0f);                                    // the balance touches it: same trim, now behind the processing
        runSeconds (1.0); remeasure(); runSeconds (5.0);
        const float newWay = vocal->getLoudness().getShortTerm();
        check (wasInFront && ! vocal->isTrimBeforeChain() && std::abs (newWay - oldWay) < 1.5f && std::abs (vocal->getParam (ids::compThreshold) - (oldThreshold - 8.0f)) < 0.01f,
               "A song from an earlier build keeps its trim in front until it is next balanced, then moves it behind without changing the sound",
               juce::String (newWay - oldWay, 2) + " dB difference; threshold " + juce::String (oldThreshold, 1) + " -> " + juce::String (vocal->getParam (ids::compThreshold), 1) + " dB");
        vocal->setTrimDb (0.0f);

        // Analyse listens first
        vocal->startListening();
        runSeconds (4.0);
        juce::String said;
        vocal->analyse (said);
        check (said.startsWith ("Listened to") && ! vocal->isListening(), "Analyse reports how much of the track it listened to before deciding", said.substring (0, 80));

        // ---- presets of your own
        auto presetRoot = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-preset-test");
        presetRoot.deleteRecursively();
        UserPresets::setFolderForTesting (presetRoot);
        remeasure(); runSeconds (4.0);
        vocal->setParam (ids::compRatio, 6.5f); vocal->setParam (ids::rvOn, 1.0f); vocal->setParam (ids::rvType, (float) Reverb::Chamber); vocal->setParam (ids::rvSize, 140.0f);
        bool ok = false;
        const auto savedAs = vocal->saveUserPreset ("chain", "  My rap: chain?  ", false, &ok);
        UserPreset onDisk;
        const bool found = UserPresets::find ("chain", "My rap chain", onDisk);
        const auto json = juce::JSON::parse (onDisk.file.loadFileAsString());
        check (ok && found && onDisk.file.isAChildOf (presetRoot) && (int) json.getProperty ("formatVersion", 0) == UserPresets::kFormatVersion && json.getProperty ("format", {}).toString() == UserPresets::kFormatName
               && onDisk.values.size() > 100 && vocal->getCurrentPresetName() == "My rap chain" && ! vocal->presetEdited ("chain"),
               "The whole chain saves as a preset of your own: a versioned file in the presets folder, with a name safe for any disk", savedAs + " (" + juce::String ((int) onDisk.values.size()) + " settings)");
        bool roleKept = true; for (auto& v : onDisk.values) roleKept = roleKept && v.id != ids::role && v.id != ids::trim && v.id != ids::bypass && v.id != ids::tuneKey;
        check (roleKept, "A chain preset does not hold the track's role, its balance, its bypass or the song's key");
        vocal->setParam (ids::compRatio, 2.0f);
        check (vocal->presetEdited ("chain"), "Moving a control marks the loaded preset as changed");
        vocal->setParam (ids::rvType, 0.0f); vocal->setParam (ids::rvSize, 100.0f);
        const auto loaded = vocal->loadUserPreset ("chain", "My rap chain");
        check (near (vocal->getParam (ids::compRatio), 6.5f, 0.01f) && (int) vocal->getParam (ids::rvType) == (int) Reverb::Chamber && near (vocal->getParam (ids::rvSize), 140.0f, 0.1f) && ! vocal->presetEdited ("chain"),
               "Loading it puts every control back", loaded);
        check (! (vocal->saveUserPreset ("chain", "My rap chain", false, &ok), ok) && ! (vocal->saveUserPreset ("chain", "Rap vocal - punchy", false, &ok), ok) && ! (vocal->saveUserPreset ("chain", " .. ", false, &ok), ok),
               "Save as refuses a name that is taken, the name of a built-in preset, and an empty name");
        // on a quieter track the thresholds follow
        const float thrSaved = vocal->getParam (ids::compThreshold);
        const float gainWas = vocalGain; vocalGain *= 0.5f;
        runSeconds (1.0); remeasure(); runSeconds (5.0);
        const auto quieter = vocal->loadUserPreset ("chain", "My rap chain");
        check (near (vocal->getParam (ids::compThreshold), thrSaved - 6.0f, 1.5f) && quieter.contains ("quieter"), "Loaded on a track 6 dB quieter, the chain's thresholds move down with it", quieter);
        vocalGain = gainWas;

        // one tool: only that tool's controls
        vocal->setParam (ids::rvMix, 33.0f); vocal->setParam (ids::rvShimmer, 77.0f); vocal->setParam (ids::compRatio, 3.0f);
        vocal->saveUserPreset ("reverb", "Big chamber", false, &ok);
        vocal->setParam (ids::rvMix, 5.0f); vocal->setParam (ids::rvShimmer, 10.0f); vocal->setParam (ids::compRatio, 9.0f);
        vocal->loadUserPreset ("reverb", "Big chamber");
        check (ok && near (vocal->getParam (ids::rvMix), 33.0f, 0.1f) && near (vocal->getParam (ids::rvShimmer), 77.0f, 0.1f) && near (vocal->getParam (ids::compRatio), 9.0f, 0.01f)
               && vocal->getModulePresetName ("reverb") == "Big chamber" && UserPresets::list ("reverb").size() == 1 && UserPresets::list ("chain").size() == 1,
               "A tool's preset holds that tool's controls and nothing else");

        // rename, export, delete, import
        const auto renamed = vocal->renameUserPreset ("reverb", "Big chamber", "Huge chamber");
        UserPreset huge; const bool haveHuge = UserPresets::find ("reverb", "Huge chamber", huge);
        check (haveHuge && UserPresets::list ("reverb").size() == 1 && vocal->getModulePresetName ("reverb") == "Huge chamber", "A preset can be renamed", renamed);
        const auto exported = presetRoot.getChildFile ("to-send").getChildFile ("for a friend.mupreset");
        exported.getParentDirectory().createDirectory();
        check (haveHuge && UserPresets::exportTo (huge, exported).wasOk() && exported.existsAsFile(), "A preset can be exported to a file");
        const auto deleted = vocal->deleteUserPreset ("reverb", "Huge chamber");
        check (UserPresets::list ("reverb").empty() && vocal->getModulePresetName ("reverb").isEmpty() && near (vocal->getParam (ids::rvMix), 33.0f, 0.1f), "Deleting a preset removes the file and leaves the settings alone", deleted);
        vocal->setParam (ids::rvMix, 1.0f);
        const auto imported = vocal->importUserPreset (exported, true);
        check (UserPresets::list ("reverb").size() == 1 && near (vocal->getParam (ids::rvMix), 33.0f, 0.1f), "An exported file imports on another setup and loads", imported);

        // files that should not be trusted
        auto bad = [&] (const juce::String& name, const juce::String& text) { const auto f = presetRoot.getChildFile ("to-send").getChildFile (name); f.replaceWithText (text); return vocal->importUserPreset (f, true); };
        const float mixBefore = vocal->getParam (ids::rvMix), ratioBefore = vocal->getParam (ids::compRatio);
        const auto notJson = bad ("a.mupreset", "this is not a preset");
        const auto wrongKind = bad ("b.mupreset", "{ \"format\": \"something-else\", \"formatVersion\": 1, \"scope\": \"reverb\", \"name\": \"x\", \"values\": { \"rvMix\": 90 } }");
        const auto tooNew = bad ("c.mupreset", "{ \"format\": \"musically-unorthodox-preset\", \"formatVersion\": 99, \"scope\": \"reverb\", \"name\": \"x\", \"values\": { \"rvMix\": 90 } }");
        const auto noValues = bad ("d.mupreset", "{ \"format\": \"musically-unorthodox-preset\", \"formatVersion\": 1, \"scope\": \"reverb\", \"name\": \"x\", \"values\": { } }");
        const auto noTool = bad ("e.mupreset", "{ \"format\": \"musically-unorthodox-preset\", \"formatVersion\": 1, \"scope\": \"teleporter\", \"name\": \"x\", \"values\": { \"rvMix\": 90 } }");
        check (near (vocal->getParam (ids::rvMix), mixBefore, 0.01f) && notJson.contains ("not a preset") && wrongKind.contains ("not a Musically Unorthodox preset") && tooNew.contains ("newer version")
               && noValues.contains ("no settings") && noTool.contains ("does not have") && UserPresets::list ("reverb").size() == 1,
               "Files that are not presets, are damaged, or come from a newer version are refused with a reason and change nothing", tooNew);
        // a tool's preset cannot reach outside its tool, and wild values are brought back into range
        const auto sneaky = bad ("f.mupreset", "{ \"format\": \"musically-unorthodox-preset\", \"formatVersion\": 1, \"scope\": \"reverb\", \"name\": \"Sneaky\", \"values\": "
                                               "{ \"rvMix\": 5000, \"compRatio\": 20, \"role\": 3, \"notAControl\": 1 } }");
        check (near (vocal->getParam (ids::rvMix), 100.0f, 0.01f) && near (vocal->getParam (ids::compRatio), ratioBefore, 0.01f) && (int) vocal->getParam (ids::role) == (int) Role::Vocal && sneaky.contains ("left out"),
               "A reverb preset can only move reverb controls, and a value outside a control's range is brought inside it", sneaky);
        // one tab: that tab's controls and nothing else, for each of the four tabs that have no tool list of their own
        {
            struct Tab { const char* scope; juce::String mine, other; float saved, moved, otherMoved; };
            const Tab tabs[] = { { "eq", ids::eqBandGain (3), ids::compRatio, 4.5f, -2.0f, 7.0f },
                                 { "dynamics", ids::compRatio, ids::satDrive, 5.0f, 2.0f, 9.0f },
                                 { "tone", ids::satDrive, ids::compRatio, 7.0f, 1.0f, 6.0f },
                                 { "master", ids::targetLufs, ids::satDrive, -11.0f, -16.0f, 4.0f } };
            bool allGood = true; juce::String detail;
            for (auto& t : tabs)
            {
                vocal->setParam (t.mine, t.saved);
                bool saved = false;
                vocal->saveUserPreset (t.scope, juce::String ("Mine ") + t.scope, false, &saved);
                vocal->setParam (t.mine, t.moved); vocal->setParam (t.other, t.otherMoved);
                const auto said4 = vocal->loadUserPreset (t.scope, juce::String ("Mine ") + t.scope);
                const bool good = saved && near (vocal->getParam (t.mine), t.saved, 0.05f) && near (vocal->getParam (t.other), t.otherMoved, 0.05f)
                                  && vocal->getModulePresetName (t.scope) == juce::String ("Mine ") + t.scope && ! vocal->presetEdited (t.scope);
                if (! good) { allGood = false; detail << t.scope << ": " << said4 << " "; }
            }
            // the role, the balance trim and the output level never travel in a tab's preset
            bool songThings = false;
            for (auto* scope : { "eq", "dynamics", "tone", "master" })
                for (auto& id : vocal->scopeParams (scope))
                    if (id == ids::role || id == ids::trim || id == ids::outputGain || id == ids::bypass) songThings = true;
            check (allGood && ! songThings, "EQ, Dynamics, Tone and Master each save and load a preset of their own that holds that tab and nothing else", detail);
            // a dynamics preset follows the level of the track it is loaded on, like a whole chain does
            vocal->setParam (ids::compThreshold, -24.0f);
            runSeconds (1.0); remeasure(); runSeconds (5.0);
            bool saved = false;
            vocal->saveUserPreset ("dynamics", "Level test", false, &saved);
            const float wasGain2 = vocalGain; vocalGain *= 0.5f;
            runSeconds (1.0); remeasure(); runSeconds (5.0);
            const auto said5 = vocal->loadUserPreset ("dynamics", "Level test");
            check (saved && near (vocal->getParam (ids::compThreshold), -30.0f, 1.5f) && said5.contains ("quieter"), "A Dynamics preset loaded on a track 6 dB quieter moves its thresholds down with it", said5);
            vocalGain = wasGain2;
            // loading a whole chain clears the name a tab was showing
            vocal->applyChainPreset (findChainPreset ("Clean slate"));
            check (vocal->getModulePresetName ("dynamics").isEmpty() && vocal->getModulePresetName ("eq").isEmpty(), "Loading a whole chain clears the names the tabs were showing");
            vocal->setParam (ids::role, (float) Role::Auto);
            runSeconds (1.0); remeasure(); runSeconds (3.0);
        }

        // nothing half-written is ever left behind
        int strays = 0; for (auto& f : presetRoot.findChildFiles (juce::File::findFiles, true)) if (! f.hasFileExtension (".mupreset")) ++strays;
        check (strays == 0, "Saving leaves no temporary files behind");
        UserPresets::setFolderForTesting ({});
        presetRoot.deleteRecursively();
        vocal->applyChainPreset (findChainPreset ("Clean slate"));
        vocal->setParam (ids::role, (float) Role::Auto);
    }

    // ---- tear down in a mixed order to exercise slot reuse
    beat.reset();
    check (master->getBus().getNumInstances() == 3, "Instance unregistered");
    vocal.reset(); bass.reset(); master.reset();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "SESSION HARNESS PASSED" : "SESSION HARNESS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
