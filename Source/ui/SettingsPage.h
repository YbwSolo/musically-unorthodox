#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace mu
{
class SettingsPage : public juce::Component
{
public:
    explicit SettingsPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    MUProcessor& proc;
    juce::Label title, status, emailL { {}, "Email" }, keyL { {}, "Licence key" }, features, about;
    juce::TextEditor email, key;
    juce::TextButton activate { "ACTIVATE PRO" }, deactivate { "REMOVE LICENCE" };
};
}
