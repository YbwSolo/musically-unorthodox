#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Widgets.h"

namespace mu
{
// Scrollable help written for students in the MU studio.
class HelpPage : public juce::Component
{
public:
    explicit HelpPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Section { juce::String title; std::unique_ptr<TextBlock> body; };
    class Content : public juce::Component
    {
    public:
        std::vector<Section>* sections = nullptr;
        void paint (juce::Graphics&) override;
    };

    void addSection (const juce::String& title, const juce::String& body);
    void layoutContent();

    MUProcessor& proc;
    juce::Viewport viewport;
    Content content;
    std::vector<Section> sections;
    std::vector<juce::TextButton*> jumpButtons;
    juce::OwnedArray<juce::TextButton> jumps;
};
}
