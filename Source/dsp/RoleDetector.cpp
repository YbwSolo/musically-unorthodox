#include "RoleDetector.h"

namespace mu
{
RoleGuess detectRole (const RoleFeatures& f) noexcept
{
    if (! f.valid) return { Role::Beat, 0.0f, "listening" };

    const float low = f.lowEnergyRatio, high = f.highEnergyRatio, cen = f.centroidHz, crest = f.crestFactorDb;

    if (low > 0.55f && cen < 350.0f)
    {
        if (crest > 13.0f) return { Role::Kick, juce::jlimit (0.4f, 0.95f, low), "sub-heavy, punchy transients" };
        return { Role::Bass808, juce::jlimit (0.4f, 0.95f, low), "sub-heavy, sustained" };
    }

    // an uncompressed rap vocal can peak 20 dB above its average, so the upper limit is generous
    if (low < 0.12f && cen > 700.0f && cen < 3200.0f && crest > 7.0f && crest < 23.0f && high < 0.35f)
        return { Role::Vocal, 0.6f + juce::jmin (0.3f, (0.12f - low) * 2.0f), "mid-focused, speech-like dynamics" };

    // Tune or Harmony on, or a mono track with its weight in the middle: a voice
    if ((f.voiceTools || f.mono) && low < 0.25f && cen > 350.0f && cen < 4500.0f)
        return { Role::Vocal, 0.55f, f.voiceTools ? "a voice tool is switched on" : "mono, mid-focused" };

    if (high > 0.35f && low < 0.08f)
        return { Role::FX, 0.55f, "bright, little low end" };

    if (crest > 15.0f && low > 0.12f && low < 0.5f)
        return { Role::Drums, 0.5f, "transient-heavy with low end" };

    if (low < 0.2f && cen > 300.0f && cen < 2000.0f && crest < 10.0f)
        return { Role::Melody, 0.45f, "sustained mid content" };

    if (! f.mono && low > 0.18f && low < 0.55f && cen > 150.0f && cen < 1800.0f)
        return { Role::Beat, 0.5f, "full-range balance" };

    // a finished beat is stereo; a mono track that fits nothing else is most often an instrument
    if (f.mono) return { low > 0.4f ? Role::Bass808 : Role::Melody, 0.3f, "mono, unclear - set the role manually" };
    return { Role::Beat, 0.25f, "unclear - set the role manually" };
}

bool roleFromName (const juce::String& name, Role& role)
{
    const auto lower = name.toLowerCase().trim();
    if (lower.isEmpty()) return false;
    // the output channel of the song
    for (auto* exact : { "master", "stereo out", "stereo output", "output 1-2", "out 1-2", "main out", "mix bus", "mixbus", "master bus" })
        if (lower == exact) { role = Role::Master; return true; }

    juce::StringArray words;
    juce::String current;
    for (auto c : lower)
    {
        if (juce::CharacterFunctions::isLetter (c) || (juce::CharacterFunctions::isDigit (c) && current.containsOnly ("0123456789"))) current << juce::String::charToString (c);
        else { if (current.isNotEmpty()) words.add (current); current.clear(); if (juce::CharacterFunctions::isDigit (c)) current << juce::String::charToString (c); }
    }
    if (current.isNotEmpty()) words.add (current);
    auto has = [&words] (std::initializer_list<const char*> list)
    {
        for (auto* w : list) if (words.contains (w)) return true;
        return false;
    };
    // most specific first: "hook adlibs" is backing, "lead synth" is a melody, "vocal bass" is still a bass
    if (lower.contains ("ad lib") || lower.contains ("ad-lib") || has ({ "adlib", "adlibs", "backing", "backings", "bv", "bvs", "bgv", "bgvs", "double", "doubles", "dub", "dubs", "stack", "stacks", "harmony", "harmonies", "harm", "stab", "stabs", "background", "choir" }))
        { role = Role::BackingVocal; return true; }
    if (has ({ "kick", "kik", "kicks" })) { role = Role::Kick; return true; }
    if (words.contains ("808") || has ({ "bass", "sub", "subs" })) { role = Role::Bass808; return true; }
    if (has ({ "drum", "drums", "snare", "snares", "hat", "hats", "hihat", "hihats", "hh", "clap", "claps", "perc", "percs", "percussion", "rim", "cymbal", "cymbals", "ride", "crash", "tom", "toms", "shaker", "tops" }))
        { role = Role::Drums; return true; }
    if (has ({ "melody", "melodies", "piano", "keys", "guitar", "gtr", "synth", "pad", "pads", "strings", "sample", "loop", "pluck", "organ", "rhodes", "flute", "brass", "bell", "bells", "arp", "violin", "sax" }))
        { role = Role::Melody; return true; }
    if (has ({ "fx", "sfx", "riser", "risers", "sweep", "impact", "noise", "ambience", "atmos", "foley" })) { role = Role::FX; return true; }
    // not "inst": that is what music software calls every new instrument track
    if (has ({ "beat", "beats", "instrumental", "prod", "bpm" }) || lower.contains ("2 track") || lower.contains ("2track") || lower.contains ("two track"))
        { role = Role::Beat; return true; }
    if (has ({ "vocal", "vocals", "vox", "voc", "lead", "rap", "raps", "verse", "verses", "hook", "hooks", "chorus", "sing", "singer", "singing", "voice", "autotune", "tune", "tuned", "acapella", "bridge", "main" }))
        { role = Role::Vocal; return true; }
    return false;
}
}
