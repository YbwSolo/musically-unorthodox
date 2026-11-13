#include "MULookAndFeel.h"
#include "Theme.h"
#include "Icons.h"

namespace mu
{
using namespace theme;

MULookAndFeel::MULookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, black);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, yellow.withAlpha (0.4f));
    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, yellow.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::TextEditor::outlineColourId, lineHi);
    setColour (juce::TextEditor::focusedOutlineColourId, yellow);
    setColour (juce::CaretComponent::caretColourId, yellow);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, yellow);
    setColour (juce::PopupMenu::highlightedTextColourId, onYellow);
    setColour (juce::ScrollBar::thumbColourId, lineHi);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, lineHi);
}

// ---- sliders ---------------------------------------------------------------------------------------
void MULookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float side = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = side * 0.5f - 2.0f;
    const bool enabled = s.isEnabled();
    const bool hot = s.isMouseOverOrDragging();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (2.5f, side * 0.07f);

    // value arc: bipolar parameters grow from the centre, everything else from the left
    const auto range = s.getRange();
    const bool bipolar = range.getStart() < 0.0 && range.getEnd() > 0.0 && std::abs (range.getStart() + range.getEnd()) < 1.0e-3;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, radius - track * 0.5f, radius - track * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (control);
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (enabled && std::abs (angle - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius - track * 0.5f, radius - track * 0.5f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (yellow);
        g.strokePath (val, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // cap: a machined disc lit from above
    const float capR = radius - track - 3.0f;
    const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cap.translated (0.0f, 1.5f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (hot ? 0xff383838 : 0xff303030), centre.x, cap.getY(), juce::Colour (0xff141414), centre.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5a5a), centre.x, cap.getY(), juce::Colour (0xff1c1c1c), centre.x, cap.getBottom(), false));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.25f, -capR + 3.0f, 2.5f, capR * 0.48f, 1.25f);
    g.setColour (enabled ? (hot ? yellow : text) : textMute);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void MULookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool enabled = s.isEnabled();
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (control);
        g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
        g.setColour (enabled ? yellow : textMute);
        g.fillRoundedRectangle (cx - 2.0f, pos, 4.0f, (float) (y + h) - pos, 2.0f);
        g.setColour (text);
        g.fillRoundedRectangle (cx - 9.0f, pos - 3.0f, 18.0f, 6.0f, 3.0f);
        return;
    }
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (control);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (enabled ? yellow : textMute);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, juce::jmax (0.0f, pos - (float) x), 4.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (pos - 7.0f, cy - 6.0f, 14.0f, 14.0f);
    g.setColour (s.isMouseOverOrDragging() ? yellow : text);
    g.fillEllipse (pos - 6.5f, cy - 6.5f, 13.0f, 13.0f);
}

juce::Label* MULookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (body (11.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textWhenEditingColourId, text);
    l->setColour (juce::Label::backgroundWhenEditingColourId, raised);
    l->setColour (juce::Label::outlineWhenEditingColourId, yellow);
    return l;
}

juce::Slider::SliderLayout MULookAndFeel::getSliderLayout (juce::Slider& s)
{
    auto layout = LookAndFeel_V4::getSliderLayout (s);
    if (s.isRotary() && s.getTextBoxPosition() == juce::Slider::TextBoxBelow)
    {
        auto r = s.getLocalBounds();
        layout.textBoxBounds = r.removeFromBottom (16);
        layout.sliderBounds = r;
    }
    return layout;
}

// ---- buttons ---------------------------------------------------------------------------------------
void MULookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
    const bool on = b.getToggleState();
    const float radius = 5.0f;
    if (primary || on)
    {
        g.setColour (b.isEnabled() ? (down ? yellow.darker (0.25f) : highlighted ? yellow.brighter (0.25f) : yellow) : control);
        g.fillRoundedRectangle (r, radius);
    }
    else
    {
        g.setColour (down ? control.brighter (0.08f) : highlighted ? control : raised);
        g.fillRoundedRectangle (r, radius);
        g.setColour (highlighted ? lineHi.brighter (0.2f) : lineHi);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }
}

void MULookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool primary = (bool) b.getProperties().getWithDefault ("primary", false);
    const bool filled = primary || b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (! b.isEnabled() ? textMute : filled ? onYellow : text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (8, 0), juce::Justification::centred, 1, 0.9f);
}

juce::Font MULookAndFeel::getTextButtonFont (juce::TextButton&, int) { return bodyBold (12.5f); }

void MULookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();
    const auto style = b.getProperties().getWithDefault ("style", "switch").toString();
    if (style == "power")
    {
        const auto box = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 20.0f), juce::jmin (r.getHeight(), 20.0f));
        g.setColour (on ? yellow : (highlighted ? control.brighter (0.1f) : control));
        g.fillEllipse (box);
        icons::draw (g, "bypass", box.reduced (4.5f), on ? onYellow : textDim);
        return;
    }
    // pill switch with the label to its right
    const float sw = 28.0f, sh = 16.0f;
    const auto pill = juce::Rectangle<float> (r.getX(), r.getCentreY() - sh * 0.5f, sw, sh);
    g.setColour (on ? yellow : control);
    g.fillRoundedRectangle (pill, sh * 0.5f);
    if (! on) { g.setColour (highlighted ? lineHi.brighter (0.2f) : lineHi); g.drawRoundedRectangle (pill.reduced (0.5f), sh * 0.5f, 1.0f); }
    const float knobX = on ? pill.getRight() - sh + 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? onYellow : textDim);
    g.fillEllipse (knobX, pill.getY() + 2.0f, sh - 4.0f, sh - 4.0f);
    g.setColour (b.isEnabled() ? (on ? text : textDim) : textMute);
    g.setFont (body (12.5f));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (sw + 8.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
}

// ---- combo / menus ---------------------------------------------------------------------------------
void MULookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (c.isMouseOver (true) ? control : raised);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (lineHi);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path chevron;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f); chevron.lineTo (cx, cy + 2.0f); chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (textDim);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font MULookAndFeel::getComboBoxFont (juce::ComboBox&) { return body (12.5f); }
void MULookAndFeel::positionComboBoxText (juce::ComboBox& c, juce::Label& l)
{
    l.setBounds (10, 0, c.getWidth() - 32, c.getHeight());
    l.setFont (getComboBoxFont (c));
}

void MULookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (raised);
    g.setColour (lineHi);
    g.drawRect (0, 0, w, h, 1);
}

void MULookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                       const juce::String& label, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (line);
        g.fillRect (area.reduced (8, 0).withSizeKeepingCentre (area.getWidth() - 16, 1));
        return;
    }
    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive) { g.setColour (yellow); g.fillRoundedRectangle (r.toFloat(), 4.0f); }
    const auto colour = ! isActive ? textMute : isHighlighted ? onYellow : text;
    g.setColour (colour);
    g.setFont (isTicked ? bodyBold (13.0f) : body (13.0f));
    auto textArea = r.reduced (10, 0);
    if (isTicked) { g.fillEllipse ((float) textArea.getX(), (float) textArea.getCentreY() - 2.5f, 5.0f, 5.0f); }
    textArea.removeFromLeft (12);
    if (hasSubMenu)
    {
        juce::Path chevron;
        const float cx = (float) r.getRight() - 12.0f, cy = (float) r.getCentreY();
        chevron.startNewSubPath (cx - 2.0f, cy - 4.0f); chevron.lineTo (cx + 2.0f, cy); chevron.lineTo (cx - 2.0f, cy + 4.0f);
        g.strokePath (chevron, juce::PathStrokeType (1.5f));
        textArea.removeFromRight (16);
    }
    g.drawFittedText (label, textArea, juce::Justification::centredLeft, 1, 0.9f);
}

void MULookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (bodyBold (11.0f));
    g.setColour (textMute);
    g.drawFittedText (name, area.reduced (16, 0), juce::Justification::centredLeft, 1);
}

juce::Font MULookAndFeel::getPopupMenuFont() { return body (13.0f); }

void MULookAndFeel::getIdealPopupMenuItemSize (const juce::String& label, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 50; idealHeight = 9; return; }
    idealHeight = 26;
    idealWidth = juce::roundToInt (body (13.0f).getStringWidthFloat (label)) + 60;
}

// ---- labels / editors ------------------------------------------------------------------------------
juce::Font MULookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void MULookAndFeel::drawLabel (juce::Graphics& g, juce::Label& l)
{
    g.fillAll (l.findColour (juce::Label::backgroundColourId));
    if (! l.isBeingEdited())
    {
        g.setColour (l.findColour (juce::Label::textColourId).withMultipliedAlpha (l.isEnabled() ? 1.0f : 0.4f));
        g.setFont (l.getFont());
        g.drawFittedText (l.getText(), l.getLocalBounds().reduced (1, 0), l.getJustificationType(), juce::jmax (1, (int) ((float) l.getHeight() / l.getFont().getHeight())), 0.85f);
    }
}

void MULookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (raised);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 3.0f);
}

void MULookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? yellow : lineHi);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 3.0f, 1.0f);
}

void MULookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (over || down ? textMute : lineHi);
    if (vertical) g.fillRoundedRectangle ((float) x + (float) w * 0.5f - 2.0f, (float) thumbStart, 4.0f, (float) thumbSize, 2.0f);
    else          g.fillRoundedRectangle ((float) thumbStart, (float) y + (float) h * 0.5f - 2.0f, (float) thumbSize, 4.0f, 2.0f);
}
}
