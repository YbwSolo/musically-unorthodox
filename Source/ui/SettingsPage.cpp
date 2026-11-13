#include "SettingsPage.h"
#include "Theme.h"

namespace mu
{
SettingsPage::SettingsPage (MUProcessor& p) : proc (p)
{
    for (auto* panel : { &instancePanel, &displayPanel, &aboutPanel }) addAndMakeVisible (panel);

    resetAll.setComponentID ("ghost");
    resetAll.setTooltip ("Puts every control on this instance back to its default. The role and targets are kept.");
    resetAll.onClick = [this]
    {
        const int role = (int) proc.getParam (ids::role);
        proc.applyChainPreset (0);
        proc.setParam (ids::role, (float) role);
        proc.resetTrim();
    };
    resetTrim.setComponentID ("ghost");
    resetTrim.onClick = [this] { proc.resetTrim(); };
    resetMeters.setComponentID ("ghost");
    resetMeters.onClick = [this] { proc.resetMeters(); };
    for (auto* b : { &resetAll, &resetTrim, &resetMeters }) instancePanel.addAndMakeVisible (b);

    instanceInfo.setFont (theme::body (12.0f));
    instanceInfo.setColour (juce::Label::textColourId, theme::textDim);
    instanceInfo.setJustificationType (juce::Justification::topLeft);
    instancePanel.addAndMakeVisible (instanceInfo);

    scaleLabel.setText ("Window size", juce::dontSendNotification);
    scaleLabel.setFont (theme::body (12.0f));
    scaleLabel.setColour (juce::Label::textColourId, theme::textDim);
    displayPanel.addAndMakeVisible (scaleLabel);
    scaleBox.addItemList ({ "Compact", "Normal", "Large" }, 1);
    scaleBox.setSelectedItemIndex (1, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        const float scales[] = { 0.9f, 1.0f, 1.2f };
        if (onScaleChanged) onScaleChanged (scales[juce::jlimit (0, 2, scaleBox.getSelectedItemIndex())]);
    };
    displayPanel.addAndMakeVisible (scaleBox);

    aboutText.setFont (theme::body (12.0f));
    aboutText.setColour (juce::Label::textColourId, theme::textDim);
    aboutText.setJustificationType (juce::Justification::topLeft);
    aboutText.setText (juce::String ("Musically Unorthodox mix assistant v") + MU_VERSION_STRING + "\n\n"
                       "Made for the MU studio. Put one instance on every track and one on the master; they talk to each other inside the project. "
                       "Nothing leaves this computer.\n\nmusicallyunorthodox.com", juce::dontSendNotification);
    aboutPanel.addAndMakeVisible (aboutText);
    refresh();
}

void SettingsPage::refresh()
{
    juce::String s;
    s << "Role: " << roleNames()[(int) proc.getEffectiveRole()] << "    Session slot: " << proc.getSessionSlot() + 1
      << "    Instances in session: " << proc.getBus().getNumInstances() << "\n"
      << "Reported latency: " << proc.getLatencySamples() << " samples at " << juce::String (proc.getSampleRate() / 1000.0, 1) << " kHz"
      << "    Chain: " << (proc.getCurrentPresetIndex() >= 0 ? chainPresetNames()[proc.getCurrentPresetIndex()] : juce::String ("custom"));
    instanceInfo.setText (s, juce::dontSendNotification);
}

void SettingsPage::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    instancePanel.setBounds (r.removeFromTop (130));
    r.removeFromTop (12);
    displayPanel.setBounds (r.removeFromTop (90));
    r.removeFromTop (12);
    aboutPanel.setBounds (r.removeFromTop (140));

    auto c = instancePanel.getContentArea();
    auto row = c.removeFromTop (30);
    resetAll.setBounds (row.removeFromLeft (190)); row.removeFromLeft (8);
    resetTrim.setBounds (row.removeFromLeft (190)); row.removeFromLeft (8);
    resetMeters.setBounds (row.removeFromLeft (150));
    c.removeFromTop (10);
    instanceInfo.setBounds (c);

    auto d = displayPanel.getContentArea();
    auto drow = d.removeFromTop (26);
    scaleLabel.setBounds (drow.removeFromLeft (110));
    scaleBox.setBounds (drow.removeFromLeft (160));

    aboutText.setBounds (aboutPanel.getContentArea());
}
}
