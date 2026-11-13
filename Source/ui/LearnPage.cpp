#include "LearnPage.h"
#include "../PluginProcessor.h"
#include "HeaderBar.h"

namespace mu
{
using namespace theme;

namespace
{
    enum class Scene { Wave, EqShapes, EqMove, FreqMap, Masking, Levels, Envelope, Saturation, Stereo, Loudness, TruePeak, Chain };
    struct Step { const char* title; const char* text; Scene scene; int variant; };
    struct Lesson { const char* chapter; const char* title; std::vector<Step> steps; const char* tryLabel; int tab; const char* preset; };

    const std::vector<Lesson>& lessons()
    {
        static const std::vector<Lesson> all = {
        { "Foundations", "How sound works", {
            { "A sound is a vibration",
              "Every sound is air moving back and forth. Slow vibrations are low notes and fast ones are high notes. The speed is called the frequency and is measured in hertz (Hz): 100 Hz means 100 swings every second. Each time the frequency doubles, the note goes up one octave.", Scene::Wave, 0 },
            { "Height is level",
              "How far the wave swings is its level, measured in decibels (dB). Decibels are a ratio, not a fixed amount: 6 dB less is half the height of the wave. Around 10 dB less is what most people hear as half as loud.", Scene::Wave, 1 },
            { "Real sounds are many frequencies at once",
              "A voice, a snare or an 808 is many of these waves added together. The lowest one is the note you hear. The ones above it, the harmonics, give the sound its character. The picture underneath is a spectrum: low notes on the left, high notes on the right, taller means louder.", Scene::Wave, 2 },
            { "The frequency map",
              "Engineers use the same few words for the same areas of the spectrum. Move the mouse across the map to read what lives in each area, what too much of it sounds like and what too little sounds like. Once you can name a problem you know where to reach.", Scene::FreqMap, 0 } },
          "Open the EQ", HeaderBar::EQ, nullptr },

        { "EQ", "What an EQ is", {
            { "A volume knob for one part of the sound",
              "An equaliser turns one range of frequencies up or down and leaves the rest alone. Watch the middle wave: the EQ pulls it down and only that part of the sound changes. That is all an EQ ever does.", Scene::Wave, 3 },
            { "The bell",
              "The most used shape is the bell. It has three controls: where it acts (frequency), how much (gain) and how wide (Q). The white line is what the EQ is doing. Grey is the sound going in, yellow is the sound coming out.", Scene::EqShapes, 0 },
            { "Width, or Q",
              "Q is the width of the band: the centre frequency divided by how wide the band is, so a bigger number means a narrower band. A low Q is wide and gentle and changes the overall tone. A high Q is narrow and precise and removes one ringing note without touching its neighbours.", Scene::EqShapes, 1 },
            { "Shelves and cuts",
              "A shelf raises or lowers everything above or below a point, like a treble or bass control. A cut filter removes everything beyond a point. Its slope says how steeply: 12 dB per octave is gentle, 24 is the usual low cut on a vocal to clear rumble below 80 to 100 Hz.", Scene::EqShapes, 2 },
            { "Your turn: cut first, then boost",
              "This vocal has a build-up in the low mids that makes it cloudy. Boosting the highs would only make everything louder and leave the cloud in place. Drag the band onto the hump and pull it down until the yellow line is smooth. Scroll to change the width.", Scene::EqMove, 0 } },
          "Analyse my track", HeaderBar::EQ, nullptr },

        { "EQ", "Making room", {
            { "Masking",
              "When two sounds are loud in the same range, the ear follows the louder one and loses the other. This is called masking. It is why a vocal can disappear under a beat even though each sounded fine on its own.", Scene::Masking, 0 },
            { "Carve a pocket",
              "Turning the vocal up only starts a volume war. The better fix is a pocket: dip the beat a few decibels in the range where the words are understood, around 2 to 4 kHz, and only while the vocal is there. Vocal space on the Tone tab and the collision fixes on the Mix tab both do this.", Scene::Masking, 1 },
            { "Dynamic bands",
              "A dynamic band is an EQ cut that waits. It does nothing until its range gets louder than a threshold, then it dips, then it lets go again. Use it for problems that come and go, such as a boomy word or a harsh note, so the track is untouched the rest of the time.", Scene::EqShapes, 4 } },
          "Find collisions", HeaderBar::Mix, nullptr },

        { "Dynamics", "What a compressor is", {
            { "No performance is even",
              "Some words are loud and some are almost lost. Turn the whole track up so the quiet words are heard and the loud ones jump out. Turn it down for the loud ones and the quiet ones vanish.", Scene::Levels, 0 },
            { "Threshold",
              "A compressor is an automatic volume control. You choose a level called the threshold. Anything below it is left alone. Only the part of the sound that goes above it, shown in yellow, gets turned down.", Scene::Levels, 1 },
            { "Ratio",
              "The ratio sets how firmly. At 4:1, a word that goes 8 dB over the threshold comes out only 2 dB over. 2:1 is gentle, 4:1 is a firm vocal setting, and around 10:1 and above it is acting as a limiter.", Scene::Levels, 2 },
            { "Makeup gain",
              "The loud words are now quieter, so the whole track can be turned back up. That is makeup gain. The loud words finish where they started and the quiet words are much closer behind them. The track sounds fuller and stays in front of the beat.", Scene::Levels, 3 },
            { "Your turn",
              "Drag up and down to move the threshold, left and right to change the ratio. Grey is what goes in, yellow is what comes out. On a vocal, aim for the loud words to be turned down by about 3 to 6 dB.", Scene::Levels, 4 } },
          "Open Dynamics", HeaderBar::Dynamics, nullptr },

        { "Dynamics", "Attack and release", {
            { "It does not happen instantly",
              "A compressor starts turning down the moment the sound crosses the threshold, but it takes time to get there. The attack time is how long it takes to do about two thirds of the job. The release time is how long it takes to give two thirds of it back once the sound drops again.", Scene::Envelope, 0 },
            { "Attack shapes the hit",
              "With a fast attack the level is already down while the hit is still happening, so the crack of a drum or the front of a word is flattened. With a slower attack, 20 to 30 ms, the hit gets through before the level drops, and the sound comes out punchier than it went in.", Scene::Envelope, 1 },
            { "Release shapes the tail",
              "A fast release brings the level straight back up between hits, which sounds loud and energetic. Too fast and you hear the volume pumping. A slow release is smoother and suits singing and bass, but if it is too slow the level is still held down when the next hit arrives.", Scene::Envelope, 2 },
            { "Your turn",
              "Drag left and right for the attack, up and down for the release, and watch what happens to the front of each hit and to the gap between them.", Scene::Envelope, 3 } },
          "Open Dynamics", HeaderBar::Dynamics, nullptr },

        { "Dynamics", "Gate and de-esser", {
            { "What is between the words",
              "A microphone keeps listening when the singer stops. Headphone bleed, breaths and room noise sit between the phrases, and compression makes them louder.", Scene::Levels, 10 },
            { "The gate",
              "A gate is the opposite of a compressor: it turns the track down when the level falls below its threshold. Set the threshold just above the noise, and check that it is not cutting the starts and ends of words. A range of 10 to 20 dB sounds more natural than silence.", Scene::Levels, 11 },
            { "The de-esser",
              "S and T sounds are short bursts of energy, usually between 5 and 8 kHz. A de-esser is a compressor that only listens to that range and dips it for the moment the S lasts. It goes after the compressor, because compression makes those sounds louder. Three to six decibels is normally enough.", Scene::EqShapes, 5 } },
          "Open Dynamics", HeaderBar::Dynamics, nullptr },

        { "Tone", "Saturation", {
            { "Bending the wave",
              "Push a signal harder than tape or a valve can handle cleanly and the tops of the wave get rounded off. That bend adds new frequencies above the note that was played. They are harmonics, and they make a sound feel thicker and closer without making it much louder.", Scene::Saturation, 0 },
            { "Which harmonics",
              "The shape of the bend decides which harmonics appear. Bend both halves of the wave the same way and you get odd harmonics: the 3rd, the 5th, edge and bite. Bend one half more than the other, as valves tend to, and even harmonics appear too. The strongest of those are octaves of the note, so the sound gets fuller instead of edgier.", Scene::Saturation, 1 },
            { "Why an 808 needs it",
              "The note of an 808 sits around 40 to 60 Hz. Phone and laptop speakers play almost nothing that low, so a clean 808 simply disappears. Saturation adds harmonics at two, three and four times the note. Those are high enough to be heard, and the ear fills in the bass from them.", Scene::Saturation, 2 } },
          "Open Tone", HeaderBar::Tone, nullptr },

        { "Tone", "Stereo and mono", {
            { "Width",
              "Stereo is two speakers. A sound that is the same in both appears in the middle. The more the two sides differ, the wider it feels. Spread pads and backing vocals out to the sides and keep the lead vocal, kick and bass in the centre, where they are solid on every system.", Scene::Stereo, 0 },
            { "The mono check",
              "Plenty of playback is close to mono: a phone, a club's subwoofers, a speaker across the room. In mono the two sides are added together. Anything that is opposite in the left and right cancels out and vanishes. The correlation meter warns you: towards +1 is safe, below zero is trouble.", Scene::Stereo, 1 } },
          "Open Tone", HeaderBar::Tone, nullptr },

        { "Finishing", "Loudness and mastering", {
            { "Peak is not loudness",
              "These two sounds reach exactly the same peak. One is a short hit, the other is held. The held one is far louder to the ear. Peak level tells you whether the signal will clip. Loudness, measured in LUFS, tells you how loud it feels.", Scene::Loudness, 0 },
            { "What streaming does to a loud master",
              "Streaming services measure every song and play them at a similar loudness: about -14 LUFS on Spotify and YouTube, -16 on Apple Music. A master pushed much louder is simply turned down. It gains nothing, and keeps the flattened peaks it paid to get there.", Scene::Loudness, 1 },
            { "True peak",
              "Digital audio is a row of samples, and the real wave runs between them. It can rise higher than any single sample. That is the true peak. Leave about 1 dB spare so the song does not distort when it is converted for streaming.", Scene::TruePeak, 0 },
            { "The limiter",
              "A limiter is a compressor with a very high ratio and a ceiling that nothing passes. It lets you raise the level until the loudest peaks touch the ceiling. A few decibels on the biggest hits is normal. Push further and every hit is shaved to the same height: louder, but flat.", Scene::Levels, 21 },
            { "Where to aim",
              "Before mastering, a mix should sit around -18 LUFS with peaks under -6 dB. That headroom is what the master uses. For streaming, finish at about -14 LUFS with true peak under -1 dB.", Scene::Loudness, 2 } },
          "Open Master", HeaderBar::Master, nullptr },

        { "Finishing", "Putting it together", {
            { "The order of a mix",
              "Work from big decisions to small ones. Balance first: most of a good mix is simply every track at the right level against the vocal. Then space: decide who wins where two tracks collide. Then tone and control on single tracks. The master comes last. It makes a good mix louder; it does not rescue a bad one.", Scene::Chain, 0 },
            { "A vocal chain, in order",
              "Each stage hears the result of the one before it, so the order matters. Clean up first, remove problems before the compressor reacts to them, control the level, tame the S sounds the compressor has raised, and only then add brightness and colour. The vocal presets are built in this order.", Scene::Chain, 1 } },
          "Load a vocal chain", HeaderBar::Dynamics, "Rap vocal - punchy" } };
        return all;
    }

    struct Region { float lo, hi; const char* name; const char* lives; const char* tooMuch; const char* tooLittle; };
    const Region kRegions[] = {
        { 20, 60,       "Sub",               "The lowest note of an 808 and the weight under a kick. Felt in the chest more than heard.", "Eats headroom and makes the limiter pump.", "The track feels small on big speakers." },
        { 60, 150,      "Bass",              "The body of bass lines and the punch of the kick.", "Boomy, and it swallows the bottom of the vocal.", "Thin, with no drive." },
        { 150, 350,     "Warmth and mud",    "The body of voices, keys and guitars. Also where reverb and room sound pile up.", "Muddy and cloudy. The most common mix problem.", "Cold and hollow." },
        { 350, 700,     "Boxiness",          "The woody middle of drums and the chest of a voice recorded in a small room.", "Sounds like it was recorded inside a cardboard box.", "Scooped, lacking body." },
        { 700, 1500,    "Honk",              "The nose of a voice and the character of mid-range instruments.", "Nasal, like a telephone.", "Distant and hard to place." },
        { 1500, 4000,   "Presence",          "Where the ear is most sensitive and where words are understood.", "Harsh and tiring within a minute.", "The vocal sinks behind the beat." },
        { 4000, 8000,   "Bite and S sounds", "Consonants, the snap of a snare, the edge of hi-hats.", "Spitty S sounds and piercing hats.", "Dull, with no definition." },
        { 8000, 12000,  "Brightness",        "Polish and shine. The sizzle on cymbals.", "Brittle and fizzy.", "Sounds like it is under a blanket." },
        { 12000, 20000, "Air",               "The breath around a voice. Openness.", "Hiss and noise come up with it.", "Closed in, though many speakers barely play this." } };
    constexpr int kNumRegions = (int) (sizeof (kRegions) / sizeof (kRegions[0]));

    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    const std::pair<float, const char*> kDecades[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
    float osc (float t, float period) { return 0.5f - 0.5f * std::cos (kTwoPi * t / period); }     // 0..1..0, smooth
    float gainOf (float db) { return std::pow (10.0f, db / 20.0f); }
    void dashedH (juce::Graphics& g, float x0, float x1, float y) { for (float x = x0; x < x1; x += 9.0f) g.fillRect (x, y - 0.5f, juce::jmin (5.0f, x1 - x), 1.0f); }
    void dashedV (juce::Graphics& g, float x, float y0, float y1) { for (float y = y0; y < y1; y += 9.0f) g.fillRect (x - 0.5f, y, 1.0f, juce::jmin (5.0f, y1 - y)); }
    void tag (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false)
    {
        g.setFont (bold ? bodyBold (12.0f) : body (11.5f));
        g.setColour (c);
        const float w = 700.0f;
        g.drawText (s, juce::Rectangle<float> (j.testFlags (juce::Justification::right) ? x - w : j.testFlags (juce::Justification::horizontallyCentred) ? x - w * 0.5f : x, y - 8.0f, w, 16.0f), j);
    }
    // a label that sits on top of busy graphics, on its own dark backing
    void tagOn (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, bool bold = true)
    {
        const auto font = bold ? bodyBold (12.0f) : body (11.5f);
        const float w = font.getStringWidthFloat (s) + 14.0f;
        g.setColour (black.withAlpha (0.82f)); g.fillRoundedRectangle (x, y - 10.0f, w, 20.0f, 4.0f);
        g.setFont (font); g.setColour (c);
        g.drawText (s, juce::Rectangle<float> (x + 7.0f, y - 8.0f, w, 16.0f), juce::Justification::centredLeft);
    }
    juce::String plainHz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1).replace (".0", "") + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }
}

// =====================================================================================================
class LessonStage : public juce::Component
{
public:
    LessonStage() { setOpaque (false); }

    void show (Scene s, int v)
    {
        if (s == scene && v == variant && started) return;
        nextScene = s; nextVariant = v;
        if (! started) { scene = s; variant = v; started = true; fade = 0.0f; fadingOut = false; t = 0.0f; }
        else fadingOut = true;
        setMouseCursor (interactive (s, v) ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
    }

    void tick (float dt)
    {
        t += dt;
        if (fadingOut) { fade -= dt * 9.0f; if (fade <= 0.0f) { fade = 0.0f; fadingOut = false; scene = nextScene; variant = nextVariant; t = 0.0f; chainPick = -1; } }
        else if (fade < 1.0f) fade = juce::jmin (1.0f, fade + dt * 6.0f);
        idle += dt;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        drawWell (g, getLocalBounds().toFloat(), 8.0f);
        const auto r = getLocalBounds().toFloat().reduced (22.0f, 18.0f);
        g.saveState();
        g.reduceClipRegion (getLocalBounds().reduced (2));
        g.beginTransparencyLayer (fade);
        switch (scene)
        {
            case Scene::Wave:       paintWave (g, r); break;
            case Scene::EqShapes:   paintEqShapes (g, r); break;
            case Scene::EqMove:     paintEqMove (g, r); break;
            case Scene::FreqMap:    paintMap (g, r); break;
            case Scene::Masking:    paintMasking (g, r); break;
            case Scene::Levels:     paintLevels (g, r); break;
            case Scene::Envelope:   paintEnvelope (g, r); break;
            case Scene::Saturation: paintSaturation (g, r); break;
            case Scene::Stereo:     paintStereo (g, r); break;
            case Scene::Loudness:   paintLoudness (g, r); break;
            case Scene::TruePeak:   paintTruePeak (g, r); break;
            case Scene::Chain:      paintChain (g, r); break;
        }
        g.endTransparencyLayer();
        g.restoreState();
        if (interactive (scene, variant))
        {
            const auto pill = juce::Rectangle<float> ((float) getWidth() - 122.0f, 12.0f, 110.0f, 20.0f);
            g.setColour (yellow.withAlpha (fade)); g.fillRoundedRectangle (pill, 10.0f);
            g.setFont (bodyBold (11.0f)); g.setColour (onYellow.withAlpha (fade));
            g.drawText (scene == Scene::FreqMap ? "Move the mouse" : scene == Scene::Chain ? "Click a stage" : "Drag to try it", pill, juce::Justification::centred);
        }
    }

    // ---- interaction -------------------------------------------------------------------------------
    static bool interactive (Scene s, int v)
    {
        return s == Scene::EqMove || s == Scene::FreqMap || s == Scene::Chain || (s == Scene::Levels && v == 4) || (s == Scene::Envelope && v == 3);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        if (scene != Scene::FreqMap) return;
        const auto r = getLocalBounds().toFloat().reduced (22.0f, 18.0f);
        const float hz = xToFreq ((float) e.x, r.getX(), r.getWidth());
        for (int i = 0; i < kNumRegions; ++i) if (hz >= kRegions[i].lo && hz < kRegions[i].hi) region = i;
        idle = 0.0f;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (scene == Scene::Chain)
        {
            const auto r = getLocalBounds().toFloat().reduced (22.0f, 18.0f);
            const int n = chainCount();
            chainPick = juce::jlimit (0, n - 1, (int) (((float) e.x - r.getX()) / (r.getWidth() / (float) n)));
            return;
        }
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat().reduced (22.0f, 18.0f);
        const float nx = juce::jlimit (0.0f, 1.0f, ((float) e.x - r.getX()) / r.getWidth()), ny = juce::jlimit (0.0f, 1.0f, ((float) e.y - r.getY()) / r.getHeight());
        if (scene == Scene::EqMove)
        {
            const auto plot = r.withTrimmedBottom (46.0f);
            eqFreq = xToFreq ((float) e.x, plot.getX(), plot.getWidth(), 40.0f, 16000.0f);
            eqGain = juce::jlimit (-15.0f, 15.0f, (plot.getCentreY() - (float) e.y) / (plot.getHeight() * 0.5f) * 15.0f);
        }
        else if (scene == Scene::Levels && variant == 4) { userThr = -4.0f - ny * 34.0f; userRatio = 1.0f + nx * nx * 11.0f; }
        else if (scene == Scene::Envelope && variant == 3) { userAttack = 0.3f * std::pow (200.0f, nx); userRelease = 400.0f * std::pow (0.05f, ny); }
        else if (scene == Scene::FreqMap) mouseMove (e);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (scene == Scene::EqMove) eqQ = juce::jlimit (0.4f, 6.0f, eqQ * std::pow (2.0f, w.deltaY * 1.5f));
    }

private:
    // ---- sound as waves ------------------------------------------------------------------------------
    void paintWave (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto spec = r.removeFromBottom (r.getHeight() * 0.30f);
        r.removeFromBottom (14.0f);
        struct Comp { float hz, amp; const char* name; };
        std::vector<Comp> comps;
        juce::String headline;
        int marked = -1;
        if (variant == 0)
        {
            const float hz = 110.0f * std::pow (2.0f, 2.0f * osc (t, 10.0f));
            comps = { { hz, 1.0f, "" } };
            headline = juce::String (juce::roundToInt (hz)) + " Hz    " + noteName (hz) + (hz < 150.0f ? "    low note, slow wave" : hz > 330.0f ? "    high note, fast wave" : "");
        }
        else if (variant == 1)
        {
            const float db = -12.0f * osc (t, 8.0f);
            comps = { { 220.0f, gainOf (db), "" } };
            headline = dbString (db) + "    wave height " + juce::String (juce::roundToInt (gainOf (db) * 100.0f)) + " %";
        }
        else
        {
            comps = { { 80.0f, 0.9f, "Bass" }, { 400.0f, 0.55f, "Mid" }, { 2400.0f, 0.3f, "High" } };
            if (variant == 3) { const float db = -16.0f * osc (t, 6.5f); comps[1].amp *= gainOf (db); marked = 1; headline = "EQ on the mid range: " + dbString (db); }
            else headline = "Three waves, added together";
        }
        tag (g, headline, r.getX(), r.getY() + 6.0f, text, juce::Justification::centredLeft, true);
        r.removeFromTop (22.0f);

        auto wave = [&] (juce::Rectangle<float> area, std::function<float (float)> f, juce::Colour c, float thickness)
        {
            juce::Path p;
            const int n = (int) area.getWidth();
            for (int i = 0; i <= n; ++i)
            {
                const float x01 = (float) i / (float) n;
                const float y = area.getCentreY() - f (x01) * area.getHeight() * 0.46f;
                if (i == 0) p.startNewSubPath (area.getX() + (float) i, y); else p.lineTo (area.getX() + (float) i, y);
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        auto compAt = [&] (const Comp& c, float x01) { return c.amp * std::sin (kTwoPi * (c.hz / 45.0f * x01 - t * 0.55f)); };

        if (comps.size() == 1)
        {
            g.setColour (juce::Colour (0xff1c1c1c)); g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
            wave (r, [&] (float x) { return compAt (comps[0], x); }, yellow, 2.4f);
        }
        else
        {
            const float rowH = r.getHeight() * 0.17f;
            for (int i = 0; i < 3; ++i)
            {
                auto row = r.removeFromTop (rowH);
                tag (g, comps[(size_t) i].name, row.getX(), row.getCentreY(), i == marked ? yellow : textDim);
                wave (row.withTrimmedLeft (52.0f), [&] (float x) { return compAt (comps[(size_t) i], x) / 0.9f; }, i == marked ? yellow : text.withAlpha (0.45f), 1.3f);
            }
            r.removeFromTop (6.0f);
            tag (g, "Sum", r.getX(), r.getCentreY(), text);
            wave (r.withTrimmedLeft (52.0f), [&] (float x) { float s = 0.0f; for (auto& c : comps) s += compAt (c, x); return s / 1.75f; }, yellow, 2.2f);
        }

        // spectrum
        const float base = spec.getBottom() - 16.0f;
        g.setColour (lineHi); g.fillRect (spec.getX(), base, spec.getWidth(), 1.0f);
        g.setFont (body (10.0f)); g.setColour (textMute);
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f })
            g.drawText (f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "k" : juce::String (juce::roundToInt (f)),
                        juce::Rectangle<float> (freqToX (f, spec.getX(), spec.getWidth(), 30.0f, 8000.0f) - 20.0f, base + 3.0f, 40.0f, 12.0f), juce::Justification::centred);
        tag (g, "Spectrum", spec.getX(), spec.getY() + 4.0f, textMute);
        for (size_t i = 0; i < comps.size(); ++i)
        {
            const float x = freqToX (comps[i].hz, spec.getX(), spec.getWidth(), 30.0f, 8000.0f);
            const float h = juce::jlimit (0.03f, 1.0f, (20.0f * std::log10 (juce::jmax (0.001f, comps[i].amp)) + 30.0f) / 30.0f) * (spec.getHeight() - 30.0f);
            g.setColour ((int) i == marked || comps.size() == 1 ? yellow : text.withAlpha (0.75f));
            g.fillRoundedRectangle (x - 7.0f, base - h, 14.0f, h, 3.0f);
        }
    }

    // ---- EQ shapes -----------------------------------------------------------------------------------
    static float backdrop (float hz, float time)
    {
        const float oct = std::log2 (hz / 500.0f);
        return -oct * oct * 0.75f + 1.6f * std::sin (oct * 2.3f + time * 0.8f) + 1.1f * std::sin (oct * 5.1f - time * 1.3f);
    }
    void paintEqShapes (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float lo = 30.0f, hi = 18000.0f;
        EQBandSettings band;
        band.enabled = true; band.type = EQType::Bell; band.freqHz = 1000.0f; band.q = 1.2f;
        juce::String info;
        float bump = 0.0f, bumpHz = 0.0f, thrDb = 0.0f;
        const char* bumpName = "";
        if (variant == 0) { band.gainDb = 9.0f * std::sin (kTwoPi * t / 6.0f); info = "Bell at 1 kHz    gain " + dbString (band.gainDb); }
        else if (variant == 1) { band.gainDb = -9.0f; band.q = 0.5f * std::pow (12.0f, osc (t, 7.0f)); info = "Q " + juce::String (band.q, 1) + (band.q < 1.0f ? "    wide" : band.q > 3.5f ? "    narrow" : ""); }
        else if (variant == 2)
        {
            const float ph = std::fmod (t, 12.0f);
            if (ph < 6.0f) { band.type = EQType::HighShelf; band.freqHz = 5000.0f; band.q = 0.71f; band.gainDb = 7.0f * std::sin (kTwoPi * ph / 6.0f); info = "High shelf at 5 kHz    " + dbString (band.gainDb); }
            else { band.type = EQType::LowCut; band.slope = 1; band.q = 0.71f; band.freqHz = 35.0f * std::pow (8.0f, osc (ph - 6.0f, 6.0f)); info = "Low cut at " + plainHz (band.freqHz) + "    24 dB per octave"; }
        }
        else
        {
            const bool ess = variant == 5;
            bumpHz = ess ? 6800.0f : 260.0f; bumpName = ess ? "S" : "Boomy word";
            const float cycle = std::fmod (t, 3.2f);
            const float level = cycle < 1.1f ? std::sin (juce::MathConstants<float>::pi * cycle / 1.1f) : 0.0f;       // the problem comes and goes
            bump = 11.0f * level; thrDb = 4.0f;
            dynCut = approach (dynCut, bump > thrDb ? -juce::jmin (8.0f, (bump - thrDb) * 1.2f) : 0.0f, 1.0f / 60.0f, bump > thrDb ? 0.03f : 0.12f);
            band.freqHz = bumpHz; band.q = ess ? 2.2f : 1.5f; band.gainDb = dynCut;
            info = juce::String (ess ? "De-esser" : "Dynamic band") + " at " + plainHz (bumpHz) + (dynCut < -0.5f ? "    cutting " + juce::String (-dynCut, 1) + " dB" : "    resting");
        }

        const auto plot = r.withTrimmedTop (22.0f);
        tag (g, info, r.getX(), r.getY() + 6.0f, text, juce::Justification::centredLeft, true);
        g.setColour (juce::Colour (0xff1a1a1a));
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.fillRect (freqToX (f, plot.getX(), plot.getWidth(), lo, hi), plot.getY(), 1.0f, plot.getHeight());
        const float zero = plot.getY() + plot.getHeight() * 0.36f;
        g.setColour (juce::Colour (0xff2a2a2a)); g.fillRect (plot.getX(), zero, plot.getWidth(), 1.0f);
        g.setFont (body (10.0f)); g.setColour (textMute);
        for (auto& p : kDecades)
            g.drawText (p.second, juce::Rectangle<float> (freqToX (p.first, plot.getX(), plot.getWidth(), lo, hi) + 4.0f, plot.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft);

        auto curveY = [&] (float db) { return zero - db / 15.0f * plot.getHeight() * 0.34f; };
        auto specY = [&] (float db) { return plot.getY() + plot.getHeight() * (0.58f - db / 50.0f); };
        juce::Path before, after, curve;
        for (int i = 0; i <= 240; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 240.0f;
            const float hz = xToFreq (x, plot.getX(), plot.getWidth(), lo, hi);
            float in = backdrop (hz, variant >= 4 ? 0.0f : t);
            if (bump > 0.0f) { const float o = std::log2 (hz / bumpHz) / (variant == 5 ? 0.3f : 0.4f); in += bump * std::exp (-o * o); }
            const float resp = EQEngine::bandResponseDb (band, hz, 48000.0);
            if (i == 0) { before.startNewSubPath (x, specY (in)); after.startNewSubPath (x, specY (in + resp)); curve.startNewSubPath (x, curveY (resp)); }
            else { before.lineTo (x, specY (in)); after.lineTo (x, specY (in + resp)); curve.lineTo (x, curveY (resp)); }
        }
        g.saveState(); g.reduceClipRegion (plot.toNearestInt());
        g.setColour (text.withAlpha (0.28f)); g.strokePath (before, juce::PathStrokeType (1.4f));
        juce::Path fill (after); fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
        g.setColour (yellow.withAlpha (0.10f)); g.fillPath (fill);
        g.setColour (yellow); g.strokePath (after, juce::PathStrokeType (2.0f));
        g.setColour (text); g.strokePath (curve, juce::PathStrokeType (1.8f));
        g.restoreState();
        if (band.type != EQType::LowCut)
        {
            const float nx = freqToX (band.freqHz, plot.getX(), plot.getWidth(), lo, hi), ny = curveY (band.gainDb);
            g.setColour (yellow); g.fillEllipse (nx - 7.0f, ny - 7.0f, 14.0f, 14.0f);
        }
        if (bumpHz > 0.0f && bump > 1.0f)
            tag (g, bumpName, freqToX (bumpHz, plot.getX(), plot.getWidth(), lo, hi), specY (backdrop (bumpHz, 0.0f) + bump) - 14.0f, text, juce::Justification::centred, true);
        tag (g, "White: what the EQ does      Grey: in      Yellow: out", r.getRight(), r.getY() + 6.0f, textMute, juce::Justification::centredRight);
    }

    // ---- EQ challenge --------------------------------------------------------------------------------
    static float vocalShape (float hz)
    {
        const float oct = std::log2 (hz / 600.0f);
        return -oct * oct * 0.9f + 9.0f * std::exp (-std::pow (std::log2 (hz / 280.0f) / 0.45f, 2.0f));
    }
    void paintEqMove (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto foot = r.removeFromBottom (40.0f);
        r.removeFromBottom (6.0f);
        const auto plot = r;
        const float lo = 40.0f, hi = 16000.0f;
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillRect (plot.getX(), plot.getCentreY(), plot.getWidth(), 1.0f);
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.fillRect (freqToX (f, plot.getX(), plot.getWidth(), lo, hi), plot.getY(), 1.0f, plot.getHeight());
        EQBandSettings band { true, EQType::Bell, eqFreq, eqGain, eqQ };
        juce::Path before, after, curve;
        auto specY = [&] (float db) { return plot.getY() + plot.getHeight() * (0.34f - db / 40.0f); };
        for (int i = 0; i <= 220; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 220.0f;
            const float hz = xToFreq (x, plot.getX(), plot.getWidth(), lo, hi);
            const float resp = EQEngine::bandResponseDb (band, hz, 48000.0);
            const float yb = specY (vocalShape (hz)), ya = specY (vocalShape (hz) + resp), yc = plot.getCentreY() - resp / 15.0f * plot.getHeight() * 0.5f;
            if (i == 0) { before.startNewSubPath (x, yb); after.startNewSubPath (x, ya); curve.startNewSubPath (x, yc); }
            else { before.lineTo (x, yb); after.lineTo (x, ya); curve.lineTo (x, yc); }
        }
        g.saveState(); g.reduceClipRegion (plot.toNearestInt());
        g.setColour (text.withAlpha (0.25f)); g.strokePath (before, juce::PathStrokeType (1.5f));
        g.setColour (yellow); g.strokePath (after, juce::PathStrokeType (2.2f));
        g.setColour (text); g.strokePath (curve, juce::PathStrokeType (1.5f));
        const juce::Point<float> node (freqToX (eqFreq, plot.getX(), plot.getWidth(), lo, hi), plot.getCentreY() - eqGain / 15.0f * plot.getHeight() * 0.5f);
        const float pulse = 1.0f + 0.25f * osc (t, 1.4f);
        g.setColour (yellow.withAlpha (0.25f)); g.fillEllipse (node.x - 9.0f * pulse, node.y - 9.0f * pulse, 18.0f * pulse, 18.0f * pulse);
        g.setColour (yellow); g.fillEllipse (node.x - 8.0f, node.y - 8.0f, 16.0f, 16.0f);
        g.restoreState();

        const bool onIt = std::abs (std::log2 (eqFreq / 280.0f)) < 0.35f;
        const bool solved = onIt && eqGain < -6.0f && eqGain > -11.0f;
        const juce::String msg = solved ? "That is it. The hump is gone, the yellow line is smooth, and nothing else was touched."
                               : onIt && eqGain <= -11.0f ? "Too far. Now there is a hole where the body of the voice should be."
                               : onIt && eqGain < 0.0f ? "Right place. Keep pulling down until the yellow line is smooth."
                               : eqGain > 2.0f ? "A boost makes it louder, and the hump is still there. Try cutting instead."
                               : "Drag the band onto the hump around 280 Hz, then pull it down.";
        tag (g, plainHz (eqFreq) + "    " + dbString (eqGain) + "    Q " + juce::String (eqQ, 1), foot.getX(), foot.getY() + 8.0f, solved ? yellow : text, juce::Justification::centredLeft, true);
        tag (g, msg, foot.getX(), foot.getY() + 28.0f, textDim);
    }

    // ---- frequency map -------------------------------------------------------------------------------
    void paintMap (juce::Graphics& g, juce::Rectangle<float> r)
    {
        if (idle > 3.0f) region = ((int) (t / 2.2f)) % kNumRegions;      // tours the map until the mouse takes over
        auto strip = r.removeFromLeft (r.getWidth() * 0.56f);
        r.removeFromLeft (26.0f);
        const float base = strip.getBottom() - 20.0f;
        for (int i = 0; i < kNumRegions; ++i)
        {
            const float x0 = freqToX (kRegions[i].lo, strip.getX(), strip.getWidth()), x1 = freqToX (kRegions[i].hi, strip.getX(), strip.getWidth());
            const bool on = i == region;
            const float sens = 0.42f + 0.58f * std::exp (-std::pow (std::log2 (std::sqrt (kRegions[i].lo * kRegions[i].hi) / 2800.0f) / 2.6f, 2.0f));
            const float h = (strip.getHeight() - 30.0f) * sens * (on ? 1.0f : 0.94f);
            g.setColour (on ? yellow : control);
            g.fillRoundedRectangle (x0 + 1.5f, base - h, x1 - x0 - 3.0f, h, 4.0f);
            g.setFont (body (10.0f)); g.setColour (on ? text : textMute);
            g.drawText (kRegions[i].lo >= 1000.0f ? juce::String (kRegions[i].lo / 1000.0f, 1).replace (".0", "") + "k" : juce::String ((int) kRegions[i].lo),
                        juce::Rectangle<float> (x0 - 1.0f, base + 4.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
        }
        tag (g, "Taller bars: where hearing is most sensitive", strip.getX(), strip.getY() + 6.0f, textMute);

        const auto& reg = kRegions[region];
        g.setFont (headline (28.0f)); g.setColour (yellow);
        g.drawText (juce::String (reg.name).toUpperCase(), r.removeFromTop (34.0f), juce::Justification::centredLeft);
        g.setFont (body (12.0f)); g.setColour (textMute);
        g.drawText (plainHz (reg.lo) + " to " + plainHz (reg.hi), r.removeFromTop (18.0f), juce::Justification::centredLeft);
        r.removeFromTop (10.0f);
        auto para = [&] (const juce::String& label, const juce::String& s)
        {
            g.setFont (bodyBold (12.5f)); g.setColour (text);
            g.drawText (label, r.removeFromTop (18.0f), juce::Justification::centredLeft);
            const float h = wrappedHeight (s, r.getWidth(), body (12.5f));
            drawWrapped (g, s, r.removeFromTop (h), body (12.5f), textDim);
            r.removeFromTop (10.0f);
        };
        para ("What lives here", reg.lives);
        para ("Too much", reg.tooMuch);
        para ("Too little", reg.tooLittle);
    }

    // ---- masking -------------------------------------------------------------------------------------
    void paintMasking (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float lo = 40.0f, hi = 16000.0f;
        const bool pocket = variant == 1;
        const float cycle = std::fmod (t, 6.0f);
        const float sing = pocket ? juce::jlimit (0.0f, 1.0f, cycle < 4.0f ? cycle / 0.5f : 1.0f - (cycle - 4.0f) / 0.5f) : 1.0f;
        dip = approach (dip, pocket ? -8.0f * sing : 0.0f, 1.0f / 60.0f, 0.18f);
        auto vocal = [&] (float hz) { const float o = std::log2 (hz / 1800.0f); return -3.0f - o * o * 2.6f + 1.2f * std::sin (o * 4.0f + t * 2.0f) * sing; };
        auto beat = [&] (float hz)
        {
            const float o = std::log2 (hz / 90.0f), p = std::log2 (hz / 2800.0f) / 0.75f;
            return -4.0f - std::abs (o) * 3.6f + 15.0f * std::exp (-p * p) + dip * std::exp (-p * p) + 0.8f * std::sin (o * 3.0f - t);
        };
        auto y = [&] (float db) { return r.getY() + r.getHeight() * (0.16f - db / 38.0f); };
        juce::Path pb, pv, clash;
        bool inClash = false; float clashStart = 0.0f;
        std::vector<juce::Rectangle<float>> zones;
        for (int i = 0; i <= 260; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 260.0f;
            const float hz = xToFreq (x, r.getX(), r.getWidth(), lo, hi);
            const float b = beat (hz), v = vocal (hz);
            if (i == 0) { pb.startNewSubPath (x, y (b)); pv.startNewSubPath (x, y (v)); } else { pb.lineTo (x, y (b)); pv.lineTo (x, y (v)); }
            const bool c = hz > 1200.0f && hz < 6000.0f && b > v + 1.0f;
            if (c && ! inClash) { inClash = true; clashStart = x; }
            if (! c && inClash) { inClash = false; zones.push_back ({ clashStart, r.getY() + 24.0f, x - clashStart, r.getHeight() - 44.0f }); }
        }
        juce::Path fill (pb); fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.saveState(); g.reduceClipRegion (r.toNearestInt());
        g.setColour (text.withAlpha (0.10f)); g.fillPath (fill);
        g.setColour (text.withAlpha (0.4f)); g.strokePath (pb, juce::PathStrokeType (1.4f));
        for (auto& z : zones) { g.setColour (over.withAlpha (0.10f + 0.10f * osc (t, 1.2f))); g.fillRect (z); }
        g.setColour (yellow.withAlpha (0.35f + 0.65f * sing)); g.strokePath (pv, juce::PathStrokeType (2.4f));
        g.restoreState();
        tag (g, "Beat", r.getX() + 6.0f, y (beat (70.0f)) - 12.0f, textDim, juce::Justification::centredLeft, true);
        tag (g, sing > 0.5f ? "Vocal" : "Vocal (resting)", freqToX (700.0f, r.getX(), r.getWidth(), lo, hi), y (vocal (700.0f)) - 14.0f, yellow, juce::Justification::centredLeft, true);
        const float px = freqToX (2800.0f, r.getX(), r.getWidth(), lo, hi);
        if (zones.empty() && pocket) tag (g, "Pocket: the words come through", px, r.getY() + 10.0f, yellow, juce::Justification::centred, true);
        else if (! zones.empty()) tag (g, "The beat is covering the words here", px, r.getY() + 10.0f, over, juce::Justification::centred, true);
        else tag (g, "Vocal out: the beat gets its full sound back", px, r.getY() + 10.0f, textDim, juce::Justification::centred, true);
        g.setFont (body (10.0f)); g.setColour (textMute);
        for (auto& p : kDecades)
            g.drawText (p.second, juce::Rectangle<float> (freqToX (p.first, r.getX(), r.getWidth(), lo, hi) - 15.0f, r.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centred);
    }

    // ---- levels over time: compressor, gate, limiter ----------------------------------------------------
    void paintLevels (juce::Graphics& g, juce::Rectangle<float> r)
    {
        static const float words[] = { -26, -12, -20, -7, -16, -24, -9, -18, -5, -14, -22, -10 };
        const int numWords = 12;
        const bool gate = variant == 10 || variant == 11, limiter = variant >= 20;
        const float floorDb = -52.0f;
        auto top = r.removeFromTop (22.0f);
        const bool inset = variant >= 2 && variant <= 4;
        const auto insetArea = inset ? r.removeFromRight (150.0f) : juce::Rectangle<float>();
        if (inset) r.removeFromRight (14.0f);
        const auto plot = r;
        auto yFor = [&] (float db) { return plot.getY() + juce::jlimit (0.0f, 1.0f, (0.0f - db) / -floorDb) * plot.getHeight(); };

        float thr = -16.0f, ratio = 1.0f, makeup = 0.0f, drive = 0.0f, ceiling = -4.0f;
        juce::String info;
        if (variant == 0) info = "Grey bars: how loud each word is";
        else if (variant == 1) info = "Threshold " + juce::String (juce::roundToInt (thr)) + " dB";
        else if (variant == 2)
        {
            static const float ratios[] = { 2.0f, 4.0f, 10.0f };
            const float target = ratios[((int) (t / 3.0f)) % 3];
            shownRatio = approach (shownRatio, target, 1.0f / 60.0f, 0.25f);
            ratio = shownRatio; info = "Ratio " + juce::String (juce::roundToInt (target)) + ":1";
        }
        else if (variant == 3)
        {
            ratio = 4.0f;
            const float full = -Compressor::computeGainDb (-5.0f, thr, ratio, 4.0f);
            const float ph = std::fmod (t, 7.0f);
            makeup = full * juce::jlimit (0.0f, 1.0f, ph < 1.5f ? 0.0f : (ph - 1.5f) / 1.5f) * (ph > 6.0f ? 1.0f - (ph - 6.0f) : 1.0f);
            info = "Makeup gain " + dbString (makeup);
        }
        else if (variant == 4) { thr = userThr; ratio = userRatio; makeup = -Compressor::computeGainDb (-5.0f, thr, ratio, 4.0f); info = "Threshold " + juce::String (juce::roundToInt (thr)) + " dB    ratio " + juce::String (ratio, 1) + ":1"; }
        else if (gate) { thr = -38.0f; info = variant == 10 ? "Between the words: bleed, breaths and room noise" : "Gate threshold " + juce::String (juce::roundToInt (thr)) + " dB    range 24 dB"; }
        else { drive = 3.0f + 11.0f * osc (t, 9.0f); info = "Input pushed up " + juce::String (drive, 1) + " dB into the ceiling"; }
        tag (g, info, top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);

        g.setFont (body (10.0f));
        for (int db = -48; db < 0; db += 12)
        {
            g.setColour (juce::Colour (0xff181818)); g.fillRect (plot.getX(), yFor ((float) db), plot.getWidth(), 1.0f);
            g.setColour (textMute); g.drawText (juce::String (db), juce::Rectangle<float> (plot.getRight() - 30.0f, yFor ((float) db) - 13.0f, 28.0f, 12.0f), juce::Justification::centredRight);
        }

        const float slot = gate ? 30.0f : 44.0f, barW = gate ? 26.0f : 30.0f;
        const int slotsPerLoop = gate ? numWords * 2 : numWords;
        const float scroll = std::fmod (t * 34.0f, slot * (float) slotsPerLoop);
        float worstCut = 0.0f, loudIn = -100.0f, quietIn = 0.0f, loudOut = -100.0f, quietOut = 0.0f;
        g.saveState(); g.reduceClipRegion (plot.toNearestInt());
        const int visible = (int) (plot.getWidth() / slot) + 3;
        for (int k = -1; k < visible; ++k)
        {
            const float x = plot.getX() + (float) k * slot - std::fmod (scroll, slot);
            const int index = (((int) (scroll / slot) + k) % slotsPerLoop + slotsPerLoop) % slotsPerLoop;
            const bool noise = gate && (index % 2 == 1);
            float in = gate ? (noise ? -45.0f + 3.0f * std::sin ((float) index * 1.7f) : words[(index / 2) % numWords]) : words[index % numWords];
            in += drive;
            float out = in;
            if (gate) out = (variant == 11 && in < thr) ? in - 24.0f : in;
            else if (limiter) out = juce::jmin (in, ceiling);
            else out = in + Compressor::computeGainDb (in, thr, ratio, 4.0f) + makeup;
            worstCut = juce::jmin (worstCut, out - in - makeup);
            if (! noise) { if (in > loudIn) { loudIn = in; loudOut = out; } if (in < quietIn) { quietIn = in; quietOut = out; } }

            const auto inBar = juce::Rectangle<float> (x, yFor (in), barW, plot.getBottom() - yFor (in));
            if (variant == 0 || variant == 10)
            {
                g.setColour (noise ? over.withAlpha (0.45f) : text.withAlpha (0.55f)); g.fillRoundedRectangle (inBar, 3.0f);
                if (! noise && variant == 0 && (in > -8.0f || in < -23.0f)) tag (g, in > -8.0f ? "loud" : "quiet", x + barW * 0.5f, yFor (in) - 10.0f, in > -8.0f ? text : textDim, juce::Justification::centred);
            }
            else if (variant == 1)
            {
                g.setColour (text.withAlpha (0.45f)); g.fillRoundedRectangle (inBar, 3.0f);
                if (in > thr) { g.setColour (yellow); g.fillRoundedRectangle (inBar.withBottom (yFor (thr)), 3.0f); }
            }
            else
            {
                g.setColour (text.withAlpha (0.20f)); g.fillRoundedRectangle (inBar.withWidth (barW * 0.46f), 2.0f);
                const float oy = yFor (out);
                g.setColour (yellow.withAlpha (out < floorDb + 6.0f ? 0.35f : 1.0f));
                g.fillRoundedRectangle (juce::Rectangle<float> (x + barW * 0.54f, oy, barW * 0.46f, juce::jmax (1.0f, plot.getBottom() - oy)), 2.0f);
                if (limiter && in > ceiling) { g.setColour (over.withAlpha (0.55f)); g.drawRect (juce::Rectangle<float> (x + barW * 0.54f, yFor (in), barW * 0.46f, oy - yFor (in)), 1.0f); }
            }
        }
        g.restoreState();

        if (variant >= 1 && ! limiter && variant != 10)
        {
            g.setColour (gate ? text : yellow); dashedH (g, plot.getX(), plot.getRight() - 34.0f, yFor (thr));
            tagOn (g, gate ? "Gate threshold" : "Threshold", plot.getX() + 4.0f, yFor (thr) - 14.0f, gate ? text : yellow);
        }
        if (limiter)
        {
            g.setColour (text); g.fillRect (plot.getX(), yFor (ceiling) - 1.0f, plot.getWidth() - 34.0f, 2.0f);
            tagOn (g, "Ceiling", plot.getX() + 4.0f, yFor (ceiling) - 14.0f, text);
            tag (g, "Taking off up to " + juce::String (-worstCut, 1) + " dB" + (worstCut < -6.0f ? ": every hit is now the same height" : ""), top.getRight(), top.getCentreY(), worstCut < -6.0f ? over : textDim, juce::Justification::centredRight);
        }
        if (variant >= 2 && variant <= 4)
        {
            // transfer curve: in along the bottom, out up the side
            const auto box = insetArea.withHeight (insetArea.getWidth()).reduced (8.0f);
            g.setColour (lineHi); g.drawRoundedRectangle (box.expanded (6.0f), 6.0f, 1.0f);
            tag (g, "In", box.getRight(), box.getBottom() + 16.0f, textMute, juce::Justification::centredRight);
            tag (g, "Out", box.getX(), box.getBottom() + 16.0f, textMute);
            g.drawLine (box.getX(), box.getBottom(), box.getRight(), box.getY(), 1.0f);
            juce::Path p;
            for (int i = 0; i <= 50; ++i)
            {
                const float in = -50.0f + (float) i;
                const float out = in + Compressor::computeGainDb (in, thr, ratio, 4.0f);
                const float px = box.getX() + (in + 50.0f) / 50.0f * box.getWidth(), py = box.getBottom() - (out + 50.0f) / 50.0f * box.getHeight();
                if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            g.setColour (yellow); g.strokePath (p, juce::PathStrokeType (2.0f));
            tag (g, "Loudest to quietest word: " + juce::String (juce::roundToInt (loudIn - quietIn)) + " dB in, " + juce::String (juce::roundToInt (loudOut - quietOut)) + " dB out",
                 top.getRight(), top.getCentreY(), textDim, juce::Justification::centredRight);
        }
        if (variant == 11) tag (g, "Noise under the threshold is turned down; the words pass untouched", top.getRight(), top.getCentreY(), textDim, juce::Justification::centredRight);
    }

    // ---- attack and release ----------------------------------------------------------------------------
    void paintEnvelope (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto top = r.removeFromTop (22.0f);
        auto foot = r.removeFromBottom (20.0f);
        const int n = 600;
        const float thr = -20.0f, ratio = 6.0f, lo = -44.0f;
        const bool step = variant == 0;
        float attack = 40.0f, release = 110.0f;
        if (variant == 1) attack = 0.4f * std::pow (90.0f, osc (t, 8.0f));
        if (variant == 1) release = 90.0f;
        if (variant == 2) { attack = 3.0f; release = 25.0f * std::pow (18.0f, osc (t, 8.0f)); }
        if (variant == 3) { attack = userAttack; release = userRelease; }
        auto input = [step] (int ms)
        {
            if (step) return (ms >= 70 && ms < 330) ? -6.0f : -34.0f;
            const int tt = ms % 300;
            return tt < 2 ? -40.0f + 37.0f * (float) tt / 2.0f : juce::jmax (-40.0f, -3.0f - 30.0f * (1.0f - std::exp (-(float) (tt - 2) / 55.0f)));
        };
        const float aCoef = std::exp (-1.0f / juce::jmax (0.05f, attack)), rCoef = std::exp (-1.0f / juce::jmax (1.0f, release));
        auto yFor = [&] (float db) { return r.getY() + (0.0f - db) / -lo * r.getHeight(); };
        auto xFor = [&] (float ms) { return r.getX() + r.getWidth() * ms / (float) (n - 1); };
        juce::Path in, out, red;
        float gr = 0.0f;
        red.startNewSubPath (r.getX(), r.getY());
        for (int pass = 0; pass < (step ? 1 : 2); ++pass)
            for (int i = 0; i < n; ++i)
            {
                const float level = input (i);
                const float want = Compressor::computeGainDb (level, thr, ratio, step ? 0.0f : 4.0f);
                gr = want < gr ? aCoef * gr + (1.0f - aCoef) * want : rCoef * gr + (1.0f - rCoef) * want;
                if (! step && pass == 0) continue;
                const float x = xFor ((float) i);
                if (i == 0) { in.startNewSubPath (x, yFor (level)); out.startNewSubPath (x, yFor (level + gr)); }
                else { in.lineTo (x, yFor (level)); out.lineTo (x, yFor (level + gr)); }
                red.lineTo (x, r.getY() + (-gr) / -lo * r.getHeight());
            }
        red.lineTo (r.getRight(), r.getY()); red.closeSubPath();
        g.setColour (yellow.withAlpha (0.20f)); g.fillPath (red);
        g.setColour (yellow.withAlpha (0.6f)); g.strokePath (red, juce::PathStrokeType (1.0f));
        g.setColour (text.withAlpha (0.35f)); g.strokePath (in, juce::PathStrokeType (1.6f));
        g.setColour (yellow); g.strokePath (out, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved));
        g.setColour (text.withAlpha (0.5f)); dashedH (g, r.getX(), r.getRight(), yFor (thr));
        tag (g, "Threshold", r.getRight(), yFor (thr) - 10.0f, textDim, juce::Justification::centredRight);
        tag (g, "Gain reduction", r.getX() + 4.0f, r.getY() + 10.0f, yellow.withAlpha (0.8f));
        g.setColour (text.withAlpha (0.22f)); g.fillRect (r.getX() + r.getWidth() * std::fmod (t * 0.45f, 1.0f), r.getY(), 1.0f, r.getHeight());

        if (step)
        {
            // mark the two-thirds points that define the attack and release times
            g.setColour (text);
            dashedV (g, xFor (70.0f), r.getY(), r.getBottom()); dashedV (g, xFor (70.0f + attack), r.getY(), r.getBottom());
            dashedV (g, xFor (330.0f), r.getY(), r.getBottom()); dashedV (g, xFor (330.0f + release), r.getY(), r.getBottom());
            tag (g, "Attack " + juce::String (juce::roundToInt (attack)) + " ms", xFor (70.0f + attack * 0.5f), foot.getCentreY() + 2.0f, text, juce::Justification::centred, true);
            tag (g, "Release " + juce::String (juce::roundToInt (release)) + " ms", xFor (330.0f + release * 0.5f), foot.getCentreY() + 2.0f, text, juce::Justification::centred, true);
            tag (g, "Two thirds of the way down", xFor (70.0f + attack) + 6.0f, r.getY() + r.getHeight() * 0.22f, textDim);
            tag (g, "Two thirds of the way back", xFor (330.0f + release) + 6.0f, r.getY() + r.getHeight() * 0.10f, textDim);
            tag (g, "A sound jumps over the threshold, holds, then drops", top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
        }
        else
        {
            const juce::String a = attack < 10.0f ? juce::String (attack, 1) : juce::String (juce::roundToInt (attack));
            tag (g, "Attack " + a + " ms    Release " + juce::String (juce::roundToInt (release)) + " ms", top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
            const juce::String msg = variant == 2 ? (release < 60.0f ? "Fast release: the level springs back between the hits" : release > 250.0f ? "Slow release: still held down when the next hit lands" : "Medium release")
                                                  : (attack < 3.0f ? "Fast attack: the crack of the hit is flattened" : attack < 15.0f ? "Some of the hit gets through" : "Slow attack: the hit passes, only the body is turned down");
            tag (g, msg, foot.getX(), foot.getCentreY() + 2.0f, textDim);
        }
        tag (g, "Grey: in    Yellow: out", r.getRight(), r.getBottom() - 10.0f, textMute, juce::Justification::centredRight);
    }

    // ---- saturation ------------------------------------------------------------------------------------
    static float bend (float x, float drive, bool asymmetric)
    {
        if (! asymmetric) return std::tanh (x * drive);
        return x >= 0.0f ? std::tanh (x * drive) : std::tanh (x * drive * 0.45f) / 0.45f * 0.75f;      // one half bends sooner than the other
    }
    void harmonics (float drive, bool asym, std::array<float, 9>& out) const
    {
        const int N = 256;
        float fundamental = 1.0e-6f;
        for (int k = 1; k <= 8; ++k)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const double ph = juce::MathConstants<double>::twoPi * (double) i / (double) N;
                const double v = bend ((float) std::sin (ph), drive, asym);
                re += v * std::cos ((double) k * ph); im += v * std::sin ((double) k * ph);
            }
            const float mag = (float) (2.0 * std::sqrt (re * re + im * im) / (double) N);
            if (k == 1) fundamental = juce::jmax (1.0e-6f, mag);
            out[(size_t) k] = 20.0f * std::log10 (juce::jmax (1.0e-6f, mag / fundamental));
        }
    }
    void paintSaturation (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const bool asym = variant >= 1;
        const float driveDb = 20.0f * osc (t, 9.0f);
        const float drive = juce::jmax (0.05f, gainOf (driveDb) * 0.35f);
        std::array<float, 9> h {};
        harmonics (drive, asym, h);
        auto top = r.removeFromTop (22.0f);
        tag (g, "Drive " + juce::String (driveDb, 1) + " dB    " + (variant == 2 ? "an 808 playing 50 Hz" : asym ? "bent unevenly (valve style)" : "bent evenly (tape style)"), top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
        const float norm = juce::jmax (0.001f, std::abs (bend (1.0f, drive, asym)), std::abs (bend (-1.0f, drive, asym)));

        if (variant == 2)
        {
            // spectrum of the note and its harmonics against what a small speaker can play
            const float lo = 32.0f, hi = 520.0f, base = r.getBottom() - 18.0f;
            const float zoneX = freqToX (200.0f, r.getX(), r.getWidth(), lo, hi);
            g.setColour (over.withAlpha (0.08f)); g.fillRect (r.getX(), r.getY() + 4.0f, zoneX - r.getX(), base - r.getY() - 4.0f);
            tag (g, "A phone speaker plays almost nothing down here", r.getX() + 8.0f, r.getY() + 16.0f, over.withAlpha (0.9f));
            tag (g, "Heard on any speaker", zoneX + 10.0f, r.getY() + 16.0f, yellow);
            g.setColour (lineHi); g.fillRect (r.getX(), base, r.getWidth(), 1.0f); dashedV (g, zoneX, r.getY() + 4.0f, base);
            g.setFont (body (10.0f));
            for (int k = 1; k <= 8; ++k)
            {
                const float hz = 50.0f * (float) k;
                const float x = freqToX (hz, r.getX(), r.getWidth(), lo, hi);
                const float height = juce::jlimit (0.0f, 1.0f, (h[(size_t) k] + 54.0f) / 54.0f) * (base - r.getY() - 40.0f);
                g.setColour (hz < 200.0f ? text.withAlpha (0.5f) : yellow);
                g.fillRoundedRectangle (x - 9.0f, base - height, 18.0f, height, 3.0f);
                g.setColour (textMute);
                g.drawText (juce::String (juce::roundToInt (hz)), juce::Rectangle<float> (x - 20.0f, base + 3.0f, 40.0f, 12.0f), juce::Justification::centred);
            }
            return;
        }

        const float side = juce::jmin (r.getHeight(), r.getWidth() * 0.3f);
        auto curveBox = r.removeFromLeft (side).withSizeKeepingCentre (side, side).reduced (4.0f);
        r.removeFromLeft (20.0f);
        auto bars = r.removeFromRight (r.getWidth() * 0.42f);
        r.removeFromRight (20.0f);

        // the bend, with the wave riding along it
        g.setColour (juce::Colour (0xff1c1c1c));
        g.fillRect (curveBox.getCentreX(), curveBox.getY(), 1.0f, curveBox.getHeight()); g.fillRect (curveBox.getX(), curveBox.getCentreY(), curveBox.getWidth(), 1.0f);
        g.setColour (lineHi); g.drawLine (curveBox.getX(), curveBox.getBottom(), curveBox.getRight(), curveBox.getY(), 1.0f);
        juce::Path c;
        for (int i = 0; i <= 80; ++i)
        {
            const float x = -1.0f + (float) i / 40.0f;
            const float px = curveBox.getCentreX() + x * curveBox.getWidth() * 0.5f, py = curveBox.getCentreY() - bend (x, drive, asym) / norm * curveBox.getHeight() * 0.5f;
            if (i == 0) c.startNewSubPath (px, py); else c.lineTo (px, py);
        }
        g.setColour (yellow); g.strokePath (c, juce::PathStrokeType (2.2f));
        const float now = std::sin (t * 3.0f);
        g.setColour (text); g.fillEllipse (curveBox.getCentreX() + now * curveBox.getWidth() * 0.5f - 5.0f, curveBox.getCentreY() - bend (now, drive, asym) / norm * curveBox.getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
        tag (g, "In", curveBox.getRight(), curveBox.getBottom() + 8.0f, textMute, juce::Justification::centredRight);

        // the wave before and after
        juce::Path wi, wo;
        for (int i = 0; i <= (int) r.getWidth(); ++i)
        {
            const float ph = kTwoPi * (1.6f * (float) i / r.getWidth() - t * 0.35f);
            const float x = r.getX() + (float) i, s = std::sin (ph);
            const float yi = r.getCentreY() - 8.0f - s * r.getHeight() * 0.36f, yo = r.getCentreY() - 8.0f - bend (s, drive, asym) / norm * r.getHeight() * 0.36f;
            if (i == 0) { wi.startNewSubPath (x, yi); wo.startNewSubPath (x, yo); } else { wi.lineTo (x, yi); wo.lineTo (x, yo); }
        }
        g.setColour (text.withAlpha (0.3f)); g.strokePath (wi, juce::PathStrokeType (1.4f));
        g.setColour (yellow); g.strokePath (wo, juce::PathStrokeType (2.2f));
        tag (g, "Grey: clean    Yellow: saturated", r.getX(), r.getBottom() - 4.0f, textMute);

        // harmonics
        const float base = bars.getBottom() - 16.0f, bw = bars.getWidth() / 8.0f;
        g.setColour (lineHi); g.fillRect (bars.getX(), base, bars.getWidth(), 1.0f);
        g.setFont (body (10.5f));
        for (int k = 1; k <= 8; ++k)
        {
            const float height = juce::jlimit (0.0f, 1.0f, (h[(size_t) k] + 60.0f) / 60.0f) * (bars.getHeight() - 40.0f);
            const auto cell = juce::Rectangle<float> (bars.getX() + (float) (k - 1) * bw, bars.getY(), bw, bars.getHeight());
            g.setColour (k == 1 ? text.withAlpha (0.5f) : (k % 2 == 1 ? yellow : text));
            g.fillRoundedRectangle (cell.getCentreX() - bw * 0.3f, base - height, bw * 0.6f, juce::jmax (1.0f, height), 3.0f);
            g.setColour (textMute);
            g.drawText (k == 1 ? juce::String ("note") : juce::String (k), juce::Rectangle<float> (cell.getX(), base + 3.0f, bw, 12.0f), juce::Justification::centred);
        }
        tag (g, "Harmonics    yellow: odd    white: even", bars.getX(), bars.getY() + 6.0f, textMute);
    }

    // ---- stereo ----------------------------------------------------------------------------------------
    void paintStereo (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto top = r.removeFromTop (22.0f);
        if (variant == 0)
        {
            const float width = 2.0f * osc (t, 9.0f);
            tag (g, "Width " + juce::String (juce::roundToInt (width * 100.0f)) + " %", top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
            const float cx = r.getCentreX(), span = r.getWidth() * 0.5f - 60.0f;
            for (int sidePos = -1; sidePos <= 1; sidePos += 2)
            {
                const auto sp = juce::Rectangle<float> (cx + (float) sidePos * (span + 30.0f) - 20.0f, r.getBottom() - 78.0f, 40.0f, 64.0f);
                g.setColour (control); g.fillRoundedRectangle (sp, 6.0f);
                g.setColour (lineHi); g.drawEllipse (sp.getCentreX() - 11.0f, sp.getY() + 28.0f, 22.0f, 22.0f, 2.0f); g.drawEllipse (sp.getCentreX() - 5.0f, sp.getY() + 9.0f, 10.0f, 10.0f, 2.0f);
                tag (g, sidePos < 0 ? "Left" : "Right", sp.getCentreX(), sp.getBottom() + 8.0f, textMute, juce::Justification::centred);
            }
            g.setColour (juce::Colour (0xff1c1c1c)); dashedV (g, cx, r.getY() + 6.0f, r.getBottom() - 6.0f);
            auto source = [&] (const char* name, float pos, float y, bool wide)
            {
                const float spread = wide ? juce::jlimit (0.0f, 1.0f, pos * width * 0.5f) : 0.0f;
                for (int s = -1; s <= 1; s += 2)
                {
                    if (! wide && s > 0) break;
                    const float x = cx + (float) s * spread * span;
                    g.setColour (wide ? yellow : text);
                    g.fillEllipse (x - 9.0f, y - 9.0f, 18.0f, 18.0f);
                }
                tag (g, name, cx, y - 20.0f, wide ? yellow : text, juce::Justification::centred, true);
            };
            source ("Lead vocal", 0.0f, r.getY() + r.getHeight() * 0.24f, false);
            source ("Backing vocals", 0.75f, r.getY() + r.getHeight() * 0.46f, true);
            source ("Pad", 1.0f, r.getY() + r.getHeight() * 0.68f, true);
            source ("Kick and bass", 0.0f, r.getY() + r.getHeight() * 0.90f, false);
        }
        else
        {
            const float phi = juce::MathConstants<float>::pi * osc (t, 9.0f);
            const float corr = std::cos (phi);
            tag (g, corr > 0.5f ? "Left and right agree" : corr > -0.2f ? "Left and right drifting apart" : "Left and right are opposite: gone in mono", top.getX(), top.getCentreY(), corr < -0.2f ? over : text, juce::Justification::centredLeft, true);
            auto meter = r.removeFromBottom (34.0f);
            const float rowH = r.getHeight() / 3.0f;
            auto row = [&] (int index, const char* name, std::function<float (float)> f, juce::Colour colour, float thick)
            {
                const auto area = juce::Rectangle<float> (r.getX() + 96.0f, r.getY() + rowH * (float) index, r.getWidth() - 96.0f, rowH);
                tag (g, name, r.getX(), area.getCentreY(), colour, juce::Justification::centredLeft, true);
                juce::Path p;
                for (int i = 0; i <= (int) area.getWidth(); ++i)
                {
                    const float y = area.getCentreY() - f (kTwoPi * (3.0f * (float) i / area.getWidth() - t * 0.3f)) * rowH * 0.36f;
                    if (i == 0) p.startNewSubPath (area.getX(), y); else p.lineTo (area.getX() + (float) i, y);
                }
                g.setColour (colour); g.strokePath (p, juce::PathStrokeType (thick));
            };
            row (0, "Left", [] (float x) { return std::sin (x); }, text.withAlpha (0.7f), 1.6f);
            row (1, "Right", [phi] (float x) { return std::sin (x + phi); }, text.withAlpha (0.7f), 1.6f);
            row (2, "Mono (L + R)", [phi] (float x) { return 0.5f * (std::sin (x) + std::sin (x + phi)); }, corr < -0.2f ? over : yellow, 2.4f);
            const auto track = juce::Rectangle<float> (meter.getX() + 96.0f, meter.getCentreY() - 3.0f, meter.getWidth() - 96.0f, 6.0f);
            tag (g, "Correlation", meter.getX(), meter.getCentreY(), textDim);
            g.setColour (control); g.fillRoundedRectangle (track, 3.0f);
            const float x = track.getCentreX() + corr * track.getWidth() * 0.5f;
            g.setColour (corr < 0.0f ? over : yellow);
            g.fillRoundedRectangle (juce::jmin (x, track.getCentreX()), track.getY(), juce::jmax (2.0f, std::abs (x - track.getCentreX())), 6.0f, 3.0f);
            g.setFont (body (10.0f)); g.setColour (textMute);
            g.drawText ("-1", juce::Rectangle<float> (track.getX(), track.getBottom() + 2.0f, 20.0f, 11.0f), juce::Justification::centredLeft);
            g.drawText ("+1", juce::Rectangle<float> (track.getRight() - 20.0f, track.getBottom() + 2.0f, 20.0f, 11.0f), juce::Justification::centredRight);
        }
    }

    // ---- loudness --------------------------------------------------------------------------------------
    static float hash (int i) { const float s = std::sin ((float) i * 12.9898f) * 43758.5453f; return s - std::floor (s); }
    void paintLoudness (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto top = r.removeFromTop (22.0f);
        if (variant == 2)
        {
            struct Bar { const char* name; float lufs; const char* note; };
            static const Bar bars[] = { { "Mix before mastering", -18.0f, "Headroom for the master" }, { "Apple Music", -16.0f, "Plays songs at about this level" },
                                        { "Spotify and YouTube", -14.0f, "Play songs at about this level" }, { "Loud hip-hop master", -9.0f, "Gets turned down on streaming" } };
            tag (g, "Integrated loudness, start to finish", top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
            const float rowH = r.getHeight() / 4.0f;
            for (int i = 0; i < 4; ++i)
            {
                auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) i, r.getWidth(), rowH).reduced (0.0f, 6.0f);
                auto head = row.removeFromTop (18.0f);
                g.setFont (bodyBold (12.5f)); g.setColour (text); g.drawText (bars[i].name, head, juce::Justification::centredLeft);
                g.setFont (headline (17.0f)); g.setColour (i == 2 ? yellow : text); g.drawText (juce::String (juce::roundToInt (bars[i].lufs)) + " LUFS", head, juce::Justification::centredRight);
                g.setFont (body (11.0f)); g.setColour (textMute); g.drawText (bars[i].note, row.removeFromBottom (14.0f), juce::Justification::centredLeft);
                const auto track = row.reduced (0.0f, 2.0f);
                g.setColour (control); g.fillRoundedRectangle (track, 3.0f);
                const float grow = juce::jlimit (0.0f, 1.0f, (t - 0.12f * (float) i) / 0.7f);
                g.setColour (i == 2 ? yellow : text.withAlpha (0.8f));
                g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * juce::jlimit (0.0f, 1.0f, (bars[i].lufs + 24.0f) / 18.0f) * (1.0f - (1.0f - grow) * (1.0f - grow)))), 3.0f);
            }
            return;
        }

        const float rowH = r.getHeight() * 0.5f;
        auto drawRow = [&] (int index, const juce::String& name, std::function<float (int)> env, float scale, juce::Colour colour, const juce::String& right)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) index, r.getWidth(), rowH).reduced (0.0f, 8.0f);
            tag (g, name, row.getX(), row.getY() + 6.0f, text, juce::Justification::centredLeft, true);
            tag (g, right, row.getRight(), row.getY() + 6.0f, colour, juce::Justification::centredRight, true);
            row.removeFromTop (18.0f);
            const float mid = row.getCentreY(), half = row.getHeight() * 0.5f;
            g.setColour (lineHi); dashedH (g, row.getX(), row.getRight(), mid - half); dashedH (g, row.getX(), row.getRight(), mid + half);
            g.setColour (colour);
            for (int i = 0; i < (int) (row.getWidth() / 3.0f); ++i)
            {
                const float a = juce::jlimit (0.0f, 1.0f, env (i) * scale) * half;
                g.fillRect (row.getX() + (float) i * 3.0f, mid - a, 2.0f, juce::jmax (1.0f, a * 2.0f));
            }
        };
        if (variant == 0)
        {
            tag (g, "Same peak, very different loudness", top.getX(), top.getCentreY(), text, juce::Justification::centredLeft, true);
            const int head = (int) (std::fmod (t, 4.0f) / 4.0f * 420.0f);
            drawRow (0, "A short hit", [head] (int i) { const int d = i - 40; return d < 0 || i > head ? 0.0f : std::exp (-(float) d / 9.0f) * (0.75f + 0.25f * hash (i)); }, 1.0f, text.withAlpha (0.8f), "Peak -1 dB    about -30 LUFS");
            drawRow (1, "A held note", [head] (int i) { return i < 40 || i > head ? 0.0f : 0.82f + 0.18f * hash (i * 3); }, 1.0f, yellow, "Peak -1 dB    about -6 LUFS");
        }
        else
        {
            const float ph = std::fmod (t, 9.0f);
            const float k = juce::jlimit (0.0f, 1.0f, (ph - 2.5f) / 1.2f) * (ph > 8.0f ? 1.0f - (ph - 8.0f) : 1.0f);      // 0 as delivered, 1 as streamed
            tag (g, k < 0.5f ? "As delivered" : "As streamed: both played at the same loudness", top.getX(), top.getCentreY(), k < 0.5f ? text : yellow, juce::Justification::centredLeft, true);
            auto dynamic = [] (int i) { const int b = i % 42; const float hit = b < 3 ? 1.0f : std::exp (-(float) (b - 3) / 6.0f); return juce::jmax (hit, 0.22f + 0.1f * hash (i)); };
            auto squashed = [] (int i) { return 0.86f + 0.14f * hash (i * 7); };
            drawRow (0, "Dynamic master, -14 LUFS", dynamic, 1.0f, yellow, k < 0.5f ? "" : "Left alone: the hits still stand out");
            drawRow (1, "Loud master, -8 LUFS", squashed, 1.0f - 0.5f * k, text.withAlpha (0.8f), k < 0.5f ? "" : "Turned down 6 dB: flat, with nothing left to hit with");
        }
    }

    // ---- true peak -------------------------------------------------------------------------------------
    void paintTruePeak (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto top = r.removeFromTop (22.0f);
        const float phi = juce::MathConstants<float>::halfPi * 0.5f * osc (t, 8.0f);      // where the samples land on the wave
        const float cycles = 5.0f, amp = r.getHeight() * 0.40f, mid = r.getCentreY();
        auto wave = [&] (float x01) { return std::sin (kTwoPi * cycles * x01 + phi); };
        float samplePeak = 0.0f;
        const int samples = (int) (cycles * 4.0f);
        for (int i = 0; i <= samples; ++i) samplePeak = juce::jmax (samplePeak, std::abs (wave ((float) i / (float) samples)));
        const float sampleDb = 20.0f * std::log10 (samplePeak);
        tag (g, "Highest sample " + juce::String (sampleDb, 1) + " dB    true peak 0.0 dB", top.getX(), top.getCentreY(), sampleDb < -0.5f ? over : text, juce::Justification::centredLeft, true);

        const float ceilY = mid - samplePeak * amp;
        juce::Path p, overP;
        bool inOver = false;
        for (int i = 0; i <= (int) r.getWidth(); ++i)
        {
            const float v = wave ((float) i / r.getWidth()), x = r.getX() + (float) i, y = mid - v * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            const bool o = v > samplePeak + 0.004f;
            if (o) { if (! inOver) { overP.startNewSubPath (x, ceilY); inOver = true; } overP.lineTo (x, y); }
            else if (inOver) { overP.lineTo (x, ceilY); overP.closeSubPath(); inOver = false; }
        }
        g.setColour (juce::Colour (0xff1c1c1c)); g.fillRect (r.getX(), mid, r.getWidth(), 1.0f);
        g.setColour (over.withAlpha (0.55f)); g.fillPath (overP);
        g.setColour (text.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (1.6f));
        g.setColour (text); dashedH (g, r.getX(), r.getRight(), ceilY);
        tag (g, "What a sample meter reads", r.getRight(), ceilY + 10.0f, textDim, juce::Justification::centredRight);
        for (int i = 0; i <= samples; ++i)
        {
            const float x01 = (float) i / (float) samples, x = r.getX() + x01 * r.getWidth(), y = mid - wave (x01) * amp;
            g.setColour (yellow.withAlpha (0.35f)); g.fillRect (x - 0.5f, juce::jmin (y, mid), 1.0f, std::abs (y - mid));
            g.setColour (yellow); g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        }
        tag (g, "Dots: the samples      Grey: the wave your speakers play      Red: what the samples miss", r.getX(), r.getBottom() - 6.0f, textMute);
    }

    // ---- signal flow -----------------------------------------------------------------------------------
    struct Block { const char* name; const char* what; };
    const Block* chainBlocks() const
    {
        static const Block mix[] = { { "Balance", "Set every track's level against the lead vocal. The Mix tab does this with Learn and balance." },
                                     { "Space", "Find where tracks collide and decide who wins. Fix the collisions the Mix tab lists." },
                                     { "Tone and control", "EQ and compression on single tracks, to fix what the balance could not." },
                                     { "Master", "Loudness and a limiter, once the mix already sounds right." } };
        static const Block vocal[] = { { "Gate", "Removes bleed and noise between phrases." }, { "Low cut", "Clears rumble below 80 to 100 Hz." },
                                       { "Cut mud", "Dips the build-up around 200 to 400 Hz before the compressor reacts to it." },
                                       { "Compress", "Turns the loud words down 3 to 6 dB so every word is heard." },
                                       { "De-ess", "Tames the S sounds, which the compressor has just made louder." },
                                       { "Brighten", "Presence around 3 kHz and air above 10 kHz, added last so it is not fighting the problems." },
                                       { "Saturate", "A little colour and thickness." }, { "Ride", "Keeps every line sitting in the same place." } };
        return variant == 0 ? mix : vocal;
    }
    int chainCount() const { return variant == 0 ? 4 : 8; }
    void paintChain (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int n = chainCount();
        const auto* blocks = chainBlocks();
        const float travel = std::fmod (t / 2.4f, (float) n);
        const int active = chainPick >= 0 ? chainPick : (int) travel;
        auto foot = r.removeFromBottom (r.getHeight() * 0.36f);
        const float cw = r.getWidth() / (float) n, lineY = r.getCentreY();
        g.setColour (lineHi); g.fillRect (r.getX() + cw * 0.5f, lineY - 1.0f, r.getWidth() - cw, 2.0f);
        g.setColour (yellow); g.fillRect (r.getX() + cw * 0.5f, lineY - 1.0f, juce::jmax (0.0f, ((float) active) * cw), 2.0f);
        for (int i = 0; i < n; ++i)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + cw * (float) i, r.getY(), cw, r.getHeight());
            const bool on = i == active, done = i < active;
            const float size = on ? 58.0f : 46.0f;
            const auto box = juce::Rectangle<float> (size, size).withCentre ({ cell.getCentreX(), lineY });
            g.setColour (on ? yellow : (done ? raised.brighter (0.08f) : raised));
            g.fillRoundedRectangle (box, 8.0f);
            if (! on) { g.setColour (done ? yellow.withAlpha (0.6f) : lineHi); g.drawRoundedRectangle (box, 8.0f, 1.5f); }
            g.setFont (headline (on ? 24.0f : 19.0f)); g.setColour (on ? onYellow : (done ? text : textDim));
            g.drawText (juce::String (i + 1), box, juce::Justification::centred);
            g.setFont (on ? bodyBold (12.0f) : body (11.5f)); g.setColour (on ? text : textDim);
            g.drawFittedText (blocks[i].name, cell.withY (box.getBottom() + 8.0f).withHeight (30.0f).reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centredTop, 2, 0.85f);
        }
        if (chainPick < 0)
        {
            const float px = r.getX() + cw * 0.5f + juce::jmin (travel, (float) (n - 1)) * cw;
            g.setColour (text); g.fillEllipse (px - 4.0f, lineY - 36.0f - 4.0f, 8.0f, 8.0f);
        }
        g.setFont (headline (22.0f)); g.setColour (yellow);
        g.drawText (juce::String (blocks[active].name).toUpperCase(), foot.removeFromTop (30.0f), juce::Justification::centredLeft);
        drawWrapped (g, blocks[active].what, foot.withWidth (juce::jmin (foot.getWidth(), 620.0f)), body (13.5f), text.withAlpha (0.85f), 1.4f);
    }

    Scene scene = Scene::Wave, nextScene = Scene::Wave;
    int variant = 0, nextVariant = 0, region = 2, chainPick = -1;
    bool started = false, fadingOut = false;
    float t = 0.0f, fade = 1.0f, idle = 10.0f;
    float eqFreq = 2500.0f, eqGain = 4.0f, eqQ = 1.4f, dynCut = 0.0f, dip = 0.0f, shownRatio = 2.0f;
    float userThr = -16.0f, userRatio = 4.0f, userAttack = 1.0f, userRelease = 120.0f;
};

// =====================================================================================================
LearnPage::LearnPage (MUProcessor& p, std::function<void (const juce::String&)> status, std::function<void (int)> go)
    : proc (p), setStatus (std::move (status)), goToTab (std::move (go))
{
    stage = std::make_unique<LessonStage>();
    addAndMakeVisible (*stage);
    styleButton (prevButton, "Back", false);
    styleButton (nextButton, "Next", true);
    styleButton (tryButton, "Try it", false);
    prevButton.onClick = [this] { if (step > 0) showStep (step - 1); };
    nextButton.onClick = [this]
    {
        const auto& all = lessons();
        if (step + 1 < (int) all[(size_t) lesson].steps.size()) showStep (step + 1);
        else showLesson ((lesson + 1) % (int) all.size(), 0);
    };
    tryButton.onClick = [this]
    {
        const auto& l = lessons()[(size_t) lesson];
        if (l.preset != nullptr)
        {
            const int i = findChainPreset (l.preset);
            if (i >= 0) setStatus (proc.applyChainPreset (i) + " Switch each module off and on to hear what it adds.");
        }
        if (goToTab) goToTab (l.tab);
    };
    for (auto* b : { &prevButton, &nextButton, &tryButton }) addAndMakeVisible (b);
    showLesson (juce::jlimit (0, (int) lessons().size() - 1, settings->getInt ("lessonLast", 0)), 0);
}

LearnPage::~LearnPage() = default;

bool LearnPage::isDone (int l) const { return (settings->getInt ("lessonsDone", 0) & (1 << l)) != 0; }

void LearnPage::showLesson (int l, int s)
{
    lesson = juce::jlimit (0, (int) lessons().size() - 1, l);
    settings->set ("lessonLast", lesson);
    tryButton.setButtonText (lessons()[(size_t) lesson].tryLabel);
    showStep (s);
    repaint();
}

void LearnPage::showStep (int s)
{
    const auto& l = lessons()[(size_t) lesson];
    step = juce::jlimit (0, (int) l.steps.size() - 1, s);
    const auto& st = l.steps[(size_t) step];
    stage->show (st.scene, st.variant);
    const bool last = step + 1 == (int) l.steps.size();
    if (last) settings->set ("lessonsDone", settings->getInt ("lessonsDone", 0) | (1 << lesson));
    prevButton.setEnabled (step > 0);
    nextButton.setButtonText (last ? "Next lesson" : "Next");
    textFade = 0.0f;
    repaint();
}

juce::Rectangle<int> LearnPage::lessonRow (int index) const
{
    const auto& all = lessons();
    int y = listArea.getY() + 50;
    const char* chapter = nullptr;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        if (chapter == nullptr || juce::String (chapter) != all[(size_t) i].chapter) { chapter = all[(size_t) i].chapter; y += 24; }
        if (i == index) return { listArea.getX() + 8, y, listArea.getWidth() - 16, 32 };
        y += 34;
    }
    return {};
}

int LearnPage::lessonAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) lessons().size(); ++i) if (lessonRow (i).contains (p)) return i;
    return -1;
}

int LearnPage::stepDotAt (juce::Point<int> p) const
{
    if (! dotsArea.expanded (0, 8).contains (p)) return -1;
    const int n = (int) lessons()[(size_t) lesson].steps.size();
    return juce::jlimit (0, n - 1, (p.x - dotsArea.getX()) * n / juce::jmax (1, dotsArea.getWidth()));
}

void LearnPage::mouseDown (const juce::MouseEvent& e)
{
    const int l = lessonAt (e.getPosition());
    if (l >= 0 && l != lesson) { showLesson (l, 0); return; }
    const int d = stepDotAt (e.getPosition());
    if (d >= 0 && d != step) showStep (d);
}

void LearnPage::mouseMove (const juce::MouseEvent& e)
{
    const int h = lessonAt (e.getPosition());
    if (h != hover) { hover = h; repaint (listArea); }
    setMouseCursor (h >= 0 || stepDotAt (e.getPosition()) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void LearnPage::mouseExit (const juce::MouseEvent&) { hover = -1; repaint (listArea); }

void LearnPage::tick (float dt)
{
    const float target = (float) lessonRow (lesson).getY();
    if (std::abs (blockY - target) > 0.3f) { blockY = approach (blockY, target, dt, 0.05f); repaint (listArea); }
    if (textFade < 1.0f) { textFade = juce::jmin (1.0f, textFade + dt * 4.5f); repaint (textArea.expanded (0, 12)); }
    if (std::abs (dotAnim - (float) step) > 0.004f) { dotAnim = approach (dotAnim, (float) step, dt, 0.07f); repaint (dotsArea.expanded (0, 6)); }
    stage->tick (dt);
}

void LearnPage::paint (juce::Graphics& g)
{
    g.fillAll (black);
    const auto& all = lessons();

    // ---- lesson list
    fillPanel (g, listArea.toFloat());
    auto head = listArea.reduced (16, 0).removeFromTop (50);
    g.setFont (headline (20.0f)); g.setColour (text);
    g.drawText ("LESSONS", head, juce::Justification::centredLeft);
    int doneCount = 0;
    for (int i = 0; i < (int) all.size(); ++i) if (isDone (i)) ++doneCount;
    g.setFont (body (11.5f)); g.setColour (textMute);
    g.drawText (juce::String (doneCount) + " of " + juce::String ((int) all.size()) + " done", head, juce::Justification::centredRight);

    g.setColour (yellow);
    g.fillRect (juce::Rectangle<float> ((float) listArea.getX() + 8.0f, blockY, (float) listArea.getWidth() - 16.0f, 32.0f));
    const char* chapter = nullptr;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto row = lessonRow (i);
        if (chapter == nullptr || juce::String (chapter) != all[(size_t) i].chapter)
        {
            chapter = all[(size_t) i].chapter;
            g.setFont (bodyBold (10.5f)); g.setColour (textMute);
            g.drawText (chapter, row.translated (8, -22).withHeight (18), juce::Justification::centredLeft);
        }
        const bool on = std::abs (blockY - (float) row.getY()) < 16.0f;
        g.setFont (on ? bodyBold (12.5f) : body (12.5f));
        g.setColour (on ? onYellow : (i == hover ? text : textDim));
        g.drawText (all[(size_t) i].title, row.withTrimmedLeft (12).withTrimmedRight (26), juce::Justification::centredLeft);
        if (isDone (i))
        {
            const auto dot = juce::Rectangle<float> ((float) row.getRight() - 22.0f, (float) row.getCentreY() - 6.0f, 12.0f, 12.0f);
            juce::Path tick; tick.startNewSubPath (dot.getX() + 2.0f, dot.getCentreY()); tick.lineTo (dot.getX() + 5.0f, dot.getBottom() - 2.5f); tick.lineTo (dot.getRight() - 1.5f, dot.getY() + 2.5f);
            g.setColour (on ? onYellow : yellow);
            g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // ---- step text
    fillPanel (g, contentArea.toFloat());
    const auto& l = all[(size_t) lesson];
    const auto& st = l.steps[(size_t) step];
    auto tArea = textArea.toFloat().translated (0.0f, (1.0f - textFade) * 8.0f);
    g.setFont (bodyBold (11.5f)); g.setColour (yellow.withAlpha (textFade));
    g.drawText (juce::String (l.title) + "    step " + juce::String (step + 1) + " of " + juce::String ((int) l.steps.size()), tArea.removeFromTop (16.0f), juce::Justification::centredLeft);
    tArea.removeFromTop (2.0f);
    g.setFont (headline (28.0f)); g.setColour (text.withAlpha (textFade));
    g.drawText (juce::String (st.title).toUpperCase(), tArea.removeFromTop (36.0f), juce::Justification::centredLeft);
    tArea.removeFromTop (6.0f);
    drawWrapped (g, st.text, tArea, body (14.0f), text.withAlpha (0.88f * textFade), 1.45f);

    // ---- step progress
    const int n = (int) l.steps.size();
    const float gap = 6.0f, w = ((float) dotsArea.getWidth() - gap * (float) (n - 1)) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> ((float) dotsArea.getX() + (float) i * (w + gap), (float) dotsArea.getCentreY() - 2.0f, w, 4.0f);
        g.setColour (control); g.fillRoundedRectangle (seg, 2.0f);
        const float amount = juce::jlimit (0.0f, 1.0f, dotAnim - (float) i + 1.0f);
        if (amount > 0.0f) { g.setColour (yellow); g.fillRoundedRectangle (seg.withWidth (w * amount), 2.0f); }
    }
}

void LearnPage::resized()
{
    auto r = getLocalBounds().reduced (12);
    listArea = r.removeFromLeft (252);
    r.removeFromLeft (10);
    contentArea = r;
    if (blockY <= 0.0f) blockY = (float) lessonRow (lesson).getY();

    auto c = contentArea.reduced (18, 16);
    stage->setBounds (c.removeFromTop (juce::jmin (350, c.getHeight() * 60 / 100)));
    c.removeFromTop (16);
    auto bottom = c.removeFromBottom (32);
    nextButton.setBounds (bottom.removeFromRight (118));
    bottom.removeFromRight (8);
    prevButton.setBounds (bottom.removeFromRight (78));
    bottom.removeFromRight (8);
    tryButton.setBounds (bottom.removeFromRight (160));
    bottom.removeFromRight (24);
    dotsArea = bottom.withSizeKeepingCentre (bottom.getWidth(), 12);
    c.removeFromBottom (10);
    textArea = c.withWidth (juce::jmin (c.getWidth(), 590));
}
}
