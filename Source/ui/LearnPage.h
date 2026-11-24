#pragma once
#include "Widgets.h"
#include "../state/LocalSettings.h"

namespace mu
{
class MUProcessor;
class LessonStage;

// The Learn section: a course in five tracks (music theory, making a beat, Logic Pro, FL Studio, mixing). Each
// lesson is a few short steps under an example the student can see, hear and usually play with: a keyboard, a
// step grid, a piano roll, a tempo counter, or an animation drawn from the same maths the plugin uses.
// The AI tutor can be asked about the step on screen.
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

private:
    void showTrack (int track);
    void showLesson (int lesson, int step = 0);
    void showStep (int step);
    void askTutor (const juce::String& question);
    void showAnswer (bool show);
    void layoutText();
    int lessonAt (juce::Point<int> p) const;
    int trackAt (juce::Point<int> p) const;
    int stepDotAt (juce::Point<int> p) const;
    juce::Rectangle<int> lessonRow (int lesson) const;
    juce::Rectangle<int> trackTab (int track) const;
    int listHeight() const;
    bool isDone (int track, int lesson) const;
    void markDone();

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    std::function<void (int)> goToTab;
    juce::SharedResourcePointer<LocalSettings> settings;
    std::unique_ptr<LessonStage> stage;
    juce::TextButton prevButton, nextButton, tryButton, askButton, backButton, getTutorButton;
    std::array<juce::TextButton, 2> starters;
    juce::TextEditor askBox, answerView;
    juce::Rectangle<int> listArea, tabsArea, rowsArea, contentArea, textArea, dotsArea, tutorArea;
    juce::StringArray doneIds;
    juce::String askedQuestion;
    int track = 0, lesson = 0, step = 0, hover = -1, tabHover = -1, lastAnswerId = 0;
    float blockY = 0.0f, textFade = 1.0f, dotAnim = 0.0f, scrollY = 0.0f, stillFor = 0.0f, aiCheck = 0.0f;
    uint64_t lastSamples = 0;
    bool answerShown = false, waiting = false, tutorReady = false, audioRuns = true;
};
}
