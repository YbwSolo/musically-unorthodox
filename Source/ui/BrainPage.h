#pragma once
#include "Widgets.h"

namespace mu
{
// Mix Brain: the tutor's own tab. A conversation about this track with the AI model that runs on this computer
// and, until that model has been downloaded, the choice of which one to get and how big each is.
class BrainPage : public juce::Component
{
public:
    BrainPage (MUProcessor&, std::function<void (const juce::String&)> setStatus);
    ~BrainPage() override;
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Talk;
    enum class View { Chat, Install, Downloading, Unsupported };
    void ask (const juce::String& question);
    void refresh();
    void layoutTalk (bool toEnd);
    juce::String sizeText (juce::int64 bytes, const char* fallback) const;

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    View view = View::Install;
    juce::Viewport viewport;
    std::unique_ptr<Talk> talk;
    juce::TextEditor askBox;
    juce::TextButton askButton, clearButton, getLight, getSmart, stopButton, otherButton;
    std::array<juce::TextButton, 4> starters;
    juce::Rectangle<int> mainArea, sideArea, cardLight, cardSmart, progressArea;
    juce::String lightName, smartName, lightSize, smartSize, smartNote, lightNote, setupMessage, modelLine, otherLine, facts;
    juce::String askedQuestion; juce::int64 askedAt = 0;
    bool smartFits = true, haveChoices = false, failed = false, busyShown = false;
    float progress = 0.0f, refreshTimer = 0.0f, dots = 0.0f;
    int lastAnswerId = 0;
};
}
