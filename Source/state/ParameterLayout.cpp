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
    p.push_back (std::make_unique<APC> (id (ids::compStyle), "Compressor style", juce::StringArray { "Clean", "Punch", "Smooth", "Glue" }, 0));
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

    return { p.begin(), p.end() };
}
}
