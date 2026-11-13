#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <utility>
#include <vector>
#include "../state/LocalSettings.h"

namespace mu
{
// What the release feed says about the built-in tutor: a small helper program and the language model it runs.
struct TutorPack
{
    juce::String version, file, sha256;                 // helper: a zip published next to latest.json
    juce::String modelName, modelUrl, modelSha256;      // model: one large file, fetched once
    juce::int64 modelBytes = 0;
};

// Optional tutor that answers questions in plain language, using an AI model that runs on this computer.
// Nothing leaves the machine and nothing costs money. Two ways to get a model, tried in this order:
//   1. the built-in tutor: one button downloads a helper program and a model, and the plugin starts it when asked
//   2. Ollama, if the person already has it running
// One per process, shared by every instance of the plugin.
class LocalAI : private juce::Thread
{
public:
    enum class Setup { Unsupported, NotInstalled, Downloading, Ready, Failed };

    struct Status
    {
        bool available = false, busy = false, builtIn = false;
        juce::String model, answer, error;
        int answerId = 0;         // goes up each time a new answer or error arrives
        Setup setup = Setup::NotInstalled;
        float progress = 0.0f;
        juce::String setupMessage;
    };

    LocalAI();
    ~LocalAI() override;
    void refresh();                                                        // look for the built-in tutor, then Ollama
    // conversation: any number that identifies who is asking. Follow-up questions from the same asker within
    // ten minutes are sent together with the last two questions and answers, so "why?" makes sense.
    void ask (const juce::String& systemPrompt, const juce::String& question, int conversation = 0);
    void forgetConversation();
    void installBuiltIn();                                                 // download the helper and the model
    void cancelInstall();
    void removeBuiltIn();
    Status getStatus() const;

    // Pure helpers, covered by the tests.
    static juce::String chooseModel (const juce::StringArray& installed, const juce::String& preferred);
    using Earlier = std::vector<std::pair<juce::String, juce::String>>;       // question, answer
    static juce::String buildRequest (const juce::String& model, const juce::String& systemPrompt, const juce::String& question, const Earlier& earlier = {});
    static juce::String buildChatRequest (const juce::String& systemPrompt, const juce::String& question, const Earlier& earlier = {});
    static juce::String parseAnswer (const juce::String& json);
    static bool parseTutorFeed (const juce::String& json, TutorPack& out, bool anyModelHost = false);
    static juce::String sha256Of (const juce::File& f);

    // Test hooks.
    void setAddressForTesting (const juce::String& a) { address = a; }
    void configureForTesting (const juce::String& feedBaseUrl, const juce::File& folder, const juce::String& extraHelperArgs = {});

private:
    enum Job { None = 0, Detect, Ask, Install };
    void run() override;
    void detect();
    void doAsk();
    void doInstall();
    void setSetup (Setup s, const juce::String& message, float progress = 0.0f);
    bool fetch (const juce::String& url, const juce::File& target, juce::int64 expectedBytes, float from, float to, const juce::String& label, juce::String& error);
    bool unpack (const juce::File& zip, const juce::File& into);
    bool ensureServer (juce::String& error);
    bool startServer (int port, bool cpuOnly);
    void stopServer();
    juce::StringArray candidateFolders() const;
    juce::File installFolder() const;
    bool findInstalled (juce::File& helper, juce::File& model, juce::String& modelName) const;

    mutable juce::CriticalSection lock;
    Status status;
    juce::String address { "http://127.0.0.1:11434" }, pendingSystem, pendingQuestion;
    juce::String feedBase, extraArgs;
    juce::File testFolder;
    bool testing = false;
    std::atomic<int> job { None };

    juce::ChildProcess server;       // the helper, when this process started it
    int serverPort = 0;
    juce::File helperPath, modelPath;
    Earlier history;
    int historyOwner = 0;
    juce::int64 historyTime = 0;
    juce::SharedResourcePointer<LocalSettings> settings;
};
}
