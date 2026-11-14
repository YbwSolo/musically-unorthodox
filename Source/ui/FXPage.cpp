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
constexpr int kRowH = 42, kRailW = 222, kOrderGap = 22;
}

FXPage::FXPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      dblVoices (p, ids::dblVoices, { "Double", "Stack" }, ids::dblVoices),
      dblMix (p, ids::dblMix, "Level", ids::dblMix), dblDetune (p, ids::dblDetune, "Detune", ids::dblDetune), dblTiming (p, ids::dblTiming, "Looseness", ids::dblTiming), dblWidth (p, ids::dblWidth, "Width", ids::dblWidth),
      dlSync (p, ids::dlSync, "Timed to tempo", "switch", ids::dlSync),
      dlNote (p, ids::dlNote, Echo::noteNames(), ids::dlNote),
      dlPattern (p, ids::dlPattern, Echo::patternNames(), ids::dlPattern), dlStyle (p, ids::dlStyle, Echo::styleNames(), ids::dlStyle),
      dlMix (p, ids::dlMix, "Level", ids::dlMix), dlTime (p, ids::dlTime, "Time", ids::dlTime), dlFeedback (p, ids::dlFeedback, "Repeats", ids::dlFeedback),
      dlTone (p, ids::dlTone, "Tone", ids::dlTone), dlWidth (p, ids::dlWidth, "Ping-pong", ids::dlWidth), dlDuck (p, ids::dlDuck, "Duck", ids::dlDuck),
      dlDrive (p, ids::dlDrive, "Drive", ids::dlDrive), dlWobble (p, ids::dlWobble, "Wobble", ids::dlWobble), dlLowCut (p, ids::dlLowCut, "Low cut", ids::dlLowCut),
      rvType (p, ids::rvType, Reverb::typeNames(), ids::rvType),
      rvAuto (p, ids::rvAuto, "Timed to tempo", "switch", ids::rvAuto),
      rvLength (p, ids::rvLength, { "1 beat", "2 beats", "4 beats" }, ids::rvLength),
      rvFreeze (p, ids::rvFreeze, "Freeze", "switch", ids::rvFreeze),
      rvMix (p, ids::rvMix, "Level", ids::rvMix), rvDecay (p, ids::rvDecay, "Decay", ids::rvDecay), rvPre (p, ids::rvPre, "Pre-delay", ids::rvPre),
      rvTone (p, ids::rvTone, "Tone", ids::rvTone), rvDuck (p, ids::rvDuck, "Duck", ids::rvDuck), rvLowCut (p, ids::rvLowCut, "Low cut", ids::rvLowCut), rvWidth (p, ids::rvWidth, "Width", ids::rvWidth),
      rvSize (p, ids::rvSize, "Size", ids::rvSize), rvDiffusion (p, ids::rvDiffusion, "Diffusion", ids::rvDiffusion), rvEarly (p, ids::rvEarly, "Early", ids::rvEarly),
      rvLowDecay (p, ids::rvLowDecay, "Low decay", ids::rvLowDecay), rvModDepth (p, ids::rvModDepth, "Movement", ids::rvModDepth), rvModRate (p, ids::rvModRate, "Move rate", ids::rvModRate),
      rvShimmer (p, ids::rvShimmer, "Shimmer", ids::rvShimmer),
      hmMode (p, ids::hmMode, PitchFX::harmonyModeNames(), ids::hmMode),
      hmVoicing (p, ids::hmVoicing, { "Third up", "Fifth up", "Third + fifth", "Octave up", "Octave down", "Third below", "Choir" }, ids::hmVoicing),
      hmLeading (p, ids::hmLeading, "Voice leading", "switch", ids::hmLeading),
      hmChordLength (p, ids::hmChordLength, { "2 bars each", "1 bar each", "Half a bar each", "1 beat each" }, ids::hmChordLength),
      hmMix (p, ids::hmMix, "Level", ids::hmMix), hmWidth (p, ids::hmWidth, "Width", ids::hmWidth), hmHumanise (p, ids::hmHumanise, "Humanise", ids::hmHumanise),
      hmChoir (p, ids::hmChoir, "Choir", ids::hmChoir), hmChordCount (p, ids::hmChordCount, "Chords", ids::hmChordCount),
      psMode (p, ids::psMode, PitchFX::shiftModeNames(), ids::psMode),
      psSemis (p, ids::psSemis, "Pitch", ids::psSemis), psCharacter (p, ids::psCharacter, "Voice size", ids::psCharacter), psMix (p, ids::psMix, "Mix", ids::psMix),
      psFine (p, ids::psFine, "Fine", ids::psFine), psDrive (p, ids::psDrive, "Drive", ids::psDrive), psWidth (p, ids::psWidth, "Width", ids::psWidth),
      phMode (p, ids::phMode, Phone::modeNames(), ids::phMode),
      phAmount (p, ids::phAmount, "Amount", ids::phAmount), phTone (p, ids::phTone, "Width", ids::phTone), phDrive (p, ids::phDrive, "Drive", ids::phDrive), phNoise (p, ids::phNoise, "Noise", ids::phNoise),
      lfNoiseType (p, ids::lfNoiseType, LoFi::noiseNames(), ids::lfNoiseType),
      lfCrush (p, ids::lfCrush, "Crush", ids::lfCrush), lfWobble (p, ids::lfWobble, "Wobble", ids::lfWobble), lfMix (p, ids::lfMix, "Mix", ids::lfMix),
      lfDrive (p, ids::lfDrive, "Drive", ids::lfDrive), lfWear (p, ids::lfWear, "Wear", ids::lfWear), lfTone (p, ids::lfTone, "Tone", ids::lfTone), lfNoise (p, ids::lfNoise, "Noise", ids::lfNoise),
      mdType (p, ids::mdType, Motion::typeNames(), ids::mdType), mdShape (p, ids::mdShape, Motion::shapeNames(), ids::mdShape),
      mdSync (p, ids::mdSync, "Timed to tempo", "switch", ids::mdSync), mdNote (p, ids::mdNote, Motion::noteNames(), ids::mdNote),
      mdRate (p, ids::mdRate, "Rate", ids::mdRate), mdDepth (p, ids::mdDepth, "Depth", ids::mdDepth), mdMix (p, ids::mdMix, "Mix", ids::mdMix), mdSmooth (p, ids::mdSmooth, "Smooth", ids::mdSmooth),
      chMode (p, ids::chMode, Chop::modeNames(), ids::chMode),
      chRate (p, ids::chRate, { "1/4", "1/8", "1/8 triplet", "1/16", "1/32" }, ids::chRate),
      chDepth (p, ids::chDepth, "Depth", ids::chDepth), chShape (p, ids::chShape, "Hardness", ids::chShape),
      chGate (p, ids::chGate, "Gate", ids::chGate), chSwing (p, ids::chSwing, "Swing", ids::chSwing), chLength (p, ids::chLength, "Steps", ids::chLength)
{
    for (auto& m : fxModules())
    {
        auto* t = powers.add (new ParamToggle (p, m.onParam, {}, "power", m.onParam));
        addAndMakeVisible (t);
    }
    styleButton (rvFit, "Fit to this track", true, "rv.fit");
    styleButton (hmFill, "Fill from key", false, "hm.fill");
    hmFill.onClick = [this]
    {
        const int scale = (int) proc.getParam (ids::tuneScale);
        if (scale < 1 || scale > 4) { setStatus (proc.fillChords (0)); return; }
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        const auto names = MUProcessor::chordProgressionNames (scale == 2 || scale == 4);
        for (int i = 0; i < names.size(); ++i) menu.addItem (i + 1, names[i]);
        // (taken as a local first: the Windows compiler reads "this" in a capture inside another lambda as the lambda itself)
        const juce::Component::SafePointer<FXPage> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&hmFill),
                            [safe] (int chosen) { if (safe != nullptr && chosen > 0) { safe->setStatus (safe->proc.fillChords (chosen - 1)); safe->repaint(); } });
    };
    styleButton (hmClear, "Clear", false, "hm.clear");
    hmClear.onClick = [this] { setStatus (proc.clearChords()); repaint(); };
    rvFit.onClick = [this] { setStatus (proc.fitReverb()); };

    // the order matches fxModules(): doubler, delay, reverb, harmony, shift, phone, lo-fi, motion, chop
    ui = {
        { { &dblVoices }, { 190 }, { &dblMix, &dblDetune, &dblTiming, &dblWidth }, {} },
        { { &dlSync, &dlNote, &dlPattern, &dlStyle }, { 158, 130, 250, 814 }, { &dlMix, &dlTime, &dlFeedback, &dlTone, &dlWidth, &dlDuck }, { &dlDrive, &dlWobble, &dlLowCut } },
        { { &rvType, &rvAuto, &rvLength, &rvFreeze, &rvFit }, { 814, 158, 220, 96, 140 }, { &rvMix, &rvDecay, &rvPre, &rvWidth, &rvTone, &rvDuck, &rvLowCut },
          { &rvSize, &rvDiffusion, &rvEarly, &rvLowDecay, &rvModDepth, &rvModRate, &rvShimmer } },
        { { &hmMode, &hmLeading, &hmChordLength, &hmFill, &hmClear, &hmVoicing }, { 270, 140, 140, 118, 62, 760 }, { &hmMix, &hmWidth, &hmHumanise, &hmChoir, &hmChordCount }, {} },
        { { &psMode }, { 300 }, { &psSemis, &psFine, &psCharacter, &psDrive, &psWidth, &psMix }, {} },
        { { &phMode }, { 640 }, { &phAmount, &phTone, &phDrive, &phNoise }, {} },
        { { &lfNoiseType }, { 190 }, { &lfDrive, &lfWobble, &lfCrush, &lfWear, &lfTone, &lfNoise, &lfMix }, {} },
        { { &mdType, &mdSync, &mdNote, &mdShape }, { 520, 158, 110, 560 }, { &mdRate, &mdDepth, &mdMix, &mdSmooth }, {} },
        { { &chMode, &chRate }, { 190, 420 }, { &chDepth, &chShape, &chGate, &chSwing, &chLength }, {} } };
    for (auto& m : ui)
    {
        for (auto* c : m.top) addChildComponent (c);
        for (auto* k : m.knobs) addChildComponent (k);
        for (auto* k : m.more) addChildComponent (k);
    }
    // the finer controls of the reverb and the delay stay out of the way until they are asked for
    showMore = (bool) proc.apvts.state.getProperty ("fxMore", false);
    styleButton (moreButton, "More", false, "fx.more");
    moreButton.setClickingTogglesState (true);
    moreButton.setToggleState (showMore, juce::dontSendNotification);
    moreButton.onClick = [this]
    {
        showMore = moreButton.getToggleState();
        proc.apvts.state.setProperty ("fxMore", showMore, nullptr);
        select (selected);
    };
    addChildComponent (moreButton);

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
    presets.enableOwnPresets (proc, [this] { return fxModules()[(size_t) selected].id; }, setStatus);
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
        for (auto* k : ui[m].more) k->setVisible (show && showMore);
    }
    {
        const int harmonyMode = (int) proc.getParam (ids::hmMode);
        shownHarmonyMode = harmonyMode;
        hmVoicing.setVisible (selected == 3 && harmonyMode == 0);
        for (auto* c : std::initializer_list<juce::Component*> { &hmChordLength, &hmFill, &hmClear }) c->setVisible (selected == 3 && harmonyMode == 2);
    }
    moreButton.setVisible (! ui[(size_t) selected].more.empty());
    moreButton.setButtonText (showMore ? "Fewer controls" : "More controls");
    presets.refresh();
    resized();
    repaint();
}

// fxModules(): 0 doubler, 1 delay, 2 reverb, 3 harmony, 4 shift, 5 phone, 6 lofi, 7 motion, 8 chop
int FXPage::chainTool (int module) const
{
    switch (module)
    {
        case 5: return fxorder::Phone;   case 6: return fxorder::Lofi;   case 7: return fxorder::Motion;
        case 8: return fxorder::Chop;    case 1: return fxorder::Delay;  case 2: return fxorder::Reverb;
        default: return -1;
    }
}

int FXPage::slotOf (int module) const
{
    if (module == 0) return 0;
    if (module == 3) return 1;
    if (module == 4) return 2;
    const auto order = fxorder::decode (juce::roundToInt (proc.getParam (ids::fxOrder)));
    const int tool = chainTool (module);
    for (int place = 0; place < fxorder::kCount; ++place) if (order[(size_t) place] == tool) return 3 + place;
    return 3;
}

juce::Rectangle<int> FXPage::rowBounds (int index) const
{
    const int slot = slotOf (index);
    return { railList.getX(), railList.getY() + slot * kRowH + (slot >= 3 ? kOrderGap : 0), railList.getWidth(), kRowH };
}

void FXPage::tick (float dt)
{
    sounds.refresh(); presets.refresh();
    if (const int order = juce::roundToInt (proc.getParam (ids::fxOrder)); order != shownOrder) { shownOrder = order; resized(); repaint (rail); }
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
    const int echoStyle = (int) proc.getParam (ids::dlStyle), echoPattern = (int) proc.getParam (ids::dlPattern);
    dlDrive.setDimmed (echoStyle != 1 && echoStyle != 2);          // only Tape and Analog saturate
    {
        // Harmony shows the controls of the mode it is in
        const int harmonyMode = (int) proc.getParam (ids::hmMode);
        if (harmonyMode != shownHarmonyMode || hmVoicing.isVisible() != (selected == 3 && harmonyMode == 0))
        {
            shownHarmonyMode = harmonyMode;
            hmVoicing.setVisible (selected == 3 && harmonyMode == 0);
            for (auto* c : std::initializer_list<juce::Component*> { &hmChordLength, &hmFill, &hmClear }) c->setVisible (selected == 3 && harmonyMode == 2);
            resized(); repaint();
        }
        hmChordCount.setDimmed (harmonyMode != 2);
        psWidth.setDimmed ((int) proc.getParam (ids::psMode) != 1);
        const bool timedMotion = proc.getParam (ids::mdSync) > 0.5f, drawn = (int) proc.getParam (ids::mdShape) == 5, pattern = (int) proc.getParam (ids::chMode) == 1;
        mdRate.setDimmed (timedMotion); mdNote.setAlpha (timedMotion ? 1.0f : 0.35f); mdSmooth.setDimmed (! drawn);
        chGate.setDimmed (! pattern); chSwing.setDimmed (! pattern); chLength.setDimmed (! pattern);
        lfNoiseType.setAlpha (proc.getParam (ids::lfNoise) > 0.5f ? 1.0f : 0.35f);
    }
    dlWidth.setDimmed (echoPattern != 0);                          // only Ping-pong bounces
    const int space = (int) proc.getParam (ids::rvType);
    rvShimmer.setDimmed (space != Reverb::Shimmer);
    rvEarly.setDimmed (space == Reverb::Plate || space == Reverb::Ambient || space == Reverb::Gated);
    rvModDepth.setDimmed (space == Reverb::Gated); rvModRate.setDimmed (space == Reverb::Gated); rvLowDecay.setDimmed (space == Reverb::Gated);
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
        if (chainTool (i) >= 0)
        {
            // a grip on the tools that can be moved
            g.setColour (i == selected || i == hoverRow ? textDim : textMute.withAlpha (0.6f));
            for (int k = -1; k <= 1; ++k) g.fillRect (row.getRight() - 20, row.getCentreY() + k * 4, 10, 1);
            if (dragging && chainTool (i) == dragTool) { g.setColour (yellow.withAlpha (0.5f)); g.drawRoundedRectangle (row.toFloat().reduced (0.5f, 2.5f), 5.0f, 1.0f); }
        }
        auto textArea = row.withTrimmedLeft (48).withTrimmedRight (8);
        g.setFont (bodyBold (13.5f)); g.setColour (on ? text : textDim);
        g.drawText (modules[(size_t) i].name, textArea.removeFromTop (row.getHeight() / 2 + 3), juce::Justification::bottomLeft);
        g.setFont (body (10.5f)); g.setColour (on ? yellow.withAlpha (0.4f + 0.6f * glow[(size_t) i]) : textMute);
        const auto loaded = proc.getModulePresetName (modules[(size_t) i].id);
        g.drawText (on ? (loaded.isEmpty() ? juce::String ("On") : loaded) : juce::String ("Off"), textArea, juce::Justification::topLeft, true);
    }
    {
        // between the voice tools and the in-line ones
        g.setColour (line); g.fillRect (orderNote.getX() + 6, orderNote.getY() + 3, orderNote.getWidth() - 12, 1);
        g.setFont (body (10.0f)); g.setColour (textMute);
        g.drawText ("Then these, in this order. Drag to change it.", orderNote.withTrimmedTop (5).reduced (6, 0), juce::Justification::centredLeft, true);
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
    const int pattern = (int) proc.getParam (ids::dlPattern), style = (int) proc.getParam (ids::dlStyle);
    auto bar = [&] (float ms, float level, bool up, int repeat)
    {
        const float x = xFor (ms), h = (plot.getHeight() * 0.5f - 6.0f) * level;
        // Diffused: every repeat is wider than the one before; Tape and Analog: every repeat a little duller
        const float w = style == 3 ? 6.0f + 7.0f * (float) repeat : 6.0f;
        const float fade = (style == 1 || style == 2) ? juce::jmax (0.45f, 1.0f - 0.12f * (float) repeat) : 1.0f;
        g.setColour (on ? yellow.withAlpha ((0.35f + 0.65f * open) * fade * (style == 3 ? 0.75f : 1.0f)) : textMute);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - w * 0.5f, up ? midY - h : midY, w, h), 2.0f);
    };
    if (pattern == 0)
    {
        float gain = 1.0f;
        for (int k = 1; (float) k * time <= total && gain > 0.02f; ++k)
        {
            const bool leftSide = (k % 2) == 1;
            bar ((float) k * time, gain, leftSide, k - 1);
            if (width < 0.98f) bar ((float) k * time, gain * (1.0f - width), ! leftSide, k - 1);        // with ping-pong down, each echo appears on both sides
            gain *= fb;
            if (fb < 0.01f) break;
        }
    }
    else
    {
        // Straight: the same echoes on both sides. Dual: the right side keeps its own, shorter time.
        for (int side = 0; side < 2; ++side)
        {
            const float t = (pattern == 2 && side == 1) ? time * 0.75f : time;
            float gain = 1.0f;
            for (int k = 1; (float) k * t <= total && gain > 0.02f; ++k)
            {
                bar ((float) k * t, gain, side == 0, k - 1);
                gain *= fb;
                if (fb < 0.01f) break;
            }
        }
    }

    const bool synced = proc.getParam (ids::dlSync) > 0.5f;
    const juce::String sum = proc.hasHostTempo() ? "60,000 / " + juce::String (bpm, bpm == std::floor (bpm) ? 0 : 1) + " BPM = " + juce::String (juce::roundToInt (beat)) + " ms a beat"
                                                 : juce::String ("No tempo from the host yet: using 120 BPM");
    const juce::String timing = Echo::styleNames()[style] + ": "
                              + (synced ? Echo::noteNames()[(int) proc.getParam (ids::dlNote)] + " note = " + juce::String (juce::roundToInt (time)) + " ms between echoes"
                                        : juce::String (juce::roundToInt (time)) + " ms between echoes (" + juce::String (time / beat, 2) + " of a beat)")
                              + (pattern == 2 ? ", " + juce::String (juce::roundToInt (time * 0.75f)) + " ms on the right" : juce::String());
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
    const float total = juce::jmax (beat * 2.2f, (pre + decay * juce::jmax (1.0f, proc.getParam (ids::rvLowDecay) * 0.01f)) * 1.12f);
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
    // The dry sound, the gap, the first reflections of this space, then the tail: mids in yellow, with the lows and
    // the highs drawn as thinner lines so their own decay times can be seen.
    g.setColour (text); g.fillRect (xFor (0.0f) - 1.5f, plot.getY() + 2.0f, 3.0f, plot.getHeight() - 2.0f);
    const float open = on ? juce::jlimit (0.25f, 1.0f, juce::Decibels::decibelsToGain (duck)) : 0.3f;
    const float size = proc.getParam (ids::rvSize) * 0.01f, earlyAmount = proc.getParam (ids::rvEarly) * 0.02f;
    const bool frozen = proc.getParam (ids::rvFreeze) > 0.5f;
    // when the first reflections start and how far apart they are (ms), and how long the tail takes to build
    static const float firstMs[Reverb::NumTypes] = { 3.1f, 0.0f, 12.7f, 6.1f, 22.3f, 0.0f, 6.1f, 12.7f, 0.0f };
    static const float lastMs[Reverb::NumTypes]  = { 34.3f, 0.0f, 86.3f, 53.3f, 135.1f, 0.0f, 53.3f, 86.3f, 0.0f };
    static const float earlyTop[Reverb::NumTypes] = { 0.80f, 0.0f, 0.45f, 0.62f, 0.34f, 0.0f, 0.55f, 0.22f, 0.0f };
    static const float buildOf[Reverb::NumTypes] = { 0.015f, 0.012f, 0.06f, 0.03f, 0.11f, 0.22f, 0.03f, 0.10f, 0.0f };
    g.setColour (on ? yellow.withAlpha (0.8f * open) : textMute);
    for (int e = 0; e < 8 && earlyTop[type] > 0.0f; ++e)
    {
        const float at = firstMs[type] + (lastMs[type] - firstMs[type]) * std::pow ((float) e / 7.0f, 1.25f);
        const float h = plot.getHeight() * juce::jmin (0.95f, earlyTop[type] * earlyAmount * (1.0f - 0.09f * (float) e));
        g.fillRect (xFor (pre + at * size) - 1.0f, plot.getBottom() - h, 2.0f, h);
    }
    const float x0 = xFor (pre);
    auto tailPath = [&] (float lengthMs, float height)
    {
        juce::Path t;
        const float x1 = xFor (pre + lengthMs);
        t.startNewSubPath (x0, plot.getBottom());
        for (int i = 0; i <= 80; ++i)
        {
            const float f = (float) i / 80.0f;
            const float rise = buildOf[type] > 0.0f ? juce::jlimit (0.0f, 1.0f, f / buildOf[type]) : 1.0f;
            const float level = frozen ? rise : type == Reverb::Gated ? (0.8f + 0.2f * f) : rise * std::pow (1.0f - f, 2.2f);
            t.lineTo (x0 + (x1 - x0) * f, plot.getBottom() - plot.getHeight() * height * level);
        }
        t.lineTo (x1, plot.getBottom());
        return t;
    };
    const float shownLength = frozen ? total : type == Reverb::Gated ? Reverb::gateSeconds (decay * 0.001f) * 1000.0f : decay;
    auto tail = tailPath (shownLength, 0.86f);
    tail.closeSubPath();
    g.setColour (yellow.withAlpha (0.20f * open)); g.fillPath (tail);
    g.setColour (on ? yellow.withAlpha (0.35f + 0.65f * open) : textMute); g.strokePath (tail, juce::PathStrokeType (1.6f));
    if (! frozen && type != Reverb::Gated)
    {
        static const float highOf[Reverb::NumTypes] = { 0.80f, 1.0f, 0.85f, 0.95f, 0.70f, 0.90f, 0.60f, 0.90f, 1.0f };
        const float lows = decay * proc.getParam (ids::rvLowDecay) * 0.01f, highs = decay * (0.25f + 0.55f * proc.getParam (ids::rvTone) * 0.01f) * highOf[type];
        g.setColour (text.withAlpha (on ? 0.55f : 0.2f)); g.strokePath (tailPath (lows, 0.70f), juce::PathStrokeType (1.0f));
        g.setColour (textDim.withAlpha (on ? 0.8f : 0.3f)); g.strokePath (tailPath (highs, 0.55f), juce::PathStrokeType (1.0f));
        g.setFont (body (10.0f));
        g.setColour (text.withAlpha (0.6f)); g.drawText ("lows " + juce::String (lows * 0.001f, 1) + " s", juce::Rectangle<float> (plot.getRight() - 190.0f, plot.getY() + 16.0f, 90.0f, 12.0f), juce::Justification::centredRight);
        g.setColour (textDim); g.drawText ("highs " + juce::String (highs * 0.001f, 1) + " s", juce::Rectangle<float> (plot.getRight() - 92.0f, plot.getY() + 16.0f, 92.0f, 12.0f), juce::Justification::centredRight);
    }

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

juce::String FXPage::intervalName (int steps)
{
    static const char* const names[] = { "Unison", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave", "9th", "10th", "11th", "12th", "13th", "14th", "2 octaves" };
    const int count = juce::jlimit (0, 14, std::abs (steps));
    return count == 0 ? juce::String (names[0]) : juce::String (names[count]) + (steps > 0 ? " up" : " down");
}

juce::Rectangle<float> FXPage::harmonyLeft() const  { return well.toFloat().reduced (1.0f).withTrimmedTop (24.0f).withTrimmedRight (490.0f); }
juce::Rectangle<float> FXPage::harmonyTable() const { return well.toFloat().reduced (1.0f).withTrimmedTop (24.0f).removeFromRight (482.0f).reduced (8.0f, 6.0f); }

// columns: 0 on, 1 interval, 2 level, 3 pan, 4 delay, 5 size, 6 detune
juce::Rectangle<float> FXPage::voiceCell (int voice, int column) const
{
    const auto table = harmonyTable();
    static const float widths[] = { 30.0f, 104.0f, 62.0f, 62.0f, 62.0f, 62.0f, 62.0f };
    const float rowH = juce::jmin (46.0f, (table.getHeight() - 20.0f) / 4.0f);
    float x = table.getX();
    for (int c = 0; c < column; ++c) x += widths[c] + 4.0f;
    return { x, table.getY() + 20.0f + rowH * (float) voice, widths[juce::jlimit (0, 6, column)], rowH - 6.0f };
}

juce::String FXPage::voiceParam (int voice, int column) const
{
    switch (column)
    {
        case 0:  return ids::hmVoiceOn (voice);      case 1:  return ids::hmVoiceSteps (voice);  case 2:  return ids::hmVoiceLevel (voice);
        case 3:  return ids::hmVoicePan (voice);     case 4:  return ids::hmVoiceDelay (voice);  case 5:  return ids::hmVoiceSize (voice);
        default: return ids::hmVoiceDetune (voice);
    }
}

// the chord grid: one column a chord, twelve rows for the notes C (bottom) to B (top)
juce::Rectangle<float> FXPage::chordCell (int slot, int note) const
{
    const auto grid = harmonyLeft().reduced (8.0f, 6.0f).withTrimmedLeft (26.0f).withTrimmedTop (18.0f);
    const float w = grid.getWidth() / 16.0f, hgt = grid.getHeight() / 12.0f;
    return { grid.getX() + w * (float) slot, grid.getBottom() - hgt * (float) (note + 1), w, hgt };
}

void FXPage::drawHarmony (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::hmOn) > 0.5f;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = (int) proc.getParam (ids::tuneNotes), voicing = (int) proc.getParam (ids::hmVoicing);
    const int harmonyMode = (int) proc.getParam (ids::hmMode);
    static const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (harmonyMode != 0)
    {
        // ---- the table of voices, on the right in both of the hand-built modes
        const auto table = harmonyTable();
        static const char* const heads[] = { "", "Interval", "Level", "Pan", "Delay", "Size", "Detune" };
        g.setFont (body (10.5f)); g.setColour (textMute);
        for (int c = 1; c < 7; ++c) g.drawText (heads[c], voiceCell (0, c).withY (table.getY()).withHeight (16.0f), juce::Justification::centredLeft);
        for (int v = 0; v < 4; ++v)
        {
            const bool voiceOn = proc.getParam (ids::hmVoiceOn (v)) > 0.5f, lit = on && voiceOn;
            const auto power = voiceCell (v, 0);
            g.setColour (voiceOn ? (on ? yellow : textDim) : control); g.fillEllipse (power.withSizeKeepingCentre (18.0f, 18.0f));
            g.setColour (voiceOn ? onYellow : textMute); g.setFont (bodyBold (10.5f)); g.drawText (juce::String (v + 1), power.withSizeKeepingCentre (18.0f, 18.0f), juce::Justification::centred);
            for (int c = 1; c < 7; ++c)
            {
                const auto cell = voiceCell (v, c);
                g.setColour (control); g.fillRoundedRectangle (cell, 4.0f);
                const auto id = voiceParam (v, c);
                const float value = proc.getParam (id);
                juce::String shown;
                if (c == 1) shown = intervalName (juce::roundToInt (value));
                else
                {
                    // a bar from the left for things that start at nothing, from the middle for things that go both ways
                    const bool bothWays = c == 3 || c == 5 || c == 6;
                    const float top = c == 4 ? 60.0f : c == 6 ? 50.0f : 100.0f;
                    const float amount = juce::jlimit (-1.0f, 1.0f, value / top);
                    auto bar = cell.reduced (2.0f);
                    if (bothWays) bar = amount >= 0.0f ? bar.withTrimmedLeft (bar.getWidth() * 0.5f).withWidth (bar.getWidth() * 0.5f * amount)
                                                       : bar.withTrimmedLeft (bar.getWidth() * 0.5f * (1.0f + amount)).withWidth (bar.getWidth() * 0.5f * -amount);
                    else bar = bar.withWidth (bar.getWidth() * juce::jmax (0.0f, amount));
                    g.setColour (lit ? yellow.withAlpha (0.3f) : lineHi); g.fillRoundedRectangle (bar, 3.0f);
                    shown = c == 2 ? juce::String (juce::roundToInt (value)) + " %" : c == 3 ? (std::abs (value) < 0.5f ? juce::String ("Centre") : juce::String (juce::roundToInt (std::abs (value))) + (value < 0.0f ? " L" : " R"))
                          : c == 4 ? juce::String (juce::roundToInt (value)) + " ms" : c == 5 ? (std::abs (value) < 0.5f ? juce::String ("Same") : juce::String (juce::roundToInt (std::abs (value))) + (value < 0.0f ? " big" : " small"))
                          : (value > 0.0f ? "+" : "") + juce::String (value, 0) + " c";
                }
                g.setFont (c == 1 ? bodyBold (11.5f) : body (11.0f)); g.setColour (lit ? text : voiceOn ? textDim : textMute);
                g.drawText (shown, cell.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
            }
        }
    }
    if (harmonyMode == 2)
    {
        // ---- the chord grid
        const int count = juce::roundToInt (proc.getParam (ids::hmChordCount)), now = proc.getChordSlotNow(), keyboard = proc.getMidiChord();
        const auto area = harmonyLeft().reduced (8.0f, 6.0f);
        for (int note = 0; note < 12; ++note)
        {
            const bool inKey = Tuner::noteAllowed (60 + note, key, sc, mask);
            g.setFont (body (9.5f)); g.setColour (note == key ? text : inKey ? textDim : textMute);
            g.drawText (noteNames[note], juce::Rectangle<float> (area.getX(), chordCell (0, note).getY(), 22.0f, chordCell (0, note).getHeight()), juce::Justification::centredRight);
        }
        for (int slot = 0; slot < 16; ++slot)
        {
            const bool used = slot < count, playing = on && used && slot == now;
            const int chord = juce::roundToInt (proc.getParam (ids::hmChord (slot)));
            for (int note = 0; note < 12; ++note)
            {
                const auto cell = chordCell (slot, note).reduced (1.0f);
                const bool inKey = Tuner::noteAllowed (60 + note, key, sc, mask), lit = ((chord >> note) & 1) != 0;
                g.setColour (lit ? (! used ? textMute : playing ? yellow : on ? yellow.withAlpha (0.6f) : textDim) : (inKey ? raised : control).withAlpha (used ? 1.0f : 0.35f));
                g.fillRoundedRectangle (cell, 2.0f);
            }
            const auto head = chordCell (slot, 11).withY (area.getY()).withHeight (16.0f);
            g.setFont (bodyBold (9.5f)); g.setColour (playing ? yellow : used ? textDim : textMute);
            const auto name = MUProcessor::chordName (chord);
            g.drawText (name.isNotEmpty() ? name : (chord == 0 ? juce::String (slot + 1) : juce::String ("?")), head, juce::Justification::centred);
        }
        auto top = r.reduced (16.0f, 8.0f).removeFromTop (16.0f);
        g.setFont (body (11.5f)); g.setColour (textDim);
        g.drawText (keyboard != 0 ? juce::String ("Following the keys held on your keyboard.")
                                  : juce::String ("Click notes to build each chord; the voices sing those notes, moving as little as they can. An empty column means: follow the key."),
                    top, juce::Justification::centredLeft, true);
        return;
    }
    const float span = 30.0f, low = harmonyCentre - span * 0.5f;
    const auto plot = harmonyMode == 0 ? r.reduced (1.0f).withTrimmedTop (24.0f) : harmonyLeft();
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
    if (harmonyMode == 0)
    {
        for (int v = 0; v < voices; ++v)
        {
            const int note = PitchFX::harmonyNote (lead, v, voicing, key, sc, mask);
            if (note >= 0) dot ((float) note, on ? yellow : textMute, 14.0f, midiName (note), offsets[v]);
        }
        dot (sungHere > 0.0f ? sungHere : (float) lead, sungHere > 0.0f || ! on ? text : textDim, 16.0f, "the lead, " + midiName (lead), -110.0f);
    }
    else
    {
        // the voices as they are sung right now (with voice leading they do not always sit a fixed distance from the lead)
        for (int v = 0; v < 4; ++v)
        {
            if (proc.getParam (ids::hmVoiceOn (v)) < 0.5f) continue;
            const int live = on && sungHere > 0.0f ? proc.getPitchFX().getHarmonyNote (v) : -1;
            const int note = live >= 0 ? live : PitchFX::stepNote (lead, juce::roundToInt (proc.getParam (ids::hmVoiceSteps (v))), key, sc, mask);
            const auto c = juce::Point<float> (cx + 26.0f + 30.0f * (float) v, yFor ((float) note));
            g.setColour ((on ? yellow : textMute).withAlpha (0.2f)); g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
            g.setColour (on ? yellow : textMute); g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
            g.setColour (onYellow); g.setFont (bodyBold (10.0f)); g.drawText (juce::String (v + 1), juce::Rectangle<float> (16.0f, 16.0f).withCentre (c), juce::Justification::centred);
        }
        const auto c = juce::Point<float> (cx - 40.0f, yFor (sungHere > 0.0f ? sungHere : (float) lead));
        g.setColour (text.withAlpha (0.2f)); g.fillEllipse (juce::Rectangle<float> (28.0f, 28.0f).withCentre (c));
        g.setColour (sungHere > 0.0f || ! on ? text : textDim); g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (c));
        g.setFont (bodyBold (11.0f)); g.drawText ("lead", juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ c.x - 46.0f, c.y }), juce::Justification::centredRight);
    }
    g.restoreState();

    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    auto top = r.reduced (16.0f, 8.0f).removeFromTop (16.0f);
    g.setFont (body (11.5f)); g.setColour (textDim);
    if (harmonyMode == 1)
    {
        g.drawText ("Four voices, each set on its own row: click a number to switch a voice on, drag a box sideways to change it, double-click to put it back.", top, juce::Justification::centredLeft, true);
        return;
    }
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
    static const char* const modes[] = { "Shift: one copy of the voice, moved by Pitch and Fine.",
                                         "Wide: two copies moved the same distance up and down, one on each side. A few cents of Fine with Pitch at zero thickens a voice without doubling it.",
                                         "Robot: whatever is sung comes out on one note, the key note of the song, moved in semitones by Pitch." };
    g.drawText (modes[juce::jlimit (0, 2, (int) proc.getParam (ids::psMode))], r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
}

void FXPage::drawPhone (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::phOn) > 0.5f;
    const auto plot = r.reduced (1.0f);
    drawFreqGrid (g, plot);
    const float tone = proc.getParam (ids::phTone) * 0.01f;
    const int mode = (int) proc.getParam (ids::phMode);
    // the same filters the sound goes through, drawn from 20 Hz to 20 kHz
    const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    const float lo = Phone::lowEdgeHz (tone, mode), hi = Phone::highEdgeHz (tone, mode);
    Phone device;
    PhoneSettings shown; shown.tone = tone; shown.mode = mode;
    device.setSettings (shown);
    device.prepare (fs, 1);
    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) i / 200.0f);
        const float db = device.responseDb (hz);
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
    static const char* const device_[] = { "Everything outside that is what a small speaker cannot play.", "That is what a phone line carries; the bump is the earpiece.",
                                           "The two peaks are the horn ringing; pushed, its driver clips hard.", "A radio levels the voice so it breathes; the noise is static between stations.",
                                           "A narrow band clipped hard, with a burst of noise as each phrase ends." };
    g.drawText ("What gets through: " + hzString (lo) + " to " + hzString (hi) + ". " + device_[juce::jlimit (0, 4, mode)], r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
    if (const float noise = proc.getParam (ids::phNoise); noise > 0.5f)
    {
        // the noise floor, at the level it sits against a voice peaking at the top of the plot
        const float y = plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, (12.0f - (-70.0f + 40.0f * std::log10 (juce::jmax (1.0f, noise)))) / 60.0f);
        g.setColour (textMute); g.drawHorizontalLine (juce::roundToInt (y), freqToX (lo, plot.getX(), plot.getWidth()), freqToX (hi, plot.getX(), plot.getWidth()));
        g.setFont (body (10.5f)); g.drawText ("noise", juce::Rectangle<float> (freqToX (hi, plot.getX(), plot.getWidth()) + 6.0f, y - 7.0f, 60.0f, 14.0f), juce::Justification::centredLeft);
    }
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
    {
        // the chain, in the order the sound goes through it; a module is lit when it is doing something
        struct Module { const char* name; bool used; };
        const Module chain[] = { { "Drive", proc.getParam (ids::lfDrive) > 0.5f }, { "Wobble", wobble > 0.005f }, { "Crush", crush > 0.005f }, { "Wear", proc.getParam (ids::lfWear) > 0.5f },
                                 { "Tone", proc.getParam (ids::lfTone) < 99.5f }, { "Noise", proc.getParam (ids::lfNoise) > 0.5f } };
        auto row = r.reduced (16.0f, 8.0f).removeFromBottom (18.0f);
        g.setFont (body (10.5f));
        for (auto& m : chain)
        {
            const auto chip = row.removeFromLeft (58.0f); row.removeFromLeft (6.0f);
            g.setColour (m.used && on ? yellow.withAlpha (0.16f) : control); g.fillRoundedRectangle (chip, 4.0f);
            g.setColour (m.used ? (on ? yellow : textDim) : textMute); g.drawText (m.name, chip, juce::Justification::centred);
        }
    }
}

juce::Rectangle<float> FXPage::motionPlot() const { return well.toFloat().reduced (28.0f, 14.0f).withTrimmedTop (22.0f).withTrimmedBottom (20.0f); }
juce::Rectangle<float> FXPage::chopPlot() const   { return well.toFloat().reduced (24.0f, 12.0f).withTrimmedTop (22.0f).withTrimmedBottom (16.0f); }

void FXPage::drawMotion (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::mdOn) > 0.5f;
    const int type = juce::jlimit (0, 5, (int) proc.getParam (ids::mdType));
    const float depth = proc.getParam (ids::mdDepth) * 0.01f;
    MotionSettings shape;
    shape.shape = (int) proc.getParam (ids::mdShape); shape.smooth = proc.getParam (ids::mdSmooth) * 0.01f;
    for (int i = 0; i < MotionSettings::kPoints; ++i) shape.curve[(size_t) i] = proc.getParam (ids::mdPoint (i)) * 0.01f;
    const bool drawn = shape.shape == 5, timed = proc.getParam (ids::mdSync) > 0.5f;
    const auto plot = motionPlot();
    static const char* const what[] = { "Chorus: copies of the sound whose timing drifts. The line is how far the drift has gone.",
                                        "Flanger: a very short echo swept between nothing and a few milliseconds. The line is its length.",
                                        "Phaser: notches swept up and down through the tone. The line is where they are.",
                                        "Filter: the top end closing and opening. Top of the plot is fully open.",
                                        "Pan: the sound moving between the sides. Top of the plot is right, bottom is left.",
                                        "Volume: the level pushed down and let back up. Top of the plot is full level." };
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText (what[type], r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);

    // one cycle of the movement, left to right
    g.setColour (line);
    for (int q = 0; q <= 4; ++q) g.fillRect (plot.getX() + plot.getWidth() * (float) q / 4.0f, plot.getY(), 1.0f, plot.getHeight());
    g.fillRect (plot.getX(), plot.getCentreY(), plot.getWidth(), 1.0f);
    // how far the movement reaches: all of the plot at full Depth, a band round the middle at less
    auto yFor = [&] (float value) { return plot.getBottom() - plot.getHeight() * (0.5f + (value - 0.5f) * (0.12f + 0.88f * depth)); };
    juce::Path curve;
    for (int i = 0; i <= 480; ++i)
    {
        const float f = (float) i / 480.0f;
        const float x = plot.getX() + plot.getWidth() * f, y = yFor (Motion::valueAt (shape, juce::jmin (f, 0.9999f)));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.setColour (yellow.withAlpha (on ? 0.12f : 0.04f)); g.fillPath (fill);
    g.setColour (on ? yellow : textMute); g.strokePath (curve, juce::PathStrokeType (2.0f));
    if (drawn)
    {
        // the sixteen points that can be dragged
        for (int i = 0; i < MotionSettings::kPoints; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) MotionSettings::kPoints, y = yFor (shape.curve[(size_t) i]);
            g.setColour (black); g.fillEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ x, y }));
            g.setColour (on ? yellow : textDim); g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ x, y }), 2.0f);
        }
    }
    if (on)
    {
        // where the movement is now
        const float phase = proc.getMotion().getPhase();
        const float x = plot.getX() + plot.getWidth() * phase;
        g.setColour (text.withAlpha (0.5f)); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        g.setColour (text); g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, yFor (Motion::valueAt (shape, phase)) }));
    }
    g.setFont (body (10.5f)); g.setColour (textMute);
    const juce::String length = timed ? "One cycle: " + Motion::noteNames()[juce::jlimit (0, 6, (int) proc.getParam (ids::mdNote))] + ", locked to the song"
                                      : "One cycle: " + juce::String (1.0f / juce::jmax (0.02f, proc.getParam (ids::mdRate)), 2) + " s";
    g.drawText (length, plot.withY (plot.getBottom() + 4.0f).withHeight (14.0f), juce::Justification::centredLeft);
    if (drawn) g.drawText ("Drag across the plot to draw the curve", plot.withY (plot.getBottom() + 4.0f).withHeight (14.0f), juce::Justification::centredRight);
}

void FXPage::drawChop (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::chOn) > 0.5f;
    const int rate = (int) proc.getParam (ids::chRate);
    const float depth = proc.getParam (ids::chDepth) * 0.01f, shape = proc.getParam (ids::chShape) * 0.01f;
    const auto plot = chopPlot();
    const float beats = Chop::rateBeats (rate);
    const float stepMs = Reverb::beatMs (proc.getHostBpm()) * beats;
    g.setFont (body (11.5f)); g.setColour (textDim);
    if ((int) proc.getParam (ids::chMode) == 1)
    {
        // the pattern: sixteen steps, the ones past the chosen length greyed out
        ChopSettings pattern;
        pattern.length = juce::roundToInt (proc.getParam (ids::chLength)); pattern.gate = proc.getParam (ids::chGate) * 0.01f; pattern.swing = proc.getParam (ids::chSwing) * 0.01f;
        for (int i = 0; i < ChopSettings::kSteps; ++i) pattern.steps[(size_t) i] = proc.getParam (ids::chStep (i)) * 0.01f;
        g.drawText ("Sixteen steps, each a " + Chop::rateNames()[rate] + " note (" + juce::String (juce::roundToInt (stepMs)) + " ms at " + juce::String (juce::roundToInt (proc.getHostBpm()))
                    + " BPM). Click a step to switch it, drag up or down for its level.", r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
        const float w = plot.getWidth() / (float) ChopSettings::kSteps;
        const float now = proc.getChop().getPhase() * (float) pattern.length;
        for (int i = 0; i < ChopSettings::kSteps; ++i)
        {
            const auto cell = juce::Rectangle<float> (plot.getX() + w * (float) i, plot.getY(), w, plot.getHeight()).reduced (2.0f, 0.0f);
            const bool used = i < pattern.length, playing = on && used && (int) now == i;
            g.setColour (control.withAlpha (used ? 1.0f : 0.4f)); g.fillRoundedRectangle (cell, 3.0f);
            const float level = pattern.steps[(size_t) i];
            if (level > 0.005f)
            {
                // the lit part is as wide as the gate leaves the step open
                auto lit = cell.withTop (cell.getBottom() - cell.getHeight() * level).withWidth (cell.getWidth() * juce::jlimit (0.1f, 1.0f, pattern.gate));
                g.setColour (! used ? textMute.withAlpha (0.35f) : playing ? yellow : on ? yellow.withAlpha (0.55f) : textMute);
                g.fillRoundedRectangle (lit, 3.0f);
            }
            if (playing) { g.setColour (text); g.fillRect (cell.getX(), cell.getBottom() + 3.0f, cell.getWidth(), 2.0f); }
            if (i % 4 == 0)
            {
                g.setColour (textMute); g.setFont (body (10.0f));
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (cell.getX(), plot.getBottom() + 6.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
            }
        }
        return;
    }
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
    g.drawText ("One bar. Each step is a " + Chop::rateNames()[rate] + " note, " + juce::String (juce::roundToInt (stepMs)) + " ms at " + juce::String (juce::roundToInt (proc.getHostBpm()))
                + " BPM: on for half of it, off for the other half. Pattern gives every step its own level.", r.reduced (16.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft, true);
}

// Drawing in the display. Returns true if the pointer was used.
bool FXPage::drawAt (juce::Point<int> where, bool firstTouch)
{
    const auto p = where.toFloat();
    if (firstTouch)
    {
        drawKind = 0;
        if (selected == 3 && (int) proc.getParam (ids::hmMode) != 0)
        {
            for (int v = 0; v < 4 && drawKind == 0; ++v)
                for (int c = 0; c < 7; ++c)
                    if (voiceCell (v, c).expanded (2.0f).contains (p))
                    {
                        cellVoice = v; cellColumn = c; dragFrom = where; cellStart = proc.getParam (voiceParam (v, c));
                        if (c == 0) { proc.setParam (ids::hmVoiceOn (v), cellStart > 0.5f ? 0.0f : 1.0f); repaint (well); return true; }
                        drawKind = 3;
                        break;
                    }
            if (drawKind == 0 && (int) proc.getParam (ids::hmMode) == 2)
                for (int slot = 0; slot < 16 && drawKind == 0; ++slot)
                    for (int note = 0; note < 12; ++note)
                        if (chordCell (slot, note).contains (p))
                        {
                            // the first cell touched decides whether this stroke adds notes or takes them away
                            chordPaint = ((juce::roundToInt (proc.getParam (ids::hmChord (slot))) >> note) & 1) == 0;
                            drawKind = 4;
                            break;
                        }
        }
        else if (selected == 7 && (int) proc.getParam (ids::mdShape) == 5 && motionPlot().expanded (8.0f).contains (p)) drawKind = 1;
        else if (selected == 8 && (int) proc.getParam (ids::chMode) == 1 && chopPlot().expanded (2.0f, 8.0f).contains (p)) drawKind = 2;
        if (drawKind == 0) return false;
    }
    if (drawKind == 1)
    {
        const auto plot = motionPlot();
        const float depth = 0.12f + 0.88f * proc.getParam (ids::mdDepth) * 0.01f;
        const int point = juce::jlimit (0, MotionSettings::kPoints - 1, juce::roundToInt ((p.x - plot.getX()) / plot.getWidth() * (float) MotionSettings::kPoints));
        const float value = 100.0f * juce::jlimit (0.0f, 1.0f, 0.5f + ((plot.getBottom() - p.y) / plot.getHeight() - 0.5f) / depth);
        if (firstTouch || drawLast < 0) { drawLast = point; drawLastValue = value; }
        // every point between the last one touched and this one, on a line between the two
        const int stepBy = point >= drawLast ? 1 : -1, count = std::abs (point - drawLast);
        for (int k = 0; k <= count; ++k)
            proc.setParam (ids::mdPoint (drawLast + stepBy * k), count == 0 ? value : drawLastValue + (value - drawLastValue) * (float) k / (float) count);
        drawLast = point; drawLastValue = value;
        repaint (well);
        return true;
    }
    if (drawKind == 3)
    {
        // a box in the voice table: drag sideways. The interval moves a step every 14 pixels; the others cross their range in about 200.
        const auto id = voiceParam (cellVoice, cellColumn);
        const float dx = (float) (where.x - dragFrom.x);
        if (cellColumn == 1) proc.setParam (id, (float) juce::jlimit (-14, 14, juce::roundToInt (cellStart) + juce::roundToInt (dx / 14.0f)));
        else if (auto* param = proc.apvts.getParameter (id))
        {
            const auto range = param->getNormalisableRange();
            proc.setParam (id, juce::jlimit (range.start, range.end, cellStart + dx / 200.0f * (range.end - range.start)));
        }
        repaint (well);
        return true;
    }
    if (drawKind == 4)
    {
        for (int slot = 0; slot < 16; ++slot)
            for (int note = 0; note < 12; ++note)
                if (chordCell (slot, note).contains (p))
                {
                    const int chord = juce::roundToInt (proc.getParam (ids::hmChord (slot)));
                    proc.setParam (ids::hmChord (slot), (float) (chordPaint ? (chord | (1 << note)) : (chord & ~(1 << note))));
                    // drawing past the end of the loop makes the loop longer
                    if (chordPaint && slot >= juce::roundToInt (proc.getParam (ids::hmChordCount))) proc.setParam (ids::hmChordCount, (float) (slot + 1));
                }
        repaint (well);
        return true;
    }
    if (drawKind == 2)
    {
        const auto plot = chopPlot();
        const int step = juce::jlimit (0, ChopSettings::kSteps - 1, (int) ((p.x - plot.getX()) / plot.getWidth() * (float) ChopSettings::kSteps));
        const float height = 100.0f * juce::jlimit (0.0f, 1.0f, (plot.getBottom() - p.y) / plot.getHeight());
        if (firstTouch)
        {
            // a click on the lit part of a step clears it and goes on clearing; anywhere else sets the level to where the pointer is
            const float have = proc.getParam (ids::chStep (step));
            drawClearing = have > 0.5f && height < have + 6.0f;
            dragFrom = where;
        }
        if (drawClearing && std::abs (where.y - dragFrom.y) > 5) drawClearing = false;      // dragging up or down sets a level after all
        const float level = drawClearing ? 0.0f : juce::jmax (8.0f, height > 92.0f ? 100.0f : height);
        if (firstTouch || drawLast < 0) drawLast = step;
        for (int k = juce::jmin (drawLast, step); k <= juce::jmax (drawLast, step); ++k) proc.setParam (ids::chStep (k), level);
        drawLast = step;
        repaint (well);
        return true;
    }
    return false;
}

//==================================================================== interaction and layout
void FXPage::mouseDown (const juce::MouseEvent& e)
{
    if (drawAt (e.getPosition(), true)) { dragTool = -1; return; }
    for (int i = 0; i < (int) fxModules().size(); ++i)
        if (rowBounds (i).contains (e.getPosition()))
        {
            select (i);
            setStatus (fxModules()[(size_t) i].name + ": " + fxModules()[(size_t) i].blurb);
            dragTool = chainTool (i); dragFrom = e.getPosition(); dragging = false;
            return;
        }
    dragTool = -1;
}

void FXPage::mouseDrag (const juce::MouseEvent& e)
{
    if (drawKind != 0) { drawAt (e.getPosition(), false); return; }
    if (dragTool < 0) return;
    if (! dragging && std::abs (e.getPosition().y - dragFrom.y) < 6) return;
    dragging = true;
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    // the place under the pointer, counted among the six in-line rows
    const int top = railList.getY() + 3 * kRowH + kOrderGap;
    const int place = juce::jlimit (0, fxorder::kCount - 1, (e.getPosition().y - top) / kRowH);
    const auto now = fxorder::decode (juce::roundToInt (proc.getParam (ids::fxOrder)));
    const auto wanted = fxorder::moved (now, dragTool, place);
    if (wanted != now) proc.setParam (ids::fxOrder, (float) fxorder::encode (wanted));
    repaint (rail);
}

void FXPage::mouseDoubleClick (const juce::MouseEvent& e)
{
    // a box in Harmony's voice table goes back to how it came
    if (selected != 3 || (int) proc.getParam (ids::hmMode) == 0) return;
    for (int v = 0; v < 4; ++v)
        for (int c = 1; c < 7; ++c)
            if (voiceCell (v, c).contains (e.getPosition().toFloat()))
                if (auto* param = proc.apvts.getParameter (voiceParam (v, c)))
                {
                    proc.setParam (voiceParam (v, c), param->getNormalisableRange().convertFrom0to1 (param->getDefaultValue()));
                    repaint (well);
                    return;
                }
}

void FXPage::mouseUp (const juce::MouseEvent&)
{
    drawKind = 0; drawLast = -1;
    if (dragging)
    {
        juce::StringArray names;
        const auto order = fxorder::decode (juce::roundToInt (proc.getParam (ids::fxOrder)));
        for (int tool : order)
            for (int i = 0; i < (int) fxModules().size(); ++i) if (chainTool (i) == tool) names.add (fxModules()[(size_t) i].name);
        setStatus ("The sound now goes through " + names.joinIntoString (", then ") + ". The order is saved with the song and in your chain presets.");
    }
    dragging = false; dragTool = -1;
    repaint (rail);
}

void FXPage::mouseMove (const juce::MouseEvent& e)
{
    juce::String key;
    int row = -1;
    for (int i = 0; i < (int) fxModules().size(); ++i) if (rowBounds (i).contains (e.getPosition())) row = i;
    if (row != hoverRow) { hoverRow = row; repaint (rail); }
    if (row >= 0) key = fxModules()[(size_t) row].onParam;
    else if (orderNote.contains (e.getPosition())) key = "fxOrder";
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
    railList = left.removeFromTop (kRowH * (int) fxModules().size() + kOrderGap);
    orderNote = { railList.getX(), railList.getY() + 3 * kRowH, railList.getWidth(), kOrderGap };
    left.removeFromTop (8);
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
    if (! m.more.empty()) { head.removeFromRight (4); moreButton.setBounds (head.removeFromRight (118).withSizeKeepingCentre (118, 26)); head.removeFromRight (12); titleArea = head; }
    auto place = [] (juce::Rectangle<int> area, const std::vector<Knob*>& list)
    {
        const int count = (int) list.size();
        const int cell = juce::jmin (118, (area.getWidth() - (count - 1) * 8) / juce::jmax (1, count));
        auto row = area.withSizeKeepingCentre (cell * count + 8 * (count - 1), area.getHeight());
        for (auto* k : list) { k->setBounds (row.removeFromLeft (cell)); row.removeFromLeft (8); }
    };
    if (showMore && ! m.more.empty()) { place (p.removeFromBottom (96), m.more); p.removeFromBottom (6); }
    place (p.removeFromBottom (96), m.knobs);
    p.removeFromBottom (10);
    if (! m.top.empty())
    {
        // left to right, starting a second row when the next one does not fit
        auto top = p.removeFromTop (28);
        for (size_t i = 0; i < m.top.size(); ++i)
        {
            if (! m.top[i]->isVisible()) continue;          // a control that belongs to another mode of the tool takes no room
            const int w = juce::jmin (m.topWidths[i], p.getWidth());
            if (w > top.getWidth()) { p.removeFromTop (8); top = p.removeFromTop (28); }
            m.top[i]->setBounds (top.removeFromLeft (w));
            top.removeFromLeft (12);
        }
        p.removeFromTop (10);
    }
    well = p;
}
}
