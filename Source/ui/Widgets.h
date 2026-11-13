#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Icons.h"
#include "HelpText.h"

namespace mu
{
class MUProcessor;

// Rotary control: label above, value underneath (click the value to type), bound to a parameter.
class Knob : public juce::Component
{
public:
    Knob (MUProcessor&, const juce::String& paramId, const juce::String& label, const juce::String& helpKey = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setDimmed (bool d) { if (dimmed != d) { dimmed = d; slider.setEnabled (! d); repaint(); } }
    juce::Slider slider;

private:
    juce::String label;
    bool dimmed = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Switch bound to a bool parameter. style: "switch" (pill with label) or "power" (round module on/off).
class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (MUProcessor&, const juce::String& paramId, const juce::String& text, const juce::String& style = "switch", const juce::String& helpKey = {});
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Row of mutually exclusive options with a highlight that slides to the choice. Items are text, or "icon:<name>".
class Segmented : public juce::Component, private juce::Timer
{
public:
    explicit Segmented (const juce::StringArray& items);
    Segmented (MUProcessor&, const juce::String& paramId, const juce::StringArray& items, const juce::String& helpKey = {});
    ~Segmented() override;
    void setIndex (int i, bool notify = false);
    int getIndex() const { return index; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    std::function<void (int)> onChange;

private:
    void timerCallback() override;
    juce::StringArray items;
    int index = 0, hover = -1;
    float animPos = 0.0f;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// Titled panel. Optional round power switch bound to a parameter, optional right-aligned readout.
class Section : public juce::Component
{
public:
    explicit Section (const juce::String& title);
    void setPower (MUProcessor&, const juce::String& paramId);
    void setReadout (const juce::String& s) { if (readout != s) { readout = s; repaint (0, 0, getWidth(), kHeader); } }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (kHeader).reduced (12, 0).withTrimmedBottom (10); }
    juce::Rectangle<int> headerRight (int width) const { return juce::Rectangle<int> (getWidth() - width - 10, 7, width, kHeader - 12); }
    bool isOn() const { return power == nullptr || power->getToggleState(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kHeader = 34;

private:
    juce::String title, readout;
    std::unique_ptr<ParamToggle> power;
};

class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& iconName, const juce::String& helpKey = {});
    void setIcon (const juce::String& n) { icon = n; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    juce::String icon;
};

juce::TextButton& styleButton (juce::TextButton& b, const juce::String& text, bool primary = false, const juce::String& helpKey = {});

// Lays components out left to right in equal cells.
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 6);

// Small vertical reduction meter (0 at the top, grows downward), used beside dynamics modules.
void drawReductionBar (juce::Graphics& g, juce::Rectangle<float> r, float reductionDb, float maxDb);
}
