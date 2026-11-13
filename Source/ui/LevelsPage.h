#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"

namespace mu
{
class LevelsPage : public juce::Component
{
public:
    explicit LevelsPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    MUProcessor& proc;
    Readout momentary, shortTerm, integrated, truePeak, crest;
    LevelMeter meter { -40.0f, 0.0f };
    juce::Slider output;
    juce::Label outputL { {}, "OUTPUT" }, checklist, explain;
    juce::TextButton resetMeters { "RESET METERS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAtt;
};
}
