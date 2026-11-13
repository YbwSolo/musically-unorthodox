#include "MULookAndFeel.h"
#include "Theme.h"

namespace mu
{
MULookAndFeel::MULookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::yellow.withAlpha (0.4f));
    setColour (juce::Slider::thumbColourId, theme::yellow);
    setColour (juce::Slider::trackColourId, theme::yellow);
    setColour (juce::Slider::backgroundColourId, theme::line);
    setColour (juce::ComboBox::backgroundColourId, theme::panelHi);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::line);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);
    setColour (juce::PopupMenu::backgroundColourId, theme::panelHi);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::yellow);
    setColour (juce::PopupMenu::highlightedTextColourId, theme::onYellow);
    setColour (juce::TextButton::buttonColourId, theme::panelHi);
    setColour (juce::TextButton::buttonOnColourId, theme::yellow);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, theme::onYellow);
    setColour (juce::ToggleButton::textColourId, theme::text);
    setColour (juce::ToggleButton::tickColourId, theme::yellow);
    setColour (juce::TextEditor::backgroundColourId, theme::black);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::line);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::yellow);
    setColour (juce::TextEditor::highlightColourId, theme::yellow.withAlpha (0.35f));
    setColour (juce::TooltipWindow::backgroundColourId, theme::panelHi);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::lineHi);
    setColour (juce::ScrollBar::thumbColourId, theme::lineHi);
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, theme::yellow);
}

void MULookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float radius = size * 0.5f;
    const float lineW = juce::jmax (2.0f, size * 0.065f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (endAngle - startAngle) : startAngle;
    const bool on = s.isEnabled();

    juce::Path track;
    track.addCentredArc (r.getCentreX(), r.getCentreY(), arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::line);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path value;
    value.addCentredArc (r.getCentreX(), r.getCentreY(), arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (on ? theme::yellow : theme::textMute);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    auto body = r.reduced (lineW * 2.2f);
    g.setColour (theme::black);
    g.fillEllipse (body);
    g.setColour (theme::lineHi);
    g.drawEllipse (body, 1.0f);

    juce::Path p;
    const float pLen = body.getWidth() * 0.30f, pW = juce::jmax (2.0f, size * 0.045f);
    p.addRoundedRectangle (-pW * 0.5f, -body.getWidth() * 0.47f, pW, pLen, pW * 0.5f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (body.getCentreX(), body.getCentreY()));
    g.setColour (on ? theme::text : theme::textMute);
    g.fillPath (p);
}

void MULookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float t = 3.0f;
    juce::Rectangle<float> track = horizontal ? r.withSizeKeepingCentre (r.getWidth(), t) : r.withSizeKeepingCentre (t, r.getHeight());
    g.setColour (theme::line);
    g.fillRect (track);

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    float zeroPos = horizontal ? track.getX() : track.getBottom();
    if (bipolar)
    {
        const float f = (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum()));
        zeroPos = horizontal ? track.getX() + f * track.getWidth() : track.getBottom() - f * track.getHeight();
    }
    juce::Rectangle<float> fill = horizontal
        ? juce::Rectangle<float> (juce::jmin (zeroPos, pos), track.getY(), std::abs (pos - zeroPos), t)
        : juce::Rectangle<float> (track.getX(), juce::jmin (zeroPos, pos), t, std::abs (pos - zeroPos));
    g.setColour (s.isEnabled() ? theme::yellow : theme::textMute);
    g.fillRect (fill);

    const float th = (float) getSliderThumbRadius (s);
    auto thumb = horizontal ? juce::Rectangle<float> (th * 2, th * 2).withCentre ({ pos, track.getCentreY() })
                            : juce::Rectangle<float> (th * 2, th * 2).withCentre ({ track.getCentreX(), pos });
    g.setColour (theme::text);
    g.fillEllipse (thumb);
    g.setColour (theme::black);
    g.fillEllipse (thumb.reduced (th * 0.6f));
}

void MULookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float h = juce::jmin (16.0f, r.getHeight() - 4.0f);
    auto pill = juce::Rectangle<float> (h * 1.9f, h).withY (r.getCentreY() - h * 0.5f).withX (r.getX() + 1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? (b.isEnabled() ? theme::yellow : theme::textMute) : theme::lineHi);
    g.fillRoundedRectangle (pill, h * 0.5f);
    auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f).withCentre ({ on ? pill.getRight() - h * 0.5f : pill.getX() + h * 0.5f, pill.getCentreY() });
    g.setColour (on ? theme::black : (highlighted ? theme::text : theme::textDim));
    g.fillEllipse (knob);
    g.setColour (b.isEnabled() ? theme::text : theme::textMute);
    g.setFont (theme::body (12.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (pill.getRight() - r.getX() + 8.0f).toNearestInt(), juce::Justification::centredLeft, true);
}

void MULookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto id = b.getComponentID();
    const bool on = b.getToggleState();
    const bool primary = id == "primary" || on;
    const bool ghost = id == "ghost";
    juce::Colour fill = primary ? theme::yellow : ghost ? juce::Colours::transparentBlack : theme::panelHi;
    juce::Colour border = primary ? theme::yellow : ghost ? theme::text.withAlpha (0.7f) : theme::line;
    if (! b.isEnabled()) { fill = fill.withAlpha (0.35f); border = border.withAlpha (0.35f); }
    if (highlighted && b.isEnabled()) fill = primary ? fill.brighter (0.08f) : ghost ? theme::text.withAlpha (0.08f) : fill.brighter (0.1f);
    if (down) fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (border);
    g.drawRoundedRectangle (r, 2.0f, ghost ? 1.5f : 1.0f);
}

void MULookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto id = b.getComponentID();
    const bool primary = id == "primary" || b.getToggleState();
    g.setFont (getTextButtonFont (b, b.getHeight()));
    juce::Colour c = primary ? theme::onYellow : theme::text;
    if (! b.isEnabled()) c = c.withAlpha (0.5f);
    g.setColour (c);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
}

void MULookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (c.isEnabled() ? theme::black : theme::panel);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (theme::lineHi);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (theme::yellow);
    g.fillPath (arrow);
}

void MULookAndFeel::positionComboBoxText (juce::ComboBox& c, juce::Label& l)
{
    l.setBounds (8, 1, c.getWidth() - 28, c.getHeight() - 2);
    l.setFont (getComboBoxFont (c));
}

juce::Font MULookAndFeel::getComboBoxFont (juce::ComboBox& c) { return theme::body (juce::jmin (13.0f, (float) c.getHeight() * 0.55f)); }
juce::Font MULookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font MULookAndFeel::getTextButtonFont (juce::TextButton&, int height) { return theme::bodyBold (juce::jmin (12.5f, (float) height * 0.48f)); }
juce::Font MULookAndFeel::getPopupMenuFont() { return theme::body (13.0f); }

void MULookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.setColour (theme::panelHi);
    g.fillRect (0, 0, w, h);
    g.setColour (theme::lineHi);
    g.drawRect (0, 0, w, h, 1);
}

void MULookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                       bool isTicked, bool, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (theme::line);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive) { g.setColour (theme::yellow); g.fillRect (r); }
    g.setColour (isHighlighted && isActive ? theme::onYellow : isActive ? theme::text : theme::textMute);
    g.setFont (theme::body (13.0f));
    auto tr = r.reduced (12, 0);
    if (isTicked)
    {
        g.setColour (isHighlighted ? theme::onYellow : theme::yellow);
        g.fillEllipse (tr.removeFromLeft (8).withSizeKeepingCentre (6, 6).toFloat());
        tr.removeFromLeft (6);
        g.setColour (isHighlighted && isActive ? theme::onYellow : theme::text);
    }
    else tr.removeFromLeft (14);
    g.drawText (text, tr, juce::Justification::centredLeft, true);
}

juce::Label* MULookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::bodyBold (11.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, theme::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

static juce::TextLayout tooltipLayout (const juce::String& text)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (text, theme::body (12.5f), theme::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    return tl;
}

juce::Rectangle<int> MULookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const juce::TextLayout tl = tooltipLayout (tipText);
    const int w = (int) (tl.getWidth() + 18.0f), h = (int) (tl.getHeight() + 12.0f);
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h).constrainedWithin (parentArea);
}

void MULookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.setColour (theme::panelHi);
    g.fillRect (0, 0, width, height);
    g.setColour (theme::yellow);
    g.fillRect (0, 0, 3, height);
    g.setColour (theme::lineHi);
    g.drawRect (0, 0, width, height, 1);
    tooltipLayout (text).draw (g, juce::Rectangle<float> (9.0f, 0.0f, (float) width - 12.0f, (float) height));
}
}
