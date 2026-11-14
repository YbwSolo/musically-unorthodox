#pragma once
#include "Widgets.h"

namespace mu
{
class CompScope;
class TransferCurve;

class DynamicsPage : public juce::Component
{
public:
    DynamicsPage (MUProcessor&, std::function<void (const juce::String&)> status);
    ~DynamicsPage() override;
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    PresetStrip presets { "dyn.presets", "Dynamics presets" };
    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    Section compSection { "Compressor" }, gateSection { "Gate" }, deEssSection { "De-esser" };
    std::unique_ptr<CompScope> scope;
    std::unique_ptr<TransferCurve> curve;
    Segmented style;
    Knob thr, ratio, attack, release, knee, makeup, mix, scHp;
    ParamToggle autoMakeup;
    juce::TextButton autoSet;
    Knob gThr, gRange, gAttack, gRelease, gHold;
    Knob dFreq, dThr, dRange;
    Segmented dMode;
    juce::Rectangle<int> styleTextArea, gateMeter, deEssMeter;
    float gateGr = 0.0f, deEssGr = 0.0f, slow = 0.0f;
    int shownStyle = -1;
};
}
