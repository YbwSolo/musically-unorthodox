#pragma once
#include <vector>

// The Learn section's course: tracks (Theory, Make a beat, Logic Pro, FL Studio, Mixing), each a list of lessons,
// each lesson a few short steps shown under an example the student can see, hear and usually play with.
namespace mu::course
{
enum class Scene
{
    Wave, EqShapes, EqMove, FreqMap, Masking, Levels, Envelope, Saturation, Stereo, Loudness, TruePeak, Chain,     // drawn by LearnPage
    Pulse, Grid, Keys, Roll, Song, Layers, Fx, Daw, Card, Game, Tool, Circle, Build                              // drawn by BeatScenes
};
// the plugin's tabs, in the order of HeaderBar::Tab, so a lesson can open the part of the plugin it is about
enum { kTabMix = 0, kTabEQ, kTabDynamics, kTabTone, kTabTune, kTabFX, kTabMaster, kTabNone = -1 };
struct Step { const char* title; const char* text; Scene scene; int variant; };
struct Lesson
{
    const char* id;                 // never changes once released: progress is saved against it
    const char* chapter;
    const char* title;
    std::vector<Step> steps;
    const char* tryLabel;           // a button that opens the matching part of the plugin, or nullptr
    int tab;
    const char* preset;
    int level = 0;                  // 0 for everyone, 1 once the basics are known, 2 advanced
};
struct Track { const char* name; const char* shortName; const char* about; std::vector<Lesson> lessons; };

const std::vector<Track>& tracks();
// the lessons of the old, single list, in their old order: progress saved by earlier builds was a bit per lesson
const char* legacyLessonId (int oldIndex);

// one file per track
std::vector<Lesson> theoryLessons();
std::vector<Lesson> beatLessons();
std::vector<Lesson> logicLessons();
std::vector<Lesson> flLessons();
std::vector<Lesson> mixingLessons();
std::vector<Lesson> practiceLessons();
std::vector<Lesson> toolLessons();
}
