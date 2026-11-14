#include "EQPage.h"
#include "../PluginProcessor.h"

namespace mu
{
using namespace theme;

static const char* typeIcon (EQType t)
{
    switch (t)
    {
        case EQType::LowCut:    return "ftype_highpass";
        case EQType::LowShelf:  return "ftype_lowshelf";
        case EQType::Bell:      return "ftype_peak";
        case EQType::HighShelf: return "ftype_highshelf";
        case EQType::HighCut:   return "ftype_lowpass";
        default:                return "ftype_notch";
    }
}
static bool typeHasGain (EQType t) { return t == EQType::Bell || t == EQType::LowShelf || t == EQType::HighShelf; }
static bool typeIsCut (EQType t)   { return t == EQType::LowCut || t == EQType::HighCut; }

// =====================================================================================================
// The display: input and output spectra, the EQ curve, and the draggable bands.
class EQGraph : public juce::Component
{
public:
    static constexpr float kGainRange = 18.0f, kSpecTop = -6.0f, kSpecBottom = -84.0f;
    static constexpr int kNodeRadius = 10;

    EQGraph (MUProcessor& p, EQPage& o) : proc (p), owner (o)
    {
        setHelp (*this, "eq.graph");
        post.fill (kSpecBottom); pre.fill (kSpecBottom);
    }

    bool showPre = true;
    int selected = 2;

    void tick (float dt)
    {
        proc.pumpAnalysis();
        const auto& a = proc.getSpectrum().getDisplayDb();
        const auto& b = proc.getPreSpectrum().getDisplayDb();
        const bool playing = proc.isPlaying();
        // The output is measured after the track's fader (balance trim and output gain). The display is about tone,
        // so the fader is taken back out and the output curve sits where the input curve does.
        const float fader = proc.isMasterInstance() ? 0.0f : proc.faderDb();
        for (int i = 0; i < SpectrumAnalyzer::kDisplayPoints; ++i)
        {
            // glide towards each new analysis frame so the curves move continuously between FFTs
            post[(size_t) i] = approach (post[(size_t) i], playing ? a[(size_t) i] - fader : kSpecBottom - 6.0f, dt, playing ? 0.05f : 0.35f);
            pre[(size_t) i]  = approach (pre[(size_t) i],  playing ? b[(size_t) i] : kSpecBottom - 6.0f, dt, playing ? 0.05f : 0.35f);
        }
        for (int bnd = 0; bnd < kNumEQBands; ++bnd)
            dynOffset[(size_t) bnd] = approach (dynOffset[(size_t) bnd], proc.getEQ().getBandDynamicOffsetDb (bnd), dt, 0.04f);
        if (scan >= 0.0f) { scan += dt / (sweeping ? 1.6f : 0.75f); if (scan > 1.2f) scan = sweeping ? 0.0f : -1.0f; }
        highlightAlpha = approach (highlightAlpha, highlightOn ? 1.0f : 0.0f, dt, 0.08f);
        for (int bnd = 0; bnd < kNumEQBands; ++bnd)
            nodeScale[(size_t) bnd] = approach (nodeScale[(size_t) bnd], proc.getParam (ids::eqBandEnabled (bnd)) > 0.5f ? 1.0f : 0.0f, dt, 0.045f);
        ghostTimer -= dt;
        if (ghostTimer <= 0.0f) { ghostTimer = 0.5f; updateGhost(); }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        drawWell (g, full);
        const auto r = plot();
        g.saveState();
        g.reduceClipRegion (full.reduced (1.0f).toNearestInt());
        drawFreqGrid (g, r);

        // gain grid and labels
        g.setFont (body (10.0f));
        for (int db = -12; db <= 12; db += 6)
        {
            const float y = gainToY (r, (float) db);
            g.setColour (db == 0 ? juce::Colour (0xff343434) : juce::Colour (0xff1a1a1a));
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
            g.setColour (textMute);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (r.getRight() - 30.0f, y - 13.0f, 26.0f, 12.0f), juce::Justification::centredRight);
        }

        if (showPre) drawSpectrum (g, r, pre, text.withAlpha (0.20f), text.withAlpha (0.06f), 1.0f);
        drawSpectrum (g, r, post, yellow.withAlpha (0.85f), yellow.withAlpha (0.13f), 1.4f);
        drawGhost (g, r);
        if (highlightAlpha > 0.01f)
        {
            // the range a Mix Brain suggestion is talking about
            const float x0 = freqToX (highlightHz / 1.35f, r.getX(), r.getWidth()), x1 = freqToX (highlightHz * 1.35f, r.getX(), r.getWidth());
            g.setColour (yellow.withAlpha (0.13f * highlightAlpha)); g.fillRect (x0, r.getY(), x1 - x0, r.getHeight());
            g.setColour (yellow.withAlpha (0.8f * highlightAlpha)); g.fillRect (freqToX (highlightHz, r.getX(), r.getWidth()) - 0.5f, r.getY(), 1.0f, r.getHeight());
        }
        drawCurves (g, r);
        drawNodes (g, r);
        if (scan >= 0.0f && scan <= 1.0f)
        {
            // one sweep across the display while the track is analysed
            const float x = r.getX() + r.getWidth() * scan;
            g.setGradientFill (juce::ColourGradient (yellow.withAlpha (0.0f), x - 90.0f, 0.0f, yellow.withAlpha (0.22f), x, 0.0f, false));
            g.fillRect (x - 90.0f, r.getY(), 90.0f, r.getHeight());
            g.setColour (yellow); g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
        }
        g.restoreState();

        // readout
        g.setFont (body (11.5f));
        if (hoverFreq > 0.0f)
        {
            g.setColour (textDim);
            g.drawText (hzString (hoverFreq) + "   " + noteName (hoverFreq) + "   " + dbString (hoverGain), juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 8.0f, 240.0f, 14.0f), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (textMute);
            g.drawText ("Double-click to add a band. Drag to move, scroll to change width.", juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 8.0f, 420.0f, 14.0f), juce::Justification::centredLeft);
        }
        // legend
        auto legend = juce::Rectangle<float> (r.getRight() - 400.0f, r.getBottom() - 34.0f, 364.0f, 14.0f);
        auto key = [&] (const juce::String& name, juce::Colour c)
        {
            const float w = body (10.5f).getStringWidthFloat (name) + 22.0f;
            auto cell = legend.removeFromRight (w);
            g.setColour (c); g.fillRect (cell.getX(), cell.getCentreY() - 1.0f, 10.0f, 2.0f);
            g.setColour (textMute); g.setFont (body (10.5f));
            g.drawText (name, cell.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
        };
        key ("Output", yellow);
        if (showPre) key ("Input", text.withAlpha (0.4f));
        if (ghostValid) key (ghostName + " (dashed)", text.withAlpha (0.5f));
    }

    // ---- interaction -------------------------------------------------------------------------------
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto r = plot();
        hoverFreq = xToFreq ((float) e.x, r.getX(), r.getWidth());
        hoverGain = yToGain (r, (float) e.y);
        const int n = nodeAt (e.position);
        setMouseCursor (n >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
        hoverNode = n;
    }
    void mouseExit (const juce::MouseEvent&) override { hoverFreq = -1.0f; hoverNode = -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragBand = nodeAt (e.position);
        if (dragBand < 0) return;
        owner.selectBand (dragBand);
        if (e.mods.isPopupMenu()) { showTypeMenu (dragBand); dragBand = -1; return; }
        const auto s = proc.getBandSettings (dragBand);
        dragStartFreq = s.freqHz; dragStartGain = s.gainDb;
        gesture (dragBand, true);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0) return;
        const auto r = plot();
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float x0 = freqToX (dragStartFreq, r.getX(), r.getWidth());
        const float hz = xToFreq (x0 + (float) e.getDistanceFromDragStartX() * fine, r.getX(), r.getWidth());
        set (ids::eqBandFreq (dragBand), juce::jlimit (20.0f, 20000.0f, hz));
        const auto type = (EQType) (int) proc.getParam (ids::eqBandType (dragBand));
        if (typeHasGain (type))
        {
            const float perPixel = 2.0f * kGainRange / r.getHeight();
            set (ids::eqBandGain (dragBand), juce::jlimit (-18.0f, 18.0f, dragStartGain - (float) e.getDistanceFromDragStartY() * perPixel * fine));
        }
        hoverFreq = hz; hoverGain = proc.getParam (ids::eqBandGain (dragBand));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand >= 0) gesture (dragBand, false);
        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int n = nodeAt (e.position);
        if (n >= 0) { proc.setParam (ids::eqBandEnabled (n), 0.0f); return; }
        const auto r = plot();
        EQBandSettings s;
        s.enabled = true;
        s.freqHz = xToFreq ((float) e.x, r.getX(), r.getWidth());
        s.gainDb = juce::jlimit (-18.0f, 18.0f, yToGain (r, (float) e.y));
        s.q = 1.0f;
        if (s.freqHz < 45.0f)        { s.type = EQType::LowCut;  s.gainDb = 0.0f; s.q = 0.71f; s.slope = 1; }
        else if (s.freqHz > 14000.0f) { s.type = EQType::HighCut; s.gainDb = 0.0f; s.q = 0.71f; }
        else                           s.type = EQType::Bell;
        const int b = proc.pickBandFor (s);
        proc.setBandSettings (b, s);
        proc.setParam (ids::eqEnabled, 1.0f);
        owner.selectBand (b);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int n = nodeAt (e.position);
        const int b = n >= 0 ? n : selected;
        if (b < 0 || proc.getParam (ids::eqBandEnabled (b)) < 0.5f) return;
        const float q = proc.getParam (ids::eqBandQ (b));
        proc.setParam (ids::eqBandQ (b), juce::jlimit (0.1f, 18.0f, q * std::pow (2.0f, w.deltaY * 1.5f)));
    }

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (1.0f); }
    static float gainToY (juce::Rectangle<float> r, float db) { return r.getCentreY() - db / kGainRange * r.getHeight() * 0.5f; }
    static float yToGain (juce::Rectangle<float> r, float y) { return (r.getCentreY() - y) / (r.getHeight() * 0.5f) * kGainRange; }
    static float specToY (juce::Rectangle<float> r, float db) { return r.getBottom() - juce::jlimit (-0.05f, 1.0f, (db - kSpecBottom) / (kSpecTop - kSpecBottom)) * r.getHeight(); }

    void set (const juce::String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    void gesture (int band, bool begin)
    {
        for (auto id : { ids::eqBandFreq (band), ids::eqBandGain (band) })
            if (auto* p = proc.apvts.getParameter (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
    }

    juce::Point<float> nodePos (juce::Rectangle<float> r, const EQBandSettings& s) const
    {
        return { freqToX (s.freqHz, r.getX(), r.getWidth()), gainToY (r, typeHasGain (s.type) ? s.gainDb : 0.0f) };
    }

    int nodeAt (juce::Point<float> p) const
    {
        const auto r = plot();
        int best = -1; float bestDist = (float) (kNodeRadius + 5);
        for (int b = 0; b < kNumEQBands; ++b)
        {
            const auto s = proc.getBandSettings (b);
            if (! s.enabled) continue;
            const float d = nodePos (r, s).getDistanceFrom (p);
            if (d < bestDist) { bestDist = d; best = b; }
        }
        return best;
    }

    void showTypeMenu (int band)
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        const auto names = eqTypeNames();
        const int current = (int) proc.getParam (ids::eqBandType (band));
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == current);
        m.addSeparator();
        m.addItem (100, proc.getParam (ids::eqBandDyn (band)) > 0.5f ? "Make static" : "Make dynamic");
        m.addItem (101, "Remove band");
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe = juce::Component::SafePointer<EQGraph> (this), band] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 100) safe->proc.setParam (ids::eqBandDyn (band), safe->proc.getParam (ids::eqBandDyn (band)) > 0.5f ? 0.0f : 1.0f);
            else if (r == 101) safe->proc.setParam (ids::eqBandEnabled (band), 0.0f);
            else safe->proc.setParam (ids::eqBandType (band), (float) (r - 1));
        });
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> r, const std::array<float, SpectrumAnalyzer::kDisplayPoints>& data, juce::Colour stroke, juce::Colour fill, float thickness)
    {
        juce::Path p;
        const int n = SpectrumAnalyzer::kDisplayPoints;
        bool any = false;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = specToY (r, data[(size_t) i]);
            if (data[(size_t) i] > kSpecBottom) any = true;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (! any) return;
        juce::Path filled (p);
        filled.lineTo (r.getRight(), r.getBottom() + 4.0f); filled.lineTo (r.getX(), r.getBottom() + 4.0f); filled.closeSubPath();
        g.setGradientFill (juce::ColourGradient (fill, 0.0f, r.getY() + r.getHeight() * 0.3f, fill.withMultipliedAlpha (0.25f), 0.0f, r.getBottom(), false));
        g.fillPath (filled);
        g.setColour (stroke);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void updateGhost()
    {
        // Show the track this one has to share space with: the lead vocal for everything else, the beat for the vocal.
        ghostValid = false;
        const auto myRole = proc.getEffectiveRole();
        const bool iAmVocal = myRole == Role::Vocal;
        for (auto& t : proc.getBus().collectTracks())
        {
            if (t.slot == proc.getSessionSlot() || ! t.hasBands || ! t.active) continue;
            const bool wanted = iAmVocal ? (t.role == Role::Beat || t.role == Role::Melody) : t.role == Role::Vocal;
            if (! wanted) continue;
            ghost = t.bandsDb; ghostName = t.name; ghostValid = true;
            break;
        }
    }

    void drawGhost (juce::Graphics& g, juce::Rectangle<float> r)
    {
        if (! ghostValid) return;
        const auto centres = analysisBandCentres();
        juce::Path p;
        bool started = false;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float hz = centres[(size_t) b];
            // a third-octave band holds several analyser bins, so bring it onto the same scale as the live curves
            const float db = ghost[(size_t) b] + 4.5f * std::log2 (hz / 1000.0f) - 4.0f;
            const float x = freqToX (hz, r.getX(), r.getWidth()), y = specToY (r, db);
            if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
        }
        const float dashes[] = { 4.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, p.createPathWithRoundedCorners (30.0f), dashes, 2);
        g.setColour (text.withAlpha (0.38f));
        g.fillPath (dashed);
    }

    void drawCurves (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const double fs = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        const bool eqOn = proc.getParam (ids::eqEnabled) > 0.5f;
        const int steps = juce::jmax (64, (int) (r.getWidth() / 2.0f));
        struct Prepared { bool on = false, dyn = false; int n = 0, nLive = 0; std::array<BiquadCoeffs, EQEngine::kMaxStages> st {}, live {}; };
        std::array<Prepared, kNumEQBands> bands;
        bool anyOn = false, anyLive = false;
        for (int b = 0; b < kNumEQBands; ++b)
        {
            auto s = proc.getBandSettings (b);
            auto& pb = bands[(size_t) b];
            pb.on = s.enabled;
            if (! s.enabled) continue;
            anyOn = true;
            pb.n = EQEngine::computeStages (s, fs, pb.st);
            pb.dyn = s.dynamic && std::abs (dynOffset[(size_t) b]) > 0.05f;
            if (pb.dyn) { anyLive = true; s.gainDb += dynOffset[(size_t) b]; pb.nLive = EQEngine::computeStages (s, fs, pb.live); }
        }
        if (! anyOn) return;

        juce::Path curve, live, selectedCurve;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) steps;
            const float hz = xToFreq (x, r.getX(), r.getWidth());
            float sum = 0.0f, sumLive = 0.0f, sel = 0.0f;
            for (int b = 0; b < kNumEQBands; ++b)
            {
                const auto& pb = bands[(size_t) b];
                if (! pb.on) continue;
                float m = 0.0f;
                for (int k = 0; k < pb.n; ++k) m += EQEngine::magnitudeDb (pb.st[(size_t) k], hz, fs);
                sum += m;
                if (b == selected) sel = m;
                if (pb.dyn) { float ml = 0.0f; for (int k = 0; k < pb.nLive; ++k) ml += EQEngine::magnitudeDb (pb.live[(size_t) k], hz, fs); sumLive += ml; }
                else sumLive += m;
            }
            const float y = gainToY (r, juce::jlimit (-40.0f, 30.0f, sum)), yl = gainToY (r, juce::jlimit (-40.0f, 30.0f, sumLive)), ys = gainToY (r, juce::jlimit (-40.0f, 30.0f, sel));
            if (i == 0) { curve.startNewSubPath (x, y); live.startNewSubPath (x, yl); selectedCurve.startNewSubPath (x, ys); }
            else { curve.lineTo (x, y); live.lineTo (x, yl); selectedCurve.lineTo (x, ys); }
        }
        const float zero = gainToY (r, 0.0f);
        const float alpha = eqOn ? 1.0f : 0.35f;

        // selected band's own shape
        if (selected >= 0 && bands[(size_t) selected].on)
        {
            juce::Path f (selectedCurve);
            f.lineTo (r.getRight(), zero); f.lineTo (r.getX(), zero); f.closeSubPath();
            g.setColour (yellow.withAlpha (0.16f * alpha));
            g.fillPath (f);
        }
        if (anyLive)
        {
            g.setColour (yellow.withAlpha (0.9f * alpha));
            g.strokePath (live, juce::PathStrokeType (1.5f));
        }
        g.setColour (text.withAlpha (alpha));
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawNodes (juce::Graphics& g, juce::Rectangle<float> r)
    {
        for (int b = 0; b < kNumEQBands; ++b)
        {
            const auto s = proc.getBandSettings (b);
            const float scale = nodeScale[(size_t) b];
            if (scale < 0.04f) continue;                       // bands grow in when added and shrink away when removed
            const auto pos = nodePos (r, s);
            const bool sel = b == selected;
            const float rad = ((float) kNodeRadius + (b == hoverNode || b == dragBand ? 1.5f : 0.0f)) * scale;

            if (s.dynamic && typeHasGain (s.type))
            {
                // how far the band can move, and where it is right now
                const float yEnd = gainToY (r, s.gainDb + s.rangeDb), yNow = gainToY (r, s.gainDb + dynOffset[(size_t) b]);
                g.setColour (yellow.withAlpha (0.45f));
                g.fillRect (pos.x - 0.75f, juce::jmin (pos.y, yEnd), 1.5f, std::abs (yEnd - pos.y));
                g.fillRect (pos.x - 5.0f, yEnd - 0.75f, 10.0f, 1.5f);
                g.setColour (yellow);
                g.fillEllipse (pos.x - 3.5f, yNow - 3.5f, 7.0f, 7.0f);
            }
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (pos.x - rad - 1.0f, pos.y - rad, rad * 2.0f + 2.0f, rad * 2.0f + 2.0f);
            g.setColour (sel ? yellow : juce::Colour (0xff1c1c1c));
            g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (sel ? yellow : text);
            g.drawEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f, 1.5f);
            g.setColour (sel ? onYellow : text);
            g.setFont (bodyBold (11.0f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f), juce::Justification::centred);
            if (s.mode != ChannelMode::Stereo)
            {
                g.setFont (bodyBold (9.0f));
                g.setColour (textDim);
                g.drawText (s.mode == ChannelMode::Mid ? "M" : "S", juce::Rectangle<float> (pos.x + rad, pos.y - rad - 4.0f, 12.0f, 10.0f), juce::Justification::centredLeft);
            }
        }
    }

    MUProcessor& proc;
    EQPage& owner;
    std::array<float, SpectrumAnalyzer::kDisplayPoints> post {}, pre {};
    std::array<float, kNumEQBands> dynOffset {};
    BandArray ghost {};
    juce::String ghostName;
    bool ghostValid = false;
    float ghostTimer = 0.0f;
    int dragBand = -1, hoverNode = -1;
    float dragStartFreq = 1000.0f, dragStartGain = 0.0f, hoverFreq = -1.0f, hoverGain = 0.0f;
    std::array<float, kNumEQBands> nodeScale {};
    float scan = -1.0f, highlightHz = 1000.0f, highlightAlpha = 0.0f;
    bool sweeping = false;                  // while the track is being listened to the sweep keeps crossing the display
    bool highlightOn = false;

public:
    void startScan() { scan = 0.0f; }
    void setListening (bool on) { sweeping = on; if (on && scan < 0.0f) scan = 0.0f; }
    void setHighlight (float hz) { highlightOn = hz > 0.0f; if (highlightOn) highlightHz = hz; }
};

// =====================================================================================================
// Eight chips, one per band: shape, frequency and gain at a glance. Click to select, double-click to switch on or off.
class BandChips : public juce::Component
{
public:
    BandChips (MUProcessor& p, EQPage& o) : proc (p), owner (o) { setMouseCursor (juce::MouseCursor::PointingHandCursor); setHelp (*this, "eq.on"); }
    int selected = 2;

    void paint (juce::Graphics& g) override
    {
        for (int b = 0; b < kNumEQBands; ++b)
        {
            const auto cell = cellBounds (b);
            const auto s = proc.getBandSettings (b);
            const bool sel = b == selected;
            g.setColour (sel ? raised : panel);
            g.fillRoundedRectangle (cell, 6.0f);
            if (sel) { g.setColour (yellow); g.fillRoundedRectangle (cell.withHeight (3.0f), 1.5f); }
            auto inner = cell.reduced (9.0f, 6.0f);
            const auto colour = s.enabled ? text : textMute;
            auto top = inner.removeFromTop (18.0f);
            g.setFont (headline (15.0f));
            g.setColour (s.enabled ? (sel ? yellow : text) : textMute);
            g.drawText (juce::String (b + 1), top.removeFromLeft (14.0f), juce::Justification::centredLeft);
            icons::draw (g, typeIcon (s.type), top.removeFromLeft (20.0f).reduced (1.0f), colour);
            if (s.dynamic) icons::draw (g, "dynamic", top.removeFromRight (16.0f).reduced (1.0f), s.enabled ? yellow : textMute);
            g.setFont (body (11.0f));
            g.setColour (s.enabled ? textDim : textMute);
            juce::String line = hzString (s.freqHz);
            if (typeHasGain (s.type)) line << "  " << dbString (s.dynamic ? s.rangeDb : s.gainDb, 1, "");
            g.drawText (s.enabled ? line : juce::String ("off"), inner, juce::Justification::centredLeft);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { const int b = bandAt (e.x); if (b >= 0) owner.selectBand (b); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int b = bandAt (e.x);
        if (b >= 0) proc.setParam (ids::eqBandEnabled (b), proc.getParam (ids::eqBandEnabled (b)) > 0.5f ? 0.0f : 1.0f);
    }

private:
    juce::Rectangle<float> cellBounds (int b) const
    {
        const float gap = 6.0f, w = ((float) getWidth() - gap * (float) (kNumEQBands - 1)) / (float) kNumEQBands;
        return { (float) b * (w + gap), 0.0f, w, (float) getHeight() };
    }
    int bandAt (int x) const { for (int b = 0; b < kNumEQBands; ++b) if (cellBounds (b).contains ((float) x, 2.0f)) return b; return -1; }
    MUProcessor& proc;
    EQPage& owner;
};

// =====================================================================================================
// Everything about the selected band.
class BandControls : public juce::Component
{
public:
    BandControls (MUProcessor& p, int b)
        : proc (p), band (b),
          power (p, ids::eqBandEnabled (b), "", "power", "eq.on"),
          type (p, ids::eqBandType (b), { "icon:ftype_highpass", "icon:ftype_lowshelf", "icon:ftype_peak", "icon:ftype_highshelf", "icon:ftype_lowpass", "icon:ftype_notch" }, "eq.type"),
          slope (p, ids::eqBandSlope (b), { "12", "24", "36", "48" }, "eq.slope"),
          mode (p, ids::eqBandMode (b), { "icon:lrmode_stereo", "icon:lrmode_mid", "icon:lrmode_side" }, "eq.mode"),
          freq (p, ids::eqBandFreq (b), "Frequency", "eq.freq"), gain (p, ids::eqBandGain (b), "Gain", "eq.gain"), q (p, ids::eqBandQ (b), "Width (Q)", "eq.q"),
          thr (p, ids::eqBandThr (b), "Threshold", "eq.thr"), range (p, ids::eqBandRange (b), "Range", "eq.range")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &power, &type, &slope, &mode, &freq, &gain, &q, &thr, &range, &dynamic, &solo, &remove }) addAndMakeVisible (c);
        dynamic.setClickingTogglesState (true);
        dynAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::eqBandDyn (b), dynamic);
        solo.setClickingTogglesState (true);
        solo.setToggleState (p.getSoloBand() == b, juce::dontSendNotification);
        solo.onClick = [this] { proc.setSoloBand (solo.getToggleState() ? band : -1); };
        remove.onClick = [this]
        {
            EQBandSettings s; s.type = kDefaultBands[band].type; s.freqHz = kDefaultBands[band].freq;
            proc.setBandSettings (band, s);
        };
        refresh();
    }
    ~BandControls() override { if (proc.getSoloBand() == band) proc.setSoloBand (-1); }

    void refresh()
    {
        const auto s = proc.getBandSettings (band);
        const bool on = s.enabled;
        slope.setEnabled (on && typeIsCut (s.type));
        slope.setAlpha (typeIsCut (s.type) ? 1.0f : 0.35f);
        gain.setDimmed (! on || ! typeHasGain (s.type));
        freq.setDimmed (! on); q.setDimmed (! on);
        const bool dyn = on && s.dynamic && typeHasGain (s.type);
        thr.setDimmed (! dyn); range.setDimmed (! dyn);
        dynamic.setEnabled (on && typeHasGain (s.type));
    }

    void paint (juce::Graphics& g) override
    {
        fillPanel (g, getLocalBounds().toFloat());
        g.setFont (body (10.5f));
        g.setColour (textMute);
        for (auto& l : labels) g.drawText (l.first, l.second, juce::Justification::centredLeft);
        g.setColour (line);
        for (int x : dividers) g.fillRect (x, 14, 1, getHeight() - 28);
    }

    void resized() override
    {
        labels.clear(); dividers.clear();
        auto r = getLocalBounds().reduced (12, 10);
        auto divider = [&] { dividers.push_back (r.getX() + 8); r.removeFromLeft (16); };

        auto shape = r.removeFromLeft (212);
        auto shapeHead = shape.removeFromTop (14);
        labels.push_back ({ "Shape", shapeHead.withTrimmedLeft (30) });
        auto shapeRow = shape.removeFromTop (30);
        power.setBounds (shapeRow.removeFromLeft (22).withSizeKeepingCentre (22, 22));
        shapeRow.removeFromLeft (8);
        type.setBounds (shapeRow);
        shape.removeFromTop (8);
        labels.push_back ({ "Slope, dB per octave", shape.removeFromTop (14).withTrimmedLeft (30) });
        slope.setBounds (shape.removeFromTop (24).withTrimmedLeft (30).removeFromLeft (150));
        divider();

        layoutRow (r.removeFromLeft (198), { &freq, &gain, &q }, 4);
        divider();

        auto channel = r.removeFromLeft (100);
        labels.push_back ({ "Channel", channel.removeFromTop (14) });
        mode.setBounds (channel.removeFromTop (30));
        channel.removeFromTop (8);
        labels.push_back ({ "Dynamic", channel.removeFromTop (14) });
        dynamic.setBounds (channel.removeFromTop (26).removeFromLeft (34));
        divider();

        auto tools = r.removeFromRight (34);
        solo.setBounds (tools.removeFromTop (34));
        tools.removeFromTop (8);
        remove.setBounds (tools.removeFromTop (34));
        r.removeFromRight (10);
        layoutRow (r, { &thr, &range }, 4);
    }

private:
    MUProcessor& proc;
    int band;
    ParamToggle power;
    Segmented type, slope, mode;
    Knob freq, gain, q, thr, range;
    IconButton dynamic { "dynamic", "eq.dyn" }, solo { "solo", "eq.solo" }, remove { "trash", "eq.delete" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynAtt;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> labels;
    std::vector<int> dividers;
};

// =====================================================================================================
// Mix Brain results: one card per finding, with the reason and a button that makes the move.
class SuggestionList : public juce::Component
{
public:
    SuggestionList (MUProcessor& p, std::function<void (const juce::String&)> s) : proc (p), setStatus (std::move (s))
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
    }

    void setSuggestions (std::vector<Suggestion> s, float firstDelay = 0.0f)
    {
        cards.clear();
        for (size_t i = 0; i < s.size(); ++i)
        {
            auto c = std::make_unique<Card> (*this, s[i], (int) i);
            c->delay += firstDelay;
            content.addAndMakeVisible (*c);
            cards.push_back (std::move (c));
        }
        layout();
    }
    bool isEmpty() const { return cards.empty(); }

    // the frequency of the suggestion under the mouse, so the display can point at it
    float hoveredHz() const
    {
        for (auto& c : cards)
            if (c->isMouseOver (true))
                return c->suggestion.kind == Suggestion::Kind::EqBand ? c->suggestion.band.freqHz : c->suggestion.freqHz;
        return -1.0f;
    }

    void setAnswer (const juce::String& question, const juce::String& text, const juce::String& footer)
    {
        answer = std::make_unique<AnswerCard>();
        answer->question = question; answer->text = text; answer->footer = footer;
        content.addAndMakeVisible (*answer);
        layout();
        viewport.setViewPosition (0, 0);
    }
    int remaining() const { int n = 0; for (auto& c : cards) if (! c->applied) ++n; return n; }

    void applyAll()
    {
        for (auto& c : cards) if (! c->applied) c->apply (false);
        setStatus ("All suggestions applied. Listen with the EQ on and off, then analyse again once the song has played for a few seconds.");
    }

    void tick (float dt)
    {
        for (auto& c : cards)
            if (c->appear < 1.0f)
            {
                c->delay -= dt;
                if (c->delay <= 0.0f) { c->appear = juce::jmin (1.0f, c->appear + dt * 5.0f); c->setAlpha (c->appear); c->setTransform (juce::AffineTransform::translation (0.0f, (1.0f - c->appear) * 14.0f)); }
            }
    }

    void resized() override { viewport.setBounds (getLocalBounds()); layout(); }

private:
    struct Card : public juce::Component
    {
        Card (SuggestionList& o, const Suggestion& s, int index) : owner (o), suggestion (s), delay ((float) index * 0.07f)
        {
            styleButton (button, "Apply", true);
            button.onClick = [this] { apply (true); };
            addAndMakeVisible (button);
            setAlpha (0.0f);
        }
        void apply (bool report)
        {
            if (applied) return;
            const auto result = owner.proc.applySuggestion (suggestion);
            applied = true;
            button.setButtonText ("Applied");
            button.setEnabled (false);
            button.getProperties().set ("primary", false);
            repaint();
            if (report) owner.setStatus (result);
        }
        int heightFor (int w) const
        {
            const float tw = (float) w - 24.0f;
            const float actionH = juce::jmax (24.0f, wrappedHeight (suggestion.action, tw - 78.0f, bodyBold (11.5f)));
            return juce::roundToInt (12.0f + wrappedHeight (suggestion.title, tw - 26.0f, bodyBold (12.5f), 1.25f) + 6.0f + wrappedHeight (suggestion.why, tw, body (11.5f)) + 10.0f + actionH + 12.0f);
        }
        void paint (juce::Graphics& g) override
        {
            fillPanel (g, getLocalBounds().toFloat(), raised);
            auto r = getLocalBounds().reduced (12, 0).toFloat();
            r.removeFromTop (12.0f);
            const float titleH = wrappedHeight (suggestion.title, r.getWidth() - 26.0f, bodyBold (12.5f), 1.25f);
            auto titleRow = r.removeFromTop (titleH);
            icons::draw (g, suggestion.kind == Suggestion::Kind::Masking ? "collision" : suggestion.kind == Suggestion::Kind::EqBand ? typeIcon (suggestion.band.type) : "dynamic",
                         titleRow.removeFromLeft (18.0f).withHeight (17.0f), applied ? textMute : yellow);
            titleRow.removeFromLeft (8.0f);
            drawWrapped (g, suggestion.title, titleRow, bodyBold (12.5f), applied ? textDim : text, 1.25f);
            r.removeFromTop (6.0f);
            const float whyH = wrappedHeight (suggestion.why, r.getWidth(), body (11.5f));
            drawWrapped (g, suggestion.why, r.removeFromTop (whyH), body (11.5f), textDim);
            r.removeFromTop (10.0f);
            drawWrapped (g, suggestion.action, r.withTrimmedRight (78.0f), bodyBold (11.5f), applied ? textMute : yellow);
        }
        void resized() override { button.setBounds (getWidth() - 12 - 70, getHeight() - 12 - 24, 70, 24); }

        SuggestionList& owner;
        Suggestion suggestion;
        juce::TextButton button;
        bool applied = false;
        float appear = 0.0f, delay = 0.0f;
    };

    struct AnswerCard : public juce::Component
    {
        int heightFor (int w) const
        {
            const float tw = (float) w - 24.0f;
            return juce::roundToInt (12.0f + wrappedHeight (question, tw, bodyBold (12.0f), 1.25f) + 8.0f + wrappedHeight (text, tw, body (12.0f), 1.4f) + 10.0f + wrappedHeight (footer, tw, body (10.5f), 1.25f) + 12.0f);
        }
        void paint (juce::Graphics& g) override
        {
            fillPanel (g, getLocalBounds().toFloat(), raised);
            g.setColour (yellow); g.fillRect (0, 10, 3, getHeight() - 20);
            auto r = getLocalBounds().reduced (12, 0).toFloat();
            r.removeFromTop (12.0f);
            const float qh = wrappedHeight (question, r.getWidth(), bodyBold (12.0f), 1.25f);
            drawWrapped (g, question, r.removeFromTop (qh), bodyBold (12.0f), text_(), 1.25f);
            r.removeFromTop (8.0f);
            const float th = wrappedHeight (text, r.getWidth(), body (12.0f), 1.4f);
            drawWrapped (g, text, r.removeFromTop (th), body (12.0f), textDim, 1.4f);
            r.removeFromTop (10.0f);
            drawWrapped (g, footer, r, body (10.5f), textMute, 1.25f);
        }
        static juce::Colour text_() { return theme::text; }
        juce::String question, text, footer;
    };

    void layout()
    {
        const int w = juce::jmax (100, viewport.getWidth() - 10);
        int y = 0;
        if (answer != nullptr)
        {
            const int h = answer->heightFor (w);
            answer->setBounds (0, y, w, h);
            y += h + 8;
        }
        for (auto& c : cards)
        {
            const int h = c->heightFor (w);
            c->setBounds (0, y, w, h);
            y += h + 8;
        }
        content.setSize (w, juce::jmax (1, y));
    }

    MUProcessor& proc;
    std::function<void (const juce::String&)> setStatus;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<Card>> cards;
    std::unique_ptr<AnswerCard> answer;
};

// =====================================================================================================
EQPage::EQPage (MUProcessor& p, std::function<void (const juce::String&)> status)
    : proc (p), setStatus (std::move (status)),
      eqPower (p, ids::eqEnabled, "", "power"), autoGain (p, ids::eqAutoGain, "Auto gain")
{
    graph = std::make_unique<EQGraph> (proc, *this);
    chips = std::make_unique<BandChips> (proc, *this);
    suggestions = std::make_unique<SuggestionList> (proc, setStatus);
    addAndMakeVisible (*graph); addAndMakeVisible (*chips); addAndMakeVisible (*suggestions);
    for (auto* c : std::initializer_list<juce::Component*> { &eqPower, &autoGain, &deltaButton, &resetButton, &showInput, &analyseButton, &applyAllButton, &captureButton, &matchButton, &matchAmount })
        addAndMakeVisible (c);

    // optional AI tutor: only shown when a local model is available
    askBox.setFont (body (12.5f));
    askBox.setTextToShowWhenEmpty ("Ask the AI tutor about this track", textMute);
    askBox.setIndents (8, 6);
    askBox.onReturnKey = [this] { askTutor(); juce::Component::unfocusAllComponents(); };
    askBox.onEscapeKey = [this] { askBox.clear(); juce::Component::unfocusAllComponents(); };
    setHelp (askBox, "eq.ask");
    styleButton (askButton, "Ask", false, "eq.ask");
    askButton.onClick = [this] { askTutor(); };
    addChildComponent (askBox); addChildComponent (askButton);
    lastAnswerId = proc.getAI().getStatus().answerId;

    deltaButton.setClickingTogglesState (true);
    deltaButton.onClick = [this] { proc.setListenMode (deltaButton.getToggleState() ? Listen::EqDelta : Listen::Normal); };
    resetButton.onClick = [this] { proc.resetEQ(); setStatus ("EQ reset to flat."); };
    showInput.setButtonText ("Show input");
    showInput.setWantsKeyboardFocus (false);
    showInput.setToggleState (true, juce::dontSendNotification);
    showInput.onClick = [this] { graph->showPre = showInput.getToggleState(); };
    setHelp (showInput, "eq.pre");

    styleButton (analyseButton, "Analyse", true, "eq.analyse");
    analyseButton.onClick = [this]
    {
        if (listening)
        {
            listening = false; proc.cancelListening(); graph->setListening (false);
            analyseButton.setButtonText ("Analyse");
            brainStatus = "Stopped. Press Analyse to listen again.";
        }
        else
        {
            // Listen first, decide afterwards: eight seconds of actual sound, so the suggestions describe the track
            // and not the one moment the button happened to be pressed.
            listening = true; listenStartMs = juce::Time::getMillisecondCounterHiRes();
            proc.startListening(); graph->setListening (true);
            brainStatus = "Listening. Keep the busiest part of the song playing: this takes " + juce::String ((int) MUProcessor::kListenSeconds) + " seconds of sound, and silence does not count. Press again to stop.";
        }
        resized(); repaint();
    };
    styleButton (applyAllButton, "Apply all", false, "eq.analyse");
    applyAllButton.onClick = [this] { suggestions->applyAll(); };
    styleButton (captureButton, "Capture", false, "eq.capture");
    captureButton.onClick = [this] { setStatus (proc.captureReference()); refreshReference(); };
    styleButton (matchButton, "Match", false, "eq.match");
    matchButton.onClick = [this] { setStatus (proc.matchReference ((float) matchAmount.getValue() * 0.01f)); };
    matchAmount.setSliderStyle (juce::Slider::LinearHorizontal);
    matchAmount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    matchAmount.setRange (0.0, 100.0, 1.0);
    matchAmount.setValue (60.0, juce::dontSendNotification);
    setHelp (matchAmount, "eq.match");

    brainStatus = "Play the song, then press Analyse. You will get a short list of what is worth changing on this track, with the reason for each.";
    selectBand (2);
    refreshReference();
}

EQPage::~EQPage() { proc.setListenMode (Listen::Normal); proc.setSoloBand (-1); }

void EQPage::visibilityChanged()
{
    if (! isVisible())
    {
        if (proc.getListenMode() == Listen::EqDelta) proc.setListenMode (Listen::Normal);
        deltaButton.setToggleState (false, juce::dontSendNotification);
    }
}

void EQPage::selectBand (int b)
{
    selected = juce::jlimit (0, kNumEQBands - 1, b);
    graph->selected = selected; chips->selected = selected;
    controls = std::make_unique<BandControls> (proc, selected);
    addAndMakeVisible (*controls);
    controls->sendLookAndFeelChange();     // sliders build their value boxes from the look-and-feel they can see at that moment
    resized();
    repaint();
}

void EQPage::runAnalysis()
{
    listening = false; graph->setListening (false);
    analyseButton.setButtonText ("Analyse");
    juce::String status;
    auto found = proc.analyse (status);
    brainStatus = status;
    const bool any = ! found.empty();
    suggestions->setSuggestions (std::move (found), any ? 0.6f : 0.0f);
    if (proc.secondsHeard() >= 3.0f) graph->startScan();
    applyAllButton.setVisible (suggestions->remaining() > 1);
    resized();
    repaint();
}

void EQPage::askTutor()
{
    const auto question = askBox.getText().trim();
    if (question.isEmpty() || proc.getAI().getStatus().busy) return;
    // a follow-up ("why?") is looked up together with the question before it, so the right notes are still found
    const bool followUp = askedQuestion.isNotEmpty() && juce::Time::currentTimeMillis() - askedAt < 10 * 60 * 1000;
    const auto lookUp = followUp ? askedQuestion + " " + question : question;
    askedQuestion = question;
    askedAt = juce::Time::currentTimeMillis();
    proc.getAI().ask (proc.tutorPrompt (lookUp), question, (int) (juce::pointer_sized_int) &proc);
    askButton.setButtonText ("...");
    setStatus ("The tutor is thinking. The first answer after a break takes a little longer while it wakes up.");
}

void EQPage::refreshReference()
{
    juce::String name;
    referenceText = proc.hasReference (name) ? "Reference: " + name : juce::String ("No reference captured yet");
    matchButton.setEnabled (proc.hasReference (name));
    repaint (referenceArea);
}

void EQPage::tick (float dt)
{
    if (listening)
    {
        const float heard = proc.listenedSeconds();
        const double waited = (juce::Time::getMillisecondCounterHiRes() - listenStartMs) * 0.001;
        analyseButton.setButtonText (juce::String (juce::jmax (1, (int) std::ceil (MUProcessor::kListenSeconds - heard))) + " s");
        if (heard >= MUProcessor::kListenSeconds || (waited > 25.0 && heard >= 3.0f)) runAnalysis();
        else if (waited > 25.0)
        {
            listening = false; proc.cancelListening(); graph->setListening (false);
            analyseButton.setButtonText ("Analyse");
            brainStatus = "Hardly anything played in 25 seconds, so there was nothing to analyse. Start playback on the busiest part of the song and press Analyse again.";
            resized(); repaint();
        }
    }
    graph->setHighlight (suggestions->hoveredHz());
    graph->tick (dt);
    suggestions->tick (dt);
    chips->repaint();
    refreshTimer -= dt;
    if (refreshTimer <= 0.0f)
    {
        refreshTimer = 0.25f;
        if (controls != nullptr) { controls->refresh(); controls->repaint(); }
        refreshReference();
        applyAllButton.setVisible (suggestions->remaining() > 1);

        const auto ai = proc.getAI().getStatus();
        // questions for the tutor are asked on the Mix Brain tab now; an answer still on its way is shown here
        if (! ai.busy) askButton.setButtonText ("Ask");
        if (ai.answerId != lastAnswerId)
        {
            lastAnswerId = ai.answerId;
            if (ai.answer.isNotEmpty())
            {
                suggestions->setAnswer (askedQuestion, ai.answer, "Answer from an AI model (" + ai.model + ") running on this computer. It can be wrong: check it with your ears.");
                askBox.clear();
            }
            else setStatus (ai.error);
        }
    }
}

void EQPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    fillPanel (g, brainArea.toFloat());
    auto head = brainArea.reduced (14, 0).removeFromTop (46);
    g.setFont (headline (20.0f));
    g.setColour (text);
    g.drawText ("ANALYSIS", head, juce::Justification::centredLeft);
    drawWrapped (g, brainStatus, statusArea.toFloat(), body (12.0f), textDim);

    g.setColour (line);
    g.fillRect (brainArea.getX() + 14, referenceArea.getY() - 10, brainArea.getWidth() - 28, 1);
    g.setFont (bodyBold (12.5f));
    g.setColour (text);
    g.drawText ("Reference match", referenceArea.withHeight (18), juce::Justification::centredLeft);
    g.setFont (body (11.5f));
    g.setColour (textDim);
    g.drawText (referenceText, referenceArea.withTrimmedTop (20).withHeight (16), juce::Justification::centredLeft);
    g.setColour (textMute);
    g.drawText ("Amount", referenceArea.withTrimmedTop (76).withHeight (18).removeFromLeft (52), juce::Justification::centredLeft);
}

void EQPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    brainArea = r.removeFromRight (290);
    r.removeFromRight (10);

    // left: toolbar, graph, chips, selected band
    toolbarArea = r.removeFromTop (30);
    auto tb = toolbarArea;
    eqPower.setBounds (tb.removeFromLeft (22).withSizeKeepingCentre (22, 22));
    tb.removeFromLeft (10);
    autoGain.setBounds (tb.removeFromLeft (110));
    showInput.setBounds (tb.removeFromLeft (120));
    resetButton.setBounds (tb.removeFromRight (30));
    tb.removeFromRight (6);
    deltaButton.setBounds (tb.removeFromRight (30));
    r.removeFromTop (8);

    if (controls != nullptr) controls->setBounds (r.removeFromBottom (112));
    r.removeFromBottom (8);
    chips->setBounds (r.removeFromBottom (50));
    r.removeFromBottom (8);
    graph->setBounds (r);

    // right: Mix Brain
    auto b = brainArea.reduced (14, 0);
    auto head = b.removeFromTop (46);
    analyseButton.setBounds (head.removeFromRight (92).withSizeKeepingCentre (92, 28));
    head.removeFromRight (6);
    applyAllButton.setBounds (head.removeFromRight (78).withSizeKeepingCentre (78, 28));
    const int statusH = juce::roundToInt (wrappedHeight (brainStatus, (float) b.getWidth(), body (12.0f))) + 4;
    statusArea = b.removeFromTop (statusH);
    b.removeFromTop (10);
    if (tutorShown)
    {
        auto ask = b.removeFromTop (28);
        askButton.setBounds (ask.removeFromRight (52));
        ask.removeFromRight (6);
        askBox.setBounds (ask);
        b.removeFromTop (10);
    }
    b.removeFromBottom (12);
    referenceArea = b.removeFromBottom (98);
    auto row = referenceArea.withTrimmedTop (42).withHeight (28);
    captureButton.setBounds (row.removeFromLeft ((row.getWidth() - 8) / 2));
    row.removeFromLeft (8);
    matchButton.setBounds (row);
    matchAmount.setBounds (referenceArea.withTrimmedTop (76).withHeight (18).withTrimmedLeft (56));
    b.removeFromBottom (20);
    suggestions->setBounds (b);
}
}
