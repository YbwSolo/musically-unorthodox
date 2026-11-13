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
    roleBox.setWantsKeyboardFocus (false);
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
    styleButton (checkButton, "Check for updates", false);
    checkButton.onClick = [this] { proc.getUpdater().checkNow(); };
    styleButton (installButton, "Install update", true);
    installButton.onClick = [this] { proc.getUpdater().installNow(); };
    styleButton (aiButton, "Look again", false);
    aiButton.onClick = [this] { proc.getAI().refresh(); };
    for (auto* c : std::initializer_list<juce::Component*> { &roleBox, &sizeSwitch, &takePart, &closeButton, &resetButton, &checkButton, &installButton, &aiButton }) addAndMakeVisible (c);

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

    const auto st = proc.getUpdater().getState();
    using Phase = UpdateState::Phase;
    juce::String line = st.message;
    if (st.phase == Phase::Idle) line = "Updates install from inside the plugin. Nothing to download by hand.";
    const bool busy = st.phase == Phase::Checking || st.phase == Phase::Downloading || st.phase == Phase::Installing;
    checkButton.setEnabled (! busy && st.phase != Phase::Installed);
    installButton.setVisible (st.phase == Phase::Available);
    const bool prog = st.phase == Phase::Downloading || st.phase == Phase::Installing;
    const auto n = (st.phase == Phase::Available || st.phase == Phase::Installed) ? st.latest.notes : juce::String();

    const auto ai = proc.getAI().getStatus();
    const juce::String aiNow = ai.available ? "On. Using the model \"" + ai.model + "\" on this computer. Ask it questions from the Mix Brain panel on the EQ tab."
                                            : juce::String ("Off. It needs Ollama, a free program that runs AI models on this computer, so nothing you ask leaves the room. "
                                                            "1. Install Ollama from ollama.com.  2. Open Terminal and run:  ollama pull llama3.2  3. Press Look again.");

    if (d != detected || line != updateLine || prog != showProgress || std::abs (st.progress - progress) > 0.004f || n != notes || aiNow != aiLine)
    {
        detected = d; updateLine = line; showProgress = prog; progress = st.progress; notes = n; aiLine = aiNow;
        repaint();
    }
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
    g.setColour (line);
    g.fillRect (card.getCentreX() + 8, card.getY() + 62, 1, card.getHeight() - 130);

    drawWrapped (g, updateLine, updateText.toFloat(), body (12.5f), textDim, 1.35f);
    if (showProgress)
    {
        g.setColour (control); g.fillRoundedRectangle (updateBar.toFloat(), 2.0f);
        g.setColour (yellow); g.fillRoundedRectangle (updateBar.toFloat().withWidth ((float) updateBar.getWidth() * juce::jlimit (0.02f, 1.0f, progress)), 2.0f);
    }
    if (notes.isNotEmpty())
    {
        g.saveState(); g.reduceClipRegion (notesArea);
        g.setFont (bodyBold (11.5f)); g.setColour (text);
        g.drawText ("What is new", notesArea.withHeight (16), juce::Justification::centredLeft);
        drawWrapped (g, notes, notesArea.withTrimmedTop (20).toFloat(), body (11.5f), textMute, 1.3f);
        g.restoreState();
    }
    drawWrapped (g, aiLine, aiText.toFloat(), body (12.0f), textDim, 1.35f);

    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText ("Musically Unorthodox mix assistant. In Music In Media C.I.C., Northampton.",
                card.reduced (24, 0).removeFromBottom (58).withTrimmedRight (110), juce::Justification::centredLeft, true);
}

void SettingsPanel::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (juce::jmin (860, getWidth() - 40), juce::jmin (560, getHeight() - 30));
    labels.clear();
    auto all = card.reduced (24, 0);
    all.removeFromTop (58);
    auto bottom = all.removeFromBottom (58);
    closeButton.setBounds (bottom.removeFromRight (96).withSizeKeepingCentre (96, 30));

    auto r = all.removeFromLeft (all.getWidth() / 2 - 16);
    all.removeFromLeft (32);
    auto row = [&r] (int h) { auto x = r.removeFromTop (h); r.removeFromTop (8); return x; };
    labels.push_back ({ "#Track role", row (18) });
    auto roleRow = row (30);
    roleBox.setBounds (roleRow.removeFromLeft (190));
    roleRow.removeFromLeft (12);
    labels.push_back ({ detected, roleRow });
    takePart.setBounds (row (22));
    r.removeFromTop (4);
    labels.push_back ({ "#Window size", row (18) });
    auto sizeRow = row (28);
    sizeSwitch.setBounds (sizeRow.removeFromLeft (250));
    resetButton.setBounds (sizeRow.removeFromRight (108));
    r.removeFromTop (4);
    labels.push_back ({ "#How loud each role sits against the lead vocal", row (18) });
    const int rowH = juce::jlimit (20, 26, (r.getHeight() - 4) / juce::jmax (1, offsetSliders.size()));
    int role = (int) Role::Vocal + 1;
    for (auto* s : offsetSliders)
    {
        auto line1 = r.removeFromTop (rowH);
        labels.push_back ({ roleNames()[role++], line1.removeFromLeft (112) });
        s->setBounds (line1);
    }

    // right column
    auto c = all;
    auto crow = [&c] (int h) { auto x = c.removeFromTop (h); c.removeFromTop (8); return x; };
    labels.push_back ({ "#Updates", crow (18) });
    labels.push_back ({ "This is version " + Updater::currentVersion() + (Updater::currentBuild() > 0 ? "" : " (a local test build)"), crow (16) });
    updateText = crow (36);
    updateBar = crow (4);
    auto buttons = crow (28);
    checkButton.setBounds (buttons.removeFromLeft (150));
    buttons.removeFromLeft (8);
    installButton.setBounds (buttons.removeFromLeft (130));
    notesArea = crow (112);
    c.removeFromTop (2);
    labels.push_back ({ "#AI tutor", crow (18) });
    aiText = crow (88);
    aiButton.setBounds (crow (28).removeFromLeft (110));
}
}
