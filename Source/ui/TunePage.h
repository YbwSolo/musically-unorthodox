#pragma once
#include "Widgets.h"
#include "../dsp/TuneGraph.h"
#include <set>

namespace mu
{
// The hand-editing side of Tune: every captured note as a block that can be moved to another pitch.
class TuneGraphView : public juce::Component
{
public:
    TuneGraphView (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // actions for the toolbar
    void zoom (float factor);
    void showAll();
    void snapSelection();                    // selected notes (or all when none are selected) to the scale
    void resetSelection();
    void splitSelection();
    void joinSelection();
    void setStraighten (float v01);
    void setGlide (float ms);
    bool hasSelection() const { return ! selected.empty(); }
    bool selectionValues (float& straighten01, float& glideMs) const;
    void setFollow (bool on) { follow = on; }
    void clearSelection() { selected.clear(); }
    std::function<void()> onSelectionChanged;

private:
    float xFor (double frame) const { return plot.getX() + (float) ((frame - viewStart) * pxPerFrame); }
    double frameAtX (float x) const { return viewStart + (double) (x - plot.getX()) / pxPerFrame; }
    float yFor (float note) const { return plot.getBottom() - (note - viewLow) / viewSpan * plot.getHeight(); }
    int noteUnder (juce::Point<float> p) const;
    void frameContent();
    void showMenu (int note, int frame);
    juce::String describe (const GraphNote& n) const;

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    juce::Rectangle<float> plot;
    double viewStart = 0.0, pxPerFrame = 1.0;
    float viewLow = 52.0f, viewSpan = 15.0f;
    std::set<int> selected;
    std::vector<GraphNote> shown;            // a copy of the notes, refreshed each frame for drawing
    std::vector<float> dragFrom;             // the pitch each selected note had when the drag began
    bool dragging = false, panning = false, moved = false, follow = true, wasCapturing = false, framed = false;
    juce::Point<float> downAt;
    double panStart = 0.0;
    float panLow = 0.0f, pulse = 0.0f;
    int lastCount = -1;
};

// The Tune tab. Simple: pitch correction that works as the song plays, with the key, the speed and a live view
// of what it is doing. Graph: capture a take, then move the notes by hand.
class TunePage : public juce::Component
{
public:
    TunePage (MUProcessor&, std::function<void (const juce::String&)> status);
    void tick (float dt);
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void drawPitch (juce::Graphics&, juce::Rectangle<float>);
    void drawNow (juce::Graphics&, juce::Rectangle<float>);
    void drawKeys (juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<float> keyBounds (int pitchClass) const;
    void showView();

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    Section section { "Tune" };
    Segmented view;
    PresetStrip presets { "tune.presets", "Tune presets" };

    juce::ComboBox keyBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt;
    Segmented scale;
    juce::TextButton findKey, sendKey;
    Knob speed, hold, vibrato, amount, formant, flex, humanise, slides;

    // graph view
    TuneGraphView graph;
    juce::TextButton capture, correct, resetNotes, clearNotes, splitNote, joinNotes, zoomOut, zoomIn, fit;
    juce::Slider straighten, glide;
    juce::ToggleButton followPlay;

    juce::Rectangle<int> pitchWell, nowArea, keysArea, keyLabel, straightenLabel, glideLabel, graphHint;
    static constexpr int kHistory = 360;                      // six seconds at 60 frames a second
    std::array<float, kHistory> sung {}, tuned {};
    int head = 0, shownMode = -1, shownTarget = -1;
    float frame = 0.0f, centre = 60.0f, sungOff = 0.0f, tunedOff = 0.0f, blink = 0.0f;
    bool updatingSliders = false;
};
}
