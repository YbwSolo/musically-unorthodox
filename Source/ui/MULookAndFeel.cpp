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
    setColour (juce::Slider::thumbColourId, theme::accent);
    setColour (juce::Slider::trackColourId, theme::accent);
    setColour (juce::Slider::backgroundColourId, theme::line);
    setColour (juce::ComboBox::backgroundColourId, theme::panelHi);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::outlineColourId, theme::line);
    setColour (juce::ComboBox::arrowColourId, theme::textDim);
    setColour (juce::PopupMenu::backgroundColourId, theme::panelHi);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, theme::panelHi);
    setColour (juce::TextButton::buttonOnColourId, theme::accent);
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ToggleButton::textColourId, theme::text);
    setColour (juce::ToggleButton::tickColourId, theme::accent);
    setColour (juce::TextEditor::backgroundColourId, theme::bg);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::outlineColourId, theme::line);
    setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.4f));
    setColour (juce::TooltipWindow::backgroundColourId, theme::panelHi);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::line);
    setColour (juce::ScrollBar::thumbColourId, theme::line);
}

void MULookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float radius = size * 0.5f;
    const float lineW = juce::jmax (2.0f, size * 0.07f);
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (endAngle - startAngle) : startAngle;

    juce::Path track;
    track.addCentredArc (r.getCentreX(), r.getCentreY(), arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::line);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (r.getCentreX(), r.getCentreY(), arcR, arcR, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (s.isEnabled() ? theme::accent : theme::textMute);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    auto body = r.reduced (lineW * 1.8f);
    g.setColour (theme::panelHi);
    g.fillEllipse (body);
    g.setColour (theme::line);
    g.drawEllipse (body, 1.0f);

    // pointer
    juce::Path p;
    const float pLen = body.getWidth() * 0.32f, pW = juce::jmax (2.0f, size * 0.05f);
    p.addRoundedRectangle (-pW * 0.5f, -body.getWidth() * 0.46f, pW, pLen, pW * 0.5f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (body.getCentreX(), body.getCentreY()));
    g.setColour (s.isEnabled() ? theme::text : theme::textMute);
    g.fillPath (p);
}

void MULookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float t = 4.0f;
    juce::Rectangle<float> track = horizontal ? r.withSizeKeepingCentre (r.getWidth(), t) : r.withSizeKeepingCentre (t, r.getHeight());
    g.setColour (theme::line);
    g.fillRoundedRectangle (track, t * 0.5f);

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
    g.setColour (s.isEnabled() ? theme::accent : theme::textMute);
    g.fillRoundedRectangle (fill, t * 0.5f);

    const float th = (float) getSliderThumbRadius (s);
    auto thumb = horizontal ? juce::Rectangle<float> (th * 2, th * 2).withCentre ({ pos, track.getCentreY() })
                            : juce::Rectangle<float> (th * 2, th * 2).withCentre ({ track.getCentreX(), pos });
    g.setColour (theme::text);
    g.fillEllipse (thumb);
    g.setColour (theme::bg);
    g.fillEllipse (thumb.reduced (th * 0.55f));
}

void MULookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float h = juce::jmin (18.0f, r.getHeight() - 4.0f);
    auto pill = juce::Rectangle<float> (h * 1.8f, h).withY (r.getCentreY() - h * 0.5f).withX (r.getX() + 2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? (b.isEnabled() ? theme::accent : theme::textMute) : theme::line);
    g.fillRoundedRectangle (pill, h * 0.5f);
    auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f).withCentre ({ on ? pill.getRight() - h * 0.5f : pill.getX() + h * 0.5f, pill.getCentreY() });
    g.setColour (highlighted ? juce::Colours::white : theme::text);
    g.fillEllipse (knob);
    g.setColour (b.isEnabled() ? theme::text : theme::textMute);
    g.setFont (theme::labelFont (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (pill.getRight() - r.getX() + 8.0f).toNearestInt(), juce::Justification::centredLeft);
}

void MULookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    juce::Colour c = on ? theme::accent : theme::panelHi;
    if (! b.isEnabled()) c = c.withAlpha (0.4f);
    if (highlighted) c = c.brighter (0.12f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? theme::accentHi : theme::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void MULookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (c.isEnabled() ? theme::panelHi : theme::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (theme::textDim);
    g.fillPath (arrow);
}

void MULookAndFeel::positionComboBoxText (juce::ComboBox& c, juce::Label& l)
{
    l.setBounds (8, 1, c.getWidth() - 28, c.getHeight() - 2);
    l.setFont (getComboBoxFont (c));
}

juce::Font MULookAndFeel::getComboBoxFont (juce::ComboBox& c) { return theme::labelFont (juce::jmin (14.0f, (float) c.getHeight() * 0.6f)); }
juce::Font MULookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font MULookAndFeel::getTextButtonFont (juce::TextButton&, int height) { return theme::titleFont (juce::jmin (14.0f, (float) height * 0.55f)); }

void MULookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.setColour (theme::panelHi);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
}

juce::Label* MULookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::monoFont (12.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, theme::textDim);
    return l;
}
}
