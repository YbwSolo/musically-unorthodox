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
}

FXPage::FXPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      scale (p, ids::tuneScale, { "Chromatic", "Major", "Minor", "Maj pent", "Min pent" }, ids::tuneScale),
      tuneSpeed (p, ids::tuneSpeed, "Speed", ids::tuneSpeed), tuneAmount (p, ids::tuneAmount, "Amount", ids::tuneAmount), tuneVibrato (p, ids::tuneVibrato, "Keep vibrato", ids::tuneVibrato),
      voices (p, ids::dblVoices, { "Double", "Stack" }, ids::dblVoices),
      dblMix (p, ids::dblMix, "Level", ids::dblMix), dblDetune (p, ids::dblDetune, "Detune", ids::dblDetune), dblTiming (p, ids::dblTiming, "Looseness", ids::dblTiming), dblWidth (p, ids::dblWidth, "Width", ids::dblWidth),
      rvAuto (p, ids::rvAuto, "Timed to tempo", "switch", ids::rvAuto),
      rvLength (p, ids::rvLength, { "1 beat", "2 beats", "4 beats" }, ids::rvLength),
      rvMix (p, ids::rvMix, "Level", ids::rvMix), rvDecay (p, ids::rvDecay, "Decay", ids::rvDecay), rvPre (p, ids::rvPre, "Pre-delay", ids::rvPre),
      rvTone (p, ids::rvTone, "Tone", ids::rvTone), rvDuck (p, ids::rvDuck, "Duck", ids::rvDuck)
{
    tune.setPower (p, ids::tuneOn); dbl.setPower (p, ids::dblOn); verb.setPower (p, ids::rvOn);
    setHelp (tune, "tuneOn"); setHelp (dbl, "dblOn"); setHelp (verb, "rvOn");

    keyBox.addItemList (Tuner::keyNames(), 1);
    keyBox.setWantsKeyboardFocus (false);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::tuneKey, keyBox);
    setHelp (keyBox, "tuneKey");

    styleButton (findKey, "Find key", false, "tune.find");
    findKey.onClick = [this] { setStatus (proc.findKey()); };

    // Styles: each one sets the controls that make up a sound, then leaves them free to adjust.
    auto styled = [this] (juce::TextButton& b, const juce::String& text, const juce::String& help, std::initializer_list<std::pair<juce::String, float>> values, const juce::String& what)
    {
        styleButton (b, text, false, help);
        std::vector<std::pair<juce::String, float>> copy (values);
        b.onClick = [this, copy, what] { for (auto& v : copy) proc.setParam (v.first, v.second); setStatus (what); };
    };
    styled (tuneNatural, "Natural", "tuneOn", { { ids::tuneOn, 1 }, { ids::tuneSpeed, 70 }, { ids::tuneVibrato, 85 }, { ids::tuneAmount, 100 } },
            "Natural tune: the centre of each note is corrected and the vibrato and slides are left as they were sung.");
    styled (tuneTight, "Tight", "tuneOn", { { ids::tuneOn, 1 }, { ids::tuneSpeed, 18 }, { ids::tuneVibrato, 35 }, { ids::tuneAmount, 100 } },
            "Tight tune: notes are pulled in quickly for a polished, modern vocal that still moves like a voice.");
    styled (tuneHard, "Hard tune", "tuneOn", { { ids::tuneOn, 1 }, { ids::tuneSpeed, 0 }, { ids::tuneVibrato, 0 }, { ids::tuneAmount, 100 } },
            "Hard tune: every note snaps onto the scale. Set the key and choose Major or Minor, or it will snap to the wrong notes.");
    styled (dblSubtle, "Subtle", "dblOn", { { ids::dblOn, 1 }, { ids::dblVoices, 0 }, { ids::dblMix, 30 }, { ids::dblDetune, 6 }, { ids::dblTiming, 25 }, { ids::dblWidth, 100 } },
            "Subtle double: a tight copy each side that thickens the lead without being heard as an effect.");
    styled (dblWide, "Wide", "dblOn", { { ids::dblOn, 1 }, { ids::dblVoices, 0 }, { ids::dblMix, 48 }, { ids::dblDetune, 12 }, { ids::dblTiming, 50 }, { ids::dblWidth, 100 } },
            "Wide double: looser, more detuned copies hard left and right.");
    styled (dblStack, "Hook stack", "dblOn", { { ids::dblOn, 1 }, { ids::dblVoices, 1 }, { ids::dblMix, 60 }, { ids::dblDetune, 11 }, { ids::dblTiming, 60 }, { ids::dblWidth, 100 } },
            "Hook stack: four extra voices, two each side, so one take sounds like a stacked hook.");
    styled (rvTight, "Tight", "rvOn", { { ids::rvOn, 1 }, { ids::rvAuto, 1 }, { ids::rvLength, 0 }, { ids::rvMix, 12 }, { ids::rvTone, 45 }, { ids::rvDuck, 70 } },
            "Tight reverb: one beat long and ducked hard, for rap verses that should stay dry and upfront.");
    styled (rvPlate, "Vocal plate", "rvOn", { { ids::rvOn, 1 }, { ids::rvAuto, 1 }, { ids::rvLength, 1 }, { ids::rvMix, 18 }, { ids::rvTone, 60 }, { ids::rvDuck, 50 } },
            "Vocal plate: two beats long with a bright top, the all-round setting for sung vocals.");
    styled (rvBig, "Big hall", "rvOn", { { ids::rvOn, 1 }, { ids::rvAuto, 1 }, { ids::rvLength, 2 }, { ids::rvMix, 26 }, { ids::rvTone, 40 }, { ids::rvDuck, 45 } },
            "Big hall: four beats long and darker, for hooks, intros and slow songs.");

    for (auto* c : std::initializer_list<juce::Component*> { &tune, &dbl, &verb, &keyBox, &scale, &findKey, &tuneNatural, &tuneTight, &tuneHard, &tuneSpeed, &tuneAmount, &tuneVibrato,
                                                              &voices, &dblMix, &dblDetune, &dblTiming, &dblWidth, &dblSubtle, &dblWide, &dblStack,
                                                              &rvAuto, &rvLength, &rvMix, &rvDecay, &rvPre, &rvTone, &rvDuck, &rvTight, &rvPlate, &rvBig })
        addAndMakeVisible (c);
    sendLookAndFeelChange();
}

void FXPage::tick (float dt)
{
    auto& t = proc.getTuner();
    frame += dt;
    while (frame >= 1.0f / 60.0f)
    {
        frame -= 1.0f / 60.0f;
        sung[(size_t) head] = t.getDetectedNote();
        tuned[(size_t) head] = t.getOutputNote();
        head = (head + 1) % kHistory;
    }
    const float out = t.getOutputNote();
    if (out > 0.0f)
    {
        centre = approach (centre, out, dt, 0.5f);
        correction = approach (correction, (out - t.getDetectedNote()) * 100.0f, dt, 0.08f);
        shownTarget = t.getTargetNote();
    }
    else correction = approach (correction, 0.0f, dt, 0.2f);

    const auto& d = proc.getDoubler();
    for (int v = 0; v < 4; ++v)
    {
        voiceCents[(size_t) v] = approach (voiceCents[(size_t) v], d.getVoiceOffsetCents (v), dt, 0.1f);
        voiceDelay[(size_t) v] = approach (voiceDelay[(size_t) v], d.getVoiceDelayMs (v), dt, 0.1f);
    }
    duck = approach (duck, proc.getReverb().getDuckDb(), dt, 0.06f);
    sweep = std::fmod (sweep + dt * 0.5f, 1.0f);

    const bool timed = proc.getParam (ids::rvAuto) > 0.5f;
    rvDecay.setDimmed (timed); rvPre.setDimmed (timed);
    rvLength.setAlpha (timed ? 1.0f : 0.35f);
    for (auto* r : { &pitchWell, &dblWell, &verbWell }) repaint (*r);
}

//==================================================================== drawing
void FXPage::drawPitch (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::tuneOn) > 0.5f;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale);
    const float span = 9.0f;                                    // semitones shown
    const float low = centre - span * 0.5f;
    auto yFor = [&] (float note) { return r.getBottom() - (note - low) / span * r.getHeight(); };
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());

    // one row per note; the rows of the chosen scale are the ones a voice can land on
    const float rowH = r.getHeight() / span;
    for (int n = (int) std::floor (low) - 1; n <= (int) std::ceil (low + span) + 1; ++n)
    {
        const bool allowed = Tuner::noteAllowed (n, key, sc);
        const auto row = juce::Rectangle<float> (r.getX(), yFor ((float) n) - rowH * 0.5f, r.getWidth(), rowH);
        if (allowed) { g.setColour (n == shownTarget && on ? yellow.withAlpha (0.10f) : raised.withAlpha (0.75f)); g.fillRect (row.reduced (0.0f, 1.0f)); }
        g.setColour (line); g.fillRect (r.getX(), yFor ((float) n), r.getWidth(), 1.0f);
        g.setFont (body (10.5f)); g.setColour (allowed ? textDim : textMute.withAlpha (0.6f));
        g.drawText (midiName (n), row.withWidth (34.0f).translated (6.0f, 0.0f), juce::Justification::centredLeft);
    }

    auto trace = [&] (const std::array<float, kHistory>& h, juce::Colour colour, float thickness)
    {
        juce::Path path; bool open = false;
        for (int i = 0; i < kHistory; ++i)
        {
            const float note = h[(size_t) ((head + i) % kHistory)];
            if (note <= 0.0f) { open = false; continue; }
            const float x = r.getX() + 40.0f + (r.getWidth() - 46.0f) * (float) i / (float) (kHistory - 1), y = yFor (note);
            if (open) path.lineTo (x, y); else { path.startNewSubPath (x, y); open = true; }
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    trace (sung, textDim.withAlpha (0.8f), 1.3f);
    if (on) trace (tuned, yellow, 2.2f);
    g.restoreState();

    // the note the voice is being moved to, and by how much
    if (on && shownTarget >= 0 && proc.getTuner().getOutputNote() > 0.0f)
    {
        auto corner = r.reduced (12.0f, 8.0f).removeFromTop (44.0f).removeFromRight (130.0f);
        g.setFont (headline (26.0f)); g.setColour (yellow);
        g.drawText (midiName (shownTarget), corner.removeFromTop (28.0f), juce::Justification::centredRight);
        g.setFont (body (11.0f)); g.setColour (textDim);
        const int cents = juce::roundToInt (correction);
        g.drawText (cents == 0 ? juce::String ("in tune") : "moved " + juce::String (cents > 0 ? "up " : "down ") + juce::String (std::abs (cents)) + " cents", corner, juce::Justification::centredRight);
    }
    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText (on ? "Grey: as sung    Yellow: after Tune" : "Switch Tune on to see the pitch as it is sung.", r.reduced (10.0f, 6.0f).removeFromBottom (14.0f), juce::Justification::centredRight);
}

void FXPage::drawDoubles (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::dblOn) > 0.5f;
    const int count = proc.getDoubler().getNumVoices();
    const auto inner = r.reduced (14.0f, 12.0f);
    const float midY = inner.getCentreY();
    g.setColour (line);
    g.fillRect (inner.getX(), midY, inner.getWidth(), 1.0f);
    g.fillRect (inner.getCentreX(), inner.getY(), 1.0f, inner.getHeight());
    g.setFont (body (10.5f)); g.setColour (textMute);
    g.drawText ("L", inner.withWidth (12.0f), juce::Justification::centredTop);
    g.drawText ("R", inner.withLeft (inner.getRight() - 12.0f), juce::Justification::centredTop);
    g.drawText ("sharper", inner.withHeight (12.0f).withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    g.drawText ("flatter", inner.withTop (inner.getBottom() - 12.0f).withTrimmedLeft (18.0f), juce::Justification::centredLeft);

    // the lead stays in the middle; each copy sits where it is panned, higher or lower by its pitch offset
    g.setColour (text); g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ inner.getCentreX(), midY }));
    const float level = proc.getParam (ids::dblMix) * 0.01f;
    for (int v = 0; v < count; ++v)
    {
        const float x = inner.getCentreX() + proc.getDoubler().getVoicePan (v) * (inner.getWidth() * 0.5f - 14.0f);
        const float y = midY - juce::jlimit (-1.0f, 1.0f, (on ? voiceCents[(size_t) v] : 0.0f) / 30.0f) * (inner.getHeight() * 0.5f - 10.0f);
        const float size = 7.0f + 9.0f * level;
        g.setColour (yellow.withAlpha (on ? 0.25f : 0.08f)); g.fillEllipse (juce::Rectangle<float> (size + 8.0f, size + 8.0f).withCentre ({ x, y }));
        g.setColour (on ? yellow : textMute); g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
    }
}

void FXPage::drawReverb (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::rvOn) > 0.5f;
    const double bpm = proc.getHostBpm();
    const float beat = Reverb::beatMs (bpm);
    const float pre = proc.getReverb().currentPreDelayMs(), decay = proc.getReverb().currentDecaySeconds() * 1000.0f;
    const auto plot = r.reduced (12.0f, 8.0f).withTrimmedTop (20.0f).withTrimmedBottom (14.0f);
    const float total = juce::jmax (beat * 2.2f, (pre + decay) * 1.12f);
    auto xFor = [&] (float ms) { return plot.getX() + plot.getWidth() * ms / total; };

    // the beats of the song
    g.setFont (body (10.0f));
    for (int b = 0; (float) b * beat <= total; ++b)
    {
        const float x = xFor ((float) b * beat);
        g.setColour (b == 0 ? lineHi : line); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (textMute);
        if (b > 0) g.drawText ("beat " + juce::String (b + 1), juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 1.0f, 60.0f, 12.0f), juce::Justification::centred);
    }
    // the dry sound, the gap, then the reverb falling away (60 dB down at the end of the decay)
    g.setColour (text); g.fillRect (xFor (0.0f) - 1.0f, plot.getY() + 2.0f, 3.0f, plot.getHeight() - 2.0f);
    juce::Path tail;
    const float x0 = xFor (pre), x1 = xFor (pre + decay);
    tail.startNewSubPath (x0, plot.getBottom());
    for (int i = 0; i <= 60; ++i)
    {
        const float f = (float) i / 60.0f;
        tail.lineTo (x0 + (x1 - x0) * f, plot.getBottom() - plot.getHeight() * 0.86f * std::pow (1.0f - f, 2.2f));
    }
    tail.lineTo (x1, plot.getBottom());
    tail.closeSubPath();
    const float open = on ? juce::jlimit (0.25f, 1.0f, juce::Decibels::decibelsToGain (duck)) : 0.3f;
    g.setColour (yellow.withAlpha (0.20f * open)); g.fillPath (tail);
    g.setColour (on ? yellow.withAlpha (0.35f + 0.65f * open) : textMute); g.strokePath (tail, juce::PathStrokeType (1.6f));
    g.setColour (textMute); g.setFont (body (10.0f));

    // the maths, spelled out
    const bool timed = proc.getParam (ids::rvAuto) > 0.5f;
    const int beats = (int) proc.getParam (ids::rvLength) == 0 ? 1 : ((int) proc.getParam (ids::rvLength) == 1 ? 2 : 4);
    const juce::String sum = proc.hasHostTempo() ? "60,000 / " + juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM = " + juce::String (juce::roundToInt (beat)) + " ms a beat"
                                                 : juce::String ("No tempo from the host yet: using 120 BPM");
    const juce::String timing = "pre-delay " + juce::String (juce::roundToInt (pre)) + " ms" + (timed ? " (1/64)" : "") + ", decay " + juce::String (decay * 0.001f, 2) + " s"
                              + (timed ? " (" + juce::String (beats) + (beats == 1 ? " beat)" : " beats)") : juce::String());
    auto top = r.reduced (12.0f, 6.0f).removeFromTop (16.0f);
    g.setFont (body (11.0f)); g.setColour (on ? textDim : textMute);
    g.drawText (timing, top.removeFromRight (juce::jmin (250.0f, top.getWidth() * 0.5f)), juce::Justification::centredRight, true);
    g.drawText (sum, top, juce::Justification::centredLeft, true);
    if (on && duck < -0.5f)
    {
        g.setColour (textMute);
        g.drawText ("stepping back " + juce::String (std::abs (duck), 1) + " dB while the voice sings", plot.withTrimmedTop (2.0f).withHeight (14.0f), juce::Justification::centredRight);
    }
}

void FXPage::paintOverChildren (juce::Graphics& g)
{
    drawPitch (g, pitchWell.toFloat());
    drawDoubles (g, dblWell.toFloat());
    drawReverb (g, verbWell.toFloat());
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText ("Key", keyLabel, juce::Justification::centredLeft);
    if (! proc.isStereoOut())
    {
        g.setFont (body (10.5f)); g.setColour (textMute);
        g.drawFittedText ("This track is mono, so the copies sit behind the lead. For wide doubles, insert the plugin as Mono > Stereo.", monoNote, juce::Justification::centredLeft, 2);
    }
}

void FXPage::mouseMove (const juce::MouseEvent& e)
{
    juce::String key;
    if (pitchWell.contains (e.getPosition())) key = "tune.display";
    else if (verbWell.contains (e.getPosition())) key = "rv.display";
    else if (dblWell.contains (e.getPosition())) key = "dblOn";
    getProperties().set ("help", key);
}

void FXPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto left = r.removeFromLeft (r.getWidth() * 53 / 100);
    r.removeFromLeft (10);
    auto top = r.removeFromTop ((r.getHeight() - 10) * 44 / 100);
    r.removeFromTop (10);
    tune.setBounds (left); dbl.setBounds (top); verb.setBounds (r);

    auto place3 = [] (juce::Rectangle<int> area, juce::TextButton& a, juce::TextButton& b, juce::TextButton& c)
    {
        const int w = (area.getWidth() - 8) / 3;
        a.setBounds (area.removeFromLeft (w)); area.removeFromLeft (4);
        b.setBounds (area.removeFromLeft (w)); area.removeFromLeft (4);
        c.setBounds (area);
    };
    auto header = [] (juce::Rectangle<int> section, int width) { return juce::Rectangle<int> (section.getRight() - width - 12, section.getY() + 6, width, Section::kHeader - 12); };

    // ---- Tune
    place3 (header (left, 252), tuneNatural, tuneTight, tuneHard);
    auto t = left.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto keys = t.removeFromTop (28);
    keyLabel = keys.removeFromLeft (30);
    keyBox.setBounds (keys.removeFromLeft (64));
    keys.removeFromLeft (8);
    findKey.setBounds (keys.removeFromRight (84));
    keys.removeFromRight (8);
    scale.setBounds (keys);
    t.removeFromTop (10);
    auto knobs = t.removeFromBottom (96);
    t.removeFromBottom (10);
    pitchWell = t;
    layoutRow (knobs.withSizeKeepingCentre (juce::jmin (knobs.getWidth(), 330), knobs.getHeight()), { &tuneSpeed, &tuneAmount, &tuneVibrato }, 10);

    // ---- Doubler
    place3 (header (top, 232), dblSubtle, dblWide, dblStack);
    auto d = top.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto dControls = d.removeFromRight (juce::jmin (318, d.getWidth() * 64 / 100));
    d.removeFromRight (12);
    dblWell = d;
    voices.setBounds (dControls.removeFromTop (28).removeFromLeft (170));
    monoNote = dControls.removeFromBottom (proc.isStereoOut() ? 0 : 26);
    dControls.removeFromTop (6);
    layoutRow (dControls.removeFromTop (juce::jmin (96, dControls.getHeight())), { &dblMix, &dblDetune, &dblTiming, &dblWidth }, 6);

    // ---- Reverb
    place3 (header (r, 232), rvTight, rvPlate, rvBig);
    auto v = r.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto vKnobs = v.removeFromBottom (96);
    v.removeFromBottom (8);
    auto timing = v.removeFromBottom (28);
    v.removeFromBottom (8);
    verbWell = v;
    rvAuto.setBounds (timing.removeFromLeft (158));
    timing.removeFromLeft (8);
    rvLength.setBounds (timing.removeFromLeft (juce::jmin (230, timing.getWidth())));
    layoutRow (vKnobs, { &rvMix, &rvDecay, &rvPre, &rvTone, &rvDuck }, 6);
}
}
