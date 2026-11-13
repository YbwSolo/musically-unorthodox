#include "HeaderBar.h"
#include "Theme.h"
#include "BinaryData.h"

namespace mu
{
static const char* kTabNames[HeaderBar::NumTabs] = { "MIX", "EQ", "COMP", "CHAIN", "LEVELS", "HELP", "SETTINGS" };
static const int kTabWidths[HeaderBar::NumTabs] = { 48, 38, 54, 60, 66, 52, 80 };

HeaderBar::HeaderBar (MUProcessor& p) : proc (p)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::mu_logo_white_png, BinaryData::mu_logo_white_pngSize);

    roleBox.addItemList (roleNames(), 1);
    roleBox.setTooltip ("What this instance is sitting on. Auto detect listens and guesses; setting it yourself gives the best balance.");
    addAndMakeVisible (roleBox);
    roleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::role, roleBox);

    chainBox.setTextWhenNothingSelected ("LOAD CHAIN");
    chainBox.addItemList (chainPresetNames(), 1);
    chainBox.setTooltip ("Whole-chain starting points built from the MU studio template. Loading one sets the role too.");
    chainBox.onChange = [this]
    {
        const int idx = chainBox.getSelectedItemIndex();
        if (idx < 0) return;
        const auto msg = proc.applyChainPreset (idx);
        if (onStatus) onStatus (msg);
    };
    addAndMakeVisible (chainBox);

    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypass);

    setTab (proc.getCurrentTab());
    refresh();
}

void HeaderBar::setTab (int t)
{
    currentTab = juce::jlimit (0, NumTabs - 1, t);
    proc.setCurrentTab (currentTab);
    repaint();
}

void HeaderBar::refresh()
{
    const auto guess = proc.getDetectedRole();
    juce::String t;
    if ((int) proc.apvts.getRawParameterValue (ids::role)->load() == (int) Role::Auto)
        t = guess.confidence > 0.0f ? "hearing: " + roleNames()[(int) guess.role].toLowerCase() : "listening...";
    if (t != detectedText) { detectedText = t; repaint(); }
    const int preset = proc.getCurrentPresetIndex();
    if (preset != chainBox.getSelectedItemIndex() && preset >= 0)
        chainBox.setSelectedItemIndex (preset, juce::dontSendNotification);
}

juce::Rectangle<int> HeaderBar::tabRect (int i) const
{
    int x = tabsArea.getX();
    for (int k = 0; k < i; ++k) x += kTabWidths[k];
    return { x, tabsArea.getY(), kTabWidths[i], tabsArea.getHeight() };
}

void HeaderBar::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < NumTabs; ++i) if (tabRect (i).contains (e.getPosition())) h = i;
    if (h != hoverTab) { hoverTab = h; repaint(); }
}

void HeaderBar::mouseExit (const juce::MouseEvent&) { hoverTab = -1; repaint(); }

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < NumTabs; ++i)
        if (tabRect (i).contains (e.getPosition()))
        {
            setTab (i);
            if (onTabChanged) onTabChanged (i);
            return;
        }
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (theme::black);
    g.fillRect (r);
    g.setColour (theme::line);
    g.fillRect (r.removeFromBottom (1));

    // logo
    auto left = getLocalBounds().removeFromLeft (236).reduced (16, 16);
    if (logo.isValid())
    {
        const float aspect = (float) logo.getWidth() / (float) logo.getHeight();
        auto area = left.toFloat();
        auto dst = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight() * aspect), juce::jmin (area.getHeight(), area.getWidth() / aspect));
        dst.setX (area.getX());
        g.drawImage (logo, dst, juce::RectanglePlacement::centred);
    }

    // tabs
    for (int i = 0; i < NumTabs; ++i)
    {
        auto tr = tabRect (i);
        const bool active = i == currentTab;
        g.setColour (active ? theme::text : (i == hoverTab ? theme::text.withAlpha (0.85f) : theme::textDim));
        g.setFont (theme::headline (16.0f));
        g.drawText (kTabNames[i], tr, juce::Justification::centred);
        if (active)
        {
            g.setColour (theme::yellow);
            g.fillRect (tr.withTrimmedLeft (8).withTrimmedRight (8).removeFromBottom (3));
        }
    }

    // detected role hint under the role box
    if (detectedText.isNotEmpty())
    {
        g.setColour (theme::textMute);
        g.setFont (theme::body (10.5f));
        g.drawText (detectedText, roleBox.getBounds().translated (0, roleBox.getHeight() + 1).withHeight (13), juce::Justification::centredLeft);
    }
    g.setColour (theme::textMute);
    g.setFont (theme::body (10.5f));
    g.drawText ("ROLE", roleBox.getBounds().translated (0, -14).withHeight (13), juce::Justification::centredLeft);
    g.drawText ("CHAIN", chainBox.getBounds().translated (0, -14).withHeight (13), juce::Justification::centredLeft);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (236);
    auto right = r.removeFromRight (436).reduced (12, 0);
    bypass.setBounds (right.removeFromRight (82).withSizeKeepingCentre (82, 24));
    right.removeFromRight (8);
    roleBox.setBounds (right.removeFromRight (146).withSizeKeepingCentre (146, 26).translated (0, 2));
    right.removeFromRight (8);
    chainBox.setBounds (right.removeFromRight (168).withSizeKeepingCentre (168, 26).translated (0, 2));

    int total = 0;
    for (int w : kTabWidths) total += w;
    tabsArea = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), total), r.getHeight()).withTrimmedBottom (1);
}
}
