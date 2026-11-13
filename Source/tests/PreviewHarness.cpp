// UI preview: a small session (vocal, beat, 808, master) fed with synthetic stems in real time, with one
// instance's editor shown in a window. Used for checking the interface with live data. Build target: MUPreview.
//   MUPreview [vocal|beat|bass|master] [tab 0-5] [preset name]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SyntheticStems.h"

using namespace mu;

class Session : private juce::Thread
{
public:
    Session() : juce::Thread ("stems")
    {
        const Role roles[] = { Role::Vocal, Role::Beat, Role::Bass808, Role::Master };
        for (auto r : roles)
        {
            auto p = std::make_unique<MUProcessor>();
            p->setPlayConfigDetails (2, 2, fs, block);
            p->prepareToPlay (fs, block);
            p->setParam (ids::role, (float) r);
            procs.push_back (std::move (p));
        }
        vocal.prepare ((float) fs); beat.prepare ((float) fs); bass.prepare ((float) fs);
        startThread();
    }
    ~Session() override { stopThread (2000); }
    MUProcessor& get (int i) { return *procs[(size_t) i]; }

private:
    void run() override
    {
        juce::AudioBuffer<float> v (2, block), b (2, block), k (2, block), mix (2, block);
        juce::MidiBuffer midi;
        double next = juce::Time::getMillisecondCounterHiRes();
        while (! threadShouldExit())
        {
            for (int i = 0; i < block; ++i)
            {
                const float vs = vocal.next() * 0.5f; v.setSample (0, i, vs); v.setSample (1, i, vs);
                float l, r; beat.next (l, r); b.setSample (0, i, l * 0.45f); b.setSample (1, i, r * 0.45f);
                const float ks = bass.next() * 0.25f; k.setSample (0, i, ks); k.setSample (1, i, ks);
            }
            procs[0]->processBlock (v, midi); procs[1]->processBlock (b, midi); procs[2]->processBlock (k, midi);
            mix.clear();
            for (auto* s : { &v, &b, &k }) for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, *s, c, 0, block);
            procs[3]->processBlock (mix, midi);
            next += 1000.0 * block / fs;
            const double wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 1.0) juce::Thread::sleep ((int) wait);
        }
    }
    const double fs = 48000.0; const int block = 480;
    std::vector<std::unique_ptr<MUProcessor>> procs;
    synth::Vocal vocal; synth::Beat beat; synth::Bass808 bass;
};

class PreviewApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MU Preview"; }
    const juce::String getApplicationVersion() override { return MU_VERSION_STRING; }
    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        const juce::String which = args.size() > 0 ? args[0] : "vocal";
        const int index = which == "beat" ? 1 : which == "bass" ? 2 : which == "master" ? 3 : 0;
        session = std::make_unique<Session>();
        auto& proc = session->get (index);
        if (args.size() > 1) proc.setCurrentTab (args[1].getIntValue());
        if (args.size() > 2) presetName = args[2].unquoted();
        window = std::make_unique<juce::DocumentWindow> ("Musically Unorthodox", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (false);
        window->setTitleBarHeight (0);
        window->setContentOwned (proc.createEditor(), true);
        window->setTopLeftPosition (0, 0);
        window->setVisible (true);
        if (presetName.isNotEmpty())
            juce::Timer::callAfterDelay (5000, [this, index] { const int i = findChainPreset (presetName); if (i >= 0) session->get (index).applyChainPreset (i); });
    }
    void shutdown() override { window.reset(); session.reset(); }

private:
    std::unique_ptr<Session> session;
    std::unique_ptr<juce::DocumentWindow> window;
    juce::String presetName;
};

START_JUCE_APPLICATION (PreviewApp)
