#pragma once
#include "Widgets.h"

namespace mu
{
class LessonDiagram;

// Short lessons with diagrams you can drag, and a button that takes each idea to your own track.
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
    void showLesson (int i);
    int lessonAt (juce::Point<int> p) const;

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    std::function<void (int)> goToTab;
    std::unique_ptr<LessonDiagram> diagram;
    juce::TextButton tryButton;
    juce::Rectangle<int> listArea, contentArea, textArea;
    int current = 0, hover = -1;
    float blockY = 0.0f, fade = 1.0f;
};
}
