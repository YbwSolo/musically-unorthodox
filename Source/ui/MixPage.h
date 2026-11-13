#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"

namespace mu
{
class SessionTable : public juce::Component
{
public:
    void update (std::vector<TrackInfo> tracks, const BalancePlan& plan, int ownSlot);
    void paint (juce::Graphics&) override;
    int preferredHeight() const { return 26 + (int) rows.size() * 24 + 8; }
private:
    struct Row { TrackInfo t; float delta = 0.0f; juce::String note; bool own = false; };
    std::vector<Row> rows;
};

class MixPage : public juce::Component
{
public:
    explicit MixPage (MUProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();   // timer

private:
    void rebuildMaskingButtons();

    MUProcessor& proc;
    SessionTable table;
    Readout masterNow, masterPeak, projected, ownLevel;
    juce::Slider targetLufs, targetTp;
    juce::Label targetLufsLabel, targetTpLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetLufsAtt, targetTpAtt;
    juce::TextButton balanceNow { "BALANCE NOW" }, makeMaster { "USE THIS INSTANCE AS MASTER" }, resetTrims { "RESET MY TRIM" };
    juce::ToggleButton autoBalance { "Auto balance (keeps following the mix)" }, acceptMoves { "Let the master move my level" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> acceptAtt;
    juce::Label summary, maskingTitle, hint;
    juce::OwnedArray<juce::TextButton> maskingButtons;
    std::vector<MaskingClash> clashes;
    bool lastWasMaster = false;
    int lastClashCount = -1;
};
}
