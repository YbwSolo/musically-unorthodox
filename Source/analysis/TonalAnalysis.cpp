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

juce::StringArray eqSourceNames()
{
    return { "Same as track kind",
             "Lead vocal", "Rap vocal", "High voice", "Low voice", "Backing vocals", "Adlibs",
             "Kick", "Snare / clap", "Hi-hats / cymbals", "Drum bus", "808 / sub bass", "Bass guitar / synth bass", "Piano / keys",
             "Acoustic guitar", "Electric guitar", "Synth lead / pluck", "Pad / strings", "Brass / horns", "Full beat (2-track)", "Sound FX" };
}

Role eqSourceRole (EqSource s, Role trackRole)
{
    switch (s)
    {
        case EqSource::LeadVocal: case EqSource::RapVocal: case EqSource::HighVocal: case EqSource::LowVocal: return Role::Vocal;
        case EqSource::BackingVocals: case EqSource::Adlibs: return Role::BackingVocal;
        case EqSource::Kick:     return Role::Kick;
        case EqSource::Sub808: case EqSource::Bass: return Role::Bass808;
        case EqSource::Snare: case EqSource::Hats: case EqSource::DrumBus: return Role::Drums;
        case EqSource::Piano: case EqSource::AcousticGuitar: case EqSource::ElectricGuitar: case EqSource::SynthLead: case EqSource::Pad: case EqSource::Brass: return Role::Melody;
        case EqSource::FullBeat: return Role::Beat;
        case EqSource::SoundFx:  return Role::FX;
        case EqSource::TrackKind: case EqSource::Count: break;
    }
    return trackRole;
}

// The shapes for the sources that have one of their own. Like the role curves, these are reasoned from how the
// instruments are built and from ordinary mixing practice (where each has its weight, its body and its edge).
// They were not measured from a library of finished stems, and only the shape matters, not the level.
BandArray sourceTargetCurve (EqSource source, Role trackRole)
{
    switch (source)
    {
        case EqSource::RapVocal:        // a spoken voice close to the microphone: the same body, a touch less air
            return curveFrom ({ { 80, -26 }, { 120, -11 }, { 200, -3 }, { 300, 0 }, { 500, 0 }, { 1000, -4 }, { 2000, -8 }, { 3000, -8.5f }, { 5000, -11 }, { 8000, -14 }, { 12000, -18 }, { 16000, -25 } });
        case EqSource::HighVocal:       // everything sits about a third higher: nothing under 160 Hz to speak of
            return curveFrom ({ { 100, -28 }, { 160, -12 }, { 250, -3 }, { 400, 0 }, { 650, 0 }, { 1200, -4 }, { 2400, -8 }, { 3500, -8.5f }, { 5500, -10.5f }, { 8500, -12.5f }, { 12000, -15.5f }, { 16000, -22 } });
        case EqSource::LowVocal:        // chest weight from 100 Hz
            return curveFrom ({ { 65, -26 }, { 100, -10 }, { 160, -3 }, { 250, 0 }, { 450, 0 }, { 900, -4 }, { 1800, -8 }, { 3000, -8.5f }, { 5000, -10.5f }, { 8000, -13 }, { 12000, -17 }, { 16000, -24 } });
        case EqSource::Adlibs:
            return roleTargetCurve (Role::BackingVocal);
        case EqSource::Snare:
            return curveFrom ({ { 100, -16 }, { 180, -2 }, { 250, 0 }, { 500, -5 }, { 1000, -6 }, { 2500, -5 }, { 5000, -6 }, { 8000, -9 }, { 12000, -14 } });
        case EqSource::Hats:
            return curveFrom ({ { 300, -36 }, { 1000, -22 }, { 3000, -8 }, { 6000, -2 }, { 9000, 0 }, { 12000, -2 }, { 16000, -8 } });
        case EqSource::Bass:
            return curveFrom ({ { 35, -6 }, { 60, 0 }, { 100, 0 }, { 200, -5 }, { 500, -12 }, { 1000, -18 }, { 2500, -24 }, { 5000, -32 } });
        case EqSource::Piano:
            return curveFrom ({ { 60, -12 }, { 120, -4 }, { 250, 0 }, { 500, -1 }, { 1000, -4 }, { 2000, -8 }, { 4000, -12 }, { 8000, -18 }, { 12000, -24 } });
        case EqSource::AcousticGuitar:
            return curveFrom ({ { 80, -14 }, { 120, -4 }, { 200, 0 }, { 400, -3 }, { 1000, -6 }, { 2500, -8 }, { 5000, -10 }, { 8000, -13 }, { 12000, -18 } });
        case EqSource::ElectricGuitar:
            return curveFrom ({ { 80, -12 }, { 150, -3 }, { 300, 0 }, { 800, -1 }, { 1500, -2 }, { 3000, -4 }, { 5000, -10 }, { 8000, -20 } });
        case EqSource::SynthLead:
            return curveFrom ({ { 150, -8 }, { 300, -2 }, { 600, 0 }, { 1500, -1 }, { 3000, -4 }, { 6000, -8 }, { 10000, -13 } });
        case EqSource::Pad:
            return curveFrom ({ { 100, -8 }, { 200, -2 }, { 400, 0 }, { 1000, -3 }, { 2500, -7 }, { 5000, -11 }, { 10000, -16 } });
        case EqSource::Brass:
            return curveFrom ({ { 120, -10 }, { 250, -2 }, { 500, 0 }, { 1200, -1 }, { 2500, -4 }, { 5000, -10 }, { 9000, -18 } });
        case EqSource::TrackKind: case EqSource::LeadVocal: case EqSource::BackingVocals: case EqSource::Kick: case EqSource::DrumBus:
        case EqSource::Sub808: case EqSource::FullBeat: case EqSource::SoundFx: case EqSource::Count: break;
    }
    return roleTargetCurve (eqSourceRole (source, trackRole));
}

namespace
{
    // One thing to look for on an instrument: a range, what counts as too much or too little there, and the move.
    struct Zone
    {
        enum Kind { Cut, DynCut, Bell, HighShelf, LowShelf };
        Kind kind; const char* title; float lo, hi, limitDb, q, most, at; const char* why;
    };
    struct Profile
    {
        const char* name; float coreLo, coreHi;
        float lowCutHz; const char* lowCutWhy;          // 0 = this source needs its low end
        float resonanceFromHz;                          // 0 = do not look for single ringing notes (a bass note is meant to stand out)
        std::vector<Zone> zones;
    };

    const Profile* profileFor (EqSource s)
    {
        static const Profile snare { "a snare", 120.0f, 8000.0f, 90.0f, "Below 90 Hz a snare or clap has nothing of its own: what is there is the kick and the 808 leaking in, and it clouds both.", 300.0f, {
            { Zone::Cut,       "Boxy",            350.0f, 700.0f, 3.0f, 1.6f, 0.0f, 0.0f,    "This is the cardboard range. Taking it out lets the body and the crack be heard separately." },
            { Zone::DynCut,    "Harsh crack",     2500.0f, 5000.0f, 4.0f, 1.8f, 0.0f, 0.0f,  "It is the range ears are most sensitive to, and it is where a snare hurts on the loud hits." },
            { Zone::Bell,      "Thin: no body",   150.0f, 250.0f, -5.0f, 1.0f, 4.0f, 200.0f, "The body of a snare is around 200 Hz. Without it the hit is all top and no weight." },
            { Zone::HighShelf, "Dull: no snap",   5000.0f, 10000.0f, -5.0f, 0.71f, 4.0f, 7000.0f, "The snap and the wires are up here. A shelf brings them out without making the crack harder." } } };
        static const Profile hats { "hi-hats", 2000.0f, 14000.0f, 300.0f, "Hi-hats and cymbals have nothing useful under 300 Hz. What is there is the rest of the kit or the beat leaking in.", 0.0f, {
            { Zone::Cut,       "Clangy",          800.0f, 2000.0f, 4.0f, 1.4f, 0.0f, 0.0f,   "The metallic clang sits here. It makes hats sound cheap and it sits right on the vocal." },
            { Zone::DynCut,    "Harsh",           3000.0f, 6000.0f, 4.0f, 1.5f, 0.0f, 0.0f,  "This is where hats get piercing on the open or accented hits." },
            { Zone::HighShelf, "Dull",            9000.0f, 15000.0f, -5.0f, 0.71f, 3.0f, 10000.0f, "The shimmer is above 9 kHz. A small shelf is enough; more turns into hiss." } } };
        static const Profile drums { "a drum bus", 60.0f, 9000.0f, 0.0f, "", 250.0f, {
            { Zone::DynCut,    "Mud",             180.0f, 300.0f, 3.5f, 1.2f, 0.0f, 0.0f,    "The ring of the toms and the kick builds up here and fills the space the bass needs." },
            { Zone::Cut,       "Boxy",            300.0f, 600.0f, 3.0f, 1.2f, 0.0f, 0.0f,    "The cardboard range of the whole kit. Cutting it makes the kit sound bigger, not smaller." },
            { Zone::DynCut,    "Harsh cymbals",   3000.0f, 6000.0f, 3.5f, 1.6f, 0.0f, 0.0f,  "The cymbals and the snare crack pile up here on the loud hits." },
            { Zone::LowShelf,  "Thin low end",    60.0f, 120.0f, -5.0f, 0.71f, 3.0f, 90.0f,  "The weight of the kick is missing. Check its level first; if that is right, a low shelf adds it." },
            { Zone::HighShelf, "Dull top",        8000.0f, 15000.0f, -5.0f, 0.71f, 3.0f, 10000.0f, "The air of the cymbals is missing. Keep the shelf modest so the hats do not get sharp." } } };
        static const Profile bass { "a bass", 40.0f, 1000.0f, 0.0f, "", 0.0f, {
            { Zone::DynCut,    "Boomy",           90.0f, 200.0f, 3.0f, 1.2f, 0.0f, 0.0f,     "Some notes are much louder than others here. It sounds big on its own and swamps the kick in the mix." },
            { Zone::Cut,       "Muddy",           200.0f, 400.0f, 3.5f, 1.2f, 0.0f, 0.0f,    "This clouds the space the body of the vocal needs, and adds no weight." },
            { Zone::Bell,      "No definition",   700.0f, 2000.0f, -6.0f, 0.9f, 4.0f, 1000.0f, "The note is felt but not heard: on a phone or a laptop the bass line disappears. The string and pick noise up here is what lets it be followed." },
            { Zone::Bell,      "Light on low end", 45.0f, 90.0f, -4.0f, 1.0f, 3.0f, 70.0f,   "The fundamental is weak. Check the part is not played too high before boosting." } } };
        static const Profile piano { "a piano", 80.0f, 6000.0f, 60.0f, "Under 60 Hz a piano in a mix gives pedal thumps and rumble, and it gets in the way of the bass.", 300.0f, {
            { Zone::DynCut,    "Mud",             180.0f, 400.0f, 3.0f, 1.2f, 0.0f, 0.0f,    "The left hand builds up here on some chords. It thickens the piano on its own and clouds the mix." },
            { Zone::Cut,       "Boxy",            400.0f, 800.0f, 3.5f, 1.4f, 0.0f, 0.0f,    "The wooden, closed-lid sound. Usually the room or the microphone position." },
            { Zone::DynCut,    "Hard hammers",    2000.0f, 4500.0f, 4.0f, 1.6f, 0.0f, 0.0f,  "The attack of the hammers gets hard when the playing is loud, and it sits where the vocal's words are." },
            { Zone::Bell,      "Lacks presence",  2000.0f, 5000.0f, -5.0f, 0.8f, 4.0f, 3000.0f, "The attack that lets each note be picked out is missing, so the piano blurs into the pad." },
            { Zone::HighShelf, "Dull",            7000.0f, 14000.0f, -6.0f, 0.71f, 3.0f, 8000.0f, "The top of the strings is missing. A small shelf opens it up." } } };
        static const Profile acoustic { "an acoustic guitar", 100.0f, 8000.0f, 80.0f, "Under 80 Hz an acoustic guitar gives handling noise and the thump of the body, not notes.", 250.0f, {
            { Zone::DynCut,    "Boomy",           100.0f, 220.0f, 3.0f, 1.3f, 0.0f, 0.0f,    "The body of the guitar booms on some notes more than others, most of all with the microphone near the sound hole." },
            { Zone::Cut,       "Boxy",            300.0f, 600.0f, 3.5f, 1.4f, 0.0f, 0.0f,    "The cardboard range. Cutting it leaves the strings and the body clearer." },
            { Zone::DynCut,    "Harsh pick",      2000.0f, 4500.0f, 4.0f, 1.8f, 0.0f, 0.0f,  "The pick attack gets hard on the loud strums and sits where the vocal's words are." },
            { Zone::Bell,      "Thin",            150.0f, 300.0f, -6.0f, 1.0f, 3.0f, 200.0f, "The warmth of the body is missing, so the guitar is all strings." },
            { Zone::HighShelf, "Dull strings",    7000.0f, 14000.0f, -5.0f, 0.71f, 4.0f, 9000.0f, "The sparkle of the strings is above 7 kHz. A shelf brings it back." } } };
        static const Profile electric { "an electric guitar", 100.0f, 6000.0f, 90.0f, "Under 90 Hz an electric guitar only adds weight that belongs to the bass.", 300.0f, {
            { Zone::Cut,       "Muddy",           180.0f, 400.0f, 3.0f, 1.2f, 0.0f, 0.0f,    "The low strings and the cabinet build up here. It makes the guitar big alone and cloudy in the mix." },
            { Zone::Cut,       "Honky",           700.0f, 1400.0f, 4.0f, 1.6f, 0.0f, 0.0f,   "The nasal, boxed-speaker sound." },
            { Zone::DynCut,    "Fizzy",           2500.0f, 5000.0f, 3.5f, 1.8f, 0.0f, 0.0f,  "The harsh fizz of the distortion. It is tiring, and it is exactly where the vocal needs to be clear." },
            { Zone::Bell,      "Buried: no bite", 1500.0f, 3500.0f, -5.0f, 0.9f, 4.0f, 2500.0f, "The bite that lets a guitar cut through is missing." } } };
        static const Profile lead { "a synth lead", 150.0f, 9000.0f, 100.0f, "Under 100 Hz a lead has nothing the song needs, and it gets in the way of the 808.", 400.0f, {
            { Zone::Cut,       "Muddy",           200.0f, 450.0f, 3.5f, 1.2f, 0.0f, 0.0f,    "Extra low-mid weight that clouds the body of the vocal." },
            { Zone::DynCut,    "Piercing",        2500.0f, 5500.0f, 3.5f, 1.8f, 0.0f, 0.0f,  "The high notes get piercing here, right where the vocal's words are." },
            { Zone::Bell,      "Thin",            250.0f, 500.0f, -6.0f, 1.0f, 3.0f, 350.0f, "The body of the sound is missing, so it sounds small." },
            { Zone::HighShelf, "Dull",            7000.0f, 14000.0f, -6.0f, 0.71f, 3.0f, 9000.0f, "The top is missing, so the lead sits behind the hats." } } };
        static const Profile pad { "a pad", 120.0f, 8000.0f, 120.0f, "Pads and strings carry low energy that nobody hears as part of the pad. It only clouds the bass.", 400.0f, {
            { Zone::DynCut,    "Mud",             200.0f, 500.0f, 3.0f, 1.0f, 0.0f, 0.0f,    "This is where pads, keys and reverb pile up, and where the vocal's body needs room." },
            { Zone::DynCut,    "Harsh",           2500.0f, 5000.0f, 4.0f, 1.6f, 0.0f, 0.0f,  "The pad is bright in the range where the vocal's words are." },
            { Zone::HighShelf, "Dull",            8000.0f, 15000.0f, -6.0f, 0.71f, 3.0f, 10000.0f, "A little air on a pad gives the mix height without getting in the vocal's way." } } };
        static const Profile brass { "brass", 150.0f, 7000.0f, 100.0f, "Under 100 Hz brass only adds rumble and weight that belongs to the bass.", 300.0f, {
            { Zone::Cut,       "Muddy",           200.0f, 400.0f, 3.5f, 1.2f, 0.0f, 0.0f,    "Extra weight that makes brass sound slow and clouds the mix." },
            { Zone::Cut,       "Honky",           800.0f, 1500.0f, 4.0f, 1.6f, 0.0f, 0.0f,   "The nasal range of brass." },
            { Zone::DynCut,    "Blaring",         2000.0f, 4500.0f, 3.5f, 1.6f, 0.0f, 0.0f,  "Brass gets very loud here when it is played hard, and it covers the vocal." },
            { Zone::Bell,      "Dull",            3000.0f, 6000.0f, -6.0f, 0.9f, 3.0f, 4500.0f, "The brightness that makes brass sound like brass is missing." } } };
        switch (s)
        {
            case EqSource::Snare: return &snare;      case EqSource::Hats: return &hats;            case EqSource::DrumBus: return &drums;
            case EqSource::Bass: return &bass;        case EqSource::Piano: return &piano;          case EqSource::AcousticGuitar: return &acoustic;
            case EqSource::ElectricGuitar: return &electric; case EqSource::SynthLead: return &lead; case EqSource::Pad: return &pad;
            case EqSource::Brass: return &brass;
            case EqSource::TrackKind: case EqSource::LeadVocal: case EqSource::RapVocal: case EqSource::HighVocal: case EqSource::LowVocal:
            case EqSource::BackingVocals: case EqSource::Adlibs: case EqSource::Kick: case EqSource::Sub808: case EqSource::FullBeat:
            case EqSource::SoundFx: case EqSource::Count: break;
        }
        return nullptr;
    }

    // what differs from one kind of voice to the next
    struct VoiceShape { float lowCutHz, mudLo, mudHi, thinLo, thinHi; bool wantsBody; };
    VoiceShape voiceShape (EqSource s, Role role)
    {
        switch (s)
        {
            case EqSource::HighVocal:     return { 110.0f, 220.0f, 420.0f, 220.0f, 450.0f, true };
            case EqSource::LowVocal:      return { 70.0f, 150.0f, 320.0f, 140.0f, 320.0f, true };
            case EqSource::RapVocal:      return { 90.0f, 180.0f, 350.0f, 180.0f, 380.0f, true };
            case EqSource::Adlibs:        return { 150.0f, 180.0f, 350.0f, 180.0f, 380.0f, false };
            case EqSource::BackingVocals: return { 130.0f, 180.0f, 350.0f, 180.0f, 380.0f, false };
            default: break;
        }
        return { role == Role::BackingVocal ? 130.0f : 85.0f, 180.0f, 350.0f, 180.0f, 380.0f, role == Role::Vocal };
    }
}

static void fitCutsToBehaviour (std::vector<Suggestion>& found, const BandArray& centres, const BandArray& swing);
static void findResonances (std::vector<Suggestion>& out, const BandArray& m, const BandArray& target, float fromHz, float toHz, float clearBy);
std::vector<Suggestion> analyseToneOnce (const BandArray& m, Role role, EqSource source);

static EqStage stageOf (const Suggestion& s)
{
    if (s.kind == Suggestion::Kind::DeEsser || s.kind == Suggestion::Kind::Masking || s.kind == Suggestion::Kind::VocalRoom) return EqStage::Dynamic;
    if (s.kind == Suggestion::Kind::Saturation) return EqStage::Tonal;
    if (s.band.type == EQType::LowCut || s.band.type == EQType::HighCut || s.band.type == EQType::Notch) return EqStage::Surgical;
    if (s.band.dynamic) return s.band.rangeDb < 0.0f ? EqStage::Dynamic : EqStage::Tonal;
    return s.band.gainDb < 0.0f ? EqStage::Surgical : EqStage::Tonal;
}

std::vector<Suggestion> analyseTone (const BandArray& m, Role role, const BandArray* swingDb, EqSource source, int passes)
{
    const auto centres = analysisBandCentres();
    const Role family = eqSourceRole (source, role);
    auto found = analyseToneOnce (m, family, source);
    // single ringing notes, which the broad ranges above average away
    {
        const auto* profile = profileFor (source);
        const bool voice = family == Role::Vocal || family == Role::BackingVocal;
        float from = profile != nullptr ? profile->resonanceFromHz : 0.0f;
        // Not on a bass or a kick (the note is meant to stand out), and not on a whole beat or a mix, where one
        // band standing clear of the rest is far more likely to be the tune than a fault.
        if (profile == nullptr) from = voice ? 350.0f : 0.0f;
        const float clearBy = (voice || family == Role::Drums) ? 5.0f : 6.5f;      // anything that plays notes has to stand further out
        if (from > 0.0f) findResonances (found, m, sourceTargetCurve (source, role), from, voice ? 5000.0f : 8000.0f, clearBy);
    }
    if (swingDb != nullptr) fitCutsToBehaviour (found, centres, *swingDb);
    for (auto& s : found) s.stage = stageOf (s);

    // Surgical, then tonal: what to bring out is worked out on the sound as it will be once the fixed cuts are in.
    // (A voice with 5 dB of mud reads as short of presence partly because the mud is there. Take the mud out
    // first, and the presence it needs is smaller.)
    BandArray after = m; bool anyCut = false;
    for (auto& s : found)
        if (s.kind == Suggestion::Kind::EqBand && s.stage == EqStage::Surgical && (passes & 1) != 0)
        {
            for (int b = 0; b < kAnalysisBands; ++b) after[(size_t) b] += EQEngine::bandResponseDb (s.band, centres[(size_t) b], 48000.0);
            anyCut = true;
        }
    if (anyCut)
    {
        auto again = analyseToneOnce (after, family, source);
        for (auto& s : again) s.stage = stageOf (s);
        found.erase (std::remove_if (found.begin(), found.end(), [] (const Suggestion& s) { return s.stage == EqStage::Tonal; }), found.end());
        for (auto& s : again) if (s.stage == EqStage::Tonal) found.push_back (s);
    }

    // which jobs were asked for, at most three of each (the EQ has eight bands), in the order they are done
    std::vector<Suggestion> out;
    for (int stage = 0; stage < 3; ++stage)
    {
        if ((passes & (1 << stage)) == 0) continue;
        std::vector<Suggestion> part;
        for (auto& s : found) if ((int) s.stage == stage) part.push_back (s);
        std::stable_sort (part.begin(), part.end(), [] (const Suggestion& a, const Suggestion& b) { return std::abs (a.amountDb) > std::abs (b.amountDb); });
        if (part.size() > 3) part.resize (3);
        out.insert (out.end(), part.begin(), part.end());
    }
    return out;
}

// A band that stands well clear of the bands either side of it, once the shape the source should have is taken
// out, is one note or one room mode ringing: the "whistle" in a voice, the ring of a snare, the note a guitar
// body booms on. The analysis bands are a third of an octave wide, so this finds where it is to within that;
// the cut is narrow, and it is for the ear to fine-tune on the EQ display.
static void findResonances (std::vector<Suggestion>& out, const BandArray& m, const BandArray& target, float fromHz, float toHz, float clearBy)
{
    const auto centres = analysisBandCentres();
    struct Peak { int band; float over; };
    std::vector<Peak> peaks;
    for (int b = 2; b < kAnalysisBands - 2; ++b)
    {
        const float f = centres[(size_t) b];
        if (f < fromHz || f > toHz || m[(size_t) b] < -90.0f) continue;
        auto d = [&] (int i) { return m[(size_t) i] - target[(size_t) i]; };
        const float around = 0.25f * (d (b - 2) + d (b - 1) + d (b + 1) + d (b + 2));
        const float over = d (b) - around;
        if (over >= clearBy && d (b) > d (b - 1) && d (b) > d (b + 1)) peaks.push_back ({ b, over });
    }
    std::sort (peaks.begin(), peaks.end(), [] (const Peak& a, const Peak& b) { return a.over > b.over; });
    int added = 0;
    for (auto& p : peaks)
    {
        if (added >= 2) break;
        const float f = centres[(size_t) p.band];
        bool taken = false;       // a broad cut already covers it
        for (auto& s : out) if (s.kind == Suggestion::Kind::EqBand && std::abs (std::log2 (s.band.freqHz / f)) < 0.35f) taken = true;
        if (taken) continue;
        auto s = cut ("Ringing around " + hz (f),
                      "One narrow range stands " + db (p.over) + " clear of everything either side of it. That is a single note, a room mode or the ring of the instrument, heard as a whistle or a hollow tone that never goes away. A narrow cut takes it out without changing the tone around it. The analysis hears in thirds of an octave: sweep the band a little either way on the display to find the exact spot.",
                      f, p.over, 4.0f, false, m[(size_t) p.band]);
        s.band.gainDb = -juce::jlimit (2.0f, 4.5f, p.over * 0.55f);
        s.action = "Narrow cut " + juce::String (s.band.gainDb, 1) + " dB at " + hz (f);
        out.push_back (s);
        ++added;
    }
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

std::vector<Suggestion> analyseToneOnce (const BandArray& m, Role role, EqSource source)
{
    std::vector<Suggestion> out;
    const auto centres = analysisBandCentres();
    const auto target = sourceTargetCurve (source, role);
    const auto* profile = profileFor (source);
    auto fam = familyFor (role);
    if (profile != nullptr) fam = { profile->coreLo, profile->coreHi };
    const auto shape = voiceShape (source, role);
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
                shape.lowCutHz, 1));
        if (auto r = excess (shape.mudLo, shape.mudHi); r.meanDev > 2.5f)
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
        if (auto r = lack (shape.thinLo, shape.thinHi); r.meanDev < -5.0f && shape.wantsBody)
            out.push_back (boost ("Thin: missing body",
                "The chest range of the voice is " + db (r.meanDev) + " lighter than usual, often because the high-pass is set too high or the singer was far from the mic. A small boost puts the weight back.",
                EQType::Bell, juce::jlimit (shape.thinLo + 20.0f, shape.thinHi - 80.0f, r.peakHz), r.meanDev, 1.0f));
    }
    else if (profile != nullptr)
    {
        // a named instrument: its own list of what goes wrong and where
        const juce::String name (profile->name);
        // (only when there is more down there than the instrument itself would put there)
        if (profile->lowCutHz > 0.0f && meanOver (rel, centres, 22.0f, profile->lowCutHz * 0.8f) > -36.0f && meanOver (dev, centres, 22.0f, profile->lowCutHz * 0.8f) > 4.0f)
            out.push_back (lowCut ("Nothing useful below " + hz (profile->lowCutHz), juce::String (profile->lowCutWhy) + " A low cut takes it out without touching the sound itself.", profile->lowCutHz, 1));
        for (auto& z : profile->zones)
        {
            const bool isCut = z.kind == Zone::Cut || z.kind == Zone::DynCut;
            if (isCut)
            {
                const auto r = excess (z.lo, z.hi);
                if (r.meanDev > z.limitDb)
                    out.push_back (cut (juce::String (z.title) + " around " + hz (r.peakHz),
                                        "Between " + hz (z.lo) + " and " + hz (z.hi) + " there is " + db (r.meanDev) + " more than usual for " + name + ". " + z.why,
                                        r.peakHz, r.meanDev, z.q, z.kind == Zone::DynCut, r.peakDb));
            }
            else
            {
                const auto r = lack (z.lo, z.hi);
                if (r.meanDev < z.limitDb)
                    out.push_back (boost (z.title, "Between " + hz (z.lo) + " and " + hz (z.hi) + " there is " + db (r.meanDev) + " less than usual for " + name + ". " + z.why,
                                          z.kind == Zone::Bell ? EQType::Bell : z.kind == Zone::HighShelf ? EQType::HighShelf : EQType::LowShelf, z.at, r.meanDev, z.q, z.most));
            }
        }
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
    return out;
}

// Where a beat (or any track) covers the lead vocal. Both spectra are as the tracks leave their channels, so the
// difference between them is how they stand against each other in the mix. A place counts when the vocal has
// real energy there and the track is within 6 dB of it or louder; the ranges that carry the words count most.
VocalRoomPlan planVocalRoom (const BandArray& vocal, const BandArray& track)
{
    VocalRoomPlan plan;
    const auto centres = analysisBandCentres();
    float vMax = -200.0f;
    for (int b = 0; b < kAnalysisBands; ++b)
        if (centres[(size_t) b] >= 150.0f && centres[(size_t) b] <= 9000.0f) vMax = juce::jmax (vMax, vocal[(size_t) b]);
    if (vMax < -90.0f) { plan.text = "The lead vocal has not been heard yet, so there is nothing to make room for. Play the song with the vocal in it and try again."; return plan; }

    BandArray need {};
    for (int b = 0; b < kAnalysisBands; ++b)
    {
        const float f = centres[(size_t) b];
        if (f < 150.0f || f > 9000.0f || track[(size_t) b] < -90.0f) continue;
        const float matters = juce::jlimit (0.0f, 1.0f, 1.0f - (vMax - vocal[(size_t) b]) / 18.0f);            // the vocal's own strong ranges
        const float words = (f >= 1500.0f && f <= 5000.0f) ? 1.0f : (f >= 200.0f && f <= 500.0f) ? 0.8f : 0.6f;  // presence first, then body
        need[(size_t) b] = juce::jmax (0.0f, track[(size_t) b] - vocal[(size_t) b] + 6.0f) * matters * words;
    }
    BandArray smooth = need;
    for (int b = 1; b < kAnalysisBands - 1; ++b) smooth[(size_t) b] = 0.25f * need[(size_t) b - 1] + 0.5f * need[(size_t) b] + 0.25f * need[(size_t) b + 1];

    struct Spot { int band; float need; };
    std::vector<Spot> spots;
    for (int k = 0; k < 3; ++k)
    {
        int best = -1; float top = 1.0f;
        for (int b = 1; b < kAnalysisBands - 1; ++b)
        {
            bool near = false;
            for (auto& sp : spots) if (std::abs (b - sp.band) < 4) near = true;        // a good octave apart
            if (! near && smooth[(size_t) b] > top) { top = smooth[(size_t) b]; best = b; }
        }
        if (best < 0) break;
        spots.push_back ({ best, top });
    }
    if (spots.empty()) { plan.text = "This track already leaves the vocal room: nowhere between 150 Hz and 9 kHz is it within 6 dB of the vocal where the vocal is strong."; return plan; }
    std::sort (spots.begin(), spots.end(), [] (const Spot& a, const Spot& b) { return a.band < b.band; });

    const float octPerBand = std::log2 (1000.0f) / (float) kAnalysisBands;
    juce::StringArray parts;
    for (size_t i = 0; i < spots.size(); ++i)
    {
        const int b = spots[i].band;
        int width = 1;
        for (int j = b - 1; j >= 0 && smooth[(size_t) j] > spots[i].need * 0.5f; --j) ++width;
        for (int j = b + 1; j < kAnalysisBands && smooth[(size_t) j] > spots[i].need * 0.5f; ++j) ++width;
        const float bw = juce::jlimit (0.6f, 2.0f, (float) width * octPerBand);
        plan.freqHz[i] = centres[(size_t) b];
        plan.cutDb[i] = -juce::jlimit (1.5f, 6.0f, 1.5f + 0.35f * spots[i].need);
        plan.q[i] = juce::jlimit (0.6f, 2.5f, std::sqrt (std::pow (2.0f, bw)) / (std::pow (2.0f, bw) - 1.0f));
        parts.add (hz (plan.freqHz[i]) + " (" + juce::String (plan.cutDb[i], 1) + " dB)");
    }
    plan.count = (int) spots.size();
    plan.found = true;
    plan.text = parts.joinIntoString (", ");
    return plan;
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
