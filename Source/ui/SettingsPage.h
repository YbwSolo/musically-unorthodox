#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Widgets.h"

namespace mu
{
class SettingsPage : public juce::Component
{
public:
    explicit SettingsPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

    std::function<void (float)> onScaleChanged;

private:
    MUProcessor& proc;
    ModulePanel instancePanel { "This instance", "housekeeping" };
    ModulePanel displayPanel { "Display", "" };
    ModulePanel aboutPanel { "About", "" };
    juce::TextButton resetAll { "RESET THIS INSTANCE" }, resetTrim { "RESET BALANCE TRIM" }, resetMeters { "RESET METERS" };
    juce::ComboBox scaleBox;
    juce::Label scaleLabel, instanceInfo, aboutText;
};
}
