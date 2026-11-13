#pragma once
#include "Widgets.h"
#include "../session/SessionBus.h"
#include <map>

namespace mu
{
class MixPage : public juce::Component
{
public:
    MixPage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    struct Lane
    {
        TrackInfo info;
        float level = -70.0f, move = 0.0f, appear = 0.0f;
        std::array<float, kAnalysisBands> heat {};
        juce::Rectangle<int> bounds, lockArea;
    };
    void refreshData();
    void drawDial (juce::Graphics&, juce::Rectangle<float>);
    void drawLanes (juce::Graphics&);
    void drawCollisions (juce::Graphics&);

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    juce::TextButton learnButton, balanceButton;
    juce::ToggleButton keepBalanced;
    std::array<juce::TextButton, 4> fixButtons;
    std::vector<Lane> lanes;
    std::map<int, Lane> laneState;
    std::vector<MaskingClash> clashes;
    BalancePlan plan;
    LearnState learn;
    juce::Rectangle<int> dialArea, actionArea, lanesArea, collisionArea;
    float march = 0.0f, refreshTimer = 0.0f, dialValue = -60.0f, dialPeak = -60.0f, learnProgress = 0.0f, pulse = 0.0f;
    bool hasMaster = false;
    float mixLufs = -100.0f, mixPeak = -100.0f, targetLufs = -18.0f, targetPeak = -6.0f;
};
}
