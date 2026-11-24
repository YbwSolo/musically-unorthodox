#pragma once
#include "Widgets.h"
#include "../state/LocalSettings.h"
#include "../dsp/TempoDetector.h"

namespace mu
{
class MUProcessor;
class LessonStage;
class LessonOutput;

// The Learn section. It opens on a home page: one card per course (music theory, making a beat, Logic Pro,
// FL Studio, mixing, practice, tools) with progress, a search box and a button that carries on where the student
// stopped. Inside a course, each lesson is a few short steps under an example that can be seen, heard and usually
// played with, and the AI tutor can be asked about the step on screen.
// The examples make their own sound. While the DAW is running the plugin they play through the track; while it is
// stopped they play straight to the computer's sound output, so nothing has to be loaded or played first.
class LearnPage : public juce::Component
{
public:
    LearnPage (MUProcessor&, std::function<void (const juce::String&)> status, std::function<void (int)> goToTab);
    ~LearnPage() override;
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void open (int track, int lesson, int step);          // for the preview harness and the tests
    void showHome();
    void search (const juce::String& words);
    int resultCount() const { return (int) results.size(); }
    juce::String resultTitle (int i) const;

private:
    struct Result { int track, lesson, step, rank; };
    void showTrack (int track);
    void showLesson (int lesson, int step = 0);
    void showStep (int step);
    void askTutor (const juce::String& question);
    void showAnswer (bool show);
    void routeSound (float dt);
    void paintHome (juce::Graphics&);
    void paintLessons (juce::Graphics&);
    int lessonAt (juce::Point<int> p) const;
    int cardAt (juce::Point<int> p) const;
    int resultAt (juce::Point<int> p) const;
    int stepDotAt (juce::Point<int> p) const;
    juce::Rectangle<int> lessonRow (int lesson) const;
    juce::Rectangle<int> cardRect (int card) const;       // the seven courses, then the "where to start" card
    juce::Rectangle<int> resultRow (int i) const;
    int listHeight() const;
    int doneIn (int track) const;
    bool isDone (int track, int lesson) const;
    void markDone();

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    std::function<void (int)> goToTab;
    juce::SharedResourcePointer<LocalSettings> settings;
    std::unique_ptr<LessonStage> stage;
    std::unique_ptr<LessonOutput> output;
    juce::TextButton prevButton, nextButton, tryButton, askButton, backButton, getTutorButton, homeButton, continueButton;
    std::array<juce::TextButton, 2> starters;
    juce::TextEditor askBox, answerView, searchBox;
    juce::Rectangle<int> listArea, rowsArea, contentArea, textArea, dotsArea, tutorArea, homeArea, cardsArea;
    juce::StringArray doneIds;
    juce::String askedQuestion;
    std::vector<Result> results;
    TempoReading tempoNow;
    int track = 0, lesson = 0, step = 0, hover = -1, cardHover = -1, lastAnswerId = 0;
    float blockY = 0.0f, textFade = 1.0f, dotAnim = 0.0f, scrollY = 0.0f, stillFor = 0.0f, aiCheck = 0.0f, quietFor = 0.0f, tempoCheck = 0.0f;
    uint64_t lastSamples = 0;
    bool home = true, answerShown = false, waiting = false, tutorReady = false, audioRuns = true, started = false;
};
}
