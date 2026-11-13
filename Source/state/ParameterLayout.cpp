#include "ParameterLayout.h"

namespace mu
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;

static juce::NormalisableRange<float> freqRange()
{
    juce::NormalisableRange<float> r (20.0f, 20000.0f, 0.01f);
    r.setSkewForCentre (632.0f);
    return r;
}

static juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
static juce::String msText (float v, int) { return v < 10.0f ? juce::String (v, 2) + " ms" : juce::String (v, 1) + " ms"; }
static juce::String hzText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto id = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };

    p.push_back (std::make_unique<APC> (id (ids::role), "Role", roleNames(), 0));
    p.push_back (std::make_unique<APB> (id (ids::bypass), "Bypass", false));
    p.push_back (std::make_unique<APF> (id (ids::trim), "Balance trim",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    p.push_back (std::make_unique<APF> (id (ids::outputGain), "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    p.push_back (std::make_unique<APB> (id (ids::autoBalance), "Accept balance moves", true));

    p.push_back (std::make_unique<APF> (id (ids::targetLufs), "Target loudness",
        juce::NormalisableRange<float> (-30.0f, -6.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " LUFS"; })));
    p.push_back (std::make_unique<APF> (id (ids::targetTruePeak), "Target true peak",
        juce::NormalisableRange<float> (-12.0f, -0.5f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dBTP"; })));

    // EQ
    p.push_back (std::make_unique<APB> (id (ids::eqEnabled), "EQ enabled", true));
    for (int b = 0; b < kNumEQBands; ++b)
    {
        const auto& d = kDefaultBands[b];
        const juce::String n = "EQ " + juce::String (b + 1) + " ";
        p.push_back (std::make_unique<APB> (id (ids::eqBandEnabled (b)), n + "on", false));
        p.push_back (std::make_unique<APC> (id (ids::eqBandType (b)), n + "type", eqTypeNames(), (int) d.type));
        p.push_back (std::make_unique<APF> (id (ids::eqBandFreq (b)), n + "freq", freqRange(), d.freq,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (hzText).withLabel ("Hz")));
        p.push_back (std::make_unique<APF> (id (ids::eqBandGain (b)), n + "gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
        juce::NormalisableRange<float> qr (0.1f, 18.0f, 0.001f);
        qr.setSkewForCentre (1.0f);
        p.push_back (std::make_unique<APF> (id (ids::eqBandQ (b)), n + "Q", qr, 0.71f));
    }

    // Compressor
    p.push_back (std::make_unique<APB> (id (ids::compEnabled), "Compressor enabled", false));
    p.push_back (std::make_unique<APF> (id (ids::compThreshold), "Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    juce::NormalisableRange<float> ratioRange (1.0f, 20.0f, 0.01f);
    ratioRange.setSkewForCentre (4.0f);
    p.push_back (std::make_unique<APF> (id (ids::compRatio), "Ratio", ratioRange, 3.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + ":1"; })));
    juce::NormalisableRange<float> attRange (0.1f, 200.0f, 0.01f);
    attRange.setSkewForCentre (10.0f);
    p.push_back (std::make_unique<APF> (id (ids::compAttack), "Attack", attRange, 10.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
    juce::NormalisableRange<float> relRange (10.0f, 2000.0f, 0.1f);
    relRange.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<APF> (id (ids::compRelease), "Release", relRange, 120.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (msText).withLabel ("ms")));
    p.push_back (std::make_unique<APF> (id (ids::compKnee), "Knee",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    p.push_back (std::make_unique<APF> (id (ids::compMakeup), "Makeup",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    p.push_back (std::make_unique<APB> (id (ids::compAutoMakeup), "Auto makeup", true));
    p.push_back (std::make_unique<APF> (id (ids::compMix), "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));

    // Vocal Space
    p.push_back (std::make_unique<APB> (id (ids::vsEnabled), "Vocal space", false));
    p.push_back (std::make_unique<APF> (id (ids::vsAmount), "Vocal space amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
    p.push_back (std::make_unique<APC> (id (ids::vsSpeed), "Vocal space speed", juce::StringArray { "Smooth", "Normal", "Fast" }, 1));

    return { p.begin(), p.end() };
}
}
