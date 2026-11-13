#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"
#include "Widgets.h"

namespace mu
{
// De-esser, saturation, rider and vocal space: the rest of the chain in signal order.
class ChainPage : public juce::Component
{
public:
    explicit ChainPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    MUProcessor& proc;
    ModulePanel dsPanel { "De-esser", "takes the sting out of S and T" };
    ModulePanel satPanel { "Saturation", "harmonics, weight, presence" };
    ModulePanel riderPanel { "Rider", "keeps the level consistent" };
    ModulePanel vsPanel { "Vocal space", "ducks the beat under the vocal" };

    std::unique_ptr<Knob> dsFreq, dsThr, dsRange, satDrive, satMix, riderTarget, riderRange, vsAmount;
    juce::ComboBox dsMode, satType, riderSpeed, vsSpeed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dsModeAtt, satTypeAtt, riderSpeedAtt, vsSpeedAtt;
    GainReductionMeter dsMeter;
    juce::Label riderGain, vsInfo, dsInfo, satInfo, orderInfo;
};
}
