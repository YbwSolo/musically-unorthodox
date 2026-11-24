#include "BeatScenes.h"

// The keyboard and the piano roll, and what happens when anything in a beat-making example is clicked.
namespace mu
{
using namespace theme;
using course::Scene;
using theory::Scale;

namespace
{
    constexpr int kLowKey = 48, kHighKey = 72;                 // C3 to C5: fifteen white keys
    bool isBlack (int note) { const int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    int whiteIndex (int note)                                  // how many white keys lie below this one, from the lowest key
    {
        int n = 0;
        for (int k = kLowKey; k < note; ++k) if (! isBlack (k)) ++n;
        return n;
    }
    const int kQuizRoots[] = { 9, 4, 2, 0, 7, 5 };             // A, E, D, C, G and F minor: none, one, one, three, two and four black keys
    const Scale kExplorerScales[] = { Scale::Minor, Scale::Major, Scale::MinorPentatonic, Scale::HarmonicMinor, Scale::Phrygian, Scale::Dorian, Scale::Blues, Scale::MajorPentatonic };

    // The examples are written in C minor, as { step, note, length in steps, voice, slide target }.
    using N = std::vector<std::array<int, 5>>;
    const N motif        = { { 0, 79, 3, 0, 0 }, { 3, 75, 3, 0, 0 }, { 6, 77, 2, 0, 0 }, { 8, 75, 6, 0, 0 } };
    const N motifTwice   = { { 0, 79, 3, 0, 0 }, { 3, 75, 3, 0, 0 }, { 6, 77, 2, 0, 0 }, { 8, 75, 6, 0, 0 },
                             { 16, 79, 3, 0, 0 }, { 19, 75, 3, 0, 0 }, { 22, 77, 2, 0, 0 }, { 24, 75, 6, 0, 0 } };
    const N motifVaried  = { { 0, 79, 3, 0, 0 }, { 3, 75, 3, 0, 0 }, { 6, 77, 2, 0, 0 }, { 8, 75, 6, 0, 0 },
                             { 16, 79, 3, 0, 0 }, { 19, 75, 3, 0, 0 }, { 22, 74, 2, 0, 0 }, { 24, 72, 8, 0, 0 } };
    const N stepsLeaps   = { { 0, 72, 2, 0, 0 }, { 2, 74, 2, 0, 0 }, { 4, 75, 4, 0, 0 }, { 8, 82, 4, 0, 0 }, { 12, 80, 2, 0, 0 }, { 14, 79, 2, 0, 0 },
                             { 16, 77, 2, 0, 0 }, { 18, 75, 2, 0, 0 }, { 20, 74, 4, 0, 0 }, { 24, 72, 8, 0, 0 } };
    const N twoChords    = { { 0, 63, 16, 1, 0 }, { 0, 67, 16, 2, 0 }, { 0, 36, 14, 3, 0 }, { 16, 60, 16, 1, 0 }, { 16, 63, 16, 2, 0 }, { 16, 44, 14, 3, 0 } };
    const N overChords   = { { 0, 79, 4, 0, 0 }, { 4, 77, 2, 0, 0 }, { 6, 75, 6, 0, 0 }, { 12, 74, 2, 0, 0 }, { 14, 72, 2, 0, 0 },
                             { 16, 75, 4, 0, 0 }, { 20, 74, 2, 0, 0 }, { 22, 72, 6, 0, 0 }, { 28, 70, 2, 0, 0 }, { 30, 72, 2, 0, 0 } };
    const N clashing     = { { 0, 79, 4, 0, 0 }, { 4, 76, 10, 0, 0 }, { 14, 72, 2, 0, 0 },
                             { 16, 75, 4, 0, 0 }, { 20, 74, 2, 0, 0 }, { 22, 72, 6, 0, 0 }, { 28, 70, 2, 0, 0 }, { 30, 72, 2, 0, 0 } };
    const N resolved     = { { 0, 79, 4, 0, 0 }, { 4, 75, 10, 0, 0 }, { 14, 72, 2, 0, 0 },
                             { 16, 75, 4, 0, 0 }, { 20, 74, 2, 0, 0 }, { 22, 72, 6, 0, 0 }, { 28, 70, 2, 0, 0 }, { 30, 72, 2, 0, 0 } };
    const N callResponse = { { 0, 72, 2, 0, 0 }, { 2, 75, 2, 0, 0 }, { 4, 79, 4, 0, 0 }, { 8, 77, 2, 0, 0 }, { 10, 75, 2, 0, 0 },
                             { 16, 70, 2, 0, 0 }, { 18, 74, 2, 0, 0 }, { 20, 77, 4, 0, 0 }, { 24, 75, 2, 0, 0 }, { 26, 72, 4, 0, 0 } };
    const N counterLine  = { { 0, 60, 10, 1, 0 }, { 12, 67, 2, 1, 0 }, { 14, 70, 2, 1, 0 }, { 16, 58, 12, 1, 0 }, { 30, 67, 2, 1, 0 } };
    // one chord a bar over four bars, on an eighth-note grid: i VI III VII, each voiced close to the last
    const N progression  = { { 0, 60, 8, 0, 0 }, { 0, 63, 8, 1, 0 }, { 0, 67, 8, 2, 0 }, { 0, 36, 7, 3, 0 },
                             { 8, 60, 8, 0, 0 }, { 8, 63, 8, 1, 0 }, { 8, 68, 8, 2, 0 }, { 8, 44, 7, 3, 0 },
                             { 16, 58, 8, 0, 0 }, { 16, 63, 8, 1, 0 }, { 16, 67, 8, 2, 0 }, { 16, 39, 7, 3, 0 },
                             { 24, 58, 8, 0, 0 }, { 24, 62, 8, 1, 0 }, { 24, 65, 8, 2, 0 }, { 24, 46, 7, 3, 0 } };
    const N arpeggio     = { { 0, 72, 2, 0, 0 }, { 2, 75, 2, 0, 0 }, { 4, 79, 2, 0, 0 }, { 6, 75, 2, 0, 0 }, { 8, 72, 2, 0, 0 }, { 10, 75, 2, 0, 0 }, { 12, 79, 2, 0, 0 }, { 14, 84, 2, 0, 0 },
                             { 16, 72, 2, 0, 0 }, { 18, 75, 2, 0, 0 }, { 20, 80, 2, 0, 0 }, { 22, 75, 2, 0, 0 }, { 24, 72, 2, 0, 0 }, { 26, 75, 2, 0, 0 }, { 28, 80, 2, 0, 0 }, { 30, 84, 2, 0, 0 },
                             { 0, 36, 14, 3, 0 }, { 16, 44, 14, 3, 0 } };
    const N bassFollows  = { { 0, 63, 16, 1, 0 }, { 0, 67, 16, 2, 0 }, { 16, 60, 16, 1, 0 }, { 16, 63, 16, 2, 0 },
                             { 0, 79, 6, 0, 0 }, { 8, 75, 6, 0, 0 }, { 16, 75, 6, 0, 0 }, { 24, 72, 6, 0, 0 },
                             { 0, 36, 5, 3, 0 }, { 6, 36, 3, 3, 0 }, { 10, 36, 5, 3, 0 }, { 16, 44, 5, 3, 0 }, { 22, 44, 3, 3, 0 }, { 26, 44, 5, 3, 0 } };
    const N slides       = { { 0, 36, 6, 3, 0 }, { 8, 36, 5, 3, 48 }, { 14, 43, 2, 3, 0 }, { 16, 36, 6, 3, 0 }, { 24, 39, 6, 3, 36 } };
    const int kSketchNotes[] = { 84, 82, 79, 77, 75, 72 };      // C minor pentatonic, top row first
}

// ---- clicks ---------------------------------------------------------------------------------------
void BeatScenes::clicked (const Hit& h, juce::Point<float>)
{
    switch (h.id)
    {
        case Play:
            if (scene == Scene::Keys) { playScale(); break; }
            togglePlay(); break;

        case Cell:
        {
            std::array<signed char, LessonPattern::kMaxSteps>* rows[4];
            int sounds[4];
            if (gridHas808) { rows[0] = &pattern.hat; rows[1] = &pattern.snare; rows[2] = &pattern.kick; rows[3] = &pattern.bass; sounds[0] = 2; sounds[1] = 1; sounds[2] = 0; sounds[3] = 5; }
            else            { rows[0] = &pattern.openHat; rows[1] = &pattern.hat; rows[2] = &pattern.snare; rows[3] = &pattern.kick; sounds[0] = 3; sounds[1] = 2; sounds[2] = 1; sounds[3] = 0; }
            auto& cell = (*rows[h.a])[(size_t) h.b];
            const bool on = cell <= 0;
            if (sounds[h.a] == 5) { cell = (signed char) (on ? 36 : 0); pattern.bassLen[(size_t) h.b] = 5; }
            else cell = (signed char) (on ? (sounds[h.a] >= 2 ? 80 : 100) : 0);
            send();
            if (on && ! sound.isPlaying()) { if (sounds[h.a] == 5) sound.playNote (36, 0.5f, 1); else sound.hit (sounds[h.a], sounds[h.a] >= 2 ? 0.63f : 0.8f); }
            break;
        }
        case RowPick:
            lengthRow = h.a;
            pattern.hat = {};
            for (int s = 0; s < 16; s += (h.a == 0 ? 4 : h.a == 1 ? 2 : 1)) pattern.hat[(size_t) s] = 100;
            send();
            if (! sound.isPlaying()) sound.start();
            break;

        case PianoKey:
        {
            const int note = h.a;
            pressedKey = note; pressedFor = 0.25f;
            prevKey = lastKey; lastKey = note;
            sound.playNote (note, 0.9f);
            if (variant == 5)                                    // the home chord underneath, so a note outside the key can be heard to rub
                for (int add : { 0, 3, 7 }) sound.playNote (48 + keyRoot + add, 1.3f);
            if (variant == 9)
            {
                const int pc = note % 12;
                if (theory::inScale (note, kQuizRoots[quizKey], Scale::Minor)) quizFound[(size_t) pc] = true;
                else { wrongKey = note; wrongFor = 0.5f; ++quizWrong; }
            }
            break;
        }
        case Root:      keyRoot = h.a; sound.playNote (60 + keyRoot, 0.7f); break;
        case ScalePick: scalePick = (Scale) h.a; playScale(); break;
        case Degree:
        {
            degree = h.a;
            const auto chords = theory::chordsOfKey (keyRoot, Scale::Minor, 48);
            if (degree < (int) chords.size()) playChord (chords[(size_t) degree].midi);
            break;
        }
        case Toggle:
            if (scene == Scene::Pulse)
            {
                halfTime = h.a == 1;
                pattern.bpm = halfTime ? 70.0f : 140.0f; pattern.stepsPerBeat = halfTime ? 8 : 4;       // the hits fall at the same moments either way
                send();
            }
            else if (variant == 8) { chordMajor = h.a == 0; playChord ({ 60, chordMajor ? 64 : 63, 67 }); }
            else if (variant == 11) { relativeMinor = h.a == 1; keyRoot = relativeMinor ? 9 : 0; playScale(); }
            break;

        case Start:
            if (variant == 3) { if (countState != 1) startHidden (true); }
            else if (sound.isPlaying()) sound.stop();
            else startHidden (false);
            break;

        case Pad:
        {
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            if (! taps.empty() && now - taps.back() > 2.5) taps.clear();          // a long gap means a fresh start
            taps.push_back (now);
            if (taps.size() > 16) taps.erase (taps.begin());
            pressedFor = 0.09f;
            break;
        }
        case Section: picked = h.a; break;
        case Area:    picked = h.a; break;
        case CardRow: picked = h.a; playProgression (h.a); break;
        case Next:    newQuizKey(); break;
        case RollCell:
        {
            auto& cell = sketch[(size_t) h.a][(size_t) h.b];
            cell = ! cell;
            rollToPattern(); send();
            if (cell && ! sound.isPlaying()) sound.playNote (kSketchNotes[h.a], 0.5f);
            break;
        }
        default: break;
    }
}

// ---- keyboard -------------------------------------------------------------------------------------
void BeatScenes::setupKeys()
{
    keyRoot = 0; degree = 0; pressedKey = lastKey = prevKey = -1; chordMajor = true; relativeMinor = false;
    scalePick = variant == 12 ? Scale::HarmonicMinor : variant == 13 ? Scale::Phrygian : variant == 14 ? Scale::Dorian : Scale::Minor;
    if (variant == 9) { quizKey = -1; newQuizKey(); }
}

void BeatScenes::newQuizKey()
{
    quizKey = (quizKey + 1) % 6;
    quizFound = {}; quizWrong = 0; wrongKey = -1;
}

juce::String BeatScenes::quizKeyName() const { return theory::noteName (kQuizRoots[juce::jmax (0, quizKey)]) + " minor"; }

Scale BeatScenes::keysScale() const
{
    switch (variant)
    {
        case 3:  return Scale::Major;
        case 6:  return Scale::MinorPentatonic;
        case 10: case 12: case 13: case 14: return scalePick;
        case 11: return relativeMinor ? Scale::Minor : Scale::Major;
        default: return Scale::Minor;
    }
}

// The notes of the chosen scale, one after another from the root up to the root above, once.
void BeatScenes::playScale()
{
    LessonPattern p; p.once = true; p.stepsPerBeat = 2; p.bpm = 132.0f;
    int n = 0;
    if (variant == 1) { for (int note : { 48, 60, 72 }) { p.lead[(size_t) (n * 2)] = (signed char) note; p.leadLen[(size_t) (n * 2)] = 2; ++n; } p.steps = 7; }
    else
    {
        const int base = 48 + keyRoot;
        auto steps = theory::scaleSteps (keysScale());
        steps.push_back (12);
        for (int st : steps) { p.lead[(size_t) n] = (signed char) (base + st); p.leadLen[(size_t) n] = 1; ++n; }
        p.steps = n + 1;
    }
    pattern = p;
    sound.stop(); send(); sound.start();
}

void BeatScenes::playChord (const std::array<int, 3>& notes, float seconds) { for (int n : notes) sound.playNote (n, seconds); }

float BeatScenes::keyX (int note, juce::Rectangle<float> keys, bool& black) const
{
    const float ww = keys.getWidth() / 15.0f;
    black = isBlack (note);
    return black ? keys.getX() + (float) whiteIndex (note) * ww : keys.getX() + ((float) whiteIndex (note) + 0.5f) * ww;      // the centre of the key
}

void BeatScenes::paintKeys (juce::Graphics& g, juce::Rectangle<float> r)
{
    const Scale scale = keysScale();
    const bool showsScale = variant >= 3 && variant != 7 && variant != 8 && variant != 9;
    const bool rootPicker = (variant >= 3 && variant <= 7) || variant == 10 || variant >= 12;
    const bool flats = variant == 9 ? theory::keyUsesFlats (kQuizRoots[quizKey], Scale::Minor) : (variant <= 2 ? false : theory::keyUsesFlats (keyRoot, scale));
    const auto chords = theory::chordsOfKey (keyRoot, Scale::Minor, 48);

    // ---- the row of choices
    auto row = r.removeFromTop (28.0f);
    row.removeFromRight (126.0f);
    if (showsScale || variant == 1) { button (g, row.removeFromLeft (variant == 1 ? 130.0f : 116.0f), variant == 1 ? "Play the three Cs" : "Play the scale", false, Play); row.removeFromLeft (12.0f); }
    if (rootPicker)
    {
        for (int pc = 0; pc < 12; ++pc) { button (g, row.removeFromLeft (30.0f), theory::noteName (pc, theory::keyUsesFlats (pc, scale)), pc == keyRoot, Root, pc); row.removeFromLeft (2.0f); }
        row.removeFromLeft (10.0f);
    }
    if (variant == 8)
    {
        button (g, row.removeFromLeft (120.0f), "C major", chordMajor, Toggle, 0); row.removeFromLeft (6.0f);
        button (g, row.removeFromLeft (120.0f), "C minor", ! chordMajor, Toggle, 1);
    }
    if (variant == 11)
    {
        button (g, row.removeFromLeft (180.0f), "Home on C: C major", ! relativeMinor, Toggle, 0); row.removeFromLeft (6.0f);
        button (g, row.removeFromLeft (180.0f), "Home on A: A minor", relativeMinor, Toggle, 1);
    }
    r.removeFromTop (8.0f);
    if (variant == 10 || variant >= 12)
    {
        auto second = r.removeFromTop (26.0f);
        for (auto s : kExplorerScales) { button (g, second.removeFromLeft (86.0f), juce::String (theory::scaleName (s)).replace ("pentatonic", "pent.").replace ("harmonic", "harm."), s == scalePick, ScalePick, (int) s); second.removeFromLeft (3.0f); }
        r.removeFromTop (8.0f);
    }
    if (variant == 7)
    {
        auto second = r.removeFromTop (26.0f);
        for (int d = 0; d < (int) chords.size(); ++d) { button (g, second.removeFromLeft (64.0f), chords[(size_t) d].numeral, d == degree, Degree, d); second.removeFromLeft (4.0f); }
        r.removeFromTop (8.0f);
    }

    // ---- what the keyboard is showing, in words
    juce::String caption;
    const auto key = theory::noteName (keyRoot, flats) + " " + theory::scaleName (scale);
    if (variant == 0) caption = lastKey >= 0 ? "That was " + theory::noteName (lastKey) + (isBlack (lastKey) ? ", also called " + theory::noteName (lastKey, true) : juce::String()) + "." : juce::String ("Twelve notes, then the same twelve again, higher.");
    else if (variant == 1) caption = "Every C. Each one is an octave above the last and vibrates twice as fast.";
    else if (variant == 2)
    {
        if (lastKey >= 0 && prevKey >= 0 && lastKey != prevKey)
        {
            const int d = std::abs (lastKey - prevKey);
            caption = theory::noteName (juce::jmin (prevKey, lastKey)) + " to " + theory::noteName (juce::jmax (prevKey, lastKey)) + ": " + juce::String (d) + (d == 1 ? " semitone" : " semitones")
                    + (d == 2 ? ", which is one tone." : d % 2 == 0 ? ", which is " + juce::String (d / 2) + " tones." : d == 12 ? ", one octave." : ".");
        }
        else caption = "Click two keys to count the distance between them. E to F and B to C have no black key between.";
    }
    else if (variant == 5) caption = lastKey < 0 ? "The yellow keys are " + key + ". Click one, then click a dim one."
                                   : theory::noteName (lastKey, flats) + (theory::inScale (lastKey, keyRoot, scale) ? " is in " + key + ": it sits with the chord." : " is outside " + key + ": hear it rub against the chord.");
    else if (variant == 7 && degree < (int) chords.size()) caption = chords[(size_t) degree].numeral + "   " + chords[(size_t) degree].name + ":  " + chords[(size_t) degree].notes;
    else if (variant == 8) caption = chordMajor ? "C major: C E G. The middle note is four semitones above the root." : "C minor: C Eb G. The middle note is three semitones above the root.";
    else if (variant == 9)
    {
        int found = 0; for (bool f : quizFound) if (f) ++found;
        caption = found >= 7 ? "All seven: " + theory::scaleNotes (kQuizRoots[quizKey], Scale::Minor) + (quizWrong == 0 ? ". Not one wrong note." : ".")
                             : "Find the notes of " + quizKeyName() + ":  " + juce::String (found) + " of 7.  Tone, semitone, tone, tone, semitone, tone, tone.";
        if (found >= 7) button (g, juce::Rectangle<float> (row.getX(), row.getY(), 110.0f, row.getHeight()), "Next key", true, Next);
    }
    else if (variant == 11) caption = "The same seven notes: C D E F G A B. Only the home note has moved.";
    else if (showsScale) caption = key + ":  " + theory::scaleNotes (keyRoot, scale);
    g.setFont (bodyBold (14.0f)); g.setColour (text);
    g.drawText (caption, r.removeFromTop (22.0f), juce::Justification::centredLeft);

    // ---- tones and semitones between the notes of the scale
    auto stepsRow = r.removeFromTop (20.0f);
    auto keys = r.withTrimmedTop (2.0f);
    keys = keys.withHeight (juce::jmin (keys.getHeight(), 190.0f));
    const float ww = keys.getWidth() / 15.0f, bw = ww * 0.6f, bh = keys.getHeight() * 0.62f;
    if (variant == 3 || variant == 4 || variant == 11)
    {
        auto st = theory::scaleSteps (scale); st.push_back (12);
        const int base = 48 + keyRoot;
        for (size_t i = 0; i + 1 < st.size(); ++i)
        {
            bool b1, b2;
            const float x1 = keyX (base + st[i], keys, b1), x2 = keyX (base + st[i + 1], keys, b2);
            const bool semi = st[i + 1] - st[i] == 1;
            g.setFont (bodyBold (11.5f)); g.setColour (semi ? yellow : textDim);
            g.drawText (semi ? "semi" : "tone", juce::Rectangle<float> ((x1 + x2) * 0.5f - 30.0f, stepsRow.getY(), 60.0f, stepsRow.getHeight()), juce::Justification::centred);
        }
    }

    // ---- the keys
    auto stateOf = [&] (int note) -> int          // 0 plain, 1 in the scale or chord, 2 the root, 3 dimmed
    {
        const int pc = note % 12;
        if (variant == 1) return pc == 0 ? 2 : 3;
        if (variant == 2) return (pc == 4 || pc == 5 || pc == 11 || pc == 0) ? 1 : 0;
        if (variant == 7 && degree < (int) chords.size())
        {
            const auto& m = chords[(size_t) degree].midi;
            if (note == m[0]) return 2;
            if (note == m[1] || note == m[2]) return 1;
            return theory::inScale (note, keyRoot, Scale::Minor) ? 0 : 3;
        }
        if (variant == 8) return note == 60 ? 2 : (note == (chordMajor ? 64 : 63) || note == 67) ? 1 : 3;
        if (variant == 9) return quizFound[(size_t) pc] ? (pc == kQuizRoots[quizKey] ? 2 : 1) : 0;
        if (showsScale) return theory::inScale (note, keyRoot, scale) ? (pc == keyRoot ? 2 : 1) : 3;
        return 0;
    };
    auto drawKey = [&] (int note)
    {
        const bool sharp = isBlack (note);
        const float x = sharp ? keys.getX() + (float) whiteIndex (note) * ww - bw * 0.5f : keys.getX() + (float) whiteIndex (note) * ww;
        const auto key1 = sharp ? juce::Rectangle<float> (x, keys.getY(), bw, bh) : juce::Rectangle<float> (x + 1.0f, keys.getY(), ww - 2.0f, keys.getHeight());
        const int state = stateOf (note);
        const bool wrong = note == wrongKey && wrongFor > 0.0f, pressed = note == pressedKey && pressedFor > 0.0f;
        juce::Colour fill = sharp ? juce::Colour (0xff161616) : juce::Colour (0xffe9e9e9);
        if (state == 1 || state == 2) fill = yellow;
        else if (state == 3) fill = sharp ? juce::Colour (0xff0c0c0c) : juce::Colour (0xff5a5a5a);
        if (wrong) fill = over;
        if (pressed) fill = fill.interpolatedWith (state == 1 || state == 2 ? juce::Colours::white : yellow, 0.45f);
        g.setColour (fill); g.fillRoundedRectangle (key1, 3.0f);
        if (sharp) { g.setColour (state == 1 || state == 2 ? juce::Colours::black : lineHi); g.drawRoundedRectangle (key1.reduced (0.5f), 3.0f, 1.0f); }
        const bool named = variant <= 2 || state == 1 || state == 2 || (variant == 5 && note == lastKey);
        if (named)
        {
            const bool dark = sharp && ! (state == 1 || state == 2 || wrong);
            g.setFont (state == 2 ? bodyBold (sharp ? 10.5f : 12.5f) : body (sharp ? 10.0f : 12.0f));
            g.setColour (dark ? textDim : juce::Colours::black);
            g.drawText (theory::noteName (note, flats), key1.withTop (key1.getBottom() - 22.0f), juce::Justification::centred);
        }
        if (state == 2) { g.setColour (juce::Colours::black); g.fillEllipse (key1.getCentreX() - 3.0f, key1.getBottom() - (sharp ? 30.0f : 34.0f), 6.0f, 6.0f); }
        add (key1, PianoKey, note);
    };
    for (int n = kLowKey; n <= kHighKey; ++n) if (! isBlack (n)) drawKey (n);
    for (int n = kLowKey; n <= kHighKey; ++n) if (isBlack (n)) drawKey (n);            // on top, and found first by a click
}

// ---- piano roll -----------------------------------------------------------------------------------
void BeatScenes::setupRoll()
{
    rollNotes.clear(); rollChords.clear();
    rollSteps = 32; rollPerBeat = 4; rollBpm = 140.0f; rollDrums = false; clashStep = -1;
    for (auto& rowOfCells : sketch) rowOfCells = {};
    auto use = [this] (const N& notes) { for (auto& n : notes) rollNotes.push_back ({ n[0], n[1], n[2], n[3], n[4] }); };
    const std::vector<std::pair<int, juce::String>> cmAb = { { 0, "C minor" }, { 16, "Ab major" } };
    switch (variant)
    {
        case 0:  use (motif); rollSteps = 16; break;
        case 1:  use (motifTwice); break;
        case 2:  use (motifVaried); break;
        case 3:  use (stepsLeaps); break;
        case 4:  use (twoChords); use (overChords); rollChords = cmAb; rollBpm = 120.0f; break;
        case 5:  use (twoChords); use (clashing); rollChords = cmAb; rollBpm = 120.0f; clashStep = 4; break;
        case 6:  use (twoChords); use (resolved); rollChords = cmAb; rollBpm = 120.0f; break;
        case 7:  use (callResponse); break;
        case 8:  use (callResponse); use (counterLine); break;
        case 9:  use (progression); rollPerBeat = 2; rollBpm = 90.0f; rollChords = { { 0, "i  C minor" }, { 8, "VI  Ab major" }, { 16, "III  Eb major" }, { 24, "VII  Bb major" } }; break;
        case 10: use (arpeggio); rollChords = cmAb; break;
        case 11: use (bassFollows); rollChords = cmAb; rollDrums = true; break;
        case 12: use (motifVaried); for (auto& n : motifVaried) rollNotes.push_back ({ n[0], n[1] - 12, n[2], 1, 0 }); break;
        case 13: use (bassFollows); rollNotes.erase (std::remove_if (rollNotes.begin(), rollNotes.end(), [] (const RollNote& n) { return n.voice == 0; }), rollNotes.end());
                 use (motifVaried); rollChords = cmAb; rollDrums = true; break;
        case 14: rollSteps = 16; rollDrums = true; rollNotes.push_back ({ 0, 36, 6, 3, 0 }); rollNotes.push_back ({ 10, 36, 5, 3, 0 }); break;
        case 15: use (slides); rollDrums = true; break;
        default: use (motif); rollSteps = 16; break;
    }
    rollToPattern();
}

void BeatScenes::rollToPattern()
{
    LessonPattern p; p.steps = rollSteps; p.stepsPerBeat = rollPerBeat; p.bpm = rollBpm;
    for (auto& n : rollNotes)
    {
        if (n.step < 0 || n.step >= LessonPattern::kMaxSteps) continue;
        const auto at = (size_t) n.step;
        const auto len = (signed char) juce::jlimit (1, 32, n.len);
        if (n.voice == 0)      { p.lead[at] = (signed char) n.note; p.leadLen[at] = len; }
        else if (n.voice == 1) { p.lead2[at] = (signed char) n.note; p.lead2Len[at] = len; }
        else if (n.voice == 2) { p.lead3[at] = (signed char) n.note; p.lead3Len[at] = len; }
        else                   { p.bass[at] = (signed char) n.note; p.bassLen[at] = len; p.bassSlide[at] = (signed char) n.slideTo; }
    }
    if (variant == 14)
        for (int s = 0; s < 16; ++s)
        {
            int voice = 0;
            for (int row = 0; row < 6 && voice < 3; ++row)
                if (sketch[(size_t) row][(size_t) s])
                {
                    auto& note = voice == 0 ? p.lead : voice == 1 ? p.lead2 : p.lead3;
                    auto& len  = voice == 0 ? p.leadLen : voice == 1 ? p.lead2Len : p.lead3Len;
                    note[(size_t) s] = (signed char) kSketchNotes[row]; len[(size_t) s] = 2; ++voice;
                }
        }
    if (rollDrums)
    {
        // half-time trap drums: a kick wherever the 808 starts, the snare on beat three, eighth-note hats
        for (int s = 0; s < rollSteps; ++s) if (p.bass[(size_t) s] > 0) p.kick[(size_t) s] = 100;
        for (int s = 8; s < rollSteps; s += 16) p.snare[(size_t) s] = 100;
        for (int s = 0; s < rollSteps; s += 2) p.hat[(size_t) s] = 62;
    }
    pattern = p;
}

void BeatScenes::paintRoll (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto tempoText = juce::String (juce::roundToInt (rollBpm)) + " BPM";
    juce::String caption = "C minor, " + tempoText + ".";
    if (variant == 5) caption = "C minor. The red note is E: one semitone from the E flat in the chord under it.";
    else if (variant == 6) caption = "C minor. The same phrase with that note moved down one key, to E flat.";
    else if (variant == 8) caption = "Yellow is the main melody. White is the counter melody, moving in its gaps.";
    else if (variant == 9) caption = "C minor, " + tempoText + ". Four bars, one chord in each.";
    else if (variant == 12) caption = "The same melody twice: white is one octave below yellow.";
    else if (variant == 14) caption = "C minor pentatonic over drums and an 808. Every row fits.";
    else if (variant == 15) caption = "An 808 line in C minor. The slanted lines are slides.";
    transport (g, r, caption);

    const int step = sound.currentStep();
    bool hasBass = false, hasKeys = variant == 14;
    int lo = 127, hi = 0;
    for (auto& n : rollNotes) { if (n.voice == 3) hasBass = true; else { hasKeys = true; lo = juce::jmin (lo, n.note); hi = juce::jmax (hi, n.note); } }
    const float gutter = 46.0f;
    const auto whole = r;
    auto drumLane = rollDrums ? r.removeFromBottom (34.0f) : juce::Rectangle<float>();
    if (rollDrums) r.removeFromBottom (4.0f);
    auto bassLane = hasBass ? r.removeFromBottom (hasKeys ? 30.0f : r.getHeight()) : juce::Rectangle<float>();
    if (hasBass) r.removeFromBottom (4.0f);
    auto chordRow = rollChords.empty() ? juce::Rectangle<float>() : r.removeFromTop (18.0f);
    const float x0 = r.getX() + gutter, cw = (r.getWidth() - gutter) / (float) rollSteps;
    const int stepsPerBar = rollPerBeat * 4;
    auto grid = [&] (juce::Rectangle<float> lane)
    {
        for (int s = 0; s <= rollSteps; s += rollPerBeat)
        {
            g.setColour (s % stepsPerBar == 0 ? lineHi : line);
            g.fillRect (x0 + (float) s * cw - 0.5f, lane.getY(), 1.0f, lane.getHeight());
        }
    };
    for (auto& c : rollChords)
    {
        g.setFont (bodyBold (11.5f)); g.setColour (step >= c.first && step < c.first + stepsPerBar ? yellow : textDim);
        g.drawText (c.second, juce::Rectangle<float> (x0 + (float) c.first * cw + 4.0f, chordRow.getY(), 200.0f, chordRow.getHeight()), juce::Justification::centredLeft);
    }

    if (variant == 14)
    {
        const float rh = r.getHeight() / 6.0f;
        for (int row = 0; row < 6; ++row)
        {
            const float y = r.getY() + (float) row * rh;
            g.setFont (kSketchNotes[row] % 12 == 0 ? bodyBold (12.0f) : body (12.0f)); g.setColour (kSketchNotes[row] % 12 == 0 ? yellow : textDim);
            g.drawText (theory::noteName (kSketchNotes[row], true), juce::Rectangle<float> (r.getX(), y, gutter - 8.0f, rh), juce::Justification::centredRight);
            for (int s = 0; s < 16; ++s)
            {
                const auto cell = juce::Rectangle<float> (x0 + (float) s * cw + 1.5f, y + 2.0f, cw - 3.0f, rh - 4.0f);
                const bool on = sketch[(size_t) row][(size_t) s], now = s == step;
                g.setColour (on ? (now ? text : yellow) : (now ? control.brighter (0.25f) : ((s / 4) % 2 == 0 ? control : raised)));
                g.fillRoundedRectangle (cell, 3.0f);
                add (cell.expanded (1.5f, 2.0f), RollCell, row, s);
            }
        }
    }
    else if (hasKeys)
    {
        lo -= 1; hi += 1;
        const int rows = hi - lo + 1;
        const float rh = r.getHeight() / (float) rows;
        for (int note = lo; note <= hi; ++note)
        {
            const float y = r.getBottom() - (float) (note - lo + 1) * rh;
            const bool in = theory::inScale (note, 0, Scale::Minor), root = note % 12 == 0;
            g.setColour (root ? raised.brighter (0.07f) : (in ? raised : black));
            g.fillRect (x0, y + 0.5f, cw * (float) rollSteps, rh - 1.0f);
            if (in && (rh >= 11.0f || note % 12 == 0 || note % 12 == 3 || note % 12 == 7))
            {
                g.setFont (root ? bodyBold (juce::jmin (11.0f, rh)) : body (juce::jmin (10.5f, rh))); g.setColour (root ? yellow : textMute);
                g.drawText (theory::noteName (note, true), juce::Rectangle<float> (r.getX(), y, gutter - 8.0f, rh), juce::Justification::centredRight);
            }
        }
        grid (r);
        for (auto& n : rollNotes)
        {
            if (n.voice == 3) continue;
            const auto box = juce::Rectangle<float> (x0 + (float) n.step * cw + 1.0f, r.getBottom() - (float) (n.note - lo + 1) * rh + 1.0f, (float) n.len * cw - 2.0f, juce::jmax (3.0f, rh - 2.0f));
            const bool now = step >= n.step && step < n.step + n.len;
            const bool clash = n.voice == 0 && n.step == clashStep;
            g.setColour (clash ? over : (n.voice == 0 || variant == 9) ? (now ? juce::Colours::white : yellow) : text.withAlpha (now ? 0.95f : 0.55f));
            g.fillRoundedRectangle (box, 2.5f);
        }
    }
    if (hasBass)
    {
        g.setColour (raised); g.fillRect (x0, bassLane.getY(), cw * (float) rollSteps, bassLane.getHeight());
        grid (bassLane);
        g.setFont (body (11.0f)); g.setColour (textMute);
        g.drawText ("808", juce::Rectangle<float> (r.getX(), bassLane.getY(), gutter - 8.0f, bassLane.getHeight()), juce::Justification::centredRight);
        int bLo = 127, bHi = 0;
        for (auto& n : rollNotes) if (n.voice == 3) { bLo = juce::jmin (bLo, n.note, n.slideTo > 0 ? n.slideTo : 127); bHi = juce::jmax (bHi, n.note, n.slideTo); }
        const float span = (float) juce::jmax (1, bHi - bLo), nh = hasKeys ? bassLane.getHeight() - 8.0f : 20.0f;
        auto yOf = [&] (int note) { return hasKeys ? bassLane.getY() + 4.0f : bassLane.getBottom() - 6.0f - nh - (bassLane.getHeight() - 12.0f - nh) * (float) (note - bLo) / span; };
        for (auto& n : rollNotes)
        {
            if (n.voice != 3) continue;
            const auto box = juce::Rectangle<float> (x0 + (float) n.step * cw + 1.0f, yOf (n.note), (float) n.len * cw - 2.0f, nh);
            const bool now = step >= n.step && step < n.step + n.len;
            g.setColour (now ? yellow : lineHi.brighter (0.25f)); g.fillRoundedRectangle (box, 2.5f);
            g.setFont (bodyBold (10.5f)); g.setColour (now ? onYellow : text);
            g.drawText (theory::noteName (n.note, true), box.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
            if (n.slideTo > 0)
            {
                const float x1 = box.getX() + box.getWidth() * 0.3f, x2 = box.getX() + box.getWidth() * 0.7f;
                const float y1 = box.getCentreY(), y2 = hasKeys ? box.getY() - 2.0f : yOf (n.slideTo) + nh * 0.5f;
                g.setColour (yellow); g.drawLine (x1, y1, x2, y2, 2.0f); g.drawLine (x2, y2, box.getRight(), y2, 2.0f);
                g.setFont (body (10.5f));
                g.drawText ("slides to " + theory::noteName (n.slideTo, true), juce::Rectangle<float> (box.getRight() + 4.0f, y2 - 8.0f, 90.0f, 16.0f), juce::Justification::centredLeft);
            }
        }
    }
    if (rollDrums)
    {
        const char* names[] = { "Hat", "Snare", "Kick" };
        const std::array<signed char, LessonPattern::kMaxSteps>* rowsOf[] = { &pattern.hat, &pattern.snare, &pattern.kick };
        const float lh = drumLane.getHeight() / 3.0f;
        g.setFont (body (10.0f));
        for (int row = 0; row < 3; ++row)
        {
            g.setColour (textMute);
            g.drawText (names[row], juce::Rectangle<float> (r.getX(), drumLane.getY() + (float) row * lh, gutter - 8.0f, lh), juce::Justification::centredRight);
            for (int s = 0; s < rollSteps; ++s)
                if ((*rowsOf[row])[(size_t) s] > 0)
                {
                    g.setColour (s == step ? yellow : textDim);
                    g.fillRoundedRectangle (x0 + (float) s * cw + 2.0f, drumLane.getY() + (float) row * lh + 2.0f, juce::jmax (3.0f, cw - 4.0f), lh - 4.0f, 2.0f);
                }
        }
    }
    if (step >= 0) { g.setColour (text.withAlpha (0.7f)); g.fillRect (x0 + (float) step * cw - 0.5f, whole.getY() + (rollChords.empty() ? 0.0f : 18.0f), 1.5f, whole.getHeight() - (rollChords.empty() ? 0.0f : 18.0f)); }
}
}
