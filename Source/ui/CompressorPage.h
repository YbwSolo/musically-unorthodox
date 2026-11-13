#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"
#include "Widgets.h"

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
    ModulePanel compPanel { "Compressor", "levels the performance" }, gatePanel { "Gate", "closes between phrases" };
    TransferCurve curve;
    std::unique_ptr<Knob> threshold, ratio, attack, release, knee, makeup, mix;
    std::unique_ptr<Knob> gThreshold, gRange, gAttack, gRelease, gHold;
    juce::ToggleButton autoMakeup { "Auto makeup" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoMakeupAtt;
    juce::TextButton autoSet { juce::String ("AUTO SET  ") + juce::String (juce::CharPointer_UTF8 ("\xE2\x86\x92")) };
    juce::Label result;
    GainReductionMeter grMeter, gateMeter;
    LevelMeter inMeter { -60.0f, 6.0f }, outMeter { -60.0f, 6.0f };
};
}
