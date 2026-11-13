#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Meters.h"
#include "Widgets.h"

namespace mu
{
class SessionTable : public juce::Component
{
public:
    void update (std::vector<TrackInfo> tracks, const BalancePlan& plan, int ownSlot);
    void paint (juce::Graphics&) override;
    int preferredHeight() const { return 30 + (int) juce::jmax ((size_t) 1, rows.size()) * 26 + 10; }
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
    void refresh();
    void setStatus (const juce::String& s);

private:
    class Content : public juce::Component { public: void paint (juce::Graphics&) override {} };

    void rebuildMaskingButtons();
    void layoutMaster (juce::Rectangle<int> r);
    void layoutTrack (juce::Rectangle<int> r);

    MUProcessor& proc;
    juce::Viewport viewport;
    Content content;

    Readout card1, card2, card3, card4;
    juce::TextButton learnBtn { juce::String ("LEARN & BALANCE  ") + juce::String (juce::CharPointer_UTF8 ("\xE2\x86\x92")) }, balanceBtn { "BALANCE NOW" }, cancelBtn { "CANCEL" };
    juce::TextButton makeMaster { "USE THIS INSTANCE AS MASTER" }, resetTrims { "RESET MY TRIM" };
    juce::ToggleButton autoBalance { "Auto balance (keeps following the mix)" }, acceptMoves { "Let the master move my level" };
    juce::ToggleButton autoTrim { "Auto trim master to target" }, peakGuard { "Peak guard (true-peak ceiling)" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> acceptAtt, autoTrimAtt, peakGuardAtt;
    ProgressStrip progress;
    juce::Label status;

    ModulePanel targetsPanel { "Targets", "where the whole song should land" };
    std::unique_ptr<HSlider> targetLufs, targetTp;
    std::vector<std::unique_ptr<HSlider>> roleSliders;
    juce::TextButton resetTargets { "RESET TARGETS" };

    ModulePanel sessionPanel { "Session", "every instance in this project" };
    SessionTable table;
    ModulePanel maskingPanel { "Masking", "tracks sitting on top of each other" };
    juce::OwnedArray<juce::TextButton> maskingButtons;
    juce::Label maskingEmpty;
    std::vector<MaskingClash> clashes;
    bool lastWasMaster = false;
    juce::String statusText;
    double statusUntil = 0.0;
};
}
