// Runs the built-in AI tutor end to end, the way the plugin does: installs it from a release feed, starts the
// helper program, and asks questions with a real tutor prompt. Used on the Mac build machine so every release
// proves the tutor works with the real helper and the real model.
//   MUTutorCheck <feed base url> <prompt file> [question ...]
// MU_TUTOR_DIR chooses where the tutor is installed; MU_TUTOR_ARGS adds arguments for the helper.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "ai/LocalAI.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 3) { std::printf ("usage: MUTutorCheck <feed base url> <prompt file> [question ...]\n"); return 2; }
    const juce::String feed (argv[1]);
    const auto prompt = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])).loadFileAsString();
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_DIR", juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-tutor-check").getFullPathName()));
    juce::StringArray questions;
    for (int i = 3; i < argc; ++i) questions.add (argv[i]);
    if (questions.isEmpty()) questions = { "Why does my vocal sound muddy and what should I do about it?", "What does the attack knob on the compressor do?" };
    if (prompt.length() < 200) { std::printf ("FAIL  prompt file is missing or empty\n"); return 1; }

    using Setup = mu::LocalAI::Setup;
    mu::LocalAI ai;
    ai.setAddressForTesting ("http://127.0.0.1:9");            // no Ollama here: only the built-in tutor counts
    ai.configureForTesting (feed, folder, juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_ARGS", {}));

    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    ai.installBuiltIn();
    juce::String lastLine;
    for (int i = 0; i < 36000 && ai.getStatus().setup == Setup::Downloading; ++i)
    {
        juce::Thread::sleep (100);
        const auto st = ai.getStatus();
        if (i % 100 == 0 && st.setupMessage != lastLine) { lastLine = st.setupMessage; std::printf ("  %s\n", lastLine.toRawUTF8()); std::fflush (stdout); }
    }
    juce::Thread::sleep (300);
    auto st = ai.getStatus();
    if (st.setup != Setup::Ready || ! st.available || ! st.builtIn)
    { std::printf ("FAIL  install: %s\n", st.setupMessage.toRawUTF8()); return 1; }
    std::printf ("PASS  installed \"%s\" in %.0f s\n", st.model.toRawUTF8(), (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);

    int failures = 0, id = 0;
    for (auto& q : questions)
    {
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        ai.ask (prompt, q);
        for (int i = 0; i < 9000 && ai.getStatus().answerId == id; ++i) juce::Thread::sleep (100);
        st = ai.getStatus();
        id = st.answerId;
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - t1) / 1000.0;
        if (st.answer.length() < 40) { ++failures; std::printf ("FAIL  \"%s\": %s\n", q.toRawUTF8(), (st.answer + st.error).toRawUTF8()); }
        else std::printf ("PASS  \"%s\" answered in %.0f s (%d words):\n%s\n\n", q.toRawUTF8(), seconds, juce::StringArray::fromTokens (st.answer, " \n", "").size(), st.answer.toRawUTF8());
        std::fflush (stdout);
    }
    std::printf (failures == 0 ? "TUTOR CHECK PASSED\n" : "TUTOR CHECK FAILED\n");
    return failures == 0 ? 0 : 1;
}
