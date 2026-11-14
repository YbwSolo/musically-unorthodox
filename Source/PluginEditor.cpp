#include "PluginEditor.h"
#include "ui/Theme.h"

namespace mu
{
MUEditor::MUEditor (MUProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      status ([this] (const juce::String& s) { info.setStatus (s); }),
      header (p), rail (p), info (p),
      mixPage (p, status), eqPage (p, status), dynamicsPage (p, status), tonePage (p), fxPage (p, status), masterPage (p, status),
      learnPage (p, status, [this] (int t) { showTab (t); }),
      settings (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    for (auto* c : std::initializer_list<juce::Component*> { &mixPage, &eqPage, &dynamicsPage, &tonePage, &fxPage, &masterPage, &learnPage }) root.addChildComponent (c);
    root.addAndMakeVisible (header);
    root.addAndMakeVisible (rail);
    root.addAndMakeVisible (info);
    root.addChildComponent (settings);

    // design-size layout, done once
    {
        auto r = juce::Rectangle<int> (0, 0, kBaseW, kBaseH);
        root.setBounds (r);
        header.setBounds (r.removeFromTop (58));
        info.setBounds (r.removeFromBottom (46));
        rail.setBounds (r.removeFromRight (78));
        for (int i = 0; i < HeaderBar::NumTabs; ++i) page (i)->setBounds (r);
        settings.setBounds (0, 0, kBaseW, kBaseH);
    }

    header.onTabChanged = [this] (int t) { showTab (t); };
    header.onStatus = status;
    header.onSettings = [this] { settings.refresh(); settings.setVisible (true); settings.toFront (false); };
    settings.onClose = [this] { settings.setVisible (false); };
    settings.onScaleChanged = [this] (float s) { setSize (juce::roundToInt ((float) kBaseW * s), juce::roundToInt ((float) kBaseH * s)); };
    sendLookAndFeelChange();   // children were built before the look-and-feel was set

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setResizeLimits (kBaseW * 3 / 4, kBaseH * 3 / 4, kBaseW * 2, kBaseH * 2);
    static const float scales[] = { 0.85f, 1.0f, 1.25f };
    const float scale = scales[juce::jlimit (0, 2, (int) proc.apvts.state.getProperty ("uiSize", 1))];
    setSize (juce::roundToInt ((float) kBaseW * scale), juce::roundToInt ((float) kBaseH * scale));

    showTab (proc.getCurrentTab());
    pageAnim = 1.0f;
    page (tab)->setAlpha (1.0f);
    proc.editorOpened();
    proc.getUpdater().autoCheck();
    proc.getAI().refresh();
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

MUEditor::~MUEditor()
{
    stopTimer();
    proc.editorClosed();
    proc.setListenMode (Listen::Normal);
    proc.setSoloBand (-1);
    setLookAndFeel (nullptr);
}

juce::Component* MUEditor::page (int t)
{
    switch (t)
    {
        case HeaderBar::Mix:      return &mixPage;
        case HeaderBar::EQ:       return &eqPage;
        case HeaderBar::Dynamics: return &dynamicsPage;
        case HeaderBar::Tone:     return &tonePage;
        case HeaderBar::FX:       return &fxPage;
        case HeaderBar::Master:   return &masterPage;
        default:                  return &learnPage;
    }
}

void MUEditor::showTab (int t)
{
    const int previous = tab;
    tab = juce::jlimit (0, HeaderBar::NumTabs - 1, t);
    slideFrom = tab > previous ? 36.0f : tab < previous ? -36.0f : 0.0f;      // the new page arrives from the side its tab is on
    for (int i = 0; i < HeaderBar::NumTabs; ++i) page (i)->setVisible (i == tab);
    proc.setCurrentTab (tab);
    header.setTab (tab);
    pageAnim = 0.0f;
    page (tab)->setAlpha (0.0f);
}

void MUEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    header.tick (dt);
    rail.tick (dt);
    info.tick (dt, root, tab);
    if (pageAnim < 1.0f)
    {
        // a new page settles into place rather than snapping
        pageAnim = juce::jmin (1.0f, pageAnim + dt * 7.0f);
        const float eased = 1.0f - (1.0f - pageAnim) * (1.0f - pageAnim);
        page (tab)->setAlpha (eased);
        page (tab)->setTransform (juce::AffineTransform::translation ((1.0f - eased) * slideFrom, 0.0f));
    }
    switch (tab)
    {
        case HeaderBar::Mix:      mixPage.tick (dt); break;
        case HeaderBar::EQ:       eqPage.tick (dt); break;
        case HeaderBar::Dynamics: dynamicsPage.tick (dt); break;
        case HeaderBar::Tone:     tonePage.tick (dt); break;
        case HeaderBar::FX:       fxPage.tick (dt); break;
        case HeaderBar::Master:   masterPage.tick (dt); break;
        default:                  learnPage.tick (dt); break;
    }
    if (settings.isVisible()) settings.refresh();
}

void MUEditor::paint (juce::Graphics& g) { g.fillAll (theme::black); }

void MUEditor::resized()
{
    const float scale = juce::jmin ((float) getWidth() / (float) kBaseW, (float) getHeight() / (float) kBaseH);
    root.setTransform (juce::AffineTransform::scale (scale));
}
}
