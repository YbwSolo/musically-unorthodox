#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

namespace mu
{
class HeaderBar : public juce::Component
{
public:
    enum Tab { Mix = 0, EQ, Comp, Chain, Levels, Help, Settings, NumTabs };

    explicit HeaderBar (MUProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setTab (int t);
    int getTab() const { return currentTab; }
    void refresh();   // called by the editor timer (role, detected role, preset name)

    std::function<void (int)> onTabChanged;
    std::function<void (const juce::String&)> onStatus;   // short message after applying a chain

private:
    juce::Rectangle<int> tabRect (int i) const;

    MUProcessor& proc;
    juce::Image logo;
    juce::ComboBox roleBox, chainBox;
    juce::ToggleButton bypass { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::Rectangle<int> tabsArea;
    int currentTab = 0, hoverTab = -1;
    juce::String detectedText;
};
}
