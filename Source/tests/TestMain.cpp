// Console test runner for the DSP and balance engine. Build target: MUTests.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>
#include "dsp/LoudnessMeter.h"
#include "dsp/EQEngine.h"
#include "dsp/Compressor.h"
#include "dsp/VocalSpace.h"
#include "dsp/RoleDetector.h"
#include "session/BalanceEngine.h"

using namespace mu;

static int failures = 0;
static void check (bool ok, const char* name, const juce::String& detail = {})
{
    std::printf ("%s  %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.isEmpty() ? "" : "  -- ", detail.toRawUTF8());
    if (! ok) ++failures;
}
static bool near (float a, float b, float tol) { return std::abs (a - b) <= tol; }

static juce::AudioBuffer<float> sine (double fs, float hz, float ampl, int numSamples, int channels = 2)
{
    juce::AudioBuffer<float> b (channels, numSamples);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < numSamples; ++i)
            b.setSample (c, i, ampl * std::sin (2.0f * juce::MathConstants<float>::pi * hz * (float) i / (float) fs));
    return b;
}

static void feedInBlocks (LoudnessMeter& m, const juce::AudioBuffer<float>& src, int block)
{
    for (int pos = 0; pos < src.getNumSamples(); pos += block)
    {
        const int len = juce::jmin (block, src.getNumSamples() - pos);
        juce::AudioBuffer<float> view (src.getNumChannels(), len);
        for (int c = 0; c < src.getNumChannels(); ++c) view.copyFrom (c, 0, src, c, pos, len);
        m.process (view);
    }
}

int main()
{
    const double fs = 48000.0;

    // --- Loudness calibration: stereo -20 dBFS 997 Hz sine must read -20.0 LUFS (BS.1770).
    {
        LoudnessMeter m;
        m.prepare (fs, 512, 2);
        auto s = sine (fs, 997.0f, 0.1f, (int) fs * 5);
        feedInBlocks (m, s, 512);
        m.readIntegrated();
        check (near (m.getMomentary(), -20.0f, 0.15f), "LUFS momentary calibration", juce::String (m.getMomentary(), 2));
        check (near (m.getShortTerm(), -20.0f, 0.15f), "LUFS short-term calibration", juce::String (m.getShortTerm(), 2));
        check (near (m.getIntegrated(), -20.0f, 0.2f), "LUFS integrated calibration", juce::String (m.getIntegrated(), 2));
        check (near (m.getTruePeakHold(), -20.0f, 0.3f), "True peak of -20 dBFS sine", juce::String (m.getTruePeakHold(), 2));
        check (near (m.getCrestFactorDb(), 3.01f, 0.2f), "Crest factor of a sine", juce::String (m.getCrestFactorDb(), 2));
    }
    // --- Integrated gating: 2 s of -20 LUFS followed by 2 s of silence still reads -20 (silence is gated out).
    {
        LoudnessMeter m;
        m.prepare (fs, 256, 2);
        auto s = sine (fs, 997.0f, 0.1f, (int) fs * 2);
        feedInBlocks (m, s, 256);
        juce::AudioBuffer<float> silence (2, (int) fs * 2);
        silence.clear();
        feedInBlocks (m, silence, 256);
        m.readIntegrated();
        check (near (m.getIntegrated(), -20.0f, 0.5f), "Integrated gating ignores silence (edge blocks count, as per BS.1770)", juce::String (m.getIntegrated(), 2));
        check (m.getMomentary() < -90.0f, "Momentary drops on silence", juce::String (m.getMomentary(), 1));
    }
    // --- True peak catches inter-sample peaks: 0 dBFS sine near Nyquist/4 with phase offset.
    {
        LoudnessMeter m;
        m.prepare (fs, 512, 2);
        auto s = sine (fs, 11025.0f, 1.0f, 8192);
        for (int i = 0; i < 2048; ++i)   // fade in so the onset itself does not ring
            for (int c = 0; c < 2; ++c) s.setSample (c, i, s.getSample (c, i) * (float) i / 2048.0f);
        feedInBlocks (m, s, 512);
        check (m.getTruePeakHold() > -0.5f && m.getTruePeakHold() < 0.6f, "True peak of 0 dBFS sine", juce::String (m.getTruePeakHold(), 2));
    }

    // --- EQ: bell +6 dB at 1 kHz Q 1.0
    {
        EQEngine eq;
        eq.prepare (fs, 512, 2);
        eq.setBand (0, { true, EQType::Bell, 1000.0f, 6.0f, 1.0f });
        check (near (eq.magnitudeDbAt (1000.0f), 6.0f, 0.05f), "Bell gain at centre", juce::String (eq.magnitudeDbAt (1000.0f), 2));
        check (near (eq.magnitudeDbAt (50.0f), 0.0f, 0.1f), "Bell gain far below", juce::String (eq.magnitudeDbAt (50.0f), 2));
        check (near (eq.magnitudeDbAt (18000.0f), 0.0f, 0.15f), "Bell gain far above", juce::String (eq.magnitudeDbAt (18000.0f), 2));

        // process a 1 kHz sine, measure the steady-state gain
        auto s = sine (fs, 1000.0f, 0.1f, (int) fs);
        for (int pos = 0; pos < s.getNumSamples(); pos += 512)
        {
            const int len = juce::jmin (512, s.getNumSamples() - pos);
            juce::AudioBuffer<float> view (s.getArrayOfWritePointers(), 2, pos, len);
            eq.process (view);
        }
        const float mag = s.getMagnitude (0, (int) fs / 2, (int) fs / 2);
        check (near (juce::Decibels::gainToDecibels (mag / 0.1f), 6.0f, 0.1f), "Bell processed gain", juce::String (juce::Decibels::gainToDecibels (mag / 0.1f), 2));

        eq.setBand (1, { true, EQType::LowCut, 100.0f, 0.0f, 0.71f });
        check (near (eq.bandMagnitudeDbAt (1, 100.0f), -3.0f, 0.2f), "Low cut -3 dB at cutoff", juce::String (eq.bandMagnitudeDbAt (1, 100.0f), 2));
        check (eq.bandMagnitudeDbAt (1, 25.0f) < -20.0f, "Low cut slope", juce::String (eq.bandMagnitudeDbAt (1, 25.0f), 2));
        eq.setBand (2, { true, EQType::HighShelf, 8000.0f, -4.0f, 0.71f });
        check (near (eq.bandMagnitudeDbAt (2, 19000.0f), -4.0f, 0.3f), "High shelf plateau", juce::String (eq.bandMagnitudeDbAt (2, 19000.0f), 2));

        // NaN safety
        juce::AudioBuffer<float> bad (2, 256);
        bad.clear();
        bad.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
        eq.process (bad);
        bool finite = true;
        for (int i = 0; i < 256; ++i) finite = finite && std::isfinite (bad.getSample (0, i));
        check (finite, "EQ scrubs NaN");
    }

    // --- Compressor static curve and processing
    {
        check (near (Compressor::computeGainDb (-6.0f, -18.0f, 4.0f, 0.0f), -9.0f, 0.01f), "Hard-knee gain computer");
        check (near (Compressor::computeGainDb (-30.0f, -18.0f, 4.0f, 6.0f), 0.0f, 0.01f), "Below knee is unity");
        check (Compressor::computeGainDb (-18.0f, -18.0f, 4.0f, 6.0f) < 0.0f, "Soft knee starts before threshold");

        Compressor c;
        c.prepare (fs, 512, 2);
        CompressorSettings st; st.enabled = true; st.thresholdDb = -20.0f; st.ratio = 4.0f; st.attackMs = 1.0f; st.releaseMs = 50.0f; st.kneeDb = 0.0f; st.autoMakeup = false; st.makeupDb = 0.0f; st.mix = 1.0f;
        c.setSettings (st);
        auto s = sine (fs, 1000.0f, 1.0f, (int) fs);   // 0 dBFS peak -> ~ -3 dB RMS; detector = max(peak, rms) = peak 0 dB
        for (int pos = 0; pos < s.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> view (s.getArrayOfWritePointers(), 2, pos, juce::jmin (512, s.getNumSamples() - pos));
            c.process (view);
        }
        // 20 dB over threshold at 4:1 -> 15 dB reduction
        check (near (c.getGainReductionDb(), -15.0f, 1.0f), "Compressor gain reduction", juce::String (c.getGainReductionDb(), 2));
        const float outMag = s.getMagnitude (0, (int) fs / 2, (int) fs / 2);
        check (near (juce::Decibels::gainToDecibels (outMag), -15.0f, 1.0f), "Compressor output level", juce::String (juce::Decibels::gainToDecibels (outMag), 2));
    }

    // --- Vocal space: with key 1 and amount 1 the 2.6 kHz band dips; with key 0 it is transparent.
    {
        VocalSpace v;
        v.prepare (fs, 512, 2);
        v.setSettings ({ true, 1.0f, 2 });
        v.setKeyLevel (1.0f);
        auto s = sine (fs, 2600.0f, 0.1f, (int) fs);
        for (int pos = 0; pos < s.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> view (s.getArrayOfWritePointers(), 2, pos, juce::jmin (512, s.getNumSamples() - pos));
            v.process (view);
        }
        const float g = juce::Decibels::gainToDecibels (s.getMagnitude (0, (int) fs / 2, (int) fs / 2) / 0.1f);
        check (near (g, -4.5f, 0.4f), "Vocal space ducks presence band", juce::String (g, 2));

        VocalSpace v2;
        v2.prepare (fs, 512, 2);
        v2.setSettings ({ true, 1.0f, 2 });
        v2.setKeyLevel (0.0f);
        auto s2 = sine (fs, 2600.0f, 0.1f, (int) fs);
        for (int pos = 0; pos < s2.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> view (s2.getArrayOfWritePointers(), 2, pos, juce::jmin (512, s2.getNumSamples() - pos));
            v2.process (view);
        }
        const float g2 = juce::Decibels::gainToDecibels (s2.getMagnitude (0, (int) fs / 2, (int) fs / 2) / 0.1f);
        check (near (g2, 0.0f, 0.05f), "Vocal space transparent without key", juce::String (g2, 2));
    }

    // --- Role detector
    {
        RoleFeatures bass; bass.valid = true; bass.lowEnergyRatio = 0.8f; bass.centroidHz = 120.0f; bass.crestFactorDb = 8.0f;
        check (detectRole (bass).role == Role::Bass808, "Detects 808/bass");
        RoleFeatures kick = bass; kick.crestFactorDb = 18.0f;
        check (detectRole (kick).role == Role::Kick, "Detects kick");
        RoleFeatures voc; voc.valid = true; voc.lowEnergyRatio = 0.03f; voc.centroidHz = 1600.0f; voc.crestFactorDb = 12.0f; voc.highEnergyRatio = 0.15f;
        check (detectRole (voc).role == Role::Vocal, "Detects vocal");
        RoleFeatures beat; beat.valid = true; beat.lowEnergyRatio = 0.35f; beat.centroidHz = 900.0f; beat.crestFactorDb = 11.0f; beat.highEnergyRatio = 0.1f;
        check (detectRole (beat).role == Role::Beat, "Detects full beat");
    }

    // --- Balance engine
    {
        std::vector<TrackInfo> tracks;
        auto mk = [] (int slot, const char* name, Role r, float lufs, float tp) { TrackInfo t; t.slot = slot; t.name = name; t.role = r; t.levelLufs = lufs; t.truePeakDb = tp; t.active = true; return t; };
        tracks.push_back (mk (0, "Vocal", Role::Vocal, -20.0f, -8.0f));
        tracks.push_back (mk (1, "Beat", Role::Beat, -14.0f, -4.0f));
        tracks.push_back (mk (2, "808", Role::Bass808, -24.0f, -10.0f));
        tracks.push_back (mk (3, "Master", Role::Master, -12.0f, -1.0f));
        BalanceTargets tg; tg.masterLufs = -18.0f; tg.masterTruePeak = -6.0f;
        auto plan = computeBalance (tracks, tg);
        check (plan.usable, "Plan usable", plan.summary);
        float dVocal = 0, dBeat = 0, d808 = 0;
        for (auto& m : plan.moves) { if (m.slot == 0) dVocal = m.deltaDb; if (m.slot == 1) dBeat = m.deltaDb; if (m.slot == 2) d808 = m.deltaDb; }
        check (dBeat < dVocal, "Beat pulled down relative to vocal", "vocal " + juce::String (dVocal, 1) + " beat " + juce::String (dBeat, 1));
        check (d808 > dBeat, "808 brought up relative to beat", juce::String (d808, 1));
        // after the moves the relative spacing matches the role table: beat = vocal - 1.5, 808 = vocal - 2.5
        check (near ((-14.0f + dBeat) - (-20.0f + dVocal), -1.5f, 0.05f), "Beat sits 1.5 LU under vocal");
        check (near ((-24.0f + d808) - (-20.0f + dVocal), -2.5f, 0.05f), "808 sits 2.5 LU under vocal");
        check (plan.projectedMasterLufs <= -18.0f + 0.3f, "Projected master at or under target", juce::String (plan.projectedMasterLufs, 2));
        check (plan.projectedMasterTruePeak <= -6.0f + 0.3f, "Projected true peak under ceiling", juce::String (plan.projectedMasterTruePeak, 2));
        check (plan.moves.size() == 3, "Master never gets a move");

        // locked track stays put
        tracks[1].acceptsBalance = false;
        auto plan2 = computeBalance (tracks, tg);
        for (auto& m : plan2.moves) if (m.slot == 1) check (m.deltaDb == 0.0f, "Locked track untouched");

        // empty session
        auto plan3 = computeBalance ({}, tg);
        check (! plan3.usable, "Empty session not usable");
    }

    // --- Masking: a beat sitting on the vocal's presence region gets flagged, beat is the one to give way.
    {
        std::array<float, kAnalysisBands> centres {};
        for (int b = 0; b < kAnalysisBands; ++b) centres[(size_t) b] = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) kAnalysisBands);
        TrackInfo voc; voc.slot = 0; voc.name = "Vocal"; voc.role = Role::Vocal; voc.levelLufs = -20.0f; voc.active = true; voc.hasBands = true;
        TrackInfo beat; beat.slot = 1; beat.name = "Beat"; beat.role = Role::Beat; beat.levelLufs = -16.0f; beat.active = true; beat.hasBands = true;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float hz = centres[(size_t) b];
            voc.bandsDb[(size_t) b] = (hz > 800.0f && hz < 4000.0f) ? -20.0f : -45.0f;
            beat.bandsDb[(size_t) b] = (hz > 1500.0f && hz < 3500.0f) ? -18.0f : -40.0f;
        }
        auto clashes = findMasking ({ voc, beat }, centres);
        check (! clashes.empty(), "Masking clash found");
        if (! clashes.empty())
        {
            check (clashes[0].slotB == 1, "Beat gives way to vocal");
            check (clashes[0].freqHz > 1200.0f && clashes[0].freqHz < 4000.0f, "Clash in presence region", juce::String (clashes[0].freqHz, 0));
            check (clashes[0].suggestedCutDb < 0.0f && clashes[0].suggestedCutDb >= -4.0f, "Cut is modest", juce::String (clashes[0].suggestedCutDb, 1));
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
