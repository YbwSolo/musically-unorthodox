#pragma once
#include "Widgets.h"

namespace mu
{
// Overlay opened from the header: track role and balance on the left; updates and the AI tutor on the right.
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (MUProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    std::function<void (float)> onScaleChanged;
    std::function<void()> onClose;

private:
    MUProcessor& proc;
    juce::Rectangle<int> card, updateText, updateBar, notesArea, aiText, aiBar;
    juce::ComboBox roleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAtt;
    Segmented sizeSwitch { juce::StringArray { "Compact", "Standard", "Large" } };
    ParamToggle takePart;
    juce::TextButton closeButton, resetButton, checkButton, installButton, aiButton;
    juce::OwnedArray<juce::Slider> offsetSliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> offsetAtts;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels;
    juce::String detected, updateLine, notes, aiLine;
    float progress = 0.0f, aiProgress = 0.0f;
    bool showProgress = false, showAiProgress = false;
};
}
