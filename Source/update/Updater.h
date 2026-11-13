#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "../state/LocalSettings.h"

namespace mu
{
struct UpdateInfo
{
    int build = 0;
    juce::String version, file, sha256, notes;
};

struct UpdateState
{
    enum class Phase { Idle, Checking, UpToDate, Available, Downloading, Installing, Installed, Failed };
    Phase phase = Phase::Idle;
    UpdateInfo latest;
    float progress = 0.0f;
    juce::String message;
};

// Checks the release feed for a newer build, downloads it, verifies it and replaces the installed plugin
// bundles in place. The new version loads the next time the host starts. One per process, shared by every instance.
class Updater : private juce::Thread
{
public:
    Updater();
    ~Updater() override;

    static int currentBuild();
    static juce::String currentVersion();

    void checkNow();
    void autoCheck();            // at most once every six hours per computer
    void installNow();
    UpdateState getState() const;

    // Pure helpers, covered by the tests.
    static bool parseFeed (const juce::String& json, UpdateInfo& out);
    static bool replaceTree (const juce::File& source, const juce::File& destination, juce::String& error);
    static juce::String sha256Of (const juce::File& f);

    // Test hook: point the updater at a local feed and at scratch install folders.
    void configureForTesting (const juce::String& feedBaseUrl, const juce::File& auDestination, const juce::File& vst3Destination, int pretendBuild);

private:
    enum class Job { None, Check, Install };
    void run() override;
    void doCheck();
    void doInstall();
    void set (UpdateState::Phase p, const juce::String& message = {}, float progress = 0.0f);
    bool download (const juce::String& url, const juce::File& target, juce::String& error);
    juce::File destinationFor (const juce::String& folder, const juce::String& bundleName, const juce::File& testOverride) const;

    mutable juce::CriticalSection lock;
    UpdateState state;
    std::atomic<int> job { 0 };
    juce::String baseUrl;
    juce::File testAU, testVST3;
    int buildOverride = -1;
    juce::SharedResourcePointer<LocalSettings> settings;
};
}
