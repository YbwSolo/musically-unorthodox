#include "TunePage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

namespace
{
const char* const kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
juce::String midiName (int note) { return juce::String (kNoteNames[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1); }
juce::String centsText (float semitones)
{
    const int cents = juce::roundToInt (semitones * 100.0f);
    if (cents == 0) return "in tune";
    return juce::String (std::abs (cents)) + (cents < 0 ? " cents flat" : " cents sharp");
}
constexpr float kRuler = 40.0f, kTimeRuler = 16.0f;
}

//==================================================================== graph view
TuneGraphView::TuneGraphView (MUProcessor& p, std::function<void (const juce::String&)> status) : proc (p), setStatus (std::move (status))
{
    setHelp (*this, "tune.graph");
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

juce::String TuneGraphView::describe (const GraphNote& n) const
{
    const int nearest = (int) std::lround (n.sung);
    juce::String s = "Sung as " + midiName (nearest) + (std::abs (n.sung - (float) nearest) < 0.05f ? juce::String() : ", " + centsText (n.sung - (float) nearest));
    if (std::abs (n.pitch - n.sung) > 0.005f)
    {
        const int to = (int) std::lround (n.pitch);
        s << ". Moved to " << midiName (to) << (std::abs (n.pitch - (float) to) < 0.01f ? juce::String() : " " + juce::String (juce::roundToInt ((n.pitch - (float) to) * 100.0f)) + " cents");
    }
    else s << ". Not moved";
    if (n.straighten > 0.005f) s << ", straightened " << juce::roundToInt (n.straighten * 100.0f) << " %";
    return s + ".";
}

void TuneGraphView::frameContent()
{
    auto& g = proc.getGraph();
    const juce::ScopedLock sl (proc.getGraphLock());
    const auto& notes = g.getNotes();
    if (notes.empty() || plot.isEmpty()) return;
    std::vector<float> pitches;
    for (auto& n : notes) pitches.push_back (n.sung);
    std::nth_element (pitches.begin(), pitches.begin() + (std::ptrdiff_t) (pitches.size() / 2), pitches.end());
    float lo = 1000.0f, hi = -1000.0f;
    for (auto& n : notes) { lo = juce::jmin (lo, n.sung, n.pitch); hi = juce::jmax (hi, n.sung, n.pitch); }
    viewSpan = juce::jlimit (13.0f, 26.0f, hi - lo + 5.0f);
    viewLow = (hi - lo + 5.0f <= 26.0f) ? lo - 2.5f : pitches[pitches.size() / 2] - viewSpan * 0.5f;
    const double first = notes.front().start, last = notes.back().end;
    pxPerFrame = juce::jlimit (0.15, 3.0, (double) plot.getWidth() / juce::jmax (200.0, (last - first) * 1.08));
    viewStart = first - 12.0 / pxPerFrame;
    framed = true;
}

void TuneGraphView::showAll() { frameContent(); repaint(); }

void TuneGraphView::zoom (float factor)
{
    const double centre = frameAtX (plot.getCentreX());
    pxPerFrame = juce::jlimit (0.1, 8.0, pxPerFrame * (double) factor);
    viewStart = centre - (double) (plot.getWidth() * 0.5f) / pxPerFrame;
    repaint();
}

void TuneGraphView::tick (float dt)
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (kRuler).withTrimmedTop (kTimeRuler);
    {
        const juce::ScopedLock sl (proc.getGraphLock());
        shown = proc.getGraph().getNotes();
    }
    if ((int) shown.size() != lastCount)
    {
        // the notes have been rebuilt, so the old selection points at the wrong ones
        if (lastCount >= 0 && ! selected.empty()) { selected.clear(); if (onSelectionChanged) onSelectionChanged(); }
        lastCount = (int) shown.size();
        if (! framed && ! shown.empty()) frameContent();
    }
    const bool capturing = proc.isCapturing();
    if (wasCapturing && ! capturing)
    {
        frameContent();
        setStatus (shown.empty() ? juce::String ("Nothing with a clear pitch was heard. Check Tune is on this vocal track and that the song was playing.")
                                 : juce::String ((int) shown.size()) + " notes captured. Press Correct all to put them on the scale, or drag any note up or down. Double-click a note to snap just that one.");
    }
    wasCapturing = capturing;
    pulse = std::fmod (pulse + dt, 1.2f);

    if (follow && proc.isHostPlaying() && ! dragging && ! panning)
    {
        const double head = proc.getHostSeconds() * (double) TuneGraph::kRate;
        const double left = viewStart, right = frameAtX (plot.getRight());
        if (head < left || head > right - (right - left) * 0.15) viewStart = head - (right - left) * 0.2;
    }
    repaint();
}

int TuneGraphView::noteUnder (juce::Point<float> p) const
{
    const float rowH = plot.getHeight() / viewSpan;
    for (int i = (int) shown.size(); --i >= 0;)
    {
        const auto& n = shown[(size_t) i];
        const auto r = juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - rowH * 0.5f, juce::jmax (4.0f, (float) ((n.end - n.start) * pxPerFrame)), rowH).expanded (2.0f, 1.0f);
        if (r.contains (p)) return i;
    }
    return -1;
}

void TuneGraphView::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    drawWell (g, all);
    if (plot.isEmpty()) return;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = (int) proc.getParam (ids::tuneNotes);
    const float rowH = plot.getHeight() / viewSpan;
    auto& graph = proc.getGraph();

    g.saveState();
    g.reduceClipRegion (all.withTrimmedTop (kTimeRuler).toNearestInt());
    // one row per note; the rows of the scale are the ones a note can be snapped to
    for (int n = (int) std::floor (viewLow) - 1; n <= (int) std::ceil (viewLow + viewSpan) + 1; ++n)
    {
        const bool allowed = Tuner::noteAllowed (n, key, sc, mask);
        const auto row = juce::Rectangle<float> (all.getX(), yFor ((float) n) - rowH * 0.5f, all.getWidth(), rowH);
        if (allowed) { g.setColour (raised.withAlpha (0.7f)); g.fillRect (row.withTrimmedLeft (kRuler).reduced (0.0f, 0.5f)); }
        g.setFont (body (rowH > 15.0f ? 10.5f : 9.0f)); g.setColour (allowed ? textDim : textMute.withAlpha (0.6f));
        if (rowH > 11.0f || n % 12 == 0) g.drawText (midiName (n), row.withWidth (kRuler - 6.0f).translated (6.0f, 0.0f), juce::Justification::centredLeft);
    }
    g.restoreState();

    // time along the top, in minutes and seconds of the song
    const double framesPerLabel = pxPerFrame > 1.2 ? 100.0 : pxPerFrame > 0.4 ? 200.0 : pxPerFrame > 0.2 ? 500.0 : 1000.0;
    g.setFont (body (10.0f));
    for (double f = std::ceil (viewStart / framesPerLabel) * framesPerLabel; f < frameAtX (plot.getRight()); f += framesPerLabel)
    {
        if (f < 0.0) continue;
        const float x = xFor (f);
        g.setColour (line); g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        const int seconds = (int) (f / (double) TuneGraph::kRate);
        g.setColour (textMute);
        g.drawText (juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2), juce::Rectangle<float> (x + 4.0f, all.getY() + 2.0f, 50.0f, kTimeRuler - 2.0f), juce::Justification::centredLeft);
    }

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    const int from = juce::jmax (0, (int) std::floor (viewStart)), to = juce::jmin (TuneGraph::kMaxFrames - 1, (int) std::ceil (frameAtX (plot.getRight())));
    const int stride = juce::jmax (1, (int) std::floor (1.0 / pxPerFrame));

    // the notes
    for (size_t i = 0; i < shown.size(); ++i)
    {
        const auto& n = shown[i];
        if (n.end < from || n.start > to) continue;
        const float x = xFor (n.start), w = juce::jmax (3.0f, (float) ((n.end - n.start) * pxPerFrame) - 1.0f);
        const bool isSelected = selected.count ((int) i) > 0, moved = n.isEdited();
        if (std::abs (n.pitch - n.sung) > 0.02f)
        {
            // where it was sung, so the size of the move is plain to see
            const auto ghost = juce::Rectangle<float> (x, yFor (n.sung) - rowH * 0.32f, w, rowH * 0.64f);
            g.setColour (textMute.withAlpha (0.7f)); g.drawRoundedRectangle (ghost.reduced (0.5f), 3.0f, 1.0f);
        }
        const auto block = juce::Rectangle<float> (x, yFor (n.pitch) - rowH * 0.36f, w, rowH * 0.72f);
        g.setColour (moved ? yellow.withAlpha (isSelected ? 0.95f : 0.72f) : text.withAlpha (isSelected ? 0.5f : 0.22f));
        g.fillRoundedRectangle (block, 3.5f);
        if (isSelected) { g.setColour (text); g.drawRoundedRectangle (block.expanded (1.0f), 4.0f, 1.6f); }
    }

    // the pitch as sung (grey) and where it is being moved to (yellow)
    auto trace = [&] (bool target, juce::Colour colour, float thickness)
    {
        juce::Path path; bool open = false; float last = 0.0f;
        for (int f = from; f <= to; f += stride)
        {
            const float v = target ? graph.targetAt (f) : graph.sungAt (f);
            if (v <= 0.0f || (open && std::abs (v - last) > 2.5f)) { open = false; if (v <= 0.0f) continue; }
            const float x = xFor (f), y = yFor (v);
            if (open) path.lineTo (x, y); else { path.startNewSubPath (x, y); open = true; }
            last = v;
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    trace (false, textDim.withAlpha (0.85f), 1.2f);
    trace (true, juce::Colours::black.withAlpha (0.55f), 3.4f);
    trace (true, yellow, 1.6f);

    // where the song is
    const float headX = xFor (proc.getHostSeconds() * (double) TuneGraph::kRate);
    if (headX >= plot.getX() && headX <= plot.getRight())
    {
        g.setColour (proc.isCapturing() ? yellow : text.withAlpha (proc.isHostPlaying() ? 0.9f : 0.45f));
        g.fillRect (headX - 0.75f, plot.getY(), 1.5f, plot.getHeight());
    }
    g.restoreState();

    if (proc.isCapturing())
    {
        auto tag = plot.reduced (12.0f, 10.0f).removeFromTop (20.0f);
        g.setColour (yellow.withAlpha (0.45f + 0.55f * std::abs (std::sin (pulse / 1.2f * juce::MathConstants<float>::pi))));
        g.fillEllipse (tag.removeFromLeft (12.0f).withSizeKeepingCentre (10.0f, 10.0f));
        g.setFont (bodyBold (12.0f)); g.setColour (yellow);
        g.drawText (proc.isHostPlaying() ? "  Capturing. Stop the song when the part has played." : "  Ready. Press play in your music software.", tag, juce::Justification::centredLeft);
    }
    else if (shown.empty())
    {
        const auto box = plot.withSizeKeepingCentre (juce::jmin (560.0f, plot.getWidth() - 40.0f), 110.0f);
        g.setFont (headline (24.0f)); g.setColour (text);
        g.drawText ("Capture a take to edit its notes", box.withHeight (30.0f), juce::Justification::centred);
        drawWrapped (g, "The plugin cannot see the audio file, so it writes the notes down as it hears them. Press Capture, play the part of the song you want to tune, then stop. "
                        "Every note appears here as a block you can drag to another pitch.",
                     box.withTrimmedTop (38.0f), body (12.5f), textDim);
    }
}

void TuneGraphView::mouseDown (const juce::MouseEvent& e)
{
    downAt = e.position; moved = false;
    const int hit = noteUnder (e.position);
    if (hit >= 0)
    {
        if (e.mods.isShiftDown()) { if (selected.count (hit)) selected.erase (hit); else selected.insert (hit); }
        else if (! selected.count (hit)) { selected.clear(); selected.insert (hit); }
        if (onSelectionChanged) onSelectionChanged();
        if (e.mods.isPopupMenu()) { showMenu (hit, (int) frameAtX (e.position.x)); return; }
        dragging = true;
        dragFrom.clear();
        for (int i : selected) dragFrom.push_back (i < (int) shown.size() ? shown[(size_t) i].pitch : 0.0f);
        if (hit < (int) shown.size()) setStatus (describe (shown[(size_t) hit]) + " Drag up or down to move it; hold Option for fine steps.");
        return;
    }
    if (! e.mods.isShiftDown() && ! selected.empty()) { selected.clear(); if (onSelectionChanged) onSelectionChanged(); }
    panning = true; panStart = viewStart; panLow = viewLow;
}

void TuneGraphView::mouseDrag (const juce::MouseEvent& e)
{
    const float rowH = plot.getHeight() / viewSpan;
    if (dragging)
    {
        const float dy = e.position.y - downAt.y;
        if (std::abs (dy) < 3.0f && ! moved) return;
        moved = true;
        const float semis = -dy / rowH;
        const juce::ScopedLock sl (proc.getGraphLock());
        auto& notes = proc.getGraph().getNotes();
        int k = 0;
        for (int i : selected)
        {
            if (i < (int) notes.size() && k < (int) dragFrom.size())
            {
                const float wanted = dragFrom[(size_t) k] + semis;
                // whole notes by default, so a dragged note always lands in tune
                notes[(size_t) i].pitch = juce::jlimit (24.0f, 108.0f, e.mods.isAltDown() ? wanted : std::round (wanted));
            }
            ++k;
        }
        proc.getGraph().rasterise();
    }
    else if (panning)
    {
        viewStart = panStart - (double) (e.position.x - downAt.x) / pxPerFrame;
        viewLow = panLow + (e.position.y - downAt.y) / rowH;
    }
}

void TuneGraphView::mouseUp (const juce::MouseEvent&)
{
    if (dragging && moved && ! selected.empty())
    {
        const juce::ScopedLock sl (proc.getGraphLock());
        const auto& notes = proc.getGraph().getNotes();
        const int i = *selected.begin();
        if (i < (int) notes.size()) setStatus (describe (notes[(size_t) i]));
    }
    dragging = false; panning = false;
}

void TuneGraphView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = noteUnder (e.position);
    if (hit < 0) return;
    selected.clear(); selected.insert (hit);
    if (onSelectionChanged) onSelectionChanged();
    bool snapped = false;
    {
        const juce::ScopedLock sl (proc.getGraphLock());
        auto& notes = proc.getGraph().getNotes();
        if (hit < (int) notes.size())
        {
            auto& n = notes[(size_t) hit];
            const float onScale = (float) Tuner::nearestAllowed (n.sung, (int) proc.getParam (ids::tuneKey), (int) proc.getParam (ids::tuneScale), (int) proc.getParam (ids::tuneNotes));
            snapped = std::abs (n.pitch - onScale) > 0.005f;
            n.pitch = snapped ? onScale : n.sung;          // a second double-click puts it back
            proc.getGraph().rasterise();
            setStatus (describe (n));
        }
    }
}

void TuneGraphView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double under = frameAtX (e.position.x);
        pxPerFrame = juce::jlimit (0.1, 8.0, pxPerFrame * std::pow (2.0, (double) w.deltaY * 2.0));
        viewStart = under - (double) (e.position.x - plot.getX()) / pxPerFrame;
    }
    else
    {
        viewStart -= (double) w.deltaX * 260.0 / pxPerFrame;
        viewLow += w.deltaY * 7.0f;
    }
    repaint();
}

void TuneGraphView::showMenu (int note, int frame)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addItem (1, "Snap to the scale");
    menu.addItem (2, "Back to how it was sung");
    menu.addSeparator();
    menu.addItem (3, "Split here");
    menu.addItem (4, "Join with the next note");
    menu.addSeparator();
    menu.addItem (5, "Straighten fully");
    menu.addItem (6, "Leave its movement alone");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
        [safe = juce::Component::SafePointer<TuneGraphView> (this), note, frame] (int result)
        {
            if (safe == nullptr || result <= 0) return;
            auto& self = *safe;
            switch (result)
            {
                case 1: self.snapSelection(); break;
                case 2: self.resetSelection(); break;
                case 3:
                {
                    const juce::ScopedLock sl (self.proc.getGraphLock());
                    self.proc.getGraph().splitNote (note, frame);
                    break;
                }
                case 4: self.joinSelection(); break;
                case 5: self.setStraighten (1.0f); break;
                case 6: self.setStraighten (0.0f); break;
                default: break;
            }
            if (self.onSelectionChanged) self.onSelectionChanged();
        });
}

void TuneGraphView::snapSelection()
{
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = (int) proc.getParam (ids::tuneNotes);
    const juce::ScopedLock sl (proc.getGraphLock());
    auto& notes = proc.getGraph().getNotes();
    for (int i = 0; i < (int) notes.size(); ++i)
        if (selected.empty() || selected.count (i)) notes[(size_t) i].pitch = (float) Tuner::nearestAllowed (notes[(size_t) i].sung, key, sc, mask);
    proc.getGraph().rasterise();
}

void TuneGraphView::resetSelection()
{
    const juce::ScopedLock sl (proc.getGraphLock());
    auto& notes = proc.getGraph().getNotes();
    for (int i = 0; i < (int) notes.size(); ++i)
        if (selected.empty() || selected.count (i)) { notes[(size_t) i].pitch = notes[(size_t) i].sung; notes[(size_t) i].straighten = 0.0f; }
    proc.getGraph().rasterise();
}

void TuneGraphView::splitSelection()
{
    if (selected.size() != 1) { setStatus ("Click one note first, then Split cuts it in two at the song position, or in the middle."); return; }
    const int i = *selected.begin();
    const juce::ScopedLock sl (proc.getGraphLock());
    auto& notes = proc.getGraph().getNotes();
    if (i >= (int) notes.size()) return;
    const int head = (int) (proc.getHostSeconds() * (double) TuneGraph::kRate);
    const int at = head > notes[(size_t) i].start + 4 && head < notes[(size_t) i].end - 4 ? head : (notes[(size_t) i].start + notes[(size_t) i].end) / 2;
    if (proc.getGraph().splitNote (i, at) < 0) setStatus ("That note is too short to split.");
    else setStatus ("Split in two. Each half can now be moved on its own.");
}

void TuneGraphView::joinSelection()
{
    if (selected.empty()) { setStatus ("Click a note first, then Join merges it with the note after it."); return; }
    const int i = *selected.begin();
    const juce::ScopedLock sl (proc.getGraphLock());
    setStatus (proc.getGraph().mergeWithNext (i) ? "Joined with the next note." : "There is no note directly after that one to join it to.");
}

void TuneGraphView::setStraighten (float v)
{
    const juce::ScopedLock sl (proc.getGraphLock());
    auto& notes = proc.getGraph().getNotes();
    for (int i = 0; i < (int) notes.size(); ++i)
        if (selected.empty() || selected.count (i)) notes[(size_t) i].straighten = juce::jlimit (0.0f, 1.0f, v);
    proc.getGraph().rasterise();
}

void TuneGraphView::setGlide (float ms)
{
    const juce::ScopedLock sl (proc.getGraphLock());
    auto& notes = proc.getGraph().getNotes();
    for (int i = 0; i < (int) notes.size(); ++i)
        if (selected.empty() || selected.count (i)) notes[(size_t) i].glideMs = juce::jlimit (0.0f, 300.0f, ms);
    proc.getGraph().rasterise();
}

bool TuneGraphView::selectionValues (float& straighten01, float& glideMs) const
{
    const juce::ScopedLock sl (proc.getGraphLock());
    const auto& notes = proc.getGraph().getNotes();
    if (notes.empty()) return false;
    const int i = selected.empty() ? 0 : *selected.begin();
    if (i >= (int) notes.size()) return false;
    straighten01 = notes[(size_t) i].straighten; glideMs = notes[(size_t) i].glideMs;
    return true;
}

//==================================================================== page
TunePage::TunePage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      view (p, ids::tuneMode, { "Simple", "Graph" }, ids::tuneMode),
      scale (p, ids::tuneScale, { "Chromatic", "Major", "Minor", "Maj pent", "Min pent", "Custom" }, ids::tuneScale),
      speed (p, ids::tuneSpeed, "Speed", ids::tuneSpeed), hold (p, ids::tuneHold, "Note hold", ids::tuneHold), vibrato (p, ids::tuneVibrato, "Keep vibrato", ids::tuneVibrato),
      amount (p, ids::tuneAmount, "Amount", ids::tuneAmount), formant (p, ids::tuneFormant, "Formant", ids::tuneFormant),
      flex (p, ids::tuneFlex, "Flex", ids::tuneFlex), humanise (p, ids::tuneHumanise, "Humanise", ids::tuneHumanise), slides (p, ids::tuneSlides, "Keep slides", ids::tuneSlides),
      graph (p, setStatus)
{
    section.setPower (p, ids::tuneOn);
    setHelp (section, "tuneOn");
    view.onChange = [this] (int i)
    {
        setStatus (i == 1 ? juce::String ("Graph: Tune now follows the notes you capture and move by hand. Nothing is corrected until you capture a take and edit it.")
                          : juce::String ("Simple: Tune corrects the voice as the song plays, using the key, the speed and the other controls on the right."));
    };

    presets.names = [] { juce::StringArray n; for (int i : modulePresetsFor ("tune")) n.add (modulePresets()[(size_t) i].name); return n; };
    presets.current = [this] { return proc.getModulePresetName ("tune"); };
    presets.enableOwnPresets (proc, [] { return juce::String ("tune"); }, setStatus);
    presets.choose = [this] (int i) { const auto list = modulePresetsFor ("tune"); if (i >= 0 && i < (int) list.size()) setStatus (proc.applyModulePreset (list[(size_t) i])); };

    keyBox.addItemList (Tuner::keyNames(), 1);
    keyBox.setWantsKeyboardFocus (false);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::tuneKey, keyBox);
    setHelp (keyBox, "tuneKey");
    styleButton (findKey, "Find key", false, "tune.find");
    findKey.onClick = [this] { setStatus (proc.findKey()); };
    styleButton (sendKey, "Send key to all tracks", false, "tune.send");
    sendKey.onClick = [this] { setStatus (proc.sendKeyToSession()); };

    // ---- graph tools
    styleButton (capture, "Capture", true, "tune.capture");
    capture.onClick = [this]
    {
        if (proc.isCapturing()) { proc.stopCapture(); return; }
        proc.startCapture();
        setStatus ("Capture is on. Play the part of the song you want to tune, then stop the song. Tune is left off while it listens, so you hear the take as it was sung.");
    };
    styleButton (correct, "Correct all", false, "tune.correct");
    correct.onClick = [this]
    {
        if (graph.hasSelection()) { graph.snapSelection(); setStatus ("The selected notes are on the nearest note of the scale."); }
        else setStatus (proc.correctAllNotes ((float) straighten.getValue() * 0.01f));
        updatingSliders = true; float s = 0, gl = 0; if (graph.selectionValues (s, gl)) straighten.setValue (s * 100.0, juce::dontSendNotification); updatingSliders = false;
    };
    styleButton (resetNotes, "Reset", false, "tune.reset");
    resetNotes.onClick = [this] { graph.resetSelection(); setStatus (graph.hasSelection() ? "The selected notes are back to how they were sung." : "Every note is back to how it was sung."); };
    styleButton (clearNotes, "Clear", false, "tune.clear");
    clearNotes.onClick = [this] { proc.clearGraph(); graph.clearSelection(); setStatus ("The captured notes have been cleared. Press Capture to start again."); };
    styleButton (splitNote, "Split", false, "tune.split");
    splitNote.onClick = [this] { graph.splitSelection(); };
    styleButton (joinNotes, "Join", false, "tune.split");
    joinNotes.onClick = [this] { graph.joinSelection(); };
    styleButton (zoomOut, "-", false, "tune.zoom"); zoomOut.onClick = [this] { graph.zoom (0.6f); };
    styleButton (zoomIn, "+", false, "tune.zoom");  zoomIn.onClick = [this] { graph.zoom (1.0f / 0.6f); };
    styleButton (fit, "Fit", false, "tune.zoom");   fit.onClick = [this] { graph.showAll(); };
    followPlay.setButtonText ("Follow");
    followPlay.setWantsKeyboardFocus (false);
    followPlay.setToggleState (true, juce::dontSendNotification);
    followPlay.onClick = [this] { graph.setFollow (followPlay.getToggleState()); };
    setHelp (followPlay, "tune.zoom");

    for (auto* s : { &straighten, &glide })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setWantsKeyboardFocus (false);
    }
    straighten.setRange (0.0, 100.0, 1.0); straighten.setValue (35.0, juce::dontSendNotification); setHelp (straighten, "tune.straighten");
    glide.setRange (0.0, 200.0, 1.0); glide.setValue (40.0, juce::dontSendNotification); setHelp (glide, "tune.glide");
    straighten.onValueChange = [this] { if (! updatingSliders) graph.setStraighten ((float) straighten.getValue() * 0.01f); };
    glide.onValueChange = [this] { if (! updatingSliders) graph.setGlide ((float) glide.getValue()); };
    graph.onSelectionChanged = [this]
    {
        float s = 0.0f, gl = 40.0f;
        if (graph.hasSelection() && graph.selectionValues (s, gl))
        {
            updatingSliders = true;
            straighten.setValue (s * 100.0, juce::dontSendNotification); glide.setValue (gl, juce::dontSendNotification);
            updatingSliders = false;
        }
    };

    for (auto* c : std::initializer_list<juce::Component*> { &section, &view, &presets, &keyBox, &scale, &findKey, &sendKey, &speed, &hold, &vibrato, &amount, &formant, &flex, &humanise, &slides,
                                                              &graph, &capture, &correct, &resetNotes, &clearNotes, &splitNote, &joinNotes, &zoomOut, &zoomIn, &fit, &followPlay, &straighten, &glide })
        addAndMakeVisible (c);
    showView();
    sendLookAndFeelChange();
}

void TunePage::showView()
{
    const bool graphView = (int) proc.getParam (ids::tuneMode) == 1;
    shownMode = graphView ? 1 : 0;
    for (auto* c : std::initializer_list<juce::Component*> { &speed, &hold, &vibrato, &amount, &formant, &flex, &humanise, &slides }) c->setVisible (! graphView);
    for (auto* c : std::initializer_list<juce::Component*> { &graph, &capture, &correct, &resetNotes, &clearNotes, &splitNote, &joinNotes, &zoomOut, &zoomIn, &fit, &followPlay, &straighten, &glide })
        c->setVisible (graphView);
    repaint();
}

void TunePage::tick (float dt)
{
    if ((int) proc.getParam (ids::tuneMode) != shownMode) showView();
    presets.refresh();
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
    if (out > 0.0f && t.getTargetNote() >= 0)
    {
        centre = approach (centre, out, dt, 0.5f);
        shownTarget = t.getTargetNote();
        sungOff = approach (sungOff, t.getDetectedNote() - (float) shownTarget, dt, 0.05f);
        tunedOff = approach (tunedOff, out - (float) shownTarget, dt, 0.05f);
    }
    else if (out > 0.0f) centre = approach (centre, out, dt, 0.5f);

    if (shownMode == 1)
    {
        blink = std::fmod (blink + dt, 1.0f);
        const bool capturing = proc.isCapturing();
        if (capture.getButtonText() != (capturing ? "Stop" : "Capture")) capture.setButtonText (capturing ? "Stop" : "Capture");
        const bool some = graph.hasSelection();
        if (correct.getButtonText() != (some ? "Correct" : "Correct all")) correct.setButtonText (some ? "Correct" : "Correct all");
        graph.tick (dt);
        repaint (straightenLabel.getUnion (glideLabel));
    }
    else for (auto* r : { &pitchWell, &nowArea, &keysArea }) repaint (*r);
}

//==================================================================== drawing: simple view
void TunePage::drawPitch (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    const bool on = proc.getParam (ids::tuneOn) > 0.5f;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = (int) proc.getParam (ids::tuneNotes);
    const float span = 11.0f;                                   // semitones shown
    const float low = centre - span * 0.5f;
    auto yFor = [&] (float note) { return r.getBottom() - (note - low) / span * r.getHeight(); };
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());

    // one row per note; the rows of the chosen scale are the ones a voice can land on
    const float rowH = r.getHeight() / span;
    for (int n = (int) std::floor (low) - 1; n <= (int) std::ceil (low + span) + 1; ++n)
    {
        const bool allowed = Tuner::noteAllowed (n, key, sc, mask);
        const auto row = juce::Rectangle<float> (r.getX(), yFor ((float) n) - rowH * 0.5f, r.getWidth(), rowH);
        if (allowed) { g.setColour (n == shownTarget && on ? yellow.withAlpha (0.10f) : raised.withAlpha (0.75f)); g.fillRect (row.reduced (0.0f, 1.0f)); }
        g.setColour (line); g.fillRect (r.getX(), yFor ((float) n), r.getWidth(), 1.0f);
        g.setFont (body (10.5f)); g.setColour (allowed ? textDim : textMute.withAlpha (0.6f));
        g.drawText (midiName (n), row.withWidth (34.0f).translated (6.0f, 0.0f), juce::Justification::centredLeft);
    }

    auto trace = [&] (const std::array<float, kHistory>& h, juce::Colour colour, float thickness)
    {
        juce::Path path; bool open = false; float last = 0.0f;
        for (int i = 0; i < kHistory; ++i)
        {
            const float note = h[(size_t) ((head + i) % kHistory)];
            // a gap in the singing, or a leap to another note, starts a new line rather than drawing a spike
            if (note <= 0.0f || (open && std::abs (note - last) > 2.5f)) { open = false; if (note <= 0.0f) continue; }
            const float x = r.getX() + 40.0f + (r.getWidth() - 46.0f) * (float) i / (float) (kHistory - 1), y = yFor (note);
            if (open) path.lineTo (x, y); else { path.startNewSubPath (x, y); open = true; }
            last = note;
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    trace (sung, textDim.withAlpha (0.8f), 1.3f);
    if (on) trace (tuned, yellow, 2.2f);
    g.restoreState();

    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText (on ? "Grey: as sung    Yellow: after Tune" : "Switch Tune on to see the pitch as it is sung.", r.reduced (10.0f, 6.0f).removeFromBottom (14.0f), juce::Justification::centredRight);
    if (proc.isMasterInstance())
    {
        const auto note = r.reduced (16.0f, 14.0f).removeFromTop (40.0f).withTrimmedLeft (36.0f);
        drawWrapped (g, "This instance is the Master. Tune works on one voice at a time, so leave it off here. Press Find key while the song plays, then Send key to all tracks.",
                     note.withWidth (juce::jmin (note.getWidth(), 520.0f)), body (12.0f), yellow);
    }
}

void TunePage::drawNow (juce::Graphics& g, juce::Rectangle<float> r)
{
    drawWell (g, r);
    auto inner = r.reduced (16.0f, 12.0f);
    const bool on = proc.getParam (ids::tuneOn) > 0.5f;
    const bool live = on && proc.getTuner().getOutputNote() > 0.0f && shownTarget >= 0;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale);

    auto top = inner.removeFromTop (78.0f);
    g.setFont (headline (70.0f)); g.setColour (live ? yellow : textMute);
    g.drawText (! on ? juce::String ("OFF") : shownTarget >= 0 ? midiName (shownTarget) : juce::String ("..."), top.removeFromLeft (170.0f), juce::Justification::centredLeft);
    g.setFont (bodyBold (13.0f)); g.setColour (text);
    g.drawText (juce::String (kNoteNames[key]) + " " + Tuner::scaleNames()[sc].toLowerCase(), top.removeFromTop (30.0f), juce::Justification::bottomRight);
    g.setFont (body (12.0f)); g.setColour (textDim);
    juce::String state = ! on ? "Tune is off" : ! live ? "Waiting for a note" : "Sung " + centsText (sungOff);
    g.drawText (state, top.removeFromTop (20.0f), juce::Justification::centredRight);
    if (live)
    {
        const int cents = juce::roundToInt ((tunedOff - sungOff) * 100.0f);
        g.setColour (textMute);
        g.drawText (cents == 0 ? juce::String ("left where it was") : "moved " + juce::String (cents > 0 ? "up " : "down ") + juce::String (std::abs (cents)) + " cents", top.removeFromTop (18.0f), juce::Justification::centredRight);
    }

    // how far off the note the voice is: flat to the left, sharp to the right, the note itself in the middle
    inner.removeFromTop (10.0f);
    auto ruler = inner.removeFromTop (30.0f);
    const float mid = ruler.getCentreX(), half = ruler.getWidth() * 0.5f - 4.0f, y = ruler.getY() + 12.0f;
    g.setColour (control); g.fillRoundedRectangle (ruler.getX(), y - 2.0f, ruler.getWidth(), 4.0f, 2.0f);
    for (int c = -50; c <= 50; c += 25)
    {
        const float x = mid + half * (float) c / 50.0f;
        g.setColour (c == 0 ? yellow : lineHi); g.fillRect (x - 0.75f, y - (c == 0 ? 9.0f : 5.0f), 1.5f, c == 0 ? 18.0f : 10.0f);
    }
    if (live)
    {
        auto marker = [&] (float semitones, juce::Colour colour, float size)
        {
            const float x = mid + half * juce::jlimit (-1.0f, 1.0f, semitones * 2.0f);
            g.setColour (colour); g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ x, y }));
        };
        marker (sungOff, textDim, 11.0f);
        marker (tunedOff, yellow, 13.0f);
    }
    g.setFont (body (10.0f)); g.setColour (textMute);
    auto captions = inner.removeFromTop (14.0f);
    g.drawText ("flat", captions, juce::Justification::centredLeft);
    g.drawText ("sharp", captions, juce::Justification::centredRight);
    g.drawText ("grey: as sung   yellow: after Tune", captions, juce::Justification::centred);
}

juce::Rectangle<float> TunePage::keyBounds (int pc) const
{
    static const int whiteIndex[] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const int afterWhite[] = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };      // a black key sits on the join after this white key
    const auto area = keysArea.toFloat().withTrimmedBottom (18.0f);
    const float w = area.getWidth() / 7.0f;
    if (whiteIndex[pc] >= 0) return { area.getX() + w * (float) whiteIndex[pc], area.getY(), w, area.getHeight() };
    const float bw = w * 0.58f;
    return { area.getX() + w * (float) (afterWhite[pc] + 1) - bw * 0.5f, area.getY(), bw, area.getHeight() * 0.6f };
}

void TunePage::drawKeys (juce::Graphics& g, juce::Rectangle<float> r)
{
    const bool on = proc.getParam (ids::tuneOn) > 0.5f;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale), mask = Tuner::scaleMask (key, sc, (int) proc.getParam (ids::tuneNotes));
    const int singing = on && proc.getTuner().getOutputNote() > 0.0f && shownTarget >= 0 ? shownTarget % 12 : -1;
    for (int pass = 0; pass < 2; ++pass)
        for (int pc = 0; pc < 12; ++pc)
        {
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            if (black != (pass == 1)) continue;
            const auto k = keyBounds (pc).reduced (1.5f, 0.0f);
            const bool allowed = (mask >> pc) & 1;
            g.setColour (pc == singing ? yellow : allowed ? (black ? juce::Colour (0xff8f8f8f) : juce::Colour (0xffe6e6e6)) : (black ? juce::Colour (0xff050505) : juce::Colour (0xff1b1b1b)));
            g.fillRoundedRectangle (k, 3.0f);
            g.setColour (lineHi);
            g.drawRoundedRectangle (k, 3.0f, 1.0f);
            g.setFont (bodyBold (black ? 9.5f : 11.0f));
            g.setColour (pc == singing || allowed ? onYellow : textMute);
            g.drawText (kNoteNames[pc], k.withTrimmedBottom (pc == key ? 12.0f : 5.0f), juce::Justification::centredBottom);
            if (pc == key) { g.setColour (pc == singing || allowed ? onYellow : textMute); g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ k.getCentreX(), k.getBottom() - 7.0f })); }
        }
    g.setFont (body (11.0f)); g.setColour (textMute);
    g.drawText ("Lit notes are allowed. Click a note to block or allow it. The dot is the key.", r.withTop (r.getBottom() - 15.0f), juce::Justification::centredLeft);
}

void TunePage::paintOverChildren (juce::Graphics& g)
{
    g.setFont (body (11.5f)); g.setColour (textDim);
    g.drawText ("Key", keyLabel, juce::Justification::centredLeft);
    if (shownMode == 1)
    {
        const bool some = graph.hasSelection();
        g.setFont (body (11.5f)); g.setColour (some ? text : textDim);
        g.drawText ("Straighten " + juce::String (juce::roundToInt (straighten.getValue())) + " %", straightenLabel, juce::Justification::centredLeft);
        g.drawText ("Glide " + juce::String (juce::roundToInt (glide.getValue())) + " ms", glideLabel, juce::Justification::centredLeft);
        g.setFont (body (11.0f)); g.setColour (textMute);
        g.drawText (some ? "Changes apply to the selected notes. Click empty space to select none; then they apply to every note."
                         : "Drag a note to move it. Double-click snaps it to the scale. Right-click for more. Drag empty space to move around; hold Cmd and scroll to zoom.",
                    graphHint, juce::Justification::centredLeft, true);
        return;
    }
    drawPitch (g, pitchWell.toFloat());
    drawNow (g, nowArea.toFloat());
    drawKeys (g, keysArea.toFloat());
}

void TunePage::mouseDown (const juce::MouseEvent& e)
{
    if (shownMode != 0 || ! keysArea.contains (e.getPosition())) return;
    int hit = -1;
    for (int pc : { 1, 3, 6, 8, 10, 0, 2, 4, 5, 7, 9, 11 })           // the black keys lie on top
        if (keyBounds (pc).contains (e.position)) { hit = pc; break; }
    if (hit < 0) return;
    const int key = (int) proc.getParam (ids::tuneKey), sc = (int) proc.getParam (ids::tuneScale);
    int mask = Tuner::scaleMask (key, sc, (int) proc.getParam (ids::tuneNotes)) ^ (1 << hit);
    if (mask == 0) { setStatus ("At least one note has to stay allowed."); return; }
    proc.setParam (ids::tuneNotes, (float) mask);
    proc.setParam (ids::tuneScale, (float) Tuner::kCustomScale);
    setStatus (juce::String (kNoteNames[hit]) + ((mask >> hit) & 1 ? " is allowed again." : " is blocked: the voice will be moved off it to the nearest lit note.")
               + " The scale now reads Custom; choose Major or Minor to go back.");
}

void TunePage::mouseMove (const juce::MouseEvent& e)
{
    juce::String key;
    if (shownMode == 0)
    {
        if (pitchWell.contains (e.getPosition())) key = "tune.display";
        else if (nowArea.contains (e.getPosition())) key = "tune.now";
        else if (keysArea.contains (e.getPosition())) key = "tune.keys";
    }
    getProperties().set ("help", key);
}

void TunePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    section.setBounds (r);
    auto head = juce::Rectangle<int> (r.getX(), r.getY() + 6, r.getWidth() - 12, Section::kHeader - 12);
    presets.setBounds (head.removeFromRight (270));
    head.removeFromRight (12);
    view.setBounds (head.removeFromRight (150));

    auto t = r.withTrimmedTop (Section::kHeader).reduced (12, 0).withTrimmedBottom (12);
    auto keys = t.removeFromTop (28);
    keyLabel = keys.removeFromLeft (30);
    keyBox.setBounds (keys.removeFromLeft (64));
    keys.removeFromLeft (8);
    scale.setBounds (keys.removeFromLeft (470));
    keys.removeFromLeft (8);
    findKey.setBounds (keys.removeFromLeft (84));
    keys.removeFromLeft (6);
    sendKey.setBounds (keys.removeFromLeft (176));
    t.removeFromTop (10);

    // ---- simple view
    {
        auto body = t;
        auto right = body.removeFromRight (380);
        body.removeFromRight (12);
        pitchWell = body;
        nowArea = right.removeFromTop (170);
        right.removeFromTop (10);
        keysArea = right.removeFromTop (104);
        right.removeFromTop (12);
        layoutRow (right.removeFromTop (96), { &speed, &flex, &hold, &slides }, 6);
        right.removeFromTop (8);
        layoutRow (right.removeFromTop (96), { &humanise, &vibrato, &amount, &formant }, 6);
    }
    // ---- graph view
    {
        auto body = t;
        auto bar = body.removeFromTop (28);
        capture.setBounds (bar.removeFromLeft (88));     bar.removeFromLeft (6);
        correct.setBounds (bar.removeFromLeft (88));     bar.removeFromLeft (6);
        resetNotes.setBounds (bar.removeFromLeft (64));  bar.removeFromLeft (6);
        clearNotes.setBounds (bar.removeFromLeft (56));  bar.removeFromLeft (16);
        zoomOut.setBounds (bar.removeFromRight (26));    bar.removeFromRight (4);
        zoomIn.setBounds (bar.removeFromRight (26));     bar.removeFromRight (6);
        fit.setBounds (bar.removeFromRight (40));        bar.removeFromRight (6);
        followPlay.setBounds (bar.removeFromRight (86)); bar.removeFromRight (10);
        straightenLabel = bar.removeFromLeft (98);
        straighten.setBounds (bar.removeFromLeft (100)); bar.removeFromLeft (12);
        glideLabel = bar.removeFromLeft (80);
        glide.setBounds (bar.removeFromLeft (100));      bar.removeFromLeft (12);
        splitNote.setBounds (bar.removeFromLeft (52));   bar.removeFromLeft (6);
        joinNotes.setBounds (bar.removeFromLeft (52));
        body.removeFromTop (8);
        graphHint = body.removeFromBottom (18);
        body.removeFromBottom (4);
        graph.setBounds (body);
    }
}
}
