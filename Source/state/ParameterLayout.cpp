#include "ParameterLayout.h"

namespace mu
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;

static juce::NormalisableRange<float> freqRange (float lo = 20.0f, float hi = 20000.0f, float centre = 632.0f)
{
    juce::NormalisableRange<float> r (lo, hi, 0.01f);
    r.setSkewForCentre (centre);
    return r;
}

static juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
static juce::String msText (float v, int) { return v < 10.0f ? juce::String (v, 2) + " ms" : juce::String (v, 1) + " ms"; }
static juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
static juce::String hzText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

static auto dbAttr() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB"); }
static auto msAttr() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms"); }
static auto hzAttr() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withLabel ("Hz"); }
static auto pctAttr() { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctText).withLabel ("%"); }
static auto semisAttr()
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        if (std::abs (v) < 0.05f) return juce::String ("0 st");
        return (v > 0.0f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.05f ? 0 : 1) + " st";
    });
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto id = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    auto range = [] (float lo, float hi, float step = 0.01f) { return juce::NormalisableRange<float> (lo, hi, step); };
    auto skewed = [] (float lo, float hi, float centre, float step = 0.01f) { juce::NormalisableRange<float> r (lo, hi, step); r.setSkewForCentre (centre); return r; };

    // ---- instance
    p.push_back (std::make_unique<APC> (id (ids::role), "Role", roleNames(), 0));
    p.push_back (std::make_unique<APB> (id (ids::bypass), "Bypass", false));
    p.push_back (std::make_unique<APF> (id (ids::trim), "Balance trim", range (-24.0f, 24.0f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::outputGain), "Output", range (-24.0f, 24.0f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APB> (id (ids::autoBalance), "Accept balance moves", true));

    // ---- master
    p.push_back (std::make_unique<APF> (id (ids::targetLufs), "Target loudness", range (-30.0f, -6.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " LUFS"; })));
    p.push_back (std::make_unique<APF> (id (ids::targetTruePeak), "True peak ceiling", range (-12.0f, -0.5f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dBTP"; })));
    p.push_back (std::make_unique<APB> (id (ids::masterAutoTrim), "Master auto trim to target", false));
    p.push_back (std::make_unique<APB> (id (ids::peakGuard), "Peak guard", false));
    for (int r = (int) Role::Vocal; r < (int) Role::Master; ++r)
    {
        const auto role = (Role) r;
        p.push_back (std::make_unique<APF> (id (ids::roleOffset (role)), roleNames()[r] + " target", range (-24.0f, 6.0f, 0.1f), defaultRoleOffsetLu (role),
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " LU"; })));
    }

    // ---- gate
    p.push_back (std::make_unique<APB> (id (ids::gateOn), "Gate", false));
    p.push_back (std::make_unique<APF> (id (ids::gateThreshold), "Gate threshold", range (-80.0f, 0.0f, 0.1f), -50.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::gateRange), "Gate range", range (0.0f, 60.0f, 0.1f), 20.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::gateAttack), "Gate attack", skewed (0.1f, 50.0f, 3.0f), 1.5f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::gateRelease), "Gate release", skewed (10.0f, 1000.0f, 150.0f), 120.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::gateHold), "Gate hold", range (0.0f, 500.0f, 1.0f), 40.0f, msAttr()));

    // ---- EQ
    p.push_back (std::make_unique<APB> (id (ids::eqEnabled), "EQ enabled", true));
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto& d = kDefaultBands[b];
        const juce::String n = "EQ " + juce::String (b + 1) + " ";
        p.push_back (std::make_unique<APB> (id (ids::eqBandEnabled (b)), n + "on", false));
        p.push_back (std::make_unique<APC> (id (ids::eqBandType (b)), n + "type", eqTypeNames(), (int) d.type));
        p.push_back (std::make_unique<APF> (id (ids::eqBandFreq (b)), n + "freq", freqRange(), d.freq, hzAttr()));
        p.push_back (std::make_unique<APF> (id (ids::eqBandGain (b)), n + "gain", range (-18.0f, 18.0f), 0.0f, dbAttr()));
        p.push_back (std::make_unique<APF> (id (ids::eqBandQ (b)), n + "Q", skewed (0.1f, 18.0f, 1.0f, 0.001f), 0.71f));
        p.push_back (std::make_unique<APB> (id (ids::eqBandDyn (b)), n + "dynamic", false));
        p.push_back (std::make_unique<APF> (id (ids::eqBandThr (b)), n + "threshold", range (-60.0f, 0.0f, 0.1f), -30.0f, dbAttr()));
        p.push_back (std::make_unique<APF> (id (ids::eqBandRange (b)), n + "range", range (-18.0f, 18.0f, 0.1f), -6.0f, dbAttr()));
        p.push_back (std::make_unique<APC> (id (ids::eqBandSlope (b)), n + "slope", juce::StringArray { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 0));
        p.push_back (std::make_unique<APC> (id (ids::eqBandMode (b)), n + "channel", juce::StringArray { "Stereo", "Mid", "Side" }, 0));
    }
    p.push_back (std::make_unique<APB> (id (ids::eqAutoGain), "EQ auto gain", false));

    // ---- compressor
    p.push_back (std::make_unique<APB> (id (ids::compEnabled), "Compressor", false));
    p.push_back (std::make_unique<APF> (id (ids::compThreshold), "Threshold", range (-60.0f, 0.0f, 0.1f), -18.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::compRatio), "Ratio", skewed (1.0f, 20.0f, 4.0f), 3.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + ":1"; })));
    p.push_back (std::make_unique<APF> (id (ids::compAttack), "Attack", skewed (0.1f, 200.0f, 10.0f), 10.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::compRelease), "Release", skewed (10.0f, 2000.0f, 150.0f, 0.1f), 120.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::compKnee), "Knee", range (0.0f, 24.0f, 0.1f), 6.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::compMakeup), "Makeup", range (-12.0f, 24.0f, 0.1f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APB> (id (ids::compAutoMakeup), "Auto makeup", true));
    p.push_back (std::make_unique<APF> (id (ids::compMix), "Mix", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::compStyle), "Compressor style", juce::StringArray { "Clean", "Punch", "Smooth", "Glue", "Vocal" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::compScHp), "Sidechain high-pass", skewed (20.0f, 500.0f, 120.0f, 1.0f), 20.0f, hzAttr()));

    // ---- smooth (resonance tamer)
    p.push_back (std::make_unique<APB> (id (ids::smOn), "Smooth", false));
    p.push_back (std::make_unique<APF> (id (ids::smDepth), "Smooth depth", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::smFocus), "Smooth focus", juce::StringArray { "Full range", "Low mids", "Harsh highs" }, 0));
    p.push_back (std::make_unique<APC> (id (ids::smSpeed), "Smooth speed", juce::StringArray { "Slow", "Normal", "Fast" }, 1));

    // ---- stereo
    p.push_back (std::make_unique<APF> (id (ids::stWidth), "Stereo width", range (0.0f, 200.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APB> (id (ids::stMonoBass), "Mono bass", false));
    p.push_back (std::make_unique<APF> (id (ids::stMonoFreq), "Mono bass below", skewed (60.0f, 300.0f, 120.0f, 1.0f), 120.0f, hzAttr()));

    // ---- master
    p.push_back (std::make_unique<APF> (id (ids::maxGain), "Limiter gain", range (0.0f, 24.0f, 0.1f), 0.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (id (ids::masterMode), "Master mode", juce::StringArray { "Mix hand-off", "Streaming master", "Loud master", "Custom" }, 0));

    // ---- de-esser
    p.push_back (std::make_unique<APB> (id (ids::dsOn), "De-esser", false));
    p.push_back (std::make_unique<APF> (id (ids::dsFreq), "De-ess frequency", freqRange (2000.0f, 12000.0f, 6000.0f), 6000.0f, hzAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dsThreshold), "De-ess threshold", range (-60.0f, 0.0f, 0.1f), -30.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dsRange), "De-ess range", range (0.0f, 18.0f, 0.1f), 6.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (id (ids::dsMode), "De-ess mode", juce::StringArray { "Split band", "Wide band" }, 0));

    // ---- saturator
    p.push_back (std::make_unique<APB> (id (ids::satOn), "Saturation", false));
    p.push_back (std::make_unique<APF> (id (ids::satDrive), "Drive", range (0.0f, 24.0f, 0.1f), 6.0f, dbAttr()));
    p.push_back (std::make_unique<APF> (id (ids::satMix), "Saturation mix", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::satType), "Saturation type", juce::StringArray { "Tape", "Tube", "Warm" }, 0));

    // ---- rider
    p.push_back (std::make_unique<APB> (id (ids::riderOn), "Rider", false));
    p.push_back (std::make_unique<APF> (id (ids::riderTarget), "Rider target", range (-40.0f, -6.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<APF> (id (ids::riderRange), "Rider range", range (0.0f, 12.0f, 0.1f), 6.0f, dbAttr()));
    p.push_back (std::make_unique<APC> (id (ids::riderSpeed), "Rider speed", juce::StringArray { "Slow", "Normal", "Fast" }, 1));

    // ---- vocal space
    p.push_back (std::make_unique<APB> (id (ids::vsEnabled), "Vocal space", false));
    p.push_back (std::make_unique<APF> (id (ids::vsAmount), "Vocal space amount", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::vsSpeed), "Vocal space speed", juce::StringArray { "Smooth", "Normal", "Fast" }, 1));

    // ---- tune
    p.push_back (std::make_unique<APB> (id (ids::tuneOn), "Tune", false));
    p.push_back (std::make_unique<APC> (id (ids::tuneKey), "Tune key", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<APC> (id (ids::tuneScale), "Tune scale", juce::StringArray { "Chromatic", "Major", "Minor", "Major pentatonic", "Minor pentatonic", "Custom" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::tuneSpeed), "Tune speed", skewed (0.0f, 400.0f, 60.0f, 0.1f), 60.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::tuneAmount), "Tune amount", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::tuneVibrato), "Tune keep vibrato", range (0.0f, 100.0f, 0.1f), 70.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::tuneMode), "Tune view", juce::StringArray { "Simple", "Graph" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::tuneHold), "Tune note hold", range (0.0f, 100.0f, 0.1f), 30.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::tuneFormant), "Tune formant", range (-12.0f, 12.0f, 0.1f), 0.0f, semisAttr()));
    p.push_back (std::make_unique<juce::AudioParameterInt> (id (ids::tuneNotes), "Tune custom notes", 0, 4095, 4095));
    p.push_back (std::make_unique<APF> (id (ids::tuneFlex), "Tune flex", range (0.0f, 50.0f, 0.1f), 8.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " cents"; })));
    p.push_back (std::make_unique<APF> (id (ids::tuneHumanise), "Tune humanise", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::tuneSlides), "Tune keep slides", range (0.0f, 100.0f, 0.1f), 70.0f, pctAttr()));

    // ---- peak guard on a track
    p.push_back (std::make_unique<APB> (id (ids::trackGuard), "Track peak guard", true));

    // ---- doubler
    p.push_back (std::make_unique<APB> (id (ids::dblOn), "Doubler", false));
    p.push_back (std::make_unique<APF> (id (ids::dblMix), "Doubler level", range (0.0f, 100.0f, 0.1f), 45.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dblDetune), "Doubler detune", range (0.0f, 25.0f, 0.1f), 9.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " cents"; })));
    p.push_back (std::make_unique<APF> (id (ids::dblTiming), "Doubler looseness", range (0.0f, 100.0f, 0.1f), 45.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dblWidth), "Doubler width", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::dblVoices), "Doubler voices", juce::StringArray { "Double", "Stack" }, 0));

    // ---- reverb
    p.push_back (std::make_unique<juce::AudioParameterInt> (id (ids::fxOrder), "FX order", 0, 719, 0, juce::AudioParameterIntAttributes().withAutomatable (false)));
    p.push_back (std::make_unique<APB> (id (ids::rvOn), "Reverb", false));
    p.push_back (std::make_unique<APF> (id (ids::rvMix), "Reverb level", range (0.0f, 100.0f, 0.1f), 18.0f, pctAttr()));
    p.push_back (std::make_unique<APB> (id (ids::rvAuto), "Reverb timed to tempo", true));
    p.push_back (std::make_unique<APC> (id (ids::rvLength), "Reverb length", juce::StringArray { "1 beat", "2 beats", "4 beats" }, 1));
    p.push_back (std::make_unique<APF> (id (ids::rvDecay), "Reverb decay", skewed (0.1f, 30.0f, 2.0f, 0.01f), 1.2f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " s"; })));
    p.push_back (std::make_unique<APF> (id (ids::rvPre), "Reverb pre-delay", skewed (0.0f, 500.0f, 40.0f, 0.1f), 25.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvTone), "Reverb tone", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvDuck), "Reverb duck", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::rvType), "Reverb space", juce::StringArray { "Room", "Plate", "Hall", "Chamber", "Cathedral", "Ambient", "Vintage", "Shimmer", "Gated" }, 1));
    p.push_back (std::make_unique<APF> (id (ids::rvLowCut), "Reverb low cut", skewed (60.0f, 600.0f, 200.0f, 1.0f), 180.0f, hzAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvWidth), "Reverb width", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvSize), "Reverb size", range (30.0f, 200.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvDiffusion), "Reverb diffusion", range (0.0f, 100.0f, 0.1f), 80.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvModDepth), "Reverb movement", range (0.0f, 100.0f, 0.1f), 30.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvModRate), "Reverb movement rate", skewed (0.1f, 3.0f, 0.7f, 0.01f), 0.6f, hzAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvLowDecay), "Reverb low decay", range (30.0f, 200.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::rvEarly), "Reverb early reflections", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APB> (id (ids::rvFreeze), "Reverb freeze", false));
    p.push_back (std::make_unique<APF> (id (ids::rvShimmer), "Reverb shimmer", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));

    // ---- delay
    p.push_back (std::make_unique<APB> (id (ids::dlOn), "Delay", false));
    p.push_back (std::make_unique<APF> (id (ids::dlMix), "Delay level", range (0.0f, 100.0f, 0.1f), 22.0f, pctAttr()));
    p.push_back (std::make_unique<APB> (id (ids::dlSync), "Delay timed to tempo", true));
    p.push_back (std::make_unique<APC> (id (ids::dlNote), "Delay note", juce::StringArray { "1/2", "1/4", "1/8 dotted", "1/8", "1/8 triplet", "1/16", "1 bar", "1/4 dotted", "1/4 triplet", "1/16 triplet", "1/32" }, 3));
    p.push_back (std::make_unique<APF> (id (ids::dlTime), "Delay time", skewed (20.0f, 4000.0f, 350.0f, 0.1f), 250.0f, msAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlFeedback), "Delay repeats", range (0.0f, 90.0f, 0.1f), 35.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlTone), "Delay tone", range (0.0f, 100.0f, 0.1f), 45.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlWidth), "Delay ping-pong", range (0.0f, 100.0f, 0.1f), 70.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlDuck), "Delay duck", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APC> (id (ids::dlStyle), "Delay style", juce::StringArray { "Digital", "Tape", "Analog", "Diffused" }, 0));
    p.push_back (std::make_unique<APC> (id (ids::dlPattern), "Delay pattern", juce::StringArray { "Ping-pong", "Straight", "Dual" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::dlDrive), "Delay drive", range (0.0f, 100.0f, 0.1f), 30.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlWobble), "Delay wobble", range (0.0f, 100.0f, 0.1f), 0.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::dlLowCut), "Delay low cut", skewed (20.0f, 1000.0f, 200.0f, 1.0f), 160.0f, hzAttr()));

    // ---- harmony
    p.push_back (std::make_unique<APB> (id (ids::hmOn), "Harmony", false));
    p.push_back (std::make_unique<APC> (id (ids::hmVoicing), "Harmony voices", juce::StringArray { "Third up", "Fifth up", "Third and fifth", "Octave up", "Octave down", "Third below", "Choir" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::hmMix), "Harmony level", range (0.0f, 100.0f, 0.1f), 45.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::hmWidth), "Harmony width", range (0.0f, 100.0f, 0.1f), 80.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::hmHumanise), "Harmony humanise", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));

    // ---- shift
    p.push_back (std::make_unique<APB> (id (ids::psOn), "Shift", false));
    p.push_back (std::make_unique<APF> (id (ids::psSemis), "Shift pitch", range (-24.0f, 24.0f, 1.0f), -12.0f, semisAttr()));
    p.push_back (std::make_unique<APF> (id (ids::psCharacter), "Shift character", range (-100.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int n = juce::roundToInt (v);
            return n == 0 ? juce::String ("natural") : juce::String (std::abs (n)) + (n < 0 ? " % bigger" : " % smaller");
        })));
    p.push_back (std::make_unique<APF> (id (ids::psMix), "Shift mix", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));

    // ---- phone
    p.push_back (std::make_unique<APB> (id (ids::phOn), "Phone", false));
    p.push_back (std::make_unique<APF> (id (ids::phAmount), "Phone amount", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::phTone), "Phone width", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::phDrive), "Phone drive", range (0.0f, 100.0f, 0.1f), 35.0f, pctAttr()));

    // ---- lo-fi
    p.push_back (std::make_unique<APB> (id (ids::lfOn), "Lo-fi", false));
    p.push_back (std::make_unique<APF> (id (ids::lfCrush), "Lo-fi crush", range (0.0f, 100.0f, 0.1f), 40.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::lfWobble), "Lo-fi wobble", range (0.0f, 100.0f, 0.1f), 30.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::lfMix), "Lo-fi mix", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));

    // ---- motion
    p.push_back (std::make_unique<APB> (id (ids::mdOn), "Motion", false));
    p.push_back (std::make_unique<APC> (id (ids::mdType), "Motion type", juce::StringArray { "Chorus", "Flanger", "Phaser" }, 0));
    p.push_back (std::make_unique<APF> (id (ids::mdRate), "Motion rate", skewed (0.05f, 8.0f, 0.8f, 0.01f), 0.6f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));
    p.push_back (std::make_unique<APF> (id (ids::mdDepth), "Motion depth", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::mdMix), "Motion mix", range (0.0f, 100.0f, 0.1f), 50.0f, pctAttr()));

    // ---- chop
    p.push_back (std::make_unique<APB> (id (ids::chOn), "Chop", false));
    p.push_back (std::make_unique<APC> (id (ids::chRate), "Chop rate", juce::StringArray { "1/4", "1/8", "1/8 triplet", "1/16", "1/32" }, 3));
    p.push_back (std::make_unique<APF> (id (ids::chDepth), "Chop depth", range (0.0f, 100.0f, 0.1f), 100.0f, pctAttr()));
    p.push_back (std::make_unique<APF> (id (ids::chShape), "Chop hardness", range (0.0f, 100.0f, 0.1f), 70.0f, pctAttr()));

    return { p.begin(), p.end() };
}
}
