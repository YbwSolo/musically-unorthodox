#include "PluginEditor.h"
#include "ui/Theme.h"

namespace mu
{
MUEditor::MUEditor (MUProcessor& p)
    : AudioProcessorEditor (p), proc (p), header (p), mixPage (p), eqPage (p), compPage (p), levelsPage (p), settingsPage (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (header);
    for (auto* c : std::initializer_list<juce::Component*> { &mixPage, &eqPage, &compPage, &levelsPage, &settingsPage })
        addChildComponent (c);
    header.onTabChanged = [this] (int t) { showTab (t); };
    showTab (proc.getCurrentTab());

    setResizable (true, true);
    setResizeLimits (1000, 580, 1800, 1100);
    setSize (1040, 640);
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
    levelsPage.setVisible (tab == HeaderBar::Levels);
    settingsPage.setVisible (tab == HeaderBar::Settings);
    header.setTab (tab);
}

void MUEditor::timerCallback()
{
    header.refresh();
    switch (tab)
    {
        case HeaderBar::Mix:      if (++slow % 6 == 0) mixPage.refresh(); break;
        case HeaderBar::EQ:       eqPage.refresh(); break;
        case HeaderBar::Comp:     compPage.refresh(); break;
        case HeaderBar::Levels:   levelsPage.refresh(); break;
        case HeaderBar::Settings: if (++slow % 15 == 0) settingsPage.refresh(); break;
        default: break;
    }
}

void MUEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
}

void MUEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (64));
    for (auto* c : std::initializer_list<juce::Component*> { &mixPage, &eqPage, &compPage, &levelsPage, &settingsPage })
        c->setBounds (r);
}
}
