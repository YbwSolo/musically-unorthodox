#pragma once
#include "Widgets.h"

namespace mu
{
// Always-visible output meter: left/right peak with hold, short-term loudness, true peak and gain reduction.
class MeterRail : public juce::Component
{
public:
    explicit MeterRail (MUProcessor&);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    MUProcessor& proc;
    std::array<float, 2> level { -100.0f, -100.0f }, hold { -100.0f, -100.0f };
    std::array<float, 2> holdAge { 0.0f, 0.0f };
    float gr = 0.0f, lufs = -100.0f, tp = -100.0f;
    float clipFlash = 0.0f;
    juce::String shownLufs, shownTp;
};
}
