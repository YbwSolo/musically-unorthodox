#pragma once
#include "Widgets.h"
#include "../state/LocalSettings.h"

namespace mu
{
class LessonStage;

// Step-by-step lessons. Each step is a couple of sentences under an animated example that is drawn from the
// same maths the plugin uses, so what moves on screen is what the processing really does.
class LearnPage : public juce::Component
{
public:
    LearnPage (MUProcessor&, std::function<void (const juce::String&)> status, std::function<void (int)> goToTab);
    ~LearnPage() override;
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void showLesson (int lesson, int step = 0);
    void showStep (int step);
    int lessonAt (juce::Point<int> p) const;
    int stepDotAt (juce::Point<int> p) const;
    juce::Rectangle<int> lessonRow (int lesson) const;
    bool isDone (int lesson) const;

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    std::function<void (int)> goToTab;
    juce::SharedResourcePointer<LocalSettings> settings;
    std::unique_ptr<LessonStage> stage;
    juce::TextButton prevButton, nextButton, tryButton;
    juce::Rectangle<int> listArea, contentArea, textArea, dotsArea;
    int lesson = 0, step = 0, hover = -1;
    float blockY = 0.0f, textFade = 1.0f, dotAnim = 0.0f;
};
}
