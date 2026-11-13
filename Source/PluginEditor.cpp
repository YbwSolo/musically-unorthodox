#include "PluginEditor.h"
#include "ui/Theme.h"

namespace mu
{
MUEditor::MUEditor (MUProcessor& p)
    : AudioProcessorEditor (p), proc (p), header (p), mixPage (p), eqPage (p), compPage (p), chainPage (p), levelsPage (p), helpPage (p), settingsPage (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (header);
    for (auto* c : std::initializer_list<juce::Component*> { &mixPage, &eqPage, &compPage, &chainPage, &levelsPage, &helpPage, &settingsPage })
        addChildComponent (c);
    header.onTabChanged = [this] (int t) { showTab (t); };
    header.onStatus = [this] (const juce::String& s)
    {
        toast.setText (s, juce::dontSendNotification);
        toast.setVisible (true);
        toastUntil = juce::Time::getMillisecondCounterHiRes() + 6000.0;
        mixPage.setStatus (s);
    };
    toast.setFont (theme::body (12.5f));
    toast.setColour (juce::Label::textColourId, theme::onYellow);
    toast.setColour (juce::Label::backgroundColourId, theme::yellow);
    toast.setJustificationType (juce::Justification::centred);
    addChildComponent (toast);

    settingsPage.onScaleChanged = [this] (float s) { setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s)); };
    sendLookAndFeelChange();   // pages were built before the look-and-feel was set; refresh cached slider text boxes etc.

    showTab (proc.getCurrentTab());
    setResizable (true, true);
    setResizeLimits (980, 600, 2000, 1300);
    setSize (kBaseW, kBaseH);
    startTimerHz (30);
}

MUEditor::~MUEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MUEditor::showTab (int t)
{
    tab = juce::jlimit (0, HeaderBar::NumTabs - 1, t);
    mixPage.setVisible (tab == HeaderBar::Mix);
    eqPage.setVisible (tab == HeaderBar::EQ);
    compPage.setVisible (tab == HeaderBar::Comp);
    chainPage.setVisible (tab == HeaderBar::Chain);
    levelsPage.setVisible (tab == HeaderBar::Levels);
    helpPage.setVisible (tab == HeaderBar::Help);
    settingsPage.setVisible (tab == HeaderBar::Settings);
    header.setTab (tab);
    if (tab == HeaderBar::Mix) mixPage.refresh();
    if (tab == HeaderBar::Settings) settingsPage.refresh();
}

void MUEditor::timerCallback()
{
    header.refresh();
    ++slow;
    switch (tab)
    {
        case HeaderBar::Mix:      if (slow % 6 == 0) mixPage.refresh(); break;
        case HeaderBar::EQ:       eqPage.refresh(); break;
        case HeaderBar::Comp:     compPage.refresh(); break;
        case HeaderBar::Chain:    chainPage.refresh(); break;
        case HeaderBar::Levels:   levelsPage.refresh(); break;
        case HeaderBar::Settings: if (slow % 15 == 0) settingsPage.refresh(); break;
        default: break;
    }
    if (toast.isVisible() && juce::Time::getMillisecondCounterHiRes() > toastUntil) toast.setVisible (false);
}

void MUEditor::paint (juce::Graphics& g) { g.fillAll (theme::bg); }

void MUEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (72));
    for (auto* c : std::initializer_list<juce::Component*> { &mixPage, &eqPage, &compPage, &chainPage, &levelsPage, &helpPage, &settingsPage })
        c->setBounds (r);
    toast.setBounds (r.removeFromBottom (26).reduced (18, 0).translated (0, -4));
}
}
