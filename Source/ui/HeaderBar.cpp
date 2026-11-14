#include "HeaderBar.h"
#include "../PluginProcessor.h"
#include "BinaryData.h"

namespace mu
{
using namespace theme;
static const char* const kTabNames[] = { "MIX", "EQ", "DYNAMICS", "TONE", "FX", "MASTER", "LEARN" };

HeaderBar::HeaderBar (MUProcessor& p) : proc (p)
{
    logo = juce::ImageCache::getFromMemory (BinaryData::mu_logo_png, BinaryData::mu_logo_pngSize);

    styleButton (presetButton, "Presets", false, "header.preset");
    presetButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetButton);

    setHelp (abSwitch, "header.ab");
    abSwitch.onChange = [this] (int side)
    {
        if (juce::ModifierKeys::getCurrentModifiers().isShiftDown())
        {
            proc.copyToOtherAB();
            abSwitch.setIndex (proc.getABSlot());
            if (onStatus) onStatus (juce::String ("Current settings copied to ") + (proc.getABSlot() == 0 ? "B." : "A."));
        }
        else if (side != proc.getABSlot())
        {
            proc.switchAB();
            if (onStatus) onStatus (juce::String ("Now hearing ") + (side == 0 ? "A" : "B") + ". Flip back and forth while the song plays; shift-click copies the current settings to the other side.");
        }
    };
    addAndMakeVisible (abSwitch);

    bypassButton.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypassButton);
    addAndMakeVisible (bypassButton);

    styleButton (updateButton, "Update", true, "header.update");
    updateButton.onClick = [this] { if (onSettings) onSettings(); };
    addChildComponent (updateButton);

    styleButton (settingsButton, "Settings", false, "header.settings");
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };
    addAndMakeVisible (settingsButton);
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void HeaderBar::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());     // menus open in their own window, so hand them the brand look
    const auto& presets = chainPresets();
    const auto current = proc.getCurrentPresetName();
    for (auto& category : chainPresetCategories())
    {
        juce::PopupMenu sub;
        sub.setLookAndFeel (&getLookAndFeel());
        bool hasCurrent = false;
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].category == category)
            {
                const bool ticked = presets[i].name == current;
                hasCurrent = hasCurrent || ticked;
                sub.addItem ((int) i + 1, presets[i].name, true, ticked);
            }
        menu.addSubMenu (category, sub, true, nullptr, hasCurrent);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
        [safe = juce::Component::SafePointer<HeaderBar> (this)] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            const auto& all = chainPresets();
            const auto note = safe->proc.applyChainPreset (result - 1);
            if (safe->onStatus) safe->onStatus (all[(size_t) (result - 1)].blurb + " " + note.fromFirstOccurrenceOf ("loaded.", false, false).trim());
        });
}

juce::Rectangle<float> HeaderBar::tabBounds (int t) const
{
    const auto font = headline (19.0f);
    float x = (float) tabArea.getX();
    for (int i = 0; i < NumTabs; ++i)
    {
        const float w = font.getStringWidthFloat (kTabNames[i]) + 26.0f;
        if (i == t) return { x, (float) tabArea.getCentreY() - 15.0f, w, 30.0f };
        x += w + 2.0f;
    }
    return {};
}

int HeaderBar::tabAt (juce::Point<int> p) const
{
    for (int i = 0; i < NumTabs; ++i)
        if (tabBounds (i).expanded (1.0f, 10.0f).contains (p.toFloat())) return i;
    return -1;
}

void HeaderBar::setTab (int t)
{
    tab = juce::jlimit (0, NumTabs - 1, t);
    if (! blockPlaced && ! tabArea.isEmpty())
    {
        const auto b = tabBounds (tab);
        blockX = b.getX(); blockW = b.getWidth(); blockPlaced = true;
    }
    repaint();
}

void HeaderBar::tick (float dt)
{
    const auto target = tabBounds (tab);
    if (! blockPlaced) { blockX = target.getX(); blockW = target.getWidth(); blockPlaced = true; repaint(); }
    if (std::abs (blockX - target.getX()) > 0.3f || std::abs (blockW - target.getWidth()) > 0.3f)
    {
        blockX = approach (blockX, target.getX(), dt, 0.055f);
        blockW = approach (blockW, target.getWidth(), dt, 0.055f);
        repaint (tabArea.expanded (4, 0));
    }

    if (proc.getBus().isLowLatency() != shownLowLatency) { shownLowLatency = ! shownLowLatency; repaint(); }

    const auto name = proc.getCurrentPresetName();
    if (name != shownPreset)
    {
        shownPreset = name;
        presetButton.setButtonText (name.isEmpty() ? "Presets" : name);
    }
    // an update that is waiting, or one that needs a restart, gets a button of its own
    const auto phase = proc.getUpdater().getState().phase;
    const int want = phase == UpdateState::Phase::Available ? 1 : phase == UpdateState::Phase::Installed ? 2 : 0;
    if (want != shownUpdate)
    {
        shownUpdate = want;
        updateButton.setButtonText (want == 2 ? "Restart to finish" : "Update ready");
        updateButton.setVisible (want != 0);
        resized();
    }
    if (want != 0)
    {
        updatePulse = std::fmod (updatePulse + dt, 2.4f);
        updateButton.setAlpha (0.82f + 0.18f * std::cos (updatePulse / 2.4f * juce::MathConstants<float>::twoPi));
    }
    if (proc.getABSlot() != shownAB)
    {
        shownAB = proc.getABSlot();
        abSwitch.setIndex (shownAB);
    }
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.fillAll (black);
    g.setColour (line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    if (logo.isValid())
    {
        const float h = 34.0f, w = h * (float) logo.getWidth() / (float) logo.getHeight();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logo, juce::Rectangle<float> (16.0f, ((float) getHeight() - h) * 0.5f, w, h));
    }

    // the active tab sits on a yellow block that slides between tabs
    g.setColour (yellow);
    g.fillRect (juce::Rectangle<float> (blockX, (float) tabArea.getCentreY() - 15.0f, blockW, 30.0f));
    g.setFont (headline (19.0f));
    for (int i = 0; i < NumTabs; ++i)
    {
        const auto b = tabBounds (i);
        const float overlap = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs ((blockX + blockW * 0.5f) - b.getCentreX()) / (b.getWidth() * 0.6f));
        g.setColour (overlap > 0.5f ? onYellow : (i == hoverTab ? text : textDim));
        g.drawText (kTabNames[i], b.translated (0.0f, 1.0f), juce::Justification::centred);
    }

    // a reminder that the session is in recording mode, not full quality
    if (shownLowLatency)
    {
        const auto last = tabBounds (NumTabs - 1);
        const auto pill = juce::Rectangle<float> (last.getRight() + 14.0f, last.getCentreY() - 11.0f, 96.0f, 22.0f);
        g.setColour (yellow); g.drawRoundedRectangle (pill, 11.0f, 1.2f);
        g.setFont (bodyBold (11.0f));
        g.drawText ("Low latency", pill, juce::Justification::centred);
    }
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    const int logoW = juce::roundToInt (34.0f * (logo.isValid() ? (float) logo.getWidth() / (float) logo.getHeight() : 4.9f));
    r.removeFromLeft (logoW + 28);
    auto right = r.removeFromRight (400).withSizeKeepingCentre (400, 28);
    settingsButton.setBounds (right.removeFromRight (76));
    right.removeFromRight (8);
    bypassButton.setBounds (right.removeFromRight (28));
    right.removeFromRight (8);
    abSwitch.setBounds (right.removeFromRight (56));
    right.removeFromRight (8);
    if (updateButton.isVisible()) { updateButton.setBounds (right.removeFromLeft (shownUpdate == 2 ? 118 : 96)); right.removeFromLeft (8); }
    presetButton.setBounds (right);
    if (tabArea != r) { tabArea = r; blockPlaced = false; }
}

void HeaderBar::mouseDown (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    if (t >= 0 && t != tab) { setTab (t); if (onTabChanged) onTabChanged (t); }
}

void HeaderBar::mouseMove (const juce::MouseEvent& e)
{
    const int t = tabAt (e.getPosition());
    if (t != hoverTab) { hoverTab = t; repaint (tabArea); }
    setMouseCursor (t >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void HeaderBar::mouseExit (const juce::MouseEvent&) { hoverTab = -1; repaint (tabArea); }
}
