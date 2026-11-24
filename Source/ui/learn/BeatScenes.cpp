#include "BeatScenes.h"

// Common parts, the tempo counter and the step grid. The keyboard and piano roll are in BeatScenesNotes.cpp and
// the pictures (arrangement, layers, effects, the DAW maps, the cards) in BeatScenesPictures.cpp.
namespace mu
{
using namespace theme;
using course::Scene;

namespace
{
    const int kHiddenTempos[] = { 96, 84, 112, 76, 104, 124, 88, 116 };
    constexpr int kNumHidden = 8;

    void setHits (std::array<signed char, LessonPattern::kMaxSteps>& row, std::initializer_list<int> steps, int velocity = 100)
    {
        for (int s : steps) if (s >= 0 && s < LessonPattern::kMaxSteps) row[(size_t) s] = (signed char) velocity;
    }
    void every (std::array<signed char, LessonPattern::kMaxSteps>& row, int from, int to, int stride, int velocity)
    {
        for (int s = from; s < to && s < LessonPattern::kMaxSteps; s += stride) row[(size_t) s] = (signed char) velocity;
    }
    // kick on one and three, snare on two and four, eighth-note hats: the plainest beat there is
    LessonPattern backbone (float bpm)
    {
        LessonPattern p; p.bpm = bpm;
        setHits (p.kick, { 0, 8 }); setHits (p.snare, { 4, 12 }); every (p.hat, 0, 16, 2, 80);
        return p;
    }
    const char* styleFor (float bpm)
    {
        if (bpm < 82.0f)  return "60 to 80: R&B and slow jams. Also how trap and drill feel when you nod to them.";
        if (bpm < 98.0f)  return "85 to 95: boom bap and classic hip hop.";
        if (bpm < 121.0f) return "100 to 120: afrobeats, dancehall and pop.";
        if (bpm < 130.0f) return "120 to 128: house and dance music.";
        if (bpm < 137.0f) return "130 to 135: UK garage and Jersey club.";
        if (bpm < 152.0f) return "130 to 150: where trap and drill projects are set (drill sits around 140 to 144).";
        return "Above 150: fast trap, drum and bass up at 170 and beyond.";
    }
}

// ---- shared ---------------------------------------------------------------------------------------
void BeatScenes::enter (Scene s, int v)
{
    sound.stop();
    scene = s; variant = v; t = 0.0f;
    hits.clear();
    pattern = {};
    picked = 0; hover = -1;
    if (scene == Scene::Pulse) setupPulse();
    else if (scene == Scene::Grid) setupGrid();
    else if (scene == Scene::Keys) setupKeys();
    else if (scene == Scene::Roll) setupRoll();
}

void BeatScenes::leave() { sound.stop(); }

bool BeatScenes::makesSound() const { return scene == Scene::Pulse || scene == Scene::Grid || scene == Scene::Keys || scene == Scene::Roll || (scene == Scene::Card && variant == 9); }

juce::String BeatScenes::hint() const
{
    if (scene == Scene::Pulse) return variant == 1 ? "Drag the tempo" : variant == 4 ? "Tap the pad" : variant == 3 ? "Press start" : variant == 5 ? "Switch the count" : "Press play";
    if (scene == Scene::Grid)  return variant == 1 ? "Click a row" : "Click the steps";
    if (scene == Scene::Keys)  return "Click the keys";
    if (scene == Scene::Roll)  return variant == 14 ? "Click to add notes" : "Press play";
    if (scene == Scene::Song)  return variant == 0 ? juce::String() : "Click a section";
    if (scene == Scene::Daw)   return "Move the mouse";
    if (scene == Scene::Card)  return variant == 9 ? "Click to hear one" : juce::String();
    return {};
}

void BeatScenes::tick (float dt)
{
    t += dt;
    if (pressedFor > 0.0f) pressedFor -= dt;
    if (wrongFor > 0.0f) wrongFor -= dt;
    if (scene == Scene::Pulse && variant == 3 && countState == 1)
    {
        countElapsed += dt;
        if (countElapsed >= 15.0f) { countState = 2; sound.stop(); }
    }
}

void BeatScenes::send() { sound.setPattern (pattern); }
void BeatScenes::togglePlay()
{
    if (sound.isPlaying()) { sound.stop(); return; }
    send(); sound.start();
}

void BeatScenes::button (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, bool on, int id, int a, int b)
{
    g.setColour (on ? yellow : control); g.fillRoundedRectangle (r, 5.0f);
    if (! on) { g.setColour (lineHi); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f); }
    g.setFont (bodyBold (juce::jmin (12.5f, r.getHeight() * 0.5f))); g.setColour (on ? onYellow : text);
    g.drawText (label, r, juce::Justification::centred);
    add (r, id, a, b);
}

// The play button and a line of text beside it, across the top of an example that can be heard.
void BeatScenes::transport (juce::Graphics& g, juce::Rectangle<float>& r, const juce::String& caption)
{
    auto row = r.removeFromTop (28.0f);
    r.removeFromTop (10.0f);
    const bool on = sound.isPlaying();
    button (g, row.removeFromLeft (84.0f), on ? "Stop" : "Play", on, Play);
    row.removeFromLeft (12.0f);
    row.removeFromRight (130.0f);                    // the corner hint sits there
    const bool silent = on && audioRunning && ! audioRunning();
    g.setFont (silent ? bodyBold (12.0f) : body (12.5f)); g.setColour (silent ? yellow : textDim);
    g.drawText (silent ? "No sound? Your DAW is not running this track yet: press play in the DAW, or select the track." : caption, row, juce::Justification::centredLeft);
}

void BeatScenes::mouseDown (juce::Point<float> p)
{
    for (int i = (int) hits.size() - 1; i >= 0; --i)
        if (hits[(size_t) i].r.contains (p)) { const auto h = hits[(size_t) i]; clicked (h, p); return; }
}

void BeatScenes::mouseDrag (juce::Point<float> p)
{
    if (scene == Scene::Pulse && variant == 1 && ! tempoTrack.isEmpty() && tempoTrack.expanded (40.0f, 60.0f).contains (p))
    {
        tempo = (float) juce::roundToInt (juce::jmap (juce::jlimit (0.0f, 1.0f, (p.x - tempoTrack.getX()) / tempoTrack.getWidth()), 60.0f, 180.0f));
        pattern.bpm = tempo; send();
    }
}

void BeatScenes::mouseMove (juce::Point<float> p)
{
    if (scene != Scene::Daw) return;
    hover = -1;
    for (int i = (int) hits.size() - 1; i >= 0; --i)
        if (hits[(size_t) i].id == Area && hits[(size_t) i].r.contains (p)) { hover = hits[(size_t) i].a; break; }
    if (hover >= 0) picked = hover;
}

void BeatScenes::paint (juce::Graphics& g, juce::Rectangle<float> r)
{
    hits.clear();
    if (scene == Scene::Pulse) paintPulse (g, r);
    else if (scene == Scene::Grid) paintGrid (g, r);
    else if (scene == Scene::Keys) paintKeys (g, r);
    else if (scene == Scene::Roll) paintRoll (g, r);
    else if (scene == Scene::Song) paintSong (g, r);
    else if (scene == Scene::Layers) paintLayers (g, r);
    else if (scene == Scene::Fx) paintFx (g, r);
    else if (scene == Scene::Daw) paintDaw (g, r);
    else if (scene == Scene::Card) paintCard (g, r);
}

// ---- tempo ----------------------------------------------------------------------------------------
void BeatScenes::setupPulse()
{
    countState = 0; countElapsed = -1.0f; taps.clear(); hiddenBpm = 0; halfTime = false; tempoTrack = {};
    tempo = variant == 1 ? 100.0f : 90.0f;
    if (variant == 0) { pattern.bpm = 90.0f; pattern.click = true; }
    else if (variant == 1) { pattern = backbone (tempo); pattern.click = true; }
    else if (variant == 2)
    {
        // sixteen beats, one step each: four bars, with a kick marking every "one"
        pattern.stepsPerBeat = 1; pattern.steps = 16; pattern.bpm = 100.0f; pattern.click = true;
        setHits (pattern.kick, { 0, 4, 8, 12 });
    }
    else if (variant == 5)
    {
        // two bars of trap at 140. The hits never change; only where the click counts does.
        pattern.steps = 32; pattern.bpm = 140.0f; pattern.click = true;
        setHits (pattern.kick, { 0, 6, 16, 22 }); setHits (pattern.snare, { 8, 24 }); every (pattern.hat, 0, 32, 2, 75);
        setHits (pattern.hat, { 13, 29, 30, 31 }, 60);
    }
}

void BeatScenes::startHidden (bool counting)
{
    hiddenIndex = (hiddenIndex + 1 + (int) (juce::Time::getMillisecondCounter() % 3)) % kNumHidden;
    hiddenBpm = kHiddenTempos[hiddenIndex];
    pattern = backbone ((float) hiddenBpm);
    taps.clear();
    countState = 1; countElapsed = counting ? 0.0f : -1.0f;
    send(); sound.start();
}

void BeatScenes::paintPulse (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int step = sound.currentStep();
    const int perBeat = juce::jmax (1, pattern.stepsPerBeat);
    auto lamps = [&] (juce::Rectangle<float> area, int count, int lit, bool numbers)
    {
        const float size = juce::jmin (area.getHeight(), area.getWidth() / (float) count - 18.0f);
        const float gap = (area.getWidth() - size * (float) count) / (float) juce::jmax (1, count - 1);
        for (int i = 0; i < count; ++i)
        {
            const auto c = juce::Rectangle<float> (area.getX() + (float) i * (size + gap), area.getCentreY() - size * 0.5f, size, size);
            const bool on = i == lit;
            g.setColour (on ? yellow : raised); g.fillEllipse (c);
            if (! on) { g.setColour (i == 0 ? yellow.withAlpha (0.55f) : lineHi); g.drawEllipse (c.reduced (1.0f), 1.5f); }
            if (numbers) { g.setFont (headline (size * 0.5f)); g.setColour (on ? onYellow : textDim); g.drawText (juce::String (i + 1), c, juce::Justification::centred); }
        }
    };

    if (variant == 0)
    {
        transport (g, r, "90 BPM. Count out loud with the click: one, two, three, four.");
        lamps (r.reduced (r.getWidth() * 0.14f, 24.0f), 4, step >= 0 ? (step / perBeat) % 4 : -1, true);
    }
    else if (variant == 1)
    {
        transport (g, r, juce::String (juce::roundToInt (tempo)) + " BPM: one beat every " + juce::String (60.0f / tempo, 2) + " seconds.");
        auto foot = r.removeFromBottom (86.0f);
        lamps (r.reduced (r.getWidth() * 0.22f, 10.0f), 4, step >= 0 ? (step / perBeat) % 4 : -1, true);
        tempoTrack = foot.removeFromTop (34.0f).reduced (30.0f, 14.0f);
        g.setColour (control); g.fillRoundedRectangle (tempoTrack, 3.0f);
        const float x = tempoTrack.getX() + tempoTrack.getWidth() * (tempo - 60.0f) / 120.0f;
        g.setColour (yellow); g.fillRoundedRectangle (tempoTrack.withRight (x), 3.0f);
        g.fillEllipse (x - 9.0f, tempoTrack.getCentreY() - 9.0f, 18.0f, 18.0f);
        g.setFont (body (11.0f)); g.setColour (textMute);
        for (int b = 60; b <= 180; b += 20)
            g.drawText (juce::String (b), juce::Rectangle<float> (tempoTrack.getX() + tempoTrack.getWidth() * (float) (b - 60) / 120.0f - 20.0f, tempoTrack.getBottom() + 6.0f, 40.0f, 14.0f), juce::Justification::centred);
        g.setFont (headline (30.0f)); g.setColour (text);
        auto line = foot.removeFromBottom (26.0f);
        g.drawText (juce::String (juce::roundToInt (tempo)), line.removeFromLeft (74.0f), juce::Justification::centredLeft);
        g.setFont (body (13.0f)); g.setColour (text.withAlpha (0.85f));
        g.drawText (styleFor (tempo), line, juce::Justification::centredLeft);
    }
    else if (variant == 2)
    {
        transport (g, r, "Four bars of four beats. The low thump marks the one of every bar.");
        const float bw = (r.getWidth() - 3.0f * 14.0f) / 4.0f;
        for (int bar = 0; bar < 4; ++bar)
        {
            auto box = juce::Rectangle<float> (r.getX() + (float) bar * (bw + 14.0f), r.getY() + 22.0f, bw, r.getHeight() - 40.0f);
            g.setFont (bodyBold (11.5f)); g.setColour (step >= 0 && step / 4 == bar ? yellow : textMute);
            g.drawText ("Bar " + juce::String (bar + 1), box.withY (r.getY()).withHeight (18.0f), juce::Justification::centredLeft);
            const float cw = (box.getWidth() - 3.0f * 5.0f) / 4.0f;
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto cell = juce::Rectangle<float> (box.getX() + (float) beat * (cw + 5.0f), box.getY(), cw, box.getHeight());
                const bool on = step == bar * 4 + beat;
                g.setColour (on ? yellow : (beat == 0 ? raised.brighter (0.12f) : raised)); g.fillRoundedRectangle (cell, 5.0f);
                g.setFont (headline (beat == 0 ? 30.0f : 22.0f)); g.setColour (on ? onYellow : (beat == 0 ? text : textDim));
                g.drawText (juce::String (beat + 1), cell, juce::Justification::centred);
            }
        }
    }
    else if (variant == 3)
    {
        auto row = r.removeFromTop (28.0f); r.removeFromTop (10.0f);
        button (g, row.removeFromLeft (150.0f), countState == 1 ? "Counting..." : countState == 2 ? "Try another tempo" : "Start 15 seconds", countState == 1, Start);
        row.removeFromLeft (12.0f); row.removeFromRight (130.0f);
        const bool silent = countState == 1 && audioRunning && ! audioRunning();
        g.setFont (body (12.5f)); g.setColour (silent ? yellow : textDim);
        g.drawText (silent ? "No sound? Your DAW is not running this track yet: press play in the DAW, or select the track."
                           : countState == 1 ? "Count every beat. Follow the snare on two and four." : "A beat plays for 15 seconds. Count the beats, then multiply by four.", row, juce::Justification::centredLeft);
        auto bar = r.removeFromTop (16.0f).reduced (0.0f, 4.0f);
        g.setColour (control); g.fillRoundedRectangle (bar, 4.0f);
        if (countState >= 1) { g.setColour (yellow); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, (countState == 2 ? 15.0f : countElapsed) / 15.0f)), 4.0f); }
        r.removeFromTop (8.0f);
        if (countState == 2)
        {
            const int beats = juce::roundToInt ((float) hiddenBpm / 4.0f);
            g.setFont (headline (58.0f)); g.setColour (yellow);
            g.drawText (juce::String (hiddenBpm) + " BPM", r.removeFromTop (r.getHeight() * 0.5f), juce::Justification::centredBottom);
            g.setFont (body (15.0f)); g.setColour (text);
            g.drawText ("You should have counted " + juce::String (beats) + " beats. " + juce::String (beats) + " x 4 = " + juce::String (beats * 4)
                        + (beats * 4 != hiddenBpm ? ", and the true tempo is " + juce::String (hiddenBpm) + ": counting gets you within a couple of BPM." : "."),
                        r.removeFromTop (30.0f), juce::Justification::centred);
            g.setFont (body (12.5f)); g.setColour (textDim);
            g.drawText ("One beat out in your count moves the answer by 4 BPM, so count twice if it matters.", r.removeFromTop (22.0f), juce::Justification::centred);
        }
        else
        {
            g.setFont (headline (countState == 1 ? 76.0f : 40.0f)); g.setColour (countState == 1 ? text : textMute);
            g.drawText (countState == 1 ? juce::String (juce::jmax (0, 15 - (int) countElapsed)) : juce::String ("beats in 15 seconds  x  4  =  BPM"), r, juce::Justification::centred);
        }
    }
    else if (variant == 4)
    {
        auto row = r.removeFromTop (28.0f); r.removeFromTop (10.0f);
        const bool on = sound.isPlaying();
        button (g, row.removeFromLeft (150.0f), on ? "Stop the beat" : (hiddenBpm > 0 ? "Another tempo" : "Start a practice beat"), on, Start);
        row.removeFromLeft (12.0f); row.removeFromRight (130.0f);
        const bool silent = on && audioRunning && ! audioRunning();
        g.setFont (body (12.5f)); g.setColour (silent ? yellow : textDim);
        g.drawText (silent ? "No sound? Your DAW is not running this track yet: press play in the DAW, or select the track."
                           : "Tap once per beat. Or leave the practice beat off and tap along to any song.", row, juce::Justification::centredLeft);
        auto pad = r.removeFromLeft (r.getHeight()).reduced (6.0f);
        const bool flash = pressedFor > 0.0f;
        g.setColour (flash ? yellow : raised); g.fillRoundedRectangle (pad, 14.0f);
        g.setColour (yellow); g.drawRoundedRectangle (pad.reduced (1.0f), 14.0f, 2.0f);
        g.setFont (headline (34.0f)); g.setColour (flash ? onYellow : text);
        g.drawText ("TAP", pad, juce::Justification::centred);
        add (pad, Pad);
        r.removeFromLeft (26.0f);
        const double mine = theory::bpmFromTaps (taps);
        g.setFont (bodyBold (11.5f)); g.setColour (textMute);
        g.drawText ("Your taps", r.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setFont (headline (64.0f)); g.setColour (mine > 0.0 ? yellow : textMute);
        g.drawText (mine > 0.0 ? juce::String (juce::roundToInt (mine)) + " BPM" : juce::String ("-- BPM"), r.removeFromTop (74.0f), juce::Justification::centredLeft);
        g.setFont (body (14.0f)); g.setColour (text.withAlpha (0.88f));
        juce::String line = taps.size() < 3 ? "Keep going: it needs at least three taps." : juce::String ((int) taps.size()) + " taps. The more you tap, the steadier the number.";
        if (hiddenBpm > 0 && taps.size() >= 8)
        {
            const int out = std::abs (juce::roundToInt (mine) - hiddenBpm);
            const bool doubled = std::abs (juce::roundToInt (mine * 0.5) - hiddenBpm) <= 3 || std::abs (juce::roundToInt (mine * 2.0) - hiddenBpm) <= 3;
            line = "The practice beat is " + juce::String (hiddenBpm) + " BPM. "
                 + (out <= 2 ? juce::String ("You have it.") : doubled ? juce::String ("You are tapping double or half: same pulse, counted at a different speed.")
                             : "You are " + juce::String (out) + " out: listen for the snare and tap again.");
        }
        drawWrapped (g, line, r.removeFromTop (60.0f).withWidth (juce::jmin (r.getWidth(), 520.0f)), body (14.0f), text.withAlpha (0.88f), 1.4f);
    }
    else
    {
        transport (g, r, halfTime ? "Counted slow: 70 BPM. The snare is on beat three of one long bar." : "Counted fast: 140 BPM. The snare is on beat three of each bar.");
        auto pick = r.removeFromTop (28.0f);
        button (g, pick.removeFromLeft (190.0f), "Count it at 140", ! halfTime, Toggle, 0);
        pick.removeFromLeft (8.0f);
        button (g, pick.removeFromLeft (190.0f), "Count it at 70", halfTime, Toggle, 1);
        r.removeFromTop (14.0f);
        // the same two bars of hits, drawn once; only the count underneath changes
        const float gutter = 46.0f;
        r.removeFromLeft (gutter);
        const float cw = r.getWidth() / 32.0f;
        auto lane = r.removeFromTop (r.getHeight() - 50.0f);
        const float lh = lane.getHeight() / 3.0f;
        const char* names[] = { "Hat", "Snare", "Kick" };
        const std::array<signed char, LessonPattern::kMaxSteps>* rows[] = { &pattern.hat, &pattern.snare, &pattern.kick };
        const int shownStep = step;
        for (int row = 0; row < 3; ++row)
        {
            g.setFont (body (12.0f)); g.setColour (text);
            g.drawText (names[row], juce::Rectangle<float> (lane.getX() - gutter, lane.getY() + (float) row * lh, gutter, lh), juce::Justification::centredLeft);
            for (int s = 0; s < 32; ++s)
            {
                const auto cell = juce::Rectangle<float> (lane.getX() + (float) s * cw + 1.0f, lane.getY() + (float) row * lh + 1.0f, cw - 2.0f, lh - 2.0f);
                const bool hit = (*rows[row])[(size_t) s] > 0;
                g.setColour (hit ? (s == shownStep ? text : yellow) : (s == shownStep ? control.brighter (0.2f) : raised));
                g.fillRoundedRectangle (cell, 2.5f);
            }
        }
        const int beats = halfTime ? 4 : 8;
        const float bw = r.getWidth() / (float) beats;
        const int beatNow = step < 0 ? -1 : step / (halfTime ? 8 : 4);
        for (int b = 0; b < beats; ++b)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + (float) b * bw + 2.0f, r.getY() + 10.0f, bw - 4.0f, 34.0f);
            g.setColour (b == beatNow ? yellow : control); g.fillRoundedRectangle (cell, 4.0f);
            g.setFont (headline (20.0f)); g.setColour (b == beatNow ? onYellow : textDim);
            g.drawText (juce::String (b % 4 + 1), cell, juce::Justification::centred);
        }
    }
}

// ---- step grid ------------------------------------------------------------------------------------
void BeatScenes::setupGrid()
{
    gridRows = 4; gridHas808 = false; gridFixed = false; lengthRow = 0;
    auto& p = pattern;
    switch (variant)
    {
        case 0:  p.bpm = 100.0f; setHits (p.kick, { 0, 4, 8, 12 }); break;
        case 1:  p.bpm = 90.0f; p.click = true; gridFixed = true; every (p.hat, 0, 16, 4, 100); break;
        case 2:  p = backbone (90.0f); break;
        case 3:  p = backbone (90.0f); p.kick = {}; setHits (p.kick, { 0, 7, 10 }); setHits (p.openHat, { 14 }, 70); p.swing = 0.55f; break;
        case 4:  p.bpm = 140.0f; p.steps = 32; setHits (p.kick, { 0, 6, 16, 19, 26 }); setHits (p.snare, { 8, 24 }); every (p.hat, 0, 32, 2, 80);
                 setHits (p.hat, { 13, 29, 31 }, 60); setHits (p.openHat, { 22 }, 70); break;
        case 5:  p.bpm = 142.0f; p.steps = 32; setHits (p.kick, { 0, 10, 16, 23, 26 }); setHits (p.snare, { 8, 28 });
                 setHits (p.hat, { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30 }, 85); break;
        case 6:  p.bpm = 70.0f; setHits (p.kick, { 0, 10 }); setHits (p.snare, { 4, 12 }); every (p.hat, 0, 16, 2, 55); setHits (p.openHat, { 14 }, 55); break;
        case 7:  // one bar in thirty-second notes: eighth-note hats, loud then soft, one double, and a rising roll into the next bar
                 p.bpm = 140.0f; p.steps = 32; p.stepsPerBeat = 8; setHits (p.kick, { 0, 12 }); setHits (p.snare, { 16 });
                 for (int s = 0; s < 24; s += 4) p.hat[(size_t) s] = (signed char) ((s / 4) % 2 == 0 ? 110 : 60);
                 p.hat[10] = 50;
                 for (int s = 24; s < 32; ++s) p.hat[(size_t) s] = (signed char) (40 + (s - 24) * 11);
                 break;
        case 8:  // six steps to the beat, so both straight eighths (every third step) and triplets (every second) fit
                 p.bpm = 140.0f; p.steps = 24; p.stepsPerBeat = 6; setHits (p.kick, { 0 }); setHits (p.snare, { 12 });
                 setHits (p.hat, { 0, 3, 6, 9 }, 85); setHits (p.hat, { 12, 14, 16, 18, 20, 22 }, 85); break;
        case 9:  p.bpm = 90.0f; break;
        case 10: p.bpm = 140.0f; p.steps = 32; gridHas808 = true; gridRows = 4;
                 setHits (p.kick, { 0, 6, 16, 22 }); setHits (p.snare, { 8, 24 }); every (p.hat, 0, 32, 2, 70);
                 for (int s : { 0, 6, 16, 22 }) { p.bass[(size_t) s] = 36; p.bassLen[(size_t) s] = (signed char) (s == 6 || s == 22 ? 9 : 5); }
                 break;
        default: p = backbone (90.0f); break;
    }
}

juce::String BeatScenes::gridCaption() const
{
    const auto bpm = juce::String (juce::roundToInt (pattern.bpm)) + " BPM";
    switch (variant)
    {
        case 0:  return "One bar, sixteen steps. The kick is on each beat: steps 1, 5, 9 and 13.";
        case 1:  return "The click is the beat. Click a row to hear that note length against it.";
        case 2:  return "The plainest beat: " + bpm + ". Snare on two and four.";
        case 3:  return "Boom bap, " + bpm + ", swing on.";
        case 4:  return "Trap, " + bpm + ", two bars. The snare is on beat three.";
        case 5:  return "Drill, " + bpm + ", two bars. Hats in threes and a two.";
        case 6:  return "R&B, " + bpm + ". Space is part of the pattern.";
        case 7:  return "One bar in thirty-second notes. Bars under the hats show how hard each hit is.";
        case 8:  return "One bar. Beats one and two are straight, three and four are triplets.";
        case 9:  return beatExerciseSolved() ? "That is a working beat. Now add kicks off the beat until it bounces."
                                             : "Needed: snare on beats 2 and 4 (steps 5 and 13), kick on beat 1, four or more hats.";
        case 10: return "Trap, " + bpm + ". The 808 starts with the kick and holds its note.";
        default: return bpm;
    }
}

bool BeatScenes::beatExerciseSolved() const
{
    int hats = 0;
    for (int s = 0; s < 16; ++s) if (pattern.hat[(size_t) s] > 0) ++hats;
    return pattern.snare[4] > 0 && pattern.snare[12] > 0 && pattern.kick[0] > 0 && hats >= 4;
}

void BeatScenes::paintGrid (juce::Graphics& g, juce::Rectangle<float> r)
{
    transport (g, r, gridCaption());
    const int steps = pattern.steps, perBeat = juce::jmax (1, pattern.stepsPerBeat), step = sound.currentStep();
    const float labelW = variant == 1 ? 124.0f : 78.0f;
    auto head = r.removeFromTop (20.0f);
    const bool velocities = variant == 7;
    auto foot = velocities ? r.removeFromBottom (46.0f) : juce::Rectangle<float>();
    const float cw = (r.getWidth() - labelW) / (float) steps;
    const float x0 = r.getX() + labelW;

    // the count above the steps
    g.setFont (body (steps > 16 ? 10.0f : 11.5f));
    for (int s = 0; s < steps; ++s)
    {
        const bool beat = s % perBeat == 0;
        juce::String label;
        if (beat) label = juce::String ((s / perBeat) % 4 + 1);
        else if (steps == 16 && perBeat == 4) label = (s % 4 == 1) ? "e" : (s % 4 == 2) ? "&" : "a";
        else if (perBeat == 4 && s % 4 == 2) label = "&";
        g.setColour (s == step ? yellow : (beat ? text : textMute));
        g.drawText (label, juce::Rectangle<float> (x0 + (float) s * cw, head.getY(), cw, head.getHeight()), juce::Justification::centred);
    }

    struct Row { const char* name; std::array<signed char, LessonPattern::kMaxSteps>* data; int sound; };
    std::vector<Row> rows;
    if (variant == 1) rows = { { "Quarter notes", nullptr, 0 }, { "Eighth notes", nullptr, 1 }, { "Sixteenth notes", nullptr, 2 } };
    else
    {
        rows = { { "Open hat", &pattern.openHat, 3 }, { "Hi-hat", &pattern.hat, 2 }, { "Snare", &pattern.snare, 1 }, { "Kick", &pattern.kick, 0 } };
        if (gridHas808) rows[0] = { "808", &pattern.bass, 5 };
        if (gridHas808) std::rotate (rows.begin(), rows.begin() + 1, rows.end());        // the 808 goes under the kick
    }
    const float rh = juce::jmin (58.0f, r.getHeight() / (float) rows.size());
    for (int row = 0; row < (int) rows.size(); ++row)
    {
        const float y = r.getY() + (float) row * rh;
        const bool chosen = variant == 1 && row == lengthRow;
        g.setFont (chosen ? bodyBold (12.0f) : body (12.0f)); g.setColour (chosen ? yellow : (variant == 1 ? textDim : text));
        const auto label = juce::Rectangle<float> (r.getX(), y, labelW, rh);
        g.drawText (rows[(size_t) row].name, label.withWidth (labelW + 40.0f), juce::Justification::centredLeft);
        if (variant == 1) add (juce::Rectangle<float> (r.getX(), y, r.getWidth(), rh), RowPick, row);
        for (int s = 0; s < steps; ++s)
        {
            const auto cell = juce::Rectangle<float> (x0 + (float) s * cw + 1.5f, y + 3.0f, cw - 3.0f, rh - 6.0f);
            bool on;
            if (variant == 1) on = s % (row == 0 ? 4 : row == 1 ? 2 : 1) == 0;
            else on = (*rows[(size_t) row].data)[(size_t) s] > 0;
            const bool beatGroup = (s / perBeat) % 2 == 0, now = s == step;
            if (variant == 1) g.setColour (on ? (chosen ? yellow : lineHi) : raised);
            else g.setColour (on ? (now ? text : yellow) : (now ? control.brighter (0.25f) : (beatGroup ? control : raised)));
            g.fillRoundedRectangle (cell, 3.0f);
            if (variant != 1) add (cell.expanded (1.5f, 3.0f), Cell, row, s);
        }
    }
    if (velocities)
    {
        g.setFont (body (11.0f)); g.setColour (textMute);
        g.drawText ("Velocity", foot.withWidth (labelW), juce::Justification::centredLeft);
        for (int s = 0; s < steps; ++s)
        {
            const int v = pattern.hat[(size_t) s];
            if (v <= 0) continue;
            const float h = (foot.getHeight() - 8.0f) * (float) v / 127.0f;
            g.setColour (s == step ? text : yellow.withAlpha (0.85f));
            g.fillRoundedRectangle (x0 + (float) s * cw + 3.0f, foot.getBottom() - h, cw - 6.0f, h, 2.0f);
        }
    }
    if (variant == 9 && beatExerciseSolved())
    {
        g.setColour (yellow); g.drawRoundedRectangle (juce::Rectangle<float> (x0 - 4.0f, r.getY() - 2.0f, cw * (float) steps + 8.0f, rh * (float) rows.size() + 4.0f), 6.0f, 2.0f);
    }
}
}
