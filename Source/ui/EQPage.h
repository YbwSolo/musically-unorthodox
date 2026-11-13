#pragma once
#include "Widgets.h"
#include "../analysis/TonalAnalysis.h"
#include "../dsp/SpectrumAnalyzer.h"

namespace mu
{
class EQGraph;
class BandControls;
class BandChips;
class SuggestionList;

class EQPage : public juce::Component
{
public:
    EQPage (MUProcessor&, std::function<void (const juce::String&)> status);
    ~EQPage() override;
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;
    void selectBand (int b);
    void visibilityChanged() override;

private:
    void runAnalysis();
    void refreshReference();

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    int selected = 2;

    std::unique_ptr<EQGraph> graph;
    std::unique_ptr<BandChips> chips;
    std::unique_ptr<BandControls> controls;
    std::unique_ptr<SuggestionList> suggestions;

    ParamToggle eqPower, autoGain;
    IconButton deltaButton { "phase", "eq.delta" }, resetButton { "reset_settings", "eq.reset" };
    juce::ToggleButton showInput;
    juce::TextButton analyseButton, applyAllButton, captureButton, matchButton;
    juce::Slider matchAmount;
    juce::String brainStatus, referenceText;
    juce::Rectangle<int> brainArea, statusArea, referenceArea, toolbarArea;
    float refreshTimer = 0.0f;
};
}
