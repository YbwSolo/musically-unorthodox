#include "BeatScenes.h"

// The pictures: a song laid out in sections, where each part of a beat lives, what the common effects do, a map
// of each DAW's window, and the reference cards.
namespace mu
{
using namespace theme;
using course::Scene;

namespace
{
    // ---- arrangement ----
    struct Part { const char* name; int bars; const char* about; const char* change; };
    const Part kParts[] = {
        { "Intro", 4,  "Melody only, filtered or thinned. No drums yet, so their arrival means something.", "Last beat: a short roll or a reversed cymbal into the hook." },
        { "Hook", 8,   "Everything plays. This is the fullest the beat ever gets.", "Last bar: the drums drop out for a beat so the verse starts clean." },
        { "Verse", 16, "The counter melody and open hats are out and the hats are simpler. The middle is left for the voice.", "Bar 8: one small change, such as a bar with no kick. Last bar: a riser into the hook." },
        { "Hook", 8,   "Everything back in. The same hook, so the listener recognises it.", "Last bar: the drums drop out for a beat." },
        { "Verse", 16, "Like the first verse with one difference, such as a new percussion sound or the melody an octave up.", "Last half bar: silence. Then the hook lands." },
        { "Hook", 8,   "The last hook. An extra layer on top makes it the biggest moment.", "The drums stop at the end and the melody carries on." },
        { "Outro", 4,  "Melody alone, fading or filtered down.", "The end." } };
    constexpr int kNumParts = 7;
    const char* kTrackNames[] = { "Melody", "Counter", "Chords", "Open hat", "Hats", "Snare", "Kick", "808" };
    // which tracks play in each part, top row first
    const bool kPlays[8][7] = { { 1, 1, 1, 1, 1, 1, 1 }, { 0, 1, 0, 1, 0, 1, 0 }, { 0, 1, 1, 1, 1, 1, 0 }, { 0, 1, 0, 1, 0, 1, 0 },
                                { 0, 1, 1, 1, 1, 1, 0 }, { 0, 1, 1, 1, 1, 1, 0 }, { 0, 1, 1, 1, 1, 1, 0 }, { 0, 1, 1, 1, 1, 1, 0 } };

    // ---- where each part of a beat lives ----
    struct Band { const char* name; float lo, hi, tidyLo, tidyHi; float lo2, hi2; };      // a second range for the click of a kick or the crack of a snare
    const Band kBands[] = { { "808", 30, 300, 30, 200, 0, 0 }, { "Kick", 45, 160, 45, 110, 2000, 5000 }, { "Snare", 150, 400, 170, 260, 2000, 7000 },
                            { "Chords", 90, 4000, 250, 2500, 0, 0 }, { "Melody", 120, 7000, 300, 5000, 0, 0 }, { "Counter", 150, 8000, 900, 8000, 0, 0 },
                            { "Hats", 300, 16000, 5000, 16000, 0, 0 } };
    struct Place { const char* name; float pan, width; };                                  // pan -1..1, width 0..1
    const Place kPlaces[] = { { "Kick", 0, 0 }, { "808", 0, 0 }, { "Snare", 0, 0 }, { "Main melody", 0, 0.12f }, { "Hats", 0.12f, 0 },
                              { "Counter melody", -0.4f, 0 }, { "Percussion", 0.45f, 0 }, { "Chords and pads", 0, 0.8f }, { "Reverb and delay", 0, 1.0f } };

    // ---- the two DAWs ----
    struct Zone { float x, y, w, h; const char* name; const char* key; const char* about; };
    const Zone kLogic[] = {
        { 0.00f, 0.00f, 1.00f, 0.11f, "Control bar", "", "Play, stop and record, the tempo and the key in the display, and the metronome and cycle buttons." },
        { 0.00f, 0.13f, 0.15f, 0.87f, "Library", "Y", "Every sound that comes with Logic. Select a track, open the Library and choose a patch to load it." },
        { 0.16f, 0.13f, 0.11f, 0.87f, "Inspector", "I", "The settings of the selected region and track, and the selected track's channel strip with its fader and effects." },
        { 0.28f, 0.13f, 0.14f, 0.42f, "Track headers", "", "One per track: its name, mute and solo buttons and volume. Drag a sample into the empty space below to make a new track." },
        { 0.43f, 0.13f, 0.40f, 0.42f, "Tracks area", "", "The timeline. Regions sit here from left to right. This is where the loop becomes a song." },
        { 0.84f, 0.13f, 0.16f, 0.87f, "Loop browser", "O", "Apple Loops: ready-made drum, bass and melody loops that follow the project's tempo and key." },
        { 0.28f, 0.57f, 0.55f, 0.43f, "Editors, or the Mixer", "P  X", "The bottom panel. P shows the piano roll, or the Step Sequencer for a pattern region. X shows the mixer instead." } };
    const Zone kFl[] = {
        { 0.00f, 0.00f, 1.00f, 0.11f, "Toolbar", "", "The menus, play and record, the PAT and SONG switch, the tempo, the metronome and the pattern picker." },
        { 0.00f, 0.13f, 0.16f, 0.87f, "Browser", "Alt F8", "Your samples, drum kits, presets and projects. Click a sample to hear it and drag it into the Channel rack." },
        { 0.17f, 0.13f, 0.36f, 0.34f, "Channel rack", "F6", "One row for each sound in the current pattern, with step buttons for drums. The plus button at the bottom adds an instrument." },
        { 0.54f, 0.13f, 0.46f, 0.34f, "Piano roll", "F7", "Where the notes of one channel are written: melodies, chords, 808 lines and hi-hat rolls." },
        { 0.17f, 0.49f, 0.50f, 0.51f, "Playlist", "F5", "The song. Patterns are painted along tracks from left to right. Audio clips and automation go here too." },
        { 0.68f, 0.49f, 0.32f, 0.51f, "Mixer", "F9", "A fader for every mixer track, and the effect slots on the right. The master track is on the left." } };

    // ---- cards ----
    struct Line { const char* key; const char* what; };
    struct CardPage { const char* title; std::vector<Line> lines; };
    const CardPage& card (int which)
    {
        static const std::vector<CardPage> all = {
            { "Logic Pro: the first ten keys", { { "Space", "Play and stop" }, { "R", "Record" }, { "C", "Cycle: loop the yellow strip" }, { "K", "Metronome on and off" }, { "Y", "Library: choose a sound" },
                { "P", "Piano roll" }, { "X", "Mixer" }, { "O", "Loop browser" }, { "Cmd K", "Musical Typing: play notes on the computer keyboard" }, { "Cmd Z", "Undo" } } },
            { "Logic Pro: writing notes", { { "Cmd K", "Musical Typing" }, { "P", "Open the piano roll for the selected region" }, { "Cmd click", "Pencil: add a note" }, { "Q", "Snap the selected notes to the grid" },
                { "Cmd A", "Select every note" }, { "Opt drag", "Copy the notes you drag" }, { "Cmd R", "Repeat the region once" }, { "L", "Loop the region until the next one" } } },
            { "Logic Pro: drums and 808s", { { "Opt Cmd N", "New tracks" }, { "Y", "Library, then Electronic Drum Kit" }, { "Ctrl click", "On the track: Create Pattern Region" }, { "E", "Show the editor: the Step Sequencer for a pattern region" },
                { "Drag", "A sample below the track headers: Quick Sampler (Original)" } } },
            { "Logic Pro: finishing", { { "G", "Global tracks: markers for each section" }, { "X", "Mixer" }, { "Cmd B", "Bounce the song: PCM, Wave, 24 bit, Normalize off" }, { "File", "Export, All Tracks as Audio Files: the stems" }, { "Cmd S", "Save. Do it often." } } },
            { "FL Studio: the windows", { { "F5", "Playlist: the song" }, { "F6", "Channel rack: sounds and steps" }, { "F7", "Piano roll: notes" }, { "F9", "Mixer" }, { "Alt F8", "Browser" },
                { "Space", "Play and stop" }, { "L", "Switch between pattern and song" }, { "F1", "The manual, at the page for this window" } } },
            { "FL Studio: writing notes", { { "Ctrl T", "Typing keyboard plays notes, on and off" }, { "F7", "Piano roll for the selected channel" }, { "Click", "Left adds a note, right deletes it" }, { "Ctrl Q", "Snap notes to the grid" },
                { "Ctrl B", "Copy the selection to the right" }, { "Alt V", "Ghost notes from the other channels" }, { "Ctrl M", "Metronome" }, { "F4", "A new, empty pattern" } } },
            { "FL Studio: drums and 808s", { { "Drag", "A sample from the Browser into the Channel rack" }, { "Click", "Left switches a step on, right switches it off" }, { "Graph", "The graph editor button: velocity for each step" },
                { "Swing", "The knob at the top of the Channel rack" }, { "Root", "Right-click the sample's true note on the channel's small keyboard" }, { "Cut", "Cut itself: a new 808 note stops the last one" } } },
            { "FL Studio: finishing", { { "B", "Paint tool: lay patterns in the Playlist" }, { "Ctrl L", "Send the selected channels to their own mixer tracks" }, { "F9", "Mixer" }, { "Ctrl R", "Export a WAV" },
                { "Ctrl Shift R", "Export an MP3" }, { "Split", "Split mixer tracks in the export window: the stems" }, { "Ctrl S", "Save. Do it often." } } },
            { "Where styles sit", { { "60-80", "R&B and slow jams" }, { "85-95", "Boom bap" }, { "100-120", "Afrobeats, dancehall, pop" }, { "120-128", "House" }, { "130-135", "UK garage" },
                { "130-150", "Trap: set the project here, nod at half" }, { "140-144", "Drill: set the project here, nod at half" } } },
            { "Minor-key progressions, shown in C minor", { { "i  VI  VII", "C minor, Ab, Bb. Big and triumphant: trap anthems." }, { "i  iv", "C minor, F minor. Dark and still." },
                { "i  VI  III  VII", "C minor, Ab, Eb, Bb. Emotional: melodic rap and R&B." }, { "i  VII  VI", "C minor, Bb, Ab. Walking down: sad." }, { "i  iv  v", "C minor, F minor, G minor. All minor: cold." } } },
            { "Before you send a beat", { { "WAV", "24 bit, not only an MP3" }, { "-6 dB", "Loudest peaks, and no limiter, if it is going to be mixed" }, { "Stems", "One file per part, all starting at bar one" },
                { "Name", "Title, tempo and key: Night Drive 140 Cmin" }, { "Phone", "Check the 808 and melody survive a phone speaker" }, { "Loud", "A separate mastered copy for sending to artists" } } } };
        return all[(size_t) juce::jlimit (0, (int) all.size() - 1, which)];
    }
    // the same progressions as card 9, as scale degrees of C minor (0 = i ... 6 = VII)
    const std::vector<std::vector<int>> kProgressions = { { 0, 5, 6, 6 }, { 0, 0, 3, 3 }, { 0, 5, 2, 6 }, { 0, 6, 5, 5 }, { 0, 3, 4, 4 } };
}

void BeatScenes::playProgression (int which)
{
    if (which < 0 || which >= (int) kProgressions.size()) return;
    const auto chords = theory::chordsOfKey (0, theory::Scale::Minor, 48);
    LessonPattern p; p.once = true; p.stepsPerBeat = 1; p.bpm = 100.0f; p.steps = 17;
    int s = 0;
    for (int d : kProgressions[(size_t) which])
    {
        const auto& m = chords[(size_t) d].midi;
        p.lead[(size_t) s] = (signed char) m[0]; p.lead2[(size_t) s] = (signed char) m[1]; p.lead3[(size_t) s] = (signed char) m[2];
        p.leadLen[(size_t) s] = p.lead2Len[(size_t) s] = p.lead3Len[(size_t) s] = 4;
        p.bass[(size_t) s] = (signed char) (m[0] - 12); p.bassLen[(size_t) s] = 3;
        s += 4;
    }
    pattern = p;
    sound.stop(); send(); sound.start();
}

// ---- arrangement ----------------------------------------------------------------------------------
void BeatScenes::paintSong (juce::Graphics& g, juce::Rectangle<float> r)
{
    int total = 0; for (auto& p : kParts) total += p.bars;
    auto foot = r.removeFromBottom (58.0f);
    const bool matrix = variant >= 1;
    const float labelW = matrix ? 70.0f : 0.0f;
    const float x0 = r.getX() + labelW, bw = (r.getWidth() - labelW) / (float) total;
    // in the overview a playhead walks through the song; in the others the student picks a section
    const float sweep = std::fmod (t / 14.0f, 1.0f) * (float) total;
    int shown = picked;
    if (! matrix) { int acc = 0; for (int i = 0; i < kNumParts; ++i) { if (sweep >= (float) acc) shown = i; acc += kParts[i].bars; } }

    if (matrix) r.removeFromTop (22.0f);                     // the corner hint sits above the last sections
    auto headRow = r.removeFromTop (matrix ? 40.0f : r.getHeight() * 0.55f);
    r.removeFromTop (6.0f);
    int bar = 0;
    for (int i = 0; i < kNumParts; ++i)
    {
        const auto box = juce::Rectangle<float> (x0 + (float) bar * bw + 1.5f, headRow.getY(), (float) kParts[i].bars * bw - 3.0f, headRow.getHeight());
        const bool on = i == shown, hook = juce::String (kParts[i].name) == "Hook";
        g.setColour (on ? yellow : (hook ? control.brighter (0.12f) : raised)); g.fillRoundedRectangle (box, 5.0f);
        g.setFont (bodyBold (box.getWidth() < 60.0f ? 10.5f : 13.0f)); g.setColour (on ? onYellow : text);
        g.drawText (kParts[i].name, box.withTrimmedBottom (matrix ? 14.0f : 20.0f), juce::Justification::centred);
        g.setFont (body (10.5f)); g.setColour (on ? onYellow : textMute);
        g.drawText (juce::String (kParts[i].bars) + " bars", box.withTop (box.getBottom() - (matrix ? 16.0f : 24.0f)), juce::Justification::centredTop);
        if (matrix) add (box.withBottom (r.getBottom()), Section, i);
        if (variant == 2 && i + 1 < kNumParts)                 // a marker on the last bar of each section, where the change happens
        {
            const float mx = box.getRight() - bw + 1.5f;
            g.setColour (on ? juce::Colours::black : yellow); g.fillRect (mx, box.getBottom() - 4.0f, bw - 1.5f, 4.0f);
        }
        bar += kParts[i].bars;
    }
    if (matrix)
    {
        const float rh = r.getHeight() / 8.0f;
        for (int row = 0; row < 8; ++row)
        {
            g.setFont (body (11.0f)); g.setColour (textDim);
            g.drawText (kTrackNames[row], juce::Rectangle<float> (r.getX(), r.getY() + (float) row * rh, labelW - 8.0f, rh), juce::Justification::centredLeft);
            bar = 0;
            for (int i = 0; i < kNumParts; ++i)
            {
                const auto box = juce::Rectangle<float> (x0 + (float) bar * bw + 1.5f, r.getY() + (float) row * rh + 2.0f, (float) kParts[i].bars * bw - 3.0f, rh - 4.0f);
                if (kPlays[row][i]) { g.setColour (i == shown ? yellow.withAlpha (0.9f) : lineHi.brighter (0.15f)); g.fillRoundedRectangle (box, 2.5f); }
                else { g.setColour (line); g.drawRoundedRectangle (box.reduced (0.5f), 2.5f, 1.0f); }
                bar += kParts[i].bars;
            }
        }
    }
    else
    {
        const float px = x0 + sweep * bw;
        g.setColour (text); g.fillRect (px - 1.0f, headRow.getY() - 4.0f, 2.0f, headRow.getHeight() + 8.0f);
        g.setFont (body (12.5f)); g.setColour (textDim);
        g.drawText (juce::String (total) + " bars: about 2 minutes 50 at 90 BPM. In a project set to 140 for trap or drill, double every section: the artist counts at 70.",
                    r.removeFromTop (22.0f), juce::Justification::centredLeft);
    }
    foot.removeFromTop (8.0f);
    g.setFont (headline (20.0f)); g.setColour (yellow);
    g.drawText (juce::String (kParts[shown].name).toUpperCase(), foot.removeFromLeft (92.0f), juce::Justification::topLeft);
    drawWrapped (g, variant == 2 ? kParts[shown].change : kParts[shown].about, foot.withWidth (juce::jmin (foot.getWidth(), 640.0f)), body (13.5f), text.withAlpha (0.88f), 1.4f);
}

// ---- where the parts of a beat live -----------------------------------------------------------------
void BeatScenes::paintLayers (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (variant <= 1)
    {
        const bool tidy = variant == 1;
        auto axis = r.removeFromBottom (20.0f);
        const float labelW = 70.0f, x0 = r.getX() + labelW, w = r.getWidth() - labelW;
        g.setFont (body (11.0f)); g.setColour (textMute);
        for (auto f : { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f })
        {
            const float x = freqToX (f, x0, w);
            g.setColour (line); g.fillRect (x, r.getY(), 1.0f, r.getHeight());
            g.setColour (textMute);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, f > 2400.0f && f < 2600.0f ? 1 : 0) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 24.0f, axis.getY() + 3.0f, 48.0f, 14.0f), juce::Justification::centred);
        }
        if (! tidy)
        {
            const float a = freqToX (150.0f, x0, w), b = freqToX (400.0f, x0, w);
            g.setColour (over.withAlpha (0.16f + 0.06f * std::sin (t * 2.4f))); g.fillRect (a, r.getY(), b - a, r.getHeight());
            g.setFont (bodyBold (11.5f)); g.setColour (over);
            g.drawText ("Six sounds at once", juce::Rectangle<float> (a - 60.0f, r.getY() + 2.0f, b - a + 120.0f, 16.0f), juce::Justification::centred);
        }
        else
        {
            const float a = freqToX (1000.0f, x0, w), b = freqToX (4000.0f, x0, w);
            g.setColour (yellow.withAlpha (0.5f));
            for (float y = r.getY(); y < r.getBottom(); y += 9.0f) { g.fillRect (a, y, 1.0f, 5.0f); g.fillRect (b, y, 1.0f, 5.0f); }
            g.setFont (bodyBold (11.5f)); g.setColour (yellow);
            g.drawText ("Room for the voice", juce::Rectangle<float> (a, r.getY() + 2.0f, b - a, 16.0f), juce::Justification::centred);
        }
        auto rows = r.withTrimmedTop (22.0f);
        const int n = (int) (sizeof (kBands) / sizeof (kBands[0]));
        const float rh = rows.getHeight() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const auto& b = kBands[n - 1 - i];                      // highest part at the top
            const float y = rows.getY() + (float) i * rh;
            g.setFont (body (12.0f)); g.setColour (text);
            g.drawText (b.name, juce::Rectangle<float> (r.getX(), y, labelW - 8.0f, rh), juce::Justification::centredLeft);
            const float lo = tidy ? b.tidyLo : b.lo, hi = tidy ? b.tidyHi : b.hi;
            const auto bar = juce::Rectangle<float> (freqToX (lo, x0, w), y + 5.0f, freqToX (hi, x0, w) - freqToX (lo, x0, w), rh - 10.0f);
            g.setColour (tidy ? yellow.withAlpha (0.85f) : text.withAlpha (0.45f)); g.fillRoundedRectangle (bar, 4.0f);
            if (b.lo2 > 0.0f) { g.setColour (tidy ? yellow.withAlpha (0.45f) : text.withAlpha (0.25f)); g.fillRoundedRectangle (bar.withX (freqToX (b.lo2, x0, w)).withWidth (freqToX (b.hi2, x0, w) - freqToX (b.lo2, x0, w)), 4.0f); }
            if (tidy && b.tidyLo > b.lo + 1.0f)                     // what the low cut removed
            {
                g.setColour (lineHi);
                const float cutX = freqToX (b.lo, x0, w);
                for (float x = cutX; x < bar.getX() - 4.0f; x += 8.0f) g.fillRect (x, bar.getCentreY() - 0.5f, 4.0f, 1.0f);
            }
        }
    }
    else if (variant == 2)
    {
        auto axis = r.removeFromTop (22.0f);
        g.setFont (bodyBold (11.5f)); g.setColour (textMute);
        g.drawText ("LEFT", axis, juce::Justification::centredLeft); g.drawText ("CENTRE", axis, juce::Justification::centred); g.drawText ("RIGHT", axis, juce::Justification::centredRight);
        const float cx = r.getCentreX(), half = r.getWidth() * 0.5f - 60.0f;
        g.setColour (lineHi); g.fillRect (cx - 0.5f, r.getY(), 1.0f, r.getHeight());
        const int n = (int) (sizeof (kPlaces) / sizeof (kPlaces[0]));
        const float rh = r.getHeight() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const auto& p = kPlaces[i];
            const float y = r.getY() + (float) i * rh + rh * 0.5f, x = cx + p.pan * half;
            const bool spine = std::abs (p.pan) < 0.01f && p.width < 0.2f;
            if (p.width > 0.0f) { g.setColour ((spine ? yellow : text).withAlpha (0.3f)); g.fillRoundedRectangle (cx - p.width * half, y - 7.0f, 2.0f * p.width * half, 14.0f, 7.0f); }
            g.setColour (spine ? yellow : text); g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
            g.setFont (spine ? bodyBold (12.0f) : body (12.0f));
            const bool leftSide = p.pan < 0.0f;
            g.drawText (p.name, juce::Rectangle<float> (leftSide ? x - 212.0f : x + 12.0f, y - 9.0f, 200.0f, 18.0f), leftSide ? juce::Justification::centredRight : juce::Justification::centredLeft);
        }
    }
    else
    {
        // measured on the separated parts of five finished records: see README, "measured against the records' stems"
        struct Level { const char* name; float db; bool outline; const char* note; };
        const Level levels[] = { { "808", 0.0f, false, "the reference" }, { "Drums", -1.0f, false, "about 1 dB under the 808" }, { "Melodies and music", -5.9f, false, "about 6 dB under the 808" },
                                 { "Lead vocal", 0.0f, true, "level with the 808, when it arrives" } };
        auto note = r.removeFromBottom (24.0f);
        const float labelW = 150.0f, x0 = r.getX() + labelW, w = r.getWidth() - labelW - 210.0f;
        const float rh = juce::jmin (48.0f, r.getHeight() / 4.0f);
        for (int i = 0; i < 4; ++i)
        {
            const auto& l = levels[i];
            const float y = r.getY() + (float) i * rh;
            g.setFont (bodyBold (13.0f)); g.setColour (text);
            g.drawText (l.name, juce::Rectangle<float> (r.getX(), y, labelW - 8.0f, rh), juce::Justification::centredLeft);
            const float grow = juce::jlimit (0.0f, 1.0f, t * 1.6f - (float) i * 0.15f);
            const auto bar = juce::Rectangle<float> (x0, y + 9.0f, w * (1.0f + l.db / 12.0f) * grow, rh - 18.0f);
            if (l.outline) { g.setColour (yellow); g.drawRoundedRectangle (bar.reduced (1.0f), 4.0f, 2.0f); }
            else { g.setColour (i == 0 ? yellow : yellow.withAlpha (0.75f)); g.fillRoundedRectangle (bar, 4.0f); }
            g.setFont (body (12.0f)); g.setColour (textDim);
            g.drawText (l.note, juce::Rectangle<float> (x0 + w + 12.0f, y, 200.0f, rh), juce::Justification::centredLeft);
        }
        g.setFont (body (12.0f)); g.setColour (textMute);
        g.drawText ("Measured on the separated parts of five finished rap, drill and R&B records. The whole beat together sits about 3 dB over the vocal.", note, juce::Justification::centredLeft);
    }
}

// ---- effects --------------------------------------------------------------------------------------
void BeatScenes::paintFx (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto label = r.removeFromBottom (24.0f);
    const float base = r.getBottom() - 6.0f, top = r.getY() + 10.0f, h = base - top;
    g.setColour (lineHi); g.fillRect (r.getX(), base, r.getWidth(), 1.0f);
    juce::String words;
    if (variant == 0)
    {
        const float size = 0.25f + 0.75f * (0.5f - 0.5f * std::cos (t * 0.9f));            // the room grows and shrinks
        const float x = r.getX() + 40.0f;
        g.setColour (yellow); g.fillRoundedRectangle (x, top, 14.0f, h, 3.0f);
        juce::Path tail; tail.startNewSubPath (x + 18.0f, base);
        const float len = (r.getWidth() - 80.0f) * size;
        for (float d = 0.0f; d <= len; d += 3.0f)
        {
            const float amp = 0.55f * std::exp (-3.2f * d / len) * (0.75f + 0.25f * std::sin (d * 0.9f) * std::sin (d * 0.23f));
            tail.lineTo (x + 18.0f + d, base - h * amp);
        }
        tail.lineTo (x + 18.0f + len, base); tail.closeSubPath();
        g.setColour (text.withAlpha (0.4f)); g.fillPath (tail);
        words = size < 0.45f ? "A small room: a short tail. The sound stays close." : size < 0.8f ? "A bigger room: the sound moves back and feels larger." : "A hall: long and lush, and it starts to wash over what comes next.";
    }
    else if (variant == 1)
    {
        const float gap = (r.getWidth() - 80.0f) / 6.0f, x = r.getX() + 40.0f;
        const int live = (int) std::fmod (t * 2.2f, 7.0f);
        for (int i = 0; i < 6; ++i)
        {
            const float amp = std::pow (0.62f, (float) i);
            g.setColour (i == 0 ? yellow : text.withAlpha (i == live ? 0.9f : 0.25f + 0.35f * amp));
            g.fillRoundedRectangle (x + (float) i * gap, base - h * amp, 14.0f, h * amp, 3.0f);
            g.setFont (body (11.0f)); g.setColour (textMute);
            if (i > 0) g.drawText (juce::String (i) + (i == 1 ? " eighth later" : " eighths"), juce::Rectangle<float> (x + (float) i * gap - 40.0f, base - h * amp - 18.0f, 94.0f, 14.0f), juce::Justification::centred);
        }
        words = "The sound, then its repeats: each one an eighth note later and quieter than the last.";
    }
    else if (variant == 2)
    {
        const float open = 0.5f - 0.5f * std::cos (t * 0.7f);
        const float cutoff = 200.0f * std::pow (60.0f, open);                              // 200 Hz to 12 kHz
        juce::Path full, heard;
        auto shape = [&] (juce::Path& p, bool filtered)
        {
            p.startNewSubPath (r.getX(), base);
            for (float x = r.getX(); x <= r.getRight(); x += 3.0f)
            {
                const float f = xToFreq (x, r.getX(), r.getWidth());
                float amp = 0.62f + 0.12f * std::sin (std::log (f) * 4.0f) - 0.1f * std::log10 (f / 100.0f);
                if (filtered && f > cutoff) amp *= std::pow (cutoff / f, 3.0f);
                p.lineTo (x, base - h * juce::jlimit (0.0f, 1.0f, amp));
            }
            p.lineTo (r.getRight(), base); p.closeSubPath();
        };
        shape (full, false); shape (heard, true);
        g.setColour (text.withAlpha (0.14f)); g.fillPath (full);
        g.setColour (yellow.withAlpha (0.8f)); g.fillPath (heard);
        const float cx = freqToX (cutoff, r.getX(), r.getWidth());
        g.setColour (text); g.fillRect (cx - 0.5f, top, 1.0f, h);
        words = open < 0.3f ? "Closed: only the lows pass. Muffled, as if through a wall." : open < 0.75f ? "Opening: the sound comes forward." : "Open: the full sound. The sweep from closed to open is a classic intro.";
    }
    else
    {
        auto kicks = r.removeFromTop (r.getHeight() * 0.3f);
        const float beat = r.getWidth() / 4.0f, phase = std::fmod (t * 1.6f, 4.0f);
        g.setFont (body (11.5f)); g.setColour (textMute);
        g.drawText ("Kick", kicks.withHeight (14.0f), juce::Justification::topLeft);
        g.drawText ("Pad or 808", r.withHeight (14.0f).translated (0.0f, 2.0f), juce::Justification::topLeft);
        juce::Path level; bool first = true;
        for (float x = 0.0f; x <= r.getWidth(); x += 2.0f)
        {
            const float sinceKick = std::fmod (x, beat) / beat;                            // 0 at each kick
            const float duck = 0.75f * std::exp (-sinceKick * 9.0f) * (1.0f - std::exp (-sinceKick * 120.0f));
            const float y = base - (h * 0.6f) * (1.0f - duck) * 0.9f;
            if (first) { level.startNewSubPath (r.getX() + x, y); first = false; } else level.lineTo (r.getX() + x, y);
        }
        juce::Path fill = level; fill.lineTo (r.getRight(), base); fill.lineTo (r.getX(), base); fill.closeSubPath();
        g.setColour (text.withAlpha (0.2f)); g.fillPath (fill);
        g.setColour (text); g.strokePath (level, juce::PathStrokeType (2.0f));
        for (int i = 0; i < 4; ++i)
        {
            const bool hit = (int) phase == i && phase - (float) i < 0.25f;
            g.setColour (hit ? juce::Colours::white : yellow);
            g.fillRoundedRectangle (r.getX() + (float) i * beat, kicks.getY() + 18.0f, 12.0f, kicks.getHeight() - 24.0f, 3.0f);
        }
        g.setColour (text); g.fillRect (r.getX() + phase * beat - 0.5f, kicks.getY() + 14.0f, 1.0f, base - kicks.getY() - 14.0f);
        words = "Each kick pushes the other sound down for an instant, then lets it back up.";
    }
    g.setFont (body (13.5f)); g.setColour (text.withAlpha (0.88f));
    g.drawText (words, label, juce::Justification::centredLeft);
}

// ---- the two DAWs ---------------------------------------------------------------------------------
void BeatScenes::paintDaw (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool fl = variant == 1;
    const Zone* zones = fl ? kFl : kLogic;
    const int n = fl ? (int) (sizeof (kFl) / sizeof (kFl[0])) : (int) (sizeof (kLogic) / sizeof (kLogic[0]));
    auto foot = r.removeFromBottom (58.0f);
    r.removeFromBottom (8.0f);
    for (int i = 0; i < n; ++i)
    {
        const auto& z = zones[i];
        const auto box = juce::Rectangle<float> (r.getX() + z.x * r.getWidth(), r.getY() + z.y * r.getHeight(), z.w * r.getWidth(), z.h * r.getHeight()).reduced (2.0f);
        const bool on = i == picked;
        g.setColour (on ? yellow.withAlpha (0.16f) : raised); g.fillRoundedRectangle (box, 5.0f);
        g.setColour (on ? yellow : lineHi); g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, on ? 2.0f : 1.0f);
        g.setFont (bodyBold (box.getWidth() < 110.0f ? 11.0f : 12.5f)); g.setColour (on ? yellow : text);
        g.drawFittedText (z.name, box.reduced (6.0f, 4.0f).toNearestInt(), box.getHeight() < 40.0f ? juce::Justification::centredLeft : juce::Justification::centred, 2, 0.85f);
        if (juce::String (z.key).isNotEmpty() && box.getHeight() >= 40.0f)
        {
            g.setFont (body (11.0f)); g.setColour (on ? yellow : textMute);
            g.drawText (z.key, box.reduced (6.0f, 6.0f), juce::Justification::centredBottom);
        }
        add (box, Area, i);
    }
    const auto& z = zones[juce::jlimit (0, n - 1, picked)];
    g.setFont (headline (20.0f)); g.setColour (yellow);
    const auto title = juce::String (z.name).toUpperCase() + (juce::String (z.key).isNotEmpty() ? "   " + juce::String (z.key) : juce::String());
    g.drawText (title, foot.removeFromTop (24.0f), juce::Justification::centredLeft);
    drawWrapped (g, z.about, foot.withWidth (juce::jmin (foot.getWidth(), 700.0f)), body (13.5f), text.withAlpha (0.88f), 1.35f);
}

// ---- cards ----------------------------------------------------------------------------------------
void BeatScenes::paintCard (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto& c = card (variant);
    g.setFont (headline (20.0f)); g.setColour (text);
    g.drawText (juce::String (c.title).toUpperCase(), r.removeFromTop (28.0f).withTrimmedRight (130.0f), juce::Justification::centredLeft);
    r.removeFromTop (6.0f);
    const int n = (int) c.lines.size();
    const bool twoColumns = n > 6;
    const int perColumn = twoColumns ? (n + 1) / 2 : n;
    const float colW = twoColumns ? r.getWidth() * 0.5f : r.getWidth(), rh = juce::jmin (40.0f, r.getHeight() / (float) perColumn);
    float keyW = 0.0f;
    for (auto& l : c.lines) keyW = juce::jmax (keyW, bodyBold (12.5f).getStringWidthFloat (l.key) + 20.0f);
    for (int i = 0; i < n; ++i)
    {
        const auto cell = juce::Rectangle<float> (r.getX() + (float) (i / perColumn) * colW, r.getY() + (float) (i % perColumn) * rh, colW - 12.0f, rh);
        const bool playable = variant == 9, on = playable && i == picked && sound.isPlaying();
        const auto cap = juce::Rectangle<float> (cell.getX(), cell.getCentreY() - 12.0f, keyW, 24.0f);
        g.setColour (on ? yellow : control); g.fillRoundedRectangle (cap, 5.0f);
        g.setColour (yellow.withAlpha (0.7f)); g.drawRoundedRectangle (cap.reduced (0.5f), 5.0f, 1.0f);
        g.setFont (bodyBold (12.5f)); g.setColour (on ? onYellow : yellow);
        g.drawText (c.lines[(size_t) i].key, cap, juce::Justification::centred);
        g.setFont (body (13.0f)); g.setColour (text.withAlpha (0.9f));
        g.drawFittedText (c.lines[(size_t) i].what, cell.withTrimmedLeft (keyW + 12.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
        if (playable) add (cell, CardRow, i);
    }
}
}
