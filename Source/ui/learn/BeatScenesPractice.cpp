#include "BeatScenes.h"

// The parts of the course that are not lessons to read: the page that works out the tempo of the student's own
// song, the practice games, the tools, the circle of fifths, and the guided builds where a beat is put together
// one layer at a time.
namespace mu
{
using namespace theme;
using course::Scene;
using theory::Scale;

namespace
{
    juce::String bpmText (float bpm) { return std::abs (bpm - std::round (bpm)) < 0.15f ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 1); }
    void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r) { g.setFont (bodyBold (10.5f)); g.setColour (textMute); g.drawText (s, r, juce::Justification::centredLeft); }

    // ---- copy the drums: one bar each, easiest first. Steps for hat, snare, kick, and the tempo.
    struct Drums { std::vector<int> hat, snare, kick; float bpm; const char* name; };
    const std::vector<Drums> kCopy = {
        { { 0, 2, 4, 6, 8, 10, 12, 14 }, { 4, 12 }, { 0, 8 }, 90.0f, "the plain beat" },
        { { 0, 2, 4, 6, 8, 10, 12, 14 }, { 4, 12 }, { 0, 7, 10 }, 90.0f, "boom bap" },
        { { 0, 2, 4, 6, 8, 10, 12, 14 }, { 8 }, { 0, 6, 10 }, 140.0f, "half-time trap" },
        { { 0, 3, 6, 8, 11, 14 }, { 8 }, { 0, 10 }, 142.0f, "drill" },
        { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, { 4, 12 }, { 0, 3, 8, 11 }, 96.0f, "sixteenth-note hats" } };
    const int kGuessTempos[] = { 70, 80, 90, 100, 110, 120, 130, 140, 150, 160 };

    // ---- guided builds: two bars, written as { step, note, length, slide target }
    using Notes = std::vector<std::array<int, 4>>;
    struct Style
    {
        const char* name; const char* key; float bpm, swing;
        Notes melody, chordLow, chordHigh, counter, bass;
        std::vector<int> kick, snare, hat, roll, openHat;
    };
    const std::vector<Style> kStyles = {
        { "Trap", "C minor", 140.0f, 0.0f,
          { { 0, 79, 3, 0 }, { 3, 75, 3, 0 }, { 6, 77, 2, 0 }, { 8, 75, 6, 0 }, { 16, 79, 3, 0 }, { 19, 75, 3, 0 }, { 22, 74, 2, 0 }, { 24, 72, 8, 0 } },
          { { 0, 63, 16, 0 }, { 16, 60, 16, 0 } }, { { 0, 67, 16, 0 }, { 16, 63, 16, 0 } },
          { { 12, 67, 2, 0 }, { 14, 70, 2, 0 }, { 30, 67, 2, 0 } },
          { { 0, 36, 5, 0 }, { 6, 36, 9, 0 }, { 16, 44, 3, 0 }, { 19, 44, 6, 0 }, { 26, 46, 5, 0 } },
          { 0, 6, 16, 19, 26 }, { 8, 24 }, { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 }, { 13, 29, 31 }, { 22 } },
        { "Drill", "F minor", 142.0f, 0.0f,
          { { 0, 72, 3, 0 }, { 3, 73, 3, 0 }, { 6, 72, 2, 0 }, { 8, 68, 4, 0 }, { 12, 65, 4, 0 }, { 16, 72, 3, 0 }, { 19, 73, 3, 0 }, { 22, 75, 2, 0 }, { 24, 72, 8, 0 } },
          { { 0, 56, 16, 0 }, { 16, 56, 16, 0 } }, { { 0, 60, 16, 0 }, { 16, 61, 16, 0 } },
          { { 14, 77, 2, 0 }, { 28, 80, 2, 0 }, { 30, 79, 2, 0 } },
          { { 0, 41, 6, 0 }, { 10, 41, 5, 53 }, { 16, 37, 6, 0 }, { 23, 37, 3, 0 }, { 26, 39, 5, 41 } },
          { 0, 10, 16, 23, 26 }, { 8, 28 }, { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 }, {}, {} },
        { "R&B", "A minor", 72.0f, 0.2f,
          { { 0, 76, 4, 0 }, { 4, 74, 2, 0 }, { 6, 72, 6, 0 }, { 12, 69, 4, 0 }, { 16, 72, 4, 0 }, { 20, 74, 2, 0 }, { 22, 76, 6, 0 }, { 28, 79, 4, 0 } },
          { { 0, 60, 16, 0 }, { 16, 57, 16, 0 } }, { { 0, 64, 16, 0 }, { 16, 60, 16, 0 } },
          { { 14, 81, 2, 0 }, { 30, 76, 2, 0 } },
          { { 0, 33, 8, 0 }, { 10, 33, 5, 0 }, { 16, 41, 8, 0 }, { 26, 43, 5, 0 } },
          { 0, 10, 16, 22, 26 }, { 4, 12, 20, 28 }, { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30 }, {}, { 14, 30 } } };
    const char* kStageSays[] = { "The melody alone. A short idea, then the same idea with a different ending.",
                                 "Chords under it: one per bar, quieter than the melody.",
                                 "Kick and snare. The snare marks time, the kick starts each bar.",
                                 "Hi-hats on top set the speed and the energy.",
                                 "The 808, starting with the kicks, on the root note of each chord.",
                                 "A counter melody in the gaps. This is the full loop." };
}

// ---- the tempo of the student's own song ------------------------------------------------------------
void BeatScenes::paintSongTempo (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto read = songTempo ? songTempo() : TempoReading();
    const double project = projectTempo ? projectTempo() : 0.0;
    const double mine = theory::bpmFromTaps (taps);
    auto top = r.removeFromTop (24.0f).withTrimmedRight (130.0f);
    g.setFont (body (12.5f)); g.setColour (read.heard ? textDim : yellow);
    g.drawText (! read.heard ? "Press play in your DAW so the song plays through this track. The plugin listens, and you tap along."
                : read.bpm <= 0.0f ? "Listening. It needs about eight seconds of the song: " + juce::String (juce::jmax (0, 8 - (int) read.seconds)) + " to go."
                : "Listening to the last " + juce::String (juce::jmin (20, (int) read.seconds)) + " seconds. Let a section with drums play.", top, juce::Justification::centredLeft);
    r.removeFromTop (12.0f);
    const float colW = (r.getWidth() - 2.0f * 18.0f) / 3.0f;
    auto heard = r.removeFromLeft (colW); r.removeFromLeft (18.0f);
    auto tapped = r.removeFromLeft (colW); r.removeFromLeft (18.0f);
    auto daw = r;
    for (auto box : { heard, tapped, daw }) { g.setColour (raised); g.fillRoundedRectangle (box, 8.0f); }
    heard = heard.reduced (14.0f, 12.0f); tapped = tapped.reduced (14.0f, 12.0f); daw = daw.reduced (14.0f, 12.0f);

    label (g, "THE PLUGIN HEARS", heard.removeFromTop (16.0f));
    g.setFont (headline (58.0f)); g.setColour (read.bpm > 0.0f ? yellow : textMute);
    g.drawText (read.bpm > 0.0f ? bpmText (read.bpm) : juce::String ("--"), heard.removeFromTop (64.0f), juce::Justification::centredLeft);
    juce::String says;
    if (read.bpm > 0.0f)
    {
        says = "BPM";
        if (read.other > 0.0f) says << ", or " << bpmText (read.other) << " counted " << (read.other > read.bpm ? "double" : "at half") << ". Both are right: for trap and drill the project is set to the faster one.";
        else says << ".";
        if (read.rival > 0.0f) says << " Not certain: " << bpmText (read.rival) << " fits nearly as well. Tap along to settle it.";
        else if (read.confidence < 0.35f) says << " Not certain yet: let more of the song play.";
    }
    else says = "The tempo appears here once it has heard enough.";
    drawWrapped (g, says, heard, body (12.5f), text.withAlpha (0.88f), 1.35f);

    label (g, "YOUR TAPS", tapped.removeFromTop (16.0f));
    auto padRow = tapped.removeFromTop (juce::jmin (96.0f, tapped.getHeight() * 0.5f));
    auto pad = padRow.removeFromLeft (padRow.getHeight()).reduced (2.0f);
    const bool flash = pressedFor > 0.0f;
    g.setColour (flash ? yellow : control); g.fillRoundedRectangle (pad, 10.0f);
    g.setColour (yellow); g.drawRoundedRectangle (pad.reduced (1.0f), 10.0f, 2.0f);
    g.setFont (headline (24.0f)); g.setColour (flash ? onYellow : text);
    g.drawText ("TAP", pad, juce::Justification::centred);
    add (pad, Pad);
    g.setFont (headline (40.0f)); g.setColour (mine > 0.0 ? text : textMute);
    g.drawText (mine > 0.0 ? juce::String (juce::roundToInt (mine)) : juce::String ("--"), padRow.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
    tapped.removeFromTop (8.0f);
    juce::String verdict = "Click the pad on every beat of the song. Follow the snare, not the hi-hats.";
    if (mine > 0.0 && taps.size() >= 6 && read.bpm > 0.0f)
    {
        const auto near = [&] (float target) { return std::abs ((float) mine - target) <= juce::jmax (2.0f, target * 0.025f); };
        if (near (read.bpm) || (read.other > 0.0f && near (read.other))) verdict = "You and the plugin agree: " + bpmText (near (read.bpm) ? read.bpm : read.other) + " BPM.";
        else if (read.rival > 0.0f && near (read.rival)) verdict = "Your taps say " + bpmText (read.rival) + ". When the plugin is unsure, a steady tap is the better witness.";
        else verdict = "Your taps and the plugin disagree. Tap eight more beats on the snare, and let the plugin hear a part with drums.";
    }
    else if (mine > 0.0 && taps.size() < 6) verdict = "Keep tapping: a few more beats steady the number.";
    drawWrapped (g, verdict, tapped, body (12.5f), text.withAlpha (0.88f), 1.35f);

    label (g, "YOUR PROJECT", daw.removeFromTop (16.0f));
    g.setFont (headline (40.0f)); g.setColour (project > 20.0 ? text : textMute);
    g.drawText (project > 20.0 ? bpmText ((float) project) : juce::String ("--"), daw.removeFromTop (50.0f), juce::Justification::centredLeft);
    juce::String advice = "The DAW has not told the plugin its tempo yet. It does once the song is playing.";
    if (project > 20.0)
    {
        if (read.bpm <= 0.0f) advice = "This is the tempo the DAW is set to. It is not the song's tempo unless you make it so.";
        else if (std::abs ((float) project - read.bpm) < 0.6f || (read.other > 0.0f && std::abs ((float) project - read.other) < 0.6f)) advice = "The project matches the song. Switch the DAW's metronome on: it should stay locked to the song from the first bar to the last.";
        else advice = "The project is not at the song's tempo. Set it to " + bpmText (read.bpm) + (read.other > 0.0f ? " or " + bpmText (read.other) : juce::String())
                    + ", line the first beat of the song up with bar one, and the metronome and the grid will follow the song.";
    }
    drawWrapped (g, advice, daw, body (12.5f), text.withAlpha (0.88f), 1.35f);
}

// ---- practice games ---------------------------------------------------------------------------------
void BeatScenes::setupGame() { round = 0; score = 0; asked = 0; chosen = -1; answer = -1; checked = false; hearingTarget = false; for (auto& m : mine) m = {}; if (variant == 2) newRound(); }

void BeatScenes::newRound()
{
    chosen = -1; checked = false;
    if (variant == 0)
    {
        // four tempos to choose from, none of them half or double another: the plain beat leaves no doubt which is meant
        std::vector<int> pool (std::begin (kGuessTempos), std::end (kGuessTempos));
        const int right = pool[(size_t) random.nextInt ((int) pool.size())];
        std::vector<int> picks { right };
        while (picks.size() < 4)
        {
            const int c = pool[(size_t) random.nextInt ((int) pool.size())];
            bool ok = true;
            for (int p : picks) if (p == c || p == c * 2 || c == p * 2 || std::abs (p - c) < 20) ok = false;
            if (ok) picks.push_back (c);
        }
        std::sort (picks.begin(), picks.end());
        for (int i = 0; i < 4; ++i) { choices[(size_t) i] = picks[(size_t) i]; if (picks[(size_t) i] == right) answer = i; }
        pattern = {}; pattern.bpm = (float) right;
        pattern.kick[0] = pattern.kick[8] = 100; pattern.snare[4] = pattern.snare[12] = 100;
        for (int s = 0; s < 16; s += 2) pattern.hat[(size_t) s] = 80;
        send(); sound.start();
    }
    else if (variant == 1)
    {
        answer = random.nextInt (2);                                   // 0 major, 1 minor
        choices[0] = 50 + random.nextInt (10);                         // the root
        sound.stop();
        playChord ({ choices[0], choices[0] + (answer == 0 ? 4 : 3), choices[0] + 7 }, 1.6f);
    }
    else
    {
        const auto& d = kCopy[(size_t) (round % (int) kCopy.size())];
        for (auto& row : target) row = {};
        for (int s : d.hat) target[0][(size_t) s] = true;
        for (int s : d.snare) target[1][(size_t) s] = true;
        for (int s : d.kick) target[2][(size_t) s] = true;
        for (auto& m : mine) m = {};
        sound.stop();
    }
    ++asked;
}

bool BeatScenes::copyExerciseSolved() const { return mine == target; }

void BeatScenes::clickedPractice (const Hit& h)
{
    auto drums = [this] (const std::array<std::array<bool, 16>, 3>& rows)
    {
        pattern = {}; pattern.bpm = kCopy[(size_t) (round % (int) kCopy.size())].bpm;
        for (int s = 0; s < 16; ++s) { if (rows[0][(size_t) s]) pattern.hat[(size_t) s] = 80; if (rows[1][(size_t) s]) pattern.snare[(size_t) s] = 100; if (rows[2][(size_t) s]) pattern.kick[(size_t) s] = 100; }
        send();
    };
    switch (h.id)
    {
        case Start:
            if (scene == Scene::Game && variant == 1 && answer >= 0 && chosen < 0) { playChord ({ choices[0], choices[0] + (answer == 0 ? 4 : 3), choices[0] + 7 }, 1.6f); break; }
            if (scene == Scene::Game && variant == 0 && answer >= 0 && chosen < 0) { if (sound.isPlaying()) sound.stop(); else { send(); sound.start(); } break; }
            newRound(); break;
        case Next: if (variant == 2) ++round; newRound(); break;
        case Choice:
            if (chosen >= 0 || answer < 0) break;
            chosen = h.a;
            if (chosen == answer) { ++score; ++streak; } else streak = 0;
            if (variant == 0) sound.stop();
            break;
        case Mine:
            mine[(size_t) h.a][(size_t) h.b] = ! mine[(size_t) h.a][(size_t) h.b];
            checked = false;
            if (sound.isPlaying() && ! hearingTarget) drums (mine);
            else if (mine[(size_t) h.a][(size_t) h.b] && ! sound.isPlaying()) sound.hit (h.a == 0 ? 2 : h.a == 1 ? 1 : 0, h.a == 0 ? 0.63f : 0.8f);
            break;
        case Hear:
            if (sound.isPlaying() && hearingTarget == (h.a == 0)) { sound.stop(); break; }
            hearingTarget = h.a == 0;
            drums (hearingTarget ? target : mine);
            sound.stop(); sound.start();
            break;
        case Check: checked = true; if (copyExerciseSolved()) ++score; break;
        case UseProject: if (projectTempo && projectTempo() > 20.0) toolBpm = (float) juce::roundToInt (projectTempo()); break;
        case CircleKey:
        {
            circleKey = h.a; circleMinor = h.b == 1;
            const int root = 48 + (circleMinor ? (circleKey * 7 + 9) % 12 : (circleKey * 7) % 12);
            playChord ({ root, root + (circleMinor ? 3 : 4), root + 7 }, 1.2f);
            break;
        }
        default: break;
    }
}

void BeatScenes::paintGame (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto top = r.removeFromTop (28.0f); r.removeFromTop (12.0f);
    top.removeFromRight (130.0f);
    if (variant <= 1)
    {
        const bool open = answer >= 0 && chosen < 0;
        const bool tempoGame = variant == 0;
        button (g, top.removeFromLeft (150.0f), answer < 0 ? "Start" : ! open ? "Next one" : tempoGame ? (sound.isPlaying() ? "Stop the beat" : "Hear it again") : "Hear it again", ! open, Start);
        top.removeFromLeft (14.0f);
        g.setFont (body (12.5f)); g.setColour (soundNote().isNotEmpty() ? yellow : textDim);
        g.drawText (soundNote().isNotEmpty() ? soundNote() : asked == 0 ? (tempoGame ? "A beat plays. Pick its tempo." : "A chord plays. Is it major or minor?")
                    : "Right so far: " + juce::String (score) + " of " + juce::String (chosen >= 0 ? asked : asked - 1) + (streak >= 3 ? ".  " + juce::String (streak) + " in a row." : juce::String (".")),
                    top, juce::Justification::centredLeft);
        const int n = tempoGame ? 4 : 2;
        auto row = r.removeFromTop (juce::jmin (110.0f, r.getHeight() * 0.5f));
        const float gap = 14.0f, bw = (row.getWidth() - gap * (float) (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const auto box = juce::Rectangle<float> (row.getX() + (float) i * (bw + gap), row.getY(), bw, row.getHeight());
            const bool right = chosen >= 0 && i == answer, wrong = chosen == i && i != answer;
            g.setColour (right ? yellow : raised); g.fillRoundedRectangle (box, 10.0f);
            g.setColour (wrong ? over : (answer >= 0 && chosen < 0 ? yellow.withAlpha (0.6f) : lineHi)); g.drawRoundedRectangle (box.reduced (1.0f), 10.0f, wrong ? 3.0f : 1.5f);
            g.setFont (headline (tempoGame ? 36.0f : 32.0f)); g.setColour (right ? onYellow : (answer < 0 ? textMute : text));
            g.drawText (tempoGame ? (answer < 0 ? juce::String ("?") : juce::String (choices[(size_t) i])) : juce::String (i == 0 ? "MAJOR" : "MINOR"), box.withTrimmedBottom (tempoGame ? 18.0f : 0.0f), juce::Justification::centred);
            if (tempoGame) { g.setFont (body (11.5f)); g.setColour (right ? onYellow : textMute); g.drawText ("BPM", box.withTop (box.getBottom() - 34.0f), juce::Justification::centredTop); }
            if (answer >= 0 && chosen < 0) add (box, Choice, i);
        }
        r.removeFromTop (16.0f);
        juce::String says;
        if (chosen >= 0 && tempoGame)
            says = chosen == answer ? "Right: " + juce::String (choices[(size_t) answer]) + " BPM. " + (choices[(size_t) answer] <= 80 ? "Slow and heavy." : choices[(size_t) answer] <= 100 ? "Head-nod tempo: boom bap lives here." : choices[(size_t) answer] <= 125 ? "Walking pace into dance tempo." : "Fast: where trap and drill projects are set.")
                                    : "It was " + juce::String (choices[(size_t) answer]) + ". A way in: one beat a second is 60, two a second is 120. Tap your foot and compare it with a clock.";
        else if (chosen >= 0)
            says = juce::String (chosen == answer ? "Right. " : "Not this time. ") + "It was " + theory::noteName (choices[0]) + (answer == 0 ? " major: the middle note is four semitones above the root, and it sounds bright and settled."
                                                                                                                     : " minor: the middle note is three semitones above the root, and it sounds dark.");
        else if (answer >= 0) says = tempoGame ? "Nod to it first. Is it slower or faster than one beat a second? Then choose." : "Listen to the mood, not the pitch. Bright and settled, or dark?";
        else says = tempoGame ? "Ear training for tempo. After twenty of these you will know roughly what 90, 120 and 140 feel like without counting." : "Ear training for chords. The same question, again and again, until the answer is instant.";
        drawWrapped (g, says, r.withWidth (juce::jmin (r.getWidth(), 640.0f)), body (14.0f), text.withAlpha (0.9f), 1.4f);
        return;
    }

    // ---- copy the drums
    const bool solved = checked && copyExerciseSolved();
    const bool playingTarget = sound.isPlaying() && hearingTarget, playingMine = sound.isPlaying() && ! hearingTarget;
    button (g, top.removeFromLeft (128.0f), playingTarget ? "Stop" : "Hear the target", playingTarget, Hear, 0); top.removeFromLeft (6.0f);
    button (g, top.removeFromLeft (104.0f), playingMine ? "Stop" : "Hear mine", playingMine, Hear, 1); top.removeFromLeft (6.0f);
    button (g, top.removeFromLeft (76.0f), "Check", false, Check); top.removeFromLeft (6.0f);
    if (solved) { button (g, top.removeFromLeft (112.0f), "Next pattern", true, Next); top.removeFromLeft (6.0f); }
    top.removeFromLeft (8.0f);
    // after a check: how many of each drum's hits were found, and how many of the student's are not in the target. Only the
    // student's own wrong steps are marked, so the answer is never shown.
    const char* drumNames[] = { "hats", "snares", "kicks" };
    juce::StringArray found;
    for (int row = 0; row < 3; ++row)
    {
        int right = 0, wanted = 0, extra = 0;
        for (int s = 0; s < 16; ++s) { if (target[(size_t) row][(size_t) s]) { ++wanted; if (mine[(size_t) row][(size_t) s]) ++right; } else if (mine[(size_t) row][(size_t) s]) ++extra; }
        found.add (juce::String (right) + " of " + juce::String (wanted) + " " + drumNames[row] + (extra > 0 ? " (" + juce::String (extra) + " wrong)" : juce::String()));
    }
    g.setFont (body (12.5f)); g.setColour (solved ? yellow : textDim);
    g.drawText (solved ? juce::String ("Exact. That was ") + kCopy[(size_t) (round % (int) kCopy.size())].name + "."
                : checked ? "Found " + found.joinIntoString (", ") + "."
                : "Pattern " + juce::String (round % (int) kCopy.size() + 1) + " of " + juce::String ((int) kCopy.size()) + ". Listen, rebuild it by ear, then check.", top, juce::Justification::centredLeft);
    const char* names[] = { "Hi-hat", "Snare", "Kick" };
    const float labelW = 70.0f, cw = (r.getWidth() - labelW) / 16.0f, rh = juce::jmin (58.0f, (r.getHeight() - 22.0f) / 3.0f);
    const int step = sound.currentStep();
    for (int s = 0; s < 16; ++s)
    {
        g.setFont (body (11.5f)); g.setColour (s == step ? yellow : (s % 4 == 0 ? text : textMute));
        g.drawText (s % 4 == 0 ? juce::String (s / 4 + 1) : s % 4 == 2 ? juce::String ("&") : juce::String(), juce::Rectangle<float> (r.getX() + labelW + (float) s * cw, r.getY(), cw, 18.0f), juce::Justification::centred);
    }
    for (int row = 0; row < 3; ++row)
    {
        const float y = r.getY() + 22.0f + (float) row * rh;
        g.setFont (body (12.0f)); g.setColour (text);
        g.drawText (names[row], juce::Rectangle<float> (r.getX(), y, labelW, rh), juce::Justification::centredLeft);
        for (int s = 0; s < 16; ++s)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + labelW + (float) s * cw + 1.5f, y + 3.0f, cw - 3.0f, rh - 6.0f);
            const bool on = mine[(size_t) row][(size_t) s], now = s == step;
            g.setColour (on ? (now ? text : yellow) : (now ? control.brighter (0.25f) : ((s / 4) % 2 == 0 ? control : raised)));
            g.fillRoundedRectangle (cell, 3.0f);
            if (checked && on && ! target[(size_t) row][(size_t) s]) { g.setColour (over); g.drawRoundedRectangle (cell.reduced (1.0f), 3.0f, 2.0f); }
            add (cell.expanded (1.5f, 3.0f), Mine, row, s);
        }
    }
}

// ---- tools --------------------------------------------------------------------------------------------
void BeatScenes::paintTool (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (variant == 0)
    {
        auto top = r.removeFromTop (58.0f);
        g.setFont (headline (46.0f)); g.setColour (yellow);
        g.drawText (juce::String (juce::roundToInt (toolBpm)), top.removeFromLeft (104.0f), juce::Justification::centredLeft);
        g.setFont (body (12.0f)); g.setColour (textMute);
        g.drawText ("BPM", top.removeFromLeft (40.0f).withTrimmedTop (22.0f), juce::Justification::centredLeft);
        top.removeFromRight (130.0f);
        const double project = projectTempo ? projectTempo() : 0.0;
        if (project > 20.0) { button (g, top.removeFromRight (190.0f).withSizeKeepingCentre (190.0f, 28.0f), "Use the project's tempo: " + bpmText ((float) project), false, UseProject); top.removeFromRight (14.0f); }
        tempoTrack = top.reduced (10.0f, 24.0f);
        g.setColour (control); g.fillRoundedRectangle (tempoTrack, 3.0f);
        const float x = tempoTrack.getX() + tempoTrack.getWidth() * (toolBpm - 60.0f) / 120.0f;
        g.setColour (yellow); g.fillRoundedRectangle (tempoTrack.withRight (x), 3.0f); g.fillEllipse (x - 9.0f, tempoTrack.getCentreY() - 9.0f, 18.0f, 18.0f);
        r.removeFromTop (8.0f);
        struct Row { const char* name; double bars; const char* use; };
        const Row rows[] = { { "One bar", 1.0, "a long reverb tail should be gone by here" }, { "Half note", 0.5, "a slow, wide echo" }, { "Quarter note", 0.25, "the classic vocal echo" },
                             { "Dotted eighth", 0.1875, "the bouncing echo that falls between the beats" }, { "Eighth note", 0.125, "a tight echo; a short reverb tail" },
                             { "Eighth triplet", 1.0 / 12.0, "a rolling echo" }, { "Sixteenth note", 0.0625, "slapback and doubling" }, { "Thirty-second", 0.03125, "reverb pre-delay: keeps the dry sound clear" } };
        const float rh = juce::jmin (27.0f, r.getHeight() / 8.0f);
        for (int i = 0; i < 8; ++i)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + (float) i * rh, r.getWidth(), rh - 2.0f);
            g.setColour (i % 2 == 0 ? raised : black); g.fillRoundedRectangle (row, 3.0f);
            row = row.reduced (10.0f, 0.0f);
            g.setFont (bodyBold (12.5f)); g.setColour (text); g.drawText (rows[i].name, row.removeFromLeft (130.0f), juce::Justification::centredLeft);
            const double ms = theory::msForNote ((double) toolBpm, rows[i].bars);
            g.setFont (headline (18.0f)); g.setColour (yellow); g.drawText (ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms", row.removeFromLeft (96.0f), juce::Justification::centredLeft);
            g.setFont (body (12.0f)); g.setColour (textDim); g.drawText (rows[i].use, row, juce::Justification::centredLeft);
        }
        return;
    }
    // ---- the key of the song playing through the track
    int key = 0; bool minor = false; float confidence = 0.0f, seconds = 0.0f;
    const bool known = songKey && songKey (key, minor, confidence, seconds) && seconds >= 6.0f && confidence > 0.05f;
    auto top = r.removeFromTop (24.0f).withTrimmedRight (130.0f);
    g.setFont (body (12.5f)); g.setColour (known ? textDim : yellow);
    g.drawText (known ? "Heard " + juce::String ((int) seconds) + " seconds of music on this track. Let a part with the melody or chords play: drums alone have no key."
                      : "Press play in your DAW so the song or the loop plays through this track. It needs a few seconds of notes.", top, juce::Justification::centredLeft);
    r.removeFromTop (10.0f);
    auto left = r.removeFromLeft (r.getWidth() * 0.36f); r.removeFromLeft (20.0f);
    g.setColour (raised); g.fillRoundedRectangle (left, 8.0f);
    left = left.reduced (14.0f, 12.0f);
    label (g, "THE KEY IT HEARS", left.removeFromTop (16.0f));
    const Scale scale = minor ? Scale::Minor : Scale::Major;
    const bool flats = theory::keyUsesFlats (key, scale);
    g.setFont (headline (46.0f)); g.setColour (known ? yellow : textMute);
    g.drawText (known ? (theory::noteName (key, flats) + (minor ? " MINOR" : " MAJOR")) : juce::String ("--"), left.removeFromTop (56.0f), juce::Justification::centredLeft);
    drawWrapped (g, ! known ? juce::String ("The key appears here once it has heard enough notes.")
                    : juce::String (confidence > 0.6f ? "A clear fit. " : confidence > 0.3f ? "A fair fit. " : "A weak fit: treat it as a first guess. ")
                      + "The same notes also make " + theory::noteName (minor ? key + 3 : key + 9, theory::keyUsesFlats (minor ? key + 3 : key + 9, minor ? Scale::Major : Scale::Minor)) + (minor ? " major" : " minor")
                      + ", so check which note feels like home: hum the note the loop could end on.", left, body (12.5f), text.withAlpha (0.88f), 1.35f);
    if (! known) return;
    label (g, "ITS SEVEN NOTES", r.removeFromTop (16.0f));
    g.setFont (headline (24.0f)); g.setColour (text);
    g.drawText (theory::scaleNotes (key, scale), r.removeFromTop (34.0f), juce::Justification::centredLeft);
    r.removeFromTop (8.0f);
    label (g, "ITS CHORDS", r.removeFromTop (16.0f));
    const auto chords = theory::chordsOfKey (key, scale);
    const float rh = juce::jmin (24.0f, (r.getHeight() - 26.0f) / 7.0f);
    for (int i = 0; i < (int) chords.size(); ++i)
    {
        auto row = juce::Rectangle<float> (r.getX(), r.getY() + (float) i * rh, r.getWidth(), rh);
        g.setFont (bodyBold (12.5f)); g.setColour (yellow); g.drawText (chords[(size_t) i].numeral, row.removeFromLeft (42.0f), juce::Justification::centredLeft);
        g.setFont (body (12.5f)); g.setColour (text); g.drawText (chords[(size_t) i].name, row.removeFromLeft (150.0f), juce::Justification::centredLeft);
        g.setColour (textDim); g.drawText (chords[(size_t) i].notes, row, juce::Justification::centredLeft);
    }
    g.setFont (body (11.5f)); g.setColour (textMute);
    g.drawText ("Tune the 808 and write every melody with these notes.", r.withTop (r.getBottom() - 18.0f), juce::Justification::centredLeft);
}

// ---- the circle of fifths ---------------------------------------------------------------------------
void BeatScenes::paintCircle (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto ring = r.removeFromLeft (r.getHeight() + 40.0f);
    r.removeFromLeft (16.0f);
    const auto centre = ring.getCentre();
    const float outer = ring.getHeight() * 0.5f - 16.0f, inner = outer * 0.62f;
    g.setColour (line); g.drawEllipse (centre.x - outer, centre.y - outer, outer * 2.0f, outer * 2.0f, 1.0f); g.drawEllipse (centre.x - inner, centre.y - inner, inner * 2.0f, inner * 2.0f, 1.0f);
    for (int ringIndex = 0; ringIndex < 2; ++ringIndex)
        for (int i = 0; i < 12; ++i)
        {
            const bool minor = ringIndex == 1;
            const int pc = minor ? (i * 7 + 9) % 12 : (i * 7) % 12;
            const float angle = juce::MathConstants<float>::twoPi * (float) i / 12.0f - juce::MathConstants<float>::halfPi, radius = minor ? inner : outer;
            const auto box = juce::Rectangle<float> (minor ? 40.0f : 38.0f, 24.0f).withCentre ({ centre.x + radius * std::cos (angle), centre.y + radius * std::sin (angle) });
            const bool chosenKey = i == circleKey && minor == circleMinor, relative = i == circleKey && minor != circleMinor;
            const bool neighbour = minor == circleMinor && (i == (circleKey + 1) % 12 || i == (circleKey + 11) % 12);
            g.setColour (chosenKey ? yellow : black); g.fillRoundedRectangle (box, 6.0f);
            g.setColour (chosenKey || relative ? yellow : neighbour ? text : lineHi); g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, relative || neighbour ? 2.0f : 1.0f);
            g.setFont (chosenKey ? bodyBold (12.5f) : body (12.0f)); g.setColour (chosenKey ? onYellow : (neighbour || relative ? text : textDim));
            g.drawText (theory::noteName (pc, theory::keyUsesFlats (pc, minor ? Scale::Minor : Scale::Major)) + (minor ? "m" : ""), box, juce::Justification::centred);
            add (box.expanded (3.0f), CircleKey, i, minor ? 1 : 0);
        }
    g.setFont (body (10.5f)); g.setColour (textMute);
    g.drawText ("major outside", juce::Rectangle<float> (120.0f, 14.0f).withCentre ({ centre.x, centre.y - 8.0f }), juce::Justification::centred);
    g.drawText ("minor inside", juce::Rectangle<float> (120.0f, 14.0f).withCentre ({ centre.x, centre.y + 8.0f }), juce::Justification::centred);

    const Scale scale = circleMinor ? Scale::Minor : Scale::Major;
    const int pc = circleMinor ? (circleKey * 7 + 9) % 12 : (circleKey * 7) % 12;
    auto name = [] (int root, Scale s) { return theory::noteName (root, theory::keyUsesFlats (root, s)) + " " + theory::scaleName (s); };
    r.removeFromRight (4.0f);
    auto head = r.removeFromTop (34.0f).withTrimmedRight (124.0f);
    g.setFont (headline (26.0f)); g.setColour (yellow);
    g.drawText (name (pc, scale).toUpperCase(), head, juce::Justification::centredLeft);
    const int sharps = circleKey <= 5 ? circleKey : 0, flatCount = circleKey >= 7 ? 12 - circleKey : 0;
    const auto accidentals = circleKey == 0 ? juce::String ("No sharps or flats: only white keys.") : circleKey == 6 ? juce::String ("Six sharps, or written the other way, six flats.")
                           : sharps > 0 ? juce::String (sharps) + (sharps == 1 ? " sharp." : " sharps.") : juce::String (flatCount) + (flatCount == 1 ? " flat." : " flats.");
    const int up = (pc + 7) % 12, down = (pc + 5) % 12, rel = circleMinor ? (pc + 3) % 12 : (pc + 9) % 12;
    juce::String says;
    says << "Notes: " << theory::scaleNotes (pc, scale) << ". " << accidentals << "\n\n"
         << "Next to it on the circle: " << name (up, scale) << " and " << name (down, scale) << ". Each shares six of its seven notes, so a melody or a chord borrowed from them still sounds at home, and a key change to them is smooth.\n\n"
         << "Its relative, on the other ring: " << name (rel, circleMinor ? Scale::Major : Scale::Minor) << ". Exactly the same seven notes, with a different home.";
    drawWrapped (g, says, r, body (13.0f), text.withAlpha (0.9f), 1.38f);
}

// ---- guided builds ------------------------------------------------------------------------------------
void BeatScenes::setupBuild()
{
    const auto& st = kStyles[(size_t) juce::jlimit (0, (int) kStyles.size() - 1, variant / 10)];
    const int stage = juce::jlimit (0, 5, variant % 10);
    LessonPattern p; p.steps = 32; p.bpm = st.bpm; p.swing = st.swing;
    auto put = [] (const Notes& notes, std::array<signed char, LessonPattern::kMaxSteps>& note, std::array<signed char, LessonPattern::kMaxSteps>& len)
    { for (auto& n : notes) { note[(size_t) n[0]] = (signed char) n[1]; len[(size_t) n[0]] = (signed char) n[2]; } };
    put (st.melody, p.lead, p.leadLen);
    if (stage >= 1) { put (st.chordLow, p.lead2, p.lead2Len); put (st.chordHigh, p.lead3, p.lead3Len); }
    if (stage >= 2) { for (int s : st.kick) p.kick[(size_t) s] = 100; for (int s : st.snare) p.snare[(size_t) s] = 100; }
    if (stage >= 3) { for (int s : st.hat) p.hat[(size_t) s] = 78; for (int s : st.roll) p.hat[(size_t) s] = 55; for (int s : st.openHat) p.openHat[(size_t) s] = 62; }
    if (stage >= 4) for (auto& n : st.bass) { p.bass[(size_t) n[0]] = (signed char) n[1]; p.bassLen[(size_t) n[0]] = (signed char) n[2]; p.bassSlide[(size_t) n[0]] = (signed char) n[3]; }
    if (stage >= 5) put (st.counter, p.lead4, p.lead4Len);
    pattern = p;
}

void BeatScenes::paintBuild (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& st = kStyles[(size_t) juce::jlimit (0, (int) kStyles.size() - 1, variant / 10)];
    const int stage = juce::jlimit (0, 5, variant % 10), step = sound.currentStep();
    transport (g, r, juce::String (st.name) + ", " + st.key + ", " + juce::String (juce::roundToInt (st.bpm)) + " BPM.  Layer " + juce::String (stage + 1) + " of 6: " + kStageSays[stage]);
    struct Lane { const char* name; int from; const Notes* notes; const std::vector<int>* hits; const std::vector<int>* more; };
    const Lane lanes[] = { { "Counter", 5, &st.counter, nullptr, nullptr }, { "Melody", 0, &st.melody, nullptr, nullptr }, { "Chords", 1, &st.chordHigh, nullptr, nullptr },
                           { "Hi-hats", 3, nullptr, &st.hat, &st.roll }, { "Snare", 2, nullptr, &st.snare, nullptr }, { "Kick", 2, nullptr, &st.kick, nullptr }, { "808", 4, &st.bass, nullptr, nullptr } };
    const float labelW = 66.0f, x0 = r.getX() + labelW, cw = (r.getWidth() - labelW) / 32.0f;
    const float heights[] = { 1.3f, 1.8f, 1.2f, 0.7f, 0.7f, 0.7f, 1.2f };
    float total = 0.0f; for (float h : heights) total += h;
    float y = r.getY();
    for (int i = 0; i < 7; ++i)
    {
        const auto& lane = lanes[i];
        const float lh = r.getHeight() * heights[i] / total;
        const bool in = stage >= lane.from, added = stage == lane.from;
        const auto area = juce::Rectangle<float> (x0, y + 1.0f, cw * 32.0f, lh - 2.0f);
        g.setColour (in ? raised : black); g.fillRoundedRectangle (area, 3.0f);
        if (! in) { g.setColour (line); g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f); }
        for (int s = 0; s <= 32; s += 4) { g.setColour (s % 16 == 0 ? lineHi : line); g.fillRect (x0 + (float) s * cw - 0.5f, area.getY(), 1.0f, area.getHeight()); }
        g.setFont (added ? bodyBold (12.0f) : body (12.0f)); g.setColour (added ? yellow : (in ? text : textMute));
        g.drawText (lane.name, juce::Rectangle<float> (r.getX(), y, labelW - 6.0f, lh), juce::Justification::centredLeft);
        if (in && lane.notes != nullptr)
        {
            int lo = 127, hi = 0;
            for (auto& n : *lane.notes) { lo = juce::jmin (lo, n[1], n[3] > 0 ? n[3] : 127); hi = juce::jmax (hi, n[1], n[3]); }
            const float nh = juce::jmin (12.0f, area.getHeight() - 4.0f), span = (float) juce::jmax (1, hi - lo);
            for (auto& n : *lane.notes)
            {
                const float ny = area.getBottom() - 2.0f - nh - (area.getHeight() - 4.0f - nh) * (float) (n[1] - lo) / span;
                const auto box = juce::Rectangle<float> (x0 + (float) n[0] * cw + 1.0f, ny, (float) n[2] * cw - 2.0f, nh);
                const bool now = step >= n[0] && step < n[0] + n[2];
                g.setColour (now ? juce::Colours::white : (added ? yellow : yellow.withAlpha (0.6f))); g.fillRoundedRectangle (box, 2.5f);
                if (box.getWidth() > 26.0f && nh >= 11.0f) { g.setFont (body (9.5f)); g.setColour (juce::Colours::black); g.drawText (theory::noteName (n[1], true), box.reduced (3.0f, 0.0f), juce::Justification::centredLeft); }
                if (n[3] > 0)
                {
                    const float ty = area.getBottom() - 2.0f - nh * 0.5f - (area.getHeight() - 4.0f - nh) * (float) (n[3] - lo) / span;
                    g.setColour (text); g.drawLine (box.getX() + box.getWidth() * 0.4f, box.getCentreY(), box.getRight(), ty, 1.8f);
                }
            }
        }
        if (in && lane.hits != nullptr)
        {
            auto dot = [&] (int s, bool soft)
            {
                g.setColour (s == step ? juce::Colours::white : (added ? yellow : yellow.withAlpha (soft ? 0.35f : 0.6f)));
                g.fillRoundedRectangle (x0 + (float) s * cw + 2.0f, area.getY() + 3.0f, juce::jmax (3.0f, cw - 4.0f), area.getHeight() - 6.0f, 2.0f);
            };
            for (int s : *lane.hits) dot (s, false);
            if (lane.more != nullptr) for (int s : *lane.more) dot (s, true);
        }
        y += lh;
    }
    if (step >= 0) { g.setColour (text.withAlpha (0.7f)); g.fillRect (x0 + (float) step * cw - 0.5f, r.getY(), 1.5f, r.getHeight()); }
}
}
