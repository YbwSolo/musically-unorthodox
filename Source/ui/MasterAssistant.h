#pragma once
#include "Widgets.h"

namespace mu
{
// The mastering assistant: one button that listens to the finished mix, decides which processors this song needs,
// sets them, and then measures the result. Everything it sets stays adjustable, here and on the other tabs.
class MasterAssistant : public juce::Component
{
public:
    MasterAssistant (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshReport();
    void pressMain();

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    juce::TextButton mainButton, againButton, undoButton, loadRefButton, playRefButton, clearRefButton;
    juce::ComboBox styleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt;
    Knob intensity, crossLow, lowThreshold, lowRatio, clipAmount, clipSoft;
    Segmented compare { juce::StringArray { "Master", "Level-matched", "Mix" } };
    std::array<std::unique_ptr<ParamToggle>, 7> switches;
    juce::TextEditor report;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String statusText, shownReport, blurb;
    juce::Rectangle<int> compareNote, leftArea, reportArea, rightArea, statusArea, blurbArea, refArea, styleLabel, refLabel, compareLabel, switchArea, knobLabelA, knobLabelB;
    std::array<juce::Rectangle<int>, 7> readouts;
    float slow = 0.0f;
    int shownStage = -1;
};
}
