#include "SettingsPanel.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

SettingsPanel::SettingsPanel (MUProcessor& p) : proc (p), takePart (p, ids::autoBalance, "Let the session balance move this track")
{
    roleBox.addItemList (roleNames(), 1);
    roleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::role, roleBox);
    setHelp (roleBox, "role");
    sizeSwitch.setIndex (juce::jlimit (0, 2, (int) p.apvts.state.getProperty ("uiSize", 1)));
    sizeSwitch.onChange = [this] (int i)
    {
        proc.apvts.state.setProperty ("uiSize", i, nullptr);
        static const float scales[] = { 0.85f, 1.0f, 1.25f };
        if (onScaleChanged) onScaleChanged (scales[i]);
    };
    styleButton (closeButton, "Done", true);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    styleButton (resetButton, "Reset meters", false);
    resetButton.onClick = [this] { proc.resetMeters(); };
    for (auto* c : std::initializer_list<juce::Component*> { &roleBox, &sizeSwitch, &takePart, &closeButton, &resetButton }) addAndMakeVisible (c);

    for (int r = (int) Role::Vocal + 1; r < (int) Role::Master; ++r)
    {
        auto* s = offsetSliders.add (new juce::Slider());
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        offsetAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids::roleOffset ((Role) r), *s));
        addAndMakeVisible (s);
    }
}

void SettingsPanel::refresh()
{
    const auto guess = proc.getDetectedRole();
    const auto d = (int) proc.getParam (ids::role) == (int) Role::Auto
                 ? "Auto is hearing this as " + roleNames()[(int) guess.role].toLowerCase() + "."
                 : juce::String ("Set by hand.");
    if (d != detected) { detected = d; repaint(); }
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e) { if (! card.contains (e.getPosition()) && onClose) onClose(); }

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    g.setColour (raised);
    g.fillRoundedRectangle (card.toFloat(), 8.0f);
    g.setColour (lineHi);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.0f);
    auto r = card.reduced (24, 0);
    g.setFont (headline (24.0f)); g.setColour (text);
    g.drawText ("SETTINGS", r.removeFromTop (58), juce::Justification::centredLeft);
    for (auto& l : labels)
    {
        const bool heading = l.first.startsWith ("#");
        g.setFont (heading ? bodyBold (12.5f) : body (12.0f));
        g.setColour (heading ? text : textDim);
        g.drawText (heading ? l.first.substring (1) : l.first, l.second, juce::Justification::centredLeft);
    }
    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText ("Musically Unorthodox mix assistant, version " + juce::String (MU_VERSION_STRING) + ". In Music In Media C.I.C., Northampton.",
                card.reduced (24, 0).removeFromBottom (58).withTrimmedRight (110), juce::Justification::centredLeft, true);
}

void SettingsPanel::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (juce::jmin (560, getWidth() - 40), juce::jmin (560, getHeight() - 30));
    labels.clear();
    auto r = card.reduced (24, 0);
    r.removeFromTop (58);
    auto bottom = r.removeFromBottom (58);
    closeButton.setBounds (bottom.removeFromRight (96).withSizeKeepingCentre (96, 30));

    auto row = [&] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x; };
    labels.push_back ({ "#Track role", row (18) });
    auto roleRow = row (30);
    roleBox.setBounds (roleRow.removeFromLeft (220));
    roleRow.removeFromLeft (14);
    labels.push_back ({ detected, roleRow });
    takePart.setBounds (row (22));
    r.removeFromTop (6);

    labels.push_back ({ "#Window size", row (18) });
    auto sizeRow = row (28);
    sizeSwitch.setBounds (sizeRow.removeFromLeft (280));
    resetButton.setBounds (sizeRow.removeFromRight (120));
    r.removeFromTop (6);

    labels.push_back ({ "#How loud each role sits against the lead vocal", row (18) });
    const int rowH = juce::jlimit (20, 26, (r.getHeight() - 4) / juce::jmax (1, offsetSliders.size()));
    int role = (int) Role::Vocal + 1;
    for (auto* s : offsetSliders)
    {
        auto line1 = r.removeFromTop (rowH);
        labels.push_back ({ roleNames()[role++], line1.removeFromLeft (130) });
        s->setBounds (line1);
    }
}
}
