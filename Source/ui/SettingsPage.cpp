#include "SettingsPage.h"
#include "Theme.h"
#include "../license/License.h"

namespace mu
{
SettingsPage::SettingsPage (MUProcessor& p) : proc (p)
{
    title.setFont (theme::titleFont (14.0f));
    title.setText ("LICENCE", juce::dontSendNotification);
    addAndMakeVisible (title);
    status.setFont (theme::labelFont (12.0f));
    addAndMakeVisible (status);
    for (auto* l : { &emailL, &keyL })
    {
        l->setFont (theme::labelFont (12.0f));
        l->setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (l);
    }
    email.setTextToShowWhenEmpty ("you@example.com", theme::textMute);
    key.setTextToShowWhenEmpty ("MU-XXXX-XXXX-XXXX-XXXX", theme::textMute);
    for (auto* e : { &email, &key })
    {
        e->setFont (theme::monoFont (13.0f));
        e->setIndents (8, 4);
        addAndMakeVisible (e);
    }
    activate.onClick = [this]
    {
        if (License::get().activate (email.getText(), key.getText()))
            status.setText ("Pro activated. Thank you.", juce::dontSendNotification);
        else
            status.setText ("That key does not match this email. Check for typos or get in touch.", juce::dontSendNotification);
        refresh();
    };
    addAndMakeVisible (activate);
    deactivate.onClick = [this] { License::get().deactivate(); refresh(); };
    addAndMakeVisible (deactivate);

    features.setFont (theme::labelFont (12.0f));
    features.setColour (juce::Label::textColourId, theme::textDim);
    features.setJustificationType (juce::Justification::topLeft);
    features.setText ("FREE\n"
                      "  Session view, role detection, loudness and true-peak metering, Balance Now, manual EQ and compressor.\n\n"
                      "PRO\n"
                      "  Auto balance that keeps following the mix, masking fixes between tracks, Clean Harsh resonance cuts,\n"
                      "  Vocal Space ducking on the beat, Auto Set on the compressor.", juce::dontSendNotification);
    addAndMakeVisible (features);

    about.setFont (theme::labelFont (11.0f));
    about.setColour (juce::Label::textColourId, theme::textMute);
    about.setJustificationType (juce::Justification::bottomLeft);
    about.setText (juce::String ("Musically Unorthodox mix assistant v") + MU_VERSION_STRING
                   + "\nHow it works: put one instance on every track plus one on the master (role: Master). Instances in the same session talk to each other; "
                     "nothing leaves your computer and nothing runs in the cloud.", juce::dontSendNotification);
    addAndMakeVisible (about);
    refresh();
}

void SettingsPage::refresh()
{
    auto& lic = License::get();
    const bool pro = lic.isPro();
    if (pro)
    {
        status.setColour (juce::Label::textColourId, theme::good);
        if (! status.getText().startsWith ("Pro activated")) status.setText ("Pro licence active for " + lic.getEmail(), juce::dontSendNotification);
        email.setText (lic.getEmail(), false);
        key.setText (lic.getKey(), false);
    }
    else
    {
        status.setColour (juce::Label::textColourId, theme::textDim);
        if (status.getText().isEmpty() || status.getText().startsWith ("Pro licence")) status.setText ("Free version. Enter your email and key to unlock Pro.", juce::dontSendNotification);
    }
    email.setEnabled (! pro); key.setEnabled (! pro);
    activate.setVisible (! pro);
    deactivate.setVisible (pro);
}

void SettingsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (16);
    theme::drawPanel (g, r.removeFromTop (150).toFloat(), 8.0f);
}

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto panel = r.removeFromTop (150).reduced (14, 10);
    title.setBounds (panel.removeFromTop (22));
    status.setBounds (panel.removeFromTop (20));
    panel.removeFromTop (6);
    auto row = panel.removeFromTop (28);
    emailL.setBounds (row.removeFromLeft (80));
    email.setBounds (row.removeFromLeft (260));
    row.removeFromLeft (16);
    keyL.setBounds (row.removeFromLeft (90));
    key.setBounds (row.removeFromLeft (230));
    panel.removeFromTop (10);
    auto brow = panel.removeFromTop (28);
    activate.setBounds (brow.removeFromLeft (150));
    deactivate.setBounds (brow.removeFromLeft (150));
    r.removeFromTop (12);
    features.setBounds (r.removeFromTop (120));
    about.setBounds (r);
}
}
