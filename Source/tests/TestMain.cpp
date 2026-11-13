// Console test runner for the DSP and balance engine. Build target: MUTests.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>
#include "dsp/LoudnessMeter.h"
#include "dsp/EQEngine.h"
#include "dsp/Compressor.h"
#include "dsp/Gate.h"
#include "dsp/DeEsser.h"
#include "dsp/Saturator.h"
#include "dsp/Rider.h"
#include "dsp/Smoother.h"
#include "dsp/StereoTool.h"
#include "dsp/Maximizer.h"
#include "dsp/TruePeak.h"
#include "dsp/SpectrumAnalyzer.h"
#include "analysis/TonalAnalysis.h"
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
        const EQBandSettings bell { true, EQType::Bell, 1000.0f, 6.0f, 1.0f };
        auto resp = [fs] (const EQBandSettings& b, float hz) { return EQEngine::bandResponseDb (b, hz, fs); };
        eq.setBand (0, bell);
        check (near (resp (bell, 1000.0f), 6.0f, 0.05f), "Bell gain at centre", juce::String (resp (bell, 1000.0f), 2));
        check (near (resp (bell, 50.0f), 0.0f, 0.1f), "Bell gain far below", juce::String (resp (bell, 50.0f), 2));
        check (near (resp (bell, 18000.0f), 0.0f, 0.15f), "Bell gain far above", juce::String (resp (bell, 18000.0f), 2));

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

        EQBandSettings lc { true, EQType::LowCut, 100.0f, 0.0f, 0.71f };
        check (near (resp (lc, 100.0f), -3.0f, 0.2f), "Low cut -3 dB at cutoff", juce::String (resp (lc, 100.0f), 2));
        check (near (resp (lc, 25.0f), -24.0f, 1.0f), "Low cut 12 dB/oct slope", juce::String (resp (lc, 25.0f), 2));
        lc.slope = 1;
        check (near (resp (lc, 100.0f), -3.0f, 0.3f), "24 dB/oct cut is -3 dB at cutoff", juce::String (resp (lc, 100.0f), 2));
        check (near (resp (lc, 50.0f), -24.0f, 1.0f), "24 dB/oct cut slope", juce::String (resp (lc, 50.0f), 2));
        lc.slope = 3;
        check (near (resp (lc, 100.0f), -3.0f, 0.3f), "48 dB/oct cut is -3 dB at cutoff", juce::String (resp (lc, 100.0f), 2));
        check (near (resp (lc, 50.0f), -48.0f, 1.5f), "48 dB/oct cut slope", juce::String (resp (lc, 50.0f), 2));
        EQBandSettings hc { true, EQType::HighCut, 2000.0f, 0.0f, 0.71f }; hc.slope = 1;
        check (near (resp (hc, 4000.0f), -24.0f, 1.5f), "24 dB/oct high cut slope", juce::String (resp (hc, 4000.0f), 2));
        const EQBandSettings hs { true, EQType::HighShelf, 8000.0f, -4.0f, 0.71f };
        check (near (resp (hs, 19000.0f), -4.0f, 0.3f), "High shelf plateau", juce::String (resp (hs, 19000.0f), 2));
        EQBandSettings off = bell; off.enabled = false;
        check (resp (off, 1000.0f) == 0.0f, "A disabled band has a flat response");
        eq.setBand (1, lc);

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


    // --- helper to run a processor over a buffer in blocks
    auto runBlocks = [] (auto& proc, juce::AudioBuffer<float>& s, int block)
    {
        for (int pos = 0; pos < s.getNumSamples(); pos += block)
        {
            juce::AudioBuffer<float> view (s.getArrayOfWritePointers(), s.getNumChannels(), pos, juce::jmin (block, s.getNumSamples() - pos));
            proc.process (view);
        }
    };
    auto tailDb = [&] (const juce::AudioBuffer<float>& s, float ref) { return juce::Decibels::gainToDecibels (s.getMagnitude (0, s.getNumSamples() / 2, s.getNumSamples() / 2) / ref); };

    // --- Gate
    {
        Gate g; g.prepare (fs, 512, 2);
        g.setSettings ({ true, -40.0f, 20.0f, 1.0f, 50.0f, 10.0f });
        auto quiet = sine (fs, 1000.0f, 0.001f, (int) fs);   // -60 dBFS
        runBlocks (g, quiet, 512);
        check (near (tailDb (quiet, 0.001f), -20.0f, 1.0f), "Gate closes by its range on quiet input", juce::String (tailDb (quiet, 0.001f), 2));
        Gate g2; g2.prepare (fs, 512, 2);
        g2.setSettings ({ true, -40.0f, 20.0f, 1.0f, 50.0f, 10.0f });
        auto loud = sine (fs, 1000.0f, 0.3f, (int) fs);
        runBlocks (g2, loud, 512);
        check (near (tailDb (loud, 0.3f), 0.0f, 0.05f), "Gate is open on loud input", juce::String (tailDb (loud, 0.3f), 2));
    }

    // --- De-esser
    {
        DeEsser d; d.prepare (fs, 512, 2);
        d.setSettings ({ true, 7000.0f, -30.0f, 6.0f, true });
        auto ess = sine (fs, 7000.0f, 0.3f, (int) fs);
        runBlocks (d, ess, 512);
        check (near (tailDb (ess, 0.3f), -6.0f, 1.0f), "De-esser pulls a loud ess by its range", juce::String (tailDb (ess, 0.3f), 2));
        DeEsser d2; d2.prepare (fs, 512, 2);
        d2.setSettings ({ true, 7000.0f, -30.0f, 6.0f, true });
        auto body = sine (fs, 300.0f, 0.3f, (int) fs);
        runBlocks (d2, body, 512);
        check (near (tailDb (body, 0.3f), 0.0f, 0.3f), "De-esser leaves the body alone", juce::String (tailDb (body, 0.3f), 2));
    }

    // --- Saturator: level compensation, harmonics, and a latency-matched dry path
    {
        Saturator sat; sat.prepare (fs, 512, 2);
        sat.setSettings ({ true, 12.0f, 1.0f, 0 });
        auto s = sine (fs, 1000.0f, 0.25f, (int) fs);
        runBlocks (sat, s, 512);
        check (near (tailDb (s, 0.25f), 0.0f, 2.0f), "Saturator stays level-compensated at -12 dBFS", juce::String (tailDb (s, 0.25f), 2));

        // harmonics: compare the processed wave against a pure sine fit -> residual must be clearly non-zero
        auto ref = sine (fs, 1000.0f, 0.25f, (int) fs);
        Saturator dry; dry.prepare (fs, 512, 2);
        dry.setSettings ({ false, 12.0f, 1.0f, 0 });
        runBlocks (dry, ref, 512);
        double resid = 0.0, total = 0.0;
        for (int i = (int) fs / 2; i < (int) fs; ++i) { const double e = s.getSample (0, i) - ref.getSample (0, i); resid += e * e; total += ref.getSample (0, i) * ref.getSample (0, i); }
        check (resid / total > 0.001, "Saturator adds harmonics", juce::String (10.0 * std::log10 (resid / total), 1) + " dB residual");

        // disabled: output equals input delayed by the reported latency
        Saturator off; off.prepare (fs, 256, 2);
        off.setSettings ({ false, 6.0f, 0.5f, 0 });
        auto in = sine (fs, 500.0f, 0.5f, 8192);
        auto out = in;
        runBlocks (off, out, 256);
        const int lat = off.getLatencySamples();
        float maxErr = 0.0f;
        for (int i = 4096; i < 8000; ++i) maxErr = juce::jmax (maxErr, std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        check (maxErr < 0.01f, "Saturator bypass is a clean delay of the reported latency", "latency " + juce::String (lat) + " samples, err " + juce::String (maxErr, 4));
    }

    // --- Rider
    {
        Rider r; r.prepare (fs, 512, 2);
        r.setSettings ({ true, -20.0f, 6.0f, 2 });
        auto s = sine (fs, 1000.0f, 0.0316f, (int) fs * 3);   // -30 dBFS peak, ~-33 dB RMS -> wants +6 (range limited)
        runBlocks (r, s, 512);
        check (near (r.getCurrentGainDb(), 6.0f, 0.3f), "Rider lifts a quiet track by up to its range", juce::String (r.getCurrentGainDb(), 2));
        Rider r2; r2.prepare (fs, 512, 2);
        r2.setSettings ({ true, -20.0f, 6.0f, 2 });
        auto hot = sine (fs, 1000.0f, 0.5f, (int) fs * 3);    // -6 dBFS peak -> rider pulls down
        runBlocks (r2, hot, 512);
        check (near (r2.getCurrentGainDb(), -6.0f, 0.3f), "Rider pulls a hot track down by up to its range", juce::String (r2.getCurrentGainDb(), 2));
    }

    // --- Dynamic EQ band
    {
        EQEngine eq; eq.prepare (fs, 512, 2);
        EQBandSettings b; b.enabled = true; b.type = EQType::Bell; b.freqHz = 1000.0f; b.gainDb = 0.0f; b.q = 1.0f; b.dynamic = true; b.thresholdDb = -40.0f; b.rangeDb = -6.0f;
        eq.setBand (0, b);
        auto loud = sine (fs, 1000.0f, 0.3f, (int) fs);
        runBlocks (eq, loud, 512);
        check (near (tailDb (loud, 0.3f), -6.0f, 0.8f), "Dynamic band cuts when its region is loud", juce::String (tailDb (loud, 0.3f), 2));
        EQEngine eq2; eq2.prepare (fs, 512, 2);
        eq2.setBand (0, b);
        auto quiet = sine (fs, 1000.0f, 0.001f, (int) fs);
        runBlocks (eq2, quiet, 512);
        check (near (tailDb (quiet, 0.001f), 0.0f, 0.3f), "Dynamic band rests when its region is quiet", juce::String (tailDb (quiet, 0.001f), 2));
        check (near (EQEngine::bandResponseDb (b, 1000.0f, fs), 0.0f, 0.01f), "Dynamic band static curve stays at its set gain");
        check (near (eq.getBandDynamicOffsetDb (0), -6.0f, 0.5f), "Dynamic band reports its live offset", juce::String (eq.getBandDynamicOffsetDb (0), 2));
    }

    // --- Balance with edited role targets
    {
        std::vector<TrackInfo> tracks;
        auto mk = [] (int slot, const char* name, Role r, float lufs) { TrackInfo t; t.slot = slot; t.name = name; t.role = r; t.levelLufs = lufs; t.truePeakDb = -8.0f; t.active = true; return t; };
        tracks.push_back (mk (0, "Vocal", Role::Vocal, -20.0f));
        tracks.push_back (mk (1, "Beat", Role::Beat, -16.0f));
        BalanceTargets tg; tg.offsets[(size_t) Role::Beat] = -4.0f;
        auto plan = computeBalance (tracks, tg);
        float dv = 0, db = 0;
        for (auto& m : plan.moves) { if (m.slot == 0) dv = m.deltaDb; if (m.slot == 1) db = m.deltaDb; }
        check (near ((-16.0f + db) - (-20.0f + dv), -4.0f, 0.05f), "Edited role target is honoured", juce::String ((-16.0f + db) - (-20.0f + dv), 2));
    }


    // --- deterministic noise for the broadband tests
    auto noise = [] (int numSamples, float ampl, bool sameBothSides = true)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        juce::Random rng (1234);
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = (rng.nextFloat() * 2.0f - 1.0f) * ampl;
            b.setSample (0, i, l);
            b.setSample (1, i, sameBothSides ? l : (rng.nextFloat() * 2.0f - 1.0f) * ampl);
        }
        return b;
    };
    auto rmsDb = [] (const juce::AudioBuffer<float>& s, int ch = 0) { return juce::Decibels::gainToDecibels (s.getRMSLevel (ch, s.getNumSamples() / 2, s.getNumSamples() / 2), -120.0f); };
    // level of one frequency in a buffer (Goertzel-style correlation over the second half)
    auto toneDb = [fs] (const juce::AudioBuffer<float>& s, float hz, int ch = 0)
    {
        double re = 0.0, im = 0.0; const int start = s.getNumSamples() / 2, len = s.getNumSamples() - start;
        for (int i = 0; i < len; ++i)
        {
            const double ph = 2.0 * juce::MathConstants<double>::pi * hz * (double) (start + i) / fs;
            re += s.getSample (ch, start + i) * std::cos (ph); im += s.getSample (ch, start + i) * std::sin (ph);
        }
        return (float) (20.0 * std::log10 (juce::jmax (1.0e-9, 2.0 * std::sqrt (re * re + im * im) / (double) len)));
    };

    // --- EQ mid/side: a side-only cut leaves mono material alone and cuts what differs between the speakers
    {
        EQBandSettings b { true, EQType::Bell, 1000.0f, -12.0f, 1.0f }; b.mode = ChannelMode::Side;
        EQEngine eq; eq.prepare (fs, 512, 2); eq.setBand (2, b);
        auto mono = sine (fs, 1000.0f, 0.25f, (int) fs);
        runBlocks (eq, mono, 512);
        check (near (tailDb (mono, 0.25f), 0.0f, 0.1f), "Side band leaves centred material untouched", juce::String (tailDb (mono, 0.25f), 2));
        EQEngine eq2; eq2.prepare (fs, 512, 2); eq2.setBand (2, b);
        auto side = sine (fs, 1000.0f, 0.25f, (int) fs);
        side.applyGain (1, 0, side.getNumSamples(), -1.0f);
        runBlocks (eq2, side, 512);
        check (near (tailDb (side, 0.25f), -12.0f, 0.3f), "Side band cuts side material", juce::String (tailDb (side, 0.25f), 2));
        b.mode = ChannelMode::Mid;
        EQEngine eq3; eq3.prepare (fs, 512, 2); eq3.setBand (2, b);
        auto side2 = sine (fs, 1000.0f, 0.25f, (int) fs);
        side2.applyGain (1, 0, side2.getNumSamples(), -1.0f);
        runBlocks (eq3, side2, 512);
        check (near (tailDb (side2, 0.25f), 0.0f, 0.1f), "Mid band leaves side material untouched", juce::String (tailDb (side2, 0.25f), 2));
    }

    // --- EQ auto gain: a big boost should not make the track louder overall
    {
        EQEngine eq; eq.prepare (fs, 512, 2);
        eq.setBand (1, { true, EQType::HighShelf, 1000.0f, 9.0f, 0.71f });
        eq.setAutoGain (true);
        auto n = noise ((int) fs * 6, 0.2f);
        const float before = rmsDb (n);
        runBlocks (eq, n, 512);
        check (near (rmsDb (n), before, 1.0f), "Auto gain keeps the level after a +9 dB shelf", juce::String (rmsDb (n) - before, 2) + " dB change, trim " + juce::String (eq.getAutoGainDb(), 2));
        EQEngine eq2; eq2.prepare (fs, 512, 2);
        eq2.setBand (1, { true, EQType::HighShelf, 1000.0f, 9.0f, 0.71f });
        auto n2 = noise ((int) fs * 6, 0.2f);
        runBlocks (eq2, n2, 512);
        check (rmsDb (n2) - before > 3.0f, "Without auto gain the same shelf is clearly louder", juce::String (rmsDb (n2) - before, 2));
    }

    // --- EQ band solo
    {
        EQEngine eq; eq.prepare (fs, 512, 2);
        eq.setBand (3, { true, EQType::Bell, 1000.0f, 3.0f, 2.0f });
        eq.setSoloBand (3);
        auto low = sine (fs, 60.0f, 0.25f, (int) fs);
        runBlocks (eq, low, 512);
        check (tailDb (low, 0.25f) < -20.0f, "Band solo removes what is outside the band", juce::String (tailDb (low, 0.25f), 2));
        EQEngine eq2; eq2.prepare (fs, 512, 2);
        eq2.setBand (3, { true, EQType::Bell, 1000.0f, 3.0f, 2.0f });
        eq2.setSoloBand (3);
        auto mid = sine (fs, 1000.0f, 0.25f, (int) fs);
        runBlocks (eq2, mid, 512);
        check (tailDb (mid, 0.25f) > -3.0f, "Band solo keeps what is inside the band", juce::String (tailDb (mid, 0.25f), 2));
    }

    // --- Compressor styles and sidechain high-pass
    {
        const char* names[] = { "Clean", "Punch", "Smooth", "Glue" };
        for (int style = 0; style < 4; ++style)
        {
            Compressor c; c.prepare (fs, 512, 2);
            CompressorSettings st; st.enabled = true; st.thresholdDb = -20.0f; st.ratio = 4.0f; st.attackMs = 5.0f; st.releaseMs = 80.0f; st.kneeDb = 0.0f; st.autoMakeup = false; st.style = (CompStyle) style;
            c.setSettings (st);
            auto s = sine (fs, 1000.0f, 1.0f, (int) fs * 2);
            runBlocks (c, s, 512);
            // steady sine 20 dB over threshold at 4:1 -> 15 dB down whichever detector is used
            check (near (c.getGainReductionDb(), -15.0f, 1.5f), (juce::String (names[style]) + " style steady-state reduction").toRawUTF8(), juce::String (c.getGainReductionDb(), 2));
            bool finite = true;
            for (int i = 0; i < s.getNumSamples(); ++i) finite = finite && std::isfinite (s.getSample (0, i));
            check (finite, (juce::String (names[style]) + " style output is finite").toRawUTF8());
        }
        Compressor a; a.prepare (fs, 512, 2);
        CompressorSettings st; st.enabled = true; st.thresholdDb = -20.0f; st.ratio = 4.0f; st.attackMs = 5.0f; st.releaseMs = 80.0f; st.kneeDb = 0.0f; st.autoMakeup = false; st.style = CompStyle::Glue;
        a.setSettings (st);
        auto sub = sine (fs, 50.0f, 1.0f, (int) fs * 2);
        runBlocks (a, sub, 512);
        const float open = a.getGainReductionDb();
        Compressor b2; b2.prepare (fs, 512, 2);
        st.sidechainHpHz = 200.0f; b2.setSettings (st);
        auto sub2 = sine (fs, 50.0f, 1.0f, (int) fs * 2);
        runBlocks (b2, sub2, 512);
        check (open < -12.0f && b2.getGainReductionDb() > -3.0f, "Sidechain high-pass stops bass from driving the compressor", juce::String (open, 1) + " dB without, " + juce::String (b2.getGainReductionDb(), 1) + " dB with");
        check (a.reductionHistory().get (0) < -10.0f, "Compressor publishes its reduction history", juce::String (a.reductionHistory().get (0), 1));
    }

    // --- Smooth: pulls down a ringing resonance, leaves even material alone
    {
        Smoother sm; sm.prepare (fs, 512, 2);
        SmootherSettings st; st.enabled = true; st.depth = 0.6f; st.focus = 0; st.speed = 1;
        sm.setSettings (st);
        auto n = noise ((int) fs * 4, 0.05f);
        const auto tone = sine (fs, 3200.0f, 0.2f, (int) fs * 4);
        for (int c = 0; c < 2; ++c) n.addFrom (c, 0, tone, c, 0, n.getNumSamples());
        auto untouched = n;
        runBlocks (sm, n, 512);
        const float cut = toneDb (n, 3200.0f) - toneDb (untouched, 3200.0f);
        check (cut < -3.0f, "Smooth pulls down a resonance that sticks out", juce::String (cut, 2) + " dB at 3.2 kHz");
        const float elsewhere = toneDb (n, 500.0f) - toneDb (untouched, 500.0f);
        check (std::abs (elsewhere) < 1.5f, "Smooth leaves the rest of the spectrum alone", juce::String (elsewhere, 2) + " dB at 500 Hz");

        Smoother sm2; sm2.prepare (fs, 512, 2); sm2.setSettings (st);
        auto flat = noise ((int) fs * 4, 0.1f);
        const float before = rmsDb (flat);
        runBlocks (sm2, flat, 512);
        check (near (rmsDb (flat), before, 0.6f), "Smooth is close to transparent on even material", juce::String (rmsDb (flat) - before, 2) + " dB");

        Smoother sm3; sm3.prepare (fs, 512, 2);
        st.enabled = false; sm3.setSettings (st);
        auto same = noise (4096, 0.1f); const auto ref = same;
        runBlocks (sm3, same, 512);
        float err = 0.0f;
        for (int i = 0; i < 4096; ++i) err = juce::jmax (err, std::abs (same.getSample (0, i) - ref.getSample (0, i)));
        check (err == 0.0f, "Smooth switched off is bit-exact");
    }

    // --- Stereo tool
    {
        StereoTool st; st.prepare (fs, 512, 2);
        st.setSettings ({ 1.0f, false, 120.0f });
        auto n = noise (8192, 0.2f, false); const auto ref = n;
        runBlocks (st, n, 512);
        float err = 0.0f;
        for (int i = 0; i < 8192; ++i) err = juce::jmax (err, std::abs (n.getSample (1, i) - ref.getSample (1, i)));
        check (err < 1.0e-5f, "Stereo tool at 100 % is transparent", juce::String (err, 6));

        StereoTool mono; mono.prepare (fs, 512, 2);
        mono.setSettings ({ 0.0f, false, 120.0f });
        auto n2 = noise ((int) fs, 0.2f, false);
        runBlocks (mono, n2, 512);
        float diff = 0.0f;
        for (int i = (int) fs / 2; i < (int) fs; ++i) diff = juce::jmax (diff, std::abs (n2.getSample (0, i) - n2.getSample (1, i)));
        check (diff < 1.0e-4f, "Width 0 % is mono", juce::String (diff, 6));

        auto sideTone = [&] (float hz) { auto s = sine (fs, hz, 0.25f, (int) fs); s.applyGain (1, 0, s.getNumSamples(), -1.0f); return s; };
        StereoTool mb; mb.prepare (fs, 512, 2); mb.setSettings ({ 1.0f, true, 120.0f });
        auto low = sideTone (50.0f);
        runBlocks (mb, low, 512);
        check (tailDb (low, 0.25f) < -12.0f, "Mono bass removes side information below the corner", juce::String (tailDb (low, 0.25f), 2));
        StereoTool mb2; mb2.prepare (fs, 512, 2); mb2.setSettings ({ 1.0f, true, 120.0f });
        auto high = sideTone (2000.0f);
        runBlocks (mb2, high, 512);
        check (near (tailDb (high, 0.25f), 0.0f, 0.3f), "Mono bass keeps the stereo image above the corner", juce::String (tailDb (high, 0.25f), 2));
        StereoTool corr; corr.prepare (fs, 512, 2); corr.setSettings ({ 1.0f, false, 120.0f });
        auto anti = sideTone (500.0f);
        runBlocks (corr, anti, 512);
        check (corr.getCorrelation() < -0.9f, "Correlation meter reads out-of-phase material", juce::String (corr.getCorrelation(), 2));
    }

    // --- True peak detector: a sine at fs/4 sampled 45 degrees off its crest hides 3 dB between the samples
    {
        TruePeakDetector tp;
        float samplePeak = 0.0f, truePeak = 0.0f;
        for (int i = 0; i < 4096; ++i)
        {
            const float x = std::sin (juce::MathConstants<float>::halfPi * (float) i + juce::MathConstants<float>::pi * 0.25f) * juce::jmin (1.0f, (float) i / 1024.0f);
            samplePeak = juce::jmax (samplePeak, std::abs (x));
            truePeak = juce::jmax (truePeak, tp.process (x));
        }
        check (near (juce::Decibels::gainToDecibels (samplePeak), -3.01f, 0.1f) && near (juce::Decibels::gainToDecibels (truePeak), 0.0f, 0.35f),
               "Inter-sample peak detection", "sample " + juce::String (juce::Decibels::gainToDecibels (samplePeak), 2) + " dB, true " + juce::String (juce::Decibels::gainToDecibels (truePeak), 2) + " dBTP");
    }

    // --- Maximizer
    {
        auto measureTp = [&] (const juce::AudioBuffer<float>& s)
        {
            LoudnessMeter m; m.prepare (fs, 512, 2);
            feedInBlocks (m, s, 512);
            return m.getTruePeakHold();
        };
        // 1. hot sine pushed 9 dB into a -1 dBTP ceiling
        Maximizer mx; mx.prepare (fs, 512, 2);
        mx.setActive (true); mx.setEnabled (true); mx.setCeilingDb (-1.0f); mx.setGainDb (9.0f);
        auto s = sine (fs, 997.0f, 0.8f, (int) fs * 2);
        runBlocks (mx, s, 512);
        check (measureTp (s) <= -1.0f + 0.05f, "Maximizer holds the true-peak ceiling on a hot sine", juce::String (measureTp (s), 2) + " dBTP");
        check (mx.getGainReductionDb() < -6.0f, "Maximizer reports its reduction", juce::String (mx.getGainReductionDb(), 2));

        // 2. worst case for inter-sample peaks: fs/4 at 45 degrees, plus drums-like bursts of noise
        Maximizer mx2; mx2.prepare (fs, 512, 2);
        mx2.setActive (true); mx2.setEnabled (true); mx2.setCeilingDb (-1.0f); mx2.setGainDb (6.0f);
        juce::AudioBuffer<float> hard (2, (int) fs * 2);
        juce::Random rng (99);
        for (int i = 0; i < hard.getNumSamples(); ++i)
        {
            const bool burst = (i / 6000) % 2 == 0;
            const float v = burst ? (rng.nextFloat() * 2.0f - 1.0f) * 0.9f
                                  : 0.9f * std::sin (juce::MathConstants<float>::halfPi * (float) i + juce::MathConstants<float>::pi * 0.25f);
            hard.setSample (0, i, v); hard.setSample (1, i, burst ? (rng.nextFloat() * 2.0f - 1.0f) * 0.9f : v);
        }
        runBlocks (mx2, hard, 480);
        check (measureTp (hard) <= -1.0f + 0.1f, "Maximizer holds the ceiling on bursts and inter-sample peaks", juce::String (measureTp (hard), 2) + " dBTP");

        // 3. under the ceiling it is a clean delay of exactly the reported latency
        Maximizer mx3; mx3.prepare (fs, 256, 2);
        mx3.setActive (true); mx3.setEnabled (true); mx3.setCeilingDb (-1.0f); mx3.setGainDb (0.0f);
        const auto in = sine (fs, 440.0f, 0.3f, 16384);
        auto out = in;
        runBlocks (mx3, out, 256);
        const int lat = mx3.getLatencySamples();
        float err = 0.0f;
        for (int i = 4096; i < 16000; ++i) err = juce::jmax (err, std::abs (out.getSample (0, i) - in.getSample (0, i - lat)));
        check (err < 1.0e-5f, "Maximizer is transparent under the ceiling", "latency " + juce::String (lat) + " samples (" + juce::String (1000.0 * lat / fs, 1) + " ms), err " + juce::String (err, 7));

        // 4. on anything that is not the master it does nothing and adds no delay
        Maximizer mx4; mx4.prepare (fs, 256, 2);
        mx4.setActive (false); mx4.setEnabled (true); mx4.setGainDb (12.0f);
        auto pass = in;
        runBlocks (mx4, pass, 256);
        float err2 = 0.0f;
        for (int i = 0; i < 16384; ++i) err2 = juce::jmax (err2, std::abs (pass.getSample (0, i) - in.getSample (0, i)));
        check (err2 == 0.0f, "Maximizer is a zero-latency pass-through off the master");

        // 5. loudness actually goes up when driven
        Maximizer mx5; mx5.prepare (fs, 512, 2);
        mx5.setActive (true); mx5.setEnabled (true); mx5.setCeilingDb (-1.0f); mx5.setGainDb (8.0f);
        auto music = noise ((int) fs * 3, 0.25f, false);
        const float before = rmsDb (music);
        runBlocks (mx5, music, 512);
        check (rmsDb (music) - before > 4.0f && measureTp (music) <= -0.9f, "Driving the maximizer raises loudness under the same ceiling",
               "+" + juce::String (rmsDb (music) - before, 1) + " dB RMS, " + juce::String (measureTp (music), 2) + " dBTP");
    }

    // --- Mix Brain: tonal analysis
    {
        const auto centres = analysisBandCentres();
        auto target = roleTargetCurve (Role::Vocal);
        check (analyseTone (target, Role::Vocal).empty(), "A vocal with a normal tonal balance gets no suggestions");

        auto muddy = target;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] > 180.0f && centres[(size_t) b] < 420.0f) muddy[(size_t) b] += 8.0f;
        auto sug = analyseTone (muddy, Role::Vocal);
        bool foundMud = false;
        for (auto& s : sug)
            if (s.kind == Suggestion::Kind::EqBand && s.band.freqHz > 150.0f && s.band.freqHz < 450.0f && (s.band.gainDb < 0.0f || (s.band.dynamic && s.band.rangeDb < 0.0f)))
                foundMud = true;
        check (foundMud, "Low-mid build-up on a vocal is found and answered with a cut", sug.empty() ? juce::String ("no suggestions") : sug[0].title + " -> " + sug[0].action);
        if (! sug.empty()) check (sug[0].why.length() > 40, "Suggestions explain themselves", sug[0].why.substring (0, 70) + "...");

        auto essy = target;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] > 5000.0f && centres[(size_t) b] < 9000.0f) essy[(size_t) b] += 9.0f;
        bool foundEss = false;
        for (auto& s : analyseTone (essy, Role::Vocal)) if (s.kind == Suggestion::Kind::DeEsser) foundEss = true;
        check (foundEss, "Excess sibilance on a vocal points to the de-esser");

        auto beat = roleTargetCurve (Role::Beat);
        { auto bs = analyseTone (beat, Role::Beat); check (bs.empty(), "A balanced beat gets no suggestions", bs.empty() ? juce::String() : bs[0].title + " / " + juce::String (bs[0].amountDb, 1)); }
        auto dull = beat;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] > 7000.0f) dull[(size_t) b] -= 9.0f;
        bool foundDull = false;
        for (auto& s : analyseTone (dull, Role::Beat)) if (s.kind == Suggestion::Kind::EqBand && s.band.type == EQType::HighShelf && s.band.gainDb > 0.0f) foundDull = true;
        check (foundDull, "A dull beat gets a high shelf");

        // analysis must not depend on how loud the track is
        auto quieter = muddy;
        for (auto& v : quieter) v -= 17.0f;
        check (analyseTone (quieter, Role::Vocal).size() == sug.size(), "Analysis is independent of overall level");
    }

    // --- EQ match
    {
        const auto centres = analysisBandCentres();
        auto mine = roleTargetCurve (Role::Beat);
        auto ref = mine;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float oct = std::log2 (centres[(size_t) b] / 3000.0f);
            ref[(size_t) b] += 5.0f * std::exp (-oct * oct * 2.0f);     // reference is brighter in the presence range
        }
        auto bands = matchReference (mine, ref, 1.0f);
        bool ok = false;
        for (auto& b : bands) if (b.freqHz > 1800.0f && b.freqHz < 5000.0f && b.gainDb > 2.0f) ok = true;
        check (ok, "EQ match finds the difference from the reference", bands.empty() ? juce::String ("no bands") : juce::String (bands[0].gainDb, 1) + " dB at " + juce::String (bands[0].freqHz, 0) + " Hz");
        check (bands.size() <= 5, "EQ match uses at most five bands", juce::String ((int) bands.size()));
        float worst = 0.0f;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            if (centres[(size_t) b] < 60.0f || centres[(size_t) b] > 14000.0f) continue;
            float fit = 0.0f;
            for (auto& band : bands) fit += EQEngine::bandResponseDb (band, centres[(size_t) b], 48000.0);
            worst = juce::jmax (worst, std::abs ((ref[(size_t) b] - mine[(size_t) b]) - fit));
        }
        check (worst < 1.6f, "Matched curve lands within 1.6 dB of the reference", juce::String (worst, 2) + " dB worst band");
        check (matchReference (mine, mine, 1.0f).empty(), "Matching a track to itself changes nothing");
        auto half = matchReference (mine, ref, 0.5f);
        check (! half.empty() && half[0].gainDb < bands[0].gainDb, "Match amount scales the correction");
    }

    // --- Spectrum analyser
    {
        SpectrumAnalyzer an; an.prepare (fs);
        auto s = sine (fs, 1000.0f, 0.25f, (int) fs * 2);
        int frames = 0;
        for (int pos = 0; pos < s.getNumSamples(); pos += 512)
        {
            juce::AudioBuffer<float> view (s.getArrayOfWritePointers(), 2, pos, juce::jmin (512, s.getNumSamples() - pos));
            an.pushBlock (view);
            frames += an.pump();
        }
        int peak = 0;
        for (int i = 1; i < SpectrumAnalyzer::kDisplayPoints; ++i) if (an.getDisplayDb()[(size_t) i] > an.getDisplayDb()[(size_t) peak]) peak = i;
        check (near (an.displayPointHz (peak), 1000.0f, 40.0f), "Analyser display peaks at the tone", juce::String (an.displayPointHz (peak), 0) + " Hz");
        check (frames > 80, "Analyser produces a new frame every 1024 samples", juce::String (frames) + " frames in 2 s");
        check (an.hasLongTerm(), "Analyser builds its long-term average");
        an.resetLongTerm();
        check (! an.hasLongTerm(), "Long-term average can be restarted");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
