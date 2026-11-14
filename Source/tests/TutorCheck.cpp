// Runs the built-in AI tutor end to end, the way the plugin does: installs it from a release feed, starts the
// helper program, and asks questions with a real tutor prompt. Used on the Mac build machine so every release
// proves the tutor works with the real helper and the real model.
//   MUTutorCheck <feed base url> <track notes file> <facts file> [questions file]
// The track notes and facts files are a snapshot of a real session, written by the session harness.
// The questions file has one question per line; a line starting with "+ " is a follow-up to the one before.
// MU_TUTOR_DIR chooses where the tutor is installed; MU_TUTOR_ARGS adds arguments for the helper.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include "ai/LocalAI.h"
#include "ai/TutorKnowledge.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 4) { std::printf ("usage: MUTutorCheck <feed base url> <track notes file> <facts file> [questions file]\n"); return 2; }
    const juce::String feed (argv[1]);
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto trackNotes = cwd.getChildFile (juce::String (argv[2])).loadFileAsString();
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_DIR", juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-tutor-check").getFullPathName()));
    juce::StringArray questions;
    juce::StringPairArray facts;
    for (auto& line : juce::StringArray::fromLines (cwd.getChildFile (juce::String (argv[3])).loadFileAsString()))
        if (line.containsChar ('=')) facts.set (line.upToFirstOccurrenceOf ("=", false, false), line.fromFirstOccurrenceOf ("=", false, false));
    if (argc > 4) questions.addLines (cwd.getChildFile (juce::String (argv[4])).loadFileAsString());
    questions.removeEmptyStrings();
    if (questions.isEmpty()) questions = { "Why does my vocal sound muddy and what should I do about it?", "What does the attack knob on the compressor do?" };
    if (trackNotes.length() < 200) { std::printf ("FAIL  track notes file is missing or empty\n"); return 1; }

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

    int failures = 0, id = 0, conversation = 0;
    juce::String previous;
    for (auto line : questions)
    {
        const bool followUp = line.startsWith ("+ ");
        const auto q = followUp ? line.substring (2).trim() : line.trim();
        if (! followUp) ++conversation;
        const auto lookUp = followUp ? previous + " " + q : q;
        previous = q;
        const auto prompt = mu::tutor::corePrompt() + mu::tutor::notesFor (lookUp, facts) + trackNotes;
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        // On a build machine with no graphics chip, loading a 4 GB model and reading the notes for the first time
        // can take longer than the plugin waits for an answer. The plugin then says "Try asking again", and the
        // second ask is quick because the model is in memory by then. The check does what a person would: it
        // asks once more. A tutor that is really broken fails both times.
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            ai.ask (prompt, q, conversation);
            for (int i = 0; i < 9000 && ai.getStatus().answerId == id; ++i) juce::Thread::sleep (100);
            st = ai.getStatus();
            id = st.answerId;
            if (st.answer.length() >= 40) break;
            if (attempt == 0) { std::printf ("  no answer in time to \"%s\" (%s); asking once more\n", q.toRawUTF8(), st.error.toRawUTF8()); std::fflush (stdout); }
        }
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - t1) / 1000.0;
        const auto notes = mu::tutor::noteTitlesFor (lookUp).joinIntoString (" | ");
        if (st.answer.length() < 40) { ++failures; std::printf ("FAIL  \"%s\": %s\n", q.toRawUTF8(), (st.answer + st.error).toRawUTF8()); }
        else std::printf ("%s \"%s\"  [%.0f s, %d words, notes: %s]\n%s\n\n", followUp ? "  +  " : "PASS ", q.toRawUTF8(), seconds,
                          juce::StringArray::fromTokens (st.answer, " \n", "").size(), notes.isEmpty() ? "none" : notes.toRawUTF8(), st.answer.toRawUTF8());
        std::fflush (stdout);
    }
    std::printf (failures == 0 ? "TUTOR CHECK PASSED\n" : "TUTOR CHECK FAILED\n");
    return failures == 0 ? 0 : 1;
}
