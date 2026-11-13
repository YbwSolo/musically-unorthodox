#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "../state/LocalSettings.h"

namespace mu
{
// Optional tutor that answers questions in plain language. It talks to an AI model running on this computer
// through Ollama (free, nothing leaves the machine). If Ollama is not installed the feature simply stays off.
class LocalAI : private juce::Thread
{
public:
    struct Status
    {
        bool available = false, busy = false;
        juce::String model, answer, error;
        int answerId = 0;         // goes up each time a new answer or error arrives
    };

    LocalAI();
    ~LocalAI() override;
    void refresh();                                                        // look for Ollama and pick a model
    void ask (const juce::String& systemPrompt, const juce::String& question);
    Status getStatus() const;
    void setAddressForTesting (const juce::String& a) { address = a; }

    static juce::String chooseModel (const juce::StringArray& installed, const juce::String& preferred);
    static juce::String buildRequest (const juce::String& model, const juce::String& systemPrompt, const juce::String& question);
    static juce::String parseAnswer (const juce::String& json);

private:
    void run() override;
    void detect();
    mutable juce::CriticalSection lock;
    Status status;
    juce::String address { "http://127.0.0.1:11434" }, pendingSystem, pendingQuestion;
    std::atomic<int> job { 0 };       // 1 detect, 2 ask
    juce::SharedResourcePointer<LocalSettings> settings;
};
}
