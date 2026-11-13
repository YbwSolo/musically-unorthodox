#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"

namespace mu
{
class TransferCurve : public juce::Component
{
public:
    explicit TransferCurve (MUProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    MUProcessor& proc;
};

class CompressorPage : public juce::Component
{
public:
    explicit CompressorPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    MUProcessor& proc;
    TransferCurve curve;
    juce::ToggleButton on { "Compressor" }, autoMakeup { "Auto makeup" };
    juce::Slider threshold, ratio, attack, release, knee, makeup, mix;
    juce::Label thresholdL { {}, "THRESHOLD" }, ratioL { {}, "RATIO" }, attackL { {}, "ATTACK" }, releaseL { {}, "RELEASE" },
                kneeL { {}, "KNEE" }, makeupL { {}, "MAKEUP" }, mixL { {}, "MIX" }, result;
    juce::TextButton autoSet { "AUTO SET" };
    GainReductionMeter grMeter;
    LevelMeter inMeter { -60.0f, 6.0f }, outMeter { -60.0f, 6.0f };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt, autoMakeupAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
};
}
