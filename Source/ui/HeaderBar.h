#pragma once
#include "Widgets.h"

namespace mu
{
class HeaderBar : public juce::Component
{
public:
    enum Tab { Mix = 0, EQ, Dynamics, Tone, Tune, FX, Master, Learn, NumTabs };

    explicit HeaderBar (MUProcessor&);
    void setTab (int t);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (int)> onTabChanged;
    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onSettings;

private:
    void showPresetMenu();
    juce::Rectangle<float> tabBounds (int t) const;
    int tabAt (juce::Point<int> p) const;

    MUProcessor& proc;
    juce::Image logo;
    int tab = 0, hoverTab = -1;
    float blockX = 0.0f, blockW = 0.0f;
    bool blockPlaced = false;
    bool shownLowLatency = false;
    int shownCheck = 0;
    juce::Rectangle<int> tabArea;
    juce::TextButton presetButton, updateButton;
    Segmented abSwitch { juce::StringArray { "A", "B" } };
    IconButton bypassButton { "bypass", "header.bypass" };
    juce::TextButton settingsButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::String shownPreset;
    int shownAB = -1, shownUpdate = -1;
    float updatePulse = 0.0f;
};
}
