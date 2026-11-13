#pragma once
#include "Widgets.h"

namespace mu
{
class MasterPage : public juce::Component
{
public:
    MasterPage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Check { int state = 0; juce::String title, detail; };   // 0 not measured, 1 good, 2 needs attention
    void updateChecks();
    int cardAt (juce::Point<int> p) const;

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    Section limiter { "Limiter" }, loudness { "Loudness" }, checks { "Release check" };
    Knob drive, ceiling, target;
    ParamToggle autoLevel;
    juce::TextButton makeMaster, resetMeasure;
    std::array<juce::Rectangle<int>, 4> cards;
    juce::Rectangle<int> historyWell, grWell, readoutArea, checkArea, bannerArea;
    std::array<Check, 5> items;
    std::array<float, 4> cardGlow {};
    int hoverCard = -1;
    float slow = 0.0f, integrated = -100.0f;
    bool isMaster = false;
};
}
