#pragma once
#include "Widgets.h"

namespace mu
{
// Tune, Doubler and Reverb: the tools that turn a dry take into a finished vocal.
class FXPage : public juce::Component
{
public:
    FXPage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void drawPitch (juce::Graphics&, juce::Rectangle<float>);
    void drawDoubles (juce::Graphics&, juce::Rectangle<float>);
    void drawReverb (juce::Graphics&, juce::Rectangle<float>);
    void style (std::initializer_list<std::pair<juce::String, float>> values, const juce::String& what);

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    Section tune { "Tune" }, dbl { "Doubler" }, verb { "Reverb" };

    juce::ComboBox keyBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt;
    Segmented scale;
    juce::TextButton findKey, tuneNatural, tuneTight, tuneHard;
    Knob tuneSpeed, tuneAmount, tuneVibrato;

    Segmented voices;
    Knob dblMix, dblDetune, dblTiming, dblWidth;
    juce::TextButton dblSubtle, dblWide, dblStack;

    ParamToggle rvAuto;
    Segmented rvLength;
    Knob rvMix, rvDecay, rvPre, rvTone, rvDuck;
    juce::TextButton rvTight, rvPlate, rvBig;

    juce::Rectangle<int> pitchWell, dblWell, verbWell, keyLabel, monoNote;
    static constexpr int kHistory = 360;                      // six seconds at 60 frames a second
    std::array<float, kHistory> sung {}, tuned {};
    int head = 0;
    float frame = 0.0f, centre = 60.0f, correction = 0.0f, duck = 0.0f, sweep = 0.0f;
    int shownTarget = -1;
    std::array<float, 4> voiceCents {}, voiceDelay {};
};
}
