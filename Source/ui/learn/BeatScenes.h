#pragma once
#include "../Theme.h"
#include "../../learn/Course.h"
#include "../../learn/LessonSound.h"
#include "../../learn/MusicTheory.h"
#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace mu
{
// The examples for the beat-making lessons: a keyboard, a step grid, a piano roll, a tempo counter and the
// pictures for arrangement, layering, effects and the two DAWs. They draw into the Learn page's stage, and the
// ones that can be heard play through LessonSound, so a student clicks a key or a step and hears it.
class BeatScenes
{
public:
    explicit BeatScenes (LessonSound& s) : sound (s) {}
    static bool handles (course::Scene s) { return (int) s >= (int) course::Scene::Pulse; }

    void enter (course::Scene s, int variant);      // a new step is shown: set the example up, silent
    void leave();                                   // the step is left: stop anything playing
    void tick (float dt);
    void paint (juce::Graphics& g, juce::Rectangle<float> r);
    void mouseDown (juce::Point<float> p);
    void mouseDrag (juce::Point<float> p);
    void mouseMove (juce::Point<float> p);
    juce::String hint() const;                      // the short instruction shown in the corner, or empty
    bool makesSound() const;

    std::function<bool()> audioRunning;             // false when the host is not running the plugin's audio

    // for the tests: the pattern a step would play, and whether the "build a beat" exercise is solved
    const LessonPattern& patternForTest() const { return pattern; }
    bool beatExerciseSolved() const;
    juce::String quizKeyName() const;

private:
    enum HitId { Play, Cell, RowPick, PianoKey, Root, ScalePick, Degree, Toggle, Start, Pad, Tempo, Section, Area, CardRow, Next, RollCell };
    struct Hit { juce::Rectangle<float> r; int id, a, b; };
    struct RollNote { int step, note, len, voice, slideTo; };      // voice: 0 melody, 1 and 2 harmony, 3 the 808

    void clicked (const Hit& h, juce::Point<float> p);
    void add (juce::Rectangle<float> r, int id, int a = 0, int b = 0) { hits.push_back ({ r, id, a, b }); }
    void button (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, bool on, int id, int a = 0, int b = 0);
    void transport (juce::Graphics& g, juce::Rectangle<float>& r, const juce::String& caption);
    void send();                                    // hand the pattern to the sound engine
    void togglePlay();

    void setupPulse(); void setupGrid(); void setupKeys(); void setupRoll();
    void paintPulse (juce::Graphics&, juce::Rectangle<float>);
    void paintGrid (juce::Graphics&, juce::Rectangle<float>);
    void paintKeys (juce::Graphics&, juce::Rectangle<float>);
    void paintRoll (juce::Graphics&, juce::Rectangle<float>);
    void paintSong (juce::Graphics&, juce::Rectangle<float>);
    void paintLayers (juce::Graphics&, juce::Rectangle<float>);
    void paintFx (juce::Graphics&, juce::Rectangle<float>);
    void paintDaw (juce::Graphics&, juce::Rectangle<float>);
    void paintCard (juce::Graphics&, juce::Rectangle<float>);

    void rollToPattern();
    void playProgression (int which);
    float keyX (int midiNote, juce::Rectangle<float> keys, bool& black) const;
    void playScale();
    void playChord (const std::array<int, 3>& notes, float seconds = 1.2f);
    void newQuizKey();
    void startHidden (bool counting);
    theory::Scale keysScale() const;
    juce::String gridCaption() const;

    LessonSound& sound;
    course::Scene scene = course::Scene::Pulse;
    int variant = 0;
    float t = 0.0f;
    std::vector<Hit> hits;
    LessonPattern pattern;

    // pulse
    float tempo = 90.0f, countElapsed = -1.0f;
    int hiddenBpm = 0, hiddenIndex = 0, countState = 0;          // 0 waiting, 1 running, 2 answer shown
    bool halfTime = false;
    std::vector<double> taps;
    juce::Rectangle<float> tempoTrack;

    // grid
    int gridRows = 4, lengthRow = 0;
    bool gridHas808 = false, gridFixed = false;

    // keys
    int keyRoot = 0, degree = 0, pressedKey = -1, lastKey = -1, prevKey = -1, quizWrong = 0, quizKey = 0;
    float pressedFor = 0.0f, wrongFor = 0.0f;
    int wrongKey = -1;
    theory::Scale scalePick = theory::Scale::Minor;
    bool chordMajor = true, relativeMinor = false;
    std::array<bool, 12> quizFound {};

    // roll
    std::vector<RollNote> rollNotes;
    std::vector<std::pair<int, juce::String>> rollChords;          // the chord sounding from each step, written above the roll
    std::array<std::array<bool, 16>, 6> sketch {};
    int rollSteps = 16, rollPerBeat = 4, clashStep = -1;
    float rollBpm = 140.0f;
    bool rollDrums = false;

    // pictures
    int picked = 0, hover = -1;
};
}
