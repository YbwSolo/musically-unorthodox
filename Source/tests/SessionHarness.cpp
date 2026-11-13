// Offline multi-instance harness: three track instances + a master, fed with synthetic stems,
// then Balance Now is run through the real SessionBus. Build target: MUSession.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "PluginProcessor.h"

using namespace mu;

static int failures = 0;
static void check (bool ok, const char* name, const juce::String& detail = {})
{
    std::printf ("%s  %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.isEmpty() ? "" : "  -- ", detail.toRawUTF8());
    if (! ok) ++failures;
}

struct Stem
{
    std::unique_ptr<MUProcessor> proc;
    float hz, ampl;
    Role role;
    juce::AudioBuffer<float> out;
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const int block = 512;

    std::vector<Stem> stems;
    auto add = [&] (Role r, float hz, float ampl)
    {
        Stem s; s.proc = std::make_unique<MUProcessor>(); s.hz = hz; s.ampl = ampl; s.role = r;
        s.proc->setPlayConfigDetails (2, 2, fs, block);
        s.proc->prepareToPlay (fs, block);
        if (auto* p = s.proc->apvts.getParameter (ids::role)) p->setValueNotifyingHost (p->convertTo0to1 ((float) r));
        s.out.setSize (2, block);
        stems.push_back (std::move (s));
    };
    add (Role::Vocal,   440.0f,  0.08f);    // quiet-ish vocal
    add (Role::Beat,    220.0f,  0.22f);    // loud beat
    add (Role::Bass808, 55.0f,   0.05f);    // quiet 808
    auto master = std::make_unique<MUProcessor>();
    master->setPlayConfigDetails (2, 2, fs, block);
    master->prepareToPlay (fs, block);
    if (auto* p = master->apvts.getParameter (ids::role)) p->setValueNotifyingHost (p->convertTo0to1 ((float) Role::Master));

    check (master->getBus().getNumInstances() == 4, "Four instances on the bus");
    check (master->getLatencySamples() > 0 && master->getLatencySamples() == stems[0].proc->getLatencySamples(), "Every instance reports the same fixed latency", juce::String (master->getLatencySamples()) + " samples");
    check (master->getBus().getMasterSlot() == master->getSessionSlot(), "Master slot found");

    juce::MidiBuffer midi;
    juce::AudioBuffer<float> mix (2, block);
    auto runSeconds = [&] (double seconds)
    {
        const int blocks = (int) (seconds * fs / block);
        static long n = 0;
        for (int b = 0; b < blocks; ++b)
        {
            mix.clear();
            for (auto& s : stems)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < block; ++i)
                        s.out.setSample (c, i, s.ampl * std::sin (2.0f * juce::MathConstants<float>::pi * s.hz * (float) (n + i) / (float) fs));
                s.proc->processBlock (s.out, midi);
                for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, s.out, c, 0, block);
            }
            master->processBlock (mix, midi);
            n += block;
            if (b % 20 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);   // let timers tick
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    };

    runSeconds (6.0);

    auto tracks = master->getBus().collectTracks();
    check (tracks.size() == 4, "collectTracks sees everyone", juce::String ((int) tracks.size()));
    for (auto& t : tracks)
        std::printf ("   %-14s role=%-14s level=%7.2f LUFS  tp=%6.2f  active=%d\n", t.name.toRawUTF8(), roleNames()[(int) t.role].toRawUTF8(), t.levelLufs, t.truePeakDb, (int) t.active);

    const float masterBefore = master->getLoudness().getIntegrated();
    check (masterBefore > -40.0f, "Master is measuring", juce::String (masterBefore, 2));

    auto plan = master->getBus().computePlan ({ -18.0f, -6.0f, 12.0f });
    check (plan.usable, "Plan usable", plan.summary);
    master->balanceNow();
    runSeconds (0.5);
    float vocalTrim = stems[0].proc->getTrimDb(), beatTrim = stems[1].proc->getTrimDb(), bassTrim = stems[2].proc->getTrimDb();
    std::printf ("   trims: vocal %.2f  beat %.2f  808 %.2f\n", vocalTrim, beatTrim, bassTrim);
    check (beatTrim < vocalTrim, "Beat trimmed below vocal");
    check (bassTrim > beatTrim, "808 lifted relative to beat");
    check (std::abs (master->getTrimDb()) < 0.01f, "Master trim untouched");

    // Re-measure from scratch and confirm the master lands near the target.
    for (auto& s : stems) s.proc->resetMeters();
    master->resetMeters();
    runSeconds (6.0);
    const float masterAfter = master->getLoudness().getIntegrated();
    check (std::abs (masterAfter - (-18.0f)) < 1.5f, "Master lands near -18 LUFS after balance", juce::String (masterAfter, 2));
    const float v = stems[0].proc->getLoudness().getIntegrated(), b = stems[1].proc->getLoudness().getIntegrated(), k = stems[2].proc->getLoudness().getIntegrated();
    std::printf ("   after: vocal %.2f  beat %.2f  808 %.2f  master %.2f\n", v, b, k, masterAfter);
    check (std::abs ((b - v) - (-1.5f)) < 1.0f, "Beat sits ~1.5 LU under vocal", juce::String (b - v, 2));

    // Chain presets load and fit thresholds to the measured level
    {
        const auto msg = stems[0].proc->applyChainPreset (1);   // MU Lead Vocal
        check (msg.contains ("fitted"), "Lead vocal chain fits thresholds to the measured level", msg);
        check ((int) stems[0].proc->getParam (ids::role) == (int) Role::Vocal, "Chain preset sets the role");
        check (stems[0].proc->getParam (ids::compEnabled) > 0.5f && stems[0].proc->getParam (ids::dsOn) > 0.5f && stems[0].proc->getParam (ids::riderOn) > 0.5f, "Lead vocal chain switches its modules on");
        stems[1].proc->applyChainPreset (6);   // MU Beat
        check (stems[1].proc->getParam (ids::vsEnabled) > 0.5f, "Beat chain turns Vocal Space on");
        master->applyChainPreset (8);          // MU Master Prep
        check (master->getParam (ids::peakGuard) > 0.5f && master->getParam (ids::masterAutoTrim) > 0.5f, "Master prep arms peak guard and auto trim");
    }

    // Master auto trim converges on the target even if the balance left a residual error
    {
        master->setParam (ids::targetLufs, -16.0f);
        for (auto& s : stems) s.proc->resetMeters();
        master->resetMeters();
        runSeconds (12.0);
        const float integ = master->getLoudness().getIntegrated();
        check (std::abs (integ - (-16.0f)) < 0.7f, "Master auto trim lands on a new target", juce::String (integ, 2) + " (trim " + juce::String (master->getMasterAutoTrimDb(), 2) + " dB)");
        check (master->getLoudness().getTruePeakHold() <= -6.0f + 0.3f, "Peak guard keeps the master under the ceiling", juce::String (master->getLoudness().getTruePeakHold(), 2));
        master->setParam (ids::targetLufs, -18.0f);
    }

    // Vocal space key: the vocal instance is loud, so beat instances should see a key level > 0.
    check (master->getBus().getVocalKeyLevel() > 0.3f, "Vocal key level present", juce::String (master->getBus().getVocalKeyLevel(), 2));

    // State round trip on the master
    juce::MemoryBlock state;
    master->setAutoBalanceMode (true);
    master->getStateInformation (state);
    auto copy = std::make_unique<MUProcessor>();
    copy->setStateInformation (state.getData(), (int) state.getSize());
    check ((int) copy->apvts.getRawParameterValue (ids::role)->load() == (int) Role::Master, "State restores role");
    check (copy->getAutoBalanceMode(), "State restores auto-balance mode");
    copy.reset();

    // Tear down in a mixed order to exercise slot reuse
    stems.erase (stems.begin() + 1);
    check (master->getBus().getNumInstances() == 3, "Instance unregistered");
    stems.clear();
    master.reset();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "SESSION HARNESS PASSED" : "SESSION HARNESS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
