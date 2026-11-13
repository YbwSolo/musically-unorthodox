#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

namespace mu
{
class HeaderBar : public juce::Component
{
public:
    enum Tab { Mix = 0, EQ, Comp, Levels, Settings, NumTabs };

    explicit HeaderBar (MUProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setTab (int t);
    int getTab() const { return currentTab; }
    void refresh();   // called by the editor timer (role, pro badge)

    std::function<void (int)> onTabChanged;

private:
    MUProcessor& proc;
    juce::Image logo;
    juce::ComboBox roleBox;
    juce::ToggleButton bypass { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    std::array<juce::TextButton, NumTabs> tabs;
    int currentTab = 0;
    juce::String detectedText;
    bool pro = false;
};
}
