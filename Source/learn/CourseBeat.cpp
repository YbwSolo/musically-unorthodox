#include "Course.h"

// Making a beat from nothing, in the order it is actually done: plan, melody, chords, drums, 808, layers,
// arrangement, mix, effects, export. The level figures in the mixing lessons are measured, not guessed: they
// come from the stems of finished records in this style (see README, "measured against the records' stems").
namespace mu::course
{
using S = Scene;

std::vector<Lesson> beatLessons()
{
    return {
    { "bt.plan", "Start", "Plan the beat", {
        { "Decide three things first",
          "Before you touch a sound, decide the style, the tempo and the key. The style tells you the tempo range and the kind of drums. The tempo goes into the project first, because everything you record follows it. The key is the family of notes the melody, chords and 808 will all share.", S::Card, 8 },
        { "Use a reference",
          "Pick one finished song that has the feeling you want. Count its tempo, the way the Theory lessons showed. Listen to what it contains: how many melodies, how busy the drums are, where the 808 plays. You are not copying it. You are learning how much, or how little, a finished beat needs.", S::Pulse, 4 },
        { "The order of work",
          "There is no law, but this order works for most people: tempo and key, the main melody, chords or a pad under it, drums, the 808, extra layers, then arrange it into a song and mix it. Click each stage. If you get stuck on the melody, start with the drums instead and come back.", S::Chain, 2 },
        { "Finish a loop before you perfect it",
          "Get a rough four or eight bar loop with every part in it before you polish anything. A simple loop that is finished teaches you more than a perfect melody with no drums. Aim to finish one small beat every session. Speed comes from repetition, and so does taste.", S::Song, 0 } },
      nullptr, kTabNone, nullptr },

    { "bt.melody", "Melody", "Write the main melody", {
        { "Pick the sound first",
          "The same notes feel completely different on a piano, a bell, a flute or a dark pad. Choose a sound that already has the mood, then write for it. Short sounds like plucks and bells suit busy rhythms. Long sounds like pads and strings suit slow, held notes.", S::Roll, 0 },
        { "Stay in the key",
          "Choose a minor key and use only its seven notes, or only the five pentatonic notes while you are starting out. Both FL Studio and Logic can highlight the scale in the piano roll so the wrong notes are marked. The DAW lessons show where that switch is.", S::Keys, 6 },
        { "Write one bar, then repeat it",
          "Write a motif of three to five notes. Copy it to the next bar. Change the ending of the copy. You now have a two bar phrase. Copy that phrase and change one thing again, and you have four bars. Same, same, different is how nearly every melody you know was made.", S::Roll, 2 },
        { "Give it a rhythm that leaves gaps",
          "Do not put a note on every step. Start some notes off the beat, hold one, leave a rest. If the melody would still make sense clapped on a table, the rhythm is good. The gaps are where the drums and the vocal will speak.", S::Roll, 7 },
        { "Your turn: sketch a melody",
          "Every row here is a note of C minor pentatonic, so nothing you click can clash. Drums and an 808 are already playing. Click to add notes, click again to remove them. Try three or four notes in the first half, then answer them in the second half.", S::Roll, 14 } },
      nullptr, kTabNone, nullptr },

    { "bt.chords", "Melody", "Chords, pads and a second melody", {
        { "Put chords under the melody",
          "Find the chords of your key and try them under the melody one at a time. The chord that contains the melody's long notes will sound right. One chord per bar, or one every two bars, is plenty. Play them on a soft, wide sound and keep them quieter than the melody.", S::Roll, 4 },
        { "Or start from the chords",
          "Many producers work the other way: choose a progression first, then find a melody from the chord notes. Play the chord, then pick out its notes one at a time and add a scale note between them. The melody cannot clash, because it comes from the chord.", S::Roll, 10 },
        { "Add a counter melody",
          "Once the main melody works, add a second sound that answers it in the gaps. Use a different kind of sound in a different octave: if the main melody is a mid-range piano, the counter can be a high bell or a low, soft flute. Two melodies in the same octave on similar sounds blur into one.", S::Roll, 8 },
        { "Layer for size",
          "To make a melody bigger, copy it to a second sound: the same notes on a different instrument, or the same instrument an octave up. Each layer should add something the other lacks: one has the attack, one has the body, one has the air. Three layers that all do the same job just get louder and muddier.", S::Roll, 12 } },
      nullptr, kTabNone, nullptr },

    { "bt.drums", "Drums", "Kick, snare and hi-hats", {
        { "Three jobs",
          "A drum pattern has three jobs. The kick is the low thump that drives the beat forward. The snare or clap is the crack that answers it and marks time. The hi-hats are the fast ticking on top that sets the speed and energy. Click any step to hear each sound, and to change the pattern.", S::Grid, 2 },
        { "The snare marks time",
          "Put the snare down first. On beats two and four it gives the straight, head-nodding feel of boom bap and R&B. On beat three only, it gives the half-time feel of trap. Everything else is built around where the snare lands.", S::Grid, 2 },
        { "The kick makes it move",
          "Start with a kick on the one, so the bar begins with weight. Then add one or two more, and try them off the beat: just before a snare, or on the \"and\" between beats. Fewer kicks hit harder. If the pattern feels stiff, move one kick a single step and listen.", S::Grid, 3 },
        { "Hi-hats set the energy",
          "Eighth-note hats, every second step, are calm. Sixteenth-note hats, every step, are busy. Start with eighths. An open hat on an off-beat just before the snare adds lift. Hats should be much quieter than the kick and snare: they are felt more than heard.", S::Grid, 6 },
        { "Your turn: build a pattern",
          "The grid is empty. Put a snare on beats two and four, a kick on the one, and at least four hi-hats. When it is a working beat, the plugin tells you. Then keep going: add kicks off the beat until it bounces.", S::Grid, 9 } },
      nullptr, kTabNone, nullptr },

    { "bt.styles", "Drums", "Drum patterns by style", {
        { "Boom bap",
          "85 to 95 BPM. Snare on two and four, a kick on the one and a couple of kicks off the beat, eighth-note hats, and swing. The swing is the style: without it the same pattern sounds like a drum machine demo. Change a kick and hear how much the feel depends on one step.", S::Grid, 3 },
        { "Trap",
          "Set the project to 130 to 150 BPM. The snare or clap lands on beat three of every bar, so it feels half as fast. Kicks are sparse and follow the 808. The hi-hats run in eighths and break into fast rolls. This grid shows two bars.", S::Grid, 4 },
        { "Drill",
          "Around 140 to 144 BPM. The hi-hats play a lopsided three, three, two pattern instead of straight eighths, and that skip is the sound of drill. The snare lands on beat three of the first bar and a beat later, on beat four, in the second bar. Kicks are sparse and the sliding 808 does the moving.", S::Grid, 5 },
        { "R&B and slow jams",
          "60 to 80 BPM. Snare on two and four, few kicks, and soft hats with space around them. The drums stay out of the way of the chords and the voice. Rim shots and finger snaps often replace the snare.", S::Grid, 6 },
        { "Learn a style by copying its drums",
          "Take a song you love, loop two bars, and rebuild only the drums by ear: snare first, then kicks, then hats. You will get it wrong, compare, and fix it. Do that ten times with ten songs and you will know the style better than any tutorial can teach it.", S::Grid, 9 } },
      nullptr, kTabNone, nullptr },

    { "bt.hats", "Drums", "Rolls, velocity and bounce", {
        { "Velocity is how hard a hit is",
          "Every hit has a velocity, from soft to hard. A drummer never hits the hat the same way twice. If every hat in your pattern is at full velocity it sounds like a machine gun. Make the hats on the beat a little louder and the ones between quieter. The bars under the grid show each hit's velocity.", S::Grid, 7 },
        { "Hi-hat rolls",
          "A roll is a burst of very fast hats: the grid here is twice as fine, thirty-second notes. Put a roll at the end of a bar leading into the next one, not everywhere. Let its velocity rise or fall across the roll so it sounds like a gesture and not a buzz.", S::Grid, 7 },
        { "Triplet rolls",
          "Three hats in the space of two give the rolling, tumbling trap sound. Mixing straight hats with a triplet burst is what makes a hi-hat line feel alive. In the DAW you get them by changing the grid to a triplet setting for a moment.", S::Grid, 8 },
        { "Bounce",
          "Bounce comes from contrast: loud against soft, on the beat against off it, busy against empty. Take a pattern that feels flat and try three things. Remove a hit. Move one kick a step earlier or later. Lower the velocity of every second hat. One of them will wake it up.", S::Grid, 3 },
        { "Percussion and fills",
          "Once kick, snare and hats work, add small extras: a rim, a shaker, a snap, a reversed cymbal into the hook. At the end of every fourth or eighth bar, change something for one beat: a roll, a dropped kick, a silent gap. The listener needs a signpost that the section is turning over.", S::Song, 2 } },
      nullptr, kTabNone, nullptr },

    { "bt.buildtrap", "Build one, layer by layer", "Build a trap beat", {
        { "Start with the melody",
          "C minor, 140 BPM. One sound, two bars. The first bar states an idea of four notes. The second bar repeats it and changes the ending so it lands on C, the home note. Press play and loop it until you can hum it. Everything else in the beat will be built to serve this.", S::Build, 0 },
        { "Chords underneath",
          "Two chords, one per bar: C minor, then A flat. The melody's long notes are notes of the chord under them, so nothing rubs. The chords are on a softer sound and quieter than the melody. Many trap beats stop here and use no chords at all: try it both ways in your own beat.", S::Build, 1 },
        { "Kick and snare",
          "The snare lands on beat three of each bar: that is the half-time feel that makes 140 nod like 70. The kick starts each bar, then falls off the beat twice. Five kicks in two bars. Count them: fewer than you expected.", S::Build, 2 },
        { "Hi-hats",
          "Eighth-note hats across both bars, with three extra quiet hits near the ends of the bars that turn into short rolls, and one open hat. The hats are quieter than everything else so far. Mute them in your head and the beat still works: they add speed, not structure.", S::Build, 3 },
        { "The 808",
          "Every 808 note starts on a kick. It plays C under the C minor bar and A flat under the A flat bar: the root of each chord. The last note steps up to B flat to lead back to the start. Notes last until just before the next one, and never overlap.", S::Build, 4 },
        { "A counter melody",
          "Three notes, placed only where the main melody is resting: the end of bar one and the end of bar two. On a different sound, an octave lower. That is the whole loop: six layers, each with one job. From here it is arrangement: take layers out to make an intro and a verse.", S::Build, 5 } },
      nullptr, kTabNone, nullptr, 1 },

    { "bt.builddrill", "Build one, layer by layer", "Build a drill beat", {
        { "A dark melody",
          "F minor, 142 BPM. The idea rocks between C and the note one semitone above it, D flat. That semitone is where the menace comes from. Then it falls to A flat and F, the home note. Short, low and repetitive: drill melodies leave most of the bar empty.", S::Build, 10 },
        { "One chord a bar",
          "F minor, then D flat. Only one note moves between the two chords, from C to D flat, the same semitone the melody is built on. Keep the chords low and quiet, more a shadow than a part.", S::Build, 11 },
        { "Kick and snare",
          "The snare is on beat three of the first bar, then on beat four of the second: one beat late. That displaced snare is the drill signature. The kicks are sparse and sit in the gaps the snare leaves.", S::Build, 12 },
        { "The hats skip",
          "Instead of even eighth notes, the hats play three steps, three steps, two steps, over and over. Count it: one-two-three, one-two-three, one-two. That lopsided pattern against the straight snare is the bounce of drill.", S::Build, 13 },
        { "The sliding 808",
          "The 808 follows the roots, F then D flat. Two of its notes bend: the second note of bar one slides up a whole octave, and the last note slides from E flat up to F, back home for the repeat. The slides are the hook of the bass line: two in two bars is plenty.", S::Build, 14 },
        { "A high answer",
          "A bell, high above everything, answers at the end of each bar with one or two notes. Because the main melody is low and sparse, there is room for it. Compare this loop with the trap one: same six layers, different rules for each.", S::Build, 15 } },
      nullptr, kTabNone, nullptr, 1 },

    { "bt.buildrnb", "Build one, layer by layer", "Build an R&B beat", {
        { "A melody that sings",
          "A minor, 72 BPM. Slower, with longer notes that move mostly by step, like something a singer could sing. It falls in the first bar and rises in the second, ending high and unresolved so the loop wants to come round again.", S::Build, 20 },
        { "Chords carry it",
          "In R&B the chords matter as much as the melody. A minor, then F: the two chords share two notes, so the change is gentle. On a warm electric piano or pad, these would be seventh chords: try that in your DAW once the triads work.", S::Build, 21 },
        { "Kick and snare",
          "At this tempo the snare goes back to beats two and four of every bar. The kicks are few and sit off the beat. A rim shot or a snap often replaces the snare. Nothing here should fight the voice that will sit on top.", S::Build, 22 },
        { "Soft hats with swing",
          "Eighth-note hats, quiet, with a little swing so they lean back, and an open hat just before the bar turns over. The hats in R&B are barely there: they keep time for the listener without being noticed.", S::Build, 23 },
        { "A bass that holds",
          "Long notes on the roots, A then F, with one step up at the end to lead back. No slides, no busy rhythm. In R&B the bass is often a warm sub or a bass guitar instead of a hard 808, played softly and held.", S::Build, 24 },
        { "A light answer",
          "Two high notes, one at the end of each bar, are all the counter melody does. The loop is finished, and it sounds half empty on purpose: the space in the middle belongs to the singer.", S::Build, 25 } },
      nullptr, kTabNone, nullptr, 1 },

    { "bt.808", "Bass", "808s and bass lines", {
        { "What an 808 is",
          "An 808 is a long, deep bass note that started as the kick drum of a drum machine. It is both drum and bass at once, and it has a pitch: it plays notes. That means it must be in key like everything else. An 808 on the wrong note is the most common reason a beginner's beat sounds off.", S::Roll, 11 },
        { "Follow the root",
          "The safe choice: the 808 plays the root note of whatever chord is sounding. If there are no chords, it plays the root of the key and moves to one or two other scale notes for variety. Low and simple wins. Listen to the bass follow the chords here.", S::Roll, 11 },
        { "Tune it",
          "An 808 sample has its own pitch, often C but not always. If the sampler thinks it is a C and it is really an F, every note you play is wrong. Set the sample's root note in the sampler, or tune it by ear against a piano playing the same key. Check it an octave up, where pitch is easier to hear.", S::Keys, 1 },
        { "Kick and 808 together",
          "They share the same low space, so they must take turns or hit as one. The usual way: the kick and the 808 start together, the kick gives the click and punch, the 808 carries the note after it. Do not let two 808 notes overlap: set the sampler to cut the old note when a new one starts.", S::Grid, 10 },
        { "Slides",
          "The slide, where an 808 bends up or down into another note, is the signature of drill and modern trap. It is made with glide or slide notes in the sampler: the DAW lessons show how in FL Studio and Logic. Slide by an octave, or to a note of the key. One or two slides in a bar is a hook. Ten is noise.", S::Roll, 15 },
        { "Hearing it on small speakers",
          "Phones and laptops cannot play the lowest octave at all. If your 808 is only a pure low sine, it disappears. A little saturation or distortion adds higher harmonics that small speakers can play, so the ear fills in the bass. Check every beat on a phone before you call it finished.", S::Saturation, 0 } },
      "Open Tone", kTabTone, nullptr },

    { "bt.arrange", "Arrangement", "From a loop to a song", {
        { "Sections",
          "A song is your loop with things taken away and put back. The usual parts: an intro, a hook, a verse, the hook again, a second verse, the hook, and an outro. Hooks are usually 8 bars, verses 16. In a trap or drill project set to 140, the artist counts at 70, so their 16 bars are 32 on your screen. Lay the sections out first, even as empty blocks.", S::Song, 0 },
        { "Build the hook first, then subtract",
          "Make the fullest version of the beat: every layer playing. That is the hook. Then make the verse by removing things: take out a melody layer, thin the hats, drop the counter melody. Click each section to see what plays in it. The verse must leave room for the voice.", S::Song, 1 },
        { "The intro",
          "Four or eight bars that set the mood without giving everything away. Often it is just the melody, filtered or with no drums. A tag or a small sound effect can mark the start. The drums arriving is the first big moment of the song, so do not waste it.", S::Song, 1 },
        { "Transitions",
          "The bar before a new section should tell the listener something is coming. Cut the drums for the last beat. Add a roll, a riser or a reversed cymbal. Drop everything to silence for half a beat before the hook lands. A hook hits hard because of the gap before it.", S::Song, 2 },
        { "Keep changing something every eight bars",
          "If nothing changes for more than eight bars, attention drifts. The change can be tiny: an extra hat, a new percussion sound, the melody an octave higher, one bar with no kick. The listener should never be able to predict the next eight bars exactly from the last eight.", S::Song, 2 } },
      nullptr, kTabNone, nullptr },

    { "bt.levels", "Mixing the beat", "Levels first", {
        { "Most of a mix is volume",
          "Before any EQ or effect, set every sound's level. Pull all the faders down. Bring up the kick and the 808 first, peaking around -6 dB on the master so there is headroom. Then bring each other sound up until it sits right against them. If you do this well, the beat is 80 percent mixed.", S::Layers, 3 },
        { "What the records do",
          "These figures were measured on the separated parts of finished records in this style, not guessed. The 808 and the drums are the loudest things in the beat and sit within about a decibel of each other. The melodies and music together sit about 6 dB quieter than the 808. Most beginners have the melody far too loud.", S::Layers, 3 },
        { "Leave room for the vocal",
          "On those records the lead vocal is about as loud as the 808, and the whole beat together is only about 3 dB louder than the voice. A beat mixed to sound huge on its own leaves no space. If the beat is for an artist, mix it so the middle feels a little empty.", S::Layers, 1 },
        { "Let the plugin measure it",
          "Put this plugin on each track, set each one's role in Settings (kick, 808, drums, melody), and press Learn and balance on the Mix tab while the loudest section plays. It measures every track and sets the levels to those same measured relationships. Then adjust by ear.", S::Chain, 0 } },
      "Open Mix", kTabMix, nullptr },

    { "bt.space", "Mixing the beat", "Give every sound its own space", {
        { "Why beats get muddy",
          "Every sound you add brings low and low-mid energy with it, even bells and hats. Stack eight sounds and the area from 150 to 400 Hz is crowded with things nobody can hear clearly. The picture shows where each part of a beat lives, all overlapping.", S::Layers, 0 },
        { "Give each sound a slot",
          "The 808 owns the bottom. The kick shares it and adds a click higher up. Melodies and chords live in the middle. Hats live at the top. Put a low cut on every sound that is not the kick or the 808, somewhere between 100 and 250 Hz, and the low end clears up at once.", S::Layers, 1 },
        { "Cut what collides",
          "If two sounds fight in the same range, pick the one that matters more there and dip the other by a few decibels in that range. Put this plugin on both tracks and press Find collisions on the Mix tab: it shows where they overlap and which one should give way.", S::Masking, 1 },
        { "Left, right and centre",
          "Keep the kick, the 808, the snare and the main melody in the centre: they are the spine, and they must survive on a phone or a club system playing in mono. Spread the extras: a counter melody a little left, a percussion sound a little right, pads wide. Width only works when something stays in the middle.", S::Layers, 2 },
        { "Keep the low end mono",
          "Below about 120 Hz, stereo width does nothing useful and causes real problems: the bass can vanish when left and right are added together. Keep the 808 and kick dead centre. The Tone tab's stereo section can make everything below a chosen frequency mono.", S::Stereo, 0 } },
      "Find collisions", kTabMix, nullptr },

    { "bt.fx", "Mixing the beat", "Effects that lift a sound", {
        { "Reverb puts a sound in a room",
          "A dry sound is right in your face. Reverb adds the reflections of a space, so the sound moves back and feels bigger. A little on a melody makes it expensive. A lot makes it distant and washes out the beat. Never on the 808 or the kick: reverb in the low end is mud.", S::Fx, 0 },
        { "Delay repeats it in time",
          "Delay is an echo locked to the tempo: an eighth note, a quarter note, a dotted eighth. It fills gaps in a sparse melody and adds movement without new notes. Keep the repeats quieter than the sound and cut their lows so they sit behind it.", S::Fx, 1 },
        { "Filters shape and move",
          "A low-pass filter removes the highs, so a sound goes dark and muffled, as if heard through a wall. Sweep it open over four bars and you have the classic intro into a hook. A high-pass does the opposite, thinning a sound out before the drop brings the weight back.", S::Fx, 2 },
        { "Saturation adds weight and edge",
          "Saturation gently distorts a sound, adding harmonics above the note. On an 808 it makes the bass audible on small speakers. On drums it adds punch and glue. On a thin melody it adds warmth. The Tone tab's saturation does this.", S::Saturation, 1 },
        { "Sidechain: duck for the kick",
          "When the kick and another sound hit together, the kick loses its punch. Sidechain compression turns the other sound down for an instant each time the kick plays, then lets it back. Used gently it is invisible and makes the kick cut through. Used hard it is the pumping sound of dance music.", S::Fx, 3 },
        { "One job each",
          "Add an effect because the sound needs something, not because the slot is empty. Ask what is missing: space, movement, weight, brightness. Then pick the one effect that gives it. Switch it off and on while the whole beat plays. If you cannot hear a difference for the better, take it off.", S::Chain, 0 } },
      "Open FX", kTabFX, nullptr },

    { "bt.finish", "Mixing the beat", "Finish and export", {
        { "Check it everywhere",
          "A beat that only sounds good on your headphones is not finished. Listen on a phone speaker, in a car if you can, quietly, and loud. On the Master tab this plugin can play the mix as mono and as a phone speaker would. If the 808 or the melody vanishes there, fix that before anything else.", S::Loudness, 0 },
        { "Leave headroom",
          "The master meter must never hit red. If the beat is going to an artist or an engineer to record and mix, export it with the loudest peaks around -6 dB and no limiter. Loudness is added at the very end, at mastering, once the vocals are in.", S::TruePeak, 0 },
        { "Export the beat, and the parts",
          "Export a WAV, not only an MP3: WAV keeps the full quality. For anyone who will mix the song, also export the stems: one file per part, such as kick, snare, hats, 808 and each melody, all starting from bar one so they line up. Name the files with the tempo and key, for example \"Night Drive 140 Cmin\".", S::Card, 10 },
        { "A loud version for sending out",
          "A beat going to artists or online has to hold up next to finished records. Put this plugin on the master, choose the House style on the Master tab, and let it set the level. It fits the tone and loudness to what was measured on finished records in this style.", S::Loudness, 1 } },
      "Open Master", kTabMaster, nullptr },
    };
}
}
