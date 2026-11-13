#include "Widgets.h"
#include "../PluginProcessor.h"

namespace mu
{
void setHelp (juce::Component& c, const juce::String& key) { c.getProperties().set ("help", key); }

using namespace theme;

// ---- Knob ------------------------------------------------------------------------------------------
Knob::Knob (MUProcessor& p, const juce::String& paramId, const juce::String& l, const juce::String& helpKey) : label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
    shown = (float) slider.valueToProportionOfLength (slider.getValue());
    // when a preset or the A/B switch moves the value, the pointer sweeps to its new position instead of jumping
    slider.onValueChange = [this]
    {
        if (slider.isMouseButtonDown() || ! isShowing()) { shown = (float) slider.valueToProportionOfLength (slider.getValue()); slider.getProperties().remove ("animPos"); }
        else if (! isTimerRunning()) startTimerHz (60);
    };
}

void Knob::timerCallback()
{
    const float target = (float) slider.valueToProportionOfLength (slider.getValue());
    shown = approach (shown, target, 1.0f / 60.0f, 0.07f);
    if (std::abs (shown - target) < 0.002f || slider.isMouseButtonDown()) { shown = target; slider.getProperties().remove ("animPos"); stopTimer(); }
    else slider.getProperties().set ("animPos", shown);
    slider.repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);
    const int side = juce::jmin (r.getWidth(), r.getHeight() - 16, 64);
    slider.setBounds (r.withSizeKeepingCentre (juce::jmax (side, 40), side + 16));
}

void Knob::paint (juce::Graphics& g)
{
    g.setFont (body (11.0f));
    g.setColour (dimmed ? textMute : textDim);
    g.drawFittedText (label, getLocalBounds().removeFromTop (14), juce::Justification::centred, 1, 0.85f);
}

// ---- ParamToggle -----------------------------------------------------------------------------------
ParamToggle::ParamToggle (MUProcessor& p, const juce::String& paramId, const juce::String& text, const juce::String& style, const juce::String& helpKey)
{
    setButtonText (text);
    getProperties().set ("style", style);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, paramId, *this);
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);     // the space bar should keep starting and stopping the song in the host
}

// ---- Segmented -------------------------------------------------------------------------------------
Segmented::Segmented (const juce::StringArray& i) : items (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

Segmented::Segmented (MUProcessor& p, const juce::String& paramId, const juce::StringArray& i, const juce::String& helpKey) : Segmented (i)
{
    if (auto* param = p.apvts.getParameter (paramId))
    {
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { setIndex (juce::roundToInt (v), false); });
        att->sendInitialUpdate();
        animPos = (float) index;
    }
    setHelp (*this, helpKey.isNotEmpty() ? helpKey : paramId);
}

Segmented::~Segmented() { stopTimer(); }

void Segmented::setIndex (int i, bool notify)
{
    i = juce::jlimit (0, juce::jmax (0, items.size() - 1), i);
    if (i != index)
    {
        index = i;
        if (isShowing()) startTimerHz (60); else animPos = (float) index;
        repaint();
    }
    if (notify)
    {
        if (att != nullptr) att->setValueAsCompleteGesture ((float) i);
        if (onChange) onChange (i);
    }
}

void Segmented::timerCallback()
{
    animPos = approach (animPos, (float) index, 1.0f / 60.0f, 0.05f);
    if (std::abs (animPos - (float) index) < 0.005f) { animPos = (float) index; stopTimer(); }
    repaint();
}

void Segmented::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (control);
    g.fillRoundedRectangle (r, 5.0f);
    if (items.isEmpty()) return;
    const float cw = r.getWidth() / (float) items.size();
    const auto sel = juce::Rectangle<float> (r.getX() + animPos * cw, r.getY(), cw, r.getHeight()).reduced (2.0f);
    g.setColour (isEnabled() ? yellow : lineHi);
    g.fillRoundedRectangle (sel, 4.0f);
    for (int i = 0; i < items.size(); ++i)
    {
        const auto cell = juce::Rectangle<float> (r.getX() + (float) i * cw, r.getY(), cw, r.getHeight());
        const bool on = std::abs (animPos - (float) i) < 0.5f;
        const auto colour = on ? onYellow : (i == hover ? text : textDim);
        if (items[i].startsWith ("icon:"))
            icons::draw (g, items[i].substring (5), cell.reduced (5.0f, 5.0f), colour);
        else
        {
            g.setColour (colour);
            g.setFont (on ? bodyBold (12.0f) : body (12.0f));
            g.drawFittedText (items[i], cell.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
        }
    }
}

void Segmented::mouseDown (const juce::MouseEvent& e)
{
    if (items.isEmpty() || ! isEnabled()) return;
    setIndex ((int) ((float) e.x / ((float) getWidth() / (float) items.size())), true);
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = items.isEmpty() ? -1 : juce::jlimit (0, items.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) items.size())));
    if (h != hover) { hover = h; repaint(); }
}

void Segmented::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

// ---- Section ---------------------------------------------------------------------------------------
Section::Section (const juce::String& t) : title (t) { setInterceptsMouseClicks (false, true); }

void Section::setPower (MUProcessor& p, const juce::String& paramId)
{
    power = std::make_unique<ParamToggle> (p, paramId, "", "power");
    power->onStateChange = [this] { repaint(); };
    addAndMakeVisible (*power);
    resized();
}

void Section::resized()
{
    if (power != nullptr) power->setBounds (10, 7, 20, 20);
}

void Section::paint (juce::Graphics& g)
{
    fillPanel (g, getLocalBounds().toFloat());
    auto header = getLocalBounds().removeFromTop (kHeader).reduced (12, 0);
    if (power != nullptr) header.removeFromLeft (26);
    g.setFont (bodyBold (13.0f));
    g.setColour (isOn() ? text : textDim);
    g.drawText (title, header, juce::Justification::centredLeft);
    if (readout.isNotEmpty())
    {
        g.setFont (body (12.0f));
        g.setColour (isOn() ? yellow : textMute);
        g.drawText (readout, header, juce::Justification::centredRight);
    }
}

// ---- IconButton ------------------------------------------------------------------------------------
IconButton::IconButton (const juce::String& iconName, const juce::String& helpKey) : juce::Button (iconName), icon (iconName)
{
    if (helpKey.isNotEmpty()) setHelp (*this, helpKey);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    g.setColour (on ? yellow : (highlighted || down) ? control.brighter (0.06f) : control);
    g.fillRoundedRectangle (r, 5.0f);
    const float pad = juce::jmax (4.0f, juce::jmin (r.getWidth(), r.getHeight()) * 0.24f);
    icons::draw (g, icon, r.reduced (pad), ! isEnabled() ? textMute : on ? onYellow : highlighted ? theme::text : theme::text.withAlpha (0.8f));
}

juce::TextButton& styleButton (juce::TextButton& b, const juce::String& text, bool primary, const juce::String& helpKey)
{
    b.setButtonText (text);
    b.getProperties().set ("primary", primary);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    b.setWantsKeyboardFocus (false);
    if (helpKey.isNotEmpty()) setHelp (b, helpKey);
    return b;
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const float cell = ((float) area.getWidth() - (float) gap * (float) (n - 1)) / (float) n;
    int i = 0;
    for (auto* c : items)
    {
        const int x0 = area.getX() + juce::roundToInt ((cell + (float) gap) * (float) i);
        const int x1 = area.getX() + juce::roundToInt ((cell + (float) gap) * (float) i + cell);
        if (c != nullptr) c->setBounds (x0, area.getY(), x1 - x0, area.getHeight());
        ++i;
    }
}

void drawReductionBar (juce::Graphics& g, juce::Rectangle<float> r, float reductionDb, float maxDb)
{
    g.setColour (control);
    g.fillRoundedRectangle (r, 2.0f);
    const float amount = juce::jlimit (0.0f, 1.0f, std::abs (reductionDb) / maxDb);
    if (amount > 0.005f)
    {
        g.setColour (yellow);
        g.fillRoundedRectangle (r.withHeight (juce::jmax (2.0f, r.getHeight() * amount)), 2.0f);
    }
}
}
