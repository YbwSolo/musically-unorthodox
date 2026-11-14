#include "TonalAnalysis.h"
#include <algorithm>
#include <vector>
#include <cmath>

namespace mu
{
BandArray analysisBandCentres()
{
    BandArray c {};
    for (int b = 0; b < kAnalysisBands; ++b)
        c[(size_t) b] = 20.0f * std::pow (1000.0f, ((float) b + 0.5f) / (float) kAnalysisBands);
    return c;
}

namespace
{
    struct Anchor { float hz, db; };

    BandArray curveFrom (std::initializer_list<Anchor> anchors)
    {
        std::vector<Anchor> a (anchors);
        const auto centres = analysisBandCentres();
        BandArray out {};
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float f = centres[(size_t) b];
            if (f <= a.front().hz) { out[(size_t) b] = a.front().db - 18.0f * std::log2 (a.front().hz / f); continue; }
            if (f >= a.back().hz)  { out[(size_t) b] = a.back().db - 12.0f * std::log2 (f / a.back().hz); continue; }
            for (size_t i = 0; i + 1 < a.size(); ++i)
                if (f >= a[i].hz && f <= a[i + 1].hz)
                {
                    const float t = std::log (f / a[i].hz) / std::log (a[i + 1].hz / a[i].hz);
                    out[(size_t) b] = a[i].db + t * (a[i + 1].db - a[i].db);
                    break;
                }
        }
        return out;
    }

    struct Family { float coreLo, coreHi; };

    Family familyFor (Role r)
    {
        switch (r)
        {
            case Role::Vocal: case Role::BackingVocal: return { 150.0f, 6000.0f };
            case Role::Bass808:                        return { 40.0f, 300.0f };
            case Role::Kick:                           return { 45.0f, 3000.0f };
            default:                                   return { 60.0f, 9000.0f };
        }
    }

    float meanOver (const BandArray& v, const BandArray& centres, float lo, float hi)
    {
        float sum = 0.0f; int n = 0;
        for (int b = 0; b < kAnalysisBands; ++b)
            if (centres[(size_t) b] >= lo && centres[(size_t) b] <= hi) { sum += v[(size_t) b]; ++n; }
        return n > 0 ? sum / (float) n : 0.0f;
    }

    struct RegionStat { float meanDev = 0.0f; float peakHz = 0.0f; float peakDb = -200.0f; bool valid = false; };

    RegionStat regionStat (const BandArray& dev, const BandArray& measured, const BandArray& centres, float lo, float hi, bool wantExcess)
    {
        RegionStat s; float sum = 0.0f; int n = 0; float best = wantExcess ? -1.0e9f : 1.0e9f;
        for (int b = 0; b < kAnalysisBands; ++b)
        {
            const float f = centres[(size_t) b];
            if (f < lo || f > hi) continue;
            sum += dev[(size_t) b]; ++n;
            if ((wantExcess && dev[(size_t) b] > best) || (! wantExcess && dev[(size_t) b] < best))
            { best = dev[(size_t) b]; s.peakHz = f; s.peakDb = measured[(size_t) b]; }
        }
        if (n > 0) { s.meanDev = sum / (float) n; s.valid = true; }
        return s;
    }

    juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f / 10.0f) * 10) + " Hz"; }
    juce::String db (float v) { return juce::String (std::abs (v), 1) + " dB"; }

    Suggestion cut (const juce::String& title, const juce::String& why, float freq, float amountDev, float q, bool dynamic, float measuredBandDb)
    {
        Suggestion s;
        s.title = title; s.why = why; s.freqHz = freq; s.amountDb = amountDev;
        // Sized to what was measured: about two thirds of the excess, up to 6 dB. (Recordings sent in from real
        // sessions were 2 to 6 dB heavy here; the old limit of 4.5 dB at 60 % left most of that in.)
        const float g = -juce::jlimit (1.5f, 6.0f, amountDev * 0.7f);
        s.band.enabled = true; s.band.type = EQType::Bell; s.band.freqHz = freq; s.band.q = q;
        if (dynamic)
        {
            s.band.gainDb = 0.0f; s.band.dynamic = true; s.band.rangeDb = g;
            s.band.thresholdDb = juce::jlimit (-60.0f, -6.0f, measuredBandDb + 1.0f);   // just under the band's usual peak level
            s.action = "Dynamic cut, up to " + juce::String (g, 1) + " dB at " + hz (freq);
        }
        else
        {
            s.band.gainDb = g;
            s.action = "Cut " + juce::String (g, 1) + " dB at " + hz (freq);
        }
        return s;
    }

    // most: the largest boost this kind of move may make. A raw vocal is usually 6 to 15 dB short of a finished
    // one in the presence and air ranges, so those need room; a boost for body or weight stays small.
    Suggestion boost (const juce::String& title, const juce::String& why, EQType type, float freq, float lackingDev, float q, float most = 3.0f)
    {
        Suggestion s;
        s.title = title; s.why = why; s.freqHz = freq; s.amountDb = lackingDev;
        const float g = juce::jlimit (1.0f, most, -lackingDev * (most > 3.0f ? 0.6f : 0.4f));
        s.band.enabled = true; s.band.type = type; s.band.freqHz = freq; s.band.q = q; s.band.gainDb = g;
        s.action = juce::String (type == EQType::Bell ? "Boost +" : "Shelf +") + juce::String (g, 1) + " dB at " + hz (freq);
        return s;
    }

    Suggestion lowCut (const juce::String& title, const juce::String& why, float freq, int slope)
    {
        Suggestion s;
        s.title = title; s.why = why; s.freqHz = freq;
        s.band.enabled = true; s.band.type = EQType::LowCut; s.band.freqHz = freq; s.band.q = 0.71f; s.band.slope = slope;
        s.action = "Low cut at " + hz (freq) + ", " + juce::String ((slope + 1) * 12) + " dB/oct";
        return s;
    }
}

BandArray roleTargetCurve (Role role)
{
    switch (role)
    {
        // Where a finished vocal sits, relative to its body. The starting point is the published long-term spectrum
        // of speech (Byrne et al. 1994; as one-third octave levels worked out from the published spectrum levels,
        // about -8 dB at 1 kHz, -13 at 2 kHz, -15 at 3 kHz and -18 at 5 kHz against 500 Hz), which is also where
        // the raw recordings this was checked on sit. On top of that goes
        // what mixing normally adds: 5 to 7 dB of presence and air once the low-mids are cleaned, the top is lifted
        // and the voice is compressed. (Up to build 12 this curve asked for about 10 dB over a natural voice at
        // 2 to 3 kHz. That is more than a mix engineer adds, and chasing it would have made voices hard.)
        case Role::Vocal:
            return curveFrom ({ { 80, -26 }, { 120, -11 }, { 200, -3 }, { 300, 0 }, { 500, 0 }, { 1000, -4 }, { 2000, -8 }, { 3000, -8.5f }, { 5000, -10.5f }, { 8000, -12.5f }, { 12000, -16 }, { 16000, -23 } });
        case Role::BackingVocal:
            return curveFrom ({ { 100, -26 }, { 160, -10 }, { 250, -3 }, { 400, 0 }, { 800, -2 }, { 2000, -7 }, { 3000, -8 }, { 5000, -9.5f }, { 8000, -11 }, { 12000, -14.5f }, { 16000, -22 } });
        case Role::Bass808:
            return curveFrom ({ { 30, -6 }, { 45, 0 }, { 60, 0 }, { 100, -4 }, { 200, -12 }, { 500, -22 }, { 1000, -30 }, { 3000, -40 } });
        case Role::Kick:
            return curveFrom ({ { 40, -4 }, { 60, 0 }, { 100, -3 }, { 200, -10 }, { 400, -16 }, { 1000, -20 }, { 3000, -22 }, { 8000, -30 } });
        case Role::Drums:
            return curveFrom ({ { 50, -3 }, { 70, 0 }, { 100, -1 }, { 200, -4 }, { 500, -8 }, { 1000, -9 }, { 3000, -10 }, { 8000, -12 }, { 12000, -16 } });
        case Role::Melody: case Role::FX:
            return curveFrom ({ { 100, -10 }, { 200, -3 }, { 400, 0 }, { 1000, -2 }, { 2000, -5 }, { 4000, -9 }, { 8000, -14 }, { 12000, -19 } });
        default:   // beat, master, anything full-range
            return curveFrom ({ { 35, -4 }, { 50, 0 }, { 100, 0 }, { 200, -2 }, { 500, -4 }, { 1000, -6 }, { 2000, -8 }, { 4000, -11 }, { 8000, -15 }, { 12000, -19 }, { 16000, -25 } });
    }
}

static void fitCutsToBehaviour (std::vector<Suggestion>& found, const BandArray& centres, const BandArray& swing);
std::vector<Suggestion> analyseToneOnce (const BandArray& m, Role role);

std::vector<Suggestion> analyseTone (const BandArray& m, Role role, const BandArray* swingDb)
{
    auto found = analyseToneOnce (m, role);
    if (swingDb != nullptr) fitCutsToBehaviour (found, analysisBandCentres(), *swingDb);
    return found;
}

// With a listening pass behind it, the analysis knows how each problem behaves over time. A build-up that is
// there all the time is best taken out with a fixed cut; one that comes and goes should only be cut when it is
// there, which is what a dynamic band does.
static void fitCutsToBehaviour (std::vector<Suggestion>& found, const BandArray& centres, const BandArray& swing)
{
    for (auto& s : found)
    {
        if (s.kind != Suggestion::Kind::EqBand || s.band.type != EQType::Bell) continue;
        const bool isCut = s.band.dynamic ? s.band.rangeDb < 0.0f : s.band.gainDb < 0.0f;
        if (! isCut) continue;
        int nearest = 0;
        for (int b = 1; b < kAnalysisBands; ++b)
            if (std::abs (std::log2 (centres[(size_t) b] / s.band.freqHz)) < std::abs (std::log2 (centres[(size_t) nearest] / s.band.freqHz))) nearest = b;
        const float sw = swing[(size_t) nearest];
        const auto where = s.band.freqHz >= 1000.0f ? juce::String (s.band.freqHz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (s.band.freqHz / 10.0f) * 10) + " Hz";
        if (sw < 3.5f && s.band.dynamic)
        {
            const float g = juce::jlimit (-4.5f, -1.0f, s.band.rangeDb * 0.75f);
            s.band.dynamic = false; s.band.gainDb = g; s.band.rangeDb = 0.0f;
            s.action = "Cut " + juce::String (g, 1) + " dB at " + where;
            s.why << " Over the stretch that was listened to it was there the whole time, so a fixed cut is the right tool.";
        }
        else if (sw > 6.5f && ! s.band.dynamic)
        {
            const float g = juce::jlimit (-4.5f, -1.5f, s.band.gainDb * 1.25f);
            s.band.dynamic = true; s.band.rangeDb = g; s.band.gainDb = 0.0f;
            s.action = "Dynamic cut, up to " + juce::String (g, 1) + " dB at " + where;
            s.why << " Over the stretch that was listened to it came and went, so the cut is dynamic: it only works while the build-up is there.";
        }
        else if (s.band.dynamic)
            s.why << " Over the stretch that was listened to it came and went, which is why the cut is dynamic.";
    }
}

std::vector<Suggestion> analyseToneOnce (const BandArray& m, Role role)
{
    std::vector<Suggestion> out;
    const auto centres = analysisBandCentres();
    const auto target = roleTargetCurve (role);
    const auto fam = familyFor (role);
    const float mMean = meanOver (m, centres, fam.coreLo, fam.coreHi);
    const float tMean = meanOver (target, centres, fam.coreLo, fam.coreHi);
    if (mMean < -110.0f) return out;

    BandArray dev {}, rel {};
    for (int b = 0; b < kAnalysisBands; ++b)
    {
        rel[(size_t) b] = m[(size_t) b] - mMean;
        dev[(size_t) b] = rel[(size_t) b] - (target[(size_t) b] - tMean);
    }
    const bool voice = role == Role::Vocal || role == Role::BackingVocal;
    if (voice)
    {
        // A voice is judged against its own body (250 Hz to 1 kHz), the way an engineer hears it: "this needs
        // more presence", "this has too much low mid". Measuring against the average of the whole range instead
        // lets a hole in the presence range drag the average down, so the body then reads as too loud and the
        // hole as half as deep as it is. (Seen on real recordings: a voice 14 dB short at 2 to 3 kHz was being
        // answered with a 5 dB cut at 1 kHz and a 3 dB boost.)
        std::vector<float> body;
        for (int b = 0; b < kAnalysisBands; ++b)
            if (centres[(size_t) b] >= 250.0f && centres[(size_t) b] <= 1000.0f) body.push_back (m[(size_t) b] - target[(size_t) b]);
        if (! body.empty())
        {
            std::sort (body.begin(), body.end());
            const float anchor = body[body.size() / 2];
            for (int b = 0; b < kAnalysisBands; ++b) dev[(size_t) b] = (m[(size_t) b] - target[(size_t) b]) - anchor;
        }
    }
    auto excess = [&] (float lo, float hi) { return regionStat (dev, m, centres, lo, hi, true); };
    auto lack   = [&] (float lo, float hi) { return regionStat (dev, m, centres, lo, hi, false); };

    const bool vocal = role == Role::Vocal || role == Role::BackingVocal;
    if (vocal)
    {
        if (meanOver (rel, centres, 25.0f, 70.0f) > -22.0f)
            out.push_back (lowCut ("Rumble under the voice",
                "There is energy below 70 Hz that is not the singer: mic-stand knocks, plosives and room rumble. It eats headroom and makes the compressor react to things you cannot hear. A low cut removes it without touching the voice.",
                role == Role::BackingVocal ? 130.0f : 85.0f, 1));
        if (auto r = excess (180.0f, 350.0f); r.meanDev > 2.5f)
            out.push_back (cut ("Mud around " + hz (r.peakHz),
                "The low mids are " + db (r.meanDev) + " heavier than a balanced vocal. This is the 'mud' zone: it makes a vocal sound boxed-in and fights the body of the beat. A dynamic cut only pulls it down when it builds up, so quiet lines keep their warmth.",
                r.peakHz, r.meanDev, 1.4f, true, r.peakDb));
        if (auto r = excess (350.0f, 700.0f); r.meanDev > 3.5f)
            out.push_back (cut ("Boxy around " + hz (r.peakHz),
                "There is " + db (r.meanDev) + " extra energy in the 'cardboard box' range, usually from the room or recording close to a wall. A medium-width cut opens the vocal up.",
                r.peakHz, r.meanDev, 1.6f, false, r.peakDb));
        // A voice that is short of presence reads as forward around 1 kHz only because everything above it is
        // missing. Cutting there as well would take away the little intelligibility it has; the presence lift
        // puts the balance right instead.
        if (auto r = excess (800.0f, 1600.0f); r.meanDev > 3.5f && lack (1600.0f, 4500.0f).meanDev > -3.0f)
            out.push_back (cut ("Nasal around " + hz (r.peakHz),
                "The vocal is " + db (r.meanDev) + " forward in the nasal / honky range. A narrow cut here makes it sound less like it is coming through a phone.",
                r.peakHz, r.meanDev, 2.0f, false, r.peakDb));
        if (auto r = excess (2500.0f, 5000.0f); r.meanDev > 3.0f)
            out.push_back (cut ("Harsh around " + hz (r.peakHz),
                "The upper mids are " + db (r.meanDev) + " hotter than usual. This is where vocals get piercing at loud moments. A dynamic cut tames only the harsh words and leaves the clarity alone the rest of the time.",
                r.peakHz, r.meanDev, 2.0f, true, r.peakDb));
        if (auto r = excess (5500.0f, 9000.0f); r.meanDev > 4.0f)
        {
            Suggestion s; s.kind = Suggestion::Kind::DeEsser; s.freqHz = r.peakHz; s.amountDb = r.meanDev;
            s.title = "Sharp S sounds around " + hz (r.peakHz);
            s.why = "The sibilance range is " + db (r.meanDev) + " above a balanced vocal. An EQ cut here would dull every word; a de-esser only ducks the S and T sounds when they happen.";
            s.action = "Switch on the de-esser at " + hz (r.peakHz);
            out.push_back (s);
        }
        // Presence: the boost goes where the hole actually is (anywhere from 2 to 4 kHz) and is wide enough to
        // lift the whole range that carries the words.
        if (auto r = lack (1600.0f, 4500.0f); r.meanDev < -3.0f)
            out.push_back (boost ("Needs presence",
                "The vocal is " + db (r.meanDev) + " short in the 2-4 kHz range where words become understandable. That is what makes a vocal sound dull and buried however loud it is. A wide boost brings it forward so you do not have to turn the whole vocal up.",
                EQType::Bell, juce::jlimit (2200.0f, 3800.0f, r.peakHz), r.meanDev, 0.7f, 7.0f));
        if (auto r = lack (6500.0f, 15000.0f); r.meanDev < -4.0f)
        {
            // a recording that is already rolling off by 5 kHz (a dark microphone, or one sung into from the side)
            // needs the shelf to start lower than one that is only missing its very top
            const bool rollsOffEarly = lack (4500.0f, 6500.0f).meanDev < -7.0f;
            out.push_back (boost ("Needs air",
                "The top of the voice, above " + juce::String (rollsOffEarly ? "5" : "7") + " kHz, is " + db (r.meanDev) + " short, so the vocal sounds closed-in next to a bright beat. A high shelf adds the breathy, finished top end. The de-esser comes after the EQ, so it keeps the S sounds in check.",
                EQType::HighShelf, rollsOffEarly ? 5500.0f : 9000.0f, r.meanDev, 0.71f, 8.0f));
        }
        // Thin means light where every voice has weight: 180 to 380 Hz. Below that depends on who is singing (a
        // woman's voice, or a man in a high register, has nothing at 120 to 180 Hz and is not thin for it; the
        // old rule looked there, and on real sessions it asked for 3 to 6 dB at 180 Hz on voices that had no
        // business having it, which is low-mid weight a muddy mix does not need).
        if (auto r = lack (180.0f, 380.0f); r.meanDev < -5.0f && role == Role::Vocal)
            out.push_back (boost ("Thin: missing body",
                "The chest range of the voice is " + db (r.meanDev) + " lighter than usual, often because the high-pass is set too high or the singer was far from the mic. A small boost puts the weight back.",
                EQType::Bell, juce::jlimit (200.0f, 300.0f, r.peakHz), r.meanDev, 1.0f));
    }
    else if (role == Role::Bass808)
    {
        if (auto r = excess (90.0f, 200.0f); r.meanDev > 3.0f)
            out.push_back (cut ("Boomy around " + hz (r.peakHz),
                "The upper bass is " + db (r.meanDev) + " heavier than a tight 808. It sounds big solo but swamps the kick and the bottom of the vocal in the mix.",
                r.peakHz, r.meanDev, 1.2f, true, r.peakDb));
        if (auto r = excess (200.0f, 450.0f); r.meanDev > 3.5f)
            out.push_back (cut ("Low-mid build-up around " + hz (r.peakHz),
                "There is " + db (r.meanDev) + " of extra low-mid energy. It clouds the space the vocal's body needs.",
                r.peakHz, r.meanDev, 1.2f, false, r.peakDb));
        if (auto r = lack (35.0f, 60.0f); r.meanDev < -4.0f)
            out.push_back (boost ("Light on sub",
                "The sub range is " + db (r.meanDev) + " under a typical 808. Check the note is not too high before boosting; if it is right, a small lift at 50 Hz adds weight.",
                EQType::Bell, 50.0f, r.meanDev, 1.0f));
        if (auto r = lack (600.0f, 2500.0f); r.meanDev < -8.0f)
        {
            Suggestion s; s.kind = Suggestion::Kind::Saturation; s.freqHz = 1000.0f; s.amountDb = r.meanDev;
            s.title = "Will vanish on small speakers";
            s.why = "Almost all the energy is below 200 Hz, which phones and laptops cannot reproduce. Saturation adds harmonics above the note so the 808 is still heard when the sub is not.";
            s.action = "Switch on Warm saturation at 40 %";
            out.push_back (s);
        }
    }
    else if (role == Role::Kick)
    {
        if (auto r = excess (250.0f, 500.0f); r.meanDev > 3.0f)
            out.push_back (cut ("Boxy around " + hz (r.peakHz),
                "The kick has " + db (r.meanDev) + " extra in the cardboard range. Cutting it makes the low end sound deeper and the click clearer without boosting anything.",
                r.peakHz, r.meanDev, 1.5f, false, r.peakDb));
        if (auto r = lack (2000.0f, 5000.0f); r.meanDev < -5.0f)
            out.push_back (boost ("No click",
                "The beater range is " + db (r.meanDev) + " low, so the kick will be felt on big speakers but lost on small ones. A boost around 3 kHz lets it cut through.",
                EQType::Bell, 3000.0f, r.meanDev, 1.2f));
    }
    else
    {
        const bool fullRange = role == Role::Beat || role == Role::Master || role == Role::Auto;
        if (fullRange && meanOver (m, centres, 20.0f, 28.0f) > meanOver (m, centres, 45.0f, 110.0f) - 6.0f)
            out.push_back (lowCut ("Sub rumble below 30 Hz",
                "There is strong energy below 30 Hz that most systems cannot play. It uses up headroom and makes limiters work harder for nothing.",
                28.0f, 1));
        if (auto r = excess (50.0f, 120.0f); fullRange && r.meanDev > 4.0f)
        {
            Suggestion s; s.title = "Bass-heavy"; s.freqHz = 100.0f; s.amountDb = r.meanDev;
            s.why = "The low end is " + db (r.meanDev) + " heavier than a balanced " + juce::String (role == Role::Master ? "master" : "beat") + ". It will sound huge here and boomy everywhere else. A gentle shelf evens it out.";
            s.band.enabled = true; s.band.type = EQType::LowShelf; s.band.freqHz = 110.0f; s.band.q = 0.71f;
            s.band.gainDb = -juce::jlimit (1.0f, 3.0f, r.meanDev * 0.5f);
            s.action = "Shelf " + juce::String (s.band.gainDb, 1) + " dB below 110 Hz";
            out.push_back (s);
        }
        if (auto r = excess (180.0f, 400.0f); r.meanDev > 3.0f)
            out.push_back (cut ("Mud around " + hz (r.peakHz),
                "The low mids are " + db (r.meanDev) + " thicker than usual. This is where pads, keys and reverb pile up and where the vocal's body needs room.",
                r.peakHz, r.meanDev, 1.2f, true, r.peakDb));
        if (auto r = excess (2500.0f, 5000.0f); r.meanDev > 3.0f)
            out.push_back (cut ("Harsh around " + hz (r.peakHz),
                "The upper mids are " + db (r.meanDev) + " hot. This is the range ears are most sensitive to and exactly where the vocal's clarity lives. A dynamic cut takes the edge off the loud moments.",
                r.peakHz, r.meanDev, 1.6f, true, r.peakDb));
        if (auto r = lack (8000.0f, 16000.0f); r.meanDev < -5.0f)
            out.push_back (boost ("Dull top end",
                "There is " + db (r.meanDev) + " less high-frequency energy than usual. A small shelf adds sparkle; keep it modest so hi-hats do not get sharp.",
                EQType::HighShelf, 10000.0f, r.meanDev, 0.71f));
        if (auto r = lack (60.0f, 150.0f); fullRange && r.meanDev < -5.0f)
            out.push_back (boost ("Thin low end",
                "The bass range is " + db (r.meanDev) + " lighter than usual. Check the 808 and kick levels first; if they are right, a low shelf adds weight.",
                EQType::LowShelf, 100.0f, r.meanDev, 0.71f));
    }

    std::sort (out.begin(), out.end(), [] (const Suggestion& a, const Suggestion& b) { return std::abs (a.amountDb) > std::abs (b.amountDb); });
    if (out.size() > 5) out.resize (5);
    return out;
}

Suggestion describeClash (const MaskingClash& c, Role winner, Role loser, bool askerIsLoser)
{
    Suggestion s;
    s.kind = Suggestion::Kind::Masking; s.clash = c; s.freqHz = c.freqHz; s.amountDb = c.severityDb;
    const juce::String at = hz (c.freqHz);
    if (winner == Role::Kick && loser == Role::Bass808)
    {
        s.title = "Kick and 808 collide at " + at;
        s.why = "Both are strongest in the same low band, so the kick loses its punch every time the 808 plays. Dipping the 808 there, only when it is loud, lets the kick hit through without making the 808 smaller.";
    }
    else if (winner == Role::Vocal)
    {
        s.title = c.nameB + " is covering the vocal at " + at;
        s.why = "\"" + c.nameB + "\" is loud in a range the lead vocal needs to be understood. Boosting the vocal would just make everything louder; a small dynamic cut on \"" + c.nameB + "\" clears the space instead.";
    }
    else
    {
        s.title = c.nameB + " sits on " + c.nameA + " at " + at;
        s.why = "These two tracks are competing for the same range. \"" + c.nameA + "\" matters more to the song there, so \"" + c.nameB + "\" gives way with a small dynamic cut.";
    }
    s.action = juce::String (askerIsLoser ? "Dynamic cut on this track, " : "Dynamic cut on \"" + c.nameB + "\", ") + juce::String (c.suggestedCutDb, 1) + " dB at " + at;
    return s;
}

std::vector<EQBandSettings> matchReference (const BandArray& measured, const BandArray& reference, float amount, int maxBands)
{
    std::vector<EQBandSettings> out;
    const auto centres = analysisBandCentres();
    const float lo = 60.0f, hi = 14000.0f;
    const float mMean = meanOver (measured, centres, lo, hi), rMean = meanOver (reference, centres, lo, hi);
    if (mMean < -110.0f || rMean < -110.0f) return out;

    BandArray diff {};
    for (int b = 0; b < kAnalysisBands; ++b)
    {
        const float f = centres[(size_t) b];
        const bool usable = f >= lo && f <= hi && measured[(size_t) b] > mMean - 40.0f && reference[(size_t) b] > rMean - 40.0f;
        diff[(size_t) b] = usable ? (reference[(size_t) b] - rMean) - (measured[(size_t) b] - mMean) : 0.0f;
    }
    BandArray smooth = diff;
    for (int b = 1; b < kAnalysisBands - 1; ++b)
        smooth[(size_t) b] = 0.25f * diff[(size_t) b - 1] + 0.5f * diff[(size_t) b] + 0.25f * diff[(size_t) b + 1];
    const float limit = 6.0f;
    for (auto& d : smooth) d = juce::jlimit (-limit, limit, d * juce::jlimit (0.0f, 1.0f, amount));

    const double fs = 48000.0;
    const float octPerBand = std::log2 (1000.0f) / (float) kAnalysisBands;
    for (int k = 0; k < maxBands; ++k)
    {
        int peak = -1; float best = 0.9f;
        for (int b = 0; b < kAnalysisBands; ++b)
            if (std::abs (smooth[(size_t) b]) > best) { best = std::abs (smooth[(size_t) b]); peak = b; }
        if (peak < 0) break;
        const float g = smooth[(size_t) peak];
        int width = 1;
        for (int b = peak - 1; b >= 0 && smooth[(size_t) b] * g > 0.0f && std::abs (smooth[(size_t) b]) > std::abs (g) * 0.5f; --b) ++width;
        for (int b = peak + 1; b < kAnalysisBands && smooth[(size_t) b] * g > 0.0f && std::abs (smooth[(size_t) b]) > std::abs (g) * 0.5f; ++b) ++width;
        const float bw = juce::jlimit (0.5f, 3.0f, (float) width * octPerBand);
        const float q = juce::jlimit (0.4f, 3.0f, std::sqrt (std::pow (2.0f, bw)) / (std::pow (2.0f, bw) - 1.0f));
        EQBandSettings band;
        band.enabled = true; band.type = EQType::Bell; band.freqHz = centres[(size_t) peak]; band.gainDb = g; band.q = q;
        out.push_back (band);
        for (int b = 0; b < kAnalysisBands; ++b)
            smooth[(size_t) b] -= EQEngine::bandResponseDb (band, centres[(size_t) b], fs);
    }
    return out;
}
}
