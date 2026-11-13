#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mu
{
// Vertical level bar. Values in dB. The owner calls setLevel() from a timer.
class LevelMeter : public juce::Component
{
public:
    LevelMeter (float minDb = -60.0f, float maxDb = 6.0f) : lo (minDb), hi (maxDb) {}
    void setLevel (float db, float peakHoldDb = -200.0f);
    void setLabel (const juce::String& s) { label = s; }
    void setTargetMarker (float db) { target = db; repaint(); }
    void paint (juce::Graphics&) override;

private:
    float lo, hi, value = -100.0f, shown = -100.0f, hold = -200.0f, target = -200.0f;
    juce::String label;
};

// Horizontal gain-reduction bar (0 .. -24 dB, grows leftwards from the right edge).
class GainReductionMeter : public juce::Component
{
public:
    void setReduction (float db);
    void paint (juce::Graphics&) override;
private:
    float shown = 0.0f;
};

// Big numeric readout with caption, optionally coloured against a target.
class Readout : public juce::Component
{
public:
    void set (const juce::String& value, const juce::String& caption, juce::Colour c);
    void paint (juce::Graphics&) override;
private:
    juce::String value, caption;
    juce::Colour colour { 0xffe9eaee };
};
}
