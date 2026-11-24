#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <vector>

// The music theory the Learn section draws and the tutor is told: note names, scales, the chords of a key and
// tempo arithmetic. Worked out here so that neither a lesson nor the AI model has to remember it.
namespace mu::theory
{
enum class Scale { Major, Minor, HarmonicMinor, MinorPentatonic, MajorPentatonic, Blues, Dorian, Phrygian, Count };

inline const char* scaleName (Scale s)
{
    static const char* names[] = { "major", "minor", "harmonic minor", "minor pentatonic", "major pentatonic", "blues", "Dorian", "Phrygian" };
    return names[(int) s];
}
inline std::vector<int> scaleSteps (Scale s)       // semitones above the root
{
    switch (s)
    {
        case Scale::Major:           return { 0, 2, 4, 5, 7, 9, 11 };
        case Scale::Minor:           return { 0, 2, 3, 5, 7, 8, 10 };
        case Scale::HarmonicMinor:   return { 0, 2, 3, 5, 7, 8, 11 };
        case Scale::MinorPentatonic: return { 0, 3, 5, 7, 10 };
        case Scale::MajorPentatonic: return { 0, 2, 4, 7, 9 };
        case Scale::Blues:           return { 0, 3, 5, 6, 7, 10 };
        case Scale::Dorian:          return { 0, 2, 3, 5, 7, 9, 10 };
        case Scale::Phrygian:        return { 0, 1, 3, 5, 7, 8, 10 };
        case Scale::Count:           break;
    }
    return {};
}
inline bool inScale (int midiNote, int root, Scale s)
{
    const int pc = ((midiNote - root) % 12 + 12) % 12;
    for (int step : scaleSteps (s)) if (step == pc) return true;
    return false;
}

// Flats for the keys that are written with flats, sharps for the rest, the way a producer would say them.
inline bool keyUsesFlats (int root, Scale s)
{
    const int pc = ((root % 12) + 12) % 12;
    const bool minorish = s != Scale::Major && s != Scale::MajorPentatonic;
    const int asMajor = minorish ? (pc + 3) % 12 : pc;                  // the major key with the same notes
    return asMajor == 5 || asMajor == 10 || asMajor == 3 || asMajor == 8 || asMajor == 1 || asMajor == 6;      // F Bb Eb Ab Db Gb
}
inline juce::String noteName (int midiNote, bool flats = false)
{
    static const char* sharp[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flat[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    const int pc = ((midiNote % 12) + 12) % 12;
    return flats ? flat[pc] : sharp[pc];
}
inline juce::String noteNameWithOctave (int midiNote, bool flats = false) { return noteName (midiNote, flats) + juce::String (midiNote / 12 - 1); }   // 60 = C4

inline juce::String scaleNotes (int root, Scale s)
{
    juce::StringArray names;
    for (int step : scaleSteps (s)) names.add (noteName (root + step, keyUsesFlats (root, s)));
    return names.joinIntoString (" ");
}

struct Chord { juce::String name, numeral, notes; std::array<int, 3> midi {}; bool major = false, diminished = false; };

// The seven three-note chords of a major or minor key: every other note of the scale, starting on each note in turn.
inline std::vector<Chord> chordsOfKey (int root, Scale s, int octaveBase = 60)
{
    std::vector<Chord> out;
    const auto steps = scaleSteps (s);
    if (steps.size() != 7) return out;
    static const char* upper[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lower[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    const bool flats = keyUsesFlats (root, s);
    const int base = octaveBase + ((root % 12) + 12) % 12;
    for (int d = 0; d < 7; ++d)
    {
        const auto at = [&] (int degree) { return base + steps[(size_t) (degree % 7)] + 12 * (degree / 7); };
        Chord c;
        c.midi = { at (d), at (d + 2), at (d + 4) };
        const int third = c.midi[1] - c.midi[0], fifth = c.midi[2] - c.midi[0];
        c.major = third == 4; c.diminished = fifth == 6;
        c.name = noteName (c.midi[0], flats) + (c.diminished ? " diminished" : c.major ? " major" : " minor");
        c.numeral = juce::String (c.major ? upper[d] : lower[d]) + (c.diminished ? juce::String::fromUTF8 ("\xc2\xb0") : juce::String());
        c.notes = noteName (c.midi[0], flats) + " " + noteName (c.midi[1], flats) + " " + noteName (c.midi[2], flats);
        out.push_back (c);
    }
    return out;
}

// ---- tempo arithmetic ----
inline double secondsPerBeat (double bpm) { return 60.0 / juce::jmax (1.0, bpm); }
inline double secondsPerBar (double bpm, int beatsPerBar = 4) { return secondsPerBeat (bpm) * (double) beatsPerBar; }
inline double msForNote (double bpm, double fractionOfBar) { return secondsPerBar (bpm) * fractionOfBar * 1000.0; }      // 0.25 = quarter note

// Tempo from a row of taps (times in seconds): the middle gap, so one late tap does not throw it.
inline double bpmFromTaps (std::vector<double> times)
{
    if (times.size() < 3) return 0.0;
    std::vector<double> gaps;
    for (size_t i = 1; i < times.size(); ++i) gaps.push_back (times[i] - times[i - 1]);
    std::sort (gaps.begin(), gaps.end());
    const double mid = gaps.size() % 2 == 1 ? gaps[gaps.size() / 2] : 0.5 * (gaps[gaps.size() / 2 - 1] + gaps[gaps.size() / 2]);
    return mid > 0.0 ? 60.0 / mid : 0.0;
}

// ---- what the tutor is told when a question names a key or a tempo ----
// Finds "F minor", "g# major", "Bb", "140 bpm" in a question and returns the facts as finished sentences.
inline juce::String factsFor (const juce::String& question)
{
    juce::String out;
    const auto q = " " + question.replace ("-", " ").replace (",", " ").replace ("?", " ").replace (".", " ") + " ";
    const auto words = juce::StringArray::fromTokens (q, " ", "");
    int found = 0;
    for (int i = 0; i < words.size() && found < 2; ++i)
    {
        const auto w = words[i];
        if (w.isEmpty() || w.length() > 2) continue;
        const juce::juce_wchar letter = juce::CharacterFunctions::toUpperCase (w[0]);
        if (letter < 'A' || letter > 'G') continue;

        static const int pcs[] = { 9, 11, 0, 2, 4, 5, 7 };
        int pc = pcs[letter - 'A'];
        if (w.length() == 2) { if (w[1] == '#') ++pc; else if (w[1] == 'b') --pc; else continue; }
        const auto next = words[juce::jmin (words.size() - 1, i + 1)].toLowerCase();
        const auto after = words[juce::jmin (words.size() - 1, i + 2)].toLowerCase();
        Scale s;
        if (next == "minor" || next == "min" || next == "m") s = after.startsWith ("pent") ? Scale::MinorPentatonic : Scale::Minor;
        else if (next == "major" || next == "maj") s = after.startsWith ("pent") ? Scale::MajorPentatonic : Scale::Major;
        else if (next == "harmonic") s = Scale::HarmonicMinor;
        else if (next == "dorian") s = Scale::Dorian;
        else if (next == "phrygian") s = Scale::Phrygian;
        else if (next == "blues") s = Scale::Blues;
        else continue;                                           // a bare letter is too often just a word
        // "a minor change" is not the key of A minor: a lower-case a counts only in a question about notes, keys, scales or chords
        if (w == "a" && (! (q.containsIgnoreCase (" note") || q.containsIgnoreCase (" key") || q.containsIgnoreCase (" scale") || q.containsIgnoreCase (" chords "))
                         || after == "chord" || after == "chords" || after == "third" || after == "triad" || after == "change")) continue;
        const int root = 60 + ((pc % 12) + 12) % 12;
        const bool flats = keyUsesFlats (root, s);
        const auto key = noteName (root, flats) + " " + scaleName (s);
        out << "The notes of " << key << " are " << scaleNotes (root, s) << ". Every other note is outside the key. ";
        const auto chords = chordsOfKey (root, s);
        if (! chords.empty())
        {
            juce::StringArray list;
            for (auto& c : chords) list.add (c.numeral + " " + c.name + " (" + c.notes + ")");
            out << "The chords of " << key << " are " << list.joinIntoString (", ") << ". ";
            if (s == Scale::Minor) out << "Its relative major, which uses the same notes, is " << noteName (root + 3, flats) << " major. ";
            if (s == Scale::Major) out << "Its relative minor, which uses the same notes, is " << noteName (root + 9, keyUsesFlats (root + 9, Scale::Minor)) << " minor. ";
        }
        ++found;
    }
    for (int i = 0; i < words.size(); ++i)
    {
        const auto w = words[i].toLowerCase();
        const bool joined = w.endsWith ("bpm") && w.length() > 3;
        const bool split = i + 1 < words.size() && words[i + 1].toLowerCase() == "bpm";
        if (! joined && ! split) continue;
        const double bpm = (joined ? w.dropLastCharacters (3) : w).getDoubleValue();
        if (bpm < 40.0 || bpm > 300.0) continue;
        const auto whole = [] (double v) { return juce::String (juce::roundToInt (v)); };
        out << "At " << whole (bpm) << " BPM one beat lasts " << whole (secondsPerBeat (bpm) * 1000.0) << " ms, one bar of four beats lasts "
            << juce::String (secondsPerBar (bpm), 2) << " seconds, an eighth note is " << whole (msForNote (bpm, 0.125)) << " ms and a sixteenth note is "
            << whole (msForNote (bpm, 0.0625)) << " ms. Half of " << whole (bpm) << " is " << whole (bpm * 0.5) << " and double is " << whole (bpm * 2.0) << ". ";
        break;
    }
    return out;
}
}
