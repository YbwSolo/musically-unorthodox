#pragma once
#include "Widgets.h"
#include "../state/ModulePresets.h"

namespace mu
{
// The FX rack. The tools are listed on the left, each with its own switch; the one that is selected fills the
// right-hand side with its controls, a live picture of what it is doing, and its presets. The Sounds browser at
// the top loads several tools together.
class FXPage : public juce::Component
{
public:
    FXPage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct ModuleUI
    {
        std::vector<juce::Component*> top;        // switches and choices, laid out left to right above the display
        std::vector<int> topWidths;
        std::vector<Knob*> knobs;
        std::vector<Knob*> more;                  // the finer controls, shown when More is on
    };
    void select (int index);
    juce::Rectangle<int> rowBounds (int index) const;
    // The rail lists the tools in the order the sound meets them: the three voice tools that work side by side,
    // then the six in-line tools in the order chosen (drag one to move it).
    int slotOf (int module) const;                 // which row of the rail a tool is on
    int chainTool (int module) const;              // its place in fxorder::Tool, or -1 for the voice tools
    int dragTool = -1, shownOrder = -1;
    // drawing in the display: Motion's curve (sixteen points) and Chop's pattern (sixteen steps)
    juce::Rectangle<float> motionPlot() const, chopPlot() const;
    bool drawAt (juce::Point<int> where, bool firstTouch);
    int drawKind = 0;              // 0 nothing, 1 motion curve, 2 chop pattern, 3 a cell of Harmony's voice table, 4 Harmony's chord grid
    // Harmony: the table of four voices (on, interval, level, pan, delay, size, detune) and the chord grid
    juce::Rectangle<float> harmonyLeft() const, harmonyTable() const, voiceCell (int voice, int column) const, chordCell (int slot, int note) const;
    juce::String voiceParam (int voice, int column) const;
    static juce::String intervalName (int steps);
    int cellVoice = -1, cellColumn = -1, shownHarmonyMode = -1;
    float cellStart = 0.0f;
    bool chordPaint = true;
    int drawLast = -1; float drawLastValue = 0.0f;       // so a fast drag fills in the points it skipped
    bool drawClearing = false;     // chop: a click on a lit step clears it, and a drag from there clears more
    bool dragging = false;
    juce::Point<int> dragFrom;
    juce::Rectangle<int> orderNote;
    void drawDoubles (juce::Graphics&, juce::Rectangle<float>);
    void drawEcho (juce::Graphics&, juce::Rectangle<float>);
    void drawReverb (juce::Graphics&, juce::Rectangle<float>);
    void drawHarmony (juce::Graphics&, juce::Rectangle<float>);
    void drawShift (juce::Graphics&, juce::Rectangle<float>);
    void drawPhone (juce::Graphics&, juce::Rectangle<float>);
    void drawLoFi (juce::Graphics&, juce::Rectangle<float>);
    void drawMotion (juce::Graphics&, juce::Rectangle<float>);
    void drawChop (juce::Graphics&, juce::Rectangle<float>);

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    int selected = 0, hoverRow = -1;
    juce::OwnedArray<ParamToggle> powers;
    PresetStrip sounds { "fx.sounds", "Choose a sound" }, presets { "fx.presets", "Presets" };
    std::vector<ModuleUI> ui;

    // doubler
    Segmented dblVoices; Knob dblMix, dblDetune, dblTiming, dblWidth;
    // delay
    ParamToggle dlSync; Choice dlNote; Segmented dlPattern, dlStyle; Knob dlMix, dlTime, dlFeedback, dlTone, dlWidth, dlDuck, dlDrive, dlWobble, dlLowCut;
    // reverb
    Segmented rvType; ParamToggle rvAuto; Segmented rvLength; ParamToggle rvFreeze; juce::TextButton rvFit; Knob rvMix, rvDecay, rvPre, rvTone, rvDuck, rvLowCut, rvWidth;
    Knob rvSize, rvDiffusion, rvEarly, rvLowDecay, rvModDepth, rvModRate, rvShimmer;
    // harmony
    Segmented hmMode, hmVoicing; ParamToggle hmLeading; Choice hmChordLength; juce::TextButton hmFill, hmClear; Knob hmMix, hmWidth, hmHumanise, hmChoir, hmChordCount;
    // shift
    Segmented psMode; Knob psSemis, psCharacter, psMix, psFine, psDrive, psWidth;
    // phone
    Segmented phMode; Knob phAmount, phTone, phDrive, phNoise;
    // lo-fi
    Segmented lfNoiseType; Knob lfCrush, lfWobble, lfMix, lfDrive, lfWear, lfTone, lfNoise;
    // motion
    Segmented mdType, mdShape; ParamToggle mdSync; Choice mdNote; Knob mdRate, mdDepth, mdMix, mdSmooth;
    // chop
    Segmented chMode, chRate; Knob chDepth, chShape, chGate, chSwing, chLength;

    juce::TextButton moreButton;
    bool showMore = false;
    juce::Rectangle<int> rail, panel, well, railList, monoNote, titleArea;
    float duck = 0.0f, echoDuck = 0.0f, sweep = 0.0f, harmonyCentre = 60.0f;
    std::array<float, 4> voiceCents {}, voiceDelay {};
    std::array<float, 9> glow {};
};
}
