#include "LocalAI.h"

namespace mu
{
LocalAI::LocalAI() : juce::Thread ("MU tutor") {}
LocalAI::~LocalAI() { stopThread (3000); }

LocalAI::Status LocalAI::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void LocalAI::refresh()
{
    if (isThreadRunning()) return;
    job.store (1);
    startThread();
}

void LocalAI::ask (const juce::String& systemPrompt, const juce::String& question)
{
    if (isThreadRunning()) return;
    {
        const juce::ScopedLock sl (lock);
        pendingSystem = systemPrompt; pendingQuestion = question;
        status.busy = true; status.error.clear();
    }
    job.store (2);
    startThread();
}

juce::String LocalAI::chooseModel (const juce::StringArray& installed, const juce::String& preferred)
{
    if (preferred.isNotEmpty() && installed.contains (preferred)) return preferred;
    // small general-purpose chat models first; anything is better than nothing
    for (auto* family : { "llama3.2", "gemma3", "llama3.1", "qwen", "phi", "mistral", "llama", "gemma" })
        for (auto& name : installed)
            if (name.startsWithIgnoreCase (family) && ! name.containsIgnoreCase ("embed")) return name;
    for (auto& name : installed) if (! name.containsIgnoreCase ("embed")) return name;
    return {};
}

juce::String LocalAI::buildRequest (const juce::String& model, const juce::String& systemPrompt, const juce::String& question)
{
    auto message = [] (const char* role, const juce::String& content)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("role", role); o->setProperty ("content", content);
        return juce::var (o);
    };
    auto* options = new juce::DynamicObject();
    options->setProperty ("temperature", 0.2);
    options->setProperty ("num_predict", 320);
    auto* root = new juce::DynamicObject();
    root->setProperty ("model", model);
    root->setProperty ("stream", false);
    root->setProperty ("messages", juce::Array<juce::var> { message ("system", systemPrompt), message ("user", question) });
    root->setProperty ("options", juce::var (options));
    return juce::JSON::toString (juce::var (root), true);
}

juce::String LocalAI::parseAnswer (const juce::String& json)
{
    const auto v = juce::JSON::parse (json);
    auto text = v.getProperty ("message", juce::var()).getProperty ("content", "").toString();
    // some models wrap their private reasoning in tags; students only need the answer
    while (text.contains ("<think>") && text.contains ("</think>"))
        text = text.upToFirstOccurrenceOf ("<think>", false, false) + text.fromFirstOccurrenceOf ("</think>", false, false);
    return text.replace ("**", "").trim();
}

void LocalAI::detect()
{
    int code = 0;
    juce::StringArray names;
    if (auto in = juce::URL (address + "/api/tags").createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                           .withConnectionTimeoutMs (1500).withStatusCode (&code)))
    {
        const auto v = juce::JSON::parse (in->readEntireStreamAsString());
        if (auto* models = v.getProperty ("models", juce::var()).getArray())
            for (auto& m : *models) names.add (m.getProperty ("name", "").toString());
    }
    const auto model = chooseModel (names, settings->get ("aiModel"));
    const juce::ScopedLock sl (lock);
    status.available = model.isNotEmpty();
    status.model = model;
    if (! status.available) status.error = names.isEmpty() ? "" : "Ollama is running but no chat model is installed.";
}

void LocalAI::run()
{
    const int j = job.exchange (0);
    if (j == 1) { detect(); return; }
    if (j != 2) return;

    if (! getStatus().available) detect();
    juce::String model, system, question;
    {
        const juce::ScopedLock sl (lock);
        model = status.model; system = pendingSystem; question = pendingQuestion;
    }
    juce::String answer, error;
    if (model.isEmpty()) error = "The AI tutor is not set up on this computer. See Settings.";
    else
    {
        int code = 0;
        const auto url = juce::URL (address + "/api/chat").withPOSTData (buildRequest (model, system, question));
        // No extra headers: Ollama reads the body as JSON whatever the content type says (its own curl examples send none).
        auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                             .withConnectionTimeoutMs (120000).withStatusCode (&code));
        if (in != nullptr) answer = parseAnswer (in->readEntireStreamAsString());
        if (answer.isEmpty()) error = "The AI tutor did not answer. Check that Ollama is still running.";
    }
    const juce::ScopedLock sl (lock);
    status.busy = false; status.answer = answer; status.error = error; ++status.answerId;
}
}
