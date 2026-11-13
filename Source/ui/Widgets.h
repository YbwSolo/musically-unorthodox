#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace mu
{
// Rotary control with a title above and the value underneath, bound to a parameter.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& title, const juce::String& tooltip = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider& getSlider() { return slider; }
    void setEnabled (bool on) { slider.setEnabled (on); repaint(); }

private:
    juce::String title;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Horizontal slider with a left label, bound to a parameter.
class HSlider : public juce::Component
{
public:
    HSlider (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& title, int labelWidth = 130, const juce::String& tooltip = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider& getSlider() { return slider; }

private:
    juce::String title;
    int labelW;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Panel with a yellow-tabbed title and an optional on/off toggle bound to a parameter.
class ModulePanel : public juce::Component
{
public:
    ModulePanel (const juce::String& title, const juce::String& hint = {});
    void bindToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> getContentArea() const;   // below the title row
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setHint (const juce::String& h) { hint = h; repaint(); }
    juce::ToggleButton* getToggle() { return toggle.get(); }

private:
    juce::String title, hint;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Small text block with word wrap (for help / explanations).
class TextBlock : public juce::Component
{
public:
    void setText (const juce::String& t, float px = 12.5f, juce::Colour c = juce::Colour (0xffa3a3a3));
    void paint (juce::Graphics&) override;
    int heightForWidth (int width) const;
private:
    juce::String text;
    float size = 12.5f;
    juce::Colour colour;
};

// Thin progress bar used for "learning".
class ProgressStrip : public juce::Component
{
public:
    void setProgress (float p, bool active) { progress = p; isActive = active; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float progress = 0.0f;
    bool isActive = false;
};
}
