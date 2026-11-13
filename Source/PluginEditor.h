#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MULookAndFeel.h"
#include "ui/HeaderBar.h"
#include "ui/MeterRail.h"
#include "ui/InfoBar.h"
#include "ui/MixPage.h"
#include "ui/EQPage.h"
#include "ui/DynamicsPage.h"
#include "ui/TonePage.h"
#include "ui/MasterPage.h"
#include "ui/LearnPage.h"
#include "ui/SettingsPanel.h"

namespace mu
{
class MUEditor : public juce::AudioProcessorEditor,
                 private juce::Timer
{
public:
    explicit MUEditor (MUProcessor&);
    ~MUEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showTab (int t);
    juce::Component* page (int t);

    MUProcessor& proc;
    MULookAndFeel lnf;
    juce::Component root;              // everything is laid out at the design size and scaled as one piece
    std::function<void (const juce::String&)> status;
    HeaderBar header;
    MeterRail rail;
    InfoBar info;
    MixPage mixPage;
    EQPage eqPage;
    DynamicsPage dynamicsPage;
    TonePage tonePage;
    MasterPage masterPage;
    LearnPage learnPage;
    SettingsPanel settings;
    int tab = 0;
    float pageAnim = 1.0f;
    double lastTick = 0.0;
    static constexpr int kBaseW = 1180, kBaseH = 720;
};
}
