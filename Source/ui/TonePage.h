#pragma once
#include "Widgets.h"
#include "../dsp/Smoother.h"
#include "../dsp/SpectrumAnalyzer.h"

namespace mu
{
class TonePage : public juce::Component
{
public:
    TonePage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    PresetStrip presets { "tone.presets", "Tone presets" };
    Section smooth { "Smooth" }, sat { "Saturation" }, rider { "Level rider" }, space { "Vocal space" }, stereo { "Stereo" }, output { "Output" };
    Knob smDepth; Segmented smFocus, smSpeed; IconButton smDelta { "phase", "sm.delta" };
    Segmented satType; Knob satDrive, satMix;
    Knob riderTarget, riderRange; Segmented riderSpeed;
    Knob vsAmount; Segmented vsSpeed;
    Knob stWidth, stMonoFreq; ParamToggle stMonoBass;
    Knob outGain, trim;
    juce::Rectangle<int> smoothWell, satWell, riderWell, spaceWell, corrWell, smFocusLabel, smSpeedLabel;
    std::array<float, Smoother::kBands> cuts {};
    std::array<float, SpectrumAnalyzer::kDisplayPoints> spectrum {};
    float rideGain = 0.0f, vsDepth = 0.0f, corr = 1.0f, peakLin = 0.0f, slow = 0.0f;
};
}
