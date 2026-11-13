#include "LearnPage.h"
#include "../PluginProcessor.h"
#include "HeaderBar.h"

namespace mu
{
using namespace theme;

namespace
{
    enum class Diagram { None, FrequencyMap, EqMove, Compressor, AttackRelease, Loudness };
    struct Lesson
    {
        const char* title; const char* body; Diagram diagram; const char* tryLabel; int tab; const char* preset;
    };

    const Lesson kLessons[] = {
        { "The order of a mix",
          "A mix goes wrong when you polish one sound before the whole song is balanced. Work from big decisions to small ones.\n\n"
          "First, levels. Get every track sitting at the right volume against the lead vocal. Most of what people call a good mix is simply a good balance.\n\n"
          "Second, space. Find where two tracks are fighting for the same frequencies and decide which one wins.\n\n"
          "Third, tone and control. EQ and compression on the individual tracks, to fix what the balance could not.\n\n"
          "Last, the master. Loudness and a limiter, once the mix already sounds right. Mastering makes a good mix louder; it does not rescue a bad one.",
          Diagram::None, "Go to Mix", HeaderBar::Mix, nullptr },

        { "The frequency map",
          "Every sound is made of low, middle and high frequencies, measured in hertz (Hz). An EQ is a volume control for one part of that range.\n\n"
          "Engineers use the same handful of words for the same areas. Learn where they are and you can name a problem before you reach for a control.\n\n"
          "Move the mouse across the map to read what lives in each area, what too much of it sounds like, and what too little sounds like.",
          Diagram::FrequencyMap, "Open the EQ", HeaderBar::EQ, nullptr },

        { "EQ: cut first, then boost",
          "This vocal has a build-up in the low mids. It sounds thick and cloudy, and it hides the words.\n\n"
          "The instinct is to boost the highs to get clarity back. That makes everything louder and leaves the cloud in place. Removing the problem is cleaner.\n\n"
          "Drag the band onto the hump and pull it down until the yellow line is smooth. Scroll to make the band wider or narrower.\n\n"
          "A cut can be narrow and deep. A boost should be wide and small. If you need more than 6 dB either way, the fix is usually at the source.",
          Diagram::EqMove, "Analyse my track", HeaderBar::EQ, nullptr },

        { "What a compressor does",
          "A performance is never even. Some words are loud, some are almost lost. Turn the vocal up for the quiet ones and the loud ones jump out.\n\n"
          "A compressor turns down only what goes over a level you choose, the threshold. The ratio sets how firmly: at 4:1, a word that goes 8 dB over comes out 2 dB over.\n\n"
          "Then makeup gain brings everything back up. The loud words end where they started and the quiet ones are closer behind them. That is how a vocal stays in front of a beat.\n\n"
          "Drag up and down for the threshold, left and right for the ratio. Grey is what goes in, yellow is what comes out.",
          Diagram::Compressor, "Open Dynamics", HeaderBar::Dynamics, nullptr },

        { "Attack and release",
          "Attack is how long the compressor waits before it clamps down. Release is how long it takes to let go.\n\n"
          "With a fast attack the very front of the sound is caught. Good for control, but a drum loses its crack. With a slower attack, 20 to 30 ms, the hit passes through before the level drops, so it sounds punchier than before.\n\n"
          "A fast release brings the level straight back and sounds loud and exciting. Too fast and you hear it pumping. A slow release is smoother and suits singing and bass.\n\n"
          "Drag left and right for attack, up and down for release, and watch what happens to the front of each hit.",
          Diagram::AttackRelease, "Open Dynamics", HeaderBar::Dynamics, nullptr },

        { "Gate, de-esser and smoothing",
          "These three all do one job: turn something down only at the moment it is a problem.\n\n"
          "A gate turns the track down when it is quiet, so headphone bleed and room noise disappear between phrases. Set the threshold just above the noise.\n\n"
          "A de-esser listens for S and T sounds, usually between 5 and 8 kHz, and dips them when they get sharp. It goes after the compressor, because compression makes them louder. Three to six decibels is enough.\n\n"
          "Smooth watches the whole spectrum for any frequency that pokes out above its neighbours, and turns just that down for as long as it does. Use it on harsh vocals, ringing samples and piercing hi-hats.",
          Diagram::None, "Open Tone", HeaderBar::Tone, nullptr },

        { "Saturation and width",
          "Saturation is gentle distortion. It adds harmonics above the note that is being played, which makes a sound feel thicker and closer.\n\n"
          "It matters most on bass. An 808 lives below 100 Hz, where phone speakers produce nothing. Add saturation and its harmonics appear at 200, 300, 400 Hz, so the bass line is still heard on a phone.\n\n"
          "Width changes how far a sound spreads between the speakers. Wide pads and backing vocals leave the centre free for the lead. Keep the lead vocal, kick and bass in the middle, and keep everything under about 120 Hz mono so the low end stays solid on club systems.",
          Diagram::None, "Open Tone", HeaderBar::Tone, nullptr },

        { "Loudness, LUFS and true peak",
          "Peak level tells you if the signal will clip. It says very little about how loud the song feels. Loudness is measured in LUFS, which follows the ear.\n\n"
          "Streaming services measure every song and turn it to about -14 LUFS. A master pushed to -8 gets turned down, and all it keeps is the damage the limiter did on the way.\n\n"
          "True peak is the highest point of the real waveform, including the parts between the samples. Leave 1 dB spare so the song does not distort when it is converted for streaming.\n\n"
          "Before mastering, a mix should sit around -18 LUFS with peaks under -6 dB. That headroom is what the master uses.",
          Diagram::Loudness, "Open Master", HeaderBar::Master, nullptr },

        { "A vocal chain, step by step",
          "The order matters, because each stage hears the result of the one before it.\n\n"
          "1. Clean up. Gate out the noise between phrases and cut the rumble below 80 to 100 Hz.\n"
          "2. Remove problems. Cut the mud around 200 to 400 Hz before anything else reacts to it.\n"
          "3. Control. Compress so the loud words drop 3 to 6 dB. Fast for rap, slower for singing.\n"
          "4. De-ess. The compressor has made the S sounds louder, so tame them now.\n"
          "5. Add character. A little presence around 3 kHz, air above 10 kHz, a touch of saturation.\n"
          "6. Ride the level so every line sits in the same place.\n\n"
          "The vocal presets are built in this order. Load one, then switch each stage off and on to hear what it adds.",
          Diagram::None, "Load a vocal chain", HeaderBar::Dynamics, "Rap vocal - punchy" },

        { "A vocal over a two-track beat",
          "When the beat is one stereo file you cannot turn the snare down or move the melody. You can still make room.\n\n"
          "Put the plugin on the beat and on the vocal. Set the roles, play the hook, and press Learn and balance: the beat will settle a little under the vocal.\n\n"
          "Load a beat preset. Vocal space dips the beat where the voice lives, only while the voice is there, so the beat keeps its energy in the gaps.\n\n"
          "Check the collisions on the Mix tab. If the beat is covering the vocal around 2 to 4 kHz, fix it with a dynamic cut on the beat. Boosting the vocal there would only make it harsh.",
          Diagram::None, "Go to Mix", HeaderBar::Mix, nullptr },
    };
    constexpr int kNumLessons = (int) (sizeof (kLessons) / sizeof (kLessons[0]));

    struct Region { float lo, hi; const char* name; const char* lives; const char* tooMuch; const char* tooLittle; };
    const Region kRegions[] = {
        { 20, 60,       "Sub",             "The lowest note of an 808 and the weight under a kick. Felt in the chest more than heard.", "Eats headroom and makes the limiter pump.", "The track feels small on big speakers." },
        { 60, 150,      "Bass",            "The body of bass lines and the punch of the kick.", "Boomy, and it swallows the vocal's low end.", "Thin, no drive." },
        { 150, 350,     "Warmth and mud",  "The body of voices, keys and guitars. Also where reverb and room sound pile up.", "Muddy and cloudy. The most common mix problem.", "Cold and hollow." },
        { 350, 700,     "Boxiness",        "The woody middle of drums and the chest of a voice recorded in a small room.", "Sounds like it was recorded in a cardboard box.", "Scooped, lacks body." },
        { 700, 1500,    "Honk",            "The nose of a voice and the character of mid-range instruments.", "Nasal, like a telephone.", "Distant and hard to place." },
        { 1500, 4000,   "Presence",        "Where the ear is most sensitive and where words are understood.", "Harsh and tiring within a minute.", "The vocal sinks behind the beat." },
        { 4000, 8000,   "Bite and S sounds", "Consonants, the snap of a snare, the edge of hi-hats.", "Spitty S sounds and piercing hats.", "Dull, no definition." },
        { 8000, 12000,  "Brightness",      "Polish and shine. The sizzle on cymbals.", "Brittle and fizzy.", "Sounds like it is under a blanket." },
        { 12000, 20000, "Air",             "The breath around a voice. Openness.", "Hiss and noise come up with it.", "Closed in, though many speakers barely play this." } };
    constexpr int kNumRegions = (int) (sizeof (kRegions) / sizeof (kRegions[0]));
}

// =====================================================================================================
class LessonDiagram : public juce::Component
{
public:
    explicit LessonDiagram (Diagram k) : kind (k) { setMouseCursor (k == Diagram::None || k == Diagram::Loudness ? juce::MouseCursor::NormalCursor : juce::MouseCursor::CrosshairCursor); }

    void tick (float dt)
    {
        phase = std::fmod (phase + dt * 0.5f, 1.0f);
        if (kind == Diagram::AttackRelease || kind == Diagram::Loudness) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        drawWell (g, getLocalBounds().toFloat());
        switch (kind)
        {
            case Diagram::FrequencyMap:  paintMap (g); break;
            case Diagram::EqMove:        paintEq (g); break;
            case Diagram::Compressor:    paintComp (g); break;
            case Diagram::AttackRelease: paintAttack (g); break;
            case Diagram::Loudness:      paintLoudness (g); break;
            default: break;
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (kind != Diagram::FrequencyMap) return;
        const auto r = getLocalBounds().toFloat().reduced (16.0f);
        const float hz = xToFreq ((float) e.x, r.getX(), r.getWidth());
        for (int i = 0; i < kNumRegions; ++i) if (hz >= kRegions[i].lo && hz < kRegions[i].hi && i != region) { region = i; repaint(); }
    }
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced (16.0f);
        const float nx = juce::jlimit (0.0f, 1.0f, ((float) e.x - r.getX()) / r.getWidth()), ny = juce::jlimit (0.0f, 1.0f, ((float) e.y - r.getY()) / r.getHeight());
        if (kind == Diagram::EqMove)
        {
            const auto plot = eqPlot();
            eqFreq = xToFreq ((float) e.x, plot.getX(), plot.getWidth(), 40.0f, 16000.0f);
            eqGain = juce::jlimit (-15.0f, 15.0f, (plot.getCentreY() - (float) e.y) / (plot.getHeight() * 0.5f) * 15.0f);
        }
        else if (kind == Diagram::Compressor) { compThr = -4.0f - ny * 30.0f; compRatio = 1.0f + nx * nx * 11.0f; }
        else if (kind == Diagram::AttackRelease) { attackMs = 0.3f * std::pow (200.0f, nx); releaseMs = 400.0f * std::pow (0.05f, ny); }
        else if (kind == Diagram::FrequencyMap) mouseMove (e);
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (kind == Diagram::EqMove) { eqQ = juce::jlimit (0.4f, 6.0f, eqQ * std::pow (2.0f, w.deltaY * 1.5f)); repaint(); }
    }

private:
    // ---- frequency map ---------------------------------------------------------------------------------
    void paintMap (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (16.0f);
        auto strip = r.removeFromTop (r.getHeight() * 0.42f);
        for (int i = 0; i < kNumRegions; ++i)
        {
            const float x0 = freqToX (kRegions[i].lo, strip.getX(), strip.getWidth()), x1 = freqToX (kRegions[i].hi, strip.getX(), strip.getWidth());
            const auto cell = juce::Rectangle<float> (x0 + 1.0f, strip.getY(), x1 - x0 - 2.0f, strip.getHeight() - 20.0f);
            const bool on = i == region;
            // taller towards the middle, where hearing is most sensitive
            const float sens = 0.45f + 0.55f * std::exp (-std::pow (std::log2 (std::sqrt (kRegions[i].lo * kRegions[i].hi) / 2800.0f) / 2.6f, 2.0f));
            const auto bar = cell.withTop (cell.getBottom() - cell.getHeight() * sens);
            g.setColour (on ? yellow : control);
            g.fillRoundedRectangle (bar, 3.0f);
            g.setFont (body (10.0f));
            g.setColour (on ? text : textMute);
            g.drawText (kRegions[i].lo >= 1000.0f ? juce::String (kRegions[i].lo / 1000.0f, kRegions[i].lo == 1500.0f ? 1 : 0) + "k" : juce::String ((int) kRegions[i].lo),
                        juce::Rectangle<float> (x0 - 2.0f, strip.getBottom() - 16.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
        }
        r.removeFromTop (10.0f);
        const auto& reg = kRegions[region];
        g.setFont (headline (26.0f)); g.setColour (yellow);
        g.drawText (juce::String (reg.name).toUpperCase(), r.removeFromTop (32.0f), juce::Justification::centredLeft);
        g.setFont (body (12.0f)); g.setColour (textMute);
        auto plain = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1).replace (".0", "") + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        g.drawText (plain (reg.lo) + " to " + plain (reg.hi), r.removeFromTop (16.0f), juce::Justification::centredLeft);
        r.removeFromTop (8.0f);
        auto para = [&] (const juce::String& label, const juce::String& s)
        {
            g.setFont (bodyBold (12.5f)); g.setColour (text);
            g.drawText (label, r.removeFromTop (18.0f), juce::Justification::centredLeft);
            const float h = wrappedHeight (s, r.getWidth(), body (12.5f));
            drawWrapped (g, s, r.removeFromTop (h), body (12.5f), textDim);
            r.removeFromTop (8.0f);
        };
        para ("What lives here", reg.lives);
        para ("Too much", reg.tooMuch);
        para ("Too little", reg.tooLittle);
    }

    // ---- EQ move ---------------------------------------------------------------------------------------
    juce::Rectangle<float> eqPlot() const { return getLocalBounds().toFloat().reduced (16.0f).withTrimmedBottom (54.0f); }
    static float vocalShape (float hz)
    {
        const float oct = std::log2 (hz / 600.0f);
        float db = -oct * oct * 0.9f;                                                   // a voice: strongest in the middle
        db += 9.0f * std::exp (-std::pow (std::log2 (hz / 280.0f) / 0.45f, 2.0f));      // the problem
        return db;
    }
    void paintEq (juce::Graphics& g)
    {
        const auto plot = eqPlot();
        const float lo = 40.0f, hi = 16000.0f;
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillRect (plot.getX(), plot.getCentreY(), plot.getWidth(), 1.0f);
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.fillRect (freqToX (f, plot.getX(), plot.getWidth(), lo, hi), plot.getY(), 1.0f, plot.getHeight());
        EQBandSettings band { true, EQType::Bell, eqFreq, eqGain, eqQ };
        juce::Path before, after, curve;
        auto specY = [&] (float db) { return plot.getY() + plot.getHeight() * (0.34f - db / 40.0f); };
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        for (int i = 0; i <= 200; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 200.0f;
            const float hz = xToFreq (x, plot.getX(), plot.getWidth(), lo, hi);
            const float resp = EQEngine::bandResponseDb (band, hz, 48000.0);
            const float yb = specY (vocalShape (hz)), ya = specY (vocalShape (hz) + resp), yc = plot.getCentreY() - resp / 15.0f * plot.getHeight() * 0.5f;
            if (i == 0) { before.startNewSubPath (x, yb); after.startNewSubPath (x, ya); curve.startNewSubPath (x, yc); }
            else { before.lineTo (x, yb); after.lineTo (x, ya); curve.lineTo (x, yc); }
        }
        g.setColour (text.withAlpha (0.25f)); g.strokePath (before, juce::PathStrokeType (1.5f));
        g.setColour (yellow); g.strokePath (after, juce::PathStrokeType (2.0f));
        g.setColour (text); g.strokePath (curve, juce::PathStrokeType (1.5f));
        const juce::Point<float> node (freqToX (eqFreq, plot.getX(), plot.getWidth(), lo, hi), plot.getCentreY() - eqGain / 15.0f * plot.getHeight() * 0.5f);
        g.setColour (yellow); g.fillEllipse (node.x - 8.0f, node.y - 8.0f, 16.0f, 16.0f);
        g.restoreState();

        const bool onIt = std::abs (std::log2 (eqFreq / 280.0f)) < 0.35f;
        const bool solved = onIt && eqGain < -6.0f && eqGain > -11.0f;
        juce::String msg = solved ? "That is it. The hump is gone, the yellow line is smooth, and nothing else was touched."
                         : onIt && eqGain <= -11.0f ? "Too far. Now there is a hole where the body of the voice should be."
                         : onIt && eqGain < 0.0f ? "Right place. Keep pulling down until the yellow line is smooth."
                         : eqGain > 2.0f ? "A boost makes it louder, and the hump is still there. Try cutting instead."
                         : "Drag the band onto the hump around 280 Hz, then pull it down.";
        auto foot = getLocalBounds().toFloat().reduced (16.0f).removeFromBottom (46.0f);
        g.setFont (bodyBold (12.5f)); g.setColour (solved ? yellow : text);
        g.drawText (hzString (eqFreq) + "   " + dbString (eqGain) + "   Q " + juce::String (eqQ, 1), foot.removeFromTop (18.0f), juce::Justification::centredLeft);
        drawWrapped (g, msg, foot, body (12.0f), textDim);
    }

    // ---- compressor ------------------------------------------------------------------------------------
    void paintComp (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (16.0f);
        auto foot = r.removeFromBottom (54.0f);
        r.removeFromBottom (10.0f);
        static const float words[] = { -28, -13, -21, -7, -17, -25, -9, -19, -5, -15, -23, -11, -27, -8 };
        const int n = (int) (sizeof (words) / sizeof (words[0]));
        const float lo = -40.0f;
        auto yFor = [&] (float db) { return r.getY() + (0.0f - db) / -lo * r.getHeight(); };
        float loudest = -100.0f, quietIn = 0.0f, quietOut = 0.0f, loudIn = -100.0f, loudOut = -100.0f;
        for (float w : words) loudest = juce::jmax (loudest, w);
        const float makeup = -Compressor::computeGainDb (loudest, compThr, compRatio, 6.0f);
        const float cw = r.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float in = words[i], out = in + Compressor::computeGainDb (in, compThr, compRatio, 6.0f) + makeup;
            const auto cell = juce::Rectangle<float> (r.getX() + (float) i * cw, r.getY(), cw, r.getHeight());
            g.setColour (text.withAlpha (0.22f));
            g.fillRoundedRectangle (cell.withTrimmedRight (cw * 0.52f).reduced (2.0f, 0.0f).withTop (yFor (in)), 2.0f);
            g.setColour (yellow);
            g.fillRoundedRectangle (cell.withTrimmedLeft (cw * 0.48f).reduced (2.0f, 0.0f).withTop (yFor (out)), 2.0f);
            if (in == -28.0f) { quietIn = in; quietOut = out; }
            if (in == loudest) { loudIn = in; loudOut = out; }
        }
        const float ty = yFor (compThr);
        g.setColour (text);
        for (float x = r.getX(); x < r.getRight(); x += 9.0f) g.fillRect (x, ty - 0.5f, 5.0f, 1.0f);
        g.setFont (bodyBold (11.0f));
        g.drawText ("Threshold", juce::Rectangle<float> (r.getX(), ty - 16.0f, 80.0f, 12.0f), juce::Justification::centredLeft);

        g.setFont (bodyBold (12.5f)); g.setColour (text);
        g.drawText ("Threshold " + juce::String (juce::roundToInt (compThr)) + " dB   Ratio " + juce::String (compRatio, 1) + ":1", foot.removeFromTop (18.0f), juce::Justification::centredLeft);
        const float before = loudIn - quietIn, after = loudOut - quietOut;
        drawWrapped (g, "The loudest word is turned down " + juce::String (makeup, 1) + " dB, then everything is brought back up. The gap between the loudest and quietest word goes from "
                        + juce::String (juce::roundToInt (before)) + " dB to " + juce::String (juce::roundToInt (after)) + " dB.", foot, body (12.0f), textDim);
    }

    // ---- attack and release ----------------------------------------------------------------------------
    void paintAttack (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (16.0f);
        auto foot = r.removeFromBottom (54.0f);
        const int n = 600;                       // one point per millisecond: two hits, 300 ms apart
        const float thr = -20.0f, ratio = 6.0f, lo = -42.0f;
        auto input = [] (int ms) { const int t = ms % 300; return t < 2 ? -40.0f + 37.0f * (float) t / 2.0f : juce::jmax (-40.0f, -3.0f - 30.0f * (1.0f - std::exp (-(float) (t - 2) / 55.0f))); };
        const float aCoef = std::exp (-1.0f / juce::jmax (0.05f, attackMs)), rCoef = std::exp (-1.0f / juce::jmax (1.0f, releaseMs));
        auto yFor = [&] (float db) { return r.getY() + (0.0f - db) / -lo * r.getHeight(); };
        juce::Path in, out, red;
        float gr = 0.0f;
        red.startNewSubPath (r.getX(), r.getY());
        for (int pass = 0; pass < 2; ++pass)     // first pass settles the envelope so the picture loops cleanly
            for (int i = 0; i < n; ++i)
            {
                const float level = input (i);
                const float want = Compressor::computeGainDb (level, thr, ratio, 4.0f);
                gr = want < gr ? aCoef * gr + (1.0f - aCoef) * want : rCoef * gr + (1.0f - rCoef) * want;
                if (pass == 0) continue;
                const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
                if (i == 0) { in.startNewSubPath (x, yFor (level)); out.startNewSubPath (x, yFor (level + gr)); }
                else { in.lineTo (x, yFor (level)); out.lineTo (x, yFor (level + gr)); }
                red.lineTo (x, r.getY() + (-gr) / -lo * r.getHeight());
            }
        red.lineTo (r.getRight(), r.getY()); red.closeSubPath();
        g.setColour (yellow.withAlpha (0.2f)); g.fillPath (red);
        g.setColour (text.withAlpha (0.3f)); g.strokePath (in, juce::PathStrokeType (1.5f));
        g.setColour (yellow); g.strokePath (out, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
        g.setColour (text.withAlpha (0.5f));
        for (float x = r.getX(); x < r.getRight(); x += 9.0f) g.fillRect (x, yFor (thr) - 0.5f, 5.0f, 1.0f);
        g.setColour (text.withAlpha (0.25f));
        g.fillRect (r.getX() + r.getWidth() * phase, r.getY(), 1.0f, r.getHeight());

        g.setFont (bodyBold (12.5f)); g.setColour (text);
        g.drawText ("Attack " + (attackMs < 10.0f ? juce::String (attackMs, 1) : juce::String (juce::roundToInt (attackMs))) + " ms   Release " + juce::String (juce::roundToInt (releaseMs)) + " ms", foot.removeFromTop (18.0f), juce::Justification::centredLeft);
        const juce::String msg = attackMs < 3.0f ? "The compressor grabs the very front of each hit. Controlled, but the crack is flattened."
                               : attackMs < 15.0f ? "A little of the hit gets through before the level drops. A good all-round vocal setting."
                               : "The hit passes untouched and only the body is turned down. The drum sounds punchier than it did going in.";
        drawWrapped (g, msg + (releaseMs < 60.0f ? " The fast release brings the level straight back between hits." : releaseMs > 220.0f ? " The slow release is still holding the level down when the next hit arrives." : ""), foot, body (12.0f), textDim);
    }

    // ---- loudness --------------------------------------------------------------------------------------
    void paintLoudness (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (16.0f);
        struct Bar { const char* name; float lufs; const char* note; };
        static const Bar bars[] = { { "Mix before mastering", -18.0f, "Headroom for the master" }, { "Apple Music", -16.0f, "Turned to about here" },
                                    { "Spotify and YouTube", -14.0f, "Turned to about here" }, { "Loud hip-hop master", -9.0f, "Gets turned down on streaming" } };
        const float rowH = r.getHeight() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) i, r.getWidth(), rowH).reduced (0.0f, 8.0f);
            g.setFont (bodyBold (12.5f)); g.setColour (text);
            g.drawText (bars[i].name, row.removeFromTop (18.0f), juce::Justification::centredLeft);
            g.setFont (body (11.5f)); g.setColour (textMute);
            g.drawText (bars[i].note, row.removeFromBottom (16.0f), juce::Justification::centredLeft);
            auto track = row.reduced (0.0f, 3.0f);
            g.setColour (control); g.fillRoundedRectangle (track, 3.0f);
            const float amount = juce::jlimit (0.0f, 1.0f, (bars[i].lufs + 24.0f) / 18.0f);
            const float grow = juce::jmin (1.0f, 0.6f + 0.4f * std::sin (juce::jmin (1.0f, phase * 1.0f + 0.5f) * juce::MathConstants<float>::halfPi));
            g.setColour (i == 2 ? yellow : text.withAlpha (0.8f));
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * amount * grow), 3.0f);
            g.setFont (headline (18.0f)); g.setColour (i == 2 ? yellow : text);
            g.drawText (juce::String (juce::roundToInt (bars[i].lufs)) + " LUFS", row.withTrimmedLeft (row.getWidth() - 90.0f).translated (0.0f, -22.0f), juce::Justification::centredRight);
        }
    }

    Diagram kind;
    int region = 2;
    float eqFreq = 2500.0f, eqGain = 4.0f, eqQ = 1.4f;
    float compThr = -16.0f, compRatio = 4.0f;
    float attackMs = 1.0f, releaseMs = 120.0f;
    float phase = 0.0f;
};

// =====================================================================================================
LearnPage::LearnPage (MUProcessor& p, std::function<void (const juce::String&)> status, std::function<void (int)> go)
    : proc (p), setStatus (std::move (status)), goToTab (std::move (go))
{
    styleButton (tryButton, "Try it", true);
    tryButton.onClick = [this]
    {
        const auto& l = kLessons[current];
        if (l.preset != nullptr)
        {
            const int i = findChainPreset (l.preset);
            if (i >= 0) setStatus (proc.applyChainPreset (i) + " Switch each module off and on to hear what it adds.");
        }
        if (goToTab) goToTab (l.tab);
    };
    addAndMakeVisible (tryButton);
    showLesson (0);
}

LearnPage::~LearnPage() = default;

void LearnPage::showLesson (int i)
{
    current = juce::jlimit (0, kNumLessons - 1, i);
    const auto& l = kLessons[current];
    diagram.reset();
    if (l.diagram != Diagram::None)
    {
        diagram = std::make_unique<LessonDiagram> (l.diagram);
        addAndMakeVisible (*diagram);
    }
    tryButton.setButtonText (l.tryLabel);
    fade = 0.0f;
    resized();
    repaint();
}

int LearnPage::lessonAt (juce::Point<int> p) const
{
    if (! listArea.contains (p)) return -1;
    const int i = (p.y - listArea.getY() - 46) / 36;
    return (i >= 0 && i < kNumLessons && p.y >= listArea.getY() + 46) ? i : -1;
}

void LearnPage::mouseDown (const juce::MouseEvent& e) { const int i = lessonAt (e.getPosition()); if (i >= 0 && i != current) showLesson (i); }
void LearnPage::mouseMove (const juce::MouseEvent& e)
{
    const int h = lessonAt (e.getPosition());
    if (h != hover) { hover = h; repaint (listArea); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void LearnPage::mouseExit (const juce::MouseEvent&) { hover = -1; repaint (listArea); }

void LearnPage::tick (float dt)
{
    const float target = (float) (listArea.getY() + 46 + current * 36);
    if (std::abs (blockY - target) > 0.3f) { blockY = approach (blockY, target, dt, 0.05f); repaint (listArea); }
    if (fade < 1.0f) { fade = juce::jmin (1.0f, fade + dt * 5.0f); repaint (contentArea); if (diagram != nullptr) diagram->setAlpha (fade); }
    if (diagram != nullptr) diagram->tick (dt);
}

void LearnPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    fillPanel (g, listArea.toFloat());
    auto head = listArea.reduced (16, 0).removeFromTop (46);
    g.setFont (headline (20.0f)); g.setColour (text);
    g.drawText ("LESSONS", head, juce::Justification::centredLeft);
    g.setColour (yellow);
    g.fillRect (juce::Rectangle<float> ((float) listArea.getX() + 8.0f, blockY, (float) listArea.getWidth() - 16.0f, 34.0f));
    for (int i = 0; i < kNumLessons; ++i)
    {
        const auto row = juce::Rectangle<int> (listArea.getX() + 8, listArea.getY() + 46 + i * 36, listArea.getWidth() - 16, 34);
        const bool on = std::abs (blockY - (float) row.getY()) < 17.0f;
        g.setFont (headline (15.0f));
        g.setColour (on ? onYellow : textMute);
        g.drawText (juce::String (i + 1), row.withTrimmedLeft (10).withWidth (22), juce::Justification::centredLeft);
        g.setFont (on ? bodyBold (12.5f) : body (12.5f));
        g.setColour (on ? onYellow : (i == hover ? text : textDim));
        g.drawText (kLessons[i].title, row.withTrimmedLeft (36), juce::Justification::centredLeft);
    }

    fillPanel (g, contentArea.toFloat());
    const auto& l = kLessons[current];
    auto t = textArea;
    g.setFont (headline (34.0f)); g.setColour (text.withAlpha (fade));
    g.drawText (juce::String (l.title).toUpperCase(), t.removeFromTop (44), juce::Justification::centredLeft);
    g.setColour (yellow.withAlpha (fade));
    g.fillRect (t.removeFromTop (3).withWidth (56));
    t.removeFromTop (18);
    drawWrapped (g, l.body, t.toFloat(), body (13.5f), text.withAlpha (0.86f * fade), 1.45f);
}

void LearnPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    listArea = r.removeFromLeft (280);
    r.removeFromLeft (10);
    contentArea = r;
    if (blockY <= 0.0f) blockY = (float) (listArea.getY() + 46 + current * 36);
    auto c = contentArea.reduced (28, 22);
    auto buttonRow = c.removeFromBottom (36);
    tryButton.setBounds (buttonRow.removeFromLeft (190));
    c.removeFromBottom (14);
    if (diagram != nullptr)
    {
        const int dw = juce::jmin (460, c.getWidth() * 48 / 100);
        diagram->setBounds (c.removeFromRight (dw));
        c.removeFromRight (28);
    }
    textArea = c.withWidth (juce::jmin (c.getWidth(), 600));
}
}
