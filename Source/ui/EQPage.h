#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace mu
{
class EQGraph : public juce::Component
{
public:
    explicit EQGraph (MUProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

    int getSelectedBand() const { return selected; }
    void setSelectedBand (int b) { selected = b; repaint(); }
    std::function<void (int)> onSelectionChanged;

private:
    float xForHz (float hz) const;
    float hzForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    int bandAt (juce::Point<float> p) const;
    juce::Rectangle<float> plotArea() const;
    void setBandParam (int band, const juce::String& id, float value, bool gesture);

    MUProcessor& proc;
    int selected = 2, hovered = -1, dragging = -1;
    const float minDb = -18.0f, maxDb = 18.0f;
};

class EQPage : public juce::Component
{
public:
    explicit EQPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void bindBand (int band);

    MUProcessor& proc;
    EQGraph graph;
    juce::ToggleButton eqOn { "EQ" }, bandOn { "Band on" }, vsOn { "Vocal space" };
    juce::ComboBox typeBox, vsSpeed;
    juce::Slider freq, gain, q, vsAmount;
    juce::Label freqL { {}, "FREQ" }, gainL { {}, "GAIN" }, qL { {}, "Q" }, bandTitle, cleanResult, vsInfo, vsAmountL { {}, "AMOUNT" };
    juce::TextButton cleanHarsh { "CLEAN HARSH" }, reset { "RESET EQ" };
    std::array<juce::TextButton, kNumEQBands> bandButtons;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, bandOnAtt, vsOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt, vsSpeedAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt, vsAmountAtt;
    int boundBand = -1;
    bool vsVisible = true;
};
}
