// UI preview: a small session (vocal, beat, 808, master) fed with synthetic stems in real time, with one
// instance's editor shown in a window. Used for checking the interface with live data. Build target: MUPreview.
//   MUPreview [vocal|beat|bass|master] [tab 0-5] [preset name]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SyntheticStems.h"

using namespace mu;

// The song position a host would report: playing, 120 BPM, counting up from bar one.
struct PreviewHead : juce::AudioPlayHead
{
    std::atomic<double> seconds { 0.0 };
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        const double t = seconds.load();
        p.setBpm (120.0); p.setIsPlaying (true); p.setTimeInSeconds (t); p.setTimeInSamples ((juce::int64) (t * 48000.0)); p.setPpqPosition (t * 2.0);
        return p;
    }
};

class Session : private juce::Thread
{
public:
    Session() : juce::Thread ("stems")
    {
        const char* const names[] = { "rap", "beat 120 bpm Gmin", "808", "Stereo Out" };
        for (int i = 0; i < 4; ++i)
        {
            auto p = std::make_unique<MUProcessor>();
            p->setPlayConfigDetails (2, 2, fs, block);
            p->prepareToPlay (fs, block);
            p->setPlayHead (&head);
            juce::AudioProcessor::TrackProperties props; props.name = names[i];
            p->updateTrackProperties (props);                 // roles are left on Auto: the names decide
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
            head.seconds.store (head.seconds.load() + (double) block / fs);
            next += 1000.0 * block / fs;
            const double wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 1.0) juce::Thread::sleep ((int) wait);
        }
    }
    const double fs = 48000.0; const int block = 480;
    PreviewHead head;
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
        if (presetName == "graph")
        {
            // capture six seconds of the vocal, then put the notes on the scale
            juce::Timer::callAfterDelay (1500, [this, index] { session->get (index).setParam (ids::tuneKey, 7.0f); session->get (index).setParam (ids::tuneScale, 2.0f); session->get (index).startCapture(); });
            juce::Timer::callAfterDelay (8000, [this, index] { session->get (index).stopCapture(); });
            juce::Timer::callAfterDelay (9000, [this, index] { session->get (index).correctAllNotes (0.35f); });
        }
        else if (presetName.startsWith ("sound:"))
            juce::Timer::callAfterDelay (3000, [this, index] { const auto& all = soundPresets(); for (size_t i = 0; i < all.size(); ++i) if (all[i].name == presetName.fromFirstOccurrenceOf (":", false, false)) session->get (index).applySound ((int) i); });
        else if (presetName.isNotEmpty())
            juce::Timer::callAfterDelay (5000, [this, index] { const int i = findChainPreset (presetName); if (i >= 0) session->get (index).applyChainPreset (i); });
    }
    void shutdown() override { window.reset(); session.reset(); }

private:
    std::unique_ptr<Session> session;
    std::unique_ptr<juce::DocumentWindow> window;
    juce::String presetName;
};

START_JUCE_APPLICATION (PreviewApp)
