#pragma once
#include "../Theme.h"
#include "../../learn/Course.h"
#include "../../learn/LessonSound.h"
#include "../../learn/MusicTheory.h"
#include "../../dsp/TempoDetector.h"
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

    // What the examples need from the plugin around them. All optional: without them an example still draws.
    std::function<bool()> silent;                   // true when no sound output could be reached at all
    std::function<TempoReading()> songTempo;        // the tempo of the song playing through the track
    std::function<void (bool)> listenToSong;        // start and stop measuring it
    std::function<double()> projectTempo;           // the DAW's own tempo, or 0 when it has not said
    std::function<bool (int& key, bool& minor, float& confidence, float& seconds)> songKey;     // the key heard in the song, false when unknown

    // for the tests: the pattern a step would play, and whether the "build a beat" exercise is solved
    const LessonPattern& patternForTest() const { return pattern; }
    bool beatExerciseSolved() const;
    bool copyExerciseSolved() const;
    int gameAnswerForTest() const { return answer; }
    juce::Rectangle<float> hitForTest (int id, int a = 0, int b = 0) const { for (auto& h : hits) if (h.id == id && h.a == a && h.b == b) return h.r; return {}; }
    enum TestIds { TestChoice = 16, TestMine = 17, TestCheck = 18, TestHear = 19, TestStart = 8, TestPlay = 0 };
    juce::String quizKeyName() const;

private:
    enum HitId { Play, Cell, RowPick, PianoKey, Root, ScalePick, Degree, Toggle, Start, Pad, Tempo, Section, Area, CardRow, Next, RollCell, Choice, Mine, Check, Hear, UseProject, CircleKey };
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
    void paintSongTempo (juce::Graphics&, juce::Rectangle<float>);
    void paintGame (juce::Graphics&, juce::Rectangle<float>);
    void paintTool (juce::Graphics&, juce::Rectangle<float>);
    void paintCircle (juce::Graphics&, juce::Rectangle<float>);
    void paintBuild (juce::Graphics&, juce::Rectangle<float>);
    void setupGame(); void setupBuild(); void newRound();
    void clickedPractice (const Hit& h);
    juce::String soundNote() const;                 // what to say when an example that should be heard cannot be

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

    // games, tools, the circle of fifths and the guided builds
    int round = 0, score = 0, asked = 0, answer = 0, chosen = -1, streak = 0, lastLoops = 0;
    std::array<int, 4> choices {};
    std::array<std::array<bool, 16>, 3> mine {}, target {};        // hat, snare, kick
    bool checked = false, hearingTarget = false, seventh = false, circleMinor = true;
    float toolBpm = 120.0f;
    int circleKey = 0;
    juce::Random random;
};
}
