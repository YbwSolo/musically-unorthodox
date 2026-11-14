// Console test runner for the DSP and balance engine. Build target: MUTests.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>
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
#include "dsp/Pitch.h"
#include "dsp/Tuner.h"
#include "dsp/Doubler.h"
#include "dsp/Reverb.h"
#include "dsp/Echo.h"
#include "dsp/PitchFX.h"
#include "dsp/ColourFX.h"
#include "dsp/PeakGuard.h"
#include "dsp/KeyFinder.h"
#include "dsp/TuneGraph.h"
#include "analysis/ReverbAdvisor.h"

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

    // ================================================================ pitch, tune, doubler, reverb
    // A voice-like tone: a fundamental with harmonics that get quieter, optionally with vibrato.
    auto voiceTone = [fs] (float hz, float seconds, int channels, float vibratoCents = 0.0f, float vibratoHz = 5.5f)
    {
        const int n = (int) (fs * seconds);
        juce::AudioBuffer<float> b (channels, n);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double f = hz * std::pow (2.0, vibratoCents * std::sin (juce::MathConstants<double>::twoPi * vibratoHz * i / fs) / 1200.0);
            phase += juce::MathConstants<double>::twoPi * f / fs;
            float v = 0.0f;
            for (int k = 1; k <= 8; ++k) v += (float) std::sin (phase * k) / (float) k;
            for (int c = 0; c < channels; ++c) b.setSample (c, i, 0.2f * v);
        }
        return b;
    };
    // Follows the pitch of a signal with a fresh detector; returns the notes heard after the first "skip" seconds.
    auto trackNotes = [fs] (const juce::AudioBuffer<float>& b, float skip)
    {
        PitchDetector d; d.prepare (fs);
        std::vector<float> notes;
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (d.push (b.getSample (0, i)) && i > (int) (fs * skip) && d.frequency() > 0.0f)
                notes.push_back (69.0f + 12.0f * std::log2 (d.frequency() / 440.0f));
        return notes;
    };
    auto meanOf = [] (const std::vector<float>& v) { double s = 0; for (float x : v) s += x; return v.empty() ? 0.0f : (float) (s / (double) v.size()); };
    auto spanOf = [] (const std::vector<float>& v) { float lo = 1e9f, hi = -1e9f; for (float x : v) { lo = std::min (lo, x); hi = std::max (hi, x); } return v.empty() ? 0.0f : hi - lo; };
    auto rmsOf = [] (const juce::AudioBuffer<float>& b, int from) { return b.getRMSLevel (0, from, b.getNumSamples() - from); };
    auto inBlocks = [] (juce::AudioBuffer<float>& b, int block, const std::function<void (juce::AudioBuffer<float>&)>& run)
    {
        for (int pos = 0; pos < b.getNumSamples(); pos += block)
        {
            const int len = std::min (block, b.getNumSamples() - pos);
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), b.getNumChannels(), pos, len);
            run (view);
        }
    };

    {
        // --- the detector names the right note across the range a voice covers
        juce::String worst; float worstCents = 0.0f;
        for (float hz : { 82.41f, 110.0f, 164.81f, 220.0f, 329.63f, 440.0f, 659.26f, 880.0f })
        {
            auto t = voiceTone (hz, 0.6f, 1);
            const auto notes = trackNotes (t, 0.2f);
            const float cents = notes.empty() ? 999.0f : std::abs (meanOf (notes) - (69.0f + 12.0f * std::log2 (hz / 440.0f))) * 100.0f;
            if (cents > worstCents) { worstCents = cents; worst = juce::String (hz, 1) + " Hz off by " + juce::String (cents, 1) + " cents"; }
        }
        check (worstCents < 4.0f, "Pitch detector is within 4 cents from 82 Hz to 880 Hz", worst);

        juce::Random rng (3);
        juce::AudioBuffer<float> noise (1, (int) fs);
        for (int i = 0; i < noise.getNumSamples(); ++i) noise.setSample (0, i, 0.2f * (rng.nextFloat() * 2.0f - 1.0f));
        check (trackNotes (noise, 0.1f).size() < 5, "Pitch detector does not invent a note in noise", juce::String ((int) trackNotes (noise, 0.1f).size()) + " frames");
        juce::AudioBuffer<float> silence (1, (int) fs); silence.clear();
        check (trackNotes (silence, 0.0f).empty(), "Pitch detector stays quiet in silence");
    }
    {
        // --- hard tune: a note sung 40 cents flat comes out exactly on the note, at the same level
        Tuner t; t.prepare (fs, 256, 2);
        TunerSettings ts; ts.enabled = true; ts.speedMs = 0.0f; ts.keepVibrato = 0.0f; ts.amount = 1.0f; ts.scale = 0;
        t.setSettings (ts);
        const float flat = 440.0f * std::pow (2.0f, -40.0f / 1200.0f);
        auto in = voiceTone (flat, 1.5f, 2);
        auto out = in;
        inBlocks (out, 256, [&t] (juce::AudioBuffer<float>& v) { t.process (v); });
        const auto notes = trackNotes (out, 0.4f);
        check (std::abs (meanOf (notes) - 69.0f) * 100.0f < 3.0f && spanOf (notes) * 100.0f < 6.0f, "Hard tune lands a flat note exactly on pitch",
               juce::String ((meanOf (notes) - 69.0f) * 100.0f, 1) + " cents off, wobble " + juce::String (spanOf (notes) * 100.0f, 1) + " cents");
        const float change = juce::Decibels::gainToDecibels (rmsOf (out, (int) fs / 2) / rmsOf (in, (int) fs / 2));
        check (std::abs (change) < 1.0f, "Tuning does not change the level", juce::String (change, 2) + " dB");
        check (t.getLatencySamples() == (int) std::round (fs * 0.020) && t.getTargetNote() == 69, "Tune reports its 20 ms delay and the note it chose");

        // --- the scale decides where a note goes: C sharp is not in C major, so a slightly sharp C sharp goes up to D
        Tuner s; s.prepare (fs, 256, 1);
        ts.key = 0; ts.scale = 1; s.setSettings (ts);
        auto cs = voiceTone (277.18f * std::pow (2.0f, 10.0f / 1200.0f), 1.2f, 1);
        inBlocks (cs, 128, [&s] (juce::AudioBuffer<float>& v) { s.process (v); });
        const auto moved = trackNotes (cs, 0.4f);
        check (std::abs (meanOf (moved) - 62.0f) * 100.0f < 4.0f, "Tune follows the chosen key and scale", juce::String (meanOf (moved), 2) + " (62 is D)");
        check (Tuner::nearestAllowed (61.4f, 0, 1) == 62 && Tuner::nearestAllowed (60.4f, 0, 1) == 60 && Tuner::noteAllowed (69, 9, 2) && ! Tuner::noteAllowed (61, 9, 4),
               "Scales contain the right notes");
    }
    {
        // --- natural tune: the centre of the note is corrected, the vibrato stays
        Tuner t; t.prepare (fs, 256, 1);
        TunerSettings ts; ts.enabled = true; ts.speedMs = 60.0f; ts.keepVibrato = 1.0f; ts.amount = 1.0f;
        t.setSettings (ts);
        auto in = voiceTone (440.0f * std::pow (2.0f, -35.0f / 1200.0f), 3.0f, 1, 30.0f);
        const float before = spanOf (trackNotes (in, 1.0f));
        inBlocks (in, 256, [&t] (juce::AudioBuffer<float>& v) { t.process (v); });
        const auto notes = trackNotes (in, 1.0f);
        check (std::abs (meanOf (notes) - 69.0f) * 100.0f < 8.0f && spanOf (notes) > before * 0.6f, "Natural tune centres the note and keeps the vibrato",
               juce::String ((meanOf (notes) - 69.0f) * 100.0f, 1) + " cents off centre, vibrato " + juce::String (spanOf (notes) * 100.0f, 0) + " of " + juce::String (before * 100.0f, 0) + " cents");
    }
    {
        // --- with nothing to correct, Tune is a pure delay; switched off it is not in the path at all
        Tuner t; t.prepare (fs, 256, 2);
        TunerSettings ts; ts.enabled = true; t.setSettings (ts);
        juce::Random rng (5);
        juce::AudioBuffer<float> in (2, (int) fs);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < in.getNumSamples(); ++i) in.setSample (c, i, 0.3f * (rng.nextFloat() * 2.0f - 1.0f));
        auto out = in;
        inBlocks (out, 100, [&t] (juce::AudioBuffer<float>& v) { t.process (v, true); });
        const int d = t.getLatencySamples();
        float worst = 0.0f;
        for (int i = (int) fs / 4; i < out.getNumSamples(); ++i) worst = std::max (worst, std::abs (out.getSample (1, i) - in.getSample (1, i - d)));
        check (worst < 2.0e-3f, "Tune passes sound through unchanged when asked to (delay only)", "largest difference " + juce::String (worst, 5));
        ts.lowLatency = true; t.setSettings (ts);
        check (t.getLatencySamples() == 0, "Low latency mode: Tune reports no delay");
        ts.enabled = false; t.setSettings (ts);
        auto untouched = in;
        t.process (untouched);
        check (untouched.getSample (0, 1000) == in.getSample (0, 1000) && t.getLatencySamples() == 0, "Tune switched off leaves the sound alone");
    }
    {
        // --- key finding: a melody in A minor is heard as A minor
        Tuner t; t.prepare (fs, 256, 1);
        TunerSettings ts; ts.enabled = true; t.setSettings (ts);
        const std::pair<int, float> melody[] = { { 57, 0.9f }, { 60, 0.4f }, { 64, 0.6f }, { 62, 0.3f }, { 59, 0.3f }, { 57, 0.8f }, { 65, 0.3f }, { 64, 0.5f }, { 55, 0.3f }, { 57, 0.9f } };
        for (auto& m : melody)
        {
            auto tone = voiceTone (440.0f * std::pow (2.0f, ((float) m.first - 69.0f) / 12.0f), m.second, 1);
            inBlocks (tone, 256, [&t] (juce::AudioBuffer<float>& v) { t.process (v); });
        }
        const auto g = t.guessKey();
        check (g.key == 9 && g.minor && g.secondsHeard > 3.0f, "Key finder hears A minor", Tuner::keyNames()[g.key] + (g.minor ? " minor" : " major") + ", confidence " + juce::String (g.confidence, 2));
    }
    {
        // --- doubler: adds copies left and right that differ from each other, and leaves the lead in the middle
        Doubler d; d.prepare (fs, 256, 2);
        DoublerSettings ds; ds.enabled = true; ds.mix = 0.6f; ds.width = 1.0f; d.setSettings (ds);
        auto in = voiceTone (220.0f, 3.0f, 2, 12.0f);
        auto out = in;
        inBlocks (out, 256, [&d] (juce::AudioBuffer<float>& v) { d.process (v); });
        const int from = (int) fs;
        double ll = 0, rr = 0, lr = 0, sideE = 0, midE = 0; bool finite = true;
        for (int i = from; i < out.getNumSamples(); ++i)
        {
            const float l = out.getSample (0, i) - in.getSample (0, i), r = out.getSample (1, i) - in.getSample (1, i);      // only what was added
            ll += l * l; rr += r * r; lr += l * r;
            const float side = 0.5f * (out.getSample (0, i) - out.getSample (1, i)), mid = 0.5f * (out.getSample (0, i) + out.getSample (1, i));
            sideE += side * side; midE += mid * mid;
            finite = finite && std::isfinite (l) && std::isfinite (r);
        }
        const float corr = (float) (lr / std::sqrt (ll * rr + 1e-20));
        const float added = juce::Decibels::gainToDecibels ((float) std::sqrt ((ll + rr) / 2.0 / (out.getNumSamples() - from)) / rmsOf (in, from));
        check (finite && corr < 0.6f && sideE > midE * 0.01, "Doubler adds different copies on the left and the right", "left/right likeness " + juce::String (corr, 2));
        check (added > -14.0f && added < -1.0f, "Doubles sit under the lead, not over it", juce::String (added, 1) + " dB against the lead");
        ds.voices = 1; d.setSettings (ds);
        check (d.getNumVoices() == 4 && std::abs (d.getVoicePan (0) + 1.0f) < 0.01f, "Stack mode uses four voices");
        ds.enabled = false; d.setSettings (ds);
        auto off = in; d.process (off);
        check (off.getSample (0, 5000) == in.getSample (0, 5000), "Doubler switched off leaves the sound alone");
    }
    {
        // --- reverb: the decay time is what was asked for, and the tempo maths is right
        auto decayOf = [fs] (float seconds, float tone)
        {
            Reverb r; r.prepare (fs, 512, 2);
            ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = seconds; rs.preDelayMs = 0.0f; rs.mix = 1.0f; rs.duck = 0.0f; rs.tone = tone;
            r.setSettings (rs);
            juce::AudioBuffer<float> b (2, (int) (fs * (seconds * 1.6f + 0.5f)));
            b.clear(); b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
            for (int pos = 0; pos < b.getNumSamples(); pos += 512)
            {
                juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, pos, std::min (512, b.getNumSamples() - pos));
                r.process (view);
            }
            b.setSample (0, 0, 0.0f); b.setSample (1, 0, 0.0f);                 // take the dry click back out
            // Schroeder integration: energy still to come at each moment, in dB; fit the fall from -5 to -25 dB
            std::vector<double> rest ((size_t) b.getNumSamples());
            double acc = 0.0;
            for (int i = b.getNumSamples() - 1; i >= 0; --i) { acc += (double) b.getSample (0, i) * b.getSample (0, i) + (double) b.getSample (1, i) * b.getSample (1, i); rest[(size_t) i] = acc; }
            int t5 = -1, t25 = -1;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double db = 10.0 * std::log10 (rest[(size_t) i] / rest[0] + 1e-30);
                if (t5 < 0 && db <= -5.0) t5 = i;
                if (t25 < 0 && db <= -25.0) { t25 = i; break; }
            }
            return (t5 >= 0 && t25 > t5) ? (float) (3.0 * (t25 - t5) / fs) : 0.0f;
        };
        const float d1 = decayOf (1.0f, 0.5f), d2 = decayOf (2.5f, 0.5f);
        check (near (d1, 1.0f, 0.2f) && near (d2, 2.5f, 0.5f), "Reverb decays in the time it is set to", juce::String (d1, 2) + " s for 1.0, " + juce::String (d2, 2) + " s for 2.5");
        check (near (Reverb::beatMs (120.0), 500.0f, 0.01f) && near (Reverb::autoPreDelayMs (120.0), 31.25f, 0.01f) && near (Reverb::autoDecaySeconds (120.0, 1), 1.0f, 0.001f)
               && near (Reverb::autoDecaySeconds (128.0, 2), 1.875f, 0.001f) && near (Reverb::autoDecaySeconds (90.0, 0), 0.6667f, 0.001f),
               "Reverb timing follows the tempo (60,000 / BPM)");

        // nothing arrives before the pre-delay; the dry sound is untouched; the tail stays put at long decays
        Reverb r; r.prepare (fs, 512, 2);
        ReverbSettings rs; rs.enabled = true; rs.autoTime = true; rs.bpm = 120.0; rs.length = 1; rs.mix = 1.0f; rs.duck = 0.0f;
        r.setSettings (rs);
        juce::AudioBuffer<float> b (2, (int) fs); b.clear(); b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
        inBlocks (b, 512, [&r] (juce::AudioBuffer<float>& v) { r.process (v); });
        float early = 0.0f, later = 0.0f;
        for (int i = 1; i < (int) (fs * 0.030); ++i) early = std::max (early, std::abs (b.getSample (0, i)));
        for (int i = (int) (fs * 0.032); i < (int) (fs * 0.2); ++i) later = std::max (later, std::abs (b.getSample (0, i)));
        check (b.getSample (0, 0) == 1.0f && early < 1.0e-4f && later > 0.01f, "Reverb waits for the pre-delay and leaves the dry sound alone",
               "before " + juce::String (early, 6) + ", after " + juce::String (later, 3));

        Reverb longOne; longOne.prepare (fs, 512, 2);
        rs.autoTime = false; rs.decaySeconds = 10.0f; rs.mix = 1.0f; longOne.setSettings (rs);
        juce::Random rng (9);
        juce::AudioBuffer<float> loud (2, (int) fs * 12);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < loud.getNumSamples(); ++i) loud.setSample (c, i, 0.25f * (rng.nextFloat() * 2.0f - 1.0f));
        inBlocks (loud, 512, [&longOne] (juce::AudioBuffer<float>& v) { longOne.process (v); });
        const float peak = loud.getMagnitude (0, loud.getNumSamples());
        check (std::isfinite (peak) && peak < 4.0f, "Reverb stays stable with a 10 second tail", "peak " + juce::String (peak, 2));

        // ducking: the reverb is quieter while the dry sound plays than after it stops
        Reverb duck; duck.prepare (fs, 512, 2);
        rs.decaySeconds = 2.0f; rs.duck = 1.0f; rs.mix = 0.5f; duck.setSettings (rs);
        auto burst = voiceTone (220.0f, 2.0f, 2);
        inBlocks (burst, 512, [&duck] (juce::AudioBuffer<float>& v) { duck.process (v); });
        const float during = duck.getDuckDb();
        juce::AudioBuffer<float> gap (2, (int) fs); gap.clear();
        inBlocks (gap, 512, [&duck] (juce::AudioBuffer<float>& v) { duck.process (v); });
        check (during < -6.0f && duck.getDuckDb() > -1.0f, "Reverb steps back while the voice sings and returns in the gaps", juce::String (during, 1) + " dB during, " + juce::String (duck.getDuckDb(), 1) + " dB after");
    }

    // ================================================================ build 11: steadier tune, graph, key, delay, voice effects
    // How much of one frequency is in a signal (a single-bin Fourier measurement), as an amplitude.
    auto amountAt = [fs] (const juce::AudioBuffer<float>& b, int channel, float hz, int from)
    {
        double re = 0, im = 0; const int n = b.getNumSamples() - from;
        for (int i = 0; i < n; ++i)
        {
            const double a = juce::MathConstants<double>::twoPi * hz * i / fs;
            re += b.getSample (channel, from + i) * std::cos (a); im += b.getSample (channel, from + i) * std::sin (a);
        }
        return (float) (2.0 * std::sqrt (re * re + im * im) / (double) juce::jmax (1, n));
    };
    auto hzOf = [] (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); };
    {
        // --- a 15 ms blip of another note (a consonant, a click) is not believed; a real leap is followed quickly
        auto a = voiceTone (220.0f, 1.0f, 1);
        auto blip = voiceTone (660.0f, 0.015f, 1);
        a.copyFrom (0, (int) (fs * 0.5), blip, 0, 0, blip.getNumSamples());
        float highest = 0.0f;
        for (float n : trackNotes (a, 0.2f)) highest = std::max (highest, n);
        check (highest < 57.0f + 2.0f, "Pitch detector ignores a 15 ms blip of another note", "highest note reported " + juce::String (highest, 2) + " (57 is the real one)");

        juce::AudioBuffer<float> leap (1, (int) fs);
        auto low = voiceTone (220.0f, 0.5f, 1), high = voiceTone (440.0f, 0.5f, 1);
        leap.copyFrom (0, 0, low, 0, 0, low.getNumSamples()); leap.copyFrom (0, low.getNumSamples(), high, 0, 0, high.getNumSamples());
        PitchDetector d; d.prepare (fs);
        int arrived = -1;
        for (int i = 0; i < leap.getNumSamples(); ++i)
            if (d.push (leap.getSample (0, i)) && arrived < 0 && i > (int) (fs * 0.5) && d.frequency() > 400.0f) arrived = i - (int) (fs * 0.5);
        check (arrived >= 0 && arrived < (int) (fs * 0.07), "Pitch detector follows a real octave leap within 70 ms", juce::String (arrived * 1000.0 / fs, 0) + " ms");
    }
    {
        // --- note hold: a voice wavering across the line between two notes flickers without it and settles with it
        auto waver = [&] (float hold)
        {
            Tuner t; t.prepare (fs, 128, 1);
            TunerSettings ts; ts.enabled = true; ts.speedMs = 0.0f; ts.keepVibrato = 0.0f; ts.hold = hold; ts.scale = 0; t.setSettings (ts);
            auto in = voiceTone (hzOf (69.5f), 3.0f, 1, 22.0f, 3.0f);          // sits between A and A sharp, drifting 22 cents either way
            int changes = 0, last = -1;
            inBlocks (in, 128, [&] (juce::AudioBuffer<float>& v) { t.process (v); const int n = t.getTargetNote(); if (n >= 0 && last >= 0 && n != last) ++changes; if (n >= 0) last = n; });
            return changes;
        };
        const int loose = waver (0.0f), held = waver (1.0f);
        check (loose >= 6 && held <= 1, "Note hold stops a wavering voice flickering between two notes", juce::String (loose) + " note changes without it, " + juce::String (held) + " with it");

        // --- custom notes: with only A allowed, an E is taken to A
        Tuner t; t.prepare (fs, 256, 1);
        TunerSettings ts; ts.enabled = true; ts.speedMs = 0.0f; ts.keepVibrato = 0.0f; ts.scale = Tuner::kCustomScale; ts.noteMask = 1 << 9; t.setSettings (ts);
        auto e = voiceTone (hzOf (64.0f), 1.2f, 1);
        inBlocks (e, 256, [&t] (juce::AudioBuffer<float>& v) { t.process (v); });
        check (std::abs (meanOf (trackNotes (e, 0.4f)) - 69.0f) * 100.0f < 5.0f, "Custom notes: with only A allowed, every note becomes an A", juce::String (meanOf (trackNotes (e, 0.4f)), 2));
        check (Tuner::scaleMask (9, 2) == ((1 << 9) | (1 << 11) | (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7)), "A minor is the white keys");

        // --- formant: the voice gets brighter and smaller, the note stays where it was
        auto bright = [&] (float semis)
        {
            Tuner f; f.prepare (fs, 256, 1);
            TunerSettings fsx; fsx.enabled = true; fsx.amount = 0.0f; fsx.formantSemis = semis; f.setSettings (fsx);
            auto v = voiceTone (220.0f, 1.5f, 1);
            inBlocks (v, 256, [&f] (juce::AudioBuffer<float>& b) { f.process (b); });
            const float top = amountAt (v, 0, 1320.0f, (int) (fs * 0.5)) + amountAt (v, 0, 1540.0f, (int) (fs * 0.5)) + amountAt (v, 0, 1760.0f, (int) (fs * 0.5));
            const float bottom = amountAt (v, 0, 220.0f, (int) (fs * 0.5));
            return std::make_pair (juce::Decibels::gainToDecibels (top / bottom), meanOf (trackNotes (v, 0.5f)));
        };
        const auto plain = bright (0.0f), up = bright (7.0f), down = bright (-7.0f);
        check (up.first > plain.first + 3.0f && down.first < plain.first - 3.0f && std::abs (up.second - 57.0f) < 0.06f && std::abs (down.second - 57.0f) < 0.06f,
               "Formant changes the size of the voice and leaves the note alone",
               "top harmonics " + juce::String (down.first, 1) + " / " + juce::String (plain.first, 1) + " / " + juce::String (up.first, 1) + " dB, note " + juce::String (up.second, 2));
    }
    {
        // --- graph view: capture two out-of-tune notes, correct them, play back
        TuneGraph g; g.allocate();
        Tuner t; t.prepare (fs, 256, 1); t.setGraph (&g);
        TunerSettings ts; ts.enabled = true; ts.mode = 1; t.setSettings (ts);
        juce::AudioBuffer<float> take (1, (int) (fs * 2.4));
        take.clear();
        auto n1 = voiceTone (hzOf (68.6f), 1.0f, 1, 25.0f), n2 = voiceTone (hzOf (71.3f), 1.0f, 1);
        take.copyFrom (0, (int) (fs * 0.1), n1, 0, 0, n1.getNumSamples());
        take.copyFrom (0, (int) (fs * 1.2), n2, 0, 0, n2.getNumSamples());
        auto playThrough = [&] (juce::AudioBuffer<float>& b, double startSeconds)
        {
            int pos = 0;
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { t.setTimeline (true, startSeconds + (double) pos / fs); t.process (v); pos += v.getNumSamples(); });
        };
        g.setCapturing (true);
        auto pass = take; playThrough (pass, 10.0);
        g.setCapturing (false);
        auto firstSecond = [fs] (const juce::AudioBuffer<float>& b) { juce::AudioBuffer<float> part (1, (int) (fs * 0.9)); part.copyFrom (0, 0, b, 0, (int) (fs * 0.2), part.getNumSamples()); return part; };
        check (std::abs (meanOf (trackNotes (firstSecond (pass), 0.2f)) - 68.6f) < 0.08f, "While capturing, the voice is heard uncorrected", juce::String (meanOf (trackNotes (firstSecond (pass), 0.2f)), 2));
        g.segment (g.passFirst(), g.passLast() + 1);
        const auto& notes = g.getNotes();
        check (notes.size() == 2 && std::abs (notes[0].sung - 68.6f) < 0.08f && std::abs (notes[1].sung - 71.3f) < 0.08f && notes[0].start >= 1005 && notes[0].start <= 1016,
               "Capture finds the two notes, where they are in the song and what was sung",
               juce::String ((int) notes.size()) + " notes" + (notes.size() == 2 ? ": " + juce::String (notes[0].sung, 2) + " from frame " + juce::String (notes[0].start) + ", " + juce::String (notes[1].sung, 2) : juce::String()));
        if (notes.size() == 2)
        {
            g.getNotes()[0].pitch = 69.0f; g.getNotes()[0].straighten = 1.0f;
            g.getNotes()[1].pitch = 71.0f;
            g.rasterise();
            t.reset();
            auto out = take; playThrough (out, 10.0);
            juce::AudioBuffer<float> first (1, (int) (fs * 0.9)), second (1, (int) (fs * 0.9));
            first.copyFrom (0, 0, out, 0, (int) (fs * 0.2), first.getNumSamples()); second.copyFrom (0, 0, out, 0, (int) (fs * 1.3), second.getNumSamples());
            const auto a = trackNotes (first, 0.2f), b = trackNotes (second, 0.2f);
            check (std::abs (meanOf (a) - 69.0f) * 100.0f < 4.0f && std::abs (meanOf (b) - 71.0f) * 100.0f < 4.0f, "Graph playback moves each note to where it was dragged",
                   juce::String (meanOf (a), 2) + " and " + juce::String (meanOf (b), 2));
            check (spanOf (a) * 100.0f < 12.0f, "Straighten flattens the wobble inside a note", juce::String (spanOf (a) * 100.0f, 1) + " cents of movement left from 50");
            // somewhere else in the song nothing is moved
            t.reset();
            auto elsewhere = take; playThrough (elsewhere, 60.0);
            check (std::abs (meanOf (trackNotes (firstSecond (elsewhere), 0.2f)) - 68.6f) < 0.08f, "Outside the captured notes the voice is left alone", juce::String (meanOf (trackNotes (firstSecond (elsewhere), 0.2f)), 2));

            const auto saved = g.toString();
            TuneGraph h;
            const bool loaded = h.fromString (saved);
            check (loaded && h.getNotes().size() == 2 && std::abs (h.getNotes()[0].pitch - 69.0f) < 0.01f && std::abs (h.targetAt (1050) - g.targetAt (1050)) < 0.02f && h.targetAt (1050) > 60.0f,
                   "The graph is saved with the song and comes back the same", juce::String (saved.length()) + " characters");
            const int right = g.splitNote (0, (g.getNotes()[0].start + g.getNotes()[0].end) / 2);
            check (right == 1 && g.getNotes().size() == 3 && g.mergeWithNext (0) && g.getNotes().size() == 2, "A note can be split in two and joined again");
        }
    }
    {
        // --- key of a full beat: chords and a bass line, many notes at once
        auto chordsIn = [&] (std::initializer_list<std::array<int, 4>> bars)
        {
            KeyFinder k; k.prepare (fs);
            for (int round = 0; round < 3; ++round)
                for (auto& chord : bars)
                {
                    juce::AudioBuffer<float> b (2, (int) (fs * 1.2)); b.clear();
                    for (int note : chord)
                    {
                        auto tone = voiceTone (hzOf ((float) note), 1.2f, 2);
                        for (int c = 0; c < 2; ++c) b.addFrom (c, 0, tone, c, 0, b.getNumSamples(), 0.4f);
                    }
                    inBlocks (b, 512, [&k] (juce::AudioBuffer<float>& v) { k.push (v.getReadPointer (0), v.getReadPointer (1), v.getNumSamples()); });
                }
            return k.estimate();
        };
        // G minor: Gm, Eb, Bb, F, with the root in the bass; then Dm to finish on the home chord
        const auto gm = chordsIn ({ { 43, 55, 58, 62 }, { 39, 55, 58, 63 }, { 46, 58, 62, 65 }, { 41, 57, 60, 65 }, { 43, 55, 58, 62 } });
        check (gm.key == 7 && gm.minor && gm.secondsHeard > 10.0f, "Key finder hears G minor in a beat with chords and bass",
               juce::String (Tuner::keyNames()[gm.key]) + (gm.minor ? " minor" : " major") + ", confidence " + juce::String (gm.confidence, 2));
        const auto cmaj = chordsIn ({ { 48, 60, 64, 67 }, { 41, 60, 65, 69 }, { 43, 59, 62, 67 }, { 48, 60, 64, 67 } });
        check (cmaj.key == 0 && ! cmaj.minor, "Key finder hears C major", juce::String (Tuner::keyNames()[cmaj.key]) + (cmaj.minor ? " minor" : " major"));

        int key = -1; bool minor = false;
        const bool a = keyFromName ("beat 128 bpm Gmin", key, minor) && key == 7 && minor;
        const bool b = keyFromName ("Dark Keys F# minor", key, minor) && key == 6 && minor;
        const bool c = keyFromName ("loop_Bbm_90", key, minor) && key == 10 && minor;
        const bool d = keyFromName ("Piano Emaj", key, minor) && key == 4 && ! minor;
        const bool e = ! keyFromName ("FM bass", key, minor) && ! keyFromName ("Audio 3", key, minor) && ! keyFromName ("Drums", key, minor) && ! keyFromName ("am i wrong", key, minor);
        check (a && b && c && d && e, "A key written in a track name is read, and ordinary names are not mistaken for one",
               juce::String (a ? "" : "Gmin ") + (b ? "" : "F#minor ") + (c ? "" : "Bbm ") + (d ? "" : "Emaj ") + (e ? "" : "false alarm"));

        Role r = Role::Auto;
        const bool names = roleFromName ("rap", r) && r == Role::Vocal && roleFromName ("Hook adlibs", r) && r == Role::BackingVocal && roleFromName ("beat 128 bpm Gmin", r) && r == Role::Beat
                        && roleFromName ("808", r) && r == Role::Bass808 && roleFromName ("Stereo Out", r) && r == Role::Master && roleFromName ("Lead Vox 2", r) && r == Role::Vocal
                        && ! roleFromName ("Audio 3", r) && ! roleFromName ("Inst 1", r) && roleFromName ("lead synth", r) && r == Role::Melody
                        && roleFromName ("Ad libs", r) && r == Role::BackingVocal;
        check (names, "Track names say what a track is");
        RoleFeatures rapper; rapper.valid = true; rapper.lowEnergyRatio = 0.05f; rapper.highEnergyRatio = 0.1f; rapper.centroidHz = 1500.0f; rapper.crestFactorDb = 18.5f; rapper.mono = true;
        check (detectRole (rapper).role == Role::Vocal, "An uncompressed rap vocal is recognised as a vocal");
    }
    {
        // --- delay: the echoes land exactly on the note, alternate sides, and each is quieter by the repeats amount
        Echo e; e.prepare (fs, 512, 2);
        EchoSettings es; es.enabled = true; es.sync = true; es.note = 3; es.bpm = 120.0; es.mix = 0.8f; es.feedback = 0.5f; es.tone = 1.0f; es.width = 1.0f; es.duck = 0.0f; e.setSettings (es);
        juce::AudioBuffer<float> b (2, (int) (fs * 1.2)); b.clear();
        // a short burst rather than a single click, so the filters in the loop do not decide the result
        for (int i = 0; i < 96; ++i) { const float v = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / (float) fs); b.setSample (0, i, v); b.setSample (1, i, v); }
        const float dry0 = b.getSample (0, 24);
        inBlocks (b, 512, [&e] (juce::AudioBuffer<float>& v) { e.process (v); });
        auto loudestIn = [&b, fs] (int channel, float fromMs, float toMs, int& at)
        {
            float best = 0.0f; at = 0;
            for (int i = (int) (fs * fromMs * 0.001); i < (int) (fs * toMs * 0.001); ++i) if (std::abs (b.getSample (channel, i)) > best) { best = std::abs (b.getSample (channel, i)); at = i; }
            return best;
        };
        int at1 = 0, at2 = 0, dummy = 0;
        const float l1 = loudestIn (0, 200, 300, at1), r1 = loudestIn (1, 200, 300, dummy), r2 = loudestIn (1, 450, 550, at2), l2 = loudestIn (0, 450, 550, dummy);
        const float between = loudestIn (0, 60, 200, dummy);
        check (near (Echo::syncedMs (120.0, 3), 250.0f, 0.01f) && near (Echo::syncedMs (128.0, 2), 351.56f, 0.05f) && near (Echo::syncedMs (90.0, 1), 666.67f, 0.05f), "Delay times: 60,000 / BPM x note length");
        check (b.getSample (0, 24) == dry0 && between < 1.0e-4f && std::abs ((at2 - at1) - (int) (fs * 0.25)) <= 2 && std::abs (at1 - (int) (fs * 0.25)) < 80,
               "The first echo arrives one eighth note after the sound, the next one an eighth later", juce::String (at1 * 1000.0 / fs, 1) + " ms and " + juce::String (at2 * 1000.0 / fs, 1) + " ms");
        check (l1 > 0.05f && r1 < l1 * 0.05f && r2 > 0.02f && l2 < r2 * 0.05f, "With ping-pong up, the echoes bounce left then right", "L " + juce::String (l1, 3) + " then R " + juce::String (r2, 3));
        check (near (juce::Decibels::gainToDecibels (r2 / l1), -6.0f, 1.2f), "Each echo is quieter by the repeats amount", juce::String (juce::Decibels::gainToDecibels (r2 / l1), 1) + " dB at 50 %");

        Echo wild; wild.prepare (fs, 512, 2);
        es.feedback = 0.9f; es.mix = 1.0f; wild.setSettings (es);
        juce::Random rng (5);
        juce::AudioBuffer<float> loud (2, (int) fs * 10);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < loud.getNumSamples(); ++i) loud.setSample (c, i, 0.25f * (rng.nextFloat() * 2.0f - 1.0f));
        inBlocks (loud, 512, [&wild] (juce::AudioBuffer<float>& v) { wild.process (v); });
        check (std::isfinite (loud.getMagnitude (0, loud.getNumSamples())) && loud.getMagnitude (0, loud.getNumSamples()) < 6.0f, "Delay stays stable with the repeats at maximum", "peak " + juce::String (loud.getMagnitude (0, loud.getNumSamples()), 2));
        es.enabled = false; wild.setSettings (es);
        auto quiet = sine (fs, 300.0f, 0.2f, 2048); const float before = quiet.getSample (0, 700);
        wild.process (quiet);
        check (quiet.getSample (0, 700) == before, "Delay switched off leaves the sound alone");
    }
    {
        // --- shift: an octave down is an octave down; harmony: the right notes of the key, really present in the sound
        PitchFX p; p.prepare (fs, 256, 2);
        PitchFXSettings ps; ps.shiftOn = true; ps.semitones = -12.0f; ps.shiftMix = 1.0f; p.setSettings (ps);
        auto v = voiceTone (hzOf (64.0f), 1.5f, 2);
        const float inLevel = rmsOf (v, (int) fs / 2);
        inBlocks (v, 256, [&p] (juce::AudioBuffer<float>& b) { p.process (b); });
        const auto notes = trackNotes (v, 0.5f);
        check (std::abs (meanOf (notes) - 52.0f) * 100.0f < 5.0f, "Shift moves the voice down exactly an octave", juce::String (meanOf (notes), 2) + " (52 is an octave below 64)");
        check (std::abs (juce::Decibels::gainToDecibels (rmsOf (v, (int) fs / 2) / inLevel)) < 2.5f, "Shift keeps the level", juce::String (juce::Decibels::gainToDecibels (rmsOf (v, (int) fs / 2) / inLevel), 1) + " dB");

        check (PitchFX::harmonyNote (60, 0, 0, 0, 1, 0xFFF) == 64 && PitchFX::harmonyNote (64, 0, 0, 0, 1, 0xFFF) == 67 && PitchFX::harmonyNote (71, 0, 0, 0, 1, 0xFFF) == 74
               && PitchFX::harmonyNote (60, 0, 1, 0, 1, 0xFFF) == 67 && PitchFX::harmonyNote (60, 2, 6, 0, 1, 0xFFF) == 48 && PitchFX::harmonyNote (60, 0, 5, 0, 1, 0xFFF) == 57
               && PitchFX::harmonyNote (57, 0, 0, 9, 2, 0xFFF) == 60 && PitchFX::harmonyNote (60, 0, 0, 0, 0, 0xFFF) == 64 && PitchFX::numVoices (6) == 3 && PitchFX::numVoices (0) == 1,
               "Harmony chooses notes of the key: a third above C in C major is E, above E it is G, above B it is D");
        PitchFX h; h.prepare (fs, 256, 2);
        PitchFXSettings hs; hs.harmonyOn = true; hs.voicing = 2; hs.harmonyMix = 0.6f; hs.key = 0; hs.scale = 1; h.setSettings (hs);
        auto lead = voiceTone (hzOf (60.0f), 2.0f, 2);
        auto sung = lead;
        inBlocks (sung, 256, [&h] (juce::AudioBuffer<float>& b) { h.process (b); });
        const int from = (int) fs;
        // the harmony voices sit a few cents off the exact note, as real singers do, so that is where to look
        const float eHz = hzOf (64.04f), gHz = hzOf (66.95f);
        const float e4 = amountAt (sung, 0, eHz, from) + amountAt (sung, 1, eHz, from), g4 = amountAt (sung, 0, gHz, from) + amountAt (sung, 1, gHz, from);
        const float wasE = amountAt (lead, 0, hzOf (64.0f), from) * 2.0f, c4 = amountAt (sung, 0, hzOf (60.0f), from) * 2.0f;
        check (e4 > c4 * 0.08f && g4 > c4 * 0.08f && wasE < c4 * 0.01f && h.getHarmonyNote (0) == 64 && h.getHarmonyNote (1) == 67, "Harmony adds an E and a G to a sung C",
               "E " + juce::String (juce::Decibels::gainToDecibels (e4 / c4), 1) + " dB and G " + juce::String (juce::Decibels::gainToDecibels (g4 / c4), 1) + " dB against the lead");
        double diff = 0.0;
        for (int i = from; i < sung.getNumSamples(); ++i) diff += std::abs (sung.getSample (0, i) - sung.getSample (1, i));
        check (diff > 1.0, "The harmony voices are spread left and right");
    }
    {
        // --- phone: only the middle of the voice gets through
        Phone ph; ph.prepare (fs, 2);
        PhoneSettings pset; pset.enabled = true; pset.tone = 0.25f; pset.drive = 0.0f; pset.amount = 1.0f; ph.setSettings (pset);
        juce::AudioBuffer<float> warm (2, 256); warm.clear(); ph.process (warm);
        check (ph.responseDb (100.0f) < -20.0f && ph.responseDb (9000.0f) < -18.0f && ph.responseDb (1500.0f) > -1.0f, "Phone keeps the middle of the voice and drops the lows and highs",
               juce::String (ph.responseDb (100.0f), 1) + " dB at 100 Hz, " + juce::String (ph.responseDb (1500.0f), 1) + " at 1.5 kHz, " + juce::String (ph.responseDb (9000.0f), 1) + " at 9 kHz");
        auto lowTone = sine (fs, 100.0f, 0.3f, (int) fs), midTone = sine (fs, 1500.0f, 0.1f, (int) fs);
        inBlocks (lowTone, 512, [&ph] (juce::AudioBuffer<float>& b) { ph.process (b); });
        ph.reset();
        inBlocks (midTone, 512, [&ph] (juce::AudioBuffer<float>& b) { ph.process (b); });
        check (lowTone.getMagnitude (0, (int) fs / 2, (int) fs / 2) < 0.03f && midTone.getMagnitude (0, (int) fs / 2, (int) fs / 2) > 0.05f, "Phone, measured on real tones");

        // --- lo-fi: transparent at zero, gritty when crushed, and the wobble bends the pitch
        LoFi lf; lf.prepare (fs, 2);
        LoFiSettings ls; ls.enabled = true; ls.crush = 0.0f; ls.wobble = 0.0f; ls.mix = 1.0f; lf.setSettings (ls);
        auto clean = sine (fs, 1000.0f, 0.25f, (int) fs);
        inBlocks (clean, 512, [&lf] (juce::AudioBuffer<float>& b) { lf.process (b); });
        check (near (juce::Decibels::gainToDecibels (amountAt (clean, 0, 1000.0f, (int) fs / 2) / 0.25f), 0.0f, 0.3f), "Lo-fi at zero leaves the sound as it was",
               juce::String (juce::Decibels::gainToDecibels (amountAt (clean, 0, 1000.0f, (int) fs / 2) / 0.25f), 2) + " dB");
        ls.crush = 0.9f; lf.setSettings (ls); lf.reset();
        auto gritty = sine (fs, 1000.0f, 0.25f, (int) fs);
        inBlocks (gritty, 512, [&lf] (juce::AudioBuffer<float>& b) { lf.process (b); });
        // crushed to about 4.6 kHz: the copies of a 1 kHz tone that the low rate creates include one near 3.6 kHz
        const float alias = amountAt (gritty, 0, LoFi::rateHz (0.9f, fs) - 1000.0f, (int) fs / 2);
        check (alias > 0.004f && std::isfinite (gritty.getMagnitude (0, gritty.getNumSamples())) && near (LoFi::bits (1.0f), 6.0f, 0.01f), "Lo-fi crush adds the grit of a low sample rate",
               juce::String (juce::Decibels::gainToDecibels (alias / 0.25f), 1) + " dB of fold-back at " + juce::String (LoFi::rateHz (0.9f, fs) - 1000.0f, 0) + " Hz");
        ls.crush = 0.0f; ls.wobble = 1.0f; lf.setSettings (ls); lf.reset();
        auto drift = voiceTone (220.0f, 3.0f, 1);
        inBlocks (drift, 512, [&lf] (juce::AudioBuffer<float>& b) { lf.process (b); });
        check (spanOf (trackNotes (drift, 0.5f)) * 100.0f > 8.0f, "Lo-fi wobble makes the pitch drift", juce::String (spanOf (trackNotes (drift, 0.5f)) * 100.0f, 1) + " cents");
    }
    {
        // --- motion: all three kinds stay finite and sensible; chop: locked to the beat
        for (int type = 0; type < 3; ++type)
        {
            Motion m; m.prepare (fs, 2);
            MotionSettings ms; ms.enabled = true; ms.type = type; ms.depth = 1.0f; ms.mix = 1.0f; ms.rateHz = 1.0f; m.setSettings (ms);
            auto v = voiceTone (220.0f, 3.0f, 2);
            const float in = rmsOf (v, (int) fs);
            auto before = v;
            inBlocks (v, 512, [&m] (juce::AudioBuffer<float>& b) { m.process (b); });
            const float change = juce::Decibels::gainToDecibels (rmsOf (v, (int) fs) / in);
            double moved = 0.0, sides = 0.0;
            for (int i = (int) fs; i < v.getNumSamples(); ++i) { moved += std::abs (v.getSample (0, i) - before.getSample (0, i)); sides += std::abs (v.getSample (0, i) - v.getSample (1, i)); }
            static const char* const names[] = { "Chorus changes the sound, widens it and keeps the level sensible", "Flanger changes the sound, widens it and keeps the level sensible", "Phaser changes the sound, widens it and keeps the level sensible" };
            check (std::isfinite (change) && change > -6.0f && change < 9.0f && moved > 10.0 && sides > 10.0, names[type], juce::String (change, 1) + " dB");
        }
        Motion off; off.prepare (fs, 2);
        MotionSettings ms; ms.enabled = true; ms.mix = 0.0f; off.setSettings (ms);
        auto same = sine (fs, 500.0f, 0.2f, 4096); const float was = same.getSample (0, 3000);
        off.process (same);
        check (near (same.getSample (0, 3000), was, 1.0e-6f), "Motion with the mix at zero is the dry sound");

        Chop c; c.prepare (fs);
        ChopSettings cs; cs.enabled = true; cs.rate = 3; cs.depth = 1.0f; cs.shape = 1.0f; cs.bpm = 120.0; c.setSettings (cs);       // sixteenths at 120: 125 ms a step
        auto tone = sine (fs, 500.0f, 0.5f, (int) fs);
        int pos = 0;
        inBlocks (tone, 256, [&] (juce::AudioBuffer<float>& b) { c.setTimeline (true, 8.0 + (double) pos / fs * 2.0); c.process (b); pos += b.getNumSamples(); });
        const int step = (int) (fs * 0.125);
        const float on = tone.getRMSLevel (0, step * 4 + step / 8, step / 4), offLevel = tone.getRMSLevel (0, step * 4 + step / 2 + step / 8, step / 4);
        check (on > 0.3f && offLevel < 0.01f && near (Chop::gateAt (0.25f, 1.0f), 1.0f, 0.001f) && near (Chop::gateAt (0.75f, 1.0f), 0.0f, 0.001f) && near (Chop::gateAt (0.25f, 0.0f), 1.0f, 0.001f) && near (Chop::gateAt (0.0f, 0.0f), 0.5f, 0.001f),
               "Chop opens on each sixteenth and closes halfway to the next", "open " + juce::String (on, 3) + ", closed " + juce::String (offLevel, 4));
    }
    {
        // --- peak guard: nothing leaves above -1 dB, and anything under it is not touched at all
        PeakGuard g; g.prepare (fs); g.setEnabled (true);
        auto hot = voiceTone (220.0f, 1.0f, 2);
        hot.applyGain (1.0f / hot.getMagnitude (0, hot.getNumSamples()) * 1.4f);          // peaks 3 dB over full scale
        inBlocks (hot, 256, [&g] (juce::AudioBuffer<float>& b) { g.process (b); });
        const float out = juce::Decibels::gainToDecibels (hot.getMagnitude (0, hot.getNumSamples()));
        check (out <= -0.99f && g.getInputPeakDb() > 2.5f && g.getReductionDb() < -3.0f, "Peak guard holds a track at -1 dB", juce::String (out, 2) + " dB out, " + juce::String (g.getInputPeakDb(), 1) + " dB in");
        PeakGuard idle; idle.prepare (fs); idle.setEnabled (true);
        auto calm = sine (fs, 300.0f, 0.5f, 4096); auto copy = calm;
        idle.process (calm);
        bool same = true; for (int i = 0; i < calm.getNumSamples(); ++i) same = same && calm.getSample (0, i) == copy.getSample (0, i);
        check (same && idle.getReductionDb() == 0.0f, "Peak guard leaves a track that is under the limit exactly as it was");
    }
    {
        // --- reverb chosen for the track
        ReverbFacts f; f.role = Role::Vocal; f.bpm = 128.0; f.measured = true; f.crestDb = 17.0f; f.highShare = 0.12f;
        const auto rap = adviseReverb (f);
        f.crestDb = 9.0f; f.sustainedNotes = true;
        const auto sung = adviseReverb (f);
        f.role = Role::Bass808; const auto bass = adviseReverb (f);
        f.role = Role::Drums; f.sustainedNotes = false; const auto drums = adviseReverb (f);
        f.role = Role::Vocal; f.highShare = 0.3f; const auto brightVoice = adviseReverb (f);
        check (rap.use && rap.type == 1 && rap.length == 0 && rap.duck >= 65.0f && sung.length >= 1 && sung.mix > rap.mix && ! bass.use && drums.type == 0 && drums.length == 0 && brightVoice.tone < rap.tone,
               "Reverb fitted to the track: short and ducked for rap, longer for singing, none for an 808, a room for drums, darker for a bright voice", rap.reason);

        // --- the three spaces all reach the decay that was asked for
        auto decayOfType = [fs, &inBlocks] (int type)
        {
            Reverb r; r.prepare (fs, 512, 2);
            ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = 1.5f; rs.preDelayMs = 0.0f; rs.mix = 1.0f; rs.duck = 0.0f; rs.tone = 1.0f; rs.type = type; r.setSettings (rs);
            juce::AudioBuffer<float> b (2, (int) (fs * 2.5)); b.clear();
            juce::Random rng (11);
            for (int i = 0; i < (int) (fs * 0.05); ++i) { const float v = rng.nextFloat() * 2.0f - 1.0f; b.setSample (0, i, v * 0.5f); b.setSample (1, i, v * 0.5f); }
            inBlocks (b, 512, [&r] (juce::AudioBuffer<float>& v) { r.process (v); });
            auto level = [&b, fs] (float at) { return juce::Decibels::gainToDecibels (b.getRMSLevel (0, (int) (fs * at), (int) (fs * 0.1))); };
            return -60.0f / ((level (1.3f) - level (0.3f)) / 1.0f);        // seconds to fall 60 dB, from the fall over one second
        };
        const float room = decayOfType (0), plate = decayOfType (1), hall = decayOfType (2);
        check (near (room, 1.5f, 0.3f) && near (plate, 1.5f, 0.3f) && near (hall, 1.5f, 0.3f), "Room, plate and hall all fade in the time that was set",
               juce::String (room, 2) + " / " + juce::String (plate, 2) + " / " + juce::String (hall, 2) + " s for 1.5");
        // width at zero puts the reverb in the middle
        Reverb narrow; narrow.prepare (fs, 512, 2);
        ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.mix = 1.0f; rs.duck = 0.0f; rs.width = 0.0f; narrow.setSettings (rs);
        auto burst = voiceTone (330.0f, 1.0f, 2);
        auto dry = burst;
        inBlocks (burst, 512, [&narrow] (juce::AudioBuffer<float>& v) { narrow.process (v); });
        double side = 0.0, wet = 0.0;
        for (int i = (int) fs / 2; i < burst.getNumSamples(); ++i) { side += std::abs (burst.getSample (0, i) - burst.getSample (1, i)); wet += std::abs (burst.getSample (0, i) - dry.getSample (0, i)); }
        check (side < wet * 1.0e-4 && wet > 1.0, "Reverb width at zero keeps the reverb in the middle");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
