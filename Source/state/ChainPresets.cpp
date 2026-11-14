#include "ChainPresets.h"
#include "ParameterLayout.h"

namespace mu
{
namespace
{
    enum Style { Clean = 0, Punch, Smooth, Glue };
    enum Sat { Tape = 0, Tube, Warm };
    enum Focus { Full = 0, LowMids, Highs };
    enum Speed { Slow = 0, Normal, Fast };

    // Small builder so each preset reads like a channel-strip recipe.
    struct Chain
    {
        ChainPreset p;

        Chain (const juce::String& category, const juce::String& name, Role role, const juce::String& blurb)
        {
            p.category = category; p.name = name; p.role = role; p.blurb = blurb;
            set (ids::role, (float) role);
            set (ids::eqEnabled, 1); set (ids::eqAutoGain, 0); set (ids::outputGain, 0);
            for (int b = 0; b < kNumEQBands; ++b)
            {
                set (ids::eqBandEnabled (b), 0); set (ids::eqBandType (b), (float) kDefaultBands[b].type);
                set (ids::eqBandFreq (b), kDefaultBands[b].freq); set (ids::eqBandGain (b), 0); set (ids::eqBandQ (b), 0.71f);
                set (ids::eqBandDyn (b), 0); set (ids::eqBandThr (b), -30); set (ids::eqBandRange (b), -6);
                set (ids::eqBandSlope (b), 0); set (ids::eqBandMode (b), 0);
            }
            set (ids::gateOn, 0); set (ids::gateThreshold, -50); set (ids::gateRange, 20); set (ids::gateAttack, 1.5f); set (ids::gateRelease, 120); set (ids::gateHold, 40);
            set (ids::compEnabled, 0); set (ids::compThreshold, -18); set (ids::compRatio, 3); set (ids::compAttack, 10); set (ids::compRelease, 120);
            set (ids::compKnee, 6); set (ids::compMakeup, 0); set (ids::compAutoMakeup, 1); set (ids::compMix, 100); set (ids::compStyle, Clean); set (ids::compScHp, 20);
            set (ids::dsOn, 0); set (ids::dsFreq, 6000); set (ids::dsThreshold, -30); set (ids::dsRange, 6); set (ids::dsMode, 0);
            set (ids::smOn, 0); set (ids::smDepth, 40); set (ids::smFocus, Full); set (ids::smSpeed, Normal);
            set (ids::satOn, 0); set (ids::satDrive, 6); set (ids::satMix, 40); set (ids::satType, Tape);
            set (ids::riderOn, 0); set (ids::riderTarget, -20); set (ids::riderRange, 6); set (ids::riderSpeed, Normal);
            set (ids::vsEnabled, 0); set (ids::vsAmount, 50); set (ids::vsSpeed, Normal);
            set (ids::stWidth, 100); set (ids::stMonoBass, 0); set (ids::stMonoFreq, 120);
            set (ids::peakGuard, 0); set (ids::masterAutoTrim, 0); set (ids::maxGain, 0);
            // Tune keeps the key and scale that are already set: a preset must not move a song to another key.
            set (ids::tuneOn, 0); set (ids::tuneSpeed, 60); set (ids::tuneAmount, 100); set (ids::tuneVibrato, 70);
            set (ids::dblOn, 0); set (ids::dblMix, 45); set (ids::dblDetune, 9); set (ids::dblTiming, 45); set (ids::dblWidth, 100); set (ids::dblVoices, 0);
            set (ids::rvOn, 0); set (ids::rvMix, 18); set (ids::rvAuto, 1); set (ids::rvLength, 1); set (ids::rvDecay, 1.2f); set (ids::rvPre, 25); set (ids::rvTone, 50); set (ids::rvDuck, 50);
        }
        // speed in ms (0 = hard tune), keepVibrato in percent
        Chain& tune (float speed, float keepVibrato, float amount = 100) { set (ids::tuneOn, 1); set (ids::tuneSpeed, speed); set (ids::tuneVibrato, keepVibrato); set (ids::tuneAmount, amount); return *this; }
        Chain& dbl (float mix, float detune, float looseness, bool stack = false, float width = 100)
        {
            set (ids::dblOn, 1); set (ids::dblMix, mix); set (ids::dblDetune, detune); set (ids::dblTiming, looseness); set (ids::dblVoices, stack ? 1.0f : 0.0f); set (ids::dblWidth, width);
            return *this;
        }
        // length: 0 = one beat, 1 = two beats, 2 = four beats of the song's tempo
        Chain& verb (float mix, int length, float tone = 50, float duck = 50) { set (ids::rvOn, 1); set (ids::rvAuto, 1); set (ids::rvMix, mix); set (ids::rvLength, (float) length); set (ids::rvTone, tone); set (ids::rvDuck, duck); return *this; }

        void set (const juce::String& id, float v)
        {
            for (auto& pv : p.values) if (pv.id == id) { pv.value = v; return; }
            p.values.push_back ({ id, v });
        }

        Chain& filter (int b, EQType type, float hz, float gain, float q, int slope)
        {
            set (ids::eqBandEnabled (b), 1); set (ids::eqBandType (b), (float) type); set (ids::eqBandFreq (b), hz);
            set (ids::eqBandGain (b), gain); set (ids::eqBandQ (b), q); set (ids::eqBandSlope (b), (float) slope);
            return *this;
        }
        Chain& lowCut (float hz, int slope = 1)  { return filter (0, EQType::LowCut, hz, 0, 0.71f, slope); }
        Chain& highCut (float hz, int slope = 0) { return filter (7, EQType::HighCut, hz, 0, 0.71f, slope); }
        Chain& bell (int b, float hz, float gain, float q)   { return filter (b, EQType::Bell, hz, gain, q, 0); }
        Chain& lowShelf (int b, float hz, float gain)        { return filter (b, EQType::LowShelf, hz, gain, 0.71f, 0); }
        Chain& highShelf (int b, float hz, float gain)       { return filter (b, EQType::HighShelf, hz, gain, 0.71f, 0); }
        Chain& dyn (int b, float hz, float range, float q, float thr = -28)
        {
            filter (b, EQType::Bell, hz, 0, q, 0);
            set (ids::eqBandDyn (b), 1); set (ids::eqBandThr (b), thr); set (ids::eqBandRange (b), range);
            return *this;
        }
        Chain& gate (float thr, float range, float att = 1.0f, float rel = 140, float hold = 60)
        {
            set (ids::gateOn, 1); set (ids::gateThreshold, thr); set (ids::gateRange, range); set (ids::gateAttack, att); set (ids::gateRelease, rel); set (ids::gateHold, hold);
            return *this;
        }
        Chain& comp (Style style, float thr, float ratio, float att, float rel, float knee, float mix = 100, float scHp = 20)
        {
            set (ids::compEnabled, 1); set (ids::compStyle, style); set (ids::compThreshold, thr); set (ids::compRatio, ratio);
            set (ids::compAttack, att); set (ids::compRelease, rel); set (ids::compKnee, knee); set (ids::compMix, mix); set (ids::compScHp, scHp);
            return *this;
        }
        Chain& deEss (float hz, float range, float thr = -30, int wide = 0)
        {
            set (ids::dsOn, 1); set (ids::dsFreq, hz); set (ids::dsRange, range); set (ids::dsThreshold, thr); set (ids::dsMode, (float) wide);
            return *this;
        }
        Chain& smooth (float depth, Focus focus = Full, Speed speed = Normal)
        {
            set (ids::smOn, 1); set (ids::smDepth, depth); set (ids::smFocus, focus); set (ids::smSpeed, speed);
            return *this;
        }
        Chain& saturate (Sat type, float drive, float mix)
        {
            set (ids::satOn, 1); set (ids::satType, type); set (ids::satDrive, drive); set (ids::satMix, mix);
            return *this;
        }
        Chain& ride (float range, Speed speed = Normal, float target = -20)
        {
            set (ids::riderOn, 1); set (ids::riderRange, range); set (ids::riderSpeed, speed); set (ids::riderTarget, target);
            return *this;
        }
        Chain& makeSpace (float amount, Speed speed = Normal) { set (ids::vsEnabled, 1); set (ids::vsAmount, amount); set (ids::vsSpeed, speed); return *this; }
        Chain& stereo (float width, bool monoBass = false, float freq = 120) { set (ids::stWidth, width); set (ids::stMonoBass, monoBass ? 1.0f : 0.0f); set (ids::stMonoFreq, freq); return *this; }
        Chain& master (int mode, float lufs, float ceiling)
        {
            set (ids::masterMode, (float) mode); set (ids::targetLufs, lufs); set (ids::targetTruePeak, ceiling);
            set (ids::peakGuard, 1); set (ids::masterAutoTrim, 1);
            return *this;
        }
    };

    std::vector<ChainPreset> build()
    {
        std::vector<ChainPreset> out;
        auto add = [&out] (Chain& c) { out.push_back (c.p); };
        const juce::String V = "Vocals", D = "Drums and bass", I = "Instruments", B = "Beat", M = "Master", U = "Utility";

        // ------------------------------------------------------------------ utility
        { Chain c (U, "Clean slate", Role::Auto, "Everything off. Metering and session balance only.");
          add (c); }
        { Chain c (U, "Recording clean-up", Role::Vocal, "No tone shaping: low cut, a light gate, gentle de-essing and resonance smoothing. Use it on a raw take before deciding on a sound.");
          c.lowCut (80, 1).gate (-52, 12, 1.0f, 180, 80).deEss (6500, 4).smooth (30, Full, Normal);
          add (c); }

        // ------------------------------------------------------------------ vocals
        { Chain c (V, "Rap vocal - punchy", Role::Vocal, "Upfront and consistent. Low cut at 90 Hz, mud kept in check, presence at 3.5 kHz, fast 4:1 compression that catches 4-6 dB on loud words, de-esser, a little tube grit and a level rider.");
          c.lowCut (90, 1).dyn (2, 250, -3.5f, 1.4f, -26).bell (4, 3500, 2.0f, 0.9f).dyn (5, 5200, -3.0f, 2.0f, -26).highShelf (6, 10000, 1.5f)
           .gate (-50, 14).comp (Punch, -22, 4, 3, 60, 3).deEss (6000, 5).saturate (Tube, 3, 30).ride (5, Normal);
          add (c); }
        { Chain c (V, "Rap vocal - aggressive", Role::Vocal, "For drill and hard trap deliveries. Hard 8:1 compression with the fastest attack, more upper-mid bite, heavier saturation, and smoothing on the top so it stays loud without hurting.");
          c.lowCut (100, 1).bell (2, 300, -3.0f, 1.2f).bell (4, 2800, 3.0f, 1.0f).highShelf (6, 9000, 2.0f)
           .gate (-48, 16, 0.5f, 110, 50).comp (Punch, -24, 8, 1, 40, 2).deEss (6500, 7).smooth (30, Highs, Fast).saturate (Tube, 6, 45).ride (6, Fast);
          add (c); }
        { Chain c (V, "Rap vocal - tuned and doubled", Role::Vocal, "The punchy rap chain with gentle pitch correction that keeps the delivery natural, a tight double either side, and a short reverb timed to the beat that steps back while you rap.");
          c.lowCut (90, 1).dyn (2, 250, -3.5f, 1.4f, -26).bell (4, 3500, 2.0f, 0.9f).dyn (5, 5200, -3.0f, 2.0f, -26).highShelf (6, 10000, 1.5f)
           .gate (-50, 14).comp (Punch, -22, 4, 3, 60, 3).deEss (6000, 5).saturate (Tube, 3, 30).ride (5, Normal)
           .tune (80, 80).dbl (35, 7, 30).verb (12, 0, 45, 70);
          add (c); }
        { Chain c (V, "Melodic trap - hard tune", Role::Vocal, "The snapped, robotic pitch sound of melodic trap. Every note jumps straight onto the scale, so set the key first (Find key on the FX tab). Bright top, firm compression, wide doubles and a two-beat reverb.");
          c.lowCut (100, 1).bell (2, 280, -2.5f, 1.2f).bell (4, 3200, 2.0f, 0.9f).highShelf (6, 10500, 3.0f)
           .gate (-50, 12).comp (Punch, -24, 5, 2, 50, 3).deEss (6500, 6).smooth (25, Highs, Normal).saturate (Tape, 3, 30)
           .tune (0, 0).dbl (40, 10, 40).verb (20, 1, 60, 55);
          add (c); }
        { Chain c (V, "Hook stack", Role::Vocal, "Makes one lead take sound like a stacked hook: four extra voices spread left and right, natural pitch correction so the stack is in tune with itself, and a longer reverb for size.");
          c.lowCut (95, 1).dyn (2, 260, -3.0f, 1.3f, -26).bell (4, 3000, 1.5f, 0.8f).highShelf (6, 11000, 2.5f)
           .comp (Smooth, -22, 3.5f, 6, 90, 3).deEss (6200, 5).smooth (25, Highs, Normal).ride (4, Normal)
           .tune (50, 60).dbl (60, 11, 60, true).verb (22, 1, 55, 50);
          add (c); }
        { Chain c (V, "Melodic vocal - smooth", Role::Vocal, "For sung hooks and R&B. Slower, levelling compression that keeps the performance natural, wide presence, open air above 11 kHz and resonance smoothing for a polished top end.");
          c.lowCut (80, 1).dyn (2, 220, -3.0f, 1.3f, -27).bell (4, 3000, 1.5f, 0.8f).highShelf (6, 11000, 3.0f)
           .gate (-52, 12, 1.5f, 200, 90).comp (Smooth, -22, 3, 12, 120, 8).deEss (7000, 5).smooth (35, Full, Normal).saturate (Tape, 3, 25).ride (6, Normal);
          add (c); }
        { Chain c (V, "Melodic vocal - bright pop", Role::Vocal, "Airy and forward. More top end and a firmer 4:1 so every word sits on top of a dense beat.");
          c.lowCut (95, 1).dyn (2, 240, -3.5f, 1.3f, -27).bell (3, 600, -1.5f, 1.2f).bell (4, 4000, 2.5f, 0.9f).highShelf (6, 12000, 4.0f)
           .gate (-52, 12, 1.5f, 180, 80).comp (Smooth, -23, 4, 8, 100, 6).deEss (7500, 6).smooth (40, Highs, Normal).saturate (Tape, 2, 20).ride (6, Normal);
          add (c); }
        { Chain c (V, "Backing vocals", Role::BackingVocal, "Sits behind the lead: higher low cut, a dip where the lead's words live, firm levelling, extra de-essing and a wider image.");
          c.lowCut (150, 1).bell (2, 350, -3.0f, 1.2f).bell (4, 2600, -2.0f, 0.8f).highShelf (6, 10000, 3.0f)
           .comp (Smooth, -24, 5, 5, 90, 6).deEss (6500, 8).smooth (30, Full, Normal).ride (4, Normal, -24).stereo (130);
          add (c); }
        { Chain c (V, "Adlibs", Role::BackingVocal, "Thin, bright and out of the way of the main vocal. Low cut at 180 Hz, capped top, crunchy saturation.");
          c.lowCut (180, 1).bell (2, 400, -3.0f, 1.2f).bell (4, 4000, 2.0f, 1.0f).highShelf (6, 10000, 2.0f).highCut (14000, 0)
           .comp (Punch, -22, 4, 5, 60, 3).deEss (6000, 6).saturate (Tube, 5, 40).stereo (120);
          add (c); }
        { Chain c (V, "Telephone effect", Role::BackingVocal, "The band-limited radio voice for intros and drops. Steep cuts at 400 Hz and 3.5 kHz with heavy tube drive.");
          c.lowCut (400, 2).bell (4, 1600, 4.0f, 1.0f).highCut (3500, 2).comp (Punch, -24, 8, 2, 60, 2).saturate (Tube, 12, 70);
          add (c); }
        { Chain c (V, "Spoken word", Role::Vocal, "For podcasts, interviews and skits. Even, close and clear without sounding processed.");
          c.lowCut (80, 1).bell (2, 250, -2.0f, 1.2f).bell (4, 4000, 2.0f, 0.9f)
           .gate (-48, 20, 2.0f, 220, 120).comp (Smooth, -22, 3, 10, 150, 8).deEss (6000, 5).ride (8, Slow);
          add (c); }

        // ------------------------------------------------------------------ drums and bass
        { Chain c (D, "Kick - punch", Role::Kick, "Weight at 60 Hz, the cardboard at 400 Hz removed, click at 3.5 kHz, and compression with a slow attack so the hit gets through before it clamps.");
          c.lowCut (30, 1).bell (1, 60, 2.0f, 1.2f).bell (3, 400, -3.5f, 1.5f).bell (5, 3500, 2.5f, 1.2f)
           .comp (Punch, -16, 4, 20, 60, 4).saturate (Warm, 3, 25).makeSpace (30, Fast);
          add (c); }
        { Chain c (D, "Kick - deep", Role::Kick, "For soft, subby kicks under an 808-led beat. More 50 Hz, less click, capped top.");
          c.lowCut (28, 1).bell (1, 50, 3.0f, 1.0f).bell (3, 300, -3.0f, 1.3f).highCut (9000, 0).comp (Clean, -16, 3, 25, 80, 6).makeSpace (25, Fast);
          add (c); }
        { Chain c (D, "808 - clean sub", Role::Bass808, "Tight and solid. Sub kept mono, boom controlled only when it builds, harmonics added so it is still heard on phones, slow compression for an even low end, and it ducks for the vocal.");
          c.lowCut (26, 1).bell (1, 55, 1.5f, 1.0f).dyn (2, 260, -3.0f, 1.2f, -24).highCut (6000, 0)
           .comp (Smooth, -20, 4, 25, 160, 6).saturate (Warm, 5, 30).stereo (100, true, 120).makeSpace (40);
          add (c); }
        { Chain c (D, "808 - distorted", Role::Bass808, "Gritty and loud in the mids so it cuts on any speaker. Heavy tube drive, a push at 900 Hz, firm compression to hold the glides.");
          c.lowCut (28, 1).bell (3, 900, 2.0f, 1.0f).dyn (2, 220, -3.0f, 1.2f, -24).highCut (8000, 0)
           .comp (Punch, -20, 6, 10, 100, 4).saturate (Tube, 12, 60).stereo (100, true, 120).makeSpace (40);
          add (c); }
        { Chain c (D, "Snare and clap", Role::Drums, "Body at 200 Hz, less box, crack at 5 kHz and air on top. Medium attack keeps the snap.");
          c.lowCut (110, 1).bell (2, 200, 2.0f, 1.2f).bell (3, 500, -2.0f, 1.4f).bell (5, 5000, 3.0f, 1.0f).highShelf (6, 10000, 2.0f)
           .comp (Punch, -18, 4, 12, 80, 4).saturate (Tape, 3, 25).makeSpace (35, Fast);
          add (c); }
        { Chain c (D, "Hi-hats", Role::Drums, "Everything below 300 Hz removed, a little air, and smoothing so fast hat rolls do not get piercing.");
          c.lowCut (300, 1).highShelf (6, 12000, 1.5f).smooth (35, Highs, Fast).makeSpace (30, Fast);
          add (c); }
        { Chain c (D, "Drum bus - glue", Role::Drums, "Makes separate drum hits feel like one kit. Gentle 3:1 with the detector high-passed so the kick does not pump everything, a touch of tape.");
          c.lowCut (35, 1).dyn (2, 300, -2.0f, 1.2f, -24).highShelf (6, 8000, 1.5f)
           .comp (Glue, -18, 3, 20, 120, 6, 100, 100).saturate (Tape, 3, 20).makeSpace (50);
          add (c); }
        { Chain c (D, "Drum bus - parallel smash", Role::Drums, "Crushes the drums 10:1 and blends 35 % of that back under the clean signal: more weight and excitement, transients intact.");
          c.lowCut (35, 1).comp (Punch, -28, 10, 2, 50, 2, 35, 80).saturate (Tape, 4, 30).makeSpace (45);
          add (c); }

        // ------------------------------------------------------------------ instruments
        { Chain c (I, "Piano and keys", Role::Melody, "Clears the low end out of the bass's way, controls low-mid build-up and leaves a pocket for the vocal.");
          c.lowCut (90, 1).dyn (2, 300, -2.5f, 1.2f, -26).dyn (4, 2500, -2.0f, 1.0f, -26).highShelf (6, 9000, 1.5f)
           .comp (Smooth, -20, 2.5f, 20, 150, 8).makeSpace (55);
          add (c); }
        { Chain c (I, "Guitar", Role::Melody, "Less boom, more string detail, gentle levelling.");
          c.lowCut (100, 1).bell (2, 250, -3.0f, 1.2f).bell (5, 5000, 2.0f, 1.0f).comp (Smooth, -20, 3, 15, 120, 6).smooth (25, Highs).makeSpace (50);
          add (c); }
        { Chain c (I, "Synth lead", Role::Melody, "Keeps a bright lead exciting without stepping on the vocal: dynamic dip at 3 kHz and smoothing on the top.");
          c.lowCut (120, 1).dyn (4, 3000, -2.5f, 1.2f, -24).highShelf (6, 10000, 1.0f).comp (Clean, -18, 3, 10, 100, 6).smooth (30, Highs).makeSpace (60);
          add (c); }
        { Chain c (I, "Pad and strings", Role::Melody, "Pushed to the back and the sides: low cut at 180 Hz, softened upper mids, capped top and a wider image.");
          c.lowCut (180, 1).bell (2, 400, -3.0f, 1.0f).bell (4, 3000, -2.0f, 0.8f).highCut (12000, 0).stereo (140).makeSpace (70, Slow);
          add (c); }
        { Chain c (I, "Sample clean-up", Role::Melody, "For chopped samples and loops with baked-in problems. Evens out resonances and mud before you build on it.");
          c.lowCut (40, 1).dyn (2, 280, -3.0f, 1.2f, -26).smooth (40, Full, Normal).makeSpace (55);
          add (c); }
        { Chain c (I, "FX and risers", Role::FX, "Low cut at 200 Hz and extra width so effects decorate the mix instead of filling it.");
          c.lowCut (200, 1).stereo (150).makeSpace (60, Slow);
          add (c); }

        // ------------------------------------------------------------------ beat
        { Chain c (B, "Beat - make room for the vocal", Role::Beat, "For a bought or bounced 2-track. Light mud and harshness control, gentle parallel glue, and Vocal Space so the beat steps back only while the vocal is singing.");
          c.lowCut (28, 1).dyn (2, 250, -2.0f, 1.2f, -22).dyn (4, 2800, -2.0f, 1.2f, -22)
           .comp (Glue, -16, 2, 30, 200, 8, 70, 120).makeSpace (60);
          add (c); }
        { Chain c (B, "Beat - brighten", Role::Beat, "Opens up a dull or muffled instrumental with a low-mid tidy and a top shelf.");
          c.lowCut (28, 1).bell (2, 250, -2.0f, 1.0f).highShelf (6, 10000, 2.5f).makeSpace (55);
          add (c); }
        { Chain c (B, "Beat - tame harshness", Role::Beat, "For over-bright or over-compressed beats. Resonance smoothing on the highs and a dynamic dip at 3.5 kHz.");
          c.lowCut (28, 1).dyn (4, 3500, -3.0f, 1.4f, -22).smooth (50, Highs, Normal).makeSpace (55);
          add (c); }

        // ------------------------------------------------------------------ master
        { Chain c (M, "Mix hand-off", Role::Master, "For sending to a mastering engineer. No tone changes. Trims the mix to -18 LUFS and keeps peaks under -6 dBTP.");
          c.master (0, -18.0f, -6.0f);
          add (c); }
        { Chain c (M, "Streaming master", Role::Master, "A finished master for Spotify, Apple Music and YouTube: -14 LUFS with a -1 dBTP ceiling. Sub rumble removed, 1-2 dB of glue, a touch of tape, mono bass, true-peak limiter.");
          c.lowCut (25, 1).comp (Glue, -14, 1.5f, 30, 200, 10, 100, 100).saturate (Tape, 2, 15).stereo (100, true, 110).master (1, -14.0f, -1.0f);
          add (c); }
        { Chain c (M, "Loud master", Role::Master, "For hip-hop, drill and club releases that need to compete: -9 LUFS with a -2 dBTP ceiling. Firmer glue and light smoothing so it gets loud without getting harsh.");
          c.lowCut (28, 1).comp (Glue, -14, 2, 30, 180, 8, 100, 100).smooth (20, Full, Normal).saturate (Tape, 3, 20).stereo (100, true, 110).master (2, -9.0f, -2.0f);
          add (c); }
        { Chain c (M, "Master - warm and wide", Role::Master, "Streaming level with more tape colour, a gentle low shelf and a slightly wider image above the bass.");
          c.lowCut (25, 1).lowShelf (1, 110, 1.0f).highShelf (6, 11000, 1.0f).comp (Glue, -14, 1.5f, 30, 220, 10, 100, 100)
           .saturate (Tape, 4, 30).stereo (112, true, 120).master (1, -14.0f, -1.0f);
          add (c); }
        return out;
    }
}

const std::vector<ChainPreset>& chainPresets()
{
    static const std::vector<ChainPreset> presets = build();
    return presets;
}

juce::StringArray chainPresetCategories()
{
    juce::StringArray c;
    for (auto& p : chainPresets()) c.addIfNotAlreadyThere (p.category);
    return c;
}

int findChainPreset (const juce::String& name)
{
    const auto& all = chainPresets();
    for (size_t i = 0; i < all.size(); ++i) if (all[i].name == name) return (int) i;
    return -1;
}
}
