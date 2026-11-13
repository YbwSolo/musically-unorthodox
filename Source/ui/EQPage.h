#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Widgets.h"

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
    void setBandParam (const juce::String& id, float value, bool gesture);

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
    juce::ToggleButton eqOn { "EQ on" }, bandOn { "Band on" }, dynOn { "Dynamic" };
    juce::ComboBox typeBox;
    juce::Slider freq, gain, q, thr, range;
    juce::Label bandTitle, cleanResult, dynHint;
    juce::TextButton cleanHarsh { "CLEAN HARSH" }, reset { "RESET EQ" };
    std::array<juce::TextButton, kNumEQBands> bandButtons;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, bandOnAtt, dynOnAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAtt, gainAtt, qAtt, thrAtt, rangeAtt;
    int boundBand = -1;
};
}
