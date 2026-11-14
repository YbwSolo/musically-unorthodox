#include "ModulePresets.h"

namespace mu
{
namespace
{
using V = std::vector<ParamValue>;

ModulePreset tune (const juce::String& name, const juce::String& blurb, float speed, float vibrato, float amount, float hold, float flex = 0.0f, float humanise = 0.0f, float slides = 0.0f,
                   float formant = 0.0f, bool robot = false)
{
    V v { { ids::tuneOn, 1 }, { ids::tuneMode, 0 }, { ids::tuneSpeed, speed }, { ids::tuneVibrato, vibrato }, { ids::tuneAmount, amount }, { ids::tuneHold, hold }, { ids::tuneFormant, formant },
          { ids::tuneFlex, flex }, { ids::tuneHumanise, humanise }, { ids::tuneSlides, slides } };
    if (robot) v.push_back ({ kRootOnly, 1 });
    return { "tune", name, blurb, v };
}
ModulePreset doubler (const juce::String& name, const juce::String& blurb, bool stack, float mix, float detune, float loose, float width)
{
    return { "doubler", name, blurb, { { ids::dblOn, 1 }, { ids::dblVoices, stack ? 1.0f : 0.0f }, { ids::dblMix, mix }, { ids::dblDetune, detune }, { ids::dblTiming, loose }, { ids::dblWidth, width } } };
}
// note: 0 = 1/2, 1 = 1/4, 2 = dotted 1/8, 3 = 1/8, 4 = 1/8 triplet, 5 = 1/16, 6 = 1 bar, 7 = dotted 1/4, 8 = 1/4 triplet, 9 = 1/16 triplet, 10 = 1/32;
// a negative note means "not timed", with ms giving the time. style: 0 digital, 1 tape, 2 analog, 3 diffused. pattern: 0 ping-pong, 1 straight, 2 dual
ModulePreset delay (const juce::String& name, const juce::String& blurb, int note, float ms, float mix, float repeats, float tone, float pingPong, float duck,
                    int style = 0, int pattern = 0, float drive = 30, float wobble = 0, float lowCut = 160)
{
    return { "delay", name, blurb, { { ids::dlOn, 1 }, { ids::dlSync, note >= 0 ? 1.0f : 0.0f }, { ids::dlNote, (float) juce::jmax (0, note) }, { ids::dlTime, ms }, { ids::dlMix, mix },
                                       { ids::dlFeedback, repeats }, { ids::dlTone, tone }, { ids::dlWidth, pingPong }, { ids::dlDuck, duck },
                                       { ids::dlStyle, (float) style }, { ids::dlPattern, (float) pattern }, { ids::dlDrive, drive }, { ids::dlWobble, wobble }, { ids::dlLowCut, lowCut } } };
}
// type: see Reverb::typeNames(). length: 0 = one beat, 1 = two, 2 = four. The rest are the finer controls, with the values a space is designed around as defaults.
struct Fine { float size = 100, diffusion = 80, early = 50, lowDecay = 100, movement = 30, rate = 0.6f, shimmer = 50; };
ModulePreset reverb (const juce::String& name, const juce::String& blurb, int type, int length, float mix, float tone, float duck, float lowCut = 180, float width = 100, Fine f = {})
{
    return { "reverb", name, blurb, { { ids::rvOn, 1 }, { ids::rvAuto, 1 }, { ids::rvType, (float) type }, { ids::rvLength, (float) length }, { ids::rvMix, mix }, { ids::rvTone, tone },
                                        { ids::rvDuck, duck }, { ids::rvLowCut, lowCut }, { ids::rvWidth, width }, { ids::rvFreeze, 0 },
                                        { ids::rvSize, f.size }, { ids::rvDiffusion, f.diffusion }, { ids::rvEarly, f.early }, { ids::rvLowDecay, f.lowDecay },
                                        { ids::rvModDepth, f.movement }, { ids::rvModRate, f.rate }, { ids::rvShimmer, f.shimmer } } };
}
ModulePreset harmony (const juce::String& name, const juce::String& blurb, int voicing, float mix, float width, float humanise = 40)
{
    return { "harmony", name, blurb, { { ids::hmOn, 1 }, { ids::hmMode, 0 }, { ids::hmVoicing, (float) voicing }, { ids::hmMix, mix }, { ids::hmWidth, width }, { ids::hmHumanise, humanise },
                                       { ids::hmLeading, 0 }, { ids::hmChoir, 0 } } };
}
// One voice of a hand-built harmony: steps of the scale from the lead, level, pan (-100..100), delay in ms, size (-100..100), detune in cents
struct Part { int steps; float level, pan, delay, size, detune; };
// mode: 1 = the voices keep their intervals, 2 = they sing the notes of the chord grid
ModulePreset voices (const juce::String& name, const juce::String& blurb, int mode, float mix, float width, float humanise, bool leading, float choir, std::vector<Part> parts)
{
    ModulePreset p { "harmony", name, blurb, { { ids::hmOn, 1 }, { ids::hmMode, (float) mode }, { ids::hmMix, mix }, { ids::hmWidth, width }, { ids::hmHumanise, humanise },
                                               { ids::hmLeading, leading ? 1.0f : 0.0f }, { ids::hmChoir, choir } } };
    for (int v = 0; v < 4; ++v)
    {
        const bool used = v < (int) parts.size();
        const Part part = used ? parts[(size_t) v] : Part { 2, 80, 0, 0, 0, 0 };
        p.values.push_back ({ ids::hmVoiceOn (v), used ? 1.0f : 0.0f });
        if (! used) continue;        // a voice that is switched off keeps whatever it was set to
        p.values.push_back ({ ids::hmVoiceSteps (v), (float) part.steps }); p.values.push_back ({ ids::hmVoiceLevel (v), part.level }); p.values.push_back ({ ids::hmVoicePan (v), part.pan });
        p.values.push_back ({ ids::hmVoiceDelay (v), part.delay }); p.values.push_back ({ ids::hmVoiceSize (v), part.size }); p.values.push_back ({ ids::hmVoiceDetune (v), part.detune });
    }
    return p;
}
// mode: 0 shift, 1 wide (two copies moved opposite ways, left and right), 2 robot (held on one note)
ModulePreset shift (const juce::String& name, const juce::String& blurb, float semis, float character, float mix, int mode = 0, float fine = 0, float drive = 0, float width = 100)
{
    return { "shift", name, blurb, { { ids::psOn, 1 }, { ids::psSemis, semis }, { ids::psCharacter, character }, { ids::psMix, mix },
                                     { ids::psMode, (float) mode }, { ids::psFine, fine }, { ids::psDrive, drive }, { ids::psWidth, width } } };
}
// mode: 0 small speaker, 1 telephone, 2 megaphone, 3 radio, 4 walkie-talkie
ModulePreset phone (const juce::String& name, const juce::String& blurb, float width, float drive, float amount = 100, int mode = 0, float noise = 0)
{
    return { "phone", name, blurb, { { ids::phOn, 1 }, { ids::phTone, width }, { ids::phDrive, drive }, { ids::phAmount, amount }, { ids::phMode, (float) mode }, { ids::phNoise, noise } } };
}
// noiseType: 0 vinyl, 1 tape
ModulePreset lofi (const juce::String& name, const juce::String& blurb, float crush, float wobble, float mix = 100,
                   float drive = 0, float noise = 0, int noiseType = 0, float wear = 0, float tone = 100)
{
    return { "lofi", name, blurb, { { ids::lfOn, 1 }, { ids::lfCrush, crush }, { ids::lfWobble, wobble }, { ids::lfMix, mix }, { ids::lfDrive, drive },
                                    { ids::lfNoise, noise }, { ids::lfNoiseType, (float) noiseType }, { ids::lfWear, wear }, { ids::lfTone, tone } } };
}
// type: 0 chorus, 1 flanger, 2 phaser, 3 filter, 4 pan, 5 volume.  shape: 0 sine, 1 triangle, 2 ramp up, 3 ramp down, 4 square, 5 drawn.
// note (when timed to the tempo): 0 = 4 bars, 1 = 2 bars, 2 = 1 bar, 3 = 1/2, 4 = 1/4, 5 = 1/8, 6 = 1/16.
ModulePreset motion (const juce::String& name, const juce::String& blurb, int type, float rate, float depth, float mix,
                     int shape = 0, int note = -1, float smooth = 100, std::vector<float> curve = {})
{
    ModulePreset p { "motion", name, blurb, { { ids::mdOn, 1 }, { ids::mdType, (float) type }, { ids::mdRate, rate }, { ids::mdDepth, depth }, { ids::mdMix, mix },
                                              { ids::mdShape, (float) shape }, { ids::mdSync, note >= 0 ? 1.0f : 0.0f }, { ids::mdNote, (float) (note >= 0 ? note : 2) }, { ids::mdSmooth, smooth } } };
    for (int i = 0; i < 16; ++i)       // every preset sets the whole curve, so nothing is left over from the one before
        p.values.push_back ({ ids::mdPoint (i), i < (int) curve.size() ? curve[(size_t) i] : std::round (1000.0f * std::pow ((float) i / 15.0f, 0.6f)) / 10.0f });
    return p;
}
// rate: 0 = 1/4, 1 = 1/8, 2 = 1/8 triplet, 3 = 1/16, 4 = 1/32.  With steps given it is a pattern; without, the plain pulse.
ModulePreset chop (const juce::String& name, const juce::String& blurb, int rate, float depth, float hardness,
                   std::vector<float> steps = {}, float gate = 100, float swing = 0)
{
    static const float standard[16] = { 100, 100, 0, 100, 100, 0, 100, 0, 100, 100, 0, 100, 100, 0, 100, 60 };
    ModulePreset p { "chop", name, blurb, { { ids::chOn, 1 }, { ids::chRate, (float) rate }, { ids::chDepth, depth }, { ids::chShape, hardness },
                                            { ids::chMode, steps.empty() ? 0.0f : 1.0f }, { ids::chLength, (float) (steps.empty() ? 16 : (int) steps.size()) },
                                            { ids::chGate, gate }, { ids::chSwing, swing } } };
    for (int i = 0; i < 16; ++i) p.values.push_back ({ ids::chStep (i), steps.empty() ? standard[i] : steps[(size_t) (i % (int) steps.size())] });
    return p;
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
        { "delay",   "Delay",   ids::dlOn,  "Echoes timed to the song: clean, tape, analog or diffused." },
        { "reverb",  "Reverb",  ids::rvOn,  "Nine spaces from five different engines." },
        { "harmony", "Harmony", ids::hmOn,  "Up to four extra voices: a ready-made voicing, set one by one, or singing chords." },
        { "shift",   "Shift",   ids::psOn,  "The voice moved up or down, spread wide as two copies, or held on one note." },
        { "phone",   "Phone",   ids::phOn,  "The voice through a telephone, a megaphone, a radio or a walkie-talkie, with its noise." },
        { "lofi",    "Lo-fi",   ids::lfOn,  "Old machines: drive, wobble, crush, wear, a dull top and noise." },
        { "motion",  "Motion",  ids::mdOn,  "Chorus, flanger, phaser, filter, pan or volume, moved by a wave or a curve you draw." },
        { "chop",    "Chop",    ids::chOn,  "Cuts the sound in time with the song: an even pulse or a sixteen-step pattern." } };
    return m;
}

const std::vector<ModulePreset>& modulePresets()
{
    static const std::vector<ModulePreset> p = {
        // ---- Tune                                                                                     speed vib amount hold flex human slides
        tune ("Natural vocal", "The centre of each note is corrected. Vibrato, slides and anything already close are left as they were sung.", 70, 85, 100, 35, 10, 50, 80),
        tune ("Transparent studio correction", "Only notes that are clearly out are touched, and only once the voice has settled on them. Meant to go unnoticed.", 90, 100, 100, 40, 18, 60, 100),
        tune ("Subtle pitch correction", "Part of the correction, slowly. For a singer who is nearly there.", 120, 100, 70, 30, 12, 50, 100),
        tune ("Smooth singing", "Polished and even, with the runs and slides still flowing.", 45, 70, 100, 40, 6, 40, 70),
        tune ("Modern R&B", "Quick enough to sound finished, with a little of the slide between notes kept.", 28, 55, 100, 45, 0, 25, 40),
        tune ("Melodic rap", "Close to hard tune, with a short glide between notes.", 10, 20, 100, 55, 0, 0, 15),
        tune ("Fast modern tune", "Notes pulled in almost at once: the tuned sound of current pop and trap, just short of the full effect.", 4, 10, 100, 50),
        tune ("Hard tune", "Every note snaps onto the scale. Set the key and Major or Minor first.", 0, 0, 100, 50),
        tune ("Hard tune, held notes", "Hard tune that holds each note firmly, so a wavering voice does not warble between two notes.", 0, 0, 100, 100),
        tune ("Rescue", "For a take that is well out of tune: fast, full correction, and each note held until the voice clearly moves on.", 12, 20, 100, 85),
        tune ("Deep and tuned", "Tight tuning with the voice made a little bigger and darker.", 20, 40, 100, 45, 0, 0, 20, -3),
        tune ("Bright and tuned", "Tight tuning with the voice made a little smaller and brighter.", 20, 40, 100, 45, 0, 0, 20, 2),
        tune ("Robotic vocal", "Only the key note is allowed, so the whole performance sits on one pitch.", 0, 0, 100, 0, 0, 0, 0, 0, true),

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
        delay ("Slap", "One quick echo, 95 ms behind. The classic way to give a dry vocal some depth.", -1, 95, 22, 0, 55, 0, 20, 0, 1),
        delay ("Tape slap", "The same single echo through tape: rounder, a little unsteady, straight down the middle.", -1, 110, 24, 0, 45, 0, 20, 1, 1, 45, 25),
        delay ("Doubling slap", "A very short echo that thickens the voice more than it repeats it.", -1, 38, 30, 0, 70, 0, 0, 0, 1),
        delay ("Eighth echo", "Clean echoes on every half beat, kept low under the words.", 3, 250, 18, 30, 45, 60, 55),
        delay ("Dotted eighth", "The off-beat echo that fills the gaps between words and gives a line bounce.", 2, 250, 20, 38, 45, 80, 55),
        delay ("Quarter throw", "A long echo on the beat that only speaks up in the gaps. Good on the last word of a line.", 1, 250, 24, 45, 40, 70, 78),
        delay ("Quarter and dotted eighth", "Two times at once: a quarter note on the left against a dotted eighth on the right.", 1, 250, 20, 35, 50, 100, 55, 0, 2),
        delay ("Ping-pong sixteenth", "Fast echoes bouncing left and right.", 5, 250, 16, 40, 55, 100, 50),
        delay ("Triplet bounce", "Echoes in threes against the beat, for a rolling feel.", 4, 250, 18, 35, 50, 90, 50),
        delay ("Tape echo", "Quarter-note repeats through tape: each one warmer and softer than the last, drifting slightly in pitch.", 1, 250, 22, 52, 40, 40, 55, 1, 0, 50, 35),
        delay ("Dark tape", "A worn tape echo. Dull, thick repeats that fall well behind the voice.", 1, 250, 22, 55, 15, 40, 55, 1, 0, 65, 45),
        delay ("Analog eighth", "The dark, smeared repeats of a delay pedal, on every half beat.", 3, 250, 22, 45, 55, 30, 50, 2, 0, 40, 15),
        delay ("Analog swirl", "A pedal pushed until the repeats nearly feed themselves, wobbling as they go.", 2, 250, 24, 72, 50, 50, 45, 2, 0, 60, 60),
        delay ("Half-note wash", "Long repeats, each more spread out than the last, that build into a wash behind a hook.", 0, 250, 20, 58, 35, 70, 62, 3),
        delay ("Diffused eighth", "Echoes that soften into a cloud after the second repeat.", 3, 250, 20, 50, 45, 80, 50, 3),
        delay ("Thirty-second flutter", "Very fast repeats that blur into a metallic flutter. An effect, not a mix tool.", 10, 250, 18, 60, 60, 100, 30),

        // ---- Reverb                                                                          type length mix tone duck lowCut width
        reverb ("Tight rap", "One beat of plate, ducked hard: rap verses that stay dry and upfront.", 1, 0, 11, 45, 70, 220),
        reverb ("Vocal plate", "Two beats of plate, the all-round setting for sung vocals.", 1, 1, 18, 60, 50),
        reverb ("Bright plate", "The plate with its top left on, for air around a dull vocal.", 1, 1, 17, 85, 55, 240),
        reverb ("Dark plate", "The same length with the top taken off, so it sits behind the voice.", 1, 1, 18, 25, 50),
        reverb ("Small room", "A short, close space. Makes a close-miked vocal sound less like a booth.", 0, 0, 14, 50, 20, 180, 100, { 70, 80, 60 }),
        reverb ("Large room", "A bigger live room with the walls further away.", 0, 1, 16, 50, 30, 180, 100, { 150, 80, 50 }),
        reverb ("Drum room", "A short room for drums and percussion; adds size without smearing the hits.", 0, 0, 16, 55, 0, 260, 100, { 100, 60, 65 }),
        reverb ("Studio chamber", "A hard-walled echo room: thicker than a room, tighter than a hall. Body for a rap vocal.", 3, 0, 15, 55, 55, 200),
        reverb ("Concert hall", "Four beats of hall for hooks, intros and slow songs.", 2, 2, 24, 45, 45),
        reverb ("Large hall", "The hall made bigger, with the lows kept short so the size does not turn into mud.", 2, 2, 26, 40, 45, 200, 100, { 150, 85, 45, 80 }),
        reverb ("Ballad hall", "A long, open hall with less ducking, so the voice swims in it.", 2, 2, 30, 50, 30),
        reverb ("Cathedral", "Huge, dark and slow. A little goes a long way.", 4, 2, 22, 35, 40, 220, 100, { 100, 85, 45, 110, 40 }),
        reverb ("Adlib space", "Wetter than a lead reverb, to push adlibs and backing vocals to the back.", 2, 1, 30, 45, 20, 230),
        reverb ("Ambient cloud", "No reflections, just a soft cloud that swells in behind the sound.", 5, 2, 34, 45, 20, 220, 100, { 110, 100, 50, 100, 55 }),
        reverb ("Ambient wash", "As much reverb as dry sound. For pads, effects and dreamy sections.", 5, 2, 45, 35, 0, 180, 100, { 130, 100, 50, 110, 60 }),
        reverb ("Vintage digital", "An early digital reverb: darker, with a tail that turns slowly.", 6, 1, 20, 50, 40, 200, 100, { 100, 70, 50, 100, 60, 0.9f }),
        reverb ("Shimmer halo", "A hall whose tail rises an octave into a glassy halo.", 7, 2, 24, 60, 35, 260, 100, { 110, 90, 30, 90, 40, 0.5f, 60 }),
        reverb ("Gated burst", "A dense burst that holds its level and stops dead. Big on snares, claps and one-word adlibs.", 8, 1, 26, 65, 0, 200, 100, { 100, 70 }),
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
        // voices set one by one                                                       mode mix width human lead choir   steps level pan delay size detune
        voices ("Third, voice-led", "A third above that holds its note when the tune moves a step and it still fits, the way a singer would.", 1, 40, 60, 40, true, 0, { { 2, 100, 40, 0, 0, 0 } }),
        voices ("Close trio", "A third and a fifth above either side, and a fourth below in the middle, moving like three singers.", 1, 42, 100, 45, true, 0,
                { { 2, 100, -60, 0, 0, 0 }, { 4, 85, 60, 6, 0, 0 }, { -3, 80, 0, 12, -10, 0 } }),
        voices ("Wide four-part", "Four voices spread across the stereo field: two above, one below, an octave on top kept quiet.", 1, 44, 100, 45, true, 0,
                { { 2, 100, -80, 0, 0, 0 }, { 4, 90, 80, 6, 0, 0 }, { -3, 85, -30, 12, -8, 0 }, { 7, 45, 30, 18, 10, 0 } }),
        voices ("Octaves either side", "The same line an octave down on the left and an octave up on the right.", 1, 40, 100, 30, false, 0, { { -7, 100, -70, 0, -15, 0 }, { 7, 60, 70, 8, 10, 0 } }),
        voices ("Low men", "Three bigger, darker voices under the lead: a fourth, a sixth and an octave below.", 1, 46, 80, 50, true, 40,
                { { -3, 100, -50, 0, -35, 0 }, { -5, 90, 50, 8, -40, 0 }, { -7, 80, 0, 14, -45, 0 } }),
        voices ("Choir", "Four parts, each one three singers: a real stack behind the lead.", 1, 50, 100, 60, true, 100,
                { { 2, 100, -70, 0, 0, 0 }, { 4, 95, 70, 5, 0, 0 }, { -3, 95, -25, 10, -12, 0 }, { 7, 55, 25, 15, 12, 0 } }),
        voices ("Whisper stack", "Two smaller, slightly out-of-tune voices that arrive late: airy and loose.", 1, 34, 100, 70, false, 30, { { 2, 100, -65, 20, 25, 8 }, { 4, 90, 65, 35, 30, -8 } }),
        voices ("Chord singers", "Three voices that sing the notes of the chord grid and move as little as they can from chord to chord. Draw the chords, or use Fill from key.", 2, 44, 100, 45, true, 0,
                { { 2, 100, -60, 0, 0, 0 }, { 4, 90, 60, 6, 0, 0 }, { -3, 85, 0, 12, -8, 0 } }),
        voices ("Chord choir", "The chord singers, three to a part.", 2, 50, 100, 60, true, 100,
                { { 2, 100, -70, 0, 0, 0 }, { 4, 95, 70, 5, 0, 0 }, { -3, 95, -25, 10, -12, 0 }, { 7, 55, 25, 15, 12, 0 } }),

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
        // wide: two copies moved opposite ways, one each side                          semis char mix mode fine drive width
        shift ("Wide thickener", "Two copies nine cents either way, left and right: the voice gets wider and thicker without sounding doubled.", 0, 0, 45, 1, 9, 0, 100),
        shift ("Super wide", "The same with fifteen cents and more of it: an obvious chorus-like spread.", 0, 0, 60, 1, 15, 0, 100),
        shift ("Octaves, left and right", "An octave down on one side and an octave up on the other, under the lead.", 12, 0, 35, 1, 0, 0, 100),
        // robot: held on one note
        shift ("Robot", "Everything on one note, the key note of the song: a talking machine.", 0, 0, 100, 2, 0, 20),
        shift ("Low robot", "The robot an octave down, bigger and with some grit.", -12, -20, 100, 2, 0, 45),
        shift ("Driven low layer", "An octave down with saturation, layered under the voice for weight that cuts through.", -12, -10, 35, 0, 0, 60),

        // ---- Phone
        phone ("Telephone", "A call on a land line: 300 to 3400 Hz, a little crunch, and the hiss of the line.", 40, 30, 100, 1, 25),
        phone ("Half phone", "The telephone mixed half and half with the clean voice.", 40, 30, 55, 1, 0),
        phone ("Bad line", "A poor connection: narrower, pushed harder, noisy.", 20, 55, 100, 1, 60),
        phone ("Megaphone", "Two horn resonances and a driver that clips, like shouting through a megaphone.", 50, 70, 100, 2, 10),
        phone ("Megaphone, flat out", "The megaphone with everything pushed: harsh and loud.", 30, 95, 100, 2, 20),
        phone ("Old radio", "A small radio: levelled so it breathes, with static crackling behind it.", 40, 45, 100, 3, 45),
        phone ("Radio, far away", "A weak station: thin, fading in and out, half lost in the static.", 20, 55, 100, 3, 80),
        phone ("Walkie-talkie", "Narrow and clipped hard, with a burst of noise as each phrase ends.", 40, 80, 100, 4, 50),
        phone ("Small speaker", "The plain small-speaker sound: a band-pass and soft overdrive, no noise.", 40, 35),
        phone ("Laptop speaker", "Just the bass and the top missing, hardly any distortion.", 85, 10),

        // ---- Lo-fi                                                                  crush wobble mix drive noise type wear tone
        lofi ("Worn tape", "A tape played too many times: the pitch drifts, the level sags and drops out, the top is gone.", 10, 55, 100, 30, 30, 1, 45, 60),
        lofi ("Cassette", "Warm saturation, a little wow, a soft top and tape hiss.", 5, 35, 100, 40, 35, 1, 25, 55),
        lofi ("Dusty record", "Crackle and rumble under a slightly dulled sound.", 15, 20, 100, 15, 55, 0, 10, 70),
        lofi ("Old sampler", "The crunch of an early sampler, steady in pitch.", 55, 0, 100, 20, 0, 0, 0, 80),
        lofi ("Crushed", "Heavy digital grit.", 85, 0),
        lofi ("Seasick", "As much pitch drift as it will give, with the tone left alone.", 10, 100),
        lofi ("Dropouts", "The sound keeps falling away for a moment, like a damaged tape.", 0, 25, 100, 10, 15, 1, 90, 75),
        lofi ("Warm drive", "Only the saturation: thicker and a touch darker, steady and clean of noise.", 0, 0, 100, 55, 0, 0, 0, 85),
        lofi ("Subtle grit", "A little crunch blended under the clean sound.", 35, 10, 40),

        // ---- Motion                                                                  type rate depth mix shape note smooth curve
        motion ("Soft chorus", "Gentle width and shimmer.", 0, 0.5f, 40, 40),
        motion ("Wide chorus", "A deeper, more obvious chorus.", 0, 0.8f, 70, 60),
        motion ("Vibrato shimmer", "Fast, shallow movement: a nervous shimmer.", 0, 5.0f, 20, 100),
        motion ("Slow flange", "A slow sweep through the tone.", 1, 0.2f, 60, 50),
        motion ("Jet", "The full jet-plane whoosh.", 1, 0.1f, 100, 80),
        motion ("Swirl", "A slow phaser swirl.", 2, 0.4f, 60, 60),
        motion ("Fast phaser", "A quick, watery phaser.", 2, 2.5f, 40, 50),
        // movement timed to the song, and movement you draw
        motion ("Sidechain pump", "The level ducks on every beat and swells back, like a kick pumping a compressor.", 5, 0.6f, 70, 100, 2, 4),
        motion ("Drawn pump", "The same idea with the curve drawn by hand: change the points to change the groove.", 5, 0.6f, 85, 100, 5, 3, 100),
        motion ("Tremolo", "A steady, fairly fast rise and fall in level.", 5, 5.5f, 50, 100),
        motion ("Slow filter sweep", "The top end closes and opens over two bars.", 3, 0.6f, 70, 100, 1, 1),
        motion ("Filter steps", "The filter jumps between sixteen settings in a bar: a sample-and-hold wah.", 3, 0.6f, 85, 100, 5, 2, 0,
                { 90, 30, 60, 10, 75, 45, 100, 20, 55, 85, 15, 70, 35, 95, 5, 65 }),
        motion ("Auto-pan", "The sound drifts from side to side once a bar.", 4, 0.6f, 80, 100, 0, 2),
        motion ("Ping-pong pan", "Hard left, hard right, changing every beat.", 4, 0.6f, 100, 100, 4, 3),

        // ---- Chop
        chop ("Sixteenth gate", "Hard chops four to the beat: the trance-gate sound.", 3, 100, 80),
        chop ("Eighth pulse", "Two chops to the beat.", 1, 100, 70),
        chop ("Triplet chop", "Chops in threes for a rolling feel.", 2, 100, 75),
        chop ("Stutter", "Very fast chops: a machine-gun stutter.", 4, 100, 90),
        chop ("Soft tremolo", "A gentle rise and fall instead of a hard cut.", 3, 50, 0),
        chop ("Quarter pump", "The level dips between beats, like a pumping sidechain.", 0, 70, 15),
        // patterns: sixteen steps, each with its own level
        chop ("Trance gate", "A sixteen-step pattern, the rhythm trance pads are chopped into.", 3, 100, 85, { 100, 100, 0, 100, 100, 0, 100, 0, 100, 100, 0, 100, 100, 0, 100, 60 }),
        chop ("Offbeat", "Silent on the beat, open between: the sound jumps to the offbeats.", 1, 100, 80, { 0, 100, 0, 100, 0, 100, 0, 100 }),
        chop ("Dotted rhythm", "Three, three, two: a syncopated push.", 3, 100, 85, { 100, 0, 0, 100, 0, 0, 100, 0 }, 80),
        chop ("Stutter build", "One hit, then two, then a run: a bar that builds.", 3, 100, 90, { 100, 0, 0, 0, 100, 0, 100, 0, 100, 100, 0, 100, 100, 100, 100, 100 }, 60),
        chop ("Swung gate", "Loud and soft steps with every second one late: a shuffle.", 3, 100, 70, { 100, 50, 100, 50, 100, 50, 100, 50 }, 70, 55),
        chop ("Rising steps", "Each step louder than the last over two beats, then again.", 3, 100, 60, { 10, 25, 40, 55, 70, 80, 90, 100 }),
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
          join ({ v (tune ("", "", 0, 0, 100, 0, 0, 0, 0, 0, true)), v (motion ("", "", 0, 0.5f, 40, 40)), v (delay ("", "", 3, 250, 16, 30, 50, 80, 50)) }) },
        { "Lo-fi memory", "Worn tape and a dark plate, for a vocal that should sound like an old recording.",
          join ({ v (lofi ("", "", 28, 45)), v (reverb ("", "", 1, 1, 20, 22, 40)) }) },
        { "Trance chop", "Sixteenth-note gate into a dotted echo and a hall, so the chops ring on.",
          join ({ v (chop ("", "", 3, 100, 80)), v (delay ("", "", 2, 250, 22, 40, 45, 80, 30)), v (reverb ("", "", 2, 1, 20, 45, 20)) }) },
        { "Underwater", "Slow phaser, drifting pitch and a dark hall.",
          join ({ v (motion ("", "", 2, 0.25f, 70, 70)), v (lofi ("", "", 20, 60)), v (reverb ("", "", 2, 2, 30, 20, 20)) }) },
        { "Wide adlibs", "Wide doubles, ping-pong echoes and a wet hall, to throw adlibs out to the sides and back.",
          join ({ v (doubler ("", "", false, 55, 12, 55, 100)), v (delay ("", "", 3, 250, 24, 35, 50, 100, 40)), v (reverb ("", "", 2, 1, 30, 45, 20, 230)) }) },
        { "Intimate ballad", "Natural tuning and a long open hall, nothing else.",
          join ({ v (tune ("", "", 70, 85, 100, 35, 10, 50, 80)), v (reverb ("", "", 2, 2, 28, 50, 32)) }) },
    };
    return s;
}
}
