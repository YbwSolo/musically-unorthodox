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
#include "dsp/PeakGuard.h"
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

    // --- Peak guard
    {
        PeakGuard pg; pg.prepare (fs, 512, 2);
        pg.setEnabled (true); pg.setCeilingDb (-6.0f);
        auto s = sine (fs, 1000.0f, 1.0f, (int) fs);
        runBlocks (pg, s, 512);
        const float peak = juce::Decibels::gainToDecibels (s.getMagnitude (0, 1000, (int) fs - 1000));
        check (peak <= -6.0f + 0.05f, "Peak guard holds the ceiling", juce::String (peak, 2));
        PeakGuard pg2; pg2.prepare (fs, 512, 2);
        pg2.setEnabled (true); pg2.setCeilingDb (-6.0f);
        auto quiet = sine (fs, 1000.0f, 0.1f, (int) fs);
        runBlocks (pg2, quiet, 512);
        check (near (juce::Decibels::gainToDecibels (quiet.getMagnitude (0, 1000, (int) fs - 1000) / 0.1f), 0.0f, 0.01f), "Peak guard is transparent under the ceiling");
        check (pg.getLatencySamples() == (int) std::round (fs * PeakGuard::kLookaheadSeconds), "Peak guard reports its look-ahead");
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
        check (near (eq.magnitudeDbAt (1000.0f), 0.0f, 0.01f), "Dynamic band static curve stays at its set gain");
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

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
