#include "MasterEngine.h"
#include "../dsp/LoudnessMeter.h"
#include "../dsp/SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace mu
{
juce::StringArray masterStyleNames() { return { "Transparent", "Warm", "Loud", "Dynamic", "Bright", "Aggressive" }; }

juce::String masterStyleBlurb (MasterStyle s)
{
    switch (s)
    {
        case MasterStyle::Transparent: return "Clean and natural. Streaming loudness, the least processing that does the job.";
        case MasterStyle::Warm:        return "Smooth and rich. A little more low end, a softer top, tape saturation, gentle glue.";
        case MasterStyle::Loud:        return "Punchy and competitive. Glue, a soft clipper in front of the limiter, loud without crushing.";
        case MasterStyle::Dynamic:     return "Keeps the hits and the quiet parts. Quieter than the others on purpose.";
        case MasterStyle::Bright:      return "Clear, polished top. Air and presence are lifted if the mix is not already bright.";
        case MasterStyle::Aggressive:  return "Hard-hitting, for rap, trap and electronic. The most drive, clipping and loudness.";
        case MasterStyle::Count: break;
    }
    return {};
}

namespace
{
    struct StyleSpec { float lufs, maxLimitDb, lowTilt, airTilt, presence; bool glue; float glueRatio; int satType; float satDrive, satMix, clip; bool multiband; };
    StyleSpec specFor (MasterStyle s)
    {
        switch (s)
        {
            case MasterStyle::Warm:       return { -13.0f, 4.0f, 1.0f, -1.0f, 0.0f, true,  1.5f, 0, 3.0f, 0.25f, 0.0f, false };
            case MasterStyle::Loud:       return { -9.5f,  6.0f, 0.5f, 0.5f,  0.0f, true,  2.0f, 2, 2.0f, 0.15f, 0.5f, true };
            case MasterStyle::Dynamic:    return { -16.0f, 3.0f, 0.0f, 0.0f,  0.0f, false, 1.5f, -1, 0.0f, 0.0f, 0.0f, false };
            case MasterStyle::Bright:     return { -12.0f, 5.0f, 0.0f, 1.5f,  0.5f, false, 1.5f, -1, 0.0f, 0.0f, 0.25f, false };
            case MasterStyle::Aggressive: return { -8.5f,  7.0f, 1.0f, 0.5f,  1.0f, true,  2.0f, 2, 4.0f, 0.30f, 0.7f, true };
            case MasterStyle::Transparent: case MasterStyle::Count: break;
        }
        return { -14.0f, 3.0f, 0.0f, 0.0f, 0.0f, false, 1.5f, -1, 0.0f, 0.0f, 0.0f, false };
    }
    float meanBetween (const BandArray& v, const BandArray& centres, float lo, float hi)
    {
        float sum = 0.0f; int n = 0;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] >= lo && centres[(size_t) b] <= hi) { sum += v[(size_t) b]; ++n; }
        return n > 0 ? sum / (float) n : 0.0f;
    }
    juce::String hzText (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f / 10.0f) * 10) + " Hz"; }
    juce::String d1 (float v) { return juce::String (v, 1); }
}

// A balanced master, leaned the way the style leans. The starting shape is the one the Mix Brain uses for a whole
// mix; like it, this is reasoned from mixing practice and not measured from a library of released songs.
BandArray masterTargetCurve (MasterStyle style)
{
    auto t = roleTargetCurve (Role::Master);
    const auto centres = analysisBandCentres();
    const auto spec = specFor (style);
    for (int b = 0; b < kAnalysisBands; ++b)
    {
        const float f = centres[(size_t) b];
        if (f < 200.0f)  t[(size_t) b] += spec.lowTilt * juce::jlimit (0.0f, 1.0f, std::log2 (200.0f / f));
        if (f > 5000.0f) t[(size_t) b] += spec.airTilt * juce::jlimit (0.0f, 1.0f, std::log2 (f / 5000.0f));
        if (f > 2000.0f && f < 6000.0f) t[(size_t) b] += spec.presence;
    }
    return t;
}

float toneDistanceDb (const BandArray& m, const BandArray& target)
{
    const auto centres = analysisBandCentres();
    const float mMean = meanBetween (m, centres, 60.0f, 12000.0f), tMean = meanBetween (target, centres, 60.0f, 12000.0f);
    double sum = 0.0; int n = 0;
    for (int b = 0; b < kAnalysisBands; ++b)
        if (centres[(size_t) b] >= 60.0f && centres[(size_t) b] <= 12000.0f) { const float d = (m[(size_t) b] - mMean) - (target[(size_t) b] - tMean); sum += (double) d * d; ++n; }
    return n > 0 ? (float) std::sqrt (sum / n) : 0.0f;
}

MasterPlan planMaster (const MasterFacts& mix, MasterStyle style, float intensity, const MasterFacts* reference)
{
    MasterPlan plan;
    const auto spec = specFor (style);
    const auto centres = analysisBandCentres();
    const float k = juce::jlimit (0.0f, 1.0f, intensity);
    const float scale = 0.4f + 0.9f * k;                  // how far every move goes: 0.4 at the bottom, 0.85 in the middle, 1.3 at the top
    const auto& p = mix.probe;
    const bool useReference = reference != nullptr && reference->enough();

    // ---- the tone to aim for
    plan.targetCurve = masterTargetCurve (style);
    if (useReference)
    {
        // the reference's own shape, smoothed, and never more than 4 dB away from the balanced shape in any band:
        // the direction of the reference, not a copy of its EQ curve
        const auto base = masterTargetCurve (MasterStyle::Transparent);
        const float rMean = meanBetween (reference->bandsDb, centres, 60.0f, 12000.0f), bMean = meanBetween (base, centres, 60.0f, 12000.0f);
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const int lo = juce::jmax (0, b - 1), hi = juce::jmin (kAnalysisBands - 1, b + 1);
            const float smooth = (reference->bandsDb[(size_t) lo] + reference->bandsDb[(size_t) b] + reference->bandsDb[(size_t) hi]) / 3.0f - rMean;
            plan.targetCurve[(size_t) b] = base[(size_t) b] + juce::jlimit (-4.0f, 4.0f, smooth - (base[(size_t) b] - bMean));
        }
    }
    const float mMean = meanBetween (mix.bandsDb, centres, 60.0f, 9000.0f), tMean = meanBetween (plan.targetCurve, centres, 60.0f, 9000.0f);
    BandArray dev {};
    for (int b = 0; b < kAnalysisBands; ++b) dev[(size_t) b] = (mix.bandsDb[(size_t) b] - mMean) - (plan.targetCurve[(size_t) b] - tMean);
    struct Region { float mean, peakHz, peakDb, swing; };
    auto region = [&] (float lo, float hi, bool excess)
    {
        Region r { 0.0f, 0.5f * (lo + hi), -120.0f, 0.0f }; float sum = 0.0f, best = excess ? -1.0e9f : 1.0e9f; int n = 0;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float f = centres[(size_t) b];
            if (f < lo || f > hi) continue;
            sum += dev[(size_t) b]; ++n; r.swing = juce::jmax (r.swing, mix.swingDb[(size_t) b]);
            if (excess ? dev[(size_t) b] > best : dev[(size_t) b] < best) { best = dev[(size_t) b]; r.peakHz = f; r.peakDb = mix.bandsDb[(size_t) b]; }
        }
        if (n > 0) r.mean = sum / (float) n;
        return r;
    };
    auto bell = [] (float hz, float gain, float q) { EQBandSettings e; e.enabled = true; e.type = EQType::Bell; e.freqHz = hz; e.gainDb = gain; e.q = q; return e; };
    auto shelf = [] (bool high, float hz, float gain) { EQBandSettings e; e.enabled = true; e.type = high ? EQType::HighShelf : EQType::LowShelf; e.freqHz = hz; e.gainDb = gain; e.q = 0.71f; return e; };
    std::vector<EQBandSettings> corrective, tonal, dynamic;
    juce::StringArray eqSurgical, eqTonal, eqDynamic;

    // ---- what is measured as too much or too little, in words (the EQ itself is fitted below)
    if (meanBetween (mix.bandsDb, centres, 20.0f, 28.0f) > meanBetween (mix.bandsDb, centres, 45.0f, 110.0f) - 6.0f)
    {
        EQBandSettings e; e.enabled = true; e.type = EQType::LowCut; e.freqHz = 25.0f; e.q = 0.71f; e.slope = 0;
        corrective.push_back (e);
        plan.detected.add ("Rumble below 30 Hz is within 6 dB of the bass itself. Nothing plays it back, and it uses up headroom.");
        eqSurgical.add ("low cut at 25 Hz");
    }
    if (auto r = region (45.0f, 110.0f, true); r.mean > 2.0f)   plan.detected.add ("The low end (45 to 110 Hz) is " + d1 (r.mean) + " dB heavier than the balance being aimed for.");
    if (auto r = region (180.0f, 400.0f, true); r.mean > 2.0f)  plan.detected.add ("Low-mid build-up: " + d1 (r.mean) + " dB too much between 180 and 400 Hz, strongest at " + hzText (r.peakHz) + (r.swing > 6.0f ? ", and it comes and goes." : "."));
    if (auto r = region (400.0f, 900.0f, true); r.mean > 2.0f)  plan.detected.add ("Boxy: " + d1 (r.mean) + " dB too much between 400 and 900 Hz.");
    if (auto r = region (8000.0f, 16000.0f, true); r.mean > 3.0f) plan.detected.add ("The top (above 8 kHz) is " + d1 (r.mean) + " dB brighter than the balance being aimed for.");
    if (auto r = region (45.0f, 110.0f, false); r.mean < -3.0f)
    {
        plan.detected.add ("Thin low end: " + d1 (-r.mean) + " dB short between 45 and 110 Hz.");
        plan.notes.add ("A master can only lift the low end that is in the mix. If the 808 or the kick is too quiet, turn it up in the mix.");
    }
    if (auto r = region (1500.0f, 4000.0f, false); r.mean < -2.0f) plan.detected.add ("Short of presence: " + d1 (-r.mean) + " dB under between 1.5 and 4 kHz, where words and attack are heard.");
    if (auto r = region (8000.0f, 16000.0f, false); r.mean < -3.0f) plan.detected.add ("Dull top: " + d1 (-r.mean) + " dB short above 8 kHz.");

    // ---- problems that are only there some of the time get a band that only works while they are there
    if (auto r = region (180.0f, 400.0f, true); r.mean > 2.5f && r.swing > 6.0f)
    {
        auto e = bell (r.peakHz, 0.0f, 1.0f); e.dynamic = true; e.rangeDb = -juce::jlimit (0.8f, 3.0f, 0.65f * r.mean * scale); e.thresholdDb = juce::jlimit (-60.0f, -6.0f, r.peakDb + 1.0f);
        dynamic.push_back (e); eqDynamic.add ("up to " + d1 (e.rangeDb) + " dB at " + hzText (r.peakHz) + " while it builds up");
    }
    if (auto r = region (2500.0f, 5000.0f, true); r.mean > 2.5f)
    {
        // harshness is worst on the loud moments, so on a master it is always taken out dynamically
        auto e = bell (r.peakHz, 0.0f, 1.4f); e.dynamic = true; e.rangeDb = -juce::jlimit (0.8f, 3.0f, 0.6f * r.mean * scale); e.thresholdDb = juce::jlimit (-60.0f, -6.0f, r.peakDb + 1.0f);
        dynamic.push_back (e);
        plan.detected.add ("Harsh upper mids: " + d1 (r.mean) + " dB too much between 2.5 and 5 kHz, strongest at " + hzText (r.peakHz) + ".");
        eqDynamic.add ("up to " + d1 (e.rangeDb) + " dB at " + hzText (r.peakHz) + " on the loud moments");
    }

    // ---- the EQ itself: fitted to the difference between the mix and the tone being aimed for.
    // Up to five broad bands, the biggest difference first, each taking out a share of it (30 % at the lowest
    // intensity, 60 % in the middle, 90 % at the top; cuts never more than 6 dB times that share, lifts 70 % of that). Where the
    // difference is under about 1 dB nothing is done. The differences are measured from the middle value of the
    // mix, not its average, so one heavy range does not make every other range look short and get boosted.
    {
        std::vector<float> sorted;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] >= 60.0f && centres[(size_t) b] <= 9000.0f) sorted.push_back (dev[(size_t) b]);
        std::sort (sorted.begin(), sorted.end());
        const float anchor = sorted.empty() ? 0.0f : sorted[sorted.size() / 2];
        const float share = 0.3f + 0.6f * k, limit = 6.0f * share;
        BandArray want {};
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float f = centres[(size_t) b];
            const bool usable = f >= 40.0f && f <= 14000.0f && mix.bandsDb[(size_t) b] > mMean - 45.0f;
            want[(size_t) b] = usable ? -(dev[(size_t) b] - anchor) : 0.0f;
        }
        for (auto& e : dynamic)       // the range a dynamic band is dealing with is not cut a second time
        {
            const float lo = e.freqHz < 1000.0f ? 170.0f : 2300.0f, hi = e.freqHz < 1000.0f ? 420.0f : 5400.0f;
            for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] >= lo && centres[(size_t) b] <= hi) want[(size_t) b] = juce::jmax (0.0f, want[(size_t) b]);
        }
        BandArray smooth = want;
        for (int b = 1; b < kAnalysisBands - 1; ++b) smooth[(size_t) b] = 0.25f * want[(size_t) b - 1] + 0.5f * want[(size_t) b] + 0.25f * want[(size_t) b + 1];
        // lifts are kept to 70 % of what cuts are allowed: adding what a mix lacks is the riskier half on a master
        for (auto& v : smooth) v = juce::jlimit (-limit, 0.7f * limit, v * share);
        const BandArray asked = smooth;
        const float octPerBand = std::log2 (1000.0f) / (float) kAnalysisBands;
        bool usedLow = false, usedHigh = false;
        std::vector<std::pair<float, float>> fitted;      // where and which way
        for (int n = 0; n < 8 && fitted.size() < 5; ++n)
        {
            int peak = -1; float best = 0.7f;
            for (int b = 0; b < kAnalysisBands; ++b) if (std::abs (smooth[(size_t) b]) > best) { best = std::abs (smooth[(size_t) b]); peak = b; }
            if (peak < 0) break;
            const float g = juce::jlimit (-limit, 0.7f * limit, smooth[(size_t) peak]), f = centres[(size_t) peak];
            // a lift right beside a cut (or the other way round) mostly cancels it: two differences that close
            // together are too fine a detail for a master, so the smaller one is let go
            bool fights = false;
            for (auto& done : fitted) if (done.second * g < 0.0f && std::abs (std::log2 (f / done.first)) < 0.75f) fights = true;
            if (fights)
            {
                for (int b = 0; b < kAnalysisBands; ++b) if (std::abs (std::log2 (centres[(size_t) b] / f)) < 0.4f) smooth[(size_t) b] = 0.0f;
                continue;
            }
            int width = 1;
            for (int b = peak - 1; b >= 0 && smooth[(size_t) b] * g > 0.0f && std::abs (smooth[(size_t) b]) > std::abs (g) * 0.5f; --b) ++width;
            for (int b = peak + 1; b < kAnalysisBands && smooth[(size_t) b] * g > 0.0f && std::abs (smooth[(size_t) b]) > std::abs (g) * 0.5f; ++b) ++width;
            const float bw = juce::jlimit (0.7f, 3.0f, (float) width * octPerBand);
            // The ends of the range are shelves, the way a mastering EQ is used; everything between is a broad bell.
            // A shelf moves everything beyond its corner, so it is used once per end and only where everything
            // beyond the corner is wanted the same way; a low end that is heavy at the bottom and short above it
            // gets two bells, not shelves stacked against each other.
            bool allBelow = true, allAbove = true;
            for (int b = 0; b < kAnalysisBands; ++b)
            {
                if (centres[(size_t) b] < f && asked[(size_t) b] * g < -0.1f) allBelow = false;
                if (centres[(size_t) b] > f && asked[(size_t) b] * g < -0.1f) allAbove = false;
            }
            const bool highShelf = f >= 7000.0f && ! usedHigh && allAbove, lowShelf = f <= 110.0f && ! usedLow && allBelow;
            usedHigh = usedHigh || highShelf; usedLow = usedLow || lowShelf;
            EQBandSettings e = highShelf ? shelf (true, juce::jmax (5000.0f, f * 0.75f), g) : lowShelf ? shelf (false, juce::jmin (160.0f, f * 1.4f), g)
                                         : bell (juce::jlimit (45.0f, 12000.0f, f), g, juce::jlimit (0.5f, 1.6f, std::sqrt (std::pow (2.0f, bw)) / (std::pow (2.0f, bw) - 1.0f)));
            fitted.push_back ({ e.freqHz, g });        // a shelf counts from its corner
            // What is left over after a band is fitted is only chased where it is still the same kind of difference
            // that was measured. A leftover of the other sign is the skirt of the band just fitted, and "fixing"
            // it would add bands that undo each other.
            for (int b = 0; b < kAnalysisBands; ++b)
            {
                smooth[(size_t) b] -= EQEngine::bandResponseDb (e, centres[(size_t) b], 48000.0);
                if (std::abs (asked[(size_t) b]) < 0.3f || smooth[(size_t) b] * asked[(size_t) b] < 0.0f) smooth[(size_t) b] = 0.0f;
            }
            const juce::String what = (e.type == EQType::Bell ? juce::String() : juce::String (e.type == EQType::HighShelf ? "high shelf " : "low shelf ")) + (g > 0.0f ? "+" : "") + d1 (g) + " dB "
                                    + (e.type == EQType::HighShelf ? "above " : e.type == EQType::LowShelf ? "below " : "at ") + hzText (e.freqHz);
            if (g < 0.0f) { corrective.push_back (e); eqSurgical.add (what); } else { tonal.push_back (e); eqTonal.add (what); }
        }
    }
    for (auto* list : { &corrective, &tonal, &dynamic }) for (auto& e : *list) if (plan.eq.size() < 8) plan.eq.push_back (e);
    if (! eqSurgical.isEmpty()) plan.processing.add ("Corrective EQ: " + eqSurgical.joinIntoString (", ") + ".");
    if (! eqTonal.isEmpty())    plan.processing.add ("Tonal EQ: " + eqTonal.joinIntoString (", ") + ".");
    if (! eqDynamic.isEmpty())  plan.processing.add ("Dynamic EQ: " + eqDynamic.joinIntoString (", ") + ".");
    if (plan.eq.empty()) plan.leftAlone.add ("EQ: the tone is within about 1 dB of the balance being aimed for everywhere, so it is not touched.");

    // ---- how squashed the mix already is
    const float peakOver = mix.peakToLoudness();
    const bool clipped = p.flatTops > 20;
    const bool dense = peakOver < 9.0f || p.punchDb < 5.0f;
    if (clipped)
    {
        plan.detected.add ("The mix arrives already clipped: " + juce::String (p.flatTops) + " flattened peaks.");
        plan.notes.add ("Clipping that is in the mix cannot be undone by a master. Turn the tracks or the mix bus down so the mix peaks under 0 dB, and master again.");
    }
    if (dense) plan.detected.add ("The mix is already dense: its peaks are only " + d1 (peakOver) + " dB above its loudness, and its hits stand " + d1 (p.punchDb) + " dB above the level around them.");

    // ---- low end that jumps about: its own band of compression
    plan.multiband.enabled = false;
    for (auto& b : plan.multiband.band) b.on = false;
    // Loud notes are what a compressor can hold back: how far the loud bass notes stand above the usual bass level.
    // (A low end that moves a lot because the bass stops playing in places is not something compression changes.)
    const float lowJump = p.lowTypicalDb > -80.0f ? p.lowLoudDb - p.lowTypicalDb : 0.0f;
    const bool lowUnsteady = lowJump >= 3.0f;
    if (lowUnsteady) plan.detected.add ("Uneven bass: the loud bass notes are " + d1 (lowJump) + " dB above the usual level of the low end (40 to 120 Hz).");
    if (lowUnsteady && ! dense && (spec.multiband || lowJump >= 5.0f))
    {
        // the low band ends where this song's bass ends: the first place above 90 Hz that is 9 dB under the strongest low band
        float top = -200.0f, cross = 160.0f;
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] >= 40.0f && centres[(size_t) b] <= 120.0f) top = juce::jmax (top, mix.bandsDb[(size_t) b]);
        for (int b = 0; b < kAnalysisBands; ++b) if (centres[(size_t) b] >= 90.0f && centres[(size_t) b] <= 260.0f && mix.bandsDb[(size_t) b] < top - 9.0f) { cross = centres[(size_t) b]; break; }
        plan.multiband.enabled = true;
        plan.multiband.crossLowHz = juce::jlimit (90.0f, 220.0f, cross);
        plan.multiband.crossHighHz = 3500.0f;
        auto& low = plan.multiband.band[0];
        low.on = true; low.ratio = 1.5f + 1.0f * k; low.attackMs = 30.0f; low.releaseMs = 200.0f;
        // the compressor reads the average level of the band, as the probe does; it starts working 1 dB above
        // the usual level, so ordinary notes pass and the loud ones are held
        low.thresholdDb = juce::jlimit (-60.0f, -3.0f, p.lowTypicalDb + 1.0f);
        const float expected = juce::jmax (0.0f, lowJump - 1.0f) * (1.0f - 1.0f / low.ratio);
        plan.processing.add ("Multiband compression, low band only (up to " + hzText (plan.multiband.crossLowHz) + ", where this song's bass ends): " + d1 (low.ratio) + ":1 from " + d1 (low.thresholdDb)
                             + " dB, about " + d1 (expected) + " dB off the loudest bass notes.");
    }
    else if (lowUnsteady) plan.leftAlone.add (juce::String ("Multiband compression: the bass is uneven, but ") + (dense ? "the mix is already dense." : "this style keeps dynamics; it is used from 5 dB."));
    else plan.leftAlone.add ("Multiband compression: the bass notes are even (the loud ones are " + d1 (lowJump) + " dB above the usual level).");

    // ---- glue
    const bool wide = p.loudnessRangeLu > 9.0f || p.levelSwingDb > 9.0f;
    if (! dense && (spec.glue || wide))
    {
        plan.glue = true; plan.glueRatio = spec.glue ? spec.glueRatio : 1.5f;
        // the glue compressor reads the average level: the threshold sits where the loud passages lose 1 to 2 dB
        const float wanted = juce::jlimit (0.8f, 2.5f, 1.5f * scale);
        plan.glueThresholdDb = juce::jlimit (-50.0f, 0.0f, mix.integratedLufs + 1.5f + 3.0f - wanted / (1.0f - 1.0f / plan.glueRatio));
        plan.processing.add ("Glue compression: " + d1 (plan.glueRatio) + ":1, slow attack so the hits pass, set for 1 to 2 dB on the loud passages"
                             + (wide && ! spec.glue ? " (the song moves over " + d1 (juce::jmax (p.loudnessRangeLu, p.levelSwingDb)) + " dB)." : "."));
    }
    else plan.leftAlone.add (dense ? "Glue compression: the mix is already dense; more compression would only flatten it."
                                   : "Glue compression: the level is steady (loudness range " + d1 (p.loudnessRangeLu) + " LU) and this style does not ask for it.");

    // ---- saturation
    if (spec.satType >= 0 && ! clipped && ! dense)
    {
        plan.saturate = true; plan.satType = spec.satType; plan.satDriveDb = spec.satDrive * (0.6f + 0.6f * k); plan.satMix = spec.satMix * (0.6f + 0.6f * k);
        plan.processing.add (juce::String ("Saturation: ") + (spec.satType == 0 ? "tape" : spec.satType == 1 ? "tube" : "warm") + ", " + d1 (plan.satDriveDb) + " dB of drive at " + juce::String (juce::roundToInt (plan.satMix * 100.0f)) + " %.");
    }
    else plan.leftAlone.add (spec.satType < 0 ? "Saturation: not part of this style." : "Saturation: the mix is already dense or clipped; more harmonics would roughen it.");

    // ---- stereo
    if (p.sideToMidDb[0] > -12.0f)
    {
        plan.monoBass = true; plan.monoBelowHz = 120.0f;
        plan.detected.add ("The low end is wide: under 150 Hz the sides are only " + d1 (-p.sideToMidDb[0]) + " dB below the middle. It loses weight in mono and on club systems.");
        plan.processing.add ("Stereo: bass made mono below 120 Hz.");
    }
    if (p.correlation < 0.25f || p.lowestCorrelation < -0.3f)
    {
        plan.width = 0.9f;
        plan.detected.add ("Phase: the two sides are close to unrelated (correlation " + juce::String (p.correlation, 2) + ", lowest " + juce::String (p.lowestCorrelation, 2) + "); in mono the mix is " + d1 (-p.monoLossDb) + " dB quieter.");
        plan.processing.add ("Stereo: width brought in to 90 %.");
        plan.notes.add ("Something in the mix is out of phase or over-widened. A master can only narrow everything; find the track and fix it there.");
    }
    else if (p.widthDb < -30.0f)
    {
        plan.detected.add ("The mix is close to mono: the sides are " + d1 (-p.widthDb) + " dB below the middle.");
        plan.notes.add ("Width cannot be added by a master when there is almost nothing at the sides to turn up. Pan and widen things in the mix.");
    }
    else if (p.widthDb < -14.0f && p.correlation > 0.7f && (style == MasterStyle::Bright || style == MasterStyle::Loud || style == MasterStyle::Aggressive) && k > 0.3f)
    {
        plan.width = 1.0f + 0.12f * k;
        plan.detected.add ("Narrow: the sides are " + d1 (-p.widthDb) + " dB below the middle, and safely in phase (correlation " + juce::String (p.correlation, 2) + ").");
        plan.processing.add ("Stereo: width to " + juce::String (juce::roundToInt (plan.width * 100.0f)) + " %.");
    }
    else if (! plan.monoBass) plan.leftAlone.add ("Stereo: width and phase are fine as they are (correlation " + juce::String (p.correlation, 2) + ", " + d1 (-p.monoLossDb) + " dB lost in mono).");

    // ---- loudness: as loud as the style asks, unless that would mean crushing it
    float target = useReference ? juce::jlimit (-18.0f, -7.5f, reference->integratedLufs) : spec.lufs + (k - 0.5f) * 1.5f;
    plan.ceilingDb = -1.0f;
    plan.clip = spec.clip > 0.0f && ! clipped && ! dense && p.punchDb >= 7.0f;
    plan.clipAmount = juce::jlimit (0.0f, 1.0f, spec.clip * (0.6f + 0.6f * k)); plan.clipSoftness = 0.6f;
    const float shave = plan.clip ? 6.0f * plan.clipAmount * 0.5f : 0.0f;                       // roughly what the clipper takes off the tallest hits
    const float maxLimit = spec.maxLimitDb * (0.7f + 0.5f * k);
    const float wouldLimit = (mix.truePeakDb + (target - mix.integratedLufs)) - plan.ceilingDb - shave;
    if (wouldLimit > maxLimit)
    {
        const float back = wouldLimit - maxLimit;
        plan.notes.add ("Loudness held back: " + d1 (target) + " LUFS would take " + d1 (wouldLimit) + " dB off the peaks of this mix. The target is " + d1 (target - back) + " LUFS instead, which takes " + d1 (maxLimit)
                        + " dB. For more, the mix needs less peak (compress the drums or the vocal in the mix).");
        target -= back;
    }
    plan.targetLufs = juce::jlimit (-24.0f, -7.0f, target);
    const float gain = plan.targetLufs - mix.integratedLufs;
    plan.detected.add ("Loudness " + d1 (mix.integratedLufs) + " LUFS, true peak " + d1 (mix.truePeakDb) + " dBTP: " + (gain > 0.5f ? "needs " + d1 (gain) + " dB to reach the target." : gain < -0.5f ? "already " + d1 (-gain) + " dB over the target." : "already at the target."));
    if (plan.clip) plan.processing.add ("Soft clipper: rounds off the hits that reach " + d1 (6.0f * (1.0f - plan.clipAmount)) + " dB above the ceiling, so the limiter has less to do.");
    else plan.leftAlone.add (spec.clip <= 0.0f ? "Soft clipper: not part of this style." : "Soft clipper: the hits are not tall enough to be worth rounding, or the mix is already dense.");
    plan.processing.add ("True peak limiter: ceiling " + d1 (plan.ceilingDb) + " dBTP, level set to " + d1 (plan.targetLufs) + " LUFS" + (useReference ? " (the reference is at " + d1 (reference->integratedLufs) + ")." : "."));
    return plan;
}

juce::StringArray masterReport (const MasterFacts& mix, const MasterFacts& master, const MasterPlan& plan)
{
    juce::StringArray out;
    auto line = [&out] (const juce::String& what, float before, float after, const juce::String& unit, const juce::String& verdict)
    { out.add (what + ": " + juce::String (before, 1) + " to " + juce::String (after, 1) + " " + unit + (verdict.isNotEmpty() ? ". " + verdict : juce::String()) + "."); };
    line ("Integrated loudness", mix.integratedLufs, master.integratedLufs, "LUFS", std::abs (master.integratedLufs - plan.targetLufs) <= 1.0f ? "On target" : "Target " + juce::String (plan.targetLufs, 1));
    line ("True peak", mix.truePeakDb, master.truePeakDb, "dBTP", master.truePeakDb <= plan.ceilingDb + 0.2f ? "Under the ceiling" : "Over the ceiling of " + juce::String (plan.ceilingDb, 1));
    const float toneBefore = toneDistanceDb (mix.bandsDb, plan.targetCurve), toneAfter = toneDistanceDb (master.bandsDb, plan.targetCurve);
    // a verdict is only given where a processor was used: without one, a difference between the two passes is the song
    line ("Tonal balance, distance from the target", toneBefore, toneAfter, "dB", plan.eq.empty() ? juce::String ("No EQ was used")
                                                                                 : toneAfter < toneBefore - 0.3f ? "Improved" : toneAfter > toneBefore + 0.3f ? "Further away" : "Unchanged");
    const float jumpBefore = mix.probe.lowLoudDb - mix.probe.lowTypicalDb, jumpAfter = master.probe.lowLoudDb - master.probe.lowTypicalDb;
    line ("Loud bass notes above the usual bass level", jumpBefore, jumpAfter, "dB", ! plan.multiband.enabled ? juce::String ("No low-end compression was used") : jumpAfter < jumpBefore - 0.5f ? "Steadier" : "Unchanged");
    line ("Peaks above loudness", mix.peakToLoudness(), master.peakToLoudness(), "dB", master.peakToLoudness() >= 8.0f ? "Punch kept" : "Dense: less would be flat");
    line ("Loudness range", mix.probe.loudnessRangeLu, master.probe.loudnessRangeLu, "LU", {});
    out.add ("Stereo: correlation " + juce::String (mix.probe.correlation, 2) + " to " + juce::String (master.probe.correlation, 2) + ", " + juce::String (-master.probe.monoLossDb, 1) + " dB lost in mono. "
             + (master.probe.correlation > 0.3f && master.probe.monoLossDb > -3.0f ? "Mono safe." : "Check it in mono."));
    out.add ("These are measurements of the mix and of the master over the two passes that were played. If the passes covered different parts of the song, small differences are the song. Whether it sounds better is for your ears: use the level-matched comparison.");
    return out;
}

MasterFacts measureMix (const juce::AudioBuffer<float>& audio, double fs)
{
    MasterFacts f;
    const int block = 512, ch = juce::jmin (2, audio.getNumChannels());
    if (ch < 1 || audio.getNumSamples() < block) return f;
    LoudnessMeter meter; meter.prepare (fs, block, ch);
    MasterProbe probe; probe.prepare (fs, ch); probe.start();
    auto analyser = std::make_unique<SpectrumAnalyzer>(); analyser->prepare (fs); analyser->startListen (true);
    for (int pos = 0; pos + block <= audio.getNumSamples(); pos += block)
    {
        const float* chans[2] = { audio.getReadPointer (0, pos), audio.getReadPointer (ch > 1 ? 1 : 0, pos) };
        const juce::AudioBuffer<float> view (const_cast<float**> (chans), ch, block);
        meter.process (view);
        probe.process (view, meter.getShortTerm(), meter.getMomentary());
        analyser->pushBlock (view);
        analyser->pump (64);
    }
    meter.readIntegrated();
    f.probe = probe.read();
    f.integratedLufs = meter.getIntegrated();
    f.truePeakDb = meter.getTruePeakHold();
    f.hasBands = analyser->listenBands (f.bandsDb, f.swingDb);
    return f;
}
}
