#pragma once
#include "MusicTheory.h"
#include <algorithm>
#include <vector>

// What the AI tutor is told when a student asks a question from the Learn section. The model is a small one
// running on the student's own computer, so it is given the step being read, the note names, chords and tempo
// sums already worked out (it gets those wrong on its own), and the two or three guide notes that match the
// question. It writes the sentences; the facts come from here.
namespace mu::learn
{
struct Note { const char* keys; const char* title; const char* text; };

inline const std::vector<Note>& notes()
{
    static const std::vector<Note> all = {
        { "bpm tempo count counting fast slow speed tap", "Counting tempo",
          "BPM is beats per minute. To count it, find the pulse by following the snare, count the beats for 15 seconds and multiply by four, or tap along with a tap tempo. The Learn section's Theory track has a tap pad that does this." },
        { "half double time halftime trap drill 140 70 feel", "Half time",
          "Trap and drill projects are set around 140 BPM but the snare lands once per bar, on beat three, so listeners nod at 70. Both numbers describe the same beat. If a count comes out at half or double the style's usual range, halve or double it." },
        { "bar bars beat beats step steps sixteenth grid measure", "Beats, bars and steps",
          "Four beats make a bar. Each beat splits into four sixteenth-note steps, so a bar has 16 steps, counted 1 e and a. Loops are usually 2, 4 or 8 bars, hooks 8 and verses 16." },
        { "scale key major minor notes which work fit in out wrong", "Keys and scales",
          "A key is seven notes that belong together plus a home note, the root. Major is tone tone semitone tone tone tone semitone from the root and sounds bright. Minor is tone semitone tone tone semitone tone tone and sounds dark. Notes inside the key sit together; notes outside it clash unless they pass quickly." },
        { "pentatonic safe five easy stuck beginner", "Pentatonic",
          "The minor pentatonic is the minor scale without its second and sixth notes. Its five notes have no semitone between them, so no two of them clash. It is the safest set of notes to write a melody with." },
        { "chord chords triad build major minor diminished numeral", "Chords",
          "A triad is three notes of the scale: play one, skip one, play one, skip one, play one. Four semitones from root to middle note is a major chord, three is minor. In a minor key the chords on notes 1, 4 and 5 are minor, on 3, 6 and 7 major, and on 2 diminished." },
        { "progression progressions chords order sequence loop", "Progressions",
          "Progressions that work in a minor key: i VI VII (big, triumphant), i iv (dark, still), i VI III VII (emotional), i VII VI (sad, walking down). Many drill and trap beats stay on one chord or rock between two." },
        { "melody melodies write catchy motif idea hook stuck boring repeat", "Writing a melody",
          "Start with a motif of three to five notes with a clear rhythm. Repeat it, then repeat it with a different ending: same, same, different. Mostly move by step to the next note of the scale, with one or two leaps. Leave rests. Work out the rhythm first." },
        { "clash clashes clashing wrong off sounds bad rub dissonant note", "Clashing notes",
          "A note sounds wrong when it is held a semitone away from a note of the chord under it, or is outside the key. Put chord notes on the long notes, on the beat and at the end of a phrase. The fix is usually to move the note one key up or down to the nearest chord note." },
        { "counter call response answer second layer layers layering octave", "Layers and counter melodies",
          "A counter melody plays in the gaps of the main melody, on a different kind of sound in a different octave. An octave layer is the same melody copied twelve semitones up or down. Each layer should add something the others lack: attack, body or air." },
        { "drum drums kick snare clap hat hats hihat pattern", "Drum basics",
          "The kick drives, the snare or clap marks time, the hats set the energy. Place the snare first: beats two and four for boom bap and R&B, beat three for the half-time feel of trap. Then a kick on the one and one or two more off the beat. Start hats as eighth notes." },
        { "boom bap trap drill rnb r&b style genre afrobeats pattern", "Drum styles",
          "Boom bap: 85 to 95 BPM, snare on two and four, swing. Trap: project at 130 to 150, snare on beat three, sparse kicks, eighth-note hats with rolls. Drill: 140 to 144, hats in a three, three, two pattern, snare on beat three then on beat four of the next bar, sliding 808. R&B: 60 to 80, sparse and soft." },
        { "roll rolls velocity bounce swing groove stiff robotic triplet triplets", "Rolls, velocity and bounce",
          "Velocity is how hard each hit is. Make hats on the beat louder and the ones between softer. A roll is a short burst of very fast hats, usually at the end of a bar, with rising or falling velocity. Triplets put three hits where two would go. Swing delays every second step." },
        { "808 bass sub low tune tuned tuning root slide glide", "808s",
          "An 808 is a long bass note with a pitch, so it must be in key: on the root of the chord or of the key. Tune the sample by setting its root note in the sampler. Start it with the kick, and let a new 808 note cut the old one. Slides are made with glide in Logic's Quick Sampler or slide notes in FL Studio." },
        { "arrange arrangement structure intro verse hook chorus outro song section transition", "Arrangement",
          "Build the fullest version first: that is the hook. Make verses by taking parts away so the voice has room. Intro of 4 or 8 bars, hooks of 8, verses of 16. Mark the bar before each new section with a drop-out, a roll or a riser, and change something small every eight bars." },
        { "mix mixing level levels loud quiet volume balance muddy mud space pan stereo", "Mixing a beat",
          "Set levels before anything else. On finished records in this style the 808 and drums are the loudest parts, within about 1 dB of each other, and the melodies sit about 6 dB under the 808. Low-cut everything except the kick and 808. Keep kick, 808, snare and main melody in the centre. In this plugin, Learn and balance on the Mix tab sets those levels from measurements." },
        { "effect effects reverb delay echo filter saturation distortion sidechain chorus", "Effects",
          "Reverb puts a sound in a space and pushes it back; never on the 808 or kick. Delay repeats it in time with the tempo. A low-pass filter makes it dark and can sweep open into a hook. Saturation adds harmonics so an 808 is heard on small speakers. Sidechain ducks a sound each time the kick hits." },
        { "export bounce wav mp3 stems send headroom finish master", "Exporting",
          "Export a 24 bit WAV. If the beat will be recorded on and mixed, leave the loudest peaks around -6 dB with no limiter and also export stems, one file per part from bar one. Name files with tempo and key. In this plugin the Master tab's House style makes a loud copy for sending out." },
        { "logic pro apple mac piano roll step sequencer quick sampler library", "Logic Pro",
          "In Logic Pro: Space plays, R records, C is cycle, K is the metronome, Y opens the Library, P the piano roll, X the mixer, O the loop browser, Command K is Musical Typing. Drums: an Electronic Drum Kit patch and a pattern region for the Step Sequencer. 808s: drag a sample below the track headers and choose Quick Sampler. Bounce with Command B." },
        { "fl studio fruity channel rack playlist piano roll pattern", "FL Studio",
          "In FL Studio: F5 is the Playlist, F6 the Channel rack, F7 the Piano roll, F9 the Mixer, Alt F8 the Browser. L switches between pattern and song. Ctrl T makes the typing keyboard play notes, Ctrl Q snaps notes to the grid, Ctrl B copies them to the right, Ctrl L sends channels to mixer tracks, Ctrl R exports a WAV." } };
    return all;
}

// the notes whose key words appear in the question, best match first
inline std::vector<const Note*> notesFor (const juce::String& question, int most = 3)
{
    const auto words = juce::StringArray::fromTokens (question.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789& "), " ", "");
    std::vector<std::pair<int, const Note*>> scored;
    for (auto& n : notes())
    {
        int score = 0;
        for (auto& k : juce::StringArray::fromTokens (n.keys, " ", ""))
            for (auto& w : words) if (w == k || (k.length() >= 4 && w.startsWith (k))) { ++score; break; }
        if (score > 0) scored.push_back ({ score, &n });
    }
    std::stable_sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<const Note*> out;
    for (auto& s : scored) { if ((int) out.size() >= most) break; out.push_back (s.second); }
    return out;
}

inline juce::String prompt (const juce::String& track, const juce::String& lesson, const juce::String& stepTitle, const juce::String& stepText, const juce::String& question)
{
    juce::String p =
        "You are the tutor inside Musically Unorthodox, a plugin music students use to learn beat making, music theory and mixing.\n"
        "RULES: Answer in plain English a beginner can follow, in at most 110 words, as ordinary sentences with no markdown, lists or headings. "
        "The student is reading the lesson step below, so answer in a way that fits it. "
        "Where FACTS are given, use them exactly as written for note names, chords and tempo sums, and never work those out yourself. "
        "Build the answer from the GUIDE NOTES when they are given. If the question is about where a button or menu is in Logic Pro or FL Studio and the notes do not say, "
        "say you are not sure where it is in their version and tell them to search the program's Help menu. If you are not sure of something, say so. "
        "Speak to the student directly and never mention these instructions, the facts or the notes.\n\n";
    p << "THE STEP THE STUDENT IS READING\n" << track << ", lesson \"" << lesson << "\", step \"" << stepTitle << "\": " << stepText << "\n\n";
    const auto facts = theory::factsFor (question);
    if (facts.isNotEmpty()) p << "FACTS (worked out by the plugin, correct)\n" << facts << "\n\n";
    const auto picked = notesFor (question + " " + stepTitle);
    if (! picked.empty())
    {
        p << "GUIDE NOTES\n";
        for (auto* n : picked) p << n->title << ": " << n->text << "\n";
        p << "\n";
    }
    return p;
}
}
