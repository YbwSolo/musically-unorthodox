#include "Widgets.h"
#include "Theme.h"

namespace mu
{
// ---- Knob ----------------------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& t, const juce::String& tooltip)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 17);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (paramId) != nullptr ? apvts.getParameter (paramId)->convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue()) : 0.0);
    if (tooltip.isNotEmpty()) slider.setTooltip (tooltip);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    const int side = juce::jmin (r.getWidth(), r.getHeight(), 124);
    slider.setBounds (r.withWidth (side).withHeight (side).withCentre ({ r.getCentreX(), r.getY() + side / 2 }));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (slider.isEnabled() ? theme::textDim : theme::textMute);
    g.setFont (theme::bodyBold (10.0f));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centred);
}

// ---- HSlider -------------------------------------------------------------------------------------
HSlider::HSlider (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& t, int labelWidth, const juce::String& tooltip)
    : title (t), labelW (labelWidth)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 78, 18);
    slider.setDoubleClickReturnValue (true, apvts.getParameter (paramId) != nullptr ? apvts.getParameter (paramId)->convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue()) : 0.0);
    if (tooltip.isNotEmpty()) slider.setTooltip (tooltip);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
}

void HSlider::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (labelW);
    slider.setBounds (r);
}

void HSlider::paint (juce::Graphics& g)
{
    g.setColour (slider.isEnabled() ? theme::textDim : theme::textMute);
    g.setFont (theme::body (12.0f));
    g.drawText (title, getLocalBounds().removeFromLeft (labelW - 8), juce::Justification::centredLeft, true);
}

// ---- ModulePanel ---------------------------------------------------------------------------------
ModulePanel::ModulePanel (const juce::String& t, const juce::String& h) : title (t), hint (h) {}

void ModulePanel::bindToggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    toggle = std::make_unique<juce::ToggleButton> ("");
    addAndMakeVisible (*toggle);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, paramId, *toggle);
    resized();
}

void ModulePanel::paint (juce::Graphics& g)
{
    theme::drawPanel (g, getLocalBounds().toFloat());
    auto head = getLocalBounds().removeFromTop (34).reduced (12, 0);
    if (toggle) head.removeFromRight (40);
    theme::drawSectionTitle (g, head, title, hint);
    g.setColour (theme::line);
    g.fillRect (getLocalBounds().withTrimmedTop (34).removeFromTop (1).reduced (1, 0));
}

void ModulePanel::resized()
{
    if (toggle) toggle->setBounds (getLocalBounds().removeFromTop (34).removeFromRight (46).reduced (6, 6));
}

juce::Rectangle<int> ModulePanel::getContentArea() const
{
    return getLocalBounds().withTrimmedTop (35).reduced (12, 10);
}

// ---- TextBlock -----------------------------------------------------------------------------------
void TextBlock::setText (const juce::String& t, float px, juce::Colour c) { text = t; size = px; colour = c; repaint(); }

void TextBlock::paint (juce::Graphics& g)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.setLineSpacing (3.0f);
    s.append (text, theme::body (size), colour);
    juce::TextLayout tl;
    tl.createLayout (s, (float) getWidth());
    tl.draw (g, getLocalBounds().toFloat());
}

int TextBlock::heightForWidth (int width) const
{
    juce::AttributedString s;
    s.setLineSpacing (3.0f);
    s.append (text, theme::body (size), colour);
    juce::TextLayout tl;
    tl.createLayout (s, (float) width);
    return (int) std::ceil (tl.getHeight()) + 4;
}

// ---- ProgressStrip -------------------------------------------------------------------------------
void ProgressStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::line);
    g.fillRoundedRectangle (r, 2.0f);
    if (isActive)
    {
        g.setColour (theme::yellow);
        g.fillRoundedRectangle (r.withWidth (juce::jmax (4.0f, r.getWidth() * juce::jlimit (0.0f, 1.0f, progress))), 2.0f);
    }
}
}
