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
#include "state/FxOrder.h"
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
        // A vocal whose own peaks would end up over the top of its channel: the whole song comes down to make room,
        // the balance between the tracks stays exactly the same, and nothing is asked to compress harder.
        auto spiky = tracks;
        spiky[0].truePeakDb = 2.0f;                  // a vocal with 22 dB between its average and its peaks: over the top already
        const auto roomy = computeBalance (spiky, tg);
        float rVocal = 0, rBeat = 0;
        for (auto& m : roomy.moves) { if (m.slot == 0) rVocal = m.deltaDb; if (m.slot == 1) rBeat = m.deltaDb; }
        check (roomy.headroomDb > 0.5f && spiky[0].truePeakDb + rVocal <= tg.trackTruePeak + 0.11f && near (rBeat - rVocal, dBeat - dVocal, 0.05f),
               "A track whose peaks would clip its channel makes the balance turn the whole song down, not that track alone",
               "song " + juce::String (roomy.headroomDb, 1) + " dB lower, vocal peaks at " + juce::String (spiky[0].truePeakDb + rVocal, 1) + " dB, balance unchanged");
        check (near (plan.headroomDb, 0.0f, 0.01f), "With room on every channel nothing is turned down for headroom");

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
        // A voice has nothing to defend under 250 Hz. On real sessions the old rule asked for cuts in the beat at
        // 50 and 130 Hz "to uncover the vocal", which only took the weight out of the beat.
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float hz = centres[(size_t) b];
            voc.bandsDb[(size_t) b] = (hz > 40.0f && hz < 180.0f) ? -20.0f : -45.0f;
            beat.bandsDb[(size_t) b] = (hz > 40.0f && hz < 180.0f) ? -14.0f : -45.0f;
        }
        check (findMasking ({ voc, beat }, centres).empty(), "Bass in the beat is not treated as covering the voice");
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
        const char* names[] = { "Clean", "Punch", "Smooth", "Glue", "Vocal" };
        for (int style = 0; style < 5; ++style)
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

    // --- FX order: one number for every order of the six in-line tools
    {
        namespace fo = mu::fxorder;
        bool roundTrip = true, allDifferent = true;
        std::vector<fo::Order> seen;
        for (int n = 0; n < fo::kOrders; ++n)
        {
            const auto o = fo::decode (n);
            if (fo::encode (o) != n) roundTrip = false;
            int mask = 0; for (int t : o) mask |= 1 << t;
            if (mask != 63 || std::find (seen.begin(), seen.end(), o) != seen.end()) allDifferent = false;
            seen.push_back (o);
        }
        const fo::Order standard { fo::Phone, fo::Lofi, fo::Motion, fo::Chop, fo::Delay, fo::Reverb };
        const fo::Order reverbFirst { fo::Reverb, fo::Phone, fo::Lofi, fo::Motion, fo::Chop, fo::Delay };
        check (roundTrip && allDifferent && fo::decode (0) == standard && fo::moved (standard, fo::Reverb, 0) == reverbFirst && fo::moved (reverbFirst, fo::Reverb, 5) == standard
               && fo::decode (-5) == standard && fo::decode (99999) == fo::decode (fo::kOrders - 1),
               "Every one of the 720 FX orders has its own number, number 0 is the order older songs were made with, and moving a tool keeps the rest in sequence");
    }

    // --- Mix Brain: tonal analysis
    {
        const auto centres = analysisBandCentres();
        auto target = roleTargetCurve (Role::Vocal);
        check (analyseTone (target, Role::Vocal).empty(), "A vocal with a normal tonal balance gets no suggestions");

        // A raw recording is not a finished vocal with a small problem. Shay's five sessions were 6 to 17 dB short
        // of a finished voice somewhere between 2 and 8 kHz, so the lift is sized to the gap, inside a ceiling.
        {
            auto dull = target;
            for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] > 1800.0f) dull[(size_t) b] -= 11.0f;
            float presence = 0.0f, air = 0.0f;
            for (auto& x : analyseTone (dull, Role::Vocal))
                if (x.kind == Suggestion::Kind::EqBand && ! x.band.dynamic)
                {
                    if (x.band.type == EQType::Bell && x.band.freqHz > 2000.0f && x.band.freqHz < 4000.0f) presence = x.band.gainDb;
                    if (x.band.type == EQType::HighShelf) air = x.band.gainDb;
                }
            check (presence >= 5.0f && presence <= 7.0f && air >= 4.0f && air <= 8.0f, "A recording 11 dB dull from 2 kHz up gets a presence and air lift sized to the gap, inside its ceiling",
                   "presence +" + juce::String (presence, 1) + " dB, air +" + juce::String (air, 1) + " dB");
            // The voice is judged against its own body (250 Hz to 1 kHz). Boom underneath the voice is a low-end
            // problem; it must not make the rest of the voice read as thin or dull.
            auto boomy = target;
            for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] < 160.0f) boomy[(size_t) b] += 12.0f;
            bool lifted = false;
            for (auto& x : analyseTone (boomy, Role::Vocal))
                if (x.kind == Suggestion::Kind::EqBand && ! x.band.dynamic && x.band.gainDb > 0.0f && x.band.freqHz > 1000.0f) lifted = true;
            check (! lifted, "Boom under a voice does not make the analysis brighten the voice");
        }

        auto muddy = target;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] > 180.0f && centres[(size_t) b] < 420.0f) muddy[(size_t) b] += 8.0f;
        auto sug = analyseTone (muddy, Role::Vocal);
        bool foundMud = false;
        for (auto& s : sug)
            if (s.kind == Suggestion::Kind::EqBand && s.band.freqHz > 150.0f && s.band.freqHz < 450.0f && (s.band.gainDb < 0.0f || (s.band.dynamic && s.band.rangeDb < 0.0f)))
                foundMud = true;
        check (foundMud, "Low-mid build-up on a vocal is found and answered with a cut", sug.empty() ? juce::String ("no suggestions") : sug[0].title + " -> " + sug[0].action);
        if (! sug.empty()) check (sug[0].why.length() > 40, "Suggestions explain themselves", sug[0].why.substring (0, 70) + "...");
        // With a listening pass behind it, the analysis knows whether a build-up is always there or comes and goes
        {
            BandArray steady {}, comesAndGoes {};
            steady.fill (1.5f); comesAndGoes.fill (9.0f);
            auto mudOf = [&] (const BandArray* swing) -> Suggestion
            {
                for (auto& x : analyseTone (muddy, Role::Vocal, swing))
                    if (x.kind == Suggestion::Kind::EqBand && x.band.type == EQType::Bell && x.band.freqHz > 150.0f && x.band.freqHz < 450.0f) return x;
                return {};
            };
            const auto fixed = mudOf (&steady), moving = mudOf (&comesAndGoes);
            check (! fixed.band.dynamic && fixed.band.gainDb < -0.9f && moving.band.dynamic && moving.band.rangeDb < -1.4f,
                   "After listening, a build-up that is always there gets a fixed cut and one that comes and goes gets a dynamic cut", fixed.action + " / " + moving.action);
        }
        // the listening pass itself: only frames with sound in them count, and the result is the average over the pass
        {
            SpectrumAnalyzer an; an.prepare (fs);
            an.startListen();
            juce::AudioBuffer<float> chunk (2, 1024);
            double phase = 0.0;
            for (int block = 0; block < 280; ++block)        // six seconds: two of a 300 Hz tone, two silent, two of the tone again
            {
                const bool sounding = block < 94 || block >= 188;
                for (int i = 0; i < 1024; ++i) { phase += juce::MathConstants<double>::twoPi * 300.0 / fs; const float v = sounding ? 0.3f * (float) std::sin (phase) : 0.0f; chunk.setSample (0, i, v); chunk.setSample (1, i, v); }
                an.pushBlock (chunk);
                an.pump();
            }
            std::array<float, SpectrumAnalyzer::kNumBands> mean {}, swing {};
            const bool have = an.listenBands (mean, swing);
            int loudest = 0; for (int b = 1; b < SpectrumAnalyzer::kNumBands; ++b) if (mean[(size_t) b] > mean[(size_t) loudest]) loudest = b;
            check (have && near (an.getListenSeconds(), 4.0f, 0.4f) && near (an.bandCentreHz (loudest), 300.0f, 60.0f) && swing[(size_t) loudest] < 1.0f,
                   "A listening pass counts only the time there was sound, and measures what was in it", juce::String (an.getListenSeconds(), 1) + " s of sound in 6 s, loudest band " + juce::String (an.bandCentreHz (loudest), 0) + " Hz");
        }

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
            // The decay time of a reverb is the decay of its mids (highs are meant to go sooner), so keep 500 Hz to
            // 2 kHz before measuring: two gentle filters each way.
            const float lo = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 500.0f / (float) fs), hi = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 2000.0f / (float) fs);
            for (int c = 0; c < 2; ++c)
            {
                float a1 = 0.0f, a2 = 0.0f, h1 = 0.0f, h2 = 0.0f;
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    float v = b.getSample (c, i);
                    h1 += (v - h1) * lo; v -= h1; h2 += (v - h2) * lo; v -= h2;
                    a1 += (v - a1) * hi; a2 += (a1 - a2) * hi;
                    b.setSample (c, i, a2);
                }
            }
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
        check (near (d1, 1.0f, 0.2f) && near (d2, 2.5f, 0.5f), "Reverb decays in the time it is set to (measured on the mids)", juce::String (d1, 2) + " s for 1.0, " + juce::String (d2, 2) + " s for 2.5");
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
        // --- build 17: Harmony as separate voices, chords, voice leading and choir; Shift's modes
        check (PitchFX::stepNote (60, 2, 0, 1, 0xFFF) == 64 && PitchFX::stepNote (60, 4, 0, 1, 0xFFF) == 67 && PitchFX::stepNote (60, 7, 0, 1, 0xFFF) == 72 && PitchFX::stepNote (60, -3, 0, 1, 0xFFF) == 55
               && PitchFX::stepNote (60, -7, 0, 1, 0xFFF) == 48 && PitchFX::stepNote (57, 2, 9, 2, 0xFFF) == 60 && PitchFX::stepNote (60, 2, 0, 0, 0xFFF) == 64 && PitchFX::stepNote (60, -2, 0, 0, 0xFFF) == 57
               && PitchFX::stepNote (60, 9, 0, 0, 0xFFF) == 76 && PitchFX::stepNote (60, 0, 0, 1, 0xFFF) == 60,
               "A voice's interval is counted in notes of the key: two up from C in C major is E, three down is G, seven up is the octave");
        check (PitchFX::ledNote (67, 64, 60) == 67 && PitchFX::ledNote (64, 65, 62) == 65 && PitchFX::ledNote (-1, 65, 62) == 65 && PitchFX::ledNote (72, 64, 60) == 64 && PitchFX::ledNote (60, 64, 60) == 64,
               "Voice leading: a voice stays on its note while it still makes a third, fifth, sixth or octave with the tune, and moves when it does not");
        {
            const int cMajor = (1 << 0) | (1 << 4) | (1 << 7);
            const int taken[] = { 64 };
            check (PitchFX::chordNote (cMajor, 65, -1, nullptr, 0) == 64 && PitchFX::chordNote (cMajor, 65, 67, nullptr, 0) == 67 && PitchFX::chordNote (cMajor, 65, -1, taken, 1) == 67
                   && PitchFX::chordNote (0, 65, 64, nullptr, 0) == 65 && PitchFX::chordNote (cMajor, 71, 60, nullptr, 0) == 72,
                   "Chords: a voice takes the chord note nearest to where it belongs, stays on its note when the new chord has it too, and leaves a note another voice has");
        }
        auto run = [&] (PitchFXSettings st, juce::AudioBuffer<float> in)
        {
            PitchFX fx; fx.prepare (fs, 256, 2); st.key = 0; st.scale = 1; fx.setSettings (st);
            inBlocks (in, 256, [&fx] (juce::AudioBuffer<float>& b) { fx.process (b); });
            return in;
        };
        const int from = (int) fs;
        auto level = [&] (const juce::AudioBuffer<float>& b, float note) { return amountAt (b, 0, hzOf (note), from) + amountAt (b, 1, hzOf (note), from); };
        const auto lead = voiceTone (hzOf (60.0f), 2.0f, 2);
        {
            // two voices set by hand: a third above on the left, a fourth below on the right
            PitchFXSettings st; st.harmonyOn = true; st.harmonyMode = 1; st.harmonyMix = 0.6f; st.harmonyWidth = 1.0f; st.humanise = 0.0f;
            st.voice[0] = { true, 2, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f }; st.voice[1] = { true, -3, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
            const auto sung = run (st, lead);
            const float c4 = level (sung, 60.0f), e4 = level (sung, 64.0f), g3 = level (sung, 55.0f), d4 = level (sung, 62.0f);       // D is in neither voice
            const float eLeft = amountAt (sung, 0, hzOf (64.0f), from), eRight = amountAt (sung, 1, hzOf (64.0f), from), gLeft = amountAt (sung, 0, hzOf (55.0f), from), gRight = amountAt (sung, 1, hzOf (55.0f), from);
            check (e4 > c4 * 0.08f && g3 > c4 * 0.08f && d4 < e4 * 0.2f && eLeft > eRight * 4.0f && gRight > gLeft * 4.0f,
                   "Harmony voices set one by one sing their own intervals in their own places: a third above on the left, a fourth below on the right",
                   "E " + juce::String (juce::Decibels::gainToDecibels (e4 / c4), 1) + " dB and low G " + juce::String (juce::Decibels::gainToDecibels (g3 / c4), 1) + " dB against the lead");
            // one voice louder than the other, by its own level
            st.voice[1].level = 0.25f;
            const auto quieter = run (st, lead);
            check (std::abs (juce::Decibels::gainToDecibels (level (quieter, 55.0f) / g3) + 12.0f) < 2.0f && std::abs (juce::Decibels::gainToDecibels (level (quieter, 64.0f) / e4)) < 1.0f,
                   "Each harmony voice has its own level", "the fourth below at a quarter: " + juce::String (juce::Decibels::gainToDecibels (level (quieter, 55.0f) / g3), 1) + " dB");
            // Chords: the same third-above voice, with F major in the grid, sings F (in the chord) and not E (not in it)
            PitchFXSettings ch = st; ch.harmonyMode = 2; ch.voice[1].on = false; ch.chordMask = (1 << 5) | (1 << 9) | (1 << 0);
            const auto inChord = run (ch, lead);
            check (level (inChord, 65.0f) > c4 * 0.08f && level (inChord, 64.0f) < level (inChord, 65.0f) * 0.2f, "In Chords mode a voice sings a note of the chord: over F major, the third above C becomes an F",
                   "F " + juce::String (juce::Decibels::gainToDecibels (level (inChord, 65.0f) / c4), 1) + " dB, E " + juce::String (juce::Decibels::gainToDecibels (level (inChord, 64.0f) / c4), 1) + " dB against the lead");
            // Choir: three singers to a part. The part gets wider and is no longer one steady tone, at about the same loudness.
            PitchFXSettings one = st; one.voice[1].on = false; one.voice[0].pan = 0.0f;
            PitchFXSettings many = one; many.choir = 1.0f;
            auto harmonyOnly = [&] (const juce::AudioBuffer<float>& b, double& sides)
            {
                double sum = 0.0; sides = 0.0;
                for (int i = from; i < b.getNumSamples(); ++i)
                {
                    const double l = b.getSample (0, i) - lead.getSample (0, i), r = b.getSample (1, i) - lead.getSample (1, i);
                    sum += l * l + r * r; sides += (l - r) * (l - r);
                }
                return sum;
            };
            double soloSides = 0.0, choirSides = 0.0;
            const double solo = harmonyOnly (run (one, lead), soloSides), choir = harmonyOnly (run (many, lead), choirSides);
            check (std::abs (10.0 * std::log10 (choir / solo)) < 4.0 && choirSides > solo * 0.05 && soloSides < solo * 0.01, "Choir makes each part three singers: wider, and about as loud as it was",
                   juce::String (10.0 * std::log10 (choir / solo), 1) + " dB against one singer; " + juce::String (100.0 * choirSides / choir, 0) + " % of it is difference between the sides");
        }
        {
            // Shift, Wide: two copies nine cents either way, one each side
            PitchFXSettings st; st.shiftOn = true; st.shiftMode = 1; st.semitones = 0.0f; st.fineCents = 9.0f; st.shiftMix = 1.0f; st.shiftWidth = 1.0f;
            const auto wide = run (st, lead);
            juce::AudioBuffer<float> rightOnly (1, wide.getNumSamples()); rightOnly.copyFrom (0, 0, wide, 1, 0, wide.getNumSamples());
            const float up = (meanOf (trackNotes (wide, 1.0f)) - 60.0f) * 100.0f, down = (meanOf (trackNotes (rightOnly, 1.0f)) - 60.0f) * 100.0f;
            check (std::abs (up - 9.0f) < 3.0f && std::abs (down + 9.0f) < 3.0f, "Shift on Wide puts a copy a few cents sharp on one side and one as flat on the other",
                   juce::String (up, 1) + " cents on the left, " + juce::String (down, 1) + " on the right");
            // Robot: a voice that slides from G3 up to D4 comes out on one note, the key note
            PitchFXSettings robot; robot.shiftOn = true; robot.shiftMode = 2; robot.semitones = 0.0f; robot.shiftMix = 1.0f;
            juce::AudioBuffer<float> slide (2, (int) (fs * 3.0));
            double phase = 0.0;
            for (int i = 0; i < slide.getNumSamples(); ++i)
            {
                const double note = 55.0 + 7.0 * (double) i / slide.getNumSamples();
                phase += juce::MathConstants<double>::twoPi * 440.0 * std::pow (2.0, (note - 69.0) / 12.0) / fs;
                float v = 0.0f; for (int k = 1; k <= 8; ++k) v += (float) std::sin (phase * k) / (float) k;
                slide.setSample (0, i, 0.2f * v); slide.setSample (1, i, 0.2f * v);
            }
            const auto flat = trackNotes (run (robot, slide), 1.0f);
            check (std::abs (meanOf (flat) - 48.0f) < 0.1f && spanOf (flat) < 0.4f, "Shift on Robot holds a sliding voice on the key note",
                   "sung from G3 to D4, heard at " + juce::String (meanOf (flat), 2) + " (48 is the C below), moving " + juce::String (spanOf (flat) * 100.0f, 0) + " cents in all");
        }
        {
            // how heavy the biggest Harmony is: four parts, three singers each
            PitchFXSettings st; st.harmonyOn = true; st.harmonyMode = 1; st.choir = 1.0f; st.harmonyMix = 0.5f;
            for (int v = 0; v < 4; ++v) st.voice[(size_t) v] = { true, v == 0 ? 2 : v == 1 ? 4 : v == 2 ? -3 : 7, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            const auto long10 = voiceTone (hzOf (60.0f), 10.0f, 2);
            const double start = juce::Time::getMillisecondCounterHiRes();
            run (st, long10);
            const double took = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
            check (took < 2.5, "Four harmony parts with a full choir run many times faster than real time", "10 seconds of sound in " + juce::String (took, 2) + " s on this machine");
        }
    }
    {
        // --- build 17: the Phone devices and their noise
        auto band = [&] (const juce::AudioBuffer<float>& b, int from, int len) { return juce::Decibels::gainToDecibels (b.getRMSLevel (0, from, len), -140.0f); };
        {
            Phone tel; PhoneSettings ts; ts.enabled = true; ts.mode = 1; ts.tone = 0.4f; ts.drive = 0.0f; tel.setSettings (ts); tel.prepare (fs, 2);
            Phone horn; PhoneSettings hs = ts; hs.mode = 2; horn.setSettings (hs); horn.prepare (fs, 2);
            check (tel.responseDb (100.0f) < -25.0f && tel.responseDb (8000.0f) < -25.0f && tel.responseDb (1000.0f) > -3.0f
                   && Phone::lowEdgeHz (0.4f, 1) > 280.0f && Phone::highEdgeHz (0.4f, 1) < 3700.0f
                   && horn.responseDb (1100.0f) > horn.responseDb (1800.0f) + 3.0f && horn.responseDb (2800.0f) > horn.responseDb (1800.0f) + 2.0f,
                   "Telephone passes what a phone line carries and Megaphone rings at its two horn resonances",
                   "telephone " + juce::String (tel.responseDb (100.0f), 0) + " / " + juce::String (tel.responseDb (1000.0f), 0) + " / " + juce::String (tel.responseDb (8000.0f), 0)
                   + " dB at 100 Hz / 1 kHz / 8 kHz; megaphone " + juce::String (horn.responseDb (1100.0f), 1) + " / " + juce::String (horn.responseDb (1800.0f), 1) + " / " + juce::String (horn.responseDb (2800.0f), 1) + " dB at 1.1 / 1.8 / 2.8 kHz");

            // noise is there while the track sounds and gone in the gaps; with Noise at zero silence stays silence
            auto phrase = [&] (int mode, float noise)
            {
                Phone ph; ph.prepare (fs, 2);
                PhoneSettings st; st.enabled = true; st.mode = mode; st.noise = noise; st.drive = 0.3f; ph.setSettings (st);
                juce::AudioBuffer<float> b (2, (int) (fs * 4.0)); b.clear();
                for (int i = 0; i < (int) fs; ++i) { const float v = 0.2f * (float) std::sin (juce::MathConstants<double>::twoPi * 700.0 * i / fs); b.setSample (0, i, v); b.setSample (1, i, v); }
                inBlocks (b, 256, [&ph] (juce::AudioBuffer<float>& x) { ph.process (x); });
                return b;
            };
            const auto quiet = phrase (1, 0.0f), hissy = phrase (1, 0.6f), walkie = phrase (4, 0.6f);
            const int sec = (int) fs;
            double hiss = 0.0;
            for (int i = sec / 2; i < sec; ++i) hiss += std::pow ((double) hissy.getSample (0, i) - quiet.getSample (0, i), 2.0);
            const float hissDb = 10.0f * (float) std::log10 (hiss / (sec / 2) + 1.0e-14);
            check (quiet.getMagnitude (0, sec * 3, sec / 2) < 1.0e-6f && hissDb > -60.0f && hissDb < -30.0f && band (hissy, sec * 3, sec / 2) < -90.0f,
                   "Phone noise is there while the track is sounding, gone two seconds after it stops, and not there at all with Noise at zero",
                   juce::String (hissDb, 0) + " dB of hiss under a voice at -17 dB; " + juce::String (band (hissy, sec * 3, sec / 2), 0) + " dB two seconds after the voice stops");
            const float burstDb = band (walkie, sec + (int) (fs * 0.045), (int) (fs * 0.05)), afterDb = band (walkie, sec + (int) (fs * 1.5), (int) (fs * 0.05));
            check (burstDb > afterDb + 12.0f && burstDb > -45.0f, "Walkie-talkie ends each phrase with a burst of noise",
                   juce::String (burstDb, 0) + " dB as the phrase ends, " + juce::String (afterDb, 0) + " dB a second and a half later");

            // the radio levels the voice: 20 dB between a quiet word and a loud one becomes much less
            auto radioOut = [&] (float amp)
            {
                Phone ph; ph.prepare (fs, 2);
                PhoneSettings st; st.enabled = true; st.mode = 3; st.drive = 0.2f; ph.setSettings (st);
                auto t = sine (fs, 900.0f, amp, (int) (fs * 2.0));
                inBlocks (t, 256, [&ph] (juce::AudioBuffer<float>& x) { ph.process (x); });
                return juce::Decibels::gainToDecibels (t.getRMSLevel (0, (int) fs, (int) fs));
            };
            const float spread = radioOut (0.3f) - radioOut (0.03f);
            check (spread < 12.0f, "Radio's automatic gain brings a quiet voice up towards a loud one", "20 dB apart going in, " + juce::String (spread, 1) + " dB apart coming out");
        }

        // --- build 17: the Lo-fi modules, one at a time
        {
            auto run = [&] (LoFiSettings st, juce::AudioBuffer<float> in)
            {
                LoFi lf; lf.prepare (fs, 2); st.enabled = true; st.mix = 1.0f; lf.setSettings (st);
                inBlocks (in, 256, [&lf] (juce::AudioBuffer<float>& x) { lf.process (x); });
                return in;
            };
            LoFiSettings plain; plain.crush = 0.0f; plain.wobble = 0.0f;
            // Tone
            auto dark = plain; dark.tone = 0.3f;
            const float open8k = band (run (plain, sine (fs, 8000.0f, 0.2f, (int) fs)), (int) fs / 2, (int) fs / 2), dull8k = band (run (dark, sine (fs, 8000.0f, 0.2f, (int) fs)), (int) fs / 2, (int) fs / 2);
            const float dull300 = band (run (dark, sine (fs, 300.0f, 0.2f, (int) fs)), (int) fs / 2, (int) fs / 2), open300 = band (run (plain, sine (fs, 300.0f, 0.2f, (int) fs)), (int) fs / 2, (int) fs / 2);
            check (dull8k < open8k - 12.0f && std::abs (dull300 - open300) < 1.0f, "Lo-fi Tone rolls the top away and leaves the body alone",
                   "8 kHz " + juce::String (dull8k - open8k, 1) + " dB, 300 Hz " + juce::String (dull300 - open300, 1) + " dB at Tone 30 %");
            // Drive: third harmonic of a 200 Hz tone
            auto third = [&] (const juce::AudioBuffer<float>& b)
            {
                double re1 = 0, im1 = 0, re3 = 0, im3 = 0;
                for (int i = (int) fs / 2; i < (int) fs; ++i)
                {
                    const double t = (double) i / fs, x = b.getSample (0, i);
                    re1 += x * std::cos (juce::MathConstants<double>::twoPi * 200.0 * t); im1 += x * std::sin (juce::MathConstants<double>::twoPi * 200.0 * t);
                    re3 += x * std::cos (juce::MathConstants<double>::twoPi * 600.0 * t); im3 += x * std::sin (juce::MathConstants<double>::twoPi * 600.0 * t);
                }
                return (float) (10.0 * std::log10 ((re3 * re3 + im3 * im3) / (re1 * re1 + im1 * im1 + 1.0e-20) + 1.0e-20));
            };
            auto hot = plain; hot.drive = 0.7f;
            const float cleanH3 = third (run (plain, sine (fs, 200.0f, 0.5f, (int) fs))), drivenH3 = third (run (hot, sine (fs, 200.0f, 0.5f, (int) fs)));
            check (cleanH3 < -60.0f && drivenH3 > -30.0f, "Lo-fi Drive saturates, and at zero adds nothing", "third harmonic " + juce::String (cleanH3, 0) + " dB clean, " + juce::String (drivenH3, 0) + " dB at Drive 70 %");
            // Wear: the level moves about
            auto swing = [&] (const juce::AudioBuffer<float>& b)
            {
                float lo = 1.0e9f, hi = -1.0e9f; const int win = (int) (fs * 0.05);
                for (int at = (int) fs; at + win < b.getNumSamples(); at += win) { const float l = juce::Decibels::gainToDecibels (b.getRMSLevel (0, at, win)); lo = std::min (lo, l); hi = std::max (hi, l); }
                return hi - lo;
            };
            auto worn = plain; worn.wear = 1.0f;
            const float steady = swing (run (plain, sine (fs, 500.0f, 0.3f, (int) (fs * 12.0)))), moving = swing (run (worn, sine (fs, 500.0f, 0.3f, (int) (fs * 12.0))));
            check (steady < 0.2f && moving > 3.0f, "Lo-fi Wear makes the level sag and drop out, and at zero leaves it steady", juce::String (steady, 2) + " dB of movement at zero, " + juce::String (moving, 1) + " dB at full");
            // Noise: under the sound while it plays, gone when it stops
            auto noisy = plain; noisy.noise = 0.6f; noisy.noiseType = 1;
            juce::AudioBuffer<float> phrase (2, (int) (fs * 6.0)); phrase.clear();
            for (int i = 0; i < (int) fs; ++i) { const float v = 0.2f * (float) std::sin (juce::MathConstants<double>::twoPi * 400.0 * i / fs); phrase.setSample (0, i, v); phrase.setSample (1, i, v); }
            const auto withNoise = run (noisy, phrase), without = run (plain, phrase);
            double diff = 0.0, sideDiff = 0.0;
            for (int i = (int) fs / 2; i < (int) fs; ++i) { diff += std::pow ((double) withNoise.getSample (0, i) - without.getSample (0, i), 2.0); sideDiff += std::pow ((double) withNoise.getSample (0, i) - withNoise.getSample (1, i), 2.0); }
            const float added = 10.0f * (float) std::log10 (diff / (fs / 2) + 1.0e-14);
            check (added > -70.0f && added < -30.0f && sideDiff > diff * 0.5 && band (withNoise, (int) (fs * 5.5), (int) (fs * 0.4)) < -90.0f,
                   "Lo-fi Noise sits under the sound, is different on each side, and fades away once the track stops",
                   juce::String (added, 0) + " dB of hiss under a voice at -17 dB; " + juce::String (band (withNoise, (int) (fs * 5.5), (int) (fs * 0.4)), 0) + " dB four and a half seconds after it stops");
        }

        // --- build 17: Motion's shapes, the drawn curve, and the three new things it can move
        {
            MotionSettings ms;
            bool shapes = near (Motion::valueAt (ms, 0.0f), 0.5f, 1.0e-4f) && near (Motion::valueAt (ms, 0.25f), 1.0f, 1.0e-4f);
            ms.shape = 1; shapes = shapes && near (Motion::valueAt (ms, 0.25f), 0.5f, 1.0e-4f) && near (Motion::valueAt (ms, 0.5f), 1.0f, 1.0e-4f);
            ms.shape = 2; shapes = shapes && near (Motion::valueAt (ms, 0.3f), 0.3f, 1.0e-4f);
            ms.shape = 3; shapes = shapes && near (Motion::valueAt (ms, 0.3f), 0.7f, 1.0e-4f);
            ms.shape = 4; shapes = shapes && near (Motion::valueAt (ms, 0.2f), 1.0f, 1.0e-4f) && near (Motion::valueAt (ms, 0.7f), 0.0f, 1.0e-4f);
            ms.shape = 5; for (int i = 0; i < 16; ++i) ms.curve[(size_t) i] = (i % 2 == 0) ? 0.2f : 0.8f;
            ms.smooth = 0.0f; const bool steps = near (Motion::valueAt (ms, 0.5f / 16.0f), 0.2f, 1.0e-4f) && near (Motion::valueAt (ms, 1.9f / 16.0f), 0.8f, 1.0e-4f);
            ms.smooth = 1.0f; const bool glides = near (Motion::valueAt (ms, 0.5f / 16.0f), 0.5f, 1.0e-3f) && near (Motion::valueAt (ms, 15.5f / 16.0f), 0.5f, 1.0e-3f);
            check (shapes && steps && glides, "Motion's shapes are what their names say, and a drawn curve is sixteen steps at Smooth 0 and a line through its points at Smooth 100");

            auto move = [&] (MotionSettings st, juce::AudioBuffer<float> in, bool timeline)
            {
                Motion m; m.prepare (fs, 2); st.enabled = true; m.setSettings (st);
                int at = 0;
                inBlocks (in, 256, [&] (juce::AudioBuffer<float>& x) { m.setTimeline (timeline, (double) at / fs * 2.0); m.process (x); at += x.getNumSamples(); });       // 120 BPM: two beats a second
                return in;
            };
            // Chorus on a plain wave: the copy that moves at half speed has a clock of its own and never jumps. (It
            // used to be worked out from the main one, and jumped by milliseconds once a cycle.)
            MotionSettings chorus; chorus.type = 0; chorus.rateHz = 2.0f; chorus.depth = 1.0f; chorus.mix = 1.0f;
            const auto swirled = move (chorus, sine (fs, 300.0f, 0.3f, (int) (fs * 4.0)), false);
            float worst = 0.0f;
            for (int i = (int) fs; i < swirled.getNumSamples(); ++i) worst = std::max (worst, std::abs (swirled.getSample (0, i) - 2.0f * swirled.getSample (0, i - 1) + swirled.getSample (0, i - 2)));
            const float toneCurve = 0.3f * (float) std::pow (juce::MathConstants<double>::twoPi * 300.0 / fs, 2.0);
            check (worst < toneCurve * 4.0f, "Chorus has no jump in it as its second copy goes round", "largest step " + juce::String (worst / toneCurve, 1) + " times the tone's own curve");

            // Volume, ramp up, one cycle a beat, locked to the song: low at the start of every beat, back up by the end
            MotionSettings pump; pump.type = 5; pump.shape = 2; pump.sync = true; pump.note = 4; pump.bpm = 120.0; pump.depth = 0.8f; pump.mix = 1.0f;
            const auto pumped = move (pump, sine (fs, 500.0f, 0.4f, (int) (fs * 3.0)), true);
            const int beat = (int) (fs * 0.5);
            const float early = pumped.getRMSLevel (0, beat * 4 + beat / 20, beat / 10), late = pumped.getRMSLevel (0, beat * 4 + beat * 17 / 20, beat / 10);
            check (juce::Decibels::gainToDecibels (late / early) > 9.0f && late > 0.22f, "Motion on Volume with a ramp, timed to a beat, ducks at the start of each beat and swells back",
                   juce::String (juce::Decibels::gainToDecibels (early / 0.2828f), 1) + " dB at the start of a beat, " + juce::String (juce::Decibels::gainToDecibels (late / 0.2828f), 1) + " dB at the end");

            // Pan: square, a beat each side; the loudness stays put while the sound changes sides
            MotionSettings pan; pan.type = 4; pan.shape = 4; pan.sync = true; pan.note = 3; pan.bpm = 120.0; pan.depth = 1.0f; pan.mix = 1.0f;
            const auto panned = move (pan, sine (fs, 500.0f, 0.4f, (int) (fs * 3.0)), true);
            const float l1 = panned.getRMSLevel (0, beat * 4 + beat / 2, beat / 4), r1 = panned.getRMSLevel (1, beat * 4 + beat / 2, beat / 4);
            const float l2 = panned.getRMSLevel (0, beat * 5 + beat / 2, beat / 4), r2 = panned.getRMSLevel (1, beat * 5 + beat / 2, beat / 4);
            const float power1 = 10.0f * std::log10 (l1 * l1 + r1 * r1), power2 = 10.0f * std::log10 (l2 * l2 + r2 * r2), centre = 10.0f * std::log10 (2.0f * 0.2828f * 0.2828f);
            check (r1 > l1 * 10.0f && l2 > r2 * 10.0f && std::abs (power1 - centre) < 0.5f && std::abs (power2 - centre) < 0.5f, "Motion on Pan moves the sound from one side to the other at the same loudness",
                   "right then left, " + juce::String (power1 - centre, 2) + " and " + juce::String (power2 - centre, 2) + " dB against the middle");

            // Filter: shut at the bottom of the movement, open at the top
            MotionSettings filter; filter.type = 3; filter.shape = 4; filter.sync = true; filter.note = 3; filter.bpm = 120.0; filter.depth = 1.0f; filter.mix = 1.0f;
            const auto swept = move (filter, sine (fs, 6000.0f, 0.3f, (int) (fs * 3.0)), true);
            const float openDb = juce::Decibels::gainToDecibels (swept.getRMSLevel (0, beat * 4 + beat / 2, beat / 4) / 0.2121f), shutDb = juce::Decibels::gainToDecibels (swept.getRMSLevel (0, beat * 5 + beat / 2, beat / 4) / 0.2121f);
            check (openDb > -1.5f && shutDb < -30.0f, "Motion on Filter lets 6 kHz through at the top of the movement and shuts it out at the bottom", juce::String (openDb, 1) + " dB open, " + juce::String (shutDb, 0) + " dB shut");

            // a curve with a cliff in it must not click on Volume
            MotionSettings cliff; cliff.type = 5; cliff.shape = 5; cliff.smooth = 0.0f; cliff.rateHz = 4.0f; cliff.depth = 1.0f; cliff.mix = 1.0f;
            for (int i = 0; i < 16; ++i) cliff.curve[(size_t) i] = i < 8 ? 1.0f : 0.0f;
            const auto gated = move (cliff, sine (fs, 200.0f, 0.4f, (int) (fs * 2.0)), false);
            float jump = 0.0f; for (int i = (int) fs / 2; i < gated.getNumSamples(); ++i) jump = std::max (jump, std::abs (gated.getSample (0, i) - gated.getSample (0, i - 1)));
            check (jump < 0.02f, "A drawn curve that jumps does not click", "largest step between samples " + juce::String (jump, 4) + " on a tone of 0.4");
        }

        // --- build 17: Chop's pattern
        {
            ChopSettings ps; ps.mode = 1; ps.length = 8; ps.gate = 1.0f;
            for (int i = 0; i < 16; ++i) ps.steps[(size_t) i] = 0.0f;
            ps.steps[0] = 1.0f; ps.steps[1] = 0.5f; ps.steps[3] = 1.0f;
            bool grid = near (Chop::patternAt (ps, 0.5), 1.0f, 1.0e-5f) && near (Chop::patternAt (ps, 1.5), 0.5f, 1.0e-5f) && near (Chop::patternAt (ps, 2.5), 0.0f, 1.0e-5f)
                        && near (Chop::patternAt (ps, 8.5), 1.0f, 1.0e-5f) && near (Chop::patternAt (ps, 11.2), 1.0f, 1.0e-5f);            // wraps after eight steps
            ps.gate = 0.5f; grid = grid && near (Chop::patternAt (ps, 0.3), 1.0f, 1.0e-5f) && near (Chop::patternAt (ps, 0.7), 0.0f, 1.0e-5f);
            ps.gate = 1.0f; ps.swing = 1.0f;       // every second step half a step late: step 2 (index 1) starts at 1.5, and step 1 runs on until then
            grid = grid && near (Chop::patternAt (ps, 1.3), 1.0f, 1.0e-5f) && near (Chop::patternAt (ps, 1.7), 0.5f, 1.0e-5f);
            check (grid, "Chop's pattern plays each step at its level, starts again after the chosen number of steps, and Gate and Swing do what they say");

            Chop c; c.prepare (fs);
            ChopSettings cs; cs.enabled = true; cs.mode = 1; cs.rate = 3; cs.depth = 1.0f; cs.shape = 1.0f; cs.bpm = 120.0; cs.length = 4;
            for (int i = 0; i < 16; ++i) cs.steps[(size_t) i] = 0.0f;
            cs.steps[0] = 1.0f; cs.steps[2] = 0.5f;
            c.setSettings (cs);
            auto tone = sine (fs, 500.0f, 0.5f, (int) fs);
            int pos = 0;
            inBlocks (tone, 256, [&] (juce::AudioBuffer<float>& b) { c.setTimeline (true, 16.0 + (double) pos / fs * 2.0); c.process (b); pos += b.getNumSamples(); });
            const int step = (int) (fs * 0.125);
            auto level = [&] (int index) { return tone.getRMSLevel (0, step * (4 + index) + step / 4, step / 2) / 0.3536f; };
            check (near (level (0), 1.0f, 0.03f) && level (1) < 0.01f && near (level (2), 0.5f, 0.03f) && level (3) < 0.01f && near (level (4), 1.0f, 0.03f),
                   "Chop's pattern is locked to the song: full, gap, half, gap, and round again",
                   juce::String (level (0), 2) + " " + juce::String (level (1), 3) + " " + juce::String (level (2), 2) + " " + juce::String (level (3), 3) + " " + juce::String (level (4), 2));
        }
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

    // ================================================================ build 12: pulse-locked pitch engine, compressor, reverb, delay
    // A voice-like test signal: a pulse on every period, like the vocal folds make, through three resonances that
    // give it a vowel, with a little breath noise. The pitch follows whatever function it is given.
    auto vowel = [fs] (double seconds, const std::function<double (double)>& hzAt, float breath = 0.003f, float jitter = 0.0f)
    {
        juce::AudioBuffer<float> b (1, (int) (fs * seconds));
        ColourBiquad f1, f2, f3;
        f1.peak (650.0f, 6.0f, 18.0f, fs); f2.peak (1100.0f, 7.0f, 15.0f, fs); f3.peak (2500.0f, 8.0f, 14.0f, fs);
        juce::Random rng (3);
        double phase = 0.0, wobbleNow = 1.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            // jitter: every period comes out a little longer or shorter than the last, as in a real voice
            if (jitter > 0.0f && std::fmod (phase, juce::MathConstants<double>::twoPi) < juce::MathConstants<double>::twoPi * 150.0 / fs) wobbleNow = 1.0 + (double) jitter * (rng.nextDouble() * 2.0 - 1.0);
            const double hz = hzAt ((double) i / fs) * wobbleNow;
            phase += juce::MathConstants<double>::twoPi * hz / fs;
            float x = 0.0f;
            for (int k = 1; (double) k * hz < 5500.0; ++k) x += (float) std::cos ((double) k * phase) / (float) k;
            x = x * 0.05f + breath * (rng.nextFloat() * 2.0f - 1.0f);
            b.setSample (0, i, f3.run (0, f2.run (0, f1.run (0, x))) * 0.25f);
        }
        return b;
    };
    // How much the level in a band wobbles from one 20 ms slice to the next, in dB. On a steady vowel it should
    // hardly move; a pitch shifter that smears the voice on and off makes it swing.
    auto bandWobble = [fs] (const juce::AudioBuffer<float>& b, float loHz, float hiHz, double fromSeconds)
    {
        ColourBiquad hp, lp; hp.highPass (loHz, 0.707f, fs); lp.lowPass (hiHz, 0.707f, fs);
        std::vector<float> levels;
        const int slice = (int) (fs * 0.02);
        double sum = 0.0; int count = 0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = lp.run (0, hp.run (0, b.getSample (0, i)));
            if (i < (int) (fs * fromSeconds)) continue;
            sum += (double) v * v;
            if (++count == slice) { levels.push_back ((float) (10.0 * std::log10 (sum / slice + 1.0e-12))); sum = 0.0; count = 0; }
        }
        double mean = 0.0; for (float l : levels) mean += l; mean /= (double) juce::jmax ((size_t) 1, levels.size());
        double var = 0.0; for (float l : levels) var += ((double) l - mean) * ((double) l - mean);
        return (float) std::sqrt (var / (double) juce::jmax ((size_t) 1, levels.size()));
    };
    {
        // --- old slicing against pulse-locked slicing, same voice, same shift (a major third up, as a harmony uses)
        const auto drifting = [] (double t) { return 140.0 * std::pow (2.0, (35.0 * std::sin (juce::MathConstants<double>::twoPi * 0.8 * t) + 12.0 * std::sin (juce::MathConstants<double>::twoPi * 5.3 * t)) / 1200.0); };
        const auto voice = vowel (4.0, drifting);
        auto shiftWith = [&] (bool locked, float ratio)
        {
            GrainEngine engine; engine.prepare (1, fs);
            GrainEngine::Voice v; engine.resetVoice (v, (float) (fs * 0.02));
            PitchDetector d; d.prepare (fs);
            juce::AudioBuffer<float> out (1, voice.getNumSamples());
            float period = (float) (fs / 140.0), now = period; bool voiced = false;
            for (int i = 0; i < voice.getNumSamples(); ++i)
            {
                const float x = voice.getSample (0, i);
                engine.write (0, x);
                if (d.push (x)) { voiced = d.frequency() > 0.0f; if (voiced) period = (float) (fs / d.frequency()); }
                now += (period - now) * 0.007f;
                float y = 0.0f;
                if (locked) engine.renderSync (v, now, voiced, ratio, (float) (fs * 0.02), &y);
                else        engine.render (v, period, voiced, ratio, (float) (fs * 0.02), &y);
                out.setSample (0, i, y);
                engine.advance();
            }
            return out;
        };
        const float dry = bandWobble (voice, 1800.0f, 3400.0f, 1.0);
        const auto oldUp = shiftWith (false, 1.26f), newUp = shiftWith (true, 1.26f), oldDown = shiftWith (false, 0.5f), newDown = shiftWith (true, 0.5f);
        const float wOld = bandWobble (oldUp, 1800.0f, 3400.0f, 1.0), wNew = bandWobble (newUp, 1800.0f, 3400.0f, 1.0);
        const float dOld = bandWobble (oldDown, 1800.0f, 3400.0f, 1.0), dNew = bandWobble (newDown, 1800.0f, 3400.0f, 1.0);
        std::printf ("      pulse-locked engine: presence-band wobble, dry voice %.2f dB; third up %.2f -> %.2f dB; octave down %.2f -> %.2f dB\n", dry, wOld, wNew, dOld, dNew);
        // a rougher, speech-like voice: the pitch sliding fast inside each syllable, 1.5 % jitter, more breath
        const auto talking = [] (double t) { const double s = std::fmod (t, 0.3) / 0.3; return 115.0 * std::pow (2.0, (7.0 * s * (1.0 - s) * 4.0 - 2.0 * s) / 12.0); };
        const auto rough = vowel (4.0, talking, 0.012f, 0.015f);
        auto clarityOf = [fs] (const juce::AudioBuffer<float>& b)
        {
            PitchDetector d; d.prepare (fs);
            double sum = 0.0; int n = 0;
            for (int i = (int) fs; i < b.getNumSamples(); ++i) if (d.push (b.getSample (0, i)) && d.frequency() > 0.0f) { sum += d.clarity(); ++n; }
            return n > 0 ? (float) (sum / n) : 0.0f;
        };
        auto shiftRough = [&] (bool locked, float ratio)
        {
            GrainEngine engine; engine.prepare (1, fs);
            GrainEngine::Voice v; engine.resetVoice (v, (float) (fs * 0.02));
            PitchDetector d; d.prepare (fs);
            juce::AudioBuffer<float> out (1, rough.getNumSamples());
            float period = (float) (fs / 140.0), now = period; bool voiced = false;
            for (int i = 0; i < rough.getNumSamples(); ++i)
            {
                const float x = rough.getSample (0, i);
                engine.write (0, x);
                if (d.push (x)) { voiced = d.frequency() > 0.0f; if (voiced) period = (float) (fs / d.frequency()); }
                now += (period - now) * 0.007f;
                float y = 0.0f;
                if (locked) engine.renderSync (v, now, voiced, ratio, (float) (fs * 0.02), &y);
                else        engine.render (v, period, voiced, ratio, (float) (fs * 0.02), &y);
                out.setSample (0, i, y);
                engine.advance();
            }
            return out;
        };
        // What the measurements show, and what is asserted. On a clean, steady voice the two engines measure the
        // same. The difference appears on a voice with real roughness (pitch moving inside each syllable, every
        // period a little different from the last): shifted up, the pulse-locked slices keep the tone steadier.
        // Shifted down an octave they are no better, and that is stated rather than hidden.
        float roughOld[3] {}, roughNew[3] {}; int k = 0;
        for (float ratio : { 1.06f, 1.26f, 0.5f })
        {
            const auto a = shiftRough (false, ratio), b2 = shiftRough (true, ratio);
            roughOld[k] = bandWobble (a, 1800.0f, 3400.0f, 1.0); roughNew[k] = bandWobble (b2, 1800.0f, 3400.0f, 1.0); ++k;
            std::printf ("      rough voice x%.2f: clarity dry %.3f old %.3f new %.3f; wobble dry %.2f old %.2f new %.2f dB\n", ratio, clarityOf (rough), clarityOf (a), clarityOf (b2),
                         bandWobble (rough, 1800.0f, 3400.0f, 1.0), roughOld[k - 1], roughNew[k - 1]);
        }
        check (wNew <= wOld + 0.1f && wNew < dry + 0.3f, "Clean voice, a third up: pulse-locked slicing is as steady as the old slicing and close to the dry voice",
               juce::String (wOld, 2) + " dB of tone wobble before, " + juce::String (wNew, 2) + " dB now (the dry voice has " + juce::String (dry, 2) + ")");
        check (roughNew[0] < roughOld[0] * 0.85f && roughNew[1] < roughOld[1] * 0.85f, "Rough voice shifted up: pulse-locked slicing keeps the tone steadier than the old slicing",
               "a semitone up " + juce::String (roughOld[0], 2) + " -> " + juce::String (roughNew[0], 2) + " dB, a third up " + juce::String (roughOld[1], 2) + " -> " + juce::String (roughNew[1], 2) + " dB");
        check (dNew < dOld + 0.3f && roughNew[2] < roughOld[2] + 0.3f, "An octave down is no worse than before (it is not better either)",
               "clean " + juce::String (dOld, 2) + " -> " + juce::String (dNew, 2) + " dB, rough " + juce::String (roughOld[2], 2) + " -> " + juce::String (roughNew[2], 2) + " dB");
        // the shifted notes are still the right notes
        juce::AudioBuffer<float> tail (1, (int) (fs * 2.5)); tail.copyFrom (0, 0, newUp, 0, (int) fs, tail.getNumSamples());
        juce::AudioBuffer<float> src (1, (int) (fs * 2.5)); src.copyFrom (0, 0, voice, 0, (int) (fs * 0.98), src.getNumSamples());
        check (std::abs (meanOf (trackNotes (tail, 0.1f)) - meanOf (trackNotes (src, 0.1f)) - 4.0f) < 0.06f, "A third up is still exactly four semitones",
               juce::String (meanOf (trackNotes (tail, 0.1f)) - meanOf (trackNotes (src, 0.1f)), 3));
    }

    {
        // --- Tune: the new controls. A voice singing 35 cents flat of A3, hard tuned in A major.
        const float flatA = 220.0f * std::pow (2.0f, -35.0f / 1200.0f);
        auto tuned = [&] (float hz, const std::function<void (TunerSettings&)>& set, float seconds = 2.0f)
        {
            Tuner t; t.prepare (fs, 256, 1);
            TunerSettings ts; ts.enabled = true; ts.key = 9; ts.scale = 1; ts.speedMs = 0.0f; ts.keepVibrato = 0.0f; ts.hold = 0.3f;
            set (ts); t.setSettings (ts);
            auto b = voiceTone (hz, seconds, 1);
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { t.process (v); });
            return meanOf (trackNotes (b, 0.6f));
        };
        const float plain = tuned (flatA, [] (TunerSettings&) {});
        check (std::abs (plain - 57.0f) < 0.04f, "Hard tune puts a note 35 cents flat exactly on the note", juce::String (plain, 3));
        // Flex: within the tolerance the note is left as sung; well outside it, it is corrected in full
        const float close = 220.0f * std::pow (2.0f, -8.0f / 1200.0f);
        const float leftAlone = tuned (close, [] (TunerSettings& ts) { ts.flexCents = 20.0f; });
        const float stillFixed = tuned (flatA, [] (TunerSettings& ts) { ts.flexCents = 20.0f; });
        check (std::abs (leftAlone - (57.0f - 0.08f)) < 0.02f && std::abs (stillFixed - 57.0f) < 0.04f, "Flex leaves a note that is 8 cents out as it was sung and still corrects one that is 35 cents out",
               juce::String ((leftAlone - 57.0f) * 100.0f, 1) + " cents and " + juce::String ((stillFixed - 57.0f) * 100.0f, 1) + " cents from the note");

        // Gentle tuning, note to note: a note sung 40 cents flat, then a note sung in tune, over and over. The
        // correction the flat note needed must not be carried on to the note that needed none.
        {
            Tuner t; t.prepare (fs, 256, 1);
            TunerSettings ts; ts.enabled = true; ts.key = 9; ts.scale = 1; ts.speedMs = 80.0f; ts.keepVibrato = 0.0f; ts.hold = 0.3f;
            t.setSettings (ts);
            const int n = (int) (fs * 5.0);
            juce::AudioBuffer<float> b (1, n);
            double phase = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double c = std::fmod ((double) i / fs, 0.6);
                const double sung = c < 0.3 ? 56.6 : 61.0;                                           // A3 40 cents flat, then C#4 in tune
                phase += juce::MathConstants<double>::twoPi * 440.0 * std::pow (2.0, (sung - 69.0) / 12.0) / fs;
                float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) std::sin (phase * h) / (float) h;
                b.setSample (0, i, 0.2f * v);
            }
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { t.process (v); });
            PitchDetector d; d.prepare (fs);
            double early = 0.0, late = 0.0, flatLate = 0.0; int earlyN = 0, lateN = 0, flatN = 0;
            for (int i = 0; i < n; ++i)
            {
                if (! d.push (b.getSample (0, i)) || d.frequency() <= 0.0f) continue;
                const double tt = (double) i / fs - 0.045;
                if (tt < 1.2) continue;
                const float out = 69.0f + 12.0f * std::log2 (d.frequency() / 440.0f);
                const double c = std::fmod (tt, 0.6);
                if (c > 0.36 && c < 0.44) { early += std::abs (out - 61.0f); ++earlyN; }             // 60 to 140 ms into the in-tune note
                if (c > 0.48 && c < 0.58) { late += std::abs (out - 61.0f); ++lateN; }
                if (c > 0.2 && c < 0.28)  { flatLate += std::abs (out - 57.0f); ++flatN; }
            }
            const float e = (float) (early / juce::jmax (1, earlyN)) * 100.0f, l = (float) (late / juce::jmax (1, lateN)) * 100.0f, f = (float) (flatLate / juce::jmax (1, flatN)) * 100.0f;
            check (e < 8.0f && l < 4.0f && f < 8.0f, "Gentle tuning: the correction a flat note needed is not carried on to the next note, and the flat note is still brought in",
                   "in-tune note " + juce::String (e, 1) + " cents off early on and " + juce::String (l, 1) + " later; the flat note ends " + juce::String (f, 1) + " cents off (sung 40 off)");
        }

        // Keep slides, on the shape real singing has: slide up to a note, hold it (30 cents flat), slide down to the
        // next, hold that (30 cents flat). While the voice is travelling it is left nearer to what was sung; once it
        // has landed the note is tuned, straight away and not a fifth of a second later.
        struct SlideResult { float movedWhileSliding = 0.0f, offWhileHeld = 0.0f; };
        auto slide = [&] (float keep)
        {
            Tuner t; t.prepare (fs, 256, 1);
            TunerSettings ts; ts.enabled = true; ts.key = 9; ts.scale = 1; ts.speedMs = 40.0f; ts.keepVibrato = 0.5f; ts.hold = 0.3f; ts.keepSlides = keep;
            t.setSettings (ts);
            const int n = (int) (fs * 5.0);
            juce::AudioBuffer<float> b (1, n), dryCopy;
            auto sungAt = [] (double tt)
            {
                const double c = std::fmod (tt, 1.0), lo = 56.7, hi = 58.7;                      // A3 and B3, both 30 cents flat
                if (c < 0.25) return lo + (hi - lo) * 0.5 * (1.0 - std::cos (juce::MathConstants<double>::pi * c / 0.25));
                if (c < 0.5)  return hi;
                if (c < 0.75) return hi + (lo - hi) * 0.5 * (1.0 - std::cos (juce::MathConstants<double>::pi * (c - 0.5) / 0.25));
                return lo;
            };
            double phase = 0.0;
            for (int i = 0; i < n; ++i)
            {
                phase += juce::MathConstants<double>::twoPi * 440.0 * std::pow (2.0, (sungAt ((double) i / fs) - 69.0) / 12.0) / fs;
                float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) std::sin (phase * h) / (float) h;
                b.setSample (0, i, 0.2f * v);
            }
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { t.process (v); });
            // the pitch that comes out, moment by moment, against the pitch that went in 20 ms earlier (the tuner's delay)
            PitchDetector d; d.prepare (fs);
            SlideResult r; int slides = 0, holds = 0;
            for (int i = 0; i < n; ++i)
            {
                if (! d.push (b.getSample (0, i)) || d.frequency() <= 0.0f) continue;
                const double tt = (double) i / fs - 0.045;                                        // the tuner's delay and half the detector's window
                if (tt < 1.0) continue;
                const float out = 69.0f + 12.0f * std::log2 (d.frequency() / 440.0f);
                const double c = std::fmod (tt, 1.0);
                const bool sliding = (c > 0.06 && c < 0.19) || (c > 0.56 && c < 0.69);
                const bool held = (c > 0.38 && c < 0.48) || (c > 0.88 && c < 0.98);                // 130 ms after landing, to the end of the note
                if (sliding) { r.movedWhileSliding += std::abs (out - (float) sungAt (tt)); ++slides; }
                if (held)    { r.offWhileHeld += std::abs (out - (c < 0.5 ? 59.0f : 57.0f)); ++holds; }
            }
            r.movedWhileSliding *= 100.0f / (float) juce::jmax (1, slides);
            r.offWhileHeld *= 100.0f / (float) juce::jmax (1, holds);
            return r;
        };
        const auto pulled = slide (0.0f), kept = slide (1.0f);
        check (kept.movedWhileSliding < pulled.movedWhileSliding * 0.9f && pulled.movedWhileSliding > 8.0f, "Keep slides leaves a sliding voice nearer to what was sung",
               juce::String (pulled.movedWhileSliding, 1) + " cents moved without it, " + juce::String (kept.movedWhileSliding, 1) + " cents with it");
        check (kept.offWhileHeld < 8.0f && pulled.offWhileHeld < 8.0f, "A note the voice slid into is tuned once it lands, with Keep slides on or off (sung 30 cents flat)",
               juce::String (pulled.offWhileHeld, 1) + " cents off without it, " + juce::String (kept.offWhileHeld, 1) + " cents with it");
        const float heldStill = tuned (flatA, [] (TunerSettings& ts) { ts.speedMs = 40.0f; ts.keepSlides = 1.0f; }, 3.0f);
        check (std::abs (heldStill - 57.0f) < 0.05f, "With keep slides on, a held note is still tuned", juce::String ((heldStill - 57.0f) * 100.0f, 1) + " cents from the note");
    }

    {
        // --- Compressor: the gain must not ride each cycle of a low note. A 110 Hz tone and a 110 Hz + 2.5 kHz pair,
        // 18 dB over the threshold, fast settings. Build 11 measured -42 dB (Punch) and -51 dB (Clean) on this test.
        auto harmonicsDb = [&] (int style, bool pair)
        {
            Compressor c; c.prepare (fs, 256, 2);
            CompressorSettings st; st.enabled = true; st.thresholdDb = -30.0f; st.ratio = 4.0f; st.attackMs = 3.0f; st.releaseMs = 60.0f; st.kneeDb = 3.0f; st.style = (CompStyle) style;
            c.setSettings (st);
            const int n = (int) (fs * 4.0);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 110.0 * i / fs)
                              + (pair ? 0.063f * (float) std::sin (juce::MathConstants<double>::twoPi * 2500.0 * i / fs) : 0.0f);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { c.process (v); });
            const int from = (int) (fs * 2.0);
            auto db = [&] (float hz) { return juce::Decibels::gainToDecibels (amountAt (b, 0, hz, from), -200.0f); };
            if (! pair) return juce::jmax (db (220.0f), db (330.0f)) - db (110.0f);
            return juce::jmax (db (2390.0f), db (2610.0f), db (2280.0f)) - db (2500.0f);
        };
        juce::String detail; float worst = -200.0f, worstPair = -200.0f;
        for (int style = 0; style < 5; ++style)
        {
            const float h = harmonicsDb (style, false), im = harmonicsDb (style, true);
            worst = juce::jmax (worst, h); worstPair = juce::jmax (worstPair, im);
            detail << Compressor::styleNames()[style] << " " << juce::String (juce::roundToInt (h)) << " / " << juce::String (juce::roundToInt (im)) << " dB  ";
        }
        check (worst < -65.0f && worstPair < -65.0f, "Compressing a low note adds no audible harmonics or sidebands in any style (harmonics / sidebands)", detail);

        // The Vocal style against the same settings without its second stage (Smooth): a voice-like pattern of
        // 150 ms words with a 4 ms spike at the front of each. The catcher takes the spikes down; the body of
        // the words comes out the same.
        auto pattern = [&] (CompStyle style, float& spikeDb, float& bodyDb)
        {
            Compressor c; c.prepare (fs, 256, 2);
            CompressorSettings st; st.enabled = true; st.thresholdDb = -20.0f; st.ratio = 3.0f; st.attackMs = 12.0f; st.releaseMs = 110.0f; st.kneeDb = 8.0f; st.autoMakeup = false; st.style = style;
            c.setSettings (st);
            const int n = (int) (fs * 3.0), word = (int) (fs * 0.3);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const int at = i % word;
                const float level = at < (int) (fs * 0.004) ? 0.9f : at < (int) (fs * 0.15) ? 0.25f : 0.05f;
                const float v = level * (float) std::sin (juce::MathConstants<double>::twoPi * 300.0 * i / fs);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { c.process (v); });
            const int from = word * 6;
            spikeDb = juce::Decibels::gainToDecibels (b.getMagnitude (0, from, (int) (fs * 0.004)));
            bodyDb = juce::Decibels::gainToDecibels (b.getRMSLevel (0, from + (int) (fs * 0.06), (int) (fs * 0.08)));
        };
        // A thump before a word (a plosive, a hand on the stand). Measured on real takes it was often the loudest
        // thing in the recording, and a leveller that hears it holds the following words down. The Vocal style
        // does not listen below 110 Hz, whatever the sidechain filter is set to.
        auto wordAfterThump = [&] (CompStyle style, bool thump)
        {
            Compressor c; c.prepare (fs, 256, 2);
            CompressorSettings st; st.enabled = true; st.thresholdDb = -24.0f; st.ratio = 3.5f; st.attackMs = 10.0f; st.releaseMs = 90.0f; st.kneeDb = 8.0f; st.autoMakeup = false; st.style = style;
            c.setSettings (st);
            const int n = (int) (fs * 3.0), cycle = (int) (fs * 0.5);
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i)
            {
                const int at = i % cycle;
                float v = at >= (int) (fs * 0.06) && at < (int) (fs * 0.3) ? 0.2f * (float) std::sin (juce::MathConstants<double>::twoPi * 400.0 * i / fs) : 0.0f;      // the word
                if (thump && at < (int) (fs * 0.05)) v += 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 55.0 * at / fs);                                  // the thump before it
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            inBlocks (b, 256, [&] (juce::AudioBuffer<float>& v) { c.process (v); });
            return juce::Decibels::gainToDecibels (b.getRMSLevel (0, cycle * 4 + (int) (fs * 0.15), (int) (fs * 0.14)));      // the word, from 100 ms after the thump (the catcher takes the thump itself and lets go in 50 ms)
        };
        const float duckedVocal = wordAfterThump (CompStyle::Vocal, false) - wordAfterThump (CompStyle::Vocal, true);
        const float duckedSmooth = wordAfterThump (CompStyle::Smooth, false) - wordAfterThump (CompStyle::Smooth, true);
        check (std::abs (duckedVocal) < 1.0f && duckedSmooth > 2.0f, "Vocal style: a thump before a word does not push the word down",
               "word " + juce::String (duckedVocal, 1) + " dB lower after a thump in the Vocal style, " + juce::String (duckedSmooth, 1) + " dB lower in Smooth with no sidechain filter");

        float spikeSmooth, bodySmooth, spikeVocal, bodyVocal;
        pattern (CompStyle::Smooth, spikeSmooth, bodySmooth); pattern (CompStyle::Vocal, spikeVocal, bodyVocal);
        check (spikeVocal < spikeSmooth - 1.5f && std::abs (bodyVocal - bodySmooth) < 1.0f, "Vocal style: the catcher takes the spikes down and leaves the body of the words to the leveller",
               "spikes " + juce::String (spikeSmooth, 1) + " dB with the leveller alone, " + juce::String (spikeVocal, 1) + " dB with the catcher; body " + juce::String (bodySmooth, 1) + " / " + juce::String (bodyVocal, 1) + " dB");
    }

    {
        // --- Reverb: nine spaces from five engines. Each is measured from what it does to a single click.
        auto clickThrough = [&] (const std::function<void (ReverbSettings&)>& set, float seconds)
        {
            Reverb r; r.prepare (fs, 512, 2);
            ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = 2.0f; rs.preDelayMs = 0.0f; rs.mix = 1.0f; rs.duck = 0.0f; rs.tone = 0.6f; rs.lowCutHz = 40.0f;
            set (rs); r.setSettings (rs);
            juce::AudioBuffer<float> b (2, (int) (fs * seconds)); b.clear();
            b.setSample (0, 100, 0.9f); b.setSample (1, 100, 0.9f);
            inBlocks (b, 512, [&r] (juce::AudioBuffer<float>& v) { r.process (v); });
            b.setSample (0, 100, b.getSample (0, 100) - 0.9f); b.setSample (1, 100, b.getSample (1, 100) - 0.9f);        // the wet sound only
            return b;
        };
        auto bandOf = [fs] (const juce::AudioBuffer<float>& in, float loHz, float hiHz)
        {
            juce::AudioBuffer<float> b; b.makeCopyOf (in);
            const float lo = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * loHz / (float) fs), hi = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hiHz / (float) fs);
            float a1 = 0.0f, a2 = 0.0f, h1 = 0.0f, h2 = 0.0f;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                float v = b.getSample (0, i);
                h1 += (v - h1) * lo; v -= h1; h2 += (v - h2) * lo; v -= h2;
                a1 += (v - a1) * hi; a2 += (a1 - a2) * hi;
                b.setSample (0, i, a2);
            }
            return b;
        };
        // decay time from the fall between 5 and 35 dB down in the energy still to come
        auto decayOf = [fs] (const juce::AudioBuffer<float>& b)
        {
            std::vector<double> rest ((size_t) b.getNumSamples());
            double acc = 0.0;
            for (int i = b.getNumSamples() - 1; i >= 0; --i) { acc += (double) b.getSample (0, i) * b.getSample (0, i); rest[(size_t) i] = acc; }
            int t5 = -1, t35 = -1;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double db = 10.0 * std::log10 (rest[(size_t) i] / rest[0] + 1e-30);
                if (t5 < 0 && db <= -5.0) t5 = i;
                if (t35 < 0 && db <= -35.0) { t35 = i; break; }
            }
            return (t5 >= 0 && t35 > t5) ? (float) (2.0 * (t35 - t5) / fs) : 0.0f;
        };
        // evenness of a stretch of tail once its own decay is taken out: 3 is smooth noise, well above it means
        // separate echoes stick out
        auto evenness = [fs] (const juce::AudioBuffer<float>& b, float from, float to)
        {
            const int a = (int) (fs * from), n = (int) (fs * (to - from)), w = 960;
            std::vector<double> x ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                double e = 0.0; for (int k = -w / 2; k < w / 2; ++k) { const double v = b.getSample (0, a + i + k); e += v * v; }
                x[(size_t) i] = b.getSample (0, a + i) / (std::sqrt (e / w) + 1e-12);
            }
            double m2 = 0.0, m4 = 0.0; for (double v : x) { m2 += v * v; m4 += v * v * v * v; }
            m2 /= n; m4 /= n;
            return (float) (m4 / (m2 * m2 + 1e-30));
        };
        auto halfEnergyMs = [fs] (const juce::AudioBuffer<float>& b)
        {
            double total = 0.0; for (int i = 0; i < b.getNumSamples(); ++i) total += (double) b.getSample (0, i) * b.getSample (0, i);
            double acc = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i) { acc += (double) b.getSample (0, i) * b.getSample (0, i); if (acc >= 0.5 * total) return (float) (1000.0 * (i - 100) / fs); }
            return 0.0f;
        };

        juce::String decays, smooth; bool decaysOk = true, smoothOk = true, finite = true; float halves[Reverb::NumTypes] {};
        for (int t = 0; t < Reverb::NumTypes; ++t)
        {
            const auto ir = clickThrough ([t] (ReverbSettings& rs) { rs.type = t; }, 6.0f);
            for (int i = 0; i < ir.getNumSamples(); i += 7) finite = finite && std::isfinite (ir.getSample (0, i)) && std::abs (ir.getSample (0, i)) < 4.0f;
            halves[t] = halfEnergyMs (ir);
            if (t == Reverb::Gated) continue;
            const float d = decayOf (bandOf (ir, 500.0f, 2000.0f)), k = evenness (ir, 0.4f, 0.8f);
            decays << Reverb::typeNames()[t] << " " << juce::String (d, 2) << "  "; smooth << Reverb::typeNames()[t] << " " << juce::String (k, 1) << "  ";
            decaysOk = decaysOk && near (d, 2.0f, 0.35f); smoothOk = smoothOk && k > 2.4f && k < 3.8f;
        }
        check (finite, "Every reverb space gives finite, bounded sound");
        check (decaysOk, "Every ringing space fades in the time that was set, measured on the mids (2.0 s asked for)", decays);
        check (smoothOk, "Every tail is smooth: no separate echoes or ringing notes stick out of it (3 is ideal)", smooth);
        check (halves[Reverb::Room] < halves[Reverb::Chamber] && halves[Reverb::Chamber] < halves[Reverb::Hall] && halves[Reverb::Hall] < halves[Reverb::Cathedral] && halves[Reverb::Ambient] > halves[Reverb::Cathedral],
               "The spaces are different sizes: half the sound has arrived later in each of room, chamber, hall, cathedral, and latest in the ambient cloud",
               juce::String (halves[Reverb::Room], 0) + " / " + juce::String (halves[Reverb::Chamber], 0) + " / " + juce::String (halves[Reverb::Hall], 0) + " / " + juce::String (halves[Reverb::Cathedral], 0) + " / " + juce::String (halves[Reverb::Ambient], 0) + " ms");

        // the plate and the vintage unit are different sounds, not renamed halls: brighter and darker
        auto topShare = [&] (int t) { const auto ir = clickThrough ([t] (ReverbSettings& rs) { rs.type = t; }, 3.0f); return juce::Decibels::gainToDecibels (bandOf (ir, 5000.0f, 20000.0f).getRMSLevel (0, 0, ir.getNumSamples()) / ir.getRMSLevel (0, 0, ir.getNumSamples())); };
        const float plateTop = topShare (Reverb::Plate), hallTop = topShare (Reverb::Hall), vintageTop = topShare (Reverb::Vintage);
        check (plateTop > hallTop + 0.8f && vintageTop < hallTop - 1.5f, "The plate is brighter than the hall and the vintage unit darker",
               "share above 5 kHz: plate " + juce::String (plateTop, 1) + ", hall " + juce::String (hallTop, 1) + ", vintage " + juce::String (vintageTop, 1) + " dB");

        // Gated: holds its level for the length of the burst, then is gone
        {
            const auto ir = clickThrough ([] (ReverbSettings& rs) { rs.type = Reverb::Gated; rs.decaySeconds = 1.5f; }, 2.0f);
            const float gate = Reverb::gateSeconds (1.5f);
            auto level = [&] (float from, float to) { return juce::Decibels::gainToDecibels (ir.getRMSLevel (0, 100 + (int) (fs * from), (int) (fs * (to - from))), -120.0f); };
            const float early = level (0.05f, 0.15f), late = level (gate - 0.12f, gate - 0.02f), after = level (gate + 0.25f, gate + 0.45f);
            check (std::abs (late - early) < 6.0f && after < late - 45.0f, "Gated reverb holds its level for the burst and then stops dead",
                   "start " + juce::String (early, 1) + " dB, end " + juce::String (late, 1) + " dB, a quarter of a second later " + juce::String (after, 1) + " dB");
        }
        // Shimmer: a 440 Hz note comes back with its octaves in the tail; the hall has none
        {
            auto octaveIn = [&] (int type)
            {
                Reverb r; r.prepare (fs, 512, 2);
                ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = 3.0f; rs.mix = 1.0f; rs.duck = 0.0f; rs.type = type; rs.shimmer = 0.6f; r.setSettings (rs);
                juce::AudioBuffer<float> b (2, (int) (fs * 4.0)); b.clear();
                for (int i = 0; i < (int) fs; ++i) { const float v = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / fs); b.setSample (0, i, v); b.setSample (1, i, v); }
                inBlocks (b, 512, [&r] (juce::AudioBuffer<float>& v) { r.process (v); });
                // the tail moves, so a note in it is a narrow band, not a single frequency: add up 40 Hz either side
                juce::AudioBuffer<float> part (1, (int) fs); part.copyFrom (0, 0, b, 0, (int) (fs * 1.5), part.getNumSamples());
                auto around = [&] (float hz) { double sum = 0.0; for (float f = hz - 40.0f; f <= hz + 40.0f; f += 1.0f) { const double a = amountAt (part, 0, f, 0); sum += a * a; } return juce::Decibels::gainToDecibels ((float) std::sqrt (sum), -200.0f); };
                return around (880.0f) - around (440.0f);
            };
            const float shimmer = octaveIn (Reverb::Shimmer), hall = octaveIn (Reverb::Hall);
            check (shimmer > -22.0f && hall < shimmer - 20.0f, "Shimmer feeds the tail back an octave higher", "octave against the note: " + juce::String (shimmer, 1) + " dB with shimmer, " + juce::String (hall, 1) + " dB in the plain hall");
        }
        // Freeze: the tail stops fading, and nothing new gets in
        {
            Reverb r; r.prepare (fs, 512, 2);
            ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = 0.6f; rs.mix = 1.0f; rs.duck = 0.0f; rs.type = Reverb::Hall; r.setSettings (rs);
            juce::AudioBuffer<float> b (2, (int) (fs * 5.0)); b.clear();
            juce::Random rng (5);
            for (int i = 0; i < (int) (fs * 0.3); ++i) { const float v = (rng.nextFloat() * 2.0f - 1.0f) * 0.3f; b.setSample (0, i, v); b.setSample (1, i, v); }
            for (int i = (int) (fs * 2.0); i < (int) (fs * 2.2); ++i) { b.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 3000.0 * i / fs)); b.setSample (1, i, b.getSample (0, i)); }
            int pos = 0;
            inBlocks (b, 512, [&] (juce::AudioBuffer<float>& v)
            {
                if (pos >= (int) (fs * 0.4) && ! rs.freeze) { rs.freeze = true; r.setSettings (rs); }
                r.process (v); pos += v.getNumSamples();
            });
            const float held1 = b.getRMSLevel (0, (int) (fs * 1.0), (int) (fs * 0.5)), held2 = b.getRMSLevel (0, (int) (fs * 4.0), (int) (fs * 0.5));
            const float newTone = juce::Decibels::gainToDecibels (amountAt (b, 0, 3000.0f, (int) (fs * 2.5)), -200.0f) - juce::Decibels::gainToDecibels (held2, -200.0f);
            check (held2 > held1 * 0.7f && held1 > 0.001f && newTone < -25.0f, "Freeze holds the tail where it is and lets nothing new in",
                   juce::String (juce::Decibels::gainToDecibels (held2 / held1), 1) + " dB change over three seconds (0.6 s decay); a new note is " + juce::String (newTone, 0) + " dB under the held tail");
        }
        // Low decay: the lows can ring shorter or longer than the rest
        {
            const float shortLows = decayOf (bandOf (clickThrough ([] (ReverbSettings& rs) { rs.type = Reverb::Hall; rs.lowDecay = 0.5f; }, 7.0f), 60.0f, 160.0f));
            const float longLows = decayOf (bandOf (clickThrough ([] (ReverbSettings& rs) { rs.type = Reverb::Hall; rs.lowDecay = 1.8f; }, 7.0f), 60.0f, 160.0f));
            check (shortLows < 1.5f && longLows > 2.6f, "Low decay sets how long the lows ring against the mids", juce::String (shortLows, 2) + " s at 50 %, " + juce::String (longLows, 2) + " s at 180 % (mids 2.0 s)");
        }
        // pushed as far as it goes, it stays stable
        {
            bool ok = true; float peak = 0.0f;
            for (int t : { (int) Reverb::Plate, (int) Reverb::Cathedral, (int) Reverb::Shimmer, (int) Reverb::Vintage })
            {
                const auto ir = clickThrough ([t] (ReverbSettings& rs) { rs.type = t; rs.decaySeconds = 30.0f; rs.size = 2.0f; rs.modDepth = 1.0f; rs.modRate = 3.0f; rs.shimmer = 1.0f; rs.lowDecay = 2.0f; rs.tone = 1.0f; }, 12.0f);
                for (int i = 0; i < ir.getNumSamples(); i += 3) { ok = ok && std::isfinite (ir.getSample (0, i)); peak = juce::jmax (peak, std::abs (ir.getSample (0, i))); }
            }
            check (ok && peak < 2.0f, "Reverb stays stable with every control at its limit (30 s decay, largest size, full movement and shimmer)", "peak " + juce::String (peak, 3));
        }
    }

    {
        // --- the same engines at 192 kHz and 44.1 kHz, every space, everything at its limit
        bool ok = true;
        for (double rate : { 192000.0, 44100.0 })
            for (int t = 0; t < Reverb::NumTypes; ++t)
            {
                Reverb r; r.prepare (rate, 256, 2);
                ReverbSettings rs; rs.enabled = true; rs.autoTime = false; rs.decaySeconds = 30.0f; rs.preDelayMs = 500.0f; rs.mix = 1.0f; rs.duck = 0.0f; rs.type = t;
                rs.size = 2.0f; rs.modDepth = 1.0f; rs.modRate = 3.0f; rs.shimmer = 1.0f; rs.lowDecay = 2.0f; rs.tone = 1.0f; rs.diffusion = 1.0f; rs.early = 1.0f;
                r.setSettings (rs);
                juce::AudioBuffer<float> b (2, (int) (rate * 1.5)); b.clear();
                juce::Random rng (t + 1);
                for (int i = 0; i < (int) (rate * 0.2); ++i) { b.setSample (0, i, rng.nextFloat() - 0.5f); b.setSample (1, i, rng.nextFloat() - 0.5f); }
                inBlocks (b, 256, [&r] (juce::AudioBuffer<float>& v) { r.process (v); });
                for (int i = 0; i < b.getNumSamples(); i += 11) ok = ok && std::isfinite (b.getSample (0, i)) && std::abs (b.getSample (0, i)) < 8.0f;
            }
        check (ok, "Every reverb space runs at 44.1 and 192 kHz with every control at its limit");
    }

    {
        // --- Delay: styles and patterns
        auto echoOf = [&] (const std::function<void (EchoSettings&)>& set, juce::AudioBuffer<float> b)
        {
            Echo e; e.prepare (fs, 512, 2);
            EchoSettings es; es.enabled = true; es.sync = false; es.timeMs = 300.0f; es.feedback = 0.5f; es.mix = 1.0f; es.duck = 0.0f; es.tone = 1.0f; es.width = 1.0f; es.lowCutHz = 20.0f;
            set (es); e.setSettings (es);
            inBlocks (b, 512, [&e] (juce::AudioBuffer<float>& v) { e.process (v); });
            return b;
        };
        auto clickBuffer = [fs] (float seconds, int width = 48) { juce::AudioBuffer<float> b (2, (int) (fs * seconds)); b.clear(); for (int i = 0; i < width; ++i) { b.setSample (0, 1000 + i, 0.8f); b.setSample (1, 1000 + i, 0.8f); } return b; };
        auto firstEchoMs = [fs] (const juce::AudioBuffer<float>& b, int channel)
        {
            for (int i = 1000 + (int) (fs * 0.05); i < b.getNumSamples(); ++i) if (std::abs (b.getSample (channel, i)) > 0.05f) return (float) (1000.0 * (i - 1000) / fs);
            return 0.0f;
        };
        const auto dual = echoOf ([] (EchoSettings& es) { es.pattern = 2; }, clickBuffer (2.0f));
        check (near (firstEchoMs (dual, 0), 300.0f, 6.0f) && near (firstEchoMs (dual, 1), 225.0f, 6.0f), "Dual delay: the right side repeats at three quarters of the left side's time",
               juce::String (firstEchoMs (dual, 0), 0) + " ms left, " + juce::String (firstEchoMs (dual, 1), 0) + " ms right");
        const auto straight = echoOf ([] (EchoSettings& es) { es.pattern = 1; }, clickBuffer (2.0f));
        double differ = 0.0; for (int i = 0; i < straight.getNumSamples(); ++i) differ += std::abs (straight.getSample (0, i) - straight.getSample (1, i));
        check (differ < 1.0e-3 && near (firstEchoMs (straight, 0), 300.0f, 6.0f), "Straight delay keeps every echo in the middle");
        const auto pong = echoOf ([] (EchoSettings&) {}, clickBuffer (2.0f));
        check (near (firstEchoMs (pong, 0), 300.0f, 6.0f) && near (firstEchoMs (pong, 1), 600.0f, 6.0f), "Ping-pong still alternates sides", juce::String (firstEchoMs (pong, 0), 0) + " ms left, " + juce::String (firstEchoMs (pong, 1), 0) + " ms right");

        // what a repeat goes through: a 300 Hz note, measured on its third repeat (0.9 to 1.2 s after it ends)
        auto toneBurst = [fs] { juce::AudioBuffer<float> b (2, (int) (fs * 2.0)); b.clear(); for (int i = 0; i < (int) (fs * 0.25); ++i) { const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 300.0 * i / fs); b.setSample (0, i, v); b.setSample (1, i, v); } return b; };
        auto thirdHarmonic = [&] (int style, float drive)
        {
            const auto b = echoOf ([style, drive] (EchoSettings& es) { es.style = style; es.drive = drive; es.pattern = 1; es.feedback = 0.7f; }, toneBurst());
            juce::AudioBuffer<float> part (1, (int) (fs * 0.2)); part.copyFrom (0, 0, b, 0, (int) (fs * 0.92), part.getNumSamples());
            return juce::Decibels::gainToDecibels (amountAt (part, 0, 900.0f, 0), -200.0f) - juce::Decibels::gainToDecibels (amountAt (part, 0, 300.0f, 0), -200.0f);
        };
        const float cleanH = thirdHarmonic (0, 1.0f), tapeH = thirdHarmonic (1, 1.0f), tapeLow = thirdHarmonic (1, 0.0f);
        auto secondHarmonic = [&] (int style)
        {
            const auto b = echoOf ([style] (EchoSettings& es) { es.style = style; es.drive = 1.0f; es.pattern = 1; es.feedback = 0.7f; }, toneBurst());
            juce::AudioBuffer<float> part (1, (int) (fs * 0.2)); part.copyFrom (0, 0, b, 0, (int) (fs * 0.92), part.getNumSamples());
            return juce::Decibels::gainToDecibels (amountAt (part, 0, 600.0f, 0), -200.0f) - juce::Decibels::gainToDecibels (amountAt (part, 0, 300.0f, 0), -200.0f);
        };
        check (cleanH < -70.0f && tapeH > -40.0f && tapeLow < tapeH - 10.0f, "Tape delay saturates its repeats and Drive sets how much; Digital adds nothing",
               "third harmonic on the third repeat: digital " + juce::String (cleanH, 0) + " dB, tape " + juce::String (tapeLow, 0) + " dB with no drive, " + juce::String (tapeH, 0) + " dB with full drive");
        check (secondHarmonic (2) > secondHarmonic (1) + 15.0f, "Analog delay adds the octave as well (uneven saturation); tape does not",
               "second harmonic: analog " + juce::String (secondHarmonic (2), 0) + " dB, tape " + juce::String (secondHarmonic (1), 0) + " dB");
        // analog is darker: the top of a click's first echo
        auto topOfEcho = [&] (int style)
        {
            const auto b = echoOf ([style] (EchoSettings& es) { es.style = style; es.pattern = 1; es.drive = 0.0f; }, clickBuffer (1.0f, 2));
            juce::AudioBuffer<float> part (1, (int) (fs * 0.1)); part.copyFrom (0, 0, b, 0, 1000 + (int) (fs * 0.29), part.getNumSamples());
            return juce::Decibels::gainToDecibels (amountAt (part, 0, 8000.0f, 0), -200.0f) - juce::Decibels::gainToDecibels (amountAt (part, 0, 500.0f, 0), -200.0f);
        };
        check (topOfEcho (2) < topOfEcho (1) - 6.0f && topOfEcho (1) < topOfEcho (0) - 1.0f, "Each style has its own top end: digital brightest, tape softer, analog darkest",
               "8 kHz against 500 Hz in the first echo: " + juce::String (topOfEcho (0), 0) + " / " + juce::String (topOfEcho (1), 0) + " / " + juce::String (topOfEcho (2), 0) + " dB");
        // diffused: a click's third repeat is spread out, not a click any more
        auto spread = [&] (int style)
        {
            const auto b = echoOf ([style] (EchoSettings& es) { es.style = style; es.pattern = 1; es.feedback = 0.8f; }, clickBuffer (2.0f));
            const int from = 1000 + (int) (fs * 0.88), n = (int) (fs * 0.14);
            return b.getMagnitude (0, from, n) / (b.getRMSLevel (0, from, n) + 1.0e-9f);
        };
        check (spread (3) < spread (0) * 0.6f, "Diffused delay smears each repeat more than the last", "peak against average on the third repeat: " + juce::String (spread (0), 1) + " digital, " + juce::String (spread (3), 1) + " diffused");
        // wobble moves the pitch of an echo by cents, not semitones
        {
            auto steady = voiceTone (220.0f, 6.0f, 2);
            for (int i = (int) (fs * 5.0); i < steady.getNumSamples(); ++i) { steady.setSample (0, i, 0.0f); steady.setSample (1, i, 0.0f); }
            auto wet = echoOf ([] (EchoSettings& es) { es.wobble = 1.0f; es.pattern = 1; es.feedback = 0.0f; es.mix = 1.0f; }, steady);
            for (int i = 0; i < wet.getNumSamples(); ++i) wet.setSample (0, i, wet.getSample (0, i) - steady.getSample (0, i));      // the echo alone
            const auto notes = trackNotes (wet, 1.0f);
            std::vector<float> inside (notes.begin(), notes.begin() + (long) std::min<size_t> (notes.size(), 380));
            const float span = spanOf (inside) * 100.0f;
            check (span > 8.0f && span < 40.0f, "Delay wobble drifts the pitch of the echoes by a few cents, like tape", juce::String (span, 1) + " cents from lowest to highest");
        }
        // pushed hard, it stays stable
        {
            auto loud = voiceTone (180.0f, 6.0f, 2); loud.applyGain (4.0f);
            bool ok = true; float peak = 0.0f;
            for (int style = 0; style < 4; ++style)
            {
                const auto b = echoOf ([style] (EchoSettings& es) { es.style = style; es.feedback = 0.9f; es.drive = 1.0f; es.wobble = 1.0f; es.timeMs = 40.0f; }, loud);
                for (int i = 0; i < b.getNumSamples(); i += 5) { ok = ok && std::isfinite (b.getSample (0, i)); peak = juce::jmax (peak, std::abs (b.getSample (0, i))); }
            }
            check (ok && peak < 40.0f, "Delay stays stable with repeats, drive and wobble at their limits", "peak " + juce::String (peak, 1));
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
