#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MULookAndFeel.h"
#include "ui/HeaderBar.h"
#include "ui/MixPage.h"
#include "ui/EQPage.h"
#include "ui/CompressorPage.h"
#include "ui/LevelsPage.h"
#include "ui/SettingsPage.h"

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

    MUProcessor& proc;
    MULookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    HeaderBar header;
    MixPage mixPage;
    EQPage eqPage;
    CompressorPage compPage;
    LevelsPage levelsPage;
    SettingsPage settingsPage;
    int tab = 0;
    int slow = 0;
};
}
