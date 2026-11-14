#include "FXPage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

namespace
{
juce::String midiName (int note)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
}
constexpr int kRowH = 44, kRailW = 222;
}

FXPage::FXPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      dblVoices (p, ids::dblVoices, { "Double", "Stack" }, ids::dblVoices),
      dblMix (p, ids::dblMix, "Level", ids::dblMix), dblDetune (p, ids::dblDetune, "Detune", ids::dblDetune), dblTiming (p, ids::dblTiming, "Looseness", ids::dblTiming), dblWidth (p, ids::dblWidth, "Width", ids::dblWidth),
      dlSync (p, ids::dlSync, "Timed to tempo", "switch", ids::dlSync),
      dlNote (p, ids::dlNote, { "1/2", "1/4", "1/8 dotted", "1/8", "1/8 triplet", "1/16" }, ids::dlNote),
      dlMix (p, ids::dlMix, "Level", ids::dlMix), dlTime (p, ids::dlTime, "Time", ids::dlTime), dlFeedback (p, ids::dlFeedback, "Repeats", ids::dlFeedback),
      dlTone (p, ids::dlTone, "Tone", ids::dlTone), dlWidth (p, ids::dlWidth, "Ping-pong", ids::dlWidth), dlDuck (p, ids::dlDuck, "Duck", ids::dlDuck),
      rvType (p, ids::rvType, { "Room", "Plate", "Hall" }, ids::rvType),
      rvAuto (p, ids::rvAuto, "Timed to tempo", "switch", ids::rvAuto),
      rvLength (p, ids::rvLength, { "1 beat", "2 beats", "4 beats" }, ids::rvLength),
      rvMix (p, ids::rvMix, "Level", ids::rvMix), rvDecay (p, ids::rvDecay, "Decay", ids::rvDecay), rvPre (p, ids::rvPre, "Pre-delay", ids::rvPre),
      rvTone (p, ids::rvTone, "Tone", ids::rvTone), rvDuck (p, ids::rvDuck, "Duck", ids::rvDuck), rvLowCut (p, ids::rvLowCut, "Low cut", ids::rvLowCut), rvWidth (p, ids::rvWidth, "Width", ids::rvWidth),
      hmVoicing (p, ids::hmVoicing, { "Third up", "Fifth up", "Third + fifth", "Octave up", "Octave down", "Third below", "Choir" }, ids::hmVoicing),
      hmMix (p, ids::hmMix, "Level", ids::hmMix), hmWidth (p, ids::hmWidth, "Width", ids::hmWidth),
      psSemis (p, ids::psSemis, "Pitch", ids::psSemis), psCharacter (p, ids::psCharacter, "Voice size", ids::psCharacter), psMix (p, ids::psMix, "Mix", ids::psMix),
      phAmount (p, ids::phAmount, "Amount", ids::phAmount), phTone (p, ids::phTone, "Width", ids::phTone), phDrive (p, ids::phDrive, "Drive", ids::phDrive),
      lfCrush (p, ids::lfCrush, "Crush", ids::lfCrush), lfWobble (p, ids::lfWobble, "Wobble", ids::lfWobble), lfMix (p, ids::lfMix, "Mix", ids::lfMix),
      mdType (p, ids::mdType, { "Chorus", "Flanger", "Phaser" }, ids::mdType),
      mdRate (p, ids::mdRate, "Rate", ids::mdRate), mdDepth (p, ids::mdDepth, "Depth", ids::mdDepth), mdMix (p, ids::mdMix, "Mix", ids::mdMix),
      chRate (p, ids::chRate, { "1/4", "1/8", "1/8 triplet", "1/16", "1/32" }, ids::chRate),
      chDepth (p, ids::chDepth, "Depth", ids::chDepth), chShape (p, ids::chShape, "Hardness", ids::chShape)
{
    for (auto& m : fxModules())
    {
        auto* t = powers.add (new ParamToggle (p, m.onParam, {}, "power", m.onParam));
        addAndMakeVisible (t);
    }
    styleButton (rvFit, "Fit to this track", true, "rv.fit");
    rvFit.onClick = [this] { setStatus (proc.fitReverb()); };

    // the order matches fxModules(): doubler, delay, reverb, harmony, shift, phone, lo-fi, motion, chop
    ui = {
        { { &dblVoices }, { 190 }, { &dblMix, &dblDetune, &dblTiming, &dblWidth } },
        { { &dlSync, &dlNote }, { 158, 470 }, { &dlMix, &dlTime, &dlFeedback, &dlTone, &dlWidth, &dlDuck } },
        { { &rvType, &rvAuto, &rvLength, &rvFit }, { 200, 158, 220, 140 }, { &rvMix, &rvDecay, &rvPre, &rvTone, &rvDuck, &rvLowCut, &rvWidth } },
        { { &hmVoicing }, { 760 }, { &hmMix, &hmWidth } },
        { {}, {}, { &psSemis, &psCharacter, &psMix } },
        { {}, {}, { &phAmount, &phTone, &phDrive } },
        { {}, {}, { &lfCrush, &lfWobble, &lfMix } },
        { { &mdType }, { 260 }, { &mdRate, &mdDepth, &mdMix } },
        { { &chRate }, { 420 }, { &chDepth, &chShape } } };
    for (auto& m : ui)
    {
        for (auto* c : m.top) addChildComponent (c);
        for (auto* k : m.knobs) addChildComponent (k);
    }

    sounds.names = [] { juce::StringArray n; for (auto& s : soundPresets()) n.add (s.name); return n; };
    sounds.current = [this] { return proc.getSoundName(); };
    sounds.choose = [this] (int i) { setStatus (proc.applySound (i)); };
    presets.names = [this] { juce::StringArray n; for (int i : modulePresetsFor (fxModules()[(size_t) selected].id)) n.add (modulePresets()[(size_t) i].name); return n; };
    presets.current = [this] { return proc.getModulePresetName (fxModules()[(size_t) selected].id); };
    presets.choose = [this] (int i)
    {
        const auto list = modulePresetsFor (fxModules()[(size_t) selected].id);
        if (i >= 0 && i < (int) list.size()) setStatus (proc.applyModulePreset (list[(size_t) i]));
    };
    addAndMakeVisible (sounds); addAndMakeVisible (presets);
    select (juce::jlimit (0, (int) fxModules().size() - 1, (int) proc.apvts.state.getProperty ("fxModule", 0)));
    sendLookAndFeelChange();
}

void FXPage::select (int index)
{
    selected = juce::jlimit (0, (int) ui.size() - 1, index);
    proc.apvts.state.setProperty ("fxModule", selected, nullptr);
    for (size_t m = 0; m < ui.size(); ++m)
    {
        const bool show = (int) m == selected;
        for (auto* c : ui[m].top) c->setVisible (show);
        for (auto* k : ui[m].knobs) k->setVisible (show);
    }
    presets.refresh();
    resized();
    repaint();
}

juce::Rectangle<int> FXPage::rowBounds (int index) const { return { railList.getX(), railList.getY() + index * kRowH, railList.getWidth(), kRowH }; }

void FXPage::tick (float dt)
{
    sounds.refresh(); presets.refresh();
    const auto& d = proc.getDoubler();
    for (int v = 0; v < 4; ++v)
    {
        voiceCents[(size_t) v] = approach (voiceCents[(size_t) v], d.getVoiceOffsetCents (v), dt, 0.1f);
        voiceDelay[(size_t) v] = approach (voiceDelay[(size_t) v], d.getVoiceDelayMs (v), dt, 0.1f);
    }
    duck = approach (duck, proc.getReverb().getDuckDb(), dt, 0.06f);
    echoDuck = approach (echoDuck, proc.getEcho().getDuckDb(), dt, 0.06f);
    sweep = std::fmod (sweep + dt * 0.5f, 1.0f);
    const float sung = proc.getPitchFX().getSungNote();
    if (sung > 0.0f) harmonyCentre = approach (harmonyCentre, sung, dt, 0.6f);
    for (size_t i = 0; i < glow.size() && i < fxModules().size(); ++i)
        glow[i] = approach (glow[i], proc.getParam (fxModules()[i].onParam) > 0.5f ? 1.0f : 0.0f, dt, 0.08f);

    const bool timed = proc.getParam (ids::rvAuto) > 0.5f;
    rvDecay.setDimmed (timed); rvPre.setDimmed (timed);
    rvLength.setAlpha (timed ? 1.0f : 0.35f);
    const bool synced = proc.getParam (ids::dlSync) > 0.5f;
    dlTime.setDimmed (synced);
    dlNote.setAlpha (synced ? 1.0f : 0.35f);
    repaint (well);
    repaint (rail);
}

//==================================================================== drawing
void FXPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    fillPanel (g, rail.toFloat());
    fillPanel (g, panel.toFloat());

    // ---- rail
    auto r = rail.reduced (14, 0);
    g.setFont (bodyBold (13.0f)); g.setColour (text);
    g.drawText ("Sounds", r.removeFromTop (Section::kHeader), juce::Justification::centredLeft);
    r.removeFromTop (28 + 6);
    drawWrapped (g, "A sound sets several tools at once. Adjust each tool below.", r.removeFromTop (30).toFloat(), body (11.0f), textMute, 1.25f);

    const auto& modules = fxModules();
    for (int i = 0; i < (int) modules.size(); ++i)
    {
        const auto row = rowBounds (i);
        const bool on = proc.getParam (modules[(size_t) i].onParam) > 0.5f;
        if (i == selected) { g.setColour (raised); g.fillRoundedRectangle (row.toFloat().reduced (0.0f, 2.0f), 5.0f); g.setColour (yellow); g.fillRect (row.getX(), row.getY() + 8, 3, row.getHeight() - 16); }
        else if (i == hoverRow) { g.setColour (raised.withAlpha (0.5f)); g.fillRoundedRectangle (row.toFloat().reduced (0.0f, 2.0f), 5.0f); }
        if (i == 3) { g.setColour (line); g.fillRect (row.getX() + 10, row.getY() - 1, row.getWidth() - 20, 1); }      // space tools above, voice and colour tools below
        auto textArea = row.withTrimmedLeft (48).withTrimmedRight (8);
        g.setFont (bodyBold (13.5f)); g.setColour (on ? text : textDim);
        g.drawText (modules[(size_t) i].name, textArea.removeFromTop (row.getHeight() / 2 + 3), juce::Justification::bottomLeft);
        g.setFont (body (10.5f)); g.setColour (on ? yellow.withAlpha (0.4f + 0.6f * glow[(size_t) i]) : textMute);
        const auto loaded = proc.getModulePresetName (modules[(size_t) i].id);
        g.drawText (on ? (loaded.isEmpty() ? juce::String ("On") : loaded) : juce::String ("Off"), textArea, juce::Justification::topLeft, true);
    }
    if (! proc.isStereoOut())
    {
        g.setFont (body (10.5f)); g.setColour (textMute);
        g.drawFittedText ("This track is mono, so wide effects stay in the middle. Insert the plugin as Mono > Stereo to hear them spread.", monoNote, juce::Justification::topLeft, 4);
    }

    // ---- panel title
    const auto& m = modules[(size_t) selected];
    auto title = titleArea;
    g.setFont (headline (26.0f)); g.setColour (proc.getParam (m.onParam) > 0.5f ? text : textDim);
    const int nameW = juce::roundToInt (headline (26.0f).getStringWidthFloat (m.name.toUpperCase())) + 16;
    g.drawText (m.name.toUpperCase(), title.removeFromLeft (nameW), juce::Justification::centredLeft);
    g.setFont (body (12.5f)); g.setColour (textDim);
    g.drawText (m.blurb, title, juce::Justification::centredLeft, true);
}

void FXPage::paintOverChildren (juce::Graphics& g)
{
    const auto w = well.toFloat();
    switch (selected)
    {
        case 0: drawDoubles (g, w); break;
        case 1: drawEcho (g, w); break;
        case 2: drawReverb (g, w); break;
        case 3: drawHarmony (g, w); break;
        case 4: drawShift (g, w); break;
        case 5: drawPhone (g, w); break;
        case 6: drawLoFi (g, w); break;
        case 7: drawMotion (g, w); break;
        default: drawChop (g, w); break;
    }
}

void FXPage::drawDoubles (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::dblOn) > 0.5f;
    const int count = proc.getDoubler().getNumVoices();
    const auto inner = r.reduced (18.0f, 14.0f);
    const float midY = inner.getCentreY();
    g.setColour (line);
    g.fillRect (inner.getX(), midY, inner.getWidth(), 1.0f);
    g.fillRect (inner.getCentreX(), inner.getY(), 1.0f, inner.getHeight());
    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText ("Left", inner.withWidth (40.0f).withHeight (14.0f).withCentre ({ inner.getX() + 20.0f, midY - 12.0f }), juce::Justification::centredLeft);
    g.drawText ("Right", inner.withWidth (40.0f).withHeight (14.0f).withCentre ({ inner.getRight() - 20.0f, midY - 12.0f }), juce::Justification::centredRight);
    g.drawText ("sharper", inner.withHeight (14.0f).withTrimmedLeft (inner.getWidth() * 0.5f + 8.0f), juce::Justification::centredLeft);
    g.drawText ("flatter", inner.withTop (inner.getBottom() - 14.0f).withTrimmedLeft (inner.getWidth() * 0.5f + 8.0f), juce::Justification::centredLeft);

    // the lead stays in the middle; each copy sits where it is panned, higher or lower by its pitch offset
    g.setColour (text); g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ inner.getCentreX(), midY }));
    g.setFont (body (11.0f)); g.setColour (textDim);
    g.drawText ("the lead", juce::Rectangle<float> (80.0f, 14.0f).withCentre ({ inner.getCentreX(), midY + 20.0f }), juce::Justification::centred);
    const float level = proc.getParam (ids::dblMix) * 0.01f;
    for (int v = 0; v < count; ++v)
    {
        const float x = inner.getCentreX() + proc.getDoubler().getVoicePan (v) * (inner.getWidth() * 0.5f - 60.0f);
        const float y = midY - juce::jlimit (-1.0f, 1.0f, (on ? voiceCents[(size_t) v] : 0.0f) / 30.0f) * (inner.getHeight() * 0.5f - 20.0f);
        const float size = 10.0f + 14.0f * level;
        g.setColour (yellow.withAlpha (on ? 0.25f : 0.08f)); g.fillEllipse (juce::Rectangle<float> (size + 10.0f, size + 10.0f).withCentre ({ x, y }));
        g.setColour (on ? yellow : textMute); g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
        if (on)
        {
            g.setFont (body (10.5f)); g.setColour (textDim);
            g.drawText (juce::String (voiceCents[(size_t) v] > 0.0f ? "+" : "") + juce::String (juce::roundToInt (voiceCents[(size_t) v])) + " cents, " + juce::String (juce::roundToInt (voiceDelay[(size_t) v])) + " ms late",
                        juce::Rectangle<float> (150.0f, 14.0f).withCentre ({ x, y + size * 0.5f + 14.0f }), juce::Justification::centred);
        }
    }
}

void FXPage::drawEcho (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::dlOn) > 0.5f;
    const double bpm = proc.getHostBpm();
    const float beat = Reverb::beatMs (bpm), time = proc.getEcho().currentTimeMs();
    const float fb = proc.getParam (ids::dlFeedback) * 0.01f, width = proc.getParam (ids::dlWidth) * 0.01f;
    const auto plot = r.reduced (16.0f, 10.0f).withTrimmedTop (22.0f).withTrimmedBottom (16.0f);
    const float total = juce::jmax (beat * 4.0f, time * 4.4f);
    auto xFor = [&] (float ms) { return plot.getX() + plot.getWidth() * ms / total; };
    const float midY = plot.getCentreY();

    g.setFont (body (10.0f));
    for (int b = 0; (float) b * beat <= total; ++b)
    {
        const float x = xFor ((float) b * beat);
        g.setColour (b == 0 ? lineHi : line); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (textMute);
        if (b > 0) g.drawText ("beat " + juce::String (b + 1), juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f), juce::Justification::centred);
    }
    g.setColour (line); g.fillRect (plot.getX(), midY, plot.getWidth(), 1.0f);
    g.setColour (textMute); g.setFont (body (10.5f));
    g.drawText ("left", plot.withHeight (14.0f).translated (6.0f, 2.0f), juce::Justification::centredLeft);
    g.drawText ("right", plot.withTop (plot.getBottom() - 16.0f).translated (6.0f, 0.0f), juce::Justification::centredLeft);

    // the dry sound, then each echo: left ones above the line, right ones below, shorter as they fade
    g.setColour (text); g.fillRect (xFor (0.0f) - 1.5f, plot.getY() + 4.0f, 3.0f, plot.getHeight() - 8.0f);
    const float open = on ? juce::jlimit (0.3f, 1.0f, juce::Decibels::decibelsToGain (echoDuck)) : 0.35f;
    float gain = 1.0f;
    for (int k = 1; (float) k * time <= total && gain > 0.02f; ++k)
    {
        const bool leftSide = (k % 2) == 1;
        const float x = xFor ((float) k * time), h = (plot.getHeight() * 0.5f - 6.0f) * gain;
        // with ping-pong down, each echo appears on both sides
        const float mine = 1.0f, other = 1.0f - width;
        g.setColour (on ? yellow.withAlpha (0.35f + 0.65f * open) : textMute);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.0f, leftSide ? midY - h * mine : midY, 6.0f, h * mine), 2.0f);
        if (other > 0.02f) g.fillRoundedRectangle (juce::Rectangle<float> (x - 3.0f, leftSide ? midY : midY - h * other, 6.0f, h * other), 2.0f);
        gain *= fb;
        if (fb < 0.01f) break;
    }

    const bool synced = proc.getParam (ids::dlSync) > 0.5f;
    const juce::String sum = proc.hasHostTempo() ? "60,000 / " + juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM = " + juce::String (juce::roundToInt (beat)) + " ms a beat"
                                                 : juce::String ("No tempo from the host yet: using 120 BPM");
    const juce::String timing = synced ? Echo::noteNames()[(int) proc.getParam (ids::dlNote)] + " note = " + juce::String (juce::roundToInt (time)) + " ms between echoes"
                                       : juce::String (juce::roundToInt (time)) + " ms between echoes (" + juce::String (time / beat, 2) + " of a beat)";
    auto top = r.reduced (16.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (body (11.5f)); g.setColour (on ? textDim : textMute);
    g.drawText (timing, top.removeFromRight (top.getWidth() * 0.5f), juce::Justification::centredRight, true);
    g.drawText (sum, top, juce::Justification::centredLeft, true);
    if (on && echoDuck < -0.5f)
    {
        g.setColour (textMute);
        g.drawText ("stepping back " + juce::String (std::abs (echoDuck), 1) + " dB while the voice sings", plot.withTrimmedTop (2.0f).withHeight (14.0f), juce::Justification::centredRight);
    }
}

void FXPage::drawReverb (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::rvOn) > 0.5f;
    const double bpm = proc.getHostBpm();
    const float beat = Reverb::beatMs (bpm);
    const float pre = proc.getReverb().currentPreDelayMs(), decay = proc.getReverb().currentDecaySeconds() * 1000.0f;
    const int type = (int) proc.getParam (ids::rvType);
    const auto plot = r.reduced (16.0f, 10.0f).withTrimmedTop (22.0f).withTrimmedBottom (16.0f);
    const float total = juce::jmax (beat * 2.2f, (pre + decay) * 1.12f);
    auto xFor = [&] (float ms) { return plot.getX() + plot.getWidth() * ms / total; };

    // the beats of the song
    g.setFont (body (10.0f));
    for (int b = 0; (float) b * beat <= total; ++b)
    {
        const float x = xFor ((float) b * beat);
        g.setColour (b == 0 ? lineHi : line); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (textMute);
        if (b > 0) g.drawText ("beat " + juce::String (b + 1), juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f), juce::Justification::centred);
    }
    // the dry sound, the gap, the first reflections (strong in a room, absent on a plate), then the tail falling away
    g.setColour (text); g.fillRect (xFor (0.0f) - 1.5f, plot.getY() + 2.0f, 3.0f, plot.getHeight() - 2.0f);
    const float open = on ? juce::jlimit (0.25f, 1.0f, juce::Decibels::decibelsToGain (duck)) : 0.3f;
    static const float earlyMs[] = { 7.3f, 13.1f, 21.7f, 33.4f, 47.9f }, earlyGain[] = { 0.50f, 0.42f, 0.34f, 0.26f, 0.19f };
    const float earlyLevel = type == 0 ? 1.5f : type == 1 ? 0.0f : 0.6f, scale = type == 0 ? 0.55f : type == 2 ? 1.4f : 1.0f;
    g.setColour (on ? yellow.withAlpha (0.8f * open) : textMute);
    for (int e = 0; e < 5 && earlyLevel > 0.0f; ++e)
        g.fillRect (xFor (pre + earlyMs[e] * scale) - 1.0f, plot.getBottom() - plot.getHeight() * juce::jmin (0.95f, earlyGain[e] * earlyLevel), 2.0f, plot.getHeight() * juce::jmin (0.95f, earlyGain[e] * earlyLevel));
    juce::Path tail;
    const float x0 = xFor (pre), x1 = xFor (pre + decay);
    // a hall takes a moment to build; a room and a plate are at full strength at once
    const float build = type == 2 ? 0.07f : 0.015f;
    tail.startNewSubPath (x0, plot.getBottom());
    for (int i = 0; i <= 80; ++i)
    {
        const float f = (float) i / 80.0f;
        const float rise = juce::jlimit (0.0f, 1.0f, f / build);
        tail.lineTo (x0 + (x1 - x0) * f, plot.getBottom() - plot.getHeight() * 0.86f * rise * std::pow (1.0f - f, 2.2f));
    }
    tail.lineTo (x1, plot.getBottom());
    tail.closeSubPath();
    g.setColour (yellow.withAlpha (0.20f * open)); g.fillPath (tail);
    g.setColour (on ? yellow.withAlpha (0.35f + 0.65f * open) : textMute); g.strokePath (tail, juce::PathStrokeType (1.6f));

    // the maths, spelled out
    const bool timed = proc.getParam (ids::rvAuto) > 0.5f;
    const int beats = (int) proc.getParam (ids::rvLength) == 0 ? 1 : ((int) proc.getParam (ids::rvLength) == 1 ? 2 : 4);
    const juce::String sum = proc.hasHostTempo() ? "60,000 / " + juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM = " + juce::String (juce::roundToInt (beat)) + " ms a beat"
                                                 : juce::String ("No tempo from the host yet: using 120 BPM");
    const juce::String timing = Reverb::typeNames()[type] + ": pre-delay " + juce::String (juce::roundToInt (pre)) + " ms" + (timed ? " (1/64)" : "") + ", decay " + juce::String (decay * 0.001f, 2) + " s"
                              + (timed ? " (" + juce::String (beats) + (beats == 1 ? " beat)" : " beats)") : juce::String());
    auto top = r.reduced (16.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (body (11.5f)); g.setColour (on ? textDim : textMute);
    g.drawText (timing, top.removeFromRight (top.getWidth() * 0.55f), juce::Justification::centredRight, true);
    g.drawText (sum, top, juce::Justification::centredLeft, true);
    if (on && duck < -0.5f)
    {
        g.setColour (textMute);
        g.drawText ("stepping back " + juce::String (std::abs (duck), 1) + " dB while the voice sings", plot.withTrimmedTop (2.0f).withHeight (14.0f), juce::Justification::centredRight);
    }
}

void FXPage::drawHarmony (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::hmOn) > 0.5f;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = (int) proc.getParam (ids::tuneNotes), voicing = (int) proc.getParam (ids::hmVoicing);
    const float span = 30.0f, low = harmonyCentre - span * 0.5f;
    const auto plot = r.reduced (1.0f).withTrimmedTop (24.0f);
    auto yFor = [&] (float note) { return plot.getBottom() - (note - low) / span * plot.getHeight(); };
    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    const float rowH = plot.getHeight() / span;
    for (int n = (int) std::floor (low) - 1; n <= (int) std::ceil (low + span) + 1; ++n)
    {
        const bool allowed = Tuner::noteAllowed (n, key, sc, mask);
        const auto row = juce::Rectangle<float> (plot.getX(), yFor ((float) n) - rowH * 0.5f, plot.getWidth(), rowH);
        if (allowed) { g.setColour (raised.withAlpha (0.7f)); g.fillRect (row.reduced (0.0f, 0.5f)); }
        if (n % 12 == key) { g.setFont (body (9.5f)); g.setColour (textDim); g.drawText (midiName (n), row.withWidth (40.0f).translated (8.0f, 0.0f), juce::Justification::centredLeft); }
    }
    // the note being sung, and the note each harmony voice takes
    const float sung = proc.getPitchFX().getSungNote();
    const float sungHere = sung > 0.0f ? sung : 0.0f;
    const int lead = sungHere > 0.0f ? Tuner::nearestAllowed (sungHere, key, sc, mask) : (int) std::lround (harmonyCentre);
    const float cx = plot.getCentreX();
    auto dot = [&] (float note, juce::Colour colour, float size, const juce::String& label, float xOffset)
    {
        const auto c = juce::Point<float> (cx + xOffset, yFor (note));
        g.setColour (colour.withAlpha (0.2f)); g.fillEllipse (juce::Rectangle<float> (size + 12.0f, size + 12.0f).withCentre (c));
        g.setColour (colour); g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
        g.setFont (bodyBold (12.0f));
        g.drawText (label, juce::Rectangle<float> (150.0f, 16.0f).withCentre ({ c.x + (xOffset >= 0.0f ? 90.0f : -90.0f), c.y }), xOffset >= 0.0f ? juce::Justification::centredLeft : juce::Justification::centredRight);
    };
    const int voices = PitchFX::numVoices (voicing);
    static const float offsets[] = { 110.0f, 190.0f, 150.0f };
    for (int v = 0; v < voices; ++v)
    {
        const int note = PitchFX::harmonyNote (lead, v, voicing, key, sc, mask);
        if (note >= 0) dot ((float) note, on ? yellow : textMute, 14.0f, midiName (note), offsets[v]);
    }
    dot (sungHere > 0.0f ? sungHere : (float) lead, sungHere > 0.0f || ! on ? text : textDim, 16.0f, "the lead, " + midiName (lead), -110.0f);
    g.restoreState();

    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto top = r.reduced (16.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText (sc == 0 ? juce::String ("No key chosen (Chromatic): harmonies use fixed distances. Set the key on the Tune tab so they follow the song.")
                        : "Harmonies follow the key on the Tune tab: " + juce::String (names[key]) + " " + Tuner::scaleNames()[sc].toLowerCase() + ". Lit rows are notes of the key.",
                top, juce::Justification::centredLeft, true);
}

void FXPage::drawShift (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::psOn) > 0.5f;
    const float semis = proc.getParam (ids::psSemis), character = proc.getParam (ids::psCharacter) * 0.01f, mix = proc.getParam (ids::psMix) * 0.01f;
    const auto plot = r.reduced (40.0f, 24.0f);
    const float midX = plot.getCentreX(), y = plot.getCentreY();
    auto xFor = [&] (float st) { return midX + st / 12.0f * (plot.getWidth() * 0.5f - 30.0f); };
    g.setColour (line); g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
    g.setFont (body (10.5f));
    for (int st = -12; st <= 12; ++st)
    {
        const float x = xFor ((float) st);
        const bool big = st % 12 == 0, mid = st == -7 || st == -5 || st == 5 || st == 7;
        g.setColour (big ? lineHi : line); g.fillRect (x - 0.5f, y - (big ? 14.0f : mid ? 9.0f : 5.0f), 1.0f, big ? 28.0f : mid ? 18.0f : 10.0f);
        if (big || mid) { g.setColour (textMute); g.drawText (st == 0 ? juce::String ("as sung") : st == -12 ? juce::String ("octave down") : st == 12 ? juce::String ("octave up") : juce::String (st > 0 ? "+" : "") + juce::String (st),
                                                               juce::Rectangle<float> (90.0f, 14.0f).withCentre ({ x, y + 30.0f }), juce::Justification::centred); }
    }
    // the real voice, and the shifted one: bigger circle = bigger, darker voice
    const float base = 34.0f;
    g.setColour (text.withAlpha (on ? juce::jmax (0.15f, 1.0f - mix) : 1.0f)); g.fillEllipse (juce::Rectangle<float> (base, base).withCentre ({ xFor (0.0f), y - 52.0f }));
    const float size = base * std::pow (2.0f, -character * 0.9f);
    const auto c = juce::Point<float> (xFor (semis), y - 52.0f);
    g.setColour (yellow.withAlpha (on ? 0.22f : 0.06f)); g.fillEllipse (juce::Rectangle<float> (size + 14.0f, size + 14.0f).withCentre (c));
    g.setColour (on ? yellow.withAlpha (juce::jmax (0.35f, mix)) : textMute); g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
    g.setFont (bodyBold (12.5f)); g.setColour (on ? yellow : textMute);
    g.drawText (proc.getParamText (ids::psSemis) + ", voice " + proc.getParamText (ids::psCharacter), juce::Rectangle<float> (260.0f, 16.0f).withCentre ({ juce::jlimit (plot.getX() + 130.0f, plot.getRight() - 130.0f, c.x), y - 52.0f - juce::jmax (size, base) * 0.5f - 16.0f }), juce::Justification::centred);
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText (mix > 0.98f ? "Mix at 100 %: only the shifted voice is heard." : "Mix at " + juce::String (juce::roundToInt (mix * 100.0f)) + " %: the shifted voice is layered under the real one.",
                r.reduced (16.0f, 8.0f).removeFromBottom (16.0f), juce::Justification::centredLeft);
}

void FXPage::drawPhone (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::phOn) > 0.5f;
    const auto plot = r.reduced (1.0f);
    drawFreqGrid (g, plot);
    const float tone = proc.getParam (ids::phTone) * 0.01f;
    // the same filters the sound goes through, drawn from 20 Hz to 20 kHz
    ColourBiquad hp1, hp2, lp1, lp2, nose;
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    const float lo = Phone::lowEdgeHz (tone), hi = Phone::highEdgeHz (tone);
    hp1.highPass (lo, 0.5412f, fs); hp2.highPass (lo, 1.3066f, fs); lp1.lowPass (hi, 0.5412f, fs); lp2.lowPass (hi, 1.3066f, fs); nose.peak (std::sqrt (lo * hi) * 1.25f, 1.1f, 5.0f, fs);
    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
        const float db = hp1.magnitudeDb (hz, fs) + hp2.magnitudeDb (hz, fs) + lp1.magnitudeDb (hz, fs) + lp2.magnitudeDb (hz, fs) + nose.magnitudeDb (hz, fs);
        const float x = freqToX (hz, plot.getX(), plot.getWidth()), y = plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, (12.0f - db) / 60.0f);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.saveState(); g.reduceClipRegion (plot.toNearestInt());
    g.setColour (yellow.withAlpha (on ? 0.14f : 0.04f)); g.fillPath (fill);
    g.setColour (on ? yellow : textMute); g.strokePath (curve, juce::PathStrokeType (2.0f));
    g.restoreState();
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText ("What gets through: " + hzString (lo) + " to " + hzString (hi) + ". Everything outside that is what a small speaker cannot play.", r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
}

void FXPage::drawLoFi (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::lfOn) > 0.5f;
    const float crush = proc.getParam (ids::lfCrush) * 0.01f, wobble = proc.getParam (ids::lfWobble) * 0.01f;
    const auto plot = r.reduced (24.0f, 30.0f);
    const float midY = plot.getCentreY(), amp = plot.getHeight() * 0.42f;
    g.setColour (line); g.fillRect (plot.getX(), midY, plot.getWidth(), 1.0f);
    // a clean wave in grey, and the same wave as the machine stores it: fewer points in time, fewer steps of level,
    // drifting slowly in pitch
    const float drift = wobble * 0.18f * std::sin (sweep * juce::MathConstants<float>::twoPi);
    auto wave = [&] (float x01, float bend) { return std::sin (juce::MathConstants<float>::twoPi * 3.0f * (x01 * (1.0f + bend))) * 0.75f + std::sin (juce::MathConstants<float>::twoPi * 7.0f * x01 * (1.0f + bend)) * 0.2f; };
    juce::Path clean, stored;
    for (int i = 0; i <= 300; ++i)
    {
        const float f = (float) i / 300.0f;
        const float x = plot.getX() + plot.getWidth() * f, y = midY - amp * wave (f, 0.0f);
        if (i == 0) clean.startNewSubPath (x, y); else clean.lineTo (x, y);
    }
    const int points = juce::jmax (8, juce::roundToInt (300.0f * std::pow (0.06f, crush)));
    const float levels = juce::jmax (3.0f, std::pow (2.0f, 7.0f - 5.0f * crush));
    float lastY = midY;
    for (int i = 0; i <= points; ++i)
    {
        const float f = (float) i / (float) points;
        const float x = plot.getX() + plot.getWidth() * f, y = midY - amp * std::round (wave (f, drift) * levels) / levels;
        if (i == 0) stored.startNewSubPath (x, y); else { stored.lineTo (x, lastY); stored.lineTo (x, y); }
        lastY = y;
    }
    g.setColour (textMute); g.strokePath (clean, juce::PathStrokeType (1.2f));
    g.setColour (on ? yellow : textMute.brighter (0.3f)); g.strokePath (stored, juce::PathStrokeType (2.0f));
    g.setFont (body (11.5f)); g.setColour (textDim);
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    g.drawText ("Stored at " + hzString (LoFi::rateHz (crush, fs)) + " with " + juce::String (LoFi::bits (crush), 0) + "-bit steps" + (wobble > 0.01f ? ", pitch drifting " + juce::String (juce::roundToInt (wobble * 30.0f)) + " cents either way" : juce::String()) + ".",
                r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
    g.setColour (textMute);
    g.drawText ("grey: the sound going in    yellow: as the machine stores it", r.reduced (16.0f, 8.0f).removeFromBottom (14.0f), juce::Justification::centredRight);
}

void FXPage::drawMotion (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::mdOn) > 0.5f;
    const int type = (int) proc.getParam (ids::mdType);
    const float depth = proc.getParam (ids::mdDepth) * 0.01f;
    const float phase = proc.getMotion().getPhase() * juce::MathConstants<float>::twoPi;
    const auto plot = r.reduced (40.0f, 34.0f);
    static const char* const what[] = { "Chorus: two copies of the sound whose timing drifts, one a little ahead on the left, one on the right.",
                                        "Flanger: a very short echo that sweeps from nothing to a few milliseconds and back, with some of it fed round again.",
                                        "Phaser: notches that sweep up and down through the tone. No echo, just a moving colour." };
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText (what[juce::jlimit (0, 2, type)], r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);

    // two lanes, left and right; the marker on each shows where its sweep is right now
    static const char* const loLabel[] = { "10 ms", "0.3 ms", "300 Hz" }, * const hiLabel[] = { "22 ms", "5 ms", "3 kHz" };
    for (int c = 0; c < 2; ++c)
    {
        const float y = plot.getY() + plot.getHeight() * (c == 0 ? 0.33f : 0.67f);
        g.setColour (control); g.fillRoundedRectangle (plot.getX(), y - 3.0f, plot.getWidth(), 6.0f, 3.0f);
        const float angle = phase + (c == 1 ? juce::MathConstants<float>::halfPi : 0.0f);
        const float pos = 0.5f + 0.5f * std::sin (angle) * (0.15f + 0.85f * depth);
        const float lo = 0.5f - 0.5f * (0.15f + 0.85f * depth), hi = 0.5f + 0.5f * (0.15f + 0.85f * depth);
        g.setColour (yellow.withAlpha (on ? 0.22f : 0.08f));
        g.fillRoundedRectangle (plot.getX() + plot.getWidth() * lo, y - 3.0f, plot.getWidth() * (hi - lo), 6.0f, 3.0f);
        g.setColour (on ? yellow : textMute);
        g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ plot.getX() + plot.getWidth() * (on ? pos : 0.5f), y }));
        g.setFont (body (11.0f)); g.setColour (textMute);
        g.drawText (c == 0 ? "left" : "right", juce::Rectangle<float> (plot.getX() - 34.0f, y - 8.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
    }
    g.setFont (body (10.5f)); g.setColour (textMute);
    g.drawText (loLabel[juce::jlimit (0, 2, type)], plot.withTop (plot.getBottom() - 12.0f), juce::Justification::centredLeft);
    g.drawText (hiLabel[juce::jlimit (0, 2, type)], plot.withTop (plot.getBottom() - 12.0f), juce::Justification::centredRight);
}

void FXPage::drawChop (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::chOn) > 0.5f;
    const int rate = (int) proc.getParam (ids::chRate);
    const float depth = proc.getParam (ids::chDepth) * 0.01f, shape = proc.getParam (ids::chShape) * 0.01f;
    const auto plot = r.reduced (24.0f, 12.0f).withTrimmedTop (22.0f).withTrimmedBottom (16.0f);
    const float beats = Chop::rateBeats (rate);
    const float steps = 4.0f / beats;                          // one bar of four beats
    // beat lines, then the level across one bar
    g.setFont (body (10.0f));
    for (int b = 0; b <= 4; ++b)
    {
        const float x = plot.getX() + plot.getWidth() * (float) b / 4.0f;
        g.setColour (b == 0 || b == 4 ? lineHi : line); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (textMute);
        if (b < 4) g.drawText ("beat " + juce::String (b + 1), juce::Rectangle<float> (x + 4.0f, plot.getBottom() + 2.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
    }
    juce::Path level;
    level.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i <= 600; ++i)
    {
        const float f = (float) i / 600.0f;
        const float p = std::fmod (f * steps, 1.0f);
        const float gain = 1.0f - depth * (1.0f - Chop::gateAt (p, shape));
        level.lineTo (plot.getX() + plot.getWidth() * f, plot.getBottom() - plot.getHeight() * 0.9f * gain);
    }
    level.lineTo (plot.getRight(), plot.getBottom());
    level.closeSubPath();
    g.setColour (yellow.withAlpha (on ? 0.22f : 0.06f)); g.fillPath (level);
    g.setColour (on ? yellow : textMute); g.strokePath (level, juce::PathStrokeType (1.5f));
    g.setFont (body (11.5f)); g.setColour (textDim);
    const float stepMs = Reverb::beatMs (proc.getHostBpm()) * beats;
    g.drawText ("One bar. Each step is a " + Chop::rateNames()[rate] + " note, " + juce::String (juce::roundToInt (stepMs)) + " ms at " + juce::String (juce::roundToInt (proc.getHostBpm())) + " BPM: on for the first half, down for the second.",
                r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
}

//==================================================================== interaction and layout
void FXPage::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) fxModules().size(); ++i)
        if (rowBounds (i).contains (e.getPosition()))
        {
            select (i);
            setStatus (fxModules()[(size_t) i].name + ": " + fxModules()[(size_t) i].blurb);
            return;
        }
}

void FXPage::mouseMove (const juce::MouseEvent& e)
{
    juce::String key;
    int row = -1;
    for (int i = 0; i < (int) fxModules().size(); ++i) if (rowBounds (i).contains (e.getPosition())) row = i;
    if (row != hoverRow) { hoverRow = row; repaint (rail); }
    if (row >= 0) key = fxModules()[(size_t) row].onParam;
    else if (well.contains (e.getPosition())) key = selected == 2 ? juce::String ("rv.display") : selected == 1 ? juce::String ("dl.display") : fxModules()[(size_t) selected].onParam;
    getProperties().set ("help", key);
    setMouseCursor (row >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void FXPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    rail = r.removeFromLeft (kRailW);
    r.removeFromLeft (10);
    panel = r;

    auto left = rail.reduced (10, 0);
    left.removeFromTop (Section::kHeader);
    sounds.setBounds (left.removeFromTop (28).reduced (4, 0));
    left.removeFromTop (6 + 30 + 8);
    railList = left.removeFromTop (kRowH * (int) fxModules().size());
    left.removeFromTop (10);
    monoNote = left.reduced (4, 0).withTrimmedBottom (8);
    for (int i = 0; i < powers.size(); ++i)
        powers[i]->setBounds (rowBounds (i).removeFromLeft (44).withSizeKeepingCentre (24, 24));

    auto p = panel.reduced (16, 0);
    auto head = p.removeFromTop (58);
    presets.setBounds (head.removeFromRight (280).withSizeKeepingCentre (280, 26));
    head.removeFromRight (12);
    titleArea = head;
    p.removeFromBottom (14);
    const auto& m = ui[(size_t) selected];
    auto knobs = p.removeFromBottom (96);
    p.removeFromBottom (10);
    if (! m.top.empty())
    {
        auto top = p.removeFromTop (28);
        for (size_t i = 0; i < m.top.size(); ++i)
        {
            m.top[i]->setBounds (top.removeFromLeft (juce::jmin (m.topWidths[i], top.getWidth())));
            top.removeFromLeft (12);
        }
        p.removeFromTop (10);
    }
    well = p;
    const int count = (int) m.knobs.size();
    const int cell = juce::jmin (118, (knobs.getWidth() - (count - 1) * 8) / juce::jmax (1, count));
    auto row = knobs.withSizeKeepingCentre (cell * count + 8 * (count - 1), knobs.getHeight());
    for (auto* k : m.knobs) { k->setBounds (row.removeFromLeft (cell)); row.removeFromLeft (8); }
}
}
