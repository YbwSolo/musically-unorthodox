#pragma once
#include <juce_core/juce_core.h>

namespace mu
{
// What the AI tutor is told. A small model knows little about mixing and nothing about this plugin, so every
// question is answered from notes: a fixed core (rules, where things are, the facts the lessons teach) plus the
// few notes from the guide below that match the question. The guide is the plugin's own help text and a set of
// mixing notes written for this plugin, so the tutor's answers agree with the rest of the plugin.
namespace tutor
{
juce::String corePrompt();

// The notes that best match a question, ready to drop into the prompt. Empty when nothing matches.
juce::String notesFor (const juce::String& question, int maxMixingNotes = 3, int maxControlNotes = 2);

// For the tests: the titles of the notes that would be chosen, best first.
juce::StringArray noteTitlesFor (const juce::String& question, int maxMixingNotes = 3, int maxControlNotes = 2);
int numMixingNotes();
}
}
