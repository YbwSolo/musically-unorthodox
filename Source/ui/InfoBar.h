#pragma once
#include "Widgets.h"

namespace mu
{
// The strip along the bottom. Hover any control and it explains what the control is for; when the mouse is
// elsewhere it shows the last result, or a live tip about what the audio is doing.
class InfoBar : public juce::Component
{
public:
    explicit InfoBar (MUProcessor&);
    void tick (float dt, juce::Component& root, int tab);
    void setStatus (const juce::String& s);
    void paint (juce::Graphics&) override;

private:
    HelpEntry coach (int tab) const;
    MUProcessor& proc;
    HelpEntry shown;
    bool shownIsHelp = false;
    juce::String status;
    float statusLeft = 0.0f, coachTimer = 0.0f, fade = 1.0f;
    HelpEntry coachEntry;
};
}
