#include "Course.h"

// Practice and Tools. These are not lessons to read: each one is something to do, or something to use while
// working on a real beat. They have one step each, so the example is the page.
namespace mu::course
{
using S = Scene;

std::vector<Lesson> practiceLessons()
{
    return {
    { "pr.tempo", "Train your ear", "Guess the tempo", {
        { "What tempo is this?",
          "A plain beat plays at a tempo you are not told. Nod to it, then pick the BPM. Do ten in a row. The aim is to feel roughly what 80, 100, 120 and 140 are without counting, the way you know roughly how warm a room is without a thermometer.", S::Game, 0 } },
      nullptr, kTabNone, nullptr },
    { "pr.chord", "Train your ear", "Major or minor?", {
        { "Bright or dark?",
          "A chord plays. Major sounds bright and settled, minor sounds dark. Only one note is different, and it changes everything. Keep going until you get ten right in a row: after that you will hear it in every song you listen to.", S::Game, 1 } },
      nullptr, kTabNone, nullptr },
    { "pr.interval", "Train your ear", "Hear the distance", {
        { "Two notes at a time",
          "Click one key, then another. You hear both together and the plugin names the distance. Try the ones that matter most: one semitone, which clashes; three and four, the middles of minor and major chords; seven, the fifth; and six, the tritone. Then guess the name before you read it.", S::Keys, 15 } },
      nullptr, kTabNone, nullptr, 1 },
    { "pr.count", "Train your ear", "Count for fifteen seconds", {
        { "Count, multiply by four",
          "A beat plays for fifteen seconds at a tempo you are not told. Count every beat, multiply by four, and compare with the answer. This is the skill for the moment when there is no tap button: a song on the radio, a rapper beatboxing an idea.", S::Pulse, 3 } },
      nullptr, kTabNone, nullptr },

    { "pr.copy", "Train your hands", "Copy the drums", {
        { "Rebuild it by ear",
          "A hidden one-bar drum pattern. Listen to the target, click the steps you think you hear, compare the two by ear, then check. Find the snare first, then the kicks, then the hats. There are five patterns, each a little harder. This is exactly how producers learn a style from records.", S::Game, 2 } },
      nullptr, kTabNone, nullptr },
    { "pr.scale", "Train your hands", "Find the notes of a key", {
        { "Seven notes from a recipe",
          "A minor key is named above the keyboard. Click its seven notes using the recipe: tone, semitone, tone, tone, semitone, tone, tone. Each time you finish, it gives you a harder key. When you can do F minor and G minor without thinking, you can write in the keys most beats are in.", S::Keys, 9 } },
      nullptr, kTabNone, nullptr },
    { "pr.grid", "Train your hands", "Build a beat on an empty grid", {
        { "From nothing",
          "An empty bar. Put a snare on beats two and four, a kick on the one, and hi-hats. Once it works, keep changing it: move a kick off the beat, take a hat out, add an open hat before the snare. Stop when it makes you nod.", S::Grid, 9 } },
      nullptr, kTabNone, nullptr },
    { "pr.sketch", "Train your hands", "Sketch a melody", {
        { "Five notes that cannot clash",
          "Drums and an 808 in C minor, and six rows of notes that all fit. Click notes in, press play, and keep the ideas that stick in your head. Try a rule: three notes in the first half, then answer them in the second half with a different ending.", S::Roll, 14 } },
      nullptr, kTabNone, nullptr },
    };
}

std::vector<Lesson> toolLessons()
{
    return {
    { "tl.songtempo", "Your song", "Tempo of your song", {
        { "What BPM is this song?",
          "Put this plugin on the track that holds the song or the beat, and press play in the DAW. The plugin listens and works out the tempo. Tap along on the pad to check it with your own ears. The third box shows what your project is set to, and what to change so the metronome and the grid line up with the song.", S::Pulse, 6 } },
      nullptr, kTabNone, nullptr },
    { "tl.songkey", "Your song", "Key of your song", {
        { "What key is this in?",
          "Play the song, the loop or the sample through this track. The plugin adds up which of the twelve notes it hears most and names the key that fits them, with its seven notes and its chords. Use it to tune an 808 to a sample, or to know which notes a melody over this beat can use.", S::Tool, 1 } },
      nullptr, kTabNone, nullptr },

    { "tl.tap", "Time", "Tap tempo", {
        { "Tap, read the BPM",
          "Click the pad on every beat of anything you can hear: a song in the room, an idea in your head, someone humming. The number is the tempo of your taps. Leave the practice beat switched off to use it as a plain tap tempo.", S::Pulse, 4 } },
      nullptr, kTabNone, nullptr },
    { "tl.metro", "Time", "Metronome", {
        { "A click at any tempo",
          "Drag to a tempo and press play. Use it to feel a tempo before you commit a project to it, or to practise saying a verse in time. The line under the number says which styles live at that speed.", S::Pulse, 1 } },
      nullptr, kTabNone, nullptr },
    { "tl.delay", "Time", "Delay and reverb times", {
        { "Milliseconds for this tempo",
          "Effects that are set in milliseconds only sound in rhythm when the number fits the tempo. Set the tempo, or take it from the project, and read off the time for each note length. Type it into a delay's time, a reverb's pre-delay, or a compressor's release.", S::Tool, 0 } },
      nullptr, kTabNone, nullptr },

    { "tl.scale", "Notes", "Scale finder", {
        { "The notes of any scale",
          "Choose a root and a scale. The keyboard shows its notes, and the line above names them. Press play to hear it. Use it beside your piano roll when you are writing in a key you do not know by heart.", S::Keys, 10 } },
      nullptr, kTabNone, nullptr },
    { "tl.chords", "Notes", "Chord finder", {
        { "The chords of any minor key",
          "Choose the root of your key, then click a number to see and hear that chord. Switch on the seventh for the richer four-note version. Every chord here belongs to the key, so any order of them will work.", S::Keys, 16 } },
      nullptr, kTabNone, nullptr },
    { "tl.circle", "Notes", "Circle of fifths", {
        { "Which keys go together",
          "Click any key. Its notes are listed, the two keys beside it share all but one of them, and the key on the other ring uses exactly the same notes. Use it to pick a second sample that will sit with the first, or a key to move to for a bridge.", S::Circle, 1 } },
      nullptr, kTabNone, nullptr },
    };
}
}
