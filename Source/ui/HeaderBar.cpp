#include "HeaderBar.h"
#include "Theme.h"
#include "BinaryData.h"
#include "../license/License.h"

namespace mu
{
HeaderBar::HeaderBar (MUProcessor& p) : proc (p)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::mu_logo_white_png, BinaryData::mu_logo_white_pngSize);

    roleBox.addItemList (roleNames(), 1);
    roleBox.setTooltip ("What this instance is sitting on. Auto detect listens and guesses; set it yourself for the best balance.");
    addAndMakeVisible (roleBox);
    roleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::role, roleBox);

    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypass);

    const char* names[NumTabs] = { "MIX", "EQ", "COMPRESSOR", "LEVELS", "SETTINGS" };
    for (int i = 0; i < NumTabs; ++i)
    {
        auto& b = tabs[(size_t) i];
        b.setButtonText (names[i]);
        b.setClickingTogglesState (false);
        b.setRadioGroupId (0);
        b.onClick = [this, i] { setTab (i); if (onTabChanged) onTabChanged (i); };
        addAndMakeVisible (b);
    }
    setTab (proc.getCurrentTab());
    refresh();
}

void HeaderBar::setTab (int t)
{
    currentTab = juce::jlimit (0, NumTabs - 1, t);
    for (int i = 0; i < NumTabs; ++i) tabs[(size_t) i].setToggleState (i == currentTab, juce::dontSendNotification);
    proc.setCurrentTab (currentTab);
    repaint();
}

void HeaderBar::refresh()
{
    const auto guess = proc.getDetectedRole();
    juce::String t;
    if ((int) proc.apvts.getRawParameterValue (ids::role)->load() == (int) Role::Auto)
        t = guess.confidence > 0.0f ? "hearing: " + roleNames()[(int) guess.role].toLowerCase() : "listening...";
    const bool newPro = License::get().isPro();
    if (t != detectedText || newPro != pro) { detectedText = t; pro = newPro; repaint(); }
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (theme::panel);
    g.fillRect (r);
    g.setColour (theme::line);
    g.fillRect (r.removeFromBottom (1));

    auto left = getLocalBounds().removeFromLeft (320).reduced (14, 12);
    auto logoArea = left.removeFromLeft (200).toFloat();
    if (logo.isValid())
    {
        const float aspect = (float) logo.getWidth() / (float) logo.getHeight();
        auto dst = logoArea.withSizeKeepingCentre (juce::jmin (logoArea.getWidth(), logoArea.getHeight() * aspect), juce::jmin (logoArea.getHeight(), logoArea.getWidth() / aspect));
        g.setOpacity (1.0f);
        g.drawImage (logo, dst, juce::RectanglePlacement::centred);
    }
    left.removeFromLeft (12);
    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (11.0f));
    g.drawText ("mix assistant", left.removeFromTop (left.getHeight() / 2), juce::Justification::bottomLeft);
    g.setColour (pro ? theme::pro : theme::textMute);
    g.setFont (theme::titleFont (11.0f));
    g.drawText (juce::String ("v") + MU_VERSION_STRING + (pro ? "  PRO" : "  FREE"), left, juce::Justification::topLeft);

    if (detectedText.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (11.0f));
        auto under = roleBox.getBounds().translated (0, roleBox.getHeight() + 1).withHeight (14);
        g.drawText (detectedText, under, juce::Justification::centredLeft);
    }
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    r.removeFromLeft (320);
    auto right = r.removeFromRight (270);
    bypass.setBounds (right.removeFromRight (90).withSizeKeepingCentre (90, 26));
    right.removeFromRight (10);
    roleBox.setBounds (right.removeFromRight (150).withSizeKeepingCentre (150, 26).translated (0, -6));

    const int widths[NumTabs] = { 56, 46, 108, 76, 90 };
    int total = 0;
    for (int w : widths) total += w;
    auto tabArea = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), total), 30);
    for (int i = 0; i < NumTabs; ++i)
        tabs[(size_t) i].setBounds (tabArea.removeFromLeft (widths[i]).reduced (3, 0));
}
}
