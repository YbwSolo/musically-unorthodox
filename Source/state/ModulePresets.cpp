#include "ModulePresets.h"

namespace mu
{
namespace
{
using V = std::vector<ParamValue>;

ModulePreset tune (const juce::String& name, const juce::String& blurb, float speed, float vibrato, float amount, float hold, float formant = 0.0f, bool robot = false)
{
    V v { { ids::tuneOn, 1 }, { ids::tuneMode, 0 }, { ids::tuneSpeed, speed }, { ids::tuneVibrato, vibrato }, { ids::tuneAmount, amount }, { ids::tuneHold, hold }, { ids::tuneFormant, formant } };
    if (robot) v.push_back ({ kRootOnly, 1 });
    return { "tune", name, blurb, v };
}
ModulePreset doubler (const juce::String& name, const juce::String& blurb, bool stack, float mix, float detune, float loose, float width)
{
    return { "doubler", name, blurb, { { ids::dblOn, 1 }, { ids::dblVoices, stack ? 1.0f : 0.0f }, { ids::dblMix, mix }, { ids::dblDetune, detune }, { ids::dblTiming, loose }, { ids::dblWidth, width } } };
}
// note: 0 = 1/2, 1 = 1/4, 2 = dotted 1/8, 3 = 1/8, 4 = 1/8 triplet, 5 = 1/16; a negative note means "not timed", with ms giving the time
ModulePreset delay (const juce::String& name, const juce::String& blurb, int note, float ms, float mix, float repeats, float tone, float pingPong, float duck)
{
    return { "delay", name, blurb, { { ids::dlOn, 1 }, { ids::dlSync, note >= 0 ? 1.0f : 0.0f }, { ids::dlNote, (float) juce::jmax (0, note) }, { ids::dlTime, ms }, { ids::dlMix, mix },
                                       { ids::dlFeedback, repeats }, { ids::dlTone, tone }, { ids::dlWidth, pingPong }, { ids::dlDuck, duck } } };
}
// type: 0 room, 1 plate, 2 hall. length: 0 = one beat, 1 = two, 2 = four
ModulePreset reverb (const juce::String& name, const juce::String& blurb, int type, int length, float mix, float tone, float duck, float lowCut = 180, float width = 100)
{
    return { "reverb", name, blurb, { { ids::rvOn, 1 }, { ids::rvAuto, 1 }, { ids::rvType, (float) type }, { ids::rvLength, (float) length }, { ids::rvMix, mix }, { ids::rvTone, tone },
                                        { ids::rvDuck, duck }, { ids::rvLowCut, lowCut }, { ids::rvWidth, width } } };
}
ModulePreset harmony (const juce::String& name, const juce::String& blurb, int voicing, float mix, float width)
{
    return { "harmony", name, blurb, { { ids::hmOn, 1 }, { ids::hmVoicing, (float) voicing }, { ids::hmMix, mix }, { ids::hmWidth, width } } };
}
ModulePreset shift (const juce::String& name, const juce::String& blurb, float semis, float character, float mix)
{
    return { "shift", name, blurb, { { ids::psOn, 1 }, { ids::psSemis, semis }, { ids::psCharacter, character }, { ids::psMix, mix } } };
}
ModulePreset phone (const juce::String& name, const juce::String& blurb, float width, float drive, float amount = 100)
{
    return { "phone", name, blurb, { { ids::phOn, 1 }, { ids::phTone, width }, { ids::phDrive, drive }, { ids::phAmount, amount } } };
}
ModulePreset lofi (const juce::String& name, const juce::String& blurb, float crush, float wobble, float mix = 100)
{
    return { "lofi", name, blurb, { { ids::lfOn, 1 }, { ids::lfCrush, crush }, { ids::lfWobble, wobble }, { ids::lfMix, mix } } };
}
ModulePreset motion (const juce::String& name, const juce::String& blurb, int type, float rate, float depth, float mix)
{
    return { "motion", name, blurb, { { ids::mdOn, 1 }, { ids::mdType, (float) type }, { ids::mdRate, rate }, { ids::mdDepth, depth }, { ids::mdMix, mix } } };
}
// rate: 0 = 1/4, 1 = 1/8, 2 = 1/8 triplet, 3 = 1/16, 4 = 1/32
ModulePreset chop (const juce::String& name, const juce::String& blurb, int rate, float depth, float hardness)
{
    return { "chop", name, blurb, { { ids::chOn, 1 }, { ids::chRate, (float) rate }, { ids::chDepth, depth }, { ids::chShape, hardness } } };
}

V join (std::initializer_list<V> parts)
{
    V out;
    for (auto& p : parts) out.insert (out.end(), p.begin(), p.end());
    return out;
}
}

const FxModule& tuneModule()
{
    static const FxModule m { "tune", "Tune", ids::tuneOn, "Pitch correction for one voice." };
    return m;
}

const std::vector<FxModule>& fxModules()
{
    static const std::vector<FxModule> m = {
        { "doubler", "Doubler", ids::dblOn, "One take made to sound like two or four." },
        { "delay",   "Delay",   ids::dlOn,  "Echoes timed to the song's tempo." },
        { "reverb",  "Reverb",  ids::rvOn,  "Room, plate or hall, timed to the song's tempo." },
        { "harmony", "Harmony", ids::hmOn,  "Extra voices singing along on other notes of the key." },
        { "shift",   "Shift",   ids::psOn,  "The whole voice moved up or down, as a layer or on its own." },
        { "phone",   "Phone",   ids::phOn,  "The voice through a small speaker: telephone, megaphone, radio." },
        { "lofi",    "Lo-fi",   ids::lfOn,  "Grit and wobble from old samplers and worn tape." },
        { "motion",  "Motion",  ids::mdOn,  "Chorus, flanger and phaser: width, whoosh and swirl." },
        { "chop",    "Chop",    ids::chOn,  "Cuts the sound in and out in time with the song." } };
    return m;
}

const std::vector<ModulePreset>& modulePresets()
{
    static const std::vector<ModulePreset> p = {
        // ---- Tune
        tune ("Natural", "The centre of each note is corrected; vibrato and slides stay as they were sung.", 70, 85, 100, 30),
        tune ("Gentle touch-up", "Only part of the correction, slowly. For a singer who is nearly there.", 120, 100, 70, 20),
        tune ("R&B smooth", "Quick enough to sound polished, slow enough that runs still flow.", 35, 60, 100, 45),
        tune ("Tight pop", "Notes pulled in fast for a modern, finished vocal that still moves like a voice.", 18, 35, 100, 35),
        tune ("Melodic rap", "Close to hard tune but with a little glide between notes.", 8, 15, 100, 55),
        tune ("Hard tune", "Every note snaps onto the scale. Set the key and Major or Minor first.", 0, 0, 100, 50),
        tune ("Hard tune, held notes", "Hard tune that holds each note firmly, so a wavering voice does not warble between two notes.", 0, 0, 100, 100),
        tune ("Rescue", "For a take that is well out of tune: fast, full correction, and each note held until the voice clearly moves on.", 12, 20, 100, 85),
        tune ("Deep and tuned", "Tight tuning with the voice made a little bigger and darker.", 20, 40, 100, 45, -3),
        tune ("Bright and tuned", "Tight tuning with the voice made a little smaller and brighter.", 20, 40, 100, 45, 2),
        tune ("Robot", "Only the key note is allowed, so the whole performance sits on one pitch.", 0, 0, 100, 0, 0, true),

        // ---- Doubler
        doubler ("Subtle", "A tight copy each side that thickens the lead without being heard as an effect.", false, 30, 6, 25, 100),
        doubler ("Tight double", "Like a singer who nailed the double first take.", false, 42, 7, 15, 100),
        doubler ("Wide", "Looser, more detuned copies hard left and right.", false, 48, 12, 50, 100),
        doubler ("Loose double", "A relaxed, human double with the timing left untidy on purpose.", false, 45, 10, 85, 100),
        doubler ("Hook stack", "Four extra voices, two each side, so one take sounds like a stacked hook.", true, 60, 11, 60, 100),
        doubler ("Thick stack", "Four voices kept tight and close for weight rather than width.", true, 55, 8, 30, 60),
        doubler ("Gang vocals", "Four loose, well-spread voices: a group shouting the line.", true, 70, 16, 90, 100),
        doubler ("Shimmer", "Heavily detuned copies that start to sound like a chorus effect.", false, 50, 20, 30, 100),
        doubler ("Centre thickener", "Copies kept in the middle. Works on a mono track and keeps the sides clear.", false, 35, 7, 30, 0),

        // ---- Delay
        delay ("Slap", "One quick echo, 95 ms behind. The classic way to give a dry vocal some depth.", -1, 95, 22, 0, 55, 0, 20),
        delay ("Doubling slap", "A very short echo that thickens the voice more than it repeats it.", -1, 38, 30, 0, 70, 0, 0),
        delay ("Eighth echo", "Echoes on every half beat, kept low under the words.", 3, 250, 18, 30, 45, 60, 55),
        delay ("Dotted eighth", "The off-beat echo that fills the gaps between words and gives a line bounce.", 2, 250, 20, 38, 45, 80, 55),
        delay ("Quarter throw", "A long echo on the beat that only speaks up in the gaps. Good on the last word of a line.", 1, 250, 24, 45, 40, 70, 78),
        delay ("Ping-pong sixteenth", "Fast echoes bouncing left and right.", 5, 250, 16, 40, 55, 100, 50),
        delay ("Triplet bounce", "Echoes in threes against the beat, for a rolling feel.", 4, 250, 18, 35, 50, 90, 50),
        delay ("Dark tape", "Each repeat is duller than the last, the way a tape echo wears the sound down.", 1, 250, 22, 52, 12, 40, 55),
        delay ("Half-note wash", "Long, slow repeats that build into a wash behind a hook.", 0, 250, 20, 58, 28, 70, 62),

        // ---- Reverb
        reverb ("Tight rap", "One beat of plate, ducked hard: rap verses that stay dry and upfront.", 1, 0, 11, 45, 70, 220),
        reverb ("Vocal plate", "Two beats of bright plate, the all-round setting for sung vocals.", 1, 1, 18, 60, 50),
        reverb ("Dark plate", "The same length with the top taken off, so it sits behind the voice.", 1, 1, 18, 25, 50),
        reverb ("Small room", "A short, natural space. Makes a close-miked vocal sound less like a booth.", 0, 0, 14, 50, 20),
        reverb ("Drum room", "A short room for drums and percussion; adds size without smearing the hits.", 0, 0, 16, 55, 0, 260),
        reverb ("Big hall", "Four beats of darker hall for hooks, intros and slow songs.", 2, 2, 26, 40, 45),
        reverb ("Ballad hall", "A long, open hall with less ducking, so the voice swims in it.", 2, 2, 30, 50, 30),
        reverb ("Adlib space", "Wetter than a lead reverb, to push adlibs and backing vocals to the back.", 2, 1, 30, 45, 20, 230),
        reverb ("Ambient wash", "As much reverb as dry sound. For pads, effects and dreamy sections.", 2, 2, 45, 35, 0),
        reverb ("Centre room", "A short room kept in the middle, for a track that should stay narrow.", 0, 0, 15, 50, 20, 180, 0),

        // ---- Harmony
        harmony ("Third above", "One voice a third higher: the first harmony most singers reach for.", 0, 40, 60),
        harmony ("Third below", "One voice a third lower, warmer and less obvious than the third above.", 5, 40, 60),
        harmony ("Fifth above", "One voice a fifth higher: open and strong.", 1, 35, 60),
        harmony ("Third and fifth", "Two voices that turn the lead into a full chord.", 2, 42, 100),
        harmony ("Low octave", "The same line an octave down, underneath the lead for weight.", 4, 45, 0),
        harmony ("High octave", "The same line an octave up, for a light shine on top.", 3, 28, 40),
        harmony ("Choir", "A third, a fifth and a low octave together.", 6, 50, 100),
        harmony ("Quiet bed", "A chord of voices tucked far under the lead: felt more than heard.", 2, 22, 100),
        harmony ("Big hook", "The full choir pushed up for a hook that should sound huge.", 6, 66, 100),

        // ---- Shift
        shift ("Demon layer", "An octave down and darker, layered under the real voice.", -12, -25, 40),
        shift ("Low octave double", "A natural-sounding octave below, kept quiet for weight.", -12, 0, 28),
        shift ("Deep voice", "The whole voice four semitones down and a little bigger.", -4, -20, 100),
        shift ("Slowed down", "Five semitones down with a much bigger voice: the sound of a slowed record.", -5, -45, 100),
        shift ("High octave layer", "An octave up, kept quiet on top of the real voice.", 12, 0, 25),
        shift ("Sped up", "An octave up with a small voice: the sound of a sped-up record.", 12, 60, 100),
        shift ("Helium", "Same notes, much smaller voice.", 0, 50, 100),
        shift ("Giant", "Same notes, much bigger voice.", 0, -60, 100),
        shift ("Fifth layer", "A copy a fifth higher under the lead, a hollow, eerie double.", 7, 0, 30),

        // ---- Phone
        phone ("Telephone", "Narrow and boxy with a little crunch, like a call.", 25, 30),
        phone ("Half phone", "The phone sound mixed half and half with the clean voice.", 35, 30, 55),
        phone ("Megaphone", "Wider and pushed hard, like shouting through a megaphone.", 55, 75),
        phone ("Old radio", "A small radio speaker: thin, a bit distorted.", 40, 50),
        phone ("Walkie-talkie", "As narrow as it goes and heavily overdriven.", 10, 90),
        phone ("Laptop speaker", "Just the bass and the top missing, hardly any distortion.", 85, 10),

        // ---- Lo-fi
        lofi ("Worn tape", "Pitch that drifts and a soft top, like a tape played too many times.", 15, 55),
        lofi ("Dusty", "A little grit and a little drift.", 30, 30),
        lofi ("Old sampler", "The crunch of an early sampler, steady in pitch.", 55, 0),
        lofi ("Crushed", "Heavy digital grit.", 85, 0),
        lofi ("Seasick", "As much pitch drift as it will give, with the tone left alone.", 10, 100),
        lofi ("Subtle grit", "A little crunch blended under the clean sound.", 35, 10, 40),

        // ---- Motion
        motion ("Soft chorus", "Gentle width and shimmer.", 0, 0.5f, 40, 40),
        motion ("Wide chorus", "A deeper, more obvious chorus.", 0, 0.8f, 70, 60),
        motion ("Vibrato shimmer", "Fast, shallow movement: a nervous shimmer.", 0, 5.0f, 20, 100),
        motion ("Slow flange", "A slow sweep through the tone.", 1, 0.2f, 60, 50),
        motion ("Jet", "The full jet-plane whoosh.", 1, 0.1f, 100, 80),
        motion ("Swirl", "A slow phaser swirl.", 2, 0.4f, 60, 60),
        motion ("Fast phaser", "A quick, watery phaser.", 2, 2.5f, 40, 50),

        // ---- Chop
        chop ("Sixteenth gate", "Hard chops four to the beat: the trance-gate sound.", 3, 100, 80),
        chop ("Eighth pulse", "Two chops to the beat.", 1, 100, 70),
        chop ("Triplet chop", "Chops in threes for a rolling feel.", 2, 100, 75),
        chop ("Stutter", "Very fast chops: a machine-gun stutter.", 4, 100, 90),
        chop ("Soft tremolo", "A gentle rise and fall instead of a hard cut.", 3, 50, 0),
        chop ("Quarter pump", "The level dips between beats, like a pumping sidechain.", 0, 70, 15),
    };
    return p;
}

std::vector<int> modulePresetsFor (const juce::String& module)
{
    std::vector<int> out;
    const auto& all = modulePresets();
    for (size_t i = 0; i < all.size(); ++i) if (all[i].module == module) out.push_back ((int) i);
    return out;
}

const std::vector<SoundPreset>& soundPresets()
{
    auto v = [] (const ModulePreset& p) { return p.values; };
    static const std::vector<SoundPreset> s = {
        { "Melodic trap lead", "Hard tune, a tight double, a dark echo that only speaks in the gaps and a low plate. The heavily tuned, moody lead of modern melodic rap.",
          join ({ v (tune ("", "", 0, 0, 100, 50)), v (doubler ("", "", false, 28, 6, 25, 100)), v (delay ("", "", 1, 250, 15, 42, 22, 70, 78)), v (reverb ("", "", 1, 1, 15, 30, 55, 200)) }) },
        { "Clean rap verse", "Almost dry: a short ducked plate and a quiet eighth-note echo. Clear words, a little depth.",
          join ({ v (reverb ("", "", 1, 0, 11, 45, 70, 220)), v (delay ("", "", 3, 250, 8, 20, 45, 50, 80)) }) },
        { "Stacked hook", "Four doubles and a quiet chord of harmonies under the lead, with a bright plate. One take, a whole hook.",
          join ({ v (doubler ("", "", true, 58, 11, 60, 100)), v (harmony ("", "", 2, 24, 100)), v (reverb ("", "", 1, 1, 20, 60, 45)) }) },
        { "Choir of one", "A third, a fifth and a low octave, wide doubles and a long hall.",
          join ({ v (harmony ("", "", 6, 55, 100)), v (doubler ("", "", false, 45, 12, 50, 100)), v (reverb ("", "", 2, 2, 28, 45, 35)) }) },
        { "Demon double", "The voice an octave down and darker, layered under itself, with a dark plate.",
          join ({ v (shift ("", "", -12, -25, 38)), v (reverb ("", "", 1, 1, 16, 25, 50)) }) },
        { "Slowed outro", "Pitched down with a bigger voice, long hall and slow echoes: the slowed-and-reverbed sound.",
          join ({ v (shift ("", "", -5, -45, 100)), v (reverb ("", "", 2, 2, 32, 40, 20)), v (delay ("", "", 0, 250, 14, 45, 25, 60, 50)) }) },
        { "Phone verse", "The whole voice through a telephone with a touch of room. A classic way to set up a drop.",
          join ({ v (phone ("", "", 25, 30)), v (reverb ("", "", 0, 0, 8, 50, 20)) }) },
        { "Radio intro", "Small radio speaker with dust and drift.",
          join ({ v (phone ("", "", 40, 50)), v (lofi ("", "", 25, 35)) }) },
        { "Robot hook", "One note only, with chorus and an eighth-note echo.",
          join ({ v (tune ("", "", 0, 0, 100, 0, 0, true)), v (motion ("", "", 0, 0.5f, 40, 40)), v (delay ("", "", 3, 250, 16, 30, 50, 80, 50)) }) },
        { "Lo-fi memory", "Worn tape and a dark plate, for a vocal that should sound like an old recording.",
          join ({ v (lofi ("", "", 28, 45)), v (reverb ("", "", 1, 1, 20, 22, 40)) }) },
        { "Trance chop", "Sixteenth-note gate into a dotted echo and a hall, so the chops ring on.",
          join ({ v (chop ("", "", 3, 100, 80)), v (delay ("", "", 2, 250, 22, 40, 45, 80, 30)), v (reverb ("", "", 2, 1, 20, 45, 20)) }) },
        { "Underwater", "Slow phaser, drifting pitch and a dark hall.",
          join ({ v (motion ("", "", 2, 0.25f, 70, 70)), v (lofi ("", "", 20, 60)), v (reverb ("", "", 2, 2, 30, 20, 20)) }) },
        { "Wide adlibs", "Wide doubles, ping-pong echoes and a wet hall, to throw adlibs out to the sides and back.",
          join ({ v (doubler ("", "", false, 55, 12, 55, 100)), v (delay ("", "", 3, 250, 24, 35, 50, 100, 40)), v (reverb ("", "", 2, 1, 30, 45, 20, 230)) }) },
        { "Intimate ballad", "Natural tuning and a long open hall, nothing else.",
          join ({ v (tune ("", "", 70, 85, 100, 30)), v (reverb ("", "", 2, 2, 28, 50, 32)) }) },
    };
    return s;
}
}
